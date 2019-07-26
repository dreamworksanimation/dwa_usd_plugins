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

/// @file Fuser/ArgSet.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ArgSet_h
#define Fuser_ArgSet_h

#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"
#include "Mat4.h"

#include <string>
#ifndef DWA_INTERNAL_BUILD
#  include <unordered_map>
#  include <unordered_set>
#endif
#include <map>
#include <set>

namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// Define some common-usage types:

#ifdef DWA_INTERNAL_BUILD
typedef std::map<std::string, std::string> KeyValueMap;
typedef std::set<std::string>              StringSet;
#else
// These are unordered to improve lookup speed at the cost of alphabetizing.
typedef std::unordered_map<std::string, std::string> KeyValueMap;
typedef std::unordered_set<std::string>              StringSet;
#endif

// Sorted variants:
typedef std::map<std::string, std::string> KeyValueSortedMap;
typedef std::set<std::string>              StringSortedSet;


extern FSR_EXPORT std::string empty_string;

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Convenience wrapper class around a KeyValueMap providing
    argument get/set access methods.

    Warning - this is a low-performance implementation!
    It does naive string conversions (no value testing) to/from
    numeric values so this should only be used for low-performance
    purposes such as passing argument lists.

    TODO: use a token system like Usd's TfToken to improve key lookup speed?
    TODO: support 'real' arg types that are more performant...?
*/
class FSR_EXPORT ArgSet
{
  public:
    //! Default ctor in an empty set
    ArgSet();
    //! Copy ctors.
    ArgSet(const ArgSet&);
    ArgSet(const KeyValueMap&);


  public:
    //! Raw read access to private KeyValueMap (std::unordered_map<std::string, std::string>)
    const KeyValueMap& args() const { return m_args; }

    //! Replaces the contents with another ArgSet or KeyValueMap.
    ArgSet& operator= (const ArgSet&);
    ArgSet& operator= (const KeyValueMap&);

    //! Number of args in set
    size_t size()     const { return m_args.size(); }
    size_t nAttribs() const { return m_args.size(); }

    // Erase all args
    void clear() { m_args.clear(); }

    //! Iterator support
    typedef KeyValueMap::iterator       iterator;
    typedef KeyValueMap::const_iterator const_iterator;

    iterator       begin()       { return m_args.begin(); }
    const_iterator begin() const { return m_args.begin(); }
    iterator       end()         { return m_args.end(); }
    const_iterator end()   const { return m_args.end(); }

    // Convenience function to transmogrify to an alphabetically-sorted map:
    void getAsSorted(KeyValueSortedMap& sorted_map) const;


  public:
    //! Does the set have a matching arg?
    bool has(const std::string& key) const;
    bool has(const char*        key) const;

    //! Get an arg's string value
    const std::string& get(const std::string& key) const;
    const std::string& get(const char*        key) const;
    const std::string& operator[] (const std::string& key) const { return get(key); }
    const std::string& operator[] (const char*        key) const { return get(key); }

    //! Set a new arg or change an existing one.
    void  set(const std::string& key, const std::string& value) { m_args[key] = value; }
    void  set(const std::string& key, const char*        value) { m_args[key] = value; }
    void  set(const char*        key, const std::string& value) { m_args[std::string(key)] = value; }
    void  set(const char*        key, const char*        value) { m_args[std::string(key)] = std::string(value); }

    //! Removes the arg from the ArgSet.
    void  remove(const std::string& key);
    void  remove(const char*        key);


    //! TODO: this is redundant until we (if ever) support embedded expressions in arg values.
    const std::string& getUnexpandedValue(const std::string& key) const;
    const std::string& getUnexpandedValue(const char*        key) const;

    //! Retrieves the value of the arg with expanded expressions.
    //std::string expandValue(const char*        source) const;
    //std::string expandValue(const std::string& source) const { return expandValue(source.c_str()); }

    //! Expand an expression string.
    //std::string expandExpression(const char*        expression) const;
    //std::string expandExpression(const std::string& expression) const { return expandExpression(expression.c_str()); }


    //! Print all args
    void print(std::ostream&) const;
    //void print(const char*   prefix,
    //           std::ostream&,
    //           bool          show_expanded=false) const;
    friend std::ostream& operator << (std::ostream& o, const ArgSet& b) { b.print(o); return o; }


