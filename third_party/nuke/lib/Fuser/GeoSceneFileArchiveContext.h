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

/// @file Fuser/GeoSceneFileArchiveContext.h
///
/// @author Jonathan Egstad

#ifndef Fuser_GeoSceneFileArchiveContext_h
#define Fuser_GeoSceneFileArchiveContext_h

#include "Node.h" // for NodeFilterPatternList, string, vector

#include <DDImage/Hash.h>

//TODO: add cleanup code so we can delete old GeoSceneFileArchiveContext that
//      haven't been used for a while since there's no practical way to
//      know if Nuke still needs these based on ctor/dtor behavior of the
//      GeoReader class - ie. we can't use simple reference counting.
// Uncomment this to try out cleanup code:
//#define TRY_CONTEXT_CLEANUP

#ifdef TRY_CONTEXT_CLEANUP
#  include <sys/time.h>
#endif


namespace Fsr {


/*! Fuser Node execution context structure passed as target data.

    This is primarily used for GeoOp geometry_engine() since we often need
    to repeatedly access a scene file archive loading points, attribs, etc
    for multiple frames.

    This is *not* normally used for SceneLoader since scene objects are
    infrequently loaded in the main thread.
*/
class FSR_EXPORT GeoSceneFileArchiveContext
{
  public:
    static const char* name; // "GeoSceneFileArchiveContext" (defined in Node.cpp)

    std::string                 scene_file;             //!< File path to scene
    std::string                 scene_context_name;     //!< Arbitrary name for this context
    //
    Fsr::NodeFilterPatternList  node_filter_patterns;   //!< List of filter expressions for node matching
    DD::Image::Hash             node_filter_hash;       //!< Hash value of filter masks
    //
    std::vector<std::string>    populate_path_masks;    //!< Archive path population mask patterns
    //
    std::set<std::string>       selected_paths;         //!< List of enabled node paths
    DD::Image::Hash             selected_paths_hash;    //!< Hash values of selected paths
    //
    std::string                 archive_context_id;     //!< Archive context identifier string
    DD::Image::Hash             archive_context_hash;   //!< Hash value for archive context
    //
    void*                       cache_data;             //!< Unmanaged pointer to arbitrary subclass data


  public:
#ifdef TRY_CONTEXT_CLEANUP
    struct   timeval creation_time;         //!< When context was created
    struct   timeval last_access_time;      //!< When context was last accessed
#endif

    uint32_t global_topology_variance;      //!< Union of all object TopologyVariances


  public:
    //!
    GeoSceneFileArchiveContext();

    //!
    virtual ~GeoSceneFileArchiveContext() {}


    //! Find a archive context with a matching hash value.
    static GeoSceneFileArchiveContext* findArchiveContext(uint64_t hash);


    //! This does not check if there's an existing context with the same hash!
    static void addArchiveContext(GeoSceneFileArchiveContext* context,
                                  uint64_t                    hash);


#ifdef TRY_CONTEXT_CLEANUP
    //!
    void updateAccessTime();

    //!
    double getTimeSinceLastAccess();
#endif



};


} // namespace Fsr


#endif

// end of GeoSceneFileArchiveContext.h


//
// Copyright 2019 DreamWorks Animation
//
