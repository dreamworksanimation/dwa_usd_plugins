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

/// @file zprBase.cpp
///
/// @author Jonathan Egstad


#include <zprender/RayShader.h>

namespace zpr {


/*! This is a sooper-simplified port of an Arnold shader written by Frankie Liu (fliu)
    with refraction support added.

    It's generally pretty naive and is not attempting to be a true GI shader.
*/
class zprBase : public RayShader
{
  public:
    // Each of these corresponds with an exposed input arrow connection:
    enum MaterialOpBindings
    {
        BG0,
        DIFFUSE1,
        SPECULAR2,
        EMISSION3,
        OPACITY4,
        DIFF_ROUGHNESS5,
        SPEC_ROUGHNESS6,
        //
        NUM_INPUTS
    };


    struct InputParams
    {
        InputBinding k_bindings[NUM_INPUTS];

        float        k_diffuse_factor;            //!< Overall diffuse multiplier
        Fsr::Vec3f   k_diffuse_tint;              //!< Color multiplier to diffuse
        float        k_diffuse_roughness;         //!< Diffuse roughness
        float        k_direct_diffuse_factor;     //!< Direct illumination multiplier
        float        k_indirect_diffuse_factor;   //!< Indirect illumination multiplier
        //
        float        k_opacity_factor;            //!< Overall opacity
        //
        float        k_specular_factor;           //!< Overall specular multiplier
        Fsr::Vec3f   k_specular_tint;             //!< Color multiplier to specular
        float        k_specular_roughness;        //!< Specular roughness
        float        k_direct_specular_factor;    //!< 
        float        k_indirect_specular_factor;  //!< 
        float        k_fresnel_factor;            //!< 
        //
        float        k_transmission_factor;       //!< Overall transmission multiplier
        double       k_index_of_refraction;       //!< Index of refraction value (where vacuum=1.0 & diamond=2.417)
        Fsr::Vec3f   k_transmission_tint;         //!< Color multiplier to transmission
        Fsr::Vec3f   k_total_int_reflection_tint; //!< Total-internal-reflection color tint
        //
        float        k_emission_factor;           //!< 
        Fsr::Vec3f   k_emission_tint;             //!< 
        //
        bool         k_enable_glossy_caustics;    //!< 
        bool         k_apply_fresnel_to_diffuse;  //!< 

        DD::Image::Channel k_direct_diffuse_output[4];      //!< AOV channels to route direct diffuse contribution to
        DD::Image::Channel k_indirect_diffuse_output[4];    //!< AOV channels to route indirect diffuse contribution to
        DD::Image::Channel k_direct_specular_output[4];     //!< AOV channels to route direct dspecular contribution to
        DD::Image::Channel k_indirect_specular_output[4];   //!< AOV channels to route indirect specular contribution to
        DD::Image::Channel k_transmission_output[4];        //!< AOV channels to route transmission contribution to
        DD::Image::Channel k_emission_output[4];            //!< AOV channels to route emission contribution to


        //!
        InputParams();
    };

    struct LocalVars
    {
        float m_opacity_factor; // clamped
        float m_fresnel_factor; // clamped

        bool  m_diffuse_enabled;
        bool  m_specular_enabled;
        bool  m_transmission_enabled;
        bool  m_emission_enabled;

        // AOV outputs:
        DD::Image::ChannelSet m_direct_diffuse_channels,  m_indirect_diffuse_channels;
        DD::Image::ChannelSet m_direct_specular_channels, m_indirect_specular_channels;
        DD::Image::ChannelSet m_transmission_channels;
        DD::Image::ChannelSet m_emission_channels;
        DD::Image::ChannelSet m_aov_channels;
    };


  public:
    InputParams inputs;
    LocalVars   locals;


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    zprBase();
    zprBase(const InputParams& _inputs);

    //!
    static void updateLocals(const InputParams& _inputs,
                             LocalVars&         _locals);

    /*virtual*/ InputBinding* getInputBinding(uint32_t input);
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       color_out);


};


} // namespace zpr

// end of zprBase.h

//
// Copyright 2020 DreamWorks Animation
//
