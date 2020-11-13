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

#include "zprModify.h"

#include <zprender/SurfaceMaterialOp.h>

#include <DDImage/VertexContext.h>
#include <DDImage/Knobs.h>


using namespace DD::Image;

namespace zpr {


/*! */
class zpSurfaceModify : public SurfaceMaterialOp
{
  protected:
    zprModify::InputParams k_inputs;


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
    zpSurfaceModify(::Node* node) : SurfaceMaterialOp(node) {}


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        RayShader* output = new zprModify(k_inputs);
        shaders.push_back(output);
        return output;
    }


    /*virtual*/ int minimum_inputs() const { return zprModify::NUM_INPUTS; }
    /*virtual*/ int maximum_inputs() const { return zprModify::NUM_INPUTS; }


    /*virtual*/
    Op* default_input(int input) const {
        if (input == 0) return Iop::default_input(input);
        return NULL; // allow null on colormap inputs
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buffer) const
    {
        if      (input == 0) buffer = const_cast<char*>("");
        else if (input == 1) buffer = const_cast<char*>("map");
        return buffer;
    }


    //----------------------------------------------------------------------------------


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBindingForOpInput(uint32_t op_input)
    {
        if      (op_input == 0) return &k_inputs.k_bindings[zprModify::BG0 ];
        else if (op_input == 1) return &k_inputs.k_bindings[zprModify::MAP1];
        return NULL;
    }

    //! Return the Op input for a shader input, or -1 if binding is not exposed.
    /*virtual*/ int32_t getOpInputForShaderInput(uint32_t shader_input)
    {
        if      (shader_input == zprModify::BG0 ) return 0;
        else if (shader_input == zprModify::MAP1) return 1;
        return -1;
    }


    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    /*virtual*/
    int32_t getGLTextureInput() const { return (k_inputs.k_shader_target >= zprModify::TARGET_RGBA_OUT) ? 1 : -1; }


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

        InputOp_knob(f, &k_inputs.k_bindings[zprModify::BG0], 0/*input_num*/);

        //----------------------------------------------------------------------------------------------
        //k_texture_filter.knobs(f, "texture_filter", "texture filter");
        //   ClearFlags(f, Knob::STARTLINE);
        //   Tooltip(f, "This is the default filter that texture sampling shaders will use.  A shader can "
        //              "override this.");
        //
        Divider(f);
        ColorMap_knob(f, &k_inputs.k_bindings[zprModify::MAP1], 1/*input*/, 4/*num_chans*/, "color"/*name prefix*/, "source var");
            Tooltip(f, "Source map or shader variable to copy from.");
        Color_knob(f, k_inputs.k_map_scale.array(), "scale", "scale");
            Tooltip(f, "Scale the xyz/rgb source channels before it's applied in the operation below.");
        Float_knob(f, &k_inputs.k_opacity_scale, "opacity_scale", "opacity scale");
            Tooltip(f, "Scale the opacity(alpha) channel, if applicable, before it's applied in the operation below.");
        Enumeration_knob(f, &k_inputs.k_matrix, zprModify::xform_modes, "transform");
            Tooltip(f, "Matrix to transform value by before it's applied in the operation below.");
        Enumeration_knob(f, &k_inputs.k_operation, zprModify::operation_types, "operation", "target operation");
            Tooltip(f, "How to combine map and destination.");
        Enumeration_knob(f, &k_inputs.k_shader_target, zprModify::shader_target_names, "target_var", "target var");
            Tooltip(f, "Shader variable to copy source channels to:\n"
                       "P - XYZ position, in world-space\n"
                       "N - Shading normal vector, in world-space\n"
                       "N+Ng - Shading normal & geometric normal vectors, in world-space\n"
                       "UV - XY texture coordinate\n"
                       "rgb-out - OUTPUT rgb color\n"
                       "rgba-out - OUTPUT rgba color");

        Obsolete_knob(f, "surface var", "knob target_var $value");
        Obsolete_knob(f, "map", "knob color_layer $value");
    }


    //----------------------------------------------------------------------------------


    /*virtual*/
    void fragment_shader(const VertexContext& vtx,
                         Pixel&               out)
    {
        SurfaceMaterialOp::fragment_shader(vtx, out);
    }


    /*virtual*/
    void vertex_shader(VertexContext& vtx)
    {
        //
        vtx.vP.Cf().set(1, 0, 0, 1);
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
