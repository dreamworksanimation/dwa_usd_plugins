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

/// @file zprender/Mesh.cpp
///
/// @author Jonathan Egstad


#include "Mesh.h"
#include "RenderContext.h"
#include "ThreadContext.h"

#include <Fuser/Node.h>
#include <Fuser/ExecuteTargetContexts.h> // for MeshTessellateContext
#include <Fuser/MeshUtils.h> // for calcPointNormals()

#include <DDImage/Thread.h> // for Lock, sleepFor

//#define DEBUG_MESH_BUILD 1

namespace zpr {


static DD::Image::Lock my_lock;

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------


/*! This constructor accepts a series of faces with any number of verts.

    Supports calling a Fsr::Node subdivider, defaulting to 'fsrOpenSubdiv'.

    TODO: figure out why passing the Fsr::ArgSet by reference causes a crash. Compiler bug? Bad coding?
*/
Mesh::Mesh(SurfaceContext*        stx,
           bool                   enable_subdivision,
           const Fsr::ArgSet&     subd_args,
           const Fsr::DoubleList& motion_times,
           const Fsr::Mat4dList&  motion_xforms,
           uint32_t               numPoints,
           const Fsr::Vec3f**     P_arrays,
           const Fsr::Vec3f**     N_arrays,
           uint32_t               numFaces,
           const uint32_t*        vertsPerFace,
           const uint32_t*        vertList,
           const Fsr::Vec2f*      UV_array,
           const Fsr::Vec4f*      Cf_array) :
    RenderPrimitive(stx, motion_times),
    m_status(SURFACE_NOT_DICED),
    m_P_offset(0.0, 0.0, 0.0),
    m_num_facetris(0),
    m_all_tris(false),
    m_all_quads(false)
{
    // No go without points or faces...
    if (motion_times.size() == 0 || numPoints == 0 || !P_arrays || numFaces == 0)
    {
        std::cerr << "Mesh::ctor(): warning, zero points, faces, disabling." << std::endl;
        return;
    }

    //---------------------------------------------------------
    // Size the motion mesh samples list and fill them:
    m_motion_meshes.resize(m_motion_times.size());

    //---------------------------------------------------------
    // Copy face vert indices:
    m_all_quads = m_all_tris = true;
    const uint32_t* vpf = vertsPerFace;
    uint32_t numVerts = 0;
    for (uint32_t i=0; i < numFaces; ++i)
    {
        const uint32_t nFaceVerts = *vpf++;
        numVerts += nFaceVerts;
        if (nFaceVerts != 4)
            m_all_quads = false;
        if (nFaceVerts != 3)
            m_all_tris = false;
    }

    // No go without any verts...
    if (numVerts < 3)
    {
        m_all_quads = m_all_tris  = false;
        std::cerr << "Mesh::ctor(): warning, zero verts, disabling." << std::endl;
        return;
    }


    //---------------------------------------------------------
    // Copy verts (point indices):
    m_vert_indice_list.resize(numVerts);
    memcpy(m_vert_indice_list.data(), vertList, numVerts*sizeof(uint32_t));

    bool build_normals = true;

    //---------------------------------------------------------
    // Determine global offset from first motion sample only
    // by building the world-space bbox.
    //
    // Build world-space bbox (with l2w xform applied), and
    // take the global offset from its rounded-off center:
#if DEBUG
    assert(P_arrays[0] != NULL);
#endif
    Fsr::Box3d global_bbox(P_arrays[0], numPoints, motion_xforms[0]);
    const Fsr::Vec3d global_bbox_center = global_bbox.getCenter();
    m_P_offset.set(floor(global_bbox_center.x),
                   floor(global_bbox_center.y),
                   floor(global_bbox_center.z));

#ifdef DEBUG_MESH_BUILD
    std::cout << "Mesh::ctor(" << this << "):";
    std::cout << " numPoints=" << numPoints;
    std::cout << ", numFaces=" << numFaces << ", numVerts=" << numVerts;
    std::cout << ", all_quads=" << m_all_quads << ", all_tris=" << m_all_tris << std::endl;
    std::cout << "  xform" << motion_xforms[0] << ", global_bbox" << global_bbox << std::endl;
    std::cout << "  m_P_offset" << m_P_offset << std::endl;
#endif

    //updateMotionSamplePoints(motion_xforms, (size_t)numPoints, P_arrays, N_arrays);
    for (size_t j=0; j < m_motion_meshes.size(); ++j)
    {
        // Copy point data with the global offset included in l2w xform:
        Sample& mesh = m_motion_meshes[j];

        // Subtract offset from xform before baking it into points:
        Fsr::Mat4d xform;
        xform.setToTranslation(-m_P_offset);
        xform *= motion_xforms[j];

        // Bake the xform into the points during copy:
#if DEBUG
        assert(P_arrays[j] != NULL);
#endif
        mesh.P_list.resize(numPoints);
        xform.transform(mesh.P_list.data()/*dst*/, P_arrays[j]/*src*/, numPoints);
#ifdef DEBUG_MESH_BUILD
        std::cout << "  sample " << j << ": xform" << xform << std::endl;
#endif

        //---------------------------------------------------------
        // Copy animating normal data:
        if (0)//(N_arrays)
        {
            build_normals = false;
#if DEBUG
            assert(N_arrays[j] != NULL);
#endif
            mesh.N_list.resize(numPoints);

            Fsr::Mat4d ixform = motion_xforms[j].inverse();
            ixform.transform(mesh.N_list.data()/*dst*/, N_arrays[j]/*src*/, numPoints);
        }
    }

    //---------------------------------------------------------
    // Copy non-animating UV data:
    if (UV_array)
    {
        m_UV_list.resize(numVerts);
        memcpy(m_UV_list.data(), UV_array, numVerts*sizeof(Fsr::Vec2f));
    }

    //---------------------------------------------------------
    // Copy non-animating Cf data:
    if (Cf_array)
    {
        m_Cf_list.resize(numVerts);
        memcpy(m_Cf_list.data(), Cf_array, numVerts*sizeof(Fsr::Vec4f));
    }


    // If we're subdividing we need copies of the source data that
    // can be modified by the subdivider plugin:
    const uint32_t* verts_per_face_array = vertsPerFace;
    Fsr::Uint32List num_verts_per_face;


    // Try to subdivide mesh:
#ifdef DEBUG_MESH_BUILD
    std::cout << "  enable_subdivision=" << enable_subdivision << ", subd_args" << subd_args << std::endl;
#endif
    if (enable_subdivision)
    {
        const std::string tessellator_plugin = subd_args.getString("subd:tessellator", "OpenSubdiv"/*default*/);
        Fsr::Node* subdivider = Fsr::Node::create(tessellator_plugin.c_str()/*node_class*/, Fsr::ArgSet());
        if (!subdivider)
        {
            // Try to find the default subdivision tessellator plugin:
            // TODO: make this a built-in Fuser node:
            if (!subdivider)
                subdivider = Fsr::Node::create("SimpleSubdiv"/*node_class*/, Fsr::ArgSet());

            // TODO: throw a warning if no provider?
            //if (!subdivider)
        }

        // Apply subdivision if we now have a subdivider:
        if (subdivider)
        {
            num_verts_per_face.resize(numFaces);
            memcpy(num_verts_per_face.data(), vertsPerFace, numFaces*sizeof(uint32_t));

            // The construction of this is based on the Fsr::Node subdivider
            // expectations:
            // TODO: formalize these expectations somewhere...!
            Fsr::MeshTessellateContext tessellate_ctx;
            tessellate_ctx.verts_per_face        = &num_verts_per_face;
            tessellate_ctx.vert_position_indices = &m_vert_indice_list;
            tessellate_ctx.all_quads             = m_all_quads;
            tessellate_ctx.all_tris              = m_all_tris;

            // Per-point position data (multiple motion samples):
            tessellate_ctx.position_lists.reserve(m_motion_meshes.size());
            for (size_t j=0; j < m_motion_meshes.size(); ++j)
                tessellate_ctx.position_lists.push_back(&m_motion_meshes[j].P_list);

            // Don't pass the normals to the subdivider since we're rebuilding
            // them anyway.

            // Vert attribs:
            // UV
            if (m_UV_list.size() > 0)
                tessellate_ctx.vert_vec2_attribs.push_back(&m_UV_list);
            // Cf
            if (m_Cf_list.size() > 0)
                tessellate_ctx.vert_vec4_attribs.push_back(&m_Cf_list);

            int res = subdivider->execute(Fsr::NodeContext(subd_args), /*target_context*/
                                          tessellate_ctx.name,         /*target_name*/
                                          &tessellate_ctx              /*target*/);
            if (res < 0)
            {
                std::cerr << "zpr::Mesh::ctor()" << " error '" << subdivider->errorMessage() << "'" << std::endl;
            }

            numPoints = (uint32_t)m_motion_meshes[0].P_list.size();

            // Update the faces counts:
            m_all_quads = tessellate_ctx.all_quads;
            m_all_tris  = tessellate_ctx.all_tris;
            if (m_all_quads)
            {
                numFaces = uint32_t(m_vert_indice_list.size() / 4);
                verts_per_face_array = NULL;
            }
            else if (m_all_tris)
            {
                numFaces = uint32_t(m_vert_indice_list.size() / 3);
                verts_per_face_array = NULL;
            }
            else
            {
                numFaces = (uint32_t)num_verts_per_face.size();
                verts_per_face_array = num_verts_per_face.data();
            }

        }
    }

    if (build_normals)
    {
        // Resize the normals and rebuild them:
        for (size_t j=0; j < m_motion_meshes.size(); ++j)
        {
            Sample& mesh = m_motion_meshes[j];
            Fsr::calcPointNormals(numPoints,
                                  mesh.P_list.data(),
                                  m_vert_indice_list.size(),
                                  m_vert_indice_list.data(),
                                  numFaces,
                                  ((m_all_quads || m_all_tris) ? NULL : verts_per_face_array),
                                  m_all_tris,
                                  m_all_quads,
                                  mesh.N_list);
            assert(mesh.N_list.size() == mesh.P_list.size());
        }
    }

    // Update the vert start list if varying face vert count,
    // and finalize the subtri count:
    m_num_facetris = 0;
    if (numFaces > 0)
    {
        if      (m_all_quads)
            m_num_facetris = numFaces*2;
        else if (m_all_tris)
            m_num_facetris = numFaces;
        else
        {
            m_vert_start_per_face.resize(numFaces+1);
#if DEBUG
            assert(verts_per_face_array);
#endif
            const uint32_t* vpf = verts_per_face_array;
            numVerts = 0;
            m_num_facetris = 0;
            for (uint32_t i=0; i < numFaces; ++i)
            {
                const uint32_t nFaceVerts = *vpf++;
                m_vert_start_per_face[i] = numVerts;
                m_num_facetris += nFaceVerts-2;
                numVerts += nFaceVerts;
            }
            m_vert_start_per_face[numFaces] = numVerts; // add end of last face
        }
    }

    // Update the motion bboxes:
    for (size_t j=0; j < m_motion_meshes.size(); ++j)
    {
        Sample& mesh = m_motion_meshes[j];
        mesh.bbox.set(mesh.P_list.data(), mesh.P_list.size());
    }

#ifdef DEBUG_MESH_BUILD
    std::cout << "  final: numPoints=" << numPoints;
    std::cout << ", numFaces=" << numFaces;
    std::cout << ", numVerts=" << numVerts;
    std::cout << ", numFaceTris=" << m_num_facetris;
    std::cout << ", all_quads=" << m_all_quads << ", all_tris=" << m_all_tris << std::endl;

#if 1
    std::cout << "out faces:" << std::endl;
    if (m_all_quads)
        std::cout << "      (all quads)" << std::endl;
    else if (m_all_tris)
        std::cout << "      (all tris)" << std::endl;
    else
    {
        uint32_t num_facetris = 0;
        for (size_t f=0; f < (m_vert_start_per_face.size()-1); ++f)
        {
            const int32_t vs = m_vert_start_per_face[f];
            const int32_t ve = m_vert_start_per_face[f+1];
            const int32_t tris = (ve - vs - 2);
            num_facetris += tris;
            std::cout << "      " << f;
            std::cout << "[" << vs << ".." << ve-1 << "]";
            std::cout << "(" << (ve - vs) << ")" << ", tris=" << tris;
            std::cout << ", numFaceTris=" << num_facetris;
            std::cout << std::endl;
        }
    }

    std::cout << "out point indices:" << std::endl;
    for (size_t p=0; p < m_vert_indice_list.size(); ++p)
        std::cout << "      " << p << ": " << m_vert_indice_list[p] << std::endl;

    for (size_t j=0; j < m_motion_meshes.size(); ++j)
    {
        Sample& mesh = m_motion_meshes[j];
        std::cout << "out points[" << j << "]:" << std::endl;
        for (size_t p=0; p < mesh.P_list.size(); ++p)
            std::cout << "      " << p << mesh.P_list[p] << std::endl;
        std::cout << "  sample " << j << ": bbox" << mesh.bbox << std::endl;
    }
#endif
#endif
}


Mesh::~Mesh()
{
    //std::cout << "Mesh::dtor(" << this << ")" << std::endl;
}


//--------------------------------------------------------------------------


#if 0
/*!
*/
int
Mesh::quadFaceIntersect(uint32_t             face,
                        int                  motion_mode,
                        uint32_t             motion_step,
                        float                motion_step_t,
                        RayShaderContext&    stx,
                        SurfaceIntersection& I) const
{
    //std::cout << "  Mesh::quadFaceIntersect() face=" << face << ", motion_mode=" << motion_mode;
    //std::cout << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t;
    //std::cout << ", m_P_offset" << m_P_offset << std::endl;
    //std::cout << "   Rtx" << stx.Rtx << std::endl;
#if DEBUG
    assert(m_motion_meshes.size() > 0);
#endif

    const uint32_t* vp = (m_all_quads) ?
                            &m_vert_indice_list[face*4] :
                                &m_vert_indice_list[m_vert_start_per_face[face]];
    const uint32_t v0 = *vp++;
    const uint32_t v1 = *vp++;
    const uint32_t v2 = *vp++;
    const uint32_t v3 = *vp++;
    if (motion_mode != MOTIONSTEP_MID)
    {
        const uint32_t motion_sample = motion_step + (motion_mode == MOTIONSTEP_START) ? 0 : 1;
#if DEBUG
        assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
        const Fsr::Vec3f& p0 = points[v0];
        const Fsr::Vec3f& p1 = points[v1];
        const Fsr::Vec3f& p2 = points[v2];
        const Fsr::Vec3f& p3 = points[v3];

        // Intersect first and second triangles:
        if (stx.use_differentials)
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setQuadIntersection(face, 0/*subtri*/, motion_step, stx, p0, p1, p2, p3, I);
            if (Fsr::intersectTriangle(m_P_offset, p0, p2, p3, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setQuadIntersection(face, 1/*subtri*/, motion_step, stx, p0, p1, p2, p3, I);
        }
        else
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setQuadIntersection(face, 0/*subtri*/, motion_step, stx, p0, p1, p2, p3, I);
            }
            if (Fsr::intersectTriangle(m_P_offset, p0, p2, p3, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setQuadIntersection(face, 1/*subtri*/, motion_step, stx, p0, p1, p2, p3, I);
            }
        }
    }
    else
    {
#if DEBUG
        assert((motion_step+1) < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points0 = m_motion_meshes[motion_step  ].P_list;
        const Fsr::Vec3fList& points1 = m_motion_meshes[motion_step+1].P_list;

        const float invt = (1.0f - motion_step_t);
        const Fsr::Vec3f p0 = Fsr::lerp(points0[v0], points1[v0], motion_step_t, invt);
        const Fsr::Vec3f p1 = Fsr::lerp(points0[v1], points1[v1], motion_step_t, invt);
        const Fsr::Vec3f p2 = Fsr::lerp(points0[v2], points1[v2], motion_step_t, invt);
        const Fsr::Vec3f p3 = Fsr::lerp(points0[v3], points1[v3], motion_step_t, invt);

        // Intersect first and second triangles:
        if (stx.use_differentials)
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setMBQuadIntersection(face, 0/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, p3, I);
            if (Fsr::intersectTriangle(m_P_offset, p0, p2, p3, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setMBQuadIntersection(face, 1/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, p3, I);
        }
        else
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setMBQuadIntersection(face, 0/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, p3, I);
            }
            if (Fsr::intersectTriangle(m_P_offset, p0, p2, p3, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setMBQuadIntersection(face, 1/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, p3, I);
            }
        }
    }

    return Fsr::RAY_INTERSECT_NONE;
}


//--------------------------------------------------------------------------


/*!
*/
int
Mesh::triFaceIntersect(uint32_t             face,
                       int                  motion_mode,
                       uint32_t             motion_step,
                       float                motion_step_t,
                       RayShaderContext&    stx,
                       SurfaceIntersection& I) const
{
    //std::cout << "  Mesh::triFaceIntersect() face=" << face << ", motion_mode=" << motion_mode;
    //std::cout << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t;
    //std::cout << ", m_P_offset" << m_P_offset << std::endl;
    //std::cout << "   Rtx" << stx.Rtx << std::endl;
#if DEBUG
    assert(m_motion_meshes.size() > 0);
#endif

    const uint32_t* vp = (m_all_tris) ?
                            &m_vert_indice_list[face*3] :
                                &m_vert_indice_list[m_vert_start_per_face[face]];
    const uint32_t v0 = *vp++;
    const uint32_t v1 = *vp++;
    const uint32_t v2 = *vp++;
    if (motion_mode != MOTIONSTEP_MID)
    {
        const uint32_t motion_sample = motion_step + (motion_mode == MOTIONSTEP_START) ? 0 : 1;
#if DEBUG
        assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
        const Fsr::Vec3f& p0 = points[v0];
        const Fsr::Vec3f& p1 = points[v1];
        const Fsr::Vec3f& p2 = points[v2];

        // Intersect first and second triangles:
        if (stx.use_differentials)
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setTriIntersection(face, 0/*subtri*/, motion_step, stx, p0, p1, p2, I);
        }
        else
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setTriIntersection(face, 0/*subtri*/, motion_step, stx, p0, p1, p2, I);
            }
        }
    }
    else
    {
#if DEBUG
        assert((motion_step+1) < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points0 =  m_motion_meshes[motion_step  ].P_list;
        const Fsr::Vec3fList& points1 =  m_motion_meshes[motion_step+1].P_list;

        const float invt = (1.0f - motion_step_t);
        const Fsr::Vec3f p0 = Fsr::lerp(points0[v0], points1[v0], motion_step_t, invt);
        const Fsr::Vec3f p1 = Fsr::lerp(points0[v1], points1[v1], motion_step_t, invt);
        const Fsr::Vec3f p2 = Fsr::lerp(points0[v2], points1[v2], motion_step_t, invt);

        // Intersect first and second triangles:
        if (stx.use_differentials)
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                return setMBTriIntersection(face, 0/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, I);
        }
        else
        {
            if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
            {
                I.Rxst = I.Ryst = I.st;
                return setMBTriIntersection(face, 0/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, I);
            }
        }
    }

    return Fsr::RAY_INTERSECT_NONE;
}


