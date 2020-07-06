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

/// @file zprModify.cpp
///
/// @author Jonathan Egstad


#include "zprModify.h"
#include <zprender/ThreadContext.h>

namespace zpr {

/*static*/ const char* const zprModify::xform_modes[] = { "none", "local-to-world", "world-to-local", 0 };
/*static*/ const char* const zprModify::shader_target_names[] = { "P", "N", "N+Ng", "UV", "Cf", "rgba-out", "rgb-out", 0 };
/*static*/ const char* const zprModify::operation_types[] = { "replace", "over", "under", "multiply", "add", "subtract", 0 };


static RayShader* shaderBuilder() { return new zprModify(); }
/*static*/ const RayShader::ShaderDescription zprModify::description("zprModify", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprModify::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
    {InputKnob("map",      PIXEL_KNOB)}, // MAP1
};
/*static*/ const RayShader::OutputKnobList zprModify::output_defs =
{
    {OutputKnob("surface", PIXEL_KNOB )},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       DOUBLE_KNOB)},
    {OutputKnob("g",       DOUBLE_KNOB)},
    {OutputKnob("b",       DOUBLE_KNOB)},
    {OutputKnob("a",       DOUBLE_KNOB)},
};


zprModify::InputParams::InputParams()
{
    DD::Image::ChannelSet Mask_N;
    Mask_N += (DD::Image::Channel)DD::Image::Chan_Nx;
    Mask_N += (DD::Image::Channel)DD::Image::Chan_Ny;
    Mask_N += (DD::Image::Channel)DD::Image::Chan_Nz;
    k_map.setChannels(Mask_N);  k_map.clear();
    k_matrix        = XFORM_NONE;
    k_operation     = OP_REPLACE;
    k_map_scale.set(1.0f);
    k_opacity_scale = 1.0f;
    k_shader_target = TARGET_N_IN;
}


/*!
*/
zprModify::zprModify() :
    RayShader(input_defs, output_defs)
{
    //std::cout << "zprModify::ctor(" << this << ")" << std::endl;
}


/*!
*/
zprModify::zprModify(const InputParams& _inputs) :
    RayShader(input_defs, output_defs),
    inputs(_inputs)
{
    //std::cout << "zprModify::ctor(" << this << ")" << std::endl;
}


/*virtual*/
InputBinding*
zprModify::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*!
*/
/*virtual*/
void
zprModify::validateShader(bool                 for_real,
                          const RenderContext& rtx)
{
    RayShader::validateShader(for_real, rtx); // < get the inputs
    //std::cout << "zprModify::validateShader() bg0=" << getInput(BG0) << ", map1=" << getInput(MAP1) << std::endl;

    m_texture_channels = DD::Image::Mask_None;
    m_output_channels  = DD::Image::Mask_None;

    const InputBinding& map1 = inputs.k_bindings[MAP1];
    if (map1.isTextureIop())
    {
        m_texture_channels = map1.asTextureIop()->channels();

        switch (inputs.k_shader_target)
        {
            case TARGET_P_IN:
            case TARGET_N_IN:
            case TARGET_UV_IN:
                // Only affect the upward stx values, not the output:
                m_texture_channels &= DD::Image::Mask_RGB;
                break;

            case TARGET_CF_IN:
                // Only affect the upward stx values, not the output:
                m_texture_channels &= DD::Image::Mask_RGBA;
                break;

            case TARGET_RGB_OUT:
                m_texture_channels &= DD::Image::Mask_RGB;
                m_output_channels = DD::Image::Mask_RGB;
                break;

            case TARGET_RGBA_OUT:
                m_texture_channels &= DD::Image::Mask_RGBA;
                m_output_channels = DD::Image::Mask_RGBA;
                break;
        }
        m_texture_channels &= map1.getChannels();
    }
}


/*!
*/
/*virtual*/
void
zprModify::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    RayShader::getActiveTextureBindings(texture_bindings); // < get the inputs

    for (uint32_t i=0; i < NUM_INPUTS; ++i)
        if (inputs.k_bindings[i].isActiveTexture())
            texture_bindings.push_back(&inputs.k_bindings[i]);
}


