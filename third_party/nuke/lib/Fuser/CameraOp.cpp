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

/// @file Fuser/CameraOp.cpp
///
/// @author Jonathan Egstad

#include "CameraOp.h"

#include <DDImage/gl.h>

namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#ifdef FUSER_USE_KNOB_RTTI
const char* FuserCameraOpRTTIKnob = "FsrCameraOp";
#endif


/*!
*/
FuserCameraOp::FuserCameraOp(::Node* node) :
    DD::Image::CameraOp(node),
    Fsr::SceneXform(),
    Fsr::SceneLoader()
{
    k_scene_ctls.S_enable = false; // don't import camera scale by default

    k_shutter.setDuration(0.5);
    k_shutter.setOffset(DD::Image::ShutterControls::eStartOffset);
    k_shutter.setCustomOffset(0.0);
    k_shutter_bias      =   0.0;
    //
    k_world_to_meters   =   1.0; // default to meters-to-meters
    //
    k_gl_solid_frustum  = false;
    k_gl_inside_frustum = false;
}


/*! Returns op cast to Fuser types if possible, otherwise NULL.

    For a statically-linked Fuser lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/ FuserCameraOp*
FuserCameraOp::asFuserCameraOp(DD::Image::Op* op)
{
#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Test for dummy knob so we can test for class without using RTTI...:
    if (op && op->knob(FuserCameraOpRTTIKnob) != NULL)
        return reinterpret_cast<FuserCameraOp*>(op);
    return NULL;
#else
    return dynamic_cast<FuserCameraOp*>(op);
#endif
}


/*!
*/
/*virtual*/
const char*
FuserCameraOp::node_help() const
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
      addTransformKnobs()
      addExtraFrontPanelKnobs()

    Projection tab:
      addProjectionTabKnobs()
*/
/*virtual*/
void
FuserCameraOp::knobs(DD::Image::Knob_Callback f)
{
    //std::cout << "FuserCameraOp::knobs(" << this->node_name() << ") makeKnobs=" << f.makeKnobs() << std::endl;

#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Dummy knob so we can test for class without using RTTI...:
    int dflt=0; DD::Image::Int_knob(f, &dflt, FuserCameraOpRTTIKnob, DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::NO_ANIMATION | DD::Image::Knob::NO_RERENDER);
#endif

    Fsr::SceneLoader::addSceneLoaderKnobs(f);

    DD::Image::Divider(f);
    addDisplayOptionsKnobs(f);

    DD::Image::Divider(f);
    addTransformKnobs(f);
    addExtraFrontPanelKnobs(f);

    DD::Image::Tab_knob(f, 0, "Projection");
    addProjectionTabKnobs(f);
}


/*! Adds the OpenGL display option controls.

    This code is duplicated on the FuserAxisOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserCameraOp::addDisplayOptionsKnobs(DD::Image::Knob_Callback f)
{
    DD::Image::Enumeration_knob(f, &display3d_, DD::Image::display3d_names_source, "display", "display");
        DD::Image::Tooltip(f, "How to draw this Op in 3D preview (Viewer can override this setting.)");
    DD::Image::Bool_knob(f, &selectable_, "selectable", "selectable");
        DD::Image::Tooltip(f, "Turn off to prevent picking with the mouse in the viewer.");
    // Adds the 'editable' switch:
    Fsr::SceneLoader::addDisplayOptionsKnobs(f);

    DD::Image::Newline(f);
    DD::Image::Bool_knob(f, &k_gl_solid_frustum,  "gl_solid_frustum",  "display solid frustum");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
    DD::Image::Bool_knob(f, &k_gl_inside_frustum, "gl_inside_frustum", "frustum inside"       );
}


/*! Adds the front-panel transform knobs.

    This code is duplicated on the FuserAxisOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserCameraOp::addTransformKnobs(DD::Image::Knob_Callback f)
{
    Fsr::SceneXform::addParentingKnobs(f, true/*group_open*/);
    DD::Image::Newline(f);

    /* Allow protected CameraOp knobs to be set by SceneXform interface by passing
       their target vars in. This avoids SceneXform needing to be a subclass of
       CameraOp.

       CameraOp.h:
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
    //CameraOp::knobs(f);
    SceneXform::_addAxisOpTransformKnobs(f,
                                         &this->localtransform_,
                                         &this->axis_knob,
                                         &this->_worldMatrixProvider);
    //SceneXform::addLookatKnobs(f);
}


/*! Adds addl front-panel knobs.
    Called after addTransformKnobs() but before addProjectionTabKnobs().
    Base class does nothing.
*/
/*virtual*/
void
FuserCameraOp::addExtraFrontPanelKnobs(DD::Image::Knob_Callback f)
{
    //
}


