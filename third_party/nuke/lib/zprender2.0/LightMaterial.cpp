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

/// @file zprender/LightMaterial.cpp
///
/// @author Jonathan Egstad


#include "LightMaterial.h"
#include "LightMaterialOp.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "Sampling.h"
#include "VolumeShader.h"


// Force the compiler to include the built-in shader static ShaderDescriptions
// otherwise RayShader::create() won't find them:
#include "zprPointLight.h"
//#include "zprRectangleLight.h"
//#include "zprSpotLight.h"


#include <Fuser/LightOp.h>


namespace zpr {


//-----------------------------------------------------------------------------


/*!
*/
LightMaterial::LightMaterial() :
    RayMaterial(),
    m_light_shader(NULL)
{
    // Force the compiler to include the built-in shader static ShaderDescriptions
    // otherwise RayShader::create() won't find them:
    static zprPointLight      dummyPointLight;
    //static zprRectangleLight  dummyRectangleLight;
    //static zprSpotLight       dummySpotLight;
}


//!
LightMaterial::LightMaterial(const Fsr::DoubleList&  motion_times,
                             const Fsr::Mat4dList&   motion_xforms,
                             std::vector<RayShader*> shaders,
                             LightShader*            output_light_shader) :
    RayMaterial(shaders),
    m_light_shader(output_light_shader),
    m_motion_times(motion_times),
    m_motion_xforms(motion_xforms)
{
    //
}


/*! Deletes any RayShader children.
*/
/*virtual*/
LightMaterial::~LightMaterial()
{
    // RayMaterial base class deletes the list of RayShaders
}


/*!
*/
/*virtual*/
void
LightMaterial::validateMaterial(bool                 for_real,
                                const RenderContext& rtx)
{
    //std::cout << "LightMaterial::validateMaterial(" << this << ") m_light_shader="<< m_light_shader << std::endl;
    m_texture_channels = DD::Image::Mask_None;
    m_output_channels  = DD::Image::Mask_None;
    m_light_volume_bbox.clear();

    // Validate any textures and light volume bboxes:
    if (m_light_shader)
    {
        // Assign xforms now so that they can be locally fiddled with:
        m_light_shader->setMotionXforms(m_motion_times, m_motion_xforms);
        m_light_shader->validateShader(for_real, &rtx, NULL/*op_ctx*/);
        m_texture_channels = m_light_shader->getTextureChannels();
        m_output_channels  = m_light_shader->getChannels();

        // If the light shader can create a LightVolume get its motion bbox:
        if (rtx.atmospheric_lighting_enabled &&
            m_light_shader->canGenerateLightVolume())
        {
            m_light_volume_bbox = m_light_shader->getLightVolumeMotionBbox();
            //std::cout << "  m_light_volume_bbox" << m_light_volume_bbox << std::endl;
        }
    }
}


/*!
*/
/*virtual*/
void
LightMaterial::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    if (m_light_shader)
        m_light_shader->getActiveTextureBindings(texture_bindings);
}


/*! Evaluate the light's contribution to a surface intersection.
    Returns false if light does not contribute to surface illumination.

    Calls illuminate() on the assigned output light shader.
*/
bool
LightMaterial::illuminate(RayShaderContext& stx,
                          Fsr::RayContext&  light_ray,
                          float&            direct_pdfW_out,
                          Fsr::Pixel&       light_color_out)
{
    if (m_light_shader)
        return m_light_shader->illuminate(stx,
                                          light_ray,
                                          direct_pdfW_out,
                                          light_color_out);
    return false;
}


//-----------------------------------------------------------------------------


