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

/// @file zprender/RaySphericalCamera.h
///
/// @author Jonathan Egstad


#ifndef zprender_RaySphericalCamera_h
#define zprender_RaySphericalCamera_h

#include "RayCamera.h"


namespace zpr {


/*! Simple spherical-projection camera class.
*/
class ZPR_EXPORT RaySphericalCamera : public RayCamera
{
  public:
    RaySphericalCamera() : RayCamera() {}


    /*! Find the spherically-projected coordinate at NDC coordinate u/v.
    */
    /*virtual*/
    Fsr::Vec3d getDirVector(const RayCamera::Sample& cam,
                            const Fsr::Vec2d&        screenWindowST) const
    {
        const double phi    = M_PI * (screenWindowST.x + 1.0) + (M_PI/2.0); // rotate 90deg
        const double sinphi = ::sin(phi);
        const double cosphi = ::cos(phi);

        const double theta    = (M_PI * (screenWindowST.y + 1.0)*0.5);
        const double sintheta = ::sin(M_PI - theta);
        const double costheta = ::cos(M_PI - theta);

        return Fsr::Vec3d(sintheta*cosphi,
                          costheta,
                          sintheta*sinphi);
    }
};


} // namespace zpr


#endif

// end of zprender/RaySphericalCamera.h

//
// Copyright 2020 DreamWorks Animation
//
