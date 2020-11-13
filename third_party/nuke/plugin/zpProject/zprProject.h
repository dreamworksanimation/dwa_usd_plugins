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

/// @file zprProject.h
///
/// @author Jonathan Egstad


#ifndef zprProject_h
#define zprProject_h

#include <zprender/RayShader.h>

namespace zpr {

/*!
*/
class zprProject : public RayShader
{
  public:
    enum { MERGE_NONE, MERGE_REPLACE, MERGE_OVER, MERGE_UNDER, MERGE_STENCIL, MERGE_MASK, MERGE_PLUS, MERGE_AVG, MERGE_MIN, MERGE_MAX };
    static const char* const operation_modes[];

    enum { FACES_BOTH, FACES_FRONT, FACES_BACK };
    static const char* const face_names[];

    enum { Z_CLIP_NONE, Z_CLIP_CAM, Z_CLIP_USER };
    static const char* const zclip_modes[];


    // Each of these corresponds with an exposed input arrow connection:
    enum MaterialOpBindings
    {
        BG0,
        MAP1,
        CAMERA2,
        //
        NUM_INPUTS
    };


    struct InputParams
    {
        InputBinding k_bindings[NUM_INPUTS];

        int                      k_operation;               //!< Merge operation to perform on A
        int                      k_faces_mode;              //!< Project on front, back or both sides
        bool                     k_crop_to_format;          //!< Crop projection at edge of projection
        DD::Image::ChannelSet    k_proj_channels;           //!< Set of channels to project
        DD::Image::Filter        k_texture_filter;          //!< Filter to use for texture filtering
        int                      k_zclip_mode;              //!< 
        double                   k_near_clip;               //!< Near Z clipping plane
        double                   k_far_clip;                //!< Far Z clipping plane
    };


  public:
    InputParams inputs;

    DD::Image::CameraOp*  m_proj_cam;
    Fsr::Mat4d            m_projectxform, m_projectproj, m_projectconcat;
    double                m_near_clip, m_far_clip;
    double                m_cam_near,  m_cam_far;

    DD::Image::ChannelSet m_project_channels;


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    zprProject();
    zprProject(const InputParams& input_params);

    //! Initialize any uniform vars prior to rendering.
    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);
    /*virtual*/ void validateShader(bool                            for_real,
                                    const RenderContext*            rtx,
                                    const DD::Image::OutputContext* op_ctx);

    /*virtual*/ InputBinding* getInputBinding(uint32_t input);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);
    /*virtual*/ void evaluateSurface(RayShaderContext& stx,
                                     Fsr::Pixel&       out);

    //!
    bool project(const Fsr::Mat4d& proj_matrix,
                 RayShaderContext& stx,
                 Fsr::Vec2f&       UV,
                 Fsr::Vec2f&       dUVdx,
                 Fsr::Vec2f&       dUVdy);
};


} // namespace zpr

#endif

// end of zprProject.h

//
// Copyright 2020 DreamWorks Animation
//
