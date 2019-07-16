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

/// @file Fuser/ArgSet.cpp
///
/// @author Jonathan Egstad

#include "ArgSet.h"
#include "NodeContext.h"

namespace Fsr {


/*extern*/ std::string empty_string = "";


//-------------------------------------------------------------------------


void
ArgSet::print(std::ostream& o) const
{
    if (m_args.size() == 0)
        return;
    KeyValueMap::const_iterator it = m_args.begin();
    o << "'" << it->first << "'=[" << it->second << "]";
    for (++it; it != m_args.end(); ++it)
        o << ", '" << it->first << "'=[" << it->second << "]";
}

void
ArgSet::getAsSorted(KeyValueSortedMap& sorted_map) const
{
    sorted_map.clear();
    if (m_args.size() == 0)
        return;
    for (KeyValueMap::const_iterator it=m_args.begin(); it != m_args.end(); ++it)
        sorted_map[it->first] = it->second;
}

//-------------------------------------------------------------------------

const std::string&
ArgSet::getString(const std::string& key, const std::string& dflt_val) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end())
        return it->second;
    return dflt_val;
}

int
ArgSet::getInt(const std::string& key, int dflt_val) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        return ::atoi(it->second.c_str());
    return dflt_val;
}

#if 1
double
ArgSet::getDouble(const std::string& key, double dflt_val) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        return ::atof(it->second.c_str());
    return dflt_val;
}
#else
// TODO: switch to C++11 string conversion methods?...check speed against atof:
double
ArgSet::getDouble(const std::string& key, double dflt_val) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        return std::stod(it->second);
    return dflt_val;
}
#endif

bool
ArgSet::getBool(const std::string& key, bool dflt_val) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        return (::atoi(it->second.c_str()) != 0);
    return dflt_val;
}

HashValue
ArgSet::getHash(const std::string& key, HashValue dflt_val) const
{
    char* end;
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        return ::strtoul(it->second.c_str(), &end, 16/*hex*/);
    return dflt_val;
}


//-------------------------------------------------------------------------


/*!
*/
Fsr::Vec2d
ArgSet::getVec2d(const std::string& key, Fsr::Vec2d v) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        sscanf(it->second.c_str(), "%lf %lf", &v.x, &v.y);
    return v;
}

/*!
*/
Fsr::Vec3d
ArgSet::getVec3d(const std::string& key, Fsr::Vec3d v) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        sscanf(it->second.c_str(), "%lf %lf %lf", &v.x, &v.y, &v.z);
    return v;
}

/*!
*/
Fsr::Vec4d
ArgSet::getVec4d(const std::string& key, Fsr::Vec4d v) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        sscanf(it->second.c_str(), "%lf %lf %lf %lf", &v.x, &v.y, &v.z, &v.w);
    return v;
}

/*!
*/
Fsr::Mat4d
ArgSet::getMat4d(const std::string& key, Fsr::Mat4d m) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        sscanf(it->second.c_str(), "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &m.a00, &m.a10, &m.a20, &m.a30,
                &m.a01, &m.a11, &m.a21, &m.a31,
                &m.a02, &m.a12, &m.a22, &m.a32,
                &m.a03, &m.a13, &m.a23, &m.a33);
    return m;
}


//-------------------------------------------------------------------------


#define DOUBLE_BUF 64

