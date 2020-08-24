//
// Copyright 2020 DreamWorks Animation
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

/// @file zprender/Mesh.h
///
/// @author Jonathan Egstad


#ifndef zprender_Mesh_h
#define zprender_Mesh_h

#include "RenderPrimitive.h"
#include "Traceable.h"
#include "Bvh.h"


namespace Fsr { class ArgSet; }


namespace zpr {

// zpr::Mesh enumeration used for SurfaceIntersection::object_type:
static const uint32_t  ZprMeshPrim  =  121;


struct FaceIndex
{
    uint32_t face;
    uint32_t subtri;

    FaceIndex() {}
    FaceIndex(uint32_t _face,
              uint32_t _subtri) : face(_face), subtri(_subtri) {}
};


typedef Bvh<FaceIndex>       FaceIndexBvh;
typedef BvhObjRef<FaceIndex> FaceIndexRef;



/*! Simple Mesh RenderPrimitive.
    Only supports homogeneous-topology motionblur, ie the topology
    cannot change in time.

    TODO: deprecate quad/poly support in favor of converting to tris in
          ctor which means duplicating some vertices and vertice data, but
          it means we can discard the vert_start_per_face list and FaceIndex
          refs become just a uint32_t, so it mostly is a wash.

*/
class ZPR_EXPORT Mesh : public Traceable,
                        public RenderPrimitive
{
  public:
    /*! Attribute counts must match for all motion samples.
        TODO: support a list of arbitrary attributes.
    */
    struct Sample
    {
        Fsr::Vec3fList P_list;          //!< Per-point world-space positions list
        Fsr::Vec3fList N_list;          //!< Per-vertex world-space normals list
        Fsr::Box3f     bbox;            //!< Derived bbox of all points
    };
    typedef std::vector<Sample> SampleList;

    // Per motion-sample data, public so it can be tweaked after construction if need be:
    SampleList  m_motion_meshes;        //! Per motion sample list of mesh samples


  protected:
    uint32_t        m_status;           //!< Surface state flags (unexpanded, etc)
    Fsr::Vec3d      m_P_offset;         //!< Positional offset for position data
    uint32_t        m_num_facetris;     //!< Total number of triangles in mesh, from all faces
    bool            m_all_tris;         //!< Is an all-tri mesh?
    bool            m_all_quads;        //!< Is an all-quad mesh?

    // Per-face, non-animating attributes
    Fsr::Uint32List m_vert_start_per_face;  //!< Starting vert, per face (+1 for last face end) - empty if all_tris, or all_quads

    // Per-vertex, non-animating attributes:
    Fsr::Uint32List m_vert_indice_list; //!< Per-vertex world-space point index
    Fsr::Vec2fList  m_UV_list;          //!< Vertex texture coords
    Fsr::Vec4fList  m_Cf_list;          //!< Vertex colors

    // Per motion-step:
    std::vector<FaceIndexBvh>   m_motion_bvhs;  //!< BVH for faces, one per motion-STEP (ie 1 less than motion-samples)


  protected:
    //! Build the BVHs in a thread-safe loop.
    bool expand(const RenderContext& rtx);


    //!
    Fsr::Vec3f getFaceNormal(uint32_t          face,
                             uint32_t          subtri,
                             const Fsr::Vec2f& st,
                             uint32_t          motion_sample=0) const;
    void       getFaceNormal(uint32_t          face,
                             uint32_t          subtri,
                             const Fsr::Vec2f& st,
                             const Fsr::Vec2f& Rxst,
                             const Fsr::Vec2f& Ryst,
                             uint32_t          motion_sample,
                             Fsr::Vec3f&       Nst,
                             Fsr::Vec3f&       NRxst,
                             Fsr::Vec3f&       NRyst) const;

    //!
    Fsr::Vec3f getMBFaceNormal(uint32_t          face,
                               uint32_t          subtri,
                               const Fsr::Vec2f& st,
                               uint32_t          motion_step,
                               float             motion_step_t) const;
    void       getMBFaceNormal(uint32_t          face,
                               uint32_t          subtri,
                               const Fsr::Vec2f& st,
                               const Fsr::Vec2f& Rxst,
                               const Fsr::Vec2f& Ryst,
                               uint32_t          motion_step,
                               float             motion_step_t,
                               Fsr::Vec3f&       Nst,
                               Fsr::Vec3f&       NRxst,
                               Fsr::Vec3f&       NRyst) const;

    //!
    int setTriIntersection(uint32_t             face,
                           uint32_t             subtri,
                           uint32_t             motion_sample,
                           RayShaderContext&    stx,
                           const Fsr::Vec3f&    p0,
                           const Fsr::Vec3f&    p1,
                           const Fsr::Vec3f&    p2,
                           SurfaceIntersection& I) const;
    int setMBTriIntersection(uint32_t             face,
                             uint32_t             subtri,
                             uint32_t             motion_step,
                             float                motion_step_t,
                             RayShaderContext&    stx,
                             const Fsr::Vec3f&    p0,
                             const Fsr::Vec3f&    p1,
                             const Fsr::Vec3f&    p2,
                             SurfaceIntersection& I) const;


  public:
    //! Accepts faces with any number of verts.
    Mesh(SurfaceContext*        stx,
         bool                   enable_subdivision,
         const Fsr::ArgSet&     subd_args,
         const Fsr::DoubleList& motion_times,
         const Fsr::Mat4dList&  motion_xforms,
         uint32_t               numPoints,
         const Fsr::Vec3f**     P_arrays,
         const Fsr::Vec3f**     N_lists,
         uint32_t               numFaces,
         const uint32_t*        vertsPerFace,
         const uint32_t*        vertList,
         const Fsr::Vec2f*      UV_array=NULL,
         const Fsr::Vec4f*      Cf_array=NULL);
    //!
    ~Mesh();


    //! Build the bvh, returns quickly if it's already been built.
    void buildBvh(const RenderContext& rtx,
                  bool                 force=false);


    //! Returns the global origin offset applied to the point data and bvhs.
    const Fsr::Vec3d& getGlobalOffset() const { return m_P_offset; }

    //! Number of faces.
    uint32_t numFaces() const;
    //! Number of face verts.
    uint32_t numVerts() const;
    //! Number of points.
    uint32_t numPoints() const;


    //! Return the world-space bbox for motion sample (no offset to origin.)
    Fsr::Box3d getBBox(uint32_t motion_sample=0) const;
    //! Return the local-space bbox for motion sample (offset to origin.)
    Fsr::Box3f getBBoxLocal(uint32_t motion_sample=0) const;

    //! Return the world-space bbox for a face (no offset to origin.)
    Fsr::Box3d getFaceBBox(uint32_t face,
                           uint32_t motion_sample=0) const;
    //! Return the local-space bbox for a face (offset to origin.)
    Fsr::Box3f getFaceBBoxLocal(uint32_t face,
                                uint32_t motion_sample=0) const;

    //! Return the world-space average center (centroid) of the face (no offset to origin.)
    Fsr::Vec3f getFaceCentroid(uint32_t face,
                               uint32_t motion_sample=0) const;
    //! Return the local-space average center (centroid) of the face (offset to origin.)
    Fsr::Vec3f getFaceCentroidLocal(uint32_t face,
                                    uint32_t motion_sample=0) const;

    //!
    Fsr::Vec3f getFaceGeometricNormal(uint32_t face,
                                      uint32_t motion_sample=0) const;

    //! Get the raw vertex list.
    const Fsr::Uint32List& getVertexList() const { return m_vert_indice_list; }
    //! Get the raw vert start per face list.
    const Fsr::Uint32List& getVertStartPerFaceList() const { return m_vert_start_per_face; }

    //!
    uint32_t getVertex(uint32_t vert) const { return m_vert_indice_list[vert]; }


    //!
    uint32_t getFaceVertStartIndex(uint32_t face) const;
    //!
    uint32_t getFaceNumVerts(uint32_t face) const;
    //!
    void     getFaceVertStartAndNumVerts(uint32_t face,
                                         uint32_t& vert_start,
                                         uint32_t& num_face_verts) const;

    //! Get the vertex indices for a face.
    void getFaceVertices(uint32_t         face,
                         Fsr::Uint32List& verts) const;

    //! Get all the world-space points for this face (no offset to origin.)
    void getFacePoints(uint32_t        face,
                       Fsr::Vec3dList& face_PWs,
                       uint32_t        motion_sample=0) const;
    //! Get all the local-space points for this face (offset to origin.)
    void getFacePointsLocal(uint32_t        face,
                            Fsr::Vec3fList& face_PLs,
                            uint32_t        motion_sample=0) const;

    //! Get all the normals for this face.
    void getFaceNormals(uint32_t        face,
                        Fsr::Vec3fList& face_normals,
                        uint32_t        motion_sample=0) const;


  public:
    /*====================================================*/
    /*               From RenderPrimitive                 */
    /*====================================================*/

    /*virtual*/ const char* getClass() const { return "Mesh"; }

    //! If this is a traceable primitive return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return static_cast<Traceable*>(this); }

    //! Get the AABB for this primitive at an optional shutter time.
    /*virtual*/ Fsr::Box3d getBBoxAtTime(double frame_time);


    //! Interpolate varying vertex attributes at SurfaceIntersection.
    /*virtual*/ void getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                                        const DD::Image::ChannelSet& mask,
                                                        Fsr::Pixel&                  v) const;
    //! Interpolate varying vertex attributes at SurfaceIntersection. This also calculates derivatives.
    /*virtual*/ void getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                                        const DD::Image::ChannelSet& mask,
                                                        Fsr::Pixel&                  v,
                                                        Fsr::Pixel&                  vdu,
                                                        Fsr::Pixel&                  vdv) const;


  public:
    /*====================================================*/
    /*                 From Traceable                     */
    /*====================================================*/

    /*virtual*/ bool intersect(RayShaderContext& stx);
    /*virtual*/ Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&,
                                                              SurfaceIntersection&);
    /*virtual*/ void getIntersections(RayShaderContext&        stx,
                                      SurfaceIntersectionList& I_list,
                                      double&                  tmin,
                                      double&                  tmax);
    /*virtual*/ int intersectLevel(RayShaderContext& stx,
                                   int               level,
                                   int               max_level);
};


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


