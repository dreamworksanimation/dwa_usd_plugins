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

/// @file zprender/VolumeShader.h
///
/// @author Jonathan Egstad


#ifndef zprender_VolumeShader_h
#define zprender_VolumeShader_h

#include "RayShader.h"
#include "Volume.h"

#include <DDImage/Knob.h>


namespace zpr {


/*! Base class of ray-tracing volume shaders.
*/
class ZPR_EXPORT VolumeShader : public RayShader
{
  public:
    //!
    VolumeShader();

    //! Must have a virtual destructor!
    virtual ~VolumeShader() {}


    /*virtual*/ VolumeShader* isVolumeShader() { return this; }

    //!
    static const char* zpClass();

    //! Returns the class name, must implement.
    /*virtual*/ const char* zprShaderClass() const { return "VolumeShader"; }


    /*! !!HACK ALERT!! This adds an invisible 'zpVolumeShader' knob
        that's used to identify a VolumeShader-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to VolumeShader*.

        Atm if this knob doesn't exist then the _evaluate*() methods will
        not be called since the node will not be recognized as a VolumeShader!
    */
    void addVolumeShaderIdKnob(DD::Image::Knob_Callback f);

    //! Initialize any vars prior to rendering.
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);


  public:
    //!
    virtual bool getVolumeIntersections(zpr::RayShaderContext&          stx,
                                        Volume::VolumeIntersectionList& vol_intersections,
                                        double&                         vol_tmin,
                                        double&                         vol_tmax,
                                        double&                         vol_depth_min,
                                        double&                         vol_depth_max) const { return false; }

    /*! Default homogenous ray march through a set of light volumes.
        If it returns false there's been a user-abort.
    */
    virtual bool volumeMarch(zpr::RayShaderContext&                stx,
                             double                                tmin,
                             double                                tmax,
                             double                                depth_min,
                             double                                depth_max,
                             float                                 surface_Z,
                             float                                 surface_alpha,
                             const Volume::VolumeIntersectionList& vol_intersections,
                             Fsr::Pixel&                           color_out,
                             Traceable::DeepIntersectionList*      deep_out=NULL) const { return false; }

};


} // namespace zpr

#endif

// end of zprender/VolumeShader.h

//
// Copyright 2020 DreamWorks Animation
//
