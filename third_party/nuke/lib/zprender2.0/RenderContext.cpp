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
#include "RayPerspectiveCamera.h"
//#include "RayOrthographicCamera.h"
//#include "RayUVCamera.h"
#include "RaySphericalCamera.h"
#include "RayCylindricalCamera.h"
#include "SurfaceHandler.h"
#include "ThreadContext.h"
#include "RayMaterial.h"
#include "RayShader.h"
#include "LightMaterial.h"
#include "LightShader.h"
#include "SurfaceMaterialOp.h"
#include "ConeVolume.h"   // for getConeBBox
#include "SphereVolume.h" // for getSphereBBox


#include <Fuser/Primitive.h>  // for FUSER_NODE_PRIMITIVE_TYPE, FUSER_MESH_PRIMITIVE_TYPE, etc
#include <Fuser/NukeGeoInterface.h> // for getObjectString()
#include <Fuser/NukeKnobInterface.h> // for getVec3Knob, etc
#include <Fuser/Primitive.h>
#include <Fuser/MaterialNode.h>


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
static Bvh<SurfaceContext*> bvh_sftx;

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
        interpolateVec4(vP[0].UV(),  vP[1].UV(),  vP[2].UV(), st, v.UV());
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
static void aov_handler_N(    const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.N.array(),    3, aov, out); }
static void aov_handler_Nf(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Nf.array(),   3, aov, out); }
static void aov_handler_Ni(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Ni.array(),   3, aov, out); }
static void aov_handler_Ng(   const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.Ng.array(),   3, aov, out); }
static void aov_handler_dNdx( const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dNdx.array(), 3, aov, out); }
static void aov_handler_dNdy( const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out) { copyAttribd(stx.dNdy.array(), 3, aov, out); }
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
    //              //
    handlers[AOV_st       ].set("st",              aov_handler_st       );
    handlers[AOV_dstdx    ].set("dstdx",           aov_handler_dstdx    );
    handlers[AOV_dstdy    ].set("dstdy",           aov_handler_dstdy    );
    //              //
    handlers[AOV_N        ].set("n",               aov_handler_N        );
    handlers[AOV_Nf       ].set("nf",              aov_handler_Nf       );
    handlers[AOV_Ni       ].set("ni",              aov_handler_Ni       );
    handlers[AOV_Ng       ].set("ng",              aov_handler_Ng       );
    handlers[AOV_dNdx     ].set("dndx",            aov_handler_dNdx     );
    handlers[AOV_dNdy     ].set("dndy",            aov_handler_dNdy     );
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
/*static*/ const char* RenderContext::sides_modes[]        = { "both", "front", "back", 0 };
/*static*/ const char* RenderContext::debug_names[]        = { "off", "low", "medium", "high", 0 };
/*static*/ const char* RenderContext::diagnostics_modes[]  = { "off", "time", "bounds", "bvh-leafs", "intersections", "volumes", "patches", "render-time", 0 };


static NullSurfaceHandler null_surface_handler;


//-----------------------------------------------------------------------------


/*!
*/
RenderContext::RenderContext(DD::Image::Op* parent) :
    m_parent(parent)
{
    //----------------------------------------------
    // Driven by knob controls:
    k_shutter_bias              = 0.0;
    k_projection_mode           = CAMERA_PROJECTION_PERSPECTIVE;
    k_camera_mode               = CAMERA_COMBINED;
    k_shading_interpolation     = SHADING_SMOOTH;
    k_sides_mode                = SIDES_FRONT;
    k_preview_mode              = false;
    k_debug                     = DEBUG_NONE;
    k_pixel_filter              = DD::Image::Filter::Cubic;
    k_pixel_filter_size[0]      = k_pixel_filter_size[1] = 1.0f;
    k_spatial_jitter_threshold  = 1;
    k_output_bbox_mode          = BBOX_SCENE_SIZE;

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
    render_projection           = DD::Image::CameraOp::LENS_PERSPECTIVE;
    texture_channels            = DD::Image::Mask_None;
    material_channels           = DD::Image::Mask_None;
    render_format               = NULL;
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

    direct_lighting_enabled      = true;
    indirect_lighting_enabled    = true;
    atmospheric_lighting_enabled = false;

    hash.reset();
    objects_initialized         = false;

    objects_bvh_initialized     = false;
    lights_bvh_initialized      = false;

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
    destroyObjectBVHs(true);
    destroyLightBVHs(true);
    destroyRayMaterials();
}


//-------------------------------------------------------------------------


/*! Delete all context allocations.
*/
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
}


/*! Delete object bvhs
*/
void
RenderContext::destroyObjectBVHs(bool force)
{
    // TODO: support hash testing before deleting all objects!
    for (uint32_t i=0; i < object_context.size(); ++i)
        delete object_context[i];
    object_context.clear();
    object_map.clear();
    objects_bvh.clear();
}


/*! Delete LightVolume bvhs
*/
void
RenderContext::destroyLightBVHs(bool force)
{
    // TODO: support hash testing before deleting all LightVolume objects!
    for (uint32_t i=0; i < light_context.size(); ++i)
        delete light_context[i];
    light_context.clear();
    light_map.clear();
    lights_bvh.clear();
}


/*!
*/
void
RenderContext::destroyTextureSamplers()
{
    //std::cout << "  RenderContext(" << this << ")::destroyTextureSamplers()" << std::endl;
    for (Texture2dSamplerMap::iterator it=texture_sampler_map.begin(); it != texture_sampler_map.end(); ++it)
        delete it->second;
    texture_sampler_map.clear();
}


