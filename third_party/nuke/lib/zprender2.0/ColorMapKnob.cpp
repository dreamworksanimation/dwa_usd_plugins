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

/// @file zprender/ColorMapKnob.cpp
///
/// @author Jonathan Egstad


#include "ColorMapKnob.h"

#include <DDImage/Knobs.h>


using namespace DD::Image;

namespace zpr {


/*! The ctor should only get called when Knob_Closure has makeKnobs()==true.
    Ctor does not require a InputBinding data pointer since it dynamically
    builds one based the the sub knobs.
*/
ColorMapKnob::ColorMapKnob(DD::Image::Knob_Closure* cb,
                           uint32_t                 input_num,
                           uint32_t                 num_chans,
                           const char*              name,
                           const char*              label) :
    DD::Image::Knob(cb, name, label),
    k_enable(true),
    k_expr("map"),
    m_input(input_num)
{
    //std::cout << "    ColorMapKnob::ctor(" << this << ")" << std::endl;
    // We don't want the knob getting written into script files or being visible:
    this->set_flag(DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::INVISIBLE);

    for (uint32_t z=0; z < num_chans; ++z)
        k_map_chans[z] = DD::Image::Channel(DD::Image::Chan_Red + z);

    DD::Image::Knob_Callback f = *cb;

    // Build knob names:
    char buf[256];
    snprintf(buf, 256, "%s_enable", name); m_knob_names[0] = strdup(buf);
    snprintf(buf, 256, "%s_source", name); m_knob_names[1] = strdup(buf);
    snprintf(buf, 256, "%s_layer",  name); m_knob_names[2] = strdup(buf);

    Newline(f, label);
    kEnable = Bool_knob(f, &k_enable, m_knob_names[0], "");
    kExpr   = String_knob(f, &k_expr, m_knob_names[1], "");
        ClearFlags(f, Knob::STARTLINE);
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, "Text entry defining where to source the map value from.\n"
                   "Supports a limited set of keywords:\n"
                   "<b>Input arrow connection:</b>"
                         "<ul>"
                         "<li><i>map, (empty string)</i> - Sample a 2D texture input (or another shader)</li>"
                         "<li><i>map1### - Sample a 2D UDIM texture ('map1004'=u3,v0, 'map1021'=u0,v2)</li>"
                         "</ul>"
                   "<b>Set map value to a constant color with an alpha of 1:</b>"
                         "<ul>"
                         "<li>type in a color value - 1,2,3&4 vals supported. For 2 & 4 the last value is alpha.</li>"
                         "<li><i>white</i>  - 1</li>"
                         "<li><i>black</i>  - 0</li>"
                         "<li><i>grey, grey18</i> - 18% grey</li>"
                         "<li><i>grey50</i> - 50% grey</li>"
                         "<li><i>inf</i>    - infinity</li>"
                         "</ul>"
                   "<b>Hardcoded shading attributes:</b>"
                         "<ul>"
                         "<li><i>V</i>      - View-vector from surface point to camera origin (normalized)</li>"
                         "<li><i>Z</i>      - Ray depth (distance) from camera</li>"
                         //"<li><i>Zl</i>     - Linearly projected depth from camera</li>"
                         "<li><i>PW</i>     - Displaced shading point in world-space</li>"
                         "<li><i>dPWdx</i>  - PW x-derivative</li>"
                         "<li><i>dPWdy</i>  - PW y-derivative</li>"
                         "<li><i>PL</i>     - Shading point in local-space</li>"
                         "<li><i>PWg</i>    - Geometric surface point (no displacement)</li>"
                         "<li><i>st</i>     - Primitive's barycentric coordinates</li>"
                         "<li><i>dstdx</i>  - st x-derivative</li>"
                         "<li><i>dstdy</i>  - st y-derivative</li>"
                         "<li><i>N</i>      - Shading normal (interpolated & bumped vertex normal)</li>"
                         "<li><i>Nf</i>     - Face-forward shading normal</li>"
                         "<li><i>Ni</i>     - Interpolated surface normal</li>"
                         "<li><i>Ng</i>     - Geometric surface normal</li>"
                         "<li><i>dNdx</i>   - N x-derivative</li>"
                         "<li><i>dNdy</i>   - N y-derivative</li>"
                         "<li><i>UV</i>     - Surface texture coordinate</li>"
                         "<li><i>dUVdx</i>  - UV x-derivative</li>"
                         "<li><i>dUVdy</i>  - UV y-derivative</li>"
                         "<li><i>Cf</i>     - vertex color (stands for 'Color front')</li>"
                         "<li><i>dCfdx</i>  - Cf x-derivative</li>"
                         "<li><i>dCfdy</i>  - Cf y-derivative</li>"
                         "<li><i>t, time</i> - frame time</li>"
                         "</ul>"
                   "<b>Shading calculations:</b>"
                         "<ul>"
                         "<li><i>VdotN</i>  - Facing-ratio of shading normal</li>"
                         "<li><i>VdotNg</i> - Facing-ratio of geometric normal</li>"
                         "<li><i>VdotNf</i> - Facing-ratio of face-forward shading normal</li>"
                         "</ul>");

