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

/// @file FuserUsdShader.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdShader_h
#define FuserUsdShader_h

#include "FuserUsdNode.h"


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/usd/usdShade/shader.h>
#  include <pxr/usd/usdShade/material.h>

#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//-------------------------------------------------------------------------------


/*! USD dummy placeholder node for a real shader.
*/
class FuserUsdShaderNode : public FuserUsdNode
{
  protected:
    Pxr::UsdPrim m_prim;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_prim; }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdShaderNode"; }

    FuserUsdShaderNode(const Pxr::UsdStageRefPtr& stage,
                       const Pxr::UsdPrim&        prim,
                       const Fsr::ArgSet&         args,
                       Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent),
        m_prim(prim)
    {
        //std::cout << "  FuserUsdShaderNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;
    }


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


/*! USD dummy placeholder node for a real shader node graph (material shader network).
*/
class FuserUsdShadeNodeGraphNode : public FuserUsdNode
{
  protected:
    Pxr::UsdPrim m_prim;

    /*virtual*/ Pxr::UsdPrim getPrim() { return m_prim; }


  public:
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return "UsdShadeNodeGraphNode"; }

    FuserUsdShadeNodeGraphNode(const Pxr::UsdStageRefPtr& stage,
                               const Pxr::UsdPrim&        prim,
                               const Fsr::ArgSet&         args,
                               Fsr::Node*                 parent) :
        FuserUsdNode(stage, args, parent),
        m_prim(prim)
    {
        //std::cout << "  FuserUsdShadeNodeGraphNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;
    }


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


} // namespace Fsr

#endif

// end of FuserUsdShader.h

//
// Copyright 2019 DreamWorks Animation
//
