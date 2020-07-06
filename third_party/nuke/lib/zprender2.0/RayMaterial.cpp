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

/// @file zprender/RayMaterial.cpp
///
/// @author Jonathan Egstad


#include "RayMaterial.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "Sampling.h"
#include "VolumeShader.h"


// Remove these when shader creation changed to RayShader::create() calls
#include "zprReadUVTexture.h"
#include "zprPreviewSurface.h"
#include "zprAttributeReader.h"


namespace zpr {


//-----------------------------------------------------------------------------


static Fsr::Vec4f vec4_zero(0.0f, 0.0f, 0.0f, 0.0f);
static Fsr::Vec4f  vec4_one(1.0f, 1.0f, 1.0f, 1.0f);


//-----------------------------------------------------------------------------


/*!
*/
RayMaterial::RayMaterial() :
    m_surface_shader(NULL),
    m_displacement_shader(NULL),
    m_volume_shader(NULL),
    m_texture_channels(DD::Image::Mask_None),
    m_output_channels(DD::Image::Mask_None)
{
    m_shaders.reserve(5);
}


//!
RayMaterial::RayMaterial(std::vector<RayShader*> shaders,
                         RayShader*              output_surface_shader,
                         RayShader*              output_displacement_shader,
                         RayShader*              output_volume_shader) :
    m_shaders(shaders),
    m_surface_shader(output_surface_shader),
    m_displacement_shader(output_displacement_shader),
    m_volume_shader(output_volume_shader),
    m_texture_channels(DD::Image::Mask_None),
    m_output_channels(DD::Image::Mask_None)
{
    //
}


/*! Deletes any RayShader children.
*/
RayMaterial::~RayMaterial()
{
    for (size_t i=0; i < m_shaders.size(); ++i)
        delete m_shaders[i];
}


/*! Adds a shader to the group list, taking ownership of shader allocation.

    Return the same pointer as a convenience so you can do this:
        RayShader* foo = material.addShader(new Foo());
*/
RayShader*
RayMaterial::addShader(RayShader* shader)
{
    if (shader)
        m_shaders.push_back(shader);
    return shader;
}


RayShader*
RayMaterial::addShader(const char* shader_class)
{
    return addShader(RayShader::create(shader_class));
}


/*!
*/
void
RayMaterial::validateMaterial(bool                 for_real,
                              const RenderContext& rtx)
{
    m_texture_channels = DD::Image::Mask_None;
    m_output_channels  = DD::Image::Mask_None;
    if (m_surface_shader)
    {
        m_surface_shader->validateShader(for_real, rtx);
        m_texture_channels = m_surface_shader->getTextureChannels();
        m_output_channels  = m_surface_shader->getChannels();
    }
}


/*!
*/
void
RayMaterial::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    if (m_surface_shader)
        m_surface_shader->getActiveTextureBindings(texture_bindings);
}


//-----------------------------------------------------------------------------


/*! Abstract surface shader entry point allows either legacy fragment shader
    or new ray-traced shader methods to be called.
*/
/*static*/
void
RayMaterial::doShading(RayShaderContext& stx,
                       Fsr::Pixel&       out)
{
    //std::cout << "RayMaterial::doShading() shader=" << stx.surface_shader << ", material=" << stx.material << std::endl;

    // If the material is a RayShader then we call it directly, otherwise
    // we construct a VertexContext that's compatible with std Nuke shaders:
    if (stx.surface_shader)
    {
#if 0
        // Evaluate global early-out shader options:

        // Skip surface if material doesn't shade this side:
        if (sides_mode() != RenderContext::SIDES_BOTH)
        {
            const double Rd_dot_N = stx.Rtx.dir().dot(stx.N);
            if (sides_mode() == RenderContext::SIDES_FRONT && Rd_dot_N >= 0.0)
                return;
            else if (sides_mode() == RenderContext::SIDES_BACK && Rd_dot_N < 0.0)
                return;
        }

        // Skip surface if material doesn't accept camera rays:
        if (!camera_visibility() && stx.Rtx.isCameraPath())
            return;
        // Skip surface if material doesn't accept shadow rays:
        if (!shadow_visibility() && stx.Rtx.isShadowPath())
            return;
        // Skip surface if material doesn't accept camera rays:
        if (!specular_visibility() && stx.Rtx.isReflectedPath())
            return;
        // Skip surface if material doesn't accept camera rays:
        if (!diffuse_visibility() && stx.Rtx.isDiffuseContributor())
            return;
        // Skip surface if material doesn't accept camera rays:
        if (!transmission_visibility() && stx.Rtx.isTransmittedPath())
            return;
#endif
        //------------------------------------------
        //------------------------------------------
        stx.surface_shader->evaluateSurface(stx, out);
        //------------------------------------------
        //------------------------------------------
    }
    else if (stx.material)
    {
        // Legacy shaders:
        updateDDImageShaderContext(stx, stx.thread_ctx->vtx);
        //------------------------------------------
        //------------------------------------------
        stx.material->fragment_shader(stx.thread_ctx->vtx, out);
        //------------------------------------------
        //------------------------------------------
    }

    // Handle cutout/no cutout result from shader:
    if (!out.channels.contains(DD::Image::Chan_Cutout_Alpha))
    {
        // No cutout channel enabled, copy it from alpha:
        out.cutoutAlpha() = out.alpha();
    }
}


