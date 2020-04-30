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

/// @file Fuser/MeshPrimitive.cpp
///
/// @author Jonathan Egstad

#include "MeshPrimitive.h"

#include "Mat4.h"
#include "NukeGeoInterface.h"
#include "MeshUtils.h" // for calcPointNormals()

#include <DDImage/PrimitiveContext.h>
#include <DDImage/Material.h>
#include <DDImage/gl.h>

#include <assert.h>

// Uncomment this to get some addl OpenGL indicators of the mesh's structure:
//#define DEBUG_MESHPRIM 1

namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*static*/ const char* MeshPrimitive::TessellateContext::name = "FsrMeshPrimitiveTessellate";


/*! 
*/
static Fsr::Node* buildMeshPrimitive(const char*        builder_class,
                                     const Fsr::ArgSet& args,
                                     Fsr::Node*         parent)
{
    return new MeshPrimitive(args, parent);
}

// Register fsrMeshPrimitive plugin:
/*static*/ FSR_EXPORT
const Fsr::Node::Description
MeshPrimitive::description("fsrMeshPrimitive"/*plugin name*/, buildMeshPrimitive/*ctor*/);



//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*! Copy ctor to support DD::Image::Primitive::duplicate()
*/
MeshPrimitive::MeshPrimitive(const MeshPrimitive& b) :
    Fsr::PointBasedPrimitive(b.m_frame),
    m_tessellator(NULL)
{
    this->copy(&b);
}


/*! Fsr::Node::create() entry point. Ignores parent.
*/
MeshPrimitive::MeshPrimitive(const Fsr::ArgSet& args,
                             Fsr::Node*         parent) :
    Fsr::PointBasedPrimitive(args),
    m_tessellator(NULL)
{
    //if (debug())
    //    std::cout << "MeshPrimitive::ctor(" << this << ") args[" << m_args << "]" << std::endl;
}


/*!
*/
MeshPrimitive::MeshPrimitive(const Fsr::ArgSet& args,
                             double             frame,
                             size_t             nVerts,
                             const uint32_t*    faceVertPointIndices,
                             size_t             nFaces,
                             const uint32_t*    nVertsPerFace) :
    Fsr::PointBasedPrimitive(args, frame),
    m_tessellator(NULL)
{
    //if (debug())
    //    std::cout << "MeshPrimitive::ctor(" << this << ") frame=" << frame << std::endl;
    addFaces(nVerts,
             faceVertPointIndices,
             nFaces,
             nVertsPerFace);
}


/*!
*/
MeshPrimitive::MeshPrimitive(const Fsr::ArgSet&     args,
                             double                 frame,
                             const Fsr::Uint32List& faceVertPointIndices,
                             const Fsr::Uint32List& nVertsPerFace) :
    Fsr::PointBasedPrimitive(args, frame),
    m_tessellator(NULL)
{
    //if (debug())
    //    std::cout << "MeshPrimitive::ctor(" << this << ") frame=" << frame << std::endl;
    addFaces(faceVertPointIndices.size(),
             faceVertPointIndices.data(),
             nVertsPerFace.size(),
             nVertsPerFace.data());
}


//---------------------------------------------------------------------------------


/*!
*/
void
MeshPrimitive::addFace(uint32_t        nFaceVerts,
                       const uint32_t* faceVertPointIndices)
{
    if (nFaceVerts < 3)
        return; // face must have at least 3 verts

    const uint32_t nVertsPerFace[] = { nFaceVerts };
    addFaces(nFaceVerts, faceVertPointIndices, 1/*nFaces*/, nVertsPerFace);
}


/*!
*/
void
MeshPrimitive::addFaces(size_t          nVerts,
                        const uint32_t* faceVertPointIndices,
                        size_t          nFaces,
                        const uint32_t* nVertsPerFace)
{
    if (nVerts < 3 || nFaces == 0)
        return;

    // Pad up capacity:
    if (vertex_.size()+nVerts >= vertex_.capacity())
        vertex_.reserve(std::max(vertex_.capacity()+nVerts,
                                 size_t(double(vertex_.capacity())*1.5)));
    if (m_num_verts_per_face.size()+nFaces >= m_num_verts_per_face.capacity())
    {
        const size_t new_face_reserve = std::max(m_num_verts_per_face.capacity()+nFaces,
                                                 size_t(double(m_num_verts_per_face.capacity())*1.5));
        m_num_verts_per_face.reserve(new_face_reserve);
        m_vert_start_per_face.reserve(new_face_reserve);
    }

    // Current vertex count is the starting vert of the new face:
    uint32_t vstart = (uint32_t)vertex_.size();
    for (uint32_t i=0; i < nVerts; ++i)
        vertex_.push_back(faceVertPointIndices[i]);

    // Add vert start/count:
    for (size_t f=0; f < nFaces; ++f)
    {
        const uint32_t face_verts = nVertsPerFace[f];
        m_num_verts_per_face.push_back(face_verts);
        m_vert_start_per_face.push_back(vstart);
        vstart += face_verts;
    }

    // Adding a face destroys the edge info and bbox:
    removeEdges();
    m_local_bbox.setToEmptyState();
}


//---------------------------------------------------------------------------------


