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


#include <zprender/SurfaceShaderOp.h>
#include <zprender/ColorMapKnob.h>
#include <zprender/LightShader.h>
#include <zprender/ThreadContext.h>

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
class zpBaseMaterial : public SurfaceShaderOp
{
    float        k_diffuse_factor;            //!< Overall diffuse multiplier
    Fsr::Vec3f   k_diffuse_tint;              //!< Color multiplier to diffuse
    ColorMapKnob k_diffuse_color_map;         //!< Texture map for diffuse color
    float        k_diffuse_roughness;         //!< Diffuse roughness
    ColorMapKnob k_diffuse_roughness_map;     //!< Texture map for diffuse roughness
    float        k_direct_diffuse_factor;     //!< Direct illumination multiplier
    float        k_indirect_diffuse_factor;   //!< Indirect illumination multiplier
    //
    float        k_opacity_factor;            //!< Overall opacity
    ColorMapKnob k_opacity_map;               //!< Texture map for opacity
    //
    float        k_specular_factor;           //!< Overall specular multiplier
    Fsr::Vec3f   k_specular_tint;             //!< Color multiplier to specular
    ColorMapKnob k_specular_color_map;        //!< Texture map for specular
    float        k_specular_roughness;        //!< Specular roughness
    ColorMapKnob k_specular_roughness_map;    //!< Texture map for specular roughness
    float        k_fresnel_factor;            //!< 
    //
    float        k_direct_specular_factor;    //!< 
    float        k_indirect_specular_factor;  //!< 
    //
    float        k_transmission_factor;       //!< Overall transmission multiplier
    double       k_index_of_refraction;       //!< Index of refraction value (where vacuum=1.0 & diamond=2.417)
    Fsr::Vec3f   k_transmission_tint;         //!< Color multiplier to transmission
    Fsr::Vec3f   k_total_int_reflection_tint; //!< Total-internal-reflection color tint
    //
    float        k_emission_factor;           //!< 
    Fsr::Vec3f   k_emission_tint;             //!< 
    ColorMapKnob k_emission_color_map;        //!< 
    //
    bool         k_enable_glossy_caustics;    //!< 
    bool         k_enable_fresnel;            //!< 
    bool         k_apply_fresnel_to_diffuse;  //!< 
    //
    bool         k_enable_diffuse;            //!< 
    bool         k_enable_specular;           //!< 
    bool         k_enable_transmission;       //!< 
    bool         k_enable_emission;           //!< 
    //
    Channel k_direct_diffuse_output[4];       //!< AOV channels to route direct diffuse contribution to
    Channel k_indirect_diffuse_output[4];     //!< AOV channels to route indirect diffuse contribution to
    Channel k_direct_specular_output[4];      //!< AOV channels to route direct dspecular contribution to
    Channel k_indirect_specular_output[4];    //!< AOV channels to route indirect specular contribution to
    Channel k_transmission_output[4];         //!< AOV channels to route transmission contribution to
    Channel k_emission_output[4];             //!< AOV channels to route emission contribution to


    float m_opacity_factor; // clamped
    bool  m_diffuse_enabled;
    bool  m_specular_enabled;
    bool  m_transmission_enabled;
    bool  m_emission_enabled;

    int mapInputsStart, mapInputsEnd;
    ChannelSet m_direct_diffuse_channels,  m_indirect_diffuse_channels;
    ChannelSet m_direct_specular_channels, m_indirect_specular_channels;
    ChannelSet m_transmission_channels;
    ChannelSet m_emission_channels;
    ChannelSet m_aov_channels;


  public:
    static const Description description;
    const char* Class()     const {return description.name;}
    const char* node_help() const {return __DATE__ " " __TIME__ " "
       "Simplistic base shader used primarily for testing ray shading system.\n";
    }


