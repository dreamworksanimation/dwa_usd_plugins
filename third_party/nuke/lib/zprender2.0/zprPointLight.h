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

/// @file zprTextureIop.h
///
/// @author Jonathan Egstad


#ifndef zprPointLight_h
#define zprPointLight_h

#include "LightShader.h"

namespace zpr {


/*!
*/
class ZPR_EXPORT zprPointLight : public LightShader
{
  public:
    struct InputParams : public BaseInputParams
    {
        double k_near;
        double k_far;
    };


  public:
    InputParams inputs;

    double m_near;      //!< Clamped k_near
    double m_far;       //!< Clamped k_far


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }


    //!
    zprPointLight();
    //!
    zprPointLight(const InputParams&     input_params,
                  const Fsr::DoubleList& motion_times,
                  const Fsr::Mat4dList&  motion_xforms);

    //! Must have a virtual destructor to subclass!
    virtual ~zprPointLight() {}


    //! Return a pointer to the input uniform parameter structure.
    /*virtual*/ BaseInputParams* uniformInputs() { return &inputs; }

    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);

    /*virtual*/ bool illuminate(RayShaderContext& stx,
                                Fsr::RayContext&  illum_ray,
                                float&            direct_pdfW_out,
                                Fsr::Pixel&       illum_color_out);


    /*! Can this light shader produce a LightVolume?
        Why yes, a simple SphereVolume.
    */
    /*virtual*/ bool canGenerateLightVolume();

    /*! Return the entire motion bbox enclosing the LightVolume that
        this shader can create during createLightVolume().
        This is a union of all the transformed motion spheres.
    */
    /*virtual*/ Fsr::Box3d getLightVolumeMotionBbox();

    /*! Create and return a SphereVolume primitive.
    */
    /*virtual*/ LightVolume* createLightVolume(const MaterialContext* material_ctx);
};


} // namespace zpr

#endif

// end of zprPointLight.h

//
// Copyright 2020 DreamWorks Animation
//