/*! Create a 'Projection' node tab and add the knobs normally found there.
    Calls CameraOp::projection_knobs() and CameraOp::lens_knobs().
*/
/*virtual*/
void
FuserCameraOp::addProjectionTabKnobs(DD::Image::Knob_Callback f)
{
    projection_knobs(f);

    lens_knobs(f);

    DD::Image::Divider(f, "@b;Shutter");
    addShuttersKnobs(f);
}


/*! Adds the shutter controls.
    By default called from addProjectionTabKnobs().
*/
/*virtual*/
void
FuserCameraOp::addShuttersKnobs(DD::Image::Knob_Callback f)
{
    k_shutter.knobs(f, true/*earlyStore*/);
    DD::Image::Double_knob(f, &k_shutter_bias, "shutter_bias", "bias");
        DD::Image::SetFlags(f, DD::Image::Knob::EARLY_STORE);
        DD::Image::ClearFlags(f, DD::Image::Knob::SLIDER | DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Biases samples toward shutter close or shutter open for stylized "
                              "motion blur. 0+ range (0 is uniform blur.)");
}


/*! Adds projection knobs normally put on 'Projection' tab.
*/
/*virtual*/
void
FuserCameraOp::projection_knobs(DD::Image::Knob_Callback f)
{
    DD::Image::CameraOp::projection_knobs(f);
}


/*! Adds 'lens' knobs normally put on 'Projection' tab underneath projection knobs.
    By default we're adding the world_to_meters control.
*/
/*virtual*/
void
FuserCameraOp::lens_knobs(DD::Image::Knob_Callback f)
{
    DD::Image::CameraOp::lens_knobs(f);

    // TODO: change the knob name...?
    DD::Image::Double_knob(f, &k_world_to_meters, "world_scale", "world to meters");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_MULTIVIEW);
        DD::Image::ClearFlags(f, DD::Image::Knob::SLIDER);
        DD::Image::Tooltip(f, "Scale value to convert world-space units to <b>meters</b> for use in "
                              "lens calculations to map to world-space units such as DOF calculations.\n"
                              "\n"
                              "Lens parameters like focal-length and aperture width/height are "
                              "defined in millimeters so this scale value is further divided by "
                              "1000 to get the final world-scale to millimeters scalar.");
    //DD::Image::Obsolete_knob(f, "world_scale", "knob world_to_meters $value");
}


/*!
*/
/*virtual*/
int
FuserCameraOp::knob_changed(DD::Image::Knob* k)
{
    //std::cout << "FuserCameraOp::knob_changed(" << k->name() << ")" << std::endl;
    int call_again = 0;

    // Let interfaces handle their changes:
    call_again =  SceneXform::knobChanged(k, call_again);
    call_again = SceneLoader::knobChanged(k, call_again);

    // Let base class handle their changes:
    if (DD::Image::CameraOp::knob_changed(k))
        call_again = 1;

    return call_again;
}