//===================================================================================
// Vertex/Face handling:
//===================================================================================

/*! Number of faces is the size of the m_vert_start_per_face list - 1, or
    if all_tris it's (m_vert_indice_list.size() / 3), or if
    all_quads it's (m_vert_indice_list.size() / 4).
*/
inline uint32_t
Mesh::numFaces() const
{
    if      (m_all_quads) return (uint32_t)(m_vert_indice_list.size() / 4);
    else if (m_all_tris ) return (uint32_t)(m_vert_indice_list.size() / 3);
    else                  return (uint32_t)(m_vert_start_per_face.size()-1);
}

inline uint32_t
Mesh::numVerts() const { return (uint32_t)m_vert_indice_list.size(); }

inline uint32_t
Mesh::numPoints() const { return (uint32_t)m_motion_meshes[0].P_list.size(); }


inline uint32_t
Mesh::getFaceNumVerts(uint32_t face) const
{
    if      (m_all_quads) return 4;
    else if (m_all_tris ) return 3;
    return (m_vert_start_per_face[face+1] - m_vert_start_per_face[face]);
}

inline uint32_t
Mesh::getFaceVertStartIndex(uint32_t face) const
{
    if      (m_all_quads) return (face*4);
    else if (m_all_tris ) return (face*3);
    else                  return m_vert_start_per_face[face];
}

