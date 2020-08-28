//
// Copyright 2019 DreamWorks Animation
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

/// @file Fuser/ObjectFilterKnob.cpp
///
/// @author Jonathan Egstad

#include "ObjectFilterKnob.h"
#include "NukeKnobInterface.h"

#include <DDImage/Application.h>
#include <DDImage/Enumeration_KnobI.h>
#include <DDImage/GeoOp.h>
#include <DDImage/Scene.h>
#include <DDImage/ViewerContext.h>


namespace Fsr {


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


ObjectFilter::ObjectFilter() :
    k_attrib("name"),
    k_mask("*"),
    k_invert(false),
    m_do_all(true)
{
    //
}


/*!
*/
void
ObjectFilter::append(DD::Image::Hash& hash)
{
    hash.append(k_attrib);
    hash.append(k_mask);
    hash.append(k_invert);
}


/*! Check attribute value against filter.
    Takes invert switch into account!
*/
bool
ObjectFilter::matchObject(const DD::Image::GeoInfo& info) const
{
    //std::cout << "    ObjectFilter::matchObject() k_attrib='" << k_attrib << "'" << std::endl;
    if (m_do_all)
        return state(true);

    const DD::Image::Attribute* attrib = info.get_group_attribute(DD::Image::Group_Object, k_attrib);
    if (!attrib || attrib->size() == 0)
        return state(false); // can't eval attrib

    // Does attrib value match filter:
    bool match = false;
    if      (attrib->type() == DD::Image::INT_ATTRIB       ) match = this->indexMatch(attrib->integer(0));
    else if (attrib->type() == DD::Image::STRING_ATTRIB    ) match = this->globMatch(attrib->string(0));
    else if (attrib->type() == DD::Image::STD_STRING_ATTRIB) match = this->globMatch(attrib->stdstring(0).c_str());

    //std::cout << "      match=" << match << std::endl;
    return state(match);
}


/*! Does the string match any token in the filter - NOT affected by invert state!
*/
bool
ObjectFilter::globMatch(const char* s) const
{
    if (!s || !s[0])
        return false;

    // Apply in order so the last mask takes precedence:
    bool match = false;
    for (size_t i=0; i < m_mask_list.size(); ++i)
    {
        const std::string& mask = m_mask_list[i];
        if ((mask[0] == '-' || mask[0] == '^') && Fsr::globMatch(mask.c_str()+1, s))
            match = false;
        else if (mask[0] == '+' && Fsr::globMatch(mask.c_str()+1, s))
            match = true;
        else if (Fsr::globMatch(mask.c_str(), s))
            match = true;
    }
    return match;
}


/*! Get the list of masks and/or indices from input string.

    If masks are glob-style text wildcards then they will be split
    and added to the mask_list.

    If mask text is a series of index numbers then index_set will
    be filled in.
    For example the string:
        '1-9, 30-35, +20 21 -6 -32'
    results in the index set:
        1 2 3 4 5 6 7 8 9 20 21 30 31 33 34 35

    If there's a '*' in the mask then the filer applies to all
    objects, otherwise an empty mask '' results in a filter
    that applies to no objects.
*/
/*static*/
void
ObjectFilter::getMasks(const std::string&        mask_text,
                       std::vector<std::string>& mask_list,
                       std::set<unsigned>&       index_set,
                       bool&                     do_all)
{
    //std::cout << "    ObjectFilter::getMasks() mask_text='" << mask_text << "'" << std::endl;
    mask_list.clear();
    index_set.clear();
    do_all = false;
    if (mask_text.empty())
        return;

    stringSplit(mask_text, ", \t\r\n", mask_list);
    if (mask_list.size() == 0)
        return;

    // Scan first for any asterisks:
    const size_t nTokens = mask_list.size();
    for (size_t j=0; j < nTokens; ++j)
    {
        std::string& token = mask_list[j];
        stringTrim(token);
        if (token == "*")
            do_all = true;
    }

    for (size_t j=0; j < nTokens; ++j)
    {
        const std::string& token = mask_list[j];
        if (token.size() == 0)
            continue; // shouldn't happen...
        if (token == "*")
            continue; // already checked

        // Note the state arg and skip the character:
        size_t a = 0;
        int32_t state = 1;
        if (token[0] == '-' || token[0] == '^')
        {
            state = -1;
            ++a;
        }
        else if (token[0] == '+')
            ++a;

        int32_t start, end;
        if (::sscanf(token.c_str()+a, "%d-%d", &start, &end) == 2)
        {
            // Number range
        }
        else if (::sscanf(token.c_str()+a, "%d", &start) == 1)
        {
            // Single number
            end = start;
        }
        else
        {
            // String arg
            if (state == -1)
                do_all = false;
            continue;
        }

        // Add or remove each index:
        for (int32_t i=start; i <= end; ++i)
        {
            if (state == -1)
            {
                const std::set<unsigned>::const_iterator iter = index_set.find(i);
                if (iter != index_set.end())
                   index_set.erase(iter);
            }
            else
                index_set.insert(i);
        }

        // TODO: the do_all logic fails when removing indices, likely need another
        // 'remove_index_set' in addition to index_set.
        if (state == -1)
            do_all = false;
    }
    //std::cout << "do_all=" << do_all << " size=" << index_set.size() << std::endl;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


//! The extra spaces helps set the initial width of the enumeration knob. */
static const char* initial_attrib_names[] = { "  <Select Object Attribute>  ", 0 };


/*!
*/
ObjectFilterKnob::ObjectFilterKnob(DD::Image::Knob_Closure* cb,
                                   ObjectFilter*            filter,
                                   const char*              name,
                                   const char*              label) :
    DD::Image::Knob(cb, name, label)
{
    //std::cout << "    ObjectFilterKnob::ctor(" << this << ")" << std::endl;
    // We don't want the wrapper knob getting written into script files or being visible:
    this->set_flag(DD::Image::Knob::DO_NOT_WRITE |
                   DD::Image::Knob::INVISIBLE |
                   DD::Image::Knob::KNOB_CHANGED_ALWAYS);

    const char* k_attrib = (filter) ? filter->k_attrib : "name";
    const char* k_mask   = (filter) ? filter->k_mask   : "*";
    bool        k_invert = (filter) ? filter->k_invert : false;

    // Build knob names:
    char buf[256];
    snprintf(buf, 256, "%s_attrib", name); m_knob_names[0] = strdup(buf);
    snprintf(buf, 256, "%s_invert", name); m_knob_names[1] = strdup(buf);
    snprintf(buf, 256, "%s_mask",   name); m_knob_names[2] = strdup(buf);

    DD::Image::Knob_Callback f = *cb;

    DD::Image::Newline(f, label);
        DD::Image::SetFlags(f,DD::Image:: Knob::STARTLINE);
    kObjectAttribString = DD::Image::String_knob(f, &k_attrib, m_knob_names[0], "attribute");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_MULTIVIEW);
        DD::Image::Tooltip(f, "Object attribute to apply mask filter to.\n"
                              "\n"
                              "'name' or 'scene:path' can be used for objects loaded through the Fuser "
                              "readers (usd, abc, etc)\n"
                              "\n"
                              "'name', 'model', or 'id' may work depending on the behavior of the stock "
                              "Nuke geometry readers.\n"
                              "");
    int dummy_int=0;
    kObjectAttributes = DD::Image::Enumeration_knob(f, &dummy_int, initial_attrib_names, "input_attributes", "");
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE/* | DD::Image::Knob::DISABLED*/);
        DD::Image::Tooltip(f, "Select an attribute name from the available input geometry object attributes.\n"
                              "\n"
                              "This only shows available string or integer attributes, other types \n"
                              "are filtered out.");