  public:
    //-------------------------------------------------------------------------
    // Typed read access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    const std::string& getString(const std::string& key, const std::string& dflt_val=empty_string) const;
    const std::string& getString(const char*        key, const std::string& dflt_val=empty_string) const;
    //
    int          getInt(const std::string& key, int       dflt_val=0) const;
    int          getInt(const char*        key, int       dflt_val=0) const;
    double    getDouble(const std::string& key, double    dflt_val=0.0) const;
    double    getDouble(const char*        key, double    dflt_val=0.0) const;
    bool        getBool(const std::string& key, bool      dflt_val=false) const;
    bool        getBool(const char*        key, bool      dflt_val=false) const;
    HashValue   getHash(const std::string& key, HashValue dflt_val=~0ULL) const;
    HashValue   getHash(const char*        key, HashValue dflt_val=~0ULL) const;
    //
    Fsr::Vec2d getVec2d(const std::string& key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const;
    Fsr::Vec2d getVec2d(const char*        key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const;
    //
    Fsr::Vec3d getVec3d(const std::string& key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const;
    Fsr::Vec3d getVec3d(const char*        key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const;
    //
    Fsr::Vec4d getVec4d(const std::string& key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const;
    Fsr::Vec4d getVec4d(const char*        key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const;
    //
    Fsr::Mat4d getMat4d(const std::string& key, Fsr::Mat4d dflt_val=Fsr::Mat4d::getIdentity()) const;
    Fsr::Mat4d getMat4d(const char*        key, Fsr::Mat4d dflt_val=Fsr::Mat4d::getIdentity()) const;


  public:
    //-------------------------------------------------------------------------
    // Typed write access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    void   setString(const std::string& key, const std::string& value);
    void   setString(const std::string& key, const char*        value);
    void   setString(const char*        key, const std::string& value);
    void   setString(const char*        key, const char*        value);
    //
    void      setInt(const std::string& key, int       value);
    void   setDouble(const std::string& key, double    value);
    void     setBool(const std::string& key, bool      value);
    void     setHash(const std::string& key, HashValue value);
    //
    void    setVec2d(const std::string& key, const Fsr::Vec2d& value);
    void    setVec3d(const std::string& key, const Fsr::Vec3d& value);
    void    setVec4d(const std::string& key, const Fsr::Vec4d& value);
    //
    void    setMat4d(const std::string& key, const Fsr::Mat4d& value);


  private:
    KeyValueMap m_args;


}; // ArgSet



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

inline ArgSet::ArgSet() { }
inline ArgSet::ArgSet(const ArgSet& b) : m_args(b.m_args) {}
inline ArgSet::ArgSet(const KeyValueMap& b) : m_args(b) {}
inline ArgSet& ArgSet::operator= (const ArgSet& b) { m_args = b.m_args; return *this; }
inline ArgSet& ArgSet::operator= (const KeyValueMap& b) { m_args = b; return *this; }

//-----------------------------------

inline bool ArgSet::has(const std::string& key) const { return (m_args.find(key) != m_args.end()); }
inline bool ArgSet::has(const char*        key) const { return has(std::string(key)); }

inline void ArgSet::remove(const std::string& key)
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end())
        m_args.erase(it);
}
inline void ArgSet::remove(const char* key) { remove(std::string(key)); }

inline const std::string& ArgSet::get(const std::string& key) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    return (it != m_args.end()) ? it->second : Fsr::empty_string;
}
inline const std::string& ArgSet::get(const char* key) const { return get(std::string(key)); }

//! TODO: this is redundant until we (if ever) support expressions in arg values:
inline const std::string& ArgSet::getUnexpandedValue(const std::string& key) const { return get(key); }
inline const std::string& ArgSet::getUnexpandedValue(const char*        key) const { return getUnexpandedValue(std::string(key)); }

inline const std::string& ArgSet::getString(const char* key, const std::string& dflt_val) const { return getString(std::string(key), dflt_val); }

//-----------------------------------

inline int          ArgSet::getInt(const char* key, int       dflt_val) const { return    getInt(std::string(key), dflt_val); }
inline double    ArgSet::getDouble(const char* key, double    dflt_val) const { return getDouble(std::string(key), dflt_val); }
inline bool        ArgSet::getBool(const char* key, bool      dflt_val) const { return   getBool(std::string(key), dflt_val); }
inline HashValue   ArgSet::getHash(const char* key, HashValue dflt_val) const { return   getHash(std::string(key), dflt_val); }

inline Fsr::Vec2d ArgSet::getVec2d(const char* key, Fsr::Vec2d dflt) const { return getVec2d(std::string(key), dflt); }
inline Fsr::Vec3d ArgSet::getVec3d(const char* key, Fsr::Vec3d dflt) const { return getVec3d(std::string(key), dflt); }
inline Fsr::Vec4d ArgSet::getVec4d(const char* key, Fsr::Vec4d dflt) const { return getVec4d(std::string(key), dflt); }
inline Fsr::Mat4d ArgSet::getMat4d(const char* key, Fsr::Mat4d dflt) const { return getMat4d(std::string(key), dflt); }

inline void ArgSet::setString(const std::string& key, const std::string& value) { set(key, value); }
inline void ArgSet::setString(const std::string& key, const char*        value) { set(key, value); }
inline void ArgSet::setString(const char*        key, const std::string& value) { set(std::string(key), value); }
inline void ArgSet::setString(const char*        key, const char*        value) { set(std::string(key), std::string(value)); }


} // namespace Fsr

#endif

// end of Fuser/ArgSet.h

//
// Copyright 2019 DreamWorks Animation
//
