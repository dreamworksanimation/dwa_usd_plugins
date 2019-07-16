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

/// @file Fuser/api.h
///
/// @author Jonathan Egstad

#ifndef FSR_EXPORT_H
#define FSR_EXPORT_H

// This should be set sutomatically by the build system:
#define IS_STATIC_LIB 1

// Static or dynamic?
#ifdef IS_STATIC_LIB
    // Use a named knob to test for class type by other plugins not linked:
#   define FUSER_USE_KNOB_RTTI 1

    // Plugin loading designed to work with static Fuser lib - not used anymore!
//#   define FUSER_STATIC_LIB 1
#endif


/* Windows compatibility.
   When compiling the library Fuser_EXPORTS is defined with -D
   When compiling programs Fuser_EXPORTS is undefined
*/
#ifndef FSR_EXPORT
#   if defined(_WIN32)
#       if defined(Fuser_EXPORTS)
#           define FSR_EXPORT __declspec(dllexport)
#       else
#           define FSR_EXPORT __declspec(dllimport)
#       endif
#   else
#       define FSR_EXPORT
#   endif
#endif

// Compatibility macro for older gccs:
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#   define GNU_CONST_DECL const
#else
#   define GNU_CONST_DECL constexpr
#endif


// Define our own version symbols.  This is not currently managed by the Make system,
// so this needs to be kept up to date manually... (which it rarely is)
// TODO: have the release num be incremented automatically
#define FuserVersion           "0.1.3"
#define FuserVersionInteger    00103
#define FuserVersionMajorNum   0
#define FuserVersionMinorNum   1
#define FuserVersionReleaseNum 3

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//
// Below here is some catch-all classes and code that should be moved
// into their own files.
//
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


// Define some types for convenience.
// TODO: move these somewhere else?

#include <stdint.h> // for uint64_t

namespace Fsr {

typedef uint64_t HashValue;                 //!< Matches type in DD::Image::Hash
const HashValue defaultHashValue = ~0ull;   //!< Matches default ctor value from DD::Image::Hash (ie 0xffffffffffffffff)

} // namespace Fsr


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


// TODO: dumping a few string utils here for now...

#include <string>
#include <vector>
#include <stdarg.h> // for va_start, va_end
#include <string.h> // for strlen

namespace Fsr {


/*! Build a string from a variable arg list.
    Expansion length is hard limited to 2048 chars.
*/
inline std::string
buildStr(const char* msg, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, 2048, msg, args);
    va_end(args);
    return std::string(buf);
}


//!
inline bool stringStartsWith(const std::string& a, const char* b) { return (a.find(b) == 0); }
inline bool stringStartsWith(const std::string& a, const std::string& b) { return (a.find(b) == 0); }
//!
inline bool stringEndsWith(const std::string& a, const char* b) { return (a.rfind(b) == (a.size() - strlen(b))); }
inline bool stringEndsWith(const std::string& a, const std::string& b) { return (a.rfind(b) == (a.size() - b.size())); }


/*! Split a string based on a delimiter list.
*/
inline void
stringSplit(const std::string&        src,
            const char*               delimiters,
            std::vector<std::string>& results)
{
    results.reserve(3);
    const size_t src_len = src.size();

    size_t i = 0;
    while (1)
    {
        const size_t a = src.find_first_of(delimiters, i);
        if (a == std::string::npos)
        {
            if (i < src_len)
                results.push_back(src.substr(i, std::string::npos)); // add last piece
            return;
        }
        if (i < a)
            results.push_back(src.substr(i, (a-i)));
        i = a+1;
    }
}

//! Alias 'cause I keep wanting to use this method name...
inline void
splitString(const std::string&        src,
            const char*               delimiters,
            std::vector<std::string>& results) { stringSplit(src, delimiters, results); }

//-------------------------------------------------------------------------

/*!
*/
inline void
splitPath(const std::string& path,
          std::string&       parent_path,
          std::string&       name)
{
    const size_t a = path.rfind('/');
    if (a == 0)
        name = path.substr(1, std::string::npos);
    else if (a == std::string::npos)
        name = path;
    else
    {
        parent_path = path.substr(0, a);
        name = path.substr(a+1, std::string::npos);
    }
}

