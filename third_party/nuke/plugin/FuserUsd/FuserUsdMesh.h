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

/// @file FuserUsdMesh.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdMesh_h
#define FuserUsdMesh_h

#include "FuserUsdXform.h"

#include <Fuser/NukeGeoInterface.h> // for GeoOpGeometryEngineContext

#ifdef DWA_INTERNAL_BUILD
#  include <dw_nuke2.1/rContext.h>
#endif

#include <DDImage/PrimitiveContext.h>
#include <DDImage/Scene.h>


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/usd/usdGeom/mesh.h>

#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*! USD dummy placeholder node for a real geom subset (faceset).
*/
class FuserUsdGeomSubsetNode : public FuserUsdNode
{
  protected:
    Pxr::UsdPrim m_prim;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_prim; }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdGeomSubsetNode"; }

    FuserUsdGeomSubsetNode(const Pxr::UsdStageRefPtr& stage,
                           const Pxr::UsdPrim&        prim,
                           const Fsr::ArgSet&         args,
                           Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent),
        m_prim(prim)
    {
        std::cout << "  FuserUsdGeomSubsetNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;
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


/*! UsdGeomXformable node wrapper.
*/
class FuserUsdMesh : public FuserUsdXform
{
  protected:

    // TODO: move this to Fuser MeshNode abstract class; make this a subclass;
    // add Pxr-specific access methods like getTime() cast to Pxr::UsdTimeCode
    struct MeshSample
    {
        Pxr::UsdTimeCode        time;           //!< Sample time
        uint32_t                id_index;       //!< ID index (arbitrary ID, object index, etc)

        // Global bbox & matrix:
        Fsr::Box3d              bbox;           //!< Derived bbox
        Fsr::Mat4d              matrix;         //!< Derived matrix

        // Mesh info:
        size_t                  nPoints;        //!< ie points.size()
        size_t                  nVerts;         //!< ie vert_indices.size()
        size_t                  nFaces;         //!< ie verts_per_face.size()

        std::vector<Fsr::Vec3f> points;         //!< Local-space point locations

        bool                    all_tris;       //!< Are verts part of an all-tri mesh? Don't need a face list if so.
        bool                    all_quads;      //!< Are verts part of an all-quad mesh? Don't need a face list if so.
        std::vector<uint32_t>   verts_per_face; //!< Per-face vert count

        bool                    cw_winding;     //!< Are mesh faces in clockwise (left-handed) winding order?

        // These are stored in Nuke-natural CCW winding order:
        std::vector<uint32_t>   vert_indices;   //!< Point location indices
        std::vector<Fsr::Vec2f> uvs;            //!< Vertex texture coord (no perspective support!)
        std::vector<Fsr::Vec3f> normals;        //!< Vertex normal
        std::vector<Fsr::Vec4f> colors;         //!< Vertex color (w is opacity)
        std::vector<Fsr::Vec3f> velocities;     //!< Point velocity (TODO: how is this defined?)

        // Subd-specific data:
        std::string             subd_scheme;    //!< Name of subdivision scheme
        uint32_t                subd_level;     //!< Current subd level
        std::vector<uint32_t>   crease_indices; //!< TODO: support! 
        std::vector<float>      crease_weights; //!< TODO: support! 
        std::vector<uint32_t>   corner_indices; //!< TODO: support!
        std::vector<float>      corner_weights; //!< TODO: support!
        std::vector<uint32_t>   holes_indices;  //!< TODO: support!

        const Fsr::Vec3f*       pointLocations() const { return points.data(); }
        const uint32_t*         vertsPerFace()   const { return verts_per_face.data(); }
        const uint32_t*         vertIndices()    const { return vert_indices.data(); }

    };


  protected:
    Pxr::UsdGeomPointBased  m_ptbased_schema;       // Store the PointBased schema (vs. Mesh) for subclasses to access
    uint32_t                m_topology_variance;    //!< Object TopologyVariances
    Fsr::Node*              m_subdivider;           //!< Subdivision provider

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_ptbased_schema.GetPrim(); }


  public:
    /*virtual*/ const char* fuserNodeClass() const { return "UsdMesh"; }

    FuserUsdMesh(const Pxr::UsdStageRefPtr& stage,
                 const Pxr::UsdPrim&        mesh_prim,
                 const Fsr::ArgSet&         args,
                 Fsr::Node*                 parent);

    //!
    /*virtual*/ ~FuserUsdMesh();

    //! Called before execution to allow node to update local data from args.
    /*virtual*/ void _validateState(const Fsr::NodeContext& args,
                                    bool                    for_real);


    //! Return abort (-1) on user-interrupt so processing can be interrupted.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1);

    //! Get vertex normals in Nuke-natural (CCW) order.
    void getVertexNormals(const MeshSample&        mesh,
                          std::vector<Fsr::Vec3f>& normals);

    //! Build vertex normals based on the mesh topology.
    void buildVertexNormals(const MeshSample&        mesh,
                            std::vector<Fsr::Vec3f>& normals);

    //! Get vertex uvs in Nuke-natural (CCW) order.
    void getVertexUVs(const MeshSample&           mesh,
                      std::vector<Fsr::Vec2f>& uvs);

    //! Get vertex colors/opacities in Nuke-natural (CCW) order.
    void getVertexColors(const MeshSample&        mesh,
                         std::vector<Fsr::Vec4f>& Cfs,
                         bool                     get_opacities=true);

    //! Get vertex velocities in Nuke-natural (CCW) order.
    void getVertexVelocities(const MeshSample&        mesh,
                             std::vector<Fsr::Vec3f>& velocities);


    //---------------------------------------------------------
    // TODO: move these to the Fuser MeshNode base class:
    //!
    /*virtual*/ void drawIcons();

    //!
    void drawMesh(DD::Image::ViewerContext*    vtx,
                  DD::Image::PrimitiveContext* ptx,
                  int                          draw_mode);
    //---------------------------------------------------------


  public:
    //! Fill in the mesh context.
    bool initializeMeshSample(MeshSample&    mesh,
                              Fsr::TimeValue time,
                              uint32_t       id_index,
                              int            target_subd_level,
                              bool           get_uvs,
                              bool           get_normals,
                              bool           get_opacities,
                              bool           get_colors=false,
                              bool           get_velocities=false);

    //! Output a USD mesh to a DD::Image::GeometryList GeoInfo.
    void geoOpGeometryEngine(Fsr::GeoOpGeometryEngineContext& geo_ctx);

    //! Output a USD mesh to a DD::Image::Scene render context.
    void tessellateToRenderScene(DD::Image::PrimitiveContext* ptx,
                                 DD::Image::Scene*            render_scene);

#ifdef DWA_INTERNAL_BUILD
    //! Create a zpRender-compatible render primitive.
    void generateRenderPrims(dw::nuke::rContext&       rtx,
                             dw::nuke::ObjectContext*  otx,
                             dw::nuke::SurfaceContext* stx,
                             std::vector<DD::Image::PrimitiveContext>& ptx_list);
#endif

};


} // namespace Fsr

#endif

// end of FuserUsdMesh.h

//
// Copyright 2019 DreamWorks Animation
//