/*! Add in knob values that affect the camera display state.
    Base class bakes down the lens window control values for OpenGL display.
*/
/*virtual*/
void
FuserCameraOp::append(DD::Image::Hash& hash)
{
    DD::Image::Knob* k;
    k = knob("win_translate");
    if (k) k->store(DD::Image::DoublePtr, &m_win_params.win_translate.x, hash, outputContext());
    k = knob("win_scale");
    if (k) k->store(DD::Image::DoublePtr, &m_win_params.win_scale.x,     hash, outputContext());
    k = knob("winroll");
    if (k) k->store(DD::Image::DoublePtr, &m_win_params.win_roll,        hash, outputContext());
}


/*!
*/
/*virtual*/
void
FuserCameraOp::_validate(bool for_real)
{
    //std::cout << "  FuserCameraOp::_validate(" << this->node_name() << ") for_real=" << for_real;
    //std::cout << " frame=" << outputContext().frame() << ", view=" << outputContext().view() << std::endl;

    // Check for any loader errors:
    SceneLoader::validateSceneLoader(for_real);

    // Builds the double-precision matrices replacing the stock single-precision ones,
    // then saves that result in the single-precision ones so that built in code still
    // works:
    //CameraOp::_validate(for_real);
    SceneXform::_validateAxisOpMatrices(for_real,
                                        &localtransform_,
                                        &local_,
                                        &matrix_,
                                        &inversion_updated);

    // Build projection matrix for correct mode:
    projection_ = this->projection(projection_mode_);

    // Precalc'd for OpenGL drawing convenience:
    m_mm_to_world = ((1.0 / 1000.0) / ::fabs(k_world_to_meters));

    //std::cout << "      localtransform_" << localtransform_ << std::endl;
    //std::cout << "               local_" << local_ << std::endl;
    //std::cout << "              matrix_" << matrix_ << std::endl;
    //std::cout << "   inversion_updated=" << inversion_updated << std::endl;
}


/*! Enable/disable any knobs that get updated by SceneLoader.
*/
/*virtual*/
void
FuserCameraOp::enableSceneLoaderExtraKnobs(bool read_enabled)
{
    // turn on local controls if not reading from file:
    const bool local_enabled = (!read_enabled);

    DD::Image::Knob* k;
    // Standard camera knobs:
    k = knob("projection_mode"); if (k) k->enable(local_enabled);
    k = knob("focal"          ); if (k) k->enable(local_enabled);
    k = knob("haperture"      ); if (k) k->enable(local_enabled);
    k = knob("vaperture"      ); if (k) k->enable(local_enabled);
    k = knob("near"           ); if (k) k->enable(local_enabled);
    k = knob("far"            ); if (k) k->enable(local_enabled);
    k = knob("focal_point"    ); if (k) k->enable(local_enabled);
    k = knob("fstop"          ); if (k) k->enable(local_enabled);
}


/*!
*/
/*virtual*/
void
FuserCameraOp::matrixAt(const DD::Image::OutputContext& context,
                        DD::Image::Matrix4&             matrix) const
{
    matrix = SceneXform::getWorldTransformAt(context).asDDImage();
}


/*! Return camera projection matrix for a particular projection mode.
    It needs to be overridden if subclasses implement a different logic to calculate
    the projection matrix.
*/
/*virtual*/
DD::Image::Matrix4
FuserCameraOp::projection(int mode) const
{
    DD::Image::Matrix4 out; out.makeIdentity();

    const bool is_perspective = (mode == LENS_PERSPECTIVE);

    // Filmback xform support is only for perspective projections, for now:
    if (is_perspective)
    {
        out.rotateZ((float)radians(win_roll_));
        out.scale(1.0f / win_scale_.x, 1.0f / win_scale_.y, 1.0f);

        float filmback_shift = 0.0f;
        out.translate(-win_translate_.x + filmback_shift, -win_translate_.y, 0.0f);
    }

    // TODO: need a Fuser replacement for this, either add to Mat4 or just break it out here
    // Build standard projection matrix and apply it:
    DD::Image::Matrix4 proj;
    proj.projection(float(focal_length_ / haperture_),
                    float(near_),
                    float(far_),
                    is_perspective);
    out *= proj;
    //std::cout << "FuserCameraOp::projection_matrix=" << out << std::endl;

    return out;
}


