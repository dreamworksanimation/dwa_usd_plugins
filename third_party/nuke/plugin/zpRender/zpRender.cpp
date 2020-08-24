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

/// @file zpRender.cpp
///
/// @author Jonathan Egstad


#include "zpRender.h"

#include <zprender/Bvh.h>
#include <zprender/ThreadContext.h>
#include <zprender/Scene.h>
#include <zprender/Sampling.h>
#include <zprender/Volume.h>
#include <zprender/RayShader.h>
#include <zprender/RayShaderContext.h>
#include <zprender/RayPerspectiveCamera.h>
#include <zprender/RaySphericalCamera.h>
#include <zprender/RayCylindricalCamera.h>
//#include <zprender/DeepPixelHandler.h>


#include <Fuser/api.h> // for stringSplit, globMatch


#include <DDImage/ARRAY.h>
#include <DDImage/Format.h>
#include <DDImage/GeoOp.h>
#include <DDImage/LightOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/Filter.h>
#include <DDImage/OpTree.h>
#include <DDImage/Row.h>
#include <DDImage/Thread.h>  // for Lock
#include <DDImage/Application.h>


#include <cassert>
#include <sys/time.h>
#include <sstream>


using namespace DD::Image;


namespace zpr {

//-------------------------------------------------------------------------------------


static DD::Image::Op* build(::Node* node) { return new zpRender(node); }
const DD::Image::Op::Description zpRender::description("zpRender", build);


//! Calculate an appropriate global world offset for the scene (usually the
//  camera translation and apply to all scene objects.
//#define APPLY_GLOBAL_OFFSET 1


//----------------------------------------------------------------------------

static DD::Image::Lock my_lock;

static DD::Image::CameraOp default_camera(NULL/*Node*/);


//----------------------------------------------------------------------------
// Surface Handlers:
//----------------------------------------------------------------------------
static DDImagePolysoupHandler       fn_polysoup_handler;        // FN_POLYSOUP_PRIM
static DDImageMeshHandler           fn_mesh_handler;            // FN_MESH_PRIM
static DDImagePolyMeshHandler       fn_polymesh_handler;        // FN_POLYMESH_PRIM
static DDImagePointHandler          fn_point_handler;           // FN_POINT_PRIM
static DDImageParticleSpriteHandler fn_particle_sprite_handler; // FN_PARTICLE_SPRITE_PRIM
//
static FsrNodePrimitiveHandler      fuser_nodeprim_handler;     // FUSER_NODEPRIM
static FsrMeshHandler               fuser_meshprim_handler;     // FUSER_MESHPRIM
static FsrPointsHandler             fuser_pointprim_handler;    // FUSER_POINTPRIM


//----------------------------------------------------------------------------
// Light Volume Handlers:
//----------------------------------------------------------------------------
static ConeHandler      lightcone_handler;  // LIGHTCONE_PRIM
static SphereHandler  lightsphere_handler;  // LIGHTSPHERE_PRIM


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


static const char* const ray_bounces_list[]   = { "0", "1", "2", "3", "4", "5", 0 };
static const char* const global_xform_modes[] = { "off", "cam-open", "manual", 0 };
//static const char* const shutter_modes[]      = { "stochastic", "slice", "offset", 0 };
static const char* const lighting_enable_modes[] = { "auto", "on", "off", 0 };


static const char* aov_unpremult_modes[] =
{
    "coverage",
    "alpha",
    "none",
    0
};

static const char* projection_modes[] =
{
    "perspective",
    //"orthographic",
    //"uv",
    "spherical",
    "cylindrical",
    "render camera",
    0
};


//----------------------------------------------------------------------------


/*!
*/
zpRender::zpRender(::Node* node) :
    DD::Image::Render(node, true/*enable_mip_filter*/),
    rtx(this),
    k_texture_filter_preview(DD::Image::Filter::Impulse, true/*enableMipType*/),
    m_black(DD::Image::Mask_RGBA)
{
#if kDDImageVersionInteger > 90000
    // Disabling the OpTree handling for 9.0+, it's causing unexpected
    // crashing which didn't seem to happen in 6.0-7.0.
    // The RenderScene.h class has this variable being created to handle
    // aborts in the build_handles() method, but it's not clear if zpRender
    // needs this now.
#else
    // This is required to stop the 'Abort/cancel checked with no valid trees' warning...
    m_op_tree = new DD::Image::OpTree(this);
    {
        std::ostringstream oss;
        oss << "zpRender " << m_op_tree->getDebugIndex();
        m_op_tree->setDebugLabel(oss.str());
    }
#endif


    //=======================================================
    // Render base class settings:
    DD::Image::Render::projection_mode_    = DD::Image::CameraOp::LENS_RENDER_CAMERA;
    DD::Image::Render::multisampling_      = SINGLE_SAMPLE;
    DD::Image::Render::samples_            = 1;
    DD::Image::Render::temporal_jitter_    = 0;
    DD::Image::Render::spatial_jitter_     = 0;
    DD::Image::Render::overscanX_          = 0.0;
    DD::Image::Render::overscanY_          = 0.0;
    //DD::Image::Render::overscanFormat_.

    //=======================================================
    // Init views:
    rtx.k_hero_view              = 1;//LFT...
    for (int i=1; i < DD::Image::OutputContext::viewcount(); ++i)
    {
        rtx.k_views.insert(i);
        rtx.render_views.push_back(i);
    }

    // State:
    rtx.k_preview_mode           = true;
    //
    k_shutter_steps_preview      = 0;
    k_shutter_steps              = 1;
    //
    rtx.k_camera_mode            = RenderContext::CAMERA_SEPARATE;
    rtx.k_projection_mode        = PROJECTION_RENDER_CAMERA;
    rtx.num_shutter_steps        = DD::Image::Render::samples_ = k_shutter_steps;
    rtx.k_shutter.setDuration(0.5);
    rtx.k_shutter.setOffset(DD::Image::ShutterControls::eStartOffset);
    rtx.k_shutter.setCustomOffset(0.0);
    rtx.k_shutter_bias           = 0.0;
    k_scene_time_offset          = 0.0;

    k_uv_mode_object_index       = 0;
    k_uv_mode_surface_index      = -1;
    k_uv_mode_tile_index[0] = k_uv_mode_tile_index[1] = 0;

    // Shading
    k_pixel_sample_mode_preview  = RenderContext::SAMPLING_1x1;
    k_pixel_sample_mode          = RenderContext::SAMPLING_5x5;
    //
    rtx.k_pixel_filter           = Filter::Parzen;
    rtx.k_pixel_filter_size[0]   = rtx.k_pixel_filter_size[1] = 1.5f;
    rtx.k_shading_interpolation  = RenderContext::SHADING_SMOOTH;
    rtx.k_spatial_jitter_threshold = 2; // start jittering at 2 or greater pixel samples
    rtx.k_output_bbox_mode       = RenderContext::BBOX_SCENE_SIZE;
    rtx.k_sides_mode             = RenderContext::SIDES_BOTH;

    k_bvh_max_depth              = 1000000;
    k_bvh_max_objects_per_leaf   = 10;

    //=======================================================

    /* Array of handler routine pointers that correspond to the primitive enums
       in RenderContext.h so we don't have to have a bunch of if-elses to call a handler.
    */
    // SURFACE HANDLERS:
    rtx.surface_handler[FN_POLYSOUP_PRIM       ] =        &fn_polysoup_handler;
    rtx.surface_handler[FN_MESH_PRIM           ] =            &fn_mesh_handler;
    rtx.surface_handler[FN_POLYMESH_PRIM       ] =        &fn_polymesh_handler;
    rtx.surface_handler[FN_POINT_PRIM          ] =           &fn_point_handler;
    rtx.surface_handler[FN_PARTICLE_SPRITE_PRIM] = &fn_particle_sprite_handler;
    //
    rtx.surface_handler[FUSER_NODEPRIM  ] =  &fuser_nodeprim_handler;
    rtx.surface_handler[FUSER_MESHPRIM  ] =  &fuser_meshprim_handler;
    rtx.surface_handler[FUSER_POINTPRIM ] = &fuser_pointprim_handler;
    //
    rtx.surface_handler[LIGHTSPHERE_PRIM  ] = &lightsphere_handler;
    rtx.surface_handler[LIGHTCONE_PRIM    ] =   &lightcone_handler;
    /*                 [LIGHTCYLINDER_PRIM] set to null handler in RenderContext ctor */
    /*                 [LIGHTCARD_PRIM    ] set to null handler in RenderContext ctor */

    //=======================================================

    k_shade_subsamples           = true;
    rtx.k_copy_specular          = false;//NUKE-1058 true;
    k_ray_use_camera_near_plane  = true;
    k_ray_near_plane             = std::numeric_limits<double>::epsilon();
    k_ray_use_camera_far_plane   = true;
    k_ray_far_plane              = 100000000.0; // hundred million
    k_ray_single_scatter_samples     = RenderContext::SAMPLING_5x5;
    //
    k_ray_diffuse_samples_preview    = RenderContext::SAMPLING_2x2;
    k_ray_diffuse_samples            = RenderContext::SAMPLING_4x4;
    //
    k_ray_glossy_samples_preview     = RenderContext::SAMPLING_2x2;
    k_ray_glossy_samples             = RenderContext::SAMPLING_4x4;
    //
    k_ray_refraction_samples_preview = RenderContext::SAMPLING_2x2;
    k_ray_refraction_samples         = RenderContext::SAMPLING_4x4;
    //
    rtx.ray_max_depth            = 10;
    rtx.ray_diffuse_max_depth    = 1;
    rtx.ray_glossy_max_depth     = 1;
    rtx.ray_reflection_max_depth = 1;
    rtx.ray_refraction_max_depth = 2; // usually need two hits to get through an object

    rtx.k_show_diagnostics       = RenderContext::DIAG_OFF;
    rtx.k_diagnostics_sample     = 0;

    k_use_direct_lighting            = true;
    k_use_indirect_lighting          = true;
    k_use_atmospheric_lighting       = false;
    k_autolighting_mode              = LIGHTING_ENABLE_AUTO;

    rtx.k_atmosphere_alpha_blending  = true;
    rtx.k_transparency_enabled       = true;
    rtx.k_alpha_threshold            = 0.0001f;
    rtx.k_dof_enabled                = false;
    rtx.k_dof_max_radius             = 0.1f;

    k_shutter_mode               = SHUTTER_STOCHASTIC;

    k_coverage_chan              = channel("mask.coverage");
    k_cutout_channel             = Chan_Mask;

    k_render_mask_channel        = Chan_Black;
    k_render_mask_threshold      = 0.01f;
    k_invert_render_mask         = false;

    k_deep_output_subpixel_masks = true;
    k_spmask_channel[0] = DD::Image::getChannel("spmask.1");
    k_spmask_channel[1] = DD::Image::getChannel("spmask.2");
    k_spmask_channel[2] = DD::Image::getChannel("spmask.3");
    k_deep_combine_threshold     = 0.1;

#ifdef APPLY_GLOBAL_OFFSET
    k_global_xform_mode          = GLOBAL_XFORM_CAM_OPEN;
#else
    k_global_xform_mode          = GLOBAL_XFORM_OFF;
#endif
    k_global_offset.set(0.0, 0.0, 0.0);
    rtx.global_xform.setToIdentity();
    rtx.global_offset.set(0.0f, 0.0f, 0.0f);

    //=======================================================

    k_one_over_Z                 = false;
    k_persp_correct_z            = true;
    k_bg_occlusion               = false;

    k_use_deep                   = false;
    k_render_only                = false;

    //=======================================================

    // Default AOV outputs to off:
    for (uint32_t j=0; j < NUM_AOV_OUTPUTS; ++j)
    {
        char buf[128];
        sprintf(buf, "aov_enable%u", j);
        m_aov_knob_names[j][0] = strdup(buf);
        sprintf(buf, "aov_name%u", j);
        m_aov_knob_names[j][1] = strdup(buf);
        sprintf(buf, "aov_merge_mode%u", j);
        m_aov_knob_names[j][2] = strdup(buf);
        sprintf(buf, "aov_unpremult_mode%u", j);
        m_aov_knob_names[j][3] = strdup(buf);
        sprintf(buf, "aov_channel%u", j);
        m_aov_knob_names[j][4] = strdup(buf);
        // Predefine some standard AOVs:
        switch (j)
        {
            case 0:
                k_aov_name[j] =  "Z";
                k_aov_unpremult[j]  = zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE;
                k_aov_merge_mode[j] = zpr::AOVLayer::AOV_MERGE_MIN;
                k_aov_output[j][0] = Chan_Z;
                k_aov_output[j][1] = Chan_Black;
                k_aov_output[j][2] = Chan_Black;
                k_aov_enable[j] = true;
                break;

            case 1:
                k_aov_name[j] =  "N";
                k_aov_unpremult[j]  = zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE;
                k_aov_merge_mode[j] = zpr::AOVLayer::AOV_MERGE_PREMULT_UNDER;
                for (uint32_t i=0; i < 3; ++i)
                   k_aov_output[j][i] = Chan_Black;
                k_aov_enable[j] = true;
                break;

            case 2:
                k_aov_name[j] =  "P";
                k_aov_unpremult[j]  = zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE;
                k_aov_merge_mode[j] = zpr::AOVLayer::AOV_MERGE_PREMULT_UNDER;
                for (uint32_t i=0; i < 3; ++i)
                   k_aov_output[j][i] = Chan_Black;
                k_aov_enable[j] = true;
                break;

            case 3:
                k_aov_name[j] = "uv";
                k_aov_unpremult[j]  = zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE;
                k_aov_merge_mode[j] = zpr::AOVLayer::AOV_MERGE_PREMULT_UNDER;
                for (uint32_t i=0; i < 3; ++i)
                   k_aov_output[j][i] = Chan_Black;
                k_aov_enable[j] = true;
                break;

            default:
                k_aov_name[j] =   "";
                k_aov_unpremult[j]  = zpr::AOVLayer::AOV_UNPREMULT_BY_COVERAGE;
                k_aov_merge_mode[j] = zpr::AOVLayer::AOV_MERGE_UNDER;
                for (uint32_t i=0; i < 3; ++i)
                   k_aov_output[j][i] = Chan_Black;
                k_aov_enable[j] = true;
                break;
        }
    }

    //=======================================================

    m_render_views_invalid       = true;
    m_pixel_sample_mode          = k_pixel_sample_mode;
    m_ray_diffuse_samples        = k_ray_diffuse_samples;
    m_ray_glossy_samples         = k_ray_glossy_samples;
    m_ray_refraction_samples     = k_ray_refraction_samples;
    m_sampler_set                = 0;

    memset(m_black.chan, 0, sizeof(float)*(DD::Image::Chan_Last+1));
}


/*!
*/
zpRender::~zpRender()
{
    // Delete allocated scenes:
    for (size_t i=0; i < rtx.input_scenes.size(); ++i)
        delete rtx.input_scenes[i];
}


//----------------------------------------------------------------------------


#if kDDImageVersionInteger > 90000
// Disabling the OpTree handling for 9.0+, it's causing unexpected
// crashing which didn't seem to happen in 6.0-7.0.
// The RenderScene.h class has this variable being created to handle
// aborts in the build_handles() method, but it's not clear if zpRender
// needs this now.
#else
/*! */
//zpRender::~zpRender() {
//    // Delete the OpTree object:
//    delete m_op_tree;
//}


/*virtual*/
void
zpRender::detach()
{
    // Node deleted -> clear op tree so errors aren't considered important
    m_op_tree->setNewRoot(NULL);
    Render::detach();
}


/*virtual*/
void
zpRender::attach()
{
    Render::attach();
    // Node created or undeleted -> ensure the op tree points here so errors are considered important
    m_op_tree->setNewRoot(this);
}
#endif


//----------------------------------------------------------------------------


/*! */
/*virtual*/
bool
zpRender::test_input(int            input,
                     DD::Image::Op* op) const
{
    switch (input) {
    case 0: return (dynamic_cast<DD::Image::Iop*>(op)      != NULL);
    case 1: return (dynamic_cast<DD::Image::GeoOp*>(op)    != NULL);
    case 2: return (dynamic_cast<DD::Image::CameraOp*>(op) != NULL);
    }
    return false;
}


/*! */
/*virtual*/
DD::Image::Op*
zpRender::default_input(int input) const
{
    switch (input) {
    case 0: return Iop::default_input(input);
    case 1: return NULL; // GeoOp
    case 2: return NULL; // CameraOp::default_camera() might work
    }
    return NULL;
}


/*! */
/*virtual*/
const char*
zpRender::input_label(int   input,
                      char* buffer) const
{
    switch (input) {
    case 0: return "bg";
    case 1: return "obj/scn";
    case 2: return "cam";
    }
    return buffer;
}


/*! Split geometry input (1) by sample number, and camera input (2) but samples&views.
*/
/*virtual*/
int
zpRender::split_input(int input) const
{
    if (input == 0)
    {
        // BG input - No multisampling needed:
        return 1;
    }
    else if (input == 1)
    {
        // GEO input - Geometry only needs splitting by number of samples:
        return this->samples();
    }
    else if (input == 2)
    {
        // CAMERA input - Camera needs samples*views:
        const_cast<zpRender*>(this)->updateRenderViews();
        //std::cout << "  nCameras=" << this->samples()*rtx.render_views.size() << std::endl;
        return (int)(this->samples()*rtx.render_views.size());
    }
    return 1;
}


#if 0
/*! Update local values that affect the inputs, like the samples count.

    From Op.h:
    Change what is in outputContext(). Nuke calls this for you.
    Subclasses can override this, but they must call the base class with
    exactly the same context. This method is a convenient place to do
    calculations that are needed before any of the following methods work:
    - int split_input(int) const;
    - float uses_input(int) const;
    - const OutputContext& inputContext(int n, int m, OutputContext&) const;
    - Op* defaultInput(int n, const OutputContext&) const;
    The knob values have been stored at this point, but no inputs
    have been created.
*/
/*virtual*/ void
zpRender::setOutputContext(const OutputContext& c)
{
    // Get full-quality or preview-quality values:
    if (!DD::Image::Application::IsGUIActive() || !rtx.k_preview_mode)
    {
        // FULL-QUALITY:
        rtx.num_shutter_steps    = std::min(std::max((int32_t)0, k_shutter_steps), (int32_t)50);
        m_pixel_sample_mode      = k_pixel_sample_mode;
        m_ray_diffuse_samples    = k_ray_diffuse_samples;
        m_ray_glossy_samples     = k_ray_glossy_samples;
        m_ray_refraction_samples = k_ray_refraction_samples;
    }
    else
    {
        // PREVIEW:
        rtx.num_shutter_steps    = std::min(std::max((int32_t)0, k_shutter_steps_preview), (int32_t)50);
        m_pixel_sample_mode      = k_pixel_sample_mode_preview;
        m_ray_diffuse_samples    = k_ray_diffuse_samples_preview;
        m_ray_glossy_samples     = k_ray_glossy_samples_preview;
        m_ray_refraction_samples = k_ray_refraction_samples_preview;
    }

    // Store this now so that it's available in split_input(), append(), etc.
    this->samples_ = rtx.numShutterSamples();

    // Force render views to update:
    m_render_views_invalid = true;
    updateRenderViews();
}
#endif


/*! Changes the time of the inputs for temporal sampling.
    Input 0 is the bg, so it is unaffected.
    Input 1 is the GeoOps, time-shift them, lock the view to the hero
    Input 2 is the camera, time-shift plus split by views

    From Op.h:
    Return the context to use for the input connected to input(n, offset). The
    most common thing to do is to change the frame number.

    The default version returns outputContext().

    You can use \a scratch as a space to construct the context and
    return it.

    This cannot look at input \a n or above, as they have not been
    created yet.  Often though it is useful to look at these inputs, for
    instance to get the frame range to make a time-reversing operator. If
    you want to do this you use node_input() to generate a "likely"
    op. You can examine any data in it that you know will not depend on
    the frame number.
*/
/*virtual*/
const DD::Image::OutputContext&
zpRender::inputContext(int                       input,
                       int                       offset,
                       DD::Image::OutputContext& context) const
{
    // Copy the context contents from this Op:
    context = outputContext();

    // No multisampling for bg input:
    if (input == 0)
        return context;

    // Geometry inputs are offset in time by sample count:
    if (input == 1)
    {
        context.setFrame(getFrameForSample(offset, context.frame()));
        //context.setView(rtx.k_hero_view);
        return context;
    }

    // Camera needs views as well:
    if (input == 2)
    {
        ((zpRender*)this)->updateRenderViews();

        // Offset camera in time:
        context.setFrame(getFrameForSample(offset/(int)rtx.render_views.size(), context.frame()));

        // Get view for this offset:
        context.setView(rtx.render_views[offset%(int)rtx.render_views.size()]);
        //std::cout << "     change input " << input << ", offset " << offset;
        //std::cout << " to sample " << offset/rtx.render_views.size();
        //std::cout << " and view " << offset%rtx.render_views.size() << std::endl;

        return context;
    }

    return context;
}


/*! Calculate the absolute frame from sample number and base frame.
    Sample zero is always base_frame.
*/
double
zpRender::getFrameForSample(uint32_t sample,
                            double   base_frame) const
{
    // Apply the global scene time offset:
    base_frame += k_scene_time_offset;

    const double duration = double(rtx.k_shutter.getDuration());

    // 0 shutter always returns base_frame:
    if (duration < std::numeric_limits<double>::epsilon() || this->samples() <= 1)
        return base_frame;

    const double offset = double(rtx.k_shutter.calcOffset());

    // Simple linear distribution with offset:
    double d = duration / double(this->samples() - 1);
    //std::cout << "  getFrameForSample(" << sample_index << ") base_frame " << base_frame;
    //std::cout << ", offset=" << offset << ", duration=" << duration << ", d=" << d;

    // We need to always keep base_frame at the current GUI frame so that
    // keyframes are set properly, so change the distribution direction
    // depending on offset.  i.e. are we interpolating from base_frame towards
    // shutter open or shutter close?
    double new_frame;
    if (offset < -(duration / 2.0))
        new_frame = base_frame - sample*d; // Offset backward towards shutter open
    else
        new_frame = base_frame + sample*d; // Offset forward towards shutter close

    if (::fabs(new_frame) < std::numeric_limits<double>::epsilon())
        return 0.0;

    return new_frame;
}


//------------------------------------------------------------------------


/*! Returns the camera attached to input 2+(sample*nViews + view).
    This overrides the Render::render_camera implementation as it requires a view argument.
*/
DD::Image::CameraOp*
zpRender::getInputCameraOpForSampleAndView(uint32_t sample,
                                           int32_t  view)
{
    const int input_num = sample*(int)rtx.render_views.size() + view;

    DD::Image::Op* op = Op::input(2, input_num);
    DD::Image::CameraOp* cam = dynamic_cast<DD::Image::CameraOp*>(op);
    if (cam)
        return cam;

#if 0
    std::cerr << "zpRender::warning - camera for sample " << sample << ", view " << view << " (input " << input_num << ")";
    if (op)
    {
        std::cerr << " is invalid (Class='" << op->Class() << "'), ";
    }
    else
    {
        std::cerr << " is null,";
    }
    std::cerr << " using default camera" << std::endl;
#endif

    return DD::Image::CameraOp::default_camera();
}


/*! Returns the GeoOp connected to input 1 for \n sample.
*/
DD::Image::GeoOp*
zpRender::getInputGeoOpForSample(uint32_t sample)
{
    // Objects start at input 1:
    return dynamic_cast<DD::Image::GeoOp*>(DD::Image::Op::input(1 + sample));
}


/*! Update enabled views.
    Strip crap views (<= 0)
*/
void
zpRender::updateRenderViews()
{
    if (!m_render_views_invalid)
        return;

    rtx.render_views.clear();
    for (std::set<int>::iterator i=rtx.k_views.begin(); i != rtx.k_views.end(); ++i)
        if (*i > 0)
            rtx.render_views.push_back(*i);
    if (rtx.render_views.empty())
        rtx.render_views.push_back(rtx.k_hero_view);

    m_render_views_invalid = false;
}


//----------------------------------------------------------------------------


/*! Returns the inverse camera matrix for a particular sample.
*/
/*virtual*/
DD::Image::Matrix4
zpRender::camera_matrix(int sample)
{
    DD::Image::CameraOp* cam = rtx.input_scenes[sample]->camera;
    if (cam)
    {
        cam->validate(true);
        return cam->imatrix();
    }
    DD::Image::Matrix4 m;
    m.makeIdentity();

    return m;
}


/*! Returns the camera projection matrix for a particular sample.
    TODO: this is pointless with other projections, figure out
    a scheme for this.
*/
/*virtual*/
DD::Image::Matrix4
zpRender::projection_matrix(int sample)
{
    const DD::Image::Format& f = info_.format();
    const float W = float(f.width());
    const float H = float(f.height());

    // Determine aperture expansion due to a format with a defined inner image area.
    // Offset and scale the aperture:
    DD::Image::Matrix4 m;
    m.translation((float(f.r() + f.x()) / W) - 1.0f, (float(f.t() + f.y()) / H) - 1.0f);
    m.scale(float(f.w())/W, float(f.w())*float(f.pixel_aspect())/H, 1.0f);

    // TODO: we can't handle non-linear projections in here...
    // Apply linear projection:
    DD::Image::CameraOp* cam = rtx.input_scenes[sample]->camera;
    if (cam)
    {
        cam->validate(true);
        m *= cam->projection(DD::Image::CameraOp::LENS_PERSPECTIVE);
    }
    else
    {
        DD::Image::Matrix4 p;
        p.projection(1.0f, 0.1f, 10000.0f);
        m *= p;
    }

    return m;
}


// Deprecated function:
///*virtual*/ void format_matrix(int sample = 0);

/*! Calculate the transformation from post-projection NDC to pixel
    space. cx,cy are where in the pixel the -1,-1 corner should be.
*/
/*virtual*/
DD::Image::Matrix4
zpRender::getFormatMatrix(float dx,
                          float dy)
{
    DD::Image::Matrix4 m;
    m.makeIdentity();

    // Scale and translate from NDC to format:
    const DD::Image::Format& f = format();
    const float W = float(f.width());
    const float H = float(f.height());
    m.translate(W / 2.0f - dx, H / 2.0f - dy);
    m.scale(W / 2.0f, H / 2.0f, 1.0f);

    return m;
}

//----------------------------------------------------------------------------


#if 0
/*!
    From DD::Image::Op.h:
    //! Enumeration for the use of doAnyHandles().   the values are defined non-consectively so that
    //! that eHandles | eHandlesCooked == eHandlesCooked
    enum HandlesMode {
      eNoHandles        = 0,              //! no handles are needed
      eHandlesUncooked  = 1,              //! handles are needed, but generate_tree does not necessarily need calling
      eHandlesCooked    = 3,              //! handles are needed, and generate_tree needs to be called, to cook out the 
      eHandlesMax       = eHandlesCooked
    };
*/
/*virtual*/
DD::Image::Op::HandlesMode
zpRender::doAnyHandles(DD::Image::ViewerContext* ctx)
{
    //std::cout << "zpRender::doAnyHandles(): transform_mode=" << ctx->transform_mode() << std::endl;
#if 1
    // We have to shift the mode to perspective as Viewer won't call build_handles
    // in 2D mode unless it's set to PERSP....????  Why????
    if (ctx->transform_mode() == DD::Image::VIEWER_2D)
        ctx->transform_mode(DD::Image::VIEWER_PERSP);

#else
    if (ctx->transform_mode() == DD::Image::VIEWER_2D &&
        ((ctx->connected() == DD::Image::SHOW_OBJECT && panel_visible()) ||
         (ctx->connected() == DD::Image::SHOW_PUSHED_OBJECT && pushed())))
    {
        return DD::Image::Op::eHandlesCooked;
    }
    else if (panel_visible())
    {
        return DD::Image::Op::eHandlesCooked;

    }
#endif

    // Fix from Foundry to stop build_handles() from crashing validate():
    DD::Image::Op::HandlesMode handles = DD::Image::Iop::doAnyHandles(ctx);
    if (handles != DD::Image::Op::eNoHandles)
        handles = DD::Image::Op::eHandlesCooked;

    return handles;
}

#else
//================================================================================
//================================================================================
/*!
    From DD::Image::Op.h:
    // Enumeration for the use of doAnyHandles().
    // The values are defined non-consectively so
    // that eHandles | eHandlesCooked == eHandlesCooked
    enum HandlesMode
    {
        eNoHandles        = 0,      //! no handles are needed
        eHandlesUncooked  = 1,      //! handles are needed, but generate_tree does not necessarily need calling
        eHandlesCooked    = 3,      //! handles are needed, and generate_tree needs to be called, to cook out the 
        eHandlesMax       = eHandlesCooked
    };
*/
/*virtual*/
DD::Image::Op::HandlesMode
zpRender::doAnyHandles(DD::Image::ViewerContext* ctx)
{
    //std::cout << "zpRender::doAnyHandles(): transform_mode=" << ctx->transform_mode() << std::endl;
    // Fix from Foundry to stop build_handles() from crashing validate():
    DD::Image::Op::HandlesMode need_handles = DD::Image::Iop::doAnyHandles(ctx);
    if (need_handles != DD::Image::Op::eNoHandles)
        need_handles = DD::Image::Op::eHandlesCooked;

#if 0
    /// we have to shift the mode to perspective as downstream nodes will make
    /// decisions based upon that type.
    if (saved_mode == DD::Image::VIEWER_2D)
        ctx->transform_mode(DD::Image::VIEWER_PERSP);

    /*bool need_handles = */Iop::doAnyHandles(ctx);
    ctx->transform_mode(saved_mode);

    return true;//need_handles;
#else
    if (ctx->transform_mode() == DD::Image::VIEWER_2D &&
        ((ctx->connected() == DD::Image::SHOW_OBJECT && panel_visible()) ||
         (ctx->connected() == DD::Image::SHOW_PUSHED_OBJECT && pushed())))
    {
        return need_handles;
    }
    // Also need handles if panel is open:
    // TODO: Can we detect if we're inside a Gizmo...?
    if (panel_visible())
        return need_handles;
#endif

    return DD::Image::Iop::doAnyHandles(ctx);
}
//================================================================================
//================================================================================
#endif


#if 0
/*!
*/
static void drawBVH(void*                     data,
                    DD::Image::ViewerContext* /*ctx*/)
{
    zpr::RenderContext* rtx = (zpr::RenderContext*)data;
    if (!rtx) 
        return;

    // Draw the object BVHs:
    //rtx.objects_bvh.drawNode();
    //for (uint32_t i=0; i < rtx->object_context.size(); ++i)
    //    rtx->object_context[i].bvh.drawNode();

    //std::cout << "drawBVH " << rtx->bvh_root << std::endl;
}
#endif



/*! Sets 2D viewer to 3D mode to draw any geometry in the input.
    Adds the camera as something that should be snapped to.
*/
/*virtual*/
void
zpRender::build_handles(DD::Image::ViewerContext* ctx)
{
    //std::cout << "zpRender::build_handles(): viewer_mode=" << ctx->viewer_mode() << ", transform_mode=" << ctx->transform_mode() << std::endl;
    const DD::Image::Matrix4 saved_matrix = ctx->modelmatrix;
    const int                saved_mode   = ctx->transform_mode();

    // Viewer appears to call the renderer with VIEWER_PERSP transform mode now,
    // not really sure why.
    // Check how other nodes call it too:
    if (ctx->viewer_mode()    == DD::Image::VIEWER_2D &&
        ctx->transform_mode() == DD::Image::VIEWER_PERSP)
    {
        DD::Image::Render::validate(false);
#ifdef ENABLE_DEEP
        DD::Image::DeepOp::validate(false);
#endif

        ctx->addCamera(getInputCameraOpForSampleAndView(0/*sample*/,
                                                        rtx.k_hero_view/*(outputContext().view()-rtx.render_views[0])*/));

        // Don't bother if there's no scene to display:
        if (rtx.input_scenes.size() == 0)
            return;  // avoid another crash...

        //std::cout << "     cam_matrix: " << ctx->cam_matrix() << std::endl;
        //std::cout << "    proj_matrix: " << ctx->proj_matrix() << std::endl;
        //std::cout << "    modelmatrix: " << ctx->modelmatrix << std::endl;
        //std::cout << "     camera_pos: " << ctx->camera_pos().x << " " << ctx->camera_pos().y << " " << ctx->camera_pos().z << " " << ctx->camera_pos().w << std::endl;
        // Apply the renderer's formatting:
        // Scale and translate from NDC to format:
#if 1
        ctx->modelmatrix *= get_format_matrix(0.0f/*dx*/, 0.0f/*dy*/);
#else
        const DD::Image::Format& f = format();
        const float fw = float(f.width()) / 2.0f;
        const float fh = float(f.height()) / 2.0f;
        ctx->modelmatrix.translate(fw, fh);
        ctx->modelmatrix.scale(fw, fh, 1.0f);
#endif
        ctx->modelmatrix *= projection_matrix(0/*sample*/);
        ctx->modelmatrix *= camera_matrix(0/*sample*/);
        ctx->transform_mode(DD::Image::VIEWER_PERSP);
    }

    //std::cout << "zpRender::cam_matrix: " << ctx->cam_matrix() << std::endl;
    //std::cout << "         proj_matrix: " << ctx->proj_matrix() << std::endl;
    //std::cout << "         modelmatrix: " << ctx->modelmatrix << std::endl;
    //std::cout << "          camera_pos: " << ctx->camera_pos().x << " " << ctx->camera_pos().y << " " << ctx->camera_pos().z << " " << ctx->camera_pos().w << std::endl;
#if 1
    build_input_handles(ctx);

    // Restore transform mode and matrix:
    ctx->transform_mode(saved_mode);
    ctx->modelmatrix = saved_matrix;

    // Let local zpRender knobs draw:
    build_knob_handles(ctx);
#else
    DD::Image::Iop::build_handles(ctx);
    ctx->transform_mode(saved_mode);

    if (ctx->transform_mode() != DD::Image::VIEWER_2D)
        ctx->add_draw_handle(drawBVH, &rtx, node());

    ctx->modelmatrix = saved_matrix;
#endif
}


//----------------------------------------------------------------------------


/*virtual*/
void
zpRender::knobs(DD::Image::Knob_Callback f)
{
    Bool_knob(f, &rtx.k_preview_mode, "preview_mode", "preview mode (does not save!)");
    SetFlags(f, Knob::EARLY_STORE | Knob::DO_NOT_WRITE);
        Tooltip(f, "Disable this to see what an executed or farm-rendered image will look like.\n"
                   "\n"
                   "The renderer has several 'preview-mode' knobs that allow the user to "
                   "increase interactive feedback by reducing image quality.  Knobs like "
                   "'motion steps' and 'pixel samples' can have dramatic impact on render speed, "
                   "so the default values for these knobs in preview-mode are low-quality "
                   "settings.\n"
                   "\n"
                   "*** This setting DOES NOT SAVE ***");
    //-------------------------------------------------------------------------------
    //Tab_knob(f, "light placement");
    AxisManipulator::addManipulatorKnobs(f, false/*inViewer*/);

    Divider(f);
#ifdef TRY_UV_MODE
    Int_knob(f, &k_uv_mode_object_index, "uv_mode_object", "object");
        ClearFlags(f, Knob::STARTLINE);
    Int_knob(f, &k_uv_mode_surface_index, "uv_mode_surface", "surface");
        ClearFlags(f, Knob::STARTLINE);
    Int_knob(f, &k_uv_mode_tile_index[0], "uv_mode_tile_u", "tile");
        ClearFlags(f, Knob::STARTLINE);
    Int_knob(f, &k_uv_mode_tile_index[1], "uv_mode_tile_v", "");
        ClearFlags(f, Knob::STARTLINE);
#endif
#if 0
    MultiView_knob(f, &rtx.k_views, "render_views", "render views");
        SetFlags(f, Knob::EARLY_STORE); // Split knobs needs this value early
    OneView_knob(f, &rtx.k_hero_view, "hero_view", "hero view");
    Newline(f);
    Enumeration_knob(f, &rtx.k_camera_mode, RenderContext::camera_modes, "camera_mode", "stereo mode");
    //-------------------------------------------------------------------------------
    Divider(f);
#endif
    static const char* shutter_steps_tooltip =
        "Sets the number of time samples that is used to generate motionblur.\n"
        "A step count of 0 effectively disables motionblur, while the default "
        "of 1 creates a straight line blur.  Increasing the number beyond 1 "
        "subdivides the straight line into more segments which may be "
        "necessary for heavy rotational blur (a spinning tire for example.)";
    Int_knob(f, &k_shutter_steps_preview, IRange(0,10), "shutter_steps_preview", "shutter steps");
        SetFlags(f, Knob::EARLY_STORE | Knob::NO_MULTIVIEW);
        Tooltip(f, shutter_steps_tooltip);
        Obsolete_knob(f, "motion_steps_preview", "knob shutter_steps_preview $value");
    Int_knob(f, &k_shutter_steps, IRange(0,10), "shutter_steps", "full-quality:");
        SetFlags(f, Knob::EARLY_STORE | Knob::NO_MULTIVIEW);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, shutter_steps_tooltip);
        Obsolete_knob(f, "motion_steps", "knob shutter_steps $value");
    Double_knob(f, &k_scene_time_offset, "scene_frame_offset", "scene frame-offset");
        SetFlags(f, Knob::EARLY_STORE | Knob::NO_MULTIVIEW);
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        Tooltip(f, "Shifts the frame time of input geometry, light & camera nodes while keeping the renderer "
                   "at the same output frame. For example if renderer is at frame 10 and offset is set to -1.0 "
                   "then the objects, lights and cameras are sampled at frame 9.0\n"
                   "The shift time can be in subframe amounts like -0.5 or 0.23 (see Note2 below)\n"
                   "\n"
                   "Note1: the noise sampling pattern seed is based on output frame, not input frame\n"
                   "\n"
                   "Note2: shifting by subframe amounts may cause the frame-number rounding on geometry and "
                   "texture sources to pick unexpected frames, especially if the shutter open/close times "
                   "straddle the integer frame 0 number (ex. shutter-open=-0.75 and shutter-close=+0.25");
    //Text_knob(f, "
    Newline(f);
    Enumeration_knob(f, &rtx.k_projection_mode, projection_modes, "projection_mode", "projection mode");
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, "Supported projection modes are:\n"
                   "<b>perspective</b>: Objects in front of the camera have the illusion "
                   "of depth defined by the camera's focal-length and aperture.\n"
                   //"<b>orthographic</b>: Objects are viewed using a parallel projection.\n"
                   //"<b>uv</b>: Every object renders its UV space into the output format. "
                   "Use this to cook out texture maps.\n"
                   "<b>spherical</b>: The entire 360deg world is rendered as a spherical map.\n"
                   //"<b>cylindrical</b>: The entire 360deg world is rendered as a cylindrical map.\n"
                   "<b>render camera</b>: Take projection mode from the camera input. Not all modes "
                   "are supported. If mode is not supported by zpRender 'perspective' is used.\n");
    Newline(f);
    rtx.k_shutter.knobs(f, true/*earlyStore*/);
    Double_knob(f, &rtx.k_shutter_bias, "shutter_bias", "bias");
        SetFlags(f, Knob::EARLY_STORE);
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        Tooltip(f, "Biases samples toward shutter close or shutter open for stylized motion blur.  0+ range (0 is uniform blur.)");

    //Enumeration_knob(f, &k_shutter_mode, shutter_modes, "shutter_mode", "mode");
    //-------------------------------------------------------------------------------
    Divider(f);
    Enumeration_knob(f, &k_autolighting_mode, lighting_enable_modes, "lighting_enable_mode", "lighting");
        Tooltip(f, "Enable lighting when lights are present in scene");
    Bool_knob(f, &k_use_direct_lighting, "lighting_enabled", "direct lighting");
        Tooltip(f, "Turn on lights.  This also is a prerequisite for atmospherics.");
    Bool_knob(f, &k_use_indirect_lighting, "bounce_lighting_enabled", "bounce lighting");
        Tooltip(f, "Enable indirect lighting.");
    Newline(f);

    Bool_knob(f, &k_use_atmospheric_lighting, "atmospherics_enabled", "atmospherics");
        Tooltip(f, "Enable atmospherics.  If a light has the optional 'illuminate atmosphere' switch "
                  "this is respected - if not the light automatically affects the atmosphere.\n"
                  "Not all light type are supported - here's the current list:\n"
                  "SpotLight, PointLight, DirectLight");
    Bool_knob(f, &rtx.k_atmosphere_alpha_blending, "atmosphere_alpha_blending", "atmo alpha blending");
        Tooltip(f, "Hold out atmosphere by surface & bg alpha.  Allows atmosphere to appear behind transparent objects.\n"
                   "However, atmosphere is not rendered for the Z ranges between two transparent surfaces that "
                   "are in front of camera.");
    Newline(f);

    //Bool_knob(f, &k_use_deep, "use_deep", "camera deep image");
    Bool_knob(f, &k_render_only, "render_only", "render only");
        Tooltip(f, "Output only the render, don't overlay on background input.");
            Obsolete_knob(f, "atmospherics_only", "knob render_only $value");
    Bool_knob(f, &k_bg_occlusion, "bg_occlusion_enabled", "do bg occlusion");
        Tooltip(f, "Holdout objects & volumes by the background input's Z channel.\n"
                   "If bg Z is closer to camera than the object then the object won't be rendered.");