/*!
*/
int
Mesh::polyFaceIntersect(uint32_t             face,
                        uint32_t             nFaceVerts,
                        int                  motion_mode,
                        uint32_t             motion_step,
                        float                motion_step_t,
                        RayShaderContext&    stx,
                        SurfaceIntersection& I) const
{
    //std::cout << "  Mesh::polyFaceIntersect() face=" << face << ", motion_mode=" << motion_mode;
    //std::cout << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t;
    //std::cout << ", m_P_offset" << m_P_offset << std::endl;
    //std::cout << "   Rtx" << stx.Rtx << std::endl;
#if DEBUG
    assert(m_motion_meshes.size() > 0);
    assert(nFaceVerts > 4); // shouldn't be handling quads or tris
#endif

    const uint32_t* vp = &m_vert_indice_list[m_vert_start_per_face[face]];
    if (motion_mode != MOTIONSTEP_MID)
    {
        const uint32_t motion_sample = motion_step + (motion_mode == MOTIONSTEP_START) ? 0 : 1;
#if DEBUG
        assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;

        const uint32_t v0 = vp[0];
        const Fsr::Vec3f& p0 = points[v0];
        for (uint32_t i=0; i < nFaceVerts; ++i)
        {
            const Fsr::Vec3f& p1 = points[vp[i+1]];
            const Fsr::Vec3f& p2 = points[vp[i+2]];

            // Intersect first and second triangles:
            if (stx.use_differentials)
            {
                if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                    return setTriIntersection(face, i/*subtri*/, motion_step, stx, p0, p1, p2, I);
            }
            else
            {
                if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
                {
                    I.Rxst = I.Ryst = I.st;
                    return setTriIntersection(face, i/*subtri*/, motion_step, stx, p0, p1, p2, I);
                }
            }
        }
    }
    else
    {
#if DEBUG
        assert((motion_step+1) < (uint32_t)m_motion_meshes.size());
#endif
        const Fsr::Vec3fList& points0 = m_motion_meshes[motion_step  ].P_list;
        const Fsr::Vec3fList& points1 = m_motion_meshes[motion_step+1].P_list;

        const float invt = (1.0f - motion_step_t);

        const uint32_t v0 = vp[0];
        const Fsr::Vec3f p0 = Fsr::lerp(points0[v0], points1[v0], motion_step_t, invt);
        for (uint32_t i=0; i < (nFaceVerts-1); ++i)
        {
            const uint32_t v1 = vp[i+1];
            const uint32_t v2 = vp[i+2];
            const Fsr::Vec3f p1 = Fsr::lerp(points0[v1], points1[v1], motion_step_t, invt);
            const Fsr::Vec3f p2 = Fsr::lerp(points0[v2], points1[v2], motion_step_t, invt);

            // Intersect first and second triangles:
            if (stx.use_differentials)
            {
                if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                    return setMBTriIntersection(face, i/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, I);
            }
            else
            {
                if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
                {
                    I.Rxst = I.Ryst = I.st;
                    return setMBTriIntersection(face, i/*subtri*/, motion_step, motion_step_t, stx, p0, p1, p2, I);
                }
            }
        }
    }

    return Fsr::RAY_INTERSECT_NONE;
}
#endif

