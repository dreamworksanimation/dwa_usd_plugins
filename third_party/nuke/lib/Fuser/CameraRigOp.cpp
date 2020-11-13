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

/// @file Fuser/CameraRigOp.cpp
///
/// @author Jonathan Egstad

#include "CameraRigOp.h"

#include <DDImage/gl.h>

namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
CameraRigOp::CameraRigOp(::Node* node) :
    FuserCameraOp(node)
{
    k_gl_show_all_rig_cameras = true;
}


/*! Adds the OpenGL display option controls.
    Adds stereo display options.
*/
/*virtual*/
void
CameraRigOp::addDisplayOptionsKnobs(DD::Image::Knob_Callback f)
{
    FuserCameraOp::addDisplayOptionsKnobs(f);

    Newline(f);
    Bool_knob(f, &k_gl_show_all_rig_cameras, "gl_show_all_rig_cameras", "show all rig cameras");
}


/*! Adds addl front-panel knobs.
    Calls addRigKnobs() with the rigName().
*/
/*virtual*/
void
CameraRigOp::addExtraFrontPanelKnobs(DD::Image::Knob_Callback f)
{
    char rig_label[256];
    snprintf(rig_label, 256, "@b;%s", rigName());
    addRigKnobs(f, rig_label);
}


/*! Add in knob values that affect the rig. Some rigs require values from
    multiple views and should evaluate those knobs at additional views,
    incorporating them into the main hash.

    Base class implementation calls appendRigKnobs(), and if hash value
    is different than the store m_rig_hash then m_cam_params is cleared
    so that it can be rebuilt in _validate().
*/
/*virtual*/
void
CameraRigOp::append(DD::Image::Hash& hash)
{
    DD::Image::Hash rig_hash;
    _appendRigValuesAt(outputContext(), rig_hash);

    // If hash has changed force _rebuildCamParamsAt() to be run
    // in _validate() by clearing m_cam_params:
    if (rig_hash != m_rig_hash)
    {
        m_rig_hash = rig_hash;
        m_cam_params.clear();
    }
    hash.append(rig_hash);
}


/*! Append controls that affect the rig cameras to a hash at the current OutputContext.
*/
void
CameraRigOp::appendRigValues(DD::Image::Hash& hash)
{
    hash.append(k_world_to_meters);
    hash.append(k_gl_solid_frustum);
    hash.append(k_gl_inside_frustum);
    //
    hash.append(k_gl_show_all_rig_cameras);

    _appendRigValuesAt(outputContext(), hash);
}


/*! Update the CamParams list if it's empty by called rebuildCamParamsAt()
    on the subclass.
*/
/*virtual*/
void
CameraRigOp::_validate(bool for_real)
{
    //std::cout << "  Fsr::CameraRigOp::_validate(" << this->node_name() << ") for_real=" << for_real << std::endl;

    FuserCameraOp::_validate(for_real);

    if (m_cam_params.empty())
        _rebuildCamParamsAt(outputContext());

    assert(m_cam_params.size() >= 1);
}


/*! Rebuild the CamParams list at the current OutputContext.
*/
void
CameraRigOp::rebuildCamParams()
{
    m_cam_params.clear();
    _rebuildCamParamsAt(outputContext());
    assert(m_cam_params.size() >= 1);
}


