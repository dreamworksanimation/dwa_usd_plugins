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


#include "zprProject.h"

#include <zprender/SurfaceMaterialOp.h>
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


/*!
    TODO: support connection to Fuser CameraOp.
*/
class zpProject : public SurfaceMaterialOp
{
  protected:
    zprProject::InputParams k_inputs;
    zprProject::LocalVars   m_locals;


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
    zpProject(::Node* node) : SurfaceMaterialOp(node) {}


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        RayShader* output = new zprProject(k_inputs);
        shaders.push_back(output);
        return output;
    }


    /*virtual*/ int minimum_inputs() const { return zprProject::NUM_INPUTS; }
    /*virtual*/ int maximum_inputs() const { return zprProject::NUM_INPUTS; }


    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if (input == 0) return SurfaceMaterialOp::test_input(0, op);
        if (input == 1) return dynamic_cast<Iop*>(op) != NULL;
        return dynamic_cast<CameraOp*>(op) != NULL;
    }


    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0) return SurfaceMaterialOp::default_input(0);
        if (input == 1) return Iop::default_input(1);
        return NULL;
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buf) const
    {
       if      (input == 0) buf[0] = 0;
       else if (input == 1) buf = const_cast<char*>("img");
       else if (input == 2) buf = const_cast<char*>("cam");
       return buf;
    }


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBinding(uint32_t input)
    {
        if      (input == 0) return &k_inputs.k_bindings[zprProject::BG0    ];
        else if (input == 1) return &k_inputs.k_bindings[zprProject::MAP1   ];
        else if (input == 2) return &k_inputs.k_bindings[zprProject::CAMERA2];
        return NULL;
    }


    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    /*virtual*/
    int32_t getGLTextureInput() const { return 1; }


    //----------------------------------------------------------------------------------


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //---------------------------------------------------------------------------------
        // This adds the 'zpSurfaceMaterialOp' knob that's used to identify a SurfaceMaterialOp
        // to other plugins (because dynamic_cast-ing fails).  Atm if this doesn't
        // exist then the _evaluate*() methods will not be called since the node
        // will not be recognized as a RayShader type:
        addSurfaceMaterialOpIdKnob(f);
        //---------------------------------------------------------------------------------
        // The top line of ray controls:
        addRayControlKnobs(f);

        InputOp_knob(f,  &k_inputs.k_bindings[zprProject::BG0    ], 0/*input*/);
        ColorMap_knob(f, &k_inputs.k_bindings[zprProject::MAP1   ], 1/*input*/, 4/*num_channels*/, "proj_map", "map");
        InputOp_knob(f,  &k_inputs.k_bindings[zprProject::CAMERA2], 2/*input*/);

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Enumeration_knob(f, &k_inputs.k_operation, zprProject::operation_modes, "operation");
            Tooltip(f, "Merge operation to perform between input 'img'(A) and input 0(B, unlabeled arrow)");
        Enumeration_knob(f, &k_inputs.k_faces_mode, zprProject::face_names, "project_on", "project on");
            Tooltip(f, "Project onto front, back or both sides of geometry, using the shading normal.");
        Bool_knob(f, &k_inputs.k_crop_to_format, "crop_to_format", "crop to format");
            Tooltip(f, "Crop the incoming image, putting black outside the format area.");
        Newline(f);
        Enumeration_knob(f, &k_inputs.k_zclip_mode, zprProject::zclip_modes, "zclip_mode", "z clip");
            Tooltip(f, "Projection Z-clip mode.  If set to 'user' the near/far clip knobs are used, "
                       "while 'cam' uses the projection camera's near & far plane settings.");
        Double_knob(f, &k_inputs.k_near_clip, IRange(1.0,100000.0), "near_clip", "near");
            ClearFlags(f, Knob::LOG_SLIDER);
        Newline(f);
        Double_knob(f, &k_inputs.k_far_clip,  IRange(1.0,100000.0), "far_clip",  "far");
            ClearFlags(f, Knob::LOG_SLIDER);
        Newline(f);
        Input_ChannelSet_knob(f, &k_inputs.k_proj_channels, 1/*input*/, "channels");
            Tooltip(f, "The set of channels from the texture input to copy to the shader output.");
        Newline(f);
        k_inputs.k_texture_filter.knobs(f, "texture_filter", "texture filter");
            Tooltip(f, "The texture filter to use for projection.");
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        //std::cout << "zpRender::knob changed='"<< k->name() << "'" << std::endl;

        if (k == &Knob::showPanel || k->name() == "zclip_mode")
        {
            knob("near_clip")->enable(k_inputs.k_zclip_mode == zprProject::Z_CLIP_USER);
            knob("far_clip" )->enable(k_inputs.k_zclip_mode == zprProject::Z_CLIP_USER);
            return 1; // call this again
        }

        return SurfaceMaterialOp::knob_changed(k);
    }


    /*virtual*/
    void append(DD::Image::Hash& hash)
    {
        SurfaceMaterialOp::append(hash);
        //std::cout << "zpProject::append(" << this << ")";
        //std::cout << "hash=0x" << std::hex << hash.value() << std::dec << std::endl;
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
        // Call base class first to get InputBindings assigned:
        SurfaceMaterialOp::_validate(for_real);

        zprProject::updateLocals(k_inputs, m_locals);

        info_.turn_on(m_locals.m_project_channels);
    }


    /*virtual*/
    HandlesMode doAnyHandles(ViewerContext* ctx)
    {
        if (!m_locals.m_proj_cam)
            return eNoHandles;

        const int saved_mode = ctx->transform_mode();
        ctx->transform_mode(VIEWER_PERSP);
        HandlesMode any = m_locals.m_proj_cam->anyHandles(ctx);
        ctx->transform_mode(saved_mode);
        return any;
    }


    /*! Adds the projection camera to the Viewer camera list,
        and draws it in 3D mode.
    */
    /*virtual*/
    void build_handles(ViewerContext* ctx)
    {
        if (!m_locals.m_proj_cam)
            return;

        // Add to viewer camera menu knob:
        ctx->addCamera(m_locals.m_proj_cam);

        // Let the camera draw itself:
        Matrix4 saved_matrix(ctx->modelmatrix);
        int saved_transform_mode = ctx->transform_mode();
        ctx->transform_mode(VIEWER_PERSP);
        ctx->modelmatrix.makeIdentity();

        add_input_handle(2, ctx);

        ctx->modelmatrix = saved_matrix;
        ctx->transform_mode(saved_transform_mode);
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
    bool set_texturemap(DD::Image::ViewerContext* ctx,
                        bool                      gl)
    {
        return SurfaceMaterialOp::set_texturemap(ctx, gl);
    }


    /*virtual*/
    bool shade_GL(ViewerContext* ctx,
                  GeoInfo&       info)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // Let's try the clipping plane to get rid of stuff behind projector
        if (m_locals.m_proj_cam)
        {
            glPushMatrix();
            const Fsr::Mat4d cam_xform(ctx->cam_matrix());
            const Fsr::Mat4d m = cam_xform * m_locals.m_proj_cam->matrix();
            glLoadMatrixd(m.array());

            if (k_inputs.k_zclip_mode == zprProject::Z_CLIP_NONE)
            {
                // Just clip behind camera:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, 0.0));
            }
            else if (k_inputs.k_zclip_mode == zprProject::Z_CLIP_CAM)
            {
                // Clip projection at the camera near & far planes:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(m_locals.m_proj_cam->Near())));
                enableClipPlane(GL_CLIP_PLANE1, Fsr::Vec3d(0.0, 0.0,  1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(m_locals.m_proj_cam->Far() )));
            }
            else if (k_inputs.k_zclip_mode == zprProject::Z_CLIP_USER)
            {
                // Clip project at the user-set near/far planes:
                enableClipPlane(GL_CLIP_PLANE0, Fsr::Vec3d(0.0, 0.0, -1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(k_inputs.k_near_clip)));
                enableClipPlane(GL_CLIP_PLANE1, Fsr::Vec3d(0.0, 0.0,  1.0), Fsr::Vec3d(0.0, 0.0, -::fabs(k_inputs.k_far_clip )));
            }

            if (k_inputs.k_faces_mode != zprProject::FACES_BOTH)
            {
                glLoadMatrixd(cam_xform.array());

                const Matrix4& pm = m_locals.m_proj_cam->matrix();
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
                if (k_inputs.k_faces_mode == zprProject::FACES_FRONT)
                    glMaterialfv(GL_FRONT, GL_DIFFUSE, t.array());
                else
                    glMaterialfv(GL_BACK, GL_DIFFUSE, t.array());
                glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
                glEnable(GL_LIGHT0);
                glEnable(GL_LIGHTING);
            }

            glPopMatrix();
        }

        if (k_inputs.k_crop_to_format)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        }
        static GLfloat xplane[4] = { 1,0,0,0 };
        static GLfloat yplane[4] = { 0,1,0,0 };
        static GLfloat zplane[4] = { 0,0,1,0 };
        glMatrixMode(GL_TEXTURE);
        glMultMatrixd(m_locals.m_projectconcat.array());
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
        if (Op::input(2) && k_inputs.k_faces_mode != zprProject::FACES_BOTH)
            glPopAttrib(); // GL_LIGHTING_BIT | GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT

        glDisable(GL_CLIP_PLANE0);
        if (k_inputs.k_zclip_mode != zprProject::Z_CLIP_NONE)
            glDisable(GL_CLIP_PLANE1);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);

        SurfaceMaterialOp::unset_texturemap(ctx);
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