    DD::Image::Newline(f);
    kMaskString = DD::Image::String_knob(f, &k_mask, m_knob_names[2], "mask");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_MULTIVIEW);
        DD::Image::Tooltip(f, "Mask string applied to object attribute value.  This is modal "
                              "depending on object attribute type:\n"
                              "\n"
                              "<b>String attribute</b>:\n"
                              "  Do text wildcard matching supporting the '?' "
                              "and '*' character.  For example '*_hand' will match 'left_hand' and "
                              "'right_hand' while 'bolt1?' will match 'bolt10' and 'bolt11', but "
                              "not 'bolt20'.\n"
                              "\n"
                              "<b>Integer attribute</b>:\n"
                              "  Do numerical range handling with the syntax: n-m, n -m\n"
                              "where a '-' in front of the number means to remove that index from the set.\n"
                              "ex '1-9 -6 30-35 20 21 -32' which results in the point list:\n"
                              "1,2,3,4,5,7,8,9,20,21,30,31,33,34,35\n"
                              "\n"
                              "Turn off objects by preceding the pattern with '-' or '^'. Priority order "
                              "is left to right so if an object is turned off by one mask it can be turned "
                              "on again by an additional mask to the right.\n");
    kInvert = DD::Image::Bool_knob(f, &k_invert, m_knob_names[1], "invert");
        DD::Image::ClearFlags(f,DD::Image:: Knob::STARTLINE);
}


/*!
*/
/*virtual*/ void
ObjectFilterKnob::append(DD::Image::Hash&                hash,
                         const DD::Image::OutputContext* context)
{
    //std::cout << "ObjectFilterKnob::append(" << this << ")" << std::endl;
}


