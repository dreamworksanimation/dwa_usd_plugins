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

/// @file Fuser/PointBasedPrimitive.h
///
/// @author Jonathan Egstad

#ifndef Fuser_PointBasedPrimitive_h
#define Fuser_PointBasedPrimitive_h

#include "Primitive.h"


namespace Fsr {

class PrimitiveViewerContext; // avoid including Node.h via ExecuteTargetContexts.h


/*! DD::Image::Primitive wrapper adding double-precision point support.

    This class contains a list of local-space single-precision point
    locations, *separate* from the DD::Image::PointList stored in the GeoInfo.

    This local list is only filled if m_xform is non-identity (m_have_xform
    = true). Use getPointLocations() to retrieve either the GeoInfo's or the
    local one.

    This duplication is unfortunately neccessary due to bugs in the Viewer
    picking code which crashes if the Primitive's average_center_and_bounds()
    method returns bounds outside of the area defined by the GeoInfo's points
    list. Or at least that's what it appears is happening...
    And adding only the 8 bbox corner points to the GeoInfo points doesn't
    work as then the Viewer picking system only thinks it's one big box and
    never calls average_center_and_bounds() on the individual faces...  :(

    To work around this we bake the primitive xform into the GeoInfo points
    but retain a unbaked copy so that we can pass double-precision point
    locations (single-precision points + double-precision matrix) to
    renderers that can support them.

    The main caveats to using this class is:
        * GeoInfo can contain only one Primitive that owns all the points
        *

*/
class FSR_EXPORT PointBasedPrimitive : public FuserPrimitive
{
  public:
    /*! ScanlineRenderer vertex array support to speed up the calling of
        vertex_shader() during tessellation.

        Stores the default attributes for the entire Primitive at vertex rate.
        Non vertex rate attributes are expanded/duplicated to vertex rate.

        These lists are kept separate so we can pass them to methods without
        splitting a big interleaved allocation up.

        The lists match the DD::Image::VArray class minus 'P' and 'MB' which are
        created internally by rPrimitives during add_to_render().

        TODO: This can be made more memory effecient by allocating a big lump
        for all attribute data since all the lists are the same size.
    */
    struct VertexBuffers
    {
        DD::Image::ChannelSet interpolateChannels;  //!< Which chans are filled/enabled

        // Per-point arrays:
        Fsr::Vec3fList   PL;        //!< Mask_PL_ - local-space point
        Fsr::Vec3fList   PW;        //!< Mask_PW_ - PL with concat xform applied (in world-space)

        // Per-vert arrays: TODO: change these to Fuser::Attributes!
        Fsr::Uint32List Pidx;       //!< Point indices into point-rate PL/PW arrays
        Fsr::Vec3fList     N;       //!< Mask_N_  - normals (in world-space if inverse concat xform applied)
        Fsr::Vec4fList    UV;       //!< Mask_UV_ - Vec4 uvs where w = perspective-coord for perspective interpolation
        Fsr::Vec4fList    Cf;       //!< Mask_Cf_ - Vec4 colors where w = alpha/opacity
        Fsr::Vec3fList   VEL;       //!< Mask_VEL_- local-space point velocity

        // Per-face info.
        bool            allTris;        //!< Are verts part of an all-tri mesh? Don't need a face list if so.
        bool            allQuads;       //!< Are verts part of an all-quad mesh? Don't need a face list if so.
        Fsr::Uint32List vertsPerFace;   //!< Number of verts per face (empty if not a mesh-like prim)


        //! Resizes arrays.
        VertexBuffers(size_t nPoints,
                      size_t nVerts,
                      size_t nPolyFaces=0) : allTris(false), allQuads(false) { resize(nPoints, nVerts, nPolyFaces); }

        //!
        void   resize(size_t nPoints,
                      size_t nVerts,
                      size_t nPolyFaces=0);
        void   resizePoints(size_t nPoints);
        void   resizeVerts(size_t nVerts);

        //! Sets allQuads/allTris to verts_per_face results if valid, otherwise false if nPolyFaces > 0
        void   resizePolyFaces(size_t          nPolyFaces,
                               const uint32_t* verts_per_face=NULL);
        //! Enable all-tris mode. Clears the vertsPerFace list and sets allTris = true.
        void   setAllTrisMode() { resizePolyFaces(0); allTris = true; allQuads = false; }
        //! Enable all-quads mode. Clears the vertsPerFace list and sets allQuads = true.
        void   setAllQuadsMode() { resizePolyFaces(0); allQuads = true; allTris = false; }

