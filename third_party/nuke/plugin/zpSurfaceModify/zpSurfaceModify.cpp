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

/// @file zpSurfaceModify.cpp
///
/// @author Jonathan Egstad


#include <zprender/SurfaceShaderOp.h>
#include <zprender/ColorMapKnob.h>
#include <zprender/RenderContext.h>

#include <DDImage/VertexContext.h>
#include <DDImage/Knobs.h>


using namespace DD::Image;

namespace zpr {


enum { XFORM_NONE, XFORM_LOCAL_TO_WORLD, XFORM_WORLD_TO_LOCAL };
const char* xform_modes[] = { "none", "local-to-world", "world-to-local", 0 };

enum { TARGET_P, TARGET_N, TARGET_N_NG, TARGET_UV, TARGET_CF, TARGET_RGBA_OUT, TARGET_RGB_OUT };
const char* const shader_target_names[] = { "P", "N", "N+Ng", "UV", "Cf", "rgba-out", "rgb-out", 0 };

enum { OP_REPLACE, OP_OVER, OP_UNDER, OP_MULT, OP_ADD, OP_SUBTRACT };
const char* const operation_types[] = { "replace", "over", "under", "multiply", "add", "subtract", 0 };


/*! */
class zpSurfaceModify : public SurfaceShaderOp
{
    ColorMapKnob k_map;                 //!< Texture map input
    int          k_matrix;              //!< Which matrix to transform value by
    int          k_operation;           //!< How to apply the result to the output channel
    float        k_map_scale[3];        //!< Scale to apply before merging
    float        k_opacity_scale;
    int          k_shader_target;       //!< Which shader variable to map layer to


  public:
    static const Description description;
    /*virtual*/ const char* Class()     const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ " "
       "Modify shader variables - for example map or project normals onto a card to bump map it.\n"
       "Choose the shader variable to map the texture input channels to. The sampled values "
       "are applied to the selected shader-global-context variable and passed on up "
       "to the shader connected to input 0 of this node.\n"
       "If you select 'rgb-out' or 'rgba-out', it applies the texture channels to the output of "
       "this shader rather than the input."
       "";
    }


    //!
    zpSurfaceModify(::Node* node) :
        SurfaceShaderOp(node),
        k_map(this, 1/*input*/, 4/*num_channels*/, Chan_Red/*first_chan*/)
    {
        k_shader_target = TARGET_N;
        k_matrix        = XFORM_NONE;
        k_operation     = OP_REPLACE;
        k_map_scale[0] = k_map_scale[1] = k_map_scale[2] = 1.0f;
        k_opacity_scale = 1.0f;
    }


