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

/// @file FuserUsdXform.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdXform.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/AxisKnob.h>     // for AxisKnobVals
#include <Fuser/ExecuteTargetContexts.h>
#include <Fuser/NukeKnobInterface.h>
#include <Fuser/NukeGeoInterface.h>

#include <DDImage/AxisOp.h>
#include <DDImage/LightOp.h>
#include <DDImage/CameraOp.h>
#include <DDImage/gl.h>


#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

// Turn off the inclusion of python files in the usd lib:
#include <pxr/pxr.h> // << this is where the #define lives, include it first
#undef PXR_PYTHON_SUPPORT_ENABLED

#include <pxr/base/tf/token.h>
#include <pxr/base/gf/math.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/transform.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/scope.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {


/*!
*/
FuserUsdXform::FuserUsdXform(const Pxr::UsdStageRefPtr& stage,
                             const Pxr::UsdPrim&        xform_prim,
                             const Fsr::ArgSet&         args,
                             Fsr::Node*                 parent) :
    FuserUsdXformableNode(stage, args, parent)
{
    //std::cout << "    FuserUsdXform::ctor(" << this << "): xform'" << xform_prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdGeomXformable:
    if (xform_prim.IsValid() && xform_prim.IsA<Pxr::UsdGeomXformable>())
    {
        m_xformable_schema = Pxr::UsdGeomXformable(xform_prim);
        if (0)//(debug())
        {
            printPrimAttributes("  Xform", xform_prim, false/*verbose*/, std::cout);
            std::cout << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "    FuserUsdXform::ctor(" << this << "): ";
            std::cerr << "warning, node '" << xform_prim.GetPath() << "'(" << xform_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdXform::_validateState(const Fsr::NodeContext& exec_ctx,
                              bool                    for_real)
{
    // Get the time value up to date:
    FuserUsdXformableNode::_validateState(exec_ctx, for_real);
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdXform::_execute(const Fsr::NodeContext& target_context,
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
        std::cout << "  FuserUsdXform::_execute(" << this << ") target='" << target_name << "'";
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
#if 0
        // TODO: implement!
        // Draw an Xform shape (axis). This should be put in a Fuser::Xform base class.
        Fsr::PrimitiveViewerContext* pv_ctx =
            reinterpret_cast<Fsr::PrimitiveViewerContext*>(target);

        // Any null pointers throw a coding error:
        if (!pv_ctx || !pv_ctx->vtx || !pv_ctx->ptx)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        drawXform(pv_ctx->vtx, pv_ctx->ptx);
#endif

        return 0; // success

    }
    else if (strcmp(target_name, Fsr::SceneOpImportContext::name)==0)
    {
        // Translate the Light node into an AxisOp.
        Fsr::SceneOpImportContext* scene_op_ctx =
            reinterpret_cast<Fsr::SceneOpImportContext*>(target);

        if (!scene_op_ctx || !scene_op_ctx->op)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        importSceneOp(scene_op_ctx->op, target_context.args());

        return 0; // success

    }
    else if (strcmp(target_name, Fsr::GeoOpGeometryEngineContext::name)==0)
    {
#if 0
        // TODO: implement!
        // Add a Fuser::Light to the geometry list
        Fsr::GeoOpGeometryEngineContext* geo_ctx =
            reinterpret_cast<Fsr::GeoOpGeometryEngineContext*>(target);

        if (!geo_ctx || !geo_ctx->geo || !geo_ctx->geometry_list)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        geoOpGeometryEngine(geo_ctx->geo, *geo_ctx->geometry_list, geo_ctx->obj_index_start);
#endif

        return 0; // success

    }

    // Let base class handle unrecognized targets:
    return FuserUsdXformableNode::_execute(target_context, target_name, target, src0, src1);
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Returns true if prim can concatenate its transform.
*/
/*static*/ bool
FuserUsdXform::canConcatenateTransform(const Pxr::UsdPrim& prim)
{
    return (prim.IsValid() && !prim.IsPseudoRoot() &&
           (prim.IsA<Pxr::UsdGeomXformable>() || prim.IsA<Pxr::UsdGeomScope>()));
}


/*! Find the total first-last keyframe ranges for the prim and all its parents.
*/
/*static*/ void
FuserUsdXform::getConcatenatedXformOpTimeSamples(const Pxr::UsdPrim& prim,
                                                 std::set<double>&   times)
{
    if (!canConcatenateTransform(prim))
    {
        times.clear();
        return; // at top, stop
    }
    getConcatenatedXformOpTimeSamples(prim.GetParent(), times); // walk up

    if (prim.IsA<Pxr::UsdGeomXformable>())
    {
        // Get the Xform key range:
        std::vector<double> xform_times;
        Pxr::UsdGeomXformable xformable = Pxr::UsdGeomXformable(prim);
        if (!xformable.GetTimeSamples(&xform_times))
            return;
        for (size_t i=0; i < xform_times.size(); ++i)
            times.insert(xform_times[i]);
    }
}


/*!
*/
/*static*/ Fsr::Mat4d
FuserUsdXform::getLocalMatrixAtPrim(const Pxr::UsdPrim&     prim,
                                    const Pxr::UsdTimeCode& timecode)
{
    // Use the Xform cache system rather than direct xform access on the schema:
    Pxr::UsdGeomXformCache xform_cache(timecode);
    bool resets_xform_stack = false;
    return Fsr::Mat4d(xform_cache.GetLocalTransformation(prim, &resets_xform_stack).GetArray());
}


/*!
*/
/*static*/ Fsr::Mat4d
FuserUsdXform::getConcatenatedMatrixAtPrim(const Pxr::UsdPrim&     prim,
                                           const Pxr::UsdTimeCode& timecode)
{
    // Use the Xform cache system rather than direct xform access on the schema:
    Pxr::UsdGeomXformCache xform_cache(timecode);
    return Fsr::Mat4d(xform_cache.GetLocalToWorldTransform(prim).GetArray());
}



/*!
*/
/*static*/ void
FuserUsdXform::getConcatenatedMatricesAtPrim(const Pxr::UsdPrim&         prim,
                                             const std::vector<double>&  times,
                                             std::vector<Fsr::Mat4d>&    matrices)
{
    matrices.clear();
    const size_t nSamples = times.size();
    Fsr::Mat4d m;
    if (nSamples == 0)
    {
        // Uniform:
        matrices.resize(1);
        Pxr::UsdGeomXformCache xform_cache(Pxr::UsdTimeCode::Default());
        matrices[0] = Fsr::Mat4d(xform_cache.GetLocalToWorldTransform(prim).GetArray());
    }
    else
    {
        // Animated:
        matrices.resize(nSamples);
        for (size_t i=0; i < nSamples; ++i)
        {
            Pxr::UsdGeomXformCache xform_cache(Pxr::UsdTimeCode(times[i]));
            matrices[i] = Fsr::Mat4d(xform_cache.GetLocalToWorldTransform(prim).GetArray());
        }
    }
}


/*! This function retrieves a rotation(s) for a given xformOp and given time sample. It
    knows how to deal with different type of ops and angle conversion.
*/
/*static*/ bool
FuserUsdXform::getXformOpAsRotations(const Pxr::UsdGeomXformOp& xform_op,
                                     const Pxr::UsdTimeCode&    timecode,
                                     Pxr::GfVec3d&              rotations)
{
    const Pxr::UsdGeomXformOp::Type opType = xform_op.GetOpType();

    if (opType == Pxr::UsdGeomXformOp::TypeScale)
        rotations = Pxr::GfVec3d(1.0);
    else
        rotations = Pxr::GfVec3d(0.0);

    // Check whether the XformOp is a type of rotation:
    int rot_axis = -1;
    double angleMult = Pxr::GfDegreesToRadians(1.0);
    switch(opType)
    {
        case Pxr::UsdGeomXformOp::TypeRotateX:   rot_axis = 0; break;
        case Pxr::UsdGeomXformOp::TypeRotateY:   rot_axis = 1; break;
        case Pxr::UsdGeomXformOp::TypeRotateZ:   rot_axis = 2; break;
        case Pxr::UsdGeomXformOp::TypeRotateXYZ:
        case Pxr::UsdGeomXformOp::TypeRotateXZY:
        case Pxr::UsdGeomXformOp::TypeRotateYXZ:
        case Pxr::UsdGeomXformOp::TypeRotateYZX:
        case Pxr::UsdGeomXformOp::TypeRotateZXY:
        case Pxr::UsdGeomXformOp::TypeRotateZYX:
            break;
        default:
            // This XformOp is not a rotation, so we're not converting an
            // angular value from degrees to radians.
            angleMult = 1.0;
            break;
    }

    // If we encounter a transform op, we treat it as a shear operation.
    if (opType == Pxr::UsdGeomXformOp::TypeTransform)
    {
        // GetOpTransform() handles the inverse op case for us.
        Pxr::GfMatrix4d xform = xform_op.GetOpTransform(timecode);
        rotations[0] = xform[1][0]; //xyVal
        rotations[1] = xform[2][0]; //xzVal
        rotations[2] = xform[2][1]; //yzVal
    }
    else if (rot_axis != -1)
    {
        // Single Axis rotation
        double v = 0.0;
        if (!xform_op.GetAs<double>(&v, timecode))
            return false;

        if (xform_op.IsInverseOp())
            v = -v;
        rotations[rot_axis] = v * angleMult;
    }
    else
    {
        Pxr::GfVec3d v;
        if (!xform_op.GetAs<Pxr::GfVec3d>(&v, timecode))
            return false;

        if (xform_op.IsInverseOp())
            v = -v;
        rotations[0] = v[0] * angleMult;
        rotations[1] = v[1] * angleMult;
        rotations[2] = v[2] * angleMult;
    }

    return true;
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Import node attributes into a Nuke Op.
*/
/*virtual*/
void
FuserUsdXform::importSceneOp(DD::Image::Op*     op,
                             const Fsr::ArgSet& exec_args)
{
    if (!op)
        return; // shouldn't happen...

    Pxr::UsdPrim xform_prim = m_xformable_schema.GetPrim();
    if (!xform_prim.IsValid())
        return; // don't crash...

    const bool debug = exec_args.getBool(Arg::Scene::read_debug, false);
    //
    const Fsr::XformOrder    decompose_xform_order =    (Fsr::XformOrder)exec_args.getInt(Arg::Scene::decompose_xform_order, (int)Fsr::SRT_ORDER);
    const Fsr::RotationOrder decompose_rot_order   = (Fsr::RotationOrder)exec_args.getInt(Arg::Scene::decompose_rot_order,   (int)Fsr::ZXY_ORDER);
    //
    const bool T_enable            = exec_args.getBool(Arg::Scene::T_enable, true);
    const bool R_enable            = exec_args.getBool(Arg::Scene::R_enable, true);
    const bool S_enable            = exec_args.getBool(Arg::Scene::S_enable, true);
    const bool euler_filter_enable = exec_args.getBool(Arg::Scene::euler_filter_enable,   true);

    const Pxr::UsdPrim parent_prim = xform_prim.GetParent();
    const bool have_parent_xform = (parent_prim.IsA<Pxr::UsdGeomXformable>());
    const bool extract_parent_enable = (have_parent_xform && exec_args.getBool(Arg::Scene::parent_extract_enable, true));

    const bool   input_lock_read_frame = exec_args.getBool(  "reader:lock_read_frame", false);
    const double input_read_frame      = exec_args.getDouble("reader:read_frame",       0.0 );
    const double output_frame_offset   = exec_args.getDouble("reader:frame_offset",     0.0 );
    const double output_frame_origin   = exec_args.getDouble("reader:frame_origin",     0.0 );
    const double output_fps            = exec_args.getDouble("reader:fps",             24.0 );
    //
    const bool         input_lock_read_view = exec_args.getBool(  "reader:lock_read_view", false);
    const std::string& input_read_view      = exec_args.getString("reader:read_view",      ""   );

    const double input_fps          = getStage()->GetTimeCodesPerSecond();
    const double input_frame_origin = (output_frame_origin / output_fps) * input_fps;


    // This list gets filled in with the final transforms:
    AxisKnobValsList axis_vals_list;

    // Support parent translate/rotate knobs.
    // First check if target AxisOp has the parent knobs. If so sample the
    // GetLocalToWorldTransform(time) xform at all parent times decomposing
    // them into the parent xform knobs.
    //
    // Any local XformOps can then be put into the local knobs.
    //
    DD::Image::Knob* kParentTranslate = op->knob("parent_translate");
    DD::Image::Knob* kParentRotate    = op->knob("parent_rotate"   );
    DD::Image::Knob* kParentScale     = op->knob("parent_scale"    );
    bool separate_parent_enabled = (extract_parent_enable &&
                                    kParentTranslate && kParentRotate && kParentScale &&
                                    canConcatenateTransform(xform_prim.GetParent()));

    if (debug)
    {
        std::cout << "      FuserUsdXform::importSceneOp('" << op->node_name() << "') exec-args";
        std::cout << exec_args;
        std::cout << std::endl;

        std::cout << "        T_enable=" << T_enable;
        std::cout << ", R_enable=" << R_enable;
        std::cout << ", S_enable=" << S_enable;
        std::cout << ", euler_filter_enable=" << euler_filter_enable;
        std::cout << ", separate_parent_enabled=" << separate_parent_enabled;
        std::cout << std::endl;

        std::cout << "        lock frame=" << input_lock_read_frame;
        if (input_lock_read_frame)
            std::cout << ": read frame=" << input_read_frame;
        else
        {
            std::cout << ", frame_origin=" << output_frame_origin;
            std::cout << ", frames_per_second=" << output_fps;
        }
        std::cout << ", frame_offset=" << output_frame_offset;
        std::cout << ", lock view=" << input_lock_read_view;
        if (input_lock_read_view)
            std::cout << ": read view='" << input_read_view << "'";

        std::cout << ", input_fps=" << input_fps;
        std::cout << ", input_frame_origin=" << input_frame_origin;

        std::cout << std::endl;
    }


    // Get unique set of concatenated times from top down to current prim:
    std::set<double> concat_times;
    if (input_lock_read_frame)
        concat_times.insert(input_read_frame);
    else
        getConcatenatedXformOpTimeSamples(xform_prim, concat_times);

    // If no samples set to default time:
    bool is_animated;
    if (concat_times.size() == 0)
    {
        is_animated = false;
        axis_vals_list.resize(1);
        axis_vals_list[0].setToDefault(0.0);
        //if (debug)
        //    std::cout << "        times[0 - 0] - set to default" << std::endl;

    }
    else if (concat_times.size() == 1)
    {
        is_animated = true;
        axis_vals_list.resize(1);
        axis_vals_list[0].setToDefault(*concat_times.begin() + output_frame_offset);

    }
    else
    {
        is_animated = true;
        axis_vals_list.resize(concat_times.size());
        //if (debug)
        //    std::cout << "times:" << std::endl;
        int j = 0;
        for (std::set<double>::const_iterator it=concat_times.begin(); it != concat_times.end(); ++it, ++j)
        {
            const double input_frame = *it;

            // Apply time warping:
            const double input_time   = ((input_frame - output_frame_origin) / output_fps);
            const double output_frame = (input_time * input_fps) + output_frame_origin + output_frame_offset;

            axis_vals_list[j].setToDefault(output_frame);
            //if (debug)
            //    std::cout << "  " << j << ": in=" << input_frame << ", out=" << output_frame << std::endl;

        }

    }


    // Handle parent-separate and parent-combined modes differently so we
    // can handle local XformOps:
    if (separate_parent_enabled)
    {
        //--------------------------------
        // PARENT-SEPARATE MODE
        //--------------------------------

        bool all_default_vals = true;

        // Extract the concat time samples and decompose to SRT down to parent prim:
        int j = 0;
        for (std::set<double>::const_iterator it=concat_times.begin(); it != concat_times.end(); ++it, ++j)
        {
            const double input_frame = *it;

            const Pxr::UsdTimeCode timecode = (is_animated) ?
                                              Pxr::UsdTimeCode(input_frame) :
                                              Pxr::UsdTimeCode::Default();

            AxisKnobVals& axis_vals = axis_vals_list[j];
            if (!axis_vals.extractFromMatrix(getConcatenatedMatrixAtPrim(parent_prim, timecode),
                                             T_enable, R_enable, S_enable,
                                             Fsr::XYZ_ORDER,
                                             true/*apply_to_parent*/))
            {
                std::cerr << "Unable to successfully decompose parent transform at USD prim <" << m_xformable_schema.GetPath().GetText() << ">" << std::endl;
                //TF_RUNTIME_ERROR("Unable to successfully decompose parent transform at USD prim <%s>", m_xformable_schema.GetPath().GetText());
                break;
            }
            axis_vals.parent_enable = true;
            if (!axis_vals.isParentXformValsDefault())
                all_default_vals = false;

        }

        // Disable parent extraction if there's no non-default keys:
        if (all_default_vals)
        {
            for (size_t j=0; j < axis_vals_list.size(); ++j)
                axis_vals_list[j].parent_enable = false;
        }


        //---------------------------------------------------------------------
        // Figure out local XformOps - we can only do this if the
        // local xform is separated from the parent hierarchy.
        //
        // If we fail to read XformOps with the correct names and orders,
        // we'll fall back to grabbing the concatenated matrix and
        // decompose it.
        //
        //---------------------------------------------------------------------

        bool resets_xform_stack = false;
        std::vector<Pxr::UsdGeomXformOp> xformops = m_xformable_schema.GetOrderedXformOps(&resets_xform_stack);

        if (debug)
        {
            std::cout << "        xformOps[" << xformops.size() << "]: [";
            for (size_t i=0; i < xformops.size(); ++i)
            {
                const Pxr::UsdGeomXformOp& xform_op = xformops[i];
                std::cout << " " << i << ":'" << xform_op.GetName() << "'";
            }
            std::cout << " ]" << std::endl;
        }


        //---------------------------------------------------------------------
        // TODO: we're punting on supporting more general XformOps for now
        //       and just supporting decomposed matrices.
        //---------------------------------------------------------------------
#if 0
        // Get XformOps.
        // If there's only a single 'TypeTransform' (matrix4) Op then we can
        // decompose it, otherwise attempt to map each transform type to the
        // parts of a AxisKnobVals and build the transform & rotation orders.

        // When we find ops, we match the ops by suffix ("" will define the basic
        // translate, rotate, scale) and by order. If we find an op with a
        // different name or out of order that will miss the match, we will rely on
        // matrix decomposition

        UsdMayaXformStack::OpClassList stackOps = \
                UsdMayaXformStack::FirstMatchingSubstack(
                        {
                            &UsdMayaXformStack::MayaStack(),
                            &UsdMayaXformStack::CommonStack()
                        },
                        xformops);

        MFnDagNode MdagNode(mayaNode);

        if (0/*!stackOps.empty()*/)
        {
            // make sure stackIndices.size() == xformops.size()
            for (size_t i=0; i < stackOps.size(); ++i)
            {
                const Pxr::UsdGeomXformOp& xformop(xformops[i]);
                const UsdMayaXformOpClassification& opDef(stackOps[i]);

                // If we got a valid stack, we have both the members of the inverted twins..
                // ...so we can go ahead and skip the inverted twin
                if (opDef.IsInvertedTwin())
                    continue;

                const TfToken& opName(opDef.GetName());

                _pushUSDXformOpToMayaXform(xformop, opName, MdagNode, args, context);
            }
        }
        else
#endif
        {
            // Extract the local xform at each concat sample time and decompose to SRT:
            const size_t nSamples = axis_vals_list.size();
            //std::cout << "      nSamples=" << nSamples << ":" << std::endl;
            for (size_t j=0; j < nSamples; ++j)
            {
                AxisKnobVals& axis_vals = axis_vals_list[j];

                axis_vals.xform_order = decompose_xform_order;
                axis_vals.rot_order   = decompose_rot_order;

                const Pxr::UsdTimeCode timecode = (is_animated) ?
                                                  Pxr::UsdTimeCode(axis_vals.time) :
                                                  Pxr::UsdTimeCode::Default();

                if (!axis_vals_list[j].extractFromMatrix(getLocalMatrixAtPrim(xform_prim, timecode),
                                                         T_enable, R_enable, S_enable,
                                                         decompose_rot_order,
                                                         false/*apply_to_parent*/))
                {
                    std::cerr << "Unable to successfully decompose transform at USD prim <" << m_xformable_schema.GetPath().GetText() << ">" << std::endl;
                    //TF_RUNTIME_ERROR("Unable to successfully decompose transform at USD prim <%s>", m_xformable_schema.GetPath().GetText());
                    break;
                }
                //std::cout << "        " << axis_vals.time;
                //std::cout << " t" << axis_vals.translate << " r" << axis_vals.rotate << " s" << axis_vals.scaling;
                //std::cout << std::endl;
            }

        } // use Xform matrix

    }
    else
    {
        //--------------------------------
        // PARENT-CONCATENATED MODE
        //--------------------------------

        // Extract the samples and decompose to SRT:
        const size_t nSamples = axis_vals_list.size();
        for (size_t j=0; j < nSamples; ++j)
        {
            AxisKnobVals& axis_vals = axis_vals_list[j];

            axis_vals.xform_order = decompose_xform_order;
            axis_vals.rot_order   = decompose_rot_order;

            const Pxr::UsdTimeCode timecode = (is_animated) ?
                                              Pxr::UsdTimeCode(axis_vals.time) :
                                              Pxr::UsdTimeCode::Default();

            if (!axis_vals_list[j].extractFromMatrix(getConcatenatedMatrixAtPrim(xform_prim, timecode),
                                                     T_enable, R_enable, S_enable,
                                                     decompose_rot_order,
                                                     false/*apply_to_parent*/))
            {
                std::cerr << "Unable to successfully decompose transform at USD prim <" << m_xformable_schema.GetPath().GetText() << ">" << std::endl;
                //TF_RUNTIME_ERROR("Unable to successfully decompose transform at USD prim <%s>", m_xformable_schema.GetPath().GetText());
                break;
            }
            //std::cout << "      " << axis_vals.time;
            //std::cout << " t" << axis_vals.translate << " r" << axis_vals.rotate << " s" << axis_vals.scaling;
            //std::cout << std::endl;
        }
    }

    // Apply euler filter to final decomposed rotations:
    if (euler_filter_enable)
        AxisKnobVals::applyEulerFilter(decompose_rot_order, axis_vals_list);

    // Stores all the AxisKnob entries in the AxisOp transform knobs:
    AxisKnobVals::store(op, axis_vals_list);

    DD::Image::CameraOp* camera = dynamic_cast<DD::Image::CameraOp*>(op);
    if (camera)
    {
        // Anything special with a CameraOp's xform we need to handle?
        return;
    }

    DD::Image::LightOp* light = dynamic_cast<DD::Image::LightOp*>(op);
    if (light)
    {
        // Anything special with a LightOp's xform we need to handle?
        return;
    }

} // importSceneOp()


} // namespace Fsr


// end of FuserUsdXform.cpp

//
// Copyright 2019 DreamWorks Animation
//
