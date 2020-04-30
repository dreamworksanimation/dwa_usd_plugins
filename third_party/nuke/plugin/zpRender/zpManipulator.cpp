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

/// @file zpManipulator.cpp
///
/// @author Jonathan Egstad


#include "zpRender.h"

#include "AxisManipulator.h"//<zprender/AxisManipulator.h>
#include <zprender/RenderContext.h>
#include <zprender/ThreadContext.h>

#include <DDImage/Thread.h>  // for Lock

static DD::Image::Lock my_lock;


namespace zpr {


/*virtual*/
bool
zpRender::intersectScene(DD::Image::ViewerContext* ctx,
                         Fsr::Vec3d&               camPW,
                         Fsr::Vec3d&               camV,
                         Fsr::Vec3d&               surfPW,
                         Fsr::Vec3d&               surfN)
{
#ifdef DEBUG_MANIPULATOR
    std::cout << "zpRender::intersectScene(" << node_name() << "): viewer_mode=" << (int)ctx->viewer_mode() << ", transform_mode=" << (int)ctx->transform_mode() << std::endl;
#endif

    zpr::Scene* scene0 = (zpr::Scene*)manipulatorScene();
    if (!scene0 || !rtx.render_format)
        return false; // just in case...
#ifdef DEBUG_MANIPULATOR
  std::cout << "  scene0=" << scene0 << ", frame=" << scene0->frame << ", motion_step=" << scene0->motion_step << std::endl;
#endif

    // Build the view ray:
    Fsr::RayContext Rtx;
    if (ctx->viewer_mode() == DD::Image::VIEWER_2D/* && ctx->transform_mode() == VIEWER_PERSP*/)
    {
#if 0
        scene0->camera->validate();
        zpr::RayCamera rayCam;
        rayCam.build(rtx, scene0->camera/*cam0*/, 0/*cam1*/, outputContext());
        rayCam.constructRay(ctx->x()+0.5f, ctx->y()+0.5f,
                            0.0f/*lensU*/, 0.0f/*lensV*/,
                            0.0f/*motion-step-time*/,
                            R);
#endif
    }
    else
    {
#if 0
        Matrix4 cm  = ctx->cam_matrix().inverse();
        R.set_origin(cm.translation());
        Fsr::Vec3d dir = (Fsr::Vec3d(ctx->x(), ctx->y(), ctx->z()) - cm.translation());
        dir.normalize();
        // Negate direction if facing away from camera:
        Fsr::Vec3d negZ = cm.transform(Fsr::Vec3d(0,0,-1.0)) - cm.translation();
        negZ.normalize();
        if (negZ.dot(dir) < 0.0)
            dir = -dir;
        R.set(cm.translation(), dir);
#endif
    }
#ifdef DEBUG_MANIPULATOR
    std::cout << "  Rtx[" << R.origin.x << " " << R.origin.y << " " << R.origin.z << "]";
    std::cout << "[" << R.direction().x << " " << R.direction().y << " " << R.direction().z << "]" << std::endl;
#endif

    if (!rtx.objects_initialized)
    {
        my_lock.lock();
        if (!rtx.objects_initialized)
        {
            if (!generate_render_primitives())
            {
               // Bail fast on user abort:
               my_lock.unlock();
               return false;
            }
        }
        // If something failed, bail:
        if (!rtx.objects_initialized)
        {
            my_lock.unlock();
            return false;
        }
        my_lock.unlock();
    }


    //========================================================================
    // Get the ThreadContext object, creating it if this is the
    // first time the thread's been used:
    ThreadContext* thread_context = NULL;
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
#ifdef DEBUG_INFO
            std::cout << "    adding thread context " << tindex << " for thread " << tID << std::endl;
#endif
        }
        my_lock.unlock();

        // This is the ThreadContext for this thread:
        assert(thread_it->second < rtx.thread_list.size());
        thread_context = rtx.thread_list[thread_it->second];
    }
    assert(thread_context);


    Fsr::Mat4d identity_matrix;
    identity_matrix.setToIdentity();

#ifdef DEBUG_MANIPULATOR
    std::cout << "  rtx.render_frame=" << rtx.render_frame << std::endl;
