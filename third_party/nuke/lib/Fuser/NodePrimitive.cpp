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

/// @file Fuser/NodePrimitive.cpp
///
/// @author Jonathan Egstad

#include "NodePrimitive.h"
#include "ExecuteTargetContexts.h" // for PrimitiveViewerContext
#include "ArgConstants.h"
#include "RayContext.h" // for ray/bbox intersection test

#include <DDImage/PrimitiveContext.h>
#include <DDImage/gl.h>

#if DEBUG_TIMES
#  include <sys/time.h>
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

namespace Fsr {

/*static*/ const char* NodePrimitive::load_modes[] = { "immediate", "deferred", 0 };
/*static*/ const char* NodePrimitive::lod_modes[]  = { "bbox", "standin", "proxy", "render", 0 };

#define NPOINTS         8   // 8 bbox corner points
#define NFACES          6   // 8 bbox faces
#define NVERTS_PER_FACE 4   // 4 verts per face
#define NVERTS          NVERTS_PER_FACE*NFACES

static const unsigned vert_to_point[NVERTS] =
{
    0, 1, 2, 3, // face 0
    1, 5, 6, 2, // face 1
    5, 4, 7, 6, // face 2
    4, 0, 3, 2, // face 3
    3, 2, 6, 7, // face 4
    4, 5, 1, 0  // face 5
};
static const unsigned vert_to_face[NVERTS*3] =
{
    // face connections per vert:
    0, 3, 5, // 0
    0, 1, 5, // 1
    0, 1, 4, // 2
    0, 3, 4, // 3
    1, 0, 5, // 4
    1, 2, 5, // 5
    1, 2, 4, // 6
    1, 0, 4, // 7
    2, 1, 5, // 8
    2, 3, 5, // 9
    2, 3, 4, // 10
    2, 1, 4, // 11
    3, 2, 5, // 12
    3, 0, 5, // 13
    3, 0, 4, // 14
    3, 2, 4, // 15
    4, 0, 3, // 16
    4, 0, 1, // 17
    4, 2, 1, // 18
    4, 2, 3, // 19
    5, 2, 3, // 20
    5, 2, 1, // 21
    5, 0, 1, // 22
    5, 0, 3, // 23
};
static const DD::Image::Vector3 face_normal[NFACES] =
{
    DD::Image::Vector3( 0, 0, 1),
    DD::Image::Vector3( 1, 0, 0),
    DD::Image::Vector3( 0, 0,-1),
    DD::Image::Vector3(-1, 0, 0),
    DD::Image::Vector3( 0, 1, 0),
    DD::Image::Vector3( 0,-1, 0)
};


//-----------------------------------------------------------------------------


/*! 
*/
static Fsr::Node* buildNodePrimitive(const char*        builder_class,
                                     const Fsr::ArgSet& args,
                                     Fsr::Node*         parent)
{
    return new NodePrimitive(args, parent);
}

// Register plugin:
/*static*/ FSR_EXPORT
const Fsr::Node::Description
NodePrimitive::description("fsrNodePrimitive"/*plugin name*/, buildNodePrimitive/*ctor*/);



//-----------------------------------------------------------------------------


/*! Copy ctor to support DD::Image::Primitive::duplicate()
*/
NodePrimitive::NodePrimitive(const NodePrimitive& b) :
    FuserPrimitive(b.m_frame)
{
    this->copy(&b);
}


/*!
*/
NodePrimitive::NodePrimitive(const char*        node_class,
                             const Fsr::ArgSet& args,
                             double             frame) :
    FuserPrimitive(args, frame),
    m_node(),
    m_proxy_lod(LOD_BBOX),
    m_render_lod(LOD_RENDER)
{
    //std::cout << "NodePrimitive::ctor(" << this << "): node_class='fuser" << node_class << "'" << std::endl;
    vertex_.resize(NVERTS);
    memcpy(vertex_.data(), vert_to_point, sizeof(unsigned)*NVERTS);

    // Attempt to instantiate the Fuser Node:
    m_node = Fsr::Node::create(node_class, args, NULL/*parent-node*/);
    if (!m_node)
    {
        std::cerr << "Fuser::NodePrimitive::ctor(): ";
        std::cerr << "warning, unable to create Fuser Node of class '" << node_class << "'";
        std::cerr << ", disabled" << std::endl;
    }
}



/*! Deletes the allocated Fsr::Node.
*/
NodePrimitive::~NodePrimitive()
{
    //std::cout << "NodePrimitive::dtor(" << this << "): m_node=" << m_node << std::endl;
    if (m_node)
        delete m_node;
}


/*! Copy the values from 'b' into this one.
    Fsr::Node pointer is copied too!

    TODO: guaranteed to crash, need to make it referenced-counted!
*/
void
NodePrimitive::copy(const NodePrimitive* b)
{
    //std::cout << "NodePrimitive::copy(" << this << ") b=" << b << std::endl;
    FuserPrimitive::copy(b);
    // Copy local vars:
    m_node       = b->m_node;       // TODO: not a good idea...m_node needs to be ref-counted!
    m_proxy_lod  = b->m_proxy_lod;
    m_render_lod = b->m_render_lod;
}


//-----------------------------------------------------------------------------


/*! In deferred-mode adds a single NodePrimitive to the output object
    which is the expanded on demand.

    Depending on the state of the Fsr::Node this may add a single
    point to the GeoInfo if nothing is loaded or the corner points of
    the bounds bbox if at least one valid node is loaded.

    Note - add only *one* NodePrimitive output GeometryList object.

    The single NodePrimitive can contain any number of Fsr::Nodes inside it,
    for example each node inside an Alembic file.

    Returns -1 on failure, otherwise the number of GeoInfos added to
    the GeometryList.
*/
/*static*/ int
NodePrimitive::addGeometryToScene(const char*                      fuser_class,
                                  int                              creation_mode,
                                  const Fsr::NodeContext&          node_args,
                                  Fsr::GeoOpGeometryEngineContext& geo_ctx)
{
    assert(geo_ctx.geo);
    assert(geo_ctx.geometry_list);

    if (!fuser_class || !fuser_class[0])
    {
        geo_ctx.geo->error("NodePrimitive::addGeometryToScene(): empty Fuser class given");
        return -1; // need a Fuser class!
    }

    const bool reload_points = (geo_ctx.geo->rebuild(DD::Image::Mask_Points) ||
                                geo_ctx.geo->rebuild(DD::Image::Mask_Object));
    const bool reload_prims  = (geo_ctx.geo->rebuild(DD::Image::Mask_Primitives) ||
                                geo_ctx.geo->rebuild(DD::Image::Mask_Vertices  ) ||
                                geo_ctx.geo->rebuild(DD::Image::Mask_Object    ) ||
                                geo_ctx.geo->rebuild(DD::Image::Mask_Attributes));
    const bool reload_attribs = geo_ctx.geo->rebuild(DD::Image::Mask_Attributes);

    if (0)//(debug)
    {
        std::cout << "  NodePrimitive::addGeometryToScene(): fuser_class='" << fuser_class << "'";
        std::cout << ", rebuild_mask=0x" << std::hex << geo_ctx.geo->rebuild_mask() << std::dec;
        std::cout << ": reload_prims=" << reload_prims << ", reload_points=" << reload_points << ", reload_attribs=" << reload_attribs;
        std::cout << ", creation_mode=" << creation_mode;
        //std::cout << " args[" << node_args.args() << "]";
        std::cout << std::endl;
    }

    // geoinfo_cache object is updated with thread-safe pointers to the underlying
    // geometry data structures stored in the GeoOp. The GeoInfo caches
    // move around in memory as the GeometryList appends objects to it:
    Fsr::GeoInfoCacheRef geoinfo_cache;
    geo_ctx.addObjectThreadSafe(node_args.args()[Arg::Scene::path], geoinfo_cache);
    assert(geoinfo_cache.obj >= 0);

    if (creation_mode == LOAD_IMMEDIATE)
    {
        //==================================================================
        // Immediate Mode
        //
        // Create a temporary NodePrimitive to execute and inject geometry into
        // the scene. Delete the temp prim afterwards.
        //
        // The Fsr::Node created by NodePrimitive is responsible for handling
        // the GeoOp reload modes (reload_prims, reload_points, etc)
        //
        //==================================================================

        // TODO: for now we do an immediate execute which causes the node to be
        // created, executed, then destroyed. We should be caching the created
        // nodes in the SceneGraphPrimitive so they can be reused.

        // Attempt to instantiate the Fuser Node:
        Fsr::Node* node = Fsr::Node::create(fuser_class, node_args.args(), NULL/*parent-node*/);
        if (!node)
        {
            std::cerr << "Fuser::NodePrimitive::addGeometryToScene(): ";
            std::cerr << "warning, unable to create Fuser Node of class '" << fuser_class << "'";
            std::cerr << ", cannot execute" << std::endl;
            return -1; // creation error
        }

        // Execute node to generate geoemtry data. Calls validateState() on the Node automatically.
        node->execute(node_args,
                      "GeoOpGeometryEngine"/*target-name*/,
                      &geo_ctx/*target-data*/);
        delete node;

    }
    else
    {
        //==================================================================
        // Deferred Mode
        //
        // A NodePrimitive (DD::Image::Primitive) is created as a wrapper for the
        // underlying geometry object. i.e. a mesh, light, camera, etc.
        //
        // The NodePrimitive class redirects the Nuke 3D DD::Image::Primitive 
        // methods using the execute() method to abstract the interface to
        // to underlying object so it doesn't need to be built against
        // DD::Image.
        //
        //==================================================================

#if DEBUG_TIMES
        struct timeval time_0;
        struct timeval time_1;
        gettimeofday(&time_0, NULL/*timezone*/);
#endif

        // Add 8 bbox corner points to GeoInfo point list. We assign their values
        // after getting the primitive bbox:
        DD::Image::PointList& points = *geo_ctx.createWritablePointsThreadSafe(geoinfo_cache, NPOINTS);

        Fsr::NodePrimitive* fprim = NULL;

        // In reload_prims create a NodePrimitive and add it to the GeometryList, and
        // when updating the object (reload_points, etc) we retrieve the same
        // primitive (always 0 at the moment.)
        //
        // TODO: allow multiple FuserNodePrims to be in the same GeoInfo and
        //  use the node path to find them during return runs.
        //
        if (reload_prims)
        {
            // Deferred mode - add the PuserPrim itself, the GeometryList
            // takes ownership of pointer:
            fprim = geo_ctx.createFuserNodePrimitiveThreadSafe(fuser_class, node_args.args());
            if (!fprim || !fprim->node())
            {
                geo_ctx.geo->error("NodePrimitive::addGeometryToScene(): cannot create Fsr::Node of type '%s'", fuser_class);
                if (fprim)
                    delete fprim;
                return -1;
            }

            geo_ctx.appendNewPrimitiveThreadSafe(geoinfo_cache, fprim, NVERTS);

            // Create object-level attributes that are expected to exist during rendering:
            {
                // Add name attribute:
                geo_ctx.setObjectStringThreadSafe(geoinfo_cache, Arg::node_name.c_str(), fprim->getName());

                // Add parent-path attribute - this will allow the xform path to be somewhat
                // reconstructed on output:
                geo_ctx.setObjectStringThreadSafe(geoinfo_cache, Arg::Scene::path.c_str(), fprim->getPath());
            }

        }
        else
        {
            // 
            fprim = geoinfo_cache.getFuserNodePrimitive(0/*obj*/);
            if (!fprim || !fprim->node())
            {
                std::stringstream err;
                err << "Fsr::Node '" << fuser_class << "'";
                err << "[" << node_args.args()[Arg::Scene::path] << "]";
                err << " disappeared!";
                geo_ctx.geo->error("NodePrimitive::addGeometryToScene(): %s", err.str().c_str());

                return -1;
            }
        }
        assert(fprim);
        assert(fprim->node());

        // Update info on the previously created NodePrimitive, this could have
        // created just now or during a previous pass:

#if 0
        std::cout << "      -------------------------------------------------------------------" << std::endl;
        std::cout << "      NodePrimitive(node=" << fprim->node() << ")::deferred:";
        std::cout << " name='" << fprim->getName() << "'";
        std::cout << ", path='" << fprim->getPath() << "'";
        std::cout << ", obj=" << obj << ", frame=" << node_args.frame();
        //std::cout << " args[" << fprim->node()->args() << "]";
        std::cout << std::endl;
#endif

        // Get the matrix and local bbox up to date:
        fprim->node()->validateState(node_args, false/*for_real*/, false/*force*/);

        // Get the *world-space* bbox for the bbox points, because the GeoInfo's
        // global matrix does not represent *this* prim's world-space xform,
        // so we place the points into world space as placeholders but we don't
        // use them for any direct purpose (yet):
        const Fsr::Box3d world_bbox = fprim->node()->getWorldBbox();
        //std::cout << ", world_bbox=" << world_bbox << std::endl;

        // Update bbox corner points:
        points[0].set(float(world_bbox.min.x), float(world_bbox.min.y), float(world_bbox.min.z));
        points[1].set(float(world_bbox.max.x), float(world_bbox.min.y), float(world_bbox.min.z));
        points[2].set(float(world_bbox.min.x), float(world_bbox.max.y), float(world_bbox.min.z));
        points[3].set(float(world_bbox.max.x), float(world_bbox.max.y), float(world_bbox.min.z));
        //
        points[4].set(float(world_bbox.min.x), float(world_bbox.min.y), float(world_bbox.max.z));
        points[5].set(float(world_bbox.max.x), float(world_bbox.min.y), float(world_bbox.max.z));
        points[6].set(float(world_bbox.min.x), float(world_bbox.max.y), float(world_bbox.max.z));
        points[7].set(float(world_bbox.max.x), float(world_bbox.max.y), float(world_bbox.max.z));

        //info.update_bbox();

#if DEBUG_TIMES
        gettimeofday(&time_1, NULL/*timezone*/);
        const double tStart = double(time_0.tv_sec) + (double(time_0.tv_usec)/1000000.0);
        const double tEnd   = double(time_1.tv_sec) + (double(time_1.tv_usec)/1000000.0);
        const double tSecs = (tEnd - tStart);
        std::cout << "NodePrimitive::deferred total delay=" << tSecs << std::endl;
#endif

    } // immediate/deferred

    //info.print_info(std::cout);

    if (reload_prims)
    {
        //****************************************************************************
        // Force bbox to get updated - this is IMPORTANT to getting the rebuilt objects
        // to validate properly:
        geo_ctx.geo->set_rebuild(DD::Image::Mask_Points | DD::Image::Mask_Attributes);
        //****************************************************************************
    }

#if 1
    // TODO: get the geo_ctx to properly contain thread-safe object count
    // for this thread.
    return 1;
#else
    const int added_objects = (obj - start_obj);
    geo_ctx.obj_index_start += added_objects; // starting index of next object to add
    geo_ctx.obj_index_end = geo_ctx.obj_index_start;

    return added_objects;
#endif

}


/*! Set the frame number and pass it to the Fsr::Node.
*/
/*virtual*/
void
NodePrimitive::setFrame(double frame)
{
    if (m_node && (fabs(m_frame - frame) > std::numeric_limits<double>::epsilon()))
    {
        m_node->setDouble("frame", frame);
        m_node->invalidateState();
    }
    m_frame = frame;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
// DD::Image::Primitive virtual functions
//
// Most of these are only used in deferred mode since in immediate mode
// the NodePrimitive is temporary.
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
DD::Image::PrimitiveType
NodePrimitive::getPrimitiveType() const
{
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
    return (DD::Image::PrimitiveType)FUSER_NODE_PRIMITIVE_TYPE;
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    return (DD::Image::PrimitiveType)FUSER_NODE_PRIMITIVE_TYPE;
#pragma GCC diagnostic pop
#endif
}


/*! Returns the primitive bounding box which is the *world-space* bbox of the
    Fsr::Node since the GeoInfo's matrix does not include the Node's transform.

    Note - this gets called *before* Primitive::validate()!
*/
/*virtual*/
DD::Image::Box3
NodePrimitive::get_bbox(const DD::Image::GeoInfo* info) const
{
    return (m_node) ? m_node->getWorldBbox().asDDImage() : DD::Image::Box3();
}


/*! Initialize any variables prior to display or rendering.
    Default just calls base class.

    Note - get_bbox() is called before this one so calcing
    the bbox in here is pointless...

*/
/*virtual*/
void
NodePrimitive::validate(DD::Image::PrimitiveContext* ptx)
{
    //std::cout << "NodePrimitive(" << getName() << ")::Primitive::validate()" << std::endl;
    FuserPrimitive::validate(ptx);

    // Probably don't need to do anything in here...
}


//-----------------------------------------------------------------------------


/*! Returns the number of faces in the Primitive.

    ***********************************************************************
    NOTE: If faces() returns > 0 then average_center_and_bounds must be
          implemented to allow OpenGL picking to be supported
          otherwise Nuke will CRASH!
    ***********************************************************************
*/
/*virtual*/
unsigned
NodePrimitive::faces() const
{
    //std::cout << "NodePrimitive(" << getName() << ")::faces()" << std::endl;
#if 1
    return NFACES; // bbox has six faces, implement average_center_and_bounds()!
#else
    return 0; // keep picking from crashing
#endif
}


/*! Returns the xyz center average of the primitive, plus local space bounds.

    ***********************************************************************
    NOTE: This must be implemented to allow OpenGL picking to be supported
          otherwise Nuke will CRASH if faces() returns > 0!
    ***********************************************************************
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::average_center_and_bounds(int                         face,
                                         const DD::Image::PointList* points,
                                         DD::Image::Vector3&         min,
                                         DD::Image::Vector3&         max) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::average_center_and_bounds(face " << face << ")" << std::endl;
    if (face >= NFACES)
        face = 0;

#if 1
    // Calc the bbox of a box face:
    DD::Image::Vector3 center(0.0f, 0.0f, 0.0f);
    min.set( INFINITY, INFINITY, INFINITY);
    max.set(-INFINITY,-INFINITY,-INFINITY);

    unsigned vindex = face*NVERTS_PER_FACE;
    for (unsigned v=0; v < NVERTS_PER_FACE; ++v)
    {
        const DD::Image::Vector3& P = (*points)[vertex_[vindex++]];
        center += P;
        min.x = std::min(P.x, min.x);
        min.y = std::min(P.y, min.y);
        min.z = std::min(P.z, min.z);
        max.x = std::max(P.x, max.x);
        max.y = std::max(P.y, max.y);
        max.z = std::max(P.z, max.z);
    }
    center /= float(NVERTS_PER_FACE);
    return center;
#else
    if (m_node)
    {
        min = m_node->getBBox().min();
        max = m_node->getBBox().max();
        return m_node->getBBox().center();
    }
#endif
}

/*! Test for the intersection of this primitive face with a given ray.

    ***********************************************************************
    NOTE: If faces() returns > 0 then average_center_and_bounds must be
          implemented to allow OpenGL picking to be supported
          otherwise Nuke will CRASH when this is called!
    ***********************************************************************

    The incoming ray has been transformed by the inverse GeoInfo matrix
    so the intersection happens in Primitive local-space, so we need
    to handle the transform to Node-space.
*/
/*virtual*/
bool
NodePrimitive::IntersectsRay(const DD::Image::Ray&       ray,
                             int                         face,
                             const DD::Image::PointList* pointList,
                             DD::Image::CollisionResult* result) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::IntersectsRay(face " << face << ")" << std::endl;

    if (!m_node)
        return false;

    Fsr::RayContext Rtx(ray);

    double tmin, tmax;
    if (Fsr::intersectAABB(m_node->getWorldBbox(), Rtx, tmin, tmax))
    {
        // Store collision result (can sometimes be NULL):
        if (result)
        {
            result->_collisionTime     = float(tmin);
            result->_collisionNormal   = get_face_normal(face, pointList);
            result->_collisionPosition = Rtx.getPositionAt(tmin);
            //result->_collisionPrimitiveIdx = 0; // set outside this method
            result->_collisionFaceIdx  = face;
            result->_collisionGeo      = NULL;
            result->_collided          = true;
        }
        //std::cout << "pPolyMesh::IntersectsRay(" << face << ") hit!" << std::endl;
        return true;
    }
    return false;

}


//-----------------------------------------------------------------------------


/*! Returns the xyz center average of the primitive.
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::average_center(const DD::Image::PointList*) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::average_center()" << std::endl;
    return NodePrimitive::get_bbox(NULL/*info*/).center();
}