/*! Returns a transformation to an output image due to the camera lens.
    It needs to be overridden if subclasses implement a different logic to calculate
    the projection matrix, in this case we're applying a stereo filmback shift
    in addition to the win_translate offsets.
*/
/*virtual*/
DD::Image::Matrix4
FuserCameraOp::projectionAt(const DD::Image::OutputContext& context)
{
    // Get knob values at context:
    double cam_focal_length, cam_haperture/*, cam_vaperture*/;
    double cam_near, cam_far;
    double cam_win_translate[2], cam_win_scale[2], cam_win_roll;
    //double cam_focal_point, cam_fstop;

    DD::Image::Knob* k;
    DD::Image::Hash dummy_hash;
    k = knob("focal"        ); if (k) k->store(DD::Image::DoublePtr, &cam_focal_length, dummy_hash, context);
    k = knob("haperture"    ); if (k) k->store(DD::Image::DoublePtr, &cam_haperture,    dummy_hash, context);
    //k = knob("vaperture"    ); if (k) k->store(DD::Image::DoublePtr, &cam_vaperture,    dummy_hash, context);
    //
    k = knob("near"         ); if (k) k->store(DD::Image::DoublePtr, &cam_near,         dummy_hash, context);
    k = knob("far"          ); if (k) k->store(DD::Image::DoublePtr, &cam_far,          dummy_hash, context);
    //
    k = knob("win_translate"); if (k) k->store(DD::Image::DoublePtr, cam_win_translate, dummy_hash, context);
    k = knob("win_scale"    ); if (k) k->store(DD::Image::DoublePtr, cam_win_scale,     dummy_hash, context);
    k = knob("winroll"      ); if (k) k->store(DD::Image::DoublePtr, &cam_win_roll,     dummy_hash, context);
    //
    //k = knob("focal_point"  ); if (k) k->store(DD::Image::DoublePtr, &cam_focal_point,  dummy_hash, context);
    //k = knob("fstop"        ); if (k) k->store(DD::Image::DoublePtr, &cam_fstop,        dummy_hash, context);

    DD::Image::Matrix4 out; out.makeIdentity();

    // Projection_mode can't animate(change per context) so we don't need to store it:
    const bool is_perspective = (projection_mode_ == LENS_PERSPECTIVE);

    // Filmback xform support is only for perspective projections, for now:
    if (is_perspective)
    {
        out.rotateZ(float(radians(cam_win_roll)));
        out.scale(float(1.0 / cam_win_scale[0]), float(1.0 / cam_win_scale[1]), 1.0f);

        double cam_filmback_shift = 0.0;
        out.translate(float(-cam_win_translate[0] + cam_filmback_shift), float(-cam_win_translate[1]), 0.0f);
    }

    // TODO: need a Fuser replacement for this, either add to Mat4 or just break it out here
    // Build standard projection matrix and apply it:
    DD::Image::Matrix4 proj;
    proj.projection(float(cam_focal_length / cam_haperture),
                    float(cam_near),
                    float(cam_far),
                    is_perspective);
    out *= proj;
    //std::cout << "FuserCameraOp::projectionAt=" << out << std::endl;

    return out;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Much of this code is duplicated on the FuserAxisOp and FuserLightOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserCameraOp::build_handles(DD::Image::ViewerContext* vtx)
{
    DD::Image::Matrix4 saved_matrix = vtx->modelmatrix;

    // Go up the inputs asking them to build their handles.
    // Do this first so that other ops always have a chance to draw!
    DD::Image::Op::build_input_handles(vtx);  // inputs are drawn in current world space

    if (node_disabled())
        return;

    // Only draw the camera's icon in 3D view:
    // TODO: what about stereo window controls in 2D?
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
        DD::Image::CameraOp::display3d_)
    {
        DD::Image::Op::add_draw_handle(vtx);

        // Expand the Viewer selection bbox to include the location of the Xform.
        // If is_selected is true then the Viewer's *active* bbox is also expanded
        // so user-focus will include this location.
        const bool is_selected = node_selected();
        const Fsr::Vec3d location = m_local_matrix.getTranslation();
        vtx->expand_bbox(is_selected, float(location.x), float(location.y), float(location.z));
    }

    vtx->addCamera(this);

    vtx->modelmatrix = saved_matrix; // don't leave matrix messed up
}