void
ArgSet::setInt(const std::string& key, int value)
{
    char buf[DOUBLE_BUF];
    std::snprintf(buf, DOUBLE_BUF, "%d", value);
    setString(key, buf);
}
void
ArgSet::setDouble(const std::string& key, double value)
{
    char buf[DOUBLE_BUF];
    std::snprintf(buf, DOUBLE_BUF, "%.20g", value);
    setString(key, buf);
}
void
ArgSet::setBool(const std::string& key, bool value)
{
    char buf[4];
    std::snprintf(buf, 4, "%d", value);
    setString(key, buf);
}
void
ArgSet::setHash(const std::string& key, HashValue value)
{
    char buf[DOUBLE_BUF];
    std::snprintf(buf, DOUBLE_BUF, "%016lx", value);
    setString(key, buf);
}
//
void
ArgSet::setVec2d(const std::string& key, const Fsr::Vec2d& v)
{
    char buf[DOUBLE_BUF*2];
    std::snprintf(buf, DOUBLE_BUF*2, "%.20g %.20g", v.x, v.y);
    setString(key, buf);
}
void
ArgSet::setVec3d(const std::string& key, const Fsr::Vec3d& v)
{
    char buf[DOUBLE_BUF*3];
    std::snprintf(buf, DOUBLE_BUF*3, "%.20g %.20g %.20g", v.x, v.y, v.z);
    setString(key, buf);
}
void
ArgSet::setVec4d(const std::string& key, const Fsr::Vec4d& v)
{
    char buf[DOUBLE_BUF*4];
    std::snprintf(buf, DOUBLE_BUF*4, "%.20g %.20g %.20g %.20g", v.x, v.y, v.z, v.w);
    setString(key, buf);
}
void
ArgSet::setMat4d(const std::string& key, const Fsr::Mat4d& m)
{
    char buf[DOUBLE_BUF*16];
    std::snprintf(buf, DOUBLE_BUF*16,
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g",
                        m.a00, m.a10, m.a20, m.a30,
                        m.a01, m.a11, m.a21, m.a31,
                        m.a02, m.a12, m.a22, m.a32,
                        m.a03, m.a13, m.a23, m.a33);
    setString(key, buf);
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// TODO: this is currently a catchall and is only being used as a 
// abstract container for the ArgSet.
// Either delete it or make it more useful.

const std::string& NodeContext::getString(const std::string& key, const std::string& dflt_val) const { return m_args.getString(key, dflt_val); }
const std::string& NodeContext::getString(const char*        key, const std::string& dflt_val) const { return m_args.getString(key, dflt_val); }

int          NodeContext::getInt(const std::string& key, int        dflt_val) const { return    m_args.getInt(key, dflt_val); }
int          NodeContext::getInt(const char*        key, int        dflt_val) const { return    m_args.getInt(key, dflt_val); }
double    NodeContext::getDouble(const std::string& key, double     dflt_val) const { return m_args.getDouble(key, dflt_val); }
double    NodeContext::getDouble(const char*        key, double     dflt_val) const { return m_args.getDouble(key, dflt_val); }
bool        NodeContext::getBool(const std::string& key, bool       dflt_val) const { return   m_args.getBool(key, dflt_val); }
bool        NodeContext::getBool(const char*        key, bool       dflt_val) const { return   m_args.getBool(key, dflt_val); }
HashValue   NodeContext::getHash(const std::string& key, HashValue  dflt_val) const { return   m_args.getHash(key, dflt_val); }
HashValue   NodeContext::getHash(const char*        key, HashValue  dflt_val) const { return   m_args.getHash(key, dflt_val); }

Fsr::Vec2d NodeContext::getVec2d(const std::string& key, Fsr::Vec2d dflt_val) const { return  m_args.getVec2d(key, dflt_val); }
Fsr::Vec2d NodeContext::getVec2d(const char*        key, Fsr::Vec2d dflt_val) const { return  m_args.getVec2d(key, dflt_val); }

Fsr::Vec3d NodeContext::getVec3d(const std::string& key, Fsr::Vec3d dflt_val) const { return  m_args.getVec3d(key, dflt_val); }
Fsr::Vec3d NodeContext::getVec3d(const char*        key, Fsr::Vec3d dflt_val) const { return  m_args.getVec3d(key, dflt_val); }

Fsr::Vec4d NodeContext::getVec4d(const std::string& key, Fsr::Vec4d dflt_val) const { return  m_args.getVec4d(key, dflt_val); }
Fsr::Vec4d NodeContext::getVec4d(const char*        key, Fsr::Vec4d dflt_val) const { return  m_args.getVec4d(key, dflt_val); }

Fsr::Mat4d NodeContext::getMat4d(const std::string& key, Fsr::Mat4d dflt_val) const { return  m_args.getMat4d(key, dflt_val); }
Fsr::Mat4d NodeContext::getMat4d(const char*        key, Fsr::Mat4d dflt_val) const { return  m_args.getMat4d(key, dflt_val); }

//-------------------------------------------------------------------------

void NodeContext::setString(const std::string& key, const std::string& value) { m_args.setString(key, value); }
void NodeContext::setString(const std::string& key, const char*        value) { m_args.setString(key, value); }
void NodeContext::setString(const char*        key, const std::string& value) { m_args.setString(key, value); }
void NodeContext::setString(const char*        key, const char*        value) { m_args.setString(key, value); }

void    NodeContext::setInt(const std::string& key, int                value) {    m_args.setInt(key, value); }
void NodeContext::setDouble(const std::string& key, double             value) { m_args.setDouble(key, value); }
void   NodeContext::setBool(const std::string& key, bool               value) {   m_args.setBool(key, value); }
void   NodeContext::setHash(const std::string& key, HashValue          value) {   m_args.setHash(key, value); }

void  NodeContext::setVec2d(const std::string& key, const Fsr::Vec2d&  value) {  m_args.setVec2d(key, value); }
void  NodeContext::setVec3d(const std::string& key, const Fsr::Vec3d&  value) {  m_args.setVec3d(key, value); }
void  NodeContext::setVec4d(const std::string& key, const Fsr::Vec4d&  value) {  m_args.setVec4d(key, value); }

void  NodeContext::setMat4d(const std::string& key, const Fsr::Mat4d&  value) {  m_args.setMat4d(key, value); }


} // namespace Fsr


// end of Fuser/ArgSet.cpp

//
// Copyright 2019 DreamWorks Animation
//