//---------------------------------------------------------------------------


/*! Build the bvhs, one for each motion step.
*/
void
Mesh::buildBvh(const RenderContext& rtx,
               bool                 force)
{
    if (m_motion_bvhs.size() > 0 && !force)
       return;

    const uint32_t nMotionSamples = (uint32_t)m_motion_meshes.size();
#if DEBUG
    assert(nMotionSamples > 0);
#endif
#ifdef DEBUG_MESH_BUILD
    std::cout << "    Mesh::buildBvh(" << this << ") nMotionSamples=" << nMotionSamples;
    std::cout << ", rtx.numShutterSamples()=" << rtx.numShutterSamples() << std::endl;
#endif

    const uint32_t nFaces = numFaces();
#if DEBUG
    assert(m_num_facetris >= nFaces);
#endif
    std::vector<FaceIndexRef> facerefs(m_num_facetris);

    if (!rtx.isMotionBlurEnabled() || nMotionSamples == 1)
    {
        //-----------------------------------------
        // No motion-blur:
        //-----------------------------------------
        const Fsr::Vec3fList& points = m_motion_meshes[0].P_list;
        if (m_all_tris)
        {
            const uint32_t* vp = m_vert_indice_list.data();
            for (uint32_t f=0; f < nFaces; ++f)
            {
#if DEBUG
                assert(f < m_num_facetris);
#endif
                FaceIndexRef& ref = facerefs[f];
                ref.data = FaceIndex(f, 0);
                ref.bbox.set(points[*vp++]); // v0
                ref.bbox.expand(points[*vp++], false/*test_empty*/); // v1
                ref.bbox.expand(points[*vp++], false/*test_empty*/); // v2
            }
        }
        else if (m_all_quads)
        {
            const uint32_t* vp = m_vert_indice_list.data();
            uint32_t subtri = 0;
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const Fsr::Vec3f& p0 = points[*vp++];
                const Fsr::Vec3f& p1 = points[*vp++];
                const Fsr::Vec3f& p2 = points[*vp++];
                const Fsr::Vec3f& p3 = points[*vp++];

#if DEBUG
                assert(subtri < m_num_facetris);
#endif
                FaceIndexRef& ref0 = facerefs[subtri++];
                ref0.data = FaceIndex(f, 0);
                ref0.bbox.set(p0); // v0
                ref0.bbox.expand(p1, false/*test_empty*/); // v1
                ref0.bbox.expand(p2, false/*test_empty*/); // v2

#if DEBUG
                assert(subtri < m_num_facetris);
#endif
                FaceIndexRef& ref1 = facerefs[subtri++];
                ref1.data = FaceIndex(f, 1);
                ref1.bbox.set(p0); // v0
                ref1.bbox.expand(p2, false/*test_empty*/); // v2
                ref1.bbox.expand(p3, false/*test_empty*/); // v3
            }
        }
        else
        {
            const uint32_t* vp = m_vert_indice_list.data();
            uint32_t subtri = 0;
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const uint32_t v0 = m_vert_start_per_face[f];
                const Fsr::Vec3f& p0 = points[vp[v0]];

                const uint32_t nFaceTris = (m_vert_start_per_face[f+1] - v0) - 2;
                for (uint32_t i=0; i < nFaceTris; ++i)
                {
                    const uint32_t v1 = vp[v0+i+1];
                    const uint32_t v2 = vp[v0+i+2];
                    const Fsr::Vec3f& p1 = points[v1];
                    const Fsr::Vec3f& p2 = points[v2];

#if DEBUG
                    assert(subtri < m_num_facetris);
#endif
                    FaceIndexRef& ref = facerefs[subtri++];
                    ref.data = FaceIndex(f, i);
                    ref.bbox.set(p0); // v0
                    ref.bbox.expand(p1, false/*test_empty*/); // v2
                    ref.bbox.expand(p2, false/*test_empty*/); // v3
                }
            }
        }

        m_motion_bvhs.resize(1);
        FaceIndexBvh& bvh = m_motion_bvhs[0];
        bvh.setName("Mesh::FaceIndexBvh");
        bvh.build(facerefs, 1/*max_objects_per_leaf*/);
        bvh.setGlobalOrigin(m_P_offset);
