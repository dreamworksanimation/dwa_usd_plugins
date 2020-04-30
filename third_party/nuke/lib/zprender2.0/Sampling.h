//
// Copyright 2020 DreamWorks Animation
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

/// @file zprender/Sampling.h
///
/// @author Jonathan Egstad


#ifndef zprender_Sampling_h
#define zprender_Sampling_h

#include <Fuser/Vec2.h>
#include <Fuser/AttributeTypes.h>


namespace zpr {


// TODO: switch to OpenDCX lib for this stuff? We're deprecating the 8x8 support...
#if 0
#  include "DeepPixelHandler.h" // for SpMask8
#else
namespace Dcx {
typedef uint64_t SpMask8;
static const size_t  SPMASK_WIDTH         =  8;
static const size_t  SPMASK_NUM_BITS      = 64;
static const SpMask8 SPMASK_ZERO_COVERAGE = 0x0ull;
static const SpMask8 SPMASK_FULL_COVERAGE = 0xffffffffffffffffull;
}
#endif


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! 2D sample coordinate.
*/
struct Sample2D
{
    Fsr::Vec2f dp;              //!< Offset - centered at 0,0
    float      radius;          //!< Distance from 0,0

    uint16_t     sp_src_x;      //!< Subpixel X location in SOURCE pixel-sample rate
    uint16_t     sp_src_y;      //!< Subpixel Y location in SOURCE pixel-sample rate
    Dcx::SpMask8 spmask;        //!< Subpixel mask for this sample (src xy may cover more than 1 output spmask bin!)

    //! Sets the location and pre-calculates radius
    void set(const Fsr::Vec2f& v)
    {
        dp = v;
        radius = dp.length();
    }
};
typedef std::vector<Sample2D> Sample2DList;


//----------------------------------------------------------


/*!
*/
class SampleGrid2D
{
  protected:
   uint32_t     m_grid_size;  //!< Width & height size of grid
   Sample2DList m_samples;    //!< List of samples

  public:
    //!
    SampleGrid2D() : m_grid_size(0) {}
    SampleGrid2D(uint32_t grid_size) { setGridSize(grid_size); }

    //! Copy constructor
    SampleGrid2D(const SampleGrid2D& b) { *this = b; }

    //! Copy operator
    SampleGrid2D& operator = (const SampleGrid2D& b)
    {
        if (&b != this)
        {
            m_grid_size = b.m_grid_size;
            m_samples   = b.m_samples;
        }
        return *this;
    }

    void setGridSize(uint32_t grid_size) { m_grid_size = grid_size; m_samples.resize(m_grid_size*m_grid_size); }

    uint32_t size()      const { return (uint32_t)m_samples.size(); }
    uint32_t grid_size() const { return m_grid_size; }

    //! Read-only access
    const Sample2D& operator[] (uint32_t i) const { return m_samples[i]; }

    //! Read/Write access
    Sample2D& sample(uint32_t x,
                     uint32_t y) { return m_samples[x + y*m_grid_size]; }
    Sample2D& sample(uint32_t i) { return m_samples[i]; }

    //! Copy samples from array of Vec2fs
    void copy(const std::vector<Fsr::Vec2f>& src)
    {
        const size_t end = (src.size() > m_samples.size())?m_samples.size():src.size();
        for (size_t i=0; i < end; ++i)
            m_samples[i].set(src[i]);
    }

    //! Copy samples
    void copy(const Sample2DList& src)
    {
        const size_t end = (src.size() > m_samples.size())?m_samples.size():src.size();
        for (size_t i=0; i < end; ++i)
            m_samples[i] = src[i];
    }
};


//----------------------------------------------------------


/*! 
*/
class StochasticSampleSet
{
  public:
    Sample2D   subpixel;                //!< 2D subpixel sample
    Fsr::Vec2f lens;                    //!< 2D lens sample for depth of field.
    float      time;                    // TODO: make a list of time samples? Or min/max?
    //
    SampleGrid2D diffuse_samples;       //!< Grid of diffuse samples
    SampleGrid2D glossy_samples;        //!< List of glossy samples
    SampleGrid2D refraction_samples;    //!< List of refraction samples

