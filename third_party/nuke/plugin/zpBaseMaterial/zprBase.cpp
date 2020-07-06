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


#include "zprBase.h"

#include <zprender/ThreadContext.h>
#include <zprender/LightShader.h>


namespace zpr {


static RayShader* shaderBuilder() { return new zprBase(); }
/*static*/ const RayShader::ShaderDescription zprBase::description("zprBase", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprBase::input_defs =
{
    {InputKnob("bg",       PIXEL_KNOB)}, // BG0
};
/*static*/ const RayShader::OutputKnobList zprBase::output_defs =
{
    {OutputKnob("surface", PIXEL_KNOB )},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       DOUBLE_KNOB)},
    {OutputKnob("g",       DOUBLE_KNOB)},
    {OutputKnob("b",       DOUBLE_KNOB)},
    {OutputKnob("a",       DOUBLE_KNOB)},
};


//!
zprBase::InputParams::InputParams()
{
    k_diffuse_factor           = 1.0f;
    k_diffuse_tint             = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
    k_diffuse_roughness        = 0.0f;
    k_direct_diffuse_factor    = 1.0f;
    k_indirect_diffuse_factor  = 0.0f;
    k_opacity_factor           = 1.0f;
    //
    k_specular_factor          = 0.0f;
    k_specular_tint            = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
    k_specular_roughness       = 0.05f;
    k_direct_specular_factor   = 1.0f;
    k_indirect_specular_factor = 1.0f;
    k_fresnel_factor           = 0.0f;
    //
    k_transmission_factor      = 0.0f;
    k_index_of_refraction      = 1.0; // no refraction
    k_transmission_tint        = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
    k_total_int_reflection_tint = Fsr::Vec3f(0.0f, 0.0f, 0.0f);
    //
    k_emission_factor          = 0.0f;
    k_emission_tint            = Fsr::Vec3f(1.0f, 1.0f, 1.0f);
    //
    k_enable_glossy_caustics   = false;
    k_apply_fresnel_to_diffuse = false;

    for (unsigned i=0; i < 4; ++i)
    {
        k_direct_diffuse_output[i] = k_indirect_diffuse_output[i] = 
        k_direct_specular_output[i] = k_indirect_specular_output[i] = 
        k_transmission_output[i] =
        k_emission_output[i] = DD::Image::Chan_Black;//DD::Image::Channel(DD::Image::Chan_Red + i);
    }
}


/*!
*/
zprBase::zprBase() :
    RayShader()
{
    //setNumInputs(NUM_INPUTS);
}


/*!
*/
zprBase::zprBase(const InputParams& _inputs) :
    RayShader(),
    inputs(_inputs)
{
    //std::cout << "zprBase::ctor(" << this << ")" << std::endl;
    //setNumInputs(NUM_INPUTS);
}


/*static*/
void
zprBase::updateLocals(const InputParams& _inputs,
                      LocalVars&         _locals)
{
    _locals.m_opacity_factor = Fsr::clamp(_inputs.k_opacity_factor); // clamp 0..1
    _locals.m_fresnel_factor = Fsr::clamp(_inputs.k_fresnel_factor); // clamp 0..1

    _locals.m_diffuse_enabled      = (_inputs.k_direct_diffuse_factor  > 0.0 || _inputs.k_indirect_diffuse_factor  > 0.0);
    _locals.m_specular_enabled     = (_inputs.k_direct_specular_factor > 0.0 || _inputs.k_indirect_specular_factor > 0.0);
    _locals.m_transmission_enabled = (_inputs.k_transmission_factor > 0.0);
    _locals.m_emission_enabled     = (_inputs.k_emission_factor > 0.0);

    // Build output AOV channel sets:
    _locals.m_direct_diffuse_channels.clear();
    _locals.m_direct_diffuse_channels.insert(_inputs.k_direct_diffuse_output, 4);
    _locals.m_indirect_diffuse_channels.clear();
    _locals.m_indirect_diffuse_channels.insert(_inputs.k_indirect_diffuse_output, 4);
    _locals.m_direct_specular_channels.clear();
    _locals.m_direct_specular_channels.insert(_inputs.k_direct_specular_output, 4);
    _locals.m_indirect_specular_channels.clear();
    _locals.m_indirect_specular_channels.insert(_inputs.k_indirect_specular_output, 4);
    _locals.m_transmission_channels.clear();
    _locals.m_transmission_channels.insert(_inputs.k_transmission_output, 4);
    _locals.m_emission_channels.clear();
    _locals.m_emission_channels.insert(_inputs.k_emission_output, 4);

    // Build final output channel mask:
    _locals.m_aov_channels.clear();
    if (_locals.m_diffuse_enabled)
    {
        _locals.m_aov_channels += _locals.m_direct_diffuse_channels;
        _locals.m_aov_channels += _locals.m_indirect_diffuse_channels;
    }
    if (_locals.m_specular_enabled)
    {
        _locals.m_aov_channels += _locals.m_direct_specular_channels;
        _locals.m_aov_channels += _locals.m_indirect_specular_channels;
    }
    _locals.m_aov_channels += _locals.m_transmission_channels;
    _locals.m_aov_channels += _locals.m_emission_channels;
}