inline void
Mesh::getFaceVertStartAndNumVerts(uint32_t  face,
                                  uint32_t& vert_start,
                                  uint32_t& num_face_verts) const
{
    if (m_all_quads)
    {
        vert_start = (face*4);
        num_face_verts = 4;
    }
    else if (m_all_tris)
    {
        vert_start = (face*4);
        num_face_verts = 3;
    }
    else
    {
        const uint32_t v0 = m_vert_start_per_face[face];
        vert_start     = v0;
        num_face_verts = (m_vert_start_per_face[face+1] - v0);
    }
}

inline Fsr::Box3f
Mesh::getFaceBBoxLocal(uint32_t face,
                       uint32_t motion_sample) const
{
#if DEBUG
    assert(face < numFaces());
    assert(motion_sample < m_motion_meshes.size());
#endif
    // Return the local bbox
    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
    if (m_all_quads)
    {
        const uint32_t* vp = &m_vert_indice_list[face*4];
        Fsr::Box3f bbox(points[*vp++]); // v0
        bbox.expand(points[*vp++], false/*test_empty*/); // v1
        bbox.expand(points[*vp++], false/*test_empty*/); // v2
        bbox.expand(points[*vp  ], false/*test_empty*/); // v3
        return bbox;
    }
    else if (m_all_tris)
    {
        const uint32_t* vp = &m_vert_indice_list[face*3];
        Fsr::Box3f bbox(points[*vp++]); // v0
        bbox.expand(points[*vp++], false/*test_empty*/); // v1
        bbox.expand(points[*vp  ], false/*test_empty*/); // v2
        return bbox;
    }

    const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
    const uint32_t nFaceVerts = getFaceNumVerts(face);
    Fsr::Box3f bbox(points[*vp++]); // v0
    for (uint32_t i=1; i < nFaceVerts; ++i)
        bbox.expand(points[*vp++], false/*test_empty*/);
    return bbox;
}
inline Fsr::Box3d
Mesh::getFaceBBox(uint32_t face,
                  uint32_t motion_sample) const
{
    // Return the world bbox which is the local bbox + m_P_offset
    const Fsr::Box3f local_bbox = getFaceBBoxLocal(face, motion_sample);
    return Fsr::Box3d(local_bbox.min + m_P_offset,
                      local_bbox.max + m_P_offset);
}