/*! Required method to support DD::Image::Primitive::duplicate().
    This should copy any vector<>'s explicitly.
*/
void MeshPrimitive::copy(const MeshPrimitive* b)
{
    if (this == b)
        return;
    // Let parent copy itself:
    Fsr::PointBasedPrimitive::copy(b);

    // Copy local variables:
    m_num_verts_per_face  = b->m_num_verts_per_face;
    m_vert_start_per_face = b->m_vert_start_per_face;
    //
    m_edge_list           = b->m_edge_list;
    //
    m_tessellator         = b->m_tessellator;
}


/*! Return the primitive type index...

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::PrimitiveType
MeshPrimitive::getPrimitiveType() const
{
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
    return (DD::Image::PrimitiveType)FUSER_MESH_PRIMITIVE_TYPE;
#else
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wconversion"
    return (DD::Image::PrimitiveType)FUSER_MESH_PRIMITIVE_TYPE;
#   pragma GCC diagnostic pop
#endif
}


/*! Print some info about this primitive.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::print_info() const
{
    std::cout << "vertices[";
    const size_t nVerts = numVerts();
    for (size_t i=0; i < nVerts; ++i)
        std::cout << " " << vertex_[i];
    std::cout << "]" << std::endl;

    std::cout << "faces[";
    const size_t nFaces = numFaces();
    for (size_t f=0; f < nFaces; ++f)
    {
        uint32_t v = m_vert_start_per_face[f];
        const uint32_t last_vert = v + m_num_verts_per_face[f];
        std::cout << " " << m_num_verts_per_face[f] << "[";
        for (; v < last_vert; ++v)
            std::cout << " " << vertex_[v];
        std::cout << " ]";
    }
    std::cout << "]" << std::endl;
}


//===================================================================================
// Vertex/Face handling:
//===================================================================================


/*! Returns the xyz center average of the subface.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
MeshPrimitive::face_average_center(int                         face,
                                   const DD::Image::PointList* point_list) const
{
#if DEBUG
    assert(face >= 0 && face < (int)numFaces());
#endif
    //std::cout << "MeshPrimitive::face_average_center('" << getPath() << "') face=" << face << std::endl;
    Fsr::Vec3f center(0.0f, 0.0f, 0.0f);

    const uint32_t nFaceVerts = m_num_verts_per_face[face];
    const Fsr::Vec3fList& points = getPointLocations(point_list);
    if (points.size() == 0 || nFaceVerts < 3)
        return center; // don't crash

    uint32_t v = m_vert_start_per_face[face];
    const uint32_t last_vert = v + nFaceVerts;
    for (; v < last_vert; ++v)
        center += getVertexPoint(v, points);
    center /= float(m_num_verts_per_face[face]);

    return (m_have_xform) ? m_xform.transform(center) : center;
}


/*! Returns the local-transformed (m_xform applied) xyz center average of a face
    and its aabb bbox.

    This is only used afaict for BVH construction by DDImage.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
MeshPrimitive::average_center_and_bounds(int                         face,
                                         const DD::Image::PointList* point_list,
                                         DD::Image::Vector3&         min,
                                         DD::Image::Vector3&         max) const
{
#if DEBUG
    assert(face >= 0 && face < (int)numFaces());
#endif
    //std::cout << "MeshPrimitive::average_center_and_bounds('" << getPath() << "') face=" << face << std::endl;

    const Fsr::Vec3fList& points = getPointLocations(point_list);

    const uint32_t nFaceVerts = m_num_verts_per_face[face];
    if (points.size() == 0 || nFaceVerts < 3)
    {
        // Returning this may crash Nuke...
        min.set(0,0,0);
        max.set(0,0,0);
        return DD::Image::Vector3(0.0f, 0.0f, 0.0f);
    }

    // Calc face bbox and center in double-precision if transformed, then return a
    // single-precision result:
    Fsr::Box3d bbox;
    Fsr::Vec3d center;
    uint32_t vindex = m_vert_start_per_face[face];
    if (m_xform.isIdentity())
    {
        const Fsr::Vec3f& P0 = getVertexPoint(vindex++, points);
        bbox.set(P0);
        center = P0;
        for (size_t i=1; i < nFaceVerts; ++i)
        {
            const Fsr::Vec3f& P1 = getVertexPoint(vindex++, points);
            bbox.expand(P1, false/*test_empty*/);
            center += P1;
        }
    }
    else
    {
        Fsr::Vec3d P = m_xform.transform(getVertexPoint(vindex++, points));
        bbox.set(P);
        center = P;
        for (size_t i=1; i < nFaceVerts; ++i)
        {
            P = m_xform.transform(getVertexPoint(vindex++, points));
            bbox.expand(P, false/*test_empty*/);
            center += P;
        }
    }
    min = bbox.min;
    max = bbox.max;
    center /= float(nFaceVerts);

    //std::cout << "  bbox" << bbox << ", center=" << center << std::endl;

    return center.asDDImage();
}


