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

/// @file zprender/RenderContext.cpp
///
/// @author Jonathan Egstad


#include "RenderContext.h"
#include "Scene.h"
#include "SurfaceHandler.h"
#include "ThreadContext.h"
#include "RayShader.h"
#include "LightShader.h"
#include "SurfaceShaderOp.h"
#include "ConeVolume.h"   // for getConeBBox
#include "SphereVolume.h" // for getSphereBBox

#include <Fuser/Primitive.h>  // for FUSER_NODE_PRIMITIVE_TYPE, FUSER_MESH_PRIMITIVE_TYPE, etc
#include <Fuser/NukeGeoInterface.h> // for getObjectString()


#include <DDImage/Iop.h>
#include <DDImage/Material.h> // for ColoredShader
#include <DDImage/Mesh.h>
#include <DDImage/Point.h>
#include <DDImage/Triangle.h>
#include <DDImage/Polygon.h>
#include <DDImage/Particles.h>
#include <DDImage/ParticlesSprite.h>
#include <DDImage/PolyMesh.h>


// Uncomment this to get some info from object expansion.
//#define DEBUG_OBJECT_EXPANSION 1


namespace zpr {


static DD::Image::Lock expand_lock;

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


// Force the template specialized function symbols to be included in the static library.
// Without this you may get symbol errors:
static Bvh<ObjectContext*>  bvh_otx;
static Bvh<SurfaceContext*> bvh_stx;

//! Used in Bvh and other places that return a const Fsr::Box3<T>&
/*extern*/ Fsr::Box3f empty_box3f;
/*extern*/ Fsr::Box3d empty_box3d;


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


/*! Set up the standard views for a DreamWorks stereo feature.
*/
DWAStereoViews::DWAStereoViews()
{
    views_map[std::string("center")] = std::string("CTR");
    views_map[std::string("mono"  )] = std::string("CTR");
    views_map[std::string("left"  )] = std::string("LFT");
    views_map[std::string("right" )] = std::string("RGT");
}

/*static*/ const std::map<std::string, std::string>&
DWAStereoViews::viewsMap()
{
    static DWAStereoViews dwa_stereo;
    return dwa_stereo.views_map;
}


/*static*/ const char* GenerateRenderPrimsContext::name = "GenerateRenderPrims";


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/*!
*/
static void
aov_handler_null(const RayShaderContext& stx,
                 const AOVLayer&         aov,
                 Fsr::Pixel&             out)
{
    // Copy black into outputs:
    const size_t nChans = aov.channel.size();
    for (size_t i=0; i < nChans; ++i)
        out[aov.channel[i]] = 0.0f;
}

/*!
*/
inline void
copyAttribf(const float*      src,
            uint32_t          max_chans,
            const AOVLayer&   aov,
            Fsr::Pixel&       out)
{
    const uint32_t nOutChans = (uint32_t)aov.channel.size();
    const uint32_t nChans    = std::min(max_chans, nOutChans);
    uint32_t i = 0;
    for (; i < nChans; ++i)
        out[aov.channel[i]] = *src++;
    for (; i < nOutChans; ++i)
        out[aov.channel[i]] = 0.0f; // fill rest of chans with zeros
}
inline void
copyAttribd(const double*     src,
            uint32_t          max_chans,
            const AOVLayer&   aov,
            Fsr::Pixel&       out)
{
    const uint32_t nOutChans = (uint32_t)aov.channel.size();
    const uint32_t nChans    = std::min(max_chans, nOutChans);
    uint32_t i = 0;
    for (; i < nChans; ++i)
        out[aov.channel[i]] = float(*src++);
    for (; i < nOutChans; ++i)
        out[aov.channel[i]] = 0.0f; // fill rest of chans with zeros
}

//----------------------------------------


/*! Generic attribute handler.
    TODO: implement or deprecate!
*/
static void
aov_handler_attribute(const RayShaderContext& stx,
                      const AOVLayer&         aov,
                      Fsr::Pixel&             out)
{
#if 1
    // do nothing for now
    aov_handler_null(stx, aov, out);

#else
    // Extract attribute from primitive:

    // Interpolate vertex attribs from parent primitive:
#if 1
    // Always take the attribute data from frame0 GeoInfo:
    const GeoInfoContext::Sample& gtx = ((GeoInfoContext*)surface_ctx->otx)->geoinfo0;
#else
    const GeoInfoContext::Sample& gtx = ((GeoInfoContext*)surface_ctx->otx)->motion_geoinfos[0];
#endif
    const Primitive* prim = gtx.info->primitive_array()[surface_ctx->prim_index()];

    VArray vP[3];
    uint32_t attrib_indices[3][Group_Last];
    for (uint32_t i=0; i < 3; ++i)
    {
        memcpy(attrib_indices[i], surface_ctx->attrib_indices, sizeof(uint32_t)*Group_Last);
        prim->build_index_array(attrib_indices[i], surface_ctx->prim_index(), vert[i]);
    }

    // Get UV:
    if (gtx.info->UV_ref)
    {
        for (uint32_t i=0; i < 3; ++i)
            gtx.info->UV_ref->copy_to_channels(attrib_indices[i], vP[i]);
        interpolateVector4(vP[0].UV(),  vP[1].UV(),  vP[2].UV(), st, v.UV());
    }
    else
    {
        v.UV().set(0,0,0,1);
    }
#endif
}


//----------------------------------------
static void aov_handler_Z(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) {
    if (aov.channel.size() == 0)
        return; // just in case...
    out[aov.channel[0]] = float(stx.distance);
}
static void aov_handler_Zl(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) {
    aov_handler_null(stx, aov, out);
}

//----------------------------------------

static void aov_handler_PW(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.PW.array(),    3, aov, out); }
static void aov_handler_dPWdx(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dPWdx.array(), 3, aov, out); }
static void aov_handler_dPWdy(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dPWdy.array(), 3, aov, out); }
static void aov_handler_PWg(  const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.PWg.array(),   3, aov, out); }
static void aov_handler_PL(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    if (!stx.w2l) {
        copyAttribd(stx.PW.array(), 3, aov, out);
    } else {
        const Fsr::Vec3d PL = stx.w2l->transform(stx.PW);
        copyAttribd(PL.array(), 3, aov, out);
    }
}
//
static void aov_handler_N(    const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.N.array(),     3, aov, out); }
static void aov_handler_Nf(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Nf.array(),    3, aov, out); }
static void aov_handler_Ng(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Ng.array(),    3, aov, out); }
static void aov_handler_Ngf(  const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Ngf.array(),   3, aov, out); }
static void aov_handler_Ns(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Ns.array(),    3, aov, out); }
static void aov_handler_dNsdx(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dNsdx.array(), 3, aov, out); }
static void aov_handler_dNsdy(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dNsdy.array(), 3, aov, out); }
//
static void aov_handler_st(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.st.array(), 2, aov, out); }
static void aov_handler_dstdx(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const Fsr::Vec3f d(stx.Rxst - stx.st);
    copyAttribf(d.array(), 2, aov, out);
}
static void aov_handler_dstdy(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const Fsr::Vec3f d(stx.Ryst - stx.st);
    copyAttribf(d.array(), 2, aov, out);
}
//
static void aov_handler_UV(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.UV.array(),    2, aov, out); }
static void aov_handler_dUVdx(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.dUVdx.array(), 2, aov, out); }
static void aov_handler_dUVdy(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.dUVdy.array(), 2, aov, out); }
//
static void aov_handler_Cf(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.Cf.array(),    4, aov, out); }
static void aov_handler_dCfdx(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.dCfdx.array(), 4, aov, out); }
static void aov_handler_dCfdy(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribf(stx.dCfdy.array(), 4, aov, out); }
//
static void aov_handler_time(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const float v = float(stx.frame_time);
    for (uint32_t i=0; i <aov.channel.size(); ++i)
        out[aov.channel[i]] = v;
}
static void aov_handler_dtdx(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    aov_handler_null(stx, aov, out);
}
static void aov_handler_dtdy(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    aov_handler_null(stx, aov, out);
}
//
static void aov_handler_surf_id(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    aov_handler_null(stx, aov, out);
}
//
static void aov_handler_V(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const Fsr::Vec3d V = -stx.Rtx.dir();
    copyAttribd(V.array(), 3, aov, out);
}
static void aov_handler_VdotN(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const float VdotN = float(stx.N.dot(stx.getViewVector()));
    copyAttribf(&VdotN, 1, aov, out);
}
static void aov_handler_VdotNg(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const float VdotNg = float(stx.Ng.dot(stx.getViewVector()));
    copyAttribf(&VdotNg, 1, aov, out);
}
static void aov_handler_VdotNf(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out)
{
    const float VdotNf = float(stx.Nf.dot(stx.getViewVector()));
    copyAttribf(&VdotNf, 1, aov, out);
}

