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

/// @file zpRectangleLight.cpp
///
/// @author Jonathan Egstad


#include "zprRectangleLight.h"

#include <zprender/LightMaterialOp.h>

#include <DDImage/ViewerContext.h>
#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>
#include <DDImage/Scene.h>
#include <DDImage/Interest.h>
#include <DDImage/gl.h>

#include <sstream>


using namespace DD::Image;

namespace zpr {


/*! A Fuser::LightOp plugin which allows a rectangular image to show up
    in a reflective surface.


*/
class zpRectangleLight : public LightMaterialOp
{
  protected:
    zprRectangleLight zprShader;        //!< Local shader allocation for knobs to write into


  public:
    static const Description description;
    const char* Class() const { return description.name; }

    const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "zpRectangleLight is a rectangular card which emits light from a texture map and is intended "
        "to be used primarily with reflection shaders.  Any diffuse influence from this shader is "
        "likely to be completely incorrect.\n";
    }


    //!
    zpRectangleLight(Node *node) :
        LightMaterialOp(node)
    {
        display3d_ = DISPLAY_TEXTURED;  // in DD::Image::LightOp
    }


    /*virtual*/ int minimum_inputs() const { return 1; }
    /*virtual*/ int maximum_inputs() const { return 1+zprRectangleLight::NUM_INPUTS; }