/*! Much of this code is duplicated on the FuserAxisOp and FuserLightOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserCameraOp::draw_handle(DD::Image::ViewerContext* vtx)
{
    //std::cout << "FuserCameraOp::draw_handle()" << std::endl;
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

    double cam_scale = (1.0f / ::fabs(k_world_to_meters));

    // The icon scaling factor is derived from how far the Viewer camera is
    // to the point passed to icon_size(). This is clamped to have the icon
    // appear a 'real-life' size when close up but not disappear when far away.
    const Fsr::Vec3d location = m_local_matrix.getTranslation();
    const double icon_scale = vtx->icon_size(float(location.x), float(location.y), float(location.z));
    if (icon_scale > cam_scale)
        cam_scale = icon_scale;

    const float aspect = float(vaperture_ / haperture_);
    const float lens   = float(haperture_ / focal_length_);

    const float zn = float(near_);
    const float zf = float(far_);
    const float xn = float(zn * lens * 0.5f);
    const float xf = float(zf * lens * 0.5f);
    const float yn = float(xn * aspect);
    const float yf = float(xf * aspect);

    // Draw the viewing frustrum during line pass:
    if (vtx->event() >= DD::Image::DRAW_STIPPLED)
    {
        glLoadMatrixd(gl_modelmatrix.array());

        // Draw the camera name:
        DD::Image::glColor(vtx->fg_color());
        drawNodeName(Fsr::Vec3d(xn, -yn, -zn));

        DD::Image::glColor((is_selected)?vtx->selected_color():vtx->node_color());

        // Draw frustum & aperture:
        {
            // Frustum and crosshairs are in world coords:
            drawWireFrustum(DD::Image::Vector3(xn, yn, zn),
                            DD::Image::Vector3(xf, yf, zf));

            // Draw focus center as a crosshair:
            {
                const float fp = float(focal_point_);
                const float v = 0.02f * fp;
                glBegin(GL_LINES);
                   glVertex3f(  -v, 0.0f, -fp);
                   glVertex3f(   v, 0.0f, -fp);
                   glVertex3f(0.0f,   -v, -fp);
                   glVertex3f(0.0f,    v, -fp);
                glEnd();
            }

            // Aperture is in mm:
            glScaled(m_mm_to_world, m_mm_to_world, m_mm_to_world);
            //glMultMatrixd(cam0.aperture_xform.array());
            drawAperture(DD::Image::Vector3(0.0f, 0.0f, 0.0f)); // 
        }

        // Draw the camera body:
        if (DD::Image::style_needs_wireframe(display3d) || is_selected || select_body_solid)
        {
            glLoadMatrixd(gl_modelmatrix.array());
            glScaled(cam_scale, cam_scale, cam_scale);
            drawCameraIcon(select_body_solid/*solid*/, false/*dashed_lines*/);
        }

    }
    else if (vtx->draw_solid())
    {
        glLoadMatrixd(gl_modelmatrix.array());
        DD::Image::glColor(vtx->node_color());

        if (k_gl_solid_frustum)
        {
            // Draw the camera frustum:
            glTranslated((m_win_params.win_translate.x*2.0)*haperture_*m_mm_to_world,
                         (m_win_params.win_translate.y*2.0)*vaperture_*m_mm_to_world,
                         0.0);
            glRotated(-m_win_params.win_roll, 0.0, 0.0, 1.0);
            glScaled(m_win_params.win_scale.x,
                     m_win_params.win_scale.y,
                     1.0);

            //glPushAttrib(GL_LIGHTING_BIT);
            //glDisable(GL_LIGHTING);
            drawSolidFrustum(DD::Image::Vector3(xn, yn, zn),
                             DD::Image::Vector3(xf, yf, zf));
            //glPopAttrib(); //GL_LIGHTING_BIT
        }

        // Draw the camera body:
        if (display3d > DD::Image::DISPLAY_WIREFRAME)
        {
            glLoadMatrixd(gl_modelmatrix.array());
            glScaled(cam_scale, cam_scale, cam_scale);
            drawCameraIcon(true/*solid*/);
        }

    }

}