/*! Returns the xyz center average of the sub face.
    Base class returns the primitive center.
    TODO: this should call Fsr::Node!
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::face_average_center(int                         face,
                                   const DD::Image::PointList* points) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::face_average_center(" << face << ")" << std::endl;
    DD::Image::Vector3 min, max;
    return average_center_and_bounds(face, points, min, max);
}


//-----------------------------------------------------------------------------


/*! Returns the number of vertices for the sub face.
*/
/*virtual*/
unsigned
NodePrimitive::face_vertices(int face) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::face_vertices(" << face << ")" << std::endl;
    return NVERTS_PER_FACE; // all faces have 4 verts
}


/*! Fill the pre-allocated array with vertices constituting the sub face.
*/
/*virtual*/
void
NodePrimitive::get_face_vertices(int       face,
                                 unsigned* array) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::get_face_vertices(" << face << ")" << std::endl;
    if (face >= NFACES)
        face = 0;
    unsigned v = face*NVERTS_PER_FACE;
    array[0] = v++;
    array[1] = v++;
    array[2] = v++;
    array[3] = v;
}


/*! Does this face in this primitive reference this vertex?
*/
/*virtual*/
bool
NodePrimitive::faceUsesVertex(unsigned face,
                              unsigned vert) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::faceUsesVertex(" << face << ", " << vert << ")" << std::endl;
    if (vert >= NVERTS)
        return false;
    return ((vert/NVERTS_PER_FACE) == face);
}


