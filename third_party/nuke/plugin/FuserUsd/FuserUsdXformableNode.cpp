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

/// @file FuserUsdXformableNode.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdXformableNode.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/ExecuteTargetContexts.h>

#include <DDImage/GeoInfo.h>
#include <DDImage/GeometryList.h>


#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <pxr/usd/usd/modelAPI.h>

#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/camera.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*!
*/
FuserUsdXformableNode::FuserUsdXformableNode(const Pxr::UsdStageRefPtr& stage,
                                             const Fsr::ArgSet&         args,
                                             Fsr::Node*                 parent) :
    FuserUsdNode(stage),
    Fsr::XformableNode(args, parent)
{
    // Copy geo debug into primary node debug:
    const bool geo_debug = args.getBool(Arg::NukeGeo::read_debug, false);
    if (geo_debug)
        setBool(Arg::node_debug, true);

#if 0
    if (debug())
    {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

        std::cout << "      FuserUsdNode::ctor(" << this << "):";
        std::cout << " frame=" << getDouble("frame");
        std::cout << ", fps=" << getDouble("fps");
        std::cout << ", output_time=" << (getDouble("frame") / getDouble("fps"));
        std::cout << std::endl;
        std::cout << "        fsrUSDIO:node:class='" << getString("fsrUSDIO:node:class") << "'" << std::endl;
        std::cout << "        path='" << getString(Arg::Scene::path) << "'" << std::endl;
    }
#endif
}


/*!
*/
FuserUsdXformableNode::~FuserUsdXformableNode()
{
    //
}


/*! Called before evaluation starts to allow node to prep any data prior to rendering.
    Updates time value and possibly local transform.
*/
/*virtual*/ void
FuserUsdXformableNode::_validateState(const Fsr::NodeContext& exec_ctx,
                                      bool                    for_real)
{
    Fsr::XformableNode::_validateState(exec_ctx, for_real);

    //---------------------------------------
    // Get time warping values:

    double input_fps  = ::fabs(getStage()->GetTimeCodesPerSecond());
    double output_fps = ::fabs(exec_ctx.getDouble("reader:fps", 24.0));

    // Keep degenerate fps from breaking calcs:
    if (input_fps < 0.001)
        input_fps = 0.001;
    if (output_fps < 0.001)
        output_fps = 0.001;

    const double output_frame = exec_ctx.getDouble("frame", 0.0);
    const double frame_origin = exec_ctx.getDouble("reader:frame_origin", 0.0);

    m_output_time = ((output_frame - frame_origin) / input_fps);
    m_input_time  = (m_output_time * output_fps) + frame_origin;

    //---------------------------------------

    const bool get_xform = exec_ctx.getBool("reader:apply_xforms", true);
    if (get_xform)
    {
        // TODO: implement! m_xform = getTransformAtTime(AbcSearch::getParentXform(object()), m_time);
        m_have_xform = !m_xform.isIdentity();
    }
    else
    {
        m_xform.setToIdentity();
        m_have_xform = false;
    }

    // Clear the bbox:
    m_local_bbox.setToEmptyState();

    if (0)//(debug())
    {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

        std::cout << "============================================================================================" << std::endl;
        std::cout << "FuserUsdXformableNode::_validateState(" << this << "): for_real=" << for_real;
        std::cout << ", m_output_time=" << m_output_time;
        std::cout << ", m_input_time=" << m_input_time;
        std::cout << ", m_local_bbox=" << m_local_bbox;
        std::cout << ", m_have_xform=" << m_have_xform;
        if (m_have_xform)
            std::cout << ", xform" << m_xform;
        if (debugAttribs())
            std::cout << ", args[" << m_args << "]";
        std::cout << std::endl;
    }
}


/*! Prints an unrecognized-target warning in debug mode and returns 0 (success).
*/
/*virtual*/ int
FuserUsdXformableNode::_execute(const Fsr::NodeContext& target_context,
                                const char*             target_name,
                                void*                   target,
                                void*                   src0,
                                void*                   src1)
{
    // Don't throw an error on an unrecognized target:
    if (debug())
    {
        std::cerr << fuserNodeClass() << ": warning, cannot handle target type '" << target_name << "'";
        std::cerr << ", ignoring." << std::endl;
    }
    return 0; // no user-abort
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*!

    UsdTyped/
        UsdGeomImageable/
            * UsdGeomScope *
            UsdGeomXformable/
                * UsdGeomCamera *
                * UsdGeomXform *
                UsdGeomBoundable/
                    UsdGeomGprim/
                        * UsdGeomCapsule *
                        * UsdGeomCone *
                        * UsdGeomCube *
                        * UsdGeomCylinder *
                        * UsdGeomPointBased *
                        * UsdGeomSphere *
                    * UsdGeomPointInstancer *

*/
static void findXformNodes(const Pxr::UsdPrim&                       prim,
                           std::vector<Pxr::UsdPrimSiblingIterator>& xformables)
{
    std::cout << "  findXformNodes() prim='" << prim.GetPath() << "'" << std::endl;

#if 1
    auto prim_flags = (Pxr::UsdPrimIsActive && Pxr::UsdPrimIsDefined && !Pxr::UsdPrimIsAbstract);
#else
    auto prim_flags = (Pxr::UsdPrimIsModel && Pxr::UsdPrimIsActive && Pxr::UsdPrimIsDefined && !Pxr::UsdPrimIsAbstract);
#endif
    const Pxr::UsdPrim::SiblingRange children = prim.GetFilteredChildren(Pxr::UsdTraverseInstanceProxies(prim_flags));
    //const Pxr::UsdPrim::SiblingRange children = prim.GetAllChildren();

    for (auto child=TfMakeIterator(children); child; ++child)
    {
        std::cout << "    node'" << child->GetPath() << "'[" << child->GetTypeName() << "]";
#if PXR_MAJOR_VERSION == 0 && PXR_MINOR_VERSION < 20
        Pxr::SdfPrimSpecHandle spec = child->GetPrimDefinition();
        if (spec)
            std::cout << ", Kind='" << spec->GetKind() << "'";
#else
        // Have to get the 'Kind' token through the UsdModelAPI interface:
        {
            const Pxr::UsdModelAPI modelAPI(*child);
            Pxr::TfToken kind;
            if (modelAPI.GetKind(&kind))
                std::cout << ", Kind='" << kind << "'";
        }
#endif
        std::cout << ", IsAbstract=" << child->IsAbstract();
        //std::cout << ", isModel=" << child->IsModel();
        //std::cout << ", isCamera=" << child->IsA<Pxr::UsdGeomCamera>();
        //std::cout << ", isScope=" << child->IsA<Pxr::UsdGeomScope>();
        std::cout << ", isXform=" << child->IsA<Pxr::UsdGeomXformable>();
        std::cout << std::endl;

        if (child->IsA<Pxr::UsdGeomXformable>())
            xformables.push_back(child);

        findXformNodes(*child, xformables);
    }
}


} // namespace Fsr


// end of FuserUsdXformableNode.cpp

//
// Copyright 2019 DreamWorks Animation
//
