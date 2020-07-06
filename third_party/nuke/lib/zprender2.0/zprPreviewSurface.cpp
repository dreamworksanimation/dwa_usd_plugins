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
#include "ThreadContext.h"

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
/*static*/ const RayShader::ShaderDescription zprPreviewSurface::description("zprPreviewSurface", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprPreviewSurface::input_defs =
{
    {InputKnob("diffuseColor",        COLOR3_KNOB)},
    {InputKnob("emissiveColor",       COLOR3_KNOB)},
    {InputKnob("useSpecularWorkflow", INT_KNOB   )},
    {InputKnob("specularColor",       COLOR3_KNOB)},
    {InputKnob("metallic",            DOUBLE_KNOB)},
    {InputKnob("roughness",           DOUBLE_KNOB)},
    {InputKnob("clearcoat",           DOUBLE_KNOB)},
    {InputKnob("clearcoatRoughness",  DOUBLE_KNOB)},
    {InputKnob("opacity",             DOUBLE_KNOB)},
    {InputKnob("opacityThreshold",    DOUBLE_KNOB)},
    {InputKnob("ior",                 DOUBLE_KNOB)},
    {InputKnob("normal",              COLOR3_KNOB)},
    {InputKnob("displacement",        DOUBLE_KNOB)},
    {InputKnob("occlusion",           DOUBLE_KNOB)},
};
/*static*/ const RayShader::OutputKnobList zprPreviewSurface::output_defs =
{
    {OutputKnob("surface",            STRING_KNOB)},
    {OutputKnob("displacement",       STRING_KNOB)},
};


//!
zprPreviewSurface::InputParams::InputParams()
{
    k_diffuseColor.set(0.18, 0.18, 0.18);
    k_emissiveColor.set(0.0, 0.0, 0.0);
    k_useSpecularWorkflow = 0;
    k_specularColor.set(0.0, 0.0, 0.0);
    k_metallic            = 0.0;
    k_roughness           = 0.5;
    k_clearcoat           = 0.0;
    k_clearcoatRoughness  = 0.01;
    k_opacity             = 1.0;
    k_opacityThreshold    = 0.0;
    k_ior                 = 1.5;
    k_normal.set(0.0, 0.0, 1.0);
    k_displacement        = 0.0;
    k_occlusion           = 1.0;
}


/*!
*/
zprPreviewSurface::zprPreviewSurface() :
    RayShader(input_defs, output_defs)
{
    //std::cout << "zprPreviewSurface::ctor(" << this << ")" << std::endl;
    assert(m_inputs.size() == input_defs.size());

    // Point the knobs to their values:
    m_inputs[ 0].data = &inputs.k_diffuseColor;
    m_inputs[ 1].data = &inputs.k_emissiveColor;
    m_inputs[ 2].data = &inputs.k_useSpecularWorkflow;
    m_inputs[ 3].data = &inputs.k_specularColor;
    m_inputs[ 4].data = &inputs.k_metallic;
    m_inputs[ 5].data = &inputs.k_roughness;
    m_inputs[ 6].data = &inputs.k_clearcoat;
    m_inputs[ 7].data = &inputs.k_clearcoatRoughness;
    m_inputs[ 8].data = &inputs.k_opacity;
    m_inputs[ 9].data = &inputs.k_opacityThreshold;
    m_inputs[10].data = &inputs.k_ior;
    m_inputs[11].data = &inputs.k_normal;
    m_inputs[12].data = &inputs.k_displacement;
    m_inputs[13].data = &inputs.k_occlusion;
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
    Fsr::Pixel& tex = stx.thread_ctx->surface_color;
    tex.setChannels(m_texture_channels);

    Fsr::Vec3f  diffuseColor(inputs.k_diffuseColor);
    if (getInput(0))
    {
        getInput(0)->evaluateSurface(stx, tex);
        diffuseColor = tex.rgb();
    }

    Fsr::Vec3f emissiveColor(inputs.k_emissiveColor);
    if (getInput(1))
    {
        getInput(1)->evaluateSurface(stx, tex);
        emissiveColor = tex.rgb();
    }

    Fsr::Vec3f specularColor(inputs.k_specularColor);
    if (getInput(3))
    {
        getInput(1)->evaluateSurface(stx, tex);
        specularColor = tex.rgb();
    }

    float occlusion = inputs.k_occlusion;
    if (getInput(13))
    {
        getInput(13)->evaluateSurface(stx, tex);
        occlusion = tex.r();
    }

    out.rgb()   = diffuseColor*occlusion + emissiveColor;
    out.alpha() = float(inputs.k_opacity);
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
