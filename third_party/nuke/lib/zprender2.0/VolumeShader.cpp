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


//!
/*static*/ const char* VolumeShader::zpClass() { return "zpVolumeShader"; }


/*!
*/
/*virtual*/ void
VolumeShader::validateShader(bool                            for_real,
                             const RenderContext*            rtx,
                             const DD::Image::OutputContext* op_ctx)
{
    RayShader::validateShader(for_real, rtx, op_ctx);
}


/*!
*/
/*static*/
bool
VolumeShader::getVolumeIntersections(zpr::RayShaderContext&          stx,
                                     Volume::VolumeIntersectionList& vol_intersections,
                                     double&                         vol_tmin,
                                     double&                         vol_tmax,
                                     double&                         vol_segment_min,
                                     double&                         vol_segment_max)
{
#if DEBUG
    assert(stx.rtx); // shouldn't happen...
#endif
    vol_intersections.clear();
    vol_segment_min = std::numeric_limits<double>::infinity();
    vol_segment_max = 0.0;

    // Get list of light volume intersections:
    Traceable::SurfaceIntersectionList& I_vol_list = stx.thread_ctx->I_vol_list;
    I_vol_list.clear();

    vol_tmin =  std::numeric_limits<double>::infinity(); // Nearest volume intersection (may be behind camera!)
    vol_tmax = -std::numeric_limits<double>::infinity(); // Farthest volume intersection
    stx.rtx->lights_bvh.getIntersections(stx, I_vol_list, vol_tmin, vol_tmax);
    //std::cout << "VolumeShader::getVolumeIntersections(): Rtx[" << stx.Rtx << "]";
    //std::cout << " I_vol_list=" << I_vol_list.size() << std::endl;

    // Volume intersections should always have two intersections, even
    // if they're behind the camera:
    const uint32_t nVolumes = (I_vol_list.size()%2 == 0) ?
                                (uint32_t)I_vol_list.size() / 2 :
                                    0;
    if (nVolumes == 0)
        return false;

    if (isnan(vol_tmin) || isnan(vol_tmax) || vol_tmin >= vol_tmax)
        return false; // invalid distances

    // Build the list of volume intersections:
    uint32_t I_index = 0;
    Volume::VolumeIntersection vI;
    for (uint32_t i=0; i < nVolumes; ++i)
    {
        // Make a single intersection for the back of the volume
        // and a volume intersection to define the entire range:
        const Traceable::SurfaceIntersection& I_enter = I_vol_list[I_index++];
        const Traceable::SurfaceIntersection& I_exit  = I_vol_list[I_index++];
        if (I_enter.object != I_exit.object)
            continue; // shouldn't happen...

        const double segment_size = (I_exit.t - I_enter.t);
        if (::fabs(segment_size) < std::numeric_limits<double>::epsilon())
            continue; // too small in depth, skip it

        // Find the min/max volume depths:
        vol_segment_min = std::min(segment_size, vol_segment_min);
        vol_segment_max = std::max(vol_segment_max, segment_size);

        // Build volume intersection:
        vI.tmin          = I_enter.t;
        vI.tmax          = I_exit.t;
        vI.object        = I_enter.object;
        vI.part_index    =   -1; // legacy, remove!
        vI.subpart_index =   -1; // legacy, remove!
        vI.coverage      = 0.0f; // legacy, remove!

        vol_intersections.push_back(vI);
    }

    return true;
}


} // namespace zpr


// end of zprender/VolumeShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