//-------------------------------------------------------------------------


/*!
*/
void
FuserCameraOp::drawNodeName(const char* subcam_name)
{
    drawNodeName(Fsr::Vec3d(0.0, 0.0, 0.0), subcam_name);
}

/*!
*/
void
FuserCameraOp::drawNodeName(const Fsr::Vec3d& xyz,
                            const char*       subcam_name)
{
    std::stringstream ss;
    DD::Image::Op::print_name(ss);

    if (subcam_name && subcam_name[0])
        ss << "(" << subcam_name << ")";

    glRasterPos3d(xyz.x, xyz.y, xyz.z);
    DD::Image::gl_text(ss.str().c_str());
}


// Also in DD::Image::gl.h
enum { XY, XZ, YZ, YX, ZX, ZY };

// Camera body:  6" wide, 10" tall, 17" long:
//                              x          y     z       r       t        f
const float cam_body[] = { -0.0764f, -0.1270f, 0.0f, 0.0764f, 0.1270f, 0.4318f };
//                             x      y         z      W       d1      d2
const float cam_fmag[]  = { 0.0f, 0.252f,  0.100f, 0.0764f, 0.250f, 0.250f };
const float cam_rmag[]  = { 0.0f, 0.252f,  0.350f, 0.0764f, 0.250f, 0.250f };
const float cam_shade[] = { 0.0f,   0.0f, -0.075f, 0.1500f, 0.300f, 0.130f };

/*! Draw a 35mm Mitchell-style camera icon in the correct size assuming
    world scale is meters.
*/
inline void drawMitchellCamera(bool solid)
{
    // In solid mode Set up polygon sides rendering such that when we're inside
    // the camera it draws wireframe and if we're outside it draws solid:
    if (solid)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        DD::Image::gl_boxf(cam_body[0], cam_body[1], cam_body[2],
                           cam_body[3], cam_body[4], cam_body[5], true/*filled*/); // Camera body
        glDisable(GL_CULL_FACE);
    }
    else
    {
        DD::Image::gl_boxf(cam_body[0], cam_body[1], cam_body[2],
                           cam_body[3], cam_body[4], cam_body[5], false/*filled*/); // Camera body
    }
    DD::Image::gl_cylinderf(cam_shade[0], cam_shade[1], cam_shade[2],
                            cam_shade[3], cam_shade[4], cam_shade[5],
                            XY, false/*capped*/, solid/*filled*/); // Lens shade
    DD::Image::gl_cylinderf(cam_fmag[0], cam_fmag[1], cam_fmag[2],
                            cam_fmag[3], cam_fmag[4], cam_fmag[5],
                            YZ, true/*capped*/, solid/*filled*/); // Front film mag
    DD::Image::gl_cylinderf(cam_rmag[0], cam_rmag[1], cam_rmag[2],
                            cam_rmag[3], cam_rmag[4], cam_rmag[5],
                            YZ, true/*capped*/, solid/*filled*/); // Rear film mag
}