#if 0
    Bool_knob(f, &rtx.k_dof_enabled, "enable_dof", "enable dof");
        Tooltip(f, "Enable depth-of-field(dof) blur.");
    Float_knob(f, &rtx.k_dof_max_radius, "dof_max_radius", "max radius");
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        SetFlags(f, Knob::NO_MULTIVIEW);
        Tooltip(f, "If dof is enabled, this clamps the maximum circle-of-confusion(coc) size.");
#endif

    Newline(f);

    Bool_knob(f, &k_one_over_Z, "one_over_z", "1/z");
        Tooltip(f, "OFF: You're feeding world-space Z into the BG input, where Z "
                   "is in world-space units increasing the farther they are from camera, "
                   "and 'no object'=infinity.\n"
                   "ON: Nuke-style which is simply 1/Z.  So 'no object'=0 and "
                   "Z *decreases* the further from camera.");
    Bool_knob(f, &k_persp_correct_z, "persp_correct_z", "persp correct z");
        Tooltip(f, "Enable this to perspective-correct the Z-depth input and output which "
                   "will produce more accurate Z intersections, especially near camera.");
    Bool_knob(f, &rtx.k_transparency_enabled, "enable_transparency", "transparency");
        Tooltip(f, "Allow transparent surfaces to blend.  If off only the front-most surface is output.");

    Float_knob(f, &rtx.k_alpha_threshold, "alpha_threshold", "");
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        Tooltip(f, "If transparency is on, this value indicates whether a surface is considered solid.\n"
                   "If the surface alpha is below this value the surface does not add to Z channel or AOVs.");

    //-------------------------------------------------------------------------------
    Divider(f);
    static const char* pixel_samples_tooltip =
        "Sets the per-pixel sampling count for camera rays - the total number is samples-squared, or samples*samples.";
    Enumeration_knob(f, &k_pixel_sample_mode_preview, RenderContext::sampling_modes, "pixel_sample_mode_preview", "pixel samples");
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, pixel_samples_tooltip);
    Enumeration_knob(f, &k_pixel_sample_mode,  RenderContext::sampling_modes, "pixel_sample_mode", "full-quality:");
        SetFlags(f, Knob::EARLY_STORE);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, pixel_samples_tooltip);
    Spacer(f, 30);
    Int_knob(f, &rtx.k_spatial_jitter_threshold, "spatial_jitter", "enable spatial jitter at");
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        Tooltip(f, "When to enable the spatial (X/Y) jittering of the sampling screen location.\n"
                   "If this is 2 then any pixel sample value >= 2 will have spatial jitter.\n"
                   "The amount is scaled by the pixel filter size.");

    Newline(f);
    Enumeration_knob(f, &rtx.k_pixel_filter, Filter::NAMES, "pixel_filter", "pixel filter");
        Tooltip(f, Filter::HELP);
    WH_knob(f, rtx.k_pixel_filter_size, "pixel_filter_size", "");
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER);
        Tooltip(f, "Anti-aliasing is performed by filtering the geometry (or super-sampling) and then "
                   "sampling at pixel locations.  Width and height specify the size of the filter in "
                   "output pixels.\n"
                   "A value of 1 indicates that the spread of the filter is one output pixel in width or height, "
                   "and a value above 1 will produce better antialiasing.  Default is 1.5.");
    Newline(f);
    Channel_knob(f, &k_coverage_chan, 1, "coverage_channel", "pixel coverage");
        Tooltip(f, "Output pixel coverage value to this channel.  This can be used to "
                   "unpremult absolute-type channels like depth, position, or normals to "
                   "eliminate antialiasing or motionblur effects.  Use the 'unpremult' switches "
                   "on the 'outputs' tab to have this done for each output.");
    Newline(f);
    Channel_knob(f, &k_cutout_channel, 1/*channels*/, "cutout_channel", "cutout channel");
        Tooltip(f, "Shaders use this channel to pass cutout info back to renderer.  This needs to match the "
                   "shader settings so that front-to-back rendering order is handled "
                   "properly.");
    Newline(f);
    static const char* texture_filter_tooltip =
        "This is the default filter that texture sampling shaders will use.  A shader can override this.";
    k_texture_filter_preview.knobs(f, "texture_filter_preview", "texture filter");
        Tooltip(f, texture_filter_tooltip);
    texture_filter_.knobs(f, "texture_filter", "full-quality:");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, texture_filter_tooltip);

    //-------------------------------------------------------------------------------
    Divider(f);
    Enumeration_knob(f, &k_global_xform_mode, global_xform_modes, "global_xform_mode", "global xform mode");
