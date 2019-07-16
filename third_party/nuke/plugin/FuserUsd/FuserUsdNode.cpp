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

/// @file FuserUsdNode.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdNode.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/ExecuteTargetContexts.h>

#include <DDImage/GeoInfo.h>
#include <DDImage/GeometryList.h>


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/usd/usdGeom/scope.h>
#  include <pxr/usd/usdGeom/xform.h>
#  include <pxr/usd/usdGeom/camera.h>
#  include <pxr/usd/usdGeom/mesh.h>

#  include <pxr/usd/usdShade/shader.h>
#  include <pxr/usd/usdShade/material.h>

#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*!
*/
FuserUsdNode::FuserUsdNode(const Pxr::UsdStageRefPtr& stage,
                           const Fsr::ArgSet&         args,
                           Fsr::Node*                 parent) :
    Fsr::XformableNode(args, parent),
    m_stage(stage),
    m_time(0.0)
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
        std::cout << " frame=" << getDouble("frame") << ", output_frame=" << getDouble("output_frame");
        std::cout << ", fps=" << getDouble("fps");
        std::cout << std::endl;
        std::cout << "        fsrUSDIO:node:class='" << getString("fsrUSDIO:node:class") << "'" << std::endl;
        std::cout << "        path='" << getString(Arg::Scene::path) << "'" << std::endl;
    }
#endif
}


/*!
*/
FuserUsdNode::~FuserUsdNode()
{
    // Don't release the archive here! We want the archive pointer to
    // stick around for multiple uses of the same FuserUsdNode path.

    //abc_archive_map.releaseCache(getString(Arg::Scene::file));
}


/*! Called before evaluation starts to allow node to prep any data prior to rendering.
    Updates time value and possibly local transform.
*/
/*virtual*/ void
FuserUsdNode::_validateState(const Fsr::NodeContext& args,
                             bool                    for_real)
{
    Fsr::XformableNode::_validateState(args, for_real);

    m_time = getDouble("frame") / getDouble("fps");

    const bool get_xform = getBool("reader:apply_matrix", true);
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
}