/*!
*/
static void
assign_aov_handlers(AOVBuiltIn* handlers)
{
    handlers[AOV_ATTRIBUTE].set("",                aov_handler_attribute);
    //
    handlers[AOV_Z        ].set("z",               aov_handler_Z        );
    handlers[AOV_Zl       ].set("zl",              aov_handler_Zl       );
    //              //
    handlers[AOV_PW       ].set("pw,p",            aov_handler_PW       );
    handlers[AOV_dPWdx    ].set("dpwdx,dpdx",      aov_handler_dPWdx    );
    handlers[AOV_dPWdy    ].set("dpwdy,dpdy",      aov_handler_dPWdy    );
    handlers[AOV_PL       ].set("pl",              aov_handler_PL       );
    handlers[AOV_PWg      ].set("pwg",             aov_handler_PWg      );
    //              //
    handlers[AOV_st       ].set("st",              aov_handler_st       );
    handlers[AOV_dstdx    ].set("dstdx",           aov_handler_dstdx    );
    handlers[AOV_dstdy    ].set("dstdy",           aov_handler_dstdy    );
    //              //
    handlers[AOV_N        ].set("n",               aov_handler_N        );
    handlers[AOV_Nf       ].set("nf",              aov_handler_Nf       );
    handlers[AOV_Ng       ].set("ng",              aov_handler_Ng       );
    handlers[AOV_Ngf      ].set("ngf",             aov_handler_Ngf      );
    handlers[AOV_Ns       ].set("ns",              aov_handler_Ns       );
    handlers[AOV_dNsdx    ].set("dnsdx",           aov_handler_dNsdx    );
    handlers[AOV_dNsdy    ].set("dnsdy",           aov_handler_dNsdy    );
    //              //
    handlers[AOV_UV       ].set("uv",              aov_handler_UV       );
    handlers[AOV_dUVdx    ].set("duvdx",           aov_handler_dUVdx    );
    handlers[AOV_dUVdy    ].set("duvdy",           aov_handler_dUVdy    );
    //              //
    handlers[AOV_Cf       ].set("cf",              aov_handler_Cf       );
    handlers[AOV_dCfdx    ].set("dcfdx",           aov_handler_dCfdx    );
    handlers[AOV_dCfdy    ].set("dcfdy",           aov_handler_dCfdy    );
    //              //
    handlers[AOV_time     ].set("time,t",          aov_handler_time     );
    handlers[AOV_dtdx     ].set("dtdx",            aov_handler_dtdx     );
    handlers[AOV_dtdy     ].set("dtdy",            aov_handler_dtdy     );
    //              //
    handlers[AOV_surf_id  ].set("surf_id,id",      aov_handler_surf_id  );
    //
    handlers[AOV_V        ].set("v",               aov_handler_V        );
    handlers[AOV_VdotN    ].set("vdotn",           aov_handler_VdotN    );
    handlers[AOV_VdotNg   ].set("vdotng",          aov_handler_VdotNg   );
    handlers[AOV_VdotNf   ].set("vdotnf",          aov_handler_VdotNf   );
}


/*static*/ const char*
AOVLayer::aov_merge_modes[] =
{
    "premult-under",
    "under",
    "plus",
    "min",
    "mid",
    "max",
    0
};


/*!
*/
bool
AOVLayer::build(AOVBuiltIn*         built_ins,
                const char*         _name,
                int                 nChans,
                DD::Image::Channel* chans)
{
    //std::cout << "AOVLayer::build('" << _name << "') nChans=" << nChans << std::endl;
    enabled = false;
    if (!_name || !_name[0])
        return false;

    // See if this is one of the predefined types:
    type = AOV_ATTRIBUTE;
    std::string s(_name);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower); 
    for (uint32_t i=0; i < AOV_LAST_TYPE; ++i)
    {
        const AOVBuiltIn& aov = built_ins[i];
        if (!aov.tokens || !aov.tokens[0])
            continue;

        std::vector<std::string> tokens;
        Fsr::stringSplit(std::string(aov.tokens), ",/"/*delimiters*/, tokens);
        for (uint32_t t=0; t < tokens.size(); ++t)
        {
            if (tokens[t] == s)
            {
                type    = (AOVType)i;
                handler = aov.handler;
                break;
            }
        }
    }
    //std::cout << "'" << _name << "' matched, type=" << (uint32_t)type << " handler=" << (void*)handler;

    name = _name;
    channel.clear();
    channel.reserve(nChans);

    mask = DD::Image::Mask_None;
    int count = 0;
    //std::cout << " channels=";
    for (int i=0; i < nChans; ++i)
    {
        const DD::Image::Channel& chan = chans[i];
        channel.push_back(chan);
        if (chan > DD::Image::Chan_Black)
        {
            mask += chan;
            //std::cout << (uint32_t)chan << " ";
            ++count;
        }
    }
    //std::cout << std::endl;
    if (count > 0)
       enabled = true;
    unpremult = AOV_UNPREMULT_BY_COVERAGE;

    return enabled;
}