/*! Return the face count this vertex connects to and fills in the list of faces.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ int
MeshPrimitive::get_vertex_faces(int               vert,
                                std::vector<int>& faces) const
{
#if DEBUG
    assert(vert < (int)numVerts());
#endif
    //std::cout << "get_vertex_faces('" << getPath() << "') vert=" << vert << std::endl;
    faces.clear();

    // Have to make this call writable and spoof the int list to an unsigned int list:
    const_cast<MeshPrimitive*>(this)->getVertexConnectedFaces(vert,
                                                         reinterpret_cast<Fsr::Uint32List&>(faces));
    //std::cout << "get_vertex_faces(" << vert << ")" << std::endl;
    //for (uint32_t i=0; i < faces.size(); ++i)
    //    std::cout << "  " << i << ": " << faces[i] << std::endl;

    return (int)faces.size();
}


/*! Return the face normal.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
MeshPrimitive::get_face_normal(int                         face,
                               const DD::Image::PointList* point_list) const
{
    //cstd::cout << "get_face_normal('" << getPath() << "') face=" << face << std::endl;
#if DEBUG
    assert(face >= 0 && face < (int)numFaces());
#endif
    const Fsr::Vec3fList& points = getPointLocations(point_list);
    if (points.size() == 0)
        return Fsr::Vec3f(0.0f, 0.0f, 0.0f); // don't crash

    const uint32_t first_vert = m_vert_start_per_face[face];
    const Fsr::Vec3f& p0 = getVertexPoint(first_vert, points);
    const Fsr::Vec3f& p1 = getVertexPoint(first_vert + 1, points);
    const Fsr::Vec3f& p2 = getVertexPoint(first_vert + m_num_verts_per_face[face] - 1, points);
    Fsr::Vec3f N = (p1 - p0).cross(p2 - p0);
    N.fastNormalize();

    return N;
}


/*! Find the average geometric normal of this vertex by adding the normals of the
   connected edges and normalizing the result.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
MeshPrimitive::get_geometric_normal(int                         vert,
                                    const DD::Image::PointList* point_list) const
{
    //std::cout << "get_geometric_normal('" << getPath() << "') vert=" << vert << std::endl;
#if DEBUG
    assert(vert < (int)numVerts());
#endif
    const Fsr::Vec3fList& points = getPointLocations(point_list);
    if (points.size() == 0)
        return Fsr::Vec3f(0.0f, 0.0f, 0.0f); // don't crash

    Fsr::Vec3f N(0.0f, 0.0f, 0.0f);

    Fsr::Uint32List connected_verts;
    const_cast<MeshPrimitive*>(this)->getVertexConnectedVerts(vert, connected_verts);
    const size_t nConnectedVerts = connected_verts.size();
    if (nConnectedVerts < 2)
        return N;  // not enough verts to calc a normal...

    const Fsr::Vec3f& p0 = getVertexPoint(vert, points);
    for (size_t i=0; i < nConnectedVerts - 1; ++i)
        N += (getVertexPoint(connected_verts[i], points) - p0).cross(getVertexPoint(connected_verts[i + 1], points) - p0);
    N += (getVertexPoint(connected_verts[nConnectedVerts - 1], points) - p0).cross(getVertexPoint(connected_verts[0], points) - p0);
    N.fastNormalize();

    return N;
}


/*! Returns the geometric normal of the vertex.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
MeshPrimitive::vertex_normal(unsigned                    vert,
                             const DD::Image::PointList* point_list) const
{
    //std::cout << "get_vertex_normal('" << getPath() << "') vert=" << vert << std::endl;
#if DEBUG
    assert(vert < (unsigned)numVerts());
#endif
#if 1
    // TODO: Just return the geometric normal for now - should this be
    // the lighting normal (accessing the N attribute)?
    return get_geometric_normal(vert, point_list);
#else
    //
    return Ns;
#endif
}


/*! Intersect a ray with a face.
*/
/*virtual*/ bool
MeshPrimitive::intersectFace(const Fsr::RayContext& Rtx,
                             uint32_t               face,
                             const Fsr::Vec3fList&  points,
                             double&                t)
{
    const uint32_t nFaceVerts = m_num_verts_per_face[face];
    if (points.size() == 0 || nFaceVerts < 3)
        return false;

    Fsr::Vec3d vert_origin(0.0, 0.0, 0.0);
    Fsr::Vec2f uv;

    const uint32_t v0 = m_vert_start_per_face[face];
    const uint32_t last_vert = v0 + nFaceVerts - 1;
    // TODO: determine if the xform needs to be taken into acct here:
    if (1)//(m_xform.isIdentity())
    {
        const Fsr::Vec3f& vA = getVertexPoint(v0, points);
        for (uint32_t v=v0+1; v < last_vert; ++v)
        {
            if (Fsr::intersectTriangle(vert_origin,
                                       vA,
                                       getVertexPoint(v,   points),
                                       getVertexPoint(v+1, points),
                                       Rtx, uv, t))
                return true;
        }
    }
    else
    {
        const Fsr::Vec3f vA = m_xform.transform(getVertexPoint(v0, points));
        for (uint32_t v=v0+1; v < last_vert; ++v)
        {
            if (Fsr::intersectTriangle(vert_origin,
                                       vA,
                                       m_xform.transform(getVertexPoint(v,   points)),
                                       m_xform.transform(getVertexPoint(v+1, points)),
                                       Rtx, uv, t))
                return true;
        }
    }

    return false;
}


