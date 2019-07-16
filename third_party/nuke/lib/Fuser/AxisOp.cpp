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

/// @file Fuser/AxisOp.cpp
///
/// @author Jonathan Egstad

#include "AxisOp.h"

#include <DDImage/gl.h>

#include <sstream> // for print_name

namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#ifdef FUSER_USE_KNOB_RTTI
const char* FuserAxisOpRTTIKnob = "FsrAxisOp";
#endif


/*!
*/
FuserAxisOp::FuserAxisOp(::Node* node) :
    DD::Image::AxisOp(node),
    Fsr::SceneXform(),
    Fsr::SceneLoader()
{
    //
}


/*! Returns op cast to Fuser types if possible, otherwise NULL.

    For a statically-linked Fuser lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/ FuserAxisOp*
FuserAxisOp::asFuserAxisOp(DD::Image::Op* op)
{
#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Test for dummy knob so we can test for class without using RTTI...:
    if (op && op->knob(FuserAxisOpRTTIKnob) != NULL)
        return reinterpret_cast<FuserAxisOp*>(op);
    return NULL;
#else
    return dynamic_cast<FuserAxisOp*>(op);
#endif
}


/*!
*/
/*virtual*/
const char*
FuserAxisOp::node_help() const
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
*/
/*virtual*/
void
FuserAxisOp::knobs(DD::Image::Knob_Callback f)
{
    //std::cout << "FuserAxisOp::knobs(" << this->node_name() << ") makeKnobs=" << f.makeKnobs() << std::endl;

#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Dummy knob so we can test for class without using RTTI...:
    int dflt=0; DD::Image::Int_knob(f, &dflt, FuserAxisOpRTTIKnob, DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::NO_ANIMATION | DD::Image::Knob::NO_RERENDER);
#endif

    Fsr::SceneLoader::addSceneLoaderKnobs(f);

    DD::Image::Divider(f);
    addDisplayOptionsKnobs(f);

    DD::Image::Divider(f);
    addTransformKnobs(f);
    addExtraFrontPanelKnobs(f);
}


