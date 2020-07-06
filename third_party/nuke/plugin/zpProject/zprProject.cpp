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

/// @file zprProject.cpp
///
/// @author Jonathan Egstad


#include "zprProject.h"

#include <zprender/RenderContext.h>


namespace zpr {

/*static*/ const char* const zprProject::operation_modes[] = { "none", "replace", "over", "under", "stencil", "mask", "plus", "average", "min", "max", 0 };
/*static*/ const char* const zprProject::face_names[]      = { "both", "front", "back", 0 };
/*static*/ const char* const zprProject::zclip_modes[]     = { "none", "cam", "user", 0 };


static RayShader* shaderBuilder() { return new zprProject(); }
/*static*/ const RayShader::ShaderDescription zprProject::description("zprProject", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprProject::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
    {InputKnob("map",      PIXEL_KNOB)}, // MAP1
    {InputKnob("camera",   PIXEL_KNOB)}, // CAMERA2
};
/*static*/ const RayShader::OutputKnobList zprProject::output_defs =
{
    {OutputKnob("surface", PIXEL_KNOB )},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       DOUBLE_KNOB)},
    {OutputKnob("g",       DOUBLE_KNOB)},
    {OutputKnob("b",       DOUBLE_KNOB)},
    {OutputKnob("a",       DOUBLE_KNOB)},
};


//!
zprProject::InputParams::InputParams() :
    k_texture_filter(DD::Image::Filter::Cubic)
{
    k_operation      = MERGE_REPLACE;
    k_faces_mode     = FACES_BOTH;
    k_crop_to_format = true;
    k_proj_channels  = DD::Image::Mask_All;
    k_zclip_mode     = Z_CLIP_CAM;
    k_near_clip      = 100.0;
    k_far_clip       = 10000.0;
}


/*!
*/
zprProject::zprProject() :
    RayShader(input_defs, output_defs)
{
    //
}


/*!
*/
zprProject::zprProject(const InputParams& _inputs) :
    RayShader(input_defs, output_defs),
    inputs(_inputs)
{
    //std::cout << "zprProject::ctor(" << this << ")" << std::endl;
    //
}


/*static*/
void
zprProject::updateLocals(const InputParams& _inputs,
                         LocalVars&         _locals)
{
    // Make projection fit into UV range 0-1, correcting for format w/h ratio:
    _locals.m_project_channels = DD::Image::Mask_None;

    DD::Image::Iop* texture = _inputs.k_bindings[MAP1].asTextureIop();
    if (texture)
    {
        texture->validate(true/*for_real*/);
        const DD::Image::Format& f = texture->format();

        _locals.m_projectproj.setToTranslation(0.5, 0.5, 0.0);
        _locals.m_projectproj.scale(0.5, 0.5*double(f.w())*f.pixel_aspect()/double(f.h()), 0.5);

        _locals.m_project_channels = texture->channels();
        _locals.m_project_channels &= _inputs.k_proj_channels;
    }
    else
    {
        _locals.m_projectproj.setToIdentity();
    }

    // Get camera transforms from inputs:
    _locals.m_proj_cam = _inputs.k_bindings[CAMERA2].asCameraOp();
    if (_locals.m_proj_cam)
    {
        _locals.m_proj_cam->validate(true/*for_real*/);
        _locals.m_projectproj  *= _locals.m_proj_cam->projection();
        _locals.m_projectxform  = _locals.m_proj_cam->imatrix();

        _locals.m_projectconcat  = _locals.m_projectproj;
        _locals.m_projectconcat *= _locals.m_projectxform;

        _locals.m_cam_near = ::fabs(_locals.m_proj_cam->Near());
        _locals.m_cam_far  = ::fabs(_locals.m_proj_cam->Far());
    }
    else
    {
        _locals.m_projectxform.setToIdentity();
        _locals.m_projectconcat.setToIdentity();
        _locals.m_cam_near = _locals.m_cam_far = 0.0;
    }

    _locals.m_near_clip = ::fabs(_inputs.k_near_clip);
    _locals.m_far_clip  = ::fabs(_inputs.k_far_clip);

    //std::cout << "m_proj_cam=" << _locals.m_proj_cam << std::endl;
    //std::cout << "m_projectxform" << _locals.m_projectxform << std::endl;
    //std::cout << "m_projectproj" << _locals.m_projectproj << std::endl;
    //std::cout << "m_project_channels=" << _locals.m_project_channels << std::endl;
}


