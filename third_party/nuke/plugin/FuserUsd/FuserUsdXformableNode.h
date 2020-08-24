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

/// @file FuserUsdXformableNode.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdXformableNode_h
#define FuserUsdXformableNode_h

#include "FuserUsdNode.h"
#include <Fuser/XformableNode.h>


#ifdef __GNUC__
// Turn off conversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/primvar.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*! USD xformable node wrapper.
*/
class FuserUsdXformableNode : public FuserUsdNode,
                              public Fsr::XformableNode
{
  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdXformableNode"; }

    //!
    FuserUsdXformableNode(const Pxr::UsdStageRefPtr& stage,
                          const Fsr::ArgSet&         args,
                          Fsr::Node*                 parent);

    //!
    ~FuserUsdXformableNode();

    /*! Called before evaluation starts to allow node to prep any data prior to rendering.
        Updates time value and possibly local transform.
    */
    /*virtual*/ void _validateState(const Fsr::NodeContext& exec_ctx,
                                    bool                    for_real);

    //! Prints an unrecognized-target warning in debug mode and returns 0 (success).
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1);

    //-------------------------------------------------------------------------------


    //! Find all Xformable prims at or underneath the given prim.
    static void findXformPrims(const Pxr::UsdPrim&                       prim,
                               std::vector<Pxr::UsdPrimSiblingIterator>& xformables);

};


} // namespace Fsr


#endif

// end of FuserUsdXformableNode.h

//
// Copyright 2019 DreamWorks Animation
//
