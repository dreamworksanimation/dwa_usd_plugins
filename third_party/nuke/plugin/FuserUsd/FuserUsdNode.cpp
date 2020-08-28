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

//#include <DDImage/GeoInfo.h>
//#include <DDImage/GeometryList.h>


#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

// For 'prim.IsA<>' tests:
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/material.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*!
*/
FuserUsdNode::FuserUsdNode(const Pxr::UsdStageRefPtr& stage) :
    m_stage(stage),
    m_input_time(0.0),
    m_output_time(0.0),
    m_is_visible(true),
    m_has_animated_visibility(false)
{
    //
}


/*!
*/
/*virtual*/
FuserUsdNode::~FuserUsdNode()
{
    // Don't release the archive here! We want the archive pointer to
    // stick around for multiple uses of the same FuserUsdNode path.
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------



/*! Make sure the prim is Loaded, and is Valid, Defined, and Active.
    Returns false if prim is not Valid, not Active, not Defined, or
    it failed to Load.

    This fails silently so if you want specific info about why the
    prim is not useable call the version that returns an ErrorNode
    which will contain that info.

    NOTE - this may change/update the passed-in UsdPrim object if it's
    an instance proxy - it will be updated to the master prim!
*/
/*static*/
bool
FuserUsdNode::isLoadedAndUseablePrim(Pxr::UsdPrim& prim)
{
    // Only load Prims that are Active (enabled) and not Abstract:
    if (!prim.IsValid() || !prim.IsActive() || prim.IsAbstract())
        return false;

    // Expand (load) all payloads - this can be expensive, but we can't
    // avoid it since we need to traverse the payload's graph too.
    if (!prim.IsLoaded())
    {
        // Potentially need to update an Instanced prim to the Master. Remember the
        // info *before* calling Load() as the prim can be trashed afterwards:
        Pxr::UsdStageRefPtr stage = prim.GetStage();
        const Pxr::SdfPath& path  = prim.GetPath();
        const bool update_prim = prim.IsInstanceProxy();
        prim.Load(Pxr::UsdLoadWithDescendants);//Pxr::UsdLoadWithoutDescendants
        if (update_prim)
            prim = stage->GetPrimAtPath(path);
    }

    // Only consider Prims that are now Loaded, Valid (filled) and
    // Defined (not an over), and test again that it's still Active:
    if (!prim.IsValid() || !prim.IsLoaded() || !prim.IsDefined() || !prim.IsActive())
        return false;

    return true; // prim ok!
}


/*! Make sure the prim is Loaded, and is Valid, Defined, and Active.

    Returns NULL if no error otherwise a newly allocated ErrorNode which
    will contains more specific info about the error. The calling method
    must take ownership of the allocation and delete it after copying
    any relevant info.

    The returned ErrorNode will detail if the prim is not Valid,
    not Active, not Defined, or it failed to Load.

    NOTE - this may change/update the passed-in UsdPrim object if it's
    an instance proxy - it will be updated to the master prim!
*/
/*static*/
Fsr::ErrorNode*
FuserUsdNode::isLoadedAndUseablePrim(const char*   fsr_builder_class,
                                     Pxr::UsdPrim& prim,
                                     const char*   prim_load_path,
                                     bool          debug_loading)
{
    if (!prim.IsValid())
        return new Fsr::ErrorNode(fsr_builder_class,
                                  -2,
                                  "prim '%s' is not Valid() for an unknown USD reason.",
                                   prim.GetName().GetString().c_str());

    // Only handle Prims that are Active (enabled) and not Abstract:
    if (!prim.IsActive())
        return new Fsr::ErrorNode(fsr_builder_class, -2, "could not load inactive prim '%s'", prim_load_path);

    if (prim.IsAbstract())
        return new Fsr::ErrorNode(fsr_builder_class, -2, "could not load abstract prim '%s'", prim_load_path);

    // Make sure the prim is loaded before checking IsActive again, IsValid, or IsDefined:
    if (!prim.IsLoaded())
    {
        if (debug_loading)
        {
            static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

            std::cout << "      prim.IsLoaded=" << prim.IsLoaded() << " ... LOADING NOW!" << std::endl;
        }

        // Potentially need to update an Instanced prim to the Master. Remember the
        // info *before* calling Load() as the prim can be trashed afterwards:
        Pxr::UsdStageRefPtr stage = prim.GetStage();
        const Pxr::SdfPath& path  = prim.GetPath();
        const bool update_prim = prim.IsInstanceProxy();
        prim.Load(Pxr::UsdLoadWithDescendants);//Pxr::UsdLoadWithoutDescendants
        if (update_prim)
            prim = stage->GetPrimAtPath(path);

        // Check if the load happened:
        if (!prim.IsLoaded())
        {
            // Hard to debug this logic unless this prints an error to shell:
            if (debug_loading)
            {
                static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

                std::cerr << "FuserUsdNode::isLoadedAndUseablePrim('" << prim_load_path << "'): ";
                std::cerr << "error, could not load undefined USD prim type <" << prim.GetTypeName() << ">";
                std::cerr << ", ignored" << std::endl;
            }
            return new Fsr::ErrorNode(fsr_builder_class,
                                      -2,
                                      "prim '%s' could not be Loaded() for an unknown USD reason.",
                                      prim.GetName().GetString().c_str());
        }
    }


    // Ok the prim is now loaded and introspectable, let's print some info
    // about it:
    if (debug_loading)
    {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

        std::cout << "      ";
        //if (object_index >= 0)
        //    std::cout << object_index;
        std::cout << "'" << prim_load_path << "': ";
        std::cout << " IsValid=" << prim.IsValid();
        if (!prim.IsValid())
        {
            std::cout << " - skipping";
        }
        else
        {
            std::cout << ", IsLoaded=" << prim.IsLoaded();
            if (prim.GetTypeName() != "")
                std::cout << ", type='" << prim.GetTypeName() << "'";

            std::cout << ", HasPayload=" << prim.HasPayload();
#if 0
            if (prim.HasPayload())
            {
                Pxr::SdfPayload payload;
                if (prim.GetMetadata<Pxr::SdfPayload>(Pxr::SdfFieldKeys->Payload, &payload))
                {
                    std::cout << ", payload='" << payload.GetAssetPath() << "'";
                }
            }
#endif

#if 0
            // TODO: Ignore variant sets for now:
            Pxr::UsdVariantSets variantSets = prim.GetVariantSets();
            std::vector<std::string> names; variantSets.GetNames(&names);
            std::cout << ", variants[";
            for (size_t i=0; i < names.size(); ++i)
            {
                const std::string& variantName = names[i];
                std::string variantValue =
                        variantSets.GetVariantSet(
                                variantName).GetVariantSelection();
                std::cout << " " << variantName << ":" << variantValue;
            }
            std::cout << " ]";
#endif

            printPrimAttributes(" attribs", prim, false/*verbose*/, std::cout);
        }
        std::cout << std::endl;
    }


    // Handle prim states that don't allow us to create anything:
    if (!prim.IsActive())
    {
        // Prim may have been de-activated after Loading()
        return new Fsr::ErrorNode(fsr_builder_class, -2, "could not load inactive prim '%s'", prim_load_path);
    }

    if (!prim.IsValid())
        return new Fsr::ErrorNode(fsr_builder_class, -2, "could not load invalid prim '%s'", prim_load_path);

    if (!prim.IsDefined())
    {
        // Hard to debug this logic unless this always prints an error to shell:
        {
            static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

            std::cerr << "FuserUsdNode::isLoadedAndUseablePrim('" << prim_load_path << "'): ";
            std::cerr << "error, could not load undefined USD prim type <" << prim.GetTypeName() << ">";
            std::cerr << ", ignored" << std::endl;
        }
        return new Fsr::ErrorNode(fsr_builder_class,
                                  -2,
                                  "could not load undefined prim '%s' of type '%s'",
                                  prim_load_path,
                                  prim.GetTypeName().GetString().c_str());
    }

    // No error!
    return NULL;
}


/*! Is the prim able to be rendered (rasterized)?
*/
/*static*/ bool
FuserUsdNode::isRenderablePrim(const Pxr::UsdPrim& prim)
{
    if (prim.IsA<Pxr::UsdGeomMesh>())
        return true;

    // TODO: check for other renderable types here? Curves? Volumes? Pointclouds?

    return false;
}


/*! Does the prim support bounds (a bounding-box)?
*/
/*static*/ bool
FuserUsdNode::isBoundablePrim(const Pxr::UsdPrim& prim)
{
    // TODO: this logic is from the UsdKatana plugin - do we need the same?
#if 0
    if (prim.IsModel() &&
        ((!prim.IsGroup()) || PxrUsdKatanaUtils::ModelGroupIsAssembly(prim)))
        return true;

    if (PxrUsdKatanaUtils::PrimIsSubcomponent(prim))
        return true;

#endif
    return prim.IsA<Pxr::UsdGeomBoundable>();
}


/*! Is the prim a usdShade prim?
*/
/*static*/ bool
FuserUsdNode::isShadingPrim(const Pxr::UsdPrim& prim)
{
    return (prim.IsA<Pxr::UsdShadeMaterial>()  ||
            prim.IsA<Pxr::UsdShadeNodeGraph>() ||
            prim.IsA<Pxr::UsdShadeShader>());
}


/*! Is the prim visible at all?
    Checks animating visibilty of this prim and its parents.
*/
/*static*/ bool
FuserUsdNode::isVisiblePrim(const Pxr::UsdPrim& prim)
{
    bool is_visible, has_animated_visibility;
    getVisibility(prim, is_visible, has_animated_visibility);
    return is_visible;
}


/*!
*/
/*static*/ void
FuserUsdNode::getVisibility(const Pxr::UsdPrim& prim,
                            bool&               is_visible,
                            bool&               has_animated_visibility)
{
    is_visible = (prim.IsValid());
    has_animated_visibility = false;
    if (!is_visible)
        return;

    // Walk up parent hierarchy checking each prim's visibility state:
    Pxr::TfToken vis;
    Pxr::UsdPrim check_prim = prim;
    while (check_prim.IsValid())
    {
        if (check_prim.IsA<Pxr::UsdGeomImageable>())
        {
            const Pxr::UsdAttribute& vis_attrib = check_prim.GetAttribute(Pxr::UsdGeomTokens->visibility);
            if (vis_attrib.IsValid())
            {
                vis_attrib.Get(&vis, Pxr::UsdTimeCode::EarliestTime());
                if (vis == Pxr::UsdGeomTokens->invisible)
                {
                    is_visible = false;
                    has_animated_visibility = vis_attrib.ValueMightBeTimeVarying();
                    if (!has_animated_visibility)
                        break;
                }
                else if (vis_attrib.ValueMightBeTimeVarying())
                {
                    has_animated_visibility = true;
                }
            }
        }
        check_prim = check_prim.GetParent();
    }
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


static std::unordered_map<std::string, PrimvarRef> primvar_refs;

/*! Builds a static map of primvar types to the DD::Image::Attribute equivalents.
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

    //std::cout << "FuserUsdNode::copyAttribToStereoKnob('" << k->name() << "')" << std::endl;
    int center_view = -1;
    int left_view   = -1;
    int right_view  = -1;

    // TODO: make this more robust. We don't want to rely on opaque ints.
    const size_t nViews = views.size();
    if (nViews == 0)
    {
        // No declared stereo view to copy into, clear the knob:
        k->reset_to_default();
        return true;
    }
    else if (nViews == 1)
    {
        // Read a attrib as a mono value:
        { DD::Image::KnobChangeGroup change_group;
            k->reset_to_default();
            const int32_t nViews = DD::Image::OutputContext::viewcount();
            if (nViews > 2)
            {
                // 'main' is 0, so unplit starting after the first stereo view:
                for (int32_t i=nViews-1; i >= 0; --i)
                    k->unsplit_view(i);
            }
            k->clear_animated(-1); // clear any existing keys on all the sub-knobs
            copyAttribToKnob(attrib, allow_animation, k, -1/*no view*/);
        } // DD::Image::KnobChangeGroup
        return true;
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
