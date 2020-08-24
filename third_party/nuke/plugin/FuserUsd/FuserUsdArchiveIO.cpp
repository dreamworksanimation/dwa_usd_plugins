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

/// @file FuserUsdArchiveIO.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdArchiveIO.h"

#include <Fuser/ExecuteTargetContexts.h>
#include <Fuser/GeoSceneGraphReader.h>

#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <pxr/usd/ar/resolver.h>

#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/relationshipSpec.h>

#include <pxr/usd/kind/registry.h>

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/modelAPI.h>

#include <pxr/usd/usdGeom/mesh.h>

#include <pxr/usd/usdShade/material.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {

//-------------------------------------------------------------------------------


static std::mutex          m_lock;
static Pxr::UsdStageRefPtr m_null_stage;


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


#if 0
// TODO: this is from the UsdKatana plugin - do we need the same?
FnKat::DoubleAttribute
_MakeBoundsAttribute(const UsdPrim&                      prim,
                     const PxrUsdKatanaUsdInPrivateData& data)
{
    if (prim.GetPath() == SdfPath::AbsoluteRootPath())
    {
        // Special-case to pre-empt coding errors.
        return FnKat::DoubleAttribute();
    }

    const std::vector<double>& motionSampleTimes =
        data.GetMotionSampleTimes();
    std::vector<GfBBox3d> bounds =
        data.GetUsdInArgs()->ComputeBounds(prim, motionSampleTimes);

    bool hasInfiniteBounds = false;
    bool isMotionBackward = motionSampleTimes.size() > 1 &&
        motionSampleTimes.front() > motionSampleTimes.back();

    FnKat::DoubleAttribute boundsAttr = 
        PxrUsdKatanaUtils::ConvertBoundsToAttribute(
            bounds, motionSampleTimes, isMotionBackward, 
            &hasInfiniteBounds);

    // Report infinite bounds as a warning.
    if (hasInfiniteBounds) {
        FnLogWarn("Infinite bounds found at "<<prim.GetPath().GetString());
    }

    return boundsAttr;
}
#endif


//-------------------------------------------------------------------------------

// TODO: should we make supporting scenegraph instancing a user-controlled thing?
const bool instancing_enabled = true;


/*!
*/
Pxr::UsdPrim
findMatchingPrimByType(const Pxr::UsdPrim& prim,
                       const std::string&  prim_type)
{
    // Only consider prims that are valid:
    if (prim.IsValid())
    {
        // Check type for match:
        //std::cout << "  findMatchingPrim() prim='" << prim.GetName() << "'";
        //std::cout << ", typeName='" << prim.GetTypeName() << "'" << std::endl;
        if (prim.GetTypeName() == prim_type)
            return prim;

        // No match, continue down hierarchy:
        auto prim_flags = (Pxr::UsdPrimIsActive && Pxr::UsdPrimIsDefined && !Pxr::UsdPrimIsAbstract);
        const Pxr::UsdPrim::SiblingRange children = (instancing_enabled) ?
                                                        prim.GetFilteredChildren(Pxr::UsdTraverseInstanceProxies(prim_flags)) :
                                                        prim.GetAllChildren();

        for (Pxr::UsdPrim::SiblingRange::iterator it=children.begin(); it != children.end(); ++it)
        {
            const Pxr::UsdPrim child(*it);
            Pxr::UsdPrim match = findMatchingPrimByType(child, prim_type);
            if (match.IsValid())
                return match;
        }
    }
    return Pxr::UsdPrim();
}

/*!
*/
Pxr::UsdPrim
findFirstMatchingPrim(const Pxr::UsdStageRefPtr& stage,
                      const std::string&         start_path,
                      const std::string&         prim_type)
{
    if (prim_type.empty())
        return Pxr::UsdPrim();

    // Find the starting prim:
    // TODO: finish this!
    Pxr::UsdPrim start_prim = stage->GetPseudoRoot();

    // Get the prim type from the prim_type:
    std::string type(prim_type);
    std::transform(type.begin(), type.end(), type.begin(), ::tolower); // to lower-case

    // TODO: change this to a map/dictionary lookup
    std::string schema_type;
    if      (type == "xform"     ) schema_type = "Xform"; // TODO: change to pxr:: constant refs like Alembic ones
    else if (type == "camera"    ) schema_type = "Camera";
    else if (type == "light"     ) schema_type = "Light";
    //
#if 0
    else if (type == "mesh"      ) schema_type = Alembic::AbcGeom::IPolyMesh::getSchemaTitle();
    else if (type == "geomsubset") schema_type = Alembic::AbcGeom::IFaceSet::getSchemaTitle();
    //
    else if (type == "points"    ) schema_type = Alembic::AbcGeom::IPoints::getSchemaTitle();
    else if (type == "curves"    ) schema_type = Alembic::AbcGeom::ICurves::getSchemaTitle();
#endif

    if (schema_type.empty())
        return Pxr::UsdPrim();

#if 0
    std::cout << "====== findFirstMatchingPrim '" << prim_type << "' starting at '" << start_path << "'";
    std::cout << ", schema_type='" << schema_type << "'" << std::endl;
    Pxr::UsdPrim match = findMatchingPrimByType(start_prim, schema_type);
    if (match.IsValid())
        std::cout << "   match='" << match.GetName() << "' ========" << std::endl;
    return match;
#else
    return findMatchingPrimByType(start_prim, schema_type);
#endif
}


