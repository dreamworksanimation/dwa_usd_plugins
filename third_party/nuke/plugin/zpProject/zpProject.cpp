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

/// @file zpProject.cpp
///
/// @author Jonathan Egstad


#include <zprender/SurfaceShaderOp.h>
#include <zprender/RenderContext.h>

#include <DDImage/VertexContext.h>
#include <DDImage/CameraOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>
#include <DDImage/gl.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/Scene.h>


using namespace DD::Image;

namespace zpr {


enum { MERGE_NONE, MERGE_REPLACE, MERGE_OVER, MERGE_UNDER, MERGE_STENCIL, MERGE_MASK, MERGE_PLUS, MERGE_AVG, MERGE_MIN, MERGE_MAX };
static const char* const operation_modes[] = { "none", "replace", "over", "under", "stencil", "mask", "plus", "average", "min", "max", 0 };

enum { FACES_BOTH, FACES_FRONT, FACES_BACK };
const char* const face_names[] = { "both", "front", "back", 0 };

enum { Z_CLIP_NONE, Z_CLIP_CAM, Z_CLIP_USER };
const char* const zclip_modes[] = { "none", "cam", "user", 0 };


/*!
    TODO: support connection to Fuser CameraOp.
*/
class zpProject : public SurfaceShaderOp
{
  protected:
    int           k_operation;                  //!< Merge operation to perform on A
    int           k_faces_mode;                 //!< Project on front, back or both sides
    bool          k_crop_to_format;             //!< Crop projection at edge of projection
    ChannelSet    k_proj_channels;              //!< Set of channels to project
    TextureFilter k_texture_filter;             //!< Filter to use for texture filtering
    int           k_zclip_mode;                 //!< 
    double        k_near_clip, k_far_clip;      //!< Near/far Z clipping planes
    //
    CameraOp*     m_proj_cam;
    Fsr::Mat4d    m_projectxform, m_projectproj, m_projectconcat;
    ChannelSet    m_project_channels;


  public:
    static const Description description;
    /*virtual*/ const char* Class()     const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "Ray-tracing replacement for the stock Project3D node with greater control "
        "over shutter time, layering, z-clipping and texture filtering.\n"
        "\n"
        "Projects an input texture image ('img' input arrow) onto geometry with time offset "
        "controls (<i>frame clamp</i> knob) to allow greater control over what frame the input texture "
        "is sampled at.  This is required when the input texture is animated and is being projected "
        "through an animating camera.\n"
        "\n"
        "The unlabeled input 0 arrow can be connected to another shader allowing multiple projections to "
        "be stacked without needing a MergeMat shader.  Use the 'operation' control to set how to combine "
        "with the input shader.";
    }


    //!
    zpProject(::Node* node) :
        SurfaceShaderOp(node),
        k_texture_filter(Filter::Cubic)
    {
        k_operation      = MERGE_REPLACE;
        k_faces_mode     = FACES_BOTH;
        k_crop_to_format = true;
        k_proj_channels  = Mask_All;
        k_zclip_mode     = Z_CLIP_CAM;
        k_near_clip      = 100.0;
        k_far_clip       = 10000.0;
        //
        m_proj_cam = 0;
        m_project_channels = Mask_None;
    }