#endif

    //========================================================================
    // Configure Camera shading context:
    zpr::RayShaderContext stx;
    {
        stx.Rtx                  = Rtx;
        stx.use_differentials    = false;
        //
        stx.heroCamOrigin        = Rtx.origin;
        stx.use_heroV_for_spec   = false;
        //
        stx.distance             = 0.0; // Distance from last intersection/camera
        //
        stx.rprim                = NULL; // Current primitive being intersected/shaded
        stx.w2l                  = &identity_matrix; // World-to-local matrix for current primitive
        stx.l2w                  = &identity_matrix; // Local-to-world matrix for current primitive
        //
        //
        stx.surface_shader       = NULL; // Current RayShader being evaluated (NULL if legacy material)
        stx.displacement_shader  = NULL; // Current RayShader being evaluated (NULL if legacy material)
        stx.atmosphere_shader    = NULL; // Current atmospheric VolumeShader being evaluated
        //
        stx.direct_lighting_enabled   = false;
        stx.indirect_lighting_enabled = false;
        //
        stx.master_light_shaders       = NULL; //List of all light shaders in scene
        stx.per_object_light_shaders   = NULL; //Per-object list of light shaders
        stx.master_lighting_scene      = NULL;//&lighting_scene;
        stx.per_object_lighting_scenes = NULL; //&per_object_lighting_scenes;
        //
        stx.material             = NULL;
        stx.texture_filter       = NULL; //&texture_filter_;
        //
        stx.depth                = 0;
        stx.diffuse_depth        = 0;
        stx.glossy_depth         = 0;
        stx.reflection_depth     = 0;
        stx.refraction_depth     = 0;
        stx.index_of_refraction  = -std::numeric_limits<double>::infinity(); // undefined
        //
        stx.sides_mode           = RenderContext::SIDES_FRONT; // Which sides to intersect against (SIDES_BOTH, SIDES_FRONT, SIDES_BACK)
        //
        stx.x                    = (int)ctx->x(); // Current output screen coords
        stx.y                    = (int)ctx->y(); // Current output screen coords
        stx.sx                   = 0.0; // Current output subpixel screen coords
        stx.sy                   = 0.0; // Current output subpixel screen coords
        stx.si                   = 0; // Current subsample index
        //
        stx.sampler              = &(m_sampler_set->getSampleSet(0)[0]); // Sampler to use
        //
        stx.frame_time           = rtx.render_frame; // Always use output render frame time
        stx.frame_time_offset    = 0.0;   // Shutter time offset from absolute frame time
        stx.mb_enabled           = false; // Whether to interpolate time
        stx.frame_shutter_step   = 0;     // Motion-step index for frame_time
        //
        stx.rtx                  = &rtx; // Global rendering context - this contains the global geometry environment
        stx.previous_stx         = NULL; // Previous RayShaderContext, normally the last surface intersected/shaded
        stx.thread_index         = 0; // Index of current thread, starting at 0
        //
        stx.cutout_channel       = k_cutout_channel;
        //
        stx.show_debug_info      = false; // For debugging
    }

int k_debug_saved = rtx.k_debug;
rtx.k_debug = RenderContext::DEBUG_LOW;
    Traceable::SurfaceIntersection I;
    I.t = std::numeric_limits<double>::infinity();
    if (rtx.objects_bvh.getFirstIntersection(stx, I) == Fsr::RAY_INTERSECT_NONE)
{
rtx.k_debug = k_debug_saved;
        return false;
}
rtx.k_debug = k_debug_saved;

    assert(!isnan(I.N.x) && !isnan(I.N.y) && !isnan(I.N.z));
    camPW  =  Rtx.origin;
    camV   = -Rtx.dir();
    surfPW =  I.PW;

#if 0
    // Get the interpolated normal if the prim exists (should always happen!):
    RenderPrimitive* prim = (RenderPrimitive*)I.object;
    if (prim)
    {
        DD::Image::VArray vP;
        rprim->getAttributesAt(I, vP);
        surfN = vP.N();
        // TODO: Can we use the stx method?
        //rprim->getAttributesAt(I, stx);
        //surfN = stx.N;
    }
    else
    {
       surfN = I.N;
    }
#else
    surfN = I.N;
#endif

#ifdef DEBUG_MANIPULATOR
    std::cout << "  intersection t=" << I.t;
    std::cout << ", I.N" << I.N << ", surfPW" << surfPW << ", surfN" << surfN;
    std::cout << std::endl;
#endif

#if 1
    // Protect against bad normals...TODO: fix root cause!!!
    if (isnan(surfN.x) || isnan(surfN.y) || isnan(surfN.z))
    {
        std::cerr << "zpRender::intersectScene(" << node_name() << "): warning, bad normal (nans), using geometric normal" << std::endl;
        surfN = I.N;
        if (isnan(surfN.x) || isnan(surfN.y) || isnan(surfN.z))
        {
            std::cerr << "zpRender::intersectScene(" << node_name() << "): warning, bad geometric normal (nans), defaulting to 0,0,1" << std::endl;
            surfN.set(0,0,1);
        }
    }
#else
    assert(!isnan(surfN.x) && !isnan(surfN.y) && !isnan(surfN.z));
#endif

    return true;
}



} // namespace zpr

// end of zpManipulator.cpp

//
// Copyright 2020 DreamWorks Animation
//
