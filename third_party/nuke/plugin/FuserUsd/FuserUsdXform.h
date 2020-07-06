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

/// @file FuserUsdXform.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdXform_h
#define FuserUsdXform_h

#include "FuserUsdXformableNode.h"


namespace Fsr {


//-------------------------------------------------------------------------------


/*! UsdGeomXformable node wrapper.
*/
class FuserUsdXform : public FuserUsdXformableNode
{
  protected:
    Pxr::UsdGeomXformable   m_xformable_schema;     // Store the Xformable (vs. Xform) for subclasses to access

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_xformable_schema.GetPrim(); }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdXform"; }

    FuserUsdXform(const Pxr::UsdStageRefPtr& stage,
                  const Pxr::UsdPrim&        xform_prim,
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


    //! Returns true if prim can concatenate its transform.
    static bool canConcatenateTransform(const Pxr::UsdPrim& prim);

    //! Find the total first-last keyframe ranges for the prim and all its parents.
    static void getConcatenatedXformOpTimeSamples(const Pxr::UsdPrim& prim,
                                                  std::set<double>&   times);

    //!
    static Fsr::Mat4d getLocalMatrixAtPrim(const Pxr::UsdPrim&     prim,
                                           const Pxr::UsdTimeCode& timecode);
    //!
    static Fsr::Mat4d getConcatenatedMatrixAtPrim(const Pxr::UsdPrim&     prim,
                                                  const Pxr::UsdTimeCode& timecode);

    //!
    static void getConcatenatedMatricesAtPrim(const Pxr::UsdPrim&         prim,
                                              const std::vector<double>&  times,
                                              std::vector<Fsr::Mat4d>&    matrices);

    //!
    static bool getXformOpAsRotations(const Pxr::UsdGeomXformOp& xform_op,
                                      const Pxr::UsdTimeCode&    timecode,
                                      Pxr::GfVec3d&              rotations);


    //! Import node attributes into a Nuke Op.
    /*virtual*/ void importSceneOp(DD::Image::Op*     op,
                                   const Fsr::ArgSet& args);

};


} // namespace Fsr

#endif

// end of FuserUsdXform.h

//
// Copyright 2019 DreamWorks Animation
//