/*! Returns the normal for face.
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::get_face_normal(int                         face,
                               const DD::Image::PointList* points) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::get_face_normal(" << face << ")" << std::endl;
    if (face >= NFACES)
        face = NFACES-1;
    return face_normal[face];
}


//-----------------------------------------------------------------------------


/*! Return the geometric normal for vertex.
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::get_geometric_normal(int                         vert,
                                    const DD::Image::PointList* points) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::get_geometric_normal(" << vert << ")" << std::endl;
    if (vert >= NVERTS)
        vert = NVERTS-1;
    return face_normal[vert/NFACES];
}


//-----------------------------------------------------------------------------


/*! Return the number of faces that vertex connects to and fills in the list of face indices.
*/
/*virtual*/
int
NodePrimitive::get_vertex_faces(int               vert,
                                std::vector<int>& faces) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::get_vertex_faces(" << vert << ")" << std::endl;
    // Corners always connect to 3 faces:
    faces.resize(3);
    if (vert >= NVERTS)
        vert = NVERTS-1;
    int v = vert/3;
    faces[0] = vert_to_face[v++];
    faces[1] = vert_to_face[v++];
    faces[2] = vert_to_face[v];
    return 3;
}


/*! TODO: this should call Fsr::Node!
*/
/*virtual*/
DD::Image::Vector3
NodePrimitive::vertex_normal(unsigned vert,
                             const DD::Image::PointList* points) const
{
    return get_geometric_normal(vert, points);
}


