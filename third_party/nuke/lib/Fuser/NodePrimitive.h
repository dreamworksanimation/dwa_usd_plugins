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

/// @file Fuser/NodePrimitive.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NodePrimitive_h
#define Fuser_NodePrimitive_h

#include "Primitive.h"
#include "Node.h"
#include "NukeGeoInterface.h" // for GeoOpGeometryEngineContext

#include <DDImage/GeoInfo.h>
#include <DDImage/GeometryList.h>


namespace Fsr {

//-------------------------------------------------------------------------


/*! Fsr::Node geometry primitive.

    Encapsulates a Fsr::Node inside a DD::Image::Primitive so that it can
    flow down the 3D tree.

    This has no implied geometry type so it's suitable for storing abstract
    info for shaders, facesets, etc.
*/
class FSR_EXPORT NodePrimitive : public FuserPrimitive
{
  public:
    //! Creation mode:
    enum {
        LOAD_IMMEDIATE,
        LOAD_DEFERRED
    };
    static const char* load_modes[];

    //! Quality modes
    enum
    {
        LOD_BBOX,       //!< Bounding-box
        LOD_STANDIN,    //!< Use standin geo (sphere, cylinder, etc)
        LOD_PROXY,      //!< Proxy-quality, if available
        LOD_RENDER      //!< Full-quality
    };
    static const char* lod_modes[];


  protected:
    // TODO: make this a RefCountedObject so it can be reference-copied between NodePrimitives:
    Fsr::Node*      m_node;         //!< Fsr::Node plugin that's loaded
    int             m_proxy_lod;    //!< Level-of-detail for proxy display (usually OpenGl)
    int             m_render_lod;   //!< Level-of-detail for render


  protected:
    //! Required method to support DD::Image::Primitive::duplicate()
    void copy(const NodePrimitive* b);


  public:
    //! Copy ctor to support DD::Image::Primitive::duplicate()
    NodePrimitive(const NodePrimitive& b);

    //! Fsr::Node::create() entry point. Ignores parent.
    NodePrimitive(const Fsr::ArgSet& args,
                  Fsr::Node*         parent) : FuserPrimitive(args) {}
    //!
    NodePrimitive(const char*        node_class,
                  const Fsr::ArgSet& args,
                  double             frame);

    //! Must have a virtual destructor! Deletes child nodes.
    virtual ~NodePrimitive();


    //! For create() method to instantiate this node by name.
    static const Fsr::Node::Description description;
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return description.fuserNodeClass(); }


    //! Return the Fsr::Node plugin pointer.
    Fsr::Node* node() const { return m_node; }

    //--------------------------------------------------------------------------------- 

    //! Some standard args:
    /*virtual*/ const std::string& getName() const { return (m_node)?m_node->getName():Fsr::empty_string; }
    /*virtual*/ const std::string& getPath() const { return (m_node)?m_node->getPath():Fsr::empty_string; }
    /*virtual*/ const std::string& getType() const { return (m_node)?m_node->getType():Fsr::empty_string; }

    //! LOD set/get methods.
    int  proxyLOD() const { return m_proxy_lod; }
    int  renderLOD() const { return m_render_lod; }
    void setLOD(int proxy, int render) { m_proxy_lod = proxy; m_render_lod = render; }

    //--------------------------------------------------------------------------------- 

    //! Set the frame number.
    /*virtual*/ void setFrame(double frame);

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    // Scene construction / deletion methods:

    //!
    static int addGeometryToScene(const char*                      fuser_class,
                                  int                              creation_mode,
                                  const Fsr::NodeContext&          node_args,
                                  Fsr::GeoOpGeometryEngineContext& geo_ctx);

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    // DD::Image::Primitive virtual methods:

    /*virtual*/ const char* Class() const { return fuserNodeClass(); }

    /*virtual*/ DD::Image::Primitive* duplicate() const { return new NodePrimitive(*this); }

    /*virtual*/ DD::Image::PrimitiveType getPrimitiveType() const;


    //! Initialize any variables prior to display or rendering.
    /*virtual*/ void validate(DD::Image::PrimitiveContext*);

    //! Returns the primitive bounding box
    /*virtual*/ DD::Image::Box3 get_bbox(const DD::Image::GeoInfo*) const;

    /*====================================================*/
    /*             Vertex/Face handling                   */
    /*====================================================*/

    //! Returns the number of faces in the Primitive.
    /*virtual*/ unsigned faces() const;

    //! Does this face in this primitive reference this vertex?
    /*virtual*/ bool faceUsesVertex(unsigned face,
                                    unsigned vert) const;

    //! Returns the xyz center average of the primitive.
    /*virtual*/ DD::Image::Vector3 average_center(const DD::Image::PointList*) const;

    //! Returns the xyz center average of the primitive, plus local space bounds.
    /*virtual*/ DD::Image::Vector3 average_center_and_bounds(int face,
                                                             const DD::Image::PointList* points,
                                                             DD::Image::Vector3& min,
                                                             DD::Image::Vector3& max) const;

    //! Returns the xyz center average of the sub face.  Base class returns the primitive center.
    /*virtual*/ DD::Image::Vector3 face_average_center(int face,
                                                       const DD::Image::PointList* points) const;

    //! Returns the number of vertices for the sub face.
    /*virtual*/ unsigned face_vertices(int n) const;

    //! Fill the pre-allocated array with vertices constituting the sub face.
    /*virtual*/ void get_face_vertices(int n, unsigned* array) const;

    //! Returns the normal for face.
    /*virtual*/ DD::Image::Vector3 get_face_normal(int face,
                                                   const DD::Image::PointList* points) const;

    //! Return the geometric normal for vertex.
    /*virtual*/ DD::Image::Vector3 get_geometric_normal(int vert,
                                                        const DD::Image::PointList* points) const;

    //! Return the number of faces that vertex connects to and fills in the list of face indices.
    /*virtual*/ int get_vertex_faces(int vert,
                                     std::vector<int>& faces) const;

    /*! Vector3 Primitive::vertex_normal(unsigned v, const PointList*) const
        Returns a normal that best represents the normal at \b point's
        location on the primitive.  Base class ignores the vertex argument and
        returns the primitive's base normal.
    */
    /*virtual*/ DD::Image::Vector3 vertex_normal(unsigned vert,
                                                 const DD::Image::PointList*) const;

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

    /*====================================================*/
    /*                   Rendering                        */
    /*====================================================*/

    /*virtual*/ void tessellate(DD::Image::Scene*,
                                DD::Image::PrimitiveContext*) const;

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
    /*virtual*/ void draw_vertex_uvs(DD::Image::ViewerContext*,
                                     DD::Image::PrimitiveContext*) const;

    /*====================================================*/
    /*        DD::Image::Ray intersection test            */
    /*====================================================*/
    //! Test for the intersection of this primitive face with a given ray.
    /*virtual*/ bool IntersectsRay(const DD::Image::Ray&       ray,
                                   int                         face,
                                   const DD::Image::PointList* pointList,
                                   DD::Image::CollisionResult* result) const;

    //!
    /*virtual*/ void print_info() const;


};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

#endif

// end of Fuser/NodePrimitive.h

//
// Copyright 2019 DreamWorks Animation
//