//-------------------------------------------------------------------------------


/*! Get a list of node descriptions from the USD file, as cheaply as possible...
*/
static void
getNodeDescriptions(const Pxr::UsdPrim&      prim,
                    Fsr::NodeDescriptionMap& node_description_map,
                    int                      depth,
                    int                      max_depth,
                    bool                     debug=false)
{
    //std::cout << "  getNodeDescriptions() prim='" << prim.GetPath() << "' depth=" << depth << ", max_depth=" << max_depth << std::endl;
    if (!prim.IsValid())
        return;

    const std::string& path = prim.GetPath().GetString();
    if (path != "/")
        ++depth; // skip pseudo root as a level

    if (depth > max_depth)
        return;

    // Only consider Prims that are valid after loading:
    if (!FuserUsdNode::isLoadedAndUseablePrim(prim))
        return;

#if 0
    std::cout << "    " << node_description_map.size();
    std::cout << " '" << prim.GetPath() << "' [" << prim.GetTypeName() << "]";
    //Pxr::SdfPrimSpecHandle spec = prim.GetPrimDefinition();
    //if (spec)
    //    std::cout << ", Kind='" << spec->GetKind() << "'";
    std::cout << ", IsLoaded=" << prim.IsLoaded();
    std::cout << ", IsValid=" << prim.IsValid();
    std::cout << ", IsActive=" << prim.IsActive();
    std::cout << ", IsDefined=" << prim.IsDefined();
    std::cout << ", IsAbstract=" << prim.IsAbstract();
    //std::cout << ", isModel=" << prim.IsModel();
    //std::cout << ", isCamera=" << prim.IsA<Pxr::UsdGeomCamera>();
    //std::cout << ", isScope=" << prim.IsA<Pxr::UsdGeomScope>();
    //std::cout << ", isXform=" << prim.IsA<Pxr::UsdGeomXformable>();
    std::cout << std::endl;
    if (0)
    {
        std::cout << "      prim layer stack:" << std::endl;
        const Pxr::SdfPrimSpecHandleVector primspecs = prim.GetPrimStack();
        TF_FOR_ALL(it, primspecs)
        {
            Pxr::SdfPrimSpecHandle child = *it;
            std::cout << "        " << (*child->GetLayer()).GetDisplayName();
            std::cout << ": " << (*child->GetLayer()).GetRealPath();
            std::cout << std::endl;
        }
    }
#endif

    // We do a manual child walk so we can keep track of the depth we're at.
    // Using just a UsdPrimRange iterator means losing where we are in depth.
    auto prim_flags = (Pxr::UsdPrimIsActive && Pxr::UsdPrimIsDefined && !Pxr::UsdPrimIsAbstract);
    const Pxr::UsdPrim::SiblingRange children = (instancing_enabled) ?
                                                    prim.GetFilteredChildren(Pxr::UsdTraverseInstanceProxies(prim_flags)) :
                                                    prim.GetAllChildren();

    // Skip the pseudo-root node and only add with real nodes:
    if (path != "/")
    {
        const bool is_leaf = (children.empty());
        const bool is_truncated = (depth == max_depth && !is_leaf);
        const bool is_visible = FuserUsdNode::isVisiblePrim(prim);

        const std::string& name = prim.GetName().GetString();
        const std::string& type = prim.GetTypeName().GetString();
        std::string note;
        if (is_truncated)
            note = "PATH_TRUNCATED";
        else if (!is_visible)
            note = "INVISIBLE";

#if 0
        std::cout << "    " << node_description_map.size();
        std::cout << " '" << path << "' ('" << name << "'[" << type << "])";
        Pxr::SdfPrimSpecHandle spec = prim.GetPrimDefinition();
        if (spec)
            std::cout << ", Kind='" << spec->GetKind() << "'";
        //std::cout << ", isModel=" << prim.IsModel();
        //std::cout << ", isCamera=" << prim.IsA<Pxr::UsdGeomCamera>();
        //std::cout << ", isScope=" << prim.IsA<Pxr::UsdGeomScope>();
        //std::cout << ", isXform=" << prim.IsA<Pxr::UsdGeomXformable>();
        std::cout << ", is_leaf=" << is_leaf << ", is_truncated=" << is_truncated;
        std::cout << std::endl;
#endif

#ifdef DWA_INTERNAL_BUILD
        // Check if it's a StereoRig by looking at the name. Bad!
        // TODO: this needs to improve with the StereoRig API.
        if (type.empty() && strncmp(name.c_str(), "stereo", 6)==0)
        {
            node_description_map[path] = Fsr::NodeDescription(name/*path*/, std::string("StereoRig")/*type*/, note);
            //std::cout << "    '" << path << "' [StereoRig]" << std::endl;
        }
        else
        {
            node_description_map[path] = Fsr::NodeDescription(name/*path*/, type/*type*/, note);
            //std::cout << "    '" << path << "' ('" << name << "'[" << type << "])" << std::endl;
        }
#else
        node_description_map[path] = Fsr::NodeDescription(name/*path*/, type/*type*/, note);
        //std::cout << "    '" << path << "' ('" << name << "'[" << type << "])" << std::endl;
#endif
    }

    for (Pxr::UsdPrim::SiblingRange::iterator it=children.begin(); it != children.end(); ++it)
    {
        const Pxr::UsdPrim child(*it);
        getNodeDescriptions(child, node_description_map, depth, max_depth, debug);
    }

}