/*! If a collision is detected, information is returned in the collision result

    DD::Image::Primitive virtual method.
*/
/*virtual*/ bool
MeshPrimitive::IntersectsRay(const DD::Image::Ray&       ray,
                             int                         face,
                             const DD::Image::PointList* point_list,
                             DD::Image::CollisionResult* result) const
{
#if DEBUG
    assert(point_list);
#endif
    //std::cout << "MeshPrimitive::IntersectsRay(" << face << ")" << std::endl;
    const Fsr::Vec3fList& points = getPointLocations(point_list);

    // Intersect against each triangle in the face:
    const Fsr::RayContext Rtx(ray);
    double t;
    if (const_cast<MeshPrimitive*>(this)->intersectFace(Rtx, face, points, t))
    {
        // Store collision result (can sometimes be NULL):
        if (result)
        {
            result->_collisionTime     = float(t);
            result->_collisionNormal   = get_face_normal(face, point_list);
            result->_collisionPosition = Rtx.getPositionAt(t);
            //result->_collisionPrimitiveIdx = 0; // set outside this method
            result->_collisionFaceIdx  = face;
            result->_collisionGeo      = 0;
        }
        //std::cout << "MeshPrimitive::IntersectsRay(" << face << ") hit! t=" << t << std::endl;
        return true;
    }

    return false;
}


/*!
*/
bool
MeshPrimitive::calcPointNormals(const DD::Image::PointList* point_list,
                                Fsr::Vec3fList&             point_normals) const
{
    const Fsr::Vec3fList& local_points = getPointLocations(point_list);
    return Fsr::calcPointNormals(local_points.size(),
                                 local_points.data(),
                                 vertex_.size(),
                                 vertex_.data(),
                                 m_num_verts_per_face.size(),
                                 m_num_verts_per_face.data(),
                                 false/*all_tris*/,
                                 false/*all_quads*/,
                                 point_normals);
}


/*!
*/
bool
MeshPrimitive::calcVertexNormals(const DD::Image::PointList* point_list,
                                 Fsr::Vec3fList&             vertex_normals) const
{
    Fsr::Vec3fList point_normals;
    if (calcPointNormals(point_list, point_normals))
    {
        const size_t nVerts = numVerts();
        vertex_normals.resize(nVerts);
        for (size_t v=0; v < nVerts; ++v)
            vertex_normals[v] = point_normals[vertex_[v]];
        return true;
    }
    else
    {
        vertex_normals.clear();
        return false;
    }
}


//===================================================================================
// Rendering:
//===================================================================================


/*! Fill in the VertexBuffers with the attribute values from this Primitive's
    GeoInfo attributes.

    If the mesh is supposed to be a subdivision mesh then subdivide it now,
    calculating new normals.

    If the mesh is missing an assigned normals attribute this will calculate
    normals for the mesh and populate the N(ormals) vertex buffer.
*/
/*virtual*/ void
MeshPrimitive::fillVertexBuffers(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                 VertexBuffers&                             vbuffers) const
{
    if (numVerts() == 0 || numFaces() == 0)
        return;

    assert(rtess_ctx.ptx && rtess_ctx.ptx->geoinfo()); // should never be NULL!
    const DD::Image::GeoInfo& info = *rtess_ctx.ptx->geoinfo();


    // Base class fills the buffers, then we may subdivide or update normals:
    PointBasedPrimitive::fillVertexBuffers(rtess_ctx, vbuffers);

    // Copy the face list data:
    vbuffers.resizePolyFaces(m_num_verts_per_face.size(), m_num_verts_per_face.data());


    // Grab subd args from the GeoInfo object:
    // TODO: should these args be acquired from the Fsr::Node? If a GeoOp
    // was used to modify the GeoInfo then it may be more up to date than the
    // Fsr::Node, but we really should have an override rule for this
    // behavior...
    // TODO: define these subd string constants somewhere common
    const int         subd_current_level =    Fsr::getObjectInt(info, "subd:current_level");
    int               subd_render_level  =    Fsr::getObjectInt(info, "subd:render_level" );
    std::string       subd_tessellator   = Fsr::getObjectString(info, "subd:tessellator"  );
    std::string       subd_scheme        = Fsr::getObjectString(info, "subd:scheme"       );

    //std::cout << "  MeshPrimitive::fillVertexBuffers('" << getPath() << "') frame=" << m_frame;
    //std::cout << ", args[" << m_args << "]";
    //std::cout << ", nPoints=" << info.points();
    //std::cout << ", numVerts=" << numVerts();
    //std::cout << ", numFaces=" << numFaces();
    //std::cout << ", subd_scheme='" << subd_scheme << "'";
    //std::cout << ", subd_current_level=" << subd_current_level;
    //std::cout << ", subd_render_level=" << subd_render_level;
    //std::cout << ", subd_tessellator='" << subd_tessellator << "'";
    //std::cout << std::endl;

    // Get the tesselator node to execute:
    if (subd_render_level > subd_current_level && m_tessellator == NULL)
    {
        if (subd_tessellator.empty())
            subd_tessellator = "OpenSubdiv"; // default

        // TODO: should we pick different nodes depending on subd_scheme?
        const_cast<MeshPrimitive*>(this)->m_tessellator = Fsr::Node::create(subd_tessellator.c_str()/*node_class*/,
                                                                            Fsr::ArgSet(),
                                                                            NULL/*parent*/);
        // Try to find the default subdivision tessellator plugin:
        // TODO: make this a built-in Fuser node:
        if (!m_tessellator)
        {
            subd_tessellator = "SimpleSubdiv";
            const_cast<MeshPrimitive*>(this)->m_tessellator = Fsr::Node::create(subd_tessellator.c_str()/*node_class*/,
                                                                                Fsr::ArgSet(),
                                                                                NULL/*parent*/);
        }

        if (!m_tessellator)
        {
            // fuser warning, no tessellator available, disable subdivision
            subd_render_level = 0; // disable subdivision
        }
    }

    if (subd_render_level > subd_current_level)
    {
        // Apply subdivision!
        assert(m_tessellator);

        // View vector - use this to help adaptive subdivision?
        //Vector3 V = render_scene->matrix(CAMERA_MATRIX).z_axis();
        //Vector3 V = render_scene->matrix(CAMERA_iMATRIX).ntransform(Vector3(0,0,1));

        // TODO: should we simply copy all args prefixed with 'subd:' to
        // subd_args? Or just pass a copy of this Node's args?
        Fsr::NodeContext subd_args;
        subd_args.setInt(   "subd:current_level", subd_current_level);
        subd_args.setInt(   "subd:target_level",  subd_render_level );
        subd_args.setString("subd:scheme",        subd_scheme       );
        //subd_args.setBool("subd:snap_to_limit", getBool("subd:snap_to_limit", false));

        TessellateContext tessellate_ctx(this, &vbuffers);
        int res = m_tessellator->execute(subd_args,           /*target_context*/
                                         tessellate_ctx.name, /*target_name*/
                                         &tessellate_ctx      /*target*/);
        if (res < 0)
        {
            if (res == -2 && debug())
                std::cerr << "MeshPrimitive::fillVertexBuffers()" << " error '" << m_tessellator->errorMessage() << "'" << std::endl;
        }
        else
        {
            // Update normals:
//            calcVertexBufferNormals(vbuffers);

            //std::cout << "  MeshPrimitive::tessellate('" << getPath() << "') frame=" << m_frame;
            //std::cout << ", nPoints=" << vbuffers.numPoints();
            //std::cout << ", nVerts=" << vbuffers.numVerts();
            //std::cout << ", nFaces=" << vbuffers.numFaces();
            //std::cout << ", allTris=" << vbuffers.allTris;
            //std::cout << ", allQuads=" << vbuffers.allQuads;
            //std::cout << std::endl;
        }
    }
    else
    {
        // No subdivision or no tessellator, update normals if there's no attribute:
        if (!info.N_ref)
        {
            // Always calculate vertex normals and enable their interpolation even if lighting is disabled:
            calcVertexNormals(info.point_list(), vbuffers.N);
            vbuffers.interpolateChannels += DD::Image::ChannelSetInit(DD::Image::Mask_N_);
        }
    }
}