#ifndef APPLY_GLOBAL_OFFSET
        SetFlags(f, Knob::DISABLED);
#endif
        Tooltip(f, "Apply a scene-level transform to all objects.\n"
                   "\n"
                   "cam-open: camera translation at shutter open is used as offset.\n"
                   "manual: manually assigned offset (uses a color control for double-precision)\n"
                   "");
    Color_knob(f, k_global_offset.array(), "global_offset", "offset");
        ClearFlags(f, Knob::STARTLINE | Knob::SLIDER);
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_COLOR_DROPDOWN);
#ifndef APPLY_GLOBAL_OFFSET
        SetFlags(f, Knob::DISABLED);
#endif
    Bool_knob(f, &k_ray_use_camera_near_plane, "use_camera_near", "use camera near plane");
        SetFlags(f, Knob::STARTLINE);
        Tooltip(f, "Disable this to use the manual near clipping value below");
    Bool_knob(f, &k_ray_use_camera_far_plane, "use_camera_far", "use camera far plane");
        Tooltip(f, "Disable this to use the manual far clipping value below");
    Newline(f);
    Double_knob(f, &k_ray_near_plane, "ray_near_plane", "near/far clipping");
        SetFlags(f, Knob::DISABLED);
        ClearFlags(f, Knob::SLIDER);
    Double_knob(f, &k_ray_far_plane, "ray_far_plane", "");
        SetFlags(f, Knob::DISABLED);
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
    Newline(f);
    Int_knob(f, &rtx.ray_max_depth, "ray_max_depth", "max ray depth");
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER);
        Tooltip(f, "The maximum depth rays can 'bounce' to.\n"
                   "Ray max depth is tested and incremented for all ray types, so the max depth can be "
                   "a mix of ray types.  For example, if the max depth is 4 then a ray bounce sequence like:\n"
                   " ray#  type\n"
                   " 1     camera\n"
                   " 2     glossy\n"
                   " 3     glossy\n"
                   " 4     diffuse\n"
                   " 5     refraction\n"
                   "will stop at the 'diffuse' ray bounce which is ray #4 in the sequence.\n"
                   "However, if glossy max depth was set to only 1 then shading would stop at ray #2 "
                   "terminating the sequence.");
    Newline(f);
    static const char* ray_samples_tooltip =
        "Sets the per-ray sampling count - the total number is samples-squared, or samples*samples.  Each camera ray is "
        "further split into n rays at surface intersections.\n"
        "For example: a 'pixel samples' of 4 and a 'diffuse' samples of 2 means each camera ray is split into 2*2 "
        "diffuse rays at a surface intersection.\n"
        "This adds up to a total ray count of 64 (4*4 * 2*2) for each pixel.";
    Enumeration_knob(f, &k_ray_diffuse_samples_preview, RenderContext::sampling_modes, "ray_diffuse_samples_preview", "diffuse samples");
        Tooltip(f, ray_samples_tooltip);
    Enumeration_knob(f, &k_ray_diffuse_samples, RenderContext::sampling_modes, "ray_diffuse_samples", "full-quality:");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, ray_samples_tooltip);
    Spacer(f, 30);
    Enumeration_knob(f, &rtx.ray_diffuse_max_depth, ray_bounces_list, "ray_diffuse_max_depth", "max depth");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Diffuse ray max depth.\n"
                   "Diffuse rays will stop when this depth count is reached.  The depth is incremented when a surface is shaded");

    Newline(f);
    Enumeration_knob(f, &k_ray_glossy_samples_preview, RenderContext::sampling_modes, "ray_glossy_samples_preview", "glossy samples");
        Tooltip(f, ray_samples_tooltip);
    Enumeration_knob(f, &k_ray_glossy_samples, RenderContext::sampling_modes, "ray_glossy_samples", "full-quality:");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, ray_samples_tooltip);
    Spacer(f, 30);
    Enumeration_knob(f, &rtx.ray_glossy_max_depth, ray_bounces_list, "ray_glossy_max_depth", "max depth");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Glossy ray max depth.\n"
                   "Glossy rays will stop when this depth count is reached.  The depth is incremented when a surface is shaded");

    Newline(f);
    Enumeration_knob(f, &k_ray_refraction_samples_preview, RenderContext::sampling_modes, "ray_refraction_samples_preview", "refraction samples");
        Tooltip(f, ray_samples_tooltip);
    Enumeration_knob(f, &k_ray_refraction_samples, RenderContext::sampling_modes, "ray_refraction_samples", "full-quality:");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, ray_samples_tooltip);
    Spacer(f, 30);
    Enumeration_knob(f, &rtx.ray_refraction_max_depth, ray_bounces_list, "ray_refraction_max_depth", "max depth");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Refraction ray max depth.\n"
                   "Refraction rays will stop when this depth count is reached.  The depth is incremented when a surface is shaded");

#if 0
    Enumeration_knob(f, &rtx.ray_reflection_max_depth, ray_bounces_list, "ray_reflection_max_depth", "max depth");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Reflection ray max depth.\n"
                   "Reflection rays will stop when this depth count is reached.  The depth is incremented when a surface is shaded");
