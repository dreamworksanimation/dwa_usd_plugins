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

/// @file Fuser/ExecuteTargetContexts.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ExecuteTargetContexts_h
#define Fuser_ExecuteTargetContexts_h

#include "Node.h" // for NodeFilterPatternList, string, vector

#include <DDImage/Hash.h>


namespace DD { namespace Image {
class Op;
class AxisOp;
class GeoOp;
class Iop;
class OutputContext;
class PrimitiveContext;
class ViewerContext;
}}


namespace Fsr {

//
// Node execution context structures passed as target data to Fsr::Node::execute()
// methods.
//
// TODO: these could be moved to other headers like NukeGeoInterface and SceneLoader.h
// TODO: make these part of a registered class that a Fuser op can declare their
//       ability to process, along with Python extensions and the like.
//


//-------------------------------------------------------------------------


/*! Generic scene archive management context used to pass archive directives
    to fsrIO nodes.
*/
struct FSR_EXPORT SceneArchiveContext
{
    static const char* name; // "SceneArchiveContext" (defined in Node.cpp)

    DD::Image::Op* op;        //!< Op being called from (optional)

    //! Ctor sets everything to invalid values.
    SceneArchiveContext() : op(NULL) {}
};


/*! A structure to get/set scene path filters (masks).
    This is a placeholder to provide a place for additional vars to be passed
    besides just the NodeFilterPatternList.
*/
struct FSR_EXPORT ScenePathFilters
{
    static const char* name; // "ScenePathFilters" (defined in Node.cpp)

    Fsr::NodeFilterPatternList* node_filter_patterns;   //!< List of filter expressions for node matching

    //! Ctor sets everything to invalid values.
    ScenePathFilters() : node_filter_patterns(NULL) {}
};


/*! A structure to get/set the catalog of nodes.
    This is a placeholder to provide a place for additional vars to be passed
    besides just the NodeDescriptionMap.
*/
struct FSR_EXPORT SceneNodeDescriptions
{
    static const char* name; // "SceneNodeDescriptions" (defined in Node.cpp)

    Fsr::NodeDescriptionMap*    node_description_map;   //!< Result of matches: key=node.name, value=node.path

    //! Ctor sets everything to invalid values.
    SceneNodeDescriptions() : node_description_map(NULL) {}
};


/*! A structure to get/set a selection set of node paths.
*/
struct FSR_EXPORT SelectedSceneNodePaths
{
    static const char* name; // "SelectedSceneNodePaths" (defined in Node.cpp)

    Fsr::NodePathSelections*    node_path_selections;   //!< List of selected node paths

    //! Ctor sets everything to invalid values.
    SelectedSceneNodePaths() : node_path_selections(NULL) {}
};


//-------------------------------------------------------------------------


/*! 
*/
struct FSR_EXPORT SceneOpImportContext
{
    static const char* name; // "SceneOpImport" (defined in Node.cpp)

    DD::Image::Op*              op;         //!< AxisOp, Iop, GeoOp, CameraOp, LightOp, etc.
    DD::Image::OutputContext    op_ctx;     //!< OutputContext to evaluate Op at

    //! Ctor sets everything to invalid values.
    SceneOpImportContext(DD::Image::Op*                  _op,
                         const DD::Image::OutputContext& _op_ctx) : op(_op), op_ctx(_op_ctx) {}
};


/*! Passed to execute when forwarding the common GL drawing routines on
    the DD::Image::Primtive class.
*/
struct FSR_EXPORT PrimitiveViewerContext
{
    static const char* name; // "drawGL" (defined in Node.cpp)

    DD::Image::ViewerContext*    vtx;
    DD::Image::PrimitiveContext* ptx;

    //! Ctor sets everything to invalid values.
    PrimitiveViewerContext(DD::Image::ViewerContext*    _vtx,
                           DD::Image::PrimitiveContext* _ptx) : vtx(_vtx), ptx(_ptx) {}
};


//-------------------------------------------------------------------------


/*! Node execution context structure passed as target data to Fsr::Node::execute()
    methods, containing generic mesh tessellation data, allowing for n numbers
    of float, Vec2, Vec3, and Vec4 vertex attribs to be passed and interpolated.

    TODO: turn the vert attrib ptrs into Fsr::Attribute ptrs so there's
          more info about each one, like their names. However this much
          info probably not needed for simple subdivision purposes.

*/
struct FSR_EXPORT MeshTessellateContext
{
    static const char* name; // "MeshTessellate"

    Fsr::Uint32List*             verts_per_face;        //!< Face vert count, may not be required
    Fsr::Uint32List*             vert_position_indices; //!< Per-vertex position indices
    bool                         all_quads;             //!< Indicates that all faces are quads
    bool                         all_tris;              //!< Indicates that all faces are triangles
    // Per-point rate:
    std::vector<Fsr::Vec3fList*> position_lists;        //!< Per-point positions data lists (motion samples)
    // Per-vertex rate:
    std::vector<Fsr::FloatList*> vert_float_attribs;    //!< Arbitrary per-vertex float data lists
    std::vector<Fsr::Vec2fList*> vert_vec2_attribs;     //!< Arbitrary per-vertex Fsr::Vec2 data lists
    std::vector<Fsr::Vec3fList*> vert_vec3_attribs;     //!< Arbitrary per-vertex Fsr::Vec3 data lists
    std::vector<Fsr::Vec4fList*> vert_vec4_attribs;     //!< Arbitrary per-vertex Fsr::Vec4 data lists

    MeshTessellateContext() :
        verts_per_face(NULL),
        vert_position_indices(NULL),
        all_quads(false),
        all_tris(false)
    {
        //
    }
};


//-------------------------------------------------------------------------


} // namespace Fsr


#endif

// end of ExecuteTargetContexts.h


//
// Copyright 2019 DreamWorks Animation
//