#ifdef DEBUG_MESH_BUILD
        std::cout << "      no mblur bvh" << bvh.bbox() << " depth=" << bvh.maxNodeDepth() << std::endl;
#endif
    }
    else
    {
        //-------------------------------------------------
        // Motion-blur - combine two motion samples bboxes
        // per motion_step.
        //-------------------------------------------------
        // There's always at least one motion sample, and two per motion-step:
        m_motion_bvhs.resize(nMotionSamples-1);

        Fsr::Box3fList prev_bboxes(m_num_facetris);

        // Get first sample face bboxes:
        const Fsr::Vec3fList& points0 = m_motion_meshes[0].P_list;
        if (m_all_tris)
        {
            const uint32_t* vp = m_vert_indice_list.data();
            for (uint32_t f=0; f < nFaces; ++f)
            {
#if DEBUG
                assert(f < m_num_facetris);
#endif
                Fsr::Box3f& bbox = prev_bboxes[f];
                bbox.set(points0[*vp++]); // v0
                bbox.expand(points0[*vp++], false/*test_empty*/); // v1
                bbox.expand(points0[*vp++], false/*test_empty*/); // v2
            }
        }
        else if (m_all_quads)
        {
            const uint32_t* vp = m_vert_indice_list.data();
            uint32_t subtri = 0;
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const Fsr::Vec3f& p0 = points0[*vp++];
                const Fsr::Vec3f& p1 = points0[*vp++];
                const Fsr::Vec3f& p2 = points0[*vp++];
                const Fsr::Vec3f& p3 = points0[*vp++];

#if DEBUG
                assert(subtri < m_num_facetris);
#endif
                Fsr::Box3f& bbox0 = prev_bboxes[subtri++];
                bbox0.set(p0); // v0
                bbox0.expand(p1, false/*test_empty*/); // v1
                bbox0.expand(p2, false/*test_empty*/); // v2

#if DEBUG
                assert(subtri < m_num_facetris);
#endif
                Fsr::Box3f& bbox1 = prev_bboxes[subtri++];
                bbox1.set(p0); // v0
                bbox1.expand(p2, false/*test_empty*/); // v2
                bbox1.expand(p3, false/*test_empty*/); // v3
            }
        }
        else
        {
            const uint32_t* vp = m_vert_indice_list.data();
            uint32_t subtri = 0;
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const uint32_t v0 = m_vert_start_per_face[f];
                const Fsr::Vec3f& p0 = points0[vp[v0]];

                const uint32_t nFaceTris = (m_vert_start_per_face[f+1] - v0) - 2;
                for (uint32_t i=0; i < nFaceTris; ++i)
                {
                    const uint32_t v1 = vp[v0+i+1];
                    const uint32_t v2 = vp[v0+i+2];
#if DEBUG
                    assert(subtri < m_num_facetris);
#endif
                    Fsr::Box3f& bbox = prev_bboxes[subtri++];
                    bbox.set(p0); // v0
                    bbox.expand(points0[v1], false/*test_empty*/); // v2
                    bbox.expand(points0[v2], false/*test_empty*/); // v3
                }
            }
        }

        // Now the rest:
        for (uint32_t j=0; j < nMotionSamples-1; ++j)
        {
            const Fsr::Vec3fList& points1 = m_motion_meshes[j+1].P_list;
            if (m_all_tris)
            {
                const uint32_t* vp = m_vert_indice_list.data();
                for (uint32_t f=0; f < nFaces; ++f)
                {
                    // Find interpolated primitive bbox at start & end of step:
#if DEBUG
                    assert(f < m_num_facetris);
#endif
                    const Fsr::Box3f prev_bbox = prev_bboxes[f]; // save prev
                    FaceIndexRef& ref = facerefs[f];
                    ref.data = FaceIndex(f, 0);
                    ref.bbox.set(points1[*vp++]); // v0
                    ref.bbox.expand(points1[*vp++], false/*test_empty*/); // v1
                    ref.bbox.expand(points1[*vp++], false/*test_empty*/); // v2
                    prev_bboxes[f] = ref.bbox;
                    ref.bbox.expand(prev_bbox, false/*test_empty*/);
                }
            }
            else if (m_all_quads)
            {
                const uint32_t* vp = m_vert_indice_list.data();
                uint32_t subtri = 0;
                for (uint32_t f=0; f < nFaces; ++f)
                {
                    // Find interpolated primitive bbox at start & end of step:
                    const Fsr::Vec3f& p0 = points1[*vp++];
                    const Fsr::Vec3f& p1 = points1[*vp++];
                    const Fsr::Vec3f& p2 = points1[*vp++];
                    const Fsr::Vec3f& p3 = points1[*vp++];

#if DEBUG
                    assert(subtri < m_num_facetris);
#endif
                    const Fsr::Box3f prev_bbox0 = prev_bboxes[subtri]; // save prev
                    FaceIndexRef& ref0 = facerefs[subtri];
                    ref0.data = FaceIndex(f, 0);
                    ref0.bbox.set(p0); // v0
                    ref0.bbox.expand(p1, false/*test_empty*/); // v1
                    ref0.bbox.expand(p2, false/*test_empty*/); // v2
                    prev_bboxes[subtri] = ref0.bbox;
                    ref0.bbox.expand(prev_bbox0, false/*test_empty*/);
                    ++subtri;

#if DEBUG
                    assert(subtri < m_num_facetris);
#endif
                    const Fsr::Box3f prev_bbox1 = prev_bboxes[subtri]; // save prev
                    FaceIndexRef& ref1 = facerefs[subtri];
                    ref1.data = FaceIndex(f, 1);
                    ref1.bbox.set(p0); // v0
                    ref1.bbox.expand(p2, false/*test_empty*/); // v1
                    ref1.bbox.expand(p3, false/*test_empty*/); // v2
                    prev_bboxes[subtri] = ref1.bbox;
                    ref1.bbox.expand(prev_bbox1, false/*test_empty*/);
                    ++subtri;
                }
            }
            else
            {
                const uint32_t* vp = m_vert_indice_list.data();
                uint32_t subtri = 0;
                for (uint32_t f=0; f < nFaces; ++f)
                {
                    const uint32_t v0 = m_vert_start_per_face[f];
                    const Fsr::Vec3f& p0 = points1[vp[v0]];

                    const uint32_t nFaceTris = (m_vert_start_per_face[f+1] - v0) - 2;
                    for (uint32_t i=0; i < nFaceTris; ++i)
                    {
                        // Find interpolated primitive bbox at start & end of step:
                        const uint32_t v1 = vp[v0+i+1];
                        const uint32_t v2 = vp[v0+i+2];
#if DEBUG
                        assert(subtri < m_num_facetris);
#endif
                        const Fsr::Box3f prev_bbox = prev_bboxes[subtri]; // save prev
                        FaceIndexRef& ref = facerefs[subtri];
                        ref.data = FaceIndex(f, i);
                        ref.bbox.set(p0); // v0
                        ref.bbox.expand(points1[v1], false/*test_empty*/); // v1
                        ref.bbox.expand(points1[v2], false/*test_empty*/); // v2
                        prev_bboxes[subtri] = ref.bbox;
                        ref.bbox.expand(prev_bbox, false/*test_empty*/);
                        ++subtri;
                    }
                }
            }

            FaceIndexBvh& bvh = m_motion_bvhs[j];
            bvh.setName("Mesh::FaceIndexBvh");
            bvh.build(facerefs, 1/*max_objects_per_leaf*/);
            bvh.setGlobalOrigin(m_P_offset);
#ifdef DEBUG_MESH_BUILD
            std::cout << "      " << j << ": mb bvh" << bvh.bbox() << " depth=" << bvh.maxNodeDepth() << std::endl;
#endif
        }
    }
}


