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
/*static*/ const RayShader::ShaderDescription zprOcclusion::description("zprOcclusion", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprOcclusion::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
};
/*static*/ const RayShader::OutputKnobList zprOcclusion::output_defs =
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
zprOcclusion::InputParams::InputParams()
{
    k_amb_ocl_enabled     = true;
    k_refl_ocl_enabled    = false;
    k_amb_ocl_mindist     =    0.0;
    k_amb_ocl_maxdist     = 1000.0;
    k_amb_ocl_cone_angle  =  180.0; // 180 degree cone
    k_refl_ocl_mindist    =    0.0;
    k_refl_ocl_maxdist    = 1000.0;
    k_refl_ocl_cone_angle =   20.0; // 20 degree cone
    k_gi_scale            =    1.0;
    //
    k_amb_ocl_output  = DD::Image::Chan_Black;
    k_refl_ocl_output = DD::Image::Chan_Black;
}


/*!
*/
zprOcclusion::zprOcclusion() :
    RayShader(input_defs, output_defs)
{
    //
}


/*!
*/
zprOcclusion::zprOcclusion(const InputParams& _inputs) :
    RayShader(input_defs, output_defs),
    inputs(_inputs)
{
    //std::cout << "zprOcclusion::ctor(" << this << ")" << std::endl;
    //
}


/*static*/
void
zprOcclusion::updateLocals(const InputParams& _inputs,
                           LocalVars&         _locals)
{
    // Precalculate and clamp some shader params:
    _locals.m_amb_ocl_cone_angle = float(clamp(_inputs.k_amb_ocl_cone_angle, 0.0, 180.0));
    _locals.m_amb_ocl_mindist = std::max(0.001, fabs(_inputs.k_amb_ocl_mindist));
    _locals.m_amb_ocl_maxdist = std::min(fabs(_inputs.k_amb_ocl_maxdist), std::numeric_limits<double>::infinity());

    _locals.m_refl_ocl_cone_angle = float(clamp(_inputs.k_refl_ocl_cone_angle, 0.0, 180.0));
    _locals.m_refl_ocl_mindist = std::max(0.001, fabs(_inputs.k_refl_ocl_mindist));
    _locals.m_refl_ocl_maxdist = std::min(fabs(_inputs.k_refl_ocl_maxdist), std::numeric_limits<double>::infinity());

}


/*virtual*/
InputBinding*
zprOcclusion::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*virtual*/
void
zprOcclusion::validateShader(bool                 for_real,
                             const RenderContext& rtx)
{
    RayShader::validateShader(for_real, rtx); // < get the inputs
    //std::cout << "zprOcclusion::validateShader() bg0=" << getInput(BG0) << std::endl;

    updateLocals(inputs, locals);

    // Enable AOV output channels:
    m_output_channels  = DD::Image::Mask_RGBA;
    m_output_channels += inputs.k_amb_ocl_output;
    m_output_channels += inputs.k_refl_ocl_output;
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
                                       Fsr::RayContext::DIFFUSE,
                                       locals.m_amb_ocl_mindist,
                                       locals.m_amb_ocl_maxdist,
                                       locals.m_amb_ocl_cone_angle,
                                       float(inputs.k_gi_scale));
    }

    if (inputs.k_refl_ocl_enabled)
    {
         refl_ocl_weight = getOcclusion(stx,
                                        Fsr::RayContext::GLOSSY,
                                        locals.m_refl_ocl_mindist,
                                        locals.m_refl_ocl_maxdist,
                                        locals.m_refl_ocl_cone_angle,
                                        float(inputs.k_gi_scale));
    }

    // Get the input shading result AFTER occlusion calc (just in case stx gets messed with):
    if (getInput(BG0))
        getInput(BG0)->evaluateSurface(stx, out);
    else
        out.rgba().set(0.0f, 0.0f, 0.0f, 1.0f);

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