/*! Add or remove a path from the selection set if it matches any patterns.

    TODO: move this to Fuser SceneLoader or NodeFilterPattern class
*/
inline void
selectMatchingPath(const std::string&                path,
                   const Fsr::NodeFilterPatternList& node_filter_patterns,
                   std::set<std::string>&            selected_paths,
                   bool                              debug=false)
{
    if (path.empty())
        return;

    for (size_t i=0; i < node_filter_patterns.size(); ++i)
    {
        const std::string& mask = node_filter_patterns[i].name_expr;
        if (mask.empty())
        {
            std::cerr << "fsrUsdIO::selectMatchingPath(): warning, mask pattern " << i << " is empty!" << std::endl;
            continue;
        }

            //std::cout << "    " << " mask='" << mask << "', match=" << Fsr::globMatch(mask, path) << std::endl;
        if ((mask[0] == '-' || mask[0] == '^') && Fsr::globMatch(mask.c_str()+1, path.c_str()))
        {
            selected_paths.erase(path);
            //std::cout << "      remove object '" <<  path << "'" << std::endl;
        }
        else if (mask[0] == '+' && Fsr::globMatch(mask.c_str()+1, path.c_str()))
        {
            selected_paths.insert(path);
            //std::cout << "      add object '" <<  path << "'" << std::endl;
        }
        else if (Fsr::globMatch(mask, path))
        {
            selected_paths.insert(path);
            //std::cout << "      add object '" <<  path << "'" << std::endl;
        }
        else
        {
            ;//std::cout << "      no match" << std::endl;
        }
    }
}



/*! Get a list of nodes with pattern-matched names from the USD file,
    as cheaply as possible...
*/
static void
findSelectedNodes(const Pxr::UsdPrim&               prim,
                  const Fsr::NodeFilterPatternList& node_filter_patterns,
                  Fsr::NodePathSelections&          selections,
                  bool                              debug=false)
{
    auto prim_flags = (Pxr::UsdPrimIsActive && Pxr::UsdPrimIsDefined && !Pxr::UsdPrimIsAbstract);
    const Pxr::UsdPrimRange range(prim, Pxr::UsdTraverseInstanceProxies(prim_flags));
    for (Pxr::UsdPrimRange::iterator it=range.begin(); it != range.end(); ++it)
    {
        const Pxr::UsdPrim& range_prim = *it;
        const std::string& path = range_prim.GetPath().GetString();
        //std::cout << "  findSelectedNodes() path='" << path << "' [" << range_prim.GetTypeName() << "]" << std::endl;

        if (path.empty())
        {
            // Empty paths shouldn't happen...
            std::cerr << "fsrUsdIO::findSelectedNodes(): warning, path for prim '" << path;
            std::cerr << "' is empty!" << std::endl;
            continue;
        }

        // Only consider Prims that are valid after loading:
        if (!FuserUsdNode::isLoadedAndUseablePrim(range_prim))
            continue;

#if 0
        std::cout << "    '" << range_prim.GetPath() << "' [" << range_prim.GetTypeName() << "]";
        //Pxr::SdfPrimSpecHandle spec = range_prim.GetPrimDefinition();
        //if (spec)
        //    std::cout << ", Kind='" << spec->GetKind() << "'";
        std::cout << ", IsLoaded=" << range_prim.IsLoaded();
        std::cout << ", IsValid=" << range_prim.IsValid();
        std::cout << ", IsActive=" << range_prim.IsActive();
        std::cout << ", IsDefined=" << range_prim.IsDefined();
        std::cout << ", IsAbstract=" << range_prim.IsAbstract();
        //std::cout << ", isModel=" << range_prim.IsModel();
        //std::cout << ", isCamera=" << range_prim.IsA<Pxr::UsdGeomCamera>();
        //std::cout << ", isScope=" << range_prim.IsA<Pxr::UsdGeomScope>();
        //std::cout << ", isXform=" << range_prim.IsA<Pxr::UsdGeomXformable>();
        std::cout << std::endl;
#endif

        if (FuserUsdNode::isRenderablePrim(range_prim))
        {
            //std::cout << "    findSelectedNodes() renderable path='" << path << "'" << std::endl;
            selectMatchingPath(path, node_filter_patterns, selections.objects);
        }
        else if (FuserUsdNode::isShadingPrim(range_prim))
        {
            // UsdShade handling - Shaders are *always* underneath a UsdShadeMaterial so instead
            // of selecting a whole tree of UsdShadeShader nodes we select the top of the network
            // by adding the top UsdShadeMaterial, then rely on the node creation logic in
            // buildUsdNode() to create the network tree underneath:
            if (range_prim.IsA<Pxr::UsdShadeMaterial>())
                selectMatchingPath(path, node_filter_patterns, selections.materials);

            it.PruneChildren(); // skip going down down shader tree
        }
#if 0
        else if (FuserUsdNode::isLightPrim(range_prim))
        {
            //std::cout << "    findSelectedNodes() light path='" << path << "'" << std::endl;
            selectMatchingPath(path, node_filter_patterns, selections.lights);
        }
#endif
        else
        {
            // Handle non-renderable types too!
            //std::cout << "      NOT RENDERABLE" << std::endl;
        }

    }

}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