/*! TODO: this should call Fsr::Node!
*/
/*virtual*/
void
NodePrimitive::vertex_shader(int                          vert,
                             DD::Image::Scene*            scene,
                             DD::Image::PrimitiveContext* ptx,
                             DD::Image::VArray&           out,
                             const DD::Image::Vector3*    normal) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::vertex_shader0(" << this << ") v=" << vert << std::endl;
    if (!m_node)
        return;

    //Primitive::vertex_shader(vert, scene, ptx, out, normal);
    // Don't call Primitive::vertex_shader() as it doesn't know how to
    // handle a NodePrimitive primitive.
}


/*! As above, but uses an existing VertexContext rather than making a temporary one.
    TODO: this should call Fsr::Node!
*/
/*virtual*/
void
NodePrimitive::vertex_shader(int                          vert,
                             DD::Image::Scene*            scene,
                             DD::Image::PrimitiveContext* ptx,
                             DD::Image::VertexContext&    vtx,
                             DD::Image::VArray&           out,
                             const DD::Image::Vector3*    normal) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::vertex_shader1(" << this << ") v=" << vert << std::endl;
    if (!m_node)
        return;

    //Primitive::vertex_shader(vert, scene, ptx, vtx, out, normal);
    // Don't call Primitive::vertex_shader() as it doesn't know how to
    // handle a NodePrimitive primitive.
}


