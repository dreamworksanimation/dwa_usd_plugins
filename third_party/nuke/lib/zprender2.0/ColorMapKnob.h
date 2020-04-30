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

/// @file zprender/ColorMapKnob.h
///
/// @author Jonathan Egstad


#ifndef zprender_ColorMapKnob_h
#define zprender_ColorMapKnob_h

#include "RayShader.h"

#include <DDImage/Iop.h>

namespace zpr {



/*!
*/
class ZPR_EXPORT ColorMapKnob
{
  public:
    struct ExprContext
    {
        RayShaderContext*  stx;
        DD::Image::Vector4 const_val;
    };

    typedef void (*Handler)(const ExprContext& etx, Fsr::Vec4f& out);


  protected:
    bool                  k_enable;             //!< Enable/disable the map
    const char*           k_binding_expr;       //!< Binding expression
    DD::Image::Channel    k_map_chans[4];       //!< Texture map channels to sample
    //
    DD::Image::Iop*       m_parent;             //!< The shader who owns us
    ColorMapKnob::Handler m_handler;            //!< Sampler handler to use
    //
    int                   m_input;              //!< The Node input # the map is coming from
    RayShader::MapBinding m_binding;            //!< Type of binding (Attribute, Material, RayShader, Iop, etc)
    int                   m_num_channels;       //!< Number of channels
    DD::Image::ChannelSet m_channels;           //!< ChannelSet built from k_map_chans[]
    DD::Image::Hash       m_hash;               //!< Hash of knob
    Fsr::Vec4f            m_constant;           //!< Value to use if expression is constant
    Fsr::Vec2f            m_uv_tile_offset;     //!< UDIM utile offset
    //
    const char*           m_knob_names[2];

    //! Parses the binding expression, returning true on success
    uint32_t parseExpression(const char* expr);


  public:
    ColorMapKnob(DD::Image::Iop*    parent,
                 int                input_num=1,
                 int                num_chans=3,
                 DD::Image::Channel first_chan=DD::Image::Chan_Red);
    virtual ~ColorMapKnob();

    //! Is the sampler enabled?
    bool isEnabled() const { return (k_enable && !m_binding.noBinding()); }
    //! Sampler binding type
    uint16_t getType()   const { return m_binding.type; }
    //! Is the map a texture/shader source?
    bool isTexture() const { return m_binding.isTexture(); }
    //! Is the input a single channel?
    bool isMono()    const { return m_binding.isMono(); }
    //! Does the input have an alpha?
    bool hasAlpha()  const { return m_binding.hasAlpha(); }
    //! Does the input have an alpha and we are using 4 channels?
    bool useAlpha()  const { return (m_binding.hasAlpha() && m_num_channels == 4); }

    //! If set to a constant this is it.
    const Fsr::Vec4f&     getConstant() const { return m_constant; }

    DD::Image::ChannelSet getChannels() const { return m_channels; }

    //! Assign the input number of the map, the number of channels to fill in, and the starting channel index.
    void setInput(int                input_num,
                  int                num_chans=3,
                  DD::Image::Channel first_chan=DD::Image::Chan_Red);

    //! Build the knobs for this.
    virtual DD::Image::Knob* addKnobs(DD::Image::Knob_Callback f,
                                      const char*              name="color_map", const char* label=0);

    //!
    virtual bool knobChanged(DD::Image::Knob* k);

    //!
    virtual void validateColorMap(bool for_real);

    //!
    virtual void requestColorMap(int count) const;

    //! Sample the texture input, returning RGB in a Vec3f and filling in the optional alpha pointer.
    virtual Fsr::Vec3f sample(RayShaderContext& stx,
                              float*            out_alpha=NULL);
};


//-------------------------------------------------------------------------


//! Texture_knob helper method.
inline DD::Image::Knob*
Texture_knob(DD::Image::Knob_Callback f,
             ColorMapKnob&            map_knob,
             const char*              name,
             const char*              label=NULL)
{
    return map_knob.addKnobs(f, name, label);
}


} // namespace zpr

#endif

// end of zprender/ColorMapKnob.h

//
// Copyright 2020 DreamWorks Animation
//