/*!
*/
void
RenderContext::destroyRayMaterials()
{
    //std::cout << "  RenderContext(" << this << ")::destroyRayMaterials()" << std::endl;

    // Delete the texture samplers *before* the RayMaterials/RayShaders
    // so any Iop-locked Tiles are release first:
    destroyTextureSamplers();

    // Delete all the RayMaterials (surface, light, etc):
    for (size_t i=0; i < active_ray_materials.size(); ++i)
        delete active_ray_materials[i];
    active_ray_materials.clear();

    // Just clear the light material lists since any shader deletions already
    // happened in the active_ray_materials list deletion:
    master_light_materials.clear();
    per_object_light_materials.clear();
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Allocated and return a RayCamera subclass based on requested type.
    Calling method takes ownership.

    TODO: change this to checking a camera type string vs. an enumeration!
*/
/*static*/
zpr::RayCamera*
RenderContext::buildRayCamera(CameraProjectionType camera_type)
{
    switch (camera_type)
    {
        default:
        case CAMERA_PROJECTION_PERSPECTIVE:  return new zpr::RayPerspectiveCamera();
        //case CAMERA_PROJECTION_ORTHOGRAPHIC: return new zpr::RayOrthoCamera();
        //case CAMERA_PROJECTION_UV:           return new zpr::RayUVCamera();
        case CAMERA_PROJECTION_SPHERICAL:    return new zpr::RaySphericalCamera();
        case CAMERA_PROJECTION_CYLINDRICAL:  return new zpr::RayCylindricalCamera();
    }
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


// This is the maximum 2D bbox we can allow, otherwise some weird numerical
// problems occur:
static DD::Image::Box max_format_bbox(-1000000, -1000000, 1000000, 1000000);


/*!
*/
bool
RenderContext::buildObjectMaterial(int32_t          obj,
                                   DD::Image::Hash& obj_geometry_hash,
                                   DD::Image::Box3& obj_bbox,
                                   DD::Image::Box&  obj_screen_bbox,
                                   RayMaterial*&    obj_created_ray_material)
{
    obj_geometry_hash.reset();
    obj_bbox.clear();
    obj_screen_bbox.clear();
    obj_created_ray_material = NULL;

#if DEBUG
    assert(obj >= 0 && obj < (int32_t)object_material_ctxs.size());
#endif
    MaterialContext& material_ctx = object_material_ctxs[obj];
    material_ctx.object_type           = SURFACE_OBJECT;
    material_ctx.scene_index           = obj;
    material_ctx.enabled               = false;
    //
    material_ctx.raymaterial           = NULL;
    material_ctx.material              = NULL;
    material_ctx.displacement_material = NULL;
    material_ctx.surface_ctx           = NULL;
    //
    material_ctx.hash.reset();
    material_ctx.texture_channels      = DD::Image::Mask_None;
    material_ctx.output_channels       = DD::Image::Mask_None;
    material_ctx.shadow_channels       = DD::Image::Mask_None;
    //
    material_ctx.displacement_enabled           = false;
    material_ctx.displacement_subdivision_level = 0;
    material_ctx.displacement_bounds.set(0.0f, 0.0f, 0.0f);
    //
    material_ctx.texture_bindings.clear();

    zpr::Scene* scene0 = shutter_scenerefs[0].scene;
#if DEBUG
    assert(scene0);
#endif

    // Skip object if render mode is off:
    DD::Image::GeoInfo& info0 = scene0->object(obj);
#if 0
    {
    static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
    std::cout << "    RenderContext(" << this << ")::buildObjectMaterial(): obj=" << obj;
    std::cout << ", name='" << Fsr::getObjectName(info0) << "', scene:path='" << Fsr::getObjectPath(info0) << "'";
    std::cout << ", scene0=" << scene0;
    std::cout << ", frame=" << scene0->frame;
    std::cout << std::endl;
    }
#endif

    if (info0.render_mode == DD::Image::RENDER_OFF)
        return false;

    //-----------------------------------------------------------
    // Determine material assignment
    //-----------------------------------------------------------

    bool material_assigned = false;

    // Check for a material Op override first (ie a connected material Op):
    if (info0.material)
    {
        // We can detect if a GeoInfo does not have an active material assignment
        // by comparing the pointer to the Iop::default_input(), which should be
        // assigned to all non-connected inputs, or if it's NULL:
        //if (info0.material != DD::Image::Iop::default_input(info0.material->outputContext()))
        //-----------------
        // TODO: Iop::default_input() sometimes locks up in OpenImageIO...! See if this is a problem
        // in 11 or 12...
        // Meantime we use the class name and test for 'Black':
        if (strcmp(info0.material->Class(), "Black") != 0)
        {
            // Only validate the material if it's not a default black Iop from a dangling input.
            // This is important to get SurfaceMaterialOps inputs up to date before calling
            // createMaterial() on them:
            info0.material->validate();

#if 0
            static DD::Image::ColoredShader solid_shader(NULL/*Node*/);
            //static SolidShader solid_shader(NULL);
            sftx->material = &solid_shader;  // default to solid shader

            if (gtx0.info->render_mode >= DD::Image::RENDER_TEXTURED)
            {
                //sftx->material = prim->material();  // Primitive material overrides geoinfo
                if (gtx0.info->material)
                    sftx->material = gtx0.info->material;
            }
#endif

            SurfaceMaterialOp* surface_material_op = SurfaceMaterialOp::getOpAsSurfaceMaterialOp(info0.material);
            if (surface_material_op)
            {
                material_ctx.raymaterial = surface_material_op->createMaterial(*this);
                // Don't crash...
                if (material_ctx.raymaterial)
                {
                    obj_created_ray_material = material_ctx.raymaterial; // < take ownership of RayMaterial*
                    //
                    material_ctx.raymaterial->validateMaterial(true/*for_real*/, *this);
                    //
                    material_ctx.texture_bindings.reserve(20);
                    material_ctx.raymaterial->getActiveTextureBindings(material_ctx.texture_bindings);
                    //
                    material_ctx.texture_channels = material_ctx.raymaterial->getTextureChannels(); 
                    material_ctx.output_channels  = material_ctx.raymaterial->getChannels();
                    //material_ctx.hash             = material_ctx.raymaterial->hash();

                    RayShader* disp_shader = material_ctx.raymaterial->getDisplacementShader();
                    if (disp_shader)
                    {
                        //material_ctx.displacement_bounds = material_ctx.raymaterial->getDisplacementBound();
#if 0
                        sftx->displacement_subdivision_level = disp_shader->getDisplacementSubdivisionLevel();
                        // Scale it by the max of the object's scale:
                        sftx->displacement_bounds = gtx0.info->matrix.scale()*disp_shader->displacement_bound();

                        if (sftx->displacement_bounds.x > std::numeric_limits<float>::epsilon() ||
                            sftx->displacement_bounds.y > std::numeric_limits<float>::epsilon() ||
                            sftx->displacement_bounds.z > std::numeric_limits<float>::epsilon())
                            sftx->displacement_enabled = true;
                        //std::cout << "displacement_bounds" << sftx->displacement_bounds << std::endl;
#endif
                    }
                }
                else
                {
                    material_ctx.texture_channels = DD::Image::Mask_None; 
                    material_ctx.output_channels  = DD::Image::Mask_None;
                    material_ctx.hash.reset();
                }
            }
            else
            {
                // Legacy material, set both to same set:
                material_ctx.material             = info0.material;
//material_ctx.surface_ctx      = ;
                material_ctx.texture_channels     = info0.material->channels(); 
                material_ctx.output_channels      = info0.material->channels();
                material_ctx.hash                 = info0.material->hash();

#if 0
                const float disp_max = info0.material->displacement_bound();
                material_ctx.displacement_bounds.set(disp_max, disp_max, disp_max);
                // Scale it by the max of the object's scale:
                sftx->displacement_bounds = gtx0.info->matrix.scale()*disp_shader->displacement_bound();

                if (sftx->displacement_bounds.x > std::numeric_limits<float>::epsilon() ||
                    sftx->displacement_bounds.y > std::numeric_limits<float>::epsilon() ||
                    sftx->displacement_bounds.z > std::numeric_limits<float>::epsilon())
                    sftx->displacement_enabled = true;
#endif
            }
            material_assigned = true;
            //{
            //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
            //std::cout << "      material_ctx:";
            //std::cout << " raymaterial=" << material_ctx.raymaterial;
            //std::cout << ", material=" << material_ctx.material;
            //std::cout << ", texture_channels=" << material_ctx.texture_channels;
            //std::cout << ", output_channels=" << material_ctx.output_channels;
            //std::cout << ", displacement_max=" << material_ctx.displacement_max;
            //std::cout << std::endl;
            //}
        }
    }


    const unsigned nPrims = info0.primitives();
    const DD::Image::Primitive** prim_array0 = info0.primitive_array();

    // No explictly connected material Op.
    // Does the object have an assigned material binding path and does it point
    // underneath this object's path? ie it's not an absolute path.
    if (!material_assigned && nPrims > 0)
    {
        const std::string material_path = Fsr::getObjectMaterialBinding(info0);
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "      obj material:binding path='" << material_path << "'";
        //std::cout << std::endl;
        //}
        if (!material_path.empty() && nPrims == 1)
        {

            // Get Fuser primitive (Mesh usually) and find the child Fuser Node
            // matching the material path:
#if DEBUG
            assert(prim_array0);
            assert(prim_array0[0]);
#endif
            const Fsr::FuserPrimitive* fsr_prim = dynamic_cast<const Fsr::FuserPrimitive*>(prim_array0[0]);
            if (fsr_prim)
            {
                // Find the Fuser MaterialNode as a child of this prim:
                const Fsr::MaterialNode* mat_node = dynamic_cast<const Fsr::MaterialNode*>(fsr_prim->getChildByPath(material_path));
                if (mat_node)
                {
                    // Create the RayShaders from the Fsr::MaterialNode.

                    // We look at all 'surface' type outputs and handle the one we
                    // know how to convert to RayShaders.

                    // For now that's just the stock UsdPreviewSurface shader set:
                    const std::vector<Fsr::ShaderNode*>& surface_outputs = mat_node->surfaceOutputs();
                    const uint32_t nSurfaceOutputs = (uint32_t)surface_outputs.size();
                    for (uint32_t i=0; i < nSurfaceOutputs; ++i)
                    {
                        Fsr::ShaderNode* output = surface_outputs[i];
                        assert(output);
                        const std::string& output_label = output->getString("material:output");
                        //std::cout << "  node('" << output->getName() << "'): output='" << output_label << "'" << std::endl;

                        // TODO: there should be a plugin callback for this conversion based on the
                        // label text:
                        if (output_label == "usd:surface")
                        {
                            // TODO: for now we hardcode a UsdPreviewSurface conversion:
                            material_ctx.raymaterial = RayMaterial::createUsdPreviewSurface(output);
                            break;
                        }
                    }

                    // Don't crash...
                    if (material_ctx.raymaterial)
                    {
                        obj_created_ray_material = material_ctx.raymaterial; // < take ownership of RayMaterial*
                        //
                        material_ctx.raymaterial->validateMaterial(true/*for_real*/, *this);
                        //
                        material_ctx.texture_bindings.reserve(20);
                        material_ctx.raymaterial->getActiveTextureBindings(material_ctx.texture_bindings);
                        //
                        material_ctx.texture_channels = material_ctx.raymaterial->getTextureChannels(); 
                        material_ctx.output_channels  = material_ctx.raymaterial->getChannels();
                        //material_ctx.hash             = material_ctx.raymaterial->hash();
                        //material_ctx.displacement_max = material_ctx.raymaterial->displacement_bound();
                    }
                    else
                    {
                        material_ctx.texture_channels = DD::Image::Mask_None; 
                        material_ctx.output_channels  = DD::Image::Mask_None;
                        material_ctx.hash.reset();
                    }

                    material_assigned = true;
                }
            }
        }
    }


    // Even if no material assignment output rgba channels if there's prims:
    if (!material_assigned && nPrims > 0)
        material_ctx.output_channels = DD::Image::Mask_RGBA;


    //-----------------------------------------------------------
    // Find object extent in worldspace and screenspace
    //-----------------------------------------------------------

    bool renderable = true;
    const uint32_t nShutterSamples = numShutterSamples();
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        zpr::Scene* scene = shutter_scenerefs[j].scene;
        assert(scene);

        // Get primary GeoInfo or the matching GeoInfo in mblur scenes
        // using the GeoInfo out_id as a key:
        DD::Image::GeoInfo* info = NULL;
        if (j == 0)
            info = &scene->object(obj);
        else
            info = scene->getMatchingObject(info0);
        if (!info)
        {
            if (j == 0)
                renderable = false;
            break; // no matching object, stop
        }

        // Combine the GeoInfo hashes together:
        obj_geometry_hash.append(info->out_id());

        // Make sure primitives and attribute references are up-to-date:
        info->validate();

        // Get object bbox, but don't use the GeoInfo::update_bbox() method.
        //info->update_bbox();
        Fsr::Box3d point_bbox;

// TODO: we don't really need to write into the GeoInfo cache for this
// as we can store the object bbox separately, but it's convenient to
// have the GeoInfo up to date for later on.
        if (!info->point_list() || info->point_list()->size() == 0)
        {
            if (j == 0)
                renderable = false;
            break; // no points!
        }

        // Do individual primitives create their point bboxes?
        // Common cases of this are particles, instances, Fuser prims, or complex
        // prims like a PointCloud with point radii that expand the points into
        // spheres, discs or cards.
        //
        const DD::Image::Primitive** prim_array = info->primitive_array();
        if (prim_array)
        {
            const uint32_t nPrims = info->primitives();
            for (uint32_t i=0; i < nPrims; ++i)
            {
                const DD::Image::Primitive* prim = *prim_array++;

                // TODO: finish this!!! Support the other types.
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
                const bool isFuserPrim = (prim->getPrimitiveType() >= FUSER_NODE_PRIMITIVE_TYPE);
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                const bool isFuserPrim = (prim->getPrimitiveType() >= FUSER_NODE_PRIMITIVE_TYPE);
#pragma GCC diagnostic pop
#endif

                if (isFuserPrim ||
                    prim->getPrimitiveType() == DD::Image::eParticlesSprite)
                {
                    const Fsr::Box3d prim_bbox(prim->get_bbox(info));
                    if (i == 0)
                        point_bbox = prim_bbox;
                    else
                        point_bbox.expand(prim_bbox, false/*test_empty*/);
                }

#if 0
                // TODO: we're ignoring per-prim material assignments - verify this is ok!!
                DD::Image::Iop* material = (*prim_array++)->material();
                if (material)
                    material->validate();
                material_ctx.output_channels += material->channels();

                // Combine the material hashes together:
                material_ctx.hash.append(material->hash());
#endif
            }
        }

        // If the point bbox is still empty then the prims didn't
        // create their bounds with get_bbox(), so just make the
        // bbox from the raw points:
        if (point_bbox.isEmpty() && info->point_list() && info->point_list()->size() > 0)
            point_bbox.set(Fsr::Box3f(reinterpret_cast<const Fsr::Vec3f*>(info->point_list()->data()),
                           info->point_list()->size()));

        // Save evaluated but un-xformed point bbox back to GeoInfo cache before
        // modifying it for rendering:
        {
            DD::Image::GeoInfo::Cache* writable_cache =
                const_cast<DD::Image::GeoInfo::Cache*>(info->get_cache_pointer());
            writable_cache->bbox = point_bbox.asDDImage();
        }

        // Don't render the object if any of the point bboxes are empty:
        if (point_bbox.isEmpty())
        {
            if (j == 0)
                renderable = false;
            break;
        }

        // Possibly expand it by displacement bounds then transform to
        // world-space before projecting:
        point_bbox.pad(material_ctx.displacement_bounds);
        point_bbox = Fsr::Mat4d(info->matrix).transform(point_bbox);

        // Screen projection still needs DDImage Box3...:
        const DD::Image::Box3 point_bboxdd = point_bbox.asDDImage();

        // Find the screen projected bbox of this object:
// TODO: This should use the code that manages the camera projections so that any lens projection can be supported
// TODO: change this to Fuser math classes
        DD::Image::Box screen_bboxdd = max_format_bbox; // default to max
        if (scene->camera && (render_projection == CAMERA_PROJECTION_PERSPECTIVE ||
                              render_projection == CAMERA_PROJECTION_ORTHOGRAPHIC))
        {
            // Check if camera is inside the object's bbox as we can't project a bbox
            // that's surrounding the camera:
            if (!point_bboxdd.inside(scene->cam_vectors.p))
            {
                // Project the object's bbox into screen space:
                point_bboxdd.project(scene->matrix(WORLD_TO_SCREEN), screen_bboxdd);
                screen_bboxdd.intersect(max_format_bbox);
            }
        }

        if (j == 0)
        {
            obj_bbox        = point_bboxdd;
            obj_screen_bbox = screen_bboxdd;
        }
        else
        {
            obj_bbox.expand(point_bboxdd);
            obj_screen_bbox.intersect(screen_bboxdd);
        }

#if 0
        {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        std::cout << "     sample " << j << ": obj=" << obj;
        std::cout << ", out_id=0x" << std::hex << info->out_id().value() << std::dec;
        std::cout << ", point_bbox" << point_bbox;
        std::cout << ", obj_bbox" << Fsr::Box3f(obj_bbox);
        std::cout << ", obj_screen_bbox" << Fsr::Box2d(obj_screen_bbox);
        std::cout << ", renderable=" << renderable;
        std::cout << std::endl;
        }
#endif
    }

    obj_bbox.append(obj_geometry_hash);

#if 0
    {
    static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
    std::cout << "      obj_outid=0x" << std::hex << info0.out_id().value() << std::dec << std::endl;
    std::cout << "      obj_bbox" << Fsr::Box3f(obj_bbox) << std::endl;
    std::cout << "      obj_screen_bbox" << Fsr::Box2d(obj_screen_bbox) << std::endl;
    std::cout << "      obj_geometry_hash=0x" << std::hex << obj_geometry_hash.value() << std::dec << std::endl;
    }
#endif

    if (!renderable || obj_bbox.empty())
        return false; // zero size, can't render

    material_ctx.enabled = true;

    return true; // render the object
}


/*! TODO: move this to header
*/
struct ObjectState
{
    DD::Image::Hash geometry_hash;
    DD::Image::Box3 bbox;
    DD::Image::Box  screen_bbox;
    RayMaterial*    created_ray_material; //!< *Allocated* RayMaterial pointer
};


/*!
*/
struct ValidateThreadContext
{
    RenderContext* rtx;
    AtomicCount32  do_obj;
    //
    std::vector<ObjectState> obj_states;

    //!
    ValidateThreadContext(RenderContext* _rtx,
                          int32_t        nObjects) :
        rtx(_rtx),
        do_obj(0)
    {
        obj_states.resize(nObjects);
    }

    /*! DD::Image::Thread spawn callback function to iterate through the object list.
    */
    static void thread_proc_cb(unsigned thread_index,
                               unsigned num_threads,
                               void*    p)
    {
        ValidateThreadContext* ctx = reinterpret_cast<ValidateThreadContext*>(p);
        assert(ctx && ctx->rtx);
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "thread_proc_cb(" << std::hex << DD::Image::Thread::GetThreadId() << std::dec << ")";
        //std::cout << " thread_index=" << thread_index << ", num_threads=" << num_threads;
        //std::cout << std::endl;
        //}

        while (1)
        {
            const int32_t obj = ctx->do_obj++; // get object to process and atomic increment
            if (obj >= (int32_t)ctx->obj_states.size())
                break;

            ObjectState& obj_state = ctx->obj_states[obj];
            if (ctx->rtx->buildObjectMaterial(obj,
                                              obj_state.geometry_hash,
                                              obj_state.bbox,
                                              obj_state.screen_bbox,
                                              obj_state.created_ray_material))
            {
                //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
                //std::cout << "      " << obj << ": thread_index=" << thread_index << ", num_threads=" << num_threads << std::endl;
                //std::cout << "      " << obj << ": obj_state.bbox" << Fsr::Box3f(obj_state.bbox) << std::endl;
                //std::cout << "      " << obj << ": obj_state.screen_bbox[" << Fsr::Box2i(obj_state.screen_bbox) << std::endl;
            }
        }
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "  thread_proc_cb(" << std::hex << DD::Image::Thread::GetThreadId() << std::dec << ")";
        //std::cout << " DONE" << std::endl;
        //}

    }
};


/*! Currently this routine assumes all lights are LightOps in a DD::Image::Scene.

    TODO: also check for Fuser Lights in the GeometryList.

    TODO: this is not threaded since there's typically very few lights
    in the Scene, but this may not be true when Fuser Lights are read
    in from large USD scenes...
*/
bool
RenderContext::buildLightMaterial(int32_t                ltindex,
                                  const Fsr::DoubleList& motion_times,
                                  const Fsr::Mat4dList&  motion_xforms,
                                  DD::Image::Hash&       lt_hash,
                                  LightMaterial*&        lt_created_material)
{
    //std::cout << "    RenderContext()::buildLightMaterial() ltindex=" << ltindex << std::endl;
    lt_hash.reset();
    lt_created_material = NULL;

#if DEBUG
    assert(ltindex >= 0 && ltindex < (int32_t)light_material_ctxs.size());
#endif
    MaterialContext& material_ctx = light_material_ctxs[ltindex];
    material_ctx.object_type           = LIGHT_OBJECT;
    material_ctx.scene_index           = ltindex;
    material_ctx.enabled               = false;
    //
    material_ctx.raymaterial           = NULL;
    material_ctx.material              = NULL;
    material_ctx.displacement_material = NULL;
    material_ctx.surface_ctx           = NULL;
    //
    material_ctx.hash.reset();
    material_ctx.texture_channels      = DD::Image::Mask_None;
    material_ctx.output_channels       = DD::Image::Mask_None;
    material_ctx.shadow_channels       = DD::Image::Mask_None;
    //
    material_ctx.displacement_enabled           = false;
    material_ctx.displacement_subdivision_level = 0;
    material_ctx.displacement_bounds.set(0.0f, 0.0f, 0.0f);
    //
    material_ctx.texture_bindings.clear();

    zpr::Scene* scene0 = shutter_scenerefs[0].scene;
#if DEBUG
    assert(scene0);
    assert(scene0->lights[ltindex]);
    assert(scene0->lights[ltindex]->light());
#endif
    DD::Image::LightOp* light = scene0->lights[ltindex]->light();
    if (light->node_disabled())
        return false;

    lt_hash = light->hash();

    // Create the LightMaterial by translating the LightOp:
    lt_created_material = LightMaterial::createLightMaterial(*this,
                                                             light,
                                                             motion_times,
                                                             motion_xforms);
    //std::cout << "      lt_created_material=" << lt_created_material << std::endl;
    if (!lt_created_material)
        return false;

    material_ctx.raymaterial = lt_created_material;

    // Let light shader calc any internal values:
    lt_created_material->validateMaterial(true/*for_real*/, *this);

    // Get the shadow channels for any legacy lights with shadow renderers:
    // TODO: do we really need this anymore...? Can't we stop using shadow renderers?
    material_ctx.shadow_channels = light->getShadowMaskChannel();
    //
    material_ctx.texture_bindings.reserve(20);
    material_ctx.raymaterial->getActiveTextureBindings(material_ctx.texture_bindings);
    //
    material_ctx.texture_channels = material_ctx.raymaterial->getTextureChannels(); 
    material_ctx.output_channels  = material_ctx.raymaterial->getChannels();

    material_ctx.enabled = true;

    return true;
}


/*! Sample index is not required since we use the absolute frame time instead.

    Called from zpRender::_validate(for_real = true).

*/
/*virtual*/
void
RenderContext::validateShutterScenes(bool for_real)
{
    //std::cout << "    ------------------------------------------------------" << std::endl;
    //std::cout << "    RenderContext()::validateShutterScene():" << std::endl;
    destroyRayMaterials();

    object_material_ctxs.clear();
    light_material_ctxs.clear();
    texture_bbox_map.clear(); // this gets filled in getTextureRequests()

    render_bbox.clear();
    render_region.clear();
    texture_channels  = DD::Image::Mask_None; // if not Mask_None after validate() there's textures to request()
    material_channels = DD::Image::Mask_None;
    shadow_channels   = DD::Image::Mask_None;

    camera_hash.reset();
    geometry_hash.reset();
    material_hash.reset();
    lighting_hash.reset();
    hash.reset();

    const uint32_t nShutterSamples = numShutterSamples();

    // The motion times for lights are all the same and match the shutter's:
    Fsr::DoubleList light_motion_times(nShutterSamples);

    //-------------------------------------------------------
    // Get light motion times and build legacy camera vectors
    //
    // TODO: do we need to build these vectors anymore...? The RayShaders
    // certainly don't need the camera vectors, but probably legacy shaders
    // like Project3D still use these for the view vector...
    //
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        const ShutterSceneRef& sref = shutter_scenerefs[j];
        zpr::Scene* scene = sref.scene;
#if DEBUG
        assert(scene);
#endif
        // All LightOps share the same motion times which are the
        // renderer's shutter samples:
        light_motion_times[j] = sref.frame;

        //std::cout << "      camera " << scene->camera->matrix() << std::endl;
        if (scene->camera)
        {
            const DD::Image::Matrix4& m = scene->camera->matrix();
            scene->cam_vectors.p.set(m.a03, m.a13, m.a23); // set the origin
            scene->cam_vectors.x.set(m.a00, m.a10, m.a20); // X axis
            scene->cam_vectors.y.set(m.a01, m.a11, m.a21); // Y axis
            scene->cam_vectors.z.set(m.a02, m.a12, m.a22); // Z axis
            scene->cam_vectors.x.normalize();
            scene->cam_vectors.y.normalize();
            scene->cam_vectors.z.normalize();
            m.append(camera_hash);
        }
        else
        {
            // No camera yet, clear the vectors:
            scene->cam_vectors.p.set(0, 0, 0); // set the origin
            scene->cam_vectors.x.set(0, 0, 0); // X axis
            scene->cam_vectors.y.set(0, 0, 0); // Y axis
            scene->cam_vectors.z.set(0, 0, 0); // Z axis
        }

        // Null the object matrix:
        //transforms_.set_object_matrix(DD::Image::Matrix4::identity());
    }


    zpr::Scene* scene0 = shutter_scenerefs[0].scene;

    const uint32_t nObjects = scene0->objects();
    if (nObjects > 0)
    {
        //-------------------------------------------------------
        // Validate object bboxes
        //
        object_material_ctxs.resize(nObjects);

        ValidateThreadContext validate_ctx(this, nObjects);

        uint32_t num_threads = DD::Image::Thread::numCPUs;
        if (nObjects < num_threads)
            num_threads = nObjects;
        if (num_threads <= 1)
        {
            // Pass 0 for num_threads so object loop knows it's not multi-threaded:
            ValidateThreadContext::thread_proc_cb(0/*thread_index*/, 0/*num_threads*/, &validate_ctx); // just do one
        }
        else
        {
            // Spawn multiple threads (minus one for this thread to execute,) then wait for them to finish:
            DD::Image::Thread::spawn(ValidateThreadContext::thread_proc_cb, num_threads-1, &validate_ctx);
            // This thread handles the last one:
            ValidateThreadContext::thread_proc_cb(num_threads-1/*thread_index*/, num_threads/*num_threads*/, &validate_ctx); // just do one
            //
            DD::Image::Thread::wait(&validate_ctx);
        }

        // Combine all objects to build global hashes and bboxes:
        for (uint32_t j=0; j < nObjects; ++j)
        {
            const ObjectState& obj_state = validate_ctx.obj_states[j];
            if (obj_state.bbox.empty())
                continue; // not renderable

            const MaterialContext& material_ctx = object_material_ctxs[j];
            if (!material_ctx.enabled)
                continue; // not renderable

            if (obj_state.created_ray_material != NULL)
            {
                active_ray_materials.push_back(obj_state.created_ray_material); // manages the allocated RayMaterial
            }

            geometry_hash.append(obj_state.geometry_hash);
            material_hash.append(material_ctx.hash);

            if (obj_state.screen_bbox.x() >= render_format->width()  ||
                obj_state.screen_bbox.y() >= render_format->height() ||
                obj_state.screen_bbox.r() <= 0 ||
                obj_state.screen_bbox.t() <= 0)
            {
                // skip it
            }
            else
            {
                render_bbox.expand(obj_state.bbox); // expand the Scene's 3D bbox
                render_region.expand(Fsr::Box2i(obj_state.screen_bbox));
            }

            texture_channels  += material_ctx.texture_channels;
            material_channels += material_ctx.output_channels;
            shadow_channels   += material_ctx.shadow_channels;
        }
    }

    const uint32_t nLights = (uint32_t)scene0->lights.size();
    if (direct_lighting_enabled && nLights > 0)
    {
        //-------------------------------------------------------
        // Validate lights
        // Consider lights that are LightVolumes as geometry and find their bboxes.
        //
        // Currently this routine assumes all lights are LightOps in a DD::Image::Scene.
        // TODO: also check for Fuser Lights in the GeometryList.
        //
        // TODO: this is not threaded since there's typically very few lights
        //  in the Scene, but this may not be true when Fuser Lights are read
        //  in from large USD scenes...
        //
        Fsr::Mat4dList light_motion_xforms;
        light_motion_xforms.reserve(nShutterSamples);

        light_material_ctxs.resize(nLights);

        DD::Image::Hash lt_hash;
        DD::Image::Box3 lt_bbox;
        DD::Image::Box  lt_screen_bbox;
        for (uint32_t ltindex=0; ltindex < nLights; ++ltindex)
        {
#if DEBUG
            assert(scene0);
            assert(scene0->lights[ltindex]);
            assert(scene0->lights[ltindex]->light());
#endif
            DD::Image::LightOp* light = scene0->lights[ltindex]->light();
            light->validate(for_real);

            // Get the motion xforms for all shutter samples, verifying that
            // the light type still matches at each one (which it always should.)
            light_motion_xforms.clear();
            light_motion_xforms.push_back(Fsr::Mat4d(light->matrix()));
            for (uint32_t j=1; j < nShutterSamples; ++j)
            {
                // Try to match the light type in the next scenes.
                // if we can't we copy the xform from the previous sample:
                zpr::Scene* scene1 = shutter_scenerefs[j].scene;
                if (!scene1->lights[ltindex])
                {
                    light_motion_xforms.push_back(light_motion_xforms[j-1]);
                    continue;
                }

                // Verify that the next light is from the same node and has same prim type:
                DD::Image::LightOp* light1 = scene1->lights[ltindex]->light();
#if DEBUG
                assert(light1);
#endif
                if (light->node() != light1->node())
                {
                    std::cerr << light->node_name() << ": light prim type or index mismatch!" << std::endl;
                    light_motion_xforms.push_back(light_motion_xforms[j-1]);
                }
                else
                {
                    light_motion_xforms.push_back(Fsr::Mat4d(light1->matrix()));
                }
            }

            LightMaterial*  lt_created_material = NULL;
            if (!buildLightMaterial(ltindex,
                                    light_motion_times,
                                    light_motion_xforms,
                                    lt_hash,
                                    lt_created_material))
                continue; // disabled, skip it

            if (lt_created_material)
            {
                // Add to list of all allocated RayMaterials:
                active_ray_materials.push_back(lt_created_material); // manages the allocated RayMaterial

                // And add a reference in the master light list:
                master_light_materials.push_back(lt_created_material);


                if (atmospheric_lighting_enabled)
                {
                    lt_bbox = lt_created_material->getLightVolumeBbox().asDDImage();
                    if (!lt_bbox.empty())
                    {

                        lt_screen_bbox = max_format_bbox; // default to max

// TODO: This should use the code that manages the camera projections so that any lens projection can be supported
// TODO: change this to Fuser math classes
                        if (scene0->camera && (render_projection == CAMERA_PROJECTION_PERSPECTIVE ||
                                               render_projection == CAMERA_PROJECTION_ORTHOGRAPHIC))
                        {
                            // Check if camera is inside the object's bbox as we can't project a bbox
                            // that's surrounding the camera:
                            if (!lt_bbox.inside(scene0->cam_vectors.p))
                            {
                                // Project the object's bbox into screen space:
                                lt_bbox.project(scene0->matrix(WORLD_TO_SCREEN), lt_screen_bbox);
                                lt_screen_bbox.intersect(max_format_bbox);
                            }
                        }

                        if (lt_screen_bbox.x() >= render_format->width()  ||
                            lt_screen_bbox.y() >= render_format->height() ||
                            lt_screen_bbox.r() <= 0 ||
                            lt_screen_bbox.t() <= 0)
                        {
                            // outside format, skip it
                        }
                        else
                        {
                            render_bbox.expand(lt_bbox); // expand the Scene's 3D bbox
                            render_region.expand(Fsr::Box2i(lt_screen_bbox));
                        }

                    }
                }
            }

            lighting_hash.append(lt_hash);

            // This MaterialContext was filled in by buildLightMaterial() above:
            const MaterialContext& material_ctx = light_material_ctxs[ltindex];
            texture_channels  += material_ctx.texture_channels;
            material_channels += material_ctx.output_channels;
            shadow_channels   += material_ctx.shadow_channels;
        }

    }
    //std::cout << "      render_region" << render_region << std::endl;
    //std::cout << "  texture_channels=" << texture_channels << std::endl;
    //std::cout << " material_channels=" << material_channels << std::endl;
    //std::cout << "       camera_hash=" << std::hex << camera_hash.value() << std::dec << std::endl;
    //std::cout << "     geometry_hash=" << std::hex << geometry_hash.value() << std::dec << std::endl;
    //std::cout << "     material_hash=" << std::hex << material_hash.value() << std::dec << std::endl;
    //std::cout << "     lighting_hash=" << std::hex << lighting_hash.value() << std::dec << std::endl;

}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
bool
RenderContext::requestObject(int32_t                      obj,
                             const DD::Image::ChannelSet& request_channels,
                             int                          request_count,
                             DD::Image::Iop*&             obj_material,
                             DD::Image::Box&              obj_material_bbox)
{
    //std::cout << "  RenderContext(" << this << ")::requestObject(): obj=" << obj;
    //std::cout << ", request_channels=" << request_channels << ", request_count=" << request_count << std::endl;

    // Only do requests on shutter_open scene:
    zpr::Scene* scene0 = shutter_scenerefs[0].scene;

    // Something broke in 7.0v1 that is not letting the materials request properly,
    // so we're re-implementing the whole thing here:
    //
    DD::Image::GeoInfo& info0 = scene0->object(obj);

    obj_material = info0.material;
    obj_material_bbox.clear();

    // Don't bother if no material or we're not rendering the object:
    if (!obj_material || info0.render_mode == DD::Image::RENDER_OFF)
        return false;

    // Default material bbox to Iop full output bbox:
    const DD::Image::Box& iop_bbox = obj_material->info();
    obj_material_bbox = iop_bbox;

#if 0
// TODO: skip finding the min/max uv range...
    if (scene0->camera && render_projection != CAMERA_PROJECTION_UV)
    {
        // Find min/max uvs for every material which limits the amount of requested
        // texture cache when only a small section of an object is visible to camera:
        DD::Image::MatrixArray tmp_transforms = transforms_;
        tmp_transforms.set_object_matrix(info0.matrix);

        DD::Image::PrimitiveContext tmp_ptx;
        DD::Image::VArray tmp_varray;

        Fsr::Box2f uv_range(std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity());

        const DD::Image::Primitive** prim_array = info0.primitive_array();
        const uint32_t nPrims = info0.primitives();
        for (uint32_t j=0; j < nPrims; ++j)
        {
            const DD::Image::Primitive* p = *prim_array++;

            tmp_ptx.set_geoinfo(&info0, /*mb*/0);
            tmp_ptx.set_transforms(&tmp_transforms, /*mb*/0);
            tmp_ptx.setPrimitive(const_cast<DD::Image::Primitive*>(p));
            tmp_ptx.setPrimitiveIndex(j);
#if 1
            // Nasty hack to get around private var - relies on C++
            // var packing to be sequential:
            uint32_t* face_clipmask = reinterpret_cast<uint32_t*>(const_cast<DD::Image::Box3*>(&tmp_ptx.face_uv_bbox() + 1));
            *face_clipmask = 0x00;
#else
            tmp_ptx.face_clipmask_ = 0x00;
#endif

            uint32_t tmp_face_vertices[1000];
            const uint32_t nFaces = p->faces();
            for (uint32_t f=0; f < nFaces; ++f)
            {
                const uint32_t nVerts = p->face_vertices(f);
                p->get_face_vertices(f, tmp_face_vertices);

                // Copying the prim context keeps the clipmask & uv_bbox empty
                // since we don't have public access to them...sigh:
                DD::Image::PrimitiveContext tmp_ptx1 = tmp_ptx;

                // Call the vertex shader for each vertex:
                for (uint32_t v=0; v < nVerts; ++v)
                    p->vertex_shader(tmp_face_vertices[v], this, &tmp_ptx, tmp_varray);

                // If all the face verts are clipped, skip those uvs:
                if      (tmp_ptx.face_clipmask() == 0x01) continue; // Right
                else if (tmp_ptx.face_clipmask() == 0x02) continue; // Left
                else if (tmp_ptx.face_clipmask() == 0x04) continue; // Top
                else if (tmp_ptx.face_clipmask() == 0x08) continue; // Bottom
                else if (tmp_ptx.face_clipmask() == 0x10) continue; // Near

                uv_range.min.x = std::min(uv_range.min.x, tmp_ptx.face_uv_bbox().x());
                uv_range.min.y = std::min(uv_range.min.y, tmp_ptx.face_uv_bbox().y());
                uv_range.max.x = std::max(uv_range.max.x, tmp_ptx.face_uv_bbox().r());
                uv_range.max.y = std::max(uv_range.max.y, tmp_ptx.face_uv_bbox().t());
            }
        }
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "    " << obj << ": uv" << uv_range << std::endl;
        //}

        if (uv_range.isEmpty())
        {
            const DD::Image::Format& f = obj_material->format();
            const float fX = float(f.x());
            const float fY = float(f.y());
            const float fW = float(f.w());
            const float fH = float(f.h());
            const int tX = int(floorf((uv_range.min.x*fW + fX) + 0.01f));
            const int tY = int(floorf((uv_range.min.y*fH + fY) + 0.01f));
            const int tR = int(floorf((uv_range.max.x*fW + fX) + 0.51f));
            const int tT = int(floorf((uv_range.max.y*fH + fY) + 0.51f));
            obj_material_bbox.set(clamp(tX, iop_bbox.x(), iop_bbox.r()),
                                  clamp(tY, iop_bbox.y(), iop_bbox.t()),
                                  clamp(tR, iop_bbox.x(), iop_bbox.r())+1,
                                  clamp(tT, iop_bbox.y(), iop_bbox.t())+1);

        }
    }
#endif

    //{
    //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
    //std::cout << "    " << obj << ": obj_material " << obj_material;
    //std::cout << " channels=" << obj_material->requested_channels();
    //std::cout << ", obj_material_bbox" << Fsr::Box2i(obj_material_bbox);
    //std::cout << ", iop_bbox=" << Fsr::Box2i(iop_bbox);
    //std::cout << std::endl;
    //}

    return true; // object material valid
}


