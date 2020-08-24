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

/// @file zprPreviewSurface.cpp
///
/// @author Jonathan Egstad


#include "zprPreviewSurface.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "LightShader.h"

/*  UsdPreviewSurface

    https://graphics.pixar.com/usd/docs/UsdPreviewSurface-Proposal.html

    Inputs:
        diffuseColor - color3f - (0.18, 0.18, 0.18)
            When using metallic workflow this is interpreted as albedo.
        emissiveColor - color3f - (0.0, 0.0, 0.0) 
            Emissive component. 
        useSpecularWorkflow - int - 0 
            This node can fundamentally operate in two modes : Specular workflow where you provide a
            texture/value to the "specularColor" input. Or, Metallic workflow where you provide a
            texture/value to the "metallic" input.  Depending on the 0 or 1 value of this parameter,
            the following parameters are conditionally enabled:

            useSpecularWorkflow = 1: (Specular workflow ) 
            specularColor - color3f - (0.0, 0.0, 0.0)
                Specular color to be used. This is the color at 0 incidence. Edge color is assumed white.
                Transition between the two colors according to Schlick fresnel approximation.
            useSpecularWorkflow = 0:   (Metalness workflow ) 
            metallic - float - 0.0
                Use 1 for metallic surfaces and 0 for non-metallic.
                - If metallic is 1.0, then F0 (reflectivity at 0 degree incidence) will be derived from
                ior ( (1-ior)/(1+ior) )^2, then multiplied by Albedo; while edge F90 reflectivity will
                simply be the Albedo.
                (As an option, you can set ior to 0 such that F0 becomes equal to F90 and thus the Albedo).
                - If metallic is 0.0, then Albedo is ignored; F0 is derived from ior and F90 is white.
                In between, we interpolate.
        roughness - float - 0.5 
            Roughness for the specular lobe. The value ranges from 0 to 1, which goes from a perfectly
            specular surface at 0.0 to maximum roughness of the specular lobe. This value is usually
            squared before use with a GGX or Beckmann lobe.
        clearcoat - float - 0.0 
            Second specular lobe amount. The color is white.
        clearcoatRoughness - float - 0.01 
            Roughness for the second specular lobe.
        opacity - float - 1.0 
            When opacity is 1.0 then the gprim is fully opaque, if it is smaller than 1.0 then the prim
            is translucent, when it is 0 the gprim is transparent.  Note that even a fully transparent
            object still receives lighting as, for example, perfectly clear glass still has a specular
            response.
        opacityThreshold - float - 0.0
            The opacityThreshold input is useful for creating geometric cut-outs based on the opacity
            input. A value of 0.0 indicates that no masking is applied to the opacity input, while a
            value greater than 0.0 indicates that rendering of the surface is limited to the areas where
            the opacity is greater than that value. A classic use of opacityThreshold is to create a
            leaf from an opacity input texture, in that case the threshold determines the parts of the
            opacity texture that will be fully transparent and not receive lighting. Note that when
            opacityThreshold is greater than zero, then opacity modulates the presence of the surface,
            rather than its transparency - pathtracers might implement this as allowing
            ((1 - opacity) * 100) % of the rays that do intersect the object to instead pass through
            it unhindered, and rasterizers may interpret opacity as pixel coverage.  Thus,
            opacityThreshold serves as a switch for how the opacity input is interpreted; this
            "translucent or masked" behavior is common in engines and renderers, and makes the
            UsdPreviewSurface easier to interchange.  It does imply, however, that it is not possible
            to faithfully recreate a glassy/translucent material that also provides an opacity-based
            mask... so no single-polygon glass leaves.
        ior - float - 1.5 
            Index of Refraction to be used for translucent objects.
        normal - normal3f - (0.0, 0.0, 1.0) 
            Expects normal in tangent space [(-1,-1,-1), (1,1,1)] This means your texture reader
            implementation should provide data to this node that is properly scaled and ready to be
            consumed as a tangent space normal.
        displacement - float - 0.0 
            Displacement in the direction of the normal.
        occlusion - float - 1.0 
            Extra information about the occlusion of different parts of the mesh that this material
            is applied to.  Occlusion only makes sense as a surface-varying signal, and pathtracers
            will likely choose to ignore it.  An occlusion value of 0.0 means the surface point is
            fully occluded by other parts of the surface, and a value of 1.0 means the surface point
            is completely unoccluded by other parts of the surface. 

    Outputs:
        surface - token
        displacement - token
*/


