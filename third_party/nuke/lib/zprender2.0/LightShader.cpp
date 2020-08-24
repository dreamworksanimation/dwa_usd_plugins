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

/// @file zprender/LightShader.cpp
///
/// @author Jonathan Egstad


#include "LightShader.h"
#include "RenderContext.h"


namespace zpr {

static const RayShader::InputKnob      m_empty_input;
static const RayShader::OutputKnob     m_empty_output;

static const RayShader::InputKnobList m_default_inputs =
{
    {RayShader::InputKnob("color",     RayShader::COLOR3_KNOB, "1 1 1")},
    {RayShader::InputKnob("intensity", RayShader::FLOAT_KNOB,  "1"    )},
};
static const RayShader::OutputKnobList m_default_outputs =
{
    {RayShader::OutputKnob("rgb",      RayShader::COLOR3_KNOB)},
    {RayShader::OutputKnob("r",        RayShader::FLOAT_KNOB )},
    {RayShader::OutputKnob("g",        RayShader::FLOAT_KNOB )},
    {RayShader::OutputKnob("b",        RayShader::FLOAT_KNOB )},
};



/*!
*/
LightShader::LightShader() :
    RayShader(m_default_inputs, m_default_outputs),
    m_enabled(true)
{
    assignInputKnob("color",     &k_color);
    assignInputKnob("intensity", &k_intensity);
}


LightShader::LightShader(const InputKnobList&  inputs,
                         const OutputKnobList& outputs) :
    RayShader(inputs, outputs),
    m_enabled(false)
{
    assignInputKnob("color",     &k_color);
    assignInputKnob("intensity", &k_intensity);

    //m_motion_times.resize(1, motion_time);
}


//-----------------------------------------------------------------------------


//!
/*static*/ const char* LightShader::zpClass() { return "zpLightShader"; }

/*!
*/
void
LightShader::addLightShaderIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, LightShader::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-----------------------------------------------------------------------------


//!
void
LightShader::setMotionXforms(const Fsr::DoubleList& motion_times,
                             const Fsr::Mat4dList&  motion_xforms)
{
    m_motion_times = motion_times;
    m_motion_xforms = motion_xforms;
#if DEBUG
    assert(m_motion_times.size() > 0);
    assert(m_motion_xforms.size() == m_motion_times.size());
#endif
}


//!
Fsr::Mat4d
LightShader::getMotionXform(double frame_time) const
{
    // Don't crash, just return identity():
    if (m_motion_xforms.size() == 0)
        return Fsr::Mat4d::getIdentity();

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, frame_time, motion_step, motion_step_t);
#if DEBUG
    assert(motion_step < m_motion_xforms.size());
#endif

    if (motion_mode == MOTIONSTEP_START)
        return m_motion_xforms[motion_step ];
    else if (motion_mode == MOTIONSTEP_END)
        return m_motion_xforms[motion_step+1];

    return Fsr::lerp(m_motion_xforms[motion_step], m_motion_xforms[motion_step+1], motion_step_t);
}


/*!
*/
/*virtual*/ void
LightShader::validateShader(bool                 for_real,
                            const RenderContext& rtx)
{
    m_color = k_color*k_intensity; // precalc output color
    m_enabled = m_color.greaterThanZero();
}


} // namespace zpr

// end of zprender/LightShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
