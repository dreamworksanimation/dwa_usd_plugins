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

/// @file zprAttributeReader.cpp
///
/// @author Jonathan Egstad


#include "zprAttributeReader.h"


namespace zpr {

static RayShader* shaderBuilder() { return new zprAttributeReader(); }
/*static*/ const RayShader::ShaderDescription zprAttributeReader::description("AttributeReader", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprAttributeReader::input_defs =
{
    {InputKnob("attrib_name",         STRING_KNOB)},
    {InputKnob("attrib_group",        STRING_KNOB)},
};
/*static*/ const RayShader::OutputKnobList zprAttributeReader::output_defs =
{
    {OutputKnob("surface",            STRING_KNOB)},
    {OutputKnob("displacement",       STRING_KNOB)},
};



/*!
*/
zprAttributeReader::zprAttributeReader() :
    RayShader(input_defs, output_defs)
{
    //std::cout << "zprAttributeReader::ctor(" << this << ")" << std::endl;
    assert(m_inputs.size() > 0 && m_inputs.size() == input_defs.size());
}


/*!
*/
zprAttributeReader::~zprAttributeReader()
{
}


/*!
*/
/*virtual*/
void
zprAttributeReader::validateShader(bool                 for_real,
                                   const RenderContext& rtx)
{
std::cout << "zprAttributeReader::validateShader()" << std::endl;
    //m_texture_channels = m_binding.getChannels();
    //m_output_channels = m_texture_channels;
}


/*!
*/
/*virtual*/
void
zprAttributeReader::evaluateSurface(RayShaderContext& stx,
                                    Fsr::Pixel&       out)
{
std::cout << "zprAttributeReader::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;
    out.rgb().set(0.0f);
    out.alpha() = 1.0f;
}


} // namespace zpr

// end of zprAttributeReader.cpp

//
// Copyright 2020 DreamWorks Animation
//
