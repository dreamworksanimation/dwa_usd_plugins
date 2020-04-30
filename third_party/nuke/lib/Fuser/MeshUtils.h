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

/// @file Fuser/MeshUtils.h
///
/// @author Jonathan Egstad

#ifndef Fuser_MeshUtils_h
#define Fuser_MeshUtils_h

#include "AttributeTypes.h" // for Vec3
#include "PointBasedPrimitive.h" // for VertexBuffers


namespace Fsr {

class VertexBuffers;


//!
FSR_EXPORT bool calcPointNormals(size_t            nPoints,
                                 const Fsr::Vec3f* points,
                                 size_t            nVerts,
                                 const uint32_t*   faceVertPointIndices,
                                 size_t            nFaces,
                                 const uint32_t*   nVertsPerFace,
                                 bool              all_tris,
                                 bool              all_quads,
                                 Fsr::Vec3fList&   point_normals);


//! Builds normals for the current VertexBuffers state.
FSR_EXPORT bool calcVertexBufferNormals(PointBasedPrimitive::VertexBuffers& vbuffers);


} // namespace Fsr

#endif

// end of Fuser/MeshUtils.h

//
// Copyright 2019 DreamWorks Animation
//