/*!
*/
/*virtual*/
void
zprModify::evaluateSurface(RayShaderContext& stx,
                           Fsr::Pixel&       out)
{
    //std::cout << "zprModify::evaluateSurface(" << this << ")" << std::endl;
    Fsr::Pixel& map = stx.thread_ctx->binding_color;

    InputBinding& map1 = inputs.k_bindings[MAP1];
    if (map1.isActiveColor())
    {
        map1.getValue(stx, map);
        map.rgb() *= inputs.k_map_scale;
        if (inputs.k_shader_target == TARGET_RGB_OUT)
            map.alpha() = inputs.k_opacity_scale;
        else
            map.alpha() *= inputs.k_opacity_scale;

        // Don't apply color here, do it after input shader call:
        if (inputs.k_shader_target < TARGET_RGBA_OUT)
        {
#if 1
            const GeoInfoContext* gptx = stx.rprim->surface_ctx->parent_object_ctx->asGeoObject();
            if (!gptx)
            {
                // Cannot evaluate as a surface, skip it:
                RayShader::evaluateSurface(stx, out);
                return;
            }

            const bool is_identity = gptx->getGeoInfoSample(0).xform_is_identity;
#else
            bool is_identity = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(0).xform_is_identity;
#endif

            switch (inputs.k_shader_target)
            {
                case TARGET_P_IN: {
                    Fsr::Vec3f& P = map.rgb();
                    if (is_identity)
                        ; // do nothing
                    else if (inputs.k_matrix == XFORM_LOCAL_TO_WORLD)
                        stx.PW = stx.PWg = stx.l2w->transform(Fsr::Vec3d(P));
                    else if (inputs.k_matrix == XFORM_WORLD_TO_LOCAL)
                        stx.PW = stx.PWg = stx.w2l->transform(Fsr::Vec3d(P));
                    break;}

                case TARGET_N_IN: {
                    Fsr::Vec3f& N = map.rgb();
                    N.normalize();
                    // Transform by inverse transposed:
                    if      (is_identity) ; // do nothing
                    else if (inputs.k_matrix == XFORM_LOCAL_TO_WORLD) { N = stx.w2l->normalTransform(N); N.normalize(); }
                    else if (inputs.k_matrix == XFORM_WORLD_TO_LOCAL) { N = stx.l2w->normalTransform(N); N.normalize(); }
                    stx.N  = stx.Ns = N; // assign shading-normal(N) & shading-normal-no-bump(Ns)
                    stx.Nf = faceOutward(N, stx); // Facing-outward shading normal
                    break;}

                case TARGET_N_NG_IN: {
                    Fsr::Vec3f& N = map.rgb();
                    N.normalize();
                    // Transform by inverse transposed:
                    if      (is_identity) ; // do nothing
                    else if (inputs.k_matrix == XFORM_LOCAL_TO_WORLD) { N = stx.w2l->normalTransform(N); N.normalize(); }
                    else if (inputs.k_matrix == XFORM_WORLD_TO_LOCAL) { N = stx.l2w->normalTransform(N); N.normalize(); }
                    stx.N  = stx.Ns = N; // assign shading-normal(N) & shading-normal-no-bump(Ns)
                    stx.Nf = stx.Ng = N;
                    break;}

                case TARGET_UV_IN:
                    stx.UV.set(map.r(), map.g());
                    break;

                case TARGET_CF_IN:
                    stx.Cf = map.rgba();
                    break;
            }
        }
    }

    // Call the input shader with a possibly modified shader context:
    if (getInput(BG0))
        getInput(BG0)->evaluateSurface(stx, out);
    else
        out.rgba().set(0.0f, 0.0f, 0.0f, 1.0f);

    if (inputs.k_shader_target >= TARGET_RGBA_OUT && map1.isActiveColor())
    {
        switch (inputs.k_operation)
        {
            case OP_REPLACE:  out.replace(map); break;
            case OP_OVER:     out.over(map, map.alpha()); break;
            case OP_UNDER:    out.under(map, out.alpha()); break;
            case OP_MULT:     out *= map; break;
            case OP_ADD:      out += map; break;
            case OP_SUBTRACT: out *= map; break;
        }
    }

}

} // namespace zpr

// end of zprModify.cpp

//
// Copyright 2020 DreamWorks Animation
//