//! Map of shared PopulationMasks keyed by stage hash:
typedef std::unordered_map<uint64_t, StageCacheReference*> SharedStageCacheReferenceMap;
static SharedStageCacheReferenceMap m_shared_stage_references;


/*! Create or update a shared StageCacheReference, keyed by 'hash'.

    If 'parent_path' is not empty it's added to the populate_mask.

    'stage_id' replaces the current one, even if empty.
*/
/*static*/
StageCacheReference*
StageCacheReference::createStageReference(uint64_t                        hash,
                                          const std::vector<std::string>& paths,
                                          const std::string&              stage_id)
{
    // If it already exists get the reference:
    StageCacheReference* stage_reference = findStageReference(hash);

    std::lock_guard<std::mutex> guard(m_lock);
    if (!stage_reference)
    {
        stage_reference = new StageCacheReference();
        m_shared_stage_references[hash] = stage_reference;
    }
    assert(stage_reference);

    for (size_t j=0; j < paths.size(); ++j)
    {
        if (!paths[j].empty())
            stage_reference->m_populate_mask.Add(Pxr::SdfPath(paths[j]));
    }
    stage_reference->m_stage_id = stage_id;

    return stage_reference;
}


/*! Find a shared StageCacheReference, keyed by 'hash'.
*/
/*static*/
StageCacheReference*
StageCacheReference::findStageReference(uint64_t hash)
{
    std::lock_guard<std::mutex> guard(m_lock);
    SharedStageCacheReferenceMap::const_iterator it = m_shared_stage_references.find(hash);
    return (it != m_shared_stage_references.end()) ? it->second : NULL;
}


#if 0
/*! Find and remove a shared StageCacheReference keyed by 'hash'.
*/
/*static*/
void
StageCacheReference::removeStageReference(uint64_t hash)
{
    std::lock_guard<std::mutex> guard(m_lock);
    SharedStageCacheReferenceMap::const_iterator it = m_shared_stage_references.find(hash);
    if (it != m_shared_stage_references.end())
    {
        delete it->second;
        m_shared_stage_references.erase(it);
    }
}
#endif


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*!
*/
class StageOpenRequest : public Pxr::UsdStageCacheRequest
{
  private:
    bool m_debug_stage;
    //
    Pxr::SdfLayerHandle           m_root_layer;         //!<
    //Pxr::SdfLayerHandle           m_session_layer;      //!< TODO: Not sure we need session layer...
    Pxr::ArResolverContext        m_path_resolver_ctx;  //!<
    Pxr::UsdStage::InitialLoadSet m_initial_load_set;   //!<
    Pxr::UsdStagePopulationMask   m_populate_mask;      //!<


  public:
    //!
    StageOpenRequest(Pxr::UsdStage::InitialLoadSet      initial_load_set,
                     const Pxr::SdfLayerHandle&         root_layer,
                     //const Pxr::SdfLayerHandle&         session_layer,
                     const Pxr::ArResolverContext&      path_resolver_ctx,
                     const Pxr::UsdStagePopulationMask& populate_mask,
                     bool                               debug_stage=false) :
        Pxr::UsdStageCacheRequest(),
        m_debug_stage(debug_stage),
        m_root_layer(root_layer),
        //m_session_layer(session_layer),
        m_path_resolver_ctx(path_resolver_ctx),
        m_initial_load_set(initial_load_set),
        m_populate_mask(populate_mask)
    {
        if (m_debug_stage)
        {
            std::cout << " StageOpenRequest::ctor(";
            std::cout << "root_layer='" << m_root_layer->GetRealPath() << "'";
            std::cout << ", populate='" << m_populate_mask << "')";
            std::cout << std::endl;
        }
    }

    //!
    /*virtual*/
    bool IsSatisfiedBy(const Pxr::UsdStageRefPtr& stage) const
    {
        if (m_debug_stage)
        {
            std::cout << " StageOpenRequest::IsSatisfiedBy(";
            std::cout << "root_layer='" << m_root_layer->GetRealPath() << "'";
            std::cout << ", populate='" << m_populate_mask << "')";
            std::cout << " stage(root_layer='" << stage->GetRootLayer()->GetRealPath() << "'";
            std::cout << ", populate='" << stage->GetPopulationMask() << "')";
            std::cout << std::endl;
        }
        return (m_root_layer        == stage->GetRootLayer() &&
                //m_session_layer     == stage->GetSessionLayer() &&
                m_path_resolver_ctx == stage->GetPathResolverContext() &&
                m_populate_mask     == stage->GetPopulationMask());
    }
    
    //!
    /*virtual*/
    bool IsSatisfiedBy(const Pxr::UsdStageCacheRequest& pending) const
    {
        const StageOpenRequest* req = dynamic_cast<const StageOpenRequest*>(&pending);
        if (!req)
            return false;

        return (m_root_layer        == req->m_root_layer &&
                //m_session_layer     == req->m_session_layer &&
                m_path_resolver_ctx == req->m_path_resolver_ctx &&
                m_populate_mask     == req->m_populate_mask);
    }