        //!
        size_t numPoints() const { return PL.size(); }
        size_t numVerts()  const { return Pidx.size(); }
        //! Only non-zero if a mesh-like prim.
        size_t numFaces() const;

        //! Copy one vert's values into a VArray. interpolate_channels is copied as well.
        void getVert(size_t             vert,
                     DD::Image::VArray& vout) const;

        //! Print out a single vert's worth of vals.
        void printVert(size_t        vert,
                       std::ostream& o) const;

        //-----------------------------------------------------------------

        //! Runs the GeoInfo's shader on all verts in the buffers, preparing them for rendering.
        void applyVertexShader(const DDImageRenderSceneTessellateContext& rtess_ctx,
                               const Fsr::Mat4d&                          local_xform=Fsr::Mat4d::getIdentity());

        //! Add vertex buffers to render scene.
        void addToRenderScene(DDImageRenderSceneTessellateContext& rtess_ctx,
                              int                                  mode) const;

        /*! Insert a rTriangle into the Scene, copying vertex values from the buffers.

            This method assumes the scene transforms in ptx have already been fiddled
            with to concatenate the GeoInfo and Fsr::Primitive's transforms.
        */
        void addRenderTriangleToScene(size_t                               v0,
                                      size_t                               v1,
                                      size_t                               v2,
                                      DDImageRenderSceneTessellateContext& rtess_ctx) const;
    };


  protected:
    Fsr::Vec3fList m_local_points;      //!< Local-space point locations - only filled if m_have_xform=true.
    Fsr::Box3f     m_local_bbox;        //!< Local-space bbox of points (untransformed by m_xform)
    Fsr::Box3d     m_xformed_bbox;      //!< World-space bbox of points (transformed by m_xform)


  protected:
    //! Required method to support DD::Image::Primitive::duplicate()
    void copy(const Fsr::PointBasedPrimitive* b);


    /*! These are called from the hijacked DD::Image::Primitive
        calls to fill in a VertexBuffer.
        Base class does nothing.
    */
    virtual void _drawWireframe(const PrimitiveViewerContext& vtx,
                                VertexBuffers&                vbuffers) {}
    virtual void _drawSolid(const PrimitiveViewerContext& vtx,
                            VertexBuffers&                vbuffers) {}


  public:
    //!
    PointBasedPrimitive(double frame=defaultFrameValue());

    //!
    PointBasedPrimitive(const ArgSet& args,
                        double        frame=defaultFrameValue());

    //! Must have a virtual destructor!
    virtual ~PointBasedPrimitive() {}


    //! If m_xform is identity return 'points' cast to Fsr::Vec3f vector, otherwise return m_local_points.
    /*virtual*/ const Fsr::Vec3fList& getPointLocations(const DD::Image::PointList* points) const
        { return (m_have_xform) ? m_local_points : FuserPrimitive::getPointLocations(points); }


    //! Return the vertex indice count. Base class returns the DD::Image::Primitive::vertex_ array size.
    virtual size_t   numVerts() const { return vertex_.size(); }

    //! Return the optional face count.  Base class returns 0.
    virtual size_t   numFaces() const { return 0; }

    //! Return the optional face vertex count.  Base class returns 0.
    virtual uint32_t numFaceVerts(uint32_t face) const { return 0; }


    //! Convenience function, same as points[vertex_[vert]], but with asserts in DEBUG mode.
    const Fsr::Vec3f& getVertexPoint(uint32_t              vert,
                                     const Fsr::Vec3fList& points) const;

    //--------------------------------------------------------------------------------- 


    //! If this has a local point list, what's its size.
    size_t numLocalPoints() const { return m_local_points.size(); }

    /*! Set the transform and pass in local-space point locations at once.
        If the transform is identity then local points values are ignored.
        Updates m_local_bbox (it will be empty if transform is identity.)
    */
    void   setTransformAndLocalPoints(const Fsr::Mat4d& xform,
                                      size_t            num_local_points,
                                      const Fsr::Vec3f* local_points);

    //! Get the local point locations (untransformed by m_xform)
    const   Fsr::Vec3fList& pointLocations() const { return m_local_points; }


    /*! Rebuild the bboxes. If force is true then the points are rescanned.
        The input PointList is used when m_xform is identity.
    */
    virtual void updateBounds(const DD::Image::PointList* geoinfo_points=NULL,
                              bool                        force=false);