namespace zpr {


static RayShader* shaderBuilder() { return new zprPreviewSurface(); }
/*static*/ const RayShader::ShaderDescription zprPreviewSurface::description("PreviewSurface", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprPreviewSurface::input_defs =
{
    {InputKnob("diffuseColor",        COLOR3_KNOB, "0.18 0.18 0.18")},
    {InputKnob("emissiveColor",       COLOR3_KNOB, "0 0 0"         )},
    {InputKnob("useSpecularWorkflow", INT_KNOB,    "0"             )},
    {InputKnob("specularColor",       COLOR3_KNOB, "0 0 0"         )},
    {InputKnob("metallic",            FLOAT_KNOB,  "0"             )},
    {InputKnob("roughness",           FLOAT_KNOB,  "0.5"           )},
    {InputKnob("clearcoat",           FLOAT_KNOB,  "0"             )},
    {InputKnob("clearcoatRoughness",  FLOAT_KNOB,  "0.01"          )},
    {InputKnob("opacity",             FLOAT_KNOB,  "1"             )},
    {InputKnob("opacityThreshold",    FLOAT_KNOB,  "0"             )},
    {InputKnob("ior",                 FLOAT_KNOB,  "1.5"           )},
    {InputKnob("normal",              COLOR3_KNOB, "0 0 1"         )},
    {InputKnob("displacement",        FLOAT_KNOB,  "0"             )},
    {InputKnob("occlusion",           FLOAT_KNOB,  "1"             )},
};
/*static*/ const RayShader::OutputKnobList zprPreviewSurface::output_defs =
{
    {OutputKnob("surface",            STRING_KNOB)},
    {OutputKnob("displacement",       STRING_KNOB)},
};


/*!
*/
zprPreviewSurface::zprPreviewSurface() :
    RayShader(input_defs, output_defs)
{
    //std::cout << "zprPreviewSurface::ctor(" << this << ")" << std::endl;

    // Point the knobs to their values:
    assert(m_inputs.size() == 14 && m_inputs.size() == input_defs.size());
    assignInputKnob("diffuseColor",        &k_diffuseColor);
    assignInputKnob("emissiveColor",       &k_emissiveColor);
    assignInputKnob("useSpecularWorkflow", &k_useSpecularWorkflow);
    assignInputKnob("specularColor",       &k_specularColor);
    assignInputKnob("metallic",            &k_metallic);
    assignInputKnob("roughness",           &k_roughness);
    assignInputKnob("clearcoat",           &k_clearcoat);
    assignInputKnob("clearcoatRoughness",  &k_clearcoatRoughness);
    assignInputKnob("opacity",             &k_opacity);
    assignInputKnob("opacityThreshold",    &k_opacityThreshold);
    assignInputKnob("ior",                 &k_ior);
    assignInputKnob("normal",              &k_normal);
    assignInputKnob("displacement",        &k_displacement);
    assignInputKnob("occlusion",           &k_occlusion);
}


/*!
*/
zprPreviewSurface::~zprPreviewSurface()
{
}


/*!
*/
/*virtual*/
void
zprPreviewSurface::validateShader(bool                 for_real,
                                  const RenderContext& rtx)
{
    //std::cout << "zprPreviewSurface::validateShader()" << std::endl;
    RayShader::validateShader(for_real, rtx);

m_texture_channels = DD::Image::Mask_RGB; // this should be max of all input channels

    //m_output_channels = m_texture_channels;
    m_output_channels = DD::Image::Mask_RGBA;
}


/*! TODO: finish this
*/
/*virtual*/
void
zprPreviewSurface::evaluateSurface(RayShaderContext& stx,
                                   Fsr::Pixel&       out)
{
    //std::cout << "zprPreviewSurface::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;
    Fsr::Pixel& tex = stx.thread_ctx->binding_color;
    tex.setChannels(m_texture_channels);

    Fsr::Vec3f diffuseColor(k_diffuseColor);
    if (getInputShader(0))
    {
        getInputShader(0)->evaluateSurface(stx, tex);
        diffuseColor = tex.rgb();
    }

    Fsr::Vec3f emissiveColor(k_emissiveColor);
    if (getInputShader(1))
    {
        getInputShader(1)->evaluateSurface(stx, tex);
        emissiveColor = tex.rgb();
    }

    float specularAmount = 1.0f;//k_;

    Fsr::Vec3f specularColor(k_specularColor);
    if (getInputShader(3))
    {
        getInputShader(3)->evaluateSurface(stx, tex);
        specularColor = tex.rgb();
    }

    float specularRoughness = k_roughness;
    if (getInputShader(5))
    {
        getInputShader(5)->evaluateSurface(stx, tex);
        specularRoughness = tex.r();
    }

    Fsr::Vec3f normal(k_normal);
    if (getInputShader(11))
    {
        getInputShader(11)->evaluateSurface(stx, tex);
        normal = tex.rgb();
    }

    float occlusion = k_occlusion;
    if (getInputShader(13))
    {
        getInputShader(13)->evaluateSurface(stx, tex);
        occlusion = tex.r();
    }

    // Evaluate all lights.
    Fsr::Vec3f illum(0.0f);
    if (stx.master_light_shaders)
        illum = evaluateLights(stx,
                               diffuseColor,
                               false/*useSpecularWorkflow*/,
                               1.0f/*ior*/,
                               1.0f/*metallic*/,
                               specularAmount,
                               specularColor,
                               specularRoughness/*specularRoughness*/,
                               1.0f/*clearcoatAmount*/,
                               Fsr::Vec3f(0.0f)/*clearcoatColor*/,
                               1.0f/*clearcoatRoughness*/,
                               occlusion);

    out.rgb()   = illum + emissiveColor;
    out.alpha() = k_opacity;
}


/*!
*/
Fsr::Vec3f
zprPreviewSurface::evaluateLights(RayShaderContext& stx,
                                  const Fsr::Vec3f& diffuseColor,
                                  bool              useSpecularWorkflow,
                                  float             ior,
                                  float             metallic,
                                  float             specularAmount,
                                  const Fsr::Vec3f& specularColor,
                                  float             specularRoughness,
                                  float             clearcoatAmount,
                                  const Fsr::Vec3f& clearcoatColor,
                                  float             clearcoatRoughness,
                                  float             occlusion) const
{
    const Fsr::Vec3d V = stx.getViewVector(); // this may build a fake-stereo view-vector
#if 0
    Fsr::Vec3d Rrefl = V.reflect(stx.N); Rrefl.normalize();

    RayShaderContext Rrefl_stx(stx,
                               Rrefl,
                               std::numeric_limits<double>::epsilon(),
                               std::numeric_limits<double>::infinity(),
                               Fsr::RayContext::GLOSSY | Fsr::RayContext::REFLECTION/*ray_type*/,
                               RenderContext::SIDES_BOTH/*sides_mode*/);
#endif

    Fsr::Vec3f directLight(0.0f);
    Fsr::Vec3f indirectLight(0.0f);

#if 1
    // TODO: finish the direct lighting! Need to test ray-traced light shadowing
    Fsr::Pixel& light_color = stx.thread_ctx->light_color;
    //light_color.setChannels(DD::Image::Mask_RGB);

    const uint32_t nLights = (uint32_t)stx.master_light_shaders->size();
    for (uint32_t i=0; i < nLights; ++i)
    {
        LightShader* lshader = (*stx.master_light_shaders)[i];
        if (!lshader)
            continue;

        Fsr::RayContext Rlight; // ray from surface to light, for shadowing, etc.
        float direct_pdfW;
        if (!lshader->illuminateSurface(stx, Rlight, direct_pdfW, light_color))
            continue; // not affecting this surface

        // Get shadowing factor for light (0=shadowed, 1=no shadow):
        //float shadow = 1.0f;
        RayShaderContext Rshadow_stx(stx,
                                     Rlight,
                                     Fsr::RayContext::SHADOW/*ray_type*/,
                                     RenderContext::SIDES_BOTH/*sides_mode*/);
        Traceable::SurfaceIntersection Ishadow(std::numeric_limits<double>::infinity());
        if (stx.rtx->objects_bvh.getFirstIntersection(Rshadow_stx, Ishadow) > Fsr::RAY_INTERSECT_NONE &&
             Ishadow.t < Rlight.maxdist)
        {
#if 1
            continue;
#else
            //std::cout << "D=" << D << ", t=" << Ishadow.t << std::endl;
            // Shadowed - make it fall off the farther the occluder is from surface(hack!!!):
            shadow = (Ishadow.t / D);//powf(float(1.0 - (Ishadow.t / D)), 2.0f);
            if (shadow <= 0.0f)
            {
                continue;
            }
#endif
        }

        const Fsr::Vec3d& L = Rlight.dir();

        // Naive diffuse lobe (lambert):
        const float N_dot_L = float(stx.Nf.dot(L));
        if (N_dot_L < 0.0f)
            continue; // surface facing away from light

        //Fsr::Vec3d H = (V + L); H.normalize(); // half-vector between view and light vectors
        //const float V_dot_H = float(stx.Nf.dot(H));

        Fsr::Vec3d Lrefl = L.reflect(stx.N); Lrefl.normalize();
        float Lrefl_dot_V = float(Lrefl.dot(V));

        //const float fresnel = powf(std::max(0.0f, 1.0f - V_dot_H), 5.0f);//SchlickFresnel(V_dot_H);

        // Evaluate diffuse
        //Fsr::Vec3f diffuseContribution = diffuseColor*float(1.0 / M_PI);//evaluateDirectDiffuse();
        Fsr::Vec3f diffuseContribution = diffuseColor*light_color.rgb()*N_dot_L;

#if 1
        // Naive specular lobe (phong):
        if (Lrefl_dot_V <= 0.0 || Lrefl_dot_V >= M_PI_2)
            Lrefl_dot_V = 0.0f;

        // This is utter junk...:
        const float spec_wt = float(pow(Lrefl_dot_V, (1.0/specularRoughness)*10.0));
        Fsr::Vec3f specularContribution = specularColor*light_color.rgb()*spec_wt;
        //Fsr::Vec3f specularContribution;
        //specularContribution.set(specularRoughness);//float(pow(Lrefl_dot_V, 1.0/0.01)));

#else
        // Evaluate specular first lobe
        Fsr::Vec3f specularContribution(0.0f);
        if (specularAmount > 0.0f)
        {
            Fsr::Vec3f F0 = specularColor;
            Fsr::Vec3f F90(1.0f);

            if (!useSpecularWorkflow)
            {
                const float R = (1.0f - ior) / (1.0f + ior);
                Fsr::Vec3f specColor = Fsr::Vec3f(1.0f).lerpTo(diffuseColor, metallic);
                F0  = specColor*(R * R);
                F90 = specColor;

                // For metallic workflows, pure metals have no diffuse
                d *= (1.0f - metallic);
            }

            specularContribution = specularAmount * evaluateDirectSpecular(F0,                // Specular color 0
                                                                           F90,               // Specular color 90
                                                                           specularRoughness, // Roughness
                                                                           fresnel,           // Fresnel
                                                                           NdotL,
                                                                           NdotE,
                                                                           NdotH,
                                                                           EdotH); // Dot products needed for lights
            //{ evaluateDirectSpecular():
            //    Fsr::Vec3f F = mix(specularColorF0, specularColorF90, fresnel);
            //    float D = NormalDistribution(specularRoughness, NdotH);
                //{ NormalDistribution():
                //    float alpha = specularRoughness * specularRoughness;
                //    float alpha2 = alpha * alpha;
                //    float NdotH2 = NdotH * NdotH;
                //    float DDenom = (NdotH2 * (alpha2 - 1.0)) + 1.0;
                //    DDenom *= DDenom;
                //    DDenom *= PI;
                //    float D = (alpha2 + EPSILON) / DDenom;
                //    return D;
                //}

            //    float G = Geometric(specularRoughness, NdotL, NdotE, NdotH, EdotH);
                //{  Geometric():
                //    float alpha = specularRoughness * specularRoughness;
                //    float k = alpha * 0.5;
                //    float G = NdotE / (NdotE * (1.0 - k) + k);
                //    G *= NdotL / (NdotL * (1.0 - k) + k);
                //    return G;
                //}

            //    Fsr::Vec3f RNum = F * G * D;
            //    float RDenom = 4.0f * NdotL * NdotE + EPSILON;
            //    return RNum / RDenom;
            //}

            // Adjust the diffuse so glazing angles have less diffuse
            diffuseContribution *= (1.0f - Fsr::lerp(F0, F90, fresnel));
        }
#endif

        directLight += diffuseContribution + specularContribution;
    }



#else
    for (int i=0; i < NUM_LIGHTS; ++i)
    {

        // Calculate necessary vector information for lighting
        Fsr::Vec4f Plight = (lightSource[i].isIndirectLight) ? Fsr::Vec4f(0,0,0,1) : lightSource[i].position;
        Fsr::Vec3f l = (Plight.w == 0.0) ? normalize(Plight.xyz) : normalize(Plight - Peye).xyz;
        Fsr::Vec3f h = normalize(e + l);
        float NdotL = max(0.0, dot(n, l));
        float NdotH = max(0.0, dot(n, h));
        float EdotH = max(0.0, dot(e, h));

        // Calculate light intesity
        float atten = lightDistanceAttenuation(Peye, i);
        float spot = lightSpotAttenuation(l, i);

        // Calculate the shadow factor
        float shadow = 1.0;
#if USE_SHADOWS
        shadow = (lightSource[i].hasShadow) ? shadowing(/*lightIndex=*/i, Peye) : 1.0;
#endif

        float intensity = atten * spot * shadow;

        Fsr::Vec3f lightDiffuseIrradiance = intensity * lightSource[i].diffuse.rgb;
        Fsr::Vec3f lightSpecularIrradiance = intensity * lightSource[i].specular.rgb;

        LightingContributions lightingContrib = evaluateLight(
            diffuseColor,
            useSpecularWorkflow,
            ior,
            metallic,
            specularAmount,
            specularColor,
            specularRoughness,
            clearcoatAmount,
            clearcoatColor,
            clearcoatRoughness,
            occlusion,
            NdotL,
            NdotE,
            NdotH,
            EdotH,
            lightDiffuseIrradiance,
            lightSpecularIrradiance);

        // calculate the indirect light (DomeLight)
        if (lightSource[i].isIndirectLight)
        {

            indirectLight = evaluateIndirectLighting(diffuseColor,
                                    specularColor, Neye, Reye, NdotE,
                                    EdotH, ior, metallic, occlusion,
                                    specularRoughness, useSpecularWorkflow,
                                    lightSource[i].worldToLightTransform);
        }
        // all other light sources contribute to the direct lighting
        else
        {
            directLight += (lightingContrib.diffuse + lightingContrib.specular);
        }
    }
#endif

    return (directLight + indirectLight);
}


} // namespace zpr

// end of zprPreviewSurface.cpp

//
// Copyright 2020 DreamWorks Animation
//


#if 0
-- configuration
{
    "techniques": {
        "default": {
            "displacementShader": {
                "source": [ "Preview.Displacement" ]
            },
            "surfaceShader": {
                "source": [ "Preview.LightStructures",
                            "Preview.Lighting",
                            "Preview.LightIntegration",
                            "Preview.Surface" ]
            }
        }
    }
}


-- glsl Preview.Displacement

vec4
displacementShader(int index, vec4 Peye, vec3 Neye, vec4 patchCoord)
{
    // Calculate scalar displacement.
    float texDisplacement = HdGet_displacement(index).x;
    return Peye + vec4(Neye*texDisplacement, 0);
}


-- glsl Preview.Surface

vec4
surfaceShader(vec4 Peye, vec3 Neye, vec4 color, vec4 patchCoord)
{
    float clearcoatAmount    = HdGet_clearcoat().x;
    float clearcoatRoughness = HdGet_clearcoatRoughness().x;
    vec3 diffuseColor        = HdGet_diffuseColor().xyz;
    vec3 emissiveColor       = HdGet_emissiveColor().xyz;
    float ior                = HdGet_ior().x;
    float metallic           = HdGet_metallic().x;
    float occlusion          = HdGet_occlusion().x;
    float opacity            = HdGet_opacity().x;
    float roughness          = HdGet_roughness().x;
    vec3 specularColor       = HdGet_specularColor().xyz;
    bool useSpecularWorkflow = (HdGet_useSpecularWorkflow().x == 1);

    vec3 clearcoatColor      = vec3(1.0);
    float specularAmount     = 1.0;

    // Selection highlighting.
    vec4 colorAndOpacity = vec4(diffuseColor, opacity);
    diffuseColor = ApplyColorOverrides(colorAndOpacity).rgb;

    // Evaluate all lights.
    vec3 c = evaluateLights(
        emissiveColor,
        diffuseColor,
        useSpecularWorkflow,
        ior,
        metallic,
        specularAmount,
        specularColor,
        roughness,
        clearcoatAmount,
        clearcoatColor,
        clearcoatRoughness,
        occlusion,
        Peye,
        Neye);

    return vec4(c, colorAndOpacity.a);
}


-- glsl Preview.LightStructures

struct LightingContributions
{
    vec3 diffuse;
    vec3 specular;
};


-- glsl Preview.Lighting

#define PI 3.1415
#define EPSILON 0.001


float
SchlickFresnel(float EdotH)
{
    return pow(max(0.0, 1.0 - EdotH), 5.0);
}

float
NormalDistribution(float specularRoughness, float NdotH)
{
    float alpha = specularRoughness * specularRoughness;
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH * NdotH;
    float DDenom = (NdotH2 * (alpha2 - 1.0)) + 1.0;
    DDenom *= DDenom;
    DDenom *= PI;
    float D = (alpha2 + EPSILON) / DDenom;
    return D;
}

float
Geometric(
        float specularRoughness,
        float NdotL,
        float NdotE,
        float NdotH,
        float EdotH)
{
    float alpha = specularRoughness * specularRoughness;
    float k = alpha * 0.5;
    float G = NdotE / (NdotE * (1.0 - k) + k);
    G *= NdotL / (NdotL * (1.0 - k) + k);
    return G;
}

vec3
evaluateDirectDiffuse()
{
    return vec3(1.0 / PI);
}

vec3
evaluateDirectSpecular(
        vec3 specularColorF0,
        vec3 specularColorF90,
        float specularRoughness,
        float fresnel,
        float NdotL,
        float NdotE,
        float NdotH,
        float EdotH)
{
    vec3 F = mix(specularColorF0, specularColorF90, fresnel);
    float D = NormalDistribution(specularRoughness, NdotH);
    float G = Geometric(specularRoughness, NdotL, NdotE, NdotH, EdotH);
    vec3 RNum = F * G * D;
    float RDenom = 4.0f * NdotL * NdotE + EPSILON;
    return RNum / RDenom;
}

LightingContributions
evaluateLight(
        vec3 diffuseColor,
        bool useSpecularWorkflow,
        float ior,
        float metallic,
        float specularAmount,
        vec3 specularColor,
        float specularRoughness,
        float clearcoatAmount,
        vec3 clearcoatColor,
        float clearcoatRoughness,
        float occlusion,
        float NdotL,
        float NdotE,
        float NdotH,
        float EdotH,
        vec3 lightDiffuseIrradiance,
        vec3 lightSpecularIrradiance)
{
    specularRoughness = max(0.001, specularRoughness);
    clearcoatRoughness = max(0.001, clearcoatRoughness);

    float fresnel = SchlickFresnel(EdotH);

    // Evaluate diffuse
    vec3 d = diffuseColor * evaluateDirectDiffuse();

    // Evaluate specular first lobe
    vec3 s1 = vec3(0.0);
    if (specularAmount > 0.0) {
        vec3 F0 = specularColor;
        vec3 F90 = vec3(1.0);

        if (!useSpecularWorkflow) {
            float R = (1.0 - ior) / (1.0 + ior);
            vec3 specColor = mix(vec3(1.0), diffuseColor, metallic);
            F0  = R * R * specColor;
            F90 = specColor;

            // For metallic workflows, pure metals have no diffuse
            d *= 1.0 - metallic;
        }

        s1 = specularAmount * evaluateDirectSpecular(
             F0,                          // Specular color 0
             F90,                         // Specular color 90
             specularRoughness,           // Roughness
             fresnel,                     // Fresnel
             NdotL, NdotE, NdotH, EdotH); // Dot products needed for lights

        // Adjust the diffuse so glazing angles have less diffuse
        d *= (1.0 - mix(F0, F90, fresnel));
    }

    // Evaluate clearcoat
    vec3 s2 = vec3(0.0);
    if (clearcoatAmount > 0.0) {
        s2 = clearcoatAmount * evaluateDirectSpecular(
             clearcoatColor,              // Clearcoat color 0
             clearcoatColor,              // Clearcoat color 90
             clearcoatRoughness,          // Roughness
             fresnel,                     // Fresnel
             NdotL, NdotE, NdotH, EdotH); // Dot products needed for lights
    }

    LightingContributions lightingContrib;

    lightingContrib.diffuse =
        occlusion * NdotL * d * lightDiffuseIrradiance;

    lightingContrib.specular =
        occlusion * NdotL * (s1 + s2) * lightSpecularIrradiance;

    return lightingContrib;
}


-- glsl Preview.LightIntegration

vec2 projectToLatLong(vec3 sample3D)
{
    // project spherical coord onto latitude-longitude map with
    // latitude: +y == pi/2 and longitude: +z == 0, +x == pi/2
    vec2 coord = vec2((atan(sample3D.z, sample3D.x) + 0.5 * PI) / (2.0 * PI),
                      acos(sample3D.y) / PI);
    return coord;
}

mat4 GetDomeLightTransform(mat4 worldToLightTransform)
{
    // transform from view space to light space
    mat4 worldToViewInverse = GetWorldToViewInverseMatrix();
    return worldToLightTransform * worldToViewInverse;
}

vec3
evaluateIndirectLighting(
        vec3 diffuseColor,
        vec3 specularColor,
        vec3 Neye,
        vec3 Reye,
        float NdotE,
        float EdotH,
        float ior,
        float metallic,
        float occlusion,
        float roughness,
        bool useSpecularWorkflow,
        mat4 worldToLightTransform)
{
    vec3 indirect = vec3(0.0);

#ifdef HD_HAS_domeLightIrradiance
    vec3 F0 = specularColor;
    vec3 F90 = vec3(1.0);
    vec3 d = diffuseColor;
    if (!useSpecularWorkflow) {
        float R = (1.0 - ior) / (1.0 + ior);
        vec3 specColor = mix(vec3(1.0), diffuseColor, metallic);
        F0  = R * R * specColor;
        F90 = specColor;

        // For metallic workflows, pure metals have no diffuse
        d *= 1.0 - metallic;
    }
    // Adjust the diffuse so glazing angles have less diffuse
    float fresnel = SchlickFresnel(EdotH);
    vec3 F = mix(F0, F90, fresnel);
    d *= (1.0 - F);

    mat4 transformationMatrix = GetDomeLightTransform(worldToLightTransform);

    // Diffuse Component
    vec3 dir = vec4(transformationMatrix * vec4(Neye,0.0)).xyz;
    vec2 coord = projectToLatLong(dir);
    vec3 diffuse = HdGet_domeLightIrradiance(coord).rgb;

    // Specular Component
    const float MAX_REFLECTION_LOD = 4.0;
    float lod = roughness * MAX_REFLECTION_LOD;
    vec3 Rdir = vec4(transformationMatrix * vec4(Reye,0.0)).xyz;
    vec2 Rcoord = projectToLatLong(Rdir);
    vec3 prefilter = textureLod(HdGetSampler_domeLightPrefilter(),
                                Rcoord, lod).rgb;

    vec2 brdf = HdGet_domeLightBRDF(vec2(NdotE, roughness)).rg;

    vec3 specular = prefilter * (F * brdf.x + brdf.y);

    // Indirect Lighting
    indirect = (d * diffuse + specular) * occlusion;
#endif

    return indirect;
}

vec3
evaluateLights(
        vec3 emissiveColor,
        vec3 diffuseColor,
        bool useSpecularWorkflow,
        float ior,
        float metallic,
        float specularAmount,
        vec3 specularColor,
        float specularRoughness,
        float clearcoatAmount,
        vec3 clearcoatColor,
        float clearcoatRoughness,
        float occlusion,
        vec4 Peye,
        vec3 Neye)
{
    vec3 n = Neye;
    vec3 e = normalize(-Peye.xyz);
    float NdotE = max(0.0, dot(n, e));

    vec3 Reye = reflect(-e, n);

    vec3 directLight = vec3(0.0);
    vec3 indirectLight = vec3(0.0);

#if NUM_LIGHTS > 0
    for (int i = 0; i < NUM_LIGHTS; ++i) {

        // Calculate necessary vector information for lighting
        vec4 Plight = (lightSource[i].isIndirectLight)
                        ? vec4(0,0,0,1)
                        : lightSource[i].position;
        vec3 l = (Plight.w == 0.0)
                    ? normalize(Plight.xyz)
                    : normalize(Plight - Peye).xyz;
        vec3 h = normalize(e + l);
        float NdotL = max(0.0, dot(n, l));
        float NdotH = max(0.0, dot(n, h));
        float EdotH = max(0.0, dot(e, h));

        // Calculate light intesity
        float atten = lightDistanceAttenuation(Peye, i);
        float spot = lightSpotAttenuation(l, i);

        // Calculate the shadow factor
        float shadow = 1.0;
    #if USE_SHADOWS
        shadow = (lightSource[i].hasShadow) ?
            shadowing(/*lightIndex=*/i, Peye) : 1.0;
    #endif

        float intensity = atten * spot * shadow;

        vec3 lightDiffuseIrradiance = intensity * lightSource[i].diffuse.rgb;
        vec3 lightSpecularIrradiance = intensity * lightSource[i].specular.rgb;

        LightingContributions lightingContrib = evaluateLight(
            diffuseColor,
            useSpecularWorkflow,
            ior,
            metallic,
            specularAmount,
            specularColor,
            specularRoughness,
            clearcoatAmount,
            clearcoatColor,
            clearcoatRoughness,
            occlusion,
            NdotL,
            NdotE,
            NdotH,
            EdotH,
            lightDiffuseIrradiance,
            lightSpecularIrradiance);

        // calculate the indirect light (DomeLight)
        if (lightSource[i].isIndirectLight) {

            indirectLight = evaluateIndirectLighting(diffuseColor,
                                    specularColor, Neye, Reye, NdotE,
                                    EdotH, ior, metallic, occlusion,
                                    specularRoughness, useSpecularWorkflow,
                                    lightSource[i].worldToLightTransform);
        }
        // all other light sources contribute to the direct lighting
        else {
            directLight += (lightingContrib.diffuse + lightingContrib.specular);
        }
    }
#endif

    return (emissiveColor + directLight + indirectLight);
}
#endif