/*virtual*/
InputBinding*
zprBase::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*virtual*/
void
zprBase::validateShader(bool                 for_real,
                        const RenderContext& rtx)
{
    RayShader::validateShader(for_real, rtx); // < get the inputs

    updateLocals(inputs, locals);

    m_texture_channels  = DD::Image::Mask_None;
    for (uint32_t i=0; i < NUM_INPUTS; ++i)
        m_texture_channels += inputs.k_bindings[i].getChannels();

    m_output_channels  = DD::Image::Mask_RGBA;
    m_output_channels += m_texture_channels;
    m_output_channels += locals.m_aov_channels;
}


/*virtual*/
void
zprBase::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    RayShader::getActiveTextureBindings(texture_bindings); // < get the inputs

    for (uint32_t i=0; i < NUM_INPUTS; ++i)
        if (inputs.k_bindings[i].isActiveTexture())
            texture_bindings.push_back(&inputs.k_bindings[i]);
}


/*!
*/
/*virtual*/
void
zprBase::evaluateSurface(RayShaderContext& stx,
                         Fsr::Pixel&       out)
{
    //std::cout << "zprBase::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;
    // Let the background get shaded first.
    if (getInput(0))
        getInput(0)->evaluateSurface(stx, out);
    else
        out.rgba().set(0.0f, 0.0f, 0.0f, 1.0f);

    // TODO: make this a debug assert and move logic to RayShader base class call
    // Don't bother if it's a shadow ray:
    if (stx.Rtx.type_mask & Fsr::RayContext::SHADOW)
        return;

    // Always output RGBA:
    out.channels += DD::Image::Mask_RGBA;
    // Enable AOV output channels:
    out.channels += locals.m_aov_channels;

    Fsr::Vec3f& out_color = out.color();
    float&      out_alpha = out.opacity();
    out_alpha = 1.0f;

    float opacity;
    Fsr::Vec3f diffColor, specColor, transColor;

    //------------------------------------------------------------------
    // Get base colors from bindings or direct knobs:
    //
    if (inputs.k_bindings[OPACITY4].isActiveColor() && locals.m_opacity_factor >= 0.0001f)
    {
        const Fsr::Vec3f op = inputs.k_bindings[OPACITY4].getValue(stx);
        opacity = op.x * locals.m_opacity_factor;
    }
    else
        opacity = locals.m_opacity_factor;

    if (locals.m_emission_enabled)
    {
        if (inputs.k_bindings[EMISSION3].isActiveColor() && inputs.k_emission_factor > 0.0001f)
            out_color = inputs.k_bindings[EMISSION3].getValue(stx)*inputs.k_emission_tint;
        else
            out_color = inputs.k_emission_tint;
        out_color *= inputs.k_emission_factor;
    }
    else
        out_color.set(0.0f, 0.0f, 0.0f);

    if (locals.m_diffuse_enabled)
    {
        if (inputs.k_bindings[DIFFUSE1].isActiveColor() && inputs.k_diffuse_factor > 0.0001f)
            diffColor = inputs.k_bindings[DIFFUSE1].getValue(stx, &out_alpha)*inputs.k_diffuse_tint;
        else
            diffColor = inputs.k_diffuse_tint;
        diffColor *= inputs.k_diffuse_factor;
    }

    //------------------------------------------------------------------
    // If no lighting enabled switch to direct diffuse mapping + emission:
    //
    if (!stx.direct_lighting_enabled && !stx.indirect_lighting_enabled)
    {
        if (locals.m_diffuse_enabled)
            out_color += diffColor;

        // Modulate final color by opacity & opacity map:
        out_color *= opacity;
        out_alpha *= opacity;
        return;
    }


    //------------------------------------------------------------------
    // Get lighting colors from bindings or direct knobs:
    //
    if (locals.m_specular_enabled)
    {
        if (inputs.k_bindings[SPECULAR2].isActiveColor())
            specColor = inputs.k_bindings[SPECULAR2].getValue(stx)*inputs.k_specular_tint;
        else
            specColor = inputs.k_specular_tint;
    }

    if (locals.m_transmission_enabled)
    {
        //if (inputs.k_bindings[TRANSMISSION].isActiveColor())
        //    transColor = inputs.k_bindings[TRANSMISSION].getValue(stx)*inputs.k_transmission_tint;
        //else
            transColor = inputs.k_transmission_tint;
    }


    //------------------------------------------------------------------
    //
    float Ks                     = 1.0f;
    float spec_roughness         = 0.0f;
    float spec_roughness_squared = 0.0f;
    if (locals.m_specular_enabled || locals.m_transmission_enabled)
    {
        Ks                     = inputs.k_specular_factor;
        spec_roughness         = clamp(inputs.k_specular_roughness);
        spec_roughness_squared = spec_roughness*spec_roughness;
    }


    const Fsr::Vec3d V = -stx.Rtx.dir();
    // Fresnel-weighted reflectance and transmission weight
    // transWeight + fresnel_factor <= 1.0
    //
    // If k_apply_fresnel_to_diffuse true, transWeight = 1-fresnel_factor
    Fsr::Vec3f transWeight(1,1,1);
    if (locals.m_fresnel_factor > 0.0)
    {
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
        specColor = specColor*(1.0f - locals.m_fresnel_factor) + specColorFresnel*locals.m_fresnel_factor;

        // If you want the transmssion and reflectance to sum to 1:
        if (inputs.k_apply_fresnel_to_diffuse)
            transWeight = Fsr::Vec3f(1,1,1) - specColor;
    }


    //------------------------------------------------------------------
    // Get ratio of reflection vs. refraction:
    //
    double ior_from = stx.index_of_refraction;
    // Default to air - this should be handled by illumination() call...:
    if (ior_from < 1.0)
        ior_from = 1.0;
    double ior_to = ::fabs(inputs.k_index_of_refraction);
    if (ior_to < 1.0)
        ior_to = 1.0;

    // Are we inside the object still?
    if (::fabs(ior_from - ior_to) < std::numeric_limits<double>::epsilon())
    {
        // 
    }
    //std::cout << "ior_from=" << ior_from << " ior_to=" << ior_to;

    float reflect_vs_transmit_ratio = 1.0f; // all reflection default
    if (locals.m_specular_enabled || locals.m_transmission_enabled)
    {
        //std::cout << " reflect_vs_transmit_ratio=" << reflect_vs_transmit_ratio;
        //std::cout << " refraction_ratio=" << refraction_ratio(V, stx.Nf, ior_from, ior_to);
        //std::cout << std::endl;

        reflect_vs_transmit_ratio = reflectionRatioSnellSchlick(V,
                                                                stx.Nf,
                                                                ior_from,
                                                                ior_to,
                                                                double(1.0f + locals.m_fresnel_factor*4.0f));
    }


    //------------------------------------------------------------------
    // Direct lighting
    //
    if (stx.direct_lighting_enabled && stx.master_light_shaders)
    {
        //------------------------------------------------------------------
        // Direct diffuse
        //
        if (locals.m_diffuse_enabled)
        {
            Fsr::Vec3f diffDirect = (diffColor*inputs.k_direct_diffuse_factor) * transWeight;
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
                    lshader->evaluateSurface(stx, light_color);
                    illum.x += light_color[DD::Image::Chan_Red  ];
                    illum.y += light_color[DD::Image::Chan_Green];
                    illum.z += light_color[DD::Image::Chan_Blue ];
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
                    Fsr::Pixel light_color(DD::Image::Mask_RGB);
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
                        illum += RayShader::vec3_from_pixel(light_color) * oren_nayer_simplified(V, stx.N, Llt, inputs.k_diffuse_roughness*inputs.k_diffuse_roughness);
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
                if (inputs.k_direct_diffuse_output[0] > DD::Image::Chan_Alpha) out[inputs.k_direct_diffuse_output[0]] = illum.x;
                if (inputs.k_direct_diffuse_output[1] > DD::Image::Chan_Alpha) out[inputs.k_direct_diffuse_output[1]] = illum.y;
                if (inputs.k_direct_diffuse_output[2] > DD::Image::Chan_Alpha) out[inputs.k_direct_diffuse_output[2]] = illum.z;
                if (inputs.k_direct_diffuse_output[3] > DD::Image::Chan_Alpha) out[inputs.k_direct_diffuse_output[3]] = 1.0f;
                //AiAOVSetRGB(sg, AiShaderEvalParamStr(p_aov_direct_diffuse), Dda);

                out_color += illum;
            }
        }

        //------------------------------------------------------------------
        // Direct specular
        //
        if (locals.m_specular_enabled)
        {
            Fsr::Vec3f specDirect = specColor*(Ks * inputs.k_direct_specular_factor)*reflect_vs_transmit_ratio;
            if (specDirect.notZero() && (stx.diffuse_depth == 0 || inputs.k_enable_glossy_caustics))
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
                    lshader->evaluateSurface(stx, light_color);
                    illum.x += light_color[DD::Image::Chan_Red  ];
                    illum.y += light_color[DD::Image::Chan_Green];
                    illum.z += light_color[DD::Image::Chan_Blue ];
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
                        if (inputs.k_specular_model == SPEC_BRDF_WARD_DUER)
                            illum += AiEvaluateLightSample(sg, SpecBrdfData, AiWardDuerMISSample, AiWardDuerMISBRDF, AiWardDuerMISPDF);
                        else
                            illum += AiEvaluateLightSample(sg, SpecBrdfData, AiCookTorranceMISSample, AiCookTorranceMISBRDF, AiCookTorranceMISPDF);           
                    }
                }