//-----------------------------------------------------------------


/*!
*/
/*virtual*/
void
NodePrimitive::tessellate(DD::Image::Scene*            render_scene,
                          DD::Image::PrimitiveContext* ptx) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::tessellate(" << this << ") m_node=" << m_node << std::endl;
    if (!m_node)
        return;

    // Execute node to generate Nuke render prims:
    DDImageRenderSceneTessellateContext rtess_ctx(this, ptx, render_scene);
    m_node->execute(Fsr::NodeContext()/*target_context*/,
                    DDImageRenderSceneTessellateContext::name,
                    &rtess_ctx/*target*/);
}

//-----------------------------------------------------------------


/*!
*/
/*virtual*/
void
NodePrimitive::draw_wireframe(DD::Image::ViewerContext*    vtx,
                              DD::Image::PrimitiveContext* ptx,
                              DD::Image::Primitive*        /*prev_prim*/) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::draw_wireframe(" << this << ") m_node=" << m_node << std::endl;
    if (!m_node)
        return;

    Fsr::PrimitiveViewerContext pv_ctx(vtx, ptx);

    // Execute node to draw geoemtry data:
    m_node->execute(Fsr::NodeContext()/*target_context*/, "DRAW_GL_WIREFRAME", &pv_ctx/*target*/);
}


