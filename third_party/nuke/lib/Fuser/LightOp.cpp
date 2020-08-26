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

/// @file Fuser/LightOp.cpp
///
/// @author Jonathan Egstad

#include "LightOp.h"
#include "NukeKnobInterface.h" // for getBoolValue

#include <DDImage/gl.h>

namespace Fsr {

float zero[4] = {0, 0, 0, 0};
float  one[4] = {1, 1, 1, 1};

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
static const DD::Image::CurveDescription falloff_profile_default[] =
{
   { "falloff", "y L 1 0 s0" },
   { 0 }
};
#else
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static const DD::Image::CurveDescription falloff_profile_default[] =
{
   { "falloff", "y L 1 0 s0" },
   { 0 }
};
#   pragma GCC diagnostic pop
#endif

//! These match DD::Image::LightOp but adds the user-curve option.
static const char* const falloff_modes[] =
{
    "No Falloff",       // eNoFalloff
    "Linear",           // eLinearFalloff
    "Quadratic",        // eQuadraticFalloff
    "Cubic",            // eCubicFalloff
    //
    // Below here is extensions of DD::Image::LightOp:
    "profile-curve",    // User-defined falloff curve
    0
};

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#ifdef FUSER_USE_KNOB_RTTI
const char* FuserLightOpRTTIKnob = "FsrLightOp";
#endif


/*!
*/
FuserLightOp::FuserLightOp(::Node* node) :
    DD::Image::ComplexLightOp(node),
    Fsr::SceneXform(),
    Fsr::SceneLoader(),
    k_falloff_profile(falloff_profile_default)
{
    k_scene_ctls.S_enable   = false; // don't import light scale by default
    //
    near_                   = 0.001;
    far_                    = 1.0;
    k_constrain_to_near_far = false;
    k_falloff_rate_bias     = 1.0;
    k_light_identifier      = "";
    k_object_mask           = "*";
}


/*! Returns op cast to Fuser types if possible, otherwise NULL.

    For a statically-linked Fuser lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/ FuserLightOp*
FuserLightOp::asFuserLightOp(DD::Image::Op* op)
{
#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Test for dummy knob so we can test for class without using RTTI...:
    if (op && op->knob(FuserLightOpRTTIKnob) != NULL)
        return reinterpret_cast<FuserLightOp*>(op);
    return NULL;
#else
    return dynamic_cast<FuserLightOp*>(op);
#endif
}


/*!
*/
/*virtual*/
const char*
FuserLightOp::node_help() const
{
    return
    __DATE__ " " __TIME__ " "
    "Defines a 3D transformation.\n"
    "Connecting this as the input to another 3D object (or another Axis) will cause "
    "that object's transformation to be parented to this one.\n"
    "\n"
    SCENE_LOADER_HELP"\n"  // scene file loading
    "\n"
    SCENE_XFORM_HELP; // parenting
}


/*! DD::Image::Op method.
    Calls the knobs methods in this order:

    Front-panel tab:
      addSceneLoaderKnobs()
      ---------------------------
      addDisplayOptionsKnobs()
      ---------------------------
      addLightKnobs()
      addTransformKnobs()
      addExtraFrontPanelKnobs()
*/
/*virtual*/
void
FuserLightOp::knobs(DD::Image::Knob_Callback f)
{
    //std::cout << "FuserLightOp::knobs(" << this->node_name() << ") makeKnobs=" << f.makeKnobs() << std::endl;

#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Dummy knob so we can test for class without using RTTI...:
    int dflt=0; DD::Image::Int_knob(f, &dflt, FuserLightOpRTTIKnob, DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::NO_ANIMATION | DD::Image::Knob::NO_RERENDER);
#endif

    Fsr::SceneLoader::addSceneLoaderKnobs(f);

    DD::Image::Divider(f);
    addDisplayOptionsKnobs(f);

    addLightKnobs(f);

    DD::Image::Divider(f);
    addTransformKnobs(f);
    addExtraFrontPanelKnobs(f);
}


/*! Adds the OpenGL display option controls.

    This code is duplicated on the FuserAxisOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserLightOp::addDisplayOptionsKnobs(DD::Image::Knob_Callback f)
{
    DD::Image::Enumeration_knob(f, &display3d_, DD::Image::display3d_names_source, "display", "display");
        DD::Image::Tooltip(f, "How to draw this Op in 3D preview (Viewer can override this setting.)");
    DD::Image::Bool_knob(f, &selectable_, "selectable", "selectable");
        DD::Image::Tooltip(f, "Turn off to prevent picking with the mouse in the viewer.");
    // Adds the 'editable' switch:
    Fsr::SceneLoader::addDisplayOptionsKnobs(f);
}


/*! Adds the front-panel transform knobs.

    This code is duplicated on the FuserAxisOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserLightOp::addTransformKnobs(DD::Image::Knob_Callback f)
{
    Fsr::SceneXform::addParentingKnobs(f, true/*group_open*/);
    DD::Image::Newline(f);

    /* Allow protected LightOp knobs to be set by SceneXform interface by passing
       their target vars in. This avoids SceneXform needing to be a subclass of
       LightOp.

       LightOp.h:
        Matrix4 localtransform_;    //!< Local matrix that Axis_Knob fills in
        Matrix4 local_;             //!< Local matrix after look at performed
        Matrix4 matrix_;            //!< Object matrix - local&parent
        Matrix4 imatrix_;           //!< Inverse object matrix
        bool    inversion_updated;  //!< Whether imatrix_ is valid

        Axis_KnobI* axis_knob;      //!< reference to the transformation knob

        WorldMatrixProvider* _worldMatrixProvider;

        int     display3d_;         //!< GUI display setting
        bool    selectable_;        //!< GUI selectable checkmark
    */
    //LightOp::knobs(f);
    SceneXform::_addAxisOpTransformKnobs(f,
                                         &this->localtransform_,
                                         &this->axis_knob,
                                         &this->_worldMatrixProvider);
    SceneXform::addLookatKnobs(f);
}


