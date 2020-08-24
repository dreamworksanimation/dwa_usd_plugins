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
    Fsr::Vec3f k_diffuseColor;          //!< (0.18, 0.18, 0.18) When using metallic workflow this is interpreted as albedo.
    Fsr::Vec3f k_emissiveColor;         //!< (0.0, 0.0, 0.0)  Emissive component. 
    int        k_useSpecularWorkflow;   //!< 0 
    Fsr::Vec3f k_specularColor;         //!< (0.0, 0.0, 0.0)
    float      k_metallic;              //!< 0.0
    float      k_roughness;             //!< 0.5   Roughness for the specular lobe.
    float      k_clearcoat;             //!< 0.0   Second specular lobe amount. The color is white.
    float      k_clearcoatRoughness;    //!< 0.01  Roughness for the second specular lobe.
    float      k_opacity;               //!< 1.0   When opacity is 1.0 then the gprim is fully opaque, if it is smaller than 1.0 then the prim is translucent, when it is 0 the gprim is transparent.
    float      k_opacityThreshold;      //!< 0.0   The opacityThreshold input is useful for creating geometric cut-outs based on the opacity input.
    float      k_ior;                   //!< 1.5   Index of Refraction to be used for translucent objects.
    Fsr::Vec3f k_normal;                //!< (0.0, 0.0, 1.0)  Expects normal in tangent space [(-1,-1,-1), (1,1,1)]
    float      k_displacement;          //!< 0.0   Displacement in the direction of the normal.
    float      k_occlusion;             //!< 1.0 

    bool m_diffuse_enabled;
    bool m_specular_enabled;
    bool m_transmission_enabled;
    bool m_emission_enabled;


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

  protected:
    //!
    Fsr::Vec3f evaluateLights(RayShaderContext& stx,
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
                              float             occlusion) const;

};


} // namespace zpr

#endif

// end of zprPreviewSurface.h

//
// Copyright 2020 DreamWorks Animation
//
