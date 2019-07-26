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

/// @file Fuser/Attribute.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Attribute_h
#define Fuser_Attribute_h

#include "api.h"

#ifdef DWA_INTERNAL_BUILD
#  include <map>
#else
#  include <unordered_map>
#endif

namespace Fsr {


/*! Abstract Attribute base class.
    Use the TypedAttribute template (AttributeTypes.h) to implement specific
    attribute types.

    This is patterned after the simple OpenEXR attribute class and intended
    to store repeating (array) data, but not for time interpolation.

    Basically a little more robust version of the DD::Image::Attribute class
    but supporting the Fuser math classes.

    TODO: use a token system like Usd's TfToken to improve key lookup speed?
*/
class Attribute
{
  public:
    FSR_EXPORT Attribute() {}
    FSR_EXPORT virtual ~Attribute() {}


  public:
    //----------------------------------------------
    // Must implement these for each concrete type.
    //----------------------------------------------

    //! The attribute's type name, ie. 'float', 'floatlist', must implement.
    virtual const char* type() const=0;

    //! The attribute's base type name, ie. 'float', 'int', must implement.
    virtual const char* baseType() const=0;

    /*! Size of the base type in bytes (4 for float, 8 for double, etc,) must implement
        If applicable - non-POD types like std::string should return 0.
    */
    virtual uint32_t    baseSize() const=0;

    //! Number of elements in the base type (1 for string, 3 for Vec3, 16 for Mat4), must implement.
    virtual uint32_t    numBaseElements() const=0;

    //! Return true if the data type is a std::vector<>, must implement.
    virtual bool        isArray() const=0;

    //------------------------------------------------------

    //! Copy the attribute's contents, must implement.
    virtual Attribute*  duplicate() const=0;

    //! Copy the attribute's contents from another. This will fail if types don't match.
    virtual void        copy(const Attribute& b)=0;


    //----------------------------------------------
    // Type management. Type names are stored in a
    // static unsorted_map for quick retrieval.
    //----------------------------------------------

    //! Returns true if the named type already exists.
    FSR_EXPORT static bool       haveType(const char* type);

    //! Create a new Attribute instance of the named type, or NULL if type not recognized.
    FSR_EXPORT static Attribute* create(const char* type);


  protected:
    //! Add an attribute type instantiator.
    FSR_EXPORT static void      _addNewType(const char* type,
                                            Attribute*  (*create)());

};


#ifdef DWA_INTERNAL_BUILD
typedef std::map<std::string, Attribute*> AttributeMap;
#else
typedef std::unordered_map<std::string, Attribute*> AttributeMap;
#endif


} // namespace Fsr

#endif

// end of Fuser/Attribute.h

//
// Copyright 2019 DreamWorks Animation
//
