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

/// @file Fuser/MeshUtils.cpp
///
/// @author Jonathan Egstad

#include "MeshUtils.h"

namespace Fsr {


/*!
*/
bool
calcPointNormals(size_t            nPoints,
                 const Fsr::Vec3f* points,
                 size_t            nVerts,
                 const uint32_t*   faceVertPointIndices,
                 size_t            nFaces,
                 const uint32_t*   nVertsPerFace,
                 bool              all_tris,
                 bool              all_quads,
                 Fsr::Vec3fList&   point_normals)
{
    if (nPoints == 0 || nVerts == 0 || nFaces == 0)
        return false;

    point_normals.resize(nPoints);
    memset(point_normals.data(), 0, nPoints*sizeof(Fsr::Vec3f));

    // This temp array stores per-point weights (just the count for now):
    std::vector<float> point_normal_weights(nPoints);
    memset(point_normal_weights.data(), 0, nPoints*sizeof(float));

    // For each face get its geometric normal and add it to its points:
    Fsr::Vec3f N;
    int vindex = 0; // global vert count
    if (all_tris && nVerts == (nFaces*3))
    {
        // Faster version for triangle meshes:
        for (size_t f=0; f < nFaces; ++f)
        {
            // CCW winding order!
            const uint32_t p0 = faceVertPointIndices[vindex++]; // v0
            const uint32_t p1 = faceVertPointIndices[vindex++]; // v1
            const uint32_t p2 = faceVertPointIndices[vindex++]; // v2
            N = (points[p1] - points[p0]).cross(points[p2] - points[p0]);
            // Add normal to each point:
            point_normals[p0] += N; point_normal_weights[p0] += 1.0f;
            point_normals[p1] += N; point_normal_weights[p1] += 1.0f;
            point_normals[p2] += N; point_normal_weights[p2] += 1.0f;
        }
    }
    else if (all_quads && nVerts == (nFaces*4))
    {
        // Faster version for quad meshes:
        for (size_t f=0; f < nFaces; ++f)
        {
            // CCW winding order!
            const uint32_t p0 = faceVertPointIndices[vindex++]; // v0
            const uint32_t p1 = faceVertPointIndices[vindex++]; // v1
            const uint32_t p2 = faceVertPointIndices[vindex++]; // v2
            const uint32_t p3 = faceVertPointIndices[vindex++]; // v3
            N = (points[p3] - points[p1]).cross(points[p0] - points[p2]);
            // Add normal to each point:
            point_normals[p0] += N; point_normal_weights[p0] += 1.0f;
            point_normals[p1] += N; point_normal_weights[p1] += 1.0f;
            point_normals[p2] += N; point_normal_weights[p2] += 1.0f;
            point_normals[p3] += N; point_normal_weights[p3] += 1.0f;
        }
    }
    else
    {
        if (nVertsPerFace == NULL)
            return false; // need the faces list!

        for (size_t f=0; f < nFaces; ++f)
        {
            const int nFaceVerts = nVertsPerFace[f];
            if (nFaceVerts < 3)
            {
                vindex += nFaceVerts; // can't build a normal without 3 or more verts
            }
            else if (nFaceVerts == 4)
            {
                // Quad - CCW winding order!
                const uint32_t p0 = faceVertPointIndices[vindex++]; // v0
                const uint32_t p1 = faceVertPointIndices[vindex++]; // v1
                const uint32_t p2 = faceVertPointIndices[vindex++]; // v2
                const uint32_t p3 = faceVertPointIndices[vindex++]; // v3
                N = (points[p3] - points[p1]).cross(points[p0] - points[p2]);
                // Add normal to each point:
                point_normals[p0] += N; point_normal_weights[p0] += 1.0f;
                point_normals[p1] += N; point_normal_weights[p1] += 1.0f;
                point_normals[p2] += N; point_normal_weights[p2] += 1.0f;
                point_normals[p3] += N; point_normal_weights[p3] += 1.0f;
            }
            else if (nFaceVerts == 3)
            {
                // Triangle - CCW winding order!
                const uint32_t p0 = faceVertPointIndices[vindex++]; // v0
                const uint32_t p1 = faceVertPointIndices[vindex++]; // v1
                const uint32_t p2 = faceVertPointIndices[vindex++]; // v2
                N = (points[p1] - points[p0]).cross(points[p2] - points[p0]);
                // Add normal to each point:
                point_normals[p0] += N; point_normal_weights[p0] += 1.0f;
                point_normals[p1] += N; point_normal_weights[p1] += 1.0f;
                point_normals[p2] += N; point_normal_weights[p2] += 1.0f;
            }
            else
            {
                // Polygon - CCW winding order!
                const uint32_t p0 = faceVertPointIndices[vindex+0]; // v0
                const uint32_t p1 = faceVertPointIndices[vindex+1]; // v1
                const uint32_t p2 = faceVertPointIndices[vindex+2]; // v2
                const uint32_t p3 = faceVertPointIndices[vindex+nFaceVerts-1]; // v3
                N = (points[p3] - points[p1]).cross(points[p0] - points[p2]);
                // Add normal to each point:
                for (int v=0; v < nFaceVerts; ++v, ++vindex)
                {
                    const uint32_t pindex = faceVertPointIndices[vindex];
                    point_normals[pindex] += N; point_normal_weights[pindex] += 1.0f;
                }
            }
        }
    }

    // Normalize normals using the final count weight:
    const float* cp = point_normal_weights.data();
    for (size_t i=0; i < nPoints; ++i, ++cp)
    {
        if (*cp <= 0.0f)
            continue;
        Fsr::Vec3f& N = point_normals[i];
        N /= *cp; // not sure if we need this, or will normalize handle it
        N.normalize();
    }

    return true;
}


/*! Builds normals for the current VertexBuffers state.
*/
bool
calcVertexBufferNormals(PointBasedPrimitive::VertexBuffers& vbuffers)
{
    Fsr::Vec3fList point_normals;
    if (!calcPointNormals(vbuffers.PL.size(),
                          vbuffers.PL.data(),
                          vbuffers.Pidx.size(),
                          vbuffers.Pidx.data(),
                          vbuffers.numFaces(),
                          vbuffers.vertsPerFace.data(),
                          vbuffers.allTris,
                          vbuffers.allQuads,
                          point_normals))
        return false;

    const size_t nVerts = vbuffers.numVerts();
    for (size_t v=0; v < nVerts; ++v)
        vbuffers.N[v] = point_normals[vbuffers.Pidx[v]];

    return true;
}


} // namespace Fsr


// end of Fuser/MeshUtils.cpp

//
// Copyright 2019 DreamWorks Animation
//