/*! Create a LightMaterial from a DD::Image::LightOp*.

    This built-in translator will check for Fuser::LightOp vs.
    DD::Image::LightOp and attempt to handle unknown light types
    by translating knob names.

*/
/*static*/
LightMaterial*
LightMaterial::createLightMaterial(const RenderContext&   rtx,
                                   DD::Image::LightOp*    light,
                                   const Fsr::DoubleList& motion_times,
                                   const Fsr::Mat4dList&  motion_xforms)
{
    //std::cout << "  LightMaterial::createLightMaterial('" << light->node_name() << "')" << std::endl;
    if (!light)
        return NULL; // don't crash...

    //------------------------------------------------------------------
    // Can this light directly create LightShader and LightMaterials?
    //
    LightMaterialOp* ltmaterial_op = LightMaterialOp::getOpAsLightMaterialOp(light);
    if (ltmaterial_op)
    {
        //std::cout << "    LightMaterialOp: lighttype=" << light->lightType();
        //std::cout << ", Class='" << light->Class() << "'" << ", ltMatOp=" << ltmaterial_op;
        //std::cout << std::endl;
        return ltmaterial_op->createMaterial(&rtx, motion_times, motion_xforms);
    }

    LightMaterial* ltmaterial = NULL;

    //------------------------------------------------------------------
    // Check first if it's a Fuser light op:
    //
    Fsr::FuserLightOp* fsr_light = Fsr::FuserLightOp::asFuserLightOp(light);
    if (fsr_light)
    {
        //std::cout << "    FuserLightOp: lighttype=" << fsr_light->lightType();
        //std::cout << ", Class='" << fsr_light->Class() << "'";
        //std::cout << std::endl;

        // TODO: extend FuserLightOp class to provide a Material/Shader
        // interface.

        // do nothing for now

        return ltmaterial;
    }


    //------------------------------------------------------------------
    // Translate a DD::Image::LightOp by checking its LightType
    // enumeration, and if 'eOtherLight' check the class name
    // against known suported nodes.
    //

    // TODO: finish this section! We likely want to create the correct
    // zprLightShader, set the values, then ask it to construct a volume
    // shader.
    return ltmaterial; // do nothing for now

std::cout << "    DD::Image::LightOp: lighttype=" << light->lightType();
std::cout << ", Class='" << light->Class() << "'";
std::cout << std::endl;


    /* In DDImage LightOp.h (valid as of Nuke 12):
        enum LightType
        { 
            ePointLight,       0
            eDirectionalLight, 1
            eSpotLight,        2
            eOtherLight        3
        };
    */

    LightShader* lt_shader = NULL;

    // Check for recognized light types:
    if (light->lightType() == DD::Image::LightOp::ePointLight)
    {
        lt_shader = dynamic_cast<LightShader*>(RayShader::create("PointLight"));
    }
    else if (light->lightType() == DD::Image::LightOp::eDirectionalLight)
    {
        //lt_shader = dynamic_cast<LightShader*>(RayShader::create("DirectLight"));

#if 0
        if (vol_type0 != zpr::UNRECOGNIZED_PRIM)
        {
            // Create CylinderVolume:
            //std::cout << " type=LIGHTCYLINDER_PRIM" << std::endl;
        }
#endif

    }
    else if (light->lightType() == DD::Image::LightOp::eSpotLight)
    {
        //lt_shader = dynamic_cast<LightShader*>(RayShader::create("SpotLight"));

#if 0
        if (vol_type0 != zpr::UNRECOGNIZED_PRIM)
        {
            // Create ConeVolume:
            //std::cout << " type=LIGHTCONE_PRIM" << std::endl;

#if 0
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
#endif


        }
#endif

    }
    else
    {
        // Check for ReflectionCard:
        if (strcmp(light->Class(), "ReflectionCard")==0 ||
            strcmp(light->Class(), "AreaLight"     )==0)
        {
            // LightCard

            //lt_shader = dynamic_cast<LightShader*>(RayShader::create("CardLight"));

            //std::cout << " type=LIGHTCARD_PRIM" << std::endl;
        }
    }


    if (lt_shader)
    {
        std::vector<RayShader*> shaders; shaders.reserve(5);
        shaders.push_back(lt_shader);

        // Assign common LightOp knob values:
        /*
            DWA PointLight node:
                color 1
                intensity 1

                near 0.001
                far 1
                falloff_rate 1
                light_identifier ""
                object_mask *

                falloff_profile_enable false
                falloff_profile


            Stock PointLight node:
                color 1
                intensity 1
                falloff_type "No Falloff"

                cast_shadows false
                shadow_mode solid
                filter Cubic
                scene_epsilon 0.001
                samples 1
                sample_width 1
                depthmap_bias 0.01
                depthmap_slope_bias 0.01
                clipping_threshold 0.5
                shadow_jitter_scale 3
                depthmap_width 1024
                shadow_mask none
        */
        int kindex = 0;
        while (1)
        {
            DD::Image::Knob* k = light->knob(kindex++);
            if (!k)
                break; // all done no more knobs

            if (lt_shader->setInputValue(k->name().c_str(), k, light->outputContext()))
            {
                //std::cout << k->name() << ": copied" << std::endl;
            }
        }
        //std::cout << *lt_shader << std::endl;

        // LightMaterial will set motion times and xforms on its output
        // LightShader when validateMaterial() is called:
        ltmaterial = new LightMaterial(motion_times,
                                       motion_xforms,
                                       shaders,
                                       lt_shader/*output_light_shader*/);

    }
    else
    {
        //std::cout << " UNRECOGNIZED LIGHT TYPE" << std::endl;
        //std::cout << "zpr::LightMaterial::createLightMaterial(): warning, unknown light type, skipping..." << std::endl;
    }

    return ltmaterial;
}


