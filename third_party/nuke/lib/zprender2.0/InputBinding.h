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

/// @file zprender/InputBinding.h
///
/// @author Jonathan Egstad


#ifndef zprender_InputBinding_h
#define zprender_InputBinding_h

#include "RayShaderContext.h"

#include <DDImage/Iop.h>
#include <DDImage/Material.h>
#include <DDImage/CameraOp.h>
#include <DDImage/LightOp.h>


namespace zpr {

class SurfaceMaterialOp;
class Texture2dSampler;


/*! RayShader input binding.
    TODO: templatize this so that we can support integer, string, etc inputs.
*/
class ZPR_EXPORT InputBinding
{
  public:
    /*! Shader input binding type primarily used to 'reach outside' the
        RayShader environment to the Nuke Op environment.
    */
    enum
    {
        NONE=0,             //!< Binding is not valid, don't use
        CONSTANT,           //!< Constant value binding     - TODO: deprecate?
        ATTRIB,             //!< Geometry attribute binding - TODO: deprecate? Change to GeoOp input?
        //
        RAYSHADER,          //!< A RayShader output
        //
        // Below are only for Op connects:
        SURFACEMATERIALOP,  //!< zpr::SurfaceMaterialOp input (handled by the SurfaceMaterialOp class)
        MATERIALIOP,        //!< DD::Image::Material Iop input - legacy Nuke shader
        TEXTUREIOP,         //!< DD::Image::Iop input using Texture2dSampler to sample
        AXISOP,             //!< DD::Image::AxisOp input
        CAMERAOP,           //!< DD::Image::CameraOp input
        LIGHTOP,            //!< DD::Image::LightOp input
    };
    // Flags:
    enum
    {
        HAS_RGB   = 0x01,   //!< Input image has at least 3 channels
        HAS_ALPHA = 0x02,   //!< Input image has an alpha channel
        IS_MONO   = 0x04    //!< Input image is single-channel - mutually-exclusive with HAS_RGB
    };


  public:
    struct ExprContext
    {
        RayShaderContext* stx;
        Fsr::Vec4f        const_val;

        ExprContext() {}
        ExprContext(RayShaderContext* _stx,
                    const Fsr::Vec4f& _const_val) : stx(_stx), const_val(_const_val) {}
    };
    typedef void (*Handler)(const ExprContext& etx, Fsr::Vec4f& out);


  public:
    uint16_t              type;             //!< Type of input binding
    uint16_t              flags;            //!< Input flags
    Fsr::Vec4f            constant_value;   //!< Value to use if binding is a constant
    void*                 input_object;     //!< Cast to an Iop*, Material*, AxisOp*, etc.
    DD::Image::Channel    rgb_chans[3];     //!< Packed list of assigned rgb channels
    DD::Image::Channel    opacity_chan;     //!< Opacity channel if available
    uint16_t              num_channels;     //!< Number of channels this binding produces (4 max)
    Fsr::Vec2f            uv_tile_offset;   //!< UDIM utile offset
    InputBinding::Handler handler;          //!< Sampler handler to use


  public:
    //!
    InputBinding(uint16_t _type=NONE);
    //!
    ~InputBinding() {}


    bool noBinding()   const { return (type == NONE); }
    bool isNone()      const { return (type == NONE); }
    bool isEnabled()   const { return (type >  NONE); }
    //
    bool isConstant()  const { return (type == CONSTANT); }
    bool isAttrib()    const { return (type == ATTRIB  ); }
    //
    bool isRayShader() const { return (type == RAYSHADER); }
    //
    bool isNukeOp()            const { return (type >= SURFACEMATERIALOP); }
    bool isSurfaceMaterialOp() const { return (type == SURFACEMATERIALOP); }
    bool isTextureIop()        const { return (type == TEXTUREIOP       ); }
    bool isMaterialIop()       const { return (type == MATERIALIOP      ); }
    bool isAxisOp()            const { return (type == AXISOP           ); }
    bool isCameraOp()          const { return (type == CAMERAOP         ); }
    bool isLightOp()           const { return (type == LIGHTOP          ); }

    //! If set to a constant this is it.
    const Fsr::Vec4f&     getConstant()    const { return constant_value; }

    //! Builds a ChannelSet on the fly from the chans.
    DD::Image::ChannelSet getChannels()    const;
    uint32_t              getNumChannels() const { return (uint32_t)num_channels; }

    //! Return true if it's a color3 or color4 output type.
    bool isActiveColor() const { return (isEnabled() && num_channels >= 3); }