/*!
*/
struct RequestThreadContext
{
    RenderContext*        rtx;
    DD::Image::ChannelSet request_channels;
    int32_t               request_count;
    //
    AtomicCount32         do_obj;
    DD::Image::Lock       lock;

    //!
    RequestThreadContext(RenderContext*               _rtx,
                         const DD::Image::ChannelSet& _request_channels,
                         int                          _request_count) :
        rtx(_rtx),
        request_channels(_request_channels),
        request_count(_request_count),
        do_obj(0)
    {
        //
    }

    /*! DD::Image::Thread spawn callback function to iterate through the object list.
    */
    static void thread_proc_cb(unsigned thread_index,
                               unsigned num_threads,
                               void*    p)
    {
        RequestThreadContext* ctx = reinterpret_cast<RequestThreadContext*>(p);
        assert(ctx && ctx->rtx);
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "thread_proc_cb(" << std::hex << DD::Image::Thread::GetThreadId() << std::dec << ")";
        //std::cout << " thread_index=" << thread_index << ", num_threads=" << num_threads;
        //std::cout << std::endl;
        //}

        while (1)
        {
            const int32_t obj = ctx->do_obj++; // get object to process and atomic increment
            if (obj >= (int32_t)ctx->rtx->object_material_ctxs.size())
                break;

            DD::Image::Iop* obj_material = NULL;
            DD::Image::Box  obj_material_bbox;
            if (ctx->rtx->requestObject(obj,
                                        ctx->request_channels,
                                        ctx->request_count,
                                        obj_material,
                                        obj_material_bbox))
            {
                assert(obj_material); // shouldn't happen...

                // Renderable object, update global hashes and bboxes:
                if (num_threads > 0)
                {
                    ctx->lock.lock();
                    ctx->rtx->texture_bbox_map[obj_material] = obj_material_bbox;
                    ctx->lock.unlock();
                }
                else
                    ctx->rtx->texture_bbox_map[obj_material] = obj_material_bbox;
            }
            //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
            //std::cout << "      " << obj << ": thread_index=" << thread_index << ", num_threads=" << num_threads << std::endl;
            //std::cout << "      " << obj << ": obj_material" << obj_material << std::endl;
            //std::cout << "      " << obj << ": obj_material_bbox[" << Fsr::Box2i(obj_material_bbox) << std::endl;
        }
        //{
        //static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly
        //std::cout << "  thread_proc_cb(" << std::hex << DD::Image::Thread::GetThreadId() << std::dec << ")";
        //std::cout << " DONE" << std::endl;
        //}

    }
};