/*! Create a LightVolume primitive appropriate for the assigned LightShader.
    Calling function takes ownership.
    MaterialContext is passed for use in the Volume ctors.

    If a light can illuminate atmosphere then it becomes a physical object
    of a certain size, so create the LightVolume primitive.

    Calls createLightVolume() on the assigned output light shader.
*/
LightVolume*
LightMaterial::createLightVolume(const MaterialContext* material_ctx)
{
    if (m_light_shader && m_light_shader->canGenerateLightVolume())
        return m_light_shader->createLightVolume(material_ctx);

    return NULL;
}




#if 0
/*!

*/
void
LightMaterial::getSurfaceIllumination(const RayShaderContext& stx,
                                      Fsr::RayContext&        light_ray,
                                      float&                  direct_pdfW_out,
                                      Fsr::Pixel&             light_color_out)
{
    if (!m_light_shader)
    {
        direct_pdfW_out = 0.0f;
        light_color_out.rgb().setToZero();
        return;
    }


    //------------------------------------------------------------------------
    // LightShader illuminate() also builds a ray from surface to light
    // which can be used for shadowing, volume calcs, etc:
    //
    Fsr::RayContext Rlight;
    float direct_pdfW;
    if (!m_light_shader->illuminate(stx,
                                    Rlight,
                                    direct_pdfW,
                                    light_color))
    {
        direct_pdfW_out = 0.0f;
        light_color_out.rgb().setToZero();
        return; // not affecting this surface
    }


    //------------------------------------------------------------------------
    // Get shadowing factor for light (0=shadowed, 1=no shadow):
    //
    //float shadow = 1.0f;
    RayShaderContext Rshadow_stx(stx,
                                 Rlight,
                                 Fsr::RayContext::shadowPath()/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

    Traceable::SurfaceIntersection Ishadow(std::numeric_limits<double>::infinity());
    if (stx.rtx->objects_bvh.getFirstIntersection(Rshadow_stx, Ishadow) > Fsr::RAY_INTERSECT_NONE &&
         Ishadow.t < Rlight.maxdist)
    {
#if 1
        continue;
#else
        //std::cout << "D=" << D << ", t=" << Ishadow.t << std::endl;
        // Shadowed - make it fall off the farther the occluder is from surface(hack!!!):
        shadow = (Ishadow.t / D);//powf(float(1.0 - (Ishadow.t / D)), 2.0f);
        if (shadow <= 0.0f)
        {
            continue;
        }
#endif
    }


    //------------------------------------------------------------------------
    // Intersect and ray march volumes
    //
    if (m_light_volume_shader)
    {
        Volume::VolumeIntersectionList& vol_intersections = stx.thread_ctx->vol_intersections;
        I_list.clear();
        double vol_tmin, vol_tmax;
        double vol_segment_min_size, vol_segment_max_size;
        if (VolumeShader::getVolumeIntersections(stx,
                                                 vol_intersections,
                                                 vol_tmin,
                                                 vol_tmax,
                                                 vol_segment_min_size,
                                                 vol_segment_max_size))
        {
            //std::cout << "RayShader::getIllumination(): Rtx[" << stx.Rtx << "] atmo-shader=" << stx.atmosphere_shader;
            //std::cout << "  nVolumeIntersections=" << vol_intersections.size() << std::endl;
            bool do_march = true;

            // If final surface alpha is 1, clamp the volume's range against the surface render.  This
            // unfortunately means that volumes between transparent surfaces are not rendered.
            // It's a compromise for speed...
            if (!stx.rtx->k_atmosphere_alpha_blending ||
                (stx.rtx->k_atmosphere_alpha_blending && out.alpha() > 0.999f))
            {
                if (vol_tmin >= surface_Zf)
                    do_march = false; // Skip if surface Z is closer than first volume
                else
                    vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(surface_Zf)));
            }

#if 0
            // TODO: re-support camera (primary) ray blending with bg Z and alpha
            // TODO: pass bg values in stx as a Pixel*?
            if (stx.Rtx.isCameraPath())
            {
                if (m_have_bg_Z && k_bg_occlusion)
                {
                    // Clamp tmax to bg Z to speed up march, but only if we're not
                    // alpha blending, and the alpha is < 1:
                    if (!stx.rtx->k_atmosphere_alpha_blending ||
                        (stx.rtx->k_atmosphere_alpha_blending && bg.alpha() > 0.999f))
                    {
                        if (vol_tmin >= bg.Z())
                            do_march = false; // Skip if bg Z is closer than first volume
                        else
                            vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(bg.Z())));
                    }
                }
            }
