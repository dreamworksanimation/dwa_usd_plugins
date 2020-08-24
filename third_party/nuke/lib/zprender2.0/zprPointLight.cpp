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

/// @file zprPointLight.cpp
///
/// @author Jonathan Egstad


#include "zprPointLight.h"
#include "RenderContext.h"


namespace zpr {


static RayShader* shaderBuilder() { return new zprPointLight(); }
/*static*/ const RayShader::ShaderDescription zprPointLight::description("PointLight", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprPointLight::input_defs =
{
    {InputKnob("color",     COLOR3_KNOB, "1 1 1")},
    {InputKnob("intensity", FLOAT_KNOB,  "1"    )},
};
/*static*/ const RayShader::OutputKnobList zprPointLight::output_defs =
{
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       FLOAT_KNOB )},
    {OutputKnob("g",       FLOAT_KNOB )},
    {OutputKnob("b",       FLOAT_KNOB )},
};


/*!
*/
zprPointLight::zprPointLight() :
    LightShader(input_defs, output_defs)
{
    //std::cout << "zprPointLight::ctor(" << this << ")" << std::endl;
}


/*!
*/
/*virtual*/
void
zprPointLight::validateShader(bool                 for_real,
                              const RenderContext& rtx)
{
    //std::cout << "zprPointLight::validateShader(" << this << ")"<< std::endl;
    LightShader::validateShader(for_real, rtx); // get light color set
}


/*!
*/
/*virtual*/
bool
zprPointLight::illuminateSurface(const RayShaderContext& stx,
                                 Fsr::RayContext&        light_ray,
                                 float&                  direct_pdfW_out,
                                 Fsr::Pixel&             light_color_out)
{
    //std::cout << "zprPointLight::illuminateSurface(" << this << ")" << std::endl;
    const Fsr::Mat4d xform = getMotionXform(stx.frame_time);

    const Fsr::Vec3d PWlt = xform.getTranslation(); // interpolated light position
    Fsr::Vec3d L(PWlt - stx.PW);     // light vector from surface to light
    const double D = L.normalize(); // normalizing light vector returns the distance between surface and light

    // Update output light ray with direction and distance:
    light_ray.set(stx.PW, L/*dir*/, stx.Rtx.time, std::numeric_limits<double>::epsilon(), D);

    // Distance-squared radial falloff for an infinitely-small point light:
    // TODO: take near/far into account for bias?
    direct_pdfW_out = float(D*D);

    light_color_out.rgb() = m_color;

    return true;
}


} // namespace zpr

// end of zprPointLight.cpp

//
// Copyright 2020 DreamWorks Animation
//
