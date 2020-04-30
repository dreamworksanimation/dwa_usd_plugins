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

/// @file zpRender.h
///
/// @author Jonathan Egstad


#ifndef zprender_zpRender_h
#define zprender_zpRender_h

#include "AxisManipulator.h"//<zprender/AxisManipulator.h>
#include "zpSamplerSet.h"

#include <zprender/RenderContext.h>
#include <zprender/Volume.h>
#include <zprender/VolumeShaderOp.h>

#include <DDImage/Render.h>
#include <DDImage/DeepOp.h>
#include <DDImage/LookupCurves.h>


#define NUM_AOV_OUTPUTS  10
#define NUM_NOISE_FUNC   3

//#define ENABLE_VOLUME_LIGHTING 1
//#define TRY_UV_MODE 1

//#define DEBUG_STARTUP 1
//#define DEBUG_ENGINE  1
//#define DEBUG_ABORTED 1


namespace zpr {


/*!
*/
class zpRender : public DD::Image::Render,
                 public DD::Image::DeepOp,
                 public zpr::AxisManipulator
{
  protected:
    enum { SHUTTER_STOCHASTIC, SHUTTER_SLICE, SHUTTER_OFFSET };
    enum { GLOBAL_XFORM_OFF, GLOBAL_XFORM_CAM_OPEN, GLOBAL_XFORM_MANUAL };
    enum { NOISE_FBM, NOISE_TURBULENCE };
    // TODO: support more of these cameras
    enum
    {
        PROJECTION_PERSPECTIVE=0,
        //PROJECTION_ORTHOGRAPHIC,
        //PROJECTION_UV,
        PROJECTION_SPHERICAL,
        //PROJECTION_CYLINDRICAL,
        PROJECTION_RENDER_CAMERA
    };


    //=======================================================
    // Shared rendering context:
    RenderContext rtx;                          //!< Render context - holds the data shared between threads


    //=======================================================
    //
    int         k_global_xform_mode;            //!<
    Fsr::Vec3d  k_global_offset;                //!<


    //=======================================================
    //
    bool        k_shade_subsamples;             //!<  TODO: deprecate?
    //
    int         k_bvh_max_depth;                //!< TODO: deprecate?
    int         k_bvh_max_objects_per_leaf;     //!< TODO: deprecate?
    //
    int         k_shutter_steps_preview;        //!< Shutter steps in preview mode (gui)
    int         k_shutter_steps;                //!< Shutter steps for render
    //
    int         k_pixel_sample_mode_preview;    //!< Pixel sample mode in preview mode (gui)
    int         k_pixel_sample_mode;            //!< Pixel sample mode for render
    //
    DD::Image::Channel k_coverage_chan;         //!< Channel to write coverage info into
    DD::Image::Channel k_cutout_channel;        //!< Channel to use for cutout logic
    DD::Image::Channel k_render_mask_channel;   //!< Channel to use for render mask
    //
    float       k_render_mask_threshold;
    bool        k_invert_render_mask;
    //
    int         k_shutter_mode;                 //!<
    double      k_scene_time_offset;            //!< Global frame offset for scene input nodes (geo, camera, etc)
    //
    int         k_uv_mode_object_index;         //!< Object index to use for uv mode
    int         k_uv_mode_surface_index;        //!< Surface index to use for uv mode
    int         k_uv_mode_tile_index[2];        //!< Tile index to use for uv mode


    DD::Image::TextureFilter k_texture_filter_preview;   //!< Default filter to use for texture filtering in preview mode


    //=======================================================
    // Ray tracing / volume rendering:
    bool        k_one_over_Z;
    bool        k_persp_correct_z;
    bool        k_bg_occlusion;
    bool        k_ray_use_camera_near_plane;
    double      k_ray_near_plane;
    bool        k_ray_use_camera_far_plane;
    double      k_ray_far_plane;

    int         k_ray_single_scatter_samples;       //!< Camera ray samples
    int         k_ray_diffuse_samples_preview;      //!< Diffuse ray samples in preview mode
    int         k_ray_diffuse_samples;              //!< Diffuse ray samples
    int         k_ray_glossy_samples_preview;       //!< Glossy ray samples in preview mode
    int         k_ray_glossy_samples;               //!< Glossy ray samples
    int         k_ray_refraction_samples_preview;   //!< Refraction ray samples in preview mode
    int         k_ray_refraction_samples;           //!< Refraction ray samples


    //=======================================================
    // Deep Options:
    bool               k_deep_output_subpixel_masks;
    DD::Image::Channel k_spmask_channel[3];              //!< 
    double             k_deep_combine_threshold;


    //=======================================================
    // AOV outputs:
    bool               k_aov_enable[NUM_AOV_OUTPUTS];
    const char*        k_aov_name[NUM_AOV_OUTPUTS];
    int                k_aov_unpremult[NUM_AOV_OUTPUTS];
    int                k_aov_merge_mode[NUM_AOV_OUTPUTS];
    DD::Image::Channel k_aov_output[NUM_AOV_OUTPUTS][3];

    bool    k_use_deep;
    bool    k_render_only;


    //=======================================================
    // Atmospheric ray-marching:
    VolumeShaderOp k_ambient_volume;


    //=======================================================
    // Derived values:
    bool m_render_views_invalid;        //!< Re-render state
    int  m_pixel_sample_mode;           //!< Number of pixel samples - selection list
    int  m_ray_diffuse_samples;         //!< Diffuse ray samples
    int  m_ray_glossy_samples;          //!< Glossy ray samples
    int  m_ray_refraction_samples;      //!< Refraction ray samples

    bool m_have_bg_Z;

    SamplerSet*       m_sampler_set;        //!< 
    DD::Image::Hash   m_sampler_set_hash;   //!< Indicates when the sampler set needs to be rebuilt
    DD::Image::Filter m_pixel_filter;       //!< Output pixel filter

    const char* m_aov_knob_names[NUM_AOV_OUTPUTS][5];

    Fsr::Pixel  m_black;   //!< Filled with zeroes


  public:
    static const Description description;
    const char* Class() const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "zpRender: new implementation of ScanlineRenderer that supports true 3D motion-blur "
        "with stochastic sampling and subpixel sampling rates from 1x1 to 64x64.\n"
        "For for usage info check the tooltips on the controls.";
    }


