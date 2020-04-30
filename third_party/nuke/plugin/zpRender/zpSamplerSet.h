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

/// @file zpSamplerSet.h
///
/// @author Jonathan Egstad


#ifndef zprender_zpSamplerSet_h
#define zprender_zpSamplerSet_h

#include <zprender/Sampling.h>
#include <zprender/Traceable.h>  // for SPMASK defines


namespace zpr {


/*! The sampler set precomputes samples.
*/
class SamplerSet
{
  public:
    std::vector<zpr::StochasticSampleSetList*> m_sets;   //!< List of sample sets

    uint32_t m_set_count;               //!< Number of sets
    int      m_iteration;               //!< Current iteration.
    uint32_t m_sample_side_count;       //!< Number of samples in a set
    uint32_t m_diffuse_side_count;
    uint32_t m_glossy_side_count;
    uint32_t m_refraction_side_count;


  public:
    SamplerSet(uint32_t sample_side_count=1, uint32_t sets=64) :
        m_set_count(std::max((uint32_t)1, sets)),
        m_iteration(0),
        m_sample_side_count(std::max((uint32_t)1, sample_side_count))
    {
        m_diffuse_side_count    = 1;
        m_glossy_side_count     = 1;
        m_refraction_side_count = 1;
    }

    uint32_t nSets() const { return m_set_count; }

    uint32_t n2DSideCount() const { return m_sample_side_count; }

    uint32_t n2DSamples() const { return m_sample_side_count*m_sample_side_count; }

    const StochasticSampleSetList& getSampleSet(int n) const { return *m_sets[n%m_set_count]; }


#if 0
    /*! Request a precomputed light sample. */
    int SamplerSet::requestLightSample(int baseSample, const Ref<Light>& light)
    {
        m_numLightSamples++;
        lights.push_back(light);
        lightBaseSamples.push_back(baseSample);
        return m_numLightSamples-1;
    }
#endif