    /*! Input 0 is Axis parent input, input 1 is texture source. */
    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if      (input == 0) return LightMaterialOp::test_input(input, op);
        else if (input == 1)
        {
            if (!op) return true; // Allow connection to nothing
            if (dynamic_cast<Iop*>(op)!=0) return true;
        }
        return false;
    }


    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0) return LightMaterialOp::default_input(input);
        return NULL; // allow null on colormap inputs
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buffer) const
    {
        if      (input == 0) buffer = const_cast<char*>("");
        else if (input == 1) buffer = const_cast<char*>("img");
        return buffer;
    }


    //------------------------------------------------------------------
    // From LightMaterialOp
    //------------------------------------------------------------------


    /*! Create the shaders for one input, returning the output surface shader.
        RenderContext is optional.
    */
    /*virtual*/
    LightShader* _createOutputLightShader(const RenderContext*     rtx,
                                          const Fsr::DoubleList&   motion_times,
                                          const Fsr::Mat4dList&    motion_xforms,
                                          std::vector<RayShader*>& shaders)

    {
        LightShader* ltshader = new zprRectangleLight(zprShader.inputs, motion_times, motion_xforms);
        shaders.push_back(ltshader);
        return ltshader;
    }


    /*! For legacy shading system.
        Return the local LightShader object which the LightMaterialOp stores
        its knobs into.
        If this LightShader is non-null it will be called in the legacy
        get_L_vector(), get_shadowing(), and get_color() methods.
    */
    /*virtual*/ LightShader* _getOpOutputLightShader() { return &zprShader; }


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBindingForOpInput(uint32_t op_input)
    {
        if (op_input == 1) return &zprShader.inputs.k_bindings[zprRectangleLight::MAP0];
        return NULL;
    }

    //! Return the Op input for a shader input, or -1 if binding is not exposed.
    /*virtual*/ int32_t getOpInputForShaderInput(uint32_t shader_input)
    {
        if (shader_input == zprRectangleLight::MAP0) return 1;
        return -1;
    }


    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    /*virtual*/
    int32_t getGLTextureInput() const { return 1; }


    //------------------------------------------------------------------
    // From DD::Image::LightOp
    //------------------------------------------------------------------


    /*virtual*/ double   hfov() const { return LightMaterialOp::hfov();   }
    /*virtual*/ double   vfov() const { return LightMaterialOp::vfov();   }
    /*virtual*/ double aspect() const { return LightMaterialOp::aspect(); }

    /*! This light does not have a delta distribution.
    */
    /*virtual*/ bool is_delta_light() const { return false; }

    /*virtual*/ int lightType() const { return eOtherLight; }

    /*virtual*/
    void addLightKnobs(DD::Image::Knob_Callback f)
    {
        //LightMaterialOp::addLightKnobs(f); // don't want the near/far controls

        Newline(f);
        Bool_knob(f, &zprShader.inputs.k_single_sided, "single_sided", "single sided");
            Tooltip(f, "If enabled only the +Z side will emit light.");
        Newline(f);
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprRectangleLight::MAP0], 1/*input*/, 4/*num_channels*/, "map", "map");

        zprShader.inputs.k_map_filter.knobs(f, "map_filter", "map filter");
        Double_knob(f, &zprShader.inputs.k_filter_size, "k_filter_size", "filter size");

        //------------------------
        Newline(f);
        Double_knob(f, &zprShader.inputs.k_lens_in_focal, "lens_in_focal", "lens-in focal");
        Double_knob(f, &zprShader.inputs.k_lens_in_haperture, "lens_in_haperture", "lens-in haperture");
        Double_knob(f, &zprShader.inputs.k_z, IRange(0.0, 500.0), "z");
            SetFlags(f, Knob::LOG_SLIDER);
            Tooltip(f, "Reflection card is placed this far from the local origin and scaled to "
                       "maintain relative size.");

        // Ignore old ReflectionCard knobs:
        Obsolete_knob(f, "map_channels", NULL);
        Obsolete_knob(f, "map_enable", NULL);
        //
        //Bool_knob(f, &zprShader.inputs.k_enable_hemisphere, "enable_hemisphere_sampling", "enable hemispherical sampling");
        //Int_knob(f, &zprShader.inputs.k_cone_samples, "cone_samples", "cone samples");
        //Double_knob(f, &zprShader.inputs.k_cone_angle, "cone_angle", "cone angle");
        Obsolete_knob(f, "enable_hemisphere_sampling", NULL);
        Obsolete_knob(f, "cone_samples", NULL);
        Obsolete_knob(f, "cone_angle", NULL);
    }


    /*virtual*/
    void _validate(bool for_real)
    {
        //std::cout << "zpRectangleLight::validate(" << this << ")"<< std::endl;
        // Copy values from the LightOp to the InputParams before
        // calling LightMaterialOp::_validate():
        zprShader.inputs.k_color.set(DD::Image::LightOp::color_[DD::Image::Chan_Red  ],
                                     DD::Image::LightOp::color_[DD::Image::Chan_Green],
                                     DD::Image::LightOp::color_[DD::Image::Chan_Blue ]);
        zprShader.inputs.k_intensity = DD::Image::LightOp::intensity_;

        // Updates the legacy-mode output LightShader:
        LightMaterialOp::_validate(for_real);
    }


    //------------------------------------------------------------------


    /*! OpenGL light settings for when objects are just about to draw.
        From DD::Image::LightOp.
    */
    //virtual
    void shade_GL(DD::Image::ViewerContext* ctx,
                  int                       lt_index)
    {
        if (node_disabled())
            return;

#if DEBUG
        assert(zprShader.numMotionXforms() > 0);
#endif
        const Fsr::Mat4d& xform = zprShader.getMotionXform(0);

        // Setting planeP.w > 0 tells OpenGL this light is a positional light
        // as opposed to a direct light like the sun.
        const Fsr::Vec4f planeP(xform.getTranslation(), 1.0f);
        const Fsr::Vec3f planeN(xform.getZAxis(), 1.0f/*normalize*/);

        glLightfv(GL_LIGHT0 + lt_index, GL_POSITION, (GLfloat*)planeP.array());
        glLightfv(GL_LIGHT0 + lt_index, GL_SPOT_DIRECTION, (GLfloat*)planeN.array());
        glLightf(GL_LIGHT0 + lt_index, GL_SPOT_CUTOFF,   90.0f);
        glLightf(GL_LIGHT0 + lt_index, GL_SPOT_EXPONENT, 0.0f);

        Fsr::Vec4f t;
        t.set(zprShader.m_color.rgb(), 0.0f);
        glLightfv(GL_LIGHT0 + lt_index, GL_DIFFUSE,  (GLfloat*)t.array());
        glLightfv(GL_LIGHT0 + lt_index, GL_SPECULAR, (GLfloat*)t.array());
        t.set(0.0f, 0.0f, 0.0f, 0.0f);
        glLightfv(GL_LIGHT0 + lt_index, GL_AMBIENT, (GLfloat*)t.array());

        // We have to set *all* light parameters because they may get
        // modified by other lights and OpenGL has no 'set to default'...:
        glLightf(GL_LIGHT0 + lt_index, GL_CONSTANT_ATTENUATION,  1.0f);
        glLightf(GL_LIGHT0 + lt_index, GL_LINEAR_ATTENUATION,    0.0f);
        glLightf(GL_LIGHT0 + lt_index, GL_QUADRATIC_ATTENUATION, 0.0f);

        // Finally turn on the light:
        glEnable(GL_LIGHT0 + lt_index);
    }


    /*! Draws a textured rectangle - unfortunately we can't get the texturemap display to respect
        the map channel requester without rewriting a bunch of code...
    */
    /*virtual*/
    void draw_handle(DD::Image::ViewerContext* ctx)
    {
        if (node_disabled())
            return;

        if (!selectable_ && ctx->hit_detect())
            return; // don't draw in hit detect-mode id not selectable

        //if (!ctx->draw_knobs())
        //    return; // don't draw during 3D solid passes

        bool selected = node_selected();
        DD::Image::Display3DMode display3d = ctx->display3d((DD::Image::Display3DMode)this->display3d_);
        if (!display3d && !selected)
            return;

        this->validate(false); // make sure matrices are up to date

#if 0
#if DEBUG
        assert(zprShader.numMotionXforms() > 0);
#endif
        const Fsr::Mat4d& xform = zprShader.getMotionXform(0);
#endif

        DD::Image::Matrix4 saved = ctx->modelmatrix;
        ctx->modelmatrix *= local();
        glLoadMatrixf(ctx->modelmatrix.array());

        glTranslated(0.0, 0.0, zprShader.inputs.k_z);

        if (node_selected())
            glColor3f(1.0f, 1.0f, 1.0f);
        else
            glColor(ctx->node_color());

        // Corners of rectangle window:
        const Fsr::Vec2f win(zprShader.m_width_half,
                             zprShader.m_height_half);

        // draw textured during both the solid and transparent pass:
        if (ctx->draw_transparent() && display3d >= DISPLAY_SOLID)
        {
            // If texturing is successful this gets turned off:
            bool solid = true;
            if (display3d >= DISPLAY_TEXTURED)
            {
                // Set up texturemap:
                const InputBinding& map0 = zprShader.inputs.k_bindings[zprRectangleLight::MAP0];
                DD::Image::Iop* map_iop = map0.asTextureIop();
                if (map0.isEnabled() && map_iop && map_iop->set_texturemap(ctx, true/*gl*/))
                {
                    glPushAttrib(GL_LIGHTING_BIT | GL_POLYGON_BIT/*for cull face*/);

                    // We don't want lighting to affect the light's icon:
                    glDisable(GL_LIGHTING);

                    // Draw the front face at max brightness:
                    if (zprShader.inputs.k_single_sided)
                    {
                        glCullFace(GL_BACK);
                        glEnable(GL_CULL_FACE);
                    }
                    glColor4f(1, 1, 1, 1);
                    glBegin(GL_QUADS);
                     glTexCoord2f(1, 1); glNormal3f(0, 0,-1); glVertex3f(-win.x,  win.y, 0);
                     glTexCoord2f(0, 1); glNormal3f(0, 0,-1); glVertex3f( win.x,  win.y, 0);
                     glTexCoord2f(0, 0); glNormal3f(0, 0,-1); glVertex3f( win.x, -win.y, 0);
                     glTexCoord2f(1, 0); glNormal3f(0, 0,-1); glVertex3f(-win.x, -win.y, 0);
                    glEnd();

                    if (zprShader.inputs.k_single_sided)
                    {
                        // Draw the back transparent:
                        glCullFace(GL_FRONT);
                        glEnable(GL_CULL_FACE);
                        float gain = 0.2f;
                        glColor4f(gain, gain, gain, gain);

                        glBegin(GL_QUADS);
                         glTexCoord2f(1, 1); glNormal3f(0, 0,-1); glVertex3f(-win.x,  win.y, 0);
                         glTexCoord2f(0, 1); glNormal3f(0, 0,-1); glVertex3f( win.x,  win.y, 0);
                         glTexCoord2f(0, 0); glNormal3f(0, 0,-1); glVertex3f( win.x, -win.y, 0);
                         glTexCoord2f(1, 0); glNormal3f(0, 0,-1); glVertex3f(-win.x, -win.y, 0);
                        glEnd();
                    }
                    glPopAttrib(); //GL_LIGHTING_BIT | GL_POLYGON_BIT

                    map_iop->unset_texturemap(ctx);

                    // Success, turn off solid flag so it doesn't draw again:
                    solid = false;
                }

            }

            // Draw solid during the solid pass or if texturing failed:
            if (ctx->draw_solid() && solid)
            {
                glColor3fv(zprShader.m_color.rgb().array());
                // Draw the card:
                glBegin(GL_QUADS);
                 glNormal3f( 0, 0, -1);
                 glVertex3f(-win.x,  win.y, 0); glVertex3f( win.x,  win.y, 0);
                 glVertex3f( win.x, -win.y, 0); glVertex3f(-win.x, -win.y, 0);
                glEnd();
            }
            glDisable(GL_CULL_FACE);
        }

        // Wireframe pass:
        if (ctx->draw_lines())
        {
            if (style_needs_wireframe(display3d) || selected)
            {
                if (selected)
                    glColor(ctx->selected_color());
                else
                    glColor(ctx->node_color());
                glBegin(GL_LINE_LOOP);
                 glVertex3f(-win.x,  win.y, 0);
                 glVertex3f( win.x,  win.y, 0);
                 glVertex3f( win.x, -win.y, 0);
                 glVertex3f(-win.x, -win.y, 0);
                glEnd();
            }
        }

#if 0
        glColor3f(0,0,1);
        glBegin(GL_LINES);
        const size_t nRays = m_rays.size();
        for (size_t i=0; i < nRays; ++i)
        {
            Vector3 dir = m_rays[i];
            glVertex3f(0,0,0); glVertex3f(dir.x, dir.y, dir.z);
        }
        glEnd();
#endif

        // Draw the direction lines if double-sided is off:
        if (zprShader.inputs.k_single_sided)
        {
            glColor3fv(zprShader.m_color.rgb().array());
            const float z = -std::min(win.x, win.y);
            glBegin(GL_LINES);
             glVertex3f( -win.x, -win.y, 0); glVertex3f( -win.x, -win.y, z);
             glVertex3f(  win.x, -win.y, 0); glVertex3f(  win.x, -win.y, z);
             glVertex3f( -win.x,  win.y, 0); glVertex3f( -win.x,  win.y, z);
             glVertex3f(  win.x,  win.y, 0); glVertex3f(  win.x,  win.y, z);
             glVertex3f(0, 0, 0); glVertex3f(0, 0, z);
            glEnd();
        }

        // Draw name:
        glColor(ctx->fg_color());
        glRasterPos3f(0, 0, 0);
        std::stringstream s; print_name(s); gl_text(s.str().c_str());

        ctx->modelmatrix = saved;
    }

};


static Op* build(Node* node) {return new zpRectangleLight(node);}
const Op::Description zpRectangleLight::description("zpRectangleLight", build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("ReflectionCard", build);
#endif

} // namespace zpr

// end of zpRectangleLight.cpp

//
// Copyright 2020 DreamWorks Animation
//
