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

/// @file Fuser/ShaderNode.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ShaderNode_h
#define Fuser_ShaderNode_h

#include "Node.h"
#include "AttributeTypes.h"


namespace Fsr {


/*! A ShaderNode contains attributes and ShaderNode inputs and outputs.

    In the scope of Fuser it's simply a storage node that may have
    additional ShaderNode children and stores the attributes from
    a (usually) imported Shader from USD or other scenegraph
    system.

    This node and its children are usually translated into real
    shader implementations for whatever system is trying to use them.

    TODO: continue fleshing this concept out
*/
class FSR_EXPORT ShaderNode : public Fsr::Node
{
  protected:
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_outputs;


  public:
    //!
    ShaderNode(Node* parent=NULL);

    //!
    ShaderNode(const ArgSet& args,
               Node*         parent=NULL);

    /*! Must have a virtual destructor!
        Dtor necessary to avoid GCC 'undefined reference to `vtable...' link error.
    */
    virtual ~ShaderNode();


};


} // namespace Fsr

#endif

// end of Fuser/ShaderNode.h

//
// Copyright 2019 DreamWorks Animation
//
