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

/// @file zprender/RayShader.cpp
///
/// @author Jonathan Egstad


#include "RayShader.h"
#include "SurfaceShaderOp.h"
#include "VolumeShader.h"
#include "SurfaceHandler.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "Sampling.h"


static DD::Image::Lock expand_lock;

namespace zpr {


//------------------------------------------------------------------------------------


static DD::Image::Vector4 vec4_zero(0.0f, 0.0f, 0.0f, 0.0f);
static DD::Image::Vector4  vec4_one(1.0f, 1.0f, 1.0f, 1.0f);


//------------------------------------------------------------------------------------


/*static*/ const char* RayShader::frame_clamp_modes[] =
{
    "none",
    "fwd-round-up",
    "fwd-round-down",
    "rev-round-up",
    "rev-round-down",
    0
};


/*!
*/
RayShader::RayShader() :
    k_sides_mode(RenderContext::SIDES_BOTH),
    k_camera_visibility(true),
    k_shadow_visibility(true),
    k_specular_visibility(true),
    k_diffuse_visibility(true),
    k_transmission_visibility(true),
    k_frame_clamp_mode(FRAME_CLAMP_NONE)
{
    //
}


//-----------------------------------------------------------------------------


//!
/*static*/ const char* RayShader::zpClass() { return "zpRayShader"; }

/*!
*/
void
RayShader::addRayShaderIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, "zpRayShader", DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
RayShader::addRayControlKnobs(DD::Image::Knob_Callback f)
{
    DD::Image::Enumeration_knob(f, &k_sides_mode, RenderContext::sides_modes, "sides_mode", "visibility");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Shader is applied to the front or back face, or both.");
    DD::Image::Bool_knob(f, &k_camera_visibility,       "camera_visibility",       "camera");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to camera rays.");
    DD::Image::Bool_knob(f, &k_shadow_visibility,       "shadow_visibility",       "shadow");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to shadow occlusion rays.");
    DD::Image::Bool_knob(f, &k_specular_visibility,     "specular_visibility",     "spec");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to specular reflection rays.");
    DD::Image::Bool_knob(f, &k_diffuse_visibility,      "diffuse_visibility",      "diff");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to diffuse reflection rays.");
    DD::Image::Bool_knob(f, &k_transmission_visibility, "transmission_visibility", "trans");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to transmissed (or refracted) rays.");
    DD::Image::Newline(f);
    DD::Image::Enumeration_knob(f, &k_frame_clamp_mode, frame_clamp_modes, "frame_clamp_mode", "frame clamp");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Modify the frame number for the shader, none, round-up or round-down.");
    DD::Image::Newline(f);
}


/*! Get the binding info for an input Op.
    If it's an Iop subclass then IS_TEXTURE is enabled.
*/
/*static*/
RayShader::MapBinding
RayShader::getOpMapBinding(DD::Image::Op*     op,
                           DD::Image::Channel alpha_chan)
{
    MapBinding binding;
    binding.type  = 0x00;
    binding.flags = 0x00;
    if (op)
    {
        binding.type |= MapBinding::OP;

        // Determine input type:
#ifdef ZPR_USE_KNOB_RTTI
        if (op->knob(SurfaceShaderOp::zpClass()))
#else
        if (dynamic_cast<SurfaceShaderOp*>(op))
#endif
            binding.type |= (MapBinding::SURFACEOP | MapBinding::MATERIAL | MapBinding::IOP);
        else if (dynamic_cast<DD::Image::Material*>(op))
            binding.type |= (MapBinding::MATERIAL | MapBinding::IOP);
        else if (dynamic_cast<DD::Image::Iop*>(op))
            binding.type |= MapBinding::IOP;

        // Handle Iop inputs:
        if (binding.isIop())
        {
            DD::Image::Iop* iop = static_cast<DD::Image::Iop*>(op);
            iop->validate(true);

            binding.flags |= MapBinding::IS_TEXTURE;

            // Does input offer an alpha?
            if (iop->channels().contains(alpha_chan))
                binding.flags |= MapBinding::HAS_ALPHA;

            // Does input offer an alpha?
            if (iop->channels().size() == 1 ||
                (iop->channels().size() == 2 && binding.hasAlpha()))
                binding.flags |= MapBinding::IS_MONO;
        }
    }
    return binding;
}


/*!
*/
/*virtual*/ void
RayShader::validateShader(bool for_real)
{
}


//-----------------------------------------------------------------------------


/*! The top-level surface evaluation shader call.
*/
void
RayShader::doGeometricShading(RayShaderContext& stx,
                              RayShaderContext& out)
{
    // TODO: The only need for this redirection is if we need to
    // pre-test / pre-build something before calling the virtual function.

    // Call virtual version:
    _evaluateGeometricShading(stx, out);
}


//-----------------------------------------------------------------------------


/*! Abstract surface shader entry point allows either legacy fragment shader
    or new ray-traced shader methods to be called.
*/
/*static*/
void
RayShader::doShading(RayShaderContext& stx,
                     Fsr::Pixel&       out)
{
    //std::cout << "RayShader::doShading() shader=" << stx.surface_shader << ", material=" << stx.material << std::endl;

    // If the material is a RayShader then we call it directly, otherwise
    // we construct a VertexContext that's compatible with std Nuke shaders:
    if (stx.surface_shader)
    {
        //------------------------------------------
        //------------------------------------------
        stx.surface_shader->evaluateShading(stx, out);
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
}


/*! Top-level ray-tracing surface shader evaluation call.
    This checks global-level params before calling the virtual subclass version.
*/
void
RayShader::evaluateShading(RayShaderContext& stx,
                           Fsr::Pixel&       out)
{
    //std::cout << "RayShader::evaluateShading(" << this << ")" << std::endl;
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
    if (!camera_visibility() && (stx.Rtx.type_mask & Fsr::RayContext::CAMERA))
        return;
    // Skip surface if material doesn't accept shadow rays:
    if (!shadow_visibility() && (stx.Rtx.type_mask & Fsr::RayContext::SHADOW))
        return;
    // Skip surface if material doesn't accept camera rays:
    if (!specular_visibility() && (stx.Rtx.type_mask & Fsr::RayContext::REFLECTION))
        return;
    // Skip surface if material doesn't accept camera rays:
    if (!diffuse_visibility() && (stx.Rtx.type_mask & Fsr::RayContext::DIFFUSE))
        return;
    // Skip surface if material doesn't accept camera rays:
    if (!transmission_visibility() && (stx.Rtx.type_mask & Fsr::RayContext::TRANSMISSION))
        return;

    // Call virtual sublcass version:
    _evaluateShading(stx, out);
}


//-----------------------------------------------------------------------------


/*! Abstract displacement entry point allows legacy displacement shader or new ray-traced
    shader methods to be called.
*/
/*static*/
void
RayShader::doDisplacement(RayShaderContext& stx,
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


/*! Top-level ray-tracing displacement shader evaluation call.
    This checks global-level params before calling the virtual subclass version.
*/
void
RayShader::evaluateDisplacement(RayShaderContext& stx,
                                Fsr::Pixel&       out)
{
    // Call virtual version:
    _evaluateDisplacement(stx, out);
}


//-----------------------------------------------------------------------------


/*! This routine copies info from the SurfaceIntersection structure into the RayShaderContext
    structure.

    Possibly auto_bumps the normal.
*/
/*static*/
void
RayShader::updateShaderContextFromIntersection(const Traceable::SurfaceIntersection& I,
                                               RayShaderContext&                     stx)
{
#if DEBUG
    assert(I.object);
#endif
    stx.rprim = static_cast<zpr::RenderPrimitive*>(I.object);

    // Assign shaders:
    stx.surface_shader        = stx.rprim->surface_ctx->surface_shader;      // RayShader
    stx.displacement_shader   = stx.rprim->surface_ctx->displacement_shader; // RayShader
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
        Fsr::Vec3d Nf;                  //!< Face-forward shading normal
        Fsr::Vec3d Ng;                  //!< Geometric surface normal
        Fsr::Vec3d Ngf;                 //!< Face-forward geometric normal
        //-------------------------------------------------------------------
        Fsr::Vec3d Ns;                  //!< Interpolated surface normal (same as N but with no bump)
        Fsr::Vec3d dNsdx;               //!< Ns x-derivative
        Fsr::Vec3d dNsdy;               //!< Ns y-derivative

        Fsr::Vec2f UV;                  //!< Surface texture coordinate
        Fsr::Vec2f dUVdx;               //!< UV x-derivative
        Fsr::Vec2f dUVdy;               //!< UV y-derivative

        Fsr::Vec4f Cf;                  //!< Vertex color
        Fsr::Vec4f dCfdx;               //!< Vertex color x-derivative
        Fsr::Vec4f dCfdy;               //!< Vertex color y-derivative
    */

    const Fsr::Vec3d V = -stx.Rtx.dir(); // view-vector

    stx.PW   = I.PW;
    stx.PWg  = I.PWg;   // PW non-displaced
    //
    stx.st   = I.st;
    stx.Rxst = I.Rxst;
    stx.Ryst = I.Ryst;
    //
    stx.Ng   = I.Ng;    // Geometric normal
    stx.Ngf  = faceForward(stx.Ng,
                           V/*view vector*/,
                           I.Ng/*geometric normal*/); // Face-forward geometric normal
    //
    stx.N    = I.N;     // May get updated by auto_bump()
    stx.Ns   = I.Ns;    // Interpolated surface normal (same as N but with no bump)
    stx.Nf   = faceForward(stx.N,
                           V/*view vector*/,
                           I.Ng/*geometric normal*/);  // Face-forward shading normal

    //------------------------------------------------------
    // Get interpolated vertex attributes from primitive:
    //------------------------------------------------------
    if (stx.use_differentials)
    {
        Fsr::Pixel vP, vdX, vdY;
        stx.rprim->getAttributesAtSurfaceIntersection(I, DD::Image::Mask_All, vP, vdX, vdY);

        stx.Ns    =  vP.N(); // Interpolated surface normal (same as N but with no bump)
        stx.dNsdx = vdX.N(); // Surface normal x-derivative
        stx.dNsdy = vdY.N(); // Surface normal y-derivative

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
              stx.rprim->surface_ctx->surface_shader)
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

        stx.Ns = v.N(); // Interpolated surface normal (same as N but with no bump)

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
              stx.rprim->surface_ctx->surface_shader)
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
RayShader::updateDDImageShaderContext(const RayShaderContext&   stx,
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
RayShader::getIllumination(RayShaderContext&                stx,
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
                surface_is_cutout = (surface_color[stx.cutout_channel] > 0.5f);
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

                    if (surface_is_cutout) {
                        // Matte object, color chans are black so just replace alpha:
                        out.alpha() = Aa;

                    } else {
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
                            } else {
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
            if (stx.Rtx.type_mask & Fsr::RayContext::CAMERA)
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


/*! Return the indirect diffuse illumination for surface point with normal N.
    Indirect diffuse means only rays that hit objects will contribute to the surface color.
*/
bool
RayShader::getIndirectDiffuse(RayShaderContext& stx,
                              const Fsr::Vec3d& N,
                              double            roughness,
                              Fsr::Pixel&       out)
{
    //std::cout << "getIndirectDiffuse(): depth=" << stx.diffuse_depth << " ray type=" << std::hex << stx.Rtx.type_mask << std::dec << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & diffuse depth:
    if (stx.Rtx.type_mask & Fsr::RayContext::DIFFUSE)
        ++stx.diffuse_depth;
    if (stx.diffuse_depth >= stx.rtx->ray_diffuse_max_depth)
        return false;

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->diffuse_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(N, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) < 0.0)
        {
            // Possibly skip rays that intersect plane of surface:
            if (i == nSamples-1 && hits == 0)
            {
                // No hits yet, do one last try that's not re-oriented:
                Rd = stx.Rtx.dir();
                if (Rd.dot(stx.Ng) < 0.0)
                    return false;
            }
            else
                continue; // skip if we have other rays to consider
        }

        // Build new diffuse ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::DIFFUSE | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayShader::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }

    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Return the indirect specular illumination for surface point with normal N.
    Indirect specular means only reflected rays that hit objects will contribute to the surface color.
*/
bool
RayShader::getIndirectGlossy(RayShaderContext& stx,
                             const Fsr::Vec3d& N,
                             double            roughness,
                             Fsr::Pixel&       out)
{
    //std::cout << "  getIndirectGlossy(): glossy depth=" << stx.glossy_depth << " ray type=0x" << std::hex << stx.Rtx.type_mask << std::dec << " max glossy=" << stx.rtx->ray_glossy_max_depth << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & glossy depth:
    if (stx.Rtx.type_mask & Fsr::RayContext::GLOSSY)
        ++stx.glossy_depth;
    if (stx.glossy_depth >= stx.rtx->ray_glossy_max_depth)
        return false;

    // Reflect the view vector:
    Fsr::Vec3d V = stx.getViewVector(); // this may build a fake-stereo view-vector
    Fsr::Vec3d Rd_reflect = V.reflect(N);
    Rd_reflect.normalize();

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->glossy_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(Rd_reflect, true/*auto_flip*/);

        // Does the reflected ray intersect the plane of surface?:
        if (Rd.dot(stx.Ng) < 0.0)
        {
            // Yes, so reflect the ray *again*, this time using Ng,
            // which is the equivalent of
            // placing a parallel plane underneath this surface to 'catch'
            // the reflected ray and send it back 'up':
            const Fsr::Vec3d Vt = -Rd;
            Rd = Vt.reflect(stx.Ng);
            // If it's still a no go and we have no other hits, and this
            // is the last sample, give up (this shouldn't happen...):
            if (hits == 0 && i == nSamples-1 && Rd.dot(stx.Ng) < 0.0)
                return false;
        }

        // Build new glossy ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::GLOSSY | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayShader::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }
    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Return the transmitted illumination for surface point with normal N.
    Transmission means only refracted rays that pass through objects will contribute to the surface color.
*/
bool
RayShader::getTransmission(RayShaderContext& stx,
                           const Fsr::Vec3d& N,
                           double            eta,
                           double            roughness,
                           Fsr::Pixel&       out)
{
    //std::cout << "getTransmission(): refraction_depth=" << stx.refraction_depth << " ray type=" << stx.Rtx.type_mask << " max refraction=" << stx.rtx->ray_refraction_max_depth << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & glossy depth:
    if (stx.Rtx.type_mask & Fsr::RayContext::GLOSSY)
        ++stx.refraction_depth;
    if (stx.refraction_depth >= stx.rtx->ray_refraction_max_depth)
        return false;

    // Refract the direction vector:
    Fsr::Vec3d Rd_refract(stx.Rtx.dir());
    RayShader::refract(stx.Rtx.dir(), stx.Nf, eta, Rd_refract);

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->refraction_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(Rd_refract, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) >= 0.0)
        {
            // Possibly skip rays that intersect plane of surface:
            if (i == nSamples-1 && hits == 0)
            {
                // No hits yet, do one last try that's not re-oriented:
                Rd = stx.Rtx.dir();
                if (Rd.dot(stx.Ng) >= 0.0)
                    return false;
            }
            else
            {
                // Skip if we have other rays to consider:
                continue;
            }
        }
        //
        // Build new glossy ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::GLOSSY | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayShader::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }
    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Get the occlusion of this surface point.

    For ambient occlusion set 'occlusion_ray_type' to DIFFUSE and
    for reflection occlusion use GLOSSY or REFLECTION, and
    TRANSMISSION for refraction occlusion.

    The value returned is between 0.0 and 1.0, where 0.0 means no
    occlusion (ie the point is completely exposed to the environment)
    and 1.0 is full-occlusion where the point has no exposure to the
    environment.
*/
/*static*/
float
RayShader::getOcclusion(RayShaderContext& stx,
                        uint32_t          occlusion_ray_type,
                        double            mindist,
                        double            maxdist,
                        double            cone_angle,
                        double            gi_scale)
{
    const SampleGrid2D* samples = NULL;

    Fsr::Vec3d N;
    switch (occlusion_ray_type)
    {
    default:
    case Fsr::RayContext::DIFFUSE:
        samples = &stx.sampler->diffuse_samples;
        N = stx.N;
        break;
    case Fsr::RayContext::REFLECTION:
    case Fsr::RayContext::GLOSSY:
    {
        samples = &stx.sampler->glossy_samples;
        Fsr::Vec3d V(-stx.Rtx.dir());
        N = V.reflect(stx.N);
        N.normalize();
        break;
    }
    case Fsr::RayContext::TRANSMISSION:
        samples = &stx.sampler->refraction_samples;
        N = -stx.N;
        break;
    case Fsr::RayContext::CAMERA:
        // Camera ray not supported for occlusion gathering:
        std::cerr << "RayShader::getOcclusion(): warning, camera ray type not supported." << std::endl;
        return 0.0f; // no occlusion
    case Fsr::RayContext::SHADOW:
        // Shadow ray not supported for occlusion gathering:
        std::cerr << "RayShader::getOcclusion(): warning, shadow ray type not supported." << std::endl;
        return 0.0f; // no occlusion
    }
    if (!samples)
        return 0.0f; // no occlusion

    if (::fabs(cone_angle) > 180.0)
        cone_angle = 180.0;
    const double cone_scale = (::fabs(cone_angle) / 180.0);

    float weight = 0.0f;
    const uint32_t nSamples = (cone_scale > std::numeric_limits<double>::epsilon()) ? samples->size() : 1;
    //std::cout << "getOcclusion [" << stx.x << " " << stx.y << "] N" << N << ", Ng" << stx.Ng;
    //std::cout << " samples=" << nSamples;
    //std::cout << std::endl;
    for (uint32_t i=0; i < nSamples; ++i)
    {
        const Sample2D& s = (*samples)[i];

        // Build a new direction vector from intersection normal:
        Fsr::Vec3d Rd(s.dp.x*cone_scale,
                      s.dp.y*cone_scale,
                      1.0 - s.radius*cone_scale);  // new ray direction
        Rd.normalize();
        Rd.orientAroundNormal(N, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) < 0.0)
            continue; // skip sample rays that self-intersect

        // Build new occlusion ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 mindist,
                                 maxdist,
                                 Fsr::RayContext::DIFFUSE | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Traceable::SurfaceIntersection Iocl(std::numeric_limits<double>::infinity());
        if (stx.rtx->objects_bvh.getFirstIntersection(stx_new, Iocl) > Fsr::RAY_INTERSECT_NONE)
        {
            // Diffuse occlusion reduces the visibility weight by the hit distance:
            float vis = (occlusion_ray_type == Fsr::RayContext::DIFFUSE) ?
                            float(1.0 / ((Iocl.t*::fabs(gi_scale)) + 1.0)) : 1.0f;

            if (Iocl.object)
            {
                zpr::RenderPrimitive* rprim = static_cast<zpr::RenderPrimitive*>(Iocl.object);

                // Only check visibility if the rprim's shader is a RayShader:
                if (rprim->surface_ctx->surface_shader)
                {
                    switch (occlusion_ray_type)
                    {
                    default:
                    case Fsr::RayContext::DIFFUSE:
                        if (!rprim->surface_ctx->surface_shader->diffuse_visibility())
                            vis = 0.0f;
                        break;
                    case Fsr::RayContext::REFLECTION:
                    case Fsr::RayContext::GLOSSY:
                        if (!rprim->surface_ctx->surface_shader->specular_visibility())
                            vis = 0.0f;
                        break;
                    case Fsr::RayContext::TRANSMISSION:
                        if (!rprim->surface_ctx->surface_shader->transmission_visibility())
                            vis = 0.0f;
                        break;
                    }

                }
            }

            weight += vis;
        }

    }

    if (weight <= 0.0f)
        return 0.0f; // no occlusion

    return clamp(weight / float(nSamples)); // partially exposed
}


} // namespace zpr

// end of zprender/RayShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