    //!
    zpRender(::Node* node);
    ~zpRender();

    //! Calculate the absolute frame from sample number and base frame.
    double getFrameForSample(uint32_t sample,
                             double   base_frame) const;


    //! Returns the camera attached to input 2+(sample*nViews + view). 
    DD::Image::CameraOp* getInputCameraOpForSampleAndView(uint32_t sample,
                                                          int32_t  view);

    //! Returns the GeoOp connected to input 1 for \n sample.
    DD::Image::GeoOp*    getInputGeoOpForSample(uint32_t sample);


    //! Update enabled views, cleaning up blank ones.
    void updateRenderViews();


    //! TODO: deprecate
    DD::Image::Matrix4 getFormatMatrix(float dx,
                                       float dy);


    //--------------------------------------------------
    // From AxisManipulator class

    //!
    /*virtual*/ DD::Image::Op*    manipulatorOp() { return this; }

    //!
    /*virtual*/ DD::Image::Scene* manipulatorScene()
    {
        if (rtx.input_scenes.size() == 0)
            return NULL;
        // Always return the first scene in the rtx.input_scenes list, this is *always* the
        // OutputContext frame which we need to use for manipulation since any offset
        // causes keyframes at the wrong frame:
        return rtx.input_scenes[0];
    }

    //!
    /*virtual*/ bool intersectScene(DD::Image::ViewerContext* ctx,
                                    Fsr::Vec3d&               camPW,
                                    Fsr::Vec3d&               camV,
                                    Fsr::Vec3d&               surfPW,
                                    Fsr::Vec3d&               surfN);


    //--------------------------------------------------
    // zpr Raytracing:


    //!
    int getRaySampleSideCount(int mode) const;

    //!
    void doLightRequests(const DD::Image::ChannelSet& light_channels,
                         int                          count);

    //! Raytracing engine entry point.
    bool tracerEngine(int y, int t, int x, int r,
                      DD::Image::ChannelMask      out_channels,
                      DD::Image::Row&             out_row,
                      DD::Image::DeepOutputPlane* deep_out_plane=NULL);


    //--------------------------------------------------
    // From DD::Image::Op:

    /*virtual*/ int minimum_inputs() const { return 3; }
    /*virtual*/ int maximum_inputs() const { return 3; }

    //!
    /*virtual*/ bool           test_input(int            input,
                                          DD::Image::Op* op) const;

    //!
    /*virtual*/ DD::Image::Op* default_input(int input) const;

    //!
    /*virtual*/ const char*    input_label(int   input,
                                           char* buffer) const;

    //! Split geometry input (1) by sample number, and camera input (2) but samples&views.
    /*virtual*/ int            split_input(int input) const;