#endif

    //-------------------------------------------------------------------------------
    //Divider(f);
    //Int_knob(f, &k_bvh_max_objects_per_leaf, "bvh_max_objects_per_leaf", "bvh max objects");
    //    SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
    //    Tooltip(f, "The maximum number of objects per leaf.  As the BVH is being expanded "
    //               "it will continue splitting the volumes until the max depth is reached or "
    //               "the number of primitives at each leaf is less than this value.\n"
    //               "For geometry with large numbers of small objects, like a point cloud, this "
    //               "value should be increased (100-200) to avoid the depth of the bvh tree getting "
    //               "too deep which can dramatically slow down rendering.\n"
    //               "For typical surface geometry this can be set fairly low (10-20).");
    //Text_knob(f, " (at each volume leaf)");
    //    ClearFlags(f, Knob::STARTLINE);
    //Int_knob(f, &k_bvh_max_depth, "bvh_max_depth", "bvh max depth");
    //    SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
    //    Tooltip(f, "The maximum recursion level for the BVHs.  As the BVH is being expanded "
    //               "it will continue splitting the volumes until this max depth value is reached or "
    //               "the number of primitives at each leaf is less than the max_objects_per_leaf value.");

    Divider(f);
    Enumeration_knob(f, &rtx.k_sides_mode, RenderContext::sides_modes, "sides_mode", "sides");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Shade only the front or back face, or shade both.");
    Enumeration_knob(f, &rtx.k_shading_interpolation, RenderContext::shading_interpolation_names, "shading_interpolation", "shading");
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "This controls how values are interpolated between shading samples (usually across "
                   "a polygon).  If type is 'constant' the color and opacity of all the pixels inside "
                   "the polygon are the same.  This is often referred to as flat or facetted shading.  "
                   "If type is 'smooth' the color and opacity of all the pixels between shaded values "
                   "are interpolated from the calculated values.");
    //Bool_knob(f, &k_shade_subsamples, "shade_subsamples", "shade sub samples");
    //    Tooltip(f, "Shade all subpixels.  When this is disabled only a single shading sample is performed "
    //               "for each output pixel even if pixel samples is higher than 1.  This can significantly "
    //               "speed up rendering when the additional sampling accuracy is not required.\n"
    //               "When lighting is enabled and high-frequency speculars are neccessary this should "
    //               "be enabled.");
    Newline(f);
    Input_Channel_knob(f, &k_render_mask_channel, 1/*channels*/, 0/*mask input*/, "render_mask", "render mask");
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, "If a pixel from this mask is <= 0.0 then the pixel isn't rendered.\n"
                   "This can substantially speed up renders that only use a small area of an object.");
    Bool_knob(f, &k_invert_render_mask, "invert_render_mask", "invert");
        Tooltip(f, "Invert the the render mask channel - this is done before threshold is tested.");
    Float_knob(f, &k_render_mask_threshold, "render_mask_threshold", "clip");
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        Tooltip(f, "If the mask value is below this, don't render the pixel.");

    Newline(f);
#ifdef DWA_INTERNAL_BUILD
    // Hide the copy specular knob for DWA builds:
    Bool_knob(f, &rtx.k_copy_specular, "copy_specular", INVISIBLE/*"copy specular from"*/);
    Tooltip(f, "Copy the camera view vector from the hero view's camera.  This eliminates the 'floating specular' "
               "problem that happens when the view vector is coming from multiple locations.");
    OneView_knob(f, &rtx.k_hero_view, "hero_view", INVISIBLE/*""*/);
        Tooltip(f, "Normally is the left view.");
#else
    Bool_knob(f, &rtx.k_copy_specular, "copy_specular", "copy specular from");
    Tooltip(f, "Copy the camera view vector from the hero view's camera.  This eliminates the 'floating specular' "
               "problem that happens when the view vector is coming from multiple locations.");
    OneView_knob(f, &rtx.k_hero_view, "hero_view", "");
        Tooltip(f, "Normally is the left view.");
#endif

    Newline(f);
    Enumeration_knob(f, &rtx.k_output_bbox_mode, RenderContext::output_bbox_modes, "output_bbox_mode", "output bbox");
    WH_knob(f, &overscanX_, "overscan", "overscan padding (x/y splittable)");
        ClearFlags(f, Knob::STARTLINE | Knob::SLIDER);
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        Tooltip(f, "The number of pixels to render beyond the left/right and top/bottom of frame, if requested by subsequent operations.");

    //-------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------
    Tab_knob(f, "outputs");
    Text_knob(f, "enable   /   attribute name   /   merge mode   /   unpremult   /   output layer");
    Newline(f);
    for (uint32_t j=0; j < NUM_AOV_OUTPUTS; ++j)
    {
        Bool_knob(f, &k_aov_enable[j], m_aov_knob_names[j][0], "");
            SetFlags(f, Knob::STARTLINE);
        String_knob(f, &k_aov_name[j], m_aov_knob_names[j][1], "");
            ClearFlags(f, Knob::STARTLINE | Knob::RESIZABLE);
            Tooltip(f, "<b>Hardcoded shading attributes:</b>"
                          "<ul>"
                          "<li><i>V</i> - View-vector from surface point to camera origin (normalized)</li>"
                          "<li><i>Z</i> - Ray depth (distance) from camera</li>"
                          "<li><i>Zl</i> - Linearly projected depth from camera</li>"
                          "<li><i>PW</i> - Displaced shading point in world-space</li>"
                          "<li><i>dPWdx</i> - PW x-derivative</li>"
                          "<li><i>dPWdy</i> - PW y-derivative</li>"
                          "<li><i>PL</i> - Shading point in local-space</li>"
                          "<li><i>PWg</i> - Geometric surface point (no displacement)</li>"
                          "<li><i>st</i> - Primitive's barycentric coordinates</li>"
                          "<li><i>dstdx</i> - st x-derivative</li>"
                          "<li><i>dstdy</i> - st y-derivative</li>"
                          "<li><i>N</i> - Shading normal (interpolated & bumped vertex normal)</li>"
                          "<li><i>Nf</i> - Face-forward shading normal</li>"
                          "<li><i>Ni</i> - Interpolated surface normal</li>"
                          "<li><i>Ng</i> - Geometric surface normal</li>"
                          "<li><i>dNsdx</i> - Ns x-derivative</li>"
                          "<li><i>dNsdy</i> - Ns y-derivative</li>"
                          "<li><i>UV</i> - Surface texture coordinate</li>"
                          "<li><i>dUVdx</i> - UV x-derivative</li>"
                          "<li><i>dUVdy</i> - UV y-derivative</li>"
                          "<li><i>Cf</i> - vertex color (stands for 'Color front')</li>"
                          "<li><i>dCfdx</i> - Cf x-derivative</li>"
                          "<li><i>dCfdy</i> - Cf y-derivative</li>"
                          "<li><i>t, time</i> - frame time</li>"
                          "</ul>"
                       "<b>Shading calculations:</b>"
                          "<ul>"
                          "<li><i>VdotN</i> - Facing-ratio of shading normal</li>"
                          "<li><i>VdotNg</i> - Facing-ratio of geometric normal</li>"
                          "<li><i>VdotNf</i> - Facing-ratio of face-forward shading normal</li>"
                          "</ul>"
                    );
        Enumeration_knob(f, &k_aov_merge_mode[j], zpr::AOVLayer::aov_merge_modes, m_aov_knob_names[j][2], "");
            ClearFlags(f, Knob::STARTLINE);
            Tooltip(f, "Math to use when merging multiple surface samples in depth front to back:"
                       "<ul>"
                       "<li><i>premult-under</i> - UNDER with A premulting (B + A*Aa*(1-Ba)) - best for vector AOVs</li>"
                       "<li><i>under</i> - UNDER (B + A*(1-Ba)) - best for color AOVs</li>"
                       "<li><i>plus</i> - B + A</li>"
                       "<li><i>min</i> - min(B, A) - best for Z</li>"
                       "<li><i>mid</i> - (B + A)/2</li>"
                       "<li><i>max</i> - max(B, A)</li>"
                       "</ul>"
                    );
        Enumeration_knob(f, &k_aov_unpremult[j], aov_unpremult_modes, m_aov_knob_names[j][3], "unpremult by");
            ClearFlags(f, Knob::STARTLINE);
            Tooltip(f, "Unpremult this AOV by coverage or alpha channel.");
        Text_knob(f, " ->  ");
            ClearFlags(f, Knob::STARTLINE);//Spacer(f, 20);
        Channel_knob(f, k_aov_output[j], 3, m_aov_knob_names[j][4], "");
            ClearFlags(f, Knob::STARTLINE);
            SetFlags(f, Knob::NO_CHECKMARKS);
            Tooltip(f, "output channels to route AOV to.");
    }


    Divider(f, "deep options");
    Bool_knob(f, &k_deep_output_subpixel_masks, "deep_output_subpixel_masks", "output subpixel masks");
        Tooltip(f, "");
    Input_Channel_knob(f, k_spmask_channel, 3/*nChans*/, 0/*input*/, "spmask_channels", "spmask channels");
        Tooltip(f, "Channels which contains the per-sample spmask & flag data.");
    Double_knob(f, &k_deep_combine_threshold, "deep_combine_threshold", "deep combine threshold");
        Tooltip(f, "");

    //-------------------------------------------------------------------------------
    Divider(f);
    Enumeration_knob(f, &rtx.k_show_diagnostics, RenderContext::diagnostics_modes, "diagnostics", "diagnostics");
    Int_knob(f, &rtx.k_diagnostics_sample, "sample");
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);

    //-------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------
    Tab_knob(f, "atmospherics:");
    k_ambient_volume.addVolumeKnobs(f);
    //--------------------------------------------------------------------------
    Tab_knob(f, "falloff");
    k_ambient_volume.addFalloffKnobs(f);

    //--------------------------------------------------------------------------
    // Noise tabs:
    k_ambient_volume.addNoiseKnobs(f);

    //=====================================================================
    if (f.makeKnobs())
    {
        //===============================================================================
        // MAKE KNOBS
        //===============================================================================
        // Construct knobs:
        // You can't access any knobs at this point, they don't exist and the 'knob()' call
        // will return 0.
        //std::cout << " <makeKnobs>" << std::endl;
    }
    else
    {
        //===============================================================================
        // STORE KNOBS
        //===============================================================================
        // Store knobs.  This is called on every user change of the tree, so if we check
        // this after all the knobs have stored themselves we can make early decisions:
        //std::cout << " <store>:" << std::endl;


        // TODO: move this to setOutputContext() method?
        // Get full-quality or preview-quality values:
        if (!DD::Image::Application::IsGUIActive() || !rtx.k_preview_mode)
        {
            // FULL-QUALITY:
            rtx.num_shutter_steps    = std::min(std::max((int32_t)0, k_shutter_steps), (int32_t)50);
            m_pixel_sample_mode      = k_pixel_sample_mode;
            m_ray_diffuse_samples    = k_ray_diffuse_samples;
            m_ray_glossy_samples     = k_ray_glossy_samples;
            m_ray_refraction_samples = k_ray_refraction_samples;
        }
        else
        {
            // PREVIEW:
            rtx.num_shutter_steps    = std::min(std::max((int32_t)0, k_shutter_steps_preview), (int32_t)50);
            m_pixel_sample_mode      = k_pixel_sample_mode_preview;
            m_ray_diffuse_samples    = k_ray_diffuse_samples_preview;
            m_ray_glossy_samples     = k_ray_glossy_samples_preview;
            m_ray_refraction_samples = k_ray_refraction_samples_preview;
        }

        // Store this now so that it's available in split_input(), append(), etc.
        this->samples_ = rtx.numShutterSamples();

        // Force render views to update:
        m_render_views_invalid = true;
        updateRenderViews();
    }
}


/*virtual*/
int
zpRender::knob_changed(DD::Image::Knob* k)
{
    //std::cout << "zpRender::knob changed='"<< k->name() << "'" << std::endl;
    if (k == 0)
        return 0;

    if (k == &Knob::inputChange)
    {
        AxisManipulator::updateManipulatorMenu();
        Render::knob_changed(k);
        return 1;
    }
    else if (k == &Knob::showPanel)
    {
        AxisManipulator::updateManipulatorMenu();
        Render::knob_changed(k);
        return 1;
    }
    else if (k->name() == "atmospherics_enabled")
    {
        if (k->get_value() > 0.5) knob("lighting_enabled")->set_value(1.0);
        return 1;
    }
    else if (k->name() == "use_camera_near")
    {
        knob("ray_near_plane")->enable(k->get_value() < 0.5);
        return 1;
    }
    else if (k->name() == "use_camera_far")
    {
        knob("ray_far_plane")->enable(k->get_value() < 0.5);
        return 1;
    }

    // Allow manipulator to change knobs:
    if (AxisManipulator::knobChanged(k))
        return 1;

    return Render::knob_changed(k);
}


/*! Add in the camera hash.
*/
/*virtual*/
void
zpRender::append(Hash& hash)
{
    const int view0 = outputContext().view()-rtx.render_views[0];
    DD::Image::CameraOp* cam = getInputCameraOpForSampleAndView(0, view0);
    if (cam)
        hash.append(cam->hash());

    DD::Image::Render::append(hash);
    //std::cout << "zpRender::append(" << this << "): 0x" << std::hex << hash.value() << std::dec << std::endl;
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


/*! Validate the 3D scene renderer.

    We construct one zpr::Scene structure per time sample.

    The Scene contains all primitives and lights transformed at that
    moment in time.
*/
/*virtual*/
void
zpRender::_validate(bool for_real)
{
    // Validate inputs and copy the bg info from input0:
    copy_info();

    // Print info after copy_indo() validates inputs.
#ifdef DEBUG_STARTUP
    std::cout << "==============================================================================" << std::endl;
    std::cout << "==============================================================================" << std::endl;
    std::cout << "zpRender::_validate(" << this << "): for_real=" << for_real << std::endl;
    for (int i=0; i < DD::Image::Op::inputs(); ++i)
    {
        DD::Image::Op* op = DD::Image::Op::input(i);
        std::cout << "    " << i << "(" << op << ") ";
        if (op)
        {
            std::cout << op->Class() << "[frame=" << op->outputContext().frame();
            std::cout << ", view=" << DD::Image::OutputContext::viewname(op->outputContext().view()) << "]";
        }
        std::cout << std::endl;
    }
#endif
    //std::cout << "info[" << info_.x() << " " << info_.y() << " " << info_.r() << " " << info_.t() << "]" << std::endl;

    // Round the render frame down:
    rtx.render_frame     = ::floor(this->outputContext().frame());
    rtx.render_view      = this->outputContext().view();
    rtx.render_view_name = DD::Image::OutputContext::viewname(rtx.render_view);

    // Update format & res factors:
    rtx.render_format = &info_.format();

    int sample_side_count = getRaySampleSideCount(m_pixel_sample_mode);
    rtx.ray_single_scatter_samples = sample_side_count*sample_side_count;
    //std::cout << "  ray_single_scatter_samples=" << rtx.ray_single_scatter_samples << std::endl;

    // Get the shutter samples value - samples_ should be rtx.num_shutter_steps + 1:
#if DEBUG
    assert(rtx.num_shutter_steps >= 0);
#endif
    const uint32_t nShutterSamples = rtx.numShutterSamples();
    //std::cout << "  nShutterSamples=" << nShutterSamples << std::endl;

    DD::Image::Render::input_scenes  = nShutterSamples;
    DD::Image::Render::render_scenes = nShutterSamples;
 

    /*

    The m_input_shutter_scenes[] list *always* goes from frame +0 to either frame +1 or frame -1.
    This is done so that frame +0's scene is *always* the zpRender's OutputContext frame.
    This means that depending on the shutter mode the frame times may decrease at each
    increasing scene index:
      frame=349:
        shutter-steps=0:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.input_scenes[ 349.0 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.input_scenes[ 349.0 ]
        shutter-steps=1:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.input_scenes[ 349.0, 349.35 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.input_scenes[ 349.0, 348.65 ]
        shutter-steps=2:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.input_scenes[ 349.0, 349.175, 349.35 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.input_scenes[ 349.0, 348.825, 348.65 ]

    The rtx.shutter_scenerefs[] list maps the scenes to shutter-steps in always increasing
    time so that a primitive can rely on linear interpolation between shutter-step 0 and
    shutter-step 1 being between a smaller frame number and a larger frame number:
      frame=349:
        shutter-steps=0:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.shutter_scenerefs[ 0.0 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.shutter_scenerefs[ 0.0 ]
        shutter-steps=1:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.shutter_scenerefs[  0.0,   0.35 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.shutter_scenerefs[ -0.35,  0.0  ]
        shutter-steps=2:
          shutter: mode='start', width=0.35, open= 0.0,  close=0.35: rtx.shutter_scenerefs[  0.0,   0.175,  0.35 ]
          shutter: mode='end',   width=0.35, open=-0.35, close=0.0 : rtx.shutter_scenerefs[ -0.35, -0.175,  0.0  ]

    */

    // Resize all the shutter sample lists:
    for (size_t i=0; i < rtx.input_scenes.size(); ++i)
        delete rtx.input_scenes[i];
    rtx.input_scenes.resize(nShutterSamples);
    rtx.shutter_scenerefs.resize(nShutterSamples);
    rtx.shutter_times.resize(nShutterSamples);

    // Fill the motion steps array:
    //std::vector<MotionSampleSceneRef> sorted_srefs;
    //sorted_srefs.reserve(rtx.input_scenes.size());
    rtx.shutter_open_offset  = 0.0f;
    rtx.shutter_close_offset = 0.0f;
    rtx.frame0 = rtx.render_frame + k_scene_time_offset;

    //std::cout << "  rtx.render_frame=" << rtx.render_frame << ", rtx.frame0=" << rtx.frame0 << std::endl;
    //std::cout << "  scenes [";
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        const double scene_frame_time = getFrameForSample(j, rtx.render_frame/*base_frame*/);
        const float  frame0_offset    = float(scene_frame_time - rtx.render_frame);

        // To identify negative shutters we set the shutter_sample to negative:
        int shutter_sample;
        if (j > 0 && frame0_offset < 0.0)
        {
            // Backwards shutter:
            rtx.shutter_open_offset = std::min(rtx.shutter_open_offset, frame0_offset);
            shutter_sample = -j;
        }
        else
        {
            // Forwards shutter:
            rtx.shutter_close_offset = std::max(rtx.shutter_close_offset, frame0_offset);
            shutter_sample = j;
        }

        // Creating the scene also assigns its motion sample and absolute frame number:
        zpr::Scene* input_scene = new zpr::Scene(shutter_sample, scene_frame_time/*frame*/);
        rtx.input_scenes[j] = input_scene;

        // Fill in the scene ref that we will sort:
        ShutterSceneRef& sref = rtx.shutter_scenerefs[j];
        sref.scene          = input_scene;
        sref.camera         = NULL; // set this after scene is built
        sref.hero_camera    = NULL; // set this after scene is built
        sref.op_input_index = j;
        sref.shutter_sample = -1; // set this after time sorting
        sref.frame0         = rtx.frame0;
        sref.frame          = scene_frame_time;
        sref.frame0_offset  = frame0_offset;

        //std::cout << " " << input_scene << "[" << scene_frame_time << ", offset=" << frame0_offset << "]";
    }
    //std::cout << " ]" << std::endl;

    // Save the shutter length as a single number for convenience:
    rtx.shutter_length = (rtx.shutter_open_offset < 0.0f && rtx.shutter_close_offset <= 0.0f)?
                            (rtx.shutter_open_offset  - rtx.shutter_close_offset):
                                (rtx.shutter_close_offset - rtx.shutter_open_offset);
    //std::cout << "shutter_length=" << rtx.shutter_length << std::endl;

    // Sort the scene refs in time to make sure they're always increasing in shutter time:
    std::sort(rtx.shutter_scenerefs.begin(), rtx.shutter_scenerefs.end());

    // Fill the motion-time list and assign the shutter sample indices:
    rtx.frame0_shutter_sample = 0;

    //std::cout << "  shutter_times[";
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        ShutterSceneRef& sref = rtx.shutter_scenerefs[j];
        sref.shutter_sample  = j;
        rtx.shutter_times[j] = sref.frame;
        // Find the motion sample index that's frame0:
        if (fabs(sref.frame - rtx.frame0) < std::numeric_limits<double>::epsilon())
            rtx.frame0_shutter_sample = j;

        //std::cout << " " << rtx.shutter_times[j] << ":" << sref.op_input_index;
    }
    //std::cout << " ] shutter_open_offset=" << rtx.shutter_open_offset;
    //std::cout << ", shutter_close_offset=" << rtx.shutter_close_offset;
    //std::cout << ", frame0_shutter_sample=" << rtx.frame0_shutter_sample << std::endl;

    // Get the correct multisampling and jitter arrays -
    // for time and spatial stochastic sampling:
    multisample_array_ = multisample_array(multisampling_);
    jitter = jitter_array(nShutterSamples);