/*!
*/
/*virtual*/ void
FuserCameraOp::drawCameraIcon(bool solid,
                              bool dashed_lines)
{
    if (solid)
    {
        drawMitchellCamera(true/*solid*/);
    }
    else
    {
        if (dashed_lines)
        {
            glPushAttrib(GL_LINE_BIT);
            glEnable(GL_LINE_STIPPLE);
            //glLineStipple(1, 0xff00); // big-dashed
            glLineStipple(1, 0xeee0); // dashed
            drawMitchellCamera(false/*solid*/);

            glColor3f(0.0f, 0.0f, 0.0f);
            //glLineStipple(1, 0x00ff); // big-dashed inverted
            glLineStipple(1, 0x111f); // dashed inverted
            drawMitchellCamera(false/*solid*/);

            glPopAttrib(); // GL_LINE_BIT
        }
        else
        {
            drawMitchellCamera(false/*solid*/);
        }
#if 0
        // draw wireframe of inside of camera
        DD::Image::glColor(vtx->node_color());
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glPushAttrib(GL_POLYGON_BIT);
        glPolygonMode(GL_BACK, GL_LINE); // Backfaces are wireframe
        DD::Image::gl_boxf(cam_body[0], cam_body[1], cam_body[2],
                           cam_body[3], cam_body[4], cam_body[5], true/*filled*/); // Camera body
        glPopAttrib(); // GL_POLYGON_BIT
        glDisable(GL_CULL_FACE);
#endif
    }
}


/*! Draw the camera's frustum.
*/
void
FuserCameraOp::drawSolidFrustum(const DD::Image::Vector3& vn,
                                const DD::Image::Vector3& vf)
{
    DD::Image::Vector3 vn0, vn1, vn2, vn3, n;
    vn0.set( -vn.x, -vn.y, -vn.z);
    vn1.set(  vn.x, -vn.y, -vn.z);
    vn2.set(  vn.x,  vn.y, -vn.z);
    vn3.set( -vn.x,  vn.y, -vn.z);
    DD::Image::Vector3 vf0, vf1, vf2, vf3;
    vf0.set( -vf.x, -vf.y, -vf.z);
    vf1.set(  vf.x, -vf.y, -vf.z);
    vf2.set(  vf.x,  vf.y, -vf.z);
    vf3.set( -vf.x,  vf.y, -vf.z);

    glPushAttrib(GL_POLYGON_BIT | GL_LIGHTING_BIT);
    if (k_gl_inside_frustum)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
    // So that both sides of frustum are shaded:
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    // Draw frustum faces:
    glBegin(GL_POLYGON); // Left
     n = (vn3 - vn0).cross(vf0 - vn0);
     glNormal3fv(n.array());
     glVertex3fv(vf0.array());
     glVertex3fv(vn0.array());
     glVertex3fv(vn3.array());
     glVertex3fv(vf3.array());
    glEnd();
     //
    glBegin(GL_POLYGON); // Right
     n.x = -n.x; n.y = -n.y;
     glNormal3fv(n.array());
     glVertex3fv(vn1.array());
     glVertex3fv(vf1.array());
     glVertex3fv(vf2.array());
     glVertex3fv(vn2.array());
    glEnd();
     //
    glBegin(GL_POLYGON); // Top
     n = (vn2 - vn3).cross(vf3 - vn3);
     glNormal3fv(n.array());
     glVertex3fv(vn3.array());
     glVertex3fv(vn2.array());
     glVertex3fv(vf2.array());
     glVertex3fv(vf3.array());
    glEnd();
     //
    glBegin(GL_POLYGON); // Bottom
     n.x = -n.x; n.y = -n.y;
     glNormal3fv(n.array());
     glVertex3fv(vf0.array());
     glVertex3fv(vf1.array());
     glVertex3fv(vn1.array());
     glVertex3fv(vn0.array());
    glEnd();
    //
    glPopAttrib(); // GL_POLYGON_BIT | GL_LIGHTING_BIT
}