    /*! Initialize the set for a given iteration and precompute all samples.
        Apply spatial jittering if sample count is greater than passed threshold value.
    */
    void initialize(int Z, int spatial_jittering_threshold=1)
    {
        //std::cout << "sampler set initialize(" << Z << "): m_sample_side_count=" << m_sample_side_count << " m_set_count=" << m_set_count;
        //std::cout << " m_diffuse_side_count=" << m_diffuse_side_count;
        //std::cout << " m_glossy_side_count=" << m_glossy_side_count;
        //std::cout << " m_refraction_side_count=" << m_refraction_side_count;
        //std::cout << std::endl;
        m_iteration = Z;
        for (uint32_t i=0; i < m_sets.size(); ++i)
            delete m_sets[i];
        m_sets.clear();

        // Square each side count:
        const uint32_t nPointSamples = m_sample_side_count*m_sample_side_count;

        int seed = (int(m_iteration*nPointSamples) / nPointSamples) * 5897;
        RandomGenerator rgen;
        rgen.setSeed(seed);

        Fsr::Vec2fList   point_samples(nPointSamples);
        Fsr::FloatList    time_samples(nPointSamples);
        Fsr::Vec2fList    lens_samples(nPointSamples);
        Fsr::Vec2fList diffuse_samples(m_diffuse_side_count*m_diffuse_side_count);
        Fsr::Vec2fList  glossy_samples(m_glossy_side_count*m_glossy_side_count);
        Fsr::Vec2fList refract_samples(m_refraction_side_count*m_refraction_side_count);

        m_sets.reserve(m_set_count);
        for (uint32_t j=0; j < m_set_count; ++j)
        {
            m_sets.push_back(new StochasticSampleSetList());
            StochasticSampleSetList& set = *m_sets[j];
            set.reserve(nPointSamples);

            zpr::jittered(time_samples, rgen);

            zpr::multiJittered(lens_samples, rgen);
            //zpr::multiUniform(lens_samples);

            if ((int)sqrtf(float(nPointSamples)) >= spatial_jittering_threshold)
                zpr::multiJittered(point_samples, rgen);
            else
                zpr::multiUniform(point_samples);

            for (uint32_t s=0; s < nPointSamples; ++s)
            {
                set.push_back(StochasticSampleSet());
                StochasticSampleSet& sample = set[set.size()-1];

                const Fsr::Vec2f& dp = point_samples[s];
                if (nPointSamples == 1)
                {
                   sample.subpixel.dp.set(0.0f, 0.0f);
                   sample.subpixel.radius = 0.0f;
                   sample.subpixel.sp_src_x = sample.subpixel.sp_src_y = 0;
                   sample.subpixel.spmask = Dcx::SPMASK_FULL_COVERAGE;
                }
                else
                {
                    // Scale down the samples by one bin width to center them in the bins, otherwise
                    // adjacent pixels will end up sampling the same location:
                    sample.subpixel.dp = dp;
                    sample.subpixel.dp *= (float(m_sample_side_count - 1) / float(m_sample_side_count));
                    sample.subpixel.radius = dp.length();

                    // Figure out which sample bin it goes in at the source pixel-sample rate:
                    sample.subpixel.sp_src_x = (uint16_t)floorf(clamp(dp.x + 0.5f)*float(m_sample_side_count));
                    sample.subpixel.sp_src_y = (uint16_t)floorf(clamp(dp.y + 0.5f)*float(m_sample_side_count));

                    //------------------------------------------------------------------------
                    // Build the output subpixel mask
                    //------------------------------------------------------------------------
                    // Find the output bins this sample overlaps:
                    if (m_sample_side_count < Dcx::SPMASK_WIDTH)
                    {
                        // uprez mask, sample covers multiple output bins:
                        sample.subpixel.spmask = Dcx::SPMASK_ZERO_COVERAGE;
                        const float bin_upscale = float(m_sample_side_count) / float(Dcx::SPMASK_WIDTH);
                        for (uint32_t out_y=0; out_y < Dcx::SPMASK_WIDTH; ++out_y)
                        {
                            const int in_y = (int)floorf((float(out_y)+0.5f) * bin_upscale);
                            for (uint32_t out_x=0; out_x < Dcx::SPMASK_WIDTH; ++out_x)
                            {
                                const int in_x = (int)floorf((float(out_x)+0.5f) * bin_upscale);
                                if (in_x == sample.subpixel.sp_src_x && in_y == sample.subpixel.sp_src_y)
                                {
                                    const uint32_t sp_bin = out_y*(uint32_t)Dcx::SPMASK_WIDTH + out_x;
                                    sample.subpixel.spmask |= (Dcx::SpMask8(0x01) << sp_bin);
                                }
                            }
                        }
                    }
                    else if (m_sample_side_count == Dcx::SPMASK_WIDTH)
                    {
                        // masks are same rez, sample exactly covers 1 output bin:
                        const uint32_t sp_bin = sample.subpixel.sp_src_y*(uint32_t)Dcx::SPMASK_WIDTH + sample.subpixel.sp_src_x;
                        sample.subpixel.spmask = (Dcx::SpMask8(0x01) << sp_bin);
                    }
                    else
                    {
                        // downrez mask, sample is inside only 1 output bin:
                        const float bin_downscale = float(Dcx::SPMASK_WIDTH) / float(m_sample_side_count);
                        const int out_x = (int)floorf((float(sample.subpixel.sp_src_x) + 0.5f) * bin_downscale);
                        const int out_y = (int)floorf((float(sample.subpixel.sp_src_y) + 0.5f) * bin_downscale);
                        const uint32_t sp_bin = out_y*(uint32_t)Dcx::SPMASK_WIDTH + out_x;
                        sample.subpixel.spmask = (Dcx::SpMask8(0x01) << sp_bin);
                    }
                }

                sample.lens.set(0.0f, 0.0f);
                sample.time = time_samples[s];
                //std::cout << s << "[" << sample.dx << " " << sample.dy << "]" << std::endl;

                sample.diffuse_samples.setGridSize(m_diffuse_side_count);
                if (m_diffuse_side_count > 0)
                {
                    ++seed;
                    rgen.setSeed(seed);
                    zpr::multiJittered(diffuse_samples, rgen);
                    sample.diffuse_samples.copy(diffuse_samples);
                }

                sample.glossy_samples.setGridSize(m_glossy_side_count);
                if (m_glossy_side_count > 0)
                {
                    ++seed;
                    rgen.setSeed(seed);
                    zpr::multiJittered(glossy_samples, rgen);
                    sample.glossy_samples.copy(glossy_samples);
                }

                sample.refraction_samples.setGridSize(m_refraction_side_count);
                if (m_refraction_side_count > 0)
                {
                    ++seed;
                    rgen.setSeed(seed);
                    zpr::multiJittered(refract_samples, rgen);
                    sample.refraction_samples.copy(refract_samples);
                }
            }

#if 0
            // Print sampling info:
            if (j == 0)
            {
                int n = 39;
                std::cout << "nPointSamples=" << nPointSamples << " x=" << n << std::endl;
                for (int y=0; y < n; ++y)
                {
                    for (int x=0; x < n; ++x)
                    {
                        uint32_t i = 0;
                        for (; i < nPointSamples; ++i)
                        {
                            StochasticSampleSet& sample = set[i];
                            //if (int(floorf((sample.subpixel.dp.x+0.5f)*n))==x && int(floorf((sample.subpixel.dp.y+0.5f)*n))==y) break;
                            if (int(floorf((sample.lens.x+1.0f)*0.5f*n))==x &&
                                int(floorf((sample.lens.y+1.0f)*0.5f*n))==y)
                                break;
                        }
                        if (i < nPointSamples)
                            std::cout << " X"; else std::cout << " -";
                        //if (i < nPointSamples) std::cout << " " << i; else std::cout << " -";
                    }
                    std::cout << std::endl;
                }
            }
#endif
            ++seed;
        }
    }

};



} // namespace zpr

#endif

// end of zpSamplerSet.h

//
// Copyright 2020 DreamWorks Animation
//