/*!
*/
/*virtual*/
void
NodePrimitive::draw_solid(DD::Image::ViewerContext*    vtx,
                          DD::Image::PrimitiveContext* ptx,
                          DD::Image::Primitive*        /*prev_prim*/) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::draw_solid(" << this << ") m_node=" << m_node << std::endl;
    if (!m_node)
        return;

#if 0
    const std::vector<DD::Image::DrawableGeo>& drawables = vtx->getDrawableGeoList();
    std::cout << "NodePrimitive(" << getName() << ")::draw_solid(" << this << ") m_node=" << m_node;
    std::cout << " drawableGeoInfos[" << drawables.size() << "]=[";
    //for (unsigned i=0; i < drawables.size(); ++i)
    //    std::cout << " " << drawables[i].geo;
    std::cout << " ]" << std::endl;
#endif

#if 0
    if (vtx->playbackInProgress())
    {
        // draw a wireframe bbox in play mode for speed:
        m_node->drawWireframe();//drawBbox();
        return;
    }
#endif

    Fsr::PrimitiveViewerContext pv_ctx(vtx, ptx);

    // Execute node to draw geoemtry data:
    if (vtx->display3d(ptx->geoinfo()->display3d) >= DD::Image::DISPLAY_TEXTURED)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        m_node->execute(Fsr::NodeContext()/*target_context*/, "DRAW_GL_TEXTURED", &pv_ctx/*target*/);
    }
    else
    {
        m_node->execute(Fsr::NodeContext()/*target_context*/, "DRAW_GL_SOLID", &pv_ctx/*target*/);
    }
}