//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------


/*! These are called from the hijacked DD::Image::Primitive
    calls to fill in a VertexBuffer.

    TODO: finish this code thought.
*/
/*virtual*/ void
MeshPrimitive::_drawWireframe(const PrimitiveViewerContext& vtx,
                              VertexBuffers&                vbuffers)
{
std::cout << "MeshPrimitive::_drawWireframe('" << this << "')" << std::endl;
}


/*! These are called from the hijacked DD::Image::Primitive
    calls to fill in a VertexBuffer.

    TODO: finish this code thought.
*/
/*virtual*/ void
MeshPrimitive::_drawSolid(const PrimitiveViewerContext& vtx,
                          VertexBuffers&                vbuffers)
{
std::cout << "MeshPrimitive::_drawSolid('" << this << "')" << std::endl;
}


//===================================================================================
// OpenGL drawing methods:
//===================================================================================


/*! Draws the mesh as a solid surface with optional faceset coloring support.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_solid(DD::Image::ViewerContext*    ctx,
                          DD::Image::PrimitiveContext* ptx,
                          DD::Image::Primitive*        prev_prim) const
{
    assert(ptx && ptx->geoinfo()); // should never be NULL!
    //std::cout << "MeshPrimitive::draw_solid('" << this << "')" << std::endl;

    if (prev_prim && !dynamic_cast<MeshPrimitive*>(prev_prim))
        glEnd();

    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    const DD::Image::AttribContext* Nattrib  = ptx->geoinfo()->get_typed_attribcontext("N",  DD::Image::NORMAL_ATTRIB);
    const DD::Image::AttribContext* UVattrib = ptx->geoinfo()->get_typed_attribcontext("uv", DD::Image::VECTOR4_ATTRIB);
    const DD::Image::AttribContext* Cfattrib = ptx->geoinfo()->get_typed_attribcontext("Cf", DD::Image::VECTOR4_ATTRIB);

    const bool normals = (Nattrib && Nattrib->not_empty());
    const bool texturing = (ctx->display3d(ptx->geoinfo()->display3d) >= DD::Image::DISPLAY_TEXTURED &&
                             UVattrib && UVattrib->not_empty());
    const bool color = (Cfattrib && Cfattrib->not_empty());
    if (texturing)
        glColor4f(1,1,1,1);

    // Have to test for presence of 'RayShader' knob due to static linking of dw_nuke2.1:
#if 1
    const bool run_vertex_shader = false;
#else
    const bool has_ray_shader = ((ptx->geoinfo()->material)?ptx->geoinfo()->material->knob("RayShader")!=NULL:false);
    const bool run_vertex_shader = (has_ray_shader &&
                                     (static_cast<dw::nuke::RayShader*>(ptx->geoinfo()->material))->vertexShaderActive());
    //std::cout << "MeshPrimitive::draw_solid('" << this << "') has_ray_shader=" << has_ray_shader << ", run_vertex_shader=" << run_vertex_shader << std::endl;
#endif

    DD::Image::Scene        tmp_scene;
    DD::Image::VArray       tmp_varray;
    DD::Image::MatrixArray* saved_transforms    = ptx->transforms();
    DD::Image::MatrixArray* saved_mb_transforms = ptx->mb_transforms();

    if (run_vertex_shader)
    {
        tmp_scene.transforms()->set_object_matrix(ptx->geoinfo()->matrix);
        tmp_scene.projection_mode(DD::Image::CameraOp::LENS_SPHERICAL);

        // Not sure why the transform pointer in the PrimitiveContext
        // would be NULL at this point... It should at least contain the
        // GeoInfo's matrix. Get around this by assigning it now:
        ptx->set_transforms(tmp_scene.transforms(), NULL/*mb_transforms*/);
    }

    const size_t nFaces = numFaces();
    for (size_t f=0; f < nFaces; ++f)
    {
        glBegin(GL_POLYGON);

        uint32_t v = m_vert_start_per_face[f];
        const uint32_t last_vert = v + m_num_verts_per_face[f];
        for (; v < last_vert; ++v)
        {
            const uint32_t pi = vertex_[v];
            const_cast<uint32_t*>(ptx->indices())[DD::Image::Group_Vertices] = v;
            const_cast<uint32_t*>(ptx->indices())[DD::Image::Group_Points  ] = pi;

            if (run_vertex_shader)
                this->vertex_shader(v, &tmp_scene, ptx, tmp_varray);

            // Normal:
            if (normals)
            {
                const DD::Image::Vector3& N = (run_vertex_shader)?tmp_varray.N():Nattrib->normal(ptx->indices());
                //std::cout << "N[" << N.x << " " << N.y << " " << N.z << "]" << std::endl;
                if (fabsf(N.x) > 0.0f || fabsf(N.y) > 0.0f || fabsf(N.z) > 0.0f)
                    glNormal3fv(N.array());
                else
                    glNormal3fv(normal_.array());

            }
            else
            {
                glNormal3fv(normal_.array());
            }

            // Texture/color:
            if (texturing)
            {
                const DD::Image::Vector4& UV = (run_vertex_shader)?tmp_varray.UV():UVattrib->vector4(ptx->indices());
                glTexCoord4fv(UV.array());

            }
            else if (color)
            {
                const DD::Image::Vector4& Cf = (run_vertex_shader)?tmp_varray.Cf():Cfattrib->vector4(ptx->indices());
                glColor4f(powf(Cf.x, 0.45f),
                          powf(Cf.y, 0.45f),
                          powf(Cf.z, 0.45f),
                          Cf.w);
            }

            // Vertex position:
#if 1
            const DD::Image::Vector3& PL = (run_vertex_shader) ? tmp_varray.PL() : points[pi];
            glVertex3fv(PL.array());
#else
            glVertex3fv(points[pi].array());
#endif
        }
        glEnd(); // GL_POLYGON
    }

    glPopMatrix();

    if (run_vertex_shader)
    {
        // Restore the transform pointer in the PrimitiveContext to its previous state:
        ptx->set_transforms(saved_transforms, saved_mb_transforms);
    }
}