inline Fsr::Vec3f
Mesh::getFaceCentroidLocal(uint32_t face,
                           uint32_t motion_sample) const
{
#if DEBUG
    assert(motion_sample < m_motion_meshes.size());
#endif
    // Return the local-space average center (centroid) of the face
    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;

    if (m_all_quads)
    {
        const uint32_t* vp = &m_vert_indice_list[face*4];
        Fsr::Vec3f c(points[*vp++]); // v0
        c += points[*vp++];          // v1
        c += points[*vp++];          // v2
        c += points[*vp  ];          // v3
        return c*(1.0f/4.0f);
    }
    else if (m_all_tris)
    {
        const uint32_t* vp = &m_vert_indice_list[face*3];
        Fsr::Vec3f c(points[*vp++]); // v0
        c += points[*vp++];          // v1
        c += points[*vp  ];          // v2
        return c*(1.0f/3.0f);
    }

    const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
    const uint32_t nFaceVerts = getFaceNumVerts(face);
    Fsr::Vec3f c(points[*vp++]); // v0
    for (uint32_t i=1; i < nFaceVerts; ++i)
        c += points[*vp++];
    return c*(1.0f/float(nFaceVerts));
}
inline Fsr::Vec3f
Mesh::getFaceCentroid(uint32_t face,
                      uint32_t motion_sample) const
{
    // Return the world-space average center (centroid) of the face
    return getFaceCentroidLocal(face, motion_sample) + m_P_offset;
}