#endif

            // Finally check if cutout surface is in front of all volumes:
            if (surface_is_cutout && surface_Zf <= vol_tmin)
                do_march = false;

            if (do_march)
            {
#if 0
                if (stx.rtx->k_show_diagnostics == RenderContext::DIAG_VOLUMES)
                {
                    Raccum.red()   += float(vol_tmin);
                    Raccum.green() += float(vol_tmax);
                    Raccum.blue()  += float(vol_segment_min_size);
                    Raccum.alpha() += float(vol_segment_max_size);
                    // Take min Z:
                    if (surface_Zf < accum_Z)
                        accum_Z = surface_Zf;
                    coverage += 1.0f;
                    ++sample_count;
                    continue;
                }
#endif

                // Ray march through volumes:
                volume_color.clearAllChannels();
                if (stx.atmosphere_shader->volumeMarch(stx,
                                                       vol_tmin,
                                                       vol_tmax,
                                                       vol_segment_min_size,
                                                       vol_segment_max_size,
                                                       surface_Zf,
                                                       out.alpha(),
                                                       vol_intersections,
                                                       volume_color,
                                                       NULL/*deep_out*/))
                {
                    // Add volume illumination to final:
                    out.color()       += volume_color.color();
                    out.alpha()       += volume_color.alpha();
                    out.cutoutAlpha() += volume_color.cutoutAlpha();

                    // Take min Z:
                    if (volume_color.Z() < surface_Zf)
                        surface_Zf = volume_color.Z();
                }
            }

        } // nVolumes > 0

    } // doVolumes

}
#endif


} // namespace zpr

// end of zprender/LightMaterial.cpp

//
// Copyright 2020 DreamWorks Animation
//
