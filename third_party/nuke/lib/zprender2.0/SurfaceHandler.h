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

/// @file zprender/SurfaceHandler.h
///
/// @author Jonathan Egstad


#ifndef zprender_SurfaceHandler_h
#define zprender_SurfaceHandler_h

#include "api.h"


namespace zpr {

class RenderContext;
class SurfaceContext;


/*! Translation wrappers that convert DDImage and zprender primitives
    to RenderPrimitive types.
*/
class ZPR_EXPORT SurfaceHandler
{
  public:
    SurfaceHandler() {}
    virtual ~SurfaceHandler() {}

    virtual const char* Class() const =0;

  public:
    /*! */
    virtual void generateRenderPrims(RenderContext&  rtx,
                                     SurfaceContext& stx)
    {
        std::cerr << Class() << "zpr::generateRenderPrims() not implemented." << std::endl;
    }
};


//-------------------------------------------------------------------------


/*! Null version.
*/
class ZPR_EXPORT NullSurfaceHandler : public SurfaceHandler
{
  public:
    /*virtual*/ const char* Class() const { return "NullSurfaceHandler"; }
};


} // namespace zpr

#endif

// end of zprender/SurfaceHandler.h

//
// Copyright 2020 DreamWorks Animation
//
