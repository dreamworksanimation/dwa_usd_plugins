//
// Copyright 2020 DreamWorks Animation
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

/// @file zprender/VolumeShader.cpp
///
/// @author Jonathan Egstad


#include "VolumeShader.h"
#include "RenderContext.h"
#include "ThreadContext.h"


namespace zpr {


/*!
*/
VolumeShader::VolumeShader() :
    RayShader()
{
}


//-----------------------------------------------------------------------------


//!
/*static*/ const char* VolumeShader::zpClass() { return "zpVolumeShader"; }

/*!
*/
void
VolumeShader::addVolumeShaderIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, VolumeShader::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/ void
VolumeShader::validateShader(bool                 for_real,
                             const RenderContext& rtx)
{
}


} // namespace zpr


// end of zprender/VolumeShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
