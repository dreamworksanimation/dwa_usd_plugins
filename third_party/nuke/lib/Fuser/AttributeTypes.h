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

/// @file Fuser/AttributeTypes.h
///
/// @author Jonathan Egstad

#ifndef Fuser_AttributeTypes_h
#define Fuser_AttributeTypes_h

#include "Attribute.h"
#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"
#include "Box2.h"
#include "Box3.h"
#include "Mat4.h"


namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


typedef std::vector<bool>        BoolList;
typedef std::vector<int32_t>     Int32List;
typedef std::vector<int64_t>     Int64List;
typedef std::vector<uint32_t>    Uint32List;
typedef std::vector<uint64_t>    Uint64List;
typedef std::vector<float>       FloatList;
typedef std::vector<double>      DoubleList;
typedef std::vector<uint64_t>    HashList;
typedef std::vector<std::string> StringList;
//
typedef std::vector<Fsr::Vec2f>  Vec2fList;
typedef std::vector<Fsr::Vec2d>  Vec2dList;
typedef std::vector<Fsr::Vec2i>  Vec2iList;
//
typedef std::vector<Fsr::Vec3f>  Vec3fList;
typedef std::vector<Fsr::Vec3d>  Vec3dList;
typedef std::vector<Fsr::Vec3i>  Vec3iList;
//
typedef std::vector<Fsr::Vec4f>  Vec4fList;
typedef std::vector<Fsr::Vec4d>  Vec4dList;
typedef std::vector<Fsr::Vec4i>  Vec4iList;
//
typedef std::vector<Fsr::Box2f>  Box2fList;
typedef std::vector<Fsr::Box2d>  Box2dList;
typedef std::vector<Fsr::Box2i>  Box2iList;
//
typedef std::vector<Fsr::Box3f>  Box3fList;
typedef std::vector<Fsr::Box3d>  Box3dList;
typedef std::vector<Fsr::Box3i>  Box3iList;
//
typedef std::vector<Fsr::Mat4f>  Mat4fList;
typedef std::vector<Fsr::Mat4d>  Mat4dList;


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Template to implement the concrete Attribute types.
    This is patterned after the OpenEXR attribute class.
*/
template <class T>
class TypedAttribute : public Attribute
{
  protected:
    T   m_value;


  protected:
    //----------------------------------------------
    // Implement these for each concrete type.
    //----------------------------------------------

    //! Static version of type(), specialized for the template type.
    static const char* _type();

    //! The attribute's base type name, ie. 'float', 'int', specialized for the template type.
    static  const char* _baseType();

    //! Size of the base type in bytes, specialized for the template type.
    static  uint32_t    _baseSize();

    //! Static version of numElements(), specialized for the template type.
    static uint32_t     _numBaseElements();

    //! Return true if the data type is a std::vector<>, specialized for the template type.
    static  bool        _isArray();

    //------------------------------------------------------

    //! Create a new instance of the specialize type.
    static Attribute* _create();

    //! Add an attribute type instantiator.
    static void       _addNewType();


  public:
    //! Default ctor should leave junk in contents.
    TypedAttribute();

    //! Copy ctors.
    TypedAttribute(const T& v);
    TypedAttribute(const TypedAttribute<T>& b);


    //! Read/write access to value, specialized for the template type.
    T&       val();
    const T& val() const;

    // Pointer to start of attribute data.
    T*       array();
    const T* array() const;


    //----------------------------------------------
    // Must implement these for each concrete type.
    //----------------------------------------------

    //! The attribute's type name, ie. 'float', 'floatlist' - calls the static specialization.
    /*virtual*/ const char* type() const { return _type(); }

    //! The attribute's base type name, ie. 'float', 'int' - calls the static specialization.
    /*virtual*/ const char* baseType() const { return _baseType(); }

    //! Size of the base type in bytes - calls the static specialization.
    /*virtual*/ uint32_t    baseSize() const { return _baseSize(); }

    //! Number of elements in the base type (1 for string, 3 for Vec3, 16 for Mat4) - calls the static specialization.
    /*virtual*/ uint32_t    numBaseElements() const { return _numBaseElements(); }

    //! Return true if the data type is a std::vector<> - calls the static specialization.
    /*virtual*/ bool        isArray() const { return _isArray(); }

    //------------------------------------------------------

    //! Copy the attribute's contents, specialized for the template type.
    /*virtual*/ Attribute*  duplicate() const;

    //! Copy the attribute's contents from another, specialized for the template type.
    /*virtual*/ void        copy(const Attribute& b);