/*! Build the Bvhs in a thread-safe loop.
*/
bool
Mesh::expand(const RenderContext& rtx)
{
    //std::cout << "  Mesh::expand(" << this << ") nMotionSamples=" << m_motion_meshes.size() << std::endl;
    //std::cout << "  rtx.numShutterSamples()=" << rtx.numShutterSamples();
    //std::cout << ", m_status=" << m_status << std::endl;
    if (m_status == SURFACE_DICED)
        return true;

    // TODO: switch this loop to a std::condition_variable mutex test!

    // Creating the Bvhs must be done thread-safe to avoid another ray thread
    // from intersecting before they exist:
    uint32_t limit_count = 6000; // 0.01*6000 = 60seconds
    while (1)
    {
        if (m_status == SURFACE_DICED)
            return true;

        if (m_status == SURFACE_NOT_DICED)
        {
            my_lock.lock();
            if (m_status == SURFACE_NOT_DICED)
            {
                // Ok, this thread takes ownership of Bvh creation:
                m_status = SURFACE_DICING;
                my_lock.unlock();
#ifdef DEBUG_MESH_BUILD
                std::cout << "  Mesh::expand(" << this << ") nMotionSamples=" << m_motion_meshes.size();
                std::cout << ", rtx.numShutterSamples()=" << rtx.numShutterSamples();
                std::cout << ", m_status=" << m_status << std::endl;
#endif

                buildBvh(rtx, false/*force*/);
                // Done, let the intersection test finish below:
                m_status = SURFACE_DICED;

                return true;
            }
            else
            {
                my_lock.unlock();
            }
        }

        // TODO: switch this loop to a std::condition_variable mutex test!
        // Pause briefly then try again:
        DD::Image::sleepFor(0.01/*seconds*/);
        if (--limit_count == 0)
        {
            std::cerr << "  Mesh::expand() limit count reached!  This is likely the result of a bug." << std::endl;
            return false;
        }

    } // while loop

    //return false; // not necessary
}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
// From RenderPrimitive:


/*! Get the AABB for this primitive at an optional motion step time.  This
   interpolates between the motion sample bboxes.
*/
/*virtual*/
Fsr::Box3d
Mesh::getBBoxAtTime(double frame_time)
{
#if DEBUG
    assert(m_motion_meshes.size() > 0);
#endif

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_meshes.size());
#endif

    Fsr::Box3d bbox;
    if (motion_mode == MOTIONSTEP_START)
        bbox = Fsr::Box3d(m_motion_meshes[motion_step].bbox);
    else if (motion_mode == MOTIONSTEP_END)
        bbox = Fsr::Box3d(m_motion_meshes[motion_step+1].bbox);
    else
        bbox = Fsr::lerp(Fsr::Box3d(m_motion_meshes[motion_step  ].bbox),
                         Fsr::Box3d(m_motion_meshes[motion_step+1].bbox),
                         motion_step_t);

    bbox.shift(m_P_offset); // to world-space

    // Expand by displacement:
    if (getDisplacementSubdivisionLevel() > 0)
        bbox.pad(getDisplacementBounds());

#ifdef DEBUG_MESH_BUILD
    std::cout << "Mesh::getBBoxAtTime(" << this << "): frame_time=" << frame_time << ", m_motion_times[";
    for (uint32_t i=0; i < m_motion_times.size(); ++i)
       std::cout << " " << m_motion_times[i];
    std::cout << " ]" << " bbox" << bbox << std::endl;
#endif

    return bbox;
}


/*! Interpolate varying vertex attributes at SurfaceIntersection, no derivatives.
*/
/*virtual*/
void
Mesh::getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                         const DD::Image::ChannelSet& mask,
                                         Fsr::Pixel&                  v) const
{
    //std::cout << "Mesh::getAttributesAtSurfaceIntersection()" << std::endl;
#if DEBUG
    assert(I.part_index    <  (int32_t)numFaces());
    assert(I.subpart_index <= (int32_t)getFaceNumVerts(I.part_index));
#endif
    uint32_t tri_start;
    if (m_all_quads)
        tri_start = I.part_index*4;
    else if (m_all_tris)
        tri_start = I.part_index*3;
    else
        tri_start = m_vert_start_per_face[I.part_index];
    const uint32_t tri_offset = tri_start + I.subpart_index;

    if (m_UV_list.size() > 0)
    {
        Fsr::Vec2f uv = Fsr::interpolateAtBaryCoord(m_UV_list[tri_start   ],
                                                    m_UV_list[tri_offset+1],
                                                    m_UV_list[tri_offset+2], I.st);
        v.UV().set(uv.x, uv.y, 0.0f, 1.0f);
    }
    else
    {
        v.UV().set(0.5f, 0.5f, 0.0f, 1.0f);
    }

    if (m_Cf_list.size() > 0)
    {
        v.Cf() = Fsr::interpolateAtBaryCoord(m_Cf_list[tri_start   ],
                                             m_Cf_list[tri_offset+1],
                                             m_Cf_list[tri_offset+2], I.st);
    }
    else
    {
        v.Cf().set(1,1,1,1);
    }
}


/*! Interpolate varying vertex attributes at SurfaceIntersection. This also calculates derivatives.
*/
/*virtual*/
void
Mesh::getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                         const DD::Image::ChannelSet& mask,
                                         Fsr::Pixel&                  v,
                                         Fsr::Pixel&                  vdu,
                                         Fsr::Pixel&                  vdv) const
{
    //std::cout << "Mesh::getAttributesAtSurfaceIntersection()" << std::endl;
#if DEBUG
    assert(I.part_index    <  (int32_t)numFaces());
    assert(I.subpart_index <= (int32_t)getFaceNumVerts(I.part_index));
#endif

    uint32_t tri_start;
    if (m_all_quads)
        tri_start = I.part_index*4;
    else if (m_all_tris)
        tri_start = I.part_index*3;
    else
        tri_start = m_vert_start_per_face[I.part_index];
    const uint32_t tri_offset = tri_start + I.subpart_index;

    if (m_UV_list.size() > 0)
    {
        Fsr::Vec2f uv, uvdu, uvdv;
        Fsr::interpolateAtBaryCoord(m_UV_list[tri_start   ],
                                    m_UV_list[tri_offset+1],
                                    m_UV_list[tri_offset+2],
                                    I.st, I.Rxst, I.Ryst,
                                    uv,   uvdu,   uvdv);
          v.UV().set(uv.x, uv.y, 0, 1);
        vdu.UV().set(uvdu.x, uvdu.y, 0, 0);
        vdv.UV().set(uvdv.x, uvdv.y, 0, 0);

    }
    else
    {
          v.UV().set(0.5,0.5,0,1);
        vdu.UV().set(0,0,0,0);
        vdv.UV().set(0,0,0,0);
    }

    if (m_Cf_list.size() > 0)
    {
        Fsr::interpolateAtBaryCoord(m_Cf_list[tri_start   ],
                                    m_Cf_list[tri_offset+1],
                                    m_Cf_list[tri_offset+2],
                                    I.st, I.Rxst, I.Ryst,
                                    v.Cf(), vdu.Cf(), vdv.Cf());
    }
    else
    {
        v.Cf().set(1,1,1,1);
        vdu.Cf().set(0,0,0,0);
        vdv.Cf().set(0,0,0,0);
    }
}


