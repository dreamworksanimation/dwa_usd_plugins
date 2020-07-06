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

/// @file zprender/Texture2dSampler.cpp
///
/// @author Jonathan Egstad


#include "Texture2dSampler.h"

#include <DDImage/Iop.h>


namespace zpr {


/*! 
*/
Texture2dSampler::Texture2dSampler(DD::Image::Iop*              iop,
                                   const DD::Image::ChannelSet& channels) :
    m_iop(iop),
    m_channels(DD::Image::Mask_None),
    m_tile(NULL),
    m_status(NOT_LOADED),
    m_error(false),
    m_offset(0.0f),
    m_scale(0.0f)
{
    //std::cout << "Texture2dSampler::ctor(" << this << ") iop=" << iop << ", channels=" << channels << std::endl;
    if (iop)
    {
        iop->validate(true);
        iop->request(channels, 1/*count*/);
        m_channels = channels;
        m_channels &= iop->channels();
    }
}


/*! 
*/
Texture2dSampler::~Texture2dSampler()
{
    //std::cout << "Texture2dSampler::dtor(" << this << ")" << std::endl;
    delete m_tile;
}


/*!
*/
bool
Texture2dSampler::buildTile()
{
    if (m_tile)
        return true; // tile's good
    else if (m_error)
        return false; // not a valid tile

    // Loop until the tile is loaded, pausing any other threads trying to
    // sample this same texture:
    while (1)
    {
        if (m_status == LOADED)
            return true;

        if (m_status == NOT_LOADED)
        {
            static DD::Image::Lock load_lock;
            load_lock.lock();

            // Check again to avoid a race condition:
            if (m_status == NOT_LOADED)
            {
                // Lock the object for us to change the status, and that will
                // keep the other threads from trying to build it:
                m_status = LOADING;

                load_lock.unlock(); // let other threads go

                // Build the tile:
                DD::Image::RawGeneralTile* tile = new DD::Image::RawGeneralTile(*m_iop,
                                                                                m_channels,
                                                                                false/*true*//*mthreaded*/,
                                                                                NULL/*ratchet*/);
                if (tile->aborted())
                {
                    // Nuke aborted during tile fill, bail but don't set error:
                    delete tile;
                    m_status = NOT_LOADED;
                    return false;
                }
                else if (!tile->valid())
                {
                    // Nuke built the tile but it's not valid, set error:
                    delete tile;
                    m_status = ERROR;
                    return true;
                }

                m_offset.set(float(tile->x()), float(tile->y()));
                m_scale.set(float(tile->w()), float(tile->h()));

                m_tile = tile;
                m_status = LOADED;

                return true; // all done!

            }
            else
            {
                // Another thread got here first but we have to wait until it's done loading:
                load_lock.unlock();
            }
        }

        // TODO: switch this to a real std::condition_variable mutex test!
        // Pause briefly then try again:
        DD::Image::sleepFor(0.01/*seconds*/);

    } // while loop

    // shouldn't ever get here...
    return false;
}



/*! Replacement for seemingly buggy Filter::apply() method...
*/
inline float
applyFilterToArray(const DD::Image::Filter::Coefficients& filter,
                   const float*                           array)
{
    const float* fp = array + filter.first;
    float weight = *fp++ * filter.array[0];
    for (int32_t i=1; i < filter.count; ++i)
        weight += *fp++ * filter.array[i*filter.delta];
    return weight;
}


/*! Poor man's EWA - calculate the major 'ellipse' axis but fitted
    inside a parallelogram using the filter kernel to approximate
    the ellipse weighting.

    TODO: it may be possible to perform line offsets of the cU/cV
    filter to more closely match the ellipse shape using the
    same DD::Image::Filter mechanisms.
*/
void
Texture2dSampler::sampleFilterered(const Fsr::Vec2f&        uv,
                                   const Fsr::Vec2f&        dUVdx,
                                   const Fsr::Vec2f&        dUVdy,
                                   const DD::Image::Filter* filter,
                                   Fsr::Pixel&              out)
{
    if (!filter || !buildTile())
        return;
#if DEBUG
    assert(m_tile);
#endif


    const Fsr::Vec2f xy(m_offset + uv*m_scale);
    const Fsr::Vec2f dx(dUVdx*m_scale);
    const Fsr::Vec2f dy(dUVdy*m_scale);

    // Calc simple parallelogram that dx/dy ellipse would fit inside.
    // Obviously for rotated thin ellipses this is a very poor
    // approximation...:
    const float ea = (dx.x*dy.y - dy.x*dx.y);
    const float ex = (dx.x*dx.x + dy.x*dy.x);
    const float ey = (dx.y*dx.y + dy.y*dy.y);

    // Normalize xy radius to largest parallelogram side:
    Fsr::Vec2f fRadius;
    if (ex < std::numeric_limits<float>::epsilon() ||
        ey < std::numeric_limits<float>::epsilon())
    {
        fRadius.set(0.0f, 0.0f); // too small, effectively a bilinear filter
    }
    else if (ex >= ey)
    {
        // Wider than tall or round:
        const float radiusX = std::sqrt(ex);
        fRadius.set(radiusX, ::fabsf(ea) / radiusX);
    }
    else
    {
        // Taller than wide:
        const float radiusY = std::sqrt(ey);
        fRadius.set(::fabsf(ea) / radiusY, radiusY);
    }

    // Fill in the U/V filter coefficient weight tables:
    DD::Image::Filter::Coefficients cU, cV;
    filter->get(xy.x, fRadius.x, cU);
    filter->get(xy.y, fRadius.y, cV);

    // Bbox extent of filter:
    const DD::Image::Box fBox(cU.first,
                              cV.first,
                              (cU.first + cU.count),
                              (cV.first + cV.count));

    // Handle filter being partially clipped at edge of tile:
    if (fBox.x() <  m_tile->x() || fBox.y() <  m_tile->y() ||
        fBox.r() >= m_tile->r() || fBox.t() >= m_tile->t())
    {
        // This is used to copy texels along the clamped edge of a tile:
        static float edge_filter[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Filter is clipped on at least one side, check which side:
        if (fBox.x() >= m_tile->r() || fBox.r() <= m_tile->x())
        {
            // X is completely clipped so copy filtered Y from edge:
            cU.array     = edge_filter;
            cU.delta     = 1;
            cU.count     = 1;
            cU.normalize = 1.0f;
        }

        // If Y is completely clipped so copy filtered X from edge:
        if (fBox.y() >= m_tile->t() || fBox.t() <= m_tile->y())
        {
            cV.array     = edge_filter;
            cV.delta     = 1;
            cV.count     = 1;
            cV.normalize = 1.0f;
        }

        // Clamped filter loop:
        const float normalize_factor = (cU.normalize*cV.normalize);
        const uint32_t nChans = out.getNumChans();
        for (uint32_t i=0; i < nChans; ++i)
        {
            const DD::Image::Channel z = out.getIdx(i);

            float weight = 0.0f;
            for (int32_t y=0; y < cV.count; ++y)
            {
                const float* fp = (*m_tile)[z][m_tile->clampy(fBox.y()+y)];
                float xweight = 0.0f;
                for (int32_t x=0; x < cU.count; ++x)
                    xweight += fp[m_tile->clampx(fBox.x()+x)]*cU.array[x*cU.delta];

                weight += xweight*cV.array[y*cV.delta];
            }
            out[z] = weight*normalize_factor;
        }

    }
    else
    {
        // Filter is unclipped on tile, iterate over entire ellipse rectangle:
        const float normalize_factor = (cU.normalize*cV.normalize);
        const uint32_t nChans = out.getNumChans();
        for (uint32_t i=0; i < nChans; ++i)
        {
            const DD::Image::Channel z = out.getIdx(i);

            float weight = 0.0f;
            for (int32_t y=0; y < cV.count; ++y)
                weight += applyFilterToArray(cU, (*m_tile)[z][fBox.y()+y]) * cV.array[y*cV.delta];

            out[z] = weight*normalize_factor;
        }
    }

}


} // namespace zpr


// end of zprender/Texture2dSampler.cpp

//
// Copyright 2020 DreamWorks Animation
//
