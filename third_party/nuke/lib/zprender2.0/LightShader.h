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

/// @file zprender/LightShader.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightShader_h
#define zprender_LightShader_h

#include "RayShader.h"

namespace zpr {


/*! Base class of ray-tracing light shaders.
    Currently only the transform supports motionblur.

    TODO: add Sample struct to support animating color, etc?
*/
class ZPR_EXPORT LightShader : public RayShader
{
  public:
    Fsr::Vec3f k_color;         //!<
    float      k_intensity;     //!<


  protected:
    Fsr::DoubleList  m_motion_times;    //!< Frame time for each motion-sample
    Fsr::Mat4dList   m_motion_xforms;   //!<
    Fsr::Vec3f       m_color;           //!< Output color - usually k_color*k_intensity
    bool             m_enabled;         //!< Light can illuminate scene (m_color > 0)


  public:
    //! Sets color & intensity to 1.0
    LightShader();
    //!
    LightShader(const InputKnobList&  inputs,
                const OutputKnobList& output);

    //! Must have a virtual destructor!
    virtual ~LightShader() {}


    //---------------------------------------------------------


    //!
    static const char* zpClass();

    //! Returns the class name, must implement.
    /*virtual*/ const char* zprShaderClass() const { return "LightShader"; }

    static const ShaderDescription description;

    /*! !!HACK ALERT!! This adds an invisible 'zpLightShader' knob
        that's used to identify a LightShader-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to LightShader*.

        Atm if this knob doesn't exist then the evaluate*() methods will
        not be called since the node will not be recognized as a LightShader!
    */
    void addLightShaderIdKnob(DD::Image::Knob_Callback f);


    //---------------------------------------------------------


    //!
    /*virtual*/ LightShader*  isLightShader() { return this; }


    //!
    void setMotionXforms(const Fsr::DoubleList& motion_times,
                         const Fsr::Mat4dList&  motion_xforms);

    //!
    Fsr::Mat4d getMotionXform(double frame_time) const;


    //---------------------------------------------------------


    //! Initialize any vars prior to rendering.
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);

    //!
    bool isEnabled() const { return m_enabled; }


    /*! Evaluate the light's contribution to a suface intersection.
        Returns false if light does not contribute to surface illumination.
        Must implement.

        'light_ray' is built in the LightShader and normally points from surface to
        light origin and can be used for shadowing, specular angle, etc.

        light_ray.maxdist should be the distance between surface point and light,
        for shadow intersection and falloff determination.
        light_ray.mindist should be set to 0 or an epsilon bias off surface.

        direct_pdfW_out is the direct lighting power distribution function weight
        of the light for light_ray.
    */
    virtual bool illuminateSurface(const RayShaderContext& stx,
                                   Fsr::RayContext&        light_ray,
                                   float&                  direct_pdfW_out,
                                   Fsr::Pixel&             light_color_out)=0;

};


} // namespace zpr

#endif

// end of zprender/LightShader.h

//
// Copyright 2020 DreamWorks Animation
//