/*virtual*/
InputBinding*
zprProject::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*virtual*/
void
zprProject::validateShader(bool                 for_real,
                           const RenderContext& rtx)
{
    RayShader::validateShader(for_real, rtx); // < get the inputs

    updateLocals(inputs, locals);

    m_texture_channels  = DD::Image::Mask_None;
    for (uint32_t i=0; i < NUM_INPUTS; ++i)
        m_texture_channels += inputs.k_bindings[i].getChannels();

    m_output_channels = m_texture_channels;
    m_output_channels += locals.m_project_channels;
}


/*virtual*/
void
zprProject::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    RayShader::getActiveTextureBindings(texture_bindings); // < get the inputs

    for (uint32_t i=0; i < NUM_INPUTS; ++i)
        if (inputs.k_bindings[i].isActiveTexture())
            texture_bindings.push_back(&inputs.k_bindings[i]);
}


//----------------------------------------------------------------


bool
zprProject::project(const Fsr::Mat4d& proj_matrix,
                    RayShaderContext& stx,
                    Fsr::Vec2f&       UV,
                    Fsr::Vec2f&       dUVdx,
                    Fsr::Vec2f&       dUVdy)
{
    // Project the world-space point backwards through projector:
    Fsr::Vec4f uv = proj_matrix.transform(stx.PW, 1.0);
    // Skip uvs behind the camera:
    if (uv.w <= 0.0f)
        return false;

    // Skip if outside the 0..1 box:
    if (inputs.k_crop_to_format &&
        (uv.x < 0.0f || uv.x > uv.w ||
         uv.y < 0.0f || uv.y > uv.w))
        return false;

    // Calculate the derivatives:
    Fsr::Vec4f uvdx = proj_matrix.transform(stx.PW + stx.dPWdx, 1.0);
    Fsr::Vec4f uvdy = proj_matrix.transform(stx.PW + stx.dPWdy, 1.0);

    UV.set(uv.x/uv.w, uv.y/uv.w);
#if 0
    dUVdx.set((uvdx.x - uv.x)/uv.w, (uvdx.y - uv.y)/uv.w);
    dUVdy.set((uvdy.x - uv.x)/uv.w, (uvdy.y - uv.y)/uv.w);
#else
    dUVdx.set((uvdx.x/uvdx.w) - UV.x, (uvdx.y/uvdx.w) - UV.y);
    dUVdy.set((uvdy.x/uvdy.w) - UV.x, (uvdy.y/uvdy.w) - UV.y);
#endif

    return true;
}


#if 0
/*! The geometric surface evaluation shader call.  If doing displacement implement the
    dedicated displacement call instead.
*/
/*virtual*/
void evaluateGeometricShading(RayShaderContext& stx,
                              RayShaderContext& out)
{
    // Make sure input types are built:
    validate(1);

    // Base class call will pass it on up to input0.  Do this first so
    // that we override any mods further up:
    RayShader::evaluateGeometricShading(stx, out);

    // If no projection enabled we're done:
    if (m_project_channels == Mask_None)
        return;

    // Project the world-space point backwards through projector:
    const Fsr::Vec4f uv = m_projectconcat.transform(stx.PW, 1.0);
    // Skip uvs behind the camera:
    if (uv.w <= 0.0f)
        out.UV.set(0.0f, 0.0f);
    else
        out.UV.set(uv.x/uv.w, uv.y/uv.w);
}
#endif


