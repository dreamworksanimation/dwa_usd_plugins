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

/// @file zprender/RayPerspectiveCamera.h
///
/// @author Jonathan Egstad


#ifndef zprender_RayPerspectiveCamera_h
#define zprender_RayPerspectiveCamera_h

#include "RayCamera.h"


namespace zpr {


/*! Linear-projection camera.
    Exposes the focal film-width (horizontal-aperture) parameters.
*/
class ZPR_EXPORT RayPerspectiveCamera : public RayCamera
{
  public:
    RayPerspectiveCamera() : RayCamera() {}

    double focalLength()    const { return cam0.focal_length; }
    double filmWidth()      const { return cam0.film_width; }

    //! Non-distorted perspective & orthographic cameras would return true.
    /*virtual*/ bool isLinearProjection() const { return true; }

    //! Zoom-scale is film_width/focal_length, or the precalculated lensScale.
    /*virtual*/ double lensMagnification() const { return cam0.lensScale; }


    /*! Find the camera-projected coordinate at screen-window NDC
        coordinate (in -1...+1 range).

        This takes into account the camera's win translate & win scale,
        but not win roll (yet).
    */
    /*virtual*/
    Fsr::Vec3d getDirVector(const RayCamera::Sample& cam,
                            const Fsr::Vec2d&        screenWindowST) const
    {
        // TODO: This does not apply win_roll yet...

        Fsr::Vec3d P;
        P.x = (screenWindowST.x * cam.win_scale.x) + cam.win_translate.x - cam.filmback_shift;
        P.x *= cam.lensScale*0.5;
        P.y = (screenWindowST.y * cam.win_scale.y) + cam.win_translate.y;
        P.y *= cam.lensScale*0.5 * m_faspect;
        P.z = -1.0;

        //std::cout << screenWindowST << "=" << P << std::endl;
        return P;
    }


    /*! Project a world-space point into perspectively-projected
        screen-window (NDC) range.
    */
    /*virtual*/
    Fsr::Vec2d projectPoint(const RayCamera::Sample& cam,
                            const Fsr::Vec3d&        worldspaceP) const
    {
        return Fsr::Vec2d(0.0, 0.0);
    }

};


} // namespace zpr

#endif

// end of zprender/RayPerspectiveCamera.h

//
// Copyright 2020 DreamWorks Animation
//