    //! Changes the time of the inputs for temporal sampling.
    /*virtual*/
    const DD::Image::OutputContext& inputContext(int                       input,
                                                 int                       offset,
                                                 DD::Image::OutputContext& context) const;


    //! From DD::Image::Op
    /*virtual*/ DD::Image::Op::HandlesMode doAnyHandles(DD::Image::ViewerContext*);

    //! Sets 2D viewer to 3D mode to draw any geometry in the input.
    /*virtual*/ void build_handles(DD::Image::ViewerContext*);

    //!
    /*virtual*/ void knobs(DD::Image::Knob_Callback f);

    //! Handle the AxisManipulator interactions.
    /*virtual*/ int  knob_changed(DD::Image::Knob* k);

    //! Add in the camera hash.
    /*virtual*/ void append(DD::Image::Hash& hash);

    //! Validate the 3D scene renderer.
    /*virtual*/ void _validate(bool for_real);


    //--------------------------------------------------
    // From DD::Image::IOp:

    //! Request input channels from background and assign output channels.
    /*virtual*/ void _request(int x, int y, int r, int t, DD::Image::ChannelMask output_channels, int count);

    //!
    /*virtual*/ void _close();

    //! 2D scanline engine.
    /*virtual*/ void engine(int y, int x, int r, DD::Image::ChannelMask out_channels, DD::Image::Row& out_row);


    //--------------------------------------------------
    // From DD::Image::Render:

    /*  Render.h virtual functions:

        virtual CameraOp* render_camera(int sample = 0);
        virtual GeoOp*    render_geo(int sample = 0) = 0;
        virtual Matrix4   camera_matrix(int sample = 0);
        virtual Matrix4   projection_matrix(int sample = 0);
        virtual void      format_matrix(int sample = 0);
        virtual void      overrideBBox(Box& f) const;
        virtual bool      generate_render_primitives();
        virtual bool      evaluate_lights(Scene* scene);
        virtual void      initialize();
        virtual int       multisamples() const;
        virtual Scene*    scene(int n = 0) const;
        virtual double    shutter() const;
        virtual double    offset() const;
        virtual uint32_t  samples() const;
        virtual void      probe(const Vector3& center, const VertexContext& vtx, Pixel& out);
    */


    //! Don't implement - use getInputCameraOpForSampleAndView() with add'l view arg.
    /*virtual*/ DD::Image::CameraOp* render_camera(int sample) { return getInputCameraOpForSampleAndView(sample, 0/*view*/); }


    //! Don't implement - use getInputGeoOpForSample() instead.
    /*virtual*/ DD::Image::GeoOp* render_geo(int sample) { return getInputGeoOpForSample(sample); }

    //! Returns the inverse camera matrix for a particular sample.
    /*virtual*/ DD::Image::Matrix4 camera_matrix(int sample);

    //! Returns the camera projection matrix for a particular sample.
    /*virtual*/ DD::Image::Matrix4 projection_matrix(int sample);

    //! Redirect to RenderContext shutter.
    /*virtual*/ double   shutter() const { return rtx.k_shutter.getDuration(); }

    //! Redirect to RenderContext shutter.
    /*virtual*/ double   offset() const { return rtx.k_shutter.calcOffset(); }

    //! Returns RenderContext::motion_samples().
    /*virtual*/ uint32_t samples() const { return rtx.numShutterSamples(); }

    //! Main entry point for constructing BVHs, called after _open().
    /*virtual*/ bool generate_render_primitives();


    //--------------------------------------------------
    // From DD::Image::DeepOp:

    //!
    /*virtual*/ DD::Image::Op* op() { return this; }

    //!
    /*virtual*/ void getDeepRequests(DD::Image::Box, const DD::Image::ChannelSet&, int count, std::vector<DD::Image::RequestData>&);

    //! Deep tile engine.
    /*virtual*/ bool doDeepEngine(DD::Image::Box bbox, const DD::Image::ChannelSet& out_channels, DD::Image::DeepOutputPlane& deep_out_plane);


  private:
#if kDDImageVersionInteger > 90000
    // Disabling the OpTree handling for 9.0+, it's causing unexpected
    // crashing which didn't seem to happen in 6.0-7.0.
#else
    /* Ripped from DDImage::RenderScene.h:
       These operations can check aborted states during build_handles, when the op may not be in another
       tree such as a viewer or write.  Because of this it needs to maintain its own additional tree.
    */
    DD::Image::OpTree* m_op_tree;
#endif

};



} // namespace zpr

#endif

// end of zpRender.h

//
// Copyright 2020 DreamWorks Animation
//
