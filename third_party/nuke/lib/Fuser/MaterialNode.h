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

/// @file Fuser/MaterialNode.h
///
/// @author Jonathan Egstad

#ifndef Fuser_MaterialNode_h
#define Fuser_MaterialNode_h

#include "ShaderNode.h"


namespace Fsr {


/*! A Material is the interface to a ShaderNode tree.

    In the scope of Fuser it's simply a grouping node that has a
    series of ShaderNode children and stores the attributes from
    a (usually) imported Material from USD or other scenegraph
    system.

    This node and its children are usually translated into real
    material/shader implementation for whatever system is trying
    to use them.

    TODO: continue fleshing this concept out

*/
class FSR_EXPORT MaterialNode : public Fsr::ShaderNode
{
  protected:
    std::vector<ShaderNode*> m_surface_outputs;         //!< Surface shader outputs from various ShaderNode trees
    std::vector<ShaderNode*> m_displacement_outputs;    //!< Displacement shader outputs from various ShaderNode trees
    std::vector<ShaderNode*> m_volume_outputs;          //!< Volume shader outputs from various ShaderNode trees


  public:
    //!
    MaterialNode(Node* parent=NULL);

    //!
    MaterialNode(const ArgSet& args,
                 Node*         parent=NULL);

    /*! Must have a virtual destructor!
        Dtor necessary to avoid GCC 'undefined reference to `vtable...' link error.
    */
    virtual ~MaterialNode();


    // The list of output shader connections.
    const std::vector<ShaderNode*>& surfaceOutputs()      const { return m_surface_outputs; }
    const std::vector<ShaderNode*>& displacementOutputs() const { return m_displacement_outputs; }
    const std::vector<ShaderNode*>& volumeOutputs()       const { return m_volume_outputs; }
};


} // namespace Fsr

#endif

// end of Fuser/MaterialNode.h

//
// Copyright 2019 DreamWorks Animation
//