#ifdef APPLY_GLOBAL_OFFSET
    rtx.global_xform.setToIdentity();
    rtx.global_offset.set(0.0, 0.0, 0.0);
    if (k_global_xform_mode == GLOBAL_XFORM_CAM_OPEN)
    {
        // Get global scene offset from the negative translation of the shutter-open
        // hero camera:
        DD::Image::CameraOp* global_xform_cam =
            getInputCameraOpForSampleAndView(rtx.shutter_scenerefs[0].op_input_index,
                                             (rtx.k_hero_view - rtx.render_views[0])/*view*/);
        if (global_xform_cam)
        {
            global_xform_cam->validate(true); // just in case...
            rtx.global_offset = -global_xform_cam->matrix().translation();
            rtx.global_xform.translation(floor(rtx.global_offset.x),
                                         floor(rtx.global_offset.y),
                                         floor(rtx.global_offset.z));
        }
        else
        {
            ; // do nothing
        }
    }
    else if (k_global_xform_mode == GLOBAL_XFORM_MANUAL)
    {
        rtx.global_xform.translation(k_global_offset[0],
                                     k_global_offset[1],
                                     k_global_offset[2]);
        rtx.global_offset.set(k_global_offset[0],
                              k_global_offset[1],
                              k_global_offset[2]);
    }
    //std::cout << "zpRender:: global_xform" << rtx.global_xform << std::endl;
#endif


    // Build the render state hash:
    DD::Image::Hash new_hash;
    {
        // Make hash unique to this Render op instance:
        Op* render_op = DD::Image::Op::firstOp();
        new_hash.append(&render_op, sizeof(void*));
    }
    Render::format().append(new_hash);
    new_hash.append(rtx.render_frame);
    new_hash.append(rtx.render_view);
    new_hash.append(rtx.k_views);
    new_hash.append(rtx.k_hero_view);
    new_hash.append(rtx.k_camera_mode);
    new_hash.append(rtx.ray_single_scatter_samples);
    new_hash.append(rtx.k_pixel_filter);
    new_hash.append(rtx.k_pixel_filter_size, sizeof(float)*2);
    //new_hash.append(k_shading_interpolation);
    rtx.k_shutter.append(new_hash);
    new_hash.append(rtx.k_shutter_bias);
    new_hash.append(rtx.k_spatial_jitter_threshold);
    new_hash.append(rtx.num_shutter_steps);

    int scene_proj_mode = DD::Image::CameraOp::LENS_PERSPECTIVE; // default

    // Initialize scenes in motion-time order:
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        // Bail quickly on user abort:
        if (DD::Image::Op::aborted())
            return;

        ShutterSceneRef& sref = rtx.shutter_scenerefs[j];
        const uint32_t input_sample = sref.op_input_index;
        zpr::Scene*    input_scene  = sref.scene;
        assert(input_scene); // Shouldn't happen...

        // Point the mb_scene at the next in line:
        if (j < rtx.shutter_scenerefs.size()-1)
            input_scene->setMotionblurScene(rtx.shutter_scenerefs[j+1].scene);
        else
            input_scene->setMotionblurScene(NULL);

        // Get the GeoOp that generates the geometry for this scene:
        DD::Image::GeoOp* geo = getInputGeoOpForSample(input_sample);
        input_scene->setGeoOp(geo);
#ifdef DEBUG_STARTUP
        std::cout << "  " << j << " input_sample=" << input_sample;
        std::cout << ", input_scene(" << input_scene << "), frame=" << input_scene->frame;
        std::cout << ", geo=" << geo << ", input_scene_mb_scene=" << input_scene->mb_scene();
#endif

        // Build the input GeometryList:
        if (geo)
        {
            geo->validate(for_real);
            geo->build_scene(*input_scene);

            new_hash.append(geo->Op::hash());
        }

        input_scene->setFormat(rtx.render_format);

        input_scene->camera = getInputCameraOpForSampleAndView(input_sample, (rtx.render_view - rtx.render_views[0]));

#ifdef DEBUG_STARTUP
        std::cout << ", geoinfos[";
        const uint32_t nObjects = input_scene->objects();
        for (uint32_t i=0; i < nObjects; ++i)
            std::cout << " " << &input_scene->object(i);
        std::cout << " ]" << std::endl;
        std::cout << "  " << j << " camera(" << input_scene->camera << ")";
        std::cout << " for view " << (rtx.render_view - rtx.render_views[0]);
        std::cout << ", matrix=" << input_scene->camera->matrix() << std::endl;
#endif

        //------------------------------------------------------------

        // Get render and scene projection mode at shutter open scene:
        if (j == 0)
        {
            if (rtx.k_projection_mode == PROJECTION_RENDER_CAMERA)
            {
                // Get projection from scene camera:
                if (input_scene->camera && input_scene->camera->projection_mode() < DD::Image::CameraOp::LENS_RENDER_CAMERA)
                {
                    scene_proj_mode = input_scene->camera->projection_mode(); // camera sets projection mode

                    // Map DD::Image::CameraOp camera projection mode to render projection mode:
                    switch (scene_proj_mode)
                    {
                        default:
                        case DD::Image::CameraOp::LENS_PERSPECTIVE:
                            rtx.render_projection = RenderContext::CAMERA_PROJECTION_PERSPECTIVE; break;
                        //case DD::Image::CameraOp::LENS_ORTHOGRAPHIC:
                        //    rtx.render_projection = RenderContext::CAMERA_PROJECTION_ORTHOGRAPHIC; break;
                        //case DD::Image::CameraOp::LENS_UV:
                        //    rtx.render_projection = RenderContext::CAMERA_PROJECTION_UV; break;
                        case DD::Image::CameraOp::LENS_SPHERICAL:
                            rtx.render_projection = RenderContext::CAMERA_PROJECTION_SPHERICAL; break;
                    }
                }
            }
            else
            {
                // zpRender sets projection modes directly:
                switch (rtx.k_projection_mode)
                {
                    default:
                    case PROJECTION_PERSPECTIVE:
                        scene_proj_mode = DD::Image::CameraOp::LENS_PERSPECTIVE;
                        rtx.render_projection = RenderContext::CAMERA_PROJECTION_PERSPECTIVE;
                        break;
                    //case PROJECTION_ORTHOGRAPHIC:
                    //    scene_proj_mode = DD::Image::CameraOp::LENS_ORTHOGRAPHIC;
                    //    rtx.render_projection = RenderContext::CAMERA_PROJECTION_ORTHOGRAPHIC;
                    //    break;
                    //case PROJECTION_UV:
                    //    scene_proj_mode = DD::Image::CameraOp::LENS_UV;
                    //    rtx.render_projection = RenderContext::CAMERA_PROJECTION_UV;
                    //    break;
                    case PROJECTION_SPHERICAL:
                        scene_proj_mode = DD::Image::CameraOp::LENS_SPHERICAL;
                        rtx.render_projection = RenderContext::CAMERA_PROJECTION_SPHERICAL;
                        break;
                    //case PROJECTION_CYLINDRICAL:
                    //    scene_proj_mode = DD::Image::CameraOp::LENS_USER_CAMERA;
                    //    rtx.render_projection = RenderContext::CAMERA_PROJECTION_CYLINDRICAL;
                    //    break;
                }
            }
        }
        input_scene->setProjectionMode(scene_proj_mode);

        //------------------------------------------------------------

        // Include all the CameraOps in the hash, including the split ones:
        if (rtx.k_camera_mode == RenderContext::CAMERA_COMBINED)
        {
            // Append all the render views:
            for (uint32_t i=0; i < rtx.render_views.size(); ++i)
            {
                //new_hash.append(rtx.render_views[i]);
                DD::Image::CameraOp* cam = getInputCameraOpForSampleAndView(j, rtx.render_views[i]);
                if (input_scene->camera)
                {
                    input_scene->camera->validate(for_real);
                    new_hash.append(cam->hash());
                }
            }
        }
        else
        {
            DD::Image::CameraOp* cam = getInputCameraOpForSampleAndView(j, (rtx.render_view-rtx.render_views[0]));
            if (cam)
            {
                cam->validate(for_real);
                new_hash.append(cam->hash());
            }
        }

        //------------------------------------------------------------

        if (input_scene->camera)
            input_scene->lens_func = input_scene->camera->lensNfunction(scene_proj_mode);
        else
            input_scene->lens_func = default_camera.lensNfunction(DD::Image::CameraOp::LENS_PERSPECTIVE);

        // TODO: don't need max_tessellation anymore...
        input_scene->setMaxTessellation(std::max(0, max_tessellation_));

        // Set texture filter on Scene for legacy shading system:
        if (!rtx.k_preview_mode)
        {
            if (texture_filter_.type() == Filter::Impulse)
                input_scene->filter(NULL);
            else
                input_scene->filter(&texture_filter_);
        }
        else
        {
            if (k_texture_filter_preview.type() == Filter::Impulse)
                input_scene->filter(NULL);
            else
                input_scene->filter(&k_texture_filter_preview);
        }

        input_scene->transparency(true);
        //input_scene->raycasting(false);

        // Assign the output matrices:
        input_scene->transforms()->set_format_matrix(get_format_matrix(0.0f/*dx*/, 0.0f/*dy*/));
        input_scene->transforms()->set_projection_matrix(projection_matrix(input_sample/*sample*/));
#if 0//def APPLY_GLOBAL_OFFSET
        DD::Image::Matrix4 cmatrix = rtx.global_xform; // initialize matrix to a translation value
        cmatrix *= camera_matrix(input_sample/*sample*/)
        input_scene->transforms()->set_camera_matrix(cam_matrix);
#else
        input_scene->transforms()->set_camera_matrix(camera_matrix(input_sample/*sample*/));
#endif

        input_scene->transforms()->set_object_matrix(DD::Image::Matrix4::identity());

        // Update the scene ref's cameras:
        sref.camera      = input_scene->camera; // could be any of the views
        sref.hero_camera = getInputCameraOpForSampleAndView(rtx.shutter_scenerefs[0].op_input_index,
                                                            (rtx.k_hero_view - rtx.render_views[0])/*view*/);

    }

    // This call finds the screen bounding-box and validates all the object material Iops.
    // The second half to this is done in _request() which calls doTextureRequests().
    rtx.validateObjects(rtx.shutter_scenerefs[0].scene,
                        for_real);

    // Add other channels we need for z and alpha compositing:
    rtx.material_channels += DD::Image::Mask_Z;        // Always need Z by default from shaders
    rtx.material_channels += DD::Image::Mask_Alpha;    // Always need transparency during shading
#ifdef DEBUG_STARTUP
    std::cout << "  rtx.texture_channels=" << rtx.texture_channels << std::endl;
    std::cout << "  rtx.material_channels=" << rtx.material_channels << std::endl;
#endif

    // Pad the render region all'round so there's one pixel of black surrounding
    // the scene, plus add'l expansion for filter size:
    if (!rtx.render_region.isEmpty())
    {
        const int x_pad = (int)ceilf(fabsf(rtx.k_pixel_filter_size[0])) + 1;
        const int y_pad = (int)ceilf(fabsf(rtx.k_pixel_filter_size[1])) + 1;
        rtx.render_region.pad(x_pad, y_pad);
    }
    else
    {
        rtx.render_region.set(0,0,0,0);
    }
#ifdef DEBUG_STARTUP
    std::cout << "  rtx.render_bbox" << rtx.render_bbox << std::endl;
    std::cout << "  rtx.render_region" << rtx.render_region << std::endl;
#endif

    // Save final RenderContext validata values into zpRender:
    DD::Image::Render::world_bbox       = rtx.render_bbox.asDDImage();
    DD::Image::Render::screen_bbox      = rtx.render_region.asDDImage();
    DD::Image::Render::projection_mode_ = scene_proj_mode; // likely not required


    // Set the validate results on all input scenes:
    for (uint32_t j=0; j < nShutterSamples; ++j)
    {
        // Bail quickly on user abort:
        if (DD::Image::Op::aborted())
            return;

        zpr::Scene* input_scene = rtx.shutter_scenerefs[j].scene;

        input_scene->setBbox(rtx.render_bbox.asDDImage());
        input_scene->setScreenBbox(rtx.render_region.asDDImage());
        input_scene->setChannels(rtx.material_channels);

        // TODO: do we still need to set these per-object transforms?
        //input_scene->clearObjectTransforms();
        //input_scene->reserveObjectTransforms(uint32_t n);
        //input_scene->setObjectTransforms(int i, DD::Image::MatrixArray* m);
    }


    // Build changed mask:
    /*
        GeometryFlag    = 0x00000001,
        MaterialsFlag   = 0x00000002,
        LightsFlag      = 0x00000004,
        CameraFlag      = 0x00000008,
    */
#ifdef DEBUG_STARTUP
    std::cout << "  rtx.geometry_hash(0x01)=0x" << std::hex << rtx.geometry_hash.value() << std::dec << std::endl;
    std::cout << "  rtx.material_hash(0x02)=0x" << std::hex << rtx.material_hash.value() << std::dec << std::endl;
    std::cout << "  rtx.lighting_hash(0x04)=0x" << std::hex << rtx.lighting_hash.value() << std::dec << std::endl;
    std::cout << "  rtx.camera_hash(0x08)  =0x" << std::hex << rtx.camera_hash.value() << std::dec << std::endl;
#endif
    m_changed_mask = 0x0;
    if (rtx.geometry_hash != m_geometry_hash)
    {
        m_changed_mask |= zpr::GeometryFlag;
        m_geometry_hash = rtx.geometry_hash;
        new_hash.append(rtx.geometry_hash);
    }
    if (rtx.material_hash != m_material_hash)
    {
        m_changed_mask |= zpr::MaterialsFlag;
        m_material_hash = rtx.material_hash;
        new_hash.append(rtx.material_hash);
    }
    if (rtx.lighting_hash != m_lighting_hash)
    {
        m_changed_mask |= zpr::LightsFlag;
        m_lighting_hash = rtx.lighting_hash;
        new_hash.append(rtx.lighting_hash);
    }
    if (rtx.camera_hash != m_camera_hash)
    {
        m_changed_mask |= zpr::CameraFlag;
        m_camera_hash = rtx.camera_hash;
        new_hash.append(rtx.camera_hash);
    }