//--------------------------------------------------------------------------
// From Traceable:


/*virtual*/
bool
Mesh::intersect(RayShaderContext& stx)
{
    SurfaceIntersection I(std::numeric_limits<double>::infinity());
    return (getFirstIntersection(stx, I) != Fsr::RAY_INTERSECT_NONE);
}


/*virtual*/
Fsr::RayIntersectionType
Mesh::getFirstIntersection(RayShaderContext&    stx,
                           SurfaceIntersection& I)
{
    //if (stx.rtx->k_debug == RenderContext::DEBUG_LOW)
    //   std::cout << "Mesh::getFirstIntersection(" << this << ") frame_time=" << stx.frame_time << std::endl;

    // Make sure Bvhs are created:
    if (!expand(*stx.rtx))
        return Fsr::RAY_INTERSECT_NONE; // error in expand

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_bvhs.size());
#endif

    // Intersect against the correct motion_step bvh:
    const FaceIndexBvh& bvh = m_motion_bvhs[motion_step];

    if (bvh.isEmpty())
        return Fsr::RAY_INTERSECT_NONE; // don't bother...

    SurfaceIntersection If;

    I.t = std::numeric_limits<double>::infinity();

    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = bvh.getNode(current_node_index);
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth();
        //std::cout << ", itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
        if (Fsr::intersectAABB(node.bbox, bvh.getGlobalOrigin(), stx.Rtx))
        {
            if (node.isLeaf())
            {
#if DEBUG
                assert(node.itemStart() < m_num_facetris);
                assert(node.numItems() == 1);
#endif
                // Intersect the subtri:
                const FaceIndex& findex = bvh.getItem(node.itemStart());
#if DEBUG
                assert(findex.face < numFaces());
#endif
                const uint32_t* vp;
                if (m_all_quads)
                    vp = &m_vert_indice_list[findex.face*4];
                else if (m_all_tris)
                    vp = &m_vert_indice_list[findex.face*3];
                else
                    vp = &m_vert_indice_list[m_vert_start_per_face[findex.face]];

                // Get points, maybe interpolated:
                Fsr::Vec3f p0, p1, p2;
                if (motion_mode != MOTIONSTEP_MID)
                {
                    // At a motion sample, no interpolation:
                    const uint32_t motion_sample = motion_step + (motion_mode == MOTIONSTEP_START) ? 0 : 1;
#if DEBUG
                    assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
                    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
                    p0 = points[vp[0              ]];
                    p1 = points[vp[findex.subtri+1]];
                    p2 = points[vp[findex.subtri+2]];
                }
                else
                {
                    // Between motion samples, interpolate:
#if DEBUG
                    assert(motion_step < (uint32_t)m_motion_meshes.size());
                    assert((motion_step+1) < (uint32_t)m_motion_meshes.size());
#endif
                    const Fsr::Vec3fList& points0 =  m_motion_meshes[motion_step  ].P_list;
                    const Fsr::Vec3fList& points1 =  m_motion_meshes[motion_step+1].P_list;

                    const uint32_t v0 = vp[0              ];
                    const uint32_t v1 = vp[findex.subtri+1];
                    const uint32_t v2 = vp[findex.subtri+2];
                    const float invt = (1.0f - motion_step_t);
                    p0 = Fsr::lerp(points0[v0], points1[v0], motion_step_t, invt);
                    p1 = Fsr::lerp(points0[v1], points1[v1], motion_step_t, invt);
                    p2 = Fsr::lerp(points0[v2], points1[v2], motion_step_t, invt);
                }

                if (stx.use_differentials)
                {
                    if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, If.st, If.Rxst, If.Ryst, If.t))
                    {
                        if (If.t < I.t)
                        {
                            I = If;
                            setTriIntersection(findex.face, findex.subtri, motion_step, stx, p0, p1, p2, I);
                        }
                    }
                }
                else
                {
                    if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, If.st, If.t))
                    {
                        if (If.t < I.t)
                        {
                            I = If;
                            I.Rxst = I.Ryst = I.st;
                            setTriIntersection(findex.face, findex.subtri, motion_step, stx, p0, p1, p2, I);
                        }
                    }
                }

                if (next_to_visit_index == 0)
                    break;
                --next_to_visit_index;
                current_node_index = nodes_to_visit_stack[next_to_visit_index];
            }
            else
            {
                // Put far Bvh node on nodes_to_visit_stack, advance to near node
                if (stx.Rtx.isSlopePositive(node.split_axis))
                {
                    nodes_to_visit_stack[next_to_visit_index++] = node.B_offset;
                    current_node_index = (current_node_index + 1);
                }
                else
                {
                    nodes_to_visit_stack[next_to_visit_index++] = (current_node_index + 1);
                    current_node_index = node.B_offset;
                }
            }

        }
        else
        {
            if (next_to_visit_index == 0)
                break;
            --next_to_visit_index;
            current_node_index = nodes_to_visit_stack[next_to_visit_index];
        }

    }

    if (I.t < std::numeric_limits<double>::infinity())
        return Fsr::RAY_INTERSECT_POINT;

    return Fsr::RAY_INTERSECT_NONE;
}