    kChans = Input_Channel_knob(f, k_map_chans, num_chans/*count*/, m_input/*input*/, m_knob_names[2], "layer:");
        ClearFlags(f, Knob::STARTLINE);
        SetFlags(f, Knob::NO_CHECKMARKS);
        if (num_chans < 4)
            SetFlags(f, Knob::NO_ALPHA_PULLDOWN);
        Tooltip(f, "Map source layer to use");
}


/*! Ctor used by InputOp_knob() method, where knob is automatically named.
*/
ColorMapKnob::ColorMapKnob(DD::Image::Knob_Closure* cb,
                           uint32_t                 input_num,
                           const char*              name) :
    DD::Image::Knob(cb, name, NULL/*label*/),
    k_enable(true),
    k_expr("input"),
    m_input(input_num),
    kEnable(NULL),
    kExpr(NULL),
    kChans(NULL)
{
    // We don't want the knob getting written into script files or being visible:
    this->set_flag(DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::INVISIBLE);

    for (uint32_t z=0; z < 4; ++z)
        k_map_chans[z] = DD::Image::Chan_Black;

    // Build knob names:
    char buf[256];
    snprintf(buf, 256, "%s_enable", name); m_knob_names[0] = strdup(buf);
    m_knob_names[1] = NULL;
    m_knob_names[2] = NULL;
}


/*!
*/
/*virtual*/ void
ColorMapKnob::append(DD::Image::Hash&                hash,
                     const DD::Image::OutputContext* context)
{
    //std::cout << "ColorMapKnob::append(" << this << ")" << std::endl;
}