/*! Adds addl front-panel knobs.
    Called after addTransformKnobs() but before addLightKnobs().
    Base class adds nothing.
*/
/*virtual*/
void
FuserLightOp::addExtraFrontPanelKnobs(DD::Image::Knob_Callback f)
{
    //
}


/*! Adds the light control knobs, by default appearing above transform controls.

    Base class adds the controls for a point light.
*/
/*virtual*/
void
FuserLightOp::addLightKnobs(DD::Image::Knob_Callback f)
{
    Newline(f);
    bool dummy_val=true;
    Bool_knob(f, &dummy_val, "sync_light_controls", "sync light controls");
        SetFlags(f, DD::Image::Knob::EARLY_STORE);
        Tooltip(f, "If enabled and 'read from file' is true, sync the light controls to "
                   "the scene file data, overwriting (*destroying*) any user-assigned values.\n"
                   "\n"
                   "When disabled the light controls are *not* overwritten and remain "
                   "available for user-assigned values.");
    Newline(f);

    DD::Image::ComplexLightOp::color_knobs(f);
    DD::Image::Double_knob(f, &near_, DD::Image::IRange(0.001, 10.0), "near");
    DD::Image::Double_knob(f, &far_,  DD::Image::IRange(1.0, 1000.0), "far" );

    //DD::Image::Double_knob(f, &k_falloff_rate, IRange(0.0, 2.0), "falloff_rate", "falloff rate");
    //DD::Image::ComplexLightOp::attenuation_knobs(f);

    DD::Image::String_knob(f, &k_light_identifier, "light_identifier", "light identifier");
       DD::Image::Tooltip(f, "Identifier string used by object light masks.  If this is empty the "
                             "node name is used.");
    DD::Image::String_knob(f, &k_object_mask, "object_mask", "object mask");
       DD::Image::Tooltip(f, "List of object names to illuminate.\n"
                             "Supports wildcard characters '*' and '?'.");

    //DD::Image::ComplexLightOp::shadow_knobs(f);

    //------------------------

    //depthmap_slope_bias 0.01

    //DD::Image::Tab_knob(f, "Falloff Profile");
    //DD::Image::Bool_knob(f, &kFalloffLutEnable, "falloff_profile_enable", "enable falloff profile");
    //DD::Image::LookupCurves_knob(f, &kFalloffLut, "falloff_profile", "falloff profile");
}