/*virtual*/
void
Mesh::getIntersections(RayShaderContext&        stx,
                       SurfaceIntersectionList& I_list,
                       double&                  tmin,
                       double&                  tmax)
{
    //std::cout << "Mesh::getIntersections(" << this << ") frame_time=" << stx.frame_time << std::endl;

    // Make sure Bvhs are created:
    if (!expand(*stx.rtx))
    {
        //std::cerr << "  Mesh::getIntersections() error in expand" << std::endl;
        return; // error in expand
    }

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_bvhs.size());
#endif

    // Intersect against the correct motion_step bvh:
    const FaceIndexBvh& bvh = m_motion_bvhs[motion_step];
    if (bvh.isEmpty())
        return; // don't bother...

    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = bvh.getNode(current_node_index);
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth();
        //std::cout << ", itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
        if (Fsr::intersectAABB(node.bbox, bvh.getGlobalOrigin(), stx.Rtx))
        {
            if (node.isLeaf())
            {
#if DEBUG
                assert(node.itemStart() < m_num_facetris);
                assert(node.numItems() == 1);
#endif
                // Intersect the subtri:
                const FaceIndex& findex = bvh.getItem(node.itemStart());
#if DEBUG
                assert(findex.face < numFaces());
#endif
                const uint32_t* vp;
                if (m_all_quads)
                    vp = &m_vert_indice_list[findex.face*4];
                else if (m_all_tris)
                    vp = &m_vert_indice_list[findex.face*3];
                else
                    vp = &m_vert_indice_list[m_vert_start_per_face[findex.face]];

                // Get points, maybe interpolated:
                Fsr::Vec3f p0, p1, p2;
                if (motion_mode != MOTIONSTEP_MID)
                {
                    // At a motion sample, no interpolation:
                    const uint32_t motion_sample = motion_step + (motion_mode == MOTIONSTEP_START) ? 0 : 1;
#if DEBUG
                    assert(motion_sample < (uint32_t)m_motion_meshes.size());
#endif
                    const Fsr::Vec3fList& points = m_motion_meshes[motion_sample].P_list;
                    p0 = points[vp[0              ]];
                    p1 = points[vp[findex.subtri+1]];
                    p2 = points[vp[findex.subtri+2]];
                }
                else
                {
                    // Between motion samples, interpolate:
#if DEBUG
                    assert(motion_step < (uint32_t)m_motion_meshes.size());
                    assert((motion_step+1) < (uint32_t)m_motion_meshes.size());
#endif
                    const Fsr::Vec3fList& points0 =  m_motion_meshes[motion_step  ].P_list;
                    const Fsr::Vec3fList& points1 =  m_motion_meshes[motion_step+1].P_list;

                    const uint32_t v0 = vp[0              ];
                    const uint32_t v1 = vp[findex.subtri+1];
                    const uint32_t v2 = vp[findex.subtri+2];
                    const float invt = (1.0f - motion_step_t);
                    p0 = Fsr::lerp(points0[v0], points1[v0], motion_step_t, invt);
                    p1 = Fsr::lerp(points0[v1], points1[v1], motion_step_t, invt);
                    p2 = Fsr::lerp(points0[v2], points1[v2], motion_step_t, invt);
                }

                if (stx.use_differentials)
                {
                    SurfaceIntersection I;
                    if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, stx.Rdif, I.st, I.Rxst, I.Ryst, I.t))
                    {
                        setTriIntersection(findex.face, findex.subtri, motion_step, stx, p0, p1, p2, I);
                        addIntersectionToList(I, I_list);
                        if (I.t < tmin)
                            tmin = I.t;
                        if (I.t > tmax)
                            tmax = I.t;
                    }
                }
                else
                {
                    SurfaceIntersection I;
                    if (Fsr::intersectTriangle(m_P_offset, p0, p1, p2, stx.Rtx, I.st, I.t))
                    {
                        I.Rxst = I.Ryst = I.st;
                        setTriIntersection(findex.face, findex.subtri, motion_step, stx, p0, p1, p2, I);
                        addIntersectionToList(I, I_list);
                        if (I.t < tmin)
                            tmin = I.t;
                        if (I.t > tmax)
                            tmax = I.t;
                    }
                }

                if (next_to_visit_index == 0)
                    break;
                --next_to_visit_index;
                current_node_index = nodes_to_visit_stack[next_to_visit_index];
            }
            else
            {
                // Put far Bvh node on nodes_to_visit_stack, advance to near node
                if (stx.Rtx.isSlopePositive(node.split_axis))
                {
                    nodes_to_visit_stack[next_to_visit_index++] = node.B_offset;
                    current_node_index = (current_node_index + 1);
                }
                else
                {
                    nodes_to_visit_stack[next_to_visit_index++] = (current_node_index + 1);
                    current_node_index = node.B_offset;
                }
            }

        }
        else
        {
            if (next_to_visit_index == 0)
                break;
            --next_to_visit_index;
            current_node_index = nodes_to_visit_stack[next_to_visit_index];
        }

    }
}


/*virtual*/
int
Mesh::intersectLevel(RayShaderContext& stx,
                     int               level,
                     int               max_level)
{
    //std::cout << "      Mesh::intersectLevel(" << this << "): parent-level=" << level << std::endl;

    // Make sure Bvhs are created:
    if (!expand(*stx.rtx))
        return -1; // error in expand

    // Don't increase level here, let the motion bvh do that:
    //++level;

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_bvhs.size());
#endif

    // Intersect the motion bvh:
    int sub_level = m_motion_bvhs[motion_step].intersectLevel(stx, level, max_level);
    if (sub_level > level)
        return sub_level;

    return -1; // not intersected
}


//--------------------------------------------------------------------------


#if 0
/*! Find the intersection point of two lines.
*/
inline bool
lineIntersectionPoint(const Vector2& p0s, const Vector2& p0e,
                      const Vector2& p1s, const Vector2& p1e,
                      double&        t0,
                      double&        t1,
                      Vector2&       intersectP)
{ 
    // Get A,B of first segment p0s to p0e:
    const double bx = (double(p0e.x) - double(p0s.x)); 
    const double by = (double(p0e.y) - double(p0s.y)); 
    // Get A,B of second segment p1s to p1e:
    const double dx = (double(p1e.x) - double(p1s.x));
    const double dy = (double(p1e.y) - double(p1s.y));

    const double b_dot_d_perp = bx*dy - by*dx;
    if (fabs(b_dot_d_perp) < std::numeric_limits<double>::epsilon())
        return false; // lines are parallel

    // Get C of both segments to solve intersection point:
    const double cx = (p1s.x - p0s.x);
    const double cy = (p1s.y - p0s.y);

    // t0 is intersection distance along the segment p0s -> p0e:
    t0 = (cx*dy - cy*dx) / b_dot_d_perp;
    if (t0 < 0.0 || t0 > 1.0)
        return false;

    // t1 is intersection distance along the segment p1s -> p1e:
    t1 = (cx*by - cy*bx) / b_dot_d_perp;
    if (t1 < 0.0 || t1 > 1.0)
        return false;

    // Get intersection point along the segment p0s -> p0e:
    intersectP.x = float(double(p0s.x) + t0*bx);
    intersectP.y = float(double(p0s.y) + t0*by);

#if 0
    std::cout << "lineIntersectionPoint():" << std::endl;
    std::cout << " p0s[" << p0s.x << " " << p0s.y << "]";
    std::cout << " p0e[" << p0e.x << " " << p0e.y << "]";
    std::cout << " p1s[" << p1s.x << " " << p1s.y << "]";
    std::cout << " p1e[" << p1e.x << " " << p1e.y << "]";
    std::cout << std::endl;
    std::cout << " u=" << u << " t=" << t;
    std::cout << " i[" << i.x << " " << i.y << "]";
    std::cout << std::endl;
#endif

    return true;
}
#endif


} // namespace zpr

// end of zprender/Mesh.cpp

//
// Copyright 2020 DreamWorks Animation
//