    //!
    zpBaseMaterial(::Node* node) :
        SurfaceShaderOp(node),
        k_diffuse_color_map(this,      1/*input*/, 4/*num_channels*/, Chan_Red/*first_chan*/),
        k_diffuse_roughness_map(this,  5/*input*/, 1/*num_channels*/, Chan_Red/*first_chan*/),
        k_opacity_map(this,            4/*input*/, 1/*num_channels*/, Chan_Red/*first_chan*/),
        k_specular_color_map(this,     2/*input*/, 3/*num_channels*/, Chan_Red/*first_chan*/),
        k_specular_roughness_map(this, 6/*input*/, 1/*num_channels*/, Chan_Red/*first_chan*/),
        k_emission_color_map(this,     3/*input*/, 3/*num_channels*/, Chan_Red/*first_chan*/)
    {
        k_diffuse_factor           = 1.0f;
        k_diffuse_tint             = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
        k_diffuse_roughness        = 0.0f;
        k_direct_diffuse_factor    = 1.0f;
        k_indirect_diffuse_factor  = 0.0f;
        k_opacity_factor           = 1.0f;
        //
        k_specular_factor          = 1.0f;
        k_specular_tint            = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
        k_specular_roughness       = 0.05f;
        k_fresnel_factor           = 1.0f;
        for (unsigned i=0; i < 4; ++i)
        {
            k_direct_diffuse_output[i] = k_indirect_diffuse_output[i] = 
            k_direct_specular_output[i] = k_indirect_specular_output[i] = 
            k_transmission_output[i] =
            k_emission_output[i] = Chan_Black;//Channel(Chan_Red + i);
        }
        //
        k_direct_specular_factor   = 1.0f;
        k_indirect_specular_factor = 1.0f;
        //
        k_transmission_factor      = 1.0f;
        k_index_of_refraction      = 1.0; // no refraction
        k_transmission_tint        = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
        k_total_int_reflection_tint = Fsr::Vec3f(0.0f, 0.0f, 0.0f);
        //
        k_emission_factor          = 0.0f;
        k_emission_tint            = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
        //
        k_enable_glossy_caustics   = false;
        k_enable_fresnel           = false;
        k_apply_fresnel_to_diffuse = false;
        //
        k_enable_diffuse           = true;
        k_enable_specular          = false;
        k_enable_transmission      = false;
        k_enable_emission          = false;

        mapInputsStart = 1;  // k_diffuse_color_map
        mapInputsEnd   = 6;  // k_specular_roughness_map
    }