/*!
*/
/*virtual*/
int
FuserLightOp::knob_changed(DD::Image::Knob* k)
{
    //std::cout << "FuserLightOp::knob_changed(" << k->name() << ")" << std::endl;
    int call_again = 0;

    // Let interfaces handle their changes:
    call_again =  SceneXform::knobChanged(k, call_again);
    call_again = SceneLoader::knobChanged(k, call_again);

    if (k->name() == "sync_light_controls")
    {
        enableSceneLoaderExtraKnobs(isSceneLoaderEnabled());
        call_again = 1; // we want to be called again
    }

    // Let base class handle their changes:
    if (DD::Image::ComplexLightOp::knob_changed(k))
        call_again = 1;

    return call_again;
}




/*! Enable/disable any knobs that get updated by SceneLoader.
*/
/*virtual*/
void
FuserLightOp::enableSceneLoaderExtraKnobs(bool enabled)
{
    if (!getBoolValue(knob("sync_light_controls"), true))
        enabled = true;

    DD::Image::Knob* k;
    // Standard light knobs:
    k = knob("color"    ); if (k) k->enable(enabled);
    k = knob("intensity"); if (k) k->enable(enabled);
    k = knob("near"     ); if (k) k->enable(enabled);
    k = knob("far"      ); if (k) k->enable(enabled);
}


/*!
    TODO: finish this - we need to handle the cone angles, etc.
*/
/*virtual*/
void
FuserLightOp::_validate(bool for_real)
{
    //std::cout << "  FuserLightOp::_validate(" << this->node_name() << ") for_real=" << for_real << std::endl;

    // Check for any loader errors:
    SceneLoader::validateSceneLoader(for_real);

    // Builds the double-precision matrices replacing the stock single-precision ones,
    // then saves that result in the single-precision ones so that built in code still
    // works:
    //LightOp::_validate(for_real);
    SceneXform::_validateAxisOpMatrices(for_real,
                                        &localtransform_,
                                        &local_,
                                        &matrix_,
                                        &inversion_updated);

    //std::cout << "      localtransform_" << localtransform_ << std::endl;
    //std::cout << "               local_" << local_ << std::endl;
    //std::cout << "              matrix_" << matrix_ << std::endl;
    //std::cout << "   inversion_updated=" << inversion_updated << std::endl;

    // Clamped standard control values:
    m_near       = float(std::min(fabs(near_), fabs(far_)));
    m_far        = float(std::max(fabs(near_), fabs(far_)));
    m_falloff_rate_bias = float(1.0 / clamp(k_falloff_rate_bias, 0.0001, 5.0));

    const int type = lightType();
    if (type == eDirectionalLight)
    {
        projection_mode_ = LENS_ORTHOGRAPHIC;
    }
    else
    {
        projection_mode_ = LENS_PERSPECTIVE;
    }
}