/*! Find an AOVLayer by name.
*/
const AOVLayer*
RenderContext::findAOVLayer(const char* name) const
{
    if (!name || !name[0])
        return NULL;

    std::map<std::string, uint32_t>::const_iterator it = aov_map.find(std::string(name));
    if (it != aov_map.end())
        return &aov_outputs[it->second];
    return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/*!
*/
ObjectContext::ObjectContext() :
    status(SURFACE_NOT_DICED)
{
    gettimeofday(&last_access, 0);
}

/*!
*/
ObjectContext::ObjectContext(zpr::Scene* scene,
                             uint32_t    index) :
    status(SURFACE_NOT_DICED)
{
    gettimeofday(&last_access, 0);
#if DEBUG
    assert(scene);
#endif
    motion_objects.resize(1);
    motion_objects[0].scene = scene;
    motion_objects[0].index = index;
    motion_times.resize(1, scene->frame);
}


/*!
*/
/*virtual*/
ObjectContext::~ObjectContext()
{
    for (size_t i=0; i < prim_list.size(); ++i)
        delete prim_list[i];
}


//!
GeoInfoContext::Sample&
GeoInfoContext::addGeoInfoSample(zpr::Scene* scene,
                                 uint32_t    obj_index)
{
#if DEBUG
    assert(scene);
#endif

    motion_objects.push_back(ObjectSample(scene, obj_index));
    motion_times.push_back(scene->frame);

#if DEBUG
    assert(obj_index < scene->objects());
#endif
    DD::Image::GeoInfo& info = scene->object(obj_index);

    motion_geoinfos.push_back(Sample());
    Sample& gtx = motion_geoinfos[motion_geoinfos.size()-1];
    gtx.info                    = &info;
    gtx.transforms              = scene->object_transforms(obj_index); // TODO: deprecate
    gtx.l2w = info.matrix;
    gtx.w2l = gtx.l2w.inverse();
    gtx.xform_is_identity = gtx.l2w.isIdentity();

    return gtx;
}


//!
LightVolumeContext::Sample&
LightVolumeContext::addLightVolumeSample(zpr::Scene* scene,
                                         uint32_t    lt_index)
{
#if DEBUG
    assert(scene);
#endif

    motion_objects.push_back(ObjectSample(scene, lt_index));
    motion_times.push_back(scene->frame);

#if DEBUG
    assert(lt_index < scene->lights.size());
    assert(lt_index < scene->light_transforms.size());
#endif
    DD::Image::LightContext* lt_ctx    = scene->lights[lt_index];
    DD::Image::MatrixArray&  lt_xforms = scene->light_transforms[lt_index];

    motion_lights.push_back(Sample());
    Sample& lvtx = motion_lights[motion_lights.size()-1];
    lvtx.lt_ctx = lt_ctx;
    lvtx.l2w    = lt_xforms.matrix(LOCAL_TO_WORLD);
    lvtx.w2l    = lvtx.l2w.inverse();
    lvtx.xform_is_identity = lvtx.l2w.isIdentity();

    return lvtx;
}


/*!
*/
GeoInfoContext*
SurfaceContext::getGeoInfoContext()
{
#if DEBUG
    assert(parent_object_ctx);
#endif
    return parent_object_ctx->asGeoObject();
}

/*!
*/
LightVolumeContext*
SurfaceContext::getLightVolumeContext()
{
#if DEBUG
    assert(parent_object_ctx);
#endif
    return parent_object_ctx->asLightVolume();
}



/*! Return the zpr::Scene* from the parent ObjectContext.
*/
zpr::Scene*
SurfaceContext::getScene(uint32_t sample)
{
#if DEBUG
    assert(parent_object_ctx);
    assert(sample < parent_object_ctx->motion_objects.size());
#endif
    return parent_object_ctx->motion_objects[sample].scene;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/*static*/ const char* RenderContext::camera_modes[]       = { "stereo-separate", "stereo-combined", 0 };
/*static*/ const char* RenderContext::shading_interpolation_names[] = { "off", "constant", "smooth", 0 };
/*static*/ const char* RenderContext::sampling_modes[]     = { "1", "2", "3", "4", "5", "8", "12", "16","32","64", 0 };
/*static*/ const char* RenderContext::output_bbox_modes[]  = { "scene", "format", 0 };
/*static*/ const char* RenderContext::sides_modes[]        = { "front", "back", "both", 0 };
/*static*/ const char* RenderContext::debug_names[]        = { "off", "low", "medium", "high", 0 };
/*static*/ const char* RenderContext::diagnostics_modes[]  = { "off", "time", "bounds", "bvh-leafs", "intersections", "volumes", "patches", "render-time", 0 };


static NullSurfaceHandler null_surface_handler;


//-----------------------------------------------------------------------------


/*!
*/
RenderContext::RenderContext()
{
    //----------------------------------------------
    // Driven by knob controls:
    k_shutter_bias              = 0.0;
    k_camera_mode               = CAMERA_COMBINED;
    k_projection_mode           = DD::Image::CameraOp::LENS_PERSPECTIVE;
    k_shading_interpolation     = SHADING_SMOOTH;
    k_sides_mode                = SIDES_FRONT;
    k_preview_mode              = false;
    k_debug                     = DEBUG_NONE;
    k_pixel_filter              = DD::Image::Filter::Cubic;
    k_pixel_filter_size[0]      = k_pixel_filter_size[1] = 1.0f;
    k_spatial_jitter_threshold  = 1;
    k_output_bbox_mode          = BBOX_SCENE_SIZE;

    k_direct_lighting_enabled    = true;
    k_indirect_lighting_enabled  = true;
    k_atmospherics_enabled       = false;
    k_atmosphere_alpha_blending  = true;
    k_transparency_enabled       = true;

    k_alpha_threshold           = 0.001f;
    k_dof_enabled               = false;
    k_dof_max_radius            = 0.1f;

    //----------------------------------------------
    // Derived or set by render environment:
    render_version              = 0;
    render_frame                = 0.0;
    frame0                      = 0.0;
    render_view                 = 1;
    render_view_name            = "main";
    scene_channels              = DD::Image::Mask_None;
    render_format               = 0;
    render_channels             = DD::Image::Mask_None;
    color_channels              = DD::Image::Mask_None;
    vector_channels             = DD::Image::Mask_None;

    num_shutter_steps           = 0; // no motion blur
    frame0_shutter_sample       = 0;
    shutter_open_offset         = 0.0f;
    shutter_close_offset        = 0.0f;
    shutter_length              = 0.0f;

    Near                        = 0.01;
    Far                         = 100000.0;
    pixel_filter                = DD::Image::Filter::Cubic;

    ray_max_depth               = 10;
    ray_diffuse_max_depth       = 1;
    ray_glossy_max_depth        = 1;
    ray_reflection_max_depth    = 1;
    ray_refraction_max_depth    = 1;

    ray_single_scatter_samples  = 5;
    ray_diffuse_samples         = 2;
    ray_glossy_samples          = 2;
    ray_refraction_samples      = 2;

    hash.reset();
    objects_initialized         = false;

    bvh_max_depth               = 256;
    bvh_max_objects             = 25;

    global_xform.setToIdentity();
    global_offset.set(0.0, 0.0, 0.0);

    //----------------------------------------------
    // Default primitive handlers to null handlers:
    for (int i=0; i < LAST_SOURCE_PRIM_TYPE; ++i)
        surface_handler[i] = &null_surface_handler;

    //----------------------------------------------
    // Assign aov handlers:
    for (int i=0; i < AOV_LAST_TYPE; ++i)
        aov_handler[i].set(0/*type*/, &aov_handler_null);
    assign_aov_handlers(aov_handler);
}


/*!
*/
/*virtual*/
RenderContext::~RenderContext()
{
    // Delete object contexts & bvhs:
    destroyAllocations(true/*force*/);
}


//-------------------------------------------------------------------------


/*! Delete all context allocations.
*/
/*virtual*/
void
RenderContext::destroyAllocations(bool force)
{
    //std::cout << "RenderContext::destroyAllocations()" << std::endl;
    for (uint32_t i=0; i < thread_list.size(); ++i)
        delete thread_list[i];
    thread_list.clear();
    thread_map.clear();

    //-----

    for (uint32_t i=0; i < ray_cameras.size(); ++i)
        delete ray_cameras[i];
    ray_cameras.clear();
    for (uint32_t i=0; i < hero_ray_cameras.size(); ++i)
        delete hero_ray_cameras[i];
    hero_ray_cameras.clear();

    //-----

    for (size_t i=0; i < master_light_shaders.size(); ++i)
        delete master_light_shaders[i];
    master_light_shaders.clear();

    for (size_t j=0; j < per_object_light_shaders.size(); ++j)
    {
        LightShaderList& lshaders = per_object_light_shaders[j];
        for (size_t i=0; i < lshaders.size(); ++i)
            delete lshaders[i];
        lshaders.clear();
    }
    per_object_light_shaders.clear();

    //-----

    // TODO: support hash testing before deleting all objects!
    for (uint32_t i=0; i < object_context.size(); ++i)
        delete object_context[i];
    object_context.clear();
    object_map.clear();
    objects_bvh.clear();

    for (uint32_t i=0; i < light_context.size(); ++i)
        delete light_context[i];
    light_context.clear();
    light_map.clear();
    lights_bvh.clear();

}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! TODO: finish implementing this
*/
void
RenderContext::buildLightShaders()
{

    // Get the light count and motion times:
    size_t nLights = 0;
    const uint32_t nScenes = (uint32_t)shutter_scenerefs.size();
    Fsr::DoubleList motion_times(nScenes);
    for (uint32_t j=0; j < nScenes; ++j)
    {
        const ShutterSceneRef& sref = shutter_scenerefs[j];
        zpr::Scene* scene = input_scenes[sref.op_input_index];
#if DEBUG
        assert(scene);
#endif
        if (j==0)
            nLights = scene->lights.size();
        else
            // verify that the lights don't change in count!!!
            assert(scene->lights.size() == nLights);

        motion_times[j] = sref.frame;
    }
    if (nLights == 0)
        return;

//std::cout << "zpr::buildLightShaders():" << std::endl;
    zpr::Scene* scene0 = input_scenes[shutter_scenerefs[0].op_input_index];
    Fsr::Mat4dList motion_xforms; motion_xforms.reserve(nScenes);
    for (size_t ltindex=0; ltindex < nLights; ++ltindex)
    {
        DD::Image::LightContext* ltx0 = scene0->lights[ltindex];
#if DEBUG
        assert(ltx0);
#endif
        DD::Image::LightOp* light0 = ltx0->light();
#if DEBUG
        assert(light0);
#endif
        if (light0->node_disabled())
            continue;

//std::cout << "  " << ltindex << ":" << std::endl;
        // Get the light's xforms:
        motion_xforms.clear();
        motion_xforms.push_back(Fsr::Mat4d(light0->matrix()));
//std::cout << "    0 xform" << motion_xforms[motion_xforms.size()-1] << std::endl;
        for (uint32_t j=1; j < nScenes; ++j)
        {
            zpr::Scene* scene1 = input_scenes[shutter_scenerefs[j].op_input_index];

            DD::Image::LightContext* ltx1 = scene1->lights[ltindex];
#if DEBUG
            assert(ltx1);
#endif
            DD::Image::LightOp* light1 = ltx1->light();
#if DEBUG
            assert(light1);
#endif

            motion_xforms.push_back(Fsr::Mat4d(light1->matrix()));
//std::cout << "    " << j << " xform" << motion_xforms[motion_xforms.size()-1] << std::endl;
        }

        // Create the LightShaders:
        LightShader* lshader = new LightShader(motion_times, motion_xforms);

        master_light_shaders.push_back(lshader);
//std::cout << "  " << ltindex << ": lshader=" << lshader << std::endl;
    }



    //per_object_light_shaders.resize();

#if 0
    //std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox():" << std::endl;
    bbox.clear();
    // Skip it if it's off:
    if (!light || light->node_disabled())
        return UNRECOGNIZED_PRIM;

    // Only create prim if light can illuminate atmosphere:
    DD::Image::Knob* k_light_illum = light->knob("illuminate_atmosphere");
    if (!k_light_illum)
        return UNRECOGNIZED_PRIM;
    bool can_illuminate_atmosphere = false;
    DD::Image::Hash junk;
    k_light_illum->store(DD::Image::BoolPtr, &can_illuminate_atmosphere, junk, light->outputContext());
    if (!can_illuminate_atmosphere)
        return UNRECOGNIZED_PRIM;

    //double near = clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity());
    //double far  = clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity());
    const Fsr::Mat4d light_xform(light->matrix());

    /* In DDImage LightOp.h:
        enum LightType
        { 
            ePointLight, 
            eDirectionalLight, 
            eSpotLight, 
            eOtherLight 
        };
    */

    // Check for recognized light types:
    if (light->lightType() == DD::Image::LightOp::eSpotLight)
    {
        // Cone:
        bbox = ConeVolume::getConeBbox(clamp(light->hfov(), 0.0001, 180.0),
                                       clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                       clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()),
                                       light_xform);
        //std::cout << " type=LIGHTCONE_PRIM, bbox" << bbox << std::endl;
        return LIGHTCONE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::ePointLight)
    {
        // LightSphere
        bbox = SphereVolume::getSphereBbox(clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                           clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()),
                                           light_xform);
        //std::cout << " type=LIGHTSPHERE_PRIM, bbox" << bbox << std::endl;
        return LIGHTSPHERE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::eDirectionalLight)
    {
        // LightCylinder

        //std::cout << " type=LIGHTCYLINDER_PRIM, bbox" << bbox << std::endl;
        return LIGHTCYLINDER_PRIM;

    }
    else
    {
        // Check for ReflectionCard:
        if (strcmp(light->Class(), "ReflectionCard")==0 ||
            strcmp(light->Class(), "AreaLight")==0)
        {
            // LightCard

            //std::cout << " type=LIGHTCARD_PRIM, bbox" << bbox << std::endl;
            return  LIGHTCARD_PRIM; 
        }
    }

    //std::cout << " UNRECOGNIZED TYPE" << std::endl;
    //std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox(): warning, unknown light type, skipping..." << std::endl;
#endif


}