/*! Draws the mesh as a wireframe where the outside perimeter is drawn as
    thick solid lines and the subfaces drawn as dashed lines (this should be a
    preference but there's no support for that yet.)

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_wireframe(DD::Image::ViewerContext*    ctx,
                              DD::Image::PrimitiveContext* ptx,
                              DD::Image::Primitive*        prev_prim) const
{
    assert(ptx && ptx->geoinfo()); // should never be NULL!
    //std::cout << "MeshPrimitive::draw_wireframe('" << this << "')" << std::endl;

    if (prev_prim && !dynamic_cast<MeshPrimitive*>(prev_prim)) glEnd();

    if (vertices() == 0)
        return;

    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    // Need half-edges for this:
    const bool temp_edges = haveEdges();
    const_cast<MeshPrimitive*>(this)->buildEdges(false/*force*/);

#ifdef DEBUG_MESHPRIM
    if (debug_face >= 0 && debug_face < (int)numFaces())
    {
        glPushAttrib(GL_CURRENT_BIT);
        glColor3f(0.0f, 0.0f, 0.2f);
        draw_solid_face(debug_face, ctx, ptx);
        std::vector<uint32_t> connected_faces;
        const_cast<MeshPrimitive*>(this)->getConnectedFaces(debug_face, connected_faces);
        glColor3f(0.5f, 0.0f, 0.0f);
        for (size_t i=0; i < connected_faces.size(); ++i)
           draw_solid_face(connected_faces[i], ctx, ptx);

        glPopAttrib(); // GL_CURRENT_BIT
    }