    //!
    /*virtual*/
    Pxr::UsdStageRefPtr Manufacture()
    {
        // TODO: do we need to be able to switch between the two modes?
        if (1)
        {
            Pxr::UsdStageRefPtr stage;
            stage = Pxr::UsdStage::OpenMasked(m_root_layer,
                                              Pxr::TfNullPtr,//m_session_layer,
                                              m_path_resolver_ctx,
                                              m_populate_mask,
                                              m_initial_load_set);
#if DEBUG
            assert(stage);
#endif
            //std::cout << "populate=[ " << stage->GetPopulationMask() << " ]" << std::endl;

            if (!m_populate_mask.IsEmpty())
            {
                // ExpandPopulationMask() searches for all relationships and includes
                // any targets in the mask.
                // However this can be very expensive, so we use a more targeted
                // version that tests for explicitly-desired relationship types,
                // like materials and instances:
                //stage->ExpandPopulationMask();

                bool added_to_mask = false;

                Pxr::UsdPrimRange range = stage->Traverse();

                // Find and expand all Meshes with material relationships.
                // TODO: check for point instances?
                {
                    for (Pxr::UsdPrimRange::iterator it=range.begin(); it != range.end(); ++it)
                    {
                        if (it->IsA<Pxr::UsdGeomMesh>())
                        {
                            //std::cout << "  mesh path '" << it->GetPath() << "'" << std::endl;
                            const Pxr::UsdRelationship mat_rel = it->GetRelationship(Pxr::TfToken("material:binding"));
                            Pxr::SdfPathVector targets;
                            mat_rel.GetTargets(&targets);
                            for (size_t i=0; i < targets.size(); ++i)
                            {
                                //std::cout << "    '" << targets[i].GetString() << "'" << std::endl;
                                m_populate_mask.Add(targets[i]);
                            }
                            added_to_mask = true;

                            it.PruneChildren(); // skip children
                        }
                        else if (it->IsA<Pxr::UsdShadeMaterial>())
                            it.PruneChildren(); // skip shader children
                    }
                }

                // SetPopulationMask() will recompose the stage making the additional
                // prims available:
                if (added_to_mask)
                    stage->SetPopulationMask(m_populate_mask);
            }

            return stage;

        }
        else
        {
            return Pxr::UsdStage::Open(m_root_layer,
                                       Pxr::TfNullPtr,//m_session_layer,
                                       m_path_resolver_ctx,
                                       m_initial_load_set);
        }

    }

};


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Attempt to Load/Find the Stage to pass to the FuserUsdNodes.

    If 'identifier' is empty then 'scene_file' is used to create the
    stage and 'identifer' is filled in from the cache.
    If 'identifier' is not empty then its value is used to retrived
    the previously cached stage.
