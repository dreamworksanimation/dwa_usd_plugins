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

/// @file FuserUsdLight.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdLight_h
#define FuserUsdLight_h

#include "FuserUsdXform.h"


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/usd/usdLux/light.h>

#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*! UsdGeomLight node wrapper.
*/
class FuserUsdLight : public FuserUsdXform
{
  protected:
    Pxr::UsdLuxLight    m_light_schema;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_light_schema.GetPrim(); }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdLight"; }

    FuserUsdLight(const Pxr::UsdStageRefPtr& stage,
                  const Pxr::UsdPrim&        light_prim,
                  const Fsr::ArgSet&         args,
                  Fsr::Node*                 parent);

    //! Called before execution to allow node to update local data from args.
    /*virtual*/ void _validateState(const Fsr::NodeContext& args,
                                    bool                    for_real);


    //! Return abort (-1) on user-interrupt so processing can be interrupted.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1);


    //! Import node attributes into a Nuke Op.
    /*virtual*/ void importSceneOp(DD::Image::Op*     op,
                                   const Fsr::ArgSet& args);

};


} // namespace Fsr

#endif

// end of FuserUsdLight.h

//
// Copyright 2019 DreamWorks Animation
//
