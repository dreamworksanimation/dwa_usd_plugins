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

// @file Fuser/HalfEdge.h
///
/// @author Jonathan Egstad

#ifndef Fuser_HalfEdge_h
#define Fuser_HalfEdge_h

#include "api.h"

#include <vector>
#include <unordered_map>
#include <iostream>
#if DEBUG
#  include <assert.h>
#endif

namespace Fsr {


/*! HalfEdge connectivity structure with circulator routines.
    Assumes winding order is counter-clockwise.

    A HalfEdge struct is 'owned' by a vertex so there's always one HalfEdge per
    vertex. ie edge_list.size == vertex_list.size.  So an edge's index in
    an edge list is the same its owner vertex's index in the companion vert list.

    -------------------------------
    |v3   <e2    v2|v7   <e2    v6|
    |              |              |
    |            /\|            /\|
    |e3    f0    e1|e3    f2    e1|
    |\/            |\/            |
    |              |              |
    |v0    e0>   v1|v4    e0>   v5|
    -------------------------------
    |v11  <e2   v10|v15  <e2   v14|
    |              |              |
    |              |            /\|
    |e3    f1    e1|e3    f3    e1|
    |\/            |\/            |
    |              |              |
    |v8    e0>   v9|v12   e0>  v13|
    -------------------------------

*/
struct FSR_EXPORT HalfEdge
{
    static GNU_CONST_DECL uint32_t INVALID_VERTEX = 0xffffffff;

    uint32_t face;  //!< The face that this half-edge is a member of
    uint32_t vert;  //!< Vertex index *at the end* of this edge, ie the edge *points to* this vert
    uint32_t twin;  //!< Second half-edge containing vertex index *at the beginning* of this edge
    uint32_t prev;  //!< Previous half-edge in face (clockwise direction)


    //----------------------------------------------------------

    HalfEdge() {}
    HalfEdge(uint32_t _face,
             uint32_t _vert,
             uint32_t _twin,
             uint32_t _prev) : face(_face), vert(_vert), twin(_twin), prev(_prev) {}

    //! Assign vals
    void set(uint32_t _face,
             uint32_t _vert,
             uint32_t _twin,
             uint32_t _prev) { face = _face; vert = _vert; twin = _twin; prev = _prev; }


    //----------------------------------------------------------

    //! Return true if this is a boundary edge (the edge's twin value is INVALID_VERTEX).
    bool        isBoundaryEdge() const { return (twin == INVALID_VERTEX); }

    //! Return true if the edge index indicates a boundary (its value is INVALID_VERTEX).
    static bool isBoundaryEdge(uint32_t edge) { return (edge == INVALID_VERTEX); }


    //!
    static bool buildEdges(const std::vector<uint32_t>& verts_per_face,
                           const std::vector<uint32_t>& vert_start_per_face,
                           const std::vector<uint32_t>& vert_indices,
                           std::vector<HalfEdge>&       edge_list);


    //------------------------------------------------------
    // Get edges, vertices & faces:
    //------------------------------------------------------
    //! Fill vectors with a list of verts, edges and faces that connect to the given vertex's outgoing edge.
    static void getVertexConnectedVertsEdgesAndFaces(const std::vector<HalfEdge>& edge_list,
                                                     uint32_t                     vert,
                                                     std::vector<uint32_t>&       connected_verts,
                                                     std::vector<uint32_t>&       connected_edges,
                                                     std::vector<uint32_t>&       connected_faces);

    //! Get the list of verts connected to the given vertex.
    static void getVertexConnectedVerts(const std::vector<HalfEdge>& edge_list,
                                        uint32_t                     vert,
                                        std::vector<uint32_t>&       connected_verts);

    //! Get the list of outgoing edges (edges pointing away from vertex) for the given vertex.
    static void getVertexOutgoingEdges(const std::vector<HalfEdge>& edge_list,
                                       uint32_t                     vert,
                                       std::vector<uint32_t>&       outgoing_edges);

    //! Get the list of incoming edges (edges pointing towards vertex) for the given vertex.
    static void getVertexIncomingEdges(const std::vector<HalfEdge>& edge_list,
                                       uint32_t                     vert,
                                       std::vector<uint32_t>&       incoming_edges);

