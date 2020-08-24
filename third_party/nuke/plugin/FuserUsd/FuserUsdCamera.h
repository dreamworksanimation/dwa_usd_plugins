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

/// @file FuserUsdCamera.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdCamera_h
#define FuserUsdCamera_h

#include "FuserUsdXform.h"


#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/scope.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*! UsdGeomCamera node wrapper.
*/
class FuserUsdCamera : public FuserUsdXform
{
  protected:
    Pxr::UsdGeomCamera m_camera_schema;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_camera_schema.GetPrim(); }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdCamera"; }

    //!
    FuserUsdCamera(const Pxr::UsdStageRefPtr& stage,
                   const Pxr::UsdPrim&        camera_prim,
                   const Fsr::ArgSet&         args,
                   Fsr::Node*                 parent);


    //-------------------------------------------------------------------------------


    //! Called before execution to allow node to update local data from args.
    /*virtual*/ void _validateState(const Fsr::NodeContext& exec_ctx,
                                    bool                    for_real);


    //! Return abort (-1) on user-interrupt so processing can be interrupted.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1);


    //! Import node attributes into a Nuke Op.
    /*virtual*/ void importSceneOp(DD::Image::Op*     op,
                                   const Fsr::ArgSet& exec_args);

    //! Specialization - import node attributes into a Nuke Iop.
    virtual void importIntoIop(DD::Image::Iop*    iop,
                               const Fsr::ArgSet& exec_args);

};


} // namespace Fsr

#endif

// end of FuserUsdCamera.h

//
// Copyright 2019 DreamWorks Animation
//