*/
/*static*/
Pxr::UsdStageRefPtr
StageCacheReference::getStage(const std::string& scene_file,
                              const uint64_t     stage_hash,
                              bool               debug_stage)
{
    Pxr::UsdStageCache& stage_cache = Pxr::UsdUtilsStageCache::Get();

    // Return a previously cached stage if there's an existing stage identifier:
    if (!m_stage_id.empty())
    {
        Pxr::UsdStageRefPtr stage = stage_cache.Find(Pxr::UsdStageCache::Id::FromString(m_stage_id));
        if (stage)
        {
            if (debug_stage)
            {
                std::cout << "       fsrUsdIO::getStage(" << std::hex << stage_hash << std::dec << "): ";
                std::cout << "EXISTING stage cache with ID '" << m_stage_id << "'";
                std::cout << std::endl;
            }
            return stage;
        }
        // ID's not in cache, fall back to using scene_file
    }

    Pxr::SdfLayerRefPtr root_layer = Pxr::SdfLayer::FindOrOpen(Pxr::TfStringTrimRight(scene_file));
    if (!root_layer)
    {
        // File not found!
        if (debug_stage)
        {
            std::cerr << "fsrUsdIO::getStage(): ";
            std::cerr << "error, USD scene file '" << scene_file << "' not found";
            std::cerr << std::endl;
        }
        return m_null_stage;
    }

    // Successfully found root scene file.
    if (debug_stage)
        std::cout << "       fsrUsdIO::getStage(" << std::hex << stage_hash << std::dec << "): root_layer='" << root_layer->GetRealPath() << "'";

    //---------------------------------------------------------------------------------------
    // Now let's get the 'base layer name' which I'm not sure we need...
    //
    // This routine is from Pxr::UsdUtilsGetModelNameFromRootLayer().
    //  Pxr::TfToken base_layer_name = Pxr::UsdUtilsGetModelNameFromRootLayer(root_layer);
    //
    // TODO: what is this for? i.e. why do we need to get the root 'base layer name'? This
    //       is normally the scene file name, but I guess if we're pointing at a lower
    //       down usd file, say 'camera.usd' then it's not really a 'scene file'...
    // 
    //       i.e. scene_file = '/foo/bar.usd', base_layer_name = 'bar'.
    //
    // First check for a default prim name:
    Pxr::TfToken base_layer_name = root_layer->GetDefaultPrim();
    if (base_layer_name.IsEmpty())
    {
        // If no default prim, see if there is a prim w/ the same "name" as the
        // file.  "name" here means the string before the first ".":
        const std::string& path = root_layer->GetRealPath();
        std::string base_name = Pxr::TfGetBaseName(path);
        base_layer_name = Pxr::TfToken(base_name.substr(0, base_name.find('.')));
        if (base_layer_name.IsEmpty() ||
            !Pxr::SdfPath::IsValidIdentifier(base_layer_name) ||
            !root_layer->GetPrimAtPath(Pxr::SdfPath::AbsoluteRootPath().AppendChild(base_layer_name)))
        {
            // Otherwise, fallback to getting the first non-class child in the layer.
            for (auto child=Pxr::TfMakeIterator(root_layer->GetRootPrims()); child; ++child)
            {
                const Pxr::SdfPrimSpecHandle& prim = *child;
                if (prim->GetSpecifier() != Pxr::SdfSpecifierClass)
                {
                    base_layer_name = prim->GetNameToken();
                    break;
                }
            }
        }
    }
    if (debug_stage)
        std::cout << ", base_layer_name='" << base_layer_name << "'";


    //---------------------------------------------------------------------------------------
    //
    // Get (or create) a shared stage instance for the root path and
    // population mask.
    // The stage is cached for the lifetime of any created FuserUsdNodes.
    //
    // TODO: how do we check that the stage memory gets released on last
    //       FuserUsdNode deletion? Do we care...?
    //
    Pxr::SdfPath root_path = Pxr::SdfPath::AbsoluteRootPath().AppendChild(base_layer_name);
    //Pxr::SdfLayerRefPtr session_layer = getSessionLayer(root_path.GetString());

    std::pair<Pxr::UsdStageRefPtr, bool> stage_ref =
        stage_cache.RequestStage(StageOpenRequest(Pxr::UsdStage::LoadNone/*initial_load_set*/,
                                                  root_layer,
                                                  //session_layer,
                                                  Pxr::ArGetResolver().GetCurrentContext()/*path_resolver_ctx*/,
                                                  m_populate_mask,
                                                  debug_stage));


    Pxr::UsdStageRefPtr stage = stage_ref.first;
    // If it returns true then the stage has been created, otherwise it's a cached stage:
    if (stage_ref.second)
    {
        if (debug_stage)
        {
            std::cout << "  {USD STAGE CACHE} Loaded stage '" << scene_file;
            std::cout << "' with UsdStage address " << std::hex << (size_t)stage.operator->() << std::dec;
            //std::cout << " and sessionAttr hash '" << sessionAttr.getHash().str().c_str() << "'";
            std::cout << std::endl;
        }

    }
    else
    {
        if (debug_stage)
        {
            std::cout << "  {USD STAGE CACHE} Fetching cached stage '" << scene_file;
            std::cout << "' with UsdStage address " << std::hex << (size_t)stage.operator->() << std::dec;
            //std::cout << " and sessionAttr hash '" << sessionAttr.getHash().str().c_str() << "'";
            std::cout << std::endl;
        }
    }


#if 0
    //---------------------------------------------------------------------------------------
    // Mute layers according to a regex.
    // TODO: replicate this mute layers feature from the UsdKatana plugin?
    std::string ignoreLayerRegex = "";
    _SetMutedLayers(stage, ignoreLayerRegex);
#endif


    if (!stage)
    {
        if (debug_stage)
        {
            std::cerr << "fsrUsdIO::getStage(): ";
            std::cerr << "error, USD Stage could not be created, unable to load node";
            std::cerr << std::endl;
        }
        return m_null_stage;
    }

    if (0)//(debug_stage)
    {
        const Pxr::SdfLayerHandleVector in_layers = stage->GetUsedLayers(true/*includeClipLayers*/);
        if (in_layers.size() > 0)
        {
            // Alphabetize them:
            std::set<std::string> sorted_layers;
            for (size_t i=0; i < in_layers.size(); ++i)
            {
                const std::string& path = in_layers[i]->GetRealPath();
                if (!path.empty())
                    sorted_layers.insert(path);
            }
            if (sorted_layers.size() > 0)
            {
                std::cout << "    Loaded layers:" << std::endl;
                for (std::set<std::string>::const_iterator it=sorted_layers.begin(); it != sorted_layers.end(); ++it)
                    std::cout << "     " << *it << std::endl;
            }
        }
    }

    // Update the reference with the new stage ID:
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_stage_id = stage_cache.GetId(stage).ToString();
    }

    if (debug_stage)
    {
        std::cout << "fsrUsdIO::getStage(" << std::hex << stage_hash << std::dec << "): ";
        std::cout << "using stage cache with ID '" << m_stage_id << "'";
        std::cout << std::endl;
    }

    return stage;
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! No stage exists yet.
*/
FuserUsdArchiveIO::FuserUsdArchiveIO(const Fsr::ArgSet& args) :
    Fsr::Node(args)
{
    //std::cout << "  FuserUsdArchiveIO::ctor(" << this << ")" << std::endl;
}


/*! Wrap a previously created stage.
*/
FuserUsdArchiveIO::FuserUsdArchiveIO(const Pxr::UsdStageRefPtr& stage,
                                     const Fsr::ArgSet&         args) :
    Fsr::Node(args),
    m_stage(stage)
{
    //std::cout << "  FuserUsdArchiveIO::ctor(" << this << ")" << std::endl;
}


