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

/// @file zprender/Texture2dSampler.h
///
/// @author Jonathan Egstad


#ifndef zprender_Texture2dSampler_h
#define zprender_Texture2dSampler_h

#include "api.h"

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/RawGeneralTile.h>
#include <DDImage/TextureFilter.h>


namespace zpr {


/*! A Texture2dSampler is intended for connection to a source
    Iop.

    This class stores a RawGeneralTile cache to speed texture filtering
    at the cost of memory, but it's only for the life of the render
    and the caches are released as soon as possible.

    Replaces using the Iop::sample() methods which have become very
    slow and unfortunately this means having to replicate much of
    the filter behavior of the Iop::sample() methods... Consequently
    we only support the base Filter kernels and not the TextureFilter
    mip ones, but we could at some later point.
*/
class ZPR_EXPORT Texture2dSampler
{
    enum
    {
        NOT_LOADED = 0,   //!<
        LOADING    = 1,   //!< 
        LOADED     = 2,   //!< 
        ERROR      = 3    //!<
    };


  protected:
    DD::Image::Iop*            m_iop;       //!< Source Iop
    DD::Image::ChannelSet      m_channels;  //!< Channels to get from Iop
    DD::Image::RawGeneralTile* m_tile;      //!< Pointer to tile cache
    int32_t                    m_status;    //!< Tile load status
    bool                       m_error;     //!< Error when creating tile
    Fsr::Vec2f                 m_offset;    //!< Float version of tile offset
    Fsr::Vec2f                 m_scale;     //!< Float version of tile scale


    //!
    bool buildTile();


  public:
    //! 
    Texture2dSampler(DD::Image::Iop*              iop,
                     const DD::Image::ChannelSet& channels);
    //!
    ~Texture2dSampler();

    //! Tile pointer is non-NULL only if the tile loaded successfully.
    bool isValid() const { return (m_status == LOADED); }

    //! Return the Iop the sampler is bound to.
    DD::Image::Iop* iop() const { return m_iop; }

    //! Get the channels this will sample.
    const DD::Image::ChannelSet& channels() const { return m_channels; }


  public:
    //! Replicates the Iop::sample() method.
    void sampleFilterered(const Fsr::Vec2f&        uv,
                          const Fsr::Vec2f&        dUVdx,
                          const Fsr::Vec2f&        dUVdy,
                          const DD::Image::Filter* filter,
                          Fsr::Pixel&              out);

    //! Sample a single texel with no filtering.
    void samplePixel(int32_t     tx,
                     int32_t     ty,
                     Fsr::Pixel& out);


  public:
    //! TODO: for future mipmap support:
    void bilinearFilterX(int32_t     tx,
                         float       dx,
                         int32_t     ty,
                         Fsr::Pixel& out);
    void bilinearFilterY(int32_t     tx,
                         int32_t     ty,
                         float       dy,
                         Fsr::Pixel& out);
    void bilinearFilterXY(int32_t     tx,
                          float       dx,
                          int32_t     ty,
                          float       dy,
                          Fsr::Pixel& out);
    void bilinearTextureFilter(const Fsr::Vec2f& uv,
                               Fsr::Pixel&       out);

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

inline void
Texture2dSampler::samplePixel(int32_t     tx,
                              int32_t     ty,
                              Fsr::Pixel& out)
{
    const uint32_t nChans = out.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = out.getIdx(i);
        out[z] = (*m_tile)[z][ty][tx];
    }
}

inline void
Texture2dSampler::bilinearFilterX(int32_t     tx,
                                  float       dx,
                                  int32_t     ty,
                                  Fsr::Pixel& out)
{
    const float idx = 1.0f - dx;
    const uint32_t nChans = out.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = out.getIdx(i);
        DD::Image::RawGeneralTile::RowPtr p = (*m_tile)[z][ty] + tx;
        const float x0 = *p++;
        const float x1 = *p;
        out[z] = x0*idx + x1*dx;
    }
}

inline void
Texture2dSampler::bilinearFilterY(int32_t     tx,
                                  int32_t     ty,
                                  float       dy,
                                  Fsr::Pixel& out)
{
    const float idy = 1.0f - dy;
    const uint32_t nChans = out.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = out.getIdx(i);
        const float y1 = (*m_tile)[z][ty+1][tx];
        const float y0 = (*m_tile)[z][ty  ][tx];
        out[z] = y0*idy + y1*dy;
    }
}

inline void
Texture2dSampler::bilinearFilterXY(int32_t     tx,
                                   float       dx,
                                   int32_t     ty,
                                   float       dy,
                                   Fsr::Pixel& out)
{
    const float idx = 1.0f - dx;
    const float idy = 1.0f - dy;
    const uint32_t nChans = out.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = out.getIdx(i);
        DD::Image::RawGeneralTile::RowPtr p1 = (*m_tile)[z][ty+1] + tx;
        DD::Image::RawGeneralTile::RowPtr p0 = (*m_tile)[z][ty  ] + tx;
        const float x0y1 = *p1++;
        const float x1y1 = *p1;
        const float x0y0 = *p0++;
        const float x1y0 = *p0;
        out[z] = (x0y0*idx + x1y0*dx)*idy + (x0y1*idx + x1y1*dx)*dy;
    }
}

inline void
Texture2dSampler::bilinearTextureFilter(const Fsr::Vec2f& uv,
                                        Fsr::Pixel&       out)
{
    const float   cx  = m_offset.x + uv.x*m_scale.x;
    const float   cy  = m_offset.y + uv.y*m_scale.y;
    const float   ftx = floorf(cx + 0.01f);
    const float   fty = floorf(cy + 0.01f);
    const int32_t tx  = int32_t(ftx);
    const int32_t ty  = int32_t(fty);
    if (tx < m_tile->x())
    {
        if (ty < m_tile->y())
            samplePixel(m_tile->x(), m_tile->y(), out); // clamped at left and bottom
        else if (ty < (m_tile->t()-1))
            bilinearFilterY(m_tile->x(), ty, (cy - fty), out); // clamped at left, interpolate Y
        else
            samplePixel(m_tile->x(), m_tile->t()-1, out); // clamped at left and top
    }
    else if (tx < (m_tile->r()-1))
    {
        if (ty < m_tile->y())
            bilinearFilterX(tx, (cx - ftx), m_tile->y(), out); // clamped at bottom, interpolate X
        else if (ty < (m_tile->t()-1))
            bilinearFilterXY(tx, (cx - ftx), ty, (cy - fty),out); // interpolate in X & Y
        else
            bilinearFilterX(tx, (cx - ftx), m_tile->t()-1, out); // clamped at top, interpolate X
    }
    else
    {
        if (ty < m_tile->y())
            samplePixel(m_tile->r()-1, m_tile->y(), out); // clamped at right and bottom
        else if (ty < (m_tile->t()-1))
            bilinearFilterY(m_tile->r()-1, ty, (cy - fty), out); // interpolate in Y
        else
            samplePixel(m_tile->r()-1, m_tile->t()-1, out); // clamped at right and top
    }
}


} // namespace zpr

#endif

// end of zprender/Texture2dSampler.h

//
// Copyright 2020 DreamWorks Animation
//
