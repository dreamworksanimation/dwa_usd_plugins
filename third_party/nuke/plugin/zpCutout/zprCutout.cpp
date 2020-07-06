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

/// @file zprCutout.cpp
///
/// @author Jonathan Egstad


#include "zprCutout.h"

namespace zpr {

static RayShader* shaderBuilder() { return new zprCutout(); }
/*static*/ const RayShader::ShaderDescription zprCutout::description("zprCutout", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprCutout::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
};
/*static*/ const RayShader::OutputKnobList zprCutout::output_defs =
{
    {OutputKnob("surface", PIXEL_KNOB )},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       DOUBLE_KNOB)},
    {OutputKnob("g",       DOUBLE_KNOB)},
    {OutputKnob("b",       DOUBLE_KNOB)},
    {OutputKnob("a",       DOUBLE_KNOB)},
};


zprCutout::InputParams::InputParams() :
    k_cutout_channel(DD::Image::Chan_Mask)
{
    //
}


//!
zprCutout::zprCutout()
{
    //std::cout << "zprCutout::ctor(" << this << ")" << std::endl;
#ifdef TRY_CUTOUT_MAP
    setNumInputs(NUM_INPUTS);
#endif
}


//!
zprCutout::zprCutout(const InputParams& _inputs) :
    inputs(_inputs)
{
    //std::cout << "zprCutout::ctor(" << this << ")" << std::endl;
#ifdef TRY_CUTOUT_MAP
    setNumInputs(NUM_INPUTS);
#endif
}


#ifdef TRY_CUTOUT_MAP
/*virtual*/
void
zprCutout::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    RayShader::getActiveTextureBindings(texture_bindings); // < get the inputs
    //
    texture_bindings.push_back(&inputs.k_cutout_map);
}
#endif


/*virtual*/
InputBinding*
zprCutout::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*virtual*/
void
zprCutout::validateShader(bool                 for_real,
                          const RenderContext& rtx)
{
    RayShader::validateShader(for_real, rtx); // < get the inputs
    //
#ifdef TRY_CUTOUT_MAP
    m_texture_channels = inputs.k_cutout_map.getChannels();
#endif
    m_output_channels = inputs.k_cutout_channel;
}


/*! The ray-tracing shader call.
*/
/*virtual*/
void
zprCutout::evaluateSurface(RayShaderContext& stx,
                           Fsr::Pixel&       out)
{
    //std::cout << "zprCutout::evaluateSurface(" << this << ")" << std::endl;
    // Let the background get shaded first.
    if (getInput(0))
        getInput(0)->evaluateSurface(stx, out);
    else
        out.rgba().set(0.0f, 0.0f, 0.0f, 1.0f);

    // Clear the output channels *EXCEPT* alpha:
    const float a = out.alpha();
    out.erase();
    out.alpha() = a;

#ifdef TRY_CUTOUT_MAP
    // Modulate final color by cutout map:
    if (inputs.k_cutout_map.isActiveColor())
    {
        Vector3 op = inputs.k_cutout_map.getValue(stx, 0/*alpha_ptr*/);
        if (op.x < 0.0f)
        {
            //
        }
        else
        {
            //
        }

    }
    else
    {
        // Indicate that this surface is completely cutout:
        if (k_cutout_channel != stx.cutout_channel)
            out[inputs.k_cutout_channel] = 1.0f;
        else
            out[stx.cutout_channel] = 1.0f;
    }
#else
    // Indicate that this surface is completely cutout:
    if (inputs.k_cutout_channel != stx.cutout_channel)
        out[inputs.k_cutout_channel] = 1.0f;
    else
        out[stx.cutout_channel] = 1.0f;
#endif
}


} // namespace zpr

// end of zprCutout.cpp

//
// Copyright 2020 DreamWorks Animation
//