/*! Adds the OpenGL display option controls.

    This code is duplicated on the FuserCameraOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserAxisOp::addDisplayOptionsKnobs(DD::Image::Knob_Callback f)
{
    DD::Image::Enumeration_knob(f, &display3d_, DD::Image::display3d_names_source, "display", "display");
        DD::Image::Tooltip(f, "How to draw this Op in 3D preview (Viewer can override this setting.)");
    DD::Image::Bool_knob(f, &selectable_, "selectable", "selectable");
        DD::Image::Tooltip(f, "Turn off to prevent picking with the mouse in the viewer.");
    // Adds the 'editable' switch:
    Fsr::SceneLoader::addDisplayOptionsKnobs(f);
}


/*! Adds the front-panel transform knobs.

    This code is duplicated on the FuserCameraOp and FuserLightOp classes as it
    can't live on the SceneXform interface class. Keep them in sync!
*/
/*virtual*/
void
FuserAxisOp::addTransformKnobs(DD::Image::Knob_Callback f)
{
    Fsr::SceneXform::addParentingKnobs(f, true/*group_open*/);
    DD::Image::Newline(f);

    /* Allow protected AxisOp knobs to be set by SceneXform interface by passing
       their target vars in. This avoids SceneXform needing to be a subclass of
       AxisOp.

       AxisOp.h:
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
    //AxisOp::knobs(f);
    SceneXform::_addAxisOpTransformKnobs(f,
                                         &this->localtransform_,
                                         &this->axis_knob,
                                         &this->_worldMatrixProvider);
    //SceneXform::addLookatKnobs(f);
}


/*! Adds addl front-panel knobs.
    Called after addTransformKnobs().
    Base class adds nothing.
*/
/*virtual*/
void
FuserAxisOp::addExtraFrontPanelKnobs(DD::Image::Knob_Callback f)
{
    //
}


/*!
*/
/*virtual*/
int
FuserAxisOp::knob_changed(DD::Image::Knob* k)
{
    //std::cout << "FuserAxisOp::knob_changed(" << k->name() << ")" << std::endl;
    int call_again = 0;

    // Let interfaces handle their changes:
    call_again =  SceneXform::knobChanged(k, call_again);
    call_again = SceneLoader::knobChanged(k, call_again);
    if (call_again)
        return call_again;

    return DD::Image::AxisOp::knob_changed(k);
}


/*!
*/
/*virtual*/
void
FuserAxisOp::_validate(bool for_real)
{
    //std::cout << "  FuserAxisOp::_validate(" << this->node_name() << ") for_real=" << for_real << std::endl;

    // Check for any loader errors:
    SceneLoader::validateSceneLoader(for_real);

    // Builds the double-precision matrices replacing the stock single-precision ones,
    // then saves that result in the single-precision ones so that built in code still
    // works:
    //AxisOp::_validate(for_real);
    SceneXform::_validateAxisOpMatrices(for_real,
                                        &localtransform_,
                                        &local_,
                                        &matrix_,
                                        &inversion_updated);

    //std::cout << "      localtransform_" << localtransform_ << std::endl;
    //std::cout << "               local_" << local_ << std::endl;
    //std::cout << "              matrix_" << matrix_ << std::endl;
    //std::cout << "   inversion_updated=" << inversion_updated << std::endl;
}


/*!
*/
/*virtual*/
void
FuserAxisOp::matrixAt(const DD::Image::OutputContext& context,
                      DD::Image::Matrix4&             matrix) const
{
    matrix = SceneXform::getWorldTransformAt(context).asDDImage();
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Much of this code is duplicated on the FuserCameraOp and FuserLightOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserAxisOp::build_handles(DD::Image::ViewerContext* vtx)
{
    DD::Image::Matrix4 saved_matrix = vtx->modelmatrix;

    // Go up the inputs asking them to build their handles.
    // Do this first so that other ops always have a chance to draw!
    DD::Image::Op::build_input_handles(vtx);  // inputs are drawn in current world space

    if (node_disabled())
        return;

    // Only draw the Axis icon in 3D view:
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
        DD::Image::AxisOp::display3d_)
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


/*! Much of this code is duplicated on the FuserCameraOp and FuserLightOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
FuserAxisOp::draw_handle(DD::Image::ViewerContext* vtx)
{
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
    //const bool select_body_solid = (display3d > DD::Image::DISPLAY_WIREFRAME &&
    //                                (vtx->event() == DD::Image::DRAW_OPAQUE ||
    //                                    vtx->event() == DD::Image::PUSH));

    // Model matrix will include the parent transform assigned in build_handles(),
    // so mult in only the local:
    Fsr::Mat4d gl_modelmatrix(vtx->modelmatrix);
    gl_modelmatrix *= m_local_matrix;

    glLoadMatrixd(gl_modelmatrix.array());

    // Draw the name:
    DD::Image::glColor(vtx->fg_color());
    drawNodeName();

    // The icon scaling factor is derived from how far the Viewer camera is
    // to the point passed to icon_size(). This is clamped to have the icon
    // appear a 'real-life' size when close up but not disappear when far away.
    const Fsr::Vec3d location = m_local_matrix.getTranslation();
    double icon_scale = vtx->icon_size(float(location.x), float(location.y), float(location.z));
    if (icon_scale < 1.0)
        icon_scale = 0.25;
    else
        icon_scale *= 0.25;

    // Draw the axis OpenGL icon, a simple xyx cross.
    if (vtx->event() >= DD::Image::DRAW_STIPPLED)
    {
        // TODO: draw something more fancy? Perhaps indicate the positive ends of the axis?
        DD::Image::glColor((is_selected)?vtx->selected_color():vtx->node_color());
        glBegin(GL_LINES);
        {
            // X:
            glVertex3d(-icon_scale,        0.0,        0.0); glVertex3d( icon_scale,        0.0,        0.0);
            // Y:
            glVertex3d(        0.0,-icon_scale,        0.0); glVertex3d(        0.0, icon_scale,        0.0);
            // Z:
            glVertex3d(        0.0,        0.0,-icon_scale); glVertex3d(        0.0,        0.0, icon_scale);
        }
        glEnd();
    }
}


/*!
*/
void
FuserAxisOp::drawNodeName()
{
    drawNodeName(Fsr::Vec3d(0,0,0));
}

/*!
*/
void
FuserAxisOp::drawNodeName(const Fsr::Vec3d& xyz)
{
    std::stringstream ss;
    DD::Image::Op::print_name(ss);

    glRasterPos3d(xyz.x, xyz.y, xyz.z);
    DD::Image::gl_text(ss.str().c_str());
}


} // namespace Fsr


// end of Fuser/AxisOp.cpp

//
// Copyright 2019 DreamWorks Animation
//
