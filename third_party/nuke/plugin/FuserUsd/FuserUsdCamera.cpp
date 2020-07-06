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

/// @file FuserUsdCamera.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdCamera.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/ExecuteTargetContexts.h>
#include <Fuser/NukeKnobInterface.h>
#include <Fuser/NukeGeoInterface.h>

#include <DDImage/CameraOp.h>
#include <DDImage/Iop.h>
#include <DDImage/gl.h>


namespace Fsr {


//--------------------------------------------------------------------------


/*!
*/
FuserUsdCamera::FuserUsdCamera(const Pxr::UsdStageRefPtr& stage,
                               const Pxr::UsdPrim&        camera_prim,
                               const Fsr::ArgSet&         args,
                               Fsr::Node*                 parent) :
    FuserUsdXform(stage, camera_prim, args, parent)
{
    //std::cout << "  FuserUsdCamera::ctor(" << this << ") cam'" << camera_prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdGeomCamera:
    if (camera_prim.IsValid() && camera_prim.IsA<Pxr::UsdGeomCamera>())
    {
        m_camera_schema = Pxr::UsdGeomCamera(camera_prim);
        if (debug())
        {
            printPrimAttributes("  Camera", camera_prim, false/*verbose*/, std::cout);
            std::cout << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "  FuserUsdCamera::ctor(" << this << "): ";
            std::cerr << "warning, node '" << camera_prim.GetPath() << "'(" << camera_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdCamera::_validateState(const Fsr::NodeContext& args,
                               bool                    for_real)
{
    // Get the time value up to date:
    FuserUsdXform::_validateState(args, for_real);
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdCamera::_execute(const Fsr::NodeContext& target_context,
                         const char*             target_name,
                         void*                   target,
                         void*                   src0,
                         void*                   src1)
{
    // We need a context and a target name to figure out what to do:
    if (!target_name || !target_name[0])
        return -1; // no context target!

    if (debug())
    {
        std::cout << "  FuserUsdCamera::_execute(" << this << ") target='" << target_name << "'";
        std::cout << " Camera";
        std::cout << " '" << getString(Arg::Scene::path) << "'";
        if (m_have_xform)
            std::cout << ", xform" << m_xform;
        else
            std::cout << ", xform disabled";
        std::cout << std::endl;
    }

    // Redirect execution depending on target type:
    if (strncmp(target_name, "DRAW_GL"/*Fsr::PrimitiveViewerContext::name*/, 7)==0)
    {
        // TODO: implement!
#if 0
        // Draw a Camera shape. This should call the Fuser::Camera base class.
        Fsr::PrimitiveViewerContext* pv_ctx =
            reinterpret_cast<Fsr::PrimitiveViewerContext*>(target);

        // Any null pointers throw a coding error:
        if (!pv_ctx || !pv_ctx->vtx || !pv_ctx->ptx)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        drawCamera(pv_ctx->vtx, pv_ctx->ptx);
#endif
        return 0; // success

    }
    else if (strcmp(target_name, Fsr::SceneOpImportContext::name)==0)
    {
        // Translate the Camera node into an AxisOp.
        Fsr::SceneOpImportContext* scene_op_ctx =
            reinterpret_cast<Fsr::SceneOpImportContext*>(target);

        // Any null pointers throw a coding error:
        if (!scene_op_ctx || !scene_op_ctx->op)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        // Possibly redirect to Iop specialization:
        DD::Image::Iop* iop = dynamic_cast<DD::Image::Iop*>(scene_op_ctx->op);
        if (iop)
            importIntoIop(iop, target_context.args());
        else
            importSceneOp(scene_op_ctx->op, target_context.args());

        return 0; // success

    }
    else if (strcmp(target_name, Fsr::GeoOpGeometryEngineContext::name)==0)
    {
        // TODO: implement!
#if 0
        // Add a Fuser::Camera to the geometry list
        Fsr::GeoOpGeometryEngineContext* geo_ctx =
            reinterpret_cast<Fsr::GeoOpGeometryEngineContext*>(target);

        if (!geo_ctx || !geo_ctx->geo || !geo_ctx->geometry_list)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        geoOpGeometryEngine(geo_ctx->geo, *geo_ctx->geometry_list, geo_ctx->obj_index_start);
#endif

        return 0; // success

    }

    // Let base class handle unrecognized targets:
    return FuserUsdXform::_execute(target_context, target_name, target, src0, src1);
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Import node attributes into a Nuke Op.
*/
/*virtual*/
void
FuserUsdCamera::importSceneOp(DD::Image::Op*     op,
                              const Fsr::ArgSet& args)
{
    // Allow camera nodes to import their xforms into any AxisOp subclass:
    if (!dynamic_cast<DD::Image::AxisOp*>(op))
        return; // shouldn't happen...

    const bool debug = args.getBool(Arg::Scene::read_debug, false);
    if (debug)
        std::cout << "    FuserUsdCamera::importSceneOp('" << op->node_name() << "')" << std::endl;

const bool allow_anim = true;

    // Import the Xform data into the Axis_Knob:
    FuserUsdXform::importSceneOp(op, args);

    DD::Image::CameraOp* camera = dynamic_cast<DD::Image::CameraOp*>(op);
    if (!camera)
        return; // skip any camera-specific data if not a CameraOp

    Pxr::UsdPrim camera_prim = m_camera_schema.GetPrim();

#if 0
    // If camera has a rotation-order hint use that for the decompose rotate order:
    const Pxr::UsdAttribute& rot_order_hint_attrib = camera_prim.GetAttribute(Pxr::TfToken("rotateOrderHint"));
    if (rot_order_hint_attrib.IsValid() && rot_order_hint_attrib.GetTypeName().GetType().IsA<std::string>())
    {
        const Pxr::UsdTimeCode timecode = Pxr::UsdTimeCode(*it);
        const double converge_dist = getPrimAttribDouble(converge_dist_attrib, timecode);

    }
#endif

    //std::cout << "    FuserUsdCamera::importSceneOp('" << camera_prim.GetName() << "')" << std::endl;

    //std::cout << "    parent is '" << camera_prim.GetPath().GetParentPath() << "'" << std::endl;
    //std::cout << "  node'" << child->GetPath() << "'[" << child->GetTypeName() << "]";


    // Copy the camera-specific attribs:
    /*
        UsdGeomCamera:
            clippingPlanes            VtArray<GfVec4f>
            clippingRange             GfVec2f          (cm)
            focalLength               float            (mm)
            focusDistance             float            (cm)
            fStop                     float            (lens pupil ratio)
            horizontalAperture        float            (mm)
            horizontalApertureOffset  float            (mm)
            projection                TfToken          ('perspective', 'orthographic')
            purpose                   TfToken          ('')
            shutter:close             double           (time offset)
            shutter:open              double           (time offset)
            stereoRole                TfToken          ('mono', 'left', 'right')
            verticalAperture          float            (mm)
            verticalApertureOffset    float            (mm)
            visibility                TfToken          ('')
            xformOp:transform         GfMat4d
            xformOpOrder              VtArray<TfToken>

        DWA custom vars ('shot_cam_dof.usd', 'shot_cam_format.usd', etc):
            dof                       custom uniform bool   1
            dofExtraFocus             custom float          0
            dofExtraNearFocus         custom float          0
            dofExtraFarFocus          custom float          0

            format                    custom string         ('hd_1080_hvp24 [-24 0 1944 1080] [0 0 1920 1080] 1.0')


        DD::Image::CameraOp:
            projection_mode     perspective
         >> focal               50
         >> haperture           24.576
         >> vaperture           18.672
         >> near                0.1
         >> far                 10000
            win_translate       0 0
            win_scale           1 1
            winroll             0
         >> focal_point         2
         >> fstop               16

        StereoCam2 / CameraOp2:
            shutter               0.5
            shutteroffset         start ('centered', 'start', 'end', 'custom')
            shuttercustomoffset   0.0
            world_scale           0.03
            dof_extra_focus_depth 0
            dof_extra_near_focus  0
            dof_extra_far_focus   0
            dof_tilt_shift_pan    0
            dof_tilt_shift_tilt   0
            dof_max_radius        100


        CamDefocus->StereoCam sync knob mappings:
            // StereoCam name           CamDefocus name
            { "focal",                 "focal"          },
            { "world_scale",           "world_scale"    },
            { "dof_aperture",          "aperture"       },
            { "dof_focus_distance",    "focus_dist"     },
            { "dof_extra_focus_depth", "extra_focus"    },
            { "dof_extra_near_focus",  "extra_near"     },
            { "dof_extra_far_focus",   "extra_far"      },
            { "dof_tilt_shift_pan",    "lens_pan"       },
            { "dof_tilt_shift_tilt",   "lens_tilt"      },
            { "haperture",             "horiz_aperture" },
            //
            { "dummy_knob",            "dummy_knob"     }, // Placeholder for LAST_SYNCABLE_KNOB enum
            //
            { "dof_max_radius",        "max_radius"     }, // Don't allow syncing of this knob for now!


    */
    // Creating a KnobChangeGroup causes Nuke to batch up knobChanged messages,
    // sending only one upon destruction:
    { DD::Image::KnobChangeGroup change_group;

        // Do we have shutter vars? Need both to convert to Nuke ShutterControls:
        int have_shutter_vars = 0;
        double shutter_open   = 0.0;
        double shutter_close  = 0.0;

        const std::vector<Pxr::UsdAttribute>& attribs = camera_prim.GetAttributes();
        for (size_t j=0; j < attribs.size(); ++j)
        {
            const Pxr::UsdAttribute& attrib = attribs[j];

            const Pxr::TfToken& name = attrib.GetName();
            //const Pxr::TfType type = attrib.GetTypeName().GetType();

            if      (name == Pxr::UsdGeomTokens->focalLength)
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("focal"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->horizontalAperture)
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("haperture"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->verticalAperture)
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("vaperture"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->clippingRange)
            {
                // Split into separate lists:
                const AttribDoubles clipping_range(attrib);
                if (clipping_range.doubles_per_value == 2 && clipping_range.isValid())
                {
                    const size_t nSamples = clipping_range.size();
                    std::vector<double> near(nSamples);
                    std::vector<double> far(nSamples);
                    for (size_t i=0; i < nSamples; ++i)
                    {
                        near[i] = clipping_range[i][0];
                        far[i]  = clipping_range[i][1];
                    }
                    Fsr::storeDoublesInKnob(camera->knob("near"), near, 1/*nDblPerVal*/,
                                            clipping_range.times,
                                            0/*element_offset*/, -1/*view*/);
                    Fsr::storeDoublesInKnob(camera->knob("far" ), far,  1/*nDblPerVal*/,
                                            clipping_range.times,
                                            0/*element_offset*/, -1/*view*/);
                }

            }
            else if (name == Pxr::UsdGeomTokens->horizontalApertureOffset)
            {
                // Convert aperture offset in mm to offset in aperture ratio
                // using horizontalAperture value:
                const AttribDoubles haperture_offset(attrib);
                if (haperture_offset.isValid())
                {
                    const size_t nSamples = haperture_offset.size();
                    std::vector<double> win_tx(nSamples);
                    for (size_t i=0; i < nSamples; ++i)
                    {
                        // Get horiz aperture width and scale offset:
                        const double haperture = getPrimAttribDouble(camera_prim, "horizontalAperture", haperture_offset.timeCode(i));
                        win_tx[i] = haperture_offset.value(i) / (haperture / 2.0);
                    }
                    Fsr::storeDoublesInKnob(camera->knob("win_translate"), win_tx, 1/*nDblPerVal*/,
                                            haperture_offset.times,
                                            0/*element_offset*/, -1/*view*/);
                }

            }
            else if (name == Pxr::UsdGeomTokens->verticalApertureOffset)
            {
                // Convert aperture offset in mm to offset in aperture ratio
                // using verticalAperture value:
                const AttribDoubles vaperture_offset(attrib);
                if (vaperture_offset.isValid())
                {
                    const size_t nSamples = vaperture_offset.size();
                    std::vector<double> win_ty(nSamples);
                    for (size_t i=0; i < nSamples; ++i)
                    {
                        // Get vert aperture height and scale offset:
                        const double vaperture = getPrimAttribDouble(camera_prim, "verticalAperture", vaperture_offset.timeCode(i));
                        win_ty[i] = vaperture_offset.value(i) / (vaperture / 2.0);
                    }
                    Fsr::storeDoublesInKnob(camera->knob("win_translate"), win_ty, 1/*nDblPerVal*/,
                                            vaperture_offset.times,
                                            1/*element_offset*/, -1/*view*/);
                }

            }
            //---------------------------------------------------------------------------------------------------
            else if (name == Pxr::UsdGeomTokens->focusDistance)
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("focal_point"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->fStop)
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("fstop"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->clippingPlanes)
            {
                ;// currently unsupported
            }
            else if (name == Pxr::UsdGeomTokens->projection)
            {
                // TODO: should we bother checking this? It's only [perspective, orthographic] atm,
                //       and Nuke's ortho support is dodgey...
                ;// currently unsupported
            }
            else if (name == Pxr::UsdGeomTokens->shutterOpen)
            {
                // For now we don't bother with multiple time samples:
                shutter_open = getPrimAttribDouble(attrib, Pxr::UsdTimeCode::EarliestTime());
                ++have_shutter_vars;
            }
            else if (name == Pxr::UsdGeomTokens->shutterClose)
            {
                // For now we don't bother with multiple time samples:
                shutter_close = getPrimAttribDouble(attrib, Pxr::UsdTimeCode::EarliestTime());
                ++have_shutter_vars;
            }
            else if (name == Pxr::UsdGeomTokens->visibility)
            {
                ;// currently unsupported
            }
            else if (name == Pxr::UsdGeomTokens->purpose)
            {
                ;// currently unsupported
            }
            //---------------------------------------------------------------------------------------------------
            else if (name == "dof")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_enable"), -1/*view*/);
            }
            else if (name == "dofWorldScale")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("world_scale"), -1/*view*/);
            }
            else if (name == "dofExtraFocus")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_extra_focus_depth"), -1/*view*/);
            }
            else if (name == "dofExtraNearFocus")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_extra_near_focus"), -1/*view*/);
            }
            else if (name == "dofExtraFarFocus")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_extra_far_focus"), -1/*view*/);
            }
            else if (name == "dofTiltShiftPan")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_tilt_shift_pan"), -1/*view*/);
            }
            else if (name == "dofTiltShiftTilt")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_tilt_shift_tilt"), -1/*view*/);
            }
            else if (name == "dofMaxRadius")
            {
                copyAttribToKnob(attrib, allow_anim, camera->knob("dof_max_radius"), -1/*view*/);
            }
        }

        // Convert the USD shutter values into Nuke ShutterControl values.
        // For now we don't bother with multiple time samples or views.
        if (have_shutter_vars > 0)
        {
            const double length = (shutter_close - shutter_open);
            if (length < std::numeric_limits<double>::epsilon())
            {
                // Off shutter:
                storeDoubleInKnob(0.0, camera->knob("shutter"            ), Fsr::defaultFrameValue());
                storeIntInKnob(     1, camera->knob("shutteroffset"      ), Fsr::defaultFrameValue()); // 'start'
                storeDoubleInKnob(0.0, camera->knob("shuttercustomoffset"), Fsr::defaultFrameValue());
            }
            else
            {
                storeDoubleInKnob(length, camera->knob("shutter"), Fsr::defaultFrameValue());
                if (fabs(shutter_open) < std::numeric_limits<double>::epsilon())
                {
                    // Forward shutter starting at 0:
                    storeIntInKnob(1, camera->knob("shutteroffset"), Fsr::defaultFrameValue()); // 'start'
                    storeDoubleInKnob(0.0, camera->knob("shuttercustomoffset"), Fsr::defaultFrameValue());
                }
                else if (fabs(shutter_close) < std::numeric_limits<double>::epsilon())
                {
                    // Backwards shutter ending at 0:
                    storeIntInKnob(2, camera->knob("shutteroffset"), Fsr::defaultFrameValue()); // 'end'
                    storeDoubleInKnob(0.0, camera->knob("shuttercustomoffset"), Fsr::defaultFrameValue());
                }
                else
                {
                    // Custom shutter:
                    // TODO: finish this - need to figure out how custom offset mode works
                    storeIntInKnob(3, camera->knob("shutteroffset"), Fsr::defaultFrameValue()); // 'custom'
                    storeDoubleInKnob(shutter_open, camera->knob("shuttercustomoffset"), Fsr::defaultFrameValue());
                }
            }
        }

    } // DD::Image::KnobChangeGroup

}