    //! Get the list of faces that connect to the given vertex. Assumes quad faces.
    static void getVertexConnectedFaces(const std::vector<HalfEdge>& edge_list,
                                        uint32_t                     vert,
                                        std::vector<uint32_t>&       connected_faces);


    //------------------------------------------------------
    // Vertex CCW (counter-clockwise) circulators:
    //------------------------------------------------------

    /*! Find the next incoming edge sweeping around a center vertex in a CCW direction.
        The start edge is the *incoming* edge of the vertex.
    */
    static uint32_t ccwVertexIncomingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                                    uint32_t                     start_edge);

    /*! Find the next outgoing edge sweeping around a center vertex in a CCW direction.
        The start edge is the *outgoing* edge of the vertex.
    */
    static uint32_t ccwVertexOutgoingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                                    uint32_t                     start_edge);


    //------------------------------------------------------
    // Vertex CW (clockwise) circulators:
    //------------------------------------------------------

    /*! Find the next incoming edge sweeping around a center vertex in a CW direction.
        The start edge is the *incoming* edge of the vertex.
    */
    static uint32_t cwVertexIncomingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                                   uint32_t                     start_edge);
    /*! Find the next outgoing edge sweeping around a center vertex in a CW direction.
        The start edge is the *outgoing* edge of the vertex.
    */
    static uint32_t cwVertexOutgoingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                                   uint32_t                     start_edge);


};


typedef std::vector<Fsr::HalfEdge> HalfEdgeList;


/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/


//! Print out components to a stream.
std::ostream& operator << (std::ostream& o, const HalfEdge& he);



//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
// INLINE METHODS:
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

inline std::ostream& operator << (std::ostream& o, const HalfEdge& he)
{
    o << "[face:" << he.face << " vert:" << he.vert;
    o << " twin:"; if (he.isBoundaryEdge()) o << "BNDRY"; else o << he.twin;
    o << " prev:" << he.prev << "]";
    return o;
}

/*static*/ inline uint32_t
HalfEdge::ccwVertexIncomingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                          uint32_t                     start_edge)
{
#if DEBUG
    assert(start_edge < (uint32_t)edge_list.size());
#endif
    const uint32_t heTwin = edge_list[start_edge].twin;
    if (isBoundaryEdge(heTwin))
        return INVALID_VERTEX;
#if DEBUG
    assert(heTwin < (uint32_t)edge_list.size());
#endif
    return edge_list[heTwin].prev;
}

/*static*/ inline uint32_t
HalfEdge::ccwVertexOutgoingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                          uint32_t                     start_edge)
{
#if DEBUG
    assert(start_edge < (uint32_t)edge_list.size());
#endif
    const uint32_t hePrev = edge_list[start_edge].prev;
#if DEBUG
    assert(hePrev < (uint32_t)edge_list.size());
#endif
    return edge_list[hePrev].twin;
}

/*static*/ inline uint32_t
HalfEdge::cwVertexIncomingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                         uint32_t                     start_edge)
{
#if DEBUG
    assert(start_edge < (uint32_t)edge_list.size());
#endif
    const uint32_t heNext = edge_list[start_edge].vert;
    if (isBoundaryEdge(heNext))
        return INVALID_VERTEX;
#if DEBUG
    assert(heNext < (uint32_t)edge_list.size());
#endif
    return edge_list[heNext].twin;
}

/*static*/ inline uint32_t
HalfEdge::cwVertexOutgoingEdgeCirculator(const std::vector<HalfEdge>& edge_list,
                                         uint32_t                     start_edge)
{
#if DEBUG
    assert(start_edge < (uint32_t)edge_list.size());
#endif
    const uint32_t heTwin = edge_list[start_edge].twin;
    if (isBoundaryEdge(heTwin))
        return INVALID_VERTEX;
#if DEBUG
    assert(heTwin < (uint32_t)edge_list.size());
#endif
    return edge_list[heTwin].vert;
}

//--------------------------------------------------------

