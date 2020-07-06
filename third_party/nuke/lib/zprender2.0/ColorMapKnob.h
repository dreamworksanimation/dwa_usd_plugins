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

#include "InputBinding.h"

#include <DDImage/Knob.h>
#include <DDImage/Iop.h>

namespace zpr {


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.

    The ColorMap Knob is the most general type supporting Texture
    and Material inputs as well as user-defined constant value,
    depending on the expression string in the input binding.

    The implied shader network implied by this can be interpreted and
    converted into separate shaders or the InputBinding::sample() methods
    can be used to simplify this without requiring separate shaders.
*/
DD::Image::Knob* ColorMap_knob(DD::Image::Knob_Callback f,
                               InputBinding*            binding,
                               uint32_t                 input_num,
                               uint32_t                 num_chans,
                               const char*              name,
                               const char*              label=NULL);


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.

    The OpInput Knob does not create any user knobs and only supports a
    blind input connection to an input Op. Use the InputBinding::asGeoOp(),
    asAxisOp(), etc methods to get the connection cast to the correct type,
    use the InputBinding::isGeoOp(), isAxisOp(), etc. methods to verify the
    type if you don't know it already.

    This knob is automatically named 'inputop<#>' using 'input_num'.
*/
DD::Image::Knob* InputOp_knob(DD::Image::Knob_Callback f,
                              InputBinding*            binding,
                              uint32_t                 input_num);




/*! Wrapper knob around the color map controls.
*/
class ZPR_EXPORT ColorMapKnob : public DD::Image::Knob
{
  protected:
    bool                  k_enable;             //!< Enable/disable the map
    const char*           k_expr;               //!< Binding expression
    DD::Image::Channel    k_map_chans[4];       //!< Texture map channels to sample
    //
    int                   m_input;              //!< The Node input # the map is coming from
    DD::Image::Hash       m_expr_hash;          //!< Hash of expression string
    InputBinding          m_expr_binding;       //!< The binding resulting from expression parsing
    //
    const char*           m_knob_names[3];
    DD::Image::Knob       *kEnable, *kExpr, *kChans;


  public:
    //! Ctor used by ColorMap_knob() method.
    ColorMapKnob(DD::Image::Knob_Closure* cb,
                 uint32_t                 input_num,
                 uint32_t                 num_chans,
                 const char*              name,
                 const char*              label);

    //! Ctor used by OpInput_knob() method.
    ColorMapKnob(DD::Image::Knob_Closure* cb,
                 uint32_t                 input_num,
                 const char*              name);

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    // From DD::Image::Knob:

    //!
    /*virtual*/ const char* Class() const { return "ColorMapKnob"; }

    //! Don't make an interface. TODO: this causes build problems...
    ///*virtual*/ WidgetPointer make_widget(const DD::Image::WidgetContext&) { return NULL; }

    //! Don't do anything since the wrapper knob should never be written to a script file.
    /*virtual*/ bool from_script(const char*) { return true; }

    //! 
    /*virtual*/ void reset_to_default() {}

    //!
    /*virtual*/ void append(DD::Image::Hash&,
                            const DD::Image::OutputContext*);

    //! Stores into an InputBinding.
    /*virtual*/ void store(DD::Image::StoreType            type,
                           void*                           p,
                           DD::Image::Hash&                hash,
                           const DD::Image::OutputContext& context);
};


} // namespace zpr

#endif

// end of zprender/ColorMapKnob.h

//
// Copyright 2020 DreamWorks Animation
//