/*! TODO: This is super-janky, clean it up!

    Need to make a mapping registration system so plugins can register mapping
    callbacks.

*/
/*virtual*/
void
FuserUsdCamera::importIntoIop(DD::Image::Iop*    iop,
                              const Fsr::ArgSet& args)
{
    if (!iop)
        return; // shouldn't happen...

    const bool debug = args.getBool(Arg::Scene::read_debug, false);

    Pxr::UsdPrim camera_prim = m_camera_schema.GetPrim();

    if (debug)
    {
        std::cout << "    FuserUsdCamera::importIntoIop('" << camera_prim.GetName() << "')";
        std::cout << "iop='" << iop->node_name() << "')";
        std::cout << std::endl;
    }

    // CamDefocus knob mappings:
    // TODO: soooooper-janky! Need to make a mapping registration system...!
    if ((strcmp(iop->Class(), "CamDefocus")==0))
    {
        /* CamDefocus:
            "focal_length"     "sync_focal_length"
            "fstop"            "sync_fstop"
            "focus_dist"       "sync_focus_dist"
            //
            "extra_focus"      "sync_extra_focus"
            "extra_near"       "sync_extra_near"
            "extra_far"        "sync_extra_far"
            //
            "lens_pan"         "sync_tiltshift_pan"
            "lens_tilt"        "sync_tiltshift_tilt"
            //
            "world_scale"      "sync_world_scale"
            "horiz_aperture"   "sync_horiz_aperture"
            "max_radius"       "sync_max_radius"
        */


const bool allow_anim = true;

        // Creating a KnobChangeGroup causes Nuke to batch up knobChanged messages,
        // sending only one upon destruction:
        { DD::Image::KnobChangeGroup change_group;

            const std::vector<Pxr::UsdAttribute>& attribs = camera_prim.GetAttributes();
            for (size_t j=0; j < attribs.size(); ++j)
            {
                const Pxr::UsdAttribute& attrib = attribs[j];

                const Pxr::TfToken& name = attrib.GetName();
                //const Pxr::TfType type = attrib.GetTypeName().GetType();

                if      (name == Pxr::UsdGeomTokens->focalLength)
                {
                    if (getBoolValue(iop->knob("sync_focal_length")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("focal_length"), -1/*view*/);
                }
                else if (name == Pxr::UsdGeomTokens->horizontalAperture)
                {
                    if (getBoolValue(iop->knob("sync_horiz_aperture")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("horiz_aperture"), -1/*view*/);
                }
                //---------------------------------------------------------------------------------------------------
                else if (name == Pxr::UsdGeomTokens->focusDistance)
                {
                    if (getBoolValue(iop->knob("sync_focus_dist")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("focus_dist"), -1/*view*/);
                }
                else if (name == Pxr::UsdGeomTokens->fStop)
                {
                    if (getBoolValue(iop->knob("sync_fstop")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("fstop"), -1/*view*/);
                }
                else if (name == Pxr::UsdGeomTokens->shutterOpen)
                {
                    ;// currently unsupported
                }
                else if (name == Pxr::UsdGeomTokens->shutterClose)
                {
                    ;// currently unsupported
                }
                //---------------------------------------------------------------------------------------------------
                else if (name == "dof")
                {
                    //copyAttribToKnob(attrib, allow_anim, iop->knob("dof_enable"), -1/*view*/);
                }
                else if (name == "dofWorldScale")
                {
                    if (getBoolValue(iop->knob("sync_world_scale")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("world_scale"), -1/*view*/);
                }
                else if (name == "dofExtraFocus")
                {
                    if (getBoolValue(iop->knob("sync_extra_focus")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("extra_focus"), -1/*view*/);
                }
                else if (name == "dofExtraNearFocus")
                {
                    if (getBoolValue(iop->knob("sync_extra_near")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("extra_near"), -1/*view*/);
                }
                else if (name == "dofExtraFarFocus")
                {
                    if (getBoolValue(iop->knob("sync_extra_far")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("extra_far"), -1/*view*/);
                }
                else if (name == "dofTiltShiftPan")
                {
                    if (getBoolValue(iop->knob("sync_tiltshift_pan")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("lens_pan"), -1/*view*/);
                }
                else if (name == "dofTiltShiftTilt")
                {
                    if (getBoolValue(iop->knob("sync_tiltshift_tilt")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("lens_tilt"), -1/*view*/);
                }
                else if (name == "dofMaxRadius")
                {
                    if (getBoolValue(iop->knob("sync_max_radius")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("max_radius"), -1/*view*/);
                }
                else if (name == "dofBlurShape")
                {
                    // Currently-supported CamDefocus shapes:
                    //      'disc', 'bladed', 'square'
                    if (getBoolValue(iop->knob("sync_disc_shape")))
                    {
                        Pxr::VtValue v; attrib.Get(&v, Pxr::UsdTimeCode::EarliestTime()/*random frame*/);
                        const std::string blur_shape(v.Get<Pxr::TfToken>().GetString());
                        if (!blur_shape.empty())
                        {
                            // TODO: put this in copyAttribToKnob():
                            DD::Image::Knob* k = iop->knob("filter_type");
                            if (k)
                                k->set_text(blur_shape.c_str());
                        }
                    }
                }
                else if (name == "dofAspectRatio")
                {
                    if (getBoolValue(iop->knob("sync_disc_aspect")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("aspect"), -1/*view*/);
                }
                else if (name == "dofBladeCount")
                {
                    if (getBoolValue(iop->knob("sync_blade_count")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("blades"), -1/*view*/);
                }
                else if (name == "dofBladeRoundness")
                {
                    if (getBoolValue(iop->knob("sync_blade_roundness")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("roundness"), -1/*view*/);
                }
                else if (name == "dofBladeRotation")
                {
                    if (getBoolValue(iop->knob("sync_blade_rotation")))
                        copyAttribToKnob(attrib, allow_anim, iop->knob("rotation"), -1/*view*/);
                }
            }

        } // DD::Image::KnobChangeGroup

    } // CamDefocus knob mappings

}


} // namespace Fsr


// end of FuserUsdCamera.cpp

//
// Copyright 2019 DreamWorks Animation
//
