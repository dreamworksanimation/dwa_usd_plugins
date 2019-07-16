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

/// @file Fuser/Mesh.h
///
/// @author Jonathan Egstad

#ifndef Fuser_MeshPrimitive_h
#define Fuser_MeshPrimitive_h

#include "PointBasedPrimitive.h"
#include "HalfEdge.h"
#include "RayContext.h" // for intersection tests


namespace Fsr {

//----------------------------------------------------------------------------------

/*!
*/
class FSR_EXPORT MeshPrimitive : public Fsr::PointBasedPrimitive
{
  protected:
    // These are stored in separate lists so they can be passed as separate contiguous arrays:
    Fsr::UintList     m_num_verts_per_face;     //!< Number of verts per face
    Fsr::UintList     m_vert_start_per_face;    //!< Starting vert index, per face

    //! Optional edge info, stored separately so the memory can be released when not required
    Fsr::HalfEdgeList m_edge_list;

    Fsr::Node*        m_tessellator;            //!< Tessellator to execute when subdividing


  protected:
    //! Required method to support DD::Image::Primitive::duplicate()
    void copy(const MeshPrimitive* b);


    /*! These are called from the hijacked DD::Image::Primitive
        calls to fill in a VertexBuffer.
    */
    /*virtual*/ void _drawWireframe(DD::Image::ViewerContext* vtx,
                                    const DD::Image::GeoInfo& info,
                                    VertexBuffers&            vbuffers);
    /*virtual*/ void _drawSolid(DD::Image::ViewerContext* vtx,
                                const DD::Image::GeoInfo& info,
                                VertexBuffers&            vbuffers);


  public:
    /*! Node execution context structure passed as target data to Fsr::Node::execute()
        methods, containing mesh tessellation data.

        src_mesh is the non-tessellated source mesh.

        TODO: this is temp standin until we combine VertexBuffers, MeshPrimitive, and MeshSample classes
    */
    struct FSR_EXPORT TessellateContext
    {
        static const char* name; // "MeshPrimitiveTessellate"

        const MeshPrimitive* mesh;
        VertexBuffers*       vbuffers;

        TessellateContext(const MeshPrimitive* _mesh,
                          VertexBuffers*       _vbuffers) :
            mesh(_mesh),
            vbuffers(_vbuffers)
        {
            //
        }
    };


    // TODO: this is temp standin until we combine VertexBuffers, MeshPrimitive, and MeshSample classes
    struct FSR_EXPORT TessellateContext2
    {
        static const char* name; // "MeshPrimitiveTessellate2"

        Fsr::UintList*  vertsPerFace;
        Fsr::Vec3fList*    P;
        Fsr::UintList*  Pidx;
        Fsr::Vec3fList*    N;
        Fsr::Vec4fList*   UV;
        Fsr::Vec4fList*   Cf;
        Fsr::Vec3fList*  VEL;

        TessellateContext2() : vertsPerFace(NULL), P(NULL), Pidx(NULL), N(NULL), UV(NULL), Cf(NULL), VEL(NULL) {}
    };


  public:
    //! Copy ctor to support DD::Image::Primitive::duplicate()
    MeshPrimitive(const MeshPrimitive& b);

    //! Fsr::Node::create() entry point. Ignores parent.
    MeshPrimitive(const Fsr::ArgSet& args,
                  Fsr::Node*         parent);

    //!
    MeshPrimitive(const Fsr::ArgSet& args,
                  double             frame,
                  size_t             nVerts,
                  const uint32_t*    vertIndices,
                  size_t             nFaces,
                  const uint32_t*    nVertsPerFace);

    //!
    MeshPrimitive(const Fsr::ArgSet&   args,
                  double               frame,
                  const Fsr::UintList& vertIndices,
                  const Fsr::UintList& nVertsPerFace);

    //! Must have a virtual destructor!
    virtual ~MeshPrimitive() {}


    //! For create() method to instantiate this node by name.
    static const Fsr::Node::Description description;
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return description.fuserNodeClass(); }


    /*virtual*/ size_t   numFaces() const { return m_num_verts_per_face.size(); }
    /*virtual*/ uint32_t numFaceVerts(uint32_t face) const { return m_num_verts_per_face[face]; }
    size_t               numEdges() const { return m_edge_list.size(); }