inline void drawFrustumOutline(const DD::Image::Vector3& vn,
                               const DD::Image::Vector3& vf)
{
    glBegin(GL_LINE_STRIP);
     glVertex3f( -vn.x, -vn.y, -vn.z);
     glVertex3f( -vn.x,  vn.y, -vn.z);
     glVertex3f(  vn.x,  vn.y, -vn.z);
     glVertex3f(  vn.x, -vn.y, -vn.z);
     glVertex3f( -vn.x, -vn.y, -vn.z);
     glVertex3f( -vf.x, -vf.y, -vf.z);
     glVertex3f( -vf.x,  vf.y, -vf.z);
     glVertex3f(  vf.x,  vf.y, -vf.z);
     glVertex3f(  vf.x, -vf.y, -vf.z);
     glVertex3f( -vf.x, -vf.y, -vf.z);
    glEnd();
    glBegin(GL_LINES);
     glVertex3f( -vn.x,  vn.y, -vn.z);
     glVertex3f( -vf.x,  vf.y, -vf.z);
     glVertex3f(  vn.x,  vn.y, -vn.z);
     glVertex3f(  vf.x,  vf.y, -vf.z);
     glVertex3f(  vn.x, -vn.y, -vn.z);
     glVertex3f(  vf.x, -vf.y, -vf.z);
    glEnd();
}


/*! Draw the camera's frustum.
*/
void
FuserCameraOp::drawWireFrustum(const DD::Image::Vector3& vn,
                               const DD::Image::Vector3& vf,
                               bool                      dashed_lines)
{
    if (dashed_lines)
    {
        glPushAttrib(GL_LINE_BIT);
        glEnable(GL_LINE_STIPPLE);
        {
            glLineStipple(1, 0xeee0); // dashed
            drawFrustumOutline(vn, vf);
            //
            glLineStipple(1, 0x111f); // dashed-inverted
            glColor3f(0.0f, 0.0f, 0.0f);
            drawFrustumOutline(vn, vf);
        }
        glPopAttrib(); // GL_LINE_BIT
    }
    else
        drawFrustumOutline(vn, vf);
}


inline void drawRectangleOutline(const DD::Image::Vector3& P,
                                 float w2,
                                 float h2)
{
    glBegin(GL_LINE_LOOP);
     glVertex3f(P.x-w2, P.y-h2, P.z);
     glVertex3f(P.x+w2, P.y-h2, P.z);
     glVertex3f(P.x+w2, P.y+h2, P.z);
     glVertex3f(P.x-w2, P.y+h2, P.z);
    glEnd();
}


/*! Draw the aperture rectangle in millimeters, centered on 'P'.
*/
void
FuserCameraOp::drawAperture(const DD::Image::Vector3& P,
                            bool                      dashed_lines)
{
    const float w2 = float(haperture_ / 2.0);
    const float h2 = float(vaperture_ / 2.0);
    if (dashed_lines)
    {
        glPushAttrib(GL_LINE_BIT);
        glEnable(GL_LINE_STIPPLE);
        {
            glLineStipple(1, 0xff00); // big-dashed
            //glLineStipple(1, 0xeee0); // dashed
            drawRectangleOutline(P, w2, h2);
            //
            glColor3f(0.0f, 0.0f, 0.0f);
            glLineStipple(1, 0x00ff); // big-dashed inverted
            //glLineStipple(1, 0x111f); // dashed inverted
            drawRectangleOutline(P, w2, h2);
        }
        glPopAttrib(); // GL_LINE_BIT
    }
    else
        drawRectangleOutline(P, w2, h2);
}


} // namespace Fsr


// end of Fuser/CameraOp.cpp

//
// Copyright 2019 DreamWorks Animation
//
