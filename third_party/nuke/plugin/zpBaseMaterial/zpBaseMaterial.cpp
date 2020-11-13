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

/// @file zpBaseMaterial.cpp
///
/// @author Jonathan Egstad


#include "zprBase.h"

#include <zprender/SurfaceMaterialOp.h>

#include <DDImage/VertexContext.h>
#include <DDImage/LightOp.h>
#include <DDImage/ViewerContext.h>
#include <DDImage/Scene.h>
#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>
#include <DDImage/gl.h>


using namespace DD::Image;

namespace zpr {


/*! This is a sooper-simplified port of an Arnold shader written by Frankie Liu (fliu)
    with refraction support added.

    It's generally pretty naive and is not attempting to be a true GI shader.
*/
class zpBaseMaterial : public SurfaceMaterialOp
{
  protected:
    zprBase zprShader;      //!< Local shader allocation for knobs to write into


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
       "Simplistic base shader used primarily for testing ray shading system.\n";
    }


    //!
    zpBaseMaterial(::Node* node) : SurfaceMaterialOp(node) {}


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        RayShader* output = new zprBase(zprShader.inputs);
        shaders.push_back(output);
        return output;
    }


    /*virtual*/ int minimum_inputs() const { return 4; }
    /*virtual*/ int maximum_inputs() const { return zprBase::NUM_INPUTS; }


    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0) return SurfaceMaterialOp::default_input(input);
        return NULL; // allow null on colormap inputs
    }


    /*virtual*/
    const char* input_label(int input, char* buffer) const
    {
        if      (input == 1) return "diff";
        else if (input == 2) return "spec";
        else if (input == 3) return "emis";
        else if (input == 4) return "opac";
        else if (input == 5) return "dRough";
        else if (input == 6) return "sRough";
        return NULL;
    }


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBindingForOpInput(uint32_t op_input)
    {
        if      (op_input == 0) return &zprShader.inputs.k_bindings[zprBase::BG0            ];
        else if (op_input == 1) return &zprShader.inputs.k_bindings[zprBase::DIFFUSE1       ];
        else if (op_input == 2) return &zprShader.inputs.k_bindings[zprBase::SPECULAR2      ];
        else if (op_input == 3) return &zprShader.inputs.k_bindings[zprBase::EMISSION3      ];
        else if (op_input == 4) return &zprShader.inputs.k_bindings[zprBase::OPACITY4       ];
        else if (op_input == 5) return &zprShader.inputs.k_bindings[zprBase::DIFF_ROUGHNESS5];
        else if (op_input == 6) return &zprShader.inputs.k_bindings[zprBase::SPEC_ROUGHNESS6];
        return NULL;
    }

    //! Return the Op input for a shader input, or -1 if binding is not exposed.
    /*virtual*/ int32_t getOpInputForShaderInput(uint32_t shader_input)
    {
        if      (shader_input == zprBase::BG0            ) return 0;
        else if (shader_input == zprBase::DIFFUSE1       ) return 1;
        else if (shader_input == zprBase::SPECULAR2      ) return 2;
        else if (shader_input == zprBase::EMISSION3      ) return 3;
        else if (shader_input == zprBase::OPACITY4       ) return 4;
        else if (shader_input == zprBase::DIFF_ROUGHNESS5) return 5;
        else if (shader_input == zprBase::SPEC_ROUGHNESS6) return 6;
        return -1;
    }


    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    /*virtual*/
    int32_t getGLTextureInput() const { return 1; }


    //----------------------------------------------------------------------------------


    /*virtual*/
    void _validate(bool for_real)
    {
        //std::cout << "zpBaseMaterial::_validate(" << for_real << ")" << std::endl;
        // Call base class first to get InputBindings assigned:
        SurfaceMaterialOp::_validate(for_real);

        zprShader.validateShader(for_real, NULL/*rtx*/, &outputContext()/*op_ctx*/);

        // Enable AOV output channels:
        info_.turn_on(zprShader.locals.m_aov_channels);
    }


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

        InputOp_knob(f, &zprShader.inputs.k_bindings[zprBase::BG0], 0/*input_num*/);

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Float_knob(f, &zprShader.inputs.k_diffuse_factor, "diffuse_factor", "diffuse factor");
            Obsolete_knob(f, "enable_diffuse", "knob diffuse_factor $value");
        Color_knob(f, &zprShader.inputs.k_diffuse_tint.x, IRange(0,1), "diffuse_tint", "diffuse tint");
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::DIFFUSE1], 1/*input*/, 4/*num_channels*/, "diffuse_color", "diffuse color map");
        //
        Float_knob(f, &zprShader.inputs.k_diffuse_roughness, "diffuse_roughness", "diffuse roughness");
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::DIFF_ROUGHNESS5], 5/*input*/, 1/*num_channels*/, "diffuse_roughness_map", "diffuse roughness map");
            Tooltip(f, "Optional input map to modulate diffuse roughness");
        //
        Float_knob(f, &zprShader.inputs.k_direct_diffuse_factor,   "direct_diffuse_factor",   "direct diffuse factor");
        Float_knob(f, &zprShader.inputs.k_indirect_diffuse_factor, "indirect_diffuse_factor", "indirect diffuse factor");
        //
        Divider(f);
        Float_knob(f, &zprShader.inputs.k_opacity_factor, "opacity_factor", "opacity factor");
            Obsolete_knob(f, "opacity", "knob opacity_factor $value");
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::OPACITY4], 4/*input*/, 1/*num_channels*/, "opacity", "opacity map");
        //
        Divider(f);
        Float_knob(f, &zprShader.inputs.k_specular_factor, "specular_factor", "specular factor");
            Obsolete_knob(f, "enable_specular", "knob specular_factor $value");
        Color_knob(f, &zprShader.inputs.k_specular_tint.x, IRange(0,1), "specular_tint", "specular tint");
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::SPECULAR2], 2/*input*/, 3/*num_channels*/, "specular_color", "specular color map");
        //
        Float_knob(f, &zprShader.inputs.k_specular_roughness, "specular_roughness", "specular roughness");
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::SPEC_ROUGHNESS6], 6/*input*/, 1/*num_channels*/, "specular_roughness_map", "specular roughness map");
            Tooltip(f, "Optional input map to modulate specular roughness");
        //
        Float_knob(f, &zprShader.inputs.k_fresnel_factor, "fresnel_factor", "frensel factor");
        //
        Float_knob(f, &zprShader.inputs.k_direct_specular_factor,   "direct_specular_factor",   "direct specular factor");
        Float_knob(f, &zprShader.inputs.k_indirect_specular_factor, "indirect_specular_factor", "indirect specular factor");
        //
        Divider(f);
        Float_knob(f, &zprShader.inputs.k_transmission_factor, "transmission_factor", "transmission factor");
            Tooltip(f, "Transmission multiplier where 0 = no transmission.");
        Color_knob(f, &zprShader.inputs.k_transmission_tint.x, IRange(0,1), "transmission_tint", "transmission tint");
        Double_knob(f, &zprShader.inputs.k_index_of_refraction, IRange(1,3), "index_of_refraction", "index of refraction");
            Tooltip(f, "Index-of-refraction value for material.  Here's a list of commonly used values:\n"
                       "vacuum          1.0\n"
                       "air @ stp       1.00029\n"
                       "ice             1.31\n"
                       "water @ 20c     1.33\n"
                       "acetone         1.36\n"
                       "ethyl alcohol   1.36\n"
                       "fluorite        1.433\n"
                       "fused quartz    1.46\n"
                       "glycerine       1.473\n"
                       "glass low       1.52\n"
                       "glass med       1.57\n"
                       "glass high      1.62\n"
                       "diamond         2.417");
        Color_knob(f, &zprShader.inputs.k_total_int_reflection_tint.x, IRange(0,1), "total_int_reflection_tint", "total int reflection tint");

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Float_knob(f, &zprShader.inputs.k_emission_factor, "emission_factor", "emission factor");
            Obsolete_knob(f, "enable_emission", "knob emission_factor $value");
        Color_knob(f, &zprShader.inputs.k_emission_tint.x, IRange(0,4), "emission_tint", "emission tint");
           SetFlags(f, Knob::LOG_SLIDER);
        ColorMap_knob(f, &zprShader.inputs.k_bindings[zprBase::EMISSION3], 3/*input*/, 3/*num_channels*/, "emission_color", "emission color map");

        //----------------------------------------------------------------------------------------------
        //----------------------------------------------------------------------------------------------
        Tab_knob(f, "aov outputs");
        //Divider(f);
        static const char* aov_tooltip =
            "Route this shader component to these output channels.  If an alpha is present in the component "
            "it will also be output (this is useful when an alpha is required from a reflected object rather "
            "than the object this shader is attached to.)";
        Channel_knob(f, zprShader.inputs.k_direct_diffuse_output,    4, "direct_diffuse_output",    "direct diffuse output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, zprShader.inputs.k_direct_specular_output,   4, "direct_specular_output",   "direct specular output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, zprShader.inputs.k_indirect_diffuse_output,  4, "indirect_diffuse_output",  "indirect diffuse output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, zprShader.inputs.k_indirect_specular_output, 4, "indirect_specular_output", "indirect specular output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, zprShader.inputs.k_transmission_output,      4, "transmission_output",      "transmission output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, zprShader.inputs.k_emission_output,          4, "emission_output",          "emission output");
           Tooltip(f, aov_tooltip);
    }


};


static Op* build(Node* node) {return new zpBaseMaterial(node);}
const Op::Description zpBaseMaterial::description("zpBaseMaterial", build);

// Map old plugin name to new:
static const Op::Description old_description("BaseSurface", build);


} // namespace zpr

// end of zpBaseMaterial.cpp

//
// Copyright 2020 DreamWorks Animation
//