#endif

    Fsr::Vec4f cur_blend_color;
    glGetFloatv(GL_BLEND_COLOR, cur_blend_color.array());

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT);
    {
        glBlendColor(1.0f, 1.0f, 1.0f, 0.25f);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);

        glEnable(GL_LINE_STIPPLE);
        //glLineStipple(1, 0x1010); // very dotty
        glLineStipple(1, 0xeee0); // dashed
        //
        const size_t nFaces = numFaces();
        for (size_t f=0; f < nFaces; ++f)
        {
            glBegin(GL_LINE_LOOP);

            uint32_t v = m_vert_start_per_face[f];
            const uint32_t last_vert = v + m_num_verts_per_face[f];
            for (; v < last_vert; ++v)
                glVertex3fv(getVertexPoint(v, points).array());

            glEnd();
        }

        // Draw boundary edges solid and thicker:
        glDisable(GL_LINE_STIPPLE);
        GLint cur_width;
        glGetIntegerv(GL_LINE_WIDTH, &cur_width);
        glLineWidth(float(cur_width*2));
        glBegin(GL_LINES);
        const uint32_t nEdges = (uint32_t)m_edge_list.size();
        for (uint32_t i=0; i < nEdges; ++i)
        {
            const Fsr::HalfEdge& he = m_edge_list[i];
            // Only draw the edges that are boundary:
            if (!he.isBoundaryEdge())
                continue;
            glVertex3fv(getVertexPoint(m_edge_list[he.prev].vert, points).array());
            glVertex3fv(getVertexPoint(he.vert, points).array());
        }
        glEnd(); // GL_LINES
    }
    glPopAttrib(); // GL_COLOR_BUFFER_BIT | GL_LINE_BIT

#ifdef DEBUG_MESHPRIM
    if (debug_vert >= 0 && debug_vert < (int)numVerts())
    {
        std::vector<uint32_t> verts;
        const_cast<MeshPrimitive*>(this)->getVertexConnectedVerts(debug_vert, verts);
        const uint32_t nVerts = (uint32_t)verts.size();
        if (nVerts > 0)
        {
            glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);
            glColor3f(0.0f, 1.0f, 0.0f);
            const Fsr::Vec3f& p0 = getVertexPoint(debug_vert, points);
            GLint cur_width;
            glGetIntegerv(GL_LINE_WIDTH, &cur_width);
            glLineWidth(float(cur_width*2));
            glBegin(GL_LINES);
            for (uint32_t i=0; i < nVerts; ++i)
            {
                const Fsr::Vec3f& p1 = getVertexPoint(verts[i], points);
                glVertex3fv(p0.array());
                glVertex3fv(p1.array());
            }
            glEnd(); // GL_LINES
            glPopAttrib(); // GL_CURRENT_BIT | GL_LINE_BIT
        }
    }
#endif

    // If we needed to create edges to draw the mesh, delete them now:
    if (temp_edges)
        const_cast<MeshPrimitive*>(this)->removeEdges();

    glPopMatrix();
}


