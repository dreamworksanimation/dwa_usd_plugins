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
*/
class ZPR_EXPORT LightShader : public RayShader
{
  protected:
    Fsr::DoubleList  m_motion_times;    //!< Frame time for each motion-sample
    Fsr::Mat4dList   m_motion_xforms;   //!<

  public:
    //!
    LightShader(const Fsr::DoubleList& motion_times,
                const Fsr::Mat4dList&  motion_xforms);

    //! Must have a virtual destructor!
    virtual ~LightShader() {}


    //!
    static const char* zpClass();

    //! Returns the class name, must implement.
    /*virtual*/ const char* zprShaderClass() const { return "LightShader"; }


    /*! !!HACK ALERT!! This adds an invisible 'zpLightShader' knob
        that's used to identify a LightShader-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to LightShader*.

        Atm if this knob doesn't exist then the _evaludate*() methods will
        not be called since the node will not be recognized as a LightShader!
    */
    void addLightShaderIdKnob(DD::Image::Knob_Callback f);


    //!
    /*virtual*/ LightShader*  isLightShader() { return this; }


    //! Initialize any vars prior to rendering.
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);


  protected:
    //------------------------------------------------------------------
    // Subclasses implement these calls to modify the shading.
    // Called from base class high-level methods like getIllumination().
    //------------------------------------------------------------------

    //! The ray-tracing surface shader evaluation call.
    /*virtual*/ void _evaluateShading(RayShaderContext& stx,
                                      Fsr::Pixel&       out);


};


} // namespace zpr

#endif

// end of zprender/LightShader.h

//
// Copyright 2020 DreamWorks Animation
//