/*! Destroy all the curently assigned lighting scenes.
*/
void
ThreadContext::clearLightingScenes()
{
    m_master_lighting_scene.delete_light_context();
    m_master_lighting_scene.light_transforms.clear();
    m_master_lighting_scene.light_renderers.clear();

    for (size_t i=0; i < m_per_object_lighting_scenes.size(); ++i)
        delete m_per_object_lighting_scenes[i];
    m_per_object_lighting_scenes.clear();
}


/*! This interpolates position and rotation - but the rotation is only valid
    within a certain range since it's a linear interpolation of the xyz normals.
*/
inline void
interpolateDDImageAxis(const DD::Image::Axis& a0,
                       const DD::Image::Axis& a1,
                       float                  t,
                       DD::Image::Axis&       out)
{
    //std::cout << "a0[" << a0.z.x << " " << a0.z.y << " " << a0.z.z << "]" << std::endl;
    //std::cout << "a1[" << a1.z.x << " " << a1.z.y << " " << a1.z.z << "]" << std::endl;
    const float inv_t = 1.0f - t;
    //
    out.p = a0.p*inv_t + a1.p*t;

    out.x = a0.x*inv_t + a1.x*t; out.x.normalize();
    out.y = a0.y*inv_t + a1.y*t; out.y.normalize();
    out.z = a0.z*inv_t + a1.z*t; out.z.normalize();
    //std::cout << "out[" << out.z.x << " " << out.z.y << " " << out.z.z << "]" << std::endl;
}


/*!
*/
void
RenderContext::updateLightingSceneVectorsTo(uint32_t    shutter_step,
                                            float       shutter_step_t,
                                            zpr::Scene* light_scene) const
{
#if DEBUG
    assert(shutter_step+1 < shutter_scenerefs.size());
#endif
    zpr::Scene* scene0 = input_scenes[shutter_scenerefs[shutter_step  ].op_input_index];
    zpr::Scene* scene1 = input_scenes[shutter_scenerefs[shutter_step+1].op_input_index];

    const size_t nLights = light_scene->lights.size();
#if DEBUG
    assert(nLights == scene0->lights.size());
    assert(nLights == scene1->lights.size());
#endif
    for (size_t i=0; i < nLights; ++i)
    {
        DD::Image::LightContext* ltx0 = scene0->lights[i];
        DD::Image::LightContext* ltx1 = scene1->lights[i];
        DD::Image::LightContext* ltx_interp = light_scene->lights[i];
#if DEBUG
        assert(ltx0);
        assert(ltx1);
        assert(ltx_interp);
#endif
        // Shift LightContext Axis in time:
        interpolateDDImageAxis(ltx0->vectors(),
                               ltx1->vectors(),
                               shutter_step_t,
                               const_cast<DD::Image::Axis&>(ltx_interp->vectors()));
    }
}


/*!
*/
void
RenderContext::updateLightingScene(const zpr::Scene* ref_scene,
                                   zpr::Scene*       lighting_scene) const
{
    // Update the lighting scene's camera vectors:
    DD::Image::Matrix4 cm = ref_scene->camera->matrix();

    // Copy the camera vectors:
    lighting_scene->cam_vectors.p = cm.translation(); // << this is really the most important one
    lighting_scene->cam_vectors.x = cm.x_axis(); // but we'll do the others for completeness:
    lighting_scene->cam_vectors.y = cm.y_axis();
    lighting_scene->cam_vectors.z = cm.z_axis();
    lighting_scene->cam_vectors.x.normalize();
    lighting_scene->cam_vectors.y.normalize();
    lighting_scene->cam_vectors.z.normalize();
    //std::cout << " camera.p[" << lighting_scene->cam_vectors.p.x << " " << lighting_scene->cam_vectors.p.y << " " << lighting_scene->cam_vectors.p.z << "]" << std::endl;
}