    //! Return true if it's a Texture binding and it's enabled and valid.
    bool isActiveTexture() const { return (isTextureIop() && isEnabled() && num_channels > 0); }

    RayShader*            asRayShader()         const { return (isRayShader()        ) ? static_cast<RayShader*>(input_object): NULL; }
    SurfaceMaterialOp*    asSurfaceMaterialOp() const { return (isSurfaceMaterialOp()) ? static_cast<SurfaceMaterialOp*>(input_object): NULL; }

    DD::Image::Iop*       asTextureIop()  const { return (isTextureIop() ) ? static_cast<DD::Image::Iop*>(input_object): NULL; }
    DD::Image::Material*  asMaterialIop() const { return (isMaterialIop()) ? static_cast<DD::Image::Material*>(input_object): NULL; }
    DD::Image::AxisOp*    asAxisOp()      const { return (isAxisOp()     ) ? static_cast<DD::Image::AxisOp*>(input_object): NULL; }
    DD::Image::CameraOp*  asCameraOp()    const { return (isCameraOp()   ) ? static_cast<DD::Image::CameraOp*>(input_object): NULL; }
    DD::Image::LightOp*   asLightOp()     const { return (isLightOp()    ) ? static_cast<DD::Image::LightOp*>(input_object): NULL; }


    bool hasRGB()      const { return (flags & HAS_RGB  ); }
    bool hasAlpha()    const { return (flags & HAS_ALPHA); }
    bool isMono()      const { return (flags & IS_MONO  ); }
    //! Does the input have an alpha and we are using 4 channels?
    bool useAlpha()    const { return (hasRGB() && hasAlpha()); }


  public:
    //!
    void setActiveChannels(DD::Image::Channel red_chan,
                           DD::Image::Channel green_chan,
                           DD::Image::Channel blue_chan,
                           DD::Image::Channel _opacity_chan=DD::Image::Chan_Alpha);

    //! No opacity
    void setToConstantBinding(const Fsr::Vec3f& constant);
    //! With opacity
    void setToConstantBinding(const Fsr::Vec4f& constant);


    //! Parses the binding expression and return a configured InputBinding.
    static InputBinding buildFromBindExpression(const char* expr);

    //! Get the binding configuration for an input Op.
    static InputBinding buildInputOpBinding(DD::Image::Op* op);

    //! Get the binding configuration for an input Iop.
    static InputBinding buildInputTextureBinding(DD::Image::Iop*    iop,
                                                 DD::Image::Channel red_chan,
                                                 DD::Image::Channel green_chan,
                                                 DD::Image::Channel blue_chan,
                                                 DD::Image::Channel _opacity_chan=DD::Image::Chan_Alpha);

    //! Return the binding's value (usually a color) depending on its type.
    void       getValue(RayShaderContext& stx,
                        Fsr::Pixel&       out) const;
    Fsr::Vec3f getValue(RayShaderContext& stx,
                        float*            out_alpha=NULL) const;

    /*! Sample the texture input filling in the binding's rgb and opacity channels in Pixel.
        Uses the UV coord and derivatives from RayShaderContext.
    */
    void sampleTexture(RayShaderContext& stx,
                       Fsr::Pixel&       tex_color) const;
    /*! Sample the texture input filling in the binding's rgb and opacity channels in Pixel.
        Overrides UV coord and derivatives in RayShaderContext but uses the texture
        filter and samplers from it.
    */
    void sampleTexture(const Fsr::Vec2f& UV,
                       const Fsr::Vec2f& dUVdx,
                       const Fsr::Vec2f& dUVdy,
                       RayShaderContext& stx,
                       Fsr::Pixel&       tex_color) const;
    /*! Sample the texture input filling in the binding's rgb and opacity channels in Pixel.
        Uses slower Iop sample routines since there's no Texture2dSampler available.
    */
    void sampleTexture(const Fsr::Vec2f&        UV,
                       const Fsr::Vec2f&        dUVdx,
                       const Fsr::Vec2f&        dUVdy,
                       const DD::Image::Filter* texture_filter,
                       Fsr::Pixel&              tex_color) const;
};


//!
std::ostream& operator << (std::ostream&, const InputBinding&);



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline void
InputBinding::sampleTexture(RayShaderContext& stx,
                            Fsr::Pixel&       tex_color) const
{
    sampleTexture(stx.UV,
                  stx.dUVdx,
                  stx.dUVdy,
                  stx,
                  tex_color);
                  
}


} // namespace zpr

#endif

// end of zprender/InputBinding.h

//
// Copyright 2020 DreamWorks Animation
//