    //!
    StochasticSampleSet() {}
    //! Copy constructor
    StochasticSampleSet(const StochasticSampleSet& b) { *this = b; }

    //! Copy operator
    StochasticSampleSet& operator = (const StochasticSampleSet& b)
    {
        if (&b != this)
        {
            subpixel           = b.subpixel;
            lens               = b.lens;
            time               = b.time;
            diffuse_samples    = b.diffuse_samples;
            glossy_samples     = b.glossy_samples;
            refraction_samples = b.refraction_samples;
        }
        return *this;
    }

};
typedef std::vector<StochasticSampleSet> StochasticSampleSetList;


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! This is cribbed from embree and stripped down a bit.
*/
class RandomGenerator
{
  public:
    //!
    RandomGenerator(int32_t seed=27) { setSeed(seed); }

    //!
    void setSeed(int32_t seed)
    {
        const int32_t a = 16807;
        const int32_t m = 2147483647;
        const int32_t q = 127773;
        const int32_t r = 2836;

        if      (seed == 0) m_seed = 1;
        else if (seed  < 0) m_seed = -seed;
        else                m_seed = seed;

        for (int32_t j=32+7; j >= 0; j--)
        {
            const int32_t k = m_seed / q;
            m_seed = a*(m_seed - k*q) - r*k;
            if (m_seed < 0)
                m_seed += m;
            if (j < 32)
                m_table[j] = m_seed;
        }
        m_state = m_table[0];
    }

    //!
    int32_t getInt()
    {
        const int32_t a = 16807;
        const int32_t m = 2147483647;
        const int32_t q = 127773;
        const int32_t r = 2836;

        const int32_t k = (m_seed / q);
        m_seed = a*(m_seed - k*q) - r*k;
        if (m_seed < 0)
            m_seed += m;
        const int32_t j = m_state / (1 + (2147483647-1) / 32);
        m_state    = m_table[j];
        m_table[j] = m_seed;

        return m_state;
    }

    //!
    int32_t getInt(int32_t limit) { return getInt() % limit; }
    float   getFloat()            { return std::min( float(getInt()) / 2147483647.0f, 1.0f - std::numeric_limits<float>::epsilon()); }
    double  getDouble()           { return std::min(double(getInt()) / 2147483647.0 , 1.0  - std::numeric_limits<double>::epsilon()); }