/*!
*/
/*virtual*/
void
zprProject::evaluateSurface(RayShaderContext& stx,
                            Fsr::Pixel&       out)
{
    //std::cout << "zprProject::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;

    // Let the background get shaded first.
    if (getInput(BG0))
        getInput(BG0)->evaluateSurface(stx, out);
    else
        out.rgba().set(0.0f, 0.0f, 0.0f, 1.0f);

    // If no projection enabled we're done:
    if (locals.m_project_channels == DD::Image::Mask_None || inputs.k_operation == MERGE_NONE)
        return;

    // Possibly motion-blur interpolate the input camera xform matrix:
    Fsr::Mat4d proj_xform  = locals.m_projectxform;
    Fsr::Mat4d proj_concat = locals.m_projectconcat;

#if 0
    // If motion blur enabled find the motionblur 'sibling' shader to interpolate with.
    if (stx.rtx->isMotionBlurEnabled() && stx.frame_shutter_step < ((uint32_t)stx.rtx->shutter_times.size()-1))
    {
#if 0
        // Do something smarter here, like put methods on RayShader to determine the frame and
        // 'sibling' shaders early on in the RenderContext construction.  I don't think we can put
        // a mb_shader pointer on RayShader since it may be different for separate zpRender
        // contexts that share the RayShader Op:
#else
        zpProject* mb_projector = 0;
#if 1
        const GeoInfoContext* gptx = stx.rprim->surface_ctx->getGeoInfoContext();
        if (!gptx)
        {
            // Cannot evaluate as a surface, skip it:
            RayShader::evaluateSurface(stx, out);
            return;
        }

        const GeoInfoContext::Sample& gtx0 = gptx->getGeoInfoSample(stx.frame_shutter_step  );
        const GeoInfoContext::Sample& gtx1 = gptx->getGeoInfoSample(stx.frame_shutter_step+1);
#else
        GeoInfoContext::Sample& gtx0 = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(stx.frame_shutter_step  );
        GeoInfoContext::Sample& gtx1 = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(stx.frame_shutter_step+1);
#endif
        //std::cout << "gtx0=" << &gtx0 << ", gtx1=" << &gtx1 << std::endl;
        if      (gtx0.info->material != this && gtx1.info->material == this)
            mb_projector = dynamic_cast<zpProject*>(gtx0.info->material);
        else if (gtx1.info->material != this && gtx0.info->material == this)
            mb_projector = dynamic_cast<zpProject*>(gtx1.info->material);
        //if (stx.x==820&&stx.y==801) std::cout << "this=" << this << ", mb_projector=" << mb_projector << std::endl;

        if (mb_projector)
        {
            // Interpolate between the material projection transforms:
            const double f0 = stx.rtx->shutter_times[stx.frame_shutter_step  ];
            const double f1 = stx.rtx->shutter_times[stx.frame_shutter_step+1];
            const float t = float((stx.frame_time - f0) / (f1 - f0));
            proj_xform = lerp(locals.m_projectxform, mb_projector->locals.m_projectxform, t);
            proj_concat = locals.m_projectproj * proj_xform;
            //if (stx.x==820&&stx.y==801) std::cout << "0: " << locals.m_projectxform << " 1: " << mb_projector->locals.m_projectxform << " proj_xform: " << proj_xform << std::endl;
        }
#endif
    }
#endif

    // Handle front/back clipping:
    if (inputs.k_faces_mode != FACES_BOTH && locals.m_proj_cam)
    {
        // Don't project on surfaces facing away from projection camera:
        const Fsr::Vec3d Vp = (Fsr::Vec3d(locals.m_proj_cam->matrix().translation()) - stx.PW);
        const double Vp_dot_N = Vp.dot(stx.Ns);
        if ((inputs.k_faces_mode == FACES_FRONT && Vp_dot_N < 0.0f) || 
            (inputs.k_faces_mode == FACES_BACK  && Vp_dot_N > 0.0f))
        {
            // Force this surface to be transparent, allowing further-back surfaces to appear.
            // If this isn't done then this surface will appear black:
            out.erase(locals.m_project_channels);
            out[DD::Image::Chan_Alpha] = 0.0f; // make sure alpha is zero too
            return;
        }
    }

    // Handle Z-clipping:
    if (inputs.k_zclip_mode != Z_CLIP_NONE)
    {
        const Fsr::Vec3d CPW = proj_xform.transform(stx.PW);
        const double Z = ::fabs(CPW.z);
        if (inputs.k_zclip_mode == Z_CLIP_USER)
        {
            // Clip project at the user-set near/far planes:
            if (Z < locals.m_near_clip || Z > locals.m_far_clip)
                return;
        }
        else if (inputs.k_zclip_mode == Z_CLIP_CAM && locals.m_proj_cam)
        {
            // Clip project at the user-set near/far planes:
            if (Z < locals.m_cam_near || Z > locals.m_cam_far)
                return;
        }
    }

    // Calculate the projection:
    Fsr::Vec2f saved_UV    = stx.UV;
    Fsr::Vec2f saved_dUVdx = stx.dUVdx;
    Fsr::Vec2f saved_dUVdy = stx.dUVdy;
    Fsr::Vec2f UV, dUVdx, dUVdy;
    if (!project(proj_concat, stx, stx.UV, stx.dUVdx, stx.dUVdy))
    {
        stx.UV = saved_UV;
        stx.dUVdx = saved_dUVdx;
        stx.dUVdy = saved_dUVdy;
        return; // outside projection area
    }

    Fsr::Pixel tex_pixel(locals.m_project_channels);
    tex_pixel.erase();

    if (inputs.k_bindings[MAP1].isActiveColor())
    {
        //DD::Image::TextureFilter* saved_filter = stx.texture_filter;
        //stx.texture_filter = &inputs.k_texture_filter;

        tex_pixel.rgb() = inputs.k_bindings[MAP1].getValue(stx, &tex_pixel.alpha());

        //stx.texture_filter = saved_filter;
    }

    stx.UV = saved_UV;
    stx.dUVdx = saved_dUVdx;
    stx.dUVdy = saved_dUVdy;

    // Merge the chans:
    switch (inputs.k_operation)
    {
        case MERGE_REPLACE:
            out.replace(tex_pixel, locals.m_project_channels);
            break;

        case MERGE_OVER:
            out.over(tex_pixel/*A*/, tex_pixel[DD::Image::Chan_Alpha]/*alpha*/, locals.m_project_channels);
            break;

        case MERGE_UNDER:
        {
            //out.under(tex_pixel/*A*/, tex_pixel[DD::Image::Chan_Alpha]/*alpha*/, locals.m_project_channels);
            const float iBa = (1.0f - out[DD::Image::Chan_Alpha]);
            if (iBa < std::numeric_limits<float>::epsilon())
            {
                ;//
            }
            else if (iBa < 1.0f)
            {
                foreach(z, locals.m_project_channels)
                    out[z] += tex_pixel[z]*iBa;
            }
            else
            {
                foreach(z, locals.m_project_channels)
                    out[z] += tex_pixel[z];
            }
            break;
        }

        case MERGE_STENCIL:
        {
            const float iAa = 1.0f - tex_pixel[DD::Image::Chan_Alpha];
            if (iAa < std::numeric_limits<float>::epsilon())
            {
                foreach(z, locals.m_project_channels)
                    out[z] = 0.0f;
            }
            else if (iAa < 1.0f)
            {
                foreach(z, locals.m_project_channels)
                    out[z] *= iAa;
            }
            else
            {
                ;//
            }
            break;
        }

        case MERGE_MASK:
        {
            const float Aa = tex_pixel[DD::Image::Chan_Alpha];
            if (Aa < std::numeric_limits<float>::epsilon())
            {
                foreach(z, locals.m_project_channels)
                    out[z] = 0.0f;
            }
            else if (Aa < 1.0f)
            {
                foreach(z, locals.m_project_channels)
                    out[z] *= Aa;
            }
            else
            {
                ;//
            }
            break;
        }

        case MERGE_PLUS:
            foreach(z, locals.m_project_channels)
                out[z] = (out[z] + tex_pixel[z]);
            break;

        case MERGE_AVG:
            foreach(z, locals.m_project_channels)
                out[z] = (out[z] + tex_pixel[z])*0.5f;
            break;

        case MERGE_MIN:
            foreach(z, locals.m_project_channels)
                out[z] = std::min(out[z], tex_pixel[z]);
            break;

        case MERGE_MAX:
            foreach(z, locals.m_project_channels)
                out[z] = std::max(out[z], tex_pixel[z]);
            break;

        default:
        //case MERGE_NONE:
            break;
    }
}


} // namespace zpr

// end of zprProject.cpp

//
// Copyright 2020 DreamWorks Animation
//