#endif

                illum *= specDirect * reflect_vs_transmit_ratio;
                if (inputs.k_direct_specular_output[0] > DD::Image::Chan_Alpha) out[inputs.k_direct_specular_output[0]] = illum.x;
                if (inputs.k_direct_specular_output[1] > DD::Image::Chan_Alpha) out[inputs.k_direct_specular_output[1]] = illum.y;
                if (inputs.k_direct_specular_output[2] > DD::Image::Chan_Alpha) out[inputs.k_direct_specular_output[2]] = illum.z;
                if (inputs.k_direct_specular_output[3] > DD::Image::Chan_Alpha) out[inputs.k_direct_specular_output[3]] = 1.0f;
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
        if (locals.m_diffuse_enabled)
        {
            Fsr::Vec3f diffColor_indirect = diffColor*(transWeight * inputs.k_indirect_diffuse_factor);
            if (diffColor_indirect.notZero())
            {
                Fsr::Vec3f illum(0,0,0); // Indirect diffuse accumulator

                //illum = AiOrenNayarIntegrate(&sg->Nf, sg, inputs.k_diffuse_roughness);
                Fsr::Pixel indirect(DD::Image::Mask_RGB);
                indirect.channels += locals.m_indirect_diffuse_channels;
                if (getIndirectDiffuse(stx, stx.Nf, inputs.k_diffuse_roughness, indirect))
                {
                    illum = indirect.color()*diffColor_indirect;
                    // Copy AOVs:
                    //out.replace(indirect, ind_aovs);
                    if (inputs.k_indirect_diffuse_output[0] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_diffuse_output[0]] = illum.x;
                    if (inputs.k_indirect_diffuse_output[1] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_diffuse_output[1]] = illum.y;
                    if (inputs.k_indirect_diffuse_output[2] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_diffuse_output[2]] = illum.z;
#if 1
                    if (inputs.k_indirect_diffuse_output[3] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_diffuse_output[3]] = indirect[DD::Image::Chan_Alpha];
#else
                    if (inputs.k_indirect_diffuse_output[3] == DD::Image::Chan_Alpha) out_alpha = indirect[DD::Image::Chan_Alpha];
                    else out[inputs.k_indirect_diffuse_output[3]] = indirect[DD::Image::Chan_Alpha];
#endif

                    out_color += illum;
                }
            }
        }


        //------------------------------------------------------------------
        // Indirect specular
        if (locals.m_specular_enabled)
        {
            const bool k_enable_internal_reflections = false; // expose this to use later

            Fsr::Vec3f specIndirect = specColor*(Ks*inputs.k_indirect_specular_factor)*reflect_vs_transmit_ratio;
            if (specIndirect.notZero() &&
                (stx.diffuse_depth == 0    || inputs.k_enable_glossy_caustics) &&
                (stx.refraction_depth == 0 || k_enable_internal_reflections))
            {
                Fsr::Vec3f illum(0,0,0); // Indirect specular accumulator

                Fsr::Pixel indirect(DD::Image::Mask_RGB);
                indirect.channels += locals.m_indirect_specular_channels;
                if (RayShader::getIndirectGlossy(stx, stx.Nf, spec_roughness_squared, indirect))
                {
                    illum = indirect.color()*specIndirect;
                    // Copy AOVs:
                    //out.replace(indirect, ind_aovs);
                    if (inputs.k_indirect_specular_output[0] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_specular_output[0]] = illum.x;
                    if (inputs.k_indirect_specular_output[1] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_specular_output[1]] = illum.y;
                    if (inputs.k_indirect_specular_output[2] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_specular_output[2]] = illum.z;
#if 1
                    if (inputs.k_indirect_specular_output[3] >  DD::Image::Chan_Alpha) out[inputs.k_indirect_specular_output[3]] = indirect[DD::Image::Chan_Alpha];
#else
                    if (inputs.k_indirect_specular_output[3] == DD::Image::Chan_Alpha) out_alpha = indirect[DD::Image::Chan_Alpha];
                    else out[inputs.k_indirect_specular_output[3]] = indirect[Chan_Alpha];
#endif

                    out_color += illum;
                }
            }
        }


        //------------------------------------------------------------------
        // Indirect transmission
        if (locals.m_transmission_enabled && reflect_vs_transmit_ratio < 1.0f)
        {
            Fsr::Vec3f transIndirect = transColor*inputs.k_transmission_factor*(1.0f - reflect_vs_transmit_ratio);
            if (transIndirect.notZero())
            {
                Fsr::Vec3f illum(0,0,0); // Indirect transmission accumulator

                const double eta = getRefractionRatio(V, stx.Nf, ior_from, ior_to);

                Fsr::Pixel transmission(DD::Image::Mask_RGB);
                transmission.channels += locals.m_transmission_channels;
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
                    if (inputs.k_transmission_output[0] > DD::Image::Chan_Alpha) out[inputs.k_transmission_output[0]] = illum.x;
                    if (inputs.k_transmission_output[1] > DD::Image::Chan_Alpha) out[inputs.k_transmission_output[1]] = illum.y;
                    if (inputs.k_transmission_output[2] > DD::Image::Chan_Alpha) out[inputs.k_transmission_output[2]] = illum.z;
                    if (inputs.k_transmission_output[3] > DD::Image::Chan_Alpha) out[inputs.k_transmission_output[3]] = transmission[DD::Image::Chan_Alpha];

                    out_color += illum;
                }
                else
                {
                    // Use tint color instead:
                    out_color += inputs.k_total_int_reflection_tint*transIndirect;
                }
            }
        }

    } // indirect lighting


    // Modulate final color by opacity & opacity map:
    out_color *= opacity;
    out_alpha *= opacity;

}


} // namespace zpr

// end of zprBase.cpp

//
// Copyright 2020 DreamWorks Animation
//