    //! Number of verts per face list.
    const Fsr::UintList&     numVertsPerFace() const { return m_num_verts_per_face; }

    //! Starting vert index, per face list.
    const Fsr::UintList&     vertStartPerFace() const { return m_vert_start_per_face; }

    //! HalfEdge list, one per vertex. Only filled if buildEdges() has been called.
    const Fsr::HalfEdgeList& edgeList()         const { return m_edge_list; }


    /*! Add one or more faces to the mesh.
        Will clear the existing edge list but not automatically create a new one.
        Call buildEdges() when finished adding faces to the mesh if you need them.
    */
    void     addFace(uint32_t        nFaceVerts,
                     const uint32_t* vertIndices);
    void     addFaces(size_t          nVerts,
                      const uint32_t* vertIndices,
                      size_t          nFaces,
                      const uint32_t* nVertsPerFace);


    //!
    static bool calcPointNormals(size_t            nPoints,
                                 const Fsr::Vec3f* points,
                                 size_t            nVerts,
                                 const uint32_t*   vert_indices,
                                 size_t            nFaces,
                                 const uint32_t*   verts_per_face,
                                 bool              all_tris,
                                 bool              all_quads,
                                 Fsr::Vec3fList&   point_normals);

    //! Builds normals for the current VertexBuffers state.
    static bool calcVertexBufferNormals(VertexBuffers& vbuffers);

    //!
    bool calcPointNormals(const DD::Image::PointList* point_list,
                          Fsr::Vec3fList&             point_normals) const;
    //!
    bool calcVertexNormals(const DD::Image::PointList* point_list,
                           Fsr::Vec3fList&             vertex_normals) const;



  public:
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    // HalfEdge functionality:
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------

    //! Have the HalfEdge lists been created?
    bool haveEdges() const { return (numFaces() > 0 && numEdges() == numVerts()); }

    //! Build all the HalfEdge references for the mesh.  If they've already been built this returns immediately.
    void buildEdges(bool force=false);

    //! Delete the HalfEdge references.
    void removeEdges() { m_edge_list = std::vector<Fsr::HalfEdge>(); }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    // Face/vert/edge access:
    //
    //  These methods require edge lists which get built automatically, but are
    //  not released automatically afterwards.
    //  Call removeEdges() to release the edge lists.
    //
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------

    //! Get the vertex indices for a face.
    void getFaceVertices(uint32_t       face,
                         Fsr::UintList& verts) const;
    //! Get the edge indices for a face. This is the same as getFaceVertices().
    void getFaceEdges(uint32_t       face,
                      Fsr::UintList& edges) const;

    //! Fill a vector with a list of edges (HalfEdge structures) for the given vertex.
    void getVertexEdges(uint32_t       vert,
                        Fsr::UintList& edges);

    //! Fill a vector with a list of vertices that connect to the given vertex.
    void getVertexConnectedVerts(uint32_t       vert,
                                 Fsr::UintList& verts);

    //! Fill a vector with a list of faces that connect to the given vertex.
    void getVertexConnectedFaces(uint32_t       vert,
                                 Fsr::UintList& faces);

    //! Fill a vector with a list of faces that connect with the the given face.
    void getFaceConnectedFaces(uint32_t       face,
                               Fsr::UintList& connected_faces);


  public:
    //! Intersect a ray with a face. If hit t contains distance to face.
    virtual bool intersectFace(const Fsr::RayContext& Rtx,
                               uint32_t               face,
                               const Fsr::Vec3fList&  points,
                               double&                t);


    /*====================================================*/
    /*                      Rendering                     */
    /*====================================================*/

    //! Fill in the VertexBuffers with the attribute values from this Primitive's GeoInfo attributes.
    /*virtual*/ void fillVertexBuffers(DD::Image::PrimitiveContext* ptx,
                                       DD::Image::Scene*            render_scene,
                                       VertexBuffers&               vbuffers) const;


  public:
    //-------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------
    // DD::Image::Primitive virtual methods:
    //-------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------

    //!
    /*virtual*/ const char* Class() const { return fuserNodeClass(); }

