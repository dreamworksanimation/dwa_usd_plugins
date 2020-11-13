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

#include "SphereVolume.h"

namespace zpr {


static RayShader* shaderBuilder() { return new zprPointLight(); }
/*static*/ const RayShader::ShaderDescription zprPointLight::description("PointLight", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprPointLight::input_defs =
{
    {InputKnob("color",     COLOR3_KNOB, "1 1 1" )},
    {InputKnob("intensity", FLOAT_KNOB,  "1"     )},
    {InputKnob("near",      FLOAT_KNOB,  "0.001" )},
    {InputKnob("far",       FLOAT_KNOB,  "100000")},
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
    inputs.k_near = 0.001;
    inputs.k_far  = 100000.0;

    // Assign the knobs to their value destinations, overwriting them:
    bindInputKnob("color",                 &inputs.k_color);
    bindInputKnob("intensity",             &inputs.k_intensity);
    bindInputKnob("near",                  &inputs.k_near);
    bindInputKnob("far",                   &inputs.k_far);
    bindInputKnob("illuminate_atmosphere", &inputs.k_illuminate_atmosphere);
}


/*!
*/
zprPointLight::zprPointLight(const InputParams&     input_params,
                             const Fsr::DoubleList& motion_times,
                             const Fsr::Mat4dList&  motion_xforms) :
    LightShader(input_defs, output_defs, motion_times, motion_xforms),
    inputs(input_params)
{
    //std::cout << "zprPointLight::ctor(" << this << ")" << std::endl;
    // Point the knobs to their already-set values:
    setInputKnobTarget("color",                 &inputs.k_color);
    setInputKnobTarget("intensity",             &inputs.k_intensity);
    setInputKnobTarget("near",                  &inputs.k_near);
    setInputKnobTarget("far",                   &inputs.k_far);
    setInputKnobTarget("illuminate_atmosphere", &inputs.k_illuminate_atmosphere);
}


/*! Initialize any uniform vars prior to rendering.
    This may be called without a RenderContext from the legacy shader system.
*/
/*virtual*/
void
zprPointLight::updateUniformLocals(double  frame,
                                   int32_t view)
{
    //std::cout << "  zprPointLight::updateUniformLocals()"<< std::endl;
    LightShader::updateUniformLocals(frame, view); // update m_color

    m_near = std::max(0.001, inputs.k_near);
    m_far  = std::max(m_near, std::min(inputs.k_far, std::numeric_limits<double>::infinity()));
    //std::cout << "m_near=" << m_near << ", m_far=" << m_far << std::endl;
}


/*!
*/
/*virtual*/
bool
zprPointLight::illuminate(RayShaderContext& stx,
                          Fsr::RayContext&  illum_ray,
                          float&            direct_pdfW_out,
                          Fsr::Pixel&       illum_color_out)
{
    //std::cout << "zprPointLight::illuminate(" << this << ")" << std::endl;
    const Fsr::Mat4d xform = getMotionXformAt(stx.frame_time);

    const Fsr::Vec3d PWlt = xform.getTranslation(); // interpolated light position
    Fsr::Vec3d L(PWlt - stx.PW);     // light vector from surface to light
    const double D = L.normalize(); // normalizing light vector returns the distance between surface and light
    if (D > m_far)
        return false; // outside light's influence

    // Update output light ray with direction and distance:
    illum_ray.set(stx.PW,
                  L/*dir*/,
                  stx.Rtx.time,
                  std::numeric_limits<double>::epsilon(),
                  D);

    if (D <= m_near)
    {
        // No attenuation within near distance:
        direct_pdfW_out = 1.0f;
    }
    else
    {
        // Distance-squared radial falloff for an infinitely-small point light:
#if 0
        const double offsetD = (D - m_near);
        const double normalizedD = 1.0 + (offsetD / m_near);

        // Base attenuation is inverse-square:
        direct_pdfW_out = float(1.0 / (normalizedD*normalizedD));

        // Scale and bias attenuation such that:
        //   direct_pdfW_out == 1 at k_near
        //   direct_pdfW_out == 0 at k_far
        const float cutoff = float(m_near);
        direct_pdfW_out = std::max(0.0f, (direct_pdfW_out - cutoff) / (1.0f - cutoff));
#else
#if 0
        direct_pdfW_out = float(1.0 / ((D - m_near)*(D - m_near)));
#else
        const double normalizedD = 1.0 - Fsr::clamp((D - m_near) / (m_far - m_near));
        direct_pdfW_out = float(std::pow(normalizedD, 2.0));
#endif
#endif
        if (direct_pdfW_out < std::numeric_limits<float>::epsilon())
            return false;
    }

    illum_color_out.rgb() = m_color.rgb();

    return true;
}


/*! Can this light shader produce a LightVolume?
    Why yes, a simple SphereVolume.
*/
/*virtual*/
bool
zprPointLight::canGenerateLightVolume()
{
    return (inputs.k_illuminate_atmosphere &&
            fabs(m_far - m_near) >= std::numeric_limits<double>::epsilon());
}


/*! Return the entire motion bbox enclosing the LightVolume that
    this shader can create during createLightVolume().

    This is a union of all the transformed motion spheres, simulated
    by transforming a box3 surrounding the sphere diameter by each
    motion xforms and concatenating the results.
*/
/*virtual*/
Fsr::Box3d
zprPointLight::getLightVolumeMotionBbox()
{
    const Fsr::Box3d sphere_bbox(-m_far, -m_far, -m_far,
                                  m_far,  m_far,  m_far);

    const size_t nMotionSamples = m_motion_times.size();
    if (nMotionSamples == 0)
        return Fsr::Box3d();
#if DEBUG
    assert(m_motion_xforms.size()  == nMotionSamples);
    assert(m_motion_ixforms.size() == nMotionSamples);
#endif
    Fsr::Box3d bbox = m_motion_xforms[0].transform(sphere_bbox);
    for (uint32_t j=1; j < nMotionSamples; ++j)
        bbox.expand(m_motion_xforms[j].transform(sphere_bbox), false/*test_empty*/);

    return bbox;
}


/*! Create a LightVolume primitive appropriate for this LightShader.
    Build the motion samples for the sphere volume prims, create and
    return a SphereVolume primitive.
*/
/*virtual*/
LightVolume*
zprPointLight::createLightVolume(const MaterialContext* material_ctx)
{
    const size_t nMotionSamples = m_motion_times.size();
#if DEBUG
    assert(m_motion_xforms.size()  == nMotionSamples);
    assert(m_motion_ixforms.size() == nMotionSamples);
#endif
    zpr::SphereVolume::SampleList motion_spheres(nMotionSamples);
    for (uint32_t j=0; j < nMotionSamples; ++j)
        motion_spheres[j].set(m_motion_xforms[j], 0.0/*near_radius*/, m_far);

    return new SphereVolume(material_ctx, m_motion_times, motion_spheres);
}


} // namespace zpr

// end of zprPointLight.cpp

//
// Copyright 2020 DreamWorks Animation
//
