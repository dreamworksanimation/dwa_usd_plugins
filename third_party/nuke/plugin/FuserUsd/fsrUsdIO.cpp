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

/// @file fsrUsdIO.cpp
///
/// @author Jonathan Egstad
///
/// @brief Fuser plugin to load USD files (.usd)

#include "FuserUsdNode.h"

#include "FuserUsdArchiveIO.h"
#include "FuserUsdCamera.h"
#include "FuserUsdLight.h"
#include "FuserUsdMesh.h"
#include "FuserUsdShader.h"
#include "FuserUsdXform.h"

#ifdef DWA_INTERNAL_BUILD
#  include "FuserUsdStereoRigDWA.h"
#endif

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/base/plug/registry.h>

#  pragma GCC diagnostic pop
#endif

#include <sys/stat.h>


namespace Fsr {


//-------------------------------------------------------------------------------


/*! USD default, or empty node wrapper.

    This node still owns a UsdStageRefPtr so the valid stage can
    be accessed through this, but the node itself has no specific
    known function.

    TODO: move to separate file, finish implementation
*/
class FuserUsdDefaultNode : public FuserUsdNode
{
  protected:
    Pxr::UsdPrim m_prim;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_prim; }


  public:
    /*virtual*/ const char* fuserNodeClass() const { return "UsdDefaultNode"; }

    FuserUsdDefaultNode(const Pxr::UsdStageRefPtr& stage,
                        const Pxr::UsdPrim&        prim,
                        const Fsr::ArgSet&         args,
                        Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent),
        m_prim(prim)
    {
        const bool scene_debug = args.getBool(  Arg::Scene::read_debug, false);
        const bool geo_debug   = args.getBool(Arg::NukeGeo::read_debug, false);

        // Print node info if we don't recognize the name:
        if (scene_debug || geo_debug)
        {
            const std::string& name = prim.GetName().GetString();
            if (name == "Looks")
            {
                // do nothing
            }
            else
            {
                std::cout << "  FuserUsdDefaultNode::ctor(" << this << ") type[" << prim.GetTypeName() << "] '" << prim.GetPath() << "'";
                printPrimAttributes("", prim, false/*verbose*/, std::cout);
                std::cout << std::endl;
            }
        }
    }


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


//-------------------------------------------------------------------------------


/*! UsdGeomScope wrapper.

    USD dummy, or empty node wrapper until we can convert to something intelligent.

    TODO: move to separate file, finish implementation
*/
class FuserUsdGeomScope : public FuserUsdNode
{
  protected:
    Pxr::UsdGeomScope   m_scope_schema;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_scope_schema.GetPrim(); }


  public:
    /*virtual*/ const char* fuserNodeClass() const { return "UsdGeomScope"; }

    FuserUsdGeomScope(const Pxr::UsdStageRefPtr& stage,
                      const Pxr::UsdPrim&        scope_prim,
                      const Fsr::ArgSet&         args,
                      Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent)
    {
        //std::cout << "    FuserUsdGeomScope::ctor(" << this << "): scope'" << scope_prim.GetPath() << "'" << std::endl;

        // Make sure it's a UsdGeomScope:
        if (scope_prim.IsValid() && scope_prim.IsA<Pxr::UsdGeomScope>())
        {
            m_scope_schema = Pxr::UsdGeomScope(scope_prim);
            if (debug())
            {
                printPrimAttributes("  GeomScope", scope_prim, false/*verbose*/, std::cout);
                std::cout << std::endl;
            }
        }
        else
        {
            if (debug())
            {
                std::cerr << "    FuserUsdXform::ctor(" << this << "): ";
                std::cerr << "warning, node '" << scope_prim.GetPath() << "'(" << scope_prim.GetTypeName() << ") ";
                std::cerr << "is invalid or wrong type";
                std::cerr << std::endl;
            }
        }
    }


