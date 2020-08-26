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

/// @file FuserUsdLight.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdLight.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/ExecuteTargetContexts.h>
#include <Fuser/NukeKnobInterface.h>
#include <Fuser/NukeGeoInterface.h>

#include <DDImage/Iop.h>
#include <DDImage/LightOp.h>
#include <DDImage/gl.h>


namespace Fsr {


//--------------------------------------------------------------------------


/*!
*/
FuserUsdLight::FuserUsdLight(const Pxr::UsdStageRefPtr& stage,
                             const Pxr::UsdPrim&        light_prim,
                             const Fsr::ArgSet&         args,
                             Fsr::Node*                 parent) :
    FuserUsdXform(stage, light_prim, args, parent)
{
    //std::cout << "  FuserUsdLight::ctor(" << this << ") cam'" << light_prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdGeomLight:
    if (light_prim.IsValid() && light_prim.IsA<Pxr::UsdLuxLight>())
    {
        m_light_schema = Pxr::UsdLuxLight(light_prim);

        // Lights can be affected by visibility:
        getVisibility(light_prim, m_is_visible, m_has_animated_visibility);

        if (debug())
        {
            printPrimAttributes("  Light", light_prim, false/*verbose*/, std::cout);
            std::cout << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "  FuserUsdLight::ctor(" << this << "): ";
            std::cerr << "warning, node '" << light_prim.GetPath() << "'(" << light_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdLight::_validateState(const Fsr::NodeContext& exec_ctx,
                              bool                    for_real)
{
    // Get the time value up to date:
    FuserUsdXform::_validateState(exec_ctx, for_real);

    //if (!m_is_visible)
    //    return; // skip rest if light is not visible
}


//-------------------------------------------------------------------------------


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdLight::_execute(const Fsr::NodeContext& target_context,
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
        std::cout << "  FuserUsdLight::_execute(" << this << ") target='" << target_name << "'";
        std::cout << " Light";
        if (!m_is_visible)
            std::cout << "(INVISIBLE)";
        std::cout << " '" << getString(Arg::Scene::path) << "'";
        if (m_have_xform)
            std::cout << ", xform" << m_xform;
        else
            std::cout << ", xform disabled";
        std::cout << std::endl;
    }

    if (!m_is_visible)
    {
        // Skip light execute methods if not visible:
        return FuserUsdXform::_execute(target_context, target_name, target, src0, src1);
    }

    // Redirect execution depending on target type:
    if (strncmp(target_name, "DRAW_GL"/*Fsr::PrimitiveViewerContext::name*/, 7)==0)
    {
        // TODO: implement!
#if 0
        // Draw a Light shape. This should be put in a Fuser::Light base class.
        Fsr::PrimitiveViewerContext* pv_ctx =
            reinterpret_cast<Fsr::PrimitiveViewerContext*>(target);

        // Any null pointers throw a coding error:
        if (!pv_ctx || !pv_ctx->vtx || !pv_ctx->ptx)
            return error("null objects in target '%s'. This is likely a coding error", target_name);

        drawLight(pv_ctx->vtx, pv_ctx->ptx);
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
        // TODO: implement!
#if 0
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
    return FuserUsdXform::_execute(target_context, target_name, target, src0, src1);
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Import node attributes into a Nuke Op.
*/
/*virtual*/
void
FuserUsdLight::importSceneOp(DD::Image::Op*     op,
                             const Fsr::ArgSet& exec_args)
{
    // Allow camera nodes to import their xforms into any AxisOp subclass:
    if (!dynamic_cast<DD::Image::AxisOp*>(op))
        return; // shouldn't happen...

    const bool debug = exec_args.getBool(Arg::Scene::read_debug, false);
    if (debug)
        std::cout << "    FuserUsdLight::importSceneOp('" << op->node_name() << "')" << std::endl;

const bool allow_anim = true;

    // Import the Xform data into the Axis_Knob:
    FuserUsdXform::importSceneOp(op, exec_args);

    DD::Image::LightOp* light = dynamic_cast<DD::Image::LightOp*>(op);
    if (!light)
        return; // skip any light-specific data if not a LightOp

    // Skip loading light controls if its disabled:
    // TODO: this kind of logic needs to go in a base 'LightLoader' class...
    const bool sync_light_controls = getBoolValue(light->knob("sync_light_controls"), true);
    if (!sync_light_controls)
        return; // don't need to load controls


    Pxr::UsdPrim light_prim = m_light_schema.GetPrim();
    //std::cout << "    FuserUsdLight::importSceneOp('" << light_prim.GetName() << "')" << std::endl;

    //std::cout << "    parent is '" << light_prim.GetPath().GetParentPath() << "'" << std::endl;
    //std::cout << "  node'" << child->GetPath() << "'[" << child->GetTypeName() << "]";


    // Copy the light-specific attribs:
    /*
        UsdLuxLight:
            intensity                 float
            exposure                  float
            color                     GfVec3f
            enableColorTemperature    bool
            colorTemperature          float
            normalize                 bool
            //
            radius                    float
            //
            xformOp:transform         GfMat4d
            xformOpOrder              VtArray<TfToken>

        DD::Image::LightOp:
         >> intensity           1
         >> color               [1 1 1]
         >> near                0.1
         >> far                 10000
    */
    // Creating a KnobChangeGroup causes Nuke to batch up knobChanged messages,
    // sending only one upon destruction:
    { DD::Image::KnobChangeGroup change_group;

        const std::vector<Pxr::UsdAttribute>& attribs = light_prim.GetAttributes();
        for (size_t j=0; j < attribs.size(); ++j)
        {
            const Pxr::UsdAttribute& attrib = attribs[j];

            const Pxr::TfToken& name = attrib.GetName();
            //const Pxr::TfType type = attrib.GetTypeName().GetType();

            if      (name == Pxr::UsdLuxTokens->color)
            {
                copyAttribToKnob(attrib, allow_anim, light->knob("color"), -1/*view*/);
            }
            else if (name == Pxr::UsdLuxTokens->intensity)
            {
                copyAttribToKnob(attrib, allow_anim, light->knob("intensity"), -1/*view*/);
            }
            else if (name == "enableColorTemperature")
            {
                copyAttribToKnob(attrib, allow_anim, light->knob("enable_color_temperature"), -1/*view*/);
            }
            else if (name == "colorTemperature")
            {
                copyAttribToKnob(attrib, allow_anim, light->knob("color_temperature"), -1/*view*/);
            }
            else if (name == Pxr::UsdGeomTokens->visibility)
            {
                ;// currently unsupported
            }
            else if (name == Pxr::UsdGeomTokens->purpose)
            {
                ;// currently unsupported
            }
        }

    } // DD::Image::KnobChangeGroup

}


} // namespace Fsr


// end of FuserUsdLight.cpp

//
// Copyright 2019 DreamWorks Animation
//
