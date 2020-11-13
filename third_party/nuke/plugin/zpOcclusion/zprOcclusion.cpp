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

/// @file zpOcclusion.cpp
///
/// @author Jonathan Egstad


#include "zprOcclusion.h"

#include <zprender/RenderContext.h>


namespace zpr {


static RayShader* shaderBuilder() { return new zprOcclusion(); }
/*static*/ const RayShader::ShaderDescription zprOcclusion::description("Occlusion", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprOcclusion::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
};
/*static*/ const RayShader::OutputKnobList zprOcclusion::output_defs =
{
    {OutputKnob("surface", PIXEL_KNOB )},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       FLOAT_KNOB )},
    {OutputKnob("g",       FLOAT_KNOB )},
    {OutputKnob("b",       FLOAT_KNOB )},
    {OutputKnob("a",       FLOAT_KNOB )},
};


/*!
*/
zprOcclusion::zprOcclusion() :
    RayShader(input_defs, output_defs)
{
    inputs.k_amb_ocl_enabled     = true;
    inputs.k_refl_ocl_enabled    = false;
    inputs.k_amb_ocl_mindist     =    0.0;
    inputs.k_amb_ocl_maxdist     = 1000.0;
    inputs.k_amb_ocl_cone_angle  =  180.0; // 180 degree cone
    inputs.k_refl_ocl_mindist    =    0.0;
    inputs.k_refl_ocl_maxdist    = 1000.0;
    inputs.k_refl_ocl_cone_angle =   20.0; // 20 degree cone
    inputs.k_gi_scale            =    1.0;
    //
    inputs.k_amb_ocl_output  = DD::Image::Chan_Black;
    inputs.k_refl_ocl_output = DD::Image::Chan_Black;

    //bindInputKnob("amb_ocl_enabled", &inputs.k_amb_ocl_enabled);
}


/*!
*/
zprOcclusion::zprOcclusion(const InputParams& input_params) :
    RayShader(input_defs, output_defs),
    inputs(input_params)
{
    //std::cout << "zprOcclusion::ctor(" << this << ")" << std::endl;
    //setInputKnobTarget("amb_ocl_enabled", &inputs.k_amb_ocl_enabled);
}


/*virtual*/
InputBinding*
zprOcclusion::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*! Initialize any uniform vars prior to rendering.
    This may be called without a RenderContext from the legacy shader system.
*/
/*virtual*/
void
zprOcclusion::updateUniformLocals(double  frame,
                                   int32_t view)
{
    //std::cout << "  zprOcclusion::updateUniformLocals()"<< std::endl;
    RayShader::updateUniformLocals(frame, view);

    // Precalculate and clamp some shader params:
    m_amb_ocl_cone_angle = float(clamp(inputs.k_amb_ocl_cone_angle, 0.0, 180.0));
    m_amb_ocl_mindist = std::max(0.001, fabs(inputs.k_amb_ocl_mindist));
    m_amb_ocl_maxdist = std::min(fabs(inputs.k_amb_ocl_maxdist), std::numeric_limits<double>::infinity());

    m_refl_ocl_cone_angle = float(clamp(inputs.k_refl_ocl_cone_angle, 0.0, 180.0));
    m_refl_ocl_mindist = std::max(0.001, fabs(inputs.k_refl_ocl_mindist));
    m_refl_ocl_maxdist = std::min(fabs(inputs.k_refl_ocl_maxdist), std::numeric_limits<double>::infinity());

}


/*virtual*/
void
zprOcclusion::validateShader(bool                            for_real,
                             const RenderContext*            rtx,
                             const DD::Image::OutputContext* op_ctx)
{
    RayShader::validateShader(for_real, rtx, op_ctx); // validate inputs, update uniforms
    //std::cout << "zprOcclusion::validateShader() bg0=" << getInputShader(BG0) << std::endl;

    // Enable AOV output channels:
    m_output_channels  = DD::Image::Mask_RGBA;
    m_output_channels += inputs.k_amb_ocl_output;
    m_output_channels += inputs.k_refl_ocl_output;

    m_texture_channels = DD::Image::Mask_None;
    m_output_channels  = DD::Image::Mask_None;
}


/*!
*/
/*virtual*/
void
zprOcclusion::evaluateSurface(RayShaderContext& stx,
                              Fsr::Pixel&       out)
{
    //std::cout << "zprOcclusion::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;
    float amb_ocl_weight  = 1.0f;
    float refl_ocl_weight = 1.0f;
    if (inputs.k_amb_ocl_enabled)
    {
         amb_ocl_weight = getOcclusion(stx,
                                       Fsr::RayContext::diffusePath(),
                                       m_amb_ocl_mindist,
                                       m_amb_ocl_maxdist,
                                       m_amb_ocl_cone_angle,
                                       float(inputs.k_gi_scale));
    }

    if (inputs.k_refl_ocl_enabled)
    {
         refl_ocl_weight = getOcclusion(stx,
                                        Fsr::RayContext::glossyPath(),
                                        m_refl_ocl_mindist,
                                        m_refl_ocl_maxdist,
                                        m_refl_ocl_cone_angle,
                                        float(inputs.k_gi_scale));
    }

    // Get the input shading result AFTER occlusion calc (just in case stx gets messed with):
    if (getInputShader(BG0))
        getInputShader(BG0)->evaluateSurface(stx, out);
    else
        out.rgba().set(1.0f, 1.0f, 1.0f, 1.0f);

    // Apply occlusion weights:
    if (inputs.k_amb_ocl_enabled)
    {
        const float wt = (1.0f - amb_ocl_weight);
        out.rgb() *= wt;

        // Copy AOVs only if they're not overwriting RGBA:
        if (inputs.k_amb_ocl_output > DD::Image::Chan_Alpha)
        {
            out.channels += inputs.k_amb_ocl_output;
            out[inputs.k_amb_ocl_output] = wt;
        }
    }
    if (inputs.k_refl_ocl_enabled)
    {
        const float wt = (1.0f - refl_ocl_weight);
        out.rgb() *= wt;

        // Copy AOVs only if they're not overwriting RGBA:
        if (inputs.k_refl_ocl_output > DD::Image::Chan_Alpha)
        {
            out.channels += inputs.k_refl_ocl_output;
            out[inputs.k_refl_ocl_output] = wt;
        }
    }
}


} // namespace zpr

// end of zprOcclusion.cpp

//
// Copyright 2020 DreamWorks Animation
//
