//
// Copyright 2019 DreamWorks Animation
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

/// @file Fuser/SceneOpExtender.h
///
/// @author Jonathan Egstad

#ifndef Fuser_SceneOpExtender_h
#define Fuser_SceneOpExtender_h

#include "api.h"

#include <DDImage/AxisOp.h>
#include <DDImage/CameraOp.h>
#include <DDImage/GeoOp.h>
#include <DDImage/Iop.h>


namespace Fsr {


/*! \class Fsr::SceneOpExtender

    \brief Interface extender base class.
*/
class FSR_EXPORT SceneOpExtender
{
  public:
    //! Must have a virtual destructor!
    virtual ~SceneOpExtender() {}

    //--------------------------------------------------------------------
    // Must implement these pure virtuals:
    //--------------------------------------------------------------------

    //! Return the Op the interface is attached to. Should return 'this'. Must implement.
    virtual DD::Image::Op*       sceneOp()=0;


    //--------------------------------------------------------------------
    // These methods should be implemented on a subclass depending on
    // the sceneOp's class:
    //--------------------------------------------------------------------

    //! If extender is attached to an AxisOp subclass return 'this'. Default returns NULL.
    virtual DD::Image::AxisOp*   asAxisOp() { return NULL; }

    //! If extender is attached to a CameraOp subclass return 'this'. Default returns NULL.
    virtual DD::Image::CameraOp* asCameraOp() { return NULL; }

    //! If extender is attached to a LightOp subclass return 'this'. Default returns NULL.
    virtual DD::Image::LightOp*  asLightOp() { return NULL; }

    //! If extender is attached to a GeoOp subclass return 'this'. Default returns NULL.
    virtual DD::Image::GeoOp*    asGeoOp() { return NULL; }

    //! If extender is attached to an Iop subclass return 'this'. Default returns NULL.
    virtual DD::Image::Iop*      asIop() { return NULL; }


    //! Return the scene node type to use when searching for a default to load - ie 'camera', 'light', 'xform', etc.
    virtual const char*          defaultSceneNodeType() { return "xform"; }


    //---------------------------------------------------------------------


    //! Call this from owner Op::knob_changed().
    virtual int knobChanged(DD::Image::Knob* k,
                            int              call_again=0) { return call_again; }

    //! Call this from owner Op::build_handles().
    virtual void buildHandles(DD::Image::ViewerContext*) {}


    //---------------------------------------------------------------------

};


} // namespace Fsr


#endif

// end of FuserSceneOpExtender.h


//
// Copyright 2019 DreamWorks Animation
//