    //! Find the xyz min/max of the Primitive's UV texture area.
    Fsr::Box3f   calcUVExtents(const Fsr::Vec4fList& UVs) const;


    //! Nodes can implement this to return a custom local-space bbox.
    /*virtual*/ Fsr::Box3d getLocalBbox() { return m_local_bbox; }
    //! Nodes can implement this to return a custom world-space bbox.
    /*virtual*/ Fsr::Box3d getWorldBbox() { return m_xformed_bbox; }



    /*====================================================*/
    /*                      Rendering                     */
    /*====================================================*/

    //! Copy attribute values from Primitive/GeoInfo attributes into a vertex data array.
    template <typename T>
    void copyFloatAttributeToVertexArray(const DD::Image::AttribContext* atx,
                                         std::vector<T>&                 out,
                                         DD::Image::ChannelSet&          out_channels) const;


    //! Fill in the VertexBuffers with the attribute values from this Primitive's attributes.
    virtual void fillVertexBuffers(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                   VertexBuffers&                             vbuffers) const;


    //! Run the material (if there is one) vertex_shader() on each vertex in the VertexBuffer.
    virtual void applyVertexShader(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                   VertexBuffers&                             vbuffers) const;


    //! Add vertex buffers to render scene.
    virtual void addToRenderScene(const VertexBuffers&                 vbuffers,
                                  DDImageRenderSceneTessellateContext& rtess_ctx,
                                  int                                  mode) const;


  public:
    //-------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------
    // DD::Image::Primitive virtual methods:
    //-------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------

    //! Returns the primitive bounding box - called before validate()!
    /*virtual*/ DD::Image::Box3 get_bbox(const DD::Image::GeoInfo* info) const;

    //! Initialize any variables prior to display or rendering.
    /*virtual*/ void validate(DD::Image::PrimitiveContext*);

    //! Returns the xyz center average of the entire primitive (not really used)
    /*virtual*/ DD::Image::Vector3 average_center(const DD::Image::PointList*) const;

    /*! Vector3 Primitive::vertex_normal(unsigned v, const PointList*) const
        Returns a normal that best represents the normal at \b point's
        location on the primitive.  Base class ignores the vertex argument and
        returns the primitive's base normal.
    */
    /*virtual*/ DD::Image::Vector3 vertex_normal(unsigned                    vert,
                                                 const DD::Image::PointList* point_list) const;


    //----------------------------------------------------------------------
    //----------------------------------------------------------------------
    // These vertex_shader() methods are replaced by applyVertexShader().
    //
    // Primitive::vertex_shader() is only called by Scene during request() to
    // find the min/max UVs required by input textures, and by the Primitive
    // subclass itself during tessellate() to prep VArrays for ScanlineRender.
    //
    // So, we let Scene::request() call the base class version to get the UVs,
    // but PointBasedPrimitive subclasses will call applyVertexShader()
    // instead.

#if 0
    /*virtual*/ void vertex_shader(int vert,
                                   DD::Image::Scene*,
                                   DD::Image::PrimitiveContext*,
                                   DD::Image::VArray& out,
                                   const DD::Image::Vector3* normal=NULL) const;
    /*virtual*/ void vertex_shader(int vert,
                                   DD::Image::Scene*,
                                   DD::Image::PrimitiveContext*,
                                   DD::Image::VertexContext&,
                                   DD::Image::VArray& out,
                                   const DD::Image::Vector3* normal=NULL) const;
#endif
    //----------------------------------------------------------------------
    //----------------------------------------------------------------------


    /*====================================================*/
    /*                   Rendering                        */
    /*====================================================*/