    /*virtual*/ int minimum_inputs() const { return 3; }
    /*virtual*/ int maximum_inputs() const { return 3; }


    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if (input == 0)
            return SurfaceShaderOp::test_input(0, op);
        if (input == 1)
            return dynamic_cast<Iop*>(op) != NULL;
        return dynamic_cast<CameraOp*>(op) != NULL;
    }


    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0)
            return SurfaceShaderOp::default_input(0);
        if (input == 1)
            return Iop::default_input(1);
        return NULL;
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buf) const
    {
       if      (input == 0) buf[0] = 0;
       else if (input == 1) buf = const_cast<char*>("img");
       else buf = const_cast<char*>("cam");
       return buf;
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //---------------------------------------------------------------------------------
        // This adds the 'zpSurfaceShaderOp' knob that's used to identify a SurfaceShaderOp
        // to other plugins (because dynamic_cast-ing fails).  Atm if this doesn't
        // exist then the _evaluate*() methods will not be called since the node
        // will not be recognized as a RayShader type:
        addSurfaceShaderOpIdKnob(f);
        //---------------------------------------------------------------------------------
        // The top line of ray controls:
        RayShader::addRayControlKnobs(f);

        Divider(f);
        Enumeration_knob(f, &k_operation, operation_modes, "operation");
            Tooltip(f, "Merge operation to perform between input 'img'(A) and input 0(B, unlabeled arrow)");
        Enumeration_knob(f, &k_faces_mode, face_names, "project_on", "project on");
            Tooltip(f, "Project onto front, back or both sides of geometry, using the shading normal.");
        Bool_knob(f, &k_crop_to_format, "crop_to_format", "crop to format");
            Tooltip(f, "Crop the incoming image, putting black outside the format area.");
        Newline(f);
        Enumeration_knob(f, &k_zclip_mode, zclip_modes, "zclip_mode", "z clip");
            Tooltip(f, "Projection Z-clip mode.  If set to 'user' the near/far clip knobs are used, "
                       "while 'cam' uses the projection camera's near & far plane settings.");
        Double_knob(f, &k_near_clip, IRange(1.0,100000.0), "near_clip", "near");
            ClearFlags(f, Knob::LOG_SLIDER);
        Newline(f);
        Double_knob(f, &k_far_clip,  IRange(1.0,100000.0), "far_clip",  "far");
            ClearFlags(f, Knob::LOG_SLIDER);
        Newline(f);
        Input_ChannelSet_knob(f, &k_proj_channels, 1/*input*/, "channels");
            Tooltip(f, "The set of channels from the texture input to copy to the shader output.");
        Newline(f);
        k_texture_filter.knobs(f, "texture_filter", "texture filter");
            Tooltip(f, "The texture filter to use for projection.");
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        //std::cout << "zpRender::knob changed='"<< k->name() << "'" << std::endl;

        if (k == &Knob::showPanel || k->name() == "zclip_mode")
        {
            knob("near_clip")->enable(k_zclip_mode == Z_CLIP_USER);
            knob("far_clip" )->enable(k_zclip_mode == Z_CLIP_USER);
            return 1; // call this again
        }

        return SurfaceShaderOp::knob_changed(k);
    }


    /*virtual*/
    void get_geometry_hash(Hash* geo_hash)
    {
        // Force the material to be reevaluated lower in the
        // tree using a random number to twiddle the hash:
        Material* m = dynamic_cast<Material*>(input(1));
        if (m)
            m->get_geometry_hash(geo_hash);
        static int x;
        geo_hash[Group_Object].append(x);
    }


    /*virtual*/
    void _validate(bool for_real)
    {
        //printf("zpProject::_validate(%d)\n", for_real);
        SurfaceShaderOp::_validate(for_real);

        // Make projection fit into UV range 0-1, correcting for format w/h ratio:
        m_project_channels = Mask_None;
        Iop* texture = (Iop*)Op::input(1);
        if (texture)
        {
            texture->validate(for_real);
            const Format& f = texture->format();
            m_projectproj.setToTranslation(0.5, 0.5, 0.0);
            m_projectproj.scale(0.5, 0.5*double(f.w())*f.pixel_aspect()/double(f.h()), 0.5);
            m_project_channels = texture->channels();
            m_project_channels &= k_proj_channels;
        }
        else
        {
            m_projectproj.setToIdentity();
        }

        // Get camera transforms from inputs:
        m_proj_cam = dynamic_cast<CameraOp*>(Op::input(2));
        if (m_proj_cam)
        {
            m_proj_cam->validate(for_real);
            m_projectproj  *= m_proj_cam->projection();
            m_projectxform  = m_proj_cam->imatrix();

            m_projectconcat =  m_projectproj;
            m_projectconcat *= m_projectxform;
        }
        else
        {
            m_projectxform.setToIdentity();
            m_projectconcat.setToIdentity();
        }

        info_.turn_on(m_project_channels);
    }


    /*virtual*/
    void _request(int x, int y, int r, int t, ChannelMask channels, int count)
    {
        //std::cout << "BaseSurface::_request()" << std::endl;
        // Requests surface color channels from input0:
        SurfaceShaderOp::_request(x, y, r, t, channels, count);

        Iop* texture = (Iop*)Op::input(1);
        if (texture)
        {
            // Request RGB map channels:
            const Box& b = texture->info();
            texture->request(b.x(), b.y(), b.r(), b.t(), m_project_channels, count);
        }
    }


    /*virtual*/
    HandlesMode doAnyHandles(ViewerContext* ctx)
    {
        if (!m_proj_cam)
            return eNoHandles;

        const int saved_mode = ctx->transform_mode();
        ctx->transform_mode(VIEWER_PERSP);
        HandlesMode any = m_proj_cam->anyHandles(ctx);
        ctx->transform_mode(saved_mode);
        return any;
    }


    /*! Adds the projection camera to the Viewer camera list,
        and draws it in 3D mode.
    */
    /*virtual*/
    void build_handles(ViewerContext* ctx)
    {
        if (!m_proj_cam)
            return;

        // Add to viewer camera menu knob:
        ctx->addCamera(m_proj_cam);

        // Let the camera draw itself:
        Matrix4 saved_matrix(ctx->modelmatrix);
        int saved_transform_mode = ctx->transform_mode();
        ctx->transform_mode(VIEWER_PERSP);
        ctx->modelmatrix.makeIdentity();

        add_input_handle(2, ctx);

        ctx->modelmatrix = saved_matrix;
        ctx->transform_mode(saved_transform_mode);
    }


    /*virtual*/
    bool set_texturemap(ViewerContext* ctx,
                        bool           gl)
    {
        return input1().set_texturemap(ctx, gl); // redirect to input 1
    }


    //! Construct an OpenGL clipping plane.
    void enableClipPlane(GLenum            plane,
                         const Fsr::Vec3d& N,
                         const Fsr::Vec3d& P)
    {
        double eq[4] = { N.x, N.y, N.z, -N.dot(P) };
        glClipPlane(plane, eq);
        glEnable(plane);
    }


    /*virtual*/
    bool shade_GL(ViewerContext* ctx,
                  GeoInfo&       info)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // Let's try the clipping plane to get rid of stuff behind projector
        if (m_proj_cam)
        {
            glPushMatrix();
            const Fsr::Mat4d cam_xform(ctx->cam_matrix());
            const Fsr::Mat4d m = cam_xform * m_proj_cam->matrix();
            glLoadMatrixd(m.array());

            if (k_zclip_mode == Z_CLIP_NONE)
            {
                // Just clip behind camera:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, 0.0));
            }
            else if (k_zclip_mode == Z_CLIP_CAM)
            {
                // Clip projection at the camera near & far planes:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(m_proj_cam->Near())));
                enableClipPlane(GL_CLIP_PLANE1, Fsr::Vec3d(0.0, 0.0,  1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(m_proj_cam->Far() )));
            }
            else if (k_zclip_mode == Z_CLIP_USER)
            {
                // Clip project at the user-set near/far planes:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(k_near_clip)));
                enableClipPlane(GL_CLIP_PLANE1, Fsr::Vec3d(0.0, 0.0,  1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(k_far_clip )));
            }

            if (k_faces_mode != FACES_BOTH)
            {
                glLoadMatrixd(cam_xform.array());

                const Matrix4& pm = m_proj_cam->matrix();
                glPushAttrib(GL_LIGHTING_BIT | GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT);
                glDisable(GL_COLOR_MATERIAL); // stop the material vals coming from vertex colors
                glDisable(GL_NORMALIZE);

                glLightfv(GL_LIGHT0, GL_POSITION, &pm.a03);
                // turn off the default 1.0 alphas so unlit areas are transparent:
                Vector4 t(0.0f, 0.0f, 0.0f, 0.0f);
                glLightModelfv(GL_LIGHT_MODEL_AMBIENT, t.array());
                glLightfv(GL_LIGHT0, GL_AMBIENT, t.array());
                glLightfv(GL_LIGHT0, GL_SPECULAR, t.array());
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, t.array());
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, t.array());
                glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, t.array());
                // Turn on all visible faces:
                t.set(1.0f, 1.0f, 1.0f, 1.0f);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, t.array());
                glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, (-pm.z_axis()).array());
                glLightf( GL_LIGHT0, GL_SPOT_CUTOFF, 90.0f);
                // Avoid diffuse falloff by using a really bright light:
                t.set(10.0f, 10.0f, 10.0f, 1.0f);
                if (k_faces_mode == FACES_FRONT)
                    glMaterialfv(GL_FRONT, GL_DIFFUSE, t.array());
                else
                    glMaterialfv(GL_BACK, GL_DIFFUSE, t.array());
                glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
                glEnable(GL_LIGHT0);
                glEnable(GL_LIGHTING);
            }

            glPopMatrix();
        }

        if (k_crop_to_format)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        }
        static GLfloat xplane[4] = { 1,0,0,0 };
        static GLfloat yplane[4] = { 0,1,0,0 };
        static GLfloat zplane[4] = { 0,0,1,0 };
        glMatrixMode(GL_TEXTURE);
        glMultMatrixd(m_projectconcat.array());
        glMultMatrixf(info.matrix.array());
        ctx->non_default_texture_matrix(true);
        glMatrixMode(GL_MODELVIEW);
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_S, GL_OBJECT_PLANE, xplane);
        glEnable(GL_TEXTURE_GEN_S);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_T, GL_OBJECT_PLANE, yplane);
        glEnable(GL_TEXTURE_GEN_T);
        glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_R, GL_OBJECT_PLANE, zplane);
        glEnable(GL_TEXTURE_GEN_R);

        return true;
    }


    /*virtual*/
    void unset_texturemap(ViewerContext* ctx)
    {
        if (Op::input(2) && k_faces_mode != FACES_BOTH)
            glPopAttrib(); // GL_LIGHTING_BIT | GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT

        glDisable(GL_CLIP_PLANE0);
        if (k_zclip_mode != Z_CLIP_NONE)
            glDisable(GL_CLIP_PLANE1);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);

        input1().unset_texturemap(ctx);
    }