/*! Return false if light disabled.
*/
bool
RenderContext::requestLight(int32_t                      ltindex,
                            const DD::Image::ChannelSet& request_channels,
                            int                          request_count)
{
    // Only do requests on shutter_open scene:
    zpr::Scene* scene0 = shutter_scenerefs[0].scene;
#if DEBUG
    assert(ltindex < (int32_t)scene0->lights.size());
#endif
    const DD::Image::LightContext* ltx = scene0->lights[ltindex];
#if DEBUG
    assert(ltx);
    assert(ltx->light());
#endif

    DD::Image::LightOp* light = ltx->light();
    if (light->node_disabled())
        return false;

    light->request(request_channels, request_count);

    return true;
}


/*!
*/
void
RenderContext::doTextureRequests(const DD::Image::ChannelSet& request_channels,
                                 int                          request_count)
{
    //std::cout << "  RenderContext(" << this << ")::doTextureRequests(): request_channels=" << request_channels << ", request_count=" << request_count << std::endl;

    // Only do requests on shutter_open scene:
    zpr::Scene* scene0 = shutter_scenerefs[0].scene;

    const uint32_t nObjects = scene0->objects();

    // Only do the requests if there's channels being published from textures:
    if (texture_channels != DD::Image::Mask_None)
    {
        if (nObjects > 0 && texture_bbox_map.empty())
        {
            // Get the texture map UV request ranges from all object:
            RequestThreadContext request_ctx(this, request_channels, request_count);

            uint32_t num_threads = DD::Image::Thread::numCPUs;
            if (nObjects < num_threads)
                num_threads = nObjects;
            if (num_threads <= 1)
            {
                // Pass 0 for num_threads so object loop knows it's not multi-threaded:
                RequestThreadContext::thread_proc_cb(0/*thread_index*/, 0/*num_threads*/, &request_ctx); // just do one
            }
            else
            {
                // Spawn multiple threads (minus one for this thread to execute,) then wait for them to finish:
                DD::Image::Thread::spawn(RequestThreadContext::thread_proc_cb, num_threads-1, &request_ctx);
                // This thread handles the last one:
                RequestThreadContext::thread_proc_cb(num_threads-1/*thread_index*/, num_threads/*num_threads*/, &request_ctx); // just do one
                //
                DD::Image::Thread::wait(&request_ctx);
            }

        }

        // Call request() on each unique material.
        for (TextureBBoxMap::const_iterator it=texture_bbox_map.begin(); it != texture_bbox_map.end(); ++it)
        {
            // Atm this also causes SurfaceMaterialOp to do requests on their ColorMapKnobs
            it->first->request(it->second, request_channels, request_count);
        }

#if 0
// TODO: this is not required atm as each SurfaceMaterialOp does requests of its local ColorMapKnobs,
//   which happens in the above TextureBBoxMap request loop
        // Call request() on each unique ColorMapKnob:
        for (uint32_t j=0; j < nObjects; ++j)
        {
            const MaterialContext& material_ctx = object_material_ctxs[j];
            const uint32_t nMaps = (uint32_t)material_ctx.colormapknob_list.size();
            for (uint32_t i=0; i < nMaps; ++i)
            {
                const ColorMapKnob* map = material_ctx.colormapknob_list[i];
                if (map && map->getChannels() != DD::Image::Mask_None)
                    map->requestColorMap(request_count);
            }
        }
#endif

    }


    const uint32_t nLights  = (uint32_t)scene0->lights.size();
    if (nLights)
    {
        // This should be a combined mask from all lights in the scene...:
        DD::Image::ChannelSet request_light_channels(DD::Image::Mask_RGB);
        request_light_channels += DD::Image::Mask_Alpha;    // always need transparency - unless we have a switch...

        for (uint32_t i=0; i < nLights; ++i)
        {
            requestLight(i, request_light_channels, request_count);
        }
    }

}


