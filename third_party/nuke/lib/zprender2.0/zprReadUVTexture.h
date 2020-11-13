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

/// @file zprReadUVTexture.h
///
/// @author Jonathan Egstad


#ifndef zprReadUVTexture_h
#define zprReadUVTexture_h

#include "RayShader.h"

#include <DDImage/Read.h>

namespace zpr {


/*!
*/
class ZPR_EXPORT zprReadUVTexture : public RayShader
{
  public:
    struct InputParams
    {
        std::string k_file;
        int         k_wrapS;
        int         k_wrapT;
        Fsr::Vec4f  k_fallback;
        Fsr::Vec4f  k_scale;
        Fsr::Vec4f  k_bias;
    };


  public:
    InputParams inputs;

    DD::Image::Hash   m_file_hash;      //!< Only update if file path updates
    DD::Image::Read*  m_read;           //!< Read Iop to access
    bool              m_file_exists;    //!< Can the file be read
    bool              m_read_error;     //!< Reader had some error
    zpr::InputBinding m_binding;        //!< Texture binding


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    //!
    zprReadUVTexture(const char* path=NULL);
    zprReadUVTexture(const InputParams& input_params);

    //! Must have a virtual destructor to subclass!
    virtual ~zprReadUVTexture();

    //!
    //bool load(bool force=false);

    //!
    void setFilename(const char* path,
                     int         version);


    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);
    /*virtual*/ void validateShader(bool                            for_real,
                                    const RenderContext*            rtx,
                                    const DD::Image::OutputContext* op_ctx=NULL);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);

};


} // namespace zpr

#endif

// end of zprReadUVTexture.h

//
// Copyright 2020 DreamWorks Animation
//