/*!
*/
/*virtual*/ void
ColorMapKnob::store(DD::Image::StoreType            type,
                    void*                           p,
                    DD::Image::Hash&                hash,
                    const DD::Image::OutputContext& context)
{
    //std::cout << "ColorMapKnob::store(" << this << ")" << std::endl;
    InputBinding* binding = reinterpret_cast<InputBinding*>(p);
#if 1//DEBUG
    assert(binding);
    assert(type == DD::Image::Custom);
#endif

    // If the kEnable knob is NULL this is an InputOpKnob:
    if (!kEnable)
    {
        //-------------------------------
        // InputOp_knob store
        //-------------------------------

        // We always know the InputBinding is an Op input so we can skip
        // expression evaluation and update the binding type from the
        // input Op type:
        *binding = InputBinding::buildInputOpBinding(DD::Image::Knob::op()->node_input(m_input, DD::Image::Op::INPUT_OP, &context));
    }
    else
    {
        //-------------------------------
        // ColorMap_knob store
        //-------------------------------
        kEnable->store(DD::Image::BoolPtr, &k_enable, hash, context);
        if (!k_enable)
        {
            // Not enabled, disable the binding:
            *binding = InputBinding(InputBinding::NONE);
        }
        else
        {
#if DEBUG
            assert(kExpr);
            assert(kChans);
#endif
            kExpr->store(DD::Image::StringPtr, &k_expr, hash, context);
            kChans->store(DD::Image::ChannelPtr, k_map_chans, hash, context);

            bool enable_channel_pulldowns = false;
            // Did the expression text possibly change? Since there's little chance
            // or the view or frame changing the expression we don't need to
            // constantly rebuild it:
            DD::Image::Hash expr_hash;
            expr_hash.append(k_enable);
            expr_hash.append(k_expr);
            expr_hash.append(k_map_chans[0]);
            expr_hash.append(k_map_chans[1]);
            expr_hash.append(k_map_chans[2]);
            expr_hash.append(k_map_chans[3]);
            if (expr_hash != m_expr_hash)
            {
                m_expr_hash = expr_hash;
                m_expr_binding = InputBinding::buildFromBindExpression(k_expr);
                //std::cout << "  m_expr_binding" << m_expr_binding << std::endl;
            }
            InputBinding input_binding = m_expr_binding;

            if (input_binding.isNukeOp())
            {
                // Check for a valid input on the Op that owns this knob:
                DD::Image::Op* input_op = DD::Image::Knob::op()->node_input(m_input, DD::Image::Op::INPUT_OP, &context);
                //std::cout << "  '" << k_expr << "', input_op=" << input_op << std::endl;
                if (input_op)
                {
                    // Update the binding type from the Op:
                    input_binding = InputBinding::buildInputOpBinding(input_op);
                    //std::cout << "  ColorMap:'" << k_expr << "', type=" << input_binding.type << std::endl;
                }

                if (input_binding.isMaterialIop())
                {
                    // If a DD::Image::Material assign the base channels:
                    *binding = input_binding;
                    binding->setActiveChannels(DD::Image::Chan_Red,
                                               DD::Image::Chan_Green,
                                               DD::Image::Chan_Blue,
                                               DD::Image::Chan_Alpha);

                }
                else if (input_binding.isTextureIop())
                {
                    // If it's an Iop let's get the channel info:
                    *binding = InputBinding::buildInputTextureBinding(input_binding.asTextureIop(),
                                                                      k_map_chans[0],
                                                                      k_map_chans[1],
                                                                      k_map_chans[2],
                                                                      k_map_chans[3]);
                    // Copy user-entered tile info from expr binding:
                    binding->uv_tile_offset = m_expr_binding.uv_tile_offset;

                    //std::cout << "  ColorMap: op=" << input_op << ", texture=" << binding->isTextureIop() << ", channels=" << binding->getChannels() << std::endl;
                    enable_channel_pulldowns = true;
                }
                else
                    *binding = input_binding;
            }
            else
                *binding = input_binding;

            kChans->enable(enable_channel_pulldowns);
        }
    }
    //std::cout << "ColorMapKnob::store(" << this << ") binding=" << binding << *binding << std::endl;

}


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.
*/
DD::Image::Knob*
ColorMap_knob(DD::Image::Knob_Callback f,
              InputBinding*            binding,
              uint32_t                 input_num,
              uint32_t                 num_chans,
              const char*              name,
              const char*              label)
{
    // TODO: no idea if this bool is needed, it matches the logic in the the CustomKnob macros.
    // This is false if the knob will be filtered out by name (used only for custom knobs.)
    const bool filter_name = f.filter(name);

    DD::Image::Knob* k = NULL;
    if (f.makeKnobs() && filter_name)
    {
        DD::Image::Knob* kColorMapKnob = new ColorMapKnob(&f, input_num, num_chans, name, label);

        // Create the ColorMap wrapper knob:
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              NULL/*data*/,
              name,
              label,
              kColorMapKnob/*extra*/);
        //std::cout << "  ColorMap_knob(" << name << ")::Knob_Callback:makeKnobs, knob=" << k << std::endl;
    }
    else
    {
        // Store the knob. This callback calls the store() method
        // below which in turn calls getMatrixAt() at the correct
        // OutputContext and fills in 'matrix'. It should return
        // the same knob pointer created above for the same Op.
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              binding,
              name,
              label,
              NULL/*extra*/);
        //std::cout << "  ColorMap_knob(" << name << ")::Knob_Callback:store, knob=" << k << std::endl;
    }
#if DEBUG
    assert(k);
#endif
    return k;
}


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.
*/
DD::Image::Knob*
InputOp_knob(DD::Image::Knob_Callback f,
             InputBinding*            binding,
             uint32_t                 input_num)
{
    // Build knob name from input_num:
    char name[32];
    snprintf(name, 256, "inputop%d", input_num);

    // TODO: no idea if this bool is needed, it matches the logic in the the CustomKnob macros.
    // This is false if the knob will be filtered out by name (used only for custom knobs.)
    const bool filter_name = f.filter(name);

    DD::Image::Knob* k = NULL;
    if (f.makeKnobs() && filter_name)
    {
        DD::Image::Knob* kInputOpKnob = new ColorMapKnob(&f, input_num, name);

        // Create the ColorMap wrapper knob:
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              NULL/*data*/,
              name,
              NULL/*label*/,
              kInputOpKnob/*extra*/);
        //std::cout << "  InputOp_knob(" << name << ")::Knob_Callback:makeKnobs, knob=" << k << std::endl;
    }
    else
    {
        // Store the knob. This callback calls the store() method
        // below which in turn calls getMatrixAt() at the correct
        // OutputContext and fills in 'matrix'. It should return
        // the same knob pointer created above for the same Op.
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              binding,
              name,
              NULL/*label*/,
              NULL/*extra*/);
        //std::cout << "  InputOp_knob(" << name << ")::Knob_Callback:store, knob=" << k << std::endl;
    }
#if DEBUG
    assert(k);
#endif
    return k;
}


} // namespace zpr

// end of zprender/ColorMapKnob.cpp

//
// Copyright 2020 DreamWorks Animation
//
