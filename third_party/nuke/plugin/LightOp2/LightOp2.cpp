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

/// @file Light2.cpp
///
/// @author Jonathan Egstad
///

#include <Fuser/LightOp.h>

using namespace DD::Image;

/*! Fuser replacement for the stock Nuke Light2 plugin that adds
    scene file loading capabilities (usd/abc/fbx/etc.)
*/
class LightOp2 : public Fsr::FuserLightOp
{
  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }

    LightOp2(Node *node) : Fsr::FuserLightOp(node) {}

    /*virtual*/ const char* displayName() const { return "Light"; }

};


static Op* buildLightOp2(Node *node) {return new LightOp2(node);}
const Op::Description LightOp2::description("Light2", buildLightOp2);

// end of LightOp2.cpp

//
// Copyright 2019 DreamWorks Animation
//
