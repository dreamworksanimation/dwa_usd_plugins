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

/// @file zpTracer.cpp
///
/// @author Jonathan Egstad


#include "zpRender.h"

#include <zprender/RenderContext.h>
#include <zprender/ThreadContext.h>
#include <zprender/RayMaterial.h>
#include <zprender/RayShaderContext.h>

#include <Fuser/NukeGeoInterface.h> // for hasObjectAttrib(), hasObjectString(), etc.

#include <DDImage/Application.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/noise.h>
#include <DDImage/PrimitiveContext.h>
#include <DDImage/Row.h>
#include <DDImage/Thread.h>  // for Lock
#include <DDImage/Tile.h>    // for InterestRatchet

static DD::Image::Lock my_lock;


namespace zpr {


//----------------------------------------------------------------------------

// TODO: switch to OpenDCX lib for this stuff?
#if 0
#  include "DeepPixelHandler.h" // for SpMask
#else
namespace Dcx {
union SpMaskFloatUnion
{
    SpMask8 as_mask;
    float   as_float[2];
};

typedef uint32_t DeepSampleFlag;
static const DeepSampleFlag DEEP_EMPTY_FLAG           = 0x00000000;  //!< Empty flag
static const DeepSampleFlag DEEP_LINEAR_INTERP_SAMPLE = 0x00000001;  //!< Linear surface sample interpolation (not volumetric)
static const DeepSampleFlag DEEP_MATTE_OBJECT_SAMPLE  = 0x00000002;  //!< Matte sample that cuts-out (blackens) other samples
static const DeepSampleFlag DEEP_ADDITIVE_SAMPLE      = 0x00000004;  //!< Additive sample which plusses with adjacent additive samples
static const DeepSampleFlag DEEP_RESERVED_FLAG1       = 0x00000008;  //!< Placeholder
static const DeepSampleFlag DEEP_RESERVED_FLAG2       = 0x00000010;  //!< Placeholder
static const DeepSampleFlag DEEP_RESERVED_FLAG3       = 0x00000020;  //!< Placeholder

enum { SPMASK_OFF, SPMASK_AUTO, SPMASK_4x4, SPMASK_8x8 };
enum { INTERP_OFF, INTERP_AUTO, INTERP_LOG, INTERP_LIN };


//! Split an 8x8 subpixel mask into 2 floats.
inline void mask8x8ToFloats(const SpMask8& spmask,
                            float&         sp0,
                            float&         sp1)
{
    SpMaskFloatUnion mask_union;
    mask_union.as_mask = spmask;
    sp0 = mask_union.as_float[0];
    sp1 = mask_union.as_float[1];
}
}
#endif

//----------------------------------------------------------------------------


/*!
*/
int
zpRender::getRaySampleSideCount(int mode) const
{
    switch (mode)
    {
        default:
        case RenderContext::SAMPLING_1x1:   return 1;
        case RenderContext::SAMPLING_2x2:   return 2;
        case RenderContext::SAMPLING_3x3:   return 3;
        case RenderContext::SAMPLING_4x4:   return 4;
        case RenderContext::SAMPLING_5x5:   return 5;
        case RenderContext::SAMPLING_8x8:   return 8;
        case RenderContext::SAMPLING_12x12: return 12;
        case RenderContext::SAMPLING_16x16: return 16;
        case RenderContext::SAMPLING_32x32: return 32;
        case RenderContext::SAMPLING_64x64: return 64;
    }
}


//----------------------------------------------------------------------------


#if 0
/*! Construct a coordinate frame from a normal
*/
inline DD::Image::Axis
frame(const Fsr::Vec3f& N)
{
    Fsr::Vec3f dx0 = cross(Fsr::Vec3f(1.0f, 0.0f, 0.0f), N);
    Fsr::Vec3f dx1 = cross(Fsr::Vec3f(0.0f, 1.0f, 0.0f), N);
    Fsr::Vec3f dx = normalize(select(dot(dx0, dx0) > dot(dx1, dx1), dx0, dx1));
    Fsr::Vec3f dy = normalize(cross(N, dx));
    return DD::Image::Axis(dx, dy, N);
}
#endif


//----------------------------------------------------------------------------


//inline T sin2cos(float x) { return sqrt(std::max((T)0, (T)1 - x*x)); }
//inline T cos2sin(float x) { return sin2cos(x); }


/*! Uniform hemisphere sampling. Up direction is the z direction.
*/
inline Fsr::Vec3f
uniformSampleHemisphere(float u,
                        float v)
{
    const float phi = u * float(M_PI*2.0);
    //const float cosTheta = v;
    const float sin_theta = sqrtf(std::max(0.0f, 1.0f - (v*v)));//cos2sin(v);
    return Fsr::Vec3f(cosf(phi)*sin_theta, sinf(phi)*sin_theta, v);
}


/*! Cosine weighted hemisphere sampling. Up direction is the z direction.
*/
inline Fsr::Vec3f
cosineSampleHemisphere(float u,
                       float v)
{
    const float phi = u * float(M_PI*2.0);
    const float cos_theta = sqrtf(v);
    const float sin_theta = sqrtf(1.0f - v);
    return Fsr::Vec3f(cosf(phi)*sin_theta, sinf(phi)*sin_theta, cos_theta);
}


//----------------------------------------------------------------------------


//-------------------------------------------------------------------------------------


/*! ========================================================================================
    ========================================================================================
    ==                            RAYTRACE ENGINE                                         ==
    ========================================================================================
    ========================================================================================
*/
bool
zpRender::tracerEngine(int y, int t, int x, int r,
                       DD::Image::ChannelMask      out_channels,
                       DD::Image::Row&             out_row,
                       DD::Image::DeepOutputPlane* deep_out_plane)
{
#ifdef DEBUG_ENGINE
    std::cout << y << "-" << t << " zpRender::tracerEngine(" << pthread_self() << ") " << x << "-" << r;
    std::cout << ", out_channels=" << out_channels;
    std::cout << ", numCPUs=" << DD::Image::Thread::numCPUs;
    std::cout << ", numThreads=" << DD::Image::Thread::numThreads;
    std::cout << std::endl;
#endif

    //-----------------------------------------------------------------
    // The firt thread to get here calls generate_render_primitives():
    if (!rtx.objects_initialized)
    {
        my_lock.lock();
        // Check again to avoid a race condition:
        if (!rtx.objects_initialized)
        {
#ifdef DEBUG_ENGINE
            std::cout << "   thread " << pthread_self() << ": call generate_render_primitives()" << std::endl;
#endif
            if (!generate_render_primitives())
            {
                // Bail fast on user abort:
                my_lock.unlock();
                return false;
            }

            // Initialize filter if not done already:
            texture_filter_.initialize();
            k_texture_filter_preview.initialize();
        }
        my_lock.unlock();
    }


    //-----------------------------------------------------------------
    // Get the ThreadContext object, creating it if this is the
    // first time the thread's been used:
    ThreadContext* thread_ctx = NULL;
    {
        my_lock.lock();

        const pthread_t tID = pthread_self();
        std::map<pthread_t, uint32_t>::iterator thread_it = rtx.thread_map.find(tID);
        if (thread_it == rtx.thread_map.end())
        {
            // Thread doesn't exist yet, create it:
            rtx.thread_list.push_back(new ThreadContext(&rtx));
            const uint32_t tindex = (uint32_t)rtx.thread_list.size()-1;
            ThreadContext* rttx = rtx.thread_list[tindex];
            rttx->setThreadID(tindex, tID);

            // Add it to the map and update the iterator:
            rtx.thread_map[tID] = tindex;
            thread_it = rtx.thread_map.find(tID);
            assert(thread_it != rtx.thread_map.end()); // shouldn't happen...
#ifdef DEBUG_ENGINE
            std::cout << "    adding thread context " << tindex << " for thread ID " << tID << std::endl;
#endif
        }
        my_lock.unlock();

        // This is the ThreadContext for this thread:
        assert(thread_it->second < rtx.thread_list.size());
        thread_ctx = rtx.thread_list[thread_it->second];
    }
    assert(thread_ctx);


    //-----------------------------------------------------------------
    // These are the channels shaders must calculate:
    DD::Image::ChannelSet shader_channels = rtx.material_channels;//out_channels;
    // Shaders calcuate Z without needing the Z bit on in the mask
    // because color channels are mixed, but not Z channels.
    // Make sure Z channel is off for the shaders:
    shader_channels -= (DD::Image::Mask_Z);
    shader_channels += DD::Image::Mask_Alpha;

    // These are the channels we copy from our background input:
    //ChannelSet bg_get_channels(out_channels);
    DD::Image::ChannelSet bg_get_channels(input0().channels());
    // Need Z if we're clipping:
    if (k_bg_occlusion)
        bg_get_channels += DD::Image::Mask_Z;
    // Need alpha if we're blending:
    if (rtx.k_atmosphere_alpha_blending)
        bg_get_channels += DD::Image::Mask_Alpha;
    // Need mask channel if masking:
    if (k_render_mask_channel != DD::Image::Chan_Black)
        bg_get_channels += k_render_mask_channel;

    // These are the channels we copy from our render to the output row:
    DD::Image::ChannelSet render_out_channels = out_channels;
    render_out_channels &= rtx.render_channels;

    const bool write_out_Z = (render_out_channels.contains(DD::Image::Chan_Z));

#ifdef DEBUG_ENGINE
    std::cout << "  bg_get_channels=" << std::hex << bg_get_channels.value();
    std::cout << ", render_channels=" << std::hex << rtx.render_channels.value();
    std::cout << ", shader_channels=" << std::hex << shader_channels.value();
    std::cout << ", render_out_channels=" << std::hex << render_out_channels.value();
    std::cout << std::dec << std::endl;
#endif


    //-----------------------------------------------------------------
    // Get background input row:
    DD::Image::Row in_row(x, r);
    in_row.get(input0(), y, x, r, bg_get_channels);

    if (Op::aborted()) {
#ifdef DEBUG_ABORTED
        std::cout << "    ******** line " << y << " engine aborted ********" << std::endl;         
#endif
        return false;
    }


    //-----------------------------------------------------------------
    // Are we outputing flat or deep data?
    // 
#ifdef ENABLE_DEEP
    const bool flat_output_mode = (deep_out_plane == NULL);
#else
    const bool flat_output_mode = true;
#endif
    if (flat_output_mode)
    {
        // Copy input row to output, and clear any
        // channels not actually in the input set:
        foreach (z, out_channels)
        {
            float* OUT = out_row.writable(z)+x;
            if (input0().channels().contains(z))
            {
                // Copy the row if it's actually from input0:
                const float* IN = in_row[z]+x;
                memcpy(OUT, IN, (r-x)*sizeof(float));
            }
            else
            {
                // Clear the row:
                if (z == DD::Image::Chan_Z && !k_one_over_Z)
                {
                    // Set Z to infinity...:
                    for (int xx=x; xx < r; ++xx)
                        *OUT++ = std::numeric_limits<float>::infinity();
                }
                else
                {
                    // Don't use Row::erase() as we want to fill the memory...:
                    memset(OUT, 0, (r-x)*sizeof(float));
                }
            }
        }

        // If this line is outside the rendering bbox bail quick:
        if (y < rtx.render_region.y() || y >= rtx.render_region.t())
            return true;
    }

    zpr::Scene* scene0 = rtx.shutter_scenerefs[0].scene;
    assert(scene0); // Shouldn't be null!

    // Bail if scene has no objects, or lights in atmospheric mode...:
    if (scene0->objects()==0 && (rtx.atmospheric_lighting_enabled && scene0->lights.size()==0))
        return true;

    rtx.updateLightingScenes(scene0/*ref_scene*/, *thread_ctx);


    //-----------------------------------------------------------------
    // Sampler set to use:
    // 
    int randomZ = int(rtx.render_frame*100.0) + rtx.render_view;//int(Op::hash().value());
    const StochasticSampleSetList& sampler_set = m_sampler_set->getSampleSet(0/*randomZ%m_sampler_set->m_set_count*/);
    uint32_t nSamples = (uint32_t)sampler_set.size();
    //std::cout << " nSamples=" << nSamples << std::endl;

    //-----------------------------------------------------------------
    // Shutter info:
    //
    const uint32_t nShutterSamples = rtx.numShutterSamples();
    const uint32_t nShutterSteps   = rtx.numShutterSteps();
    const float    f_motion_samples_minus_one = float(nShutterSamples - 1);
    const float    f_msds                     = 1.0f / f_motion_samples_minus_one;
    const double   shutter_open_time  = rtx.getShutterOpenTime();
    const double   shutter_close_time = rtx.getShutterCloseTime();


    //-----------------------------------------------------------------
    // Color/AOV storage:
    //
    DD::Image::ChannelSet bg_render_channels = bg_get_channels;
    bg_render_channels += rtx.render_channels;

    Fsr::Pixel bg(bg_render_channels);        // Background color
    Fsr::Pixel Rcolor(rtx.render_channels);   // Final combined color
    Fsr::Pixel Raccum(rtx.render_channels);   // Accumulated ray color


    DD::Image::TextureFilter* shading_texture_filter = NULL;
    if (!rtx.k_preview_mode)
    {
        shading_texture_filter = &texture_filter_;
    }
    else
    {
        shading_texture_filter = &k_texture_filter_preview;
    }


    //-----------------------------------------------------------------
    // Get pixel filter coefficient table:
    // 
    const uint32_t pf_filter_width = 16;
    DD::Image::Filter::Coefficients pf_cU;
    m_pixel_filter.get(0.0f, pf_filter_width, pf_cU);
    //std::cout << "px=" << rtx.ray_single_scatter_samples << " count=" << pf_cU.count << std::endl;
    //for (int i=0; i < pf_cU.count; ++i)
    //   std::cout << pf_cU.array[pf_cU.delta*i] << " ";
    //std::cout << std::endl;

    bool use_shutter_bias = (fabs(rtx.k_shutter_bias) > std::numeric_limits<float>::epsilon());

    // This scales the ray differentials for camera rays:
    const float inv_nSamples = 1.0f / float(nSamples);

    // Pixel-filter scaling factors:
    const float pf_scale_x = fabsf(rtx.k_pixel_filter_size[0]);
    const float pf_scale_y = fabsf(rtx.k_pixel_filter_size[1]);
    const float pf_bin_scale = float(pf_cU.count) / 2.0f;
    bool enable_pixel_filter = (!m_pixel_filter.impulse() && (nSamples > 1));
    std::vector<float> pf_weights(nSamples);
    if (enable_pixel_filter)
    {
        float* p = &pf_weights[0];
        float norm = 0.0f;
        for (uint32_t i=0; i < nSamples; ++i, ++p)
        {
            const float t = (1.0f - sampler_set[i].subpixel.radius)*pf_bin_scale;
            const int pf_bin = int(fast_floor(t));
            const float dt = t - float(pf_bin);
            const float w = ::lerp(pf_cU.array[pf_cU.delta * pf_bin], pf_cU.array[pf_cU.delta*(pf_bin + 1)], dt);
            pf_weights[i] = w;
            //std::cout << i << " r=" << jp->r << " bin=" << pf_bin << " dt=" << dt << " wt=" << *p << " count=" << pf_cU.count << std::endl;
            norm += w;
        }

        // Apply normalization:
        norm = 1.0f / (norm / float(nSamples));
        for (uint32_t i=0; i < nSamples; ++i)
            pf_weights[i] *= norm;
    }


    // Camera ray clipping plane overrides:
    const double camera_near_plane_override = fabs(std::min(k_ray_near_plane, k_ray_far_plane));
    const double camera_far_plane_override  = fabs(std::max(k_ray_near_plane, k_ray_far_plane));


#ifndef TRY_UV_MODE
    const bool uv_mode = false;
#else
    const bool uv_mode = (projection_mode_ == DD::Image::CameraOp::LENS_UV);

    UVSegmentIntersectionList obj_uv_intersections;
    uint32_t nUVIntersections = 0;
    Fsr::Vec2f prevUVIntersectionST;
    if (uv_mode)
    {
        //------------------------------------------------------
        // UV mode - we intersect this scanline with
        // all object BVHS
        //------------------------------------------------------

        thread_ctx->clearShaderContexts();
        zpr::RayShaderContext& stx = rtx.startShaderContext(thread_ctx->index());

        UVSegmentIntersectionList uv_intersections;
        uv_intersections.reserve(20);

        float V = float(y) / float(format().height()-1);
        // Offset V to requested tile:
        V += float(std::max(0, k_uv_mode_tile_index[1]));
        float u_offset = float(std::max(0, k_uv_mode_tile_index[0]));
        Fsr::Vec2f uv0(u_offset - std::numeric_limits<float>::epsilon(), V); // Bias
        Fsr::Vec2f uv1(u_offset + 1.0f + std::numeric_limits<float>::epsilon(), V);
        rtx.objects_bvh.getIntersectionsWithUVs(stx, uv0, uv1, uv_intersections);

        nUVIntersections = (uint32_t)uv_intersections.size();
        if (y == 117)
            std::cout << "line " << y << ": nUVIntersections=" << nUVIntersections << ":" << std::endl;

        // Cull out only the ones for the object we're targeting:
        obj_uv_intersections.reserve(uv_intersections.size());
        for (uint32_t i=0; i < nUVIntersections; ++i)
        {
            UVSegmentIntersection& I = uv_intersections[i];
            assert(I.object);
            const RenderPrimitive* rprim = (RenderPrimitive*)I.object;

            // Skip ones not in the target object:
            const int obj  = rprim->surface_ctx->getObjectIndex();
            const int prim = rprim->surface_ctx->getPrimIndex();
            if ((k_uv_mode_object_index  > -1 && obj != k_uv_mode_object_index ) ||
                (k_uv_mode_surface_index > -1 && prim != k_uv_mode_surface_index))
                continue;

            // Skip ones not in the target tile:
            //if (U < I.uv0.x || U > I.uv1.x)
            //   continue;

            obj_uv_intersections.push_back(I);
#if 1
            if (y == 117 && i < 2)
            {
                std::cout << "  " << i << " Intersect " << rprim;
                std::cout << " uv0[" << uv0.x << " " << uv0.y << "]";
                std::cout << " uv1[" << uv1.x << " " << uv1.y << "]";
                //std::cout << " I.uv0[" << I.uv0.x << " " << I.uv0.y << "]";
                //std::cout << " I.uv1[" << I.uv1.x << " " << I.uv1.y << "]";
                std::cout << " I.st0[" << I.st0.x << " " << I.st0.y << "]";
                std::cout << " I.st1[" << I.st1.x << " " << I.st1.y << "]";
                std::cout << std::endl;
            }
#endif
        }
        nUVIntersections = (uint32_t)obj_uv_intersections.size();
    }
#endif

    assert(rtx.ray_cameras[0]);
    const RayCamera& rcam0 = *rtx.ray_cameras[0];

#ifdef ENABLE_DEEP
    // x-r pixel loop:
    if (!flat_output_mode)
    {
        //std::cout << "deep_bbox[" << x << " " << y << " " << r << " " << t << "], out_channel=" << out_channels << std::endl;         
        *deep_out_plane = DD::Image::DeepOutputPlane(out_channels,
                                                     DD::Image::Box(x, y, r, t)/*, DD::Image::DeepPixel::eZAscending*/);
    }
#endif
    DD::Image::ChannelSet deep_color_channels(rtx.render_channels);
    deep_color_channels -= DD::Image::Chan_Z;
    deep_color_channels -= DD::Image::Mask_Deep;


    // List and map of Pixels for all samples:
    Traceable::DeepIntersectionList deep_accum_list; // the accumulated list for the whole pixel
    deep_accum_list.reserve(nSamples*10);
    Traceable::DeepIntersectionList m_deep_intersection_list, m_deep_static_intersection_list; // the list send to the shading pipe
    m_deep_intersection_list.reserve(20);
    m_deep_static_intersection_list.reserve(20);
    Traceable::DeepIntersectionMap deep_intersection_map;

    zpr::VolumeShader* ambient_volume = rtx.atmospheric_lighting_enabled ? &k_ambient_volume : NULL;

    //-----------------------------------------------------------------
    // Pixel region loops
    //
    Fsr::Vec2d fXY;     // float version of current x/y pixel coord
    Fsr::Vec2d fUV;     // normalized fXY in -0.5...+0.5 range, where 0,0 is nominal lens center
    double     fRadius; // distance from nominal lens center to fUV, for perspective compensation
    for (int yy=y; yy < t; ++yy)
    {
        //std::cout << "y=" << yy << std::endl;
        if (yy < rtx.render_region.y() || yy >= rtx.render_region.t())
        {
            // In deep mode fill the empty output line with holes:
            if (deep_out_plane)
            {
                //std::cout << "skip line " << yy << std::endl;
                for (int xx=x; xx < r; ++xx)
                    deep_out_plane->addHole();
            }
            continue;
        }

        fXY.y = double(yy) + 0.5;

        for (int xx=x; xx < r; ++xx)
        {
            //std::cout << "y=" << yy << " x=" << xx << std::endl;

            if (xx < rtx.render_region.x() || xx >= rtx.render_region.r())
            {
                // In deep mode fill empty output pixels with holes:
                if (deep_out_plane)
                    deep_out_plane->addHole();
                continue;
            }

            // Keep this in the outer-loop:
            if (DD::Image::Op::aborted())
            {
#ifdef DEBUG_ABORTED
                std::cout << "    ******** pixel[" << xx << " " << yy << "] engine aborted ********" << std::endl;         
#endif
                return false;
            }

            //-----------------------------------------------------------------
            //
            fXY.x = double(xx) + 0.5;
            rcam0.pixelXYToScreenWindowXY(fXY, fUV); // returns fUV in range of -0.5..+0.5
            fUV.y *= rcam0.apertureAspect(); // <<< TODO: is this still required...?
            // fRadius is only used for perspective compensation:
            fRadius = (projection_mode_ == DD::Image::CameraOp::LENS_PERSPECTIVE) ? fUV.length() : 0.0;


            //-----------------------------------------------------------------
            // Update bg Pixel contents from bg input:
            //
            bg.clearAllChannels();
            foreach (z, bg_get_channels)
                bg[z] = in_row[z][xx];

            // Update bg Z:
            if (m_have_bg_Z)
            {
                float& z = bg.Z();
                if (k_one_over_Z)
                {
                    if (isnan(z))
                        z = std::numeric_limits<float>::infinity();
                    else
                        z = (z > std::numeric_limits<float>::epsilon()) ?
                                1.0f/z :
                                std::numeric_limits<float>::infinity();
                }
                else
                {
                    if (isnan(z))
                        z = std::numeric_limits<float>::infinity();
                }
                if (projection_mode_ == DD::Image::CameraOp::LENS_PERSPECTIVE)
                {
                    // Perspective correct input Z?
                    if (k_persp_correct_z)
                        z /= float(::cos(::atan(fRadius*rcam0.lensMagnification() / 2.0))); // scale Z
                }
            }


            //-----------------------------------------------------------------
            // Handle per-pixel render masking:
            //
            if (k_render_mask_channel != DD::Image::Chan_Black)
            {
                float mask = bg[k_render_mask_channel];
                if (k_invert_render_mask)
                    mask = 1.0f - mask;

                if (mask < k_render_mask_threshold)
                {
                    // In deep mode we fill empty output pixels with holes:
                    if (deep_out_plane)
                        deep_out_plane->addHole();
                    continue;
                }
            }


            //-----------------------------------------------------------------
            // Clear output value accumulators:
            //
            Raccum.clearAllChannels();
    
            float coverage = 0.0f;
            float accum_Z = std::numeric_limits<float>::infinity();

            deep_accum_list.clear();
            deep_intersection_map.clear();


            //-----------------------------------------------------------------
            // Time of the sample, between shutter open & shutter close:
            //
            float shutter_t      = 0.0f; // default to shutter open
            float shutter_step_t = 0.0f;


            //-----------------------------------------------------------------
            // Get a sampling set for this pixel.
            // To save time we use a fake randomization offset - TODO this should be improved!!!

            // p_random is very slowww....so we're going to only calculate it *once* per pixel
            // then further randomize at each subsample with the Sampler:
            const double randomization_offset = DD::Image::p_random(xx, y, randomZ);

            // getSampleSet() does a modulus with the total set count:
            const StochasticSampleSetList& subpixel_sampling_set =
                m_sampler_set->getSampleSet(int(fast_floor(randomization_offset*1234567.123)));


            //-----------------------------------------------------------------
            // Sampling loop! This is where the magic happens...
            //

            for (uint32_t sample_index=0; sample_index < nSamples; ++sample_index)
            {
                //std::cout << xx << "," << y << ": sample " << sample_index << std::endl;

                //-----------------------------------------------------------------
                // Initialize the first shader context as the camera ray:
                thread_ctx->clearShaderContexts();
                zpr::RayShaderContext& stx = rtx.startShaderContext(thread_ctx->index());

                //---------------------------------------------------------------------------------------------
                // TODO: Move these assignments to a better spot...?

                // TODO: figure out best sample (center-most) to volume sample on:
                stx.atmosphere_shader = ambient_volume;//(sample_index == 0) ? ambient_volume : NULL;
                //
                stx.texture_filter   = shading_texture_filter;
                stx.cutout_channel   = k_cutout_channel;
                //---------------------------------------------------------------------------------------------
                stx.master_light_shaders       = NULL;
                stx.per_object_light_shaders   = NULL;
                // legacy lighting:
                stx.master_lighting_scene      = &thread_ctx->masterLightingScene();
                stx.per_object_lighting_scenes = &thread_ctx->perObjectLightingSceneList();
                //---------------------------------------------------------------------------------------------
                stx.depth            = 0;
                stx.diffuse_depth    = 0;
                stx.glossy_depth     = 0;
                stx.reflection_depth = 0;
                stx.refraction_depth = 0;
                //---------------------------------------------------------------------------------------------

                //-------------------------------------------------
                // Jittered screen location:
                stx.sampler = &subpixel_sampling_set[sample_index];
                const Fsr::Vec2d fsXY(fXY.x + (stx.sampler->subpixel.dp.x*pf_scale_x),
                                      fXY.y + (stx.sampler->subpixel.dp.y*pf_scale_y));

                stx.x  = xx;     // Current output X screen coord
                stx.y  = yy;     // Current output Y screen coord
                stx.sx = fsXY.x; // Current output X subpixel screen coord
                stx.sy = fsXY.y; // Current output Y subpixel screen coord
                stx.si = sample_index; // Current subsample ray index

                //-----------------------------------------------------------------
                // Jittered lens location:
                const Fsr::Vec2d lensUV((rtx.k_dof_enabled)?stx.sampler->lens.x:0.0f,
                                        (rtx.k_dof_enabled)?stx.sampler->lens.y:0.0f);


                //-----------------------------------------------------------------
                // Get frame & shutter times:
                //
                // Further randomize shutter time at each sample:
                if (use_shutter_bias)
                {
                    // Biased shutter needs a much more random distribution - take
                    // the p_random hit on every sample...:
                    shutter_t = float(DD::Image::p_random(xx, y, randomZ+sample_index)); // returns a shutter value between 0-1
                    // Weight the shutter time by the shutter bias:
                    if (rtx.k_shutter_bias > 0.0)
                    {
                        // Bias towards shutter-close:
                        shutter_t = ::powf(shutter_t, float(1.0 / rtx.k_shutter_bias + 1.0));
                    }
                    else if (rtx.k_shutter_bias < 0.0)
                    {
                        // Bias towards shutter-open:
                        shutter_t = 1.0f - ::powf(1.0f - shutter_t, float(1.0 / -rtx.k_shutter_bias + 1.0));
                    }
                }
                else
                {
                    // Fast version jitters shutter time with fixed-array:
                    shutter_t = clamp(stx.sampler->time, 0.0f, 1.0f);
                }

                // Set absolute frame time:
                if      (shutter_t <= 0.0f)
                    stx.frame_time = shutter_open_time;
                else if (shutter_t < 1.0f)
                    stx.frame_time = ::lerp(shutter_open_time, shutter_close_time, shutter_t);
                else
                    stx.frame_time = shutter_close_time;

                // Shutter time offset from frame 0:
                stx.frame_time_offset = (stx.frame_time - rtx.frame0);
                if (fabs(stx.frame_time_offset) < std::numeric_limits<double>::epsilon())
                    stx.frame_time_offset = 0.0;

                // Find the shutter step for this time sample:
                if (nShutterSteps == 0)
                {
                    stx.frame_shutter_step = 0;
                    stx.mb_enabled = false;
                }
                else if (nShutterSteps == 1)
                {
                    // Only one sample:
                    stx.frame_shutter_step = 0;
                    shutter_step_t = shutter_t;
                    stx.mb_enabled = true;
                }
                else
                {
                    // Find the motion step index:
                    stx.frame_shutter_step = int(fast_floor(shutter_t*0.99f * f_motion_samples_minus_one));
                    // Offset & scale the shutter time within the motion-step's range:
                    shutter_step_t = clamp((shutter_t - (float(stx.frame_shutter_step)*f_msds)) * f_motion_samples_minus_one);
                    stx.mb_enabled = true;
                }

                // Output shutter info for diagnostics if desired:
                if (rtx.k_show_diagnostics == RenderContext::DIAG_TIME)
                {
                    if ((int)sample_index == rtx.k_diagnostics_sample)
                    {
                        Raccum.color().set(float(stx.frame_time),
                                           float(stx.frame_time_offset),
                                           float(stx.frame_shutter_step));
                        coverage = 1.0f;
                        break; // done!
                    }
                    continue; // next sample!
                }


                // Update start of trace time if we are tracking elapsed time:
                if (rtx.k_show_diagnostics > RenderContext::DIAG_RENDER_TIME)
                    gettimeofday(&stx.start_time, 0);


                //-----------------------------------------------------------------
                // Handle different projection modes
                // TODO: this needs to be abstracted so we can call lens shaders!
                //
                if (!uv_mode)
                {
                    //-----------------------------------------------------------------
                    //-----------------------------------------------------------------
                    // LENS MODE
                    // Lens projections like perspective, orthographic, spherical,
                    // cylindrical, fisheye, etc.
                    // TODO: this needs to be abstracted so we can call lens shaders!

                    //-----------------------------------------------------------------
                    // Construct camera ray & its differentials:
#if DEBUG
                    assert(rtx.ray_cameras[stx.frame_shutter_step]);
#endif
                    const RayCamera& rcam = *rtx.ray_cameras[stx.frame_shutter_step];

                    if (!stx.texture_filter)
                    {
                        // No differentials since we don't need to filter:
                        // TODO: this is not strictly true as shaders that don't
                        // do texture filtering may still need differential, we
                        // should clarify texture diffs vs. shader diffs.
                        rcam.constructRay(fsXY/*pixelXY*/,
                                          lensUV/*lensDuDv*/,
                                          shutter_step_t/*shutter_percentage*/,
                                          stx.Rtx);
                        stx.use_differentials = false;
                    }
                    else
                    {
                        // Construct differentials, scaled by the inverse sample count
                        // so that more ray samples per pixel make the differential
                        // cone smaller:
                        rcam.constructRay(fsXY/*pixelXY*/,
                                          Fsr::Vec2d(inv_nSamples, inv_nSamples)/*pixelDxDy*/,
                                          lensUV/*lensDuDv*/,
                                          shutter_step_t/*shutter_percentage*/,
                                          stx.Rtx,
                                          stx.Rdif);
                        stx.use_differentials = true;
                    }

                    // Do we need the hero ray direction?
                    // TODO: move this logic to RayCamera class?
                    stx.use_heroV_for_spec = (rtx.hero_ray_cameras.size() > 0);
                    if (stx.use_heroV_for_spec)
                    {
                        // Get hero ray direction:
                        assert(rtx.hero_ray_cameras[stx.frame_shutter_step]);
                        const RayCamera& heroRcam = *rtx.hero_ray_cameras[stx.frame_shutter_step];
                        Fsr::RayContext heroR;
                        heroRcam.constructRay(fsXY,
                                              Fsr::Vec2d(0.0, 0.0)/*lensUV*/,
                                              shutter_step_t/*shutter_percentage*/,
                                              heroR);
                        stx.heroCamOrigin = heroR.origin;
                    }
                    else
                    {
                        // Default hero ray to same as primary R:
                        stx.heroCamOrigin = stx.Rtx.origin;
                    }

                    // Possibly override the primary ray clipping planes assigned by constructRay():
                    if (!k_ray_use_camera_near_plane)
                        stx.Rtx.mindist = camera_near_plane_override;
                    if (!k_ray_use_camera_far_plane)
                        stx.Rtx.maxdist = camera_far_plane_override;

                    // Clamp ray maxdist to bg z if desired:
                    if (m_have_bg_Z && k_bg_occlusion)
                        stx.Rtx.maxdist = std::min(double(bg.Z()) + std::numeric_limits<double>::epsilon(), stx.Rtx.maxdist);


                    //-----------------------------------------------------------------
                    // Interpolate LightContext vectors if lighting enabled:
                    // TODO: rework this to use Fuser Light classes
                    //
                    if (rtx.direct_lighting_enabled)
                    {
                        if (nShutterSteps > 0)
                        {
                            rtx.updateLightingSceneVectorsTo(stx.frame_shutter_step,
                                                             shutter_step_t,
                                                             &thread_ctx->masterLightingScene());
                        }
                        stx.master_light_shaders       = &rtx.master_light_shaders;
                        stx.per_object_light_shaders   = &rtx.per_object_light_shaders;
                        // legacy lighting:
                        stx.master_lighting_scene      = &thread_ctx->masterLightingScene();
                        stx.per_object_lighting_scenes = &thread_ctx->perObjectLightingSceneList();
                    }
                    else
                    {
                        stx.master_light_shaders       = NULL;
                        stx.per_object_light_shaders   = NULL;
                        // legacy lighting:
                        stx.master_lighting_scene      = NULL;
                        stx.per_object_lighting_scenes = NULL;
                    }


                    //-----------------------------------------------------------------
                    // Trace the primary camera ray:
                    //


                    //-----------------------------------------------------------------
                    // Hard surfaces:
                    //
                    // Final Z and cutout status for surface sample:
                    float surface_Zf    =  std::numeric_limits<float>::infinity();
                    float surface_Zb    = -std::numeric_limits<float>::infinity();
#ifdef ENABLE_VOLUME_LIGHTING
                    float surface_alpha = 0.0f;
#endif


                    if (flat_output_mode)
                    {
                        //======================================================================================
                        //======================================================================================
                        // FLAT:
                        //

                        //=========================================================
                        //=========================================================
                        RayMaterial::getIllumination(stx, Rcolor, NULL/*deep_out*/);
                        //=========================================================
                        //=========================================================

                        // Note - final cutout alpha is still in Chan_Cutout_Alpha!
                        // This is moved to Chan_Alpha after final cutout handling is done.

                        // Final Z for surface sample:
                        surface_Zf = Rcolor.Z();
                        surface_Zb = surface_Zf;


                        //-----------------------------------------------------------------
                        // Overlay some Bvh diagnostic info if desired:
                        //
                        if (rtx.k_show_diagnostics == RenderContext::DIAG_BOUNDS)
                        {
                            // Object intersection depth is shoved into green channel:
                            int level = rtx.objects_bvh.intersectLevel(stx, -1/*level*/, rtx.k_diagnostics_sample/*max_level*/);
                            if (level >= 0)
                            {
                                const float lf = float(1+level) / float(2+std::max(0, rtx.k_diagnostics_sample));
                                Raccum.g() = std::max(Raccum.g(), powf(lf, 1.0f/0.3f));

                            }

                            // Light volume intersection depth is shoved into red channel:
                            if (rtx.atmospheric_lighting_enabled)
                            {
                                level = rtx.lights_bvh.intersectLevel(stx, -1/*level*/, rtx.k_diagnostics_sample/*max_level*/);
                                if (level >= 0)
                                {
                                    const float lf = float(1+level) / float(1+std::max(0, rtx.k_diagnostics_sample));
                                    Raccum.r() = std::max(Raccum.r(), powf(lf, 1.0f/0.3f));
                                }
                            }
                        }
                        //
                        // FLAT
                        //======================================================================================

                    }
                    else
                    {
                        //======================================================================================
                        //======================================================================================
                        // DEEP:
                        //
                        // Final color for surfaces:
                        m_deep_intersection_list.clear();
                        RayMaterial::getIllumination(stx, Rcolor, &m_deep_intersection_list);

                        // Collapse like-object shader fragments together:
                        const uint32_t nDeepIntersections = (uint32_t)m_deep_intersection_list.size();
                        if (nDeepIntersections > 0)
                        {
                            for (uint32_t ds_index=0; ds_index < nDeepIntersections; ++ds_index)
                            {
                                Traceable::DeepIntersection& ds = m_deep_intersection_list[ds_index];
                                assert(ds.I.object);

                                // We don't perform a cutout operation in deep mode, we simply pass the
                                // cutout info out the deep flags:
                                //const bool is_cutout = (ds.color[k_cutout_channel] > 0.5f);

                                // Always convert I.t into cam-space Z (ignore k_persp_correct_z switch):
                                const float dsZ = float(ds.I.t * cos(atan(fRadius*rcam.lensMagnification() / 2.0)));

                                // Assign Chan_DeepFront/Chan_DeepBack:
                                ds.color.Zf() = dsZ;
                                ds.color.Zb() = dsZ;
                                ds.color.Z()  = dsZ;
                                // Make sure the Pixel mask is only color channels:
                                ds.color.channels = deep_color_channels;

                                // Find min/max Z range for opaque samples:
                                if (ds.color.alpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
                                {
                                    surface_Zf = std::min(surface_Zf, dsZ);
                                    surface_Zb = std::max(surface_Zb, dsZ);
                                }

#if 0
                                if (stx.x==1 && stx.y==1)
                                {
                                    // Get axis-aligned Z by transforming PW into camera-space:
                                    DD::Image::Vector3 PC = ((DD::Image::CameraOp*)rcam.cam0.cam)->imatrix().transform(ds.I.PW);
                                    //std::cout << " PW[" << ds.I.PW.x << " " << ds.I.PW.y << " " << ds.I.PW.z << "]" << std::endl;
                                    //std::cout << " PC[" << PC.x << " " << PC.y << " " << PC.z << "]" << std::endl;
                                    std::cout << " fsU=" << fsU << " fsV=" << fsV << " fsR=" << fsR << std::endl;
                                    std::cout.precision(20);
                                    std::cout << " PC.z=" << -PC.z << " dsZ=" << dsZ << " I.t=" << ds.I.t << std::endl;
                                    std::cout.precision(5);
                                }
#endif
                                //----------------------------------------------------------------------
                                // Try to combine this with other samples of the same RenderPrimitive.
                                // TODO: This may not be good enough - may need to test ObjectContext
                                // or SurfaceContext rather than RenderPrimitive:
                                //----------------------------------------------------------------------

                                // Get RenderPrimitive:
                                const RenderPrimitive* rprim = (RenderPrimitive*)ds.I.object;

                                Traceable::DeepIntersectionMap::iterator it = deep_intersection_map.find(rprim->surface_ctx);
                                if (it == deep_intersection_map.end())
                                {
                                    // Not in map yet, add it to the accum list:
                                    deep_accum_list.push_back(ds);
                                    const uint32_t map_index = (uint32_t)deep_accum_list.size()-1;
                                    Traceable::DeepSurfaceIntersectionList dil;
                                    dil.reserve(10);
                                    dil.push_back(map_index);
                                    deep_intersection_map[rprim->surface_ctx] = dil;
                                }
                                else
                                {
                                    // Already in map, see if it's close enough in Z and N to combine together with one
                                    // of the instances:
                                    Traceable::DeepSurfaceIntersectionList& dil = it->second;
                                    const uint32_t nCurrentInstances = (uint32_t)dil.size();

                                    bool match = false;
                                    for (uint32_t j=0; j < nCurrentInstances; ++j)
                                    {
                                        Traceable::DeepIntersection& map_ds = deep_accum_list[dil[j]];
                                        float& minZ = map_ds.color.Zf();
                                        float& maxZ = map_ds.color.Zb();

                                        if (dsZ > (minZ - k_deep_combine_threshold) &&
                                            dsZ < (maxZ + k_deep_combine_threshold) &&
                                            ds.I.N.dot(map_ds.I.N) >= 0.5f)
                                        {
                                            minZ = std::min(minZ, dsZ);
                                            maxZ = std::max(maxZ, dsZ);
                                            // Add colors together:
                                            map_ds.color += ds.color;
                                            ++map_ds.count;
                                            // Or the subpixel masks:
                                            map_ds.spmask |= ds.spmask;
                                            match = true;
                                            break;
                                        }
                                    }

                                    if (!match)
                                    {
                                        // No match in current surface list, add this one as unique:
                                        if (dil.size() > 50)
                                        {
                                            std::cout << stx.x << ":" << stx.y << " " << ds_index;
                                            std::cout << ", nDeepIntersections=" << nDeepIntersections;
                                            std::cout << " !!!! too many surface instances !!!!";
                                            std::cout << ", nCurrentInstances=" << nCurrentInstances;
                                            std::cout << ", dil.size()=" << dil.size();
                                            std::cout << ", deep_accum_list.size()=" << deep_accum_list.size();
                                            std::cout << std::endl;
                                            break;
                                        }

                                        deep_accum_list.push_back(ds);
                                        const uint32_t map_index = (uint32_t)deep_accum_list.size()-1;
                                        dil.push_back(map_index);
                                    }
                                }
                            }
                        }

#ifdef ENABLE_VOLUME_LIGHTING
                        // Only volume march on one sample and if volume is at least partially in front
                        // of closest opaque surface:
                        if (nVolIntersections > 0 && vol_tmin < double(surface_Zf))
                        {
                            bool do_march = true;

                            if (!k_atmosphere_alpha_blending ||
                                (k_atmosphere_alpha_blending && Rcolor.alpha() > 0.999f))
                                vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(surface_Zf)));

                            if (m_have_bg_Z && k_bg_occlusion)
                            {
                                // Clamp tmax to bg Z to speed up march, but only if we're not
                                // alpha blending, and the alpha is < 1:
                                if (!k_atmosphere_alpha_blending ||
                                    (k_atmosphere_alpha_blending && bg.alpha() > 0.999f))
                                {
                                    if (vol_tmin >= bg.Z())
                                        do_march = false; // Skip if bg Z is closer than first volume
                                    else
                                        vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(bg.Z())));
                                }
                            }

                            if (do_march)
                                march(stx,
                                      vol_tmin,
                                      vol_tmax,
                                      vol_depth_min,
                                      vol_depth_max,
                                      surface_Zf,
                                      surface_alpha,
                                      vol_intersections,
                                      Rvolume,
                                      &deep_accum_list);

                        } // nVolumes > 0
#endif
                        //
                        // DEEP
                        //======================================================================================

                    } // flat/deep


                    //-----------------------------------------------------------------
                    // Get coverage if surface Z is within valid intersections
                    //
                    float sample_coverage;
                    if (surface_Zf > 0.0f && surface_Zf < std::numeric_limits<float>::infinity())
                    {
                        //-----------------------------------------------------------------
                        // Output diagnostic trace time value if within valid intersections
                        //
                        if (rtx.k_show_diagnostics > RenderContext::DIAG_RENDER_TIME)
                        {
                            if ((int)sample_index == rtx.k_diagnostics_sample)
                            {
                                struct timeval current_time;
                                gettimeofday(&current_time, 0);

                                // Times in seconds:
                                double tStart = double(stx.start_time.tv_sec) + (double(stx.start_time.tv_usec)/1000000.0);
                                double tEnd   = double(  current_time.tv_sec) + (double(  current_time.tv_usec)/1000000.0);
                                double tSecs = (tEnd - tStart);
                                Raccum.color().set(float(tSecs), 0.0f, 0.0f);
                                Raccum.a() = 1.0f;
                                coverage = 1.0f;
                                break; // done!
                            }

                            continue; // next sample!
                        }

                        sample_coverage = 1.0f;
                        coverage += 1.0f;
                    }
                    else
                        sample_coverage = 0.0f;


                    //-----------------------------------------------------------------
                    // Merge the bg under the final color
                    //
                    if (!k_render_only)
                    {
                        if (stx.rtx->k_transparency_enabled)
                        {
                            //RayShader::A_under_B(bg, Rcolor, Rcolor.channels);
                            const float Ba  = Rcolor.alpha();
                            const float iBa = 1.0f - Ba;
                            if (Ba < std::numeric_limits<float>::epsilon())
                            {
                                foreach(z, Rcolor.channels)
                                    Rcolor[z] += bg[z];
                                Rcolor.cutoutAlpha() += bg.alpha();
                            }
                            else if (Ba < 1.0f)
                            {
                                foreach(z, Rcolor.channels)
                                    Rcolor[z] += bg[z]*iBa;
                                Rcolor.cutoutAlpha() += bg.alpha()*iBa;
                            }
                            else
                            {
                                ;// saturated B alpha - do nothing
                            }

                            // Take min Z:
                            if (m_have_bg_Z)
                            {
                                if (k_bg_occlusion && (bg.Z() < surface_Zf))
                                {
                                    surface_Zf = bg.Z();
                                }
                                else
                                {
                                    // Put bg Z wherever render coverage is 0:
                                    if (sample_coverage < std::numeric_limits<float>::epsilon())
                                        surface_Zf = bg.Z();
                                }
                            }

                        }
                        else if (Rcolor.alpha() < stx.rtx->k_alpha_threshold)
                        {
                            // Surface transparent, copy bg:
                            foreach(z, Rcolor.channels)
                                Rcolor[z] = bg[z];
                            Rcolor.cutoutAlpha() = bg.alpha();

                            if (m_have_bg_Z)
                                surface_Zf = bg.Z();
                        }

                    }


                    //-----------------------------------------------------------------
                    // Final alpha is copied from cutout-alpha channel
                    //
                    Rcolor.alpha() =
                        (Rcolor.cutoutAlpha() >= (1.0f - std::numeric_limits<float>::epsilon())) ?
                            1.0f : 
                                Rcolor.cutoutAlpha();


                    //-----------------------------------------------------------------
                    // Add final color and Z to accumulators
                    //
                    if (0)//(enable_pixel_filter)
                    {
                        // Multiply the final result by the pixel filter:
                        const float pfw = pf_weights[sample_index];
                        foreach (z, rtx.render_channels)
                            Raccum[z] += Rcolor[z]*pfw;
                        const uint32_t nChans = Rcolor.getNumChans();
                        for (uint32_t i=0; i < nChans; ++i)
                        {
                            const DD::Image::Channel z = Rcolor.getIdx(i);
                            Raccum[z] += Rcolor[z]*pfw;
                        }
                    }
                    else
                    {
                        // No individual sample weighting:
                        Raccum += Rcolor;
                    }

                    // Take min Z - TODO: this should support taking min Z of greatest coverage surface!
                    if (surface_Zf < accum_Z)
                        accum_Z = surface_Zf;



                    //
                    // LENS MODE
                    //-----------------------------------------------------------------

                }
                else
                {
                    //-----------------------------------------------------------------
                    //-----------------------------------------------------------------
                    // UV MODE
                    // Don't need a camera!
                    //
                    Rcolor.clearAllChannels();
                    Rcolor.cutoutAlpha() = 0.0f;
                    Rcolor.cutoutAlpha() = 0.0f;

                    const Fsr::Vec2f uvdx(1.0f / float(format().width()-1), 0.0f);
                    const Fsr::Vec2f uvdy(0.0f, 1.0f / float(format().height()-1));

                    float U = float(xx) / float(format().width()-1);
                    U += float(std::max(0, k_uv_mode_tile_index[0]));
                    float V = float(yy) / float(format().height()-1);
                    V += float(std::max(0, k_uv_mode_tile_index[1]));

                    // bias U on both ends a hair:
                    const Fsr::Vec2f uv0(-std::numeric_limits<float>::epsilon(),       V);
                    const Fsr::Vec2f uv1(1.0f + std::numeric_limits<float>::epsilon(), V);

#if 1
                    const Fsr::Vec2f uvP(Fsr::Vec2f(0.0f, V)+uvdx*float(xx));
#else
                    const Fsr::Vec2f uvP(U, V);
#endif

                    Traceable::UVSegmentIntersectionList& uv_intersections = thread_ctx->uv_intersections;

                    //=========================================================
                    //=========================================================
                    rtx.objects_bvh.getIntersectionsWithUVs(stx,
                                                            uv0,
                                                            uv1,
                                                            uv_intersections);
                    //=========================================================
                    //=========================================================

                    const uint32_t nUVIntersections = (uint32_t)uv_intersections.size();
                    for (uint32_t i=0; i < nUVIntersections; ++i)
                    {
                        const Traceable::UVSegmentIntersection& I = uv_intersections[i];
                        assert(I.object); // gotta have an object...
                        RenderPrimitive* rprim = (RenderPrimitive*)I.object;
                        assert(rprim->isTraceable()); // has to be traceable...

                        if (U < I.uv0.x || U > I.uv1.x)
                            continue;

                        const float length = (I.uv1.x - I.uv0.x);
                        if (length <= 0.0f)
                            continue;

//      Int_knob(f, &k_uv_mode_tile_index[0], "uv_mode_tile_u", "tile");
//         ClearFlags(f, Knob::STARTLINE);
//      Int_knob(f, &k_uv_mode_tile_index[1], "uv_mode_tile_v", "");

                        // Interpolate the st coordinate:
#if 1
                        Fsr::Vec2f st, Rxst, Ryst;
                        rprim->isTraceable()->getStCoordAtUv(uvP, st);
                        rprim->isTraceable()->getStCoordAtUv(uvP+uvdx, Rxst);
                        rprim->isTraceable()->getStCoordAtUv(uvP+uvdy, Ryst);
#else
                        // Distance between segment ends:
                        const float d = (U - I.uv0.x) / length;
                        const Fsr::Vec2f st = I.st0*(1.0f - d) + I.st1*d;
#endif

                        const Fsr::Vec3d PW;// = rprim->get_PW_at(st, stx.frame_time_offset);
                        const Fsr::Vec3d N;//  = rprim->get_N_at(st);
                        const Fsr::Vec3d Ng;// = rprim->geometric_normal();

                        // Build a phony camera ray as if the ray has already hit
                        // the object surface, using the geometric normal:
                        stx.Rtx.set(PW - N/*origin*/, -N/*dir*/, std::numeric_limits<double>::epsilon()/*min*/, std::numeric_limits<double>::infinity()/*max*/);
                        stx.Rtx.type_mask = Fsr::RayContext::CAMERA;
                        //if (xx==0 && yy==550 && sample_index==0) {
                        //   std::cout << xx << ":" << y << " u=" << U << " v=" << V << " d=" << d << " st[" << st.x << " " << st.y << "]";
                        //   std::cout << " PW[" << PW.x << " " << PW.y << " " << PW.z << "]";
                        //   std::cout << " Ng[" << Ng.x << " " << Ng.y << " " << Ng.z << "]";
                        //   std::cout << " ray=" << stx.Rtx << std::endl;
                        //}

                        if (stx.texture_filter)
                            stx.use_differentials = true;

                        // Get the surface params at this st coord into the shader context:
                        Traceable::SurfaceIntersection tI;
                        tI.st          = st; // Primitive's parametric coordinates at intersection
                        tI.Rxst        = Rxst; // Primitive's parametric coordinates at intersection
                        tI.Ryst        = Ryst; // Primitive's parametric coordinates at intersection
                        tI.t           = 1.0f; // Phony distance from R.origin to intersection point vtx.PW
                        tI.object      = I.object; // Object pointer for this intersection
                        tI.object_type = I.object_type; // Object ID
                        tI.PW          = PW;
                        tI.PWg         = PW;
                        tI.N           = N;  // Interpolated surface normal (vertex normal) possibly with bump
                        tI.Ns          = N;  // Interpolated surface normal - with no bump
                        tI.Ng          = Ng; // Geometric surface normal

                        // Offset tile UV into 0-1 range so that the 

#if 0
                        DD::Image::VArray vP;
                        rprim->getAttributesAt(st, vP);
                        Rcolor.color().set(vP.UV().x / vP.UV().w, vP.UV().y / vP.UV().w, d);
#else
                        // Final color for surfaces.
                        // We can't call RayShader::illumination() first as this assumes
                        // there's an intersection list available.
                        {
                            // Evaluate the surface shader and determine if it's transparent enough to
                            // continue tracing:
                            RayShaderContext stx_shade(stx);
                            RayMaterial::updateShaderContextFromIntersection(tI, stx_shade);

                            // Having surface_color be black is essential to front-to-back
                            // under-ing because the Nuke legacy shaders are doing overs
                            // internally:
                            //Pixel surface_color(out.channels);
                            //surface_color.clearAllChannels();

                            //------------------------------------------------
                            //------------------------------------------------
                            RayMaterial::doShading(stx_shade, Rcolor/*surface_color*/);
                            //------------------------------------------------
                            //------------------------------------------------
                        }
#endif

                        // Only one surface allowed:
                        break;
                    }

                    // Add color to accumulation pixel:
                    Raccum += Rcolor;
                    //
                    // UV MODE
                    //------------------------------------------------------

                } // lens/uv proj?

            } // samples loop