    //! Called before execution to allow node to update local data from args.
    /*virtual*/ void _validateState(const Fsr::NodeContext& args,
                                    bool                    for_real) {}


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


//-------------------------------------------------------------------------------


/*! A standin for a geometry payload.

    USD dummy, or empty node wrapper until we can convert to something intelligent.

    This node still owns a UsdStageRefPtr so the valid stage can
    be accessed through this, but the node itself has no specific
    known function.

    TODO: move to separate file, finish implementation
*/
class FuserUsdGeoOpaquePayload : public FuserUsdNode
{
  protected:
    Pxr::UsdPrim m_prim;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_prim; }


  public:
    /*virtual*/ const char* fuserNodeClass() const { return "FuserUsdGeoOpaquePayload"; }

    FuserUsdGeoOpaquePayload(const Pxr::UsdStageRefPtr& stage,
                             const Pxr::UsdPrim&        prim,
                             const Fsr::ArgSet&         args,
                             Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent),
        m_prim(prim)
    {
        std::cout << "  FuserUsdGeoOpaquePayload::ctor(" << this << ") type[" << prim.GetTypeName() << "] '" << prim.GetPath() << "'" << std::endl;
    }


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


//-------------------------------------------------------------------------------


/*! Decide what type of FuserUsdNode to create based on args like 'fsrUsdIO:node:class'
    and load modes.

    If the arg doesn't exist or is empty then a special Node that wraps the entire
    archive as child nodes is created.

    If there's an error that should stop the Nuke Op return a Fsr::ErrorNode() since
    this is a static method and there is no current Fsr::Node object.

*/
static
Fsr::Node* buildNode(const char*        builder_class,
                     const Fsr::ArgSet& args,
                     Fsr::Node*         parent)
{
    // The build directive helps inform this routine what kind of Fuser::Node to
    // create. Some are executed immediately and discarded like FuserUsdArchiveIO
    // while others are created and kept around for reuse:
    std::string        build_directive    = args.getString(Arg::node_directive);
    //
    const std::string& scene_file         = args.getString(Arg::Scene::file);
    //const std::string& archive_context_id   = args.getString(Arg::Scene::file_archive_context_id  );
    const uint64_t     archive_context_hash = args.getHash(Arg::Scene::file_archive_context_hash);
    //
    const std::string& scene_node_path    = args.getString(Arg::Scene::path       );
    const std::string& scene_node_type    = args.getString(Arg::Scene::node_type  );
    //const std::string& geo_node_type      = args.getString(Arg::NukeGeo::node_type);
    //
    //const int          object_index       = args.getInt(Arg::NukeGeo::object_index, -1);
    //
    const bool         scene_debug        = args.getBool(Arg::Scene::read_debug,  false);
    const bool         geo_debug          = args.getBool(Arg::NukeGeo::read_debug, false);
    //
    const bool         debug_archive      = args.getBool(Arg::Scene::file_archive_debug, false);//args.getBool("UsdIO:debug_archive_loading", false);


    if (debug_archive || scene_debug)
        std::cout << "  fsrUsdIO::buildNode('" << build_directive << "') args=[ " << args << " ]" << std::endl;

    // We really need a build_directive to do anything meaningful.
    if (build_directive.empty())
    {
        return new Fsr::ErrorNode(builder_class,
                                  -2,
                                  "fsrUsdIO::buildNode('%s'): "
                                  "warning, missing directive to perform build operation, "
                                  "this is likely a plugin coding error.",
                                  scene_file.c_str());
    }


    // No file to load, bail:
    if (scene_file.empty())
    {
        return new Fsr::ErrorNode(builder_class, -2, "file path empty");
    }
    else
    {
        // TODO: abstract this so non-posix paths can be used:
        struct stat st;
        if (stat(scene_file.c_str(), &st) == -1)
            return new Fsr::ErrorNode(builder_class, -2, "file does not exist");
    }

#ifdef DWA_INTERNAL_BUILD
    // TODO: set this bool via a runtime variable that only gets set when in a show env
    const bool is_show_env = false;

    findAndRegisterDefaultUsdFolios(is_show_env, debug_archive);
#endif

    // First check if we want to manage a stage archive (a stage cache) and
    // create a FuserUsdArchiveIO node which has dedicated methods for this:
    //! Is the node directive token one of the archive management ones?
    if (Fsr::stringStartsWith(build_directive, Arg::Scene::file_archive_arg))
        return new FuserUsdArchiveIO(args); // node should be immediately executed and discarded


    // Get the shared stage reference from the static list keyed to archive_context_hash.
    uint64_t stage_hash = archive_context_hash;

    StageCacheReference* stage_reference = NULL;

    // If stage hash is non-default check for an existing stage cache:
    if (stage_hash != Fsr::defaultHashValue)
    {
        stage_reference = StageCacheReference::findStageReference(stage_hash);

        if (debug_archive && stage_reference)
        {
            std::cout << "      existing stage_reference for stage hash 0x" << std::hex << stage_hash << std::dec;
            std::cout << ", populate mask='" << stage_reference->populateMask() << "'";
            std::cout << ", stageid='" << stage_reference->stageId() << "'" << std::endl;
        }
    }

    if (!stage_reference)
    {
        // Existing stage reference not found or empty cache hash, try to create a new one:
        DD::Image::Hash new_stage_hash;
        new_stage_hash.append(scene_file);

        std::vector<std::string> populate_mask_paths;

        // If an explicit scene node path is declared use that instead of
        // a node pattern:
        // TODO: make the pattern & scene node path be the same thing!!
        //       ie. use node_filter_patterns always for this, or support patterns
        //       in scene path.
        if (!scene_node_path.empty() && scene_node_path[0] == '/')
        {
            // TODO: move this logic to the FuserUsdArchiveIO class!
            std::string parent_path;
            if (scene_node_path.size() == 1)
            {
                parent_path = "/"; // default to all
            }
            else
            {
                std::vector<std::string> segments; segments.reserve(10);
                Fsr::stringSplit(scene_node_path, "/", segments);

                for (size_t i=0; i < segments.size(); ++i)
                {
                    const std::string& segment = segments[i];
                    //std::cout << "    " << i << ":'" << segment << "'" << std::endl;
                    if (segment.find_first_of("*?") != std::string::npos)
                    {
                        if (i == 0)
                            parent_path = '/';
                        break;
                    }
                    parent_path += '/';
                    parent_path += segment;
                }
            }
            populate_mask_paths.push_back(parent_path);
            new_stage_hash.append(parent_path);

            if (debug_archive)
                std::cout << "    (scene_node_path '" << parent_path << "') stage_hash=" << std::hex  << new_stage_hash.value() << std::dec;

            stage_reference = StageCacheReference::createStageReference(new_stage_hash.value(), populate_mask_paths);
            stage_hash      = new_stage_hash.value();
        }

        // If no explict node path see if there's filter patterns provided:
        if (!stage_reference && args.has(Arg::Scene::node_filter_patterns))
        {
            // Extract node pattern masks from argument:
            const std::string& patterns_arg = args[Arg::Scene::node_filter_patterns];
            std::vector<std::string> patterns;
            Fsr::stringSplit(patterns_arg, ",", patterns);

            // TODO: what's the state of this code? Is this done or do we need to
            // add the pattern strings to the populate_mask_paths...?

                //new_stage_hash.append(pattern);

            // TODO: call the above logic in FuserUsdArchiveIO class!
            if (debug_archive)
                std::cout << "    (node_filter_patterns) new_stage_hash=" << std::hex  << new_stage_hash.value() << std::dec << std::endl;

            // Create with an empty populate_mask:
            populate_mask_paths.clear();
            stage_reference = StageCacheReference::createStageReference(new_stage_hash.value(), populate_mask_paths);
            stage_hash      = new_stage_hash.value();
        }

        if (debug_archive && stage_reference)
        {
            std::cout << "      new populate mask='" << stage_reference->populateMask() << "'";
            std::cout << ", stageid='" << stage_reference->stageId() << "'" << std::endl;
        }
    }

    // Error, no node paths found or constructed!
    if (!stage_reference)
        return new Fsr::ErrorNode(builder_class, -2, "no node paths found");

    // Get the new or cached stage:
    Pxr::UsdStageRefPtr stage = stage_reference->getStage(scene_file,
                                                          stage_hash,
                                                          debug_archive);
    // Error, no valid stage!
    if (!stage)
        return new Fsr::ErrorNode(builder_class, -2, "USD Stage could not be acquired, unable to load node(s)");


#if 0
    // TODO: do we need the timecode info from the stage for anything?
    const double stage_start_timecode = stage->GetStartTimeCode();
    const double stage_end_timecode   = stage->GetEndTimeCode();
    //std::cout << "    stage timecode=" << stage_start_timecode << ", " << stage_end_timecode << std::endl;
#endif


    // If no node build directive then change it to auto-detect which
    // will attempt to determine node type based on node path:
    if (build_directive.empty())
        build_directive = Arg::Scene::node_type_auto;


    if (build_directive == Arg::Scene::node_type_contents)
    {
        //------------------------------------------------------------------
        //
        // 'build_directive' == 'get-contents'
        // Read the contents of the scene file starting at 'scene:path'
        //
        //------------------------------------------------------------------
        if (scene_debug)
            std::cout << "    scene:get contents at path '" << scene_node_path << "'" << std::endl;
        else if (geo_debug)
            std::cout << "    geo:get contents at path '" << scene_node_path << "'" << std::endl;

        return new FuserUsdArchiveIO(stage, args); // node should be immediately executed and discarded

    }
    else if (build_directive == Arg::Scene::node_find_first_valid)
    {
        //------------------------------------------------------------------
        //
        // Search for the first valid node.
        // If scene_node_type is defined then that specific node type
        // is used for the search.
        //
        //------------------------------------------------------------------

        if (scene_debug)
            std::cout << "    scene:find first valid node of type '" << scene_node_type << "'" << std::endl;

        return new FuserUsdArchiveIO(stage, args); // node should be immediately executed and discarded

    }
    else if (build_directive == Arg::Scene::node_type_auto)
    {
        //------------------------------------------------------------------
        //
        // Auto-detect node class from the node path
        //
        // If there's a valid Stage and node path use the UsdPrim's class
        // to determine which FuserUsdNode type to instantiate.
        //
        //------------------------------------------------------------------
        if (scene_debug)
            std::cout << "    scene:auto-detect node at path '" << scene_node_path << "'" << std::endl;

        //Pxr::SdfPathSet loadable_paths = stage->FindLoadable();
        //std::cout << "      loadable_paths=" << loadable_paths.size() << std::endl;
        //for (Pxr::SdfPathSet::const_iterator it=loadable_paths.begin(); it != loadable_paths.end(); ++it)
        //    std::cout << "      '" << *it << "'" << std::endl;

        const Pxr::UsdPrim prim = stage->GetPrimAtPath(Pxr::SdfPath(scene_node_path));
        if (!prim.IsValid())
        {
            if (geo_debug)
            {
                static std::mutex print_lock; print_lock.lock();
                std::cout << "      '" << scene_node_path << "': IsValid=" << prim.IsValid() << " - skipping" << std::endl;
                print_lock.unlock();
            }
            return new Fsr::ErrorNode(builder_class, -2, "could not load null prim '%s'", scene_node_path.c_str());
        }

        if (!prim.IsLoaded())
        {
            if (scene_debug)
            {
                static std::mutex print_lock; print_lock.lock();
                std::cout << "      prim.IsLoaded=" << prim.IsLoaded() << " ... LOADING NOW!" << std::endl;
                print_lock.unlock();
            }

            // Load the prim here. LoadWithoutDescendents means load the parents of
            // this node and the node itself, but not any children:
            prim.Load(Pxr::UsdLoadWithoutDescendants/*Pxr::UsdLoadWithDescendants*/);
            if (!prim.IsLoaded())
                return new Fsr::ErrorNode(builder_class,
                                          -2,
                                          "prim '%s' could not be Loaded() for an unknown USD reason.",
                                          prim.GetName().GetString().c_str());
        }

        if (!prim.IsDefined())
        {
            // Hard to debug unless this prints an error:
            if (1)//(scene_debug)
            {
                std::cerr << "fsrUsdIO::buildNode('" << scene_node_path << "'): <Scene> ";
                std::cerr << "error, could not load undefined USD prim type <" << prim.GetTypeName() << ">";
                std::cerr << ", ignored" << std::endl;
            }
            return new Fsr::ErrorNode(builder_class,
                                      -2,
                                      "could not load undefined prim '%s' of type '%s'",
                                      scene_node_path.c_str(),
                                      prim.GetTypeName().GetString().c_str());
        }

        //-----------------------------------------------------------------------
        //
        // We have a valid & loaded UsdPrim, determine its type to figure
        // out what kind of FuserUsdNode to create.
        //
        //-----------------------------------------------------------------------

        /*! Handle these basic scene types for now:
            UsdTyped/
                UsdGeomImageable/
                    * UsdGeomScope *
                    UsdGeomXformable/
                        * UsdGeomCamera *
                        * UsdGeomXform  *
                        * UsdLuxLight   *

        */

#ifdef DWA_INTERNAL_BUILD
        // Check first is prim is part of a CameraRig/StereoRig assembly.
        // TODO: update this to use the StereoRigAPI!
        {
            std::string stereo_rig_name = args.getString("default_stereo_rig");
            if (stereo_rig_name.empty())
                stereo_rig_name = "stereoRig1";
            Pxr::UsdPrim center_cam, rig_root;
            std::vector<Pxr::UsdPrim> rig_cams;
            if (FuserUsdStereoRig::isCameraRig(prim, stereo_rig_name, center_cam, rig_root, rig_cams) &&
                    rig_cams.size() > 0)
                return new FuserUsdStereoRig(stage, center_cam, rig_root, rig_cams, args, parent);
        }
#endif

        if      (0) { /*start of if-else chain*/ }

        // UsdGeomXformable subclasses - check for subclasses first, then the base class:
        else if (prim.IsA<Pxr::UsdGeomCamera>()      ) return new FuserUsdCamera(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxCylinderLight>()) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDiskLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDistantLight>() ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDomeLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxGeometryLight>()) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxRectLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxSphereLight>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        else if (prim.IsA<Pxr::UsdLuxLight>()        ) return new FuserUsdLight(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxLightFilter>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxLightPortal>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        else if (prim.IsA<Pxr::UsdGeomXform>()       ) return new FuserUsdXform(stage, prim, args, parent);

        // This should catch any non-specific types that have a transform we can extract:
        else if (prim.IsA<Pxr::UsdGeomXformable>()   ) return new FuserUsdXform(stage, prim, args, parent);

        // No idea how to handle this prim type. Print a warning in debug mode
        // and return a wrapper node rather than erroring:
        if (scene_debug)
        {
            std::cerr << "fsrUsdIO::buildNode('" << scene_node_path << "'): ";
            std::cerr << "warning, ignoring unsupported USD prim of type '" << prim.GetTypeName() << "'";
            std::cerr << std::endl;
        }
        return new FuserUsdDefaultNode(stage, prim, args, parent);

    } // scene auto-detect
    else if (build_directive == Arg::NukeGeo::node_type_contents)
    {
        //------------------------------------------------------------------
        //
        // 'build_directive' == 'get-contents'
        // Read the contents of the scene file starting at 'scene:path'
        //
        //------------------------------------------------------------------
        if (geo_debug)
            std::cout << "    geo:get contents at path '" << scene_node_path << "'" << std::endl;

        return new FuserUsdArchiveIO(stage, args); // node should be immediately executed and discarded

    }
    else if (build_directive == Arg::NukeGeo::node_type_auto)
    {
        //------------------------------------------------------------------
        //
        // Auto-detect node class from the node path
        //
        // If there's a valid Stage and node path use the UsdPrim's class
        // to determine which FuserUsdNode type to instantiate.
        //
        // TODO: at the moment we only support a few specific types, but perhaps
        //       we can send abstract nodes down the geometry pipeline that just
        //       store attributes.
        //
        //------------------------------------------------------------------

        const Pxr::UsdPrim prim = stage->GetPrimAtPath(Pxr::SdfPath(scene_node_path));
        if (!prim.IsValid())
        {
            if (geo_debug)
            {
                static std::mutex print_lock; print_lock.lock();
                std::cout << "      '" << scene_node_path << "': IsValid=" << prim.IsValid() << " - skipping" << std::endl;
                print_lock.unlock();
            }

            return new Fsr::ErrorNode(builder_class, -2, "could not load null prim '%s'", scene_node_path.c_str());
        }

        if (!prim.IsLoaded())
        {
            if (geo_debug)
            {
                static std::mutex print_lock; print_lock.lock();
                std::cout << "      prim.IsLoaded=" << prim.IsLoaded() << " ... LOADING NOW!" << std::endl;
                print_lock.unlock();
            }

            // Load the prim here. LoadWithoutDescendents means load the parents of
            // this node and the node itself, but not any children:
            prim.Load(Pxr::UsdLoadWithoutDescendants/*Pxr::UsdLoadWithDescendants*/);
            if (!prim.IsLoaded())
                return new Fsr::ErrorNode(builder_class,
                                          -2,
                                          "prim '%s' could not be Loaded() for an unknown USD reason.",
                                          prim.GetName().GetString().c_str());
        }

        if (geo_debug)
        {
            // Lock to make the output print cleanly:
            static std::mutex print_lock; print_lock.lock();

            std::cout << "      '" << scene_node_path << "': ";
            if (prim.GetTypeName() != "")
                std::cout << ", type='" << prim.GetTypeName() << "'";
            std::cout << ", HasPayload=" << prim.HasPayload();
            std::cout << ", IsActive=" << prim.IsActive();
            std::cout << ", IsDefined=" << prim.IsDefined();
            std::cout << ", IsAbstract=" << prim.IsAbstract();
            std::cout << ", isModel=" << prim.IsModel();
            std::cout << ", isGprim=" << prim.IsA<Pxr::UsdGeomGprim>();
            std::cout << ", isScope=" << prim.IsA<Pxr::UsdGeomScope>();

            Pxr::UsdVariantSets variantSets = prim.GetVariantSets();
            std::vector<std::string> names; variantSets.GetNames(&names);
            if (names.size() > 0)
            {
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
            }
            FuserUsdNode::printPrimAttributes(" attribs", prim, false/*verbose*/, std::cout);
            std::cout << std::endl;

            print_lock.unlock();
        }

        if (!prim.IsDefined())
        {
            // Don't skip these types, just add a default placeholder node:
            return new FuserUsdDefaultNode(stage, prim, args, parent);
        }

        // Geometry payloads may have a bounds hints - try to get them:
        bool has_extents_hint = false;
        Fsr::Box3d extents_hint;
        const Pxr::UsdAttribute extentsHint_attrib = prim.GetAttribute(Pxr::UsdGeomTokens->extentsHint);
        if (extentsHint_attrib)
        {
            Pxr::VtVec3fArray v; extentsHint_attrib.Get(&v, Pxr::UsdTimeCode::Default());
            if (v.size() == 2)
            {
                extents_hint.set(v[0][0], v[0][1], v[0][2],
                                 v[1][0], v[1][1], v[1][2]);
                // TODO: this hint may be junk, but since it's indicating a payload
                // point we're always saying we have one:
                has_extents_hint = true;
            }
            //std::cout << "      " << prim.GetPath() << ":  extentsHint" << extents_hint << std::endl;
        }

        //-----------------------------------------------------------------------
        // Ok, we have a valid & loaded UsdPrim.
        // Determine it's type to figure out what kind of FuserUsdNode to create.
        //
        // TODO: at the moment we only support a few types, but perhaps we can
        //       send abstract nodes down the geometry pipeline that just store
        //       attributes.
        //-----------------------------------------------------------------------

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
        if      (0) { /*start of if-else chain*/ }

        // UsdGeomGprim subclasses:
        //else if (prim.IsA<Pxr::UsdGeomCapsule>()     ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomCone>()        ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomCube>()        ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomCylinder>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomSphere>()      ) return new FuserUsdDefaultNode(stage, prim, args, parent);

        // UsdGeomPointBased subclasses:
        else if (has_extents_hint                    ) return new FuserUsdGeoOpaquePayload(stage, prim, args, parent);
        else if (prim.IsA<Pxr::UsdGeomMesh>()        ) return new FuserUsdMesh(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomCurves>()      ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomNurbsPatch>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdGeomPoints>()      ) return new FuserUsdDefaultNode(stage, prim, args, parent);

        // This should catch any non-specific types that have a bbox we can extract:
        else if (prim.IsA<Pxr::UsdGeomScope>()       ) return new FuserUsdGeomScope(stage, prim, args, parent);

        // UsdGeomXformable subclasses - check for subclasses first, then the base class:
        else if (prim.IsA<Pxr::UsdGeomCamera>()      ) return new FuserUsdCamera(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxCylinderLight>()) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDiskLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDistantLight>() ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxDomeLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxGeometryLight>()) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxRectLight>()    ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxSphereLight>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        else if (prim.IsA<Pxr::UsdLuxLight>()        ) return new FuserUsdLight(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxLightFilter>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        //else if (prim.IsA<Pxr::UsdLuxLightPortal>()  ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        else if (prim.IsA<Pxr::UsdGeomXform>()       ) return new FuserUsdXform(stage, prim, args, parent);

        // This should catch any non-specific types that have a transform we can extract:
        else if (prim.IsA<Pxr::UsdGeomXformable>()   ) return new FuserUsdXform(stage, prim, args, parent);

        // UsdShadeNodeGraph subclasses - check for subclasses first, then the base class:
        else if (prim.IsA<Pxr::UsdShadeNodeGraph>()  ) return new FuserUsdShadeNodeGraphNode(stage, prim, args, parent);

        // UsdShadeShader subclasses - check for subclasses first, then the base class:
        else if (prim.IsA<Pxr::UsdShadeShader>()     ) return new FuserUsdShaderNode(stage, prim, args, parent);


        // If it's an abstract UsdPrim we can do some more checking to figure out
        // what's up and if we can handle it:
        if      (prim.GetTypeName() == ""          ) return new FuserUsdDefaultNode(stage, prim, args, parent);
        else if (prim.GetTypeName() == "GeomSubset") return new FuserUsdGeomSubsetNode(stage, prim, args, parent);


        // No idea how to handle this prim type. Print a warning in debug mode
        // and return a wrapper node rather than erroring:
        if (geo_debug)
        {
            std::cerr << "fsrUsdIO::buildNode('" << scene_node_path << "'): ";
            std::cerr << "warning, ignoring unsupported USD prim of type '" << prim.GetTypeName() << "'";
            std::cerr << std::endl;
        }
        return new FuserUsdDefaultNode(stage, prim, args, parent);

    } // geo auto-detect

    // Don't recognize this node build directive - error. Since this is a Node build
    // routine there is no current Node to set an error on so we need to create an
    // ErroNode to return to the create() method:
    return new Fsr::ErrorNode(builder_class, -2/*err-code*/,
                              "unrecognized build directive '%s'. This is likely a plugin coding error.",
                              build_directive.c_str());

} // buildNode()


} // namespace Fsr


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


// Add the file naming variants as separate descriptions to match the
// tcl redirector files:
static const Fsr::Node::Description  registerUsdIONode( "UsdIO", Fsr::buildNode/*ctor*/);
static const Fsr::Node::Description registerUsdaIONode("UsdaIO", Fsr::buildNode/*ctor*/);
static const Fsr::Node::Description registerUsdcIONode("UsdcIO", Fsr::buildNode/*ctor*/);
static const Fsr::Node::Description registerUsdzIONode("UsdzIO", Fsr::buildNode/*ctor*/);


// end of fsrUsdIO.cpp

//
// Copyright 2019 DreamWorks Animation
//