    /*virtual*/ int minimum_inputs() const { return 1 + (mapInputsEnd-mapInputsStart+1); }
    /*virtual*/ int maximum_inputs() const { return 1 + (mapInputsEnd-mapInputsStart+1); }


    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0)
            return SurfaceShaderOp::default_input(input);
        if (input >= mapInputsStart && input <= mapInputsEnd)
            return NULL;
        return NULL;
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
        Bool_knob(f, &k_enable_diffuse, "enable_diffuse", "enable diffuse");
        Bool_knob(f, &k_enable_specular, "enable_specular", "enable specular");
        Bool_knob(f, &k_enable_transmission, "enable_transmission", "enable transmission");
        Newline(f);
        Bool_knob(f, &k_enable_emission, "enable_emission", "enable emission");
        Bool_knob(f, &k_enable_fresnel, "enable_fresnel", "enable fresnel");

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Float_knob(f, &k_diffuse_factor, "diffuse_factor", "diffuse factor");
        Color_knob(f, &k_diffuse_tint.x, IRange(0,1), "diffuse_tint", "diffuse tint");
        Texture_knob(f, k_diffuse_color_map, "diffuse_color", "diffuse color map");
        //
        Float_knob(f, &k_diffuse_roughness, "diffuse_roughness", "diffuse roughness");
        Texture_knob(f, k_diffuse_roughness_map, "diffuse_roughness_map", "diffuse roughness map");
            Tooltip(f, "Optional input map to modulate diffuse roughness");
        //
        Float_knob(f, &k_direct_diffuse_factor,   "direct_diffuse_factor",   "direct diffuse factor");
        Float_knob(f, &k_indirect_diffuse_factor, "indirect_diffuse_factor", "indirect diffuse factor");
        //
        Divider(f);
        Float_knob(f, &k_opacity_factor, "opacity_factor", "opacity factor");
            Obsolete_knob(f, "opacity", "knob opacity_factor $value");
        Texture_knob(f, k_opacity_map, "opacity", "opacity map");
        //
        Divider(f);
        Float_knob(f, &k_specular_factor, "specular_factor", "specular factor");
        Color_knob(f, &k_specular_tint.x, IRange(0,1), "specular_tint", "specular tint");
        Texture_knob(f, k_specular_color_map, "specular_color", "specular color map");
        //
        Float_knob(f, &k_specular_roughness, "specular_roughness", "specular roughness");
        Texture_knob(f, k_specular_roughness_map, "specular_roughness_map", "specular roughness map");
            Tooltip(f, "Optional input map to modulate specular roughness");
        //
        Float_knob(f, &k_fresnel_factor, "fresnel_factor", "frensel factor");
        //
        Float_knob(f, &k_direct_specular_factor,   "direct_specular_factor",   "direct specular factor");
        Float_knob(f, &k_indirect_specular_factor, "indirect_specular_factor", "indirect specular factor");
        //
        Divider(f);
        Float_knob(f, &k_transmission_factor, "transmission_factor", "transmission factor");
            Tooltip(f, "Transmission multiplier where 0 = no transmission.");
        Color_knob(f, &k_transmission_tint.x, IRange(0,1), "transmission_tint", "transmission tint");
        Double_knob(f, &k_index_of_refraction, IRange(1,3), "index_of_refraction", "index of refraction");
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
        Color_knob(f, &k_total_int_reflection_tint.x, IRange(0,1), "total_int_reflection_tint", "total int reflection tint");

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Float_knob(f, &k_emission_factor, "emission_factor", "emission factor");
        Color_knob(f, &k_emission_tint.x, IRange(0,4), "emission_tint", "emission tint");
           SetFlags(f, Knob::LOG_SLIDER);
        Texture_knob(f, k_emission_color_map, "emission_color", "emission color map");

        //----------------------------------------------------------------------------------------------
        //----------------------------------------------------------------------------------------------
        Tab_knob(f, "aov outputs");
        //Divider(f);
        static const char* aov_tooltip =
            "Route this shader component to these output channels.  If an alpha is present in the component "
            "it will also be output (this is useful when an alpha is required from a reflected object rather "
            "than the object this shader is attached to.)";
        Channel_knob(f, k_direct_diffuse_output,    4, "direct_diffuse_output",    "direct diffuse output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, k_direct_specular_output,   4, "direct_specular_output",   "direct specular output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, k_indirect_diffuse_output,  4, "indirect_diffuse_output",  "indirect diffuse output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, k_indirect_specular_output, 4, "indirect_specular_output", "indirect specular output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, k_transmission_output,      4, "transmission_output",      "transmission output");
           Tooltip(f, aov_tooltip);
        Channel_knob(f, k_emission_output,          4, "emission_output",          "emission output");
           Tooltip(f, aov_tooltip);
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        int ret = 0;
        if (k_diffuse_color_map.knobChanged(k))
            ++ret;
        if (k_diffuse_roughness_map.knobChanged(k))
            ++ret;
        if (k_specular_color_map.knobChanged(k))
            ++ret;
        if (k_specular_roughness_map.knobChanged(k))
            ++ret;
        if (k_emission_color_map.knobChanged(k))
            ++ret;
        if (k_opacity_map.knobChanged(k))
            ++ret;
        return (ret > 0);
    }


    //----------------------------------------------------------------------------------


    /*virtual*/
    bool set_texturemap(ViewerContext* ctx, bool gl)
    {
        if (k_enable_diffuse && Op::input(1))
            return ((Iop*)Op::input(1))->set_texturemap(ctx, gl);
        else if (k_enable_emission && Op::input(3))
            return ((Iop*)Op::input(3))->set_texturemap(ctx, gl);
        return false;
    }


    /*virtual*/
    bool shade_GL(ViewerContext* ctx, GeoInfo& geo)
    {
        if (k_enable_diffuse && Op::input(1) && !((Iop*)Op::input(1))->shade_GL(ctx, geo))
            return false;
        else if (k_enable_emission && Op::input(3) && !((Iop*)Op::input(3))->shade_GL(ctx, geo))
            return false;
        return true;
    }


    /*virtual*/
    void unset_texturemap(ViewerContext* ctx)
    {
        if (k_enable_diffuse && Op::input(1))
            ((Iop*)Op::input(1))->unset_texturemap(ctx);
        if (k_enable_emission && Op::input(3))
            ((Iop*)Op::input(3))->unset_texturemap(ctx);
    }


    //----------------------------------------------------------------------------------


    /*virtual*/
    void _validate(bool for_real)
    {
        //std::cout << "zpBaseMaterial::_validate(" << for_real << ")" << std::endl;
        SurfaceShaderOp::_validate(for_real);

        // Validate map knobs:
        k_diffuse_color_map.validateColorMap(for_real);
        k_diffuse_roughness_map.validateColorMap(for_real);
        k_specular_color_map.validateColorMap(for_real);
        k_specular_roughness_map.validateColorMap(for_real);
        k_emission_color_map.validateColorMap(for_real);
        k_opacity_map.validateColorMap(for_real);

        m_opacity_factor       = Fsr::clamp(k_opacity_factor); // clamp 0..1
        m_diffuse_enabled      = (k_enable_diffuse  && (k_direct_diffuse_factor  > 0.0 || k_indirect_diffuse_factor ));
        m_specular_enabled     = (k_enable_specular && (k_direct_specular_factor > 0.0 || k_indirect_specular_factor));
        m_transmission_enabled = (k_enable_transmission && k_transmission_factor > 0.0);
        m_emission_enabled     = (k_enable_emission && k_emission_factor > 0.0);

        // Build output AOV channel sets:
        m_direct_diffuse_channels.clear();
        m_direct_diffuse_channels.insert(k_direct_diffuse_output, 4);
        m_indirect_diffuse_channels.clear();
        m_indirect_diffuse_channels.insert(k_indirect_diffuse_output, 4);
        m_direct_specular_channels.clear();
        m_direct_specular_channels.insert(k_direct_specular_output, 4);
        m_indirect_specular_channels.clear();
        m_indirect_specular_channels.insert(k_indirect_specular_output, 4);
        m_transmission_channels.clear();
        m_transmission_channels.insert(k_transmission_output, 4);
        m_emission_channels.clear();
        m_emission_channels.insert(k_emission_output, 4);

        // Build final output channel mask:
        m_aov_channels.clear();
        if (k_enable_diffuse)
        {
            m_aov_channels += m_direct_diffuse_channels;
            m_aov_channels += m_indirect_diffuse_channels;
        }
        if (k_enable_specular)
        {
            m_aov_channels += m_direct_specular_channels;
            m_aov_channels += m_indirect_specular_channels;
        }
        m_aov_channels += m_transmission_channels;
        m_aov_channels += m_emission_channels;

        // Enable AOV output channels:
        info_.turn_on(m_aov_channels);
    }


    /*virtual*/
    void _request(int x, int y, int r, int t, ChannelMask channels, int count)
    {
        //std::cout << "zpBaseMaterial::_request()" << std::endl;
        // Requests surface color channels from input0:
        SurfaceShaderOp::_request(x, y, r, t, channels, count);

        // Request map knobs:
        if (m_diffuse_enabled)
        {
            k_diffuse_color_map.requestColorMap(count);
            k_diffuse_roughness_map.requestColorMap(count);
        }
        if (m_specular_enabled)
        {
            k_specular_color_map.requestColorMap(count);
            k_specular_roughness_map.requestColorMap(count);
        }
        if (m_emission_enabled)
            k_emission_color_map.requestColorMap(count);
        k_opacity_map.requestColorMap(count);
    }


    //----------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------


    /*! The ray-tracing shader call.
    */
    /*virtual*/
    void _evaluateShading(RayShaderContext& stx,
                          Fsr::Pixel&       out)
    {
        //std::cout << "zpBaseMaterial::_evaluate_shading() [" << stx.x << " " << stx.y << "]" << std::endl;
        // TODO: make this a debug assert and move logic to RayShader base class call
        // Don't bother if it's a shadow ray:
        if (stx.Rtx.type_mask & Fsr::RayContext::SHADOW)
            return;

        // Always output RGBA:
        out.channels += Mask_RGBA;
        // Enable AOV output channels:
        out.channels += m_aov_channels;

        Fsr::Vec3f& out_color = out.color();
        float&      out_alpha = out.opacity();
        out_alpha = 1.0f;

        float opacity;
        Fsr::Vec3f diffColor, specColor, transColor;

        //------------------------------------------------------------------
        // Get base colors from bindings or direct knobs:
        //
        if (k_opacity_map.isEnabled() && m_opacity_factor >= 0.0001f)
        {
            const Fsr::Vec3f op = k_diffuse_color_map.sample(stx, 0/*alpha*/);
            opacity = op.x;
        }
        else
            opacity = m_opacity_factor;

        if (m_emission_enabled)
        {
            if (k_emission_color_map.isEnabled() && k_emission_factor > 0.0001f)
                out_color = k_emission_color_map.sample(stx)*k_emission_tint;
            else
                out_color = k_emission_tint;
            out_color *= k_emission_factor;
        }
        else
            out_color.set(0.0f, 0.0f, 0.0f);

        if (m_diffuse_enabled)
        {
            if (k_diffuse_color_map.isEnabled() && k_diffuse_factor > 0.0001f)
                diffColor = k_diffuse_color_map.sample(stx, &out_alpha)*k_diffuse_tint;
            else
                diffColor = k_diffuse_tint;
            diffColor *= k_diffuse_factor;
        }


        //------------------------------------------------------------------
        // If no lighting enabled switch to direct diffuse mapping + emission:
        //
        if (!stx.direct_lighting_enabled && !stx.indirect_lighting_enabled)
        {
            if (m_diffuse_enabled)
                out_color += diffColor;

            // Modulate final color by opacity & opacity map:
            if (opacity >= 0.0001f)
            {
                out_color *= opacity;
                out_alpha *= opacity;
            }
            return;
        }


        //------------------------------------------------------------------
        // Get lighting colors from bindings or direct knobs:
        //
        if (m_specular_enabled)
        {
            if (k_specular_color_map.isEnabled())
                specColor = k_specular_color_map.sample(stx)*k_specular_tint;
            else
                specColor = k_specular_tint;
        }

        if (m_transmission_enabled)
        {
            //if (k_transmission_map.isEnabled())
            //    transColor = k_transmission_map.sample(stx)*k_transmission_tint;
            //else
                transColor = k_transmission_tint;
        }


        //------------------------------------------------------------------
        //
        float Ks                     = 1.0f;
        float spec_roughness         = 0.0f;
        float spec_roughness_squared = 0.0f;
        if (m_specular_enabled || m_transmission_enabled)
        {
            Ks                     = k_specular_factor;
            spec_roughness         = clamp(k_specular_roughness);
            spec_roughness_squared = spec_roughness*spec_roughness;
        }


        const Fsr::Vec3d V = -stx.Rtx.dir();
        // Fresnel-weighted reflectance and transmission weight
        // transWeight + fresnel_factor <= 1.0
        //
        // If k_apply_fresnel_to_diffuse true, transWeight = 1-fresnel_factor
        Fsr::Vec3f transWeight(1,1,1);
        float      fresnel_factor = 0.0f;
        if (k_enable_fresnel)
        {
            fresnel_factor = clamp(k_fresnel_factor);
#if 1
            Fsr::Vec3f specColorFresnel(1,1,1);
#else
            //Fsr::Vec3f specColorFresnel(1,1,1);
            //AiFresnelWeightRGB(&sg->Nf, &sg->Rd, &specColor, &specColorFresnel);
            double eta     = ior_from/ior_to
            double base    = std::max(0.0, 1.0 - N.dot(V));
            double fZero   = pow((1.0 - eta) / (1.0 + eta), 2.0);
            double fresnel_weight = fZero + (1 - fZero)*pow(base, 5.0);
#endif
            // this is just a lerp!:
            specColor = specColor*(1.0f - fresnel_factor) + specColorFresnel*fresnel_factor;

            // If you want the transmssion and reflectance to sum to 1:
            if (k_apply_fresnel_to_diffuse)
                transWeight = Fsr::Vec3f(1,1,1) - specColor;
        }


        //------------------------------------------------------------------
        // Get ratio of reflection vs. refraction:
        //
        double ior_from = stx.index_of_refraction;
        // Default to air - this should be handled by illumination() call...:
        if (ior_from < 1.0)
            ior_from = 1.0;
        double ior_to = ::fabs(k_index_of_refraction);
        if (ior_to < 1.0)
            ior_to = 1.0;

        // Are we inside the object still?
        if (::fabs(ior_from - ior_to) < std::numeric_limits<double>::epsilon())
        {
            // 
        }
        //std::cout << "ior_from=" << ior_from << " ior_to=" << ior_to;

        float reflect_vs_transmit_ratio = 1.0f; // all reflection default
        if (m_specular_enabled || m_transmission_enabled)
        {
            //std::cout << " reflect_vs_transmit_ratio=" << reflect_vs_transmit_ratio;
            //std::cout << " refraction_ratio=" << refraction_ratio(V, stx.Nf, ior_from, ior_to);
            //std::cout << std::endl;

            reflect_vs_transmit_ratio = reflectionRatioSnellSchlick(V,
                                                                    stx.Nf,
                                                                    ior_from,
                                                                    ior_to,
                                                                    double(1.0f + fresnel_factor*4.0f));
        }


        //------------------------------------------------------------------
        // Direct lighting
        //
        if (stx.direct_lighting_enabled && stx.master_light_shaders)
        {
            //------------------------------------------------------------------
            // Direct diffuse
            //
            if (m_diffuse_enabled)
            {
                Fsr::Vec3f diffDirect = (diffColor*k_direct_diffuse_factor) * transWeight;
                if (diffDirect.notZero())
                {
                    Fsr::Vec3f illum(0,0,0); // Direct illumination accumulator

// TODO: finish the direct lighting! Need to test ray-traced light shadowing
#if 1
                    Fsr::Pixel& light_color = stx.thread_ctx->light_color;
                    light_color.channels = DD::Image::Mask_RGB;

                    const uint32_t nLights = (uint32_t)stx.master_light_shaders->size();
                    for (uint32_t i=0; i < nLights; ++i)
                    {
                        LightShader* lshader = (*stx.master_light_shaders)[i];
                        lshader->evaluateShading(stx, light_color);
                        illum.x += light_color[Chan_Red  ];
                        illum.y += light_color[Chan_Green];
                        illum.z += light_color[Chan_Blue ];
                        //std::cout << "  light_color" << light_color.color() << std::endl;
                        //std::cout << "  illum" << illum << std::endl;
                    }
#if 0
                    const Vector3 V = stx.getViewVector(); // this may build a fake-stereo view-vector

                    // Get current ObjectContext index to find lighting scene:
                    const ObjectContext* otx = stx.rprim->surface_ctx->otx;
                    if (stx.per_object_lighting_scenes && stx.per_object_lighting_scenes->size() > otx->index)
                    {
                        const rScene* lscene = (*stx.per_object_lighting_scenes)[otx->index];
                        assert(lscene);
                        //
                        Fsr::Pixel light_color(Mask_RGB);
                        const unsigned nLights = lscene->lights.size();
                        for (unsigned i=0; i < nLights; i++)
                        {
                            LightContext& ltx = *lscene->lights[i];

                            // Get vector from surface to light:
                            Vector3 Llt; float Dlt;
                            ltx.light()->get_L_vector(ltx, stx.PW, stx.N, Llt, Dlt);
                            Llt = -Llt; // flip light vector into same space as surface normal

                            // TODO: check shadowing here!
                            // Skip if no intersection:
                            if (Dlt == INFINITY)
                                continue;
                            else if (Dlt < 0.0f)
                                Dlt = INFINITY; // assume negative or zero distance is a dumb light

                            // Get light color:
                            ltx.light()->get_color(ltx, stx.PW, stx.N, Llt, Dlt, light_color);

                            // Apply Simplified Oren-Nayer diffuse function:
                            illum += RayShader::vec3_from_pixel(light_color) * oren_nayer_simplified(V, stx.N, Llt, k_diffuse_roughness*k_diffuse_roughness);
                        }
                    }
#endif

#else
                    // Initialize Light loop
                    AiLightsPrepare(sg);
                    while (AiLightsGetSample(sg))
                    {
                        if (AiLightGetAffectDiffuse(sg->Lp))
                        {
                            //AI_API Fsr::Vec3f   AiEvaluateLightSample(AtShaderGlobals* sg, const void* brdf_data, AtBRDFEvalSampleFunc eval_sample, AtBRDFEvalBrdfFunc eval_brdf, AtBRDFEvalPdfFunc eval_pdf);
                            illum += AiEvaluateLightSample(sg, OrenNayerData, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF);
                        }
                    }
#endif

                    illum *= diffDirect;
                    if (k_direct_diffuse_output[0] > Chan_Alpha) out[k_direct_diffuse_output[0]] = illum.x;
                    if (k_direct_diffuse_output[1] > Chan_Alpha) out[k_direct_diffuse_output[1]] = illum.y;
                    if (k_direct_diffuse_output[2] > Chan_Alpha) out[k_direct_diffuse_output[2]] = illum.z;
                    if (k_direct_diffuse_output[3] > Chan_Alpha) out[k_direct_diffuse_output[3]] = 1.0f;
                    //AiAOVSetRGB(sg, AiShaderEvalParamStr(p_aov_direct_diffuse), Dda);

                    out_color += illum;
                }
            }

            //------------------------------------------------------------------
            // Direct specular
            //
            if (m_specular_enabled)
            {
                Fsr::Vec3f specDirect = specColor*(Ks * k_direct_specular_factor)*reflect_vs_transmit_ratio;
                if (specDirect.notZero() && (stx.diffuse_depth == 0 || k_enable_glossy_caustics))
                {
                    Fsr::Vec3f illum(0,0,0); // Direct illumination accumulator
// TODO: finish the direct lighting! Need to test ray-traced light shadowing
#if 1
                    Fsr::Pixel& light_color = stx.thread_ctx->light_color;
                    light_color.setChannels(DD::Image::Mask_RGB);

                    const uint32_t nLights = (uint32_t)stx.master_light_shaders->size();
                    for (uint32_t i=0; i < nLights; ++i)
                    {
                        LightShader* lshader = (*stx.master_light_shaders)[i];
                        lshader->evaluateShading(stx, light_color);
                        illum.x += light_color[Chan_Red  ];
                        illum.y += light_color[Chan_Green];
                        illum.z += light_color[Chan_Blue ];
                        //std::cout << "  light_color" << light_color.color() << std::endl;
                        //std::cout << "  illum" << illum << std::endl;
                    }
#else
                    // Initialize Light loop
                    AiLightsPrepare(sg);
                    while (AiLightsGetSample(sg))
                    {
                        if (AiLightGetAffectSpecular(sg->Lp))
                        {
                            if (k_specular_model == SPEC_BRDF_WARD_DUER)
                                illum += AiEvaluateLightSample(sg, SpecBrdfData, AiWardDuerMISSample, AiWardDuerMISBRDF, AiWardDuerMISPDF);
                            else
                                illum += AiEvaluateLightSample(sg, SpecBrdfData, AiCookTorranceMISSample, AiCookTorranceMISBRDF, AiCookTorranceMISPDF);           
                        }
                    }
#endif

                    illum *= specDirect * reflect_vs_transmit_ratio;
                    if (k_direct_specular_output[0] > Chan_Alpha) out[k_direct_specular_output[0]] = illum.x;
                    if (k_direct_specular_output[1] > Chan_Alpha) out[k_direct_specular_output[1]] = illum.y;
                    if (k_direct_specular_output[2] > Chan_Alpha) out[k_direct_specular_output[2]] = illum.z;
                    if (k_direct_specular_output[3] > Chan_Alpha) out[k_direct_specular_output[3]] = 1.0f;
                    //AiAOVSetRGB(sg, AiShaderEvalParamStr(p_aov_direct_specular), illum);

                    out_color += illum;
                }
            }

        } // direct lighting


        //------------------------------------------------------------------
        // Indirect lighting
        //
        if (stx.indirect_lighting_enabled)
        {
            //------------------------------------------------------------------
            // Indirect diffuse
            if (m_diffuse_enabled)
            {
                Fsr::Vec3f diffColor_indirect = diffColor*(transWeight * k_indirect_diffuse_factor);
                if (diffColor_indirect.notZero())
                {
                    Fsr::Vec3f illum(0,0,0); // Indirect diffuse accumulator

                    //illum = AiOrenNayarIntegrate(&sg->Nf, sg, k_diffuse_roughness);
                    Fsr::Pixel indirect(Mask_RGB);
                    indirect.channels += m_indirect_diffuse_channels;
                    if (getIndirectDiffuse(stx, stx.Nf, k_diffuse_roughness, indirect))
                    {
                        illum = indirect.color()*diffColor_indirect;
                        // Copy AOVs:
                        //out.replace(indirect, ind_aovs);
                        if (k_indirect_diffuse_output[0] >  Chan_Alpha) out[k_indirect_diffuse_output[0]] = illum.x;
                        if (k_indirect_diffuse_output[1] >  Chan_Alpha) out[k_indirect_diffuse_output[1]] = illum.y;
                        if (k_indirect_diffuse_output[2] >  Chan_Alpha) out[k_indirect_diffuse_output[2]] = illum.z;
#if 1
                        if (k_indirect_diffuse_output[3] >  Chan_Alpha) out[k_indirect_diffuse_output[3]] = indirect[Chan_Alpha];
#else
                        if (k_indirect_diffuse_output[3] == Chan_Alpha) out_alpha = indirect[Chan_Alpha];
                        else out[k_indirect_diffuse_output[3]] = indirect[Chan_Alpha];
#endif

                        out_color += illum;
                    }
                }
            }


            //------------------------------------------------------------------
            // Indirect specular
            if (m_specular_enabled)
            {
                const bool k_enable_internal_reflections = false; // expose this to use later

                Fsr::Vec3f specIndirect = specColor*(Ks*k_indirect_specular_factor)*reflect_vs_transmit_ratio;
                if (specIndirect.notZero() &&
                    (stx.diffuse_depth == 0    || k_enable_glossy_caustics) &&
                    (stx.refraction_depth == 0 || k_enable_internal_reflections))
                {
                    Fsr::Vec3f illum(0,0,0); // Indirect specular accumulator

                    Fsr::Pixel indirect(Mask_RGB);
                    indirect.channels += m_indirect_specular_channels;
                    if (RayShader::getIndirectGlossy(stx, stx.Nf, spec_roughness_squared, indirect))
                    {
                        illum = indirect.color()*specIndirect;
                        // Copy AOVs:
                        //out.replace(indirect, ind_aovs);
                        if (k_indirect_specular_output[0] >  Chan_Alpha) out[k_indirect_specular_output[0]] = illum.x;
                        if (k_indirect_specular_output[1] >  Chan_Alpha) out[k_indirect_specular_output[1]] = illum.y;
                        if (k_indirect_specular_output[2] >  Chan_Alpha) out[k_indirect_specular_output[2]] = illum.z;
#if 1
                        if (k_indirect_specular_output[3] >  Chan_Alpha) out[k_indirect_specular_output[3]] = indirect[Chan_Alpha];
#else
                        if (k_indirect_specular_output[3] == Chan_Alpha) out_alpha = indirect[Chan_Alpha];
                        else out[k_indirect_specular_output[3]] = indirect[Chan_Alpha];
#endif

                        out_color += illum;
                    }
                }
            }


            //------------------------------------------------------------------
            // Indirect transmission
            if (m_transmission_enabled && reflect_vs_transmit_ratio < 1.0f)
            {
                Fsr::Vec3f transIndirect = transColor*k_transmission_factor*(1.0f - reflect_vs_transmit_ratio);
                if (transIndirect.notZero())
                {
                    Fsr::Vec3f illum(0,0,0); // Indirect transmission accumulator

                    const double eta = getRefractionRatio(V, stx.Nf, ior_from, ior_to);

                    Fsr::Pixel transmission(Mask_RGB);
                    transmission.channels += m_transmission_channels;
                    const double saved_ior = stx.index_of_refraction;
                    stx.index_of_refraction = ior_to;
                    const bool have_transmission =
                        RayShader::getTransmission(stx, stx.Nf, eta, spec_roughness_squared, transmission);
                    stx.index_of_refraction = saved_ior;

                    if (have_transmission)
                    {
                        illum = transmission.color()*transIndirect;

                        // Copy AOVs:
                        //out.replace(transmission, trans_aovs);
                        if (k_transmission_output[0] > Chan_Alpha) out[k_transmission_output[0]] = illum.x;
                        if (k_transmission_output[1] > Chan_Alpha) out[k_transmission_output[1]] = illum.y;
                        if (k_transmission_output[2] > Chan_Alpha) out[k_transmission_output[2]] = illum.z;
                        if (k_transmission_output[3] > Chan_Alpha) out[k_transmission_output[3]] = transmission[Chan_Alpha];

                        out_color += illum;
                    }
                    else
                    {
                        // Use tint color instead:
                        out_color += k_total_int_reflection_tint*transIndirect;
                    }
                }
            }

        } // indirect lighting


        // Modulate final color by opacity & opacity map:
        if (opacity >= 0.0001f)
        {
            out_color *= opacity;
            out_alpha *= opacity;
        }

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