//-----------------------------------------------------------------------------


/*! Abstract displacement entry point allows legacy displacement shader or new ray-traced
    shader methods to be called.
*/
/*static*/
void
RayMaterial::doDisplacement(RayShaderContext& stx,
                            Fsr::Pixel&       out)
{
    // If the material is a RayShader then we call it directly, otherwise
    // we construct a VertexContext that's compatible with std Nuke shaders:
    if (stx.displacement_shader)
    {
        //------------------------------------------
        //------------------------------------------
        stx.displacement_shader->evaluateDisplacement(stx, out);
        //------------------------------------------
        //------------------------------------------
    }
    else if (stx.displacement_material)
    {
        // Legacy shaders:
        updateDDImageShaderContext(stx, stx.thread_ctx->vtx);
        //------------------------------------------
        //------------------------------------------
        stx.displacement_material->displacement_shader(stx.thread_ctx->vtx, stx.thread_ctx->varray);
        //------------------------------------------
        //------------------------------------------
        memcpy(out.array(), stx.thread_ctx->varray.chan, sizeof(float)*/*DD::Image::*/VARRAY_CHANS);
    }
}


//-----------------------------------------------------------------------------


/*! This routine copies info from the SurfaceIntersection structure into the RayShaderContext
    structure.

    Possibly auto_bumps the normal.
*/
/*static*/
void
RayMaterial::updateShaderContextFromIntersection(const Traceable::SurfaceIntersection& I,
                                                 RayShaderContext&                     stx)
{
#if DEBUG
    assert(I.object);
#endif
    stx.rprim = static_cast<zpr::RenderPrimitive*>(I.object);

    // Assign shaders:
    RayMaterial* raymaterial = stx.rprim->surface_ctx->raymaterial;
    if (raymaterial)
    {
        stx.surface_shader        = raymaterial->getSurfaceShader();
        stx.displacement_shader   = raymaterial->getDisplacementShader();
    }
    stx.atmosphere_shader     = NULL; // Current VolumeShader being evaluated

    stx.material              = stx.rprim->surface_ctx->material;              // legacy Nuke shader (fragment_shader)
    stx.displacement_material = stx.rprim->surface_ctx->displacement_material; // legacy Nuke shader (vertex shander)

    // TODO: add w2l, t2w to surface context?
#if 0
    stx.w2l = NULL;
    stx.l2w = NULL;
#else
    //DD::Image::MatrixArray* transforms = ((GeoInfoContext*)stx.rprim->surface_ctx->parent_object_ctx)->motion_geoinfos[0].transforms;
    stx.w2l = NULL;//&transforms->matrix(WORLD_TO_LOCAL);
    stx.l2w = NULL;//&transforms->matrix(LOCAL_TO_WORLD);
#endif


    //----------------------------------------------
    // Update geometric params from Intersection:
    //----------------------------------------------
    stx.distance = I.t;

    /* RayShaderContext:
        Fsr::Vec3d PW;                  //!< Displaced shading point in world-space
        Fsr::Vec3d dPWdx;               //!< PW x-derivative
        Fsr::Vec3d dPWdy;               //!< PW y-derivative
        Fsr::Vec3d PWg;                 //!< Geometric surface point (no displacement)

        Fsr::Vec2f st;                  //!< Primitive's barycentric coordinates at Rtx intersection
        Fsr::Vec2f Rxst;                //!< Primitive's barycentric coordinates at Rtdx intersection
        Fsr::Vec2f Ryst;                //!< Primitive's barycentric coordinates at Rtdy intersection

        Fsr::Vec3d N;                   //!< Shading normal (interpolated & bumped vertex normal)
        Fsr::Vec3d dNdx;                //!< N x-derivative
        Fsr::Vec3d dNdy;                //!< N y-derivative
        Fsr::Vec3d Nf;                  //!< Face-forward shading normal
        Fsr::Vec3d Ng;                  //!< Geometric surface normal
        Fsr::Vec3d Ngf;                 //!< Face-forward geometric normal
        Fsr::Vec3d Ns;                  //!< Interpolated surface normal (same as N but with no bump)

        Fsr::Vec2f UV;                  //!< Surface texture coordinate
        Fsr::Vec2f dUVdx;               //!< UV x-derivative
        Fsr::Vec2f dUVdy;               //!< UV y-derivative

        Fsr::Vec4f Cf;                  //!< Vertex color
        Fsr::Vec4f dCfdx;               //!< Vertex color x-derivative
        Fsr::Vec4f dCfdy;               //!< Vertex color y-derivative
    */

    const Fsr::Vec3d V = -stx.Rtx.dir(); // view-vector

    stx.PW    = I.PW;
    stx.PWg   = I.PWg;   // PW non-displaced
    stx.dPWdx = (I.RxPW - I.PW); // PW x-derivative
    stx.dPWdy = (I.RyPW - I.PW); // PW y-derivative
    //
    stx.st    = I.st;
    stx.Rxst  = I.Rxst;
    stx.Ryst  = I.Ryst;
    //
    stx.dNdx  = (I.RxN  - I.N);  // Surface normal x-derivative
    stx.dNdy  = (I.RyN  - I.N);  // Surface normal y-derivative
    //
    stx.Ng    = I.Ng;    // Geometric normal
    stx.Ngf   = RayShader::faceForward(stx.Ng,
                                       V/*view vector*/,
                                       I.Ng/*geometric normal*/); // Face-forward geometric normal
    //
    stx.N    = I.N;     // May get updated by auto_bump()
    stx.Ns   = I.Ns;    // Interpolated surface normal (same as N but with no bump)
    stx.Nf   = RayShader::faceForward(stx.N,
                                      V/*view vector*/,
                                      I.Ng/*geometric normal*/);  // Face-forward shading normal

    //------------------------------------------------------
    // Get interpolated vertex attributes from primitive:
    //------------------------------------------------------
    if (stx.use_differentials)
    {
        Fsr::Pixel vP, vdX, vdY;
        stx.rprim->getAttributesAtSurfaceIntersection(I, DD::Image::Mask_All, vP, vdX, vdY);

        const Fsr::Vec4f& uv = vP.UV();
        stx.UV.set(uv.x/uv.w, uv.y/uv.w);

        const Fsr::Vec4f& dUVdu = vdX.UV();
        const Fsr::Vec4f& dUVdv = vdY.UV();
        const float iw2 = 1.0f / (uv.w * uv.w);
        stx.dUVdx.set((dUVdu.x*uv.w - dUVdu.w*uv.x)*iw2, (dUVdu.y*uv.w - dUVdu.w*uv.y)*iw2);
        stx.dUVdy.set((dUVdv.x*uv.w - dUVdv.w*uv.x)*iw2, (dUVdv.y*uv.w - dUVdv.w*uv.y)*iw2);

        // Apply autobump, this will update I.PW & I.N:
#if 0
#if 0
        stx.rprim->doAutoBump(stx, I);
#else
        if (stx.rprim->surface_ctx->displacement_enabled &&
              stx.rprim->surface_ctx->raymaterial)
        {
           Fsr::Pixel out;
           RayShader::doDisplacement(stx, out);
           // Update intersection surface info:
           stx.PW = out.PW();
           stx.N  = out.N();
           // TODO: update derivatives as well...?
        }
#endif
#endif

        // Vertex attribs:
        stx.Cf    = vP.Cf();  // Cf
        stx.dCfdx = vdX.Cf(); // Cf x-derivative
        stx.dCfdy = vdY.Cf(); // Cf y-derivative
    }
    else
    {
        Fsr::Pixel v;
        stx.rprim->getAttributesAtSurfaceIntersection(I, DD::Image::Mask_All, v);

        const Fsr::Vec4f& uv = v.UV();
        stx.UV.set(uv.x/uv.w, uv.y/uv.w);

        // Disable texture filtering if no differentials:
        stx.dUVdx.set(0.0f, 0.0f);
        stx.dUVdy.set(0.0f, 0.0f);
        stx.texture_filter = NULL;

        // Apply autobump, this will update I.PW & I.N:
#if 0
#if 0
        stx.rprim->doAutoBump(stx, I);
#else
        if (stx.rprim->surface_ctx->displacement_enabled &&
              stx.rprim->surface_ctx->raymaterial)
        {
           Fsr::Pixel out;
           RayShader::doDisplacement(stx, out);
           // Update intersection surface info:
           stx.PW = out.PW();
           stx.N  = out.N();
           // TODO: update derivatives as well...?
        }
#endif
#endif

        // Vertex attribs:
        stx.Cf = v.Cf(); // Cf
    }

}