/*! Per-pixel motionblurred lighting in Nuke's shading system requires a thread-safe
    local copy of a Scene structure that contains the list of LightContext pointers
    that the shaders use to light with.

    Because we're changing the LightContext's Axis vectors every subpixel as time
    changes we need to pass a dummy Scene up the shading tree with modified
    LightContext pointers.

    To handle per-object light filters we store a lighting scene per-object in a list
    correspoding to the object ObjectContext index.  Each scene contains a subset of
    the master lighting scene.
*/
void
RenderContext::updateLightingScenes(const zpr::Scene* ref_scene,
                                    ThreadContext&    ttx) const
{
#if DEBUG
    assert(ref_scene);
#endif

    // Build the master lighting scene:
    zpr::Scene& master_lighting_scene = ttx.masterLightingScene();
    master_lighting_scene.copyInfo(ref_scene);
    //master_lighting_scene.object_list_.clear();
    //master_lighting_scene.object_transforms_.clear();

    // Clear all light info initially:
    ttx.clearLightingScenes();

    master_lighting_scene.transparency(true);

    // Build the per-object lights first:
    const uint32_t nObjects = (uint32_t)object_context.size();

    LightingSceneList& per_object_lighting_scenes = ttx.perObjectLightingSceneList();
    per_object_lighting_scenes.reserve(nObjects);
    for (uint32_t i=0; i < nObjects; ++i)
    {
        GeoInfoContext* otx = object_context[i];
        //
        per_object_lighting_scenes.push_back(new zpr::Scene());
        zpr::Scene* lscene = per_object_lighting_scenes[i];
        //
        // Copy from reference scene:
        lscene->copyInfo(ref_scene);
        // Clear all lights initially:
        lscene->delete_light_context();
        lscene->light_transforms.clear();
        lscene->light_renderers.clear();
        lscene->transparency(true);
        //
        // Get the list of enabled lights from the object context:
        lscene->lights.reserve(otx->enabled_lights.size());
        lscene->light_transforms.reserve(otx->enabled_lights.size());
        lscene->light_renderers.reserve(otx->enabled_lights.size());

        for (std::set<uint32_t>::const_iterator it=otx->enabled_lights.begin(); it != otx->enabled_lights.end(); ++it)
        {
            DD::Image::LightContext* ltx = ref_scene->lights[*it];
#if DEBUG
            assert(ltx); // Shouldn't happen...
#endif
            //std::cout << "  enabled light '" << ltx->light()->node_name() << "'" << std::endl;
            lscene->lights.push_back(ltx->clone());
            lscene->light_transforms.push_back(ref_scene->light_transforms[*it]);
            lscene->light_renderers.push_back(NULL);
        }
    }

    if (k_direct_lighting_enabled)
    {
        // Copy the light context list out of the scene to make a
        // thread-safe local version.  We'll update these LightContexts
        // at each subpixel with interpolated light vectors:
        const uint32_t nLights = (uint32_t)ref_scene->lights.size();
        master_lighting_scene.lights.reserve(nLights);
        master_lighting_scene.light_transforms.reserve(nLights);
        master_lighting_scene.light_renderers.reserve(nLights);

        for (uint32_t i=0; i < nLights; ++i)
        {
            master_lighting_scene.lights.push_back(ref_scene->lights[i]->clone());
            master_lighting_scene.light_transforms.push_back(ref_scene->light_transforms[i]);
            master_lighting_scene.light_renderers.push_back(NULL);
            //========================================================================
            // TODO: enable this code...?:
            // enable light output?
            // if ( ref_scene->lights[i]->light()->getShadowMaskChannel() != Mask_None)
            //    shader_channels += ref_scene->lights[i]->light()->getShadowMaskChannel();
            //========================================================================================
        }

        // We calculate the light Axis vectors when the Ray is initialized.
        updateLightingScene(ref_scene, &master_lighting_scene);
        for (uint32_t i=0; i < per_object_lighting_scenes.size(); ++i)
            updateLightingScene(ref_scene, per_object_lighting_scenes[i]);
    }
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Start a shader context list owned by thread_index and returning a reference to the first one.
*/
RayShaderContext&
RenderContext::startShaderContext(uint32_t thread_index)
{
#if DEBUG
    assert(thread_index < thread_list.size());
#endif

    ThreadContext* rttx = thread_list[thread_index];
#if DEBUG
    assert(rttx);
#endif

    // Add the new context:
    const uint32_t stx_index = rttx->pushShaderContext(0/*current*/);

    return rttx->getShaderContext(stx_index);
}

/*! Add a shader context to the end of the list, copying the current context, and returning a reference to it.
*/
RayShaderContext&
RenderContext::pushShaderContext(uint32_t               thread_index,
                                 const RayShaderContext* current)
{
#if DEBUG
    assert(current); // must have an existing context to copy
    assert(thread_index < thread_list.size());
#endif
    ThreadContext* rttx = thread_list[thread_index];
#if DEBUG
    assert(rttx);
#endif

    // Add the new context:
    const uint32_t stx_index = rttx->pushShaderContext(current);
    //std::cout << "Context::push_shader_context(current=" << current << "): stx_index=" << stx_index << " new stx=" << &rttx->get_shader_context(stx_index) << std::endl;

    return rttx->getShaderContext(stx_index);
}

/*! Add a shader context to the end of the list, copying the
    current context, and returning a reference to it.
*/
RayShaderContext&
RenderContext::pushShaderContext(uint32_t                         thread_index,
                                 const RayShaderContext*          current,
                                 const Fsr::RayContext&           R,
                                 const Fsr::RayContext::TypeMask& ray_type,
                                 const Fsr::RayDifferentials*     Rdif)
{
#if DEBUG
    assert(current); // must have an existing context to copy
    assert(thread_index < thread_list.size());
#endif
    ThreadContext* rttx = thread_list[thread_index];
#if DEBUG
    assert(rttx);
#endif

    // Add the new context:
    const uint32_t stx_index = rttx->pushShaderContext(current);
    RayShaderContext& stx = rttx->getShaderContext(stx_index);

    stx.setRayContext(R, ray_type, Rdif);

    // Default to Context sides mode if camera ray, otherwise
    // uses sides both for any bounce:
    if (ray_type & Fsr::RayContext::CAMERA)
        stx.sides_mode = k_sides_mode;
    else
        stx.sides_mode = RenderContext::SIDES_BOTH;

    return stx;
}

/*! Remove a RayShaderContext from the end of the list, and return the new index.
*/
int
RenderContext::popShaderContext(uint32_t thread_index)
{
#if DEBUG
    assert(thread_index < thread_list.size());
#endif
    return thread_list[thread_index]->popShaderContext();
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Constructor requires an Context, thread ID and it's index in the thread list.
*/
ThreadContext::ThreadContext(RenderContext* rtx) :
    m_rtx(rtx),
    m_index(0),
    m_ID(0)
{
    // Reserve space for 10 ray bounces:
    m_stx_list.reserve(10);

    // Generous reserves for scratch-space lists:
    bvh_leafs.reserve(500);
    I_list.reserve(500);
    I_vol_list.reserve(500);
    index_list.reserve(500);
    vol_intersections.reserve(500);
    uv_intersections.reserve(500);

    surface_color.setInterestRatchet(&surfaceColorInterestRatchet);
    light_color.setInterestRatchet(&lightColorInterestRatchet);
    volume_color.setInterestRatchet(&volumeColorInterestRatchet);
}

/*!
*/
ThreadContext::~ThreadContext()
{
    clearLightingScenes();
}


/*! Add a RayShaderContext to the end of the list, and return its index.
*/
uint32_t
ThreadContext::pushShaderContext(const RayShaderContext* current)
{
#if DEBUG
    assert(m_rtx); // have to have a valid Context...
#endif

    // Add a new shader context:
    m_stx_list.push_back(RayShaderContext());
    const uint32_t stx_index = (uint32_t)m_stx_list.size()-1;
    RayShaderContext& stx = m_stx_list[stx_index];
    //std::cout << "ThreadContext::push_shader_context() - new stx_index=" << stx_index << std::endl;

    // Copy the current context first:
    if (current)
    {
        stx = *current;
        //memcpy(&stx, current, sizeof(RayShaderContext));

        // Update some of the info:
        stx.previous_stx = const_cast<RayShaderContext*>(current);

    }
    else
    {
        // Assign some initial params:
        if (stx_index == 0)
            stx.previous_stx = NULL; // No previous stx
        else
            stx.previous_stx = &m_stx_list[stx_index - 1];
        stx.rtx              = m_rtx;
        stx.thread_index     = m_index;
        stx.thread_ctx       = this;
        //
        stx.texture_filter   = NULL;//&texture_filter_; //TODO set this to a default!
        //
        stx.direct_lighting_enabled   = m_rtx->k_direct_lighting_enabled;
        stx.indirect_lighting_enabled = m_rtx->k_indirect_lighting_enabled;
        stx.master_lighting_scene     = NULL;
        //
        stx.sides_mode          = m_rtx->k_sides_mode; // Which sides to intersect against (SIDES_BOTH, SIDES_FRONT, SIDES_BACK)
        stx.index_of_refraction = -std::numeric_limits<double>::infinity(); // undefined
        //
        stx.sampler             = NULL; // Sampler to use
        //
        //stx.cutout_channel      = m_rtx->k_cutout_channel;
        //
        stx.show_debug_info     = false; // For debugging
    }

    // Reset intersection pointers:
    stx.rprim                 = NULL; // Current primitive being evaluated (intersected/shaded)
    stx.surface_shader        = NULL; // Current RayShader being evaluated (NULL if legacy material)
    stx.displacement_shader   = NULL; // Current RayShader being evaluated (NULL if legacy material)
    stx.atmosphere_shader     = NULL; // Current atmospheric VolumeShader being evaluated
    stx.material              = NULL; // Current material on primitive - legacy
    stx.displacement_material = NULL; // Current displacement material on primitive - legacy
    stx.w2l                   = NULL; // World-to-local matrix for current primitive (identity=NULL)
    stx.l2w                   = NULL; // Local-to-world matrix for current primitive (identity=NULL)

    return stx_index;
}


/*! Remove a RayShaderContext from the end of the list, and return the new index.
*/
int
ThreadContext::popShaderContext()
{
    m_stx_list.pop_back();
    return (int)m_stx_list.size() - 1;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Expand each object into surface context.

    This is a thread-safe call as each object has a status that's checked before the
    object is expanded.  If multiple threads share the same objects in its list they
    will have to wait until all the common objects are expanded before this method
    will return.

    Returns false on user-abort.
*/
bool
RenderContext::expandObject(ObjectContext* otx)
{
#if DEBUG
    assert(otx);  // shouldn't happen...
#endif
    //std::cout << "RenderContext::expandObjects()" << std::endl;
    if (otx->status == SURFACE_DICED)
        return true; // return fast if already done

    // TODO: switch this loop to a std::condition_variable mutex test!

    // Loop until all surfaces are expanded - this means possibly waiting for another
    // thread to finish expanding an object:
    while (1)
    {
        if (otx->status == SURFACE_DICED)
            return true;

        if (otx->status == SURFACE_NOT_DICED)
        {
            expand_lock.lock();
            // Check again to avoid a race condition:
            if (otx->status == SURFACE_NOT_DICED)
            {
                // Lock the object for us to change the status, and that will
                // keep the other threads from trying to build it:
                otx->status = SURFACE_DICING;
                otx->clearSurfacesAndRenderPrims();

                expand_lock.unlock();
#ifdef DEBUG_OBJECT_EXPANSION
                if (k_debug == RenderContext::DEBUG_LOW)
                {
                    std::cout << "-----------------------------------------------------------------------" << std::endl;
                    std::cout << "RenderContext::expandObjects(" << otx << ")" << std::endl;
                }
#endif

                if (!generateSurfaceContextsForObject(otx))
                {
                    std::cout << "  RenderContext::expandObject() aborted generateSurfaceContextsForObject()" << std::endl;
                    otx->status = SURFACE_NOT_DICED;
                    return false; // user-abort
                }

                if (!generateRenderPrimitivesForObject(otx))
                {
                    std::cout << "  RenderContext::expandObject() aborted generateRenderPrimitivesForObject()" << std::endl;
                    otx->status = SURFACE_NOT_DICED;
                    return false; // user-abort
                }

                // Indicate the object's been fully expanded:
                otx->status = SURFACE_DICED;

                return true; // all done!

            }
            else
            {
                // Another thread got to it before us, but we still have to wait until
                // it's done.
                expand_lock.unlock();
            }

        } // not expanded?

        // TODO: switch this to a real std::condition_variable mutex test!
        // Pause briefly then try again:
        DD::Image::sleepFor(0.01/*seconds*/);

    } // while loop

    //return true; // unnecesary
}


/*! Return false on user-abort.
*/
bool
RenderContext::generateSurfaceContextsForObject(ObjectContext* otx)
{
#if DEBUG
    assert(otx);
    assert(otx->motion_objects.size() > 0);
    assert(otx->motion_times.size() == otx->motion_objects.size());
#endif
#ifdef DEBUG_OBJECT_EXPANSION
    std::cout << "RenderContext::generateSurfaceContexts(" << otx << ")" << std::endl;
#endif

    GeoInfoContext* gptx = dynamic_cast<GeoInfoContext*>(otx);
    if (gptx)
    {
        //================================================
        // Geometry type:
        //================================================
        const uint32_t nGeos = (uint32_t)gptx->motion_geoinfos.size();

        GeoInfoContext::Sample& gtx0 = gptx->motion_geoinfos[0];
        const uint32_t nPrimitives = (uint32_t)gtx0.info->primitives();

        // Skip object if it's not supposed to render, or no prims::
        if (gtx0.info->render_mode == DD::Image::RENDER_OFF || nPrimitives == 0)
            return true;

#if 0
        // Is there a displacement bounds attribute?  If so save the value:
        const Attribute* db_attrib = gtx0.info->get_typed_group_attribute(DD::Image::Group_Object, "displacementbound", DD::Image::FLOAT_ATTRIB);
        float displacement_bounds;
        if (db_attrib && db_attrib->size() > 0) displacement_bounds = db_attrib->flt();
        else displacement_bounds = 0.0f;
#endif

        // Check for motion-blur method to determine if we check for paired primitive:
        bool check_for_mblur_primitive = true;
        std::string mb_method = Fsr::getObjectString(*gtx0.info, "mblur_method");
        if (mb_method == "velocity_forward" ||
            mb_method == "velocity_backward" ||
            mb_method == "constant")
            check_for_mblur_primitive = false; // vertex/points can change so don't bother verifying them.

        Fsr::Uint32List polysoupPrims; polysoupPrims.reserve(nPrimitives);

#ifdef DEBUG_OBJECT_EXPANSION
        uint32_t count = 0;
#endif
        for (uint32_t primIndex=0; primIndex < nPrimitives; ++primIndex)
        {
            // Get the base primitive (motion step 0):
            const DD::Image::Primitive* prim0 = gtx0.info->primitive_array()[primIndex];
#ifdef DEBUG_OBJECT_EXPANSION
            std::cout << "    " << primIndex << ": " << prim0->Class() << "(" << prim0 << ") getPrimitiveType()=" << (int)prim0->getPrimitiveType() << std::endl;
#endif

            // See if we can match the Nuke primitive type.
            // Check if it's a primitive type we specifically recognize from DDImage
            // or Fuser by doing a simple const char ptr or getPrimitiveType comparison.
            SourcePrimitiveType prim_type;

            /* DD::Image::Primitive.h
                enum  PrimitiveType
                {
                    eUnknownPrimitive = -1,
                    eTriangle,
                    ePolygon,
                    eMesh,
                    ePoint,
                    eParticles,
                    eParticlesSprite,
                    ePolyMesh,

                    ePrimitiveTypeCount
                };
            */

            if        (prim0->getPrimitiveType() == DD::Image::eTriangle) {
                prim_type = FN_POLYSOUP_PRIM;
                polysoupPrims.push_back(primIndex);

            } else if (prim0->getPrimitiveType() == DD::Image::ePolygon) {
                prim_type = FN_POLYSOUP_PRIM;
                polysoupPrims.push_back(primIndex);

            } else if (prim0->getPrimitiveType() == DD::Image::ePoint) {
                prim_type = FN_POINT_PRIM;
                polysoupPrims.clear();

            } else if (prim0->getPrimitiveType() == DD::Image::eMesh) {
                prim_type = FN_MESH_PRIM;
                polysoupPrims.clear();

            } else if (prim0->getPrimitiveType() == DD::Image::eParticlesSprite) {
                prim_type = FN_PARTICLE_SPRITE_PRIM;
                check_for_mblur_primitive = false; // the vertex and point count can change so don't bother verifying them.
                polysoupPrims.clear();

            //} else if (prim0->Class() == DD::Image::ePoint) {

// Ignore 'warning: the result of the conversion is unspecified because ## is
// outside the range of type DD::Image::PrimitiveType [-Wconversion]'
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
            } else if (prim0->getPrimitiveType() == FUSER_NODE_PRIMITIVE_TYPE) {
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            } else if (prim0->getPrimitiveType() == FUSER_NODE_PRIMITIVE_TYPE) {
#pragma GCC diagnostic pop
#endif
                prim_type = FUSER_NODEPRIM;
                polysoupPrims.clear();

// Ignore 'warning: the result of the conversion is unspecified because ## is
// outside the range of type DD::Image::PrimitiveType [-Wconversion]'
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
            } else if (prim0->getPrimitiveType() == FUSER_MESH_PRIMITIVE_TYPE) {
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            } else if (prim0->getPrimitiveType() == FUSER_MESH_PRIMITIVE_TYPE) {
#pragma GCC diagnostic pop
#endif
                prim_type = FUSER_MESHPRIM;
                polysoupPrims.clear();

            // Check for 7.0 PolyMesh first...:
            } else if (prim0->getPrimitiveType() == DD::Image::ePolyMesh) {
                prim_type = FN_POLYMESH_PRIM;
                polysoupPrims.clear();

            } else {
                // No direct support for this primitive type,
                // Have the primitive generate it's render primitives:
                std::cerr << "zpr::Scene::warning - unsupported primitive type '" << prim0->Class() << "'" << std::endl;
                continue;
            }

            SurfaceContext* stx = NULL;
            // Attempt to combine Triangle and Polygon atomic DD::Image::Primitives
            // together into a polysoup assuming they're part of the same mesh:
            if (polysoupPrims.size() > 0)
            {
                // Only combine if all prims in the GeoInfo can be in a polysoup
                // or this is the last prim (should be the same usually):
                if (polysoupPrims.size() == nPrimitives ||
                    primIndex == (nPrimitives-1))
                {
                    // Build the polysoup SurfaceContext:
                    stx = otx->addSurface();
                    stx->handler        = surface_handler[prim_type];
                    stx->obj_index      = gptx->motion_objects[0].index;
                    stx->prim_index     = -1; // prim_index not needed for PolySoup
                    stx->polysoup_prims = polysoupPrims;
#ifdef DEBUG_OBJECT_EXPANSION
                    std::cout << "  adding SurfaceContext for polysoup prims[" << primIndex << std::endl;
                    for (uint32_t i=0; i < nPrimitives; ++i)
                        std::cout << " " << polysoupPrims[i];
                    std::cout << " ]" << std::endl;
                    ++count;
#endif
                }
            }
            else
            {
                // Verify the motionblur primitives are good:
                if (check_for_mblur_primitive)
                {
                    bool ok = true;
                    const DD::Image::Primitive* prev_prim = prim0;
                    for (uint32_t i=1; i < nGeos; ++i)
                    {
                        const GeoInfoContext::Sample& gtx = gptx->motion_geoinfos[i];
                        const DD::Image::Primitive* prim = gtx.info->primitive_array()[primIndex];
                        if (prim != prev_prim &&
                            (strcmp(prim->Class(), prev_prim->Class())!=0 ||
                             prim->vertices() != prev_prim->vertices() ||
                             prim->faces()    != prev_prim->faces()))
                        {
                            // Most likely the prims are the NOT the same...
                            // Can't mblur two prims with a different vert/face count...
                            ok = false;
                            break;
                        }
                        prev_prim = prim;
                    }
                    // Skip this primitive if not ok:
                    if (!ok)
                        continue;
                }

                // Build the prim SurfaceContext:
                stx = otx->addSurface();
                stx->handler    = surface_handler[prim_type];
                stx->obj_index  = gptx->motion_objects[0].index;
                stx->prim_index = primIndex;

#ifdef DEBUG_OBJECT_EXPANSION
                std::cout << "  adding SurfaceContext for prim " << primIndex << std::endl;
                ++count;
#endif
            }

            // If there's a valid SurfaceContext configure the materials:
            if (stx != NULL)
            {
                static DD::Image::ColoredShader solid_shader(NULL/*Node*/);
                //static SolidShader solid_shader(NULL);
                stx->material = &solid_shader;  // default to solid shader

                if (gtx0.info->render_mode >= DD::Image::RENDER_TEXTURED)
                {
                    //stx->material = prim->material();  // Primitive material overrides geoinfo
                    if (gtx0.info->material)
                        stx->material = gtx0.info->material;
                }

                // Check if material is a RayShader:
                // This is a hack to get around dyload not exposing symbols to other plugins:
#ifdef ZPR_USE_KNOB_RTTI
                stx->surface_shader = (stx->material->knob(SurfaceShaderOp::zpClass())) ?
                                        static_cast<SurfaceShaderOp*>(stx->material) :
                                            NULL;
#else
                stx->surface_shader = dynamic_cast<SurfaceShaderOp*>(op);
#endif

                // Determine displacement for this surface:
                stx->displacement_shader   = NULL;
                stx->displacement_material = NULL; // TODO: set this!
                stx->displacement_enabled  = false;
                stx->displacement_subdivision_level = 0;
                stx->displacement_bounds.set(0.0f, 0.0f, 0.0f);
                if (stx->surface_shader)
                {
                    stx->material = NULL;
                    stx->displacement_shader = stx->surface_shader;
                    stx->displacement_subdivision_level = stx->surface_shader->getDisplacementSubdivisionLevel();

#if 0
                    // Scale it by the max of the object's scale:
                    stx->displacement_bounds = gtx0.info->matrix.scale()*stx->surface_shader->displacement_bound();

                    if (stx->displacement_bounds.x > std::numeric_limits<float>::epsilon() ||
                        stx->displacement_bounds.y > std::numeric_limits<float>::epsilon() ||
                        stx->displacement_bounds.z > std::numeric_limits<float>::epsilon())
                        stx->displacement_enabled = true;
                    //std::cout << "displacement_bounds" << stx->displacement_bounds << std::endl;
#endif
                }

#if 0
// TODO: finish this! Need to connect the attribs the geometry contain to their AOVLayer outputs
                // Assign the AOVLayers:
                stx->aov_layers.clear();
                stx->aov_layers.reserve(50);
                //stx->aov_layers.push_back(rtx.aov_outputs[0]);
#endif

                // If this is a polysoup prim run, bail:
                if (polysoupPrims.size() > 0)
                    break; // all done!
            }

        }
#ifdef DEBUG_OBJECT_EXPANSION
        std::cout << "  generated " << count << " surface contexts." << std::endl;
#endif

        return true; // no user-abort
    }


    // Not a geo, is it a light?
    LightVolumeContext* ltctx = dynamic_cast<LightVolumeContext*>(otx);
    if (k_atmospherics_enabled && k_direct_lighting_enabled && ltctx)
    {
        //================================================
        // Light Volume type:
        //================================================
#if DEBUG
        assert(ltctx->getLightVolumeSample(0).lt_ctx);
#endif
        DD::Image::LightOp* light = ltctx->getLightVolumeSample(0).lt_ctx->light();

        //! Get the prim type to create for a LightOp, or none if light can't create one.
        Fsr::Box3d lt_bbox;
        SourcePrimitiveType prim_type = getVolumeLightTypeAndBbox(light, lt_bbox);
        if (prim_type != UNRECOGNIZED_PRIM)
        {
            const uint32_t lt_index0 = ltctx->motion_objects[0].index;

            // Build the SurfaceContext:
            SurfaceContext* stx = otx->addSurface();
            stx->handler    = surface_handler[prim_type];
            stx->obj_index  = lt_index0;
            stx->prim_index = -1; // prim_index not needed

            //std::cout << "  adding SurfaceContext for Light " << lt_index0 << std::endl;
        }

        return true; // no user-abort
    }

    // Hm, not recognized...
    std::cerr << "RenderContext::generateSurfaceContexts(" << otx << ")";
    std::cerr << " object type not recognized, ignoring." << std::endl;

    return true; // no user-abort

} // generateSurfaceContextsForObject()


/*! If a light can illuminate atmosphere then it becomes a physical object of a certain size.
    Returns the source prim type if the LightOp can create a VolumeLight, and the
    bbox it encompasses.
*/
SourcePrimitiveType
RenderContext::getVolumeLightTypeAndBbox(const DD::Image::LightOp* light,
                                         Fsr::Box3d&               bbox) const
{
    //std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox():" << std::endl;
    bbox.clear();
    // Skip it if it's off:
    if (!light || light->node_disabled())
        return UNRECOGNIZED_PRIM;

    // Only create prim if light can illuminate atmosphere:
    DD::Image::Knob* k_light_illum = light->knob("illuminate_atmosphere");
    if (!k_light_illum)
        return UNRECOGNIZED_PRIM;
    bool can_illuminate_atmosphere = false;
    DD::Image::Hash junk;
    k_light_illum->store(DD::Image::BoolPtr, &can_illuminate_atmosphere, junk, light->outputContext());
    if (!can_illuminate_atmosphere)
        return UNRECOGNIZED_PRIM;

    //double near = clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity());
    //double far  = clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity());
    const Fsr::Mat4d light_xform(light->matrix());

    /* In DDImage LightOp.h:
        enum LightType
        { 
            ePointLight, 
            eDirectionalLight, 
            eSpotLight, 
            eOtherLight 
        };
    */

    // Check for recognized light types:
    if (light->lightType() == DD::Image::LightOp::eSpotLight)
    {
        // Cone:
        bbox = ConeVolume::getConeBbox(clamp(light->hfov(), 0.0001, 180.0),
                                       clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                       clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()),
                                       light_xform);
        //std::cout << " type=LIGHTCONE_PRIM, bbox" << bbox << std::endl;
        return LIGHTCONE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::ePointLight)
    {
        // LightSphere
        bbox = SphereVolume::getSphereBbox(clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                           clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()),
                                           light_xform);
        //std::cout << " type=LIGHTSPHERE_PRIM, bbox" << bbox << std::endl;
        return LIGHTSPHERE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::eDirectionalLight)
    {
        // LightCylinder

        //std::cout << " type=LIGHTCYLINDER_PRIM, bbox" << bbox << std::endl;
        return LIGHTCYLINDER_PRIM;

    }
    else
    {
        // Check for ReflectionCard:
        if (strcmp(light->Class(), "ReflectionCard")==0 ||
            strcmp(light->Class(), "AreaLight")==0)
        {
            // LightCard

            //std::cout << " type=LIGHTCARD_PRIM, bbox" << bbox << std::endl;
            return  LIGHTCARD_PRIM; 
        }
    }

    //std::cout << " UNRECOGNIZED TYPE" << std::endl;
    //std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox(): warning, unknown light type, skipping..." << std::endl;

    return UNRECOGNIZED_PRIM;
}


/*! Return false on user-abort.
*/
bool
RenderContext::generateRenderPrimitivesForObject(ObjectContext* otx)
{
#if DEBUG
    assert(otx);
    assert(otx->motion_objects.size() > 0);
    assert(otx->motion_times.size() == otx->motion_objects.size());
#endif
    //std::cout << "RenderContext::generateRenderPrimitivesForObject(" << otx << ")" << std::endl;

    const uint32_t nSurfaces = (uint32_t)otx->surface_list.size();
    if (nSurfaces == 0)
        return true; // nothing to generate

#ifdef DEBUG_OBJECT_EXPANSION
    if (k_debug == RenderContext::DEBUG_LOW)
        std::cout << otx << ": building rprims for " << nSurfaces << " surfaces:" << std::endl;
#endif

    // Create RenderPrimitives by calling zpRender surface handlers:
    for (uint32_t i=0; i < nSurfaces; ++i)
    {
#if DEBUG
        assert(otx->surface_list[i]);
#endif
        SurfaceContext& stx = *otx->surface_list[i];
        if (stx.status == SURFACE_NOT_DICED)
        {
#if DEBUG
            assert(stx.handler);
#endif
#ifdef DEBUG_OBJECT_EXPANSION
            std::cout << "  dicing surface " << i << " using handler ";
            std::cout << stx.handler->Class() << "()::generateRenderPrims()";
            std::cout << std::endl;
#endif

            //-------------------------------------------
            stx.handler->generateRenderPrims(*this, stx);
            //-------------------------------------------

            stx.status = SURFACE_DICED;
        }
    }

    return true; // no user-abort

} // generateRenderPrimitivesForObject()


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// Bvh specializations
// Implementation of specialized methods must be outside the class definition:


#if 0
#include <DDImage/gl.h>

/*! */
template <class T>
void
Bvh<T>::drawNode() const
{
    if (m_data)
    {
        // LEAF NODE (no children), draw our bbox in green:
        glPushAttrib(GL_LINE_BIT);

        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x00ff);

        glColor3f(0.0f, 0.7f, 0.0f);
        gl_boxf(m_bbox.min().x, m_bbox.min().y, m_bbox.min().z, m_bbox.max().x, m_bbox.max().y, m_bbox.max().z);

        glPopAttrib(); // GL_LINE_BIT
    }
    else
    {
        glPushAttrib(GL_LINE_BIT);

        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xff00);

        glColor3f(0.0f, 0.0f, 0.7f);
        gl_boxf(m_bbox.min().x, m_bbox.min().y, m_bbox.min().z, m_bbox.max().x, m_bbox.max().y, m_bbox.max().z);

        glPopAttrib(); // GL_LINE_BIT

        // Draw the children nodes:
        if (A) A->drawNode();
        if (B) B->drawNode();
    }
}
#endif