//!
inline Fsr::Vec3f
Mesh::getFaceGeometricNormal(uint32_t face,
                             uint32_t motion_sample) const
{
#if DEBUG
    assert(motion_sample < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;

    if (m_all_quads)
    {
        const uint32_t* vp = &m_vert_indice_list[face*4];
        return zpr::getQuadGeometricNormal(points[vp[0]],
                                           points[vp[1]],
                                           points[vp[2]],
                                           points[vp[3]]); // in Traceable.h
    }
    else if (m_all_tris)
    {
        const uint32_t* vp = &m_vert_indice_list[face*3];
        return zpr::getTriGeometricNormal(points[vp[0]],
                                          points[vp[1]],
                                          points[vp[2]]); // in Traceable.h
    }

    const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
    const uint32_t nFaceVerts = getFaceNumVerts(face);
#if DEBUG
    assert(nFaceVerts >= 3);
#endif
    if (nFaceVerts == 4)
    {
        return zpr::getQuadGeometricNormal(points[vp[0]],
                                           points[vp[1]],
                                           points[vp[2]],
                                           points[vp[3]]); // in Traceable.h
    }
    else if (nFaceVerts == 3)
    {
        return zpr::getTriGeometricNormal(points[vp[0]],
                                          points[vp[1]],
                                          points[vp[2]]); // in Traceable.h
    }

    // Choose the first, second and last verts:
    const Fsr::Vec3f& p0 = points[*vp             ]; // v0
    const Fsr::Vec3f& p1 = points[*vp+1           ]; // v1
    const Fsr::Vec3f& p2 = points[*vp+nFaceVerts-1]; // v2
    Fsr::Vec3f N = (p1 - p0).cross(p2 - p0);
    N.fastNormalize();
    return N;
}


/*! Get the vertex indices for this face.
*/
inline void
Mesh::getFaceVertices(uint32_t  face,
                      Fsr::Uint32List& verts) const
{
    if (m_all_quads)
    {
        verts.resize(4);
        uint32_t v = m_vert_start_per_face[face*4];
        verts[0] = v++;
        verts[1] = v++;
        verts[2] = v++;
        verts[3] = v;
    }
    else if (m_all_tris)
    {
        verts.resize(3);
        uint32_t v = m_vert_start_per_face[face*3];
        verts[0] = v++;
        verts[1] = v++;
        verts[2] = v++;
    }
    else
    {
        const uint32_t nFaceVerts = getFaceNumVerts(face);
        verts.resize(nFaceVerts);
        uint32_t v = m_vert_start_per_face[face];
        for (uint32_t i=0; i < nFaceVerts; ++i)
            verts[i] = v++;
    }
}


/*! Get all the world-space points for this face at once.  This is more efficient than
   getting the points for each vertex separately.
*/
inline void
Mesh::getFacePoints(uint32_t        face,
                    Fsr::Vec3dList& face_PWs,
                    uint32_t        motion_sample) const
{
#if DEBUG
    assert(motion_sample < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
    if (m_all_quads)
    {
        face_PWs.resize(4);
        const uint32_t* vp = &m_vert_indice_list[face*4];
        face_PWs[0] = points[*vp++] + m_P_offset;
        face_PWs[1] = points[*vp++] + m_P_offset;
        face_PWs[2] = points[*vp++] + m_P_offset;
        face_PWs[3] = points[*vp  ] + m_P_offset;
    }
    else if (m_all_tris)
    {
        face_PWs.resize(3);
        const uint32_t* vp = &m_vert_indice_list[face*3];
        face_PWs[0] = points[*vp++] + m_P_offset;
        face_PWs[1] = points[*vp++] + m_P_offset;
        face_PWs[2] = points[*vp  ] + m_P_offset;
    }
    else
    {
        const uint32_t nFaceVerts = getFaceNumVerts(face);
        face_PWs.resize(nFaceVerts);
        const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
        for (uint32_t i=0; i < nFaceVerts; ++i)
            face_PWs[i] = (points[*vp++] + m_P_offset);
    }
}


/*! Get all the local points for this face at once.  This is more efficient than
   getting the points for each vertex separately.
*/
inline void
Mesh::getFacePointsLocal(uint32_t        face,
                         Fsr::Vec3fList& face_PLs,
                         uint32_t        motion_sample) const
{
#if DEBUG
    assert(motion_sample < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
    if (m_all_quads)
    {
        face_PLs.resize(4);
        const uint32_t* vp = &m_vert_indice_list[face*4];
        face_PLs[0] = points[*vp++];
        face_PLs[1] = points[*vp++];
        face_PLs[2] = points[*vp++];
        face_PLs[3] = points[*vp  ];
    }
    else if (m_all_tris)
    {
        face_PLs.resize(3);
        const uint32_t* vp = &m_vert_indice_list[face*3];
        face_PLs[0] = points[*vp++];
        face_PLs[1] = points[*vp++];
        face_PLs[2] = points[*vp  ];
    }
    else
    {
        const uint32_t nFaceVerts = getFaceNumVerts(face);
        face_PLs.resize(nFaceVerts);
        const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
        for (uint32_t i=0; i < nFaceVerts; ++i)
            face_PLs[i] = points[*vp++];
    }
}


/*! Get all the  normals for this face at once.  This is more efficient than
    getting the normals for each vertex separately.

    TODO: test this out!
*/
inline void
Mesh::getFaceNormals(uint32_t        face,
                     Fsr::Vec3fList& face_normals,
                     uint32_t        motion_sample) const
{
#if DEBUG
    assert(motion_sample < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& normals = m_motion_meshes[motion_sample].N_list;
    if (normals.size() == 0)
    {
        // No normals, get geometric normal instead:
        const uint32_t nFaceVerts = getFaceNumVerts(face);
        face_normals.resize(nFaceVerts);
        const Fsr::Vec3f Ng = getFaceGeometricNormal(face, motion_sample);
        for (uint32_t i=0; i < nFaceVerts; ++i)
            face_normals[i] = Ng;
        return;
    }

    if (m_all_quads)
    {
        face_normals.resize(4);
        const uint32_t* vp = &m_vert_indice_list[face*4];
        face_normals[0] = normals[*vp++];
        face_normals[1] = normals[*vp++];
        face_normals[2] = normals[*vp++];
        face_normals[3] = normals[*vp  ];
    }
    else if (m_all_tris)
    {
        face_normals.resize(3);
        const uint32_t* vp = &m_vert_indice_list[face*3];
        face_normals[0] = normals[*vp++];
        face_normals[1] = normals[*vp++];
        face_normals[2] = normals[*vp  ];
    }
    else
    {
        const uint32_t nFaceVerts = getFaceNumVerts(face);
        face_normals.resize(nFaceVerts);
        const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
        for (uint32_t i=0; i < nFaceVerts; ++i)
            face_normals[i] = normals[*vp++];
    }
}


/*! Return the bbox for motion sample.
*/
inline Fsr::Box3d
Mesh::getBBox(uint32_t motion_sample) const
{
    Fsr::Box3d bbox;
    const uint32_t nFaces = numFaces();
    if (nFaces > 0)
    {
        bbox = getFaceBBox(0/*face*/, motion_sample);
        for (uint32_t f=1; f < nFaces; ++f)
            bbox.expand(getFaceBBox(f/*face*/, motion_sample), false/*test_empty*/);
    }
    return bbox;
}


/*! Return the local-space bbox for motion sample.
*/
inline Fsr::Box3f
Mesh::getBBoxLocal(uint32_t motion_sample) const
{
    Fsr::Box3f bbox;
    const uint32_t nFaces = numFaces();
    if (nFaces > 0)
    {
        bbox = getFaceBBoxLocal(0/*face*/, motion_sample);
        for (uint32_t f=1; f < nFaces; ++f)
            bbox.expand(getFaceBBoxLocal(f/*face*/, motion_sample), false/*test_empty*/);
    }
    return bbox;
}


//!
inline Fsr::Vec3f
Mesh::getFaceNormal(uint32_t          face,
                    uint32_t          subtri,
                    const Fsr::Vec2f& st,
                    uint32_t          motion_sample) const
{
#if DEBUG
    assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& normals = m_motion_meshes[motion_sample].N_list;
    if (normals.size() == 0)
    {
        // No normals, get geometric normal instead:
        return getFaceGeometricNormal(face, motion_sample);
    }

    const uint32_t* vp = m_vert_indice_list.data() + getFaceVertStartIndex(face);
    Fsr::Vec3f N = Fsr::interpolateAtBaryCoord(normals[vp[0       ]],
                                               normals[vp[subtri+1]],
                                               normals[vp[subtri+2]],
                                                st);
    N.fastNormalize();
    return N;
}
inline void
Mesh::getFaceNormal(uint32_t          face,
                    uint32_t          subtri,
                    const Fsr::Vec2f& st,
                    const Fsr::Vec2f& Rxst,
                    const Fsr::Vec2f& Ryst,
                    uint32_t          motion_sample,
                    Fsr::Vec3f&       Nst,
                    Fsr::Vec3f&       NRxst,
                    Fsr::Vec3f&       NRyst) const
{
#if DEBUG
    assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& normals = m_motion_meshes[motion_sample].N_list;
    if (normals.size() == 0)
    {
        // No normals, get geometric normal instead:
        Nst = getFaceGeometricNormal(face, motion_sample);
        NRxst = NRyst = Nst;
        return;
    }

    const uint32_t* vp = m_vert_indice_list.data() + getFaceVertStartIndex(face);
    const Fsr::Vec3f& n0 = normals[vp[0       ]];
    const Fsr::Vec3f& n1 = normals[vp[subtri+1]];
    const Fsr::Vec3f& n2 = normals[vp[subtri+2]];
    Nst   = Fsr::interpolateAtBaryCoord(n0, n1, n2, st  );   Nst.fastNormalize();
    NRxst = Fsr::interpolateAtBaryCoord(n0, n1, n2, Rxst); NRxst.fastNormalize();
    NRyst = Fsr::interpolateAtBaryCoord(n0, n1, n2, Ryst); NRyst.fastNormalize();
}


//!
inline Fsr::Vec3f
interpolateNormalAt(const Fsr::Vec3fList& normals0,
                    const Fsr::Vec3fList& normals1,
                    uint32_t              v0,
                    uint32_t              v1,
                    uint32_t              v2,
                    float                 motion_step_t,
                    const Fsr::Vec2f&     st)

{
    const Fsr::Vec3f Ns0 = Fsr::interpolateAtBaryCoord(normals0[v0],
                                                       normals0[v1],
                                                       normals0[v2],
                                                       st);
    const Fsr::Vec3f Ns1 = Fsr::interpolateAtBaryCoord(normals1[v0],
                                                       normals1[v1],
                                                       normals1[v2],
                                                       st);
    Fsr::Vec3f N = Ns0.interpolateTo(Ns1, motion_step_t);
    N.fastNormalize();
    return N;
}

//!
inline Fsr::Vec3f
Mesh::getMBFaceNormal(uint32_t          face,
                      uint32_t          subtri,
                      const Fsr::Vec2f& st,
                      uint32_t          motion_step,
                      float             motion_step_t) const
{
#if DEBUG
    assert(motion_step < m_motion_meshes.size());
    assert((motion_step+1) < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& normals0 = m_motion_meshes[motion_step  ].N_list;
    const Fsr::Vec3fList& normals1 = m_motion_meshes[motion_step+1].N_list;
    if (normals0.size() == 0)
    {
        // No normals, get geometric normal instead:
        const Fsr::Vec3f Ng0 = getFaceGeometricNormal(face, motion_step  );
        const Fsr::Vec3f Ng1 = getFaceGeometricNormal(face, motion_step+1);
        Fsr::Vec3f Ng = Ng0.interpolateTo(Ng1, motion_step_t);
        Ng.fastNormalize();
        return Ng;
    }

    const uint32_t* vp = m_vert_indice_list.data() + getFaceVertStartIndex(face);
    return interpolateNormalAt(normals0, normals1, vp[0], vp[subtri+1], vp[subtri+2], motion_step_t,   st);
}
//!
inline void
Mesh::getMBFaceNormal(uint32_t          face,
                      uint32_t          subtri,
                      const Fsr::Vec2f& st,
                      const Fsr::Vec2f& Rxst,
                      const Fsr::Vec2f& Ryst,
                      uint32_t          motion_step,
                      float             motion_step_t,
                      Fsr::Vec3f&       Nst,
                      Fsr::Vec3f&       NRxst,
                      Fsr::Vec3f&       NRyst) const
{
#if DEBUG
    assert(motion_step < m_motion_meshes.size());
    assert((motion_step+1) < m_motion_meshes.size());
#endif
    const Fsr::Vec3fList& normals0 = m_motion_meshes[motion_step  ].N_list;
    const Fsr::Vec3fList& normals1 = m_motion_meshes[motion_step+1].N_list;
    if (normals0.size() == 0)
    {
        // No normals, get geometric normal instead:
        const Fsr::Vec3f Ng0 = getFaceGeometricNormal(face, motion_step  );
        const Fsr::Vec3f Ng1 = getFaceGeometricNormal(face, motion_step+1);
        Nst = Ng0.interpolateTo(Ng1, motion_step_t);
        Nst.fastNormalize();
        return;
    }

    const uint32_t* vp = m_vert_indice_list.data() + getFaceVertStartIndex(face);
    const uint32_t v0 = vp[0       ];
    const uint32_t v1 = vp[subtri+1];
    const uint32_t v2 = vp[subtri+2];
    Nst   = interpolateNormalAt(normals0, normals1, v0, v1, v2, motion_step_t,   st);
    NRxst = interpolateNormalAt(normals0, normals1, v0, v1, v2, motion_step_t, Rxst);
    NRyst = interpolateNormalAt(normals0, normals1, v0, v1, v2, motion_step_t, Ryst);
}


//!
inline int
Mesh::setTriIntersection(uint32_t             face,
                         uint32_t             subtri,
                         uint32_t             motion_sample,
                         RayShaderContext&    stx,
                         const Fsr::Vec3f&    p0,
                         const Fsr::Vec3f&    p1,
                         const Fsr::Vec3f&    p2,
                         SurfaceIntersection& I) const
{
    I.object        = static_cast<RenderPrimitive*>(const_cast<Mesh*>(this));
    I.object_type   = ZprMeshPrim;
    I.object_ref    = 1;        // one hit
    I.part_index    = face;     // the face index
    I.subpart_index = subtri;   // the subtriangle index
    // TODO: switch to using interpolatAtBaryCoord for PW?
    I.PW            = I.PWg = stx.Rtx.getPositionAt(I.t); // I.t was set in intersectTriangle()
    I.RxPW          = Fsr::interpolateAtBaryCoord(p0, p1, p2, I.Rxst) + m_P_offset;
    I.RyPW          = Fsr::interpolateAtBaryCoord(p0, p1, p2, I.Ryst) + m_P_offset;
    I.Ng            = zpr::getTriGeometricNormal(p0, p1, p2);
    getFaceNormal(face, subtri, I.st, I.Rxst, I.Ryst, motion_sample, I.Ni, I.RxN, I.RyN);
    I.N = I.Ni;

    return Fsr::RAY_INTERSECT_POINT;
}

//!
inline int
Mesh::setMBTriIntersection(uint32_t             face,
                           uint32_t             subtri,
                           uint32_t             motion_step,
                           float                motion_step_t,
                           RayShaderContext&    stx,
                           const Fsr::Vec3f&    p0,
                           const Fsr::Vec3f&    p1,
                           const Fsr::Vec3f&    p2,
                           SurfaceIntersection& I) const
{
    I.object        = static_cast<RenderPrimitive*>(const_cast<Mesh*>(this));
    I.object_type   = ZprMeshPrim;
    I.object_ref    = 1;        // one hit
    I.part_index    = face;     // the face index
    I.subpart_index = subtri;   // the subtriangle index
    // TODO: switch to using interpolatAtBaryCoord for PW?
    I.PW            = I.PWg = stx.Rtx.getPositionAt(I.t); // I.t was set in intersectTriangle()
    I.RxPW          = Fsr::interpolateAtBaryCoord(p0, p1, p2, I.Rxst) + m_P_offset;
    I.RyPW          = Fsr::interpolateAtBaryCoord(p0, p1, p2, I.Ryst) + m_P_offset;
    I.Ng            = zpr::getTriGeometricNormal(p0, p1, p2);
    getMBFaceNormal(face, subtri, I.st, I.Rxst, I.Ryst, motion_step, motion_step_t, I.Ni, I.RxN, I.RyN);
    I.N = I.Ni;

    return Fsr::RAY_INTERSECT_POINT;
}


//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------


} // namespace zpr

#endif

// end of zprender/Mesh.h

//
// Copyright 2020 DreamWorks Animation
//
