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

/// @file CameraOp2.cpp
///
/// @author Jonathan Egstad
///

#include <Fuser/CameraOp.h>

using namespace DD::Image;

/*! Fuser replacement for the stock Nuke CameraOp2 plugin that adds
    scene file loading capabilities (usd/abc/fbx/etc.)
*/
class CameraOp2 : public Fsr::FuserCameraOp
{
  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }

    CameraOp2(::Node* node) : Fsr::FuserCameraOp(node) {}

    /*virtual*/ const char* displayName() const { return "Camera"; }

};


static Op* buildCameraOp2(Node* node) { return new CameraOp2(node); }
const Op::Description CameraOp2::description("Camera2", buildCameraOp2);


// end of CameraOp2.cpp

//
// Copyright 2019 DreamWorks Animation
//
