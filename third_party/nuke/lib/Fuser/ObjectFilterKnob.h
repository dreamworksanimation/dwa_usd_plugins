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

/// @file Fuser/ObjectFilterKnob.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ObjectFilterKnob_h
#define Fuser_ObjectFilterKnob_h

#include "api.h"

#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>


namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


class FSR_EXPORT ObjectFilter
{
  public:
    const char*              k_attrib;      //!< Object attribute name
    const char*              k_mask;        //!< Raw mask text as entered
    bool                     k_invert;      //!< Invert the match logic

    std::vector<std::string> m_mask_list;   //!< Extracted mask entries in application order
    std::set<unsigned>       m_index_set;   //!< Set of indices extracted from mask text
    bool                     m_do_all;      //!< Filter applies to all objects


  public:
    //!
    ObjectFilter();

    //!
    void set(const char* object_attrib,
             const char* mask_text,
             bool        invert=false)
    {
        k_attrib = object_attrib;
        k_mask   = mask_text;
        k_invert = invert;
    }


    //!
    bool state(bool v) const { return (k_invert) ? !v : v; }

    //! The filter affects all objects.
    bool all() const { return state(m_do_all); }

    //!
    void append(DD::Image::Hash& hash);


    //! Check attribute value against filter. Takes invert switch into account!
    bool matchObject(const DD::Image::GeoInfo& info) const;

    //! Return true if the string matches any token in the filter - NOT affected by invert state!
    bool globMatch(const char* s) const;

    //! Return true if index is in set - NOT affected by invert state!
    bool indexMatch(int32_t index) const { return (m_index_set.find(index) != m_index_set.end()); }


    //! Separate the list of masks and indices from a mask string in order of application.
    static void getMasks(const std::string&        mask_text,
                         std::vector<std::string>& mask_list,
                         std::set<unsigned>&       index_set,
                         bool&                     do_all);

};


/*! Knob construction/store callback 'macro' similar to the ones
    defined in Knobs.h. It declares a DD::Image::CUSTOM_KNOB
    enumeration and a DD::Image::Custom data type.
*/
DD::Image::Knob* ObjectFilter_knob(DD::Image::Knob_Callback f,
                                   ObjectFilter*            filter,
                                   const char*              name,
                                   const char*              label);


/*! \class Fsr::ObjectFilterKnob.

*/
class FSR_EXPORT ObjectFilterKnob : public DD::Image::Knob
{
  protected:
    DD::Image::Hash  m_filter_hash;         //!< Indicates when maska need recalcing
    DD::Image::Hash  m_geo_hash;            //!< Indicated when object attribs need updating
    const char*      m_knob_names[3];       //!<
    DD::Image::Knob* kObjectAttribString;   //!< Object attribute StringKnob
    DD::Image::Knob* kObjectAttributes;     //!< List of input object attributes
    DD::Image::Knob* kInvert;               //!<
    DD::Image::Knob* kMaskString;           //!< Mask StringKnob


  public:
    //! 
    ObjectFilterKnob(DD::Image::Knob_Closure* cb,
                     ObjectFilter*            filter,
                     const char*              name,
                     const char*              label);


    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    // From DD::Image::Knob:

    //!
    /*virtual*/ const char* Class() const { return "FsrObjectFilterKnob"; }

    //! Don't make an interface. TODO: this causes build problems w/Qt...
    ///*virtual*/ WidgetPointer make_widget(const DD::Image::WidgetContext&) { return NULL; }

    //! Don't do anything since the wrapper knob should never be written to a script file.
    /*virtual*/ bool from_script(const char*) { return true; }

    //! Do nothing since we're not a 'real' knob.
    /*virtual*/ void reset_to_default() {}

    //!
    /*virtual*/ void changed();

    //!
    /*virtual*/ void updateUI(const DD::Image::OutputContext&);

    //!
    /*virtual*/ void append(DD::Image::Hash&,
                            const DD::Image::OutputContext*);

    //! Stores 
    /*virtual*/ void store(DD::Image::StoreType            type,
                           void*                           p,
                           DD::Image::Hash&                hash,
                           const DD::Image::OutputContext& context);

};


} // namespace Fsr



#endif

// end of FuserObjectFilterKnob.h


//
// Copyright 2019 DreamWorks Animation
//
