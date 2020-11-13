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

static const RayShader::InputKnobList  m_default_inputs = {};
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
    m_enabled(false)
{
    //
}


/*!
*/
LightShader::LightShader(const InputKnobList&  inputs,
                         const OutputKnobList& outputs) :
    RayShader(inputs, outputs),
    m_enabled(false)
{
    m_motion_times.resize(1, 0.0);
    m_motion_xforms.resize(1, Fsr::Mat4d::getIdentity());
    m_motion_ixforms.resize(1, Fsr::Mat4d::getIdentity());
}


/*!
*/
LightShader::LightShader(const Fsr::DoubleList& motion_times,
                         const Fsr::Mat4dList&  motion_xforms) :
    RayShader(m_default_inputs, m_default_outputs),
    m_enabled(false)
{
    setMotionXforms(motion_times, motion_xforms);
}


/*!
*/
LightShader::LightShader(const InputKnobList&   inputs,
                         const OutputKnobList&  outputs,
                         const Fsr::DoubleList& motion_times,
                         const Fsr::Mat4dList&  motion_xforms) :
    RayShader(inputs, outputs),
    m_enabled(false)
{
    setMotionXforms(motion_times, motion_xforms);
}


/*! Initialize any uniform vars prior to rendering.

    LightShader base class calls calculates 'm_color' from
    'k_color' and 'k_intensity'.
*/
/*virtual*/ void
LightShader::updateUniformLocals(double  frame,
                                 int32_t view)
{
    RayShader::updateUniformLocals(frame, view);

    const BaseInputParams* inputs = uniformInputs();
    if (inputs)
    {
        m_color.setToRGBChannels();
        m_color.rgb() = inputs->k_color*inputs->k_intensity; // precalc output color
    }
    else
    {
        m_color.setToRGBChannels();
        m_color.rgb().setToZero();
    }
}


/*!
*/
/*virtual*/
void
LightShader::validateShader(bool                            for_real,
                            const RenderContext*            rtx,
                            const DD::Image::OutputContext* op_ctx)
{
    RayShader::validateShader(for_real, rtx, op_ctx);

    // Enable light id m_color.rgb() is non-zero:
    m_enabled = m_color.rgb().greaterThanZero();
}


//-----------------------------------------------------------------------------


//!
/*static*/ const char* LightShader::zpClass() { return "zpLightShader"; }


/*! Print input and output knob values to stream.
*/
/*virtual*/
void
LightShader::print(std::ostream& o) const
{
    RayShader::print(o);
}


//-----------------------------------------------------------------------------


//!
/*virtual*/
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
    m_motion_ixforms.resize(m_motion_xforms.size());
    for (size_t i=0; i < m_motion_xforms.size(); ++i)
        m_motion_ixforms[i] = m_motion_xforms[i].inverse();
}


//!
Fsr::Mat4d
LightShader::getMotionXformAt(double frame_time) const
{
    return zpr::getMotionXformAt(m_motion_times,
                                 frame_time,
                                 m_motion_xforms);
}


//!
Fsr::Mat4d
LightShader::getInverseMotionXformAt(double frame_time) const
{
    return zpr::getMotionXformAt(m_motion_times,
                                 frame_time,
                                 m_motion_ixforms);
}


//!
void
LightShader::getMotionXformsAt(double      frame_time,
                              Fsr::Mat4d& xform,
                              Fsr::Mat4d& ixform) const
{
    zpr::getMotionXformsAt(m_motion_times,
                           frame_time,
                           m_motion_xforms,
                           m_motion_ixforms,
                           xform,
                           ixform);
}


} // namespace zpr

// end of zprender/LightShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