/*!
*/
inline std::string
fileNameFromPath(const std::string& path)
{
    if (path.size() == 0)
        return std::string();

    const size_t a = path.rfind('/');
    if (a != std::string::npos)
        return path.substr(a+1, std::string::npos);
    else if (path[0] == '.')
        return path;
    else
        return path; // no leading path
}

//-------------------------------------------------------------------------


/*! Trim off characters from left side of string.
    Defaults to whitespace characters.
*/
inline std::string
stringTrimLeft(const std::string& str,
               const char*        trim=" \t\r\n")
{
    const size_t a = str.find_first_not_of(trim);
    return (a == std::string::npos) ? std::string("") : str.substr(a, std::string::npos);
}

/*! Trim off characters from right side of string.
    Defaults to whitespace characters.
*/
inline std::string
stringTrimRight(const std::string& str,
                const char*        trim=" \t\r\n")
{
    const size_t a = str.find_last_not_of(trim);
    return str.substr(0, a+1);
}

/*! Trim off characters from both sides of string.
    Defaults to whitespace characters.
*/
inline std::string
stringTrim(const std::string& str,
           const char*        trim=" \t\r\n")
{
    const size_t a = str.find_first_not_of(trim);
    if (a == std::string::npos)
        return std::string("");
    const size_t b = str.find_last_not_of(trim);
    if (b == std::string::npos)
        return std::string(""); // shouldn't happen!
    return str.substr(a, (b-a+1));
}

/*! Find all instances of string 'find' in 'str' and replace
    them in place with string 'replace'.
*/
inline int
stringReplaceAll(std::string& str,
                 const char*  find,
                 const char*  replace_with)
{
    if (!find || !find[0] || !replace_with)
        return 0;

    size_t find_len = strlen(find);
    size_t repl_len = strlen(replace_with);
    if (repl_len > 0)
        ++repl_len;

    int count = 0;
    size_t i = 0;
    while (1)
    {
        const size_t a = str.find(find, i);
        if (a == std::string::npos)
           return count;

        str.replace(a, find_len, replace_with);
        i = a + repl_len;
        ++count;
    }
}

//-------------------------------------------------------------------------

/*! Match pattern string in text string using glob-like rules.
*/
inline bool
globMatch(const char* pattern,
          const char* text)
{
    if (pattern == NULL || text == NULL)
        return false;

    const char *pp, *tp;
    bool restart_pattern, have_asterisk = false;
    // Loop until we don't have to evaluate pattern string anymore:
    while (1)
    {
        restart_pattern = false;
        for (pp=pattern, tp=text; *tp != 0; ++pp, ++tp)
        {
            if (*pp == '*') // match all
            {
                have_asterisk = true;
                pattern = pp;
                text    = tp;
                if (*++pattern == 0)
                     return true;
                restart_pattern = true;
                break;
            }
            else if (*pp == '?') // match single
            {
                if (*tp == '.')
                {
                    if (!have_asterisk)
                        return false;
                    ++text;
                    restart_pattern = true;
                    break;
                }
            }
            else
            {
                if (*tp != *pp)
                {
                    if (!have_asterisk)
                        return false;
                    ++text;
                    restart_pattern = true;
                    break;
                }
            }
        }
        if (!restart_pattern)
        {
            // We're done:
            if (*pp == '*')
                ++pp;
            return (*pp == 0);
        }
    }
}

inline bool
globMatch(const std::string& pattern,
          const std::string& text)
{
    return globMatch(pattern.c_str(), text.c_str());
}


} // namespace Fsr


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

namespace DD { namespace Image {

/*! This is missing from DD::Image and is used in PrimitiveContext(face_clipmask_)
    and VertexContext(face_clipmask_) to pass the clip status of verts to the Scene
    during rPrimitive addition to the render primitives list.

    However I don't think the face_clipmask in either of those contexts are used
    in practice anymore as the rPrimitive is in charge of determining its own clip
    status (rPrimitive::add_to_render()) since things like displacement and
    non-linear projections make these simple clip planes moot.
*/
enum ClippingPlanes
{
    CLIP_PLANE_RIGHT  = 0x01,
    CLIP_PLANE_LEFT   = 0x02,
    CLIP_PLANE_TOP    = 0x04,
    CLIP_PLANE_BOTTOM = 0x08,
    CLIP_PLANE_NEAR   = 0x10,
    CLIP_PLANE_FAR    = 0x20
};

}}


