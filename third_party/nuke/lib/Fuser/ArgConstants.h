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

/// @file Fuser/ArgConstants.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ArgConstants_h
#define Fuser_ArgConstants_h

#include "api.h"

#include <string>

namespace Fsr {

// We use namespaces to echo the namespacing in the attribute names, so these are
// accessed like so: 'Arg::node_type'
namespace Arg {

//-------------------------------------------------------------------------
// TODO: use a token system like Usd's TfToken to improve arg lookup speed?


// Standard key values the Fuser::Node base class recognizes:

static std::string              node_arg("fsr:node"              ); // To check if it's a Fuser node arg
static std::string             node_type("fsr:node:type"         ); // type desc (same as nodeClass())
static std::string             node_name("fsr:node:name"         ); // unique name
static std::string             node_path("fsr:node:path"         ); // absolute *fuser* node path (NOT the same as scenegraph path!!!)

static std::string        node_directive("fsr:node:directive"    ); // node builder/execution directive

static std::string            node_debug("fsr:node:debug"        ); // debug mode (integer)
static std::string    node_debug_attribs("fsr:node:debug:attribs"); // debug attribs mode (integer)

static std::string      node_error_state("fsr:node:error_state"  ); // last error state (integer)
static std::string        node_error_msg("fsr:node:error_msg"    ); // last error message


//--------------------------------------------------------------------------------------


static std::string         invalid_token("<invalid>"             ); // special token indicating an uninitialized string
static std::string           empty_token("<empty>"               ); // special token indicating an intentionally-empty string


//--------------------------------------------------------------------------------------


// We use namespaces to echo the namespacing in the attribute names, so these are
// accessed like so: 'Arg::Scene::read_debug'
namespace Scene {

static std::string              read_debug("scene:read:debug"           ); // debug mode (bool)

static std::string   decompose_xform_order("scene:decompose_xform_order"); // Preferred decompose xform order
static std::string     decompose_rot_order("scene:decompose_rot_order"  ); // Preferred decompose rotation order

static std::string                T_enable("scene:T_enable"             ); // import translations (bool)
static std::string                R_enable("scene:R_enable"             ); // import rotations (bool)
static std::string                S_enable("scene:S_enable"             ); // import scaling (bool)
static std::string     euler_filter_enable("scene:euler_filter:enable"  ); // apply euler filter to rotations (bool)
static std::string   parent_extract_enable("scene:parent_extract:enable"); // separate parent transform from local (bool)

// Some standard arg names that Fuser I/O Nodes leveraged by NukeSceneImporter recognize:
static std::string                    file("scene:file"                 ); // scene file path

static std::string          file_archive_arg("scene:file:archive"             ); // To check if it's a archive arg
static std::string         file_archive_open("scene:file:archive:open"        ); // Open a scene archive file
static std::string        file_archive_close("scene:file:archive:close"       ); // Close a scene archive file and release cache
static std::string   file_archive_invalidate("scene:file:archive:invalidate"  ); // Invalidate a scene archive file cache
static std::string   file_archive_context_id("scene:file:archive:context:id"  ); // scene archive file context identifier
static std::string file_archive_context_hash("scene:file:archive:context:hash"); // scene archive file context hash
static std::string     file_archive_variance("scene:file:archive:variance"    ); // get the topology variance for the archive
static std::string        file_archive_debug("scene:file:archive:debug"       ); // enable archive handling debugging

static std::string    node_filter_patterns("scene:node:filter:patterns" ); // scene node filter pattern list
static std::string        node_filter_hash("scene:node:filter:hash"     ); // scene node filter id hash
static std::string     node_selection_hash("scene:node:selection:hash"  ); // scene node selection id hash

static std::string                    path("scene:path"                 ); // scene graph path (NOT the same as fuser node path!!)
static std::string          path_max_depth("scene:path:max_depth"       ); // scene graph max depth (integer)

static std::string                node_arg("scene:node"                 ); // To check if it's a Scene node arg
static std::string               node_type("scene:node:type"            ); // node class hint for IO plugin
static std::string   node_find_first_valid("scene:node:find-first-valid"); // Search for the first valid node for the context
static std::string          node_type_auto("scene:node:auto-detect"     ); // Determine node class from node path
static std::string      node_type_contents("scene:node:get-contents"    ); // Get contents at node path

}

//--------------------------------------------------------------------------------------


// We use namespaces to echo the namespacing in the arg names, so these are
// accessed like so: 'Arg::NukeGeo::read_debug'
namespace NukeGeo {

static std::string              read_debug("geo:read_debug"            ); // debug mode (bool)

// Some standard arg names that Fuser I/O Nodes leveraged by FuserGeoReader recognize:
static std::string          node_directive("geo:node"                  ); // To check if it's a Geo node directive
static std::string               node_type("geo:node:type"             ); // node class hint for IO plugin
static std::string   node_find_first_valid("geo:node:find-first-valid" ); // Search for the first valid node for the context
static std::string          node_type_auto("geo:node:auto-detect"      ); // Determine node class from node path
static std::string      node_type_contents("geo:node:get-contents"     ); // Get contents at node path
static std::string  node_topology_variance("geo:node:topology-variance"); // get the topology variance for the node

// The standard key values used to look up GeoInfo source attribute names:
static std::string            object_index("nuke:geo:object_index"     ); // Current object index in GeometryList

// The standard key values used to look up GeoInfo source attribute names:
static std::string              uvs_attrib("nuke:geo:uvs"              );
static std::string          normals_attrib("nuke:geo:normals"          );
static std::string           colors_attrib("nuke:geo:colors"           );
static std::string         velocity_attrib("nuke:geo:velocity"         );


}


//--------------------------------------------------------------------------------------

} // namespace Arg

} // namespace Fsr


#endif

// end of Fuser/ArgConstants.h

//
// Copyright 2019 DreamWorks Animation
//
