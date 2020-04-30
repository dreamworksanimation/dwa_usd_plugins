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

/// @file AxisManipulator.cpp
///
/// @author Jonathan Egstad


#include "AxisManipulator.h"

#include <Fuser/api.h>  // for stringTrim

#include <DDImage/GeoOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/Scene.h>
#include <DDImage/Application.h>
#include <DDImage/Enumeration_KnobI.h>
#include <DDImage/gl.h>

#include <sstream>

using namespace DD::Image;

namespace zpr {


/*static*/ int              AxisManipulator::global_placement_mode        = AxisManipulator::PLACE_OFF;
/*static*/ AxisManipulator* AxisManipulator::global_placement_manipulator = NULL;


/*!
*/
AxisManipulator::AxisManipulator()
{
    k_place_light         = 0;
    k_place_distance      = 1000.0;
    k_place_maintain_size = true;
}


/*!
*/
void
AxisManipulator::addManipulatorKnobs(Knob_Callback f,
                                     bool          in_viewer)
{
    // Add viewer HUD knobs:
    BeginGroup(f, "manipulator_toolbar", "Light Manipulator Controls");
    {
        if (in_viewer)
        {
            SetFlags(f, Knob::TOOLBAR_GROUP | Knob::TOOLBAR_TOP);
            Text_knob(f, "AxisManipulator");
                ClearFlags(f, Knob::STARTLINE);
        }
        else
        {
            SetFlags(f, Knob::CLOSED);
        }
        kViewerLightNames = Enumeration_knob(f, &k_place_light, 0, "light_list", "    Place Light:");
            SetFlags(f, Knob::SAVE_MENU);
            Tooltip(f, "Select the light you want to manipulate from this list then click one of the "
                       "manipulate mode buttons below.\n"
                       "Using the mouse-left button click & drag in the image where you want the "
                       "light to be placed or directed.  This also works in the 3D viewer.");
        if (in_viewer) {
            Text_knob(f, " -> ");
                ClearFlags(f, Knob::STARTLINE);
        } else {
             Newline(f);
        }
        kPlaceLightOnReflection = PyScript_knob(f, "", "place_on_reflection", " On Reflection Vec ");
            SetFlags(f, Knob::DO_NOT_WRITE | Knob::NO_ANIMATION | Knob::NO_RERENDER | Knob::NO_UNDO);
            Tooltip(f, "Places the light on the reflected vector from "
                       "the current viewpoint to the selected surface normal "
                       "and orients it towards the surface point.");
        kPlaceLightOnNormal = PyScript_knob(f, "", "place_on_normal", "  On Normal  ");
            SetFlags(f, Knob::DO_NOT_WRITE | Knob::NO_ANIMATION | Knob::NO_RERENDER | Knob::NO_UNDO);
            Tooltip(f, "Places the light on the selected surface normal and orients it towards the surface point.");
        kPlaceLightOnSurface = PyScript_knob(f, "", "place_on_surface", "  At Surface  ");
            SetFlags(f, Knob::DO_NOT_WRITE | Knob::NO_ANIMATION | Knob::NO_RERENDER | Knob::NO_UNDO);
            Tooltip(f, "Places the light on the selected surface point and orients it to the surface normal.");
        kOrientLightToSurface = PyScript_knob(f, "", "orient_to_surface", "  Point At Surface  ");
            SetFlags(f, Knob::DO_NOT_WRITE | Knob::NO_ANIMATION | Knob::NO_RERENDER | Knob::NO_UNDO);
            Tooltip(f, "Orients the light to the selected surface point.  Might not work if the light is parented.");
        Double_knob(f, &k_place_distance, "place_distance", "At Z Distance");
            ClearFlags(f, Knob::SLIDER);
            if (in_viewer)
               ClearFlags(f, Knob::STARTLINE);
            else
               SetFlags(f, Knob::STARTLINE);
            SetFlags(f, Knob::NO_MULTIVIEW);
            Tooltip(f, "Z distance to place light away from selected surface.");
        Bool_knob(f, &k_place_maintain_size, "autosize", "autosize");
    }
    EndGroup(f);
    // This custom knob gets us geometry feedback:
    CustomKnob1(GlueKnob, f, this, "geo_feedback_dummy");
}


/*!
*/
int
AxisManipulator::knobChanged(Knob* k)
{
    //std::cout << "AxisManipulator::knobChanged(" << this << ")" << std::endl;
    if (k == 0)
        return 0;

    if (k == kViewerLightNames)
    {
        //manipulatorOp()->knob("place_enable")->set_value(1);
        //setManipulatorMode(PLACE_OFF, 0/*knob*/);
        updateManipulatorMenu();
        return 1;
    }
    //else if (k->is("place_enable"))
    //{
    //    setManipulatorMode(PLACE_OFF, k);
    //    updateManipulatorMenu();
    //    return 1;
    //}
    else if (k == kPlaceLightOnReflection)
    {
        setManipulatorMode(PLACE_ON_REFLECTION, k);
        return 1;
    }
    else if (k == kPlaceLightOnNormal)
    {
        setManipulatorMode(PLACE_ON_NORMAL, k);
        return 1;
    }
    else if (k == kPlaceLightOnSurface)
    {
        setManipulatorMode(PLACE_ON_SURFACE, k);
        return 1;
    }
    else if (k == kOrientLightToSurface)
    {
        setManipulatorMode(ORIENT_LIGHT_TO_SURFACE, k);
        return 1;
    }
    return 0;
}


#if 0
/*!
*/
static void turn_button_off(Knob* k)
{
    if (!k)
        return;
    k->set_value(false);
    k->changed();
}
#endif


/*!
*/
void
AxisManipulator::setManipulatorMode(int new_mode, Knob* /*k_changed*/)
{
    //std::cout << "AxisManipulator::setManipulatorMode(" << this << "): current mode=" << global_placement_mode << ", new mode=" << new_mode;

    // Turn old mode off:
    if (global_placement_manipulator == this &&
        (global_placement_mode == new_mode || new_mode == PLACE_OFF))
    {
        global_placement_mode = PLACE_OFF;
        global_placement_manipulator = 0;
        //turn_button_off(m_current_placement_mode);
        //m_current_placement_mode = 0;
    }
    else
    {
#if 0
        Knob* k = 0;
        switch (new_mode) {
        case PLACE_ON_REFLECTION:     k = kPlaceLightOnReflection; break;
        case PLACE_ON_NORMAL:         k = kPlaceLightOnNormal; break;
        case PLACE_ON_SURFACE:        k = kPlaceLightOnSurface; break;
        case ORIENT_LIGHT_TO_SURFACE: k = kOrientLightToSurface; break;
        }
        m_current_placement_mode = k;
#endif
        global_placement_mode = new_mode;
        global_placement_manipulator = this;
    }
    //std::cout << ", new global_placement_mode=" << global_placement_mode << std::endl;
}


/*!
*/
void
AxisManipulator::updateManipulatorMenu()
{
    if (!DD::Image::Application::gui)
        return;

    DD::Image::Scene* scene = manipulatorScene();

    DD::Image::Hash light_list_hash;
    if (scene)
    {
        const uint32_t nLights = (uint32_t)scene->lights.size();
        for (uint32_t i=0; i < nLights; ++i)
        {
            light_list_hash.append(i);
            const LightContext* ltx = scene->lights[i];
            if (ltx && ltx->light())
                light_list_hash.append(ltx->light()->hash());
        }
    }
    if (light_list_hash == m_light_list_hash)
        return;  // no change, skip update
    m_light_list_hash = light_list_hash;

    Enumeration_KnobI* the_menu = kViewerLightNames->enumerationKnob();
    if (!the_menu)
        return;

    if (the_menu->menu().size() != 0)
    {
        // Strip spaces:
        m_current_light = Fsr::stringTrim(the_menu->menu()[k_place_light], " ");
    }
    //std::cout << "AxisManipulator::updateLightMenu('" << m_current_light << "')" << std::endl;

    std::vector<std::string> light_names;

    // Have default be 'none':
    light_names.push_back(std::string("none"));

    if (!scene)
    {
        m_current_light = "none";
        kViewerLightNames->set_value(0);
    }
    else
    {
        const uint32_t nLights = (uint32_t)scene->lights.size();
        light_names.reserve(nLights);
        uint32_t matched = 0;
        for (uint32_t i=0; i < nLights; ++i)
        {
            const LightContext* ltx = scene->lights[i];
            if (!ltx || !ltx->light())
            {
                light_names.push_back("-");
            }
            else
            {
                std::stringstream ss;
                ltx->light()->print_name(ss);
                light_names.push_back(ss.str());
                if (ss.str() == m_current_light)
                    matched = (uint32_t)light_names.size()-1;
            }
        }
        //std::cout << "   matched=" << matched << std::endl;

        the_menu->menu(light_names);
        kViewerLightNames->set_value(matched);
    }

    kViewerLightNames->updateWidgets();
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


/*! And you need to implement this to make Nuke call draw_handle.
*/
bool
AxisManipulator::GlueKnob::build_handle(DD::Image::ViewerContext* /*ctx*/)
{
    assert(parent);
    if (!global_placement_manipulator ||
         global_placement_manipulator->manipulatorOp()->firstOp() != parent->manipulatorOp()->firstOp())
        return false;
    //std::cout << "AxisManipulator::GlueKnob::build_handle(" << parent << ") mode=" << global_placement_mode << std::endl;
    return true;//(ctx->transform_mode() != VIEWER_2D);
}


/*! This is what Nuke will call once the below stuff is executed:
*/
static bool
handle_click_cb(DD::Image::ViewerContext* ctx,
                DD::Image::Knob*          knob,
                int                       /*index*/)
{
    if (!AxisManipulator::global_placement_manipulator)
        return false;
    return AxisManipulator::global_placement_manipulator->handleManipulatorClick(ctx);
}


/*! Nuke calls this to draw the handle, this then calls make_handle
    which tells Nuke to call the above function when the mouse does
    something...
*/
void
AxisManipulator::GlueKnob::draw_handle(DD::Image::ViewerContext* ctx)
{
    assert(parent);
    if (!global_placement_manipulator ||
          global_placement_manipulator->manipulatorOp()->firstOp() != parent->manipulatorOp()->firstOp())
        return;

    //std::cout << "AxisManipulator::GlueKnob::draw_handle(" << parent << "): event=" << ctx->event() << std::endl;
    if (//ctx->event() == DRAW_OPAQUE ||
        //ctx->event() == DRAW_LINES  ||
        ctx->event() == PUSH        || // true for clicking hit-detection
        ctx->event() == DRAG        || //true when cursor moves in screen
        //ctx->event() == CURSOR      || //true when cursor moves in screen
        ctx->event() == RELEASE        //true for selection box hit-detection
        )
    {
        // Make clicks anywhere in the viewer call handle() with index = 0.
        // This takes the lowest precedence over, so above will be detected first.
        begin_handle(Knob::ANYWHERE, ctx, handle_click_cb, 0/*index*/, 0,0,0/*xyz*/);
        end_handle(ctx);
    }

    global_placement_manipulator->drawManipulatorIcons(ctx);
}


/*!
*/
void
AxisManipulator::drawManipulatorIcons(DD::Image::ViewerContext* ctx)
{
    // Draw crosshair:
    if (ctx->event() == DRAW_LINES ||
        ctx->event() == DRAW_SHADOW)
    {
        if (ctx->event() == DRAW_LINES)
        {
            float px = ctx->x();
            float py = ctx->y();
            glColor3f(1.0f, 1.0f, 0.0f); // Yellow
            if (ctx->viewer_mode() == VIEWER_2D)
            {
                glBegin(GL_LINES);
                   glVertex2f(px-10, py);
                   glVertex2f(px+10, py);
                   glVertex2f(px,    py-10);
                   glVertex2f(px,    py+10);
                glEnd();

            }
            else
            {
                const float d = ctx->icon_size();
                glBegin(GL_LINES);
                   glVertex3f(ctx->x()-d, ctx->y(),   ctx->z());
                   glVertex3f(ctx->x()+d, ctx->y(),   ctx->z());
                   glVertex3f(ctx->x(),   ctx->y()-d, ctx->z());
                   glVertex3f(ctx->x(),   ctx->y()+d, ctx->z());
                   glVertex3f(ctx->x(),   ctx->y(),   ctx->z()-d);
                   glVertex3f(ctx->x(),   ctx->y(),   ctx->z()+d);
                glEnd();
            }
        }

        // Draw placement mode message at top of Viewer:
        const Box& vbox = ctx->visibleViewportArea();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, vbox.w(), 0, vbox.h(), -100, 100);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        const char* msg = "PLACEMENT MODE ENABLED";
        int charW = 10;
        int px = int(vbox.center_x() - float((int)strlen(msg)/2*charW));
        int py = vbox.t()-70-charW;
        if (ctx->event() == DRAW_LINES)
        {
            glRasterPos2i(px, py);
        }
        else
        {
            // Shadow - draw black with offset
            glColor3f(0.0f, 0.0f, 0.0f);
            glRasterPos2i(px-1, py-1);
        }
        gl_text(msg);

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


static void
setValue(Knob* k, double v)
{
    if (!k)
        return;
    //k->clear_animated(-1); // clear all the sub-knobs
    k->set_value(v, 0);
    k->changed();
}

static void
setXY(Knob* k, double x, double y)
{
    if (!k)
        return;
    //k->clear_animated(-1); // clear all the sub-knobs
    k->set_value(x, 0);
    k->set_value(y, 1);
    k->changed();
}
static void
setXYZ(Knob* k, double x, double y, double z)
{
    if (!k)
        return;
    //k->clear_animated(-1); // clear all the sub-knobs
    k->set_value(x, 0);
    k->set_value(y, 1);
    k->set_value(z, 2);
    k->changed();
}
static void
setXYZ(Knob* k, const Fsr::Vec3d& v)
{
    if (!k)
        return;
    //k->clear_animated(-1); // clear all the sub-knobs
    k->set_value(v.x, 0);
    k->set_value(v.y, 1);
    k->set_value(v.z, 2);
    k->changed();
}


//----------------------------------------------------------------------------


/*! This is the eventual function that will be called, where you put
    in whatever you really want to have happen as the user clicks on
    the viewer.
*/
bool
AxisManipulator::handleManipulatorClick(DD::Image::ViewerContext* ctx)
{
#ifdef DEBUG_MANIPULATOR
    std::cout << "AxisManipulator::handleManipulatorClick(): viewer_mode=" << (int)ctx->viewer_mode() << ", event=" << (int)ctx->event() << std::endl;
#endif
    DD::Image::Scene* scene = manipulatorScene();
    DD::Image::Op* op = manipulatorOp();
#ifdef DEBUG_MANIPULATOR
    std::cout << "  global_placement_mode=" << global_placement_mode << ", scene=" << scene << ", op=" << op << std::endl;
    if (op)
        std::cout << "  node_name='" << op->node_name() << "'" << std::endl;
#endif

#if 1
    // TODO: reintroducing this light0 bug until we can fix the way lights
    // are addressed in the placement UI:
    if (global_placement_mode == PLACE_OFF || !scene || !op)
#else
    if (k_place_light <= 0/*none*/ || global_placement_mode == PLACE_OFF || !scene || !op)
#endif
        return false;

    // Get light pointer:
    DD::Image::AxisOp* lightOp = 0;
    // Get AxisOp* from map:
#ifdef DEBUG_MANIPULATOR
    std::cout << "k_place_light=" << k_place_light;
    std::cout << ", lights=" << scene->lights.size() << std::endl;
#endif
    if (scene->lights.size() == 0)
        return false;

    lightOp = scene->lights[clamp(k_place_light-1, 0, (int)scene->lights.size()-1)]->light();


    if (!lightOp)
        return false;
    lightOp->validate(true);
    DD::Image::AxisOp* parentOp = lightOp->input0();
    // Is it a ReflectionCard?
    bool is_reflection_card = (strcmp(lightOp->Class(), "ReflectionCard")==0);

    const uint32_t nObjects = scene->objects();
#ifdef DEBUG_MANIPULATOR
    std::cout << "nObjects=" << nObjects << std::endl;
#endif
    if (nObjects == 0)
        return false;

    if (ctx->event() == RELEASE)
    {
        setManipulatorMode(PLACE_OFF, 0/*knob*/);
#ifdef DEBUG_MANIPULATOR
        std::cout << "AxisManipulator::handle_click: RELEASE" << std::endl;
#endif
        // True means we want to 'eat' the event, don't pass it on:
        return true;

    }
    else if (ctx->event() != PUSH && ctx->event() != DRAG)
    {
        // False means we don't care about the event, pass it on:
        return false;
    }
#ifdef DEBUG_MANIPULATOR
    std::cout << "AxisManipulator::handleManipulatorClick():";
    std::cout << " viewer_mode=" << (int)ctx->viewer_mode() << ", transform_mode=" << (int)ctx->transform_mode();
    std::cout << ", xyz=" << ctx->x() << "," << ctx->y() << "," << ctx->z();
    std::cout << ", mousexy=" << ctx->mouse_x() << "," << ctx->mouse_y() << " key=" << ctx->key() << std::endl;
#endif

#if 0
    // Build the view ray:
    DD::Image::Ray R;
    if (ctx->viewer_mode() == VIEWER_2D)
    {
        //std::cout << "   cam_matrix: " << ctx->cam_matrix() << std::endl;
        //std::cout << "  proj_matrix: " << ctx->proj_matrix() << std::endl;
        //std::cout << "  modelmatrix: " << ctx->modelmatrix << std::endl;
        //std::cout << "   camera_pos: " << ctx->camera_pos().x << " " << ctx->camera_pos().y << " " << ctx->camera_pos().z << " " << ctx->camera_pos().w << std::endl;
        /*
            struct ViewerWindowFormatContext
            {
                DD::Image::Format   format;
                DD::Image::Vector2  formatCenter;
                float               formatWidth;
                bool                ignoreFormatPixelApsect;
            };
        */
DD::Image::Format& f = ctx->viewerWindowFormatContext().format;
std::cout << "      format: " << f.x() << " " << f.y() << " " << f.r() << " " << f.t() << std::endl;
        float panx, pany, zoomx, zoomy;
        ctx->getViewerWindowPanZoom(ctx->viewerWindowFormatContext(), panx, pany, zoomx, zoomy);
std::cout << "      panx=" << panx << ", pany=" << pany << ", zoomx=" << zoomx << ", zoomy=" << zoomy << std::endl;
        float fcx = float(f.r() - f.x())*0.5f*zoomx + panx;
        float fcy = float(f.t() - f.y())*0.5f*zoomy + pany;
std::cout << "      fcx=" << fcx << ", fcy=" << fcy << std::endl;

        Vector3 P0, P1;
        GLUnproject(fcx, fcy, 0.0f, ctx->modelmatrix, ctx->viewport(), &P0.x, &P0.y, &P0.z);
        GLUnproject(0.0f*zoomx + panx, fcy, -100000.0f, ctx->modelmatrix, ctx->viewport(), &P1.x, &P1.y, &P1.z);
//Matrix4 proj = ctx->modelmatrix.inverse();
//Vector3 P0 = proj.transform(Vector3(ctx->x(), ctx->y(), 0.0f));
//Vector3 P1 = proj.transform(Vector3(ctx->x(), ctx->y(), 1.0f));
        std::cout << " P0[" << P0.x << " " << P0.y << " " << P0.z << "]" << std::endl;
        std::cout << " P1[" << P1.x << " " << P1.y << " " << P1.z << "]" << std::endl;
        R.src = P0;
        R.dir = (P1 - P0);
        R.dir.normalize();

    }
    else
    {
        Matrix4 icam = ctx->cam_matrix().inverse();
        Vector3 P0 = icam.translation();
        Matrix4 proj = ctx->proj_matrix() * ctx->cam_matrix();
        Vector3 P1;
        GLUnproject(ctx->x()+0.5f, ctx->y()+0.5f, -1.0f, proj, ctx->viewport(), &P1.x, &P1.y, &P1.z);
        std::cout << " P0[" << P0.x << " " << P0.y << " " << P0.z << "]" << std::endl;
        std::cout << " P1[" << P1.x << " " << P1.y << " " << P1.z << "]" << std::endl;
        R.src = P0;
        R.dir = (P1 - P0);
        R.dir.normalize();
    }
    R.minT = 0.0f;
    R.maxT = std::numeric_limits<float>::infinity();
    std::cout << "  R[" << R.src.x << " " << R.src.y << " " << R.src.z << "]";
    std::cout << "[" << R.dir.x << " " << R.dir.y << " " << R.dir.z << "]" << std::endl;

    //bool testRayIntersection(Ray& ray, const VertexContext* vtx = NULL, const Iop* material = NULL );
    if (scene->testRayIntersection(R, 0/*vtx*/, 0/*material*/))
    {
        // Hit something, get intersection info:
    }
#endif
    
    //manipulateLight(R.origin, -R.direction(), PW, N);
    {
        Fsr::Vec3d camPW, camV, surfPW, surfN;
        if (intersectScene(ctx, camPW, camV, surfPW, surfN))
        {
            Fsr::Vec3d N = surfN;
            double ang_x, ang_y;

            switch (global_placement_mode)
            {
                case PLACE_ON_REFLECTION:
                    //printf("PLACE_ON_REFLECTION\n");
                    // Get the reflected surface normal and the rotation angles:
                    //N = camV.reflect(N);
                    N = N*(camV.dot(N) * 2.0) - camV;
                    N.normalize();

                case PLACE_ON_NORMAL:
                    // Orient the light using the defined normal:
                    ang_y = degrees( atan2(N.x, N.z));
                    ang_x = degrees(-atan2(N.y, sqrt(N.x*N.x + N.z*N.z)));
                    if (parentOp)
                    {
                        // Place light parent translate at surface point and rotate it:
                        setXYZ(parentOp->knob("translate"), surfPW);
                        setXYZ(parentOp->knob("rotate"), ang_x, ang_y, 0.0);
                        if (k_place_maintain_size)
                        {
                            if (is_reflection_card)
                            {
                                setValue(lightOp->knob("z"), k_place_distance);
                            }
                            else
                            {
                                setXYZ(lightOp->knob("translate"), 0, 0, k_place_distance);
                                setValue(lightOp->knob("uniform_scale"), k_place_distance);
                            }

                        }
                        else
                        {
                            setXYZ(lightOp->knob("translate"), 0, 0, k_place_distance);
                            // Reset the light local translate except to set Z
                            // to the distace between the light and point,
                            // this should keep the light in a relatively stable location:
                            //setXYZ(lightOp->knob("translate"), 0, 0, lightOp->matrix().translation().distanceBetween(surfPW));
                        }
                    }
                    else
                    {
                        // No parent, place the light at the k_place_distance
                        // away from the surface point along the normal vector:
                        if (k_place_maintain_size)
                        {
                            if (is_reflection_card)
                            {
                                setXYZ(lightOp->knob("translate"), surfPW);
                                setValue(lightOp->knob("z"), k_place_distance);
                            }
                            else
                            {
                                setXYZ(lightOp->knob("translate"), surfPW + N*k_place_distance);
                                setValue(lightOp->knob("uniform_scale"), k_place_distance);
                            }
                        }
                        else
                        {
                            setXYZ(lightOp->knob("translate"), surfPW + N*k_place_distance);
                            if (is_reflection_card)
                                setValue(lightOp->knob("z"), k_place_distance);
                        }
                        setXY(lightOp->knob("rotate"), ang_x, ang_y);
                    }
                    break;

                case PLACE_ON_SURFACE:
                    //printf("PLACE_ON_SURFACE\n");
                    // Place light translate at surface point:
                    setXYZ(lightOp->knob("translate"), surfPW);

                    // Orient the light using the surface normal:
                    ang_y = degrees( atan2(surfN.x, surfN.z));
                    ang_x = degrees(-atan2(surfN.y, sqrt(surfN.x*surfN.x + surfN.z*surfN.z)));

                    setXY(lightOp->knob("rotate"), ang_x, ang_y);

                    if (is_reflection_card)
                        setValue(lightOp->knob("z"), 0.0);
                    break;

                case ORIENT_LIGHT_TO_SURFACE:
                    //printf("ORIENT_LIGHT_TO_SURFACE\n");
                    // Orient the light using the reflected surface normal:
                    N = lightOp->matrix().translation() - surfPW;
                    N.normalize();

                    ang_y = degrees(atan2(N.x, N.z));
                    ang_x = degrees(atan2(-N.y, sqrt(N.x*N.x + N.z*N.z)));

                    setXY(lightOp->knob("rotate"), ang_x, ang_y);
                    break;
            }

            op->asapUpdate();
        }

    } // manipulateLight

    // Return true and 'eat' the event even if the intersect fails
    // so that we don't drop out of draw-anywhere mode:
    return true;
}


} // namespace zpr

// end of AxisManipulator.cpp

//
// Copyright 2020 DreamWorks Animation
//