/*! Prints an unrecognized-target warning in debug mode and returns 0 (success).
*/
/*virtual*/ int
FuserUsdNode::_execute(const Fsr::NodeContext& target_context,
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
    for (auto child=TfMakeIterator(prim.GetAllChildren()); child; ++child)
#else
    for (auto child=TfMakeIterator(prim.GetFilteredChildren(Pxr::UsdPrimIsModel)); child; ++child)
#endif
    {
        std::cout << "    node'" << child->GetPath() << "'[" << child->GetTypeName() << "]";
        Pxr::SdfPrimSpecHandle spec = child->GetPrimDefinition();
        if (spec)
            std::cout << ", Kind='" << spec->GetKind() << "'";
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


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


static std::unordered_map<std::string, PrimvarRef> primvar_refs;

/*! Builds a static map of primvar types to the DD::Image::Atrribute equivalents.
    This allows a fast map lookup to be used rather than a series if/then
    comparisions.
*/
struct PrimvarRefMapBuilder
{
    PrimvarRefMapBuilder()
    {
        /* PrimvarRef(bool is_array_type,
                      bool is_int_type,
                      bool is_float_type,
                      int  _bytes_per_element,
                      int  _num_elements,
                      DD::Image::AttribType _nk_attrib)
        */
        //                                        array   int   float byt nElem   nk_attrib
        primvar_refs["string"      ] = PrimvarRef(false, false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        primvar_refs["string[]"    ] = PrimvarRef(true,  false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        primvar_refs["token"       ] = PrimvarRef(false, false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        primvar_refs["token[]"     ] = PrimvarRef(true,  false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        primvar_refs["asset"       ] = PrimvarRef(false, false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        primvar_refs["asset[]"     ] = PrimvarRef(true,  false, false, 0,  1, DD::Image::STD_STRING_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["bool"        ] = PrimvarRef(false, true,  false, 1,  1, DD::Image::INT_ATTRIB);
        primvar_refs["bool[]"      ] = PrimvarRef(true,  true,  false, 1,  1, DD::Image::INT_ATTRIB);
        primvar_refs["int"         ] = PrimvarRef(false, true,  false, 4,  1, DD::Image::INT_ATTRIB);
        primvar_refs["int[]"       ] = PrimvarRef(true,  true,  false, 4,  1, DD::Image::INT_ATTRIB);
        primvar_refs["int64"       ] = PrimvarRef(false, true,  false, 8,  1, DD::Image::INT_ATTRIB);
        primvar_refs["int64[]"     ] = PrimvarRef(true,  true,  false, 8,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uchar"       ] = PrimvarRef(false, true,  false, 1,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uchar[]"     ] = PrimvarRef(true,  true,  false, 1,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uint"        ] = PrimvarRef(false, true,  false, 4,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uint[]"      ] = PrimvarRef(true,  true,  false, 4,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uint64"      ] = PrimvarRef(false, true,  false, 8,  1, DD::Image::INT_ATTRIB);
        primvar_refs["uint64[]"    ] = PrimvarRef(true,  true,  false, 8,  1, DD::Image::INT_ATTRIB);
        primvar_refs["half"        ] = PrimvarRef(false, false, true,  2,  1, DD::Image::FLOAT_ATTRIB);
        primvar_refs["half[]"      ] = PrimvarRef(true,  false, true,  2,  1, DD::Image::FLOAT_ATTRIB);
        primvar_refs["float"       ] = PrimvarRef(false, false, true,  4,  1, DD::Image::FLOAT_ATTRIB);
        primvar_refs["float[]"     ] = PrimvarRef(true,  false, true,  4,  1, DD::Image::FLOAT_ATTRIB);
        primvar_refs["double"      ] = PrimvarRef(false, false, true,  8,  1, DD::Image::FLOAT_ATTRIB);
        primvar_refs["double[]"    ] = PrimvarRef(true,  false, true,  8,  1, DD::Image::FLOAT_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["int2"        ] = PrimvarRef(false, true,  false, 4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["int2[]"      ] = PrimvarRef(true,  true,  false, 4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["half2"       ] = PrimvarRef(false, false, true,  2,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["half2[]"     ] = PrimvarRef(true,  false, true,  2,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["float2"      ] = PrimvarRef(false, false, true,  4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["float2[]"    ] = PrimvarRef(true,  false, true,  4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["double2"     ] = PrimvarRef(false, false, true,  8,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["double2[]"   ] = PrimvarRef(true,  false, true,  8,  2, DD::Image::VECTOR2_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["int3"        ] = PrimvarRef(false, true,  true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["int3[]"      ] = PrimvarRef(true,  true,  true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["half3"       ] = PrimvarRef(false, false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["half3[]"     ] = PrimvarRef(true,  false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["float3"      ] = PrimvarRef(false, false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["float3[]"    ] = PrimvarRef(true,  false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["double3"     ] = PrimvarRef(false, false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["double3[]"   ] = PrimvarRef(true,  false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3h"     ] = PrimvarRef(false, false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3h[]"   ] = PrimvarRef(true,  false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3f"     ] = PrimvarRef(false, false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3f[]"   ] = PrimvarRef(true,  false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3d"     ] = PrimvarRef(false, false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["color3d[]"   ] = PrimvarRef(true,  false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["normal3h"    ] = PrimvarRef(false, false, true,  2,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["normal3h[]"  ] = PrimvarRef(true,  false, true,  2,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["normal3d"    ] = PrimvarRef(false, false, true,  4,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["normal3d[]"  ] = PrimvarRef(true,  false, true,  4,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["normal3f"    ] = PrimvarRef(false, false, true,  8,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["normal3f[]"  ] = PrimvarRef(true,  false, true,  8,  3, DD::Image::NORMAL_ATTRIB);
        primvar_refs["point3h"     ] = PrimvarRef(false, false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["point3h[]"   ] = PrimvarRef(true,  false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["point3d"     ] = PrimvarRef(false, false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["point3d[]"   ] = PrimvarRef(true,  false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["point3f"     ] = PrimvarRef(false, false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["point3f[]"   ] = PrimvarRef(true,  false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3h"    ] = PrimvarRef(false, false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3h[]"  ] = PrimvarRef(true,  false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3f"    ] = PrimvarRef(false, false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3f[]"  ] = PrimvarRef(true,  false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3d"    ] = PrimvarRef(false, false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["vector3d[]"  ] = PrimvarRef(true,  false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["int4"        ] = PrimvarRef(false, true,  true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["int4[]"      ] = PrimvarRef(true,  true,  true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["half4"       ] = PrimvarRef(false, false, true,  2,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["half4[]"     ] = PrimvarRef(true,  false, true,  2,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["float4"      ] = PrimvarRef(false, false, true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["float4[]"    ] = PrimvarRef(true,  false, true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["double4"     ] = PrimvarRef(false, false, true,  8,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["double4[]"   ] = PrimvarRef(true,  false, true,  8,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4h"     ] = PrimvarRef(false, false, true,  2,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4h[]"   ] = PrimvarRef(true,  false, true,  2,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4f"     ] = PrimvarRef(false, false, true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4f[]"   ] = PrimvarRef(true,  false, true,  4,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4d"     ] = PrimvarRef(false, false, true,  8,  4, DD::Image::VECTOR4_ATTRIB);
        primvar_refs["color4d[]"   ] = PrimvarRef(true,  false, true,  8,  4, DD::Image::VECTOR4_ATTRIB);
        //--------------------------------------------------------------------------
        primvar_refs["texCoord2h"  ] = PrimvarRef(false, false, true,  2,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord2h[]"] = PrimvarRef(true,  false, true,  2,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord2f"  ] = PrimvarRef(false, false, true,  4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord2f[]"] = PrimvarRef(true,  false, true,  4,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord2d"  ] = PrimvarRef(false, false, true,  8,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord2d[]"] = PrimvarRef(true,  false, true,  8,  2, DD::Image::VECTOR2_ATTRIB);
        primvar_refs["texCoord3h"  ] = PrimvarRef(false, false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["texCoord3h[]"] = PrimvarRef(true,  false, true,  2,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["texCoord3f"  ] = PrimvarRef(false, false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["texCoord3f[]"] = PrimvarRef(true,  false, true,  4,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["texCoord3d"  ] = PrimvarRef(false, false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        primvar_refs["texCoord3d[]"] = PrimvarRef(true,  false, true,  8,  3, DD::Image::VECTOR3_ATTRIB);
        //--------------------------------------------------------------------------
        //primvar_refs["matrix2d"    ] = PrimvarRef(false, false, true,  8,      4, DD::Image::MATRIX2_ATTRIB);
        //primvar_refs["matrix2d[]"  ] = PrimvarRef(true,  false, true,  8,      4, DD::Image::MATRIX2_ATTRIB);
        primvar_refs["matrix3d"    ] = PrimvarRef(false, false, true,  8,  9, DD::Image::MATRIX3_ATTRIB);
        primvar_refs["matrix3d[]"  ] = PrimvarRef(true,  false, true,  8,  9, DD::Image::MATRIX3_ATTRIB);
        primvar_refs["matrix4d"    ] = PrimvarRef(false, false, true,  8, 16, DD::Image::MATRIX4_ATTRIB);
        primvar_refs["matrix4d[]"  ] = PrimvarRef(true,  false, true,  8, 16, DD::Image::MATRIX4_ATTRIB);
        primvar_refs["frame4d"     ] = PrimvarRef(false, false, true,  8, 16, DD::Image::MATRIX4_ATTRIB);
        primvar_refs["frame4d[]"   ] = PrimvarRef(true,  false, true,  8, 16, DD::Image::MATRIX4_ATTRIB);
        //--------------------------------------------------------------------------
        //primvar_refs["quath"       ] = PrimvarRef(false, false, true,  2,  1, DD::Image::INT_ATTRIB);
        //primvar_refs["quath[]"     ] = PrimvarRef(true,  false, true,  2,  1, DD::Image::INT_ATTRIB);
        //primvar_refs["quatf"       ] = PrimvarRef(false, false, true,  4,  1, DD::Image::INT_ATTRIB);
        //primvar_refs["quatf[]"     ] = PrimvarRef(true,  false, true,  4,  1, DD::Image::INT_ATTRIB);
        //primvar_refs["quatd"       ] = PrimvarRef(false, false, true,  8,  1, DD::Image::INT_ATTRIB);
        //primvar_refs["quatd[]"     ] = PrimvarRef(true,  false, true,  8,  1, DD::Image::INT_ATTRIB);
        //--------------------------------------------------------------------------

        //for (std::unordered_map<std::string, PrimvarRef>::const_iterator it=primvar_refs.begin(); it != primvar_refs.end(); ++it)
        //    std::cout << "*********************************** '" << it->first << "'" << std::endl;
    }

};
static PrimvarRefMapBuilder primvar_ref_builder;


/* Retrieve a reference object to a primvar type.
*/
/*static*/ const PrimvarRef*
PrimvarRef::get(const Pxr::UsdGeomPrimvar& primvar)
{
#if 1
    std::stringstream type; type << primvar.GetTypeName();

    const std::unordered_map<std::string, PrimvarRef>::const_iterator it =
        primvar_refs.find(type.str());
#else
    // Is this faster than using the stringstream?
    const std::unordered_map<std::string, PrimvarRef>::const_iterator it =
        primvar_refs.find(primvar.GetTypeName().GetType().GetString());
#endif
    return (it != primvar_refs.end()) ? &it->second : NULL;
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*!
*/
/*static*/ void
FuserUsdNode::printPrimAttributes(const char*         prefix,
                                  const Pxr::UsdPrim& prim,
                                  bool                verbose,
                                  std::ostream&       o)
{
    o << prefix << "[ ";
    const std::vector<Pxr::UsdAttribute>& attribs = prim.GetAttributes();
    for (size_t i=0; i < attribs.size(); ++i)
    {
        const Pxr::UsdAttribute& attrib = attribs[i];
        if (i > 0)
            o << ", ";
        o << attrib.GetName();
        if (verbose)
        {
            o << "(";
            if (attrib.IsCustom())
                o << "custom ";
            o << attrib.GetTypeName().GetType() << ")";
            if (attrib.GetNumTimeSamples() > 0)
                o << "[" << attrib.GetNumTimeSamples() << "]";
        }
    }
    o << " ]";
}


/*!
*/
/*static*/ bool
FuserUsdNode::isPrimAttribVarying(const Pxr::UsdAttribute& attr,
                                  double                   time) 
{
    // XXX: Copied from UsdImagingDelegate::_TrackVariability.
    // XXX: This logic is highly sensitive to the underlying quantization of
    //      time. Also, the epsilon value (.000001) may become zero for large
    //      time values.
    double queryTime = time + std::numeric_limits<double>::epsilon();

    // TODO: migrate this logic into UsdAttribute.
    double lower, upper;
    bool hasSamples = false;
    if (!attr.GetBracketingTimeSamples(queryTime, &lower, &upper, &hasSamples))
    {
        // error - TODO: what kind of errors do we need to handle here...?
        return false;
    }

    // The potential results are:
    //    * Requested time was between two time samples
    //    * Requested time was out of the range of time samples (lesser)
    //    * Requested time was out of the range of time samples (greater)
    //    * There was a time sample exactly at the requested time or
    //      there was exactly one time sample.

    // Between samples?
    if (fabs(upper - lower) > 0.0)
        return true;

    // Out of range (lower) or exactly on a time sample?
    attr.GetBracketingTimeSamples((lower + std::numeric_limits<double>::epsilon()), &lower, &upper, &hasSamples);
    if (fabs(upper - lower) > 0.0)
        return true;

    // Out of range (greater)?
    attr.GetBracketingTimeSamples((lower - std::numeric_limits<double>::epsilon()), &lower, &upper, &hasSamples);
    if (fabs(upper - lower) > 0.0)
        return true;

    // Really only one time sample --> not varying for our purposes
 
    return hasSamples;
}


/*! Returns false if times[] contains a single UsdTimeCode::Default() entry,
    ie. is not animated.
*/
/*static*/ bool
FuserUsdNode::getPrimAttribTimeSamples(const Pxr::UsdAttribute& attrib,
                                       std::vector<double>&     times)
{
    if (attrib.GetTimeSamples(&times) && times.size() > 0)
        return true; // animated

    // Not animated, store special 'not-animated' time(nan) at index 0:
    Fsr::setNotAnimated(times);
    return false; // not animated
}


/*! If not animated UsdTimeCode::Default() is added to set.
*/
/*static*/ void
FuserUsdNode::concatenatePrimAttribTimeSamples(const Pxr::UsdAttribute&  attrib,
                                               std::set<Fsr::TimeValue>& concat_times)
{
    std::vector<double> times;
    if (attrib.GetTimeSamples(&times) && times.size() > 0)
    {
        concat_times.erase(Fsr::defaultTimeValue());
        const size_t nSamples = times.size();
        for (size_t i=0; i < nSamples; ++i)
            concat_times.insert(times[i]);
    }
    else
    {
        concat_times.insert(Fsr::defaultTimeValue());
    }
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*!
*/
/*static*/ double
FuserUsdNode::getPrimAttribDouble(const Pxr::UsdPrim&     prim,
                                  const char*             attrib_name,
                                  const Pxr::UsdTimeCode& time,
                                  unsigned                element_index)
{
    if (Pxr::UsdAttribute attrib = prim.GetAttribute(Pxr::TfToken(attrib_name)))
        return getPrimAttribDouble(attrib, time, element_index);
    return 0.0; // attrib not found
}


/*!
*/
/*static*/ double
FuserUsdNode::getPrimAttribDouble(const Pxr::UsdAttribute& attrib,
                                  const Pxr::UsdTimeCode&  time,
                                  unsigned                 element_index)
{
    const Pxr::TfType type = attrib.GetTypeName().GetType();

    // TODO: use PrimvarRef or an equiv to speed this if/then trees up:

    if (type.IsA<double>())
    {
        double v; attrib.Get<double>(&v, time);
        return v;
    }
    else if (type.IsA<float>())
    {
        float v; attrib.Get<float>(&v, time);
        return double(v);
    }
    else if (type.IsA<int>())
    {
        int v; attrib.Get<int>(&v, time);
        return double(v);
    }
    else if (type.IsA<bool>())
    {
        bool v; attrib.Get<bool>(&v, time);
        return (v)?1.0:0.0;
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec2i>())
    {
        Pxr::GfVec2i v; attrib.Get<Pxr::GfVec2i>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec2f>())
    {
        Pxr::GfVec2f v; attrib.Get<Pxr::GfVec2f>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec2d>())
    {
        Pxr::GfVec2d v; attrib.Get<Pxr::GfVec2d>(&v, time);
        return v[element_index];
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec3i>())
    {
        Pxr::GfVec3i v; attrib.Get<Pxr::GfVec3i>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec3f>())
    {
        Pxr::GfVec3f v; attrib.Get<Pxr::GfVec3f>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec3d>())
    {
        Pxr::GfVec3d v; attrib.Get<Pxr::GfVec3d>(&v, time);
        return v[element_index];
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec4i>())
    {
        Pxr::GfVec4i v; attrib.Get<Pxr::GfVec4i>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec4f>())
    {
        Pxr::GfVec4f v; attrib.Get<Pxr::GfVec4f>(&v, time);
        return double(v[element_index]);
    }
    else if (type.IsA<Pxr::GfVec4d>())
    {
        Pxr::GfVec4d v; attrib.Get<Pxr::GfVec4d>(&v, time);
        return v[element_index];
    }
    return 0.0; // unsupported type
}


/*! Extract doubles from a UsdAttrib.
    Returns false if attrib does not support doubles.
    If allow_animation is false only copy the first value.
*/
bool
AttribDoubles::getFromAttrib(const Pxr::UsdAttribute& attrib,
                             bool                     allow_animation)
{
    if (!attrib.IsValid())
        return false;

    if (allow_animation)
        FuserUsdNode::getPrimAttribTimeSamples(attrib, times);
    else
        Fsr::setNotAnimated(times); // force no animation (stores 'not-animated' time at index 0)

    const size_t nSamples = times.size();
    const Pxr::TfType type = attrib.GetTypeName().GetType();

    // TODO: use PrimvarRef or an equiv to speed this if/then trees up:

    if (type.IsA<double>())
    {
        doubles_per_value = 1;
        values.resize(nSamples);
        for (size_t i=0; i < nSamples; ++i)
            attrib.Get<double>(&values[i], times[i]);
        return true;
    }
    else if (type.IsA<float>())
    {
        doubles_per_value = 1;
        values.resize(nSamples);
        for (size_t i=0; i < nSamples; ++i)
            { float v; attrib.Get<float>(&v, times[i]); values[i] = double(v); }
        return true;
    }
    else if (type.IsA<int>())
    {
        doubles_per_value = 1;
        values.resize(nSamples);
        for (size_t i=0; i < nSamples; ++i)
            { int v; attrib.Get<int>(&v, times[i]); values[i] = double(v); }
        return true;
    }
    else if (type.IsA<bool>())
    {
        doubles_per_value = 1;
        values.resize(nSamples);
        for (size_t i=0; i < nSamples; ++i)
            { bool v; attrib.Get<bool>(&v, times[i]); values[i] = (v)?1.0:0.0; }
        return true;
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec2i>())
    {
        doubles_per_value = 2;
        values.resize(nSamples*2);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec2i v; attrib.Get<Pxr::GfVec2i>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec2f>())
    {
        doubles_per_value = 2;
        values.resize(nSamples*2);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec2f v; attrib.Get<Pxr::GfVec2f>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec2d>())
    {
        doubles_per_value = 2;
        values.resize(nSamples*2);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec2d v; attrib.Get<Pxr::GfVec2d>(&v, times[i]);
            values[vi++] = v[0]; values[vi++] = v[1];
        }
        return true;
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec3i>())
    {
        doubles_per_value = 3;
        values.resize(nSamples*3);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec3i v; attrib.Get<Pxr::GfVec3i>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
            values[vi++] = double(v[2]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec3f>())
    {
        doubles_per_value = 3;
        values.resize(nSamples*3);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec3f v; attrib.Get<Pxr::GfVec3f>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
            values[vi++] = double(v[2]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec3d>())
    {
        doubles_per_value = 3;
        values.resize(nSamples*3);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec3d v; attrib.Get<Pxr::GfVec3d>(&v, times[i]);
            values[vi++] = v[0]; values[vi++] = v[1];
            values[vi++] = v[2];
        }
        return true;
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfVec4i>())
    {
        doubles_per_value = 4;
        values.resize(nSamples*4);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec4i v; attrib.Get<Pxr::GfVec4i>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
            values[vi++] = double(v[2]); values[vi++] = double(v[3]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec4f>())
    {
        doubles_per_value = 4;
        values.resize(nSamples*4);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec4f v; attrib.Get<Pxr::GfVec4f>(&v, times[i]);
            values[vi++] = double(v[0]); values[vi++] = double(v[1]);
            values[vi++] = double(v[2]); values[vi++] = double(v[3]);
        }
        return true;
    }
    else if (type.IsA<Pxr::GfVec4d>())
    {
        doubles_per_value = 4;
        values.resize(nSamples*4);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfVec4d v; attrib.Get<Pxr::GfVec4d>(&v, times[i]);
            values[vi++] = v[0]; values[vi++] = v[1];
            values[vi++] = v[2]; values[vi++] = v[3];
        }
        return true;
    }
    //---------------------------------------------------------------------------
    else if (type.IsA<Pxr::GfMatrix4d>())
    {
        doubles_per_value = 16;
        values.resize(nSamples*16);
        for (size_t i=0, vi=0; i < nSamples; ++i)
        {
            Pxr::GfMatrix4d m; attrib.Get<Pxr::GfMatrix4d>(&m, times[i]);
            for (int mx=0; mx < 4; ++mx)
                for (int my=0; my < 4; ++my)
                    values[vi++] = m[mx][my];
        }
        return true;
    }
    return false; // unsupported type
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Copies all the keys from the attrib to the Nuke Knob with optional
    scale/offset applied (in that order.)

    Returns false if error occured.
*/
/*static*/ bool
FuserUsdNode::copyAttribToKnob(const Pxr::UsdAttribute& attrib,
                               bool                     allow_animation,
                               DD::Image::Knob*         k,
                               int                      view,
                               double                   scale,
                               double                   offset)
{
    if (!k)
        return false; // no knob, don't crash...

    const Pxr::TfType type = attrib.GetTypeName().GetType();

    // This is 0 if not an ArrayKnob type:
    const int nKnobDoubles = Fsr::getNumKnobDoubles(k);

    const bool debug = false;
    if (debug)
    {
        std::cout << "  copyToNukeKnob('" << attrib.GetName() << "' -> '" << k->name() << "')";
        std::cout << "[" << type << "]";
        std::cout << " isCustom=" << attrib.IsCustom();
        std::cout << ", timeSamples=" << attrib.GetNumTimeSamples();
        std::cout << ", nKnobDoubles=" << nKnobDoubles << ", view=" << view;
        std::cout << ", offset=" << offset << ", scale=" << scale;
        std::cout << std::endl;
    }

    // Handle attributes that are numeric:
    if (nKnobDoubles > 0)
    {
        AttribDoubles knob_doubles;
        if (knob_doubles.getFromAttrib(attrib, allow_animation))
        {
            // Attrib is a numeric type that we can copy into a
            // DD::Image::ArrayKnob:
            if (!knob_doubles.isValid())
                return false; // no data values, bail

            Fsr::storeDoublesInKnob(k, knob_doubles, 0/*knob_index_start*/, view);
            return true;
        }
    }

    bool ok = false;

    // Try handling other types:
    std::vector<double> times;
    if (allow_animation)
        FuserUsdNode::getPrimAttribTimeSamples(attrib, times);
    else
        Fsr::setNotAnimated(times); // force no animation (stores 'not-animated' time at index 0)

    return ok;
}


/*! Copies all the keys from the stereo attrib to a split Nuke Knob.
    Returns false if error occured.

    TODO: make this more robust. We don't want to rely on list of opaque view ints.
*/
/*static*/ bool
FuserUsdNode::copyAttribToStereoKnob(const Pxr::UsdAttribute& attrib,
                                     bool                     allow_animation,
                                     DD::Image::Knob*         k,
                                     const std::vector<int>&  views)
{
    if (!k)
        return false; // no knob, don't crash...

    int center_view = -1;
    int left_view   = -1;
    int right_view  = -1;

    // TODO: make this more robust. We don't want to rely on opaque ints.
    const size_t nViews = views.size();
    if (nViews < 2)
    {
        // Views are the split completely into stereo.
        // TODO: what to do in this case?
        return false;
    }
    else if (nViews == 2)
    {
        // Stereo only, no center:
        left_view  = views[0];
        right_view = views[1];
    }
    else if (nViews >= 3)
    {
        // 3+ views, just take the first 3, center is always last:
        left_view   = views[0];
        right_view  = views[1];
        center_view = views[2];
    }
    else
    {
    }
    //std::cout << "left_view=" << left_view;
    //std::cout << ", right_view=" << right_view;
    //std::cout << ", center_view=" << center_view << std::endl;

    // If both views are unique and in script, split the interaxial knob
    // and apply the value to the right view:
    if (left_view >= 0 && right_view >= 0 && right_view != left_view)
    {
        if (center_view >= 0 && center_view != left_view && center_view != right_view)
        {
            // TODO: support center camera!
#if 0
            // If there's a center camera split the knob for both left and
            // right and store interaxial offset for both:
            k->split_view(left_view);
            k->split_view(right_view);
            if (k->is_animated_view(left_view))
                k->clear_animated_view(left_view, -1/*index*/); // just in case...
            if (k->is_animated_view(center_view))
                k->clear_animated_view(center_view, -1/*index*/); // just in case...
            if (k->is_animated_view(right_view))
                k->clear_animated_view(right_view, -1/*index*/); // just in case...

            //
            // TODO: we need to decide how to handle the split stereo weight here.
            //

            copyAttribToKnob(attrib, allow_animation, k, left_view );
            copyAttribToKnob(attrib, allow_animation, k, right_view);
#endif
        }
        else
        {
            // Left == center, only split right:
            k->split_view(right_view);
            if (k->is_animated_view(left_view))
                k->clear_animated_view(left_view, -1/*index*/); // just in case...
            if (k->is_animated_view(right_view))
                k->clear_animated_view(right_view, -1/*index*/); // just in case...
            copyAttribToKnob(attrib, allow_animation, k, right_view);
        }
    }
    else
    {
        // Views are the same or not separate from default.
        // TODO: what to do in this case?
        return false;
    }

    return true;
}


} // namespace Fsr


// end of FuserUsdNode.cpp

//
// Copyright 2019 DreamWorks Animation
//
