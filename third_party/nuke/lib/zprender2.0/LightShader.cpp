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


/*!
*/
LightShader::LightShader(const Fsr::DoubleList& motion_times,
                         const Fsr::Mat4dList&  motion_xforms) :
    RayShader(),
    m_motion_times(motion_times),
    m_motion_xforms(motion_xforms)
{
#if DEBUG
    assert(motion_times.size() > 0);
    assert(motion_xforms.size() == motion_times.size());
#endif
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


/*!
*/
/*virtual*/ void
LightShader::validateShader(bool                 for_real,
                            const RenderContext& rtx)
{
}


//-----------------------------------------------------------------------------


/*! TODO: this is all temp - finish!  (make a new shader method for lights)
*/
/*virtual*/
void
LightShader::_evaluateShading(RayShaderContext& stx,
                              Fsr::Pixel&       out)
{
    //std::cout << "LightShader::_evaluateShading(" << this << "):" << std::endl;
#if DEBUG
    assert(m_motion_xforms.size() > 0);
#endif
    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_xforms.size());
#endif

    Fsr::Mat4d xform;
    if (motion_mode == MOTIONSTEP_START)
        xform = m_motion_xforms[motion_step ];
    else if (motion_mode == MOTIONSTEP_END)
        xform = m_motion_xforms[motion_step+1];
    else
        xform = Fsr::lerp(m_motion_xforms[motion_step], m_motion_xforms[motion_step+1], motion_step_t);

    const Fsr::Vec3d P = xform.getTranslation();
    Fsr::Vec3d L(stx.PW - P);
//const float D = float(L.normalize());

    const float N_dot_L = float(stx.Nf.dot(-L));
    if (N_dot_L <= 0.0f)
    {
        foreach(z, out.channels)
            out[z] = 0.0f;
        return;
    }

const Fsr::Vec3f m_color(1,1,1);
const float      m_intensity = 1.0f;

    float i = 0.0f;
#if 1
    i = m_intensity;
#else
    switch (falloffType_)
    {
        default:
        case eNoFalloff:        i = m_intensity; break;
        case eLinearFalloff:    i = m_intensity / D; break;
        case eQuadraticFalloff: i = m_intensity / (D*D); break;
        case eCubicFalloff:     i = m_intensity / (D*D*D); break;
    }
#endif
    //std::cout << "  P" << P << ", L" << L << ", D=" << D << ", i=" << i << std::endl;

    foreach(z, out.channels)
        out[z] = 1.0f * i * N_dot_L;

#if 0
    const int type = lightType();
    if (type == eDirectionalLight)
    {
        // Direct light illumination angle is always the same:
        Lout = -matrix().z_axis();
        distance_out = (surfP - ltx.p()).length();
    }
    else
    {
        // 
        Lout = surfP - ltx.p();
        distance_out = Lout.normalize(); // returns the length of Lout before normalizing
    }

    // Modify intensity by distance from emission source (falloff):
    float intensity = fabsf(intensity_);

    // If constraining illumination range change distance to position inside near/far:
    if (k_constrain_to_near_far || DD::Image::LightOp::falloffType() == Falloff_Curve)
        distance = 1.0f - clamp((distance - m_near) / (m_far - m_near));

    switch (DD::Image::LightOp::falloffType())
    {
        case Falloff_None:   break;
        case Falloff_Linear: intensity *= powf(distance, m_falloff_rate_bias     ); break;
        case Falloff_Square: intensity *= powf(distance, m_falloff_rate_bias*2.0f); break;
        case Falloff_Cubic:  intensity *= powf(distance, m_falloff_rate_bias*3.0f); break;
        case Falloff_Curve:  intensity *= float(clamp(k_falloff_profile.getValue(0.0, distance)));
    }

    // TODO: this should clamp the channel set to the intersection...
    foreach(z, out.channels)
        out[z] = color_[z]*intensity;
#endif
}


} // namespace zpr

// end of zprender/LightShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