/*! Draw the single face of the mesh.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_solid_face(int                          face,
                               DD::Image::ViewerContext*    ctx,
                               DD::Image::PrimitiveContext* ptx) const
{
    if (face < 0 || face >= (int)numFaces())
        return;
    assert(ptx && ptx->geoinfo()); // should never be NULL!

    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    glBegin(GL_POLYGON);

    uint32_t v = m_vert_start_per_face[face];
    const uint32_t last_vert = v + m_num_verts_per_face[face];
    for (; v < last_vert; ++v)
        glVertex3fv(getVertexPoint(v, points).array());

    glEnd();

    glPopMatrix();
}


//===================================================================================
// OpenGL component drawing:
//===================================================================================


/*! Draw all the face normals.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_primitive_normal(DD::Image::ViewerContext*    ctx,
                                     DD::Image::PrimitiveContext* ptx) const
{
    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    glBegin(GL_LINES);
    const size_t nFaces = numFaces();
    for (size_t f=0; f < nFaces; ++f)
    {
        const DD::Image::Vector3 p = face_average_center((unsigned)f, ptx->geoinfo()->point_list());
        const DD::Image::Vector3 n = get_face_normal((unsigned)f, ptx->geoinfo()->point_list());
        DD::Image::Primitive::draw_normal(p, n, ctx, ptx);
    }
    glEnd(); // GL_LINES

    const DD::Image::Attribute* VEL =
        ptx->geoinfo()->get_typed_group_attribute(DD::Image::Group_Points, "VEL", DD::Image::VECTOR3_ATTRIB);
    if (!VEL)
       VEL = ptx->geoinfo()->get_typed_group_attribute(DD::Image::Group_Points, "vel", DD::Image::VECTOR3_ATTRIB);
    if (VEL && VEL->size() == ptx->geoinfo()->points())
    {
        glColor4f(0.35f, 0.35f, 0.0f, 1.0f);

        GLint cur_width;
        glGetIntegerv(GL_LINE_WIDTH, &cur_width);
        glLineWidth(float(cur_width*2));

        glBegin(GL_LINES);
        for (size_t f=0; f < nFaces; ++f)
        {
            uint32_t v = m_vert_start_per_face[f];
            const uint32_t last_vert = v + m_num_verts_per_face[f];
            for (; v < last_vert; ++v)
            {
                //glColor3f(clamp(p_random((int)i*3+0)),
                //          clamp(p_random((int)i*3+1)),
                //          clamp(p_random((int)i*3+2)));
                DD::Image::Vector3 P = getVertexPoint(v, points);
                glColor4f(0.35f, 0.35f, 0.0f, 0.1f);
                glVertex3fv(P.array());
                P += VEL->vector3(vertex_[v]);
                glColor4f(0.35f, 0.35f, 0.0f, 1.0f);
                glVertex3fv(P.array());
            }
        }
        glEnd();
    }

    glPopMatrix();
}


/*! Draw the mesh's primitive index at vertex 0 (rather than the center.)

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_primitive_num(DD::Image::ViewerContext*    ctx,
                                  DD::Image::PrimitiveContext* ptx) const
{
    PointBasedPrimitive::draw_primitive_num(ctx, ptx);

#ifdef DEBUG_MESHPRIM
    // DEBUG - show the face info in GL:
    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    char buf[128];

    const size_t nFaces = numFaces();
#if 1
    for (size_t f=0; f < nFaces; ++f)
    {
        const DD::Image::Vector3 P = face_average_center((unsigned)f, ptx->geoinfo()->point_list());
        glRasterPos3f(P.x, P.y, P.z);
        snprintf(buf, 128, "%lu", f);
        DD::Image::gl_text(buf);
    }
#else
    // Draw the sub-face numbers:
    // TODO: make this a new draw option in the ViewerContext?
    const DD::Image::Attribute* tileid_attrib =
        ptx->geoinfo()->get_group_attribute(DD::Image::Group_Vertices, "tileid");
    if (tileid_attrib)
    {
        for (size_t f=0; f < nFaces; ++f)
        {
            const DD::Image::Vector3 P = face_average_center((unsigned)f, ptx->geoinfo()->point_list());
            glRasterPos3f(P.x, P.y, P.z);
            snprintf(buf, 128, "%lu:%d", f, tileid_attrib->integer(m_vert_start_per_face[f]));
            DD::Image::gl_text(buf);
        }
    }
    else
    {
        for (size_t f=0; f < nFaces; ++f)
        {
            const DD::Image::Vector3 P = face_average_center((unsigned)f, ptx->geoinfo()->point_list());
            glRasterPos3f(P.x, P.y, P.z);
            snprintf(buf, 128, "%lu", f);
            DD::Image::gl_text(buf);
        }
    }
#endif

    glPopMatrix();
#endif
}

//===================================
//  Vertex info:
//===================================


/*! DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_vertex_num(DD::Image::ViewerContext*    ctx,
                               DD::Image::PrimitiveContext* ptx) const
{
#ifdef DEBUG_MESHPRIM
    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    char buf[128];

    if (debug_face >= 0 && debug_face < (int)numFaces())
    {
        glPushAttrib(GL_CURRENT_BIT);
        glColor3f(1.0f, 1.0f, 1.0f);
        uint32_t v = m_vert_start_per_face[f];
        const uint32_t last_vert = v + m_num_verts_per_face[f];
        for (; v < last_vert; ++v)
        {
            const Fsr::Vec3f& p = getVertexPoint(v, points);
            glRasterPos3f(p.x, p.y, p.z);
            snprintf(buf, 128, "%u", v);
            DD::Image::gl_text(buf);
        }
        glPopAttrib(); // GL_CURRENT_BIT

    }
    else
    {
        if (debug_vert >= 0 && debug_vert < (int)numVerts())
        {
            const Fsr::Vec3f& p = getVertexPoint(debug_vert, points);
            glRasterPos3f(p.x, p.y, p.z);
            snprintf(buf, 128, "%d", debug_vert);
            DD::Image::gl_text(buf);
        }
        else
        {
            const uint32_t nVerts = (uint32_t)numVerts();
            for (uint32_t v=0; v < nVerts; ++v)
            {
               const Fsr::Vec3f& p = getVertexPoint(v, points);
               glRasterPos3f(p.x, p.y, p.z);
               snprintf(buf, 128, "%u", v);
               DD::Image::gl_text(buf);
            }
        }
    }

    glPopMatrix();
#else
    PointBasedPrimitive::draw_vertex_num(ctx, ptx);
#endif
}


/*! DD::Image::Primitive virtual method.
*/
/*virtual*/ void
MeshPrimitive::draw_vertex_normals(DD::Image::ViewerContext*    ctx,
                                   DD::Image::PrimitiveContext* ptx) const
{
#ifdef DEBUG_MESHPRIM
    assert(ptx && ptx->geoinfo()); // should never be NULL!

    const DD::Image::Attribute* N =
        ptx->geoinfo()->get_typed_group_attribute(DD::Image::Group_Vertices, "N", DD::Image::NORMAL_ATTRIB);
    if (!N || N->size() == 0)
        return;

    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    glBegin(GL_LINES);

    if (debug_face >= 0 && debug_face < (int)numFaces())
    {
        uint32_t v = m_vert_start_per_face[f];
        const uint32_t last_vert = v + m_num_verts_per_face[f];
        for (; v < last_vert; ++v)
            DD::Image::Primitive::draw_normal(getVertexPoint(v, points), N->normal(v), ctx, ptx);
    }
    else
    {
        if (debug_vert >= 0 && debug_vert < (int)numVerts())
        {
            DD::Image::Primitive::draw_normal(getVertexPoint(debug_vert, points), N->normal(debug_vert), ctx, ptx);
        }
        else
        {
            const uint32_t nVerts = (uint32_t)numVerts();
            for (uint32_t v=0; v < nVerts; ++v)
                DD::Image::Primitive::draw_normal(getVertexPoint(v, points), N->normal(v), ctx, ptx);
        }
    }
    glEnd(); // GL_LINES

    glPopMatrix();
#else
    PointBasedPrimitive::draw_vertex_normals(ctx, ptx);
#endif
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace Fsr


// end of Fuser/MeshPrimitive.cpp

//
// Copyright 2019 DreamWorks Animation
//
