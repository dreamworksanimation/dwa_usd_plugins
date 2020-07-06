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


#ifndef zprIopUVTexture_h
#define zprIopUVTexture_h

#include "RayShader.h"

#include <DDImage/Iop.h>

namespace zpr {


/*!
*/
class ZPR_EXPORT zprIopUVTexture : public RayShader
{
  public:
    InputBinding m_binding;


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    //!
    zprIopUVTexture(DD::Image::Iop* iop=NULL);
    //! Ctor assumes this is being constructed from the InputBinding of another RayShader.
    zprIopUVTexture(const InputBinding& binding);


    /*virtual*/ void validateShader(bool                 for_real,
                                    const RenderContext& rtx);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);

};


} // namespace zpr

#endif

// end of zprIopUVTexture.h

//
// Copyright 2020 DreamWorks Animation
//