            //-----------------------------------------------------------------
            // Output integrated values to output buffers:
            //
            if (flat_output_mode)
            {
                //======================================================================================
                //======================================================================================
                // FLAT:
                //
                const float final_weight = (nSamples > 1) ? 1.0f/float(nSamples) : 1.0f;
                coverage *= final_weight;
                // Final color:
                Raccum *= final_weight;
                Raccum[k_coverage_chan] = coverage;

                const uint32_t nAOVs = (uint32_t)rtx.aov_outputs.size();
                if (nAOVs > 0 && coverage > 0.0f)
                {
                    // Unpremult AOV channels:
                    const float inv_coverage =
                        (coverage >= std::numeric_limits<float>::epsilon()) ? 1.0f / coverage : 0.0f;
                    const float inv_alpha =
                        (Raccum.alpha() >= std::numeric_limits<float>::epsilon()) ?
                            1.0f / Raccum.alpha() :
                                0.0f;

                    for (uint32_t i=0; i < nAOVs; ++i)
                    {
                        const AOVLayer& aov = rtx.aov_outputs[i];
                        if (aov.unpremult == zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE)
                        {
                            foreach (z, aov.mask)
                                Raccum[z] *= inv_coverage;
                        }
                        else if (aov.unpremult == zpr::AOVLayer::AOV_UNPREMULT_BY_ALPHA)
                        {
                            foreach (z, aov.mask)
                                Raccum[z] *= inv_alpha;
                        }
                    }
                }

                // Final Z:
                if (write_out_Z)
                {
                    if (k_persp_correct_z)
                        accum_Z *= float(::cos(::atan(fRadius*rcam0.lensMagnification() / 2.0))); // scale Z
                    if (k_one_over_Z)
                    {
                        if (accum_Z < std::numeric_limits<float>::epsilon() || accum_Z >= std::numeric_limits<float>::infinity())
                            accum_Z = 0.0f;
                        else
                            accum_Z = 1.0f / accum_Z;
                    }
                    else
                    {
                        if (accum_Z < std::numeric_limits<float>::epsilon())
                            accum_Z = std::numeric_limits<float>::infinity();
                    }
                }

                // Copy final colors to output line:
                const uint32_t nChans = Raccum.getNumChans();
                for (uint32_t i=0; i < nChans; ++i)
                {
                    const DD::Image::Channel z = Raccum.getIdx(i);
                    *(out_row.writable(z) + xx) = Raccum.chan[z];
                }
                out_row.writable(DD::Image::Chan_Z)[xx] = accum_Z;
                //
                // FLAT
                //======================================================================================

            }
            else
            {
                //======================================================================================
                //======================================================================================
                // DEEP:
                //
                // Output final deep samples:
                const uint32_t nDeepIntersections = (uint32_t)deep_accum_list.size();
                if (nDeepIntersections == 0)
                {
                    deep_out_plane->addHole();
                    continue;
                }

                //if (stx.x==1 && stx.y==1) std::cout << stx.x << ":" << stx.y << " accum samples:" << std::endl;
                DD::Image::DeepOutPixel out_pixel(nDeepIntersections*out_channels.size());
                //std::cout << "out_channels=" << out_channels << ", rtx.render_channels=" << rtx.render_channels << std::endl;
                for (uint32_t i=0; i < nDeepIntersections; ++i)
                {
                    const Traceable::DeepIntersection& ds = deep_accum_list[i];
                    const float weight = 1.0f / float(ds.count);
                    float sp1, sp2;
                    Dcx::mask8x8ToFloats(ds.spmask, sp1, sp2);

                    //const float coverage = (k_deep_output_subpixel_masks)?:1.0f;
                    foreach (z, out_channels)
                    {
                        float v;
                        if        (z == DD::Image::Chan_Z) {
                            v = ds.color[DD::Image::Chan_Z];

                        } else if (z == DD::Image::Chan_DeepFront) {
                            v = ds.color[DD::Image::Chan_DeepFront];

                        } else if (z == DD::Image::Chan_DeepBack) {
                            v = ds.color[DD::Image::Chan_DeepBack];

                        } else if (z == k_spmask_channel[0] && k_deep_output_subpixel_masks) {
                            v = sp1;

                        } else if (z == k_spmask_channel[1] && k_deep_output_subpixel_masks) {
                            v = sp2;

                        } else if (z == k_spmask_channel[2] && k_deep_output_subpixel_masks)
                        {
                            Dcx::DeepSampleFlag flags = Dcx::DEEP_EMPTY_FLAG;
                            // Null intersection object pointer indicates a volume:
                            if (ds.I.object != 0)
                               flags |= Dcx::DEEP_LINEAR_INTERP_SAMPLE;
                            if (ds.color[k_cutout_channel] > 0.5f)
                               flags |= Dcx::DEEP_MATTE_OBJECT_SAMPLE;
                            v = float(flags);

                        } else {
                            v = ds.color[z]*weight;
                        }
                        //std::cout << "  z=" << z << ", v=" << v << std::endl;

                        out_pixel.push_back(v);
                    }
                }
                deep_out_plane->addPixel(out_pixel);
                //
                // DEEP
                //======================================================================================

            } // flat/deep?

        } // pixel loop x->r

    } // pixel loop y->t


    //-----------------------------------------------------------------
    // Destroy the temp lighting scenes:
    //
    thread_ctx->clearLightingScenes();


    //-----------------------------------------------------------------
    // Check if any ObjectContexts are stale (old) and delete their
    // bvh's if so. They will get rebuilt on next render pass if
    // they're still needed.
    //
#if 0
    my_lock.lock();
    struct timeval the_time;
    gettimeofday(&the_time, 0);
    const uint32_t nObjects = rtx.object_context.size();
    for (uint32_t i=0; i < nObjects; ++i)
    {
        ObjectContext* otx = rtx.object_context[i];

        // Times in seconds:
        double tS = double(otx->last_access.tv_sec) + double(otx->last_access.tv_usec)/1000000.0;
        double tE = double(the_time.tv_sec  ) + double(the_time.tv_usec  )/1000000.0;
        double tSecs = (tE - tS);

        const cleanup_delay_secs = 4;
        if (tSecs > cleanup_delay_secs)
            otx->clearSurfacesAndRenderPrims();
    }
    my_lock.unlock();
#endif

    return true;

} // tracerEngine()


} // namespace zpr

// end of zpTracer.cpp

//
// Copyright 2020 DreamWorks Animation
//
