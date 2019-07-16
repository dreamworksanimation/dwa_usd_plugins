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

/// @file Fuser/CameraOp.h
///
/// @author Jonathan Egstad

#ifndef Fuser_CameraOp_h
#define Fuser_CameraOp_h


#include "SceneXform.h"
#include "SceneLoader.h"

#include <DDImage/CameraOp.h>
#include <DDImage/Shutter.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>


namespace Fsr {


/*! DD::Image::CameraOp wrapper adding Fuser scene loading and double-precision
    matrix support.

    Fsr::FuserCameraOp may be a little redundant but it's easier to keep straight.

    This may duplicate some code on FuserAxisOp and FuserLightOp, but we have
    to since these are subclassed off separate DD::Image::AxisOp branches.
*/
class FSR_EXPORT FuserCameraOp : public DD::Image::CameraOp,
                                 public Fsr::SceneXform,
                                 public Fsr::SceneLoader
{
  protected:
    //! Local baked down window parameters used primarily for OpenGL display
    struct BakedKnobParams
    {
        Fsr::Vec2d win_translate;
        Fsr::Vec2d win_scale;
        double     win_roll;
    };


  protected:
    DD::Image::ShutterControls k_shutter;       //!< Shutter controls which may be referenced by a renderer
    double                     k_shutter_bias;  //!< Weights the shutter samples towards shutter close with a power function
    //
    double k_world_to_meters;       //!< World to meters - used to convert lens mm values to world scale
    //
    bool   k_gl_solid_frustum;      //!< Draw the camera frustum as solid
    bool   k_gl_inside_frustum;     //!< Indicated whether to draw the frustum as an inside or outside box

    BakedKnobParams m_win_params;   //!< Used primarily for OpenGL display
    double          m_mm_to_world;  //!< Precalc'd from k_world_to_meters for OpenGL drawing convenience


  public:
    FuserCameraOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~FuserCameraOp() {}


    //! Returns op cast to Fuser types if possible, otherwise NULL.
    static FuserCameraOp* asFuserCameraOp(DD::Image::Op* op);


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
    /*virtual*/ DD::Image::Op*       sceneOp() { return this; }

    //! SceneExtender:: If extender is attached to a AxisOp subclass return 'this'.
    /*virtual*/ DD::Image::AxisOp*   asAxisOp() { return this; }

    //! SceneExtender:: If extender is attached to an CameraOp subclass return 'this'.
    /*virtual*/ DD::Image::CameraOp* asCameraOp() { return this; }

    //! Return the scene node type to use when searching for a default to load - ie 'camera', 'light', 'xform', etc.
    /*virtual*/ const char*          defaultSceneNodeType() { return "camera"; }

    //! Enable/disable any knobs that get updated by SceneLoader.
    /*virtual*/ void enableSceneLoaderExtraKnobs(bool read_enabled);


    //------------------------------------------------------------
    // DD::Image::Op/AxisOp virtual methods.

    /*virtual*/ const char*    node_help() const;

    // Redirect Op input config methods to SceneXform interface:
    /*virtual*/ int            minimum_inputs()                         const { return SceneXform::xformInputs(); }
    /*virtual*/ int            maximum_inputs()                         const { return SceneXform::xformInputs(); }
    /*virtual*/ bool           test_input(int input, DD::Image::Op* op) const { return SceneXform::testInput(input, op); }
    /*virtual*/ DD::Image::Op* default_input(int input)                 const { return SceneXform::defaultInput(input); }
    /*virtual*/ const char*    input_label(int input, char* buffer)     const { return SceneXform::inputLabel(input, buffer); }


    //!
    /*virtual*/ void append(DD::Image::Hash& hash);
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
    // DD::Image::CameraOp virtual methods.

    /*virtual*/ void projection_knobs(DD::Image::Knob_Callback);
    /*virtual*/ void lens_knobs(DD::Image::Knob_Callback);

#if 0
    // TODO: need to bother reimplementing these? Probably not.
    /*virtual*/ bool                            projection_is_linear(int mode) const;
    /*virtual*/ DD::Image::CameraOp::LensFunc*  lens_function(int mode) const;
    /*virtual*/ DD::Image::CameraOp::LensNFunc* lensNfunction(int mode) const;
#endif

    //! Returns projection of camera at current OutputContext.
    /*virtual*/ DD::Image::Matrix4 projection(int mode) const;
    //! Returns projection of camera at specific OutputContext at the current projection mode.
    /*virtual*/ DD::Image::Matrix4 projectionAt(const DD::Image::OutputContext&);

    //------------------------------------------------------------


    //! Adds the OpenGL display option controls. This is duplicated on the FuserAxisOp and FuserLightOp classes.
    virtual void addDisplayOptionsKnobs(DD::Image::Knob_Callback f);

    //! Adds the front-panel transform knobs. This is duplicated on the FuserAxisOp and FuserLightOp classes.
    virtual void addTransformKnobs(DD::Image::Knob_Callback f);

    //! Adds addl front-panel knobs. Called after addTransformKnobs() but before addProjectionTabKnobs().
    virtual void addExtraFrontPanelKnobs(DD::Image::Knob_Callback f);

    //! Create a 'Projection' node tab and add the knobs normally found there.
    virtual void addProjectionTabKnobs(DD::Image::Knob_Callback);

    //! Adds the shutter controls. By default called from addProjectionTabKnobs().
    virtual void addShuttersKnobs(DD::Image::Knob_Callback f);


    //------------------------------------------------------------


    //! Draw the camera's frustum outline solid or wireframe, dashed if 'dashed_lines'=true.
    virtual void drawCameraIcon(bool solid,
                                bool dashed_lines=false);


    //! Draw the node name at position 0,0,0 which will be the local center.
    void drawNodeName(const char* subcam_name="");

    //! Draw the node name at an xyz position relative to the local center.
    void drawNodeName(const Fsr::Vec3d& xyz,
                      const char*       subcam_name="");

    //! Draw the aperture rectangle in millimeters, centered on 'P'.
    void drawAperture(const DD::Image::Vector3& P,
                      bool                      dashed_lines=false);

    //! Draw the camera's frustum.
    void drawSolidFrustum(const DD::Image::Vector3& vn,
                          const DD::Image::Vector3& vf);
    //! Draw the camera's frustum outline, dashed if 'dashed_lines'=true.
    void drawWireFrustum(const DD::Image::Vector3& vn,
                         const DD::Image::Vector3& vf,
                         bool                      dashed_lines=false);

};


} // namespace Fsr

#endif

// end of Fuser/CameraOp.h

//
// Copyright 2019 DreamWorks Animation
//
