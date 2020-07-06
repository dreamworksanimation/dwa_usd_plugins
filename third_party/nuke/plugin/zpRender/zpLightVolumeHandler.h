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

/// @file zpLightVolumeHandler.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightVolumeHandler_h
#define zprender_LightVolumeHandler_h

#include <zprender/SurfaceHandler.h>
#include <zprender/ConeVolume.h>
#include <zprender/SphereVolume.h>
#include <zprender/VolumeMaterialOp.h>


namespace zpr {


class AmbientVolumeShader : public VolumeMaterialOp
{
  public:
    AmbientVolumeShader() {}

    //! Allocate and return a RayShader object. Calling object takes ownership of pointer.
    /*virtual*/ VolumeShader* createShader(const RenderContext& rtx) { return NULL; }
};


/*!
*/
class ConeHandler : public SurfaceHandler
{
  public:
    ConeHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "ConeHandler"; }

  public:
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        //std::cout << "  ConeHandler(" << stx.obj_index << ")::generate_render_prims()" << std::endl;
        LightVolumeContext* lvctx = stx.getLightVolumeContext();
        if (!lvctx)
        {
            std::cerr << "Incorrect ObjectContext type for LightCone primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }

        const uint32_t nMotionSamples = lvctx->numMotionSamples();
        if (nMotionSamples == 0)
        {
            std::cerr << "Error, zero light motion samples, likely due to a coding error." << std::endl;
        }
        else
        {
            zpr::ConeVolume::SampleList motion_cones(nMotionSamples);
            for (uint32_t j=0; j < nMotionSamples; ++j)
            {
                const LightVolumeContext::Sample& lvtx = lvctx->getLightVolumeSample(j);
#if DEBUG
                assert(lvtx.lt_ctx);
                assert(lvtx.lt_ctx->light());
#endif
                const DD::Image::LightOp* light = lvtx.lt_ctx->light();
                const Fsr::Mat4d l2w(light->matrix());
                motion_cones[j].set(l2w,//lvtx.l2w
                                    clamp(light->hfov(), 0.0001, (double)180.0),
                                    clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                    clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()));
            }

            ConeVolume* cone = new ConeVolume(&stx, rtx.shutter_times, motion_cones);

            lvctx->addPrim(cone);
        }
    }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
class SphereHandler : public SurfaceHandler
{
  public:
    SphereHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "SphereHandler"; }

  public:
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
std::cout << "  SphereHandler(" << stx.obj_index << ")::generate_render_prims()" << std::endl;
        LightVolumeContext* lvctx = stx.getLightVolumeContext();
        if (!lvctx)
        {
            std::cerr << "Incorrect ObjectContext type for LightCone primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }

        const uint32_t nMotionSamples = (uint32_t)lvctx->numMotionSamples();
        if (nMotionSamples == 0)
        {
            std::cerr << "Error, zero light motion samples, likely due to a coding error." << std::endl;
        }
        else
        {
            zpr::SphereVolume::SampleList motion_spheres(nMotionSamples);
            for (uint32_t j=0; j < nMotionSamples; ++j)
            {
                const LightVolumeContext::Sample& lvtx = lvctx->getLightVolumeSample(j);
                const DD::Image::LightContext* lt_ctx = lvtx.lt_ctx;

                zpr::SphereVolume::Sample& sphere = motion_spheres[j];
                sphere.inv_xform   = lvtx.w2l;
                sphere.radius_near = clamp(lt_ctx->light()->Near(), 0.0001, std::numeric_limits<double>::infinity());
                sphere.radius_far  = clamp(lt_ctx->light()->Far(),  0.0001, std::numeric_limits<double>::infinity());
            }

            SphereVolume* sphere = new SphereVolume(&stx, rtx.shutter_times, motion_spheres);

            lvctx->addPrim(sphere);
        }
    }
};



} // namespace zpr

#endif

// end of zpLightVolumeHandler.h

//
// Copyright 2020 DreamWorks Animation
//