    //! Attempt to cast the attribute to another type, otherwise return NULL.
    static       TypedAttribute* cast(      Attribute* b);
    static       TypedAttribute& cast(      Attribute& b);
    static const TypedAttribute* cast(const Attribute* b);
    static const TypedAttribute& cast(const Attribute& b);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

typedef TypedAttribute<bool>            BoolAttribute;
typedef TypedAttribute<int32_t>         IntAttribute;
typedef TypedAttribute<float>           FloatAttribute;
typedef TypedAttribute<double>          DoubleAttribute;
typedef TypedAttribute<uint64_t>        HashAttribute;
typedef TypedAttribute<std::string>     StringAttribute;
//
typedef TypedAttribute<Fsr::Vec2f>      Vec2fAttribute;
typedef TypedAttribute<Fsr::Vec2d>      Vec2dAttribute;
typedef TypedAttribute<Fsr::Vec2i>      Vec2iAttribute;
//
typedef TypedAttribute<Fsr::Vec3f>      Vec3fAttribute;
typedef TypedAttribute<Fsr::Vec3d>      Vec3dAttribute;
typedef TypedAttribute<Fsr::Vec3i>      Vec3iAttribute;
//
typedef TypedAttribute<Fsr::Vec4f>      Vec4fAttribute;
typedef TypedAttribute<Fsr::Vec4d>      Vec4dAttribute;
typedef TypedAttribute<Fsr::Vec4i>      Vec4iAttribute;
//
typedef TypedAttribute<Fsr::Box2f>      Box2fAttribute;
typedef TypedAttribute<Fsr::Box2d>      Box2dAttribute;
typedef TypedAttribute<Fsr::Box2i>      Box2iAttribute;
//
typedef TypedAttribute<Fsr::Box3f>      Box3fAttribute;
typedef TypedAttribute<Fsr::Box3d>      Box3dAttribute;
typedef TypedAttribute<Fsr::Box3i>      Box3iAttribute;
//
typedef TypedAttribute<Fsr::Mat4f>      Mat4fAttribute;
typedef TypedAttribute<Fsr::Mat4d>      Mat4dAttribute;

//-------------------------------------------------------------------------

typedef TypedAttribute<Fsr::BoolList>   BoolListAttribute;
typedef TypedAttribute<Fsr::Int32List>  IntListAttribute;
typedef TypedAttribute<Fsr::FloatList>  FloatListAttribute;
typedef TypedAttribute<Fsr::DoubleList> DoubleListAttribute;
typedef TypedAttribute<Fsr::HashList>   HashListAttribute;
typedef TypedAttribute<Fsr::StringList> StringListAttribute;
//
typedef TypedAttribute<Fsr::Vec2fList>  Vec2fListAttribute;
typedef TypedAttribute<Fsr::Vec2dList>  Vec2dListAttribute;
typedef TypedAttribute<Fsr::Vec2iList>  Vec2iListAttribute;
//
typedef TypedAttribute<Fsr::Vec3fList>  Vec3fListAttribute;
typedef TypedAttribute<Fsr::Vec3dList>  Vec3dListAttribute;
typedef TypedAttribute<Fsr::Vec3iList>  Vec3iListAttribute;
//
typedef TypedAttribute<Fsr::Vec4fList>  Vec4fListAttribute;
typedef TypedAttribute<Fsr::Vec4dList>  Vec4dListAttribute;
typedef TypedAttribute<Fsr::Vec4iList>  Vec4iListAttribute;
//
typedef TypedAttribute<Fsr::Box2fList>  Box2fListAttribute;
typedef TypedAttribute<Fsr::Box2dList>  Box2dListAttribute;
typedef TypedAttribute<Fsr::Box2iList>  Box2iListAttribute;
//
typedef TypedAttribute<Fsr::Box3fList>  Box3fListAttribute;
typedef TypedAttribute<Fsr::Box3dList>  Box3dListAttribute;
typedef TypedAttribute<Fsr::Box3iList>  Box3iListAttribute;
//
typedef TypedAttribute<Fsr::Mat4fList>  Mat4fListAttribute;
typedef TypedAttribute<Fsr::Mat4dList>  Mat4dListAttribute;


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template <class T>
inline TypedAttribute<T>::TypedAttribute() : Attribute(), m_value(T()) {}
template <class T>
inline TypedAttribute<T>::TypedAttribute(const T& b) : Attribute(), m_value(b) {}
template <class T>
inline TypedAttribute<T>::TypedAttribute(const TypedAttribute<T>& b) : Attribute(b) { copy(b); }

template <class T>
inline       T& TypedAttribute<T>::val()       { return m_value; }
template <class T>
inline const T& TypedAttribute<T>::val() const { return m_value; }

template <class T>
inline       T* TypedAttribute<T>::array()       { return &m_value; }
template <class T>
inline const T* TypedAttribute<T>::array() const { return &m_value; }

template <class T>
inline /*virtual*/ Attribute* TypedAttribute<T>::duplicate() const
{
    Attribute* attribute = new TypedAttribute<T>();
    attribute->copy(*this);
    return attribute;
}
template <class T>
inline /*virtual*/ void TypedAttribute<T>::copy(const Attribute& b) { m_value = cast(b).m_value; }

template <class T>
inline Attribute* TypedAttribute<T>::_create() { return new TypedAttribute<T>(); }
template <class T>
inline void TypedAttribute<T>::_addNewType() { Attribute::_addNewType(_type(), _create); }

template <class T>
inline       TypedAttribute<T>* TypedAttribute<T>::cast(      Attribute* b)
{
    return dynamic_cast<TypedAttribute<T>* >(b);
}
template <class T>
inline const TypedAttribute<T>* TypedAttribute<T>::cast(const Attribute* b)
{
    return dynamic_cast<const TypedAttribute<T>* >(b);
}
template <class T>
inline       TypedAttribute<T>& TypedAttribute<T>::cast(      Attribute& b) { return *cast(&b); }
template <class T>
inline const TypedAttribute<T>& TypedAttribute<T>::cast(const Attribute& b) { return *cast(&b); }

//-------------------------------------------------------------------------

template <> FSR_EXPORT /*static*/ const char* BoolAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* BoolAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    BoolAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    BoolAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        BoolAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* IntAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* IntAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    IntAttribute::_baseSize() ;
template <> FSR_EXPORT /*static*/ uint32_t    IntAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        IntAttribute::_isArray() ;
//
template <> FSR_EXPORT /*static*/ const char* FloatAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* FloatAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    FloatAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    FloatAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        FloatAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* DoubleAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* DoubleAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    DoubleAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    DoubleAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        DoubleAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* StringAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* StringAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    StringAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    StringAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        StringAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec2fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec2dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2dAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec2iAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2iAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2iAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2iAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2iAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec3fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec3dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3dAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec3iAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3iAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3iAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3iAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3iAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec4fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec4dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4dAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec4iAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4iAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4iAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4iAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4iAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Box2fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box2dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2dAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box2iAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2iAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2iAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2iAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2iAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Box3fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box3dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3dAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box3iAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3iAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3iAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3iAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3iAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Mat4fAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Mat4fAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4fAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4fAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Mat4fAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Mat4dAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Mat4dAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4dAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4dAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Mat4dAttribute::_isArray();

//-------------------------------------------------------------------------

template <> FSR_EXPORT /*static*/ const char* BoolListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* BoolListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    BoolListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    BoolListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        BoolListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* IntListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* IntListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    IntListAttribute::_baseSize() ;
template <> FSR_EXPORT /*static*/ uint32_t    IntListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        IntListAttribute::_isArray() ;
//
template <> FSR_EXPORT /*static*/ const char* FloatListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* FloatListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    FloatListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    FloatListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        FloatListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* DoubleListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* DoubleListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    DoubleListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    DoubleListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        DoubleListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* StringListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* StringListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    StringListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    StringListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        StringListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec2fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec2dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2dListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec2iListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec2iListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2iListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec2iListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec2iListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec3fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec3dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3dListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec3iListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec3iListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3iListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec3iListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec3iListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Vec4fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec4dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4dListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Vec4iListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Vec4iListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4iListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Vec4iListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Vec4iListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Box2fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box2dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2dListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box2iListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box2iListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box2iListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box2iListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box2iListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Box3fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box3dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3dListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Box3iListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Box3iListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Box3iListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Box3iListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Box3iListAttribute::_isArray();
//--------------------------------------
template <> FSR_EXPORT /*static*/ const char* Mat4fListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Mat4fListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4fListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4fListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Mat4fListAttribute::_isArray();
//
template <> FSR_EXPORT /*static*/ const char* Mat4dListAttribute::_type();
template <> FSR_EXPORT /*static*/ const char* Mat4dListAttribute::_baseType();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4dListAttribute::_baseSize();
template <> FSR_EXPORT /*static*/ uint32_t    Mat4dListAttribute::_numBaseElements();
template <> FSR_EXPORT /*static*/ bool        Mat4dListAttribute::_isArray();


} // namespace Fsr

#endif

// end of Fuser/AttributeTypes.h

//
// Copyright 2019 DreamWorks Animation
//