/*! Returns -1 on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdArchiveIO::_execute(const Fsr::NodeContext& target_context,
                            const char*             target_name,
                            void*                   target,
                            void*                   src0,
                            void*                   src1)
{
    // We need a context and a target name to figure out what to do:
    if (!target_name || !target_name[0])
        return -1; // no context target!

    const bool debug_archive = target_context.getBool(Arg::Scene::file_archive_debug, false);//getBool("UsdIO:debug_archive_loading", false);

    if (debug() || debug_archive)
    {
        std::cout << "  FuserUsdArchiveIO::_execute(" << this << ") args[" << args() << "]";
        std::cout << ": target='" << target_name << "'";
        std::cout << " args[" << target_context.args() << "]";
        std::cout << std::endl;
    }

    // If there's been no stage assigned yet we can only execute the open-cache target:
    if (!m_stage)
    {
        if (strcmp(target_name, Fsr::GeoSceneFileArchiveContext::name)==0)
        {
            //-----------------------------------------------------------
            // Execution target for opening a stage for the first time
            //-----------------------------------------------------------
            Fsr::GeoSceneFileArchiveContext* cache_ctx  = reinterpret_cast<Fsr::GeoSceneFileArchiveContext*>(src0);
            std::vector<std::string>*     populate_mask = reinterpret_cast<std::vector<std::string>*>(src1);
            std::string*                  cache_id      = reinterpret_cast<std::string*>(target);

            // Any null pointers throw a coding error:
            if (!cache_ctx || !populate_mask || !cache_id)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            if (debug_archive)
            {
                std::cout << "       openCacheReference(" << std::hex << cache_ctx->archive_context_hash.value() << std::dec << "):";
                std::cout << " populate_mask=[";
                for (size_t j=0; j < populate_mask->size(); ++j)
                    std::cout << " '" << (*populate_mask)[j] << "'";
                std::cout << " ]";
                std::cout << std::endl;
            }

            // Which command are we executing for this target?
            const std::string& archive_command = getString(Arg::node_directive);
            if (archive_command == Arg::Scene::file_archive_open)
            {
                if (debug_archive)
                    std::cout << "       *************  OPEN ARCHIVE (GEO) *************" << std::endl;

                // Create stage with the provided population mask:
                StageCacheReference* stage_reference =
                    StageCacheReference::createStageReference(cache_ctx->archive_context_hash.value(),
                                                              *populate_mask);
                assert(stage_reference);

                // getStage() will/may update the stage_id string in the shared map:
                Pxr::UsdStageRefPtr stage = stage_reference->getStage(cache_ctx->scene_file,
                                                                      cache_ctx->archive_context_hash.value(),
                                                                      debug_archive);
                // Return the stage cache ID string:
                *cache_id = stage_reference->stageId();

                if (debug_archive)
                    std::cout << "         new cache_id=" << *cache_id << std::endl;

            }
            else if (archive_command == Arg::Scene::file_archive_invalidate)
            {
                // TODO: finish this
                if (debug_archive)
                    std::cout << "       *************  INVALIDATE ARCHIVE (GEO) *************" << std::endl;
            }
            else if (archive_command == Arg::Scene::file_archive_close)
            {
                if (debug_archive)
                    std::cout << "       *************  INVALIDATE ARCHIVE (GEO) *************" << std::endl;

                Pxr::SdfLayerRefPtr root_layer = Pxr::SdfLayer::FindOrOpen(Pxr::TfStringTrimRight(target_context.getString(Arg::Scene::file)));
                if (root_layer)
                {
                    Pxr::UsdUtilsStageCache::Get().EraseAll(root_layer);
                    //const size_t ret = Pxr::UsdUtilsStageCache::Get().EraseAll(root_layer);
                    //std::cout << "           erased " << ret << " stage(s)" << std::endl;
                }

            }
            else
            {
                if (debug_archive)
                    std::cerr << "FuserUsdArchiveIO: warning, unrecognized archive command '" << archive_command << "', ignoring." << std::endl;
                return 0; // no user-abort
            }

            return 0; // success
        }
        else if (strcmp(target_name, Fsr::SceneArchiveContext::name)==0)
        {
            // Which command are we executing for this target?
            const std::string& archive_command = getString(Arg::node_directive);
            if (archive_command == Arg::Scene::file_archive_open)
            {
                if (debug_archive)
                    std::cout << "       *************  OPEN ARCHIVE (SCENE) *************" << std::endl;
            }
            else if (archive_command == Arg::Scene::file_archive_invalidate)
            {
                if (debug_archive)
                    std::cout << "       *************  INVALIDATE ARCHIVE (SCENE) *************" << std::endl;

                Pxr::SdfLayerRefPtr root_layer = Pxr::SdfLayer::FindOrOpen(Pxr::TfStringTrimRight(target_context.getString(Arg::Scene::file)));
                if (root_layer)
                {
                    Pxr::UsdUtilsStageCache::Get().EraseAll(root_layer);
                    //const size_t ret = Pxr::UsdUtilsStageCache::Get().EraseAll(root_layer);
                    //std::cout << "           erased " << ret << " stage(s)" << std::endl;
                }

            }
            else if (archive_command == Arg::Scene::file_archive_close)
            {
                if (debug_archive)
                    std::cout << "       *************  CLOSE ARCHIVE (SCENE) *************" << std::endl;
            }
            else
            {
                if (debug_archive)
                    std::cerr << "FuserUsdArchiveIO: warning, unrecognized archive command '" << archive_command << "', ignoring." << std::endl;
                return 0; // no user-abort
            }
            
            return 0; // success

        }

    }
    else
    {
        // Following targets require a stage:
        if (strcmp(target_name, Fsr::SceneNodeDescriptions::name)==0)
        {
            //-----------------------------------------------------------
            // Execution target requiring a previously created stage
            //-----------------------------------------------------------
            Fsr::SceneNodeDescriptions* scene_nodes_ctx = reinterpret_cast<Fsr::SceneNodeDescriptions*>(target);

            // Any null pointers throw a coding error:
            if (!scene_nodes_ctx || !scene_nodes_ctx->node_description_map)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            const std::string& search_command  = getString(Arg::node_directive);
            const std::string& scene_node_path = getString(Arg::Scene::path   );
            const bool         scene_debug     = getBool(  Arg::Scene::read_debug, false);
            const bool         geo_debug       = getBool(Arg::NukeGeo::read_debug, false);
            if (debug_archive)
            {
                std::cout << "       search_command '" << search_command << "'";
                std::cout << ", scene_node_path='" << scene_node_path << "'";
                std::cout << std::endl;
            }

            scene_nodes_ctx->node_description_map->clear();

            // Which command are we executing for this target?
            if (search_command == Arg::Scene::node_find_first_valid)
            {
                const std::string& scene_node_type = target_context.getString(  Arg::Scene::node_type);
                const std::string& geo_node_type   = target_context.getString(Arg::NukeGeo::node_type);
                if (scene_node_type.empty() && geo_node_type.empty())
                {
                    if (scene_debug || geo_debug)
                        std::cerr << "FuserUsdArchiveIO: warning, unable to search for a default node, default type is missing" << std::endl;
                    return true; // no user-abort
                }

                const std::string node_type((!scene_node_type.empty()) ? scene_node_type : geo_node_type);

                const Pxr::UsdPrim prim = findFirstMatchingPrim(m_stage, scene_node_path, node_type);
                if (prim.IsValid())
                {
                    (*scene_nodes_ctx->node_description_map)[prim.GetPath().GetString()] =
                        Fsr::NodeDescription(prim.GetName().GetString()/*name*/, node_type/*type*/);
                }

            }
            else if (search_command == Arg::Scene::node_type_contents)
            {
                Pxr::UsdPrim start_prim = m_stage->GetPseudoRoot();
                if (target_context.args().has(Arg::Scene::path))
                {
                    std::string start_path = target_context.getString(Arg::Scene::path);
                    if (!start_path.empty() && start_path != "/")
                    {
                        // Trim '/' off end if it's not root:
                        if (start_path[start_path.size()-1] == '/')
                            start_path[start_path.size()-1] = 0;
                        start_prim = m_stage->GetPrimAtPath(Pxr::SdfPath(start_path));
                    }
                }

                if (start_prim.IsValid())
                {
                    getNodeDescriptions(start_prim,
                                        *scene_nodes_ctx->node_description_map,
                                        0/*depth*/,
                                        target_context.getInt(Arg::Scene::path_max_depth, 5),
                                        scene_debug);
                }
            }
            else
            {
                if (scene_debug || geo_debug)
                    std::cerr << "FuserUsdArchiveIO: warning, unrecognized search directive '" << search_command << "', ignoring." << std::endl;
                return 0; // no user-abort
            }

            return 0; // success
        }
        else if (strcmp(target_name, Fsr::ScenePathFilters::name)==0)
        {
            //-----------------------------------------------------------
            // Execution target requiring a previously created stage
            //-----------------------------------------------------------
            Fsr::NodeFilterPatternList*  node_filter_patterns = reinterpret_cast<Fsr::NodeFilterPatternList*>(src0);
            Fsr::SelectedSceneNodePaths* node_selections      = reinterpret_cast<Fsr::SelectedSceneNodePaths*>(target);

            // Any null pointers throw a coding error:
            if (!node_filter_patterns || !node_selections || !node_selections->node_path_selections)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            if (debug_archive)
            {
                std::cout << "FuserUsdArchiveIO::getSceneNodes() masks[";
                for (size_t i=0; i < node_filter_patterns->size(); ++i)
                    std::cout << " '" << (*node_filter_patterns)[i].name_expr << "'";
                std::cout << " ]" << std::endl;
            }

            node_selections->node_path_selections->clear();
            if (node_filter_patterns->size() == 0)
                return true; // no user-abort

            findSelectedNodes(m_stage->GetPseudoRoot(), *node_filter_patterns, *node_selections->node_path_selections, debug_archive);
            //std::cout << "  selection_paths=" << selection_paths->size() << std::endl;

            return 0; // success
        }
    }

    // Don't throw an error on an unrecognized target:
    if (debug())
        std::cerr << "FuserUsdArchiveIO: warning, cannot handle target type '" << target_name << "', ignoring." << std::endl;

    return 0; // no user-abort
}


//-------------------------------------------------------------------------------


} // namespace Fsr

// end of UsdArchiveIO.cpp

//
// Copyright 2019 DreamWorks Animation
//
