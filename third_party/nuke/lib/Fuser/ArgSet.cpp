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
        sorted_map.insert({it->first, it->second});
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
        std::sscanf(it->second.c_str(), "%lf %lf", &v.x, &v.y);
    return v;
}

/*!
*/
Fsr::Vec3d
ArgSet::getVec3d(const std::string& key, Fsr::Vec3d v) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        std::sscanf(it->second.c_str(), "%lf %lf %lf", &v.x, &v.y, &v.z);
    return v;
}

/*!
*/
Fsr::Vec4d
ArgSet::getVec4d(const std::string& key, Fsr::Vec4d v) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        std::sscanf(it->second.c_str(), "%lf %lf %lf %lf", &v.x, &v.y, &v.z, &v.w);
    return v;
}

/*!
*/
Fsr::Mat4d
ArgSet::getMat4d(const std::string& key, Fsr::Mat4d m) const
{
    const KeyValueMap::const_iterator it = m_args.find(key);
    if (it != m_args.end() && !it->second.empty())
        std::sscanf(it->second.c_str(), "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                    &m.a00, &m.a10, &m.a20, &m.a30,
                    &m.a01, &m.a11, &m.a21, &m.a31,
                    &m.a02, &m.a12, &m.a22, &m.a32,
                    &m.a03, &m.a13, &m.a23, &m.a33);
    return m;
}


//-------------------------------------------------------------------------


#define DBL_STR_SIZE 64

void
ArgSet::setInt(const std::string& key, int value)
{
    char buf[DBL_STR_SIZE];
    std::snprintf(buf, DBL_STR_SIZE, "%d", value);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setDouble(const std::string& key, double value)
{
    char buf[DBL_STR_SIZE];
    std::snprintf(buf, DBL_STR_SIZE, "%.20g", value);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setBool(const std::string& key, bool value)
{
    char buf[4];
    std::snprintf(buf, 4, "%d", value);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setHash(const std::string& key, HashValue value)
{
    char buf[DBL_STR_SIZE];
    std::snprintf(buf, DBL_STR_SIZE, "%016lx", value);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
//
void
ArgSet::setVec2d(const std::string& key, const Fsr::Vec2d& v)
{
    char buf[DBL_STR_SIZE*2];
    std::snprintf(buf, DBL_STR_SIZE*2, "%.20g %.20g", v.x, v.y);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setVec3d(const std::string& key, const Fsr::Vec3d& v)
{
    char buf[DBL_STR_SIZE*3];
    std::snprintf(buf, DBL_STR_SIZE*3, "%.20g %.20g %.20g", v.x, v.y, v.z);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setVec4d(const std::string& key, const Fsr::Vec4d& v)
{
    char buf[DBL_STR_SIZE*4];
    std::snprintf(buf, DBL_STR_SIZE*4, "%.20g %.20g %.20g %.20g", v.x, v.y, v.z, v.w);
    m_args.insert(std::make_pair(key, std::string(buf)));
}
void
ArgSet::setMat4d(const std::string& key, const Fsr::Mat4d& m)
{
    char buf[DBL_STR_SIZE*16];
    std::snprintf(buf, DBL_STR_SIZE*16,
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g "
                    "%.20g %.20g %.20g %.20g",
                        m.a00, m.a10, m.a20, m.a30,
                        m.a01, m.a11, m.a21, m.a31,
                        m.a02, m.a12, m.a22, m.a32,
                        m.a03, m.a13, m.a23, m.a33);
    m_args.insert(std::make_pair(key, std::string(buf)));
}


} // namespace Fsr


// end of Fuser/ArgSet.cpp

//
// Copyright 2019 DreamWorks Animation
//