/*!
*/
/*virtual*/
void
FuserLightOp::matrixAt(const DD::Image::OutputContext& context,
                       DD::Image::Matrix4&             matrix) const
{
    matrix = SceneXform::getWorldTransformAt(context).asDDImage();
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Intersect the ray with this light returning the intersection distances. Assumes simple geometry.
    TODO: finish this!
*/
/*virtual*/
bool
FuserLightOp::intersectRay(const Fsr::RayContext& Rtx,
                           double&                tmin,
                           double&                tmax)
{
    // TODO: implement basic sphere intersection.
    return false;
}


/*!
    TODO: finish this!
*/
/*virtual*/
bool
FuserLightOp::canIlluminatePoint(const Fsr::Vec3d& surfP,
                                 const Fsr::Vec3f* surfN)
{
    return false;
}


//-------------------------------------------------------------------------


/*! Returns the correct value for the standard defined types.
*/
/*virtual*/
bool
FuserLightOp::is_delta_light() const
{
    //DD::Image::ComplexLightOp::is_delta_light();

    const int type = lightType();
    return (type != eDirectionalLight);
}


/*! Calculate a normalized direction vector Lout and distance_out to surface point P.
    Supports the standard defined types.

    TODO: finish this - we need to handle the cone angles, etc.
*/
/*virtual*/
void
FuserLightOp::get_L_vector(DD::Image::LightContext&  ltx,
                           const DD::Image::Vector3& surfP,
                           const DD::Image::Vector3& surfN,
                           DD::Image::Vector3&       Lout,
                           float&                    distance_out) const
{
    //DD::Image::ComplexLightOp::get_L_vector(ltx, surfP, surfN, Lout, distance_out);

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
}


/*! Returns the amount of light striking the current surface point from this light.
    Supports the standard defined types.

    For ray-tracing lights this should also take shadowing into consideration rather
    than relying on LightOp::get_shadowing() to be called from a surface shader.

    TODO: finish this - we need to handle the cone angles, etc.
*/
/*virtual*/
void
FuserLightOp::get_color(DD::Image::LightContext&  ltx,
                        const DD::Image::Vector3& surfP,
                        const DD::Image::Vector3& surfN,
                        const DD::Image::Vector3& surfL,
                        float                     distance,
                        DD::Image::Pixel&         out)
{
    //DD::Image::ComplexLightOp::get_color(ltx, surfP, surfN, surfL, distance, out);

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
}


//-------------------------------------------------------------------------


/*! Much of this code is duplicated on the FuserAxisOp and FuserCameraOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserLightOp::build_handles(DD::Image::ViewerContext* vtx)
{
    DD::Image::Matrix4 saved_matrix = vtx->modelmatrix;

    // Go up the inputs asking them to build their handles.
    // Do this first so that other ops always have a chance to draw!
    DD::Image::Op::build_input_handles(vtx);  // inputs are drawn in current world space

    if (node_disabled())
        return;

    // Only draw the Light's icon in 3D view:
    if (vtx->transform_mode() == DD::Image::VIEWER_2D)
        return;

    vtx->modelmatrix = saved_matrix;

    this->validate(false); // get transforms up to date

    // Local knobs are drawn/manipulated in parent's space context,
    // so mult in just parent xform. vtx->modelmatrix will be saved
    // in each build-knob entry:
    // TODO: mult the double-precision matrices together first so there's only one down convert here
    vtx->modelmatrix *= m_input_matrix.asDDImage();
    vtx->modelmatrix *= m_parent_matrix.asDDImage();

    // Let op build any of its local-space handles (3D transform, 2D controls, etc):
    if (k_editable)
        DD::Image::Op::build_knob_handles(vtx);

    // Only draw the camera icon if viewer is in 3D mode:
    if (vtx->viewer_mode() > DD::Image::VIEWER_2D &&
        DD::Image::ComplexLightOp::display3d_)
    {
        DD::Image::Op::add_draw_handle(vtx);

        // Expand the Viewer selection bbox to include the location of the Xform.
        // If is_selected is true then the Viewer's *active* bbox is also expanded
        // so user-focus will include this location.
        const bool is_selected = node_selected();
        const Fsr::Vec3d location = m_local_matrix.getTranslation();
        vtx->expand_bbox(is_selected, float(location.x), float(location.y), float(location.z));
    }

    vtx->addLight(this);
    vtx->addCamera(this); // add a camera so we can look through the light in the Viewer

    vtx->modelmatrix = saved_matrix; // don't leave matrix messed up
}


/*! Much of this code is duplicated on the FuserAxisOp and FuserCameraOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserLightOp::draw_handle(DD::Image::ViewerContext* vtx)
{
    //std::cout << "FuserLightOp::draw_handle()" << std::endl;
    // TODO: change this logic if we want to view the stereo controls while locked!
    // If the user has locked to this camera, don't bother
    // drawing the icon in the 3D view since we won't see it.
    // In the 2D view we want to see the frame lines:
    if (vtx->locked() && vtx->viewer_mode() <= DD::Image::VIEWER_PERSP && vtx->camera() == this)
        return;

    // Disallow selection if the selectable button is disabled:
    // TODO: support a 'modifiable' mode vs. 'selectable'?
    if (!selectable_ && vtx->hit_detect())
        return;

    // In 3D this method is usually called with event sequence DRAW_OPAQUE, DRAW_STIPPLED, DRAW_LINES

    const bool is_selected = node_selected();
    const int  display3d   = vtx->display3d((DD::Image::Display3DMode)this->display3d_);
    if (display3d == DD::Image::VIEWER_2D && !is_selected)
        return;

    // If we're selecting in the viewer and the icon wants to be drawn solid
    // then make sure that happens even in pick mode:
    const bool select_body_solid = (display3d > DD::Image::DISPLAY_WIREFRAME &&
                                    (vtx->event() == DD::Image::DRAW_OPAQUE ||
                                        vtx->event() == DD::Image::PUSH));

    // Model matrix will include the parent transform assigned in build_handles(),
    // so mult in only the local:
    Fsr::Mat4d gl_modelmatrix(vtx->modelmatrix);
    gl_modelmatrix *= m_local_matrix;

    double light_scale = 1.0;

    // The icon scaling factor is derived from how far the Viewer camera is
    // to the point passed to icon_size(). This is clamped to have the icon
    // appear a 'real-life' size when close up but not disappear when far away.
    const Fsr::Vec3d location = m_local_matrix.getTranslation();
    const double icon_scale = vtx->icon_size(float(location.x), float(location.y), float(location.z));
    if (icon_scale > 1.0)
        light_scale = icon_scale;

    // Draw the light shape during line pass:
    if (vtx->event() >= DD::Image::DRAW_STIPPLED)
    {
        glLoadMatrixd(gl_modelmatrix.array());

        // Draw the name:
        DD::Image::glColor(vtx->fg_color());
        drawNodeName();

        if (DD::Image::style_needs_wireframe(display3d) || is_selected || select_body_solid)
        {
            // Sphereish shape:
            glColor3f(color_[DD::Image::Chan_Red],
                      color_[DD::Image::Chan_Green],
                      color_[DD::Image::Chan_Blue]);
            glScaled(light_scale, light_scale, light_scale);
            drawLightIcon(vtx, display3d);
        }
    }
}


//-------------------------------------------------------------------------


/*! Base class draws a point light icon.

    TODO: finish this - we need to handle the light types.
*/
/*virtual*/
void
FuserLightOp::drawLightIcon(DD::Image::ViewerContext* vtx,
                            int                       display3d)
{
    // If subclass using 'eOtherLight' type hasn't implemented
    // this method it gets drawn as a point light:

    // Visualize point light's far-extent as a sphere. Do this before
    // icons scale so that the sphere is always in world-space units:
    if (m_far > 0.001)
        DD::Image::gl_sphere(float(m_far));

    // Draw omnidirectional 'emission' lines a bit thicker:
    GLint cur_width;
    glGetIntegerv(GL_LINE_WIDTH, &cur_width);
    glLineWidth(4);

    glBegin(GL_LINES);
    //------------------- straight rays ---------------------
    glVertex3f(-0.5f, 0.0f, 0.0f); glVertex3f(-1.0f, 0.0f, 0.0f);
    glVertex3f( 0.5f, 0.0f, 0.0f); glVertex3f( 1.0f, 0.0f, 0.0f);
    glVertex3f( 0.0f,-0.5f, 0.0f); glVertex3f( 0.0f,-1.0f, 0.0f);
    glVertex3f( 0.0f, 0.5f, 0.0f); glVertex3f( 0.0f, 1.0f, 0.0f);
    glVertex3f( 0.0f, 0.0f,-0.5f); glVertex3f( 0.0f, 0.0f,-1.0f);
    glVertex3f( 0.0f, 0.0f, 0.5f); glVertex3f( 0.0f, 0.0f, 1.0f);
    //------------------- top angled rays ---------------------
    glVertex3f(-0.25f, 0.35f, 0.25f); glVertex3f(-0.5f, 0.7f, 0.5f);
    glVertex3f(-0.25f, 0.35f,-0.25f); glVertex3f(-0.5f, 0.7f,-0.5f);
    glVertex3f( 0.25f, 0.35f, 0.25f); glVertex3f( 0.5f, 0.7f, 0.5f);
    glVertex3f( 0.25f, 0.35f,-0.25f); glVertex3f( 0.5f, 0.7f,-0.5f);
    //------------------- bottom angled rays ------------------
    glVertex3f(-0.25f,-0.35f, 0.25f); glVertex3f(-0.5f,-0.7f, 0.5f);
    glVertex3f(-0.25f,-0.35f,-0.25f); glVertex3f(-0.5f,-0.7f,-0.5f);
    glVertex3f( 0.25f,-0.35f, 0.25f); glVertex3f( 0.5f,-0.7f, 0.5f);
    glVertex3f( 0.25f,-0.35f,-0.25f); glVertex3f( 0.5f,-0.7f,-0.5f);
    glEnd();

    glLineWidth(float(cur_width)); // restore the width
}


/*!
*/
void
FuserLightOp::drawNodeName(int view)
{
    drawNodeName(Fsr::Vec3d(0,0,0), view);
}

/*!
*/
void
FuserLightOp::drawNodeName(const Fsr::Vec3d& xyz,
                           int               view)
{
    std::stringstream ss;
    DD::Image::Op::print_name(ss);

    if (view >= 0)
        ss << "(" << DD::Image::OutputContext::viewname(view) << ")";

    glRasterPos3d(xyz.x, xyz.y, xyz.z);
    DD::Image::gl_text(ss.str().c_str());
}


/*! Configure OpenGL for a pointlight simulation by default.

    TODO: finish this - we need to handle the light types.
*/
/*virtual*/
void
FuserLightOp::shade_GL(DD::Image::ViewerContext* vtx,
                       int                       light_idx)
{
    if (node_disabled())
        return;

    //DD::Image::ComplexLightOp::shade_GL(vtx, light_idx);

    light_idx += GL_LIGHT0; // offset into GL light list
    Fsr::Vec4f value;

    // Set light origin:
    value = m_local_matrix.getTranslation();
    value.w = (lightType()==eDirectionalLight)?0.0:1.0; // Setting w=0 indicates it's a direct light
    glLightfv(light_idx, GL_POSITION, (GLfloat*)value.array()/*param_value*/);
    //
    glLightfv(light_idx, GL_AMBIENT,  zero);
    value = ((Fsr::Vec3f&)color_[DD::Image::Chan_Red])*intensity_; value.w = 0.0;
    glLightfv(light_idx, GL_DIFFUSE,  (GLfloat*)value.array());
    glLightfv(light_idx, GL_SPECULAR, (GLfloat*)value.array());
    //
    glLightf(light_idx, GL_SPOT_CUTOFF, 180.0f);
    switch (DD::Image::LightOp::falloffType())
    {
    case eNoFalloff:
        // No falloff
        glLightf(light_idx, GL_CONSTANT_ATTENUATION,  1.0f);
        glLightf(light_idx, GL_LINEAR_ATTENUATION,    0.0f);
        glLightf(light_idx, GL_QUADRATIC_ATTENUATION, 0.0f);
        break;
    case eLinearFalloff:
        // Linear falloff
        glLightf(light_idx, GL_CONSTANT_ATTENUATION,  0.0f);
        glLightf(light_idx, GL_LINEAR_ATTENUATION,    1.0f);
        glLightf(light_idx, GL_QUADRATIC_ATTENUATION, 0.0f);
        break;
    case eQuadraticFalloff:
        // Quadratic falloff
        glLightf(light_idx, GL_CONSTANT_ATTENUATION,  0.0f);
        glLightf(light_idx, GL_LINEAR_ATTENUATION,    0.0f);
        glLightf(light_idx, GL_QUADRATIC_ATTENUATION, 1.0f);
        break;
    case eCubicFalloff:
        // Can't simulate cubic?
        break;
    }
    glEnable(light_idx);
}


} // namespace Fsr


// end of Fuser/LightOp.cpp

//
// Copyright 2019 DreamWorks Animation
//