  private:
    int32_t m_seed;
    int32_t m_state;
    int32_t m_table[32];
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Create a set of n jittered 1D samples, using the provided 
    random number generator.
*/
inline void
jittered(std::vector<float>& samples,
         RandomGenerator&    rgen)
{
    int32_t n = (int32_t)samples.size();
    if (n == 0) {
        n = 1;
        samples.resize(1);
    }

    // Build permutation table:
    int32_t perm[n];
    for (int32_t i=0; i < n; ++i)
        perm[i] = i;
    for (int32_t i=0; i < n; ++i)
        std::swap(perm[i], perm[rgen.getInt(n)]);

    // Apply:
    const float inv_total = 1.0f / float(n);
    for (int32_t i=0; i < n; ++i)
        samples[perm[i]] = (float(i) + rgen.getFloat())*inv_total;
}


/*! Create a set of n multi-jittered 2D samples, using the provided 
    random number generator.
*/
inline void
multiJittered(Fsr::Vec2fList&  samples,
              RandomGenerator& rgen)
{
    int32_t n = (int32_t)samples.size();
    if (n == 0)
    {
       n = 1;
       samples.resize(1);
    }
    const int32_t nBinsPerSide = int32_t(std::sqrt(float(n)));
    const float   inv_total    = 1.0f / float(nBinsPerSide*nBinsPerSide);

    uint32_t index[nBinsPerSide];
    for (int32_t i=0; i < nBinsPerSide; ++i)
        index[i] = i;

    Fsr::Vec2f bin_grid[nBinsPerSide][nBinsPerSide];

    // Sort X samples:
    for (int32_t i=0; i < nBinsPerSide; ++i)
    {
        for (int32_t j=0; j < nBinsPerSide; ++j)
        {
            // At each bin get a random number within the PerSide range:
            const int32_t jit_bin = rgen.getInt() % nBinsPerSide;
            std::swap(index[i], index[jit_bin]);
        }

        for (int32_t j=0; j < nBinsPerSide; ++j)
            bin_grid[i][j].x = clamp(float(i)/float(nBinsPerSide) + (float(index[j])+rgen.getFloat())*inv_total) - 0.5f;
    }
    // Sort Y samples:
    for (int32_t j=0; j < nBinsPerSide; ++j)
    {
        for (int32_t i=0; i < nBinsPerSide; ++i)
        {
            const int32_t jit_bin = rgen.getInt() % nBinsPerSide;
            std::swap(index[j], index[jit_bin]);
        }

        for (int32_t i=0; i < nBinsPerSide; ++i)
            bin_grid[i][j].y = clamp(float(j)/float(nBinsPerSide) + (float(index[i])+rgen.getFloat())*inv_total) - 0.5f;
    }

    // Build permutation table:
    int32_t perm[n];
    for (int32_t i=0; i < n; ++i)
        perm[i] = i;
    for (int32_t i=0; i < n; ++i)
        std::swap(perm[i], perm[rgen.getInt(n)]);

    // Save in sample array:
    uint32_t count = 0;
    for (int32_t j=0; j < nBinsPerSide; ++j)
        for (int32_t i=0; i < nBinsPerSide; ++i)
            samples[perm[count++]] = bin_grid[i][j];
#if 0
    // Print out pattern:
    int32_t m = 39;
    std::cout << "jittered sample_count=" << n << std::endl;
    for (int32_t y=0; y < m; ++y)
    {
        for (int32_t x=0; x < m; ++x)
        {
            uint32_t i = 0;
            for (; i < n; ++i)
            {
                if (int32_t(floorf((samples[i].x + 0.5f)*(m - 1)))==x &&
                    int32_t(floorf((samples[i].y + 0.5f)*(m - 1)))==y)
                    break;
            }
            if (i < n)
                std::cout << " " << i;
            else
                std::cout << " -";
        }
        std::cout << std::endl;
    }
#endif
}


/*! Create a set of n uniformly-distributed 2D samples.
*/
inline void
multiUniform(Fsr::Vec2fList& samples)
{
    const int32_t n = (int32_t)samples.size();
    if (n <= 1)
    {
        samples[0].x = samples[0].y = 0.0f;
    }
    else
    {
        const int32_t nBinsPerSide = int32_t(std::sqrt(float(n)));
        const float   inv_s        = 1.0f / float(nBinsPerSide-1);

        uint32_t count = 0;
        for (int32_t j=0; j < nBinsPerSide; ++j)
        {
            const float y = (float(j)*inv_s) - 0.5f;
            for (int32_t i=0; i < nBinsPerSide; ++i)
            {
                Fsr::Vec2f& sample = samples[count++];
                sample.x = (float(i)*inv_s) - 0.5f;
                sample.y = y;
            }
        }
    }
#if 0
    // Print out pattern:
    int32_t m = 39;
    std::cout << "uniform sample_count=" << n << std::endl;
    for (int32_t y=0; y < m; ++y)
    {
        for (int32_t x=0; x < m; ++x)
        {
            uint32_t i = 0;
            for (; i < n; ++i)
            {
                if (int32_t(floorf((samples[i].x + 0.5f)*(m - 1)))==x &&
                    int32_t(floorf((samples[i].y + 0.5f)*(m - 1)))==y)
                     break;
            }
            if (i < n)
                std::cout << " " << i;
            else
                std::cout << " -";
        }
        std::cout << std::endl;
    }
#endif
}


} // namespace zpr


#endif

// end of zprender/Sampling.h

//
// Copyright 2020 DreamWorks Animation
//