#ifdef DEBUG_STARTUP
    std::cout << "    m_changed_mask=0x" << std::hex << m_changed_mask << std::dec << std::endl;
#endif

    //
    if (new_hash != rtx.hash)
    {
        if (m_changed_mask & zpr::GeometryFlag)
        {
            // This indicates that no object bvhs have been generated yet:
            rtx.objects_bvh_initialized = false;
            rtx.lights_bvh_initialized  = false;
        }

        // Force generate_render_primitives() to get called:
        rtx.objects_initialized = false;

        rtx.hash = new_hash;
        //std::cout << "render hash changed to 0x" << std::hex << new_hash.value() << std::dec << std::endl;
    }


    // Derive final Iop bbox, which includes the bg pixels bbox:
    const int overscanX = std::max(0, (int)DD::Image::Render::overscanX_);
    const int overscanY = std::max(0, (int)DD::Image::Render::overscanY_);

    // Only allow the format to be expanded:
    //DD::Image::Render::overscanFormat_ = info_.format();
    //DD::Image::Render::overscanFormat_.pad(overscanX, overscanY);

    DD::Image::Box overscan_bbox(-overscanX,
                                 -overscanY,
                                 rtx.render_format->width()+overscanX,
                                 rtx.render_format->height()+overscanY);

    // Clamp to overscan format:
    switch (rtx.k_output_bbox_mode)
    {
        default:
        case RenderContext::BBOX_SCENE_SIZE:
            DD::Image::Render::screen_bbox.intersect(overscan_bbox);
            if (k_render_only)
                info_.Box::set(DD::Image::Render::screen_bbox);
            else
                info_.Box::merge(DD::Image::Render::screen_bbox);
            break;

        case RenderContext::BBOX_CLAMP_TO_FORMAT:
            // Expand bbox to format:
            info_.Box::set(overscan_bbox);
            break;
    }
    //std::cout << "Render::output_bbox" << Fsr::Box2i(info_) << std::endl;


    m_have_bg_Z = (input0().channels() & DD::Image::Mask_Z);

    // Channels we're going to fill in:
    rtx.render_channels = input0().channels();
    rtx.render_channels += rtx.material_channels;
    if (rtx.atmospheric_lighting_enabled)
       rtx.render_channels += DD::Image::Mask_RGBA;
    rtx.render_channels += DD::Image::Mask_Z;  // always output Z
    rtx.render_channels += k_coverage_chan;


    if (for_real)
    {
       //------------------------------------------------
       // Do work that's only for actual rendering:
       //------------------------------------------------

       //std::cout << "---------------------------------------" << std::endl;
       //std::cout << "  for_real:" << std::endl;

        sample_side_count = getRaySampleSideCount(m_ray_diffuse_samples);
        rtx.ray_diffuse_samples = sample_side_count*sample_side_count;
        sample_side_count = getRaySampleSideCount(m_ray_glossy_samples);
        rtx.ray_glossy_samples = sample_side_count*sample_side_count;
        sample_side_count = getRaySampleSideCount(m_ray_refraction_samples);
        rtx.ray_refraction_samples = sample_side_count*sample_side_count;

        // Copy the cameras from all views into local structures.  We have to
        // do this in validate() as changing the outputcontext on the knobs at render
        // time (the cleaner 'Nuke' way) has been flaky and prone to crashing Nuke.  I think
        // it's due to the Text_knob not handling itself properly when its context
        // is changed...maybe this is fixed in 6.1.
        // Anyway, this is cheap and reliable:
        for (uint32_t i=0; i < rtx.shutter_scenerefs.size(); ++i)
        {
            // Bail quickly on user abort:
            if (DD::Image::Op::aborted())
                return;

            const ShutterSceneRef& sref = rtx.shutter_scenerefs[i];
            const uint32_t input_sample = sref.op_input_index;

            // Assign camera matrices from all views to this scene:
            for (uint32_t j=0; j < rtx.render_views.size(); ++j)
            {
                DD::Image::CameraOp* cam = getInputCameraOpForSampleAndView(input_sample, j);
                // Don't bother if there's no camera...:
                if (!cam)
                    continue;
                cam->validate(true);
                //printf("%d:%s: cam=%p, cv=%p\n", input_sample, DD::Image::OutputContext::viewname(rtx.render_views[j]).c_str(), cam, &cv);
            }
        }

        DD::Image::CameraOp* cam =
            getInputCameraOpForSampleAndView(rtx.shutter_scenerefs[0].op_input_index, rtx.k_hero_view);
        if (cam)
        {
            rtx.Near = cam->Near();
            rtx.Far  = cam->Far();
        }

        //------------------------------------------------
        // Pixel Filter:
        //------------------------------------------------
        m_pixel_filter.type(rtx.k_pixel_filter);
        m_pixel_filter.initialize(); // Just in case

        //==============================================================
        // Build list of AOV layers to output:
        //==============================================================

        rtx.color_channels  = Mask_None;
        rtx.vector_channels = Mask_None;
        rtx.aov_channels    = Mask_None;

        rtx.aov_outputs.clear();
        rtx.aov_outputs.reserve(NUM_AOV_OUTPUTS);
        AOVLayer aov_layer;
        for (uint32_t j=0; j < NUM_AOV_OUTPUTS; ++j)
        {
            if (!k_aov_enable[j] || k_aov_name[j] == 0 || k_aov_name[j][0] == 0)
                continue;

            if (aov_layer.build(rtx.aov_handler, k_aov_name[j], 3, k_aov_output[j]))
            {
                if (aov_layer.enabled)
                {
                    aov_layer.unpremult  = char(k_aov_unpremult[j]);
                    aov_layer.merge_mode = k_aov_merge_mode[j];
                    rtx.aov_outputs.push_back(aov_layer);
                    rtx.aov_map[aov_layer.name] = (uint32_t)(rtx.aov_outputs.size()-1);
                    // Add the aov channels to the render & unpremult set:
                    rtx.aov_channels    += aov_layer.mask;
                    rtx.render_channels += aov_layer.mask;
                    rtx.vector_channels += aov_layer.mask;
                }
            }
        }
        rtx.under_channels  = rtx.render_channels;   // default to UNDER all render channels
        rtx.under_channels -= rtx.aov_channels;      // but remove the ones we're merging special
        rtx.under_channels -= DD::Image::Chan_Z;     // and never UNDER Z!
        rtx.under_channels += DD::Image::Chan_Alpha; // always need alpha!


        // Build set of color channels and vector channels so that we can identify
        // which should be unpremulted:
        // Find the RGB layers in the channel set:
        foreach(ch, rtx.render_channels)
        {
            std::string name(DD::Image::getName(ch));
            if (name.empty())
                continue;

            std::string layer;
            size_t a = name.find_last_of('.');
            if (a != std::string::npos)
            {
                layer = name.substr(0, a);
                name  = name.substr(a+1);
            }
            // Check for color layers we recognize:
            if (layer == "rgb"   ||
                layer == "rgba"  ||
                layer == "alpha" ||
                layer == "mask"  ||
                layer == "rotopaint_mask")
            {
                rtx.color_channels += ch;
                continue;
            }
#if 0
            // Check for vector layers we recognize:
            if (layer == "depth"  ||
                layer == "motion" ||
                layer == "N" || layer == "nrmls" || layer == "normals" ||
                layer == "P" || layer == "pnts"  || layer == "points"  || layer == "positions")
            {
                rtx.vector_channels += ch;
                continue;
            }
#endif
            // Don't recognize the layer, check the channel name.
            // Color channels:
            if (name == "red"   ||
                name == "green" ||
                name == "blue"  ||
                name == "alpha")
            {
                rtx.color_channels += ch;
                continue;
            }
#if 0
            // Vector channels:
            if (name == "x" || name == "X" ||
                name == "y" || name == "Y" ||
                name == "z" || name == "Z" ||
                name == "w" || name == "W" ||
                name == "u" || name == "U" ||
                name == "v" || name == "V")
            {
                rtx.vector_channels += ch;
                continue;
            }
#endif
            // Don't recognize the channel, default to a color:
            rtx.color_channels += ch;
        }
#ifdef DEBUG_STARTUP
        std::cout << "    render_channels=" << rtx.render_channels << std::endl;
        std::cout << "   texture_channels=" << rtx.texture_channels << std::endl;
        std::cout << "  material_channels=" << rtx.material_channels << std::endl;
        std::cout << "    shadow_channels=" << rtx.shadow_channels << std::endl;
        std::cout << "     color_channels=" << rtx.color_channels << std::endl;
        std::cout << "    vector_channels=" << rtx.vector_channels << std::endl;
        std::cout << "       aov_channels=" << rtx.aov_channels << std::endl;
        std::cout << "     under_channels=" << rtx.under_channels << std::endl;
#endif
        

        //============================================================
        // Setup Volume Render Parameters:
        //============================================================
        if (rtx.atmospheric_lighting_enabled)
            k_ambient_volume.validate(for_real);


        //============================================================
        // Setup Lighting Parameters:
        //============================================================
        if (k_autolighting_mode == LIGHTING_ENABLE_AUTO)
        {
            zpr::Scene* scene0 = rtx.shutter_scenerefs[0].scene;
            uint32_t nLights  = (uint32_t)scene0->lights.size();
            uint32_t nEnabledLights = 0;
            for (uint32_t lt_index=0; lt_index < nLights; ++lt_index)
            {
                DD::Image::LightContext* ltx = scene0->lights[lt_index];
                assert(ltx); // Shouldn't happen...
                if (!ltx->light()->node_disabled())
                    ++nEnabledLights;
            }
            if (nEnabledLights > 0)
            {
                rtx.direct_lighting_enabled      = k_use_direct_lighting;
                rtx.indirect_lighting_enabled    = k_use_indirect_lighting;
                rtx.atmospheric_lighting_enabled = k_use_atmospheric_lighting;
            }
            else
            {
                rtx.direct_lighting_enabled      = false;
                rtx.indirect_lighting_enabled    = false;
                rtx.atmospheric_lighting_enabled = false;
            }
        }
        else if (k_autolighting_mode == LIGHTING_ENABLED)
        {
            rtx.direct_lighting_enabled      = k_use_direct_lighting;
            rtx.indirect_lighting_enabled    = k_use_indirect_lighting;
            rtx.atmospheric_lighting_enabled = k_use_atmospheric_lighting;
        }
        else
        {
            rtx.direct_lighting_enabled      = false;
            rtx.indirect_lighting_enabled    = false;
            rtx.atmospheric_lighting_enabled = false;
        }
#ifdef DEBUG_STARTUP
        std::cout << "  direct_lighting_enabled=" << rtx.direct_lighting_enabled << std::endl;
        std::cout << "  indirect_lighting_enabled=" << rtx.indirect_lighting_enabled << std::endl;
        std::cout << "  atmospheric_lighting_enabled=" << rtx.atmospheric_lighting_enabled << std::endl;
#endif

    } // for_real = true


    // We output these channels:
    info_.turn_on(rtx.render_channels);

    info_.ydirection(1);

    // This is going to be slow:
    slowness(10);
    //std::cout << "---------------------------------------" << std::endl;

    // No delay for close() so we can clean up fast:
    DD::Image::Op::callCloseAfter(0.0/*seconds*/);

#ifdef ENABLE_DEEP
    //------------------------------------------------------------------
    // Set deep output params:
    DD::Image::ChannelSet deep_channels = info_.channels();
    deep_channels += DD::Image::Chan_DeepFront;
    deep_channels += DD::Image::Chan_DeepBack;
    if (k_deep_output_subpixel_masks)
    {
        deep_channels += k_spmask_channel[0];
        deep_channels += k_spmask_channel[1];
        deep_channels += k_spmask_channel[2];
    }
    _deepInfo = DD::Image::DeepInfo(info_.formats(), info_, deep_channels);
    //------------------------------------------------------------------
#endif

    AxisManipulator::updateManipulatorMenu();

} // _validate()


/*! Request input channels from background and assign output channels.
*/
/*virtual*/
void
zpRender::_request(int x, int y, int r, int t,
                   DD::Image::ChannelMask output_channels,
                   int                    count)
{
#ifdef DEBUG_STARTUP
    std::cout << "zpRender::_request(" << this << "): " << x << ", " << y << ", " << r << ", " << t;
    std::cout << ", channels=" << output_channels << ", count=" << count;
    std::cout << ", changed_mask=0x" << std::hex << m_changed_mask << std::dec;
    std::cout << std::endl;
#endif

    // These are the channels we get from our background input:
    DD::Image::ChannelSet bg_get_channels(output_channels);
    // Need Z if we're clipping:
    if (k_bg_occlusion)
        bg_get_channels += DD::Image::Mask_Z;
    // Need alpha if we're blending:
    if (rtx.k_atmosphere_alpha_blending)
        bg_get_channels += DD::Image::Mask_Alpha;
    // Need mask channel if masking:
    if (k_render_mask_channel != DD::Image::Chan_Black)
        bg_get_channels += k_render_mask_channel;

    // Request the background image source:
    DD::Image::ChannelSet request_channels(bg_get_channels);
    request_channels += input0().channels();
    input0().request(x, y, r, t, request_channels, count);

#ifdef DEBUG_STARTUP
    std::cout << std::hex;
    std::cout << "  bg_get_channels=" << bg_get_channels.value();
    std::cout << ", request_channels=" << request_channels.value();
    std::cout << ", material_channels=" << rtx.material_channels.value();
    std::cout << ", light_channels=" << light_channels.value();
    std::cout << std::dec << std::endl;
#endif

    // *************************************************************************
    //                      **** IMPORTANT ***
    //    If rtx.doTextureRequests() is not called on *every* zpRender::_request()
    //    then Nuke will go into an infinite loop and repeatedly call 
    //    zpRender::_request() forever.
    //
    // *************************************************************************
    // This replaces scene->request():
    DD::Image::ChannelSet get_material_channels(rtx.material_channels);
    get_material_channels &= output_channels;
    rtx.doTextureRequests(get_material_channels, count);

    //==============================================================
    // Update the map of active TextureSamplers:
    // TODO: I'm not sure exactly why this needs to be in request,
    //  but when request is repeatedly called we don't want to
    //  destroy the map repeatedly
    //==============================================================

    rtx.requestTextureSamplers();


    // This should be a combined mask from all lights in the scene...:
    DD::Image::ChannelSet light_channels(DD::Image::Mask_RGB);
    light_channels += DD::Image::Mask_Alpha;    // always need transparency - unless we have a switch...

    doLightRequests(light_channels, count);
}


/*! Iop 2D scanline engine.
    Redirects to the tracerEngine() method.
*/
/*virtual*/
void
zpRender::engine(int y, int x, int r,
                 DD::Image::ChannelMask out_channels,
                 DD::Image::Row&        out_row)
{
#if 0//def DEBUG_ENGINE
    std::cout << "zpRender::engine(" << y << " " << x << ".." << r << "): out_channels=" << out_channels << std::endl;
#endif
    tracerEngine(y, y+1, x, r, out_channels, out_row);
}


#ifdef ENABLE_DEEP
/*!
*/
/*virtual*/ 
void
zpRender::getDeepRequests(DD::Image::Box                       bbox,
                          const DD::Image::ChannelSet&         output_channels,
                          int                                  count,
                          std::vector<DD::Image::RequestData>& reqData)
{
#ifdef DEBUG_STARTUP
    std::cout << "zpRender::getDeepRequests(" << this << "): " << bbox.x() << " " << bbox.y() << " " << bbox.r() << " " << bbox.t();
    std::cout << ", channels=" << output_channels << " count=" << count << std::endl;
#endif

    // Fill in a single request:
#if 0
    doInputRequests(x, y, r, t, output_channels, count);
#else
    // These are the channels we need from our background input:
    DD::Image::ChannelSet bg_input_channels(output_channels);
    // Need Z if we're clipping:
    if (k_bg_occlusion)
        bg_input_channels += DD::Image::Mask_Z;
    // Need alpha if we're blending:
    if (rtx.k_atmosphere_alpha_blending)
        bg_input_channels += DD::Image::Mask_Alpha;
    // Need mask channel if masking:
    if (k_render_mask_channel != DD::Image::Chan_Black)
        bg_input_channels += k_render_mask_channel;

    // Request the background image source:
    DD::Image::ChannelSet request_channels(bg_input_channels);
    request_channels += input0().channels();

    // This should be a combined mask from all lights in the scene...:
    DD::Image::ChannelSet light_channels(DD::Image::Mask_RGB);
    light_channels += DD::Image::Mask_Alpha; // always need transparency - unless we have a switch...

#ifdef DEBUG_STARTUP
    std::cout << std::hex;
    std::cout << "  bg_input_channels=" << bg_input_channels.value();
    std::cout << ", request_channels=" << request_channels.value();
    std::cout << ", material_channels=" << rtx.material_channels.value();
    std::cout << ", light_channels=" << light_channels.value();
    std::cout << std::dec << std::endl;
#endif
#endif

    doLightRequests(light_channels, count);

    reqData.push_back(DD::Image::RequestData(DD::Image::Iop::input(0), bbox, request_channels, count));
}