/*! Legacy shader interface.
    We construct a DD::Image::VertexContext that can be passed to a fragment_shader():
*/
/*static*/
void
RayMaterial::updateDDImageShaderContext(const RayShaderContext&   stx,
                                        DD::Image::VertexContext& vtx)
{
#if DEBUG
    assert(stx.rprim);
    assert(stx.rprim->surface_ctx);
    assert(stx.rprim->surface_ctx->parent_object_ctx);
#endif

    // Always use the displaced point from the intersection test:
    vtx.PW()    = (stx.PW - stx.rtx->global_offset).asVec3f(); // back to world-space
    vtx.dPWdu() = stx.dPWdx;
    vtx.dPWdv() = stx.dPWdy;

    // Get view vector (also called eye(I) vector) which is just the inverted ray direction:
    //Fsr::Vec3d V = -stx.Rtx.dir();

    // If we're shading flat then use Ng::
    if (stx.rtx->k_shading_interpolation == RenderContext::SHADING_CONSTANT)
    {
        vtx.N() = stx.Ng;
    }
    else
    {
        vtx.N() = stx.N;//face_forward(stx.N, V, stx.Ng);
    }

    vtx.UV().set(stx.UV.x, stx.UV.y, 0.0f, 1.0f);
    vtx.dUVdu().set(stx.dUVdx.x, stx.dUVdx.y, 0.0f, 0.0f);
    vtx.dUVdv().set(stx.dUVdy.x, stx.dUVdy.y, 0.0f, 0.0f);

    vtx.Cf()    = stx.Cf;
    vtx.dCfdu() = stx.dCfdx;
    vtx.dCfdv() = stx.dCfdy;
    //std::cout << "Cf[" << vtx.r() << " " << vtx.g() << " " << vtx.b() << "]" << std::endl;

    // Assign current scene, primitive, primitive transforms,
    // render primitive and render material for shader access:
    const GeoInfoContext*         gptx = (const GeoInfoContext*)stx.rprim->surface_ctx->parent_object_ctx;
    const uint32_t                obj0 = gptx->motion_objects[0].index;
    const GeoInfoContext::Sample& gtx0 = gptx->motion_geoinfos[0];

    vtx.set_transforms(gtx0.transforms);
    vtx.set_geoinfo(gtx0.info);
    vtx.set_renderstate(&gtx0.info->renderState);
    vtx.set_primitive(NULL); // is this safe...? I think so since we're only calling fragment_shader()
    //vtx.set_primitive(gtx0.info->primitive_array()[stx.rprim->surface_ctx->prim_index]);
    vtx.set_rprimitive(NULL);

    // Make sure P().w is 1.0 - if not shaders that assume the vertex params are
    // in homogeneous space may screw up their calculations...
    vtx.P().set(float(stx.x), float(stx.y), 0.0f, 1.0f);
    vtx.dPdu() = vec4_zero;
    vtx.dPdv() = vec4_zero;

    vtx.ambient.set(0.0f, 0.0f, 0.0f);

    // This is set by the first Iop that fragment_shader() is called
    // on and is used by the fragment blending logic.
    vtx.blending_shader = 0;

    // Whether the default shader should sample its texture map.
    // Relighting systems turn this off because they've already
    // sampled their texture:
    vtx.texture_sampling = true;

    vtx.set_rmaterial(stx.material);

    if (stx.master_lighting_scene)
    {
        // Lighting enabled:
        if (stx.per_object_lighting_scenes)
        {
#if DEBUG
            assert(obj0 < stx.per_object_lighting_scenes->size());
#endif
            vtx.set_scene((*stx.per_object_lighting_scenes)[obj0]);
        }
        else
        {
            vtx.set_scene(stx.master_lighting_scene);
        }
    }
    else
    {
        // No lighting enabled:
        vtx.set_scene(&stx.thread_ctx->dummy_lighting_scene);
    }

    if (!stx.use_differentials)
        vtx.scene()->filter(NULL);
    else
        vtx.scene()->filter(stx.texture_filter);

    // Set this to false to avoid the Iop::fragment_shader() from over-ing
    // the sample - unfortunately it won't sample alpha properly then, so
    // we must to it to true...:
    vtx.scene()->transparency(true);
}


