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

/// @file zprRectangleLight.h
///
/// @author Jonathan Egstad


#ifndef zprRectangleLight_h
#define zprRectangleLight_h

#include <zprender/LightShader.h>

#include <DDImage/Filter.h>


namespace zpr {


/*!
*/
class zprRectangleLight : public LightShader
{
  public:
    // Each of these corresponds with an exposed input arrow connection
    // skipping input 0 (axis):
    enum MaterialOpBindings
    {
        MAP0,
        //
        NUM_INPUTS
    };

    struct InputParams : public BaseInputParams
    {
        InputBinding k_bindings[NUM_INPUTS];

        double       k_lens_in_focal;           //!< Focal-length of card
        double       k_lens_in_haperture;       //!< Horiz-aperture of card
        double       k_z;                       //!< Z distance from local origin
        //
        bool         k_single_sided;            //!< Does the light emit in both directions?
        Fsr::Pixel   k_map;                     //!< Texture input
        bool         k_map_enable;              //!< Enable texture map
        double       k_filter_size;             //!< Scale filter kernel
        //
        DD::Image::Filter  k_map_filter;
        DD::Image::Channel k_map_channel[4];    //!< Channels to use from map
    };


  public:
    InputParams inputs;

    float          m_width_half;
    float          m_height_half;
    Fsr::Vec2f     m_filter_dx;
    Fsr::Vec2f     m_filter_dy;


  public:
    static const ShaderDescription description;
    /*virtual*/ const char* zprShaderClass() const { return description.shaderClass(); }
    static const InputKnobList  input_defs;
    static const OutputKnobList output_defs;
    /*virtual*/ const InputKnobList&  getInputKnobDefinitions() const  { return input_defs;  }
    /*virtual*/ const OutputKnobList& getOutputKnobDefinitions() const { return output_defs; }

    //!
    zprRectangleLight();
    //!
    zprRectangleLight(const InputParams&     input_params,
                      const Fsr::DoubleList& motion_times,
                      const Fsr::Mat4dList&  motion_xforms);


    //! Return a pointer to the input uniform parameter structure.
    /*virtual*/ BaseInputParams* uniformInputs() { return &inputs; }

    /*virtual*/ InputBinding* getInputBinding(uint32_t input);
    /*virtual*/ void setMotionXforms(const Fsr::DoubleList& motion_times,
                                     const Fsr::Mat4dList&  motion_xforms);
    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);
    /*virtual*/ void validateShader(bool                            for_real,
                                    const RenderContext*            rtx,
                                    const DD::Image::OutputContext* op_ctx);
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);

    //!
    /*virtual*/ bool illuminate(RayShaderContext& stx,
                                Fsr::RayContext&  illum_ray,
                                float&            direct_pdfW_out,
                                Fsr::Pixel&       illum_color_out);


    //-----------------------------------------------------------------------
    // Legacy shading support:

    /*! Calculate a normalized direction vector 'lightNOut' and distance
        'lightDistOut' from the light to surface point 'surfP'.

        Normalized vector 'lobeN' is passed to allow lights like area lights
        to simulate a large emission surface. 'lobeN' is usually the surface
        normal when querying the diffuse surface contribution and the
        reflection vector off the surface when querying specular contribution.
    */
    /*virtual*/ void getLightVector(const DD::Image::LightContext& ltx,
                                    const DD::Image::Vector3&      surfP,
                                    const DD::Image::Vector3&      lobeN,
                                    DD::Image::Vector3&            lightNOut,
                                    float&                         lightDistOut) const;


    /*! Return the amount of shadowing the light creates at surface point surfP,
        and optionally copies the shadow mask to a channel in shadowChanOut.
    */
    /*virtual*/ float getShadowing(const DD::Image::LightContext&  ltx,
                                   const DD::Image::VertexContext& vtx,
                                   const DD::Image::Vector3&       surfP,
                                   DD::Image::Pixel&               shadowChanOut) const;


    /*! Returns the color of the light (possibly) using the current
        surface point and normal to calculate attenuation and penumbra.
    */
    /*virtual*/ void getColor(const DD::Image::LightContext& ltx,
                              const DD::Image::Vector3&      surfP,
                              const DD::Image::Vector3&      lobeN,
                              const DD::Image::Vector3&      lightN,
                              float                          lightDist,
                              DD::Image::Pixel&              colorChansOut) const;

};


} // namespace zpr

#endif

// end of zprRectangleLight.h

//
// Copyright 2020 DreamWorks Animation
//