/*! DeepOp deep tile engine.
    Redirects to the tracerEngine() method.
*/
/*virtual*/
bool
zpRender::doDeepEngine(DD::Image::Box               bbox,
                       const DD::Image::ChannelSet& out_channels,
                       DD::Image::DeepOutputPlane&  deep_out_plane)
{
#if 0//def DEBUG_ENGINE
    std::cout << "zpRender::doDeepEngine(" << bbox.x() << " " << bbox.y() << " " << bbox.r() << " " << bbox.t() << "): out_channels=" << out_channels << std::endl;
#endif

    /*
      DeepOutputPlane(DD::Image::ChannelSet channels,
                      DD::Image::Box box,
                      DeepPixel::Ordering ordering = DeepPixel::eUnordered)
        : DeepPlane(channels, box, ordering)
    */
#if 1
    DD::Image::Row dummy_row(bbox.x(), bbox.r());
    return tracerEngine(bbox.y(), bbox.t(), bbox.x(), bbox.r(), out_channels, dummy_row, &deep_out_plane);
#else
    uint32_t count = 0;
    deep_out_plane = DD::Image::DeepOutputPlane(out_channels, bbox/*, DeepPixel::eZAscending*/);
    for (int yy=bbox.y(); yy < bbox.t(); ++yy)
    {
        for (int xx=bbox.x(); xx < bbox.r(); ++xx)
        {
            deep_out_plane.addHole(DD::Image::DeepOutPixel());
            ++count;
        }
    }
    std::cout << "  count=" << count << std::endl;
    return true;
#endif
}
#endif


/*! */
/*virtual*/
void
zpRender::_close()
{
    //std::cout << "zpRender::_close()" << std::endl;
    /* TODO: Should we delete the ObjectContext's here...?  If we do we'll just
       have to reconstruct the bvhs for any geoinfo that haven't changed.  Perhaps
       we can set the close delay to be 5-10secs...?
    */
    // This crashes the renderer since the engine relies on the allocations...
    //rtx.destroyAllocations(true/*force*/);
    //rtx.destroyObjectBVHs(true/*force*/);
    //rtx.destroyLightBVHs(true/*force*/);

    rtx.destroyTextureSamplers();
}


//----------------------------------------------------------------------------