    //!
    /*virtual*/ DD::Image::Primitive* duplicate() const { return new MeshPrimitive(*this); }

    //!
    /*virtual*/ DD::Image::PrimitiveType getPrimitiveType() const;

    //!
    /*virtual*/ void print_info() const;


    /*====================================================*/
    /*                 Face handling                      */
    /*====================================================*/

    //! Returns the number of sub faces this primitive generates.
    /*virtual*/ unsigned faces() const { return (unsigned)numFaces(); }

    //! Does this face in this primitive reference this vertex?
    /*virtual*/ bool faceUsesVertex(unsigned face,
                                    unsigned vert) const;

    /*! Returns the local-transformed (m_xform applied) xyz center average of a face
        and its aabb bbox.
        This is only used afaict for BVH construction by DDImage for OpenGL selection.
    */
    /*virtual*/ DD::Image::Vector3 average_center_and_bounds(int                         face,
                                                             const DD::Image::PointList* point_list,
                                                             DD::Image::Vector3&         min,
                                                             DD::Image::Vector3&         max) const;

    //! Returns the xyz center average of the sub face.
    /*virtual*/ DD::Image::Vector3 face_average_center(int                         face,
                                                       const DD::Image::PointList* point_list) const;

    //! Returns the number of vertices for the sub face.
    /*virtual*/ unsigned face_vertices(int face) const { return (int)numFaceVerts(face); }

    //! Fill the pre-allocated array with vertices constituting the sub face.
    /*virtual*/ void get_face_vertices(int       face,
                                       unsigned* array) const;

    //! Returns the normal for face.
    /*virtual*/ DD::Image::Vector3 get_face_normal(int                         face,
                                                   const DD::Image::PointList* point_list) const;

    //! Return the geometric normal for vertex.
    /*virtual*/ DD::Image::Vector3 get_geometric_normal(int                         vert,
                                                        const DD::Image::PointList* point_list) const;

    //! Return the number of faces that vertex connects to and fills in the list of face indices.
    /*virtual*/ int get_vertex_faces(int               vert,
                                     std::vector<int>& faces) const;

    /*! Vector3 Primitive::vertex_normal(unsigned v, const PointList*) const
        Returns a normal that best represents the normal at \b point's
        location on the primitive.  Base class ignores the vertex argument and
        returns the primitive's base normal.
    */
    /*virtual*/ DD::Image::Vector3 vertex_normal(unsigned                    vert,
                                                 const DD::Image::PointList* point_list) const;
#if 0
    /*virtual*/ void vertex_shader(int vert,
                                   DD::Image::Scene*,
                                   DD::Image::PrimitiveContext*,
                                   DD::Image::VArray& out,
                                   const DD::Image::Vector3* normal=NULL) const;

    //! As above, but uses an existing VertexContext rather than making a temporary one.
    /*virtual*/ void vertex_shader(int vert,
                                   DD::Image::Scene*,
                                   DD::Image::PrimitiveContext*,
                                   DD::Image::VertexContext&,
                                   DD::Image::VArray& out,
                                   const DD::Image::Vector3* normal=NULL) const;
#endif


