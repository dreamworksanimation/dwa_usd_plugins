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

/// @file zprender/LightVolume.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightVolume_h
#define zprender_LightVolume_h

#include "Volume.h"
#include "RenderPrimitive.h"


namespace zpr {


class LightEmitter;


/*! Make sure this subclasses from RenderPrimitive first so that
    static_cast<RenderPrimitive*> works!

*/
class ZPR_EXPORT LightVolume : public RenderPrimitive,
                               public Volume
                               
{
  public:
    //!
    LightVolume(const MaterialContext* material_info,
                double                 motion_time) :
        RenderPrimitive(material_info, motion_time),
        Volume(2/*nSurfaces*/)
    {
        //
    }


    //!
    LightVolume(const MaterialContext* material_info,
                const Fsr::DoubleList& motion_times) :
        RenderPrimitive(material_info, motion_times),
        Volume(2/*nSurfaces*/)
    {
        //
    }

};


} // namespace zpr


#endif

// end of zprender/LightVolume.h

//
// Copyright 2020 DreamWorks Animation
//