/*!
*/
void
zpRender::doLightRequests(const ChannelSet& light_channels,
                          int               count)
{
    // Call request on materials and lights:
    for (uint32_t n=0; n < rtx.input_scenes.size(); ++n)
    {
        zpr::Scene* scene = rtx.input_scenes[n];
        assert(scene); // Shouldn't happen...

        const size_t nLights = scene->lights.size();
        for (size_t i=0; i < nLights; ++i)
        {
            LightContext& ltx = *(scene->lights[i]);
            LightOp* l = ltx.light();
            assert(l); // Shouldn't happen...
            if (l->node_disabled())
                continue;

//              if (ltx.depthmap())
//              {
//                  DD::Image::Iop* op = ltx.depthmap();
//                  op->request(op->info().x(), op->info().y(),
//                              op->info().r(), op->info().t(),
//                              DD::Image::Mask_Z, count * 2);
//              }

            // Request RGBA from each light:
            // Though this is also done in Scene::request() above, we want
            // control over the exact set of channels requested.
            // Scene::request() only requests RGBA:
            l->request(light_channels, count);
        }

    }
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


/*! Initialize each sample Scene object and generate it's renderable primitives.

    This DD::Image::Render method is used in zpRender to boostrap the construction
    of the BVHs, while the actual RenderPrimitive creation is deferred when
    a ray hits an ObjectContext BVH leaf.
*/
/*virtual*/
bool
zpRender::generate_render_primitives()
{
    ++rtx.render_version;
#if 0//def DEBUG_STARTUP
    std::cout << "zpRender(" << this << ")::generate_render_primitives(version " << rtx.render_version << ")" << std::endl;
#endif

    assert(rtx.input_scenes.size() > 0 && rtx.input_scenes[0] != 0); // shouldn't happen...

    // Delete any existing info:
    rtx.destroyAllocations(false/*force*/);

    // Initialize the thread map & list:
    rtx.thread_list.reserve(DD::Image::Thread::numThreads);
    //std::cout << "  numThreadContexts=" << DD::Image::Thread::numThreads << std::endl;


    // Print pixel filter coefficient table:
    //Filter::Coefficients pf_cU;
    //m_pixel_filter.get(0.5f, 8, pf_cU);
    //std::cout << "px=" << 8 << " count=" << pf_cU.count << std::endl;
    //for (int i=0; i < pf_cU.count; ++i)
    //   std::cout << pf_cU.array[pf_cU.delta*i] << " ";
    //std::cout << std::endl;


    //==============================================================
    // Sampler initialization:
    //==============================================================

    int randomZ = int(rtx.render_frame*1234.0) + rtx.render_view;//int(Op::hash().value())
    DD::Image::Hash new_sampler_set_hash;
    new_sampler_set_hash.append(randomZ);
    new_sampler_set_hash.append(rtx.k_spatial_jitter_threshold);
    new_sampler_set_hash.append(rtx.ray_single_scatter_samples);
    new_sampler_set_hash.append(rtx.ray_diffuse_samples);
    new_sampler_set_hash.append(rtx.ray_glossy_samples);
    new_sampler_set_hash.append(rtx.ray_refraction_samples);
    if (!m_sampler_set || m_sampler_set_hash != new_sampler_set_hash)
    {
        m_sampler_set_hash = new_sampler_set_hash;
        delete m_sampler_set;

        m_sampler_set = new SamplerSet(getRaySampleSideCount(m_pixel_sample_mode)/*sample_side_count*/, 512/*sets*/);
        m_sampler_set->m_diffuse_side_count    = getRaySampleSideCount(m_ray_diffuse_samples);
        m_sampler_set->m_glossy_side_count     = getRaySampleSideCount(m_ray_glossy_samples);
        m_sampler_set->m_refraction_side_count = getRaySampleSideCount(m_ray_refraction_samples);

        m_sampler_set->initialize(randomZ, rtx.k_spatial_jitter_threshold);
    }


    //==============================================================
    // Resize the camera lists. If not doing stereo the hero camera
    // list is empty.
    // TODO: this should be in a RenderContext method.
    //==============================================================
    const bool make_hero_cameras = (rtx.k_copy_specular && rtx.render_view != rtx.k_hero_view);

    for (uint32_t i=0; i < rtx.ray_cameras.size(); ++i)
        delete rtx.ray_cameras[i];
    rtx.ray_cameras.clear();
    for (uint32_t i=0; i < rtx.hero_ray_cameras.size(); ++i)
        delete rtx.hero_ray_cameras[i];
    rtx.hero_ray_cameras.clear();

    rtx.ray_cameras.resize(rtx.shutter_scenerefs.size(), NULL);
    if (make_hero_cameras)
        rtx.hero_ray_cameras.resize(rtx.shutter_scenerefs.size(), NULL);


    //==============================================================
    // Build Scene xforms:
    //==============================================================

    // Update the object transforms for ALL scenes, including
    // any extra motionblur scenes in the list.
    //std::cout << "  build objects - shutter_times[";
    const uint32_t nScenes = (uint32_t)rtx.shutter_scenerefs.size();
    for (uint32_t i=0; i < nScenes; ++i)
    {
        const ShutterSceneRef& sref = rtx.shutter_scenerefs[i];
        const uint32_t input_sample = sref.op_input_index;
        //std::cout << " " << i << ":" << input_sample;
        zpr::Scene* input_scene = sref.scene;
#if DEBUG
        assert(input_scene); // Shouldn't happen...
#endif

        const DD::Image::CameraOp* mb_cam = NULL;
        uint32_t next_input_sample = input_sample;
        if (i < nScenes-1)
        {
           next_input_sample = rtx.shutter_scenerefs[i+1].op_input_index;
           mb_cam = rtx.shutter_scenerefs[i+1].camera;
        }

        // Build ray camera:
        int proj_mode = rtx.k_projection_mode;
        if (proj_mode == PROJECTION_RENDER_CAMERA)
        {
            // Map camera projection modes to render projection modes.
            // Unsupported modes default to perspective.
            switch (sref.camera->projection_mode())
            {
                default:
                case DD::Image::CameraOp::LENS_PERSPECTIVE:  proj_mode = PROJECTION_PERSPECTIVE; break;
                case DD::Image::CameraOp::LENS_ORTHOGRAPHIC: proj_mode = PROJECTION_PERSPECTIVE; break;//PROJECTION_ORTHOGRAPHIC
                case DD::Image::CameraOp::LENS_UV:           proj_mode = PROJECTION_PERSPECTIVE; break;//PROJECTION_UV
                case DD::Image::CameraOp::LENS_SPHERICAL:    proj_mode = PROJECTION_SPHERICAL;   break;
                case DD::Image::CameraOp::LENS_USER_CAMERA:  proj_mode = PROJECTION_PERSPECTIVE; break;
            }
        }

        switch (proj_mode)
        {
            case PROJECTION_PERSPECTIVE:
                rtx.ray_cameras[i] = new zpr::RayPerspectiveCamera();
                if (make_hero_cameras)
                    rtx.hero_ray_cameras[i] = new zpr::RayPerspectiveCamera();
                break;

            //case PROJECTION_ORTHOGRAPHIC:
            //    rtx.ray_cameras[i] = new zpr::RayCamera();
            //    if (make_hero_cameras)
            //        rtx.hero_ray_cameras[i] = new zpr::RayOrthoCamera();
            //    break;

            //case PROJECTION_UV:
            //    rtx.ray_cameras[i] = new zpr::RayUVCamera();
            //    if (make_hero_cameras)
            //        rtx.hero_ray_cameras[i] = new zpr::RayUVCamera();
            //    break;

            case PROJECTION_SPHERICAL:
                rtx.ray_cameras[i] = new zpr::RaySphericalCamera();
                if (make_hero_cameras)
                    rtx.hero_ray_cameras[i] = new zpr::RaySphericalCamera();
                break;

            case PROJECTION_CYLINDRICAL:
                rtx.ray_cameras[i] = new zpr::RayCylindricalCamera();
                break;

        }
#if DEBUG
        assert(rtx.ray_cameras[i]);
#endif
        rtx.ray_cameras[i]->build(rtx,
                                  sref.camera/*cam0*/,
                                  mb_cam/*cam1*/,
                                  outputContext());

        // Build hero ray camera if current view is not the hero:
        if (make_hero_cameras)
        {
            // Not hero view (like RGT), copy the cam vectors from the hero camera:
            const DD::Image::CameraOp* camHero0 =
                getInputCameraOpForSampleAndView(input_sample/*sample*/, (rtx.k_hero_view-rtx.render_views[0]));
            const DD::Image::CameraOp* camHero1 =
                (i < nScenes-1) ? 
                    getInputCameraOpForSampleAndView(next_input_sample/*sample*/, (rtx.k_hero_view-rtx.render_views[0])) : NULL;
#if DEBUG
            assert(camHero0); // shouldn't happen...
#endif

            // Copy the current context, set view to hero and build the render camera:
            DD::Image::OutputContext hero_context = outputContext();
            hero_context.setView(rtx.k_hero_view);
            //std::cout << "Make hero cameras, view=" << hero_context.viewname() << std::endl;

            //Fsr::Vec3d V = -rtx.ray_cameras[i].cam0.matrix.getZAxis();
            //std::cout << "     V" << V << std::endl;

            rtx.hero_ray_cameras[i]->build(rtx, camHero0, camHero1, hero_context);
            //Fsr::Vec3d heroV = -rtx.hero_ray_cameras[i].cam0.matrix.getZAxis();
            //std::cout << " heroV" << heroV << std::endl;
        }

        const uint32_t nObjects = input_scene->objects();
        input_scene->object_transforms_list().clear();
        input_scene->object_transforms_list().reserve(nObjects);
        for (uint32_t obj=0; obj < nObjects; ++obj)
        {
            // Bail quickly on user-interrupt:
            if (DD::Image::Op::aborted())
            {
#ifdef DEBUG_ABORTED
                std::cout << "    ******** generate_render_primitives(): engine aborted ********" << std::endl;         
#endif
                return false;
            }

            DD::Image::GeoInfo& info = input_scene->object(obj);

            // Create an MatrixArray that's unique for each object.
            // Copy the base scene transforms:
            input_scene->object_transforms_list().push_back(*input_scene->transforms());
            // Update the object matrix:
            input_scene->object_transforms_list()[obj].set_object_matrix(info.matrix);
            //std::cout << " " << i << " info matrix:" << info.matrix << std::endl;
        }

        // Transform the lights and let lights that generate atmosphere volumes
        // add themselves to the surface contexts:
        if (!input_scene->evaluate_lights())
        {
#ifdef DEBUG_ABORTED
            std::cout << "    ******** generate_render_primitives(): engine aborted ********" << std::endl;         
#endif
            return false;
        }

    } // nScenes loop
    //std::cout << "]" << std::endl;

    const uint32_t nShutterSamples = rtx.numShutterSamples();
#if DEBUG
    assert(nShutterSamples > 0); // shouldn't happen...
    assert(rtx.shutter_scenerefs.size() > 0);
#endif

    zpr::Scene* scene0 = rtx.shutter_scenerefs[0].scene;
    uint32_t nObjects = (uint32_t)scene0->objects();
    uint32_t nLights  = (uint32_t)scene0->lights.size();

    //if (!rtx.objects_bvh_initialized)
    {
        // These lists are passed to the BVHs to build them:
        std::vector<zpr::ObjectContextRef> objref_list;
        objref_list.reserve(nObjects);

        rtx.destroyObjectBVHs(true/*force*/);

        //==============================================================
        // Geometry Objects:
        //==============================================================

        // Map of objects we're keeping:
        std::map<uint64_t, GeoInfoContext*> keep_map;

        for (uint32_t obj_index=0; obj_index < nObjects; ++obj_index)
        {
            // Bail quickly on user-interrupt:
            if (DD::Image::Op::aborted())
            {
#ifdef DEBUG_ABORTED
                std::cout << "    ******** generate_render_primitives(): engine aborted ********" << std::endl;         
#endif
                return false;
            }

            // Build a GeoInfoContext:
            GeoInfoContext* gptx = new GeoInfoContext();
            gptx->motion_objects.reserve(nShutterSamples);
            gptx->motion_times.reserve(nShutterSamples);
            gptx->motion_geoinfos.reserve(nShutterSamples);

            // Get the GeoInfo motion samples. If the GeoInfo's don't all match
            // the first sample (sample 0) then the sample will be skipped.
            // Store motion sample 0:
            GeoInfoContext::Sample& gtx0 = gptx->addGeoInfoSample(scene0, obj_index);
            // Replace the local-to-world xform to include the global xform:
            gtx0.l2w = rtx.global_xform;
            gtx0.l2w *= gtx0.info->matrix;
            gtx0.w2l = gtx0.l2w.inverse();

            gptx->enabled_lights.clear();

            gptx->hash.reset();
            gptx->hash.append(gtx0.info->out_id());
            gptx->hash.append(gtx0.info->vertices());
            if (gtx0.info->point_array())
                gptx->hash.append(gtx0.info->point_array(), sizeof(void*));

            // AttribContextList attributes;
            if (gtx0.info->material)
                gptx->hash.append(gtx0.info->material, sizeof(void*));

            Fsr::Box3d bbox0(gtx0.info->bbox());
            // Do the primitives inside the GeoInfo expand the bbox further than the
            // point values imply? This is material displacement done below.
            // Example is a PointCloud with point radii that expand the points into
            // spheres or discs.
            //
            const DD::Image::Primitive** prim_array = gtx0.info->primitive_array();
            if (prim_array)
            {
                gptx->hash.append(gtx0.info->primitive_array(), sizeof(void*));

                const uint32_t nPrims = gtx0.info->primitives();
                for (uint32_t j=0; j < nPrims; ++j)
                {
                    const DD::Image::Primitive* prim = *prim_array++;
                    if (prim->getPrimitiveType() > DD::Image::ePrimitiveTypeCount ||
                        prim->getPrimitiveType() == DD::Image::eParticlesSprite)
                        bbox0.expand(prim->get_bbox(gtx0.info));
                }
            }

            // Determine displacement for this object:
            if (gtx0.info->material)
            {
                // Apply in local-space:
                const float displace = gtx0.info->material->displacement_bound();
                if (displace > std::numeric_limits<float>::epsilon())
                    bbox0.pad(Fsr::Vec3f(displace, displace, displace));
            }

            gptx->bbox = gtx0.l2w.transform(bbox0);
            //std::cout << "[" << obj_index << "]: hash=0x" << std::hex << gptx->hash.value() << std::dec;
            //std::cout << ", prim=" << gtx0.info->primitive_array()[0];
            //std::cout << ", bbox" << gptx->bbox;
            //std::cout << std::endl;

            // Match the motion-blurred GeoInfo's together:
            uint32_t nMotionSamples = 1;
            if (rtx.isMotionBlurEnabled())
            {
                // Build motion sample list:
                DD::Image::GeoInfo* current_info = gtx0.info;
                zpr::Scene* this_scene = scene0;
                zpr::Scene* next_scene = (zpr::Scene*)this_scene->mb_scene();
                while (1)
                {
                    //std::cout << "  this_scene(" << this_scene << "), next_scene(" << next_scene << ")" << std::endl;
                    if (current_info == NULL || next_scene == NULL)
                        break;

                    //std::cout << "    checking motion object 0x" << current_info->out_id() << " ptr=" << current_info << std::endl;
                    // Find matching object id in motionblur scene object map:
                    const int next_obj_index = next_scene->findObject(current_info->out_id().value());
                    if (next_obj_index < 0)
                        break; // not found
                    //std::cout << "      match in scene " << next_scene << std::endl;

                    DD::Image::GeoInfo* next_info = &next_scene->object(next_obj_index);
                    assert(next_info); // shouldn't happen...

                    // Make sure primitives and attribute references are up-to-date:
                    GeoInfoContext::Sample& gtx = gptx->addGeoInfoSample(next_scene, next_obj_index);
                    // Replace the local-to-world xform to include the global xform:
                    gtx.l2w = rtx.global_xform;
                    gtx.l2w *= gtx.info->matrix;
                    gtx.w2l = gtx.l2w.inverse();

                    gptx->hash.append(gtx.info->out_id());
                    gptx->hash.append(gtx.info->vertices());
                    if (gtx.info->point_array())
                        gptx->hash.append(gtx.info->point_array(), sizeof(void*));
                    //AttribContextList attributes;
                    if (gtx.info->material)
                        gptx->hash.append(gtx.info->material, sizeof(void*));

                    //if (Fsr::getObjectString(*current_info, "name")=="string1")
                    //{
                    //std::cout << "'string1'[0]=" << obj_index << ", bbox=" << gtx0.info->bbox() << ", l2w=" << gtx0.l2w;
                    //std::cout << std::endl;
                    //std::cout << "'" << Fsr::getObjectString(*next_info, "name") << "'[" << shutter_sample << "]=" << next_obj_index << ", bbox=" << gtx.info->bbox() << ", l2w=" << gtx.l2w;
                    //std::cout << std::endl;
                    //}

                    Fsr::Box3d bbox(gtx.info->bbox());
                    // Do the primitives inside the GeoInfo expand the bbox further than the
                    // point values imply? This is material displacement, that's done below.
                    // Example is a PointCloud with point radii that expand the points into
                    // spheres or discs.
                    //
                    const DD::Image::Primitive** prim_array = gtx.info->primitive_array();
                    if (prim_array)
                    {
                        gptx->hash.append(gtx.info->primitive_array(), sizeof(void*));

                        const uint32_t nPrims = gtx.info->primitives();
                        for (uint32_t j=0; j < nPrims; ++j)
                        {
                            const DD::Image::Primitive* prim = *prim_array++;
                            if (prim->getPrimitiveType() > DD::Image::ePrimitiveTypeCount ||
                                prim->getPrimitiveType() == DD::Image::eParticlesSprite)
                                bbox.expand(prim->get_bbox(gtx.info));
                        }
                    }

                    gptx->bbox.expand(gtx.l2w.transform(bbox));
                    //std::cout << "[" << obj_index << "]: hash=0x" << std::hex << gptx->hash.value() << std::dec;
                    //std::cout << ", prim=" << gtx.info->primitive_array()[0];
                    //std::cout << ", bbox" << gptx->bbox;
                    //std::cout << std::endl;

                    current_info = next_info;
                    this_scene = next_scene;
                    next_scene = (zpr::Scene*)this_scene->mb_scene();

                    ++nMotionSamples;
                }
            }
            gptx->hash.append(nMotionSamples); // make sure motion_sample count is taken into account
//gptx->bbox.append(gptx->hash);
            // Force it to change every render pass:
            gptx->hash.append(rtx.render_version);

            // Build the list of enabled lights for this object:
            if (rtx.direct_lighting_enabled)
            {
                //std::cout << "  lighting enabled - link lights to objects:" << std::endl;
                std::set<uint32_t> light_mask_enabled;
                if (Fsr::hasObjectAttrib(*gtx0.info, "light_mask"))
                {
                    std::string light_mask = Fsr::getObjectString(*gtx0.info, "light_mask");
                    //std::cout << "    light_mask('" << light_mask << "'):" << std::endl;
                    // Special-case the default '*' value:
                    if (light_mask == "*")
                    {
                        for (uint32_t lt=0; lt < nLights; ++lt)
                        {
                            DD::Image::LightContext* ltx = scene0->lights[lt];
                            assert(ltx); // Shouldn't happen...
                            if (ltx->light()->node_disabled())
                                continue;
                            light_mask_enabled.insert(lt);
                        }
                    }
                    else if (light_mask.empty())
                    {
                        // do nothing
                    }
                    else
                    {
                        // Tokenize the light_mask string:
                        std::vector<std::string> masks;
                        Fsr::stringSplit(light_mask, ", \t\n", masks);
                        if (masks.size() > 0)
                        {
                            for (uint32_t lt=0; lt < nLights; ++lt)
                            {
                                DD::Image::LightContext* ltx = scene0->lights[lt];
                                assert(ltx); // Shouldn't happen...
                                if (ltx->light()->node_disabled())
                                    continue;
                                //std::cout << "    checking light '" << ltx->light()->node_name() << "'" << std::endl;

                                // Check for identifer knob first, otherwise default to node name:
                                std::string light_id;
                                DD::Image::Knob* k = ltx->light()->knob("light_identifier");
                                if (k && k->get_text())
                                    light_id = k->get_text();
                                if (light_id.empty())
                                    light_id = ltx->light()->node_name(); // No id, default to node name
                                if (light_id.empty())
                                    continue; // shouldn't happen...

                                // Check against each mask:
                                for (uint32_t i=0; i < masks.size(); ++i)
                                {
                                    const std::string& mask = masks[i];
                                    if ((mask[0] == '-' || mask[0] == '^') && Fsr::globMatch(mask.c_str()+1, light_id.c_str()))
                                        light_mask_enabled.erase(lt);
                                    else if (mask[0] == '+' && Fsr::globMatch(mask.c_str()+1, light_id.c_str()))
                                        light_mask_enabled.insert(lt);
                                    else if (Fsr::globMatch(mask, light_id))
                                        light_mask_enabled.insert(lt);
                                }
                            }
                        }
                    }

                }
                else
                {
                    //std::cout << "    no light mask on object, add all lights:" << std::endl;
                    // No light mask, add all lights:
                    for (uint32_t lt=0; lt < nLights; ++lt)
                    {
                        DD::Image::LightContext* ltx = scene0->lights[lt];
                        assert(ltx); // Shouldn't happen...
                        if (ltx->light()->node_disabled())
                            continue;
                        light_mask_enabled.insert(lt);
                    }
                }
                //std::cout << "      enabled lights[";
                //for (std::set<uint32_t>::const_iterator it=light_mask_enabled.begin(); it != light_mask_enabled.end(); ++it)
                //    std::cout << " " << scene0->lights[*it]->light()->node_name() << ",";
                //std::cout << " ]" << std::endl;


                // Now check the object mask in each enabled light the object name:
                std::string obj_name;
                if (Fsr::hasObjectAttrib(*gtx0.info, "scene_path"))
                    obj_name = Fsr::getObjectString(*gtx0.info, "scene_path");
                else if (Fsr::hasObjectAttrib(*gtx0.info, "name"))
                    obj_name = Fsr::getObjectString(*gtx0.info, "name");
                else
                {
                    obj_name = "unnamed";
                }
                //std::cout << "    object name='" << obj_name << "'" << std::endl;

                for (std::set<uint32_t>::const_iterator it=light_mask_enabled.begin();
                      it != light_mask_enabled.end(); ++it)
                {
                    DD::Image::LightContext* ltx = scene0->lights[*it];
                    assert(ltx); // Shouldn't happen...

                    // Check for identifer knob first, otherwise default to node name:
                    std::string object_mask;
                    DD::Image::Knob* k = ltx->light()->knob("object_mask");
                    if (!k)
                    {
                        // If light doesn't have an object mask control always enable it:
                        gptx->enabled_lights.insert(*it);
                        continue;
                    }
                    object_mask = k->get_text();
                    //std::cout << "      '" << ltx->light()->node_name() << "': object_mask='" << object_mask << "'" << std::endl;
                    if (object_mask.empty())
                        continue;

                    // Tokenize the light_mask string:
                    std::vector<std::string> masks;
                    Fsr::stringSplit(object_mask, ", \t\n", masks);
                    if (masks.size() == 0)
                        continue;

                    // Check enabled lights against each mask:
                    for (uint32_t i=0; i < masks.size(); ++i)
                    {
                        const std::string& mask = masks[i];
                        //std::cout << "        light '" << ltx->light()->node_name() << "': obj_mask='" << mask << "'" << std::endl;
                        if ((mask[0] == '-' || mask[0] == '^') && Fsr::globMatch(mask.c_str()+1, obj_name.c_str()))
                            gptx->enabled_lights.erase(*it);
                        else if (mask[0] == '+' && Fsr::globMatch(mask.c_str()+1, obj_name.c_str()))
                            gptx->enabled_lights.insert(*it);
                        else if (Fsr::globMatch(mask, obj_name))
                            gptx->enabled_lights.insert(*it);
                    }
                }
                //std::cout << "      enabled lights[";
                //for (std::set<uint32_t>::const_iterator it=gptx->enabled_lights.begin(); it != gptx->enabled_lights.end(); ++it)
                //    std::cout << " " << scene0->lights[*it]->light()->node_name() << ",";
                //std::cout << " ]" << std::endl;

            } // lighting enabled

#if 1
            rtx.object_context.push_back(gptx);
            objref_list.push_back(zpr::ObjectContextRef(gptx, gptx->bbox));
            //std::cout << "  obj[" << obj_index << "]: info=" << gtx0.info;
            //std::cout << ", bbox(non-xformed)" << Fsr::Box3f(gtx0.info->bbox());
            //std::cout << ", otx->bbox(xformed)" << gptx->bbox << std::endl;

#else
            // Determine if this context matches any existing ones in the map:
            std::map<uint64_t, uint32_t>::iterator it = rtx.object_map.find(gptx->hash.value());
            if (it == rtx.object_map.end())
            {
                // Not in current map, add to keep map:
                keep_map[gptx->hash.value()] = gptx;
            }
            else
            {
                // Already in map, add it to keep map and delete the temp gptx:
                GeoInfoContext* existing_gptx = rtx.object_context[it->second];
                keep_map[gptx->hash.value()] = existing_gptx;
                // Update the existing context with the corrected GeoInfo
                // indices:  TODO: change this so that the indices aren't required...
                existing_gptx->motion_geoinfos = gptx->motion_geoinfos;
                delete gptx;
            }
#endif

        } // nObjects loop


#if 0
        // Delete the ones not in keep map:
        nObjects = rtx.object_context.size();
        for (uint32_t i=0; i < nObjects; ++i)
        {
            GeoInfoContext* gptx = rtx.object_context[i];
            std::map<uint64_t, GeoInfoContext*>::iterator it = keep_map.find(gptx->hash.value());
            if (it == keep_map.end())
            {
                delete gptx;
                rtx.object_context[i] = 0;
            }
        }

        // Rebuild list & map from keep map:
        nObjects = keep_map.size();
        //
        rtx.object_context.clear();
        rtx.object_context.reserve(nObjects + nLights);

        rtx.object_map.clear();
        std::map<uint64_t, GeoInfoContext*>::iterator it = keep_map.begin();
        for (; it != keep_map.end(); ++it)
        {
            GeoInfoContext* gptx = it->second;
            rtx.object_context.push_back(gptx);
            const uint32_t obj = rtx.object_context.size()-1;
            rtx.object_map[gptx->hash.value()] = obj;
            gptx->index = obj; // update the index

            objref_list.push_back(zpr::ObjectContextRef(gptx, gptx->bbox));
            //std::cout << "obj[" << obj << "]: bbox=" << gptx->bbox << std::endl;
        }
#endif

        //==============================================================
        // Build BVH:
        //==============================================================

        rtx.bvh_max_depth   = k_bvh_max_depth;
        rtx.bvh_max_objects = k_bvh_max_objects_per_leaf;

        // Build the primary intersection test BVH, which is simply the bboxes of all the ObjectContexts:
        if (objref_list.size() > 0)
        {
            rtx.objects_bvh.build(objref_list, 1/*max_objects_per_leaf*/);
            rtx.objects_bvh.setName("object_bvh");
            rtx.objects_bvh.setGlobalOrigin(Fsr::Vec3d(0,0,0));
            //std::cout << "    object_bvh" << rtx.objects_bvh.bbox() << ", depth=" << rtx.objects_bvh.maxNodeDepth() << std::endl;
        }

        rtx.objects_bvh_initialized = true;
    }



    //if (!rtx.lights_bvh_initialized)
    {
        // These lists are passed to the BVHs to build them:
        std::vector<zpr::ObjectContextRef> ltvref_list;
        ltvref_list.reserve(nLights);

        rtx.destroyLightBVHs(true/*force*/);

        //==============================================================
        // Light Volume Objects:
        //==============================================================
        if (rtx.atmospheric_lighting_enabled)
        {
            for (uint32_t lt_index=0; lt_index < nLights; ++lt_index)
            {
                // Bail quickly on user-interrupt:
                if (Op::aborted())
                {
#ifdef DEBUG_ABORTED
                    std::cout << "    ******** generate_render_primitives(): engine aborted ********" << std::endl;         
#endif
                    return false;
                }

                DD::Image::LightContext* ltx = scene0->lights[lt_index];
                assert(ltx); // Shouldn't happen...
                if (ltx->light()->node_disabled())
                    continue; // skip it if it's off

                // Get the bbox, but also whether this light can produce a volume:
                Fsr::Box3d lt_bbox;
                zpr::SourcePrimitiveType prim_type = rtx.getVolumeLightTypeAndBbox(ltx->light(), lt_bbox);
                if (prim_type == zpr::UNRECOGNIZED_PRIM)
                    continue;

                // Build a LightVolumeContext:
                LightVolumeContext* otx = new LightVolumeContext();
                otx->motion_objects.reserve(nShutterSamples);
                otx->motion_times.reserve(nShutterSamples);
                otx->motion_lights.reserve(nShutterSamples);

                // Store sample 0:
                otx->addLightVolumeSample(scene0, lt_index);

                otx->hash.reset();
                otx->hash.append(nShutterSamples);
                otx->hash.append(ltx->light()->hash());
                otx->bbox = lt_bbox;

                // Match the motion-blurred GeoInfo's together:
                uint32_t nMotionSamples = 1;
                if (rtx.isMotionBlurEnabled())
                {
                    // Build motion sample list:
                    DD::Image::LightContext* current_ltx = ltx;
                    zpr::Scene* this_scene = scene0;
                    zpr::Scene* next_scene = (zpr::Scene*)this_scene->mb_scene();
                    while (1)
                    {
                        //std::cout << "  this_scene(" << this_scene << "), next_scene(" << next_scene << ")" << std::endl;
                        if (current_ltx == 0 || next_scene == 0)
                            break;

                        // Verify that the next light is from the same node and has same prim type:
                        DD::Image::LightContext* next_ltx = next_scene->lights[lt_index];
                        zpr::SourcePrimitiveType next_prim_type = rtx.getVolumeLightTypeAndBbox(next_ltx->light(), lt_bbox);
                        if (next_prim_type != prim_type ||
                            current_ltx->light()->node() != next_ltx->light()->node())
                        {
                            // shouldn't happen...
                            std::cerr << "light prim type or index mismatch!" << std::endl;
                            break;
                        }

                        otx->addLightVolumeSample(next_scene, lt_index);

                        otx->hash.append(next_ltx->light()->hash());
                        otx->bbox.expand(lt_bbox);

                        current_ltx = next_ltx;
                        this_scene = next_scene;
                        next_scene = (zpr::Scene*)this_scene->mb_scene();

                        ++nMotionSamples;
                    }

                    //std::cout << "[" << ltx << " " << ltx << "]: hash=" << std::hex << otx->hash.value() << std::dec;
                    //std::cout << std::endl;
                }
                otx->hash.append(nMotionSamples); // make sure motion_sample count is taken into account
                otx->bbox.append(otx->hash);
                // Force it to change every render pass:
                otx->hash.append(rtx.render_version);

                rtx.light_context.push_back(otx);

                ltvref_list.push_back(zpr::ObjectContextRef(otx, otx->bbox));
                //std::cout << "  lt[" << otx << "]: bbox=" << otx->bbox << std::endl;

            } // nLights loop
        }

        //==============================================================
        // Build BVH:
        //==============================================================

        rtx.bvh_max_depth   = k_bvh_max_depth;
        rtx.bvh_max_objects = k_bvh_max_objects_per_leaf;

        // Build the primary intersection test BVH, which is simply the bboxes of all the ObjectContexts:
        if (ltvref_list.size() > 0)
        {
            rtx.lights_bvh.build(ltvref_list, 1/*max_objects_per_leaf*/);
            rtx.objects_bvh.setName("lights_bvh");
            rtx.objects_bvh.setGlobalOrigin(Fsr::Vec3d(0,0,0));
            //std::cout << "    lights_bvh" << rtx.lights_bvh.bbox() << ", depth=" << rtx.lights_bvh.maxNodeDepth() << std::endl;
        }

        rtx.lights_bvh_initialized = true;
    }

    //==============================================================
    // Build Light Shaders:
    //==============================================================

    rtx.buildLightShaders();


    // Ok we're done:
    rtx.objects_initialized = true;

    return true;
}


} // namespace zpr

// end of zpRender.cpp

//
// Copyright 2020 DreamWorks Animation
//
