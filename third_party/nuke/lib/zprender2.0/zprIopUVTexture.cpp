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

/// @file zprIopUVTexture.cpp
///
/// @author Jonathan Egstad


#include "zprIopUVTexture.h"

namespace zpr {


static RayShader* shaderBuilder() { return new zprIopUVTexture(); }
/*static*/ const RayShader::ShaderDescription zprIopUVTexture::description("IopUVTexture", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprIopUVTexture::input_defs =
{
    //{InputKnob("iop",      OP_KNOB)},
    {InputKnob("uv",       VEC2_KNOB, "0.5 0.5")},
};
/*static*/ const RayShader::OutputKnobList zprIopUVTexture::output_defs =
{
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("r",       FLOAT_KNOB )},
    {OutputKnob("g",       FLOAT_KNOB )},
    {OutputKnob("b",       FLOAT_KNOB )},
    {OutputKnob("a",       FLOAT_KNOB )},
};


/*!
*/
zprIopUVTexture::zprIopUVTexture(DD::Image::Iop* iop) :
    RayShader(input_defs, output_defs)
{
    //std::cout << "zprIopUVTexture::ctor(" << this << ")" << std::endl;
    if (iop)
    {
        m_binding = InputBinding::buildInputTextureBinding(iop,
                                                           DD::Image::Chan_Red,
                                                           DD::Image::Chan_Green,
                                                           DD::Image::Chan_Blue,
                                                           DD::Image::Chan_Alpha);
    }
}


/*!
*/
zprIopUVTexture::zprIopUVTexture(const InputBinding& binding) :
    RayShader(),
    m_binding(binding)
{
    //std::cout << "zprIopUVTexture::ctor(" << this << ")" << std::endl;
}


/*! Initialize any uniform vars prior to rendering.
    This may be called without a RenderContext from the legacy shader system.
*/
/*virtual*/
void
zprIopUVTexture::updateUniformLocals(double  frame,
                                     int32_t view)
{
    //std::cout << "  zprIopUVTexture::updateUniformLocals()"<< std::endl;
    RayShader::updateUniformLocals(frame, view);
}


/*!
*/
/*virtual*/
void
zprIopUVTexture::validateShader(bool                            for_real,
                                const RenderContext*            rtx,
                                const DD::Image::OutputContext* op_ctx)
{
    //std::cout << "zprIopUVTexture::validateShader()" << std::endl;
    RayShader::validateShader(for_real, rtx, op_ctx); // updates the uniform locals

    m_texture_channels = m_binding.getChannels();
    m_output_channels  = m_texture_channels;
    //std::cout << "  texture_channels=" << m_texture_channels << std::endl;
    //std::cout << "  output_channels=" << m_output_channels << std::endl;
}


/*!
*/
/*virtual*/
void
zprIopUVTexture::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    if (m_binding.isActiveTexture())
        texture_bindings.push_back(&m_binding);
}


/*!
*/
/*virtual*/
void
zprIopUVTexture::evaluateSurface(RayShaderContext& stx,
                                 Fsr::Pixel&       out)
{
    //std::cout << "zprIopUVTexture::evaluateSurface(" << this << ")" << std::endl;
    if (m_texture_channels.empty())
    {
        //out.setChannels(getChannels());
        //out[m_binding.rgb_chans[0]] = 0.0f;
        //out[m_binding.rgb_chans[1]] = 0.0f;
        //out[m_binding.rgb_chans[2]] = 0.0f;
        //out[m_binding.opacity_chan] = 0.0f;
        out.rgb().set(0.0f);
        out.alpha() = 1.0f;
    }
    else
    {
        m_binding.sampleTexture(stx, out);
        if (!m_binding.hasAlpha())
            out.alpha() = 1.0f;
    }
}


} // namespace zpr

// end of zprIopUVTexture.cpp

//
// Copyright 2020 DreamWorks Animation
//