/*! Much of this code is duplicated on the FuserAxisOp, FuserCameraOp and FuserLightOp classes.

    Since there's no common base class (Fsr::SceneXform won't work) we end up
    having to duplicate a lot of this...  :(
*/
/*virtual*/
void
CameraRigOp::draw_handle(DD::Image::ViewerContext* ctx)
{
    //std::cout << "CameraRigOp::draw_handle(): event=" << ctx->event() << std::endl;
    if (m_cam_params.size() == 0)
        return; // not initialized yet, don't crash

    // TODO: change this logic if we want to view the stereo controls while locked!
    // If the user has locked to this camera, don't bother
    // drawing the icon in the 3D view since we won't see it.
    // In the 2D view we want to see the frame lines:
    if (ctx->locked() && ctx->viewer_mode() <= DD::Image::VIEWER_PERSP && ctx->camera() == this)
        return;

    // Disallow selection if the selectable button is disabled:
    // TODO: support a 'modifiable' mode vs. 'selectable'?
    if (!selectable_ && ctx->hit_detect())
        return;

    // In 3D this method is usually called with event sequence DRAW_OPAQUE, DRAW_STIPPLED, DRAW_LINES

    const bool is_selected = node_selected();
    const int  display3d = ctx->display3d((DD::Image::Display3DMode)this->display3d_);
    if (display3d == DD::Image::VIEWER_2D && !is_selected)
        return;

    // If we're selecting in the viewer and the icon wants to be drawn solid
    // then make sure that happens even in pick mode:
    const bool select_body_solid = (display3d > DD::Image::DISPLAY_WIREFRAME &&
                                    (ctx->event() == DD::Image::DRAW_OPAQUE ||
                                        ctx->event() == DD::Image::PUSH));

    // Model matrix will include the parent transform assigned in build_handles(),
    // so mult in only the local:
    Fsr::Mat4d gl_modelmatrix(ctx->modelmatrix);
    gl_modelmatrix *= m_local_matrix;

    double cam_scale = (1.0f / ::fabs(k_world_to_meters));

    // The icon scaling factor is derived from how far the Viewer camera is
    // to the point passed to icon_size(). This is clamped to have the icon
    // appear a 'real-life' size when close up but not disappear when far away.
    const Fsr::Vec3d location = m_local_matrix.getTranslation();
    const double icon_scale = ctx->icon_size(float(location.x), float(location.y), float(location.z));
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

    assert(m_cam_params.size() >= 1); // shouldn't happen...
    const CamParams& cam0 = m_cam_params[0];

    // Draw lines and pick modes:
    if (ctx->event() >= DD::Image::DRAW_STIPPLED)
    {
        // Draw the camera name(s):
        if (m_cam_params.size() > 1 && k_gl_show_all_rig_cameras)
        {
            for (size_t i=0; i < m_cam_params.size(); ++i)
            {
                const CamParams& cam = m_cam_params[i];
                glLoadMatrixd(gl_modelmatrix.array());
                glMultMatrixd(cam.local_xform.array());
                DD::Image::glColor((is_selected) ? ctx->selected_color() : cam.gl_color);
                drawNodeName(Fsr::Vec3d(xn, -yn, -zn), cam.name);
            }
            glLoadMatrixd(gl_modelmatrix.array());
        }
        else
        {
            // Only one camera:
            glLoadMatrixd(gl_modelmatrix.array());
            glMultMatrixd(cam0.local_xform.array());
            DD::Image::glColor((is_selected) ? ctx->selected_color() : cam0.gl_color);
            drawNodeName(Fsr::Vec3d(xn, -yn, -zn));
        }

        // Draw cam0 frustum & aperture:
        {
            DD::Image::glColor((is_selected) ? ctx->selected_color() : cam0.gl_color);

            glLoadMatrixd(gl_modelmatrix.array());
            glMultMatrixd(cam0.local_xform.array());

            // Frustum and crosshairs are in world coords:
            drawWireFrustum(DD::Image::Vector3(xn, yn, zn),
                            DD::Image::Vector3(xf, yf, zf));
            {
                // Draw focus center as a crosshair:
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
            glMultMatrixd(cam0.aperture_xform.array());
            drawAperture(DD::Image::Vector3(0.0f, 0.0f, 0.0f)); // 
        }

        // Show rig camera apertures and frustums:
        if (m_cam_params.size() > 1 && k_gl_show_all_rig_cameras)
        {
            glPushAttrib(GL_LINE_BIT);
            {
                for (size_t i=1; i < m_cam_params.size(); ++i)
                {
                    const CamParams& cam = m_cam_params[i];
                    glLoadMatrixd(gl_modelmatrix.array());
                    glMultMatrixd(cam.local_xform.array());

                    DD::Image::glColor((is_selected) ? ctx->selected_color() : cam.gl_color);

                    // Frustum is in world coords:
                    // TODO: add a control to show other rig camera frustums?
                    //drawWireFrustum(DD::Image::Vector3(xn, yn, zn),
                    //                DD::Image::Vector3(xf, yf, zf), true/*dashed_lines*/);

                    // Aperture is in mm:
                    glScaled(m_mm_to_world, m_mm_to_world, m_mm_to_world);
                    glMultMatrixd(cam.aperture_xform.array());
                    drawAperture(DD::Image::Vector3(0.0f, 0.0f, 0.0f), true/*dashed_lines*/); // 

                }
            }
            glPopAttrib(); // GL_LINE_BIT
        }

        // Draw the camera body(s):
        if (DD::Image::style_needs_wireframe(display3d) || is_selected || select_body_solid)
        {
            glLoadMatrixd(gl_modelmatrix.array());
            glMultMatrixd(cam0.local_xform.array());
            glScaled(cam_scale, cam_scale, cam_scale);

            DD::Image::glColor((is_selected) ? ctx->selected_color() : cam0.gl_color);
            drawCameraIcon(select_body_solid/*solid*/, false/*dashed_lines*/);

            // Show the other rig cameras wireframe:
            // TODO: add a control for this behavior?
            if (m_cam_params.size() > 1 && k_gl_show_all_rig_cameras)
            {
                for (size_t i=1; i < m_cam_params.size(); ++i)
                {
                    const CamParams& cam = m_cam_params[i];
                    glLoadMatrixd(gl_modelmatrix.array());
                    glMultMatrixd(cam.local_xform.array());
                    glScaled(cam_scale, cam_scale, cam_scale);
                    DD::Image::glColor((is_selected) ? ctx->selected_color() : cam.gl_color);
                    drawCameraIcon(select_body_solid/*solid*/, true/*dashed_lines*/);
                }
            }
        }

    }
    else if (ctx->draw_solid())
    {
        if (k_gl_solid_frustum)
        {
            DD::Image::glColor(cam0.gl_color);

            glLoadMatrixd(gl_modelmatrix.array());
            glMultMatrixd(cam0.local_xform.array());

            //glPushAttrib(GL_LIGHTING_BIT);
            //glDisable(GL_LIGHTING);
            drawSolidFrustum(DD::Image::Vector3(xn, yn, zn),
                             DD::Image::Vector3(xf, yf, zf));
            //glPopAttrib(); //GL_LIGHTING_BIT

#if 0
            // Draw the camera frustums as solid:
            // TODO: add a control to show other rig camera frustums?
            if (m_cam_params.size() > 1 && k_gl_show_all_rig_cameras)
            {
                for (size_t i=0; i < m_cam_params.size(); ++i)
                {
                    const CamParams& cam = m_cam_params[i];
                    glLoadMatrixd(gl_modelmatrix.array());
                    glMultMatrixd(cam.local_xform.array());

                    DD::Image::glColor(cam.gl_color);

                    //glPushAttrib(GL_LIGHTING_BIT);
                    //glDisable(GL_LIGHTING);
                    DD::Image::glColor(cam.gl_color);
                    drawSolidFrustum(DD::Image::Vector3(xn, yn, zn),
                                     DD::Image::Vector3(xf, yf, zf));
                    //glPopAttrib(); //GL_LIGHTING_BIT
                }
            }
#endif
        }

        // Draw the camera body(s):
        if (display3d > DD::Image::DISPLAY_WIREFRAME)
        {
            glLoadMatrixd(gl_modelmatrix.array());
            glMultMatrixd(cam0.local_xform.array());
            glScaled(cam_scale, cam_scale, cam_scale);

            DD::Image::glColor(cam0.gl_color);
            drawCameraIcon(true/*solid*/);

#if 0
            // Show the other rig cameras as solid:
            // TODO: add a control for this behavior?
            if (m_cam_params.size() > 1 && k_gl_show_all_rig_cameras)
            {
                for (size_t i=1; i < m_cam_params.size(); ++i)
                {
                    const CamParams& cam = m_cam_params[i];
                    glLoadMatrixd(gl_modelmatrix.array());
                    glMultMatrixd(cam.local_xform.array());
                    glScaled(cam_scale, cam_scale, cam_scale);
                    DD::Image::glColor(cam.gl_color);
                    drawCameraIcon(true/*solid*/);
                }
            }
#endif
        }

    }

}


} // namespace Fsr


// end of Fuser/CameraRigOp.cpp

//
// Copyright 2019 DreamWorks Animation
//
