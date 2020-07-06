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

/// @file zprPreviewSurface.h
///
/// @author Jonathan Egstad


#ifndef zprPreviewSurface_h
#define zprPreviewSurface_h

#include "RayShader.h"


namespace zpr {


/*!
*/
class ZPR_EXPORT zprPreviewSurface : public RayShader
{
  public:
    struct InputParams
    {
        Fsr::Vec3d k_diffuseColor;          //!< (0.18, 0.18, 0.18) When using metallic workflow this is interpreted as albedo.
        Fsr::Vec3d k_emissiveColor;         //!< (0.0, 0.0, 0.0)  Emissive component. 
        int        k_useSpecularWorkflow;   //!< 0 
        Fsr::Vec3d k_specularColor;         //!< (0.0, 0.0, 0.0)
        double     k_metallic;              //!< 0.0
        double     k_roughness;             //!< 0.5   Roughness for the specular lobe. The value ranges from 0 to 1, which goes from a perfectly specular surface at 0.0 to maximum roughness of the specular lobe. This value is usually squared before use with a GGX or Beckmann lobe.
        double     k_clearcoat;             //!< 0.0   Second specular lobe amount. The color is white.
        double     k_clearcoatRoughness;    //!< 0.01  Roughness for the second specular lobe.
        double     k_opacity;               //!< 1.0   When opacity is 1.0 then the gprim is fully opaque, if it is smaller than 1.0 then the prim is translucent, when it is 0 the gprim is transparent.  Note that even a fully transparent object still receives lighting as, for example, perfectly clear glass still has a specular response.
        double     k_opacityThreshold;      //!< 0.0   The opacityThreshold input is useful for creating geometric cut-outs based on the opacity input. A value of 0.0 indicates that no masking is applied to the opacity input, while a value greater than 0.0 indicates that rendering of the surface is limited to the areas where the opacity is greater than that value. A classic use of opacityThreshold is to create a leaf from an opacity input texture, in that case the threshold determines the parts of the opacity texture that will be fully transparent and not receive lighting. Note that when opacityThreshold is greater than zero, then opacity modulates the presence of the surface, rather than its transparency - pathtracers might implement this as allowing ((1 - opacity) * 100) % of the rays that do intersect the object to instead pass through it unhindered, and rasterizers may interpret opacity as pixel coverage.  Thus, opacityThreshold serves as a switch for how the opacity input is interpreted; this "translucent or masked" behavior is common in engines and renderers, and makes the UsdPreviewSurface easier to interchange.  It does imply, however, that it is not possible to faithfully recreate a glassy/translucent material that also provides an opacity-based mask... so no single-polygon glass leaves.
        double     k_ior;                   //!< 1.5   Index of Refraction to be used for translucent objects.
        Fsr::Vec3d k_normal;                //!< (0.0, 0.0, 1.0)  Expects normal in tangent space [(-1,-1,-1), (1,1,1)] This means your texture reader implementation should provide data to this node that is properly scaled and ready to be consumed as a tangent space normal.
        double     k_displacement;          //!< 0.0   Displacement in the direction of the normal.
        double     k_occlusion;             //!< 1.0 


        //!
        InputParams();
    };


    struct LocalVars
    {
        bool m_diffuse_enabled;
        bool m_specular_enabled;
        bool m_transmission_enabled;
        bool m_emission_enabled;
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

    //!
    zprPreviewSurface();
    ~zprPreviewSurface();

    //!
    void setFilename(const char* path,
                     int         version);


    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);

};


} // namespace zpr

#endif

// end of zprPreviewSurface.h

//
// Copyright 2020 DreamWorks Animation
//