/*virtual*/
void
NodePrimitive::draw_solid_face(int face,
                               DD::Image::ViewerContext*    vtx,
                               DD::Image::PrimitiveContext* ptx) const
{
    if (!m_node)
        return;
}


/*virtual*/
void
NodePrimitive::draw_primitive_normal(DD::Image::ViewerContext*    vtx,
                                     DD::Image::PrimitiveContext* ptx) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::draw_primitive_normal(" << this << ") m_node=" << m_node << std::endl;
    if (!m_node)
        return;
}


/*virtual*/
void
NodePrimitive::draw_primitive_num(DD::Image::ViewerContext*    vtx,
                                  DD::Image::PrimitiveContext* ptx) const
{
    //std::cout << "NodePrimitive(" << getName() << ")::draw_primitive_num(" << this << ") m_node=" << m_node << std::endl;
    if (!m_node)
        return;

    glPushMatrix();
    glMultMatrixd(m_node->getWorldTransform().array());
    glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);

#if 0
    m_node->drawIcons();
#else
    const Fsr::Box3d& bbox = m_node->getLocalBbox();

    glRasterPos3dv(bbox.min.array());
    DD::Image::gl_text(getName().c_str());

    //if (vtx->what_to_draw() & DD::Image::SHOW_PRIMITIVE_NUM)
    {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xcccc);//glLineStipple(1, 0xff00);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        DD::Image::gl_boxf(float(bbox.min.x), float(bbox.min.y), float(bbox.min.z),
                           float(bbox.max.x), float(bbox.max.y), float(bbox.max.z));
    }
#endif

    glPopAttrib(); // GL_CURRENT_BIT | GL_LINE_BIT
    glPopMatrix();
}


/*virtual*/
void
NodePrimitive::draw_vertex_num(DD::Image::ViewerContext*    vtx,
                               DD::Image::PrimitiveContext* ptx) const
{
    if (!m_node)
        return;
}


/*virtual*/
void
NodePrimitive::draw_vertex_normals(DD::Image::ViewerContext*    vtx,
                                   DD::Image::PrimitiveContext* ptx) const
{
    if (!m_node)
        return;
}


/*virtual*/
void
NodePrimitive::draw_vertex_uvs(DD::Image::ViewerContext*    vtx,
                               DD::Image::PrimitiveContext* ptx) const
{
    if (!m_node)
        return;
}


/*!
*/
/*virtual*/
void
NodePrimitive::print_info() const
{
    if (!m_node)
        return;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace Fsr


// end of Fuser/NodePrimitive.cpp

//
// Copyright 2019 DreamWorks Animation
//