//-----------------------------------------------------------------------------


/*! Abstracted illumination entry point.
*/
/*static*/
void
RayMaterial::getIllumination(RayShaderContext&                stx,
                             Fsr::Pixel&                      out,
                             Traceable::DeepIntersectionList* deep_out)
{
#if DEBUG
    assert(stx.rtx); // shouldn't happen...
#endif
    //std::cout << "RayShader::getIllumination(): Rtx[" << stx.Rtx << "]" << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();
    out.Z() = std::numeric_limits<float>::infinity();

    if (deep_out)
        deep_out->clear();

    // Make sure ray is valid:
    const Fsr::Vec3d& Rd = stx.Rtx.dir();
    if (::isnan(Rd.x) || ::isnan(Rd.y) || ::isnan(Rd.z))
        return;

    // Are we at max depth?
    if (++stx.depth >= stx.rtx->ray_max_depth)
        return;

    // Validate the current index-of-refraction.
    // If not yet defined, default to air
    if (stx.index_of_refraction < 0.0)
        stx.index_of_refraction = 1.00029; // ior of air

    Fsr::Pixel& surface_color = stx.thread_ctx->surface_color;
    surface_color.channels = out.channels;
    Fsr::Pixel& volume_color = stx.thread_ctx->volume_color;
    volume_color.channels = out.channels;

    bool  surface_is_cutout = false;
    float surface_Zf        = 0.0f;

    //-----------------------------------------------------------
    // Intersect and shade hard surfaces
    //
    Traceable::SurfaceIntersectionList& I_list = stx.thread_ctx->I_list;
    I_list.clear();

    double tmin = stx.Rtx.mindist;
    double tmax = stx.Rtx.maxdist;
    stx.rtx->objects_bvh.getIntersections(stx, I_list, tmin, tmax);
    uint32_t nSurfaces = (uint32_t)I_list.size();

    if (nSurfaces > 10000)
    {
        std::cout << "error! intersection count " << nSurfaces;
        std::cout << " exceeds max allowed - tmin=" << tmin << " tmax=" << tmax;
        std::cout << ", this is likely due to a coding error.";
        std::cout << std::endl;
    }
    else if (nSurfaces > 0 && tmin < tmax)
    {
        std::vector<uint32_t>& sorted_list = stx.thread_ctx->index_list;
        sorted_list.clear();

        // TODO: isn't there a faster way to sort these...? If we sort the intersection
        // list using std::sort() there's a bunch of memcps and a SurfaceIntersection is
        // kinda big. So I think we just need another struct with Z and index and sort that,
        // just like we do in an OpenDCX DeepPixel.
#if 0
        Traceable::sortIntersections(I_list);
#else
        if (nSurfaces == 1)
        {
            // No need to sort:
            sorted_list.push_back(0);
        }
        else if (nSurfaces == 2)
        {
            if (I_list[0].t < I_list[1].t)
            {
                sorted_list.push_back(0);
                sorted_list.push_back(1);
            }
            else
            {
                sorted_list.push_back(1);
                sorted_list.push_back(0);
            }
        }
        else
        {
            for (uint32_t i=0; i < nSurfaces; ++i)
            {
                const Traceable::SurfaceIntersection& I = I_list[i];
                sorted_list.push_back(0);
                for (int j=(int)sorted_list.size() - 1; ; --j)
                {
                    if (j == 0 || I.t > I_list[sorted_list[j - 1]].t)
                    {
                        sorted_list[j] = i;
                        break;
                    }
                    sorted_list[j] = sorted_list[j - 1];
                }
            }
            //
            nSurfaces = (uint32_t)sorted_list.size();
        }
#endif

        if (stx.rtx->k_show_diagnostics == RenderContext::DIAG_BVH_LEAF)
        {
            //-----------------------------------------------------------
            // Output diagnostic info
            //
            const Traceable::SurfaceIntersection& I = I_list[sorted_list[0]];
            const float Rd_dot_N = powf(float(stx.Rtx.dir().dot(-I.Ns))*0.5f, 1.0f/0.26f);

            out.color().set(Rd_dot_N);
            out.alpha() = 1.0f;
            out.cutoutAlpha() = 1.0f;
        }
        else
        {
            //-----------------------------------------------------------
            // Shade the surfaces from front to back
            //
            bool have_first_solid_surface = false;

            // Temp channel to accumulate 'true' alpha, don't add to out's channel set:
            out.cutoutAlpha() = 0.0f;

            // Iterate through surfaces from near to far:
            for (uint32_t i=0; i < nSurfaces; ++i)
            {
                const Traceable::SurfaceIntersection& I = I_list[sorted_list[i]];

                // Skip surface if it's too close to Ray origin or no object:
                if (I.t < std::numeric_limits<double>::epsilon() || I.object == NULL)
                    continue;

                // Evaluate the surface shader and determine if it's transparent enough to
                // continue tracing:
                // TODO: we need to use the RayShaderContexts in the thread_ctx for this!
                RayShaderContext stx_shade(stx);
                updateShaderContextFromIntersection(I, stx_shade);

                // Having surface_color be black is essential to front-to-back
                // under-ing because the Nuke legacy shaders are doing overs
                // internally:
                surface_color.clearAllChannels();

                //------------------------------------------------
                //------------------------------------------------
                doShading(stx_shade,
                          surface_color);

                surface_Zf = float(I.t); // default Z to intersection distance
                surface_color.Z() = surface_Zf; 
                //------------------------------------------------
                //------------------------------------------------

                if (deep_out)
                    deep_out->push_back(Traceable::DeepIntersection(I, surface_color, stx.sampler->subpixel.spmask));

                // Save current A & B alphas, as out[Chan_Alpha] can get mucked up in merges below:
                const float Aa = surface_color.alpha();
                const bool is_solid_surface = (Aa >= stx.rtx->k_alpha_threshold);
                // If the surface isn't solid don't bother adding it to output:
                if (!is_solid_surface)
                    continue;

                // Get AOVs:
                const uint32_t nAOVs = (uint32_t)stx.rtx->aov_outputs.size();
                if (nAOVs > 0)
                {
                    //===========================================================
                    //                        AOVs
                    //===========================================================
                    for (uint32_t j=0; j < nAOVs; ++j)
                    {
                        const AOVLayer& aov = stx.rtx->aov_outputs[j];
#if DEBUG
                        assert(aov.type < AOV_LAST_TYPE && aov.handler);
#endif
                        // Call the aov handler to extract values:
                        aov.handler(stx_shade, aov, surface_color);
                    }
                }

                if (!have_first_solid_surface)
                {
                    //==========================================
                    // First surface - direct copy
                    //==========================================
                    have_first_solid_surface = true;

                    if (surface_is_cutout)
                    {
                        // Matte object, color chans are black so just replace alpha:
                        out.alpha() = Aa;
                    }
                    else
                    {
                        // First surface is normally just a replace:
                        foreach(z, stx.rtx->under_channels)
                            out[z] = surface_color[z];
                        out.cutoutAlpha() = Aa;
                    }

                    bool do_Z = true;
                    if (nAOVs > 0)
                    {
                        //===========================================================
                        //                        AOVs
                        //===========================================================
                        for (uint32_t j=0; j < nAOVs; ++j)
                        {
                            const AOVLayer& aov = stx.rtx->aov_outputs[j];
                            //
                            if (aov.mask.contains(DD::Image::Chan_Z))
                                do_Z = false; // This AOV writes Z
                            //
                            // Only apply the premulting to AOV when there's more than one surface:
                            if (aov.merge_mode == AOVLayer::AOV_MERGE_PREMULT_UNDER &&
                                stx.rtx->k_transparency_enabled)
                            {
                                // Premult AOV by Aa:
                                foreach(z, aov.mask)
                                    out[z] = surface_color[z]*Aa;
                            }
                            else
                            {
                                // Just copy:
                                foreach(z, aov.mask)
                                    out[z] = surface_color[z];
                            }
                        }
                    }
                    // Handle Z even if no AOV has:
                    if (do_Z)
                        out.Z() = float(I.t);

                    // If we're not allowing transparency or the surface is solid
                    // we're done at solid first surface:
                    if (!stx.rtx->k_transparency_enabled ||
                        out.alpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
                    {
                        if (out.alpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
                           out.alpha() = 1.0f;

                        // Final cutout alpha remains in the Chan_Cutout_Alpha!  We don't move it
                        // to Chan_Alpha here so that we can do more cutout logic post illumination()
                        if (out.cutoutAlpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
                            out.cutoutAlpha() = 1.0f;

                        break;
                    }

                    continue;
                }

                const float Ba  = out.alpha();
                const float iBa = (1.0f - Ba);

                // UNDER the non-aov channels:
                if (surface_is_cutout)
                {
                    // Matte object, color chans are black so just under alpha:
                    out.alpha() += Aa*iBa;
                }
                else
                {
                    //A_under_B(surface_color, out, stx.rtx->under_channels);
                    if (Ba < std::numeric_limits<float>::epsilon())
                    {
                        foreach(z, stx.rtx->under_channels)
                            out[z] += surface_color[z];
                    }
                    else if (Ba < 1.0f)
                    {
                        foreach(z, stx.rtx->under_channels)
                            out[z] += surface_color[z]*iBa;
                    }
                    else
                    {
                        // saturated B alpha - do nothing
                    }
                    out.cutoutAlpha() += Aa*iBa;
                }

                bool do_Z = true;
                if (nAOVs > 0)
                {
                    //===========================================================
                    //                        AOVs
                    // TODO: implement aov merge handlers!
                    //===========================================================
                    for (uint32_t j=0; j < nAOVs; ++j)
                    {
                        const AOVLayer& aov = stx.rtx->aov_outputs[j];
                        //
                        if (aov.mask.contains(DD::Image::Chan_Z))
                            do_Z = false; // This AOV writes Z
                        //
                        switch (aov.merge_mode) {
                        case AOVLayer::AOV_MERGE_UNDER: {
                            foreach(z, aov.mask)
                            {
                                if (z == DD::Image::Chan_Z && ::isinf(out[z]))
                                    out[z] = surface_color[z];
                                else
                                    out[z] += surface_color[z]*iBa;
                            }
                            break;}

                        case AOVLayer::AOV_MERGE_PREMULT_UNDER: {
                            foreach(z, aov.mask)
                            {
                                if (z == DD::Image::Chan_Z && ::isinf(out[z]))
                                    out[z] = surface_color[z]*Aa;
                                else
                                    out[z] += surface_color[z]*Aa*iBa;
                            }
                            break;}

                        case AOVLayer::AOV_MERGE_PLUS:
                            foreach(z, aov.mask)
                            {
                                if (z == DD::Image::Chan_Z && ::isinf(out[z]))
                                    out[z] = surface_color[z];
                                else
                                    out[z] += surface_color[z];
                            }
                            break;

                        case AOVLayer::AOV_MERGE_MIN:
                            foreach(z, aov.mask)
                                out[z] = std::min(out[z], surface_color[z]);
                            break;

                        case AOVLayer::AOV_MERGE_MID:
                            foreach(z, aov.mask)
                            {
                                if (z == DD::Image::Chan_Z && ::isinf(out[z]))
                                    out[z] = surface_color[z]; // don't max if Z is infinity
                                else
                                    out[z] = (surface_color[z] + out[z])*0.5f;
                            }
                            break;

                        case AOVLayer::AOV_MERGE_MAX:
                            foreach(z, aov.mask)
                            {
                                if (z == DD::Image::Chan_Z && ::isinf(out[z]))
                                    out[z] = surface_color[z]; // don't max if Z is infinity
                                else
                                    out[z] = std::max(out[z], surface_color[z]);
                            }
                            break;
                        }
                    }
                }
                // Handle Z even if no AOV has:
                if (do_Z && is_solid_surface)
                    out.Z() = std::min(out.Z(), float(I.t));

                // Now check surface transparency - if it's almost 1.0 we can stop:
                if (out.alpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
                {
                    out.alpha() = 1.0f;
                    break;
                }

            } // diagnostic or surface shading

        } // nSurfaces loop

        // Update the final cutout status:
        surface_is_cutout = (out[stx.cutout_channel] > 0.5f);

    } // have surfaces


    //-----------------------------------------------------------
    // Intersect and ray march volumes
    //
    if (stx.atmosphere_shader)
    {
        Volume::VolumeIntersectionList& vol_intersections = stx.thread_ctx->vol_intersections;
        I_list.clear();
        double vol_tmin, vol_tmax;
        double vol_segment_min_size, vol_segment_max_size;
        if (stx.atmosphere_shader->getVolumeIntersections(stx,
                                                          vol_intersections,
                                                          vol_tmin,
                                                          vol_tmax,
                                                          vol_segment_min_size,
                                                          vol_segment_max_size))
        {
            //std::cout << "RayShader::getIllumination(): Rtx[" << stx.Rtx << "] atmo-shader=" << stx.atmosphere_shader;
            //std::cout << "  nVolumeIntersections=" << vol_intersections.size() << std::endl;
            bool do_march = true;

            // If final surface alpha is 1, clamp the volume's range against the surface render.  This
            // unfortunately means that volumes between transparent surfaces are not rendered.
            // It's a compromise for speed...
            if (!stx.rtx->k_atmosphere_alpha_blending ||
                (stx.rtx->k_atmosphere_alpha_blending && out.alpha() > 0.999f))
            {
                if (vol_tmin >= surface_Zf)
                    do_march = false; // Skip if surface Z is closer than first volume
                else
                    vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(surface_Zf)));
            }

#if 0
            // TODO: re-support camera (primary) ray blending with bg Z and alpha
            // TODO: pass bg values in stx as a Pixel*?
            if (stx.Rtx.isCameraPath())
            {
                if (m_have_bg_Z && k_bg_occlusion)
                {
                    // Clamp tmax to bg Z to speed up march, but only if we're not
                    // alpha blending, and the alpha is < 1:
                    if (!stx.rtx->k_atmosphere_alpha_blending ||
                        (stx.rtx->k_atmosphere_alpha_blending && bg.alpha() > 0.999f))
                    {
                        if (vol_tmin >= bg.Z())
                            do_march = false; // Skip if bg Z is closer than first volume
                        else
                            vol_tmax = std::min(vol_tmax, std::max(vol_tmin, double(bg.Z())));
                    }
                }
            }
#endif

            // Finally check if cutout surface is in front of all volumes:
            if (surface_is_cutout && surface_Zf <= vol_tmin)
                do_march = false;

            if (do_march)
            {
#if 0
                if (stx.rtx->k_show_diagnostics == RenderContext::DIAG_VOLUMES)
                {
                    Raccum.red()   += float(vol_tmin);
                    Raccum.green() += float(vol_tmax);
                    Raccum.blue()  += float(vol_segment_min_size);
                    Raccum.alpha() += float(vol_segment_max_size);
                    // Take min Z:
                    if (surface_Zf < accum_Z)
                        accum_Z = surface_Zf;
                    coverage += 1.0f;
                    ++sample_count;
                    continue;
                }
#endif

                // Ray march through volumes:
                volume_color.clearAllChannels();
                if (stx.atmosphere_shader->volumeMarch(stx,
                                                       vol_tmin,
                                                       vol_tmax,
                                                       vol_segment_min_size,
                                                       vol_segment_max_size,
                                                       surface_Zf,
                                                       out.alpha(),
                                                       vol_intersections,
                                                       volume_color,
                                                       NULL/*deep_out*/))
                {
                    // Add volume illumination to final:
                    out.color()       += volume_color.color();
                    out.alpha()       += volume_color.alpha();
                    out.cutoutAlpha() += volume_color.cutoutAlpha();

                    // Take min Z:
                    if (volume_color.Z() < surface_Zf)
                        surface_Zf = volume_color.Z();
                }
            }

        } // nVolumes > 0

    } // doVolumes

    // Final cutout alpha remains in the Chan_Cutout_Alpha!  We don't move it
    // to Chan_Alpha here so that we can do more cutout logic post illumination()
    if (out.cutoutAlpha() >= (1.0f - std::numeric_limits<float>::epsilon()))
        out.cutoutAlpha() = 1.0f;
}


//-----------------------------------------------------------------------------


/*! Create and connect up the input shaders for this Fuser ShaderNode.
    Returns the output connection to the created RayShader(s).
*/
RayShader*
createSurfaceShaders(const Fsr::ShaderNode*   fsr_shader,
                     std::vector<RayShader*>& ray_shaders)
{
    if (!fsr_shader)
        return NULL; // don't crash

    const std::string shader_class = fsr_shader->getString("shader:class");    
    //std::cout << "  createSurfaceShaders(" << fsr_shader->getName() << ") class='" << shader_class << "'" << std::endl;

    // TODO: this isn't huge but we could use a map table lookup here to speed things up
    RayShader* output = NULL;
    if      (shader_class == "UsdPreviewSurface")
    {
        zprPreviewSurface* psurf = new zprPreviewSurface();
        output = psurf;
    }
    //---------------------------------------------------------------
    else if (shader_class == "UsdUVTexture")
    {
        // Change these to RayShader::create() calls:
        zprReadUVTexture* reader = new zprReadUVTexture(""/*path*/);
        reader->k_wrapS = 0;
        reader->k_wrapT = 0;
        reader->k_fallback.set(1.0f);
        reader->k_scale.set(1.0f);
        reader->k_bias.set(0.0f);

        output = reader;
    }
    //---------------------------------------------------------------
    /*
    UsdPrimvarReader types supported by Storm:
        UsdPrimvarReader_string
        UsdPrimvarReader_int
        UsdPrimvarReader_float
        UsdPrimvarReader_float2
        UsdPrimvarReader_float3
        UsdPrimvarReader_float4
        UsdPrimvarReader_point
        UsdPrimvarReader_normal
        UsdPrimvarReader_vector
        UsdPrimvarReader_matrix
    */
    else if (shader_class == "UsdPrimvarReader_string")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_int")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_float")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_float2")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_float3")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_float4")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_point")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_normal")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_vector")
    {
        output = new zprAttributeReader();
    }
    else if (shader_class == "UsdPrimvarReader_matrix")
    {
        output = new zprAttributeReader();
    }
    //---------------------------------------------------------------
    else if (shader_class == "Transform2d")
    {
        output = NULL;
    }

    if (output)
    {
        output->setName(fsr_shader->getName());

        ray_shaders.push_back(output);

        // Convert each input and attach them:
        const uint32_t nInputs = fsr_shader->numInputs();
        for (uint32_t i=0; i < nInputs; ++i)
        {
            const Fsr::ShaderNode::InputBinding& fsr_binding = fsr_shader->getInput(i);

            const int32_t ray_shader_input = output->getInputByName(fsr_binding.name.c_str());
            if (ray_shader_input < 0)
                continue; // no match on RayShader, skip it

            if (fsr_binding.source_shader)
            {
                // Input binding:
                // Create and connect up SurfaceMaterialOp input:
                RayShader* input_ray_shader = createSurfaceShaders(fsr_binding.source_shader, ray_shaders);
                if (input_ray_shader)
                {
                    //std::cout << "      " << fsr_shader->getName() << ": connect input " << i << "'" << fsr_binding.name << "'";
                    //std::cout << "(" << fsr_binding.type << ")";
                    //std::cout << " to shader '" << input_ray_shader->getName() << "'" << std::endl;

                    output->connectInput(ray_shader_input, input_ray_shader, fsr_binding.source_output_name.c_str());
                }
            }
            else
            {
                // Knob binding:
                //std::cout << "      " << fsr_shader->getName() << ": knob '" << fsr_binding.name << "'";
                //std::cout << "(" << fsr_binding.type << ")";
                //std::cout << "=[" << fsr_binding.value << "]" << std::endl;

                // Copy value from ShaderNode knob to RayShader knob:
                const RayShader::InputKnob& output_knob = output->getInputKnob(ray_shader_input);
                if      (fsr_binding.type == "int"   )
                {
                    if (output_knob.type == RayShader::INT_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "double")
                {
                    if (output_knob.type == RayShader::DOUBLE_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "string" || fsr_binding.type == "file")
                {
                    if (output_knob.type == RayShader::STRING_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                //
                else if (fsr_binding.type == "vec2"  )
                {
                    if (output_knob.type == RayShader::VEC2_KNOB ||
                        output_knob.type == RayShader::COLOR2_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "vec3"  )
                {
                    if (output_knob.type == RayShader::VEC3_KNOB ||
                        output_knob.type == RayShader::COLOR3_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "vec4"  )
                {
                    if (output_knob.type == RayShader::VEC4_KNOB ||
                        output_knob.type == RayShader::COLOR4_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                //
                else if (fsr_binding.type == "vec2[]")
                {
                    if (output_knob.type == RayShader::VEC2ARRAY_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "vec3[]")
                {
                    if (output_knob.type == RayShader::VEC3ARRAY_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else if (fsr_binding.type == "vec4[]")
                {
                    if (output_knob.type == RayShader::VEC4ARRAY_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                //
                else if (fsr_binding.type == "mat4"  )
                {
                    if (output_knob.type == RayShader::MAT4_KNOB)
                        output->setInputValue(ray_shader_input, fsr_binding.value.c_str());
                }
                else
                {
                    std::cout << "      " << fsr_shader->getName() << ": warning, ignoring knob ";
                    std::cout << "'" << fsr_binding.name << "'";
                    std::cout << "(" << fsr_binding.type << ")";
                    std::cout << "=[" << fsr_binding.value << "]" << std::endl;
                }
            }
        }
    }

    return output;
}





/*! Build a RayMaterial with the Fuser ShaderNodes converted to zpr RayShader
    equivalents of UsdPreviewSurface shaders.
*/
/*static*/
RayMaterial*
RayMaterial::createUsdPreviewSurface(Fsr::ShaderNode* surface_output)
{
    assert(surface_output);
    //std::cout << "RayMaterial::createUsdPreviewSurface(): '" << surface_output->getName() << "'" << std::endl;

    std::vector<RayShader*> all_shaders;
    all_shaders.reserve(50);

    RayShader* output_surface_shader      = createSurfaceShaders(surface_output, all_shaders);
    RayShader* output_displacement_shader = NULL;//createDisplacementShaders(rtx, all_shaders);
    RayShader* output_volume_shader       = NULL;//createVolumeShaders(rtx, all_shaders);

    if (!output_surface_shader || all_shaders.size() == 0)
        return NULL;

    //for (size_t i=0; i < all_shaders.size(); ++i)
    //    std::cout << "  " << i << ": '" << all_shaders[i]->zprShaderClass() << "'" << std::endl;

    // Create a new material and built its shader tree:
    RayMaterial* material = new RayMaterial(all_shaders,
                                            output_surface_shader,
                                            output_displacement_shader,
                                            output_volume_shader);

    return material;
}


//-----------------------------------------------------------------------------


} // namespace zpr

// end of zprender/RayShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