//--------------------------------------------------------------------------


/*virtual*/
template<>
Fsr::RayIntersectionType
ObjectContextBvh::getFirstIntersection(RayShaderContext&    stx,
                                       SurfaceIntersection& I)
{
    //std::cout << "ObjectContextBvh::getFirstIntersection(" << this << ")" << bbox();
    //std::cout << " " << stx.x << " " << stx.y << std::endl;
    if (this->isEmpty())
        return Fsr::RAY_INTERSECT_NONE;

    Fsr::RayIntersectionType obj_hit = Fsr::RAY_INTERSECT_NONE;

    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = m_node_list[current_node_index];
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth();
        //std::cout << ", itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
        if (Fsr::intersectAABB(node.bbox, m_bbox_origin, stx.Rtx))
        {
            if (node.isLeaf())
            {
                //std::cout << "LEAF: itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
#if DEBUG
                assert(node.numItems() == 1);
#endif

                SurfaceIntersection It;
                It.t = std::numeric_limits<double>::infinity();

                // Expand then intersect each ObjectContext:
                ObjectContext* otx = this->getItem(node.itemStart());
#if DEBUG
                assert(otx);
#endif
                // Make sure ObjectContext has been expanded:
                if (!stx.rtx->expandObject(otx))
                    return Fsr::RAY_INTERSECT_NONE; // bail fast on user-abort
#if DEBUG
                assert(otx->status == SURFACE_DICED);
#endif

                //std::cout << "      " << obj << ": leaf item[" << obj << "] otx=" << otx << " nPrims=" << otx->prim_list.size() << std::endl;

                const uint32_t nPrims = (uint32_t)otx->prim_list.size();
                for (uint32_t p=0; p < nPrims; ++p)
                {
                    RenderPrimitive* rprim = otx->prim_list[p];
#if DEBUG
                    assert(rprim);
#endif
                    // Only intersect tracable primitives:
                    if (!rprim->isTraceable())
                        continue; // don't bother...

                    //std::cout << "    prim=" << prim->getClass() << std::endl;
                    Fsr::RayIntersectionType hit = rprim->isTraceable()->getFirstIntersection(stx, It);
                    if (hit > Fsr::RAY_INTERSECT_NONE && It.t < I.t)
                    {
                        if (hit > obj_hit)
                            obj_hit = hit;
                        I = It;
                    }
                }
                //if (obj_hit) std::cout << "  ObjectContextBvh::getFirstIntersection(" << stx.x << " " << stx.y << ") obj_hit=" << obj_hit << ", I.t=" << I.t << std::endl;
                //m_bbox.printInfo("  "); std::cout << " obj_hit=" << obj_hit << std::endl;

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

    return obj_hit;
}


//--------------------------------------------------------------------------


/*virtual*/
template<>
void
ObjectContextBvh::getIntersections(RayShaderContext&        stx,
                                   SurfaceIntersectionList& I_list,
                                   double&                  tmin,
                                   double&                  tmax)
{
    //std::cout << "ObjectContextBvh::getIntersections(" << this << ")" << bbox();
    //std::cout << " " << stx.x << " " << stx.y << std::endl;
    if (this->isEmpty())
        return;

    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = m_node_list[current_node_index];
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth();
        //std::cout << ", itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
        if (Fsr::intersectAABB(node.bbox, m_bbox_origin, stx.Rtx))
        {
            if (node.isLeaf())
            {
                //std::cout << "LEAF: itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
#if DEBUG
                assert(node.numItems() == 1);
#endif

                // Expand then intersect each ObjectContext:
                ObjectContext* otx = this->getItem(node.itemStart());
#if DEBUG
                assert(otx);
#endif
                // Make sure ObjectContext has been expanded:
                if (!stx.rtx->expandObject(otx))
                    return; // bail fast on user-abort
#if DEBUG
                assert(otx->status == SURFACE_DICED);
#endif

                //std::cout << "      " << obj << ": leaf item[" << obj << "] otx=" << otx << " nPrims=" << otx->prim_list.size() << std::endl;

                const uint32_t nPrims = (uint32_t)otx->prim_list.size();
                for (uint32_t p=0; p < nPrims; ++p)
                {
                    RenderPrimitive* rprim = otx->prim_list[p];
#if DEBUG
                    assert(rprim);
#endif
                    // Only intersect tracable primitives:
                    if (!rprim->isTraceable())
                        continue; // don't bother...

                    //std::cout << "    prim=" << prim->getClass() << std::endl;
                    rprim->isTraceable()->getIntersections(stx, I_list, tmin, tmax);
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


//-----------------------------------------------------------------------------------------------------------


/*virtual*/
template<>
int
ObjectContextBvh::intersectLevel(RayShaderContext& stx,
                                 int               level,
                                 int               max_level)
{
    //std::cout << "  ObjectContextBvh::intersectLevel(" << this << ")" << this->bbox();
    //std::cout << ": current-level=" << level << ", max_level=" << max_level << std::endl;
    if (this->isEmpty())
        return level;

    // TODO: test using getIntersectedLeafs() rather than re-implementing this logic:

    int out_level = level;
    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = m_node_list[current_node_index];
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth();
        //std::cout << ", itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
        if (Fsr::intersectAABB(node.bbox, m_bbox_origin, stx.Rtx))
        {
            const int node_level = level + 1 + node.getDepth();
            if (node_level >= max_level)
                return node_level;
            else if (node_level > out_level)
                out_level = node_level;

            if (node.isLeaf())
            {
                //std::cout << "LEAF: itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
#if DEBUG
                assert(node.numItems() == 1);
#endif

                // Expand then intersect each ObjectContext:
                ObjectContext* otx = this->getItem(node.itemStart());
#if DEBUG
                assert(otx);
#endif
                // Make sure ObjectContext has been expanded:
                if (!stx.rtx->expandObject(otx))
                    return out_level; // bail fast on user-abort
#if DEBUG
                assert(otx->status == SURFACE_DICED);
#endif

                const uint32_t nPrims = (uint32_t)otx->prim_list.size();
                for (uint32_t p=0; p < nPrims; ++p)
                {
                    RenderPrimitive* rprim = otx->prim_list[p];
#if DEBUG
                    assert(rprim);
#endif
                    // Only intersect tracable primitives:
                    if (!rprim->isTraceable())
                        continue; // don't bother...

                    //std::cout << "ObjectContextBvh::intersectLevel(" << this << ") prim=" << prim->getClass() << std::endl;
                    int sub_level = rprim->isTraceable()->intersectLevel(stx, node_level-1, max_level);
                    if (sub_level >= max_level)
                        return sub_level;
                    else if (sub_level > out_level)
                        out_level = sub_level;
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

    //std::cout << "      out_level=" << out_level << std::endl;
    return out_level; // no hits
}


//-----------------------------------------------------------------------------------------------------------


/*virtual*/
template<>
void
ObjectContextBvh::getIntersectionsWithUVs(RayShaderContext&          stx,
                                          const Fsr::Vec2f&          uv0,
                                          const Fsr::Vec2f&          uv1,
                                          UVSegmentIntersectionList& I_list)
{
std::cout << "ObjectContextBvh::getIntersectionsWithUVs(" << this << ")" << bbox() << std::endl;

#if 1
#else
    // If node's a leaf we intersect the objects and return:
    BoundingVolumeLeaf<ObjectContext*>* leaf = isLeaf();
    if (leaf)
    {
        // Check if objects have been expanded yet:
        if (!leaf->expand(*stx.rtx))
        {
            std::cout << "  ObjectContextBvh::getIntersectionsWithUVs() abort" << std::endl;
            return; // bail on user-abort
        }

        // All objects should have their Bvhs created now:
        const std::vector<ObjectContext*>& objects = leaf->data();
        const uint32_t nObjects = (uint32_t)objects.size();
        for (uint32_t obj=0; obj < nObjects; ++obj)
        {
            ObjectContext* otx = objects[obj];
#if DEBUG
            assert(otx);
            assert(otx->prim_bvhs.size() > 0);
#endif
            otx->prim_bvhs[0/*shutter_step*/].getIntersectionsWithUVs(stx, uv0, uv1, I_list);
        }

        return;
    }

    // Check children next:
    for (uint32_t i=0; i < 2; ++i)
        if (child[i])
            child[i]->getIntersectionsWithUVs(stx, uv0, uv1, I_list);
#endif
}



} // namespace zpr

// end of zprender/RenderContext.cpp

//
// Copyright 2020 DreamWorks Animation
//
