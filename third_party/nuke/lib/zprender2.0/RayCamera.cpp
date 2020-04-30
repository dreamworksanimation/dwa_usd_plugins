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

/// @file zprender/RayCamera.cpp
///
/// @author Jonathan Egstad


#include "RayCamera.h"
#include "RenderContext.h"

#include <DDImage/Hash.h>

namespace zpr {


/*! Copy parameters from the DD::Image::Format and Camera(s), constructing vars to
    speed up the creation of rays at render time.
*/
/*virtual*/
void
RayCamera::build(const RenderContext&            rtx,
                 const DD::Image::CameraOp*      camera0,
                 const DD::Image::CameraOp*      camera1,
                 const DD::Image::OutputContext& context)
{
    assert(camera0); // always need at least one camera!

    cam0.build(rtx, camera0, context);
    cam1.cam = NULL;
    if (camera1)
        cam1.build(rtx, camera1, context);

    // Floating-point version of image area.  This image area is where the NDC range
    // is normalize to, with the values < -1.0 and > 1.0 going in the overall format's
    // width & height:
    m_fbbox.min.x = double(rtx.render_format->x());
    m_fbbox.min.y = double(rtx.render_format->y());
    m_fbbox.max.x = double(rtx.render_format->r());
    m_fbbox.max.y = double(rtx.render_format->t());
    m_fbbox_w     = double(rtx.render_format->w());
    m_fbbox_h     = double(rtx.render_format->h());

    // Image aspect mixes the pixel image aspect into it:
    m_faspect = (m_fbbox_h / m_fbbox_w)/rtx.render_format->pixel_aspect();

    // Float versions of format full width & height:
    m_fwidth  = double(rtx.render_format->width());
    m_fheight = double(rtx.render_format->height());
}


/*! Copy params from a DD::Image::CameraOp.

    Construct vars to speed up the creation of rays at render time.
    This requires an output context to get the custom stereo params out of the StereoCam plugin knobs.
*/
void
RayCamera::Sample::build(const RenderContext&            rtx,
                         const DD::Image::CameraOp*      camera,
                         const DD::Image::OutputContext& context)
{
    //std::cout << "RayCamera::Sample::build(" << this << ")" << std::endl;
#if DEBUG
    assert(camera);
#endif
    cam = camera;

    // Copy some params out of the CameraOp:
    film_width    = ::fabs(camera->film_width());
    focal_length  = ::fabs(camera->focal_length());
    focus_dist    = ::fabs(camera->focal_point());
    lensScale     = (film_width / focal_length);
    fstop         = ::fabs(camera->fstop());
    near          = clamp(camera->Near(), std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::infinity());
    far           = clamp(camera->Far(),  0.0,                                    std::numeric_limits<double>::infinity());
    win_translate = Fsr::Vec2d(cam->win_translate().array());
    win_rotate    = cam->win_roll();
    win_scale     = Fsr::Vec2d(cam->win_scale().array());

    // Camera xform includes scene global xform:
    matrix = rtx.global_xform;
    matrix *= camera->matrix();
    //std::cout << "  cam_xform" << matrix << std::endl;

    Pnear = matrix.transform(Fsr::Vec3d(0.0, 0.0, -near));
    Pfar  = matrix.transform(Fsr::Vec3d(0.0, 0.0, -far ));

    // TODO: we don't use these currently, can these help speed up the computation of ray differentials...?
    Pdu = matrix.getXAxis();
    Pdu.normalize();
    Pdv = matrix.getYAxis();
    Pdv.normalize();

    // Get filmback shift:
    filmback_shift = 0.0;


    // TODO: support stereo controls in a more reliable fashion rather than using harcoded knob names!

    DD::Image::Knob* kFbShift = camera->knob("convergence_filmback_offset");
    if (kFbShift)
    {
        // StereoCam2 has an explicit filmback shift:
        DD::Image::Hash junk;
        double fb_shift;
        kFbShift->store(DD::Image::DoublePtr, &fb_shift, junk, context);
        filmback_shift = (fb_shift / (camera->film_width() / 2.0));
    }
    else if (camera->knob("stereo_converge_distance") && camera->knob("interaxial"))
    {
        // StereoCam calcs filmback shift from old DWA stereo convergence calculation:
        // Formula to calculate horizontal aperture offset is:
        //    -(interaxial * (focal / coverge_dist)) / (haperture / 2)
        //
        // Need to get values from knobs this way to make sure the context is correct:
        DD::Image::Hash junk;
        double stereo_converge_distance;
        camera->knob("stereo_converge_distance")->store(DD::Image::DoublePtr, &stereo_converge_distance, junk, context);
        double interaxial;
        camera->knob("interaxial")->store(DD::Image::DoublePtr, &interaxial, junk, context);
        //
        if (stereo_converge_distance > 0.0 && ::fabs(interaxial) > 0.0 && camera->film_width() > 0.0)
        {
            filmback_shift =
                ((interaxial * (camera->focal_length() / stereo_converge_distance)) / (camera->film_width() / 2.0));
        }
    }
}


//-------------------------------------------------------------------------


/*! Subclass virtual call.
    Base class initializes the ray aligned with this camera at a
    screen pixel coordinate and optional shutter time.
*/
/*virtual*/
void
RayCamera::_constructRay(const Fsr::Vec2d&      pixelXY,
                         const Fsr::Vec2d&      pixelDxDy,
                         const Fsr::Vec2d&      lensDuDv,
                         float                  shutter_percentage,
                         Fsr::RayContext&       Rout,
                         Fsr::RayDifferentials* Rdif) const
{
    //std::cout << "pixelXY" << pixelXY << ", lensDuDv" << lensdDuDv << std::endl;

    // Get screenWindow (NDC) coords -1.0 - +1.0:
    Fsr::Vec2d screenWindowST;
    pixelXYToScreenWindowXY(pixelXY, screenWindowST);

    Rout.type_mask = Fsr::RayContext::CAMERA;

    Rout.mindist = cam0.near + std::numeric_limits<double>::epsilon();
    Rout.maxdist = cam0.far;
    if (Rout.maxdist < Rout.mindist)
        Rout.maxdist = Rout.mindist;

    Fsr::Vec3d origin;
    Fsr::Vec3d dir;

    if (shutter_percentage < std::numeric_limits<float>::epsilon() || !cam1.cam)
    {
        // All cam0:
        origin = cam0.matrix.getTranslation();
        dir    = cam0.matrix.vecTransform(getDirVector(cam0, screenWindowST));
        if (Rdif)
        {
            const Fsr::Vec2d screenWindowSTx(pixelXToScreenWindowX(pixelXY.x + pixelDxDy.x), screenWindowST.y);
            const Fsr::Vec2d screenWindowSTy(screenWindowST.x, pixelYToScreenWindowY(pixelXY.y + pixelDxDy.y));
            Rdif->setXYDir(cam0.matrix.vecTransform(getDirVector(cam0, screenWindowSTx)),
                           cam0.matrix.vecTransform(getDirVector(cam0, screenWindowSTy)));
        }
    }
    else if (shutter_percentage > (1.0f - std::numeric_limits<float>::epsilon()))
    {
        // All cam1:
        origin = cam1.matrix.getTranslation();
        dir    = cam1.matrix.vecTransform(getDirVector(cam1, screenWindowST));
        if (Rdif)
        {
            const Fsr::Vec2d screenWindowSTx(pixelXToScreenWindowX(pixelXY.x + pixelDxDy.x), screenWindowST.y);
            const Fsr::Vec2d screenWindowSTy(screenWindowST.x, pixelYToScreenWindowY(pixelXY.y + pixelDxDy.y));
            Rdif->setXYDir(cam1.matrix.vecTransform(getDirVector(cam1, screenWindowSTx)),
                           cam1.matrix.vecTransform(getDirVector(cam1, screenWindowSTy)));
        }
    }
    else
    {
        // lerp between cameras:
        origin = cam0.matrix.getTranslation().interpolateTo(cam1.matrix.getTranslation(), double(shutter_percentage));
        dir    = cam0.matrix.vecTransform(getDirVector(cam0, screenWindowST)).interpolateTo(cam1.matrix.vecTransform(getDirVector(cam1, screenWindowST)), double(shutter_percentage));
        if (Rdif)
        {
            const Fsr::Vec2d screenWindowSTx(pixelXToScreenWindowX(pixelXY.x + pixelDxDy.x), screenWindowST.y);
            const Fsr::Vec2d screenWindowSTy(screenWindowST.x, pixelYToScreenWindowY(pixelXY.y + pixelDxDy.y));
            Rdif->setXYDir(cam0.matrix.vecTransform(getDirVector(cam0, screenWindowSTx)),
                           cam0.matrix.vecTransform(getDirVector(cam0, screenWindowSTy)));
        }
    }

    // TODO: add DOF functions back in


    Rout.setOrigin(origin);
    dir.normalize();
    Rout.setDirection(dir);

    //std::cout << "  new.origin" << Rout.origin << std::endl;
    //std::cout << "         dir" << Rout.dir() << std::endl;
}


} // namespace zpr

// end of zprender/RayCamera.cpp

//
// Copyright 2020 DreamWorks Animation
//
