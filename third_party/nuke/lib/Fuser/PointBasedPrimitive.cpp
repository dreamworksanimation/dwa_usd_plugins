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

/// @file Fuser/PointBasedPrimitive.cpp
///
/// @author Jonathan Egstad

#include "PointBasedPrimitive.h"
#include "ExecuteTargetContexts.h" // for PrimitiveViewerContext

#include <DDImage/PrimitiveContext.h>
#include <DDImage/Material.h>
#include <DDImage/gl.h>

#include <assert.h>

namespace Fsr {


//---------------------------------------------------------------------------------


/*! This empty dtor is necessary to avoid GCC 'undefined reference to `vtable...' link error.
    Must be in implemenation file, not header.
*/
FuserPrimitive::~FuserPrimitive()
{
    //
}


//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------


/*!
*/
PointBasedPrimitive::PointBasedPrimitive(double frame) :
    FuserPrimitive(frame)
{
    //
}


/*!
*/
PointBasedPrimitive::PointBasedPrimitive(const ArgSet& args,
                                         double        frame) :
    FuserPrimitive(args, frame)
{
    //
}


/*! Required method to support DD::Image::Primitive::duplicate()
*/
void
PointBasedPrimitive::copy(const Fsr::PointBasedPrimitive* b)
{
    FuserPrimitive::copy(this);
    m_local_points = b->m_local_points;
    m_local_bbox   = b->m_local_bbox;
    m_xformed_bbox = b->m_xformed_bbox;
}


//---------------------------------------------------------------------------------


/*! Returns the local-transformed bbox (with m_xform applied if there is one).

    Forces an update of m_local_bbox if not done yet.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Box3
PointBasedPrimitive::get_bbox(const DD::Image::GeoInfo* info) const
{
    if (info)
        const_cast<PointBasedPrimitive*>(this)->updateBounds(info->point_list());
    else
        const_cast<PointBasedPrimitive*>(this)->updateBounds();

    //std::cout << "Fsr::PointBasedPrimitive::get_bbox(" << this << ")";
    //std::cout << " points=" << info->points() << ", verts=" << numVerts();
    //std::cout << ", faces=" << numFaces();
    //std::cout << ", m_local_bbox[" << m_local_bbox << "]";
    //std::cout << ", m_xformed_bbox[" << m_xformed_bbox << "]";
    //std::cout << std::endl;

    return m_xformed_bbox.asDDImage();
}


/*! Calculate the surface normal - does nothing for a poly mesh.
    Updates m_local_bbox if not done yet.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
PointBasedPrimitive::validate(DD::Image::PrimitiveContext* ptx)
{
    FuserPrimitive::validate(ptx);

#if DEBUG
    assert(ptx);
#endif
    if (!ptx->geoinfo())
        return; // avoid crashing on ill-formed GeoInfos

    updateBounds(ptx->geoinfo()->point_list());

    // Poly mesh can never have a single normal, set it to +Z:
    normal_.set(0.0f, 0.0f, 1.0f);

    // Assign material here from GeoInfo?
    material_ = NULL;//ptx->geoinfo()->material;

    //std::cout << "Fsr::PointBasedPrimitive::validate(" << this << ")";
    //std::cout << " points=" << ptx->geoinfo()->points() << ", verts=" << numVerts();
    //std::cout << ", geoinfo-verts=" << ptx->geoinfo()->vertices();
    //std::cout << ", faces=" << numFaces();
    //std::cout << ", m_local_bbox[" << m_local_bbox << "]";
    //std::cout << ", m_xformed_bbox[" << m_xformed_bbox << "]";
    //std::cout << ", geoinfo: verts=" << ptx->geoinfo()->vertices();
    //std::cout << ", xform=" << getParentXform(*ptx->geoinfo());
    //std::cout << std::endl;
}


/*! Returns the center of the transformed bbox (m_xform applied).

    Don't bother trying to determine average of mesh points, this is
    just used for things like drawing text in OpenGL. Center of bbox is
    good enough and much cheaper...

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
PointBasedPrimitive::average_center(const DD::Image::PointList* point_list) const
{
    return m_xformed_bbox.getCenter();
}


/*! Returns the geometric normal of the vertex.

    DD::Image::Primitive virtual method.
*/
/*virtual*/ DD::Image::Vector3
PointBasedPrimitive::vertex_normal(unsigned                    vert,
                                   const DD::Image::PointList* point_list) const
{
    std::cout << "get_vertex_normal('" << getPath() << "') vert=" << vert << std::endl;
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


//===================================================================================
// Rendering:
//===================================================================================


/*!
*/
void
PointBasedPrimitive::VertexBuffers::resize(size_t nPoints,
                                           size_t nVerts,
                                           size_t nPolyFaces)
{
    resizePoints(nPoints);
    resizeVerts(nVerts);
    resizePolyFaces(nPolyFaces);
}
void
PointBasedPrimitive::VertexBuffers::resizePoints(size_t nPoints)
{
    PL.resize(nPoints);
    PW.resize(nPoints);
}
void
PointBasedPrimitive::VertexBuffers::resizeVerts(size_t nVerts)
{
    Pidx.resize(nVerts);
       N.resize(nVerts);
      UV.resize(nVerts);
      Cf.resize(nVerts);
     VEL.resize(nVerts);
}
void
PointBasedPrimitive::VertexBuffers::resizePolyFaces(size_t          nPolyFaces,
                                                    const uint32_t* verts_per_face)
{
    allQuads = allTris = false;
    if (nPolyFaces == 0)
    {
        if (vertsPerFace.size() > 0)
            vertsPerFace = Fsr::Uint32List(); // release allocation
    }
    else if (verts_per_face)
    {
        // Test the face counts:
        const uint32_t* vpf = verts_per_face;
        allQuads = allTris = true;
        for (size_t i=0; i < nPolyFaces; ++i)
        {
            const uint32_t numFaceVerts = *vpf++;
            if (numFaceVerts != 3)
                allTris = false;
            else if (numFaceVerts != 4)
                allQuads = false;
        }
        // If allTris or allQuads, clear vertsPerFace:
        if (allTris || allQuads)
            vertsPerFace = Fsr::Uint32List(); // release allocation
        else
        {
            vertsPerFace.resize(nPolyFaces);
            memcpy(vertsPerFace.data(), verts_per_face, sizeof(uint32_t)*nPolyFaces);
        }
    }
}



/*!
*/
void
PointBasedPrimitive::VertexBuffers::getVert(size_t             vert,
                                            DD::Image::VArray& vout) const
{
#if DEBUG
    assert(vert < Pidx.size());
#endif
    const uint32_t pnt = Pidx[vert];
#if DEBUG
    assert(pnt < PL.size());
#endif
    vout.PL()  = PL[pnt];
    vout.PW()  = PW[pnt];
    vout.P().set(vout.PW(), 1.0f);
    //
    vout.N()   =  N[vert];
    vout.UV()  = UV[vert];
    vout.Cf()  = Cf[vert];
    //
    vout.VEL() = VEL[vert];

    vout.channels = interpolateChannels;

    //printVert(vert, std::cout);
}


/*!
*/
void
PointBasedPrimitive::VertexBuffers::printVert(size_t        vert,
                                              std::ostream& o) const
{
    const uint32_t pidx = Pidx[vert];
    o << vert;
    o << ":" << pidx << "( PL" << PL[pidx] << ", PW"  <<  PW[pidx];
    o << ", N"  <<  N[vert] << ", UV"  <<  UV[vert] << ", Cf"  << Cf[vert];
    o << ", VEL" << VEL[vert];
    o << " ) " << std::endl;
}


//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------


// The PointBased tessellate base class calls the subclass methods to construct
// a VertexBuffers object, runs the vertex shader on it, then decomposes
// it into rTriangles (if a mesh type.)
// See header for more info.
/*!
*/
/*virtual*/ void
PointBasedPrimitive::tessellate(DD::Image::Scene*            render_scene,
                                DD::Image::PrimitiveContext* ptx) const
{
    if (numVerts() == 0 || numFaces() == 0)
        return;

    assert(ptx && ptx->geoinfo()); // should never be NULL!

    //std::cout << "---------------------------------------------" << std::endl;
    //std::cout << "PointBasedPrimitive:tessellate('" << getPath() << "') frame=" << m_frame;
    //std::cout << ", nPoints=" << ptx->geoinfo()->points();
    //std::cout << ", nVerts=" << numVerts();
    //std::cout << ", nFaces=" << numFaces();
    //std::cout << std::endl;

    //-------------------------------------------------------------------------
    // Vertex arrays - attributes including transformed points get expanded
    // to face-varying rate.
    //
    // Check out DD::Image::Channel3D.h for the list of officially-supported
    // vertex attributes. This list was originally intended to be varying in
    // size but the management of those arrays was never completed in the
    // original (pre-Foundry) 3D system, then it all became frozen in time...
    //
    VertexBuffers vbuffers(ptx->geoinfo()->points(),
                           numVerts(),
                           numFaces());

    DDImageRenderSceneTessellateContext rtess_ctx(this, ptx, render_scene);

    // This may perform subdivision on the vertex buffers:
    fillVertexBuffers(rtess_ctx, vbuffers);

    // Allow vertex shaders to change values, and produce final transformed PW and N:
    applyVertexShader(rtess_ctx, vbuffers);

    // Have vertex buffer output render prims to render scene, possibly
    // with a material override:
    addToRenderScene(vbuffers, rtess_ctx, 0/*mode*/);
}


//---------------------------------------------------------------------------------


/*! Fill in the VertexBuffers with the attribute values from this Primitive's
    GeoInfo attributes.

    By the end of this method PW should either be empty, filled with zeros, or
    a copy of PL.

    The final PW is created in applyVertexShader().
*/
/*virtual*/ void
PointBasedPrimitive::fillVertexBuffers(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                       VertexBuffers&                             vbuffers) const
{
    //std::cout << "  PointBasedPrimitive::fillVertexBuffers('" << path() << "')";
    //std::cout << " nPoints=" << ptx->geoinfo()->points();
    //std::cout << " nVerts=" << numVerts() << std::endl;

    if (!rtess_ctx.isValid())
        return; // don't crash

    const DD::Image::GeoInfo& info = *rtess_ctx.ptx->geoinfo();
    if (info.points() == 0)
        return; // don't crash

    const Fsr::Vec3fList& local_points = getPointLocations(info.point_list());

    //-------------------------------------------------------------------------
    // Vertex arrays - attributes including transformed points get expanded
    // to face-varying rate.
    //
    // Check out DD::Image::Channel3D.h for the list of officially-supported
    // vertex attributes. This list was originally intended to be varying in
    // size but the management of those arrays was never completed in the
    // original DD (pre-Foundry) 3D system, then it all became frozen in time.
    //
    const size_t nPoints = local_points.size();
    const size_t nVerts  = numVerts();
    vbuffers.resize(nPoints, nVerts, numFaces());

    //------------------------------------------------------------------
    // Copy point indices:
    memcpy(vbuffers.Pidx.data(), vertex_.data(), sizeof(uint32_t)*nVerts);

    //------------------------------------------------------------------
    // Point locations:
    //
    // This needs to stay single-precision as unfortunately there's
    // too many places where PL is transformed by a scene transform
    // (DD::Image::MatrixArray) that we don't have control over,
    // specifically in the projection and creation of VArray 'P'.
    //
    // Apply m_xform if non-identity to produce PL:
    if (!m_xform.isIdentity())
    {
        for (size_t i=0; i < nPoints; ++i)
            vbuffers.PL[i] = m_xform.transform(local_points[i]);
    }
    else
        memcpy(vbuffers.PL.data(), local_points.data(), sizeof(Fsr::Vec3f)*nPoints);

    memcpy(vbuffers.PW.data(), vbuffers.PL.data(), sizeof(Fsr::Vec3f)*nPoints);
    vbuffers.interpolateChannels += DD::Image::ChannelSetInit(DD::Image::Mask_PL_ |
                                                               DD::Image::Mask_PW_ |
                                                               DD::Image::Mask_P_);

    //------------------------------------------------------------------
    // Normals:
    if (info.N_ref)
    {
        copyFloatAttributeToVertexArray<Fsr::Vec3f>(info.N_ref, vbuffers.N, vbuffers.interpolateChannels);
    }

    //------------------------------------------------------------------
    // UVs:
    if (info.UV_ref)
        copyFloatAttributeToVertexArray<Fsr::Vec4f>(info.UV_ref, vbuffers.UV, vbuffers.interpolateChannels);
    else
    {
        const Fsr::Vec4f default_uv(0.5f, 0.5f, 0.0f, 1.0f);
        Fsr::Vec4f* UVp = vbuffers.UV.data();
        for (size_t v=0; v < nVerts; ++v)
            *UVp++ = default_uv;
        // No interpolation needed since all verts are the same UV.
        // Materials may enable this if they re-assign the values.
        //vbuffers.interpolateChannels += DD::Image::ChannelSetInit(DD::Image::Mask_UV_);
    }

    //------------------------------------------------------------------
    // Colors:
    if (info.Cf_ref)
        copyFloatAttributeToVertexArray<Fsr::Vec4f>(info.Cf_ref, vbuffers.Cf, vbuffers.interpolateChannels);
    else
    {
        const Fsr::Vec4f default_color(0.18f, 0.18f, 0.18f, 1.0f);
        Fsr::Vec4f* Cfp = vbuffers.Cf.data();
        for (size_t v=0; v < nVerts; ++v)
            *Cfp++ = default_color;
        // No interpolation needed since all verts are the same color.
        // Materials may enable this if they re-assign the values.
        //vbuffers.interpolateChannels += DD::Image::ChannelSetInit(DD::Image::Mask_Cf_);
    }

    //------------------------------------------------------------------
    // Motionblur:
    if (rtess_ctx.render_scene->mb_scene())
    {
        // TODO: match up Fsr::FuserPrimitive in mb_geoinfo, if they're not topology-varying:
        if (rtess_ctx.ptx->mb_geoinfo())
        {
            memset(vbuffers.VEL.data(), 0, nVerts*sizeof(Fsr::Vec3f));
            //for (size_t v=0; v < nVerts; ++v)
            //    vbuffers.VEL[v] = (rtess_ctx.ptx->mb_geoinfo()->point_array()[pindex] - vbuffers.PL[v]);
            vbuffers.interpolateChannels += DD::Image::ChannelSetInit(DD::Image::Mask_VEL_);
        }
        else
            memset(vbuffers.VEL.data(), 0, nVerts*sizeof(Fsr::Vec3f));
    }

} // fillVertexBuffers



class ColoredShader2 : public DD::Image::SolidShader
{
  public:
    ColoredShader2() : DD::Image::SolidShader(NULL)
    {
        //
    }

    /*virtual*/ const char* Class() const { return "ColoredShader2"; }
    /*virtual*/ const char* node_help() const { return ""; }

    /*virtual*/ bool shade_GL(DD::Image::ViewerContext* vtx,
                              DD::Image::GeoInfo&       info)
    {
        return SolidShader::shade_GL(vtx, info);
    }

    /*virtual*/ void unset_texturemap(DD::Image::ViewerContext* vtx) { }

    /*virtual*/ void fragment_shader(const DD::Image::VertexContext& vtx,
                                     DD::Image::Pixel&               out)
    {
        SolidShader::fragment_shader(vtx, out);
std::cout << "[" << out[DD::Image::Chan_Red];
std::cout << " " << out[DD::Image::Chan_Green];
std::cout << " " << out[DD::Image::Chan_Blue ];
std::cout << "]" << std::endl;
    }
}; 



/*! Run the material (if there is one) vertex_shader() on each vertex in
    the VertexBuffer, possibly changing values.
*/
/*virtual*/ void
PointBasedPrimitive::applyVertexShader(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                       VertexBuffers&                             vbuffers) const
{
    // just in case...
    if (rtess_ctx.isValid())
        vbuffers.applyVertexShader(rtess_ctx, m_xform);
}


/*! Run the material (if there is one) vertex_shader() on each vertex in
    the VertexBuffer, possibly changing values, preparing them for rendering.

    This replaces the DD::Image::Primitive::vertex_shader() method which
    is normally called by Primitive::tessellate() to fill VArrays for
    ScanlineRender consumption.

    Primitive::vertex_shader() is described thusly in DDImage/Primitive.h:
        -------------------------------------------------------------------------
        Fill in the VArray with the correct values for this vertex.
        The values in this array as set by each vertex in the primitive are
        linerly interpolated by the renderer and passed to the fragment_shader.

        Before being passed to the fragment_shader, everything is divided by
        w and w is replaced with 1/w. This is to produce a linearly-interpolated
        value. Probably this function should do the division, not the caller!

        I guess subclasses could move the points around, but that probably
        could be done by the shaders instead...
        -------------------------------------------------------------------------

    So we do the same thing but iterate over all vertices at the same time.

    TODO: The VertexBuffers might be sized different than the Primitive's vertex
    count due to subdivision, so we need to see if any vertex shaders don't like that.

*/
void
PointBasedPrimitive::VertexBuffers::applyVertexShader(const DDImageRenderSceneTessellateContext& rtess_ctx,
                                                      const Fsr::Mat4d&                          local_xform)
{
    if (!rtess_ctx.isValid())
        return; // don't crash!
    const DD::Image::GeoInfo& info = *rtess_ctx.ptx->geoinfo();

    const size_t nPoints = numPoints();
    const size_t nVerts  = numVerts();

    //std::cout << "  VertexBuffers::applyVertexShader()";
    //std::cout << " nPoints=" << nPoints;
    //std::cout << " nVerts=" << nVerts;
    //std::cout << std::endl;


    const Fsr::Mat4f geoinfo_xform(info.matrix);
    const Fsr::Mat4d  concat_xform(local_xform * Fsr::Mat4d(info.matrix));

    const bool apply_geoinfo_xform = geoinfo_xform.isNotIdentity();
    const bool apply_concat_xform  =  concat_xform.isNotIdentity();

    // Apply GeoInfo's xform if non-identity to produce PW:
    if (PW.size() < PL.size())
        PW.resize(PL.size()); // just in case...
    if (apply_concat_xform)
    {
        for (size_t i=0; i < nPoints; ++i)
            PW[i] = concat_xform.transform(PL[i]);
    }
    else
        memcpy(PW.data(), PL.data(), sizeof(Fsr::Vec3f)*nPoints);

    // Apply normals xform to normals - this is the GeoInfo transform
    // since the attribute is assumed to be in GeoInfo local space:
    if (apply_geoinfo_xform)
    {
        const Fsr::Mat4f normals_xform(geoinfo_xform.inverse());

        const size_t nVerts = numVerts();
        Fsr::Vec3f* Np = N.data();
        for (size_t v=0; v < nVerts; ++v, ++Np)
        {
            if (Np->notZero())
                *Np = normals_xform.normalTransform(*Np);
        }
    }


    //------------------------------------------------------------------
    // Get the vertex shader Material to call if in RENDER_TEXTURED mode.
    // Other modes don't call the material tree during render, so there's
    // not much point in calling it now.
    DD::Image::Iop* shader = NULL;
#if 0
    if (info.render_mode >= DD::Image::RENDER_TEXTURED)
    {
        // Grab the assigned shader first from the Primitive then from the GeoInfo:
        if (rtess_ctx.ptx->primitive()->material() && rtess_ctx.ptx->primitive()->material()->channels() != DD::Image::Mask_None)
            shader = rtess_ctx.ptx->primitive()->material();
        else if (info.material && info.material->channels() != DD::Image::Mask_None)
            shader = info.material;

        // Don't bother calling the vertex shader if it's the default 'Black' material:
        if (shader && strcmp(shader->Class(), "Black")==0)
            shader = NULL;
    }
#else
    // If it's == Iop::default_input() then don't bother as we're
    // replacing the functionality of Iop::vertex_shader().
    //
    /* In GeoInfo.h:
        enum RenderMode {
          RENDER_OFF = 0,
          RENDER_WIREFRAME,
          RENDER_SOLID,
          RENDER_SOLID_LINES,
          RENDER_TEXTURED,
          RENDER_TEXTURED_LINES,
          RENDER_UNCHANGED // Must be last/highest number
        };
    */
    if      (info.render_mode == DD::Image::RENDER_WIREFRAME)
    {
        shader = &DD::Image::WireframeShader::sWireframeShader;
    }
    else if (info.render_mode == DD::Image::RENDER_SOLID)
    {
        // ColoredShader supports default lighting calcs, otherwise
        // we need to use Textured mode.
        static ColoredShader2 default_solid_shader;
        //static DD::Image::ColoredShader default_solid_shader(NULL);
        shader = &default_solid_shader;
    }
    else if (info.render_mode == DD::Image::RENDER_SOLID_LINES)
    {
        shader = &DD::Image::WireframeShader::sWireframeShader;
    }
    else if (info.render_mode >= DD::Image::RENDER_TEXTURED)
    {
        // Grab the assigned shader first from the Primitive then from the GeoInfo:
        if (rtess_ctx.ptx->primitive()->material() && rtess_ctx.ptx->primitive()->material()->channels() != DD::Image::Mask_None)
            shader = rtess_ctx.ptx->primitive()->material();
        else if (info.material && info.material->channels() != DD::Image::Mask_None)
            shader = info.material;

        // Don't bother calling the vertex shader if it's the default 'Black' material:
        if (shader && strcmp(shader->Class(), "Black")==0)
            shader = NULL;
    }
#endif
    //std::cout << "    shader=" << shader;
    //if (shader)
    //    std::cout << " '" << shader->node_name() << "'";
    //std::cout << std::endl;


    //------------------------------------------------------------------
    // Possibly call the material vertex_shader() method on each vertex
    // and copy the VArray result out, possibly updating the vertex
    // array values. This replaces the vertex_shader() call done in
    // Primitive::vertex_shader().
    //
    // We only do this if the assigned material is not the
    // Iop::default_input() (ie the Black iop) since we don't need to
    // call its default Iop::vertex_shader().
    //
    // The only reason to call the default Iop::vertex_shader() is to
    // project the point locations into homogeneous clip space to
    // determine a clipmask and the uv texture extents which are then
    // stored in the passed-in PrimitiveContext, then returned and
    // passed on to the rPrimitive being set up.
    //
    // However this doesn't need to be done at the *top* of shader tree
    // by the Iop::vertex_shader() and can/should be done on return
    // at the *bottom* of the tree by the Primitive::vertex_shader(),
    // which we're replacing here. Thus we can skip calling
    // Iop::vertex_shader() completely unless the shader is a non-standard
    // one as this will be a side-effect of it calling up its input tree.
    //
    // So, if the connected shader is a Material then it's possible
    // there's something actually being twiddled in the vertex data like
    // uvs or normals or even point locations, so we will call Materials
    // but still do the uv texture extents determination even though it
    // it was also done by the Material walking up the shader tree.
    if (shader)
    {
        // This is passed to the vertex shader and the VArray result copied out:
        DD::Image::VertexContext vtx;

#if 0
        // Don't think this is required for vertex shading:
        DD::Image::GeoInfoRenderState renderstate;
        renderstate.castShadow    = true;          //!< Cast shadow
        renderstate.receiveShadow = true;          //!< Receive shadow
        renderstate.preMultiplierAlphaComp = true; //!< Force the pre-multiplier alpha compositing.
        renderstate.polygonEdge   = false;         //!< When enabled keeps polygons edges information
        renderstate.displacement  = ?              //!< displacement coefficients
#endif

        vtx.set_scene(rtess_ctx.render_scene);
        vtx.set_geoinfo(&info);
        vtx.set_transforms(rtess_ctx.ptx->transforms());
        vtx.set_primitive(rtess_ctx.ptx->primitive());
        //
        vtx.set_renderstate(NULL); // not required for vertex shader...?
        vtx.set_rprimitive(NULL);  // not required for vertex shader.
        vtx.set_rmaterial(NULL);   // not required for vertex shader.
        //
        vtx.vP.channels = interpolateChannels;

        Fsr::Vec3f*  Np  = N.data();
        Fsr::Vec4f* UVp  = UV.data();
        Fsr::Vec4f* Cfp  = Cf.data();
        Fsr::Vec3f* VELp = VEL.data();

        // Primitive vertex attributes are stored in a packed list of all Prims in
        // the GeoInfo, so we need to know the Prim's offset in that list:
        const uint32_t prim_vertattrib_offset = rtess_ctx.ptx->primitive()->vertex_offset();

        for (size_t v=0; v < nVerts; ++v)
        {
            const uint32_t pindex = Pidx[v];

            // In case vertex_shaders need the vertex or point indices:
            // TODO: this is not valid if the point or vertex count has been changed
            // by fillVertexBuffers!!!!
            const_cast<uint32_t*>(rtess_ctx.ptx->indices())[DD::Image::Group_Vertices] = prim_vertattrib_offset + (int)v;
            const_cast<uint32_t*>(rtess_ctx.ptx->indices())[DD::Image::Group_Points  ] = pindex;

            //if (v < 5 || abs(int(v - nVerts)) <= 5)
            //    printVert(v, std::cout);
            vtx.vP.PL()  = PL[pindex];
            vtx.vP.PW()  = PW[pindex];
            vtx.vP.N()   =  *Np;
            vtx.vP.UV()  = *UVp;
            vtx.vP.Cf()  = *Cfp;
            vtx.vP.VEL() = *VELp;

            shader->vertex_shader(vtx);

            PL[pindex] = vtx.vP.PL();
            PW[pindex] = vtx.vP.PW();
             *Np++  = vtx.vP.N();
            *UVp++  = vtx.vP.UV();
            *Cfp++  = vtx.vP.Cf();
            *VELp++ = vtx.vP.VEL();
            //if (v < 5 || abs(int(v - nVerts)) <= 5)
            //    printVert(v, std::cout);


        }

    }

} // applyVertexShader


/*! Add vertex buffers to render scene.

    TODO: attempt to override the render material...
*/
/*virtual*/ void
PointBasedPrimitive::addToRenderScene(const VertexBuffers&                 vbuffers,
                                      DDImageRenderSceneTessellateContext& rtess_ctx,
                                      int                                  mode) const
{
#if 0
    // Attempt to override the material used by the renderer:
    // TODO: this fails miserably...
    DD::Image::RenderMode saved_render_mode = rtess_ctx.ptx->geoinfo()->render_mode;
    DD::Image::Iop*       saved_material    = rtess_ctx.ptx->geoinfo()->material;

#if 1
static ColoredShader2 default_solid_shader;
const_cast<DD::Image::GeoInfo*>(rtess_ctx.ptx->geoinfo())->render_mode = DD::Image::RENDER_TEXTURED;
const_cast<DD::Image::GeoInfo*>(rtess_ctx.ptx->geoinfo())->material    = &default_solid_shader;
#else
    	  // Assign material to primitive based on parent object render setting:
	  static SolidShader solid_shader(0);
	  Iop* m;
	  if (p->geoinfo()->render_mode >= RENDER_TEXTURED) {
		m = p->parent()->material(); // Primitive material overrides geoinfo
		if (!m) m = p->geoinfo()->material; // GeoInfo material
		if (!m) m = &solid_shader; // Default to solid shader
	  } else {
		m = &solid_shader;
	  }
#endif
#endif

    // Have vertex buffer output render prims to render scene, in mesh mode:
    vbuffers.addToRenderScene(rtess_ctx, 0/*mode*/);

#if 0
    const_cast<DD::Image::GeoInfo*>(rtess_ctx.ptx->geoinfo())->render_mode = saved_render_mode;
    const_cast<DD::Image::GeoInfo*>(rtess_ctx.ptx->geoinfo())->material    = saved_material;
#endif
}


/*!
*/
void
PointBasedPrimitive::VertexBuffers::addToRenderScene(DDImageRenderSceneTessellateContext& rtess_ctx,
                                                     int                                  mode) const
{
    if (mode == 0)
    {
        // Mesh mode.
        // Convert quads/polys to DD::Image::rTriangles using the baked vertex buffers:
        if (allTris && (numVerts() % 3) == 0)
        {
            const size_t nFaces = (numVerts() / 3);
            uint32_t v0 = 0; // global vert count
            for (size_t f=0; f < nFaces; ++f, v0 += 3)
                addRenderTriangleToScene( v0, v0+1, v0+2, rtess_ctx);
        }
        else if (allQuads && (numVerts() % 4) == 0)
        {
            const size_t nFaces = (numVerts() / 4);
            uint32_t v0 = 0; // global vert count
            for (size_t f=0; f < nFaces; ++f, v0 += 4)
            {
                addRenderTriangleToScene(  v0, v0+1, v0+2, rtess_ctx);
                addRenderTriangleToScene(v0+2, v0+3,   v0, rtess_ctx);
            }
        }
        else
        {
            const size_t nFaces = vertsPerFace.size();
            uint32_t v0 = 0; // global vert count
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const uint32_t nFaceVerts = vertsPerFace[f];
                if (nFaceVerts >= 3)
                {
                    if (nFaceVerts == 3)
                    {
                        // Triangle:
                        addRenderTriangleToScene(v0, v0+1, v0+2, rtess_ctx);
                    }
                    else if (nFaceVerts == 4)
                    {
                        // Quad:
                        addRenderTriangleToScene(  v0, v0+1, v0+2, rtess_ctx);
                        addRenderTriangleToScene(v0+2, v0+3,   v0, rtess_ctx);
                    }
                    else
                    {
                        // nPoly:
                        // TODO: support ngons...?   :(
                        const uint32_t last_vert = v0 + nFaceVerts - 1;
                        for (uint32_t v=v0+1; v < last_vert; ++v)
                            addRenderTriangleToScene(v0, v, v+1, rtess_ctx);
                    }
                }
                v0 += nFaceVerts;
            }
        }
    }
    else
    {
        // TODO: handle other default modes
    }
}


/*! Insert a rTriangle into the Scene, copying vertex values from a VertexBuffer.

    This method assumes the scene transforms in ptx have already been fiddled
    with to concatenate the GeoInfo and Fsr::Primitive's transforms.
*/
void
PointBasedPrimitive::VertexBuffers::addRenderTriangleToScene(size_t                               v0,
                                                             size_t                               v1,
                                                             size_t                               v2,
                                                             DDImageRenderSceneTessellateContext& rtess_ctx) const
{
#if DEBUG
    assert(v0 < numVerts());
    assert(v1 < numVerts());
    assert(v2 < numVerts());
    assert(rtess_ctx.isValid());
#endif
    // The Scene will delete this allocation when the render is done:
    DD::Image::rTriangle* tri = new DD::Image::rTriangle(rtess_ctx.ptx->geoinfo(), rtess_ctx.ptx->primitive());
    getVert(v0, tri->v[0]);
    getVert(v1, tri->v[1]);
    getVert(v2, tri->v[2]);

    // Scene::add_render_primitive() will immediately call back to the rTriangle's
    // add_to_render() method or add_to_displacement_render() and the rTriangle
    // may do further dicing of itself to support non-linear projections or
    // displacement.
    //
    // We don't bother pre-clipping and using the add_clipped_render_primitive()
    // method.
    //
    rtess_ctx.render_scene->add_render_primitive(tri, rtess_ctx.ptx);
}


/*! Find the min/max of the Primitive's UV texture area.

    This is sorta hacky but handy in a texture projection context
    where only a small subset of a texture may appear on the surface
    of an object despite what the assigned UVs are. When a texture is
    huge (4k, 8k) this can significantly reduce the memory
    requirements for the texture input Tile.

    For projections this relies on the projection Material node
    implementing its vertex_shader() method correctly!
*/
Fsr::Box3f
PointBasedPrimitive::calcUVExtents(const Fsr::Vec4fList& UVs) const
{
    Fsr::Box3f texture_extents;
    const size_t nVerts = UVs.size();
    if (nVerts > 0)
    {
        const Fsr::Vec4f* UVp = UVs.data();
        for (size_t v=0; v < nVerts; ++v)
        {
            const Fsr::Vec4f& uv = *UVp++;
            if (uv.w >= std::numeric_limits<float>::epsilon())
                texture_extents.expand(uv.wNormalized());
        }
    }
    return texture_extents;
}


//===================================================================================
// OpenGL drawing methods:
//===================================================================================


/*! Draw the mesh's primitive index at vertex 0 (rather than the center.)

    DD::Image::Primitive virtual method.
*/
/*virtual*/ void
PointBasedPrimitive::draw_primitive_num(DD::Image::ViewerContext*    ctx,
                                        DD::Image::PrimitiveContext* ptx) const
{
    //glColor(ctx->fg_color());
    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    char buf[128];

    const Fsr::Vec3f& p = getVertexPoint(0, points);
    glRasterPos3f(p.x, p.y, p.z);

    const DD::Image::Attribute* name =
        ptx->geoinfo()->get_group_attribute(DD::Image::Group_Object, "name");
    if (name && name->size() > 0)
    {
        if (name->type() == DD::Image::STRING_ATTRIB)
            DD::Image::gl_text(name->string());
        else if (name->type() == DD::Image::STD_STRING_ATTRIB)
            DD::Image::gl_text(name->stdstring().c_str());
    }
    else
    {
        snprintf(buf, 128, "%u", ptx->index(DD::Image::Group_Primitives));
        DD::Image::gl_text(buf);
    }
    glPopMatrix();
}


/*! DD::Image::Primitive virtual method.
*/
/*virtual*/ void
PointBasedPrimitive::draw_vertex_num(DD::Image::ViewerContext*    ctx,
                                     DD::Image::PrimitiveContext* ptx) const
{
    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    char buf[128];

    const uint32_t nVerts = (uint32_t)numVerts();
    for (uint32_t v=0; v < nVerts; ++v)
    {
        const Fsr::Vec3f& p = getVertexPoint(v, points);
        glRasterPos3f(p.x, p.y, p.z);
        snprintf(buf, 128, "%u", v);
        DD::Image::gl_text(buf);
    }

    glPopMatrix();
}


/*! DD::Image::Primitive virtual method.
*/
/*virtual*/ void
PointBasedPrimitive::draw_vertex_normals(DD::Image::ViewerContext*    ctx,
                                         DD::Image::PrimitiveContext* ptx) const
{
    assert(ptx && ptx->geoinfo()); // Should always be non-zero!

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

    const uint32_t nVerts = (uint32_t)numVerts();
    for (uint32_t v=0; v < nVerts; ++v)
        DD::Image::Primitive::draw_normal(getVertexPoint(v, points), N->normal(v), ctx, ptx);

    glEnd(); // GL_LINES

    glPopMatrix();
}


/*! DD::Image::Primitive virtual method.
*/
/*virtual*/ void
PointBasedPrimitive::draw_vertex_uvs(DD::Image::ViewerContext*    ctx,
                                     DD::Image::PrimitiveContext* ptx) const
{
    assert(ptx && ptx->geoinfo()); // Should always be non-zero!

    const DD::Image::Attribute* UV =
        ptx->geoinfo()->get_typed_group_attribute(DD::Image::Group_Vertices, "uv", DD::Image::VECTOR4_ATTRIB);
    if (!UV || UV->size() == 0)
        return;

    const Fsr::Vec3fList& points = getPointLocations(ptx->geoinfo()->point_list());
    if (points.size() == 0)
        return; // don't crash

    char buf[128];

    glPushMatrix();
    glMultMatrixd(m_xform.array());

    const uint32_t nVerts = (uint32_t)numVerts();
    for (uint32_t v=0; v < nVerts; ++v)
    {
        const DD::Image::Vector4& uv = UV->vector4(v);
        snprintf(buf, 128, "[%f %f]", uv.x/uv.w, uv.y/uv.w);
        const Fsr::Vec3f& p = getVertexPoint(v, points);
        glRasterPos3f(p.x, p.y, p.z);
        DD::Image::gl_text(buf);
    }

    glPopMatrix();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// TODO: finish this code!

//typedef std::unordered_map<Fsr::HashValue, PointBasedPrimitive::VertexBuffers*> VertexBuffersMap;
//typedef std::vector<PointBasedPrimitive::VertexBuffers*> VertexBuffersList;

struct GeoInfoVertexBuffers
{
    DD::Image::Hash                     vb_hash;    //!< State of GeoInfo when last vb filled
    PointBasedPrimitive::VertexBuffers* vbuffers;   //!<

    GeoInfoVertexBuffers() : vbuffers(NULL) {}
};

typedef std::vector<GeoInfoVertexBuffers> GeoInfoVertexBuffersList;


struct GeoOpVertexBuffers
{
    DD::Image::Hash          geo_hash;          //!< State of GeoOp when last drawn (all geo hashes combined)
    GeoInfoVertexBuffersList geoinfo_vb_list;   //!< VertexBuffer pointers
    bool                     initialized;       //!<

    GeoOpVertexBuffers() : initialized(false) {}
};


static DD::Image::Lock cache_lock;

class VertexBuffersCache
{
  private:
#if __cplusplus <= 201103L
    typedef std::map<std::string, GeoOpVertexBuffers*> NodeGeoOpVertexBuffersMap;
#else
    typedef std::unordered_map<std::string, GeoOpVertexBuffers*> NodeGeoOpVertexBuffersMap;
#endif
    NodeGeoOpVertexBuffersMap m_vbuffers_map;


  public:
    //!
    VertexBuffersCache()
    {
        //std::cout << "VertexBuffersCache::ctor(): VertexBuffers singleton=" << &m_vbuffers_map << std::endl;
        //m_vbuffers_map[std::string("Foo")] = NULL;
    }

    /*! Return the static VertexBuffersMap singleton.
    */
    static NodeGeoOpVertexBuffersMap* vbuffersMap()
    {
        static VertexBuffersCache m_instance;
        //std::cout << "  VertexBuffersMap::instance(): singleton=" << &m_instance.m_vbuffers_map << std::endl;
        return &m_instance.m_vbuffers_map;
    }

    //!
    static GeoOpVertexBuffers* get(const std::string& node_name)
    {
        if (node_name.empty())
            return NULL;
        DD::Image::Guard guard(cache_lock); // just in case...
        NodeGeoOpVertexBuffersMap::const_iterator it = vbuffersMap()->find(node_name);
        if (it != vbuffersMap()->end())
            return it->second;
        return NULL; // not found
    }
    
    //!
    static void add(const std::string&  node_name,
                    GeoOpVertexBuffers* vbuffers)
    {
        if (node_name.empty() || !vbuffers)
            return;
        DD::Image::Guard guard(cache_lock); // just in case...
        (*vbuffersMap())[node_name] = vbuffers;
    }

    //!
    static void remove(const std::string& node_name)
    {
        if (node_name.empty())
            return;
        DD::Image::Guard guard(cache_lock); // just in case...
        NodeGeoOpVertexBuffersMap::const_iterator it = vbuffersMap()->find(node_name);
        if (it != vbuffersMap()->end())
        {
            delete it->second;
            vbuffersMap()->erase(it);
        }
    }
};


//! Return true if GeoInfo's PrimitiveArray is in the GeoOp's cache list.
bool infoIsInGeoOpCache(const DD::Image::GeoInfo* info,
                        DD::Image::GeoOp*         geo)
{
    if (!info || !geo || !geo->scene())
        return false;
    assert(geo->scene()->object_list()); // shouldn't happen...

    const DD::Image::GeometryList& geometry_list = *geo->scene()->object_list();
    const size_t nGeoInfos = geometry_list.size();
    for (size_t j=0; j < nGeoInfos; ++j)
        if (geometry_list[j].primitive_array() == info->primitive_array())
            return true;
    return false;
}


/*!
    TODO: finish this code!
*/
/*virtual*/ void
PointBasedPrimitive::draw_solid(DD::Image::ViewerContext*    vtx,
                                DD::Image::PrimitiveContext* ptx,
                                DD::Image::Primitive*        prev_prim) const
{
    assert(ptx);
    const DD::Image::GeoInfo* info = ptx->geoinfo();
    assert(info);

    GLint cur_gl_mode, cur_gl_list;
    glGetIntegerv(GL_LIST_MODE,  &cur_gl_mode);
    glGetIntegerv(GL_LIST_INDEX, &cur_gl_list);
    //std::cout << "cur_gl_list=" << cur_gl_list << ", info_list=" << info->callLists()->solid_call_list << std::endl;

    // Current GL drawlist mode should be GL_COMPILE, and the
    // active GL draw list id index should match GeoInfo's solid_call_list.
    // If both of those are not true then our scheme won't work...
    if (cur_gl_mode != GL_COMPILE ||
        cur_gl_list == (GLint)info->callLists()->solid_call_list)
    {
        // TODO: need a way to communicate this back to Fuser prims
        return;
    }

    // Find the GeoOp which owns this GeoInfo (which is not readily obvious) so
    // we can get the list of all GeoInfos to multi-thread through.
    //
    // GeoInfo:final_geo seems to be the best GeoOp to use as it's the one being
    // called by the draw handles, but check both final and source:
    DD::Image::GeoOp* geo = NULL;
    if (infoIsInGeoOpCache(info, info->final_geo))
        geo = info->final_geo;
    else if (infoIsInGeoOpCache(info, info->source_geo))
        geo = info->source_geo;
    if (!geo)
    {
        // TODO: need a way to communicate this back to Fuser prims
        return; // shouldn't happen, but don't crash
    }


    // Get or create the list of vertex buffers tied to the GeoOp's
    // node name which should be stable.
    // Also check using firstOp().
    // TODO: we need some way to clean up these caches!
    GeoOpVertexBuffers* node_vbs = VertexBuffersCache::get(geo->node_name());
    if (!node_vbs)
    {
        node_vbs = new GeoOpVertexBuffers();
        VertexBuffersCache::add(geo->node_name(), node_vbs);
    }
    assert(node_vbs);


    // Check the global GeometryList hash state:
    DD::Image::Hash geo_hash;
    {
        for (uint32_t i=0; i < DD::Image::Group_Last; ++i)
            geo_hash.append(geo->hash(i));

        // TODO: add anything else to the hash? GeometryList count or Scene info?
    }


    if (geo_hash == node_vbs->geo_hash)
    {
        // No global state change, fire off the worker threads:
std::cout << "  geo='" << geo->node_name() << "': no change" << std::endl;
    }
    else 
    {
std::cout << "  geo='" << geo->node_name() << "': changed, UPDATE" << std::endl;
        node_vbs->geo_hash = geo_hash;

        if (!node_vbs->initialized)
        {
            // Get the list of FuserPrimitives in all the GeoInfos:
            const DD::Image::GeometryList& geometry_list = *geo->scene()->object_list();
            const size_t nSceneGeoInfos = geometry_list.size();
std::cout << "    nSceneGeoInfos=" << nSceneGeoInfos << ":" << std::endl;
            size_t nVBs = 0;
            for (size_t j=0; j < nSceneGeoInfos; ++j)
            {
                const DD::Image::GeoInfo& scene_info = geometry_list[j];
                const uint32_t nPrims = scene_info.primitives();
                for (uint32_t i=0; i < nPrims; ++i)
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
                    if (info->primitive(i)->getPrimitiveType() > FUSER_NODE_PRIMITIVE_TYPE)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    if (info->primitive(i)->getPrimitiveType() > FUSER_NODE_PRIMITIVE_TYPE)
#pragma GCC diagnostic pop
#endif
                        ++nVBs;
            }
std::cout << "      nVBs=" << nVBs << std::endl;

            if (nVBs > 0)
            {
                // Create the VertexBuffers:
                node_vbs->geoinfo_vb_list.resize(nVBs);
                nVBs = 0;
                for (size_t j=0; j < nSceneGeoInfos; ++j)
                {
                    const DD::Image::GeoInfo& scene_info = geometry_list[j];
                    const uint32_t nPrims = scene_info.primitives();
                    for (uint32_t i=0; i < nPrims; ++i)
                    {
                        const DD::Image::Primitive* prim = info->primitive(i);
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
                        if (prim->getPrimitiveType() > FUSER_NODE_PRIMITIVE_TYPE)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        if (prim->getPrimitiveType() > FUSER_NODE_PRIMITIVE_TYPE)
#pragma GCC diagnostic pop
#endif
                        {
#if 0
                            PointBasedPrimitive::VertexBuffers* vb = new PointBasedPrimitive::VertexBuffers();
                            assert(vb);

                            // TODO: figure out the correct hash state to build here:
                            DD::Image::Hash geoinfo_hash;
                            geoinfo_hash.append(scene_info.src_id());
                            geoinfo_hash.append(scene_info.out_id());

                            GeoInfoVertexBuffers& geoinfo_vb = node_vbs->geoinfo_vb_list[nVBs];
                            geoinfo_vb.vb_hash  = geoinfo_hash;
                            geoinfo_vb.vbuffers = vb;

                            ++nVBs;
#endif
                        }
                    }
                }
            }

            node_vbs->initialized = true;
        }

        //const GeoInfoVertexBuffersList& vb_list = 
    }
    
}


/*virtual*/ void
PointBasedPrimitive::draw_wireframe(DD::Image::ViewerContext*,
                                    DD::Image::PrimitiveContext*,
                                    DD::Image::Primitive*) const
{
    // TODO: finish this code!
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace Fsr


// end of Fuser/PointBasedPrimitive.cpp

//
// Copyright 2019 DreamWorks Animation
//
