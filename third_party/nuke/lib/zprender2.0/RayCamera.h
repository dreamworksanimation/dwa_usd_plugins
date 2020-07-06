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

/// @file zprender/RayCamera.h
///
/// @author Jonathan Egstad


#ifndef zprender_RayCamera_h
#define zprender_RayCamera_h

#include "api.h"
#include <Fuser/RayContext.h>

#include <DDImage/CameraOp.h>


namespace zpr {

class RenderContext;


/*! Ray-tracing camera base class.

    This also stores the baseline perspective camera vars since most
    cameras have focal, film_width even though they may not get used
    in the calculation of the projection.


    TODO: change to absolute frame time and support more than two camera
          motion samples
*/
class ZPR_EXPORT RayCamera
{
  public:

    /*! Helper class to precalc various camera parameters from a DDImage CameraOp
        plus some rendering params like resolution to make generating camera rays
        easier and fast.
    */
    class Sample
    {
      public:
        const DD::Image::CameraOp* cam;
        //
        Fsr::Mat4d matrix;
        double     focal_length;
        double     film_width;
        double     lensScale;
        double     fstop;
        //
        double     near, far;
        double     focus_dist;
        //
        double     filmback_shift;
        Fsr::Vec2d win_translate;
        double     win_rotate;
        Fsr::Vec2d win_scale;

        //! For possible ray-differential use:
        // TODO: deprecate these?
        Fsr::Vec3d Pnear, Pfar;
        Fsr::Vec3d Pdu,   Pdv;

        //! Copy params from a DD::Image::CameraOp.
        void build(const RenderContext&            rtx,
                   const DD::Image::CameraOp*      cam,
                   const DD::Image::OutputContext& context);
    };


  public://protected:
    Sample cam0, cam1;  //! TODO: make this a vector of motion samples


    // Floating-point version of Nuke Format params:
    Fsr::Box2d m_fbbox;                 //!< Image (projection) area rectangle
    double     m_fbbox_w;               //!< Image (projection) width (same as r-x)
    double     m_fbbox_h;               //!< Image (projection) height (same as t-y)
    double     m_faspect;               //!< Image aspect ratio - with pixel aspect mixed in too
    double     m_fwidth, m_fheight;     //!< Overall format size


  public:
    RayCamera() {}
    virtual ~RayCamera() {}

    double near() const { return cam0.near; }
    double far()  const { return cam0.far; }

    double focusDistance() const { return cam0.focus_dist; }
    double fStop()         const { return cam0.fstop; }

    //! Aspect ratio of the image space, with pixel-aspect mixed in.
    double apertureAspect() const { return m_faspect; }


    //! Non-distorted perspective & orthographic cameras would return true.
    virtual bool isLinearProjection() const { return false; }

    //! Zoom-scale is film_width / focal_length for a perspective camera, or 1.0 for a non-linear one.
    virtual double lensMagnification() const { return 1.0; }


    //! Copy parameters from the DD::Image::Format and Camera(s).
    virtual void build(const RenderContext&            rtx,
                       const DD::Image::CameraOp*      camera0,
                       const DD::Image::CameraOp*      camera1,
                       const DD::Image::OutputContext& context);


  public:
    //! Convert a pixel-space coordinate into -1...+1 screen-window (NDC) range.
    double pixelXToScreenWindowX(double pixelX) const;
    double pixelYToScreenWindowY(double pixelY) const;
    void   pixelXYToScreenWindowXY(const Fsr::Vec2d& pixelXY,
                                   Fsr::Vec2d&       screenWindowST) const;


    /*! Initialize a ray aligned with this camera at a screen pixel
        coordinate and optional shutter time.
        Calls the subclass's virtual method.
    */
    void constructRay(const Fsr::Vec2d& pixelXY,
                      const Fsr::Vec2d& lensDuDv,
                      float             shutter_percentage,
                      Fsr::RayContext&  Rout) const;

    /*! Initialize a ray and its differntials, aligned with this camera at a
        screen pixel coordinate and optional shutter time.
        Calls the subclass's virtual method.
    */
    void constructRay(const Fsr::Vec2d&      pixelXY,
                      const Fsr::Vec2d&      pixelDxDy,
                      const Fsr::Vec2d&      lensDuDv,
                      float                  shutter_percentage,
                      Fsr::RayContext&       Rout,
                      Fsr::RayDifferentials& Rdif) const;


    //! Find the camera-projected coordinate at screen-window NDC coordinate (in -1...+1 range).
    virtual Fsr::Vec3d getDirVector(const RayCamera::Sample& cam,
                                    const Fsr::Vec2d&        screenWindowST) const=0;

    //! Project a world-space point into -1...+1 screen-window (NDC) range.
    virtual Fsr::Vec2d projectPoint(const RayCamera::Sample& cam,
                                    const Fsr::Vec3d&        worldspaceP) const=0;

    //! Project a world-space AABB into -1...+1 screen-window (NDC) range.
    virtual Fsr::Vec2d projectBbox(const RayCamera::Sample& cam,
                                   const Fsr::Box3d&        worldspaceBbox) const;


  protected:
    //! Subclass virtual call.
    virtual void _constructRay(const Fsr::Vec2d&      pixelXY,
                               const Fsr::Vec2d&      pixelDxDy,
                               const Fsr::Vec2d&      lensDuDv,
                               float                  shutter_percentage,
                               Fsr::RayContext&       Rout,
                               Fsr::RayDifferentials* Rdif=NULL) const;

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline double
RayCamera::pixelXToScreenWindowX(double pixelX) const { return ((pixelX - m_fbbox.min.x)/m_fbbox_w)*2.0 - 1.0; }
inline double
RayCamera::pixelYToScreenWindowY(double pixelY) const { return ((pixelY - m_fbbox.min.y)/m_fbbox_h)*2.0 - 1.0; }

inline void
RayCamera::pixelXYToScreenWindowXY(const Fsr::Vec2d& pixelXY,
                                   Fsr::Vec2d&       screenWindowST) const
{
    screenWindowST.x = pixelXToScreenWindowX(pixelXY.x);
    screenWindowST.y = pixelYToScreenWindowY(pixelXY.y);
}


inline void
RayCamera::constructRay(const Fsr::Vec2d& pixelXY,
                        const Fsr::Vec2d& lensDuDv,
                        float             shutter_percentage,
                        Fsr::RayContext&  Rout) const
{
    _constructRay(pixelXY, Fsr::Vec2d(0.0, 0.0), lensDuDv, shutter_percentage, Rout, NULL);
}

inline void
RayCamera::constructRay(const Fsr::Vec2d&      pixelXY,
                        const Fsr::Vec2d&      pixelDxDy,
                        const Fsr::Vec2d&      lensDuDv,
                        float                  shutter_percentage,
                        Fsr::RayContext&       Rout,
                        Fsr::RayDifferentials& Rdif) const
{
    _constructRay(pixelXY, pixelDxDy, lensDuDv, shutter_percentage, Rout, &Rdif);
}


} // namespace zpr

#endif

// end of zprender/RayCamera.h

//
// Copyright 2020 DreamWorks Animation
//