/*static*/ inline void
HalfEdge::getVertexConnectedVertsEdgesAndFaces(const std::vector<HalfEdge>& edge_list,
                                               uint32_t                     vert,
                                               std::vector<uint32_t>&       connected_verts,
                                               std::vector<uint32_t>&       connected_edges,
                                               std::vector<uint32_t>&       connected_faces)
{
#if DEBUG
    assert(vert < (uint32_t)edge_list.size());
    int32_t max_count = 10000;
#endif
    connected_verts.clear(); connected_verts.reserve(4);
    connected_edges.clear(); connected_edges.reserve(4);
    connected_faces.clear(); connected_faces.reserve(8);

    // Add the first vert, edge and face:
    connected_verts.push_back(edge_list[vert].vert);
    connected_edges.push_back(vert);
    connected_faces.push_back(edge_list[vert].face);

    // Circulate around center vertex adding edges and faces to the list:
    uint32_t heCurrent = vert;
    uint32_t hePrev, heNext;
    while (1)
    {
#if DEBUG
        assert(--max_count > 0); // just in case...
#endif
        heNext = ccwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
        if (isBoundaryEdge(heNext))
        {
            // Hit a boundary, add the last edge then go in other direction:
            hePrev = edge_list[heCurrent].prev;
#if DEBUG
            assert(hePrev < (uint32_t)edge_list.size());
#endif
            connected_edges.push_back(hePrev); // add the previous incoming edge
            hePrev = edge_list[hePrev].prev; // go to previous, previous edge to get the vert
#if DEBUG
            assert(hePrev < edge_list.size());
#endif
            connected_verts.push_back(edge_list[hePrev].vert);
            while (1)
            {
#if DEBUG
                assert(--max_count > 0); // just in case...
#endif
                heNext = cwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
                if (isBoundaryEdge(heNext) || heNext == vert)
                    break; // at boundary or looped around, stop

                // Add the vert, edge and face:
                connected_verts.push_back(edge_list[heNext].vert);
                connected_edges.push_back(heNext);
                connected_faces.push_back(edge_list[heNext].face);
                heCurrent = heNext;
            }
            break;

        } else if (heNext == vert)
            break; // looped around, stop

        // Add the edge and face:
        connected_verts.push_back(edge_list[heNext].vert);
        connected_edges.push_back(heNext);
        connected_faces.push_back(edge_list[heNext].face);
        heCurrent = heNext;
    }
}


/*static*/ inline void
HalfEdge::getVertexOutgoingEdges(const std::vector<HalfEdge>& edge_list,
                                 uint32_t                     vert,
                                 std::vector<uint32_t>&       outgoing_edges)
{
#if DEBUG
    assert(vert < (uint32_t)edge_list.size());
    int max_count = 10000;
#endif
    outgoing_edges.clear(); outgoing_edges.reserve(4);
    outgoing_edges.push_back(vert); // add first outgoing edge

    // Circulate around center vertex adding outgoing edges to the list:
    uint32_t heCurrent = vert;
    while (1)
    {
#if DEBUG
        assert(--max_count > 0); // just in case...
#endif
        uint32_t heNext = ccwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
        if (isBoundaryEdge(heNext))
        {
            // Hit a boundary, go in other direction:
            while (1)
            {
#if DEBUG
                assert(--max_count > 0); // just in case...
#endif
                heNext = cwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
                if (isBoundaryEdge(heNext) || heNext == vert)
                    break; // at boundary or looped around, stop
                outgoing_edges.push_back(heNext);
                heCurrent = heNext;
            }
            break;
        } else if (heNext == vert)
            break;
        outgoing_edges.push_back(heNext);
        heCurrent = heNext;
    }
}