//-------------------------------------------------------------------------


/*! Create RawGeneralTile objects for all textures in the materials list.

    Per-pixel texture sampling calling the built-in Iop::sample() methods has become
    extremely slow, so we create RawGeneralTile for all used textures in the
    MaterialContext and pass them down to the samplers in the shaders.
*/
void
updateSamplerMap(std::vector<MaterialContext>& material_ctx_list,
                 Texture2dSamplerMap&          texture_sampler_map)
{
    const uint32_t nMaterials = (uint32_t)material_ctx_list.size();
    for (uint32_t j=0; j < nMaterials; ++j)
    {
        const MaterialContext& material_ctx = material_ctx_list[j];

        const uint32_t nBindings = (uint32_t)material_ctx.texture_bindings.size();
        for (uint32_t i=0; i < nBindings; ++i)
        {
            const InputBinding* binding = material_ctx.texture_bindings[i];
            assert(binding);
            if (binding && binding->getNumChannels() > 0)
            {
                DD::Image::Iop* iop = binding->asTextureIop();
                if (iop)
                {
                    // Request entire texture map region and channels:
                    const DD::Image::Box& b = iop->info();
                    iop->request(b.x(), b.y(), b.r(), b.t(), binding->getChannels(), 1/*count*/);

                    // Only add unique & valid Iop samplers:
                    if (texture_sampler_map.find(iop) == texture_sampler_map.end())
                    {
                        Texture2dSampler* sampler = new Texture2dSampler(iop, binding->getChannels());
                        texture_sampler_map[iop] = sampler;
                    }
                }
            }
        }
    }

}