    /*! Decompose the Primitive into Nuke-3D render primitives for consumption by the
        built-in ScanlineRender renderer.

        DD::Image::Primitive virtual method.

        This is called once from Scene::generate_render_primitives() inside the
        DD::Image::Render base class' engine() method. In generate_render_primitives()
        the Scene is requested from the input GeoOp, then for each Primitive in each
        GeoInfo in the Scene's GeometryList this method is called and passed a pre-filled
        PrimitiveContext with the Scene transforms, primitive index, GeoInfo pointer
        and index, and the (theoretically) matching motion-blur GeoInfo.

        This PrimitiveContext can be completely changed by tessellate() as there's nothing
        done with it afterwards. i.e. anything we put in the PrimitiveContext is only
        valid inside tessellate(), however if doing this it's best to make a copy of the
        PrimitiveContext so it's not too messed up for the next GeoInfo Primitive.

        It's assumed that in tessellate() all rPrimitives are inserted into the Scene
        via the Scene::add_*_render_primitive() methods. This means that we have
        total control over the scene transforms and geoinfo pointers and can leverage
        this to support our internal double-precision transforms...

        ...unfortunately the resulting rPrimitive is still single-precision but it
        allows us to concatenate the internal xform with the GeoInfo's and update
        the scene transform's object matrix.

        TODO: Another drawback is that the Scene's transforms are used later during
        surface shading so if they're modified here that may cause issues... Check
        that out.

        The PointBased base class calls the subclass methods to construct
        a VertexBuffers class, runs the vertex shader on it, then decomposes
        it into rTriangles (if a mesh type.)
    */
    /*virtual*/ void tessellate(DD::Image::Scene*,
                                DD::Image::PrimitiveContext*) const;