    /*virtual*/ int minimum_inputs() const { return 2; }
    /*virtual*/ int maximum_inputs() const { return 2; }


    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if (input == 0)
            return SurfaceShaderOp::test_input(0, op);
        return
            dynamic_cast<Iop*>(op) != NULL;
    }


    /*virtual*/
    Op* default_input(int input) const {
        if (input == 0)
            return Iop::default_input(input);
        return NULL;
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buf) const
    {
        if (input == 0)
            buf[0] = 0;
        else
            buf = const_cast<char*>("map");
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

        //k_texture_filter.knobs(f, "texture_filter", "texture filter");
        //   ClearFlags(f, Knob::STARTLINE);
        //   Tooltip(f, "This is the default filter that texture sampling shaders will use.  A shader can "
        //              "override this.");
        //
        Divider(f);
        Texture_knob(f, k_map, "color"/*name prefix*/, "source var");
            Tooltip(f, "Source map or shader variable to copy from.");
        Color_knob(f, k_map_scale, "scale", "scale");
            Tooltip(f, "Scale the xyz/rgb source channels before it's applied in the operation below.");
        Float_knob(f, &k_opacity_scale, "opacity_scale", "opacity scale");
            Tooltip(f, "Scale the opacity(alpha) channel, if applicable, before it's applied in the operation below.");
        Enumeration_knob(f, &k_matrix, xform_modes, "transform");
            Tooltip(f, "Matrix to transform value by before it's applied in the operation below.");
        Enumeration_knob(f, &k_operation, operation_types, "operation", "target operation");
            Tooltip(f, "How to combine map and destination.");
        Enumeration_knob(f, &k_shader_target, shader_target_names, "target_var", "target var");
            Tooltip(f, "Shader variable to copy source channels to:\n"
                       "P - XYZ position, in world-space\n"
                       "N - Shading normal vector, in world-space\n"
                       "N+Ng - Shading normal & geometric normal vectors, in world-space\n"
                       "UV - XY texture coordinate\n"
                       "rgb-out - OUTPUT rgb color\n"
                       "rgba-out - OUTPUT rgba color");
        //
        Obsolete_knob(f, "surface var", "knob target_var $value");
        Obsolete_knob(f, "map", "knob color_layer $value");
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        if (k_map.knobChanged(k))
            return 1;
        return 0;
    }


    /*virtual*/
    void _validate(bool for_real)
    {
        //printf("zpSurfaceModify::_validate(%d)\n", for_real);
        // This validates all inputs:
        SurfaceShaderOp::_validate(for_real);
        // Validate map knob:
        k_map.validateColorMap(for_real);
    }


    /*virtual*/
    void _request(int x, int y, int r, int t, ChannelMask channels, int count)
    {
        //std::cout << "zpSurfaceModify::_request()" << std::endl;
        // Requests surface color channels from input0:
        SurfaceShaderOp::_request(x, y, r, t, channels, count);
        // Request map knobs:
        k_map.requestColorMap(count);
    }


    /*! The ray-tracing shader call.
    */
    /*virtual*/
    void _evaluateShading(RayShaderContext& stx,
                          Fsr::Pixel&       color_out)
    {
        Fsr::Vec3f map_value(0.0f, 0.0f, 0.0f);
        float map_opacity = 1.0f;

        bool sampled = false;
        if (k_map.isEnabled())
        {
            map_value = k_map.sample(stx, &map_opacity);
            sampled = true;
            map_value.x *= k_map_scale[0];
            map_value.y *= k_map_scale[1];
            map_value.z *= k_map_scale[2];
            map_opacity *= k_opacity_scale;

#if 1
            const GeoInfoContext* gptx = stx.rprim->surface_ctx->parent_object_ctx->asGeoObject();
            if (!gptx)
            {
                // Cannot evaluate as a surface, skip it:
                SurfaceShaderOp::_evaluateShading(stx, color_out);
                return;
            }

            const bool is_identity = gptx->getGeoInfoSample(0).xform_is_identity;
#else
            bool is_identity = ((GeoInfoContext*)stx.rprim->surface_ctx->otx)->getGeoInfoSample(0).xform_is_identity;
#endif

            // Don't apply color here, do it after input shader call:
            if (k_shader_target < TARGET_RGBA_OUT)
            {
                switch (k_shader_target)
                {
                    case TARGET_P:
                        if (is_identity)
                            ; // do nothing
                        else if (k_matrix == XFORM_LOCAL_TO_WORLD)
                            stx.PW = stx.PWg = stx.l2w->transform(Fsr::Vec3d(map_value));
                        else if (k_matrix == XFORM_WORLD_TO_LOCAL)
                            stx.PW = stx.PWg = stx.w2l->transform(Fsr::Vec3d(map_value));
                        break;

                    case TARGET_N: {
                        Fsr::Vec3f N(map_value);
                        N.normalize();
                        // Transform by inverse transposed:
                        if      (is_identity) ; // do nothing
                        else if (k_matrix == XFORM_LOCAL_TO_WORLD) { N = stx.w2l->normalTransform(N); N.normalize(); }
                        else if (k_matrix == XFORM_WORLD_TO_LOCAL) { N = stx.l2w->normalTransform(N); N.normalize(); }
                        stx.N  = stx.Ns = N; // assign shading-normal(N) & shading-normal-no-bump(Ns)
                        stx.Nf = faceOutward(N, stx); // Facing-outward shading normal
                        break;}

                    case TARGET_N_NG: {
                        Fsr::Vec3f N(map_value);
                        N.normalize();
                        // Transform by inverse transposed:
                        if      (is_identity) ; // do nothing
                        else if (k_matrix == XFORM_LOCAL_TO_WORLD) { N = stx.w2l->normalTransform(N); N.normalize(); }
                        else if (k_matrix == XFORM_WORLD_TO_LOCAL) { N = stx.l2w->normalTransform(N); N.normalize(); }
                        stx.N  = stx.Ns = N; // assign shading-normal(N) & shading-normal-no-bump(Ns)
                        stx.Nf = stx.Ng = N;
                        break;}

                    case TARGET_UV:
                        stx.UV = Fsr::Vec2f(map_value.x, map_value.y);
                        break;

                    case TARGET_CF:
                        stx.Cf = Fsr::Vec4f(map_value.x, map_value.y, map_value.z, map_opacity);
                        break;
                }
            }
        }

        // Base class call will pass it on up to input0:
        SurfaceShaderOp::_evaluateShading(stx, color_out);

        if (sampled && k_shader_target >= TARGET_RGBA_OUT)
        {
            Fsr::Pixel src(Mask_RGBA);
            src[Chan_Red  ] = map_value.x;
            src[Chan_Green] = map_value.y;
            src[Chan_Blue ] = map_value.z;
            if (k_shader_target == TARGET_RGB_OUT)
            {
                // Only affect RGB:
                src.channels = Mask_RGB;
                src[Chan_Alpha] = 1.0f;
            }
            else
            {
                src[Chan_Alpha] = map_opacity;
            }
            switch (k_operation)
            {
                case OP_REPLACE:  color_out.replace(src); break;
                case OP_OVER:     color_out.over(src, map_opacity); break;
                case OP_UNDER:    color_out.under(src, color_out[Chan_Alpha]); break;
                case OP_MULT:     color_out *= src; break;
                case OP_ADD:      color_out += src; break;
                case OP_SUBTRACT: color_out *= src; break;
            }
        }
    }

    /*virtual*/
    void fragment_shader(const VertexContext& vtx,
                         Pixel&               out)
    {
        SurfaceShaderOp::fragment_shader(vtx, out);
    }

    /*virtual*/
    void vertex_shader(VertexContext& vtx)
    {
        //
        vtx.vP.Cf().set(1, 0, 0, 1);
    }


    //----------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------


    /*virtual*/
    bool set_texturemap(ViewerContext* vtx,
                        bool           gl)
    {
        if (k_map.isEnabled() && Op::input(1) && (input(0) == default_input(0)))
            return input1().set_texturemap(vtx, gl);
        else
            return input0().set_texturemap(vtx, gl);
    }


    /*virtual*/
    bool shade_GL(ViewerContext* vtx,
                  GeoInfo&       geo)
    {
        if (input(0) != default_input(0))
        {
            // Let input0 set itself up if connected:
            input0().shade_GL(vtx, geo);
        }
        else
        {
            // Otherwise do map input:
            if (k_map.isEnabled() && Op::input(1))
                input1().shade_GL(vtx, geo);
        }
        return true;
    }


    /*virtual*/
    void unset_texturemap(ViewerContext* vtx)
    {
        if (k_map.isEnabled() && Op::input(1) && (input(0) == default_input(0)))
            input1().unset_texturemap(vtx);
        else
            input0().unset_texturemap(vtx);
    }

};

static Op* build(Node* node) { return new zpSurfaceModify(node); }
const Op::Description zpSurfaceModify::description("zpSurfaceModify", build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("ModifySurface", build);
#endif

} // namespace zpr

// end of zpSurfaceModify.cpp

//
// Copyright 2020 DreamWorks Animation
//
