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

/// @file Fuser/AxisOp.h
///
/// @author Jonathan Egstad

#ifndef Fuser_AxisOp_h
#define Fuser_AxisOp_h


#include "SceneXform.h"
#include "SceneLoader.h"

#include <DDImage/AxisOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>


namespace Fsr {


/*! DD::Image::AxisOp wrapper adding Fuser scene loading and double-precision
    matrix support.

    Fsr::FuserAxisOp may be a little redundant but it's easier to keep straight.

    This may duplicate some code on FuserCameraOp and FuserLightOp, but we have
    to since these are subclassed off separate DD::Image::AxisOp branches.
*/
class FSR_EXPORT FuserAxisOp : public DD::Image::AxisOp,
                               public Fsr::SceneXform,
                               public Fsr::SceneLoader
{
  public:
    FuserAxisOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~FuserAxisOp() {}


    //! Returns op cast to Fuser types if possible, otherwise NULL.
    static FuserAxisOp* asFuserAxisOp(DD::Image::Op* op);


    //------------------------------------------------------------
    // SceneXform virtual methods:

    //! SceneXform:: Return the parenting input number, or -1 if the parenting source is local. Must implement.
    /*virtual*/ int parentingInput() const { return 0; }

    //! SceneXform:: Return the lookat input number, or -1 if the lookat source is local. Must implement.
    /*virtual*/ int lookatInput() const { return 1; }

    //! SceneXform:: If attached Op has an Axis_knob to fill in for the local transform, return it. Must implement.
    ///*virtual*/ Knob* localTransformKnob() const { return this->knob("transform"); }


    //------------------------------------------------------------
    // SceneExtender/SceneLoader virtual methods:

    //! SceneExtender:: Should return 'this'. Must implement.
    /*virtual*/ DD::Image::Op*     sceneOp() { return this; }

    //! SceneExtender:: If extender is attached to an AxisOp subclass return 'this'.
    /*virtual*/ DD::Image::AxisOp* asAxisOp() { return this; }

    //! Allow subclasses to gain access to sibling functions:
    /*virtual*/ SceneXform*  asSceneXform()   { return this; }
    /*virtual*/ SceneLoader* asSceneLoader()  { return this; }

    //------------------------------------------------------------
    // DD::Image::AxisOp virtual methods.

    /*virtual*/ const char*    node_help() const;

    // Redirect Op input config methods to SceneXform interface:
    /*virtual*/ int            minimum_inputs()                         const { return SceneXform::xformInputs(); }
    /*virtual*/ int            maximum_inputs()                         const { return SceneXform::xformInputs(); }
    /*virtual*/ bool           test_input(int input, DD::Image::Op* op) const { return SceneXform::testInput(input, op); }
    /*virtual*/ DD::Image::Op* default_input(int input)                 const { return SceneXform::defaultInput(input); }
    /*virtual*/ const char*    input_label(int input, char* buffer)     const { return SceneXform::inputLabel(input, buffer); }


    //!
    /*virtual*/ void knobs(DD::Image::Knob_Callback);
    /*virtual*/ int  knob_changed(DD::Image::Knob*);
    /*virtual*/ void _validate(bool for_real);
    /*virtual*/ void build_handles(DD::Image::ViewerContext*);
    /*virtual*/ void draw_handle(DD::Image::ViewerContext*);

    //!
    /*virtual*/ void matrixAt(const DD::Image::OutputContext& context,
                              DD::Image::Matrix4&             matrix) const;

    //------------------------------------------------------------


    //! Adds the OpenGL display option controls. This is duplicated on the FuserCameraOp and FuserLightOp classes.
    virtual void addDisplayOptionsKnobs(DD::Image::Knob_Callback f);

    //! Adds the front-panel transform knobs. This is duplicated on the FuserCameraOp and FuserLightOp classes.
    virtual void addTransformKnobs(DD::Image::Knob_Callback f);

    //! Adds addl front-panel knobs. Called after addTransformKnobs(). Base class adds nothing.
    virtual void addExtraFrontPanelKnobs(DD::Image::Knob_Callback f);


    //------------------------------------------------------------


    //! Draw the node name at position 0,0,0 which will be the local center.
    void drawNodeName();

    //! Draw the node name at an xyz position relative to the local center.
    void drawNodeName(const Fsr::Vec3d& xyz);
};


} // namespace Fsr

#endif

// end of Fuser/AxisOp.h

//
// Copyright 2019 DreamWorks Animation
//
