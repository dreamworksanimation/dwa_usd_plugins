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

/// @file zprender/api.h
///
/// @author Jonathan Egstad


#ifndef zprender_api_H
#define zprender_api_H

// Define our own version symbols.  This is not currently managed by the Make system,
// so this needs to be kept up to date:
#define zprenderVersion "2.0.1"
#define zprenderVersionInteger 20001
#define zprenderVersionMajorNum   2
#define zprenderVersionMinorNum   0
#define zprenderVersionReleaseNum 1


// This should be set automatically by the build system:
#define IS_STATIC_LIB 1

// Static or dynamic?
#ifdef IS_STATIC_LIB
#   define ZPR_STATIC_LIB 1
    // Use a named knob to test for class type by other plugins not linked:
#   define ZPR_USE_KNOB_RTTI 1
#endif


/* Windows compatibility.
   When compiling the library zprender_EXPORTS is defined with -D
   When compiling programs zprender_EXPORTS is undefined
*/
#ifndef ZPR_EXPORT
# if defined(_WIN32)
#  if defined(zprender_EXPORTS)
#   define ZPR_EXPORT __declspec(dllexport)
#  else
#   define ZPR_EXPORT __declspec(dllimport)
#  endif
# else
#  define ZPR_EXPORT
# endif
#endif


// Used to magically (hackily) identify extended classes without using
// dynamic_cast. No idea if this is a reasonably unique pattern to
// expect it 'never' occur after an allocated class...but it should
// work most of the time...
// We rely on C++ packing the zpr::Scene struct vars right
// after the DD::Image::Scene ones...
//
//    magic_token = 0100 1100 0111 0000 1111 0000 0111 1100 0011 1110 0000 1111 0000 1110 0011 0010
//                  0x4c70f07c3e0f0e32
#define ZPR_MAGIC_TOKEN 0x4c70f07c3e0f0e32ull


#endif

// end of zprender/api.h

//
// Copyright 2020 DreamWorks Animation
//
