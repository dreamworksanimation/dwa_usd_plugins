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

/// @file zprModify.h
///
/// @author Jonathan Egstad


#ifndef zprModify_h
#define zprModify_h

#include <zprender/RayShader.h>
#include <zprender/RenderContext.h> // for GeoInfoContext

namespace zpr {


/*!
*/
class zprModify : public RayShader
{
  public:
    enum { XFORM_NONE, XFORM_LOCAL_TO_WORLD, XFORM_WORLD_TO_LOCAL };
    static const char* const xform_modes[];

    enum { TARGET_P_IN, TARGET_N_IN, TARGET_N_NG_IN, TARGET_UV_IN, TARGET_CF_IN, TARGET_RGBA_OUT, TARGET_RGB_OUT };
    static const char* const shader_target_names[];

    enum { OP_REPLACE, OP_OVER, OP_UNDER, OP_MULT, OP_ADD, OP_SUBTRACT };
    static const char* const operation_types[];

    // Each of these corresponds with an exposed input arrow connection:
    enum MaterialOpBindings
    {
        BG0,
        MAP1,
        //
        NUM_INPUTS
    };

    struct InputParams
    {
        InputBinding k_bindings[NUM_INPUTS];

        Fsr::Pixel k_map;
        int        k_matrix;            //!< Which matrix to transform value by
        int        k_operation;         //!< How to apply the result to the output channel
        Fsr::Vec3f k_map_scale;         //!< Scale to apply before merging
        float      k_opacity_scale;     //!< 
        int        k_shader_target;     //!< Which shader variable to map layer to


        //!
        InputParams();
    };

    InputParams inputs;


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    zprModify();
    zprModify(const InputParams& _inputs);

    /*virtual*/ InputBinding* getInputBinding(uint32_t input);
    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);

};


} // namespace zpr

#endif

// end of zprModify.h

//
// Copyright 2020 DreamWorks Animation
//