#if 0
    /*! Project a vertex and pass the uv result up the tree so
        the geometry's uv extent bbox is expanded in Iop::vertex_shader.
    */
    /*virtual*/
    void vertex_shader(VertexContext& vtx)
    {
        // In UV render mode the entire input UV range is required:
        if (vtx.scene()->projection_mode() != CameraOp::LENS_UV)
        {
            // Project the world-space point backwards into projector:
            const Fsr::Vec4f uv = m_projectconcat.transform(vtx.PW(), 1.0);
            if (UV.w > 0.0f)
                vtx.vP.UV() = UV;
            else
                vtx.vP.UV().set(0,0,0,1);
        }
        // Continue on up the tree:
        input0().vertex_shader(vtx);

        // Make sure N and PW are interpolated for fragment_shader():
        vtx.vP.channels += ChannelSetInit( Mask_N_ | Mask_PW_ );
    }
#endif


    //---------------------------------------------------------------------------


    /*! The geometric surface evaluation shader call.  If doing displacement implement the
        dedicated displacement call instead.
    */
    /*virtual*/
    void _evaluateGeometricShading(RayShaderContext& stx,
                                   RayShaderContext& out)
    {
        // Make sure input types are built:
        validate(1);

        // Base class call will pass it on up to input0.  Do this first so
        // that we override any mods further up:
        SurfaceShaderOp::_evaluateGeometricShading(stx, out);

        // If no projection enabled we're done:
        if (m_project_channels == Mask_None)
            return;

        // Project the world-space point backwards through projector:
        const Fsr::Vec4f uv = m_projectconcat.transform(stx.PW, 1.0);
        // Skip uvs behind the camera:
        if (uv.w <= 0.0f)
            out.UV.set(0.0f, 0.0f);
        else
            out.UV.set(uv.x/uv.w, uv.y/uv.w);
    }


    /*! The ray-tracing shader call.
    */
    /*virtual*/
    void _evaluateShading(RayShaderContext& stx,
                          Fsr::Pixel&       out)
    {
        //std::cout << "zpProject::evaluateShading() frame=" << stx.frame_time << std::endl;
        // Let the background get shaded first.
        // Base class call will pass it on up to input0:
        RayShaderContext stx1 = stx;
        SurfaceShaderOp::_evaluateShading(stx1, out);
        // If no projection enabled we're done:
        if (m_project_channels == Mask_None || k_operation == MERGE_NONE)
            return;

        // Possibly motion-blur interpolate the input camera xform matrix:
        Fsr::Mat4d proj_xform  = m_projectxform;
        Fsr::Mat4d proj_concat = m_projectconcat;

        // If motion blur enabled find the motionblur 'sibling' shader to interpolate with.
        if (stx.rtx->isMotionBlurEnabled() && stx.frame_shutter_step < ((uint32_t)stx.rtx->shutter_times.size()-1))
        {
#if 0
            // Do something smarter here, like put methods on RayShader to determine the frame and
            // 'sibling' shaders early on in the RenderContext construction.  I don't think we can put
            // a mb_shader pointer on RayShader since it may be different for separate zpRender
            // contexts that share the RayShader Op:
#else
            zpProject* mb_projector = 0;
#if 1
            const GeoInfoContext* gptx = stx.rprim->surface_ctx->getGeoInfoContext();
            if (!gptx)
            {
                // Cannot evaluate as a surface, skip it:
                SurfaceShaderOp::_evaluateShading(stx, out);
                return;
            }

            const GeoInfoContext::Sample& gtx0 = gptx->getGeoInfoSample(stx.frame_shutter_step  );
            const GeoInfoContext::Sample& gtx1 = gptx->getGeoInfoSample(stx.frame_shutter_step+1);
#else
            GeoInfoContext::Sample& gtx0 = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(stx.frame_shutter_step  );
            GeoInfoContext::Sample& gtx1 = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(stx.frame_shutter_step+1);
#endif
            //std::cout << "gtx0=" << &gtx0 << ", gtx1=" << &gtx1 << std::endl;
            if      (gtx0.info->material != this && gtx1.info->material == this)
                mb_projector = dynamic_cast<zpProject*>(gtx0.info->material);
            else if (gtx1.info->material != this && gtx0.info->material == this)
                mb_projector = dynamic_cast<zpProject*>(gtx1.info->material);
            //if (stx.x==820&&stx.y==801) std::cout << "this=" << this << ", mb_projector=" << mb_projector << std::endl;

            if (mb_projector)
            {
                // Interpolate between the material projection transforms:
                const double f0 = stx.rtx->shutter_times[stx.frame_shutter_step  ];
                const double f1 = stx.rtx->shutter_times[stx.frame_shutter_step+1];
                const float t = float((stx.frame_time - f0) / (f1 - f0));
                proj_xform = lerp(m_projectxform, mb_projector->m_projectxform, t);
                proj_concat = m_projectproj * proj_xform;
                //if (stx.x==820&&stx.y==801) std::cout << "0: " << m_projectxform << " 1: " << mb_projector->m_projectxform << " proj_xform: " << proj_xform << std::endl;
            }
#endif
        }

        // Handle front/back clipping:
        if (k_faces_mode != FACES_BOTH && m_proj_cam)
        {
            // Don't project on surfaces facing away from projection camera:
            const Fsr::Vec3d Vp = (Fsr::Vec3d(m_proj_cam->matrix().translation()) - stx.PW);
            const double Vp_dot_N = Vp.dot(stx.Ns);
            if ((k_faces_mode == FACES_FRONT && Vp_dot_N < 0.0f) || 
                (k_faces_mode == FACES_BACK  && Vp_dot_N > 0.0f))
            {
                // Force this surface to be transparent, allowing further-back surfaces to appear.
                // If this isn't done then this surface will appear black:
                out.erase(m_project_channels);
                out[Chan_Alpha] = 0.0f; // make sure alpha is zero too
                return;
            }
        }

        // Handle Z-clipping:
        if (k_zclip_mode != Z_CLIP_NONE)
        {
            const Fsr::Vec3d CPW = proj_xform.transform(stx.PW);
            const double Z = ::fabs(CPW.z);
            if (k_zclip_mode == Z_CLIP_USER)
            {
                // Clip project at the user-set near/far planes:
                if (Z < fabs(k_near_clip) || Z > fabs(k_far_clip))
                    return;
            }
            else if (k_zclip_mode == Z_CLIP_CAM && m_proj_cam)
            {
                // Clip project at the user-set near/far planes:
                if (Z < fabs(m_proj_cam->Near()) || Z > fabs(m_proj_cam->Far()))
                    return;
            }
        }

        // Calculate the projection:
        Fsr::Vec2f uv, uvdx, uvdy;
        if (project(proj_concat, stx, stx.UV, stx.dUVdx, stx.dUVdy))
        {
            Fsr::Pixel tex_pixel(m_project_channels);
            tex_pixel.erase();

            stx.texture_filter = &k_texture_filter;

            // Pass it on up if input 1 is another RayShader:
            SurfaceShaderOp* ray_shader = rayShaderInput(1);
            if (ray_shader)
            {
                ray_shader->evaluateShading(stx, tex_pixel);
            }
            else if (iopInput(1))
            {
                // Call legacy shader:
                DD::Image::VertexContext vtx; //!< Contains surface attribs
                updateDDImageShaderContext(stx, vtx);
                //------------------------------------------
                //------------------------------------------
                iopInput(1)->fragment_shader(vtx, tex_pixel);
            }

            // Merge the chans:
            switch (k_operation)
            {
                case MERGE_REPLACE:
                    out.replace(tex_pixel, m_project_channels);
                    break;

                case MERGE_OVER:
                    out.over(tex_pixel/*A*/, tex_pixel[Chan_Alpha]/*alpha*/, m_project_channels);
                    break;

                case MERGE_UNDER:
                {
                    //out.under(tex_pixel/*A*/, tex_pixel[Chan_Alpha]/*alpha*/, m_project_channels);
                    const float iBa = (1.0f - out[Chan_Alpha]);
                    if (iBa < std::numeric_limits<float>::epsilon())
                    {
                        ;//
                    }
                    else if (iBa < 1.0f)
                    {
                        foreach(z, m_project_channels)
                            out[z] += tex_pixel[z]*iBa;
                    }
                    else
                    {
                        foreach(z, m_project_channels)
                            out[z] += tex_pixel[z];
                    }
                    break;
                }

                case MERGE_STENCIL:
                {
                    const float iAa = 1.0f - tex_pixel[Chan_Alpha];
                    if (iAa < std::numeric_limits<float>::epsilon())
                    {
                        foreach(z, m_project_channels)
                            out[z] = 0.0f;
                    }
                    else if (iAa < 1.0f)
                    {
                        foreach(z, m_project_channels)
                            out[z] *= iAa;
                    }
                    else
                    {
                        ;//
                    }
                    break;
                }

                case MERGE_MASK:
                {
                    const float Aa = tex_pixel[Chan_Alpha];
                    if (Aa < std::numeric_limits<float>::epsilon())
                    {
                        foreach(z, m_project_channels)
                            out[z] = 0.0f;
                    }
                    else if (Aa < 1.0f)
                    {
                        foreach(z, m_project_channels)
                            out[z] *= Aa;
                    }
                    else
                    {
                        ;//
                    }
                    break;
                }

                case MERGE_PLUS:
                    foreach(z, m_project_channels)
                        out[z] = (out[z] + tex_pixel[z]);
                    break;

                case MERGE_AVG:
                    foreach(z, m_project_channels)
                        out[z] = (out[z] + tex_pixel[z])*0.5f;
                    break;

                case MERGE_MIN:
                    foreach(z, m_project_channels)
                        out[z] = std::min(out[z], tex_pixel[z]);
                    break;

                case MERGE_MAX:
                    foreach(z, m_project_channels)
                        out[z] = std::max(out[z], tex_pixel[z]);
                    break;

                default:
                //case MERGE_NONE:
                    break;
            }
        }
    }


    //!
    bool project(const Fsr::Mat4d& proj_matrix,
                 RayShaderContext& stx,
                 Fsr::Vec2f&       UV,
                 Fsr::Vec2f&       dUVdx,
                 Fsr::Vec2f&       dUVdy)
    {
        // Project the world-space point backwards through projector:
        Fsr::Vec4f uv = proj_matrix.transform(stx.PW, 1.0);
        // Skip uvs behind the camera:
        if (uv.w <= 0.0f)
            return false;

        // Skip if outside the 0..1 box:
        if (k_crop_to_format &&
            (uv.x < 0.0f || uv.x > uv.w ||
             uv.y < 0.0f || uv.y > uv.w))
            return false;

        // Calculate the derivatives:
        Fsr::Vec4f uvdx = proj_matrix.transform(stx.PW + stx.dPWdx, 1.0);
        Fsr::Vec4f uvdy = proj_matrix.transform(stx.PW + stx.dPWdy, 1.0);

        UV.set(uv.x/uv.w, uv.y/uv.w);
#if 0
        dUVdx.set((uvdx.x - uv.x)/uv.w, (uvdx.y - uv.y)/uv.w);
        dUVdy.set((uvdy.x - uv.x)/uv.w, (uvdy.y - uv.y)/uv.w);
#else
        dUVdx.set((uvdx.x/uvdx.w) - UV.x, (uvdx.y/uvdx.w) - UV.y);
        dUVdy.set((uvdy.x/uvdy.w) - UV.x, (uvdy.y/uvdy.w) - UV.y);
#endif

        return true;
    }

};


static Op* build(Node* node) { return new zpProject(node); }
const Op::Description zpProject::description("zpProject", build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("ProjectSurface", build);
#endif

} // namespace zpr

// end of zpProject.cpp

//
// Copyright 2020 DreamWorks Animation
//