/*! Per-pixel texture sampling calling the built-in Iop::sample() methods has become
    extremely slow, so we create RawGeneralTile for all used textures in the scene
    and pass them down to the samples in the shaders.
*/
void
RenderContext::updateTextureSamplerMap()
{
    //std::cout << "  RenderContext(" << this << ")::updateTextureSamplerMap()" << std::endl;

    updateSamplerMap(object_material_ctxs, texture_sampler_map);
    updateSamplerMap(light_material_ctxs,  texture_sampler_map);
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Build the LightVolume Bvh

    Called from zpRender::generate_primitives().

    LightMaterials will only be built if atmospheric_lighting_enabled
    is true or there's created RayMaterials that may access LightMaterials
    during surface shading.
*/
void
RenderContext::buildLightVolumeBvh()
{
    // Go through each light MaterialContext and see if the active
    // LightMaterial's assigned LightShader can construct a LightVolume:

    const size_t nLightMaterials = light_material_ctxs.size();
    //std::cout << "----------------------------------------------------------" << std::endl;
    //std::cout << "zpr::buildLightVolumeBvh(): nLightMaterials=" << nLightMaterials << std::endl;

    if (!atmospheric_lighting_enabled || nLightMaterials == 0)
        return; // no light volumes

    zpr::Scene* scene0 = shutter_scenerefs[0].scene;
#if DEBUG
    assert(scene0);
#endif

    std::vector<zpr::ObjectContextRef> ltvref_list; // for building Light Volume Bvh
    ltvref_list.reserve(nLightMaterials);

    // Build LightVolumes by calling the LightMaterial method:
    for (uint32_t j=0; j < nLightMaterials; ++j)
    {
        if (m_parent->aborted())
            return;  // bail quickly on user-interrupt

        MaterialContext& material_ctx = light_material_ctxs[j];
        if (!material_ctx.enabled || !material_ctx.raymaterial)
            continue; // no assigned or active LightMaterial, skip

        LightMaterial* lt_material = static_cast<LightMaterial*>(material_ctx.raymaterial);
        LightVolume*   lt_volume   = lt_material->createLightVolume(&material_ctx);
        //std::cout << "  " << j << ": lt_volume=" << lt_volume << ", material_ctx=" << &material_ctx << std::endl;
        if (lt_volume)
        {
            // Assign the volume shader:
            //lt_material->m_light_volume_shader = lt_volume;
            //lt_material->m_light_volume_bbox

            // We don't really need this anymore as we could have a
            // single Bvh contain the Traceable volume prims vs. the
            // outer Context Bvh which is used to call back to zpRender
            // for the light volume creation handlers.
            LightVolumeContext* otx = new LightVolumeContext(lt_material);
            this->light_context.push_back(otx);

            // Only need one concatenated light volume sample (deprecate!):
            otx->addLightVolumeSample(scene0, (uint32_t)material_ctx.scene_index);

            otx->bbox = lt_material->getLightVolumeBbox();

            otx->hash.reset();
            //otx->hash.append(light->hash());
            //vol_bbox0.append(otx->hash);
            // Force it to change every render pass:
            //otx->hash.append(render_version);

            // Build the SurfaceContext and the volume prim:
            SurfaceContext* sftx    = otx->addSurface();
            sftx->handler           = NULL; // not required
            sftx->parent_object_ctx = NULL; // not required??
            sftx->obj_index         = (uint32_t)material_ctx.scene_index;
            sftx->prim_index        = -1; // prim_index not needed

            // Build the pointers to/from material and surface contexts:
            material_ctx.surface_ctx = sftx;
            sftx->material_ctx = &material_ctx;

            otx->addPrim(lt_volume);

            ltvref_list.push_back(zpr::ObjectContextRef(otx, otx->bbox));
            //std::cout << "    ltvol[" << otx << "]: bbox=" << otx->bbox << std::endl;

            // Stop the ObjectContext from trying to expand itself upon ray intersection:
            otx->status = SURFACE_DICED;
        }
    }


    // Build the primary intersection test BVH, which is simply the bboxes of all the ObjectContexts:
    if (ltvref_list.size() > 0)
    {
        lights_bvh.build(ltvref_list, 1/*max_objects_per_leaf*/);
        objects_bvh.setName("lights_bvh");
        objects_bvh.setGlobalOrigin(Fsr::Vec3d(0,0,0));
    }
    //std::cout << "    lights_bvh" << lights_bvh.bbox() << ", depth=" << lights_bvh.maxNodeDepth() << std::endl;

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


/*! Update the lighting scenes light and camera vectors.
    For legacy shading only.
*/
void
RenderContext::updateLightingSceneVectorsTo(const Fsr::RayContext& cameraRtx,
                                            uint32_t               shutter_step,
                                            float                  shutter_step_t,
                                            ThreadContext&         ttx) const
{
#if DEBUG
    assert(shutter_step+1 < shutter_scenerefs.size());
#endif


    zpr::Scene& master_lighting_scene = ttx.masterLightingScene();

    // Update the master scene's camera vectors from the current view ray:
    master_lighting_scene.cam_vectors.p = cameraRtx.origin.asDDImage();
    master_lighting_scene.cam_vectors.z = cameraRtx.dir().asDDImage();
    // TODO: do we need accurate xy camera vectors...? I think really only
    // position and possibly z-axis are ever used:
    //master_lighting_scene.cam_vectors.x = cameraRtx.origin.asDDImage();
    //master_lighting_scene.cam_vectors.y = cameraRtx.origin.asDDImage();


    zpr::Scene* scene0 = input_scenes[shutter_scenerefs[shutter_step  ].op_input_index];
    zpr::Scene* scene1 = input_scenes[shutter_scenerefs[shutter_step+1].op_input_index];
#if DEBUG
    assert(scene0);
    assert(scene1);
#endif

    // Update the light vectors in the master lighting scene, then copy them
    // to the per-object scenes:
    const size_t nMasterLights = master_lighting_scene.lights.size();
#if DEBUG
    assert(nMasterLights == scene0->lights.size());
    assert(nMasterLights == scene1->lights.size());
#endif
    for (size_t j=0; j < nMasterLights; ++j)
    {
        // Lighting scenes always contain RayLightContexts (vs. the source Scenes
        // filled in by the input GeoOp):
#if DEBUG
        assert(RayLightContext::isRayLightContext(master_lighting_scene.lights[j]) != NULL);
#endif
        RayLightContext* rltx = static_cast<RayLightContext*>(master_lighting_scene.lights[j]);
#if DEBUG
        assert(rltx->ltindex < (int32_t)scene0->lights.size());
        assert(rltx->ltindex < (int32_t)scene1->lights.size());
#endif

        DD::Image::LightContext* ltx0 = scene0->lights[rltx->ltindex];
        DD::Image::LightContext* ltx1 = scene1->lights[rltx->ltindex];
#if DEBUG
        assert(ltx0);
        assert(ltx1);
#endif
        // Shift LightContext Axis in time:
        interpolateDDImageAxis(ltx0->vectors(),
                               ltx1->vectors(),
                               shutter_step_t,
                               const_cast<DD::Image::Axis&>(rltx->vectors()));
    }


    // Copy the interpolated vectors to per-object lighting scenes:
    LightingSceneList& per_object_lighting_scenes = ttx.perObjectLightingSceneList();
    const size_t nObjects = per_object_lighting_scenes.size();
    for (size_t j=0; j < nObjects; ++j)
    {
        zpr::Scene* ltscene = per_object_lighting_scenes[j];

        // Copy camera vectors:
        ltscene->cam_vectors = master_lighting_scene.cam_vectors;

        // Copy light vectors:
        const size_t nObjectLights = ltscene->lights.size();
        for (size_t i=0; i < nObjectLights; ++i)
        {
            // Lighting scenes always contain RayLightContexts (vs. the source Scenes
            // filled in by the input GeoOp):
#if DEBUG
            assert(RayLightContext::isRayLightContext(ltscene->lights[i]) != NULL);
#endif
            RayLightContext* rltx = static_cast<RayLightContext*>(ltscene->lights[i]);
#if DEBUG
            assert(rltx->ltindex < (int32_t)master_lighting_scene.lights.size());
#endif

            const_cast<DD::Image::Axis&>(rltx->vectors()) = rltx->vectors();
        }
    }
}


//-------------------------------------------------------------------------


//! Also copies LightContext contents.
RayLightContext::RayLightContext(ThreadContext*           _ttx,
                                 int32_t                  _ltindex,
                                 zpr::LightMaterial*      _light_material,
                                 DD::Image::LightContext* _ltx) :
    LightContext(_ltx),
    magic_token(ZPR_MAGIC_TOKEN),
    ttx(_ttx),
    ltindex(_ltindex),
    light_material(_light_material)
{
    m_enabled = (ttx && light_material && light_material->getLightShader());
}


//! Copy constructor
RayLightContext::RayLightContext(const RayLightContext* b) :
    LightContext(b),
    magic_token(ZPR_MAGIC_TOKEN),
    m_enabled(b->m_enabled),
    ttx(b->ttx),
    ltindex(b->ltindex),
    light_material(b->light_material)
{
    //
}


//! Get the current active RayShaderContext.
RayShaderContext&
RayLightContext::getShaderContext() const
{
#if DEBUG
    assert(ttx);
#endif
    return ttx->currentShaderContext();
}


//-------------------------------------------------------------------------


/*! Per-subpixel motionblurred lighting in Nuke's legacy shading system requires a
    thread-safe local copy of a Scene structure that contains the list of LightContext
    pointers that the shaders use to light with.

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
    // If the render versions match then we've already done the work
    // for this ThreadContext:
    if (ttx.getRenderVersion() == render_version)
        return; // already updated

    //std::cout << "RenderContext::updateLightingScenes(): ref_scene=" << ref_scene << std::endl;
#if DEBUG
    assert(ref_scene);
#endif

    ttx.setRenderVersion(render_version);

    // Clear all light info initially:
    ttx.clearLightingScenes();

    // Build the master lighting scene:
    zpr::Scene& master_lighting_scene = ttx.masterLightingScene();
    master_lighting_scene.copyInfo(ref_scene);
    //master_lighting_scene.object_list_.clear();
    //master_lighting_scene.object_transforms_.clear();
    // Clear all lights initially as we'll add them back in below:
    master_lighting_scene.delete_light_context();
    master_lighting_scene.light_transforms.clear();
    master_lighting_scene.light_renderers.clear();
    master_lighting_scene.transparency(true);

    const size_t nLightMaterials = light_material_ctxs.size();
    if (nLightMaterials == 0)
    {
        // No lights, don't need to assign per-object lights:
        ttx.perObjectLightingSceneList().clear();
        return;
    }

    // Copy the light context list out of the scene to make a
    // thread-safe local version.  We'll update these LightContexts
    // at each subpixel with interpolated light vectors:
    const uint32_t nLights = (uint32_t)ref_scene->lights.size();
    //std::cout << "  master_lighting_scene=" << &master_lighting_scene << ", nLights=" << nLights << std::endl;
    master_lighting_scene.lights.reserve(nLights);
    master_lighting_scene.light_transforms.reserve(nLights);
    master_lighting_scene.light_renderers.reserve(nLights);

    for (uint32_t ltindex=0; ltindex < nLights; ++ltindex)
    {
#if DEBUG
        assert(ref_scene->lights[ltindex]);
        assert(ltindex < (uint32_t)ref_scene->light_transforms.size());
        assert(ltindex < (uint32_t)light_material_ctxs.size());
#endif
        LightMaterial* lt_material = static_cast<LightMaterial*>(light_material_ctxs[ltindex].raymaterial);

        RayLightContext* rltx = new RayLightContext(&ttx,
                                                    ltindex,
                                                    lt_material,
                                                    ref_scene->lights[ltindex]);
        rltx->set_scene(&master_lighting_scene);

        master_lighting_scene.lights.push_back(rltx);
        master_lighting_scene.light_transforms.push_back(ref_scene->light_transforms[ltindex]);
        master_lighting_scene.light_renderers.push_back(NULL);
        //========================================================================
        // TODO: enable this code...?:
        // enable light output?
        // if ( ref_scene->lights[i]->light()->getShadowMaskChannel() != Mask_None)
        //    shader_channels += ref_scene->lights[i]->light()->getShadowMaskChannel();
        //========================================================================================
    }


    // Build the per-object lighting scenes:
    const uint32_t nObjects = (uint32_t)object_context.size();
    LightingSceneList& per_object_lighting_scenes = ttx.perObjectLightingSceneList();
    per_object_lighting_scenes.reserve(nObjects);
    for (uint32_t i=0; i < nObjects; ++i)
    {
        GeoInfoContext* otx = object_context[i];
        //
        per_object_lighting_scenes.push_back(new zpr::Scene());
        zpr::Scene* ltscene = per_object_lighting_scenes[i];
        //std::cout << "    " << i << ": ltscene=" << ltscene << std::endl;

        // Copy from reference scene:
        ltscene->copyInfo(ref_scene);
        // Clear all lights initially:
        ltscene->delete_light_context();
        ltscene->light_transforms.clear();
        ltscene->light_renderers.clear();
        ltscene->transparency(true);

        // Get the list of enabled lights from the object context:
        ltscene->lights.reserve(otx->enabled_lights.size());
        ltscene->light_transforms.reserve(otx->enabled_lights.size());
        ltscene->light_renderers.reserve(otx->enabled_lights.size());

        for (std::set<uint32_t>::const_iterator it=otx->enabled_lights.begin(); it != otx->enabled_lights.end(); ++it)
        {
            const uint32_t ltindex = *it;
#if DEBUG
            assert(ref_scene->lights[ltindex]);
            assert(ltindex < (uint32_t)ref_scene->light_transforms.size());
            assert(ltindex < (uint32_t)light_material_ctxs.size());
#endif
            LightMaterial* lt_material = static_cast<LightMaterial*>(light_material_ctxs[ltindex].raymaterial);

            RayLightContext* rltx = new RayLightContext(&ttx,
                                                        ltindex,
                                                        lt_material,
                                                        ref_scene->lights[ltindex]);
            rltx->set_scene(ltscene);
            //std::cout << "  enabled light '" << ltx->light()->node_name() << "'" << std::endl;

            ltscene->lights.push_back(rltx);
            ltscene->light_transforms.push_back(ref_scene->light_transforms[ltindex]);
            ltscene->light_renderers.push_back(NULL);
        }
    }

}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


#if 0
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
    const uint32_t stx_index = rttx->pushShaderContext(NULL/*current*/);

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
#endif


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Constructor requires an Context, thread ID and it's index in the thread list.
*/
ThreadContext::ThreadContext(RenderContext* rtx) :
    m_rtx(rtx),
    m_render_version(-1),
    m_index(0),
    m_ID(0)
{
    assert(m_rtx);

    // Reserve space for 10 ray bounces:
    m_stx_list.reserve(10);

    // Generous reserves for scratch-space lists:
    bvh_leafs.reserve(500);
    I_list.reserve(500);
    I_vol_list.reserve(500);
    index_list.reserve(500);
    vol_intersections.reserve(500);
    uv_intersections.reserve(500);

    texture_color.setChannels(DD::Image::Mask_RGBA);
    texture_color.setInterestRatchet(&textureColorInterestRatchet);
    binding_color.setChannels(DD::Image::Mask_RGBA);
    binding_color.setInterestRatchet(&bindingColorInterestRatchet);
    surface_color.setChannels(DD::Image::Mask_RGB);
    surface_color.setInterestRatchet(&surfaceColorInterestRatchet);
    illum_color.setChannels(DD::Image::Mask_RGB);
    illum_color.setInterestRatchet(&illumColorInterestRatchet);
    volume_color.setChannels(DD::Image::Mask_RGB);
    volume_color.setInterestRatchet(&volumeColorInterestRatchet);

    //std::vector<TextureSamplerContext*> m_tex_samplers;     //!< List of active texture samplers for this thread
    //std::map<DD::Image::Iop*, uint32_t> m_tex_sampler_map;  //!< Texture ID to texture sampler index map

#if 0
    const uint32_t nSamplers = (uint32_t)m_rtx->texture_samplers.size();
    for (uint32_t j=0; j < nSamplers; ++j)
    {
        const TextureSamplerContext& ctx = m_rtx->texture_samplers[j];

        m_tex_samplers[j] = 
    }
#endif
}

/*!
*/
ThreadContext::~ThreadContext()
{
    clearLightingScenes();
}


/*! Add a RayShaderContext to the end of the list, and return its reference.
*/
RayShaderContext&
ThreadContext::pushShaderContext(const RayShaderContext* src_stx)
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
    if (src_stx)
    {
        stx = *src_stx;
        //memcpy(&stx, current, sizeof(RayShaderContext));

        // Update some of the info:
        stx.previous_stx = const_cast<RayShaderContext*>(src_stx);

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
        stx.direct_lighting_enabled   = m_rtx->direct_lighting_enabled;
        stx.indirect_lighting_enabled = m_rtx->indirect_lighting_enabled;
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
    stx.w2l                   = NULL; // World-to-local matrix for current primitive (identity=NULL)
    stx.l2w                   = NULL; // Local-to-world matrix for current primitive (identity=NULL)
    //
    stx.surface_shader        = NULL; // Current surface RayShader to evaluate (NULL if legacy material)
    stx.atmosphere_shader     = NULL; // Current atmospheric VolumeShader being evaluated
    //
    stx.material              = NULL; // Current material on primitive - legacy
    stx.displacement_material = NULL; // Current displacement material on primitive - legacy

    return stx;
}


/*! Add a shader context to the end of the list, copying the
    source context, and returning a reference to it.
*/
RayShaderContext&
ThreadContext::pushShaderContext(const RayShaderContext&          src_stx,
                                 const Fsr::RayContext&           Rtx,
                                 uint32_t                         ray_type,
                                 uint32_t                         sides_mode,
                                 const Fsr::RayDifferentials*     Rdif)
{
    RayShaderContext& stx = pushShaderContext(&src_stx);
    stx.setRayContext(Rtx,
                      ray_type,
                      Rdif);
    stx.sides_mode = sides_mode;

    return stx;
}


/*! Add a shader context to the end of the list, copying the
    source context, and returning a reference to it.
*/
RayShaderContext&
ThreadContext::pushShaderContext(const RayShaderContext&          src_stx,
                                 const Fsr::Vec3d&                Rdir,
                                 double                           tmin,
                                 double                           tmax,
                                 uint32_t                         ray_type,
                                 uint32_t                         sides_mode,
                                 const Fsr::RayDifferentials*     Rdif)
{
    RayShaderContext& stx = pushShaderContext(&src_stx);

    stx.Rtx.setDirAndDistance(Rdir, tmin, tmax);
    stx.Rtx.type_mask = ray_type;
    if (Rdif)
    {
        stx.Rdif = *Rdif;
        stx.use_differentials = true;
    }
    stx.sides_mode = sides_mode;

    return stx;
}


/*! Remove a RayShaderContext from the end of the list, and return the new index.
*/
int
ThreadContext::popShaderContext()
{
    m_stx_list.pop_back();
    return ((int)m_stx_list.size() - 1);
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
    const uint32_t objIndex = otx->motion_objects[0].index;

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
                std::cerr << "zpr::RenderContext::warning - unsupported primitive type '" << prim0->Class() << "'" << std::endl;
                continue;
            }

            SurfaceContext* sftx = NULL;
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
                    sftx = otx->addSurface();
                    sftx->handler        = surface_handler[prim_type];
                    sftx->obj_index      = objIndex;
                    sftx->prim_index     = -1; // prim_index not needed for PolySoup
                    sftx->polysoup_prims = polysoupPrims;

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
                sftx = otx->addSurface();
                sftx->handler    = surface_handler[prim_type];
                sftx->obj_index  = objIndex;
                sftx->prim_index = primIndex;

#ifdef DEBUG_OBJECT_EXPANSION
                std::cout << "  adding SurfaceContext for prim " << primIndex << std::endl;
                ++count;
#endif
            }

            // If there's a valid SurfaceContext configure the material info:
            if (sftx != NULL)
            {
                //------------------------------------------------------------------
                // Update the MaterialContext to point back at the new SurfaceContext.
                // The MaterialContext for this object was previously configured
                // in buildObjectMaterial() which is called by zpRender::_validate():
                //
#if DEBUG
                assert(objIndex < (uint32_t)object_material_ctxs.size());
#endif
                MaterialContext& material_ctx = object_material_ctxs[objIndex];
                material_ctx.surface_ctx = sftx;
                sftx->material_ctx = &material_ctx;


                //------------------------------------------------------------------
                // Configure AOV layer attribute connections:
                //
#if 0
// TODO: finish this! Need to connect the attribs the geometry contain to their AOVLayer outputs
                // Assign the AOVLayers:
                sftx->aov_layers.clear();
                sftx->aov_layers.reserve(50);
                //sftx->aov_layers.push_back(rtx.aov_outputs[0]);
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
    if (atmospheric_lighting_enabled && direct_lighting_enabled && ltctx)
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
            SurfaceContext* sftx = otx->addSurface();
            sftx->handler    = surface_handler[prim_type];
            sftx->obj_index  = lt_index0;
            sftx->prim_index = -1; // prim_index not needed

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
std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox('" << light->node_name() << "'):" << std::endl;
    bbox.clear();
    // Skip it if it's off:
    if (!light || light->node_disabled())
        return UNRECOGNIZED_PRIM;

    // Only create prim if light can illuminate atmosphere:
    DD::Image::Knob* k_light_illum = light->knob("illuminate_atmosphere");
    if (!k_light_illum)
    {
std::cout << " light has no 'illuminate_atmosphere' knob, skipping..." << std::endl;
        return UNRECOGNIZED_PRIM;
    }

    bool can_illuminate_atmosphere = false;
    DD::Image::Hash junk;
    k_light_illum->store(DD::Image::BoolPtr, &can_illuminate_atmosphere, junk, light->outputContext());
    if (!can_illuminate_atmosphere)
    {
std::cout << " light has 'illuminate_atmosphere' turned off." << std::endl;
        return UNRECOGNIZED_PRIM;
    }

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
std::cout << " type=LIGHTCONE_PRIM, bbox" << bbox << std::endl;
        return LIGHTCONE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::ePointLight)
    {
        // LightSphere
        bbox = SphereVolume::getSphereBbox(clamp(light->Near(), 0.0001, std::numeric_limits<double>::infinity()),
                                           clamp(light->Far(),  0.0001, std::numeric_limits<double>::infinity()),
                                           light_xform);
std::cout << " type=LIGHTSPHERE_PRIM, bbox" << bbox << std::endl;
        return LIGHTSPHERE_PRIM;

    }
    else if (light->lightType() == DD::Image::LightOp::eDirectionalLight)
    {
        // LightCylinder

std::cout << " type=LIGHTCYLINDER_PRIM, bbox" << bbox << std::endl;
        return LIGHTCYLINDER_PRIM;

    }
    else
    {
        // Check for ReflectionCard:
        if (strcmp(light->Class(), "ReflectionCard")==0 ||
            strcmp(light->Class(), "AreaLight")==0)
        {
            // LightCard

std::cout << " type=LIGHTCARD_PRIM, bbox" << bbox << std::endl;
            return LIGHTCARD_PRIM; 
        }
    }

std::cout << " UNRECOGNIZED TYPE" << std::endl;
std::cout << "zpr::RenderContext::getVolumeLightTypeAndBbox(): warning, unknown light type, skipping..." << std::endl;

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
        SurfaceContext& sftx = *otx->surface_list[i];
        if (sftx.status == SURFACE_NOT_DICED)
        {
#if DEBUG
            assert(sftx.handler);
#endif
#ifdef DEBUG_OBJECT_EXPANSION
            std::cout << "  dicing surface " << i << " using handler ";
            std::cout << sftx.handler->Class() << "()::generateRenderPrims()";
            std::cout << std::endl;
#endif

            //-------------------------------------------
            sftx.handler->generateRenderPrims(*this, sftx);
            //-------------------------------------------

            sftx.status = SURFACE_DICED;
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
