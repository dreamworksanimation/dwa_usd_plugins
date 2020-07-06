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

/// @file zprOcclusion.cpp
///
/// @author Jonathan Egstad


#include <zprender/RayShader.h>

namespace zpr {


/*!
*/
class zprOcclusion : public RayShader
{
  public:
    // Each of these corresponds with an exposed input arrow connection:
    enum MaterialOpBindings
    {
        BG0,
        //
        NUM_INPUTS
    };

    struct InputParams
    {
        InputBinding k_bindings[NUM_INPUTS];

        bool       k_amb_ocl_enabled;
        bool       k_refl_ocl_enabled;
        double     k_amb_ocl_mindist,  k_amb_ocl_maxdist,  k_amb_ocl_cone_angle;
        double     k_refl_ocl_mindist, k_refl_ocl_maxdist, k_refl_ocl_cone_angle;
        double     k_gi_scale;

        // AOV outputs:
        DD::Image::Channel k_amb_ocl_output;
        DD::Image::Channel k_refl_ocl_output;


        //!
        InputParams();
    };

    struct LocalVars
    {
        float  m_amb_ocl_cone_angle, m_refl_ocl_cone_angle;
        double m_amb_ocl_mindist,    m_refl_ocl_mindist;
        double m_amb_ocl_maxdist,    m_refl_ocl_maxdist;
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

    zprOcclusion();
    zprOcclusion(const InputParams& _inputs);

    //!
    static void updateLocals(const InputParams& _inputs,
                             LocalVars&         _locals);

    /*virtual*/ InputBinding* getInputBinding(uint32_t input);
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);


};


} // namespace zpr

// end of zprOcclusion.h

//
// Copyright 2020 DreamWorks Animation
//
