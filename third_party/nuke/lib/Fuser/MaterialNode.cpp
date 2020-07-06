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

/// @file Fuser/MaterialNode.cpp
///
/// @author Jonathan Egstad

#include "MaterialNode.h"

#include <mutex> // for std::mutex

namespace Fsr {


/*!
*/
MaterialNode::MaterialNode(Node* parent) :
    Fsr::ShaderNode(parent)
{
    //std::cout << "  MaterialNode::ctor(" << this << ")" << std::endl;
}

/*!
*/
MaterialNode::MaterialNode(const ArgSet& args,
                           Node*         parent) :
    Fsr::ShaderNode(args, parent)
{
    //std::cout << "  MaterialNode::ctor(" << this << ")" << std::endl;

    if (debug())
    {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "Fsr::MaterialNode('" << getName() << "')";
        std::cout << " args[" << m_args << "]" << std::endl;
    }

    m_surface_outputs.reserve(3);
    m_displacement_outputs.reserve(3);
    m_volume_outputs.reserve(3);
}


/*! This empty dtor is necessary to avoid GCC 'undefined reference to `vtable...' link error.
    Must be in implemenation file, not header.
*/
MaterialNode::~MaterialNode()
{
    //std::cout << "  MaterialNode::dtor(" << this << ")" << std::endl;
}



} // namespace Fsr


// end of Fuser/MaterialNode.cpp

//
// Copyright 2019 DreamWorks Animation
//