/*! Stores the transform into a double-precision Fuser Mat4d.
*/
/*virtual*/ void
ObjectFilterKnob::store(DD::Image::StoreType            type,
                        void*                           p,
                        DD::Image::Hash&                hash,
                        const DD::Image::OutputContext& context)
{
    //std::cout << "    ObjectFilterKnob::store(" << this << ")";
    ObjectFilter* filter = reinterpret_cast<ObjectFilter*>(p);
#if 1//DEBUG
    assert(filter);
    assert(type == DD::Image::Custom);
#endif

#if DEBUG
    assert(kObjectAttribString);
    assert(kObjectAttributes);
    assert(kInvert);
    assert(kMaskString);
#endif

    // First check if the object attribute selection has been changed by
    // the user to something other that the first entry:
    if ((int32_t)kObjectAttributes->get_value() > 0)
    {
        const std::string attrib_name = kObjectAttributes->enumerationKnob()->getSelectedItemString();
        if (attrib_name != "<none>")
        {
            kObjectAttribString->set_text(attrib_name.c_str());
            kObjectAttribString->changed();
        }
        kObjectAttributes->set_value(0);
    }

    DD::Image::Hash filter_hash;
    kObjectAttribString->store(DD::Image::StringPtr, &filter->k_attrib, filter_hash, context);
    kMaskString->store(        DD::Image::StringPtr, &filter->k_mask,   filter_hash, context);
    kInvert->store(            DD::Image::BoolPtr,   &filter->k_invert, filter_hash, context);

    if (filter_hash != m_filter_hash)
    {
        m_filter_hash = filter_hash;
        filter->getMasks(filter->k_mask,
                         filter->m_mask_list,
                         filter->m_index_set,
                         filter->m_do_all);
    }
    //std::cout << " filter_hash=" << std::hex << filter_hash.value() << std::dec;
    //std::cout << std::endl;

    hash.append(filter_hash);
}


//!
/*virtual*/ void
ObjectFilterKnob::changed()
{
    //std::cout << "    ObjectFilterKnob::changed(" << this << ")" << std::endl;
}


//!
/*virtual*/ void
ObjectFilterKnob::updateUI(const DD::Image::OutputContext& context)
{
    //std::cout << "    ObjectFilterKnob::updateUI(" << this << ")" << std::endl;
    if (!DD::Image::Application::gui)
        return;

    std::set<std::string> attrib_names;
    DD::Image::GeoOp* geo = dynamic_cast<DD::Image::GeoOp*>(op());
    if (geo)
    {
        // Assume GeoOp owner is getting its geometry from input 0:
        // TODO: expose an input arg to knob
        DD::Image::GeoOp* input_geo = dynamic_cast<DD::Image::GeoOp*>(geo->node_input(0/*input*/, DD::Image::Op::INPUT_OP, &context));
        if (!input_geo || input_geo->Op::hash() == m_geo_hash)
        {
            DD::Image::Knob::updateUI(context);
            return; // don't change menu
        }
        m_geo_hash = input_geo->Op::hash();

        input_geo->setupScene();
        DD::Image::Scene* scene = input_geo->scene();
        const unsigned nObjects = scene->objects();
        //std::cout << "      nObjects=" << nObjects << std::endl;

        for (unsigned obj=0; obj < nObjects; ++obj)
        {
            const DD::Image::GeoInfo& info = scene->object(obj);

            const unsigned nAttribs = info.get_attribcontext_count();
            for (unsigned i=0; i < nAttribs; ++i)
            {
                assert(info.get_attribcontext(i));
                const DD::Image::AttribContext& attrib = *info.get_attribcontext(i);
                if (attrib.group == DD::Image::Group_Object && !attrib.empty())
                    attrib_names.insert(attrib.name);
            }

        }
    }

#if DEBUG
    assert(kObjectAttributes && kObjectAttributes->enumerationKnob());
#endif

    std::vector<std::string> menu_entries;
    menu_entries.reserve(attrib_names.size()+2);

    menu_entries.push_back(initial_attrib_names[0]);
    if (attrib_names.size() == 0)
        menu_entries.push_back("<none>");
    else
    {
        for (std::set<std::string>::iterator it=attrib_names.begin(); it != attrib_names.end(); ++it)
            menu_entries.push_back(*it);
    }

    kObjectAttributes->enumerationKnob()->menu(menu_entries);
    kObjectAttributes->set_value(0);

    DD::Image::Knob::updateUI(context);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.
*/
DD::Image::Knob*
ObjectFilter_knob(DD::Image::Knob_Callback f,
                  ObjectFilter*            filter,
                  const char*              name,
                  const char*              label)
{
    // TODO: no idea if this bool is needed, it matches the logic in the the CustomKnob macros.
    // This is false if the knob will be filtered out by name (used only for custom knobs.)
    const bool filter_name = f.filter(name);

    DD::Image::Knob* k = NULL;
    if (f.makeKnobs() && filter_name)
    {
        // Create the ObjectFilterKnob wrapper knob:
        DD::Image::Knob* wrapper_knob = new ObjectFilterKnob(&f, filter, name, label);
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              NULL/*data*/,
              name,
              label,
              wrapper_knob/*extra*/);
        //std::cout << "  ObjectFilter_knob(" << name << ")::Knob_Callback:makeKnobs, knob=" << k << std::endl;
    }
    else
    {
        // Store the knob. This callback calls the store() method
        // below which in turn calls getMatrixAt() at the correct
        // OutputContext and fills in 'matrix'. It should return
        // the same knob pointer created above for the same Op.
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              filter,
              name,
              label,
              NULL/*extra*/);
        //std::cout << "  ObjectFilter_knob(" << name << ")::Knob_Callback:store, knob=" << k << std::endl;
    }
#if DEBUG
    assert(k);
#endif
    return k;
}



} // namespace Fsr


// end of Fuser/ObjectFilterKnob.cpp


//
// Copyright 2019 DreamWorks Animation
//