// Get around the #defines in DD::Image::MatrixArray.h not being explicit about
// the DD::Image namespace for the *_SPACE enums:
#include <DDImage/MatrixArray.h>
#undef LOCAL_TO_WORLD
#undef LOCAL_TO_EYE
#undef LOCAL_TO_CLIP
#undef LOCAL_TO_SCREEN
#undef WORLD_TO_LOCAL
#undef WORLD_TO_EYE
#undef WORLD_TO_CLIP
#undef WORLD_TO_SCREEN
#undef EYE_TO_LOCAL
#undef EYE_TO_WORLD
#undef EYE_TO_CLIP
#undef EYE_TO_SCREEN
#undef CLIP_TO_LOCAL
#undef CLIP_TO_WORLD
#undef CLIP_TO_EYE
#undef CLIP_TO_SCREEN
#undef SCREEN_TO_LOCAL
#undef SCREEN_TO_WORLD
#undef SCREEN_TO_EYE
#undef SCREEN_TO_CLIP
//
#define LOCAL_TO_WORLD   DD::Image::LOCAL_SPACE  * TRANSFORM_NUM + DD::Image::WORLD_SPACE
#define LOCAL_TO_EYE     DD::Image::LOCAL_SPACE  * TRANSFORM_NUM + DD::Image::EYE_SPACE
#define LOCAL_TO_CLIP    DD::Image::LOCAL_SPACE  * TRANSFORM_NUM + DD::Image::CLIP_SPACE
#define LOCAL_TO_SCREEN  DD::Image::LOCAL_SPACE  * TRANSFORM_NUM + DD::Image::SCREEN_SPACE

#define WORLD_TO_LOCAL   DD::Image::WORLD_SPACE  * TRANSFORM_NUM + DD::Image::LOCAL_SPACE
#define WORLD_TO_EYE     DD::Image::WORLD_SPACE  * TRANSFORM_NUM + DD::Image::EYE_SPACE
#define WORLD_TO_CLIP    DD::Image::WORLD_SPACE  * TRANSFORM_NUM + DD::Image::CLIP_SPACE
#define WORLD_TO_SCREEN  DD::Image::WORLD_SPACE  * TRANSFORM_NUM + DD::Image::SCREEN_SPACE

#define EYE_TO_LOCAL     DD::Image::EYE_SPACE    * TRANSFORM_NUM + DD::Image::LOCAL_SPACE
#define EYE_TO_WORLD     DD::Image::EYE_SPACE    * TRANSFORM_NUM + DD::Image::WORLD_SPACE
#define EYE_TO_CLIP      DD::Image::EYE_SPACE    * TRANSFORM_NUM + DD::Image::CLIP_SPACE
#define EYE_TO_SCREEN    DD::Image::EYE_SPACE    * TRANSFORM_NUM + DD::Image::SCREEN_SPACE

#define CLIP_TO_LOCAL    DD::Image::CLIP_SPACE   * TRANSFORM_NUM + DD::Image::LOCAL_SPACE
#define CLIP_TO_WORLD    DD::Image::CLIP_SPACE   * TRANSFORM_NUM + DD::Image::WORLD_SPACE
#define CLIP_TO_EYE      DD::Image::CLIP_SPACE   * TRANSFORM_NUM + DD::Image::EYE_SPACE
#define CLIP_TO_SCREEN   DD::Image::CLIP_SPACE   * TRANSFORM_NUM + DD::Image::SCREEN_SPACE

#define SCREEN_TO_LOCAL  DD::Image::SCREEN_SPACE * TRANSFORM_NUM + DD::Image::LOCAL_SPACE
#define SCREEN_TO_WORLD  DD::Image::SCREEN_SPACE * TRANSFORM_NUM + DD::Image::WORLD_SPACE
#define SCREEN_TO_EYE    DD::Image::SCREEN_SPACE * TRANSFORM_NUM + DD::Image::EYE_SPACE
#define SCREEN_TO_CLIP   DD::Image::SCREEN_SPACE * TRANSFORM_NUM + DD::Image::CLIP_SPACE

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


#endif

//
// Copyright 2019 DreamWorks Animation
//