    /*====================================================*/
    /*             OpenGL drawing methods                 */
    /*====================================================*/
    /*virtual*/ void draw_wireframe(DD::Image::ViewerContext*,
                                    DD::Image::PrimitiveContext*,
                                    DD::Image::Primitive*) const;
    /*virtual*/ void draw_solid(DD::Image::ViewerContext*,
                                DD::Image::PrimitiveContext*,
                                DD::Image::Primitive*) const;
    /*virtual*/ void draw_solid_face(int face,
                                     DD::Image::ViewerContext*,
                                     DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_primitive_normal(DD::Image::ViewerContext*,
                                           DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_primitive_num(DD::Image::ViewerContext*,
                                        DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_vertex_num(DD::Image::ViewerContext*,
                                     DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_vertex_normals(DD::Image::ViewerContext*,
                                         DD::Image::PrimitiveContext*) const;


    /*====================================================*/
    /*        DD::Image::Ray intersection test            */
    /*====================================================*/

    //! Test for the intersection of this primitive face with a given ray.
    /*virtual*/ bool IntersectsRay(const DD::Image::Ray&       ray,
                                   int                         face,
                                   const DD::Image::PointList* point_list,
                                   DD::Image::CollisionResult* result) const;


};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline void
MeshPrimitive::buildEdges(bool force)
{
    if (force || !haveEdges())
        HalfEdge::buildEdges(m_num_verts_per_face,
                             m_vert_start_per_face,
                             reinterpret_cast<Fsr::UintList&>(vertex_),
                             m_edge_list);
}

/*virtual*/ inline void
MeshPrimitive::get_face_vertices(int       face,
                                 unsigned* array) const
{
#if DEBUG
    if (face < 0 || face >= (int)numFaces())
        return;
#endif
    uint32_t v = m_vert_start_per_face[face];
    const uint32_t last_vert = v + m_num_verts_per_face[face];
    for (; v < last_vert; ++v)
        *array++ = v;
}

inline void
MeshPrimitive::getFaceVertices(uint32_t       face,
                               Fsr::UintList& verts) const
{
#if DEBUG
    if (face >= (uint32_t)numFaces())
    {
        verts.clear();
        return;
    }
#endif
    const uint32_t nFaceVerts = m_num_verts_per_face[face];
    const uint32_t vstart = m_vert_start_per_face[face];
    verts.resize(nFaceVerts);
    for (uint32_t i=0; i < nFaceVerts; ++i)
        verts[i] = vstart+i;

}
inline void
MeshPrimitive::getFaceEdges(uint32_t       face,
                            Fsr::UintList& edges) const
{
    getFaceVertices(face, edges); // verts and edges are same indices
}

/*virtual*/ inline bool
MeshPrimitive::faceUsesVertex(unsigned face,
                              unsigned vert) const
{
#if DEBUG
    if (face >= (unsigned)numFaces())
        return false;
#endif
    //std::cout << "faceUsesVertex(" << face << ", " << vert << ")" << std::endl;
    const uint32_t vstart = m_vert_start_per_face[face];
    return (vert >= vstart && vert < (vstart + m_num_verts_per_face[face]));
}

inline void
MeshPrimitive::getVertexConnectedVerts(uint32_t       vert,
                                       Fsr::UintList& verts)
{
#if DEBUG
    assert(vert < (uint32_t)numVerts());
#endif
   buildEdges(false/*force*/);
   Fsr::HalfEdge::getVertexConnectedVerts(m_edge_list, vert, verts);
}

inline void
MeshPrimitive::getVertexConnectedFaces(uint32_t       vert,
                                       Fsr::UintList& faces)
{
#if DEBUG
    assert(vert < (uint32_t)numVerts());
#endif
   buildEdges(false/*force*/);
   Fsr::HalfEdge::getVertexConnectedFaces(m_edge_list, vert, faces);
}

inline void
MeshPrimitive::getVertexEdges(uint32_t       vert,
                              Fsr::UintList& edges)
{
#if DEBUG
    assert(vert < (uint32_t)numVerts());
#endif
   buildEdges(false/*force*/);
   Fsr::HalfEdge::getVertexOutgoingEdges(m_edge_list, vert, edges);
}

inline void
MeshPrimitive::getFaceConnectedFaces(uint32_t       face,
                                     Fsr::UintList& connected_faces)
{
#if DEBUG
    assert(face < (uint32_t)numFaces());
#endif
    connected_faces.clear(); // make sure it's empty

    // Find the list of half-edges for this face:
    Fsr::UintList edges;
    getFaceEdges(face, edges);
    if (edges.size() == 0)
        return; // bail quick if no edges

    // Iterate over edges adding connected faces:
    const size_t nEdges = edges.size();
    for (size_t e=0; e < nEdges; ++e)
    {
        const uint32_t heIndex = edges[e];
        if (HalfEdge::isBoundaryEdge(heIndex))
            continue;
        const Fsr::HalfEdge& he = m_edge_list[heIndex];
        if (he.isBoundaryEdge())
            continue;
        const Fsr::HalfEdge& heTwin = m_edge_list[he.twin];
        if (heTwin.twin == heIndex)
            connected_faces.push_back(heTwin.face);
    }
}


} // namespace Fsr

#endif

// end of Fuser/MeshPrimitive.h

//
// Copyright 2019 DreamWorks Animation
//