/*static*/ inline void
HalfEdge::getVertexIncomingEdges(const std::vector<HalfEdge>& edge_list,
                                 uint32_t                     vert,
                                 std::vector<uint32_t>&       incoming_edges)
{
#if DEBUG
    assert(vert < (uint32_t)edge_list.size());
    int max_count = 10000;
#endif
    incoming_edges.clear(); incoming_edges.reserve(4);
    if (edge_list[vert].twin)
        incoming_edges.push_back(edge_list[vert].twin);  // add first incoming edge

    // Circulate around center vertex adding incoming edges to the list:
    uint32_t heCurrent = vert;
    while (1)
    {
#if DEBUG
        assert(--max_count > 0); // just in case...
#endif
        uint32_t heNext = ccwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
        if (isBoundaryEdge(heNext))
        {
            // Hit a boundary, there's no incoming edge, so go in other direction:
            while (1)
            {
#if DEBUG
                assert(--max_count > 0); // just in case...
#endif
                heNext = cwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
                if (isBoundaryEdge(heNext) || heNext == vert)
                    break; // at boundary or looped around, stop
                // Add the incoming edge if it exists:
                if (edge_list[heNext].twin)
                    incoming_edges.push_back(edge_list[heNext].twin);
                heCurrent = heNext;
            }
            break;

        } else if (heNext == vert)
            break;
        if (edge_list[heNext].twin)
            incoming_edges.push_back(edge_list[heNext].twin);
        heCurrent = heNext;
    }
}


/*static*/ inline void
HalfEdge::getVertexConnectedVerts(const std::vector<HalfEdge>& edge_list,
                                  uint32_t                     vert,
                                  std::vector<uint32_t>&       connected_verts)
{
#if DEBUG
    assert(vert < (uint32_t)edge_list.size());
    int max_count = 10000;
#endif
    connected_verts.clear(); connected_verts.reserve(4);
    connected_verts.push_back(edge_list[vert].vert); // add first vert

    // Circulate around center vertex adding verts to the list:
    uint32_t heCurrent = vert;
    while (1)
    {
#if DEBUG
        assert(--max_count > 0); // just in case...
#endif
        uint32_t heNext = ccwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
        if (isBoundaryEdge(heNext))
        {
            // Hit a boundary, add the last vert then go in other direction:
            uint32_t hePrev = edge_list[heCurrent].prev;
#if DEBUG
            assert(hePrev < (uint32_t)edge_list.size());
#endif
            hePrev = edge_list[hePrev].prev;
#if DEBUG
            assert(hePrev < (uint32_t)edge_list.size());
#endif
            connected_verts.push_back(edge_list[hePrev].vert);
            while (1)
            {
#if DEBUG
                assert(--max_count > 0); // just in case...
#endif
                heNext = cwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
                if (isBoundaryEdge(heNext) || heNext == vert)
                    break; // at boundary or looped around, stop
                // Add the vertex:
                connected_verts.push_back(edge_list[heNext].vert);
                heCurrent = heNext;
            }
            break;

        } else if (heNext == vert)
            break;
        connected_verts.push_back(edge_list[heNext].vert);
        heCurrent = heNext;
    }
}


// This macro assumes a quad mesh! ie 4 verts for every face, in order:
// TODO: to support n-vert faces we need to pass in a verts-per-face list.
#define FACE_INDEX(A) (A >> 2)

/*static*/ inline void
HalfEdge::getVertexConnectedFaces(const std::vector<HalfEdge>& edge_list,
                                  uint32_t                     vert,
                                  std::vector<uint32_t>&       connected_faces)
{
#if DEBUG
    assert(vert < (uint32_t)edge_list.size());
    int max_count = 10000;
#endif
    connected_faces.clear(); connected_faces.reserve(8);
    connected_faces.push_back(FACE_INDEX(vert)); // add first face

    // Circulate around center vertex adding faces to the list:
    uint32_t heCurrent = vert;
    while (1)
    {
#if DEBUG
        assert(--max_count > 0); // just in case...
#endif
        uint32_t heNext = ccwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
        if (isBoundaryEdge(heNext))
        {
            // Hit a boundary, go in other direction:
            while (1)
            {
#if DEBUG
                assert(--max_count > 0); // just in case...
#endif
                heNext = cwVertexOutgoingEdgeCirculator(edge_list, heCurrent);
                if (isBoundaryEdge(heNext) || heNext == vert)
                    break; // at boundary or looped around, stop
                connected_faces.push_back(FACE_INDEX(heNext));
                heCurrent = heNext;
            }
            break;
        } else if (heNext == vert)
            break;
        connected_faces.push_back(FACE_INDEX(heNext));
        heCurrent = heNext;
    }
}


