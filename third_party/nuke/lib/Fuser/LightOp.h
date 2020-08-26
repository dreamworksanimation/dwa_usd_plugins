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

/// @file Fuser/LightOp.h
///
/// @author Jonathan Egstad

#ifndef Fuser_LightOp_h
#define Fuser_LightOp_h


#include "SceneXform.h"
#include "SceneLoader.h"
#include "RayContext.h"

#include <DDImage/ComplexLightOp.h>
#include <DDImage/LightContext.h>
#include <DDImage/LookupCurves.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>


namespace Fsr {


/*! DD::Image::ComplexLightOp wrapper adding Fuser scene loading and double-precision
    matrix support.

    Fsr::FuserLightOp may be a little redundant but it's easier to keep straight.

    We're not bothering with wrapping the LightOp base class since ComplexLightOp only
    adds a few extra parameters which could've easily been added to LightOp...sigh...) 

    This may duplicate some code on FuserAxisOp and FuserCameraOp, but we have
    to since these are subclassed off separate DD::Image::AxisOp branches.
*/
class FSR_EXPORT FuserLightOp : public DD::Image::ComplexLightOp,
                                public Fsr::SceneXform,
                                public Fsr::SceneLoader
{
  public:
    /*! Energy falloff by distance presets.

        A 360deg emitting point light has a natural inverse-square energy
        falloff by distance. That is the energy of the emitted light
        diminishes by the inverse-square of the distance away from the
        emission source.

        Spotlights and other light types like lasers do not exhibit
        a simple inverse-square falloff as the emitted light is
        focused. In the case of a laser (or a direct light) there's
        almost no falloff, and in the case of spotlights the type of
        falloff depends on the settings of the focusing elements.

        The standard defaults assume a falloff in a vacuum, but when
        there's a participating medium like fog there's a faster
        falloff due to energy absorption.
    */
    enum FalloffType
    {
        Falloff_None,       //!< No energy falloff (a perfect laser or a direct light)
        Falloff_Linear,     //!< 
        Falloff_Square,     //!< Inverse-square
        Falloff_Cubic,      //!< Inverse cubic
        //
        Falloff_Curve       //!< User defined profile curve
    };


  protected:
    /*
      Inherited from DD::Image::CameraOp:
        double near_, far_;                 //!< Near and far Z clipping planes

      Inherited from DD::Image::LightOp:
        Pixel  color_;                      //!< Color of the light (can be >3 channels!)
        float  intensity_;                  //!< Global intensity
        bool   falloff_;                    //!< Whether to factor in physical falloff
        int    falloffType_;                //!< falloff type
        int    samples_;                    //!< The number of samples (for area testing)
        float  sample_width_;               //!< sample width (for shadowing)

        bool   cast_shadows_;               //!< Whether light casts shadows
        int    shadows_mode_;               //!< Shadow casting mode

      Inherited from DD::Image::ComplexLightOp:
        int    _lightType;                  //!< One of LightOp::LightType
        //
        double _coneAngleNotClamp;          //!< Cone angle knob value
        double _conePenumbraAngleNotClamp;  //!< Cone penumbra angle knob value
        double _coneFalloffNotClamp;        //!< Cone inner-to-outer falloff rate knob value
        //
        float  _coneAngle;                  //!< Clamped cone angle
        float  _conePenumbraAngle;          //!< Clamped cone penumbra angle
        float  _coneFalloff;                //!< Clamped cone inner-to-outer falloff rate
        //
        float  _coneOuterCo;                //!< Derived cone outer cosine angle
        float  _iconeCos;                   //!< Derived inverse cone cosine...?
        float  _coneInnerCos;               //!< Derived cone inner cosine angle
        //
        double _intensityScale;             //!< Used to bias file-loaded light values...?
    */

    bool        k_constrain_to_near_far;    //!< Constrain illumination inside near/far range
    double      k_falloff_rate_bias;        //!< Bias to the standard presets
    DD::Image::LookupCurves k_falloff_profile; //!< User-defined falloff curve lut
    //
    const char* k_light_identifier;         //!< Light identifier string (used for grouping, etc)
    const char* k_object_mask;              //!< Object name filter

    float       m_near, m_far;              //!< Clamped near_/far_ from CamerOp base class
    float       m_falloff_rate_bias;        //!< Clamped k_falloff_rate_bias


  public:
    FuserLightOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~FuserLightOp() {}


    //! Returns op cast to Fuser types if possible, otherwise NULL.
    static FuserLightOp* asFuserLightOp(DD::Image::Op* op);


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

    //! Allow subclasses to gain access to sibling functions:
    /*virtual*/ SceneXform*  asSceneXform()   { return this; }
    /*virtual*/ SceneLoader* asSceneLoader()  { return this; }

    //! SceneExtender:: If extender is attached to a AxisOp subclass return 'this'.
    /*virtual*/ DD::Image::AxisOp*   asAxisOp() { return this; }

    //! SceneExtender:: If extender is attached to an CameraOp subclass return 'this'.
    /*virtual*/ DD::Image::CameraOp* asCameraOp() { return this; }

    //! SceneExtender:: If extender is attached to an LightOp subclass return 'this'.
    /*virtual*/ DD::Image::LightOp*  asLightOp() { return this; }

    //! Return the scene node type to use when searching for a default to load - ie 'camera', 'light', 'xform', etc.
    /*virtual*/ const char*          defaultSceneNodeType() { return "light"; }

    //! Enable/disable any knobs that get updated by SceneLoader.
    /*virtual*/ void enableSceneLoaderExtraKnobs(bool enabled);


    //------------------------------------------------------------


    //! Adds the OpenGL display option controls. This is duplicated on the FuserAxisOp and FuserCameraOp classes.
    virtual void addDisplayOptionsKnobs(DD::Image::Knob_Callback f);

    //! Adds the front-panel transform knobs. This is duplicated on the FuserAxisOp and FuserCameraOp classes.
    virtual void addTransformKnobs(DD::Image::Knob_Callback f);

    //! Adds addl front-panel knobs. Called after addTransformKnobs() but before addLightKnobs().
    virtual void addExtraFrontPanelKnobs(DD::Image::Knob_Callback f);

    //! Adds the light control knobs, by default appearing above transform controls.
    virtual void addLightKnobs(DD::Image::Knob_Callback);


    //------------------------------------------------------------


    //! Intersect the ray with this light returning the intersection distances. Assumes simple geometry.
    virtual bool intersectRay(const Fsr::RayContext& Rtx,
                              double&                tmin,
                              double&                tmax);

    //! Is surface point visible from light? Normal is optional. This only works if light clearly understands its bounds.
    virtual bool canIlluminatePoint(const Fsr::Vec3d& surfP,
                                    const Fsr::Vec3f* surfN);


    //------------------------------------------------------------

    //!
    virtual void drawLightIcon(DD::Image::ViewerContext* vtx,
                               int                       display3d);

    //! Draw the node name at position 0,0,0 which will be the local center.
    void drawNodeName(int view=-1);

    //! Draw the node name at an xyz position relative to the local center.
    void drawNodeName(const Fsr::Vec3d& xyz,
                      int               view=-1);

    //------------------------------------------------------------
    // DD::Image::LightOp virtual methods.

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
    /*virtual*/ void shade_GL(DD::Image::ViewerContext* vtx,
                              int                       light_idx);

    //!
    /*virtual*/ void matrixAt(const DD::Image::OutputContext& context,
                              DD::Image::Matrix4&             matrix) const;

    //! Base class identifies as a point light by default.
    /*virtual*/ int  lightType() const { return DD::Image::LightOp::ePointLight; } 

    //! Returns the correct value for the standard defined types.
    /*virtual*/ bool is_delta_light() const;

    //! Calculate a normalized direction vector Lout and distance_out to surface point P. Supports the standard defined types.
    /*virtual*/ void get_L_vector(DD::Image::LightContext&  ltx,
                                  const DD::Image::Vector3& surfP,
                                  const DD::Image::Vector3& surfN,
                                  DD::Image::Vector3&       Lout,
                                  float&                    distance_out) const;

    //! Returns the amount of light striking the current surface point from this light. Supports the standard defined types.
    /*virtual*/ void get_color(DD::Image::LightContext&  ltx,
                               const DD::Image::Vector3& surfP,
                               const DD::Image::Vector3& surfN,
                               const DD::Image::Vector3& surfL,
                               float                     distance,
                               DD::Image::Pixel&         out);


};


} // namespace Fsr

#endif

// end of Fuser/LightOp.h

//
// Copyright 2019 DreamWorks Animation
//