    /*====================================================*/
    /*             OpenGL drawing methods                 */
    /*====================================================*/
    /*virtual*/ void draw_primitive_num(DD::Image::ViewerContext*,
                                        DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_vertex_num(DD::Image::ViewerContext*,
                                     DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_vertex_normals(DD::Image::ViewerContext*,
                                         DD::Image::PrimitiveContext*) const;
    /*virtual*/ void draw_vertex_uvs(DD::Image::ViewerContext*,
                                     DD::Image::PrimitiveContext*) const;


    /*! These are used to hijack the GeoInfo draw() method...

        This method is (still as of Nuke 12) called directly by the GeoInfo
        owning the list of Primitive's draw() method, and that GeoInfo
        is in a std::vector<GeoInfo::Cache> 'cache_list' on a GeoOp.

        That GeoOp's add_draw_geometry() adds draw handle callbacks
        which then call GeoInfo::draw(), passing in the ViewerContext. In
        draw() the GeoInfo sets up a PrimitiveContext and calls
        Primitive::draw_solid() and Primitive::draw_wireframe() for all
        prims in its list, depending on the OpenGL draw mode.

        This is all done single threaded by the main thread so to
        multithread the GeoOp's draw handles callback we're going to
        hijack this method to fire off worker threads leap-frogging
        their way through the GeoInfo's Primitives looking for
        FuserPrimitives that can generate vertex buffers to pass to
        OpenGL. This will dramatically accelerate the deferred-load
        Fuser nodes since they need to generate their geometry at the
        moment OpenGL wants to draw, so single-threaded stepping through
        those can be very slow for large scenes with animating character
        meshes.

        The wrinkle to all this is the OpenGL draw lists that the GeoInfo
        creates and expects the Primitive to fill in, then calls for
        all subsequent draw() callbacks until the GeoInfos are rebuilt.
        The Primitive has no control over this draw list behavior and is
        in fact very limited in what OpenGL calls it can do within a
        draw list.

        To get around this we know that draw_solid() and draw_wireframe()
        are called after the GeoInfo has created and opened a new draw list
        via glNewList(), and that the index of the list is stored in the
        GeoInfo's DrawLists struct write-accessible via GeoInfo::callLists().
        So the dastardly plan is to close out and delete the draw list with
        glEndList() & glDeleteLists(), then set the GeoInfo's list
        index to 0, indicating no list exists. This will force the GeoInfo
        to call draw_solid() again.

        TODO: finish this code thought! (and these notes)

        an already-open draw list in GL_COMPILE mode that must be closed
        while remembering its index, then re-opened once the vertex
        buffers are available to draw.    
    */
    /*virtual*/ void draw_solid(DD::Image::ViewerContext*,
                                DD::Image::PrimitiveContext*,
                                DD::Image::Primitive*) const;
    /*virtual*/ void draw_wireframe(DD::Image::ViewerContext*,
                                    DD::Image::PrimitiveContext*,
                                    DD::Image::Primitive*) const;

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline size_t
PointBasedPrimitive::VertexBuffers::numFaces() const
{
    if (allTris)
        return (numVerts() / 3);
    else if (allQuads)
        return (numVerts() / 4);
    else
        return vertsPerFace.size();
}


/*virtual*/ inline void
PointBasedPrimitive::updateBounds(const DD::Image::PointList* geoinfo_points,
                                  bool                        force)
{
    if (m_local_points.size() == 0 && geoinfo_points == NULL)
    {
        // No point source!
        m_local_bbox.setToEmptyState();
        m_xformed_bbox.setToEmptyState();
        return;
    }

    if (m_have_xform)
    {
        // Local points is used when m_xform is not identity:
        if (force || m_xformed_bbox.isEmpty())
        {
            Fsr::getLocalAndTransformedBbox(m_local_points.data(),
                                            m_local_points.size(),
                                            m_local_bbox,
                                            m_xform,
                                            m_xformed_bbox);
        }
    }
    else
    {
        // Input PointList is used when m_xform is identity:
        const Fsr::Vec3fList& points = FuserPrimitive::getPointLocations(geoinfo_points);
        if (force || m_local_bbox.isEmpty())
        {
            m_local_bbox.set(points.data(),
                             points.size());
            m_xformed_bbox.set(m_local_bbox.min, m_local_bbox.max);
        }
    }
}


inline void
PointBasedPrimitive::setTransformAndLocalPoints(const Fsr::Mat4d& xform,
                                                size_t            num_local_points,
                                                const Fsr::Vec3f* local_points)
{
    // If the transform is identity then local points values are ignored:
    m_xform = xform;
    m_have_xform = !xform.isIdentity();
    if (num_local_points > 0 && local_points != NULL)
    {
        m_local_points.resize(num_local_points);
        memcpy(m_local_points.data(), local_points, num_local_points*sizeof(Fsr::Vec3f));
        updateBounds(NULL/*geoinfo_points*/, true/*force*/);
    }
    else
    {
        m_local_points = Fsr::Vec3fList(); // release any reserved memory
        m_local_bbox.setToEmptyState();
        m_xformed_bbox.setToEmptyState();
    }
}


inline const Fsr::Vec3f&
PointBasedPrimitive::getVertexPoint(uint32_t              vert,
                                    const Fsr::Vec3fList& points) const
{
#if DEBUG
    assert(vert < vertex_.size());
    assert(vertex_[vert] < points.size());
#endif
    return points[vertex_[vert]];
}


/*! Copy attribute values from Primitive/GeoInfo attributes into a vertex data array.
*/
template <typename T>
inline void
PointBasedPrimitive::copyFloatAttributeToVertexArray(const DD::Image::AttribContext* atx,
                                                     std::vector<T>&                 out,
                                                     DD::Image::ChannelSet&          out_channels) const
{
    const size_t nVerts = numVerts();
    out.resize(nVerts);
    memset(out.data(), 0, sizeof(T)*nVerts); // fill with zeros by default

    //atx->print_info(); std::cout << std::endl;

    if (!atx || !atx->attribute || nVerts == 0 || atx->attribute->floats() != sizeof(T)/sizeof(float))
        return;

    if (atx->group == DD::Image::Group_Vertices)
    {
        // Primitive vertex attributes are stored in a packed list of all Prims in
        // the GeoInfo, so we need to know the Prim's offset in that list:
        const uint32_t prim_vertattrib_offset = DD::Image::Primitive::vertex_offset();

        // TODO: clamp nVerts to the legal range between prim_vert_offset and prim_vert_offset+nVerts
        memcpy(out.data(), atx->attribute->array(prim_vertattrib_offset), sizeof(T)*nVerts);
    }
    else if (atx->group == DD::Image::Group_Points)
    {
        // Copy point-rate attribute to per-vertex:
        for (size_t i=0; i < nVerts; ++i)
            out[i] = *reinterpret_cast<T*>(atx->attribute->array(vertex_[i]));
    }
    else if (atx->group == DD::Image::Group_Object)
    {
        // Always attribute index 0, duplicate it to all verts:
        const T v = *reinterpret_cast<T*>(atx->attribute->array(0));
        for (size_t i=0; i < nVerts; ++i)
            out[i] = v;
    }
    else if (atx->group == DD::Image::Group_Primitives)
    {
        // Always attribute index 0 (for now), duplicate to all verts:
        // TODO: store the prim index from the GeoInfo primitive-array in Fsr::Primitive for this
        const T v = *reinterpret_cast<T*>(atx->attribute->array(0/*m_prim_index*/));
        for (size_t i=0; i < nVerts; ++i)
            out[i] = v;
    }

    // Turn on interpolation channel bits:
    const uint32_t last_chan = (atx->channel + atx->attribute->floats());
    for (uint32_t i=atx->channel; i < last_chan; ++i)
        out_channels += DD::Image::Channel(i);
}


} // namespace Fsr

#endif

// end of Fuser/PointBasedPrimitive.h

//
// Copyright 2019 DreamWorks Animation
//