/*static*/ inline bool
HalfEdge::buildEdges(const std::vector<uint32_t>& verts_per_face,
                     const std::vector<uint32_t>& vert_start_per_face,
                     const std::vector<uint32_t>& vert_indices,
                     std::vector<HalfEdge>&       edge_list)
{
    edge_list.clear();

    const size_t nFaces = verts_per_face.size();
    const size_t nVerts = vert_indices.size();
    if (nFaces == 0 || vert_start_per_face.size() != nFaces || nVerts == 0)
        return false; // don't crash...

    // Make sure face & vertex arrays are sized correctly:
    edge_list.resize(nVerts);

    // Temporary half-edge map indexed by edge point indices, to speed up
    // the matching of twin edges.
    //
    // On the 1st pass for each edge we build a hash of the start and end
    // point indices of the edge and use that as the key in the map.
    //
    // On the 2nd pass we find any edge with the same point indices
    // but in the reverse order. If there's a match those two edges are
    // considered 'twins'.
    std::unordered_map<uint64_t, uint32_t> edge_map;

    // Find max edge count:
    size_t nEdges = 0;
    for (size_t f=0; f < nFaces; ++f)
        nEdges += verts_per_face[f];
    if (nEdges == 0)
        return false; // don't crash...
    edge_map.reserve(nEdges);

    // Build edges for each face in the mesh in the 1st pass, then go back
    // and assign the twin connections:
    DD::Image::Hash hash;
    uint32_t heCurrent = 0;
    for (size_t f=0; f < nFaces; ++f)
    {
        // Each face points at the first edge in its list:
        const uint32_t heStart = heCurrent;

        // We step around the face, creating HalfEdges which points to the
        // next vertex, so we start with the first edge and work our way
        // around to the last which will wrap around to point at the first
        // vertex.
        const uint32_t nFaceVerts = verts_per_face[f];
        const uint32_t vstart     = vert_start_per_face[f];
        for (uint32_t i=0; i < nFaceVerts; ++i)
        {
            const uint32_t vCurrent = vstart + i;
            const uint32_t vNext    = vstart + ((i+1)%nFaceVerts);

            // This half-edge points at the *next* vertex:
            Fsr::HalfEdge& he = edge_list[heCurrent];
            he.vert = vNext;
            he.twin = -1; // default to boundary edge
            he.prev = heStart + ((i-1 + nFaceVerts)%nFaceVerts);
            he.face = (uint32_t)f;

            // Add forward point order to edge reference map:
            hash.reset();
            hash.append(vert_indices[vCurrent]);
            hash.append(vert_indices[vNext   ]);
            edge_map[hash.value()] = heCurrent;

            ++heCurrent;
        }
    }

    // 2nd pass - find all twin assignments by finding reverse point
    // indice matches in the edge map:
    heCurrent = 0;
    for (size_t f=0; f < nFaces; ++f)
    {
        const uint32_t nFaceVerts = verts_per_face[f];
        const uint32_t vstart     = vert_start_per_face[f];
        for (uint32_t i=0; i < nFaceVerts; ++i)
        {
            const uint32_t vCurrent = vstart + i;
            const uint32_t vNext    = vstart + ((i+1)%nFaceVerts);

            Fsr::HalfEdge& heOutgoing = edge_list[heCurrent];

            // Find the reverse point order edge in map, which
            // should come from another face:
            hash.reset();
            hash.append(vert_indices[vNext   ]);
            hash.append(vert_indices[vCurrent]);
            std::unordered_map<uint64_t, uint32_t>::const_iterator iter = edge_map.find(hash.value());
            if (iter != edge_map.end())
            {
                Fsr::HalfEdge& heIncoming = edge_list[iter->second];
                heIncoming.twin = heCurrent;
                heOutgoing.twin = iter->second;
            }
            ++heCurrent;
        }
    }

    return true;
}


} // namespace Fsr


#endif

// end of HalfEdge.h

//
// Copyright 2019 DreamWorks Animation
//
