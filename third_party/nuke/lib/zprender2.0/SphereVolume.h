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

/// @file zprender/SphereVolume.h
///
/// @author Jonathan Egstad


#ifndef zprender_SphereVolume_h
#define zprender_SphereVolume_h

#include "Volume.h"
#include "RenderPrimitive.h"

namespace zpr {

// zpr::SphereVolumePrim enumeration used for VolumeIntersection::object_type:
static const uint32_t  ZprSphereVolumePrim  =  510;


/*!
    TODO: finish!
*/
class ZPR_EXPORT SphereVolume : public RenderPrimitive,
                                public Volume
{
  public:
    /*!
    */
    struct Sample
    {
        Fsr::Mat4d inv_xform;       //!< For transforming ray into sphere's space
        double     radius_near;     //!< Inner shell (optional for point light fakery)
        double     radius_far;      //!< Outside shell

        //! Default ctor leaves junk in vars.
        Sample() {}

        //!
        Sample(const Fsr::Mat4d& _xform,
               double            _radius_near,
               double            _radius_far) :
            inv_xform(_xform.inverse()),
            radius_near(std::min(_radius_near, _radius_far)),
            radius_far(std::max(_radius_near, _radius_far))
        {
            //
        }
    };
    typedef std::vector<Sample> SampleList;


  public:
    //!
    SphereVolume(SurfaceContext*   stx,
                 double            motion_time,
                 const Fsr::Mat4d& xform       =Fsr::Mat4d::getIdentity(),
                 double            radius_near =0.0,
                 double            radius_far  =1.0);

    //!
    SphereVolume(SurfaceContext*                 stx,
                 const Fsr::DoubleList&          motion_times,
                 const SphereVolume::SampleList& motion_spheres);

    /*virtual*/ const char* getClass() const { return "SphereVolume"; }

    //! If this is a volume primitive return this cast to Volume.
    /*virtual*/ Volume* isVolume() { return this; }

    //! This is a traceable primitive, so return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return this; }

    //--------------------------------------------------------------------------------- 

    //! Intersect a ray with the sphere, return true if it did.
    /*virtual*/ bool intersect(RayShaderContext& stx);

    //! Intersect a ray with the sphere.
    /*virtual*/ Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&    stx,
                                                              SurfaceIntersection& I);

    //! Intersect a ray with this sphere, adding two intersections at most.
    /*virtual*/ void getIntersections(RayShaderContext&        stx,
                                      SurfaceIntersectionList& I_list,
                                      double&                  tmin,
                                      double&                  tmax);

    //! Get the AABB for this primitive at an optional time.
    /*virtual*/ Fsr::Box3d getBBoxAtTime(double frame_time);

    //! Print some information about this object.
    /*virtual*/ void printInfo() const;

    //--------------------------------------------------------------------------------- 

    //!
    static Fsr::Box3d getSphereBbox(double            radius_near,
                                    double            radius_far,
                                    const Fsr::Mat4d& xform);

  protected:
    SampleList     m_motion_spheres;       //! Per motion sample list of sphere samples

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline
SphereVolume::SphereVolume(SurfaceContext*   stx,
                           double            motion_time,
                           const Fsr::Mat4d& xform,
                           double            radius_near,
                           double            radius_far) :
    RenderPrimitive(stx, motion_time),
    Volume(2/*nSurfaces*/)
{
    m_motion_spheres.resize(1, Sample(xform, radius_near, radius_far));
}


inline
SphereVolume::SphereVolume(SurfaceContext*                 stx,
                           const Fsr::DoubleList&          motion_times,
                           const SphereVolume::SampleList& motion_spheres) :
    RenderPrimitive(stx, motion_times),
    Volume(2/*nSurfaces*/),
    m_motion_spheres(motion_spheres)
{
#if DEBUG
    assert(m_motion_spheres.size() == m_motion_times.size());
#endif
}


/*static*/
inline Fsr::Box3d
SphereVolume::getSphereBbox(double            radius_near,
                            double            radius_far,
                            const Fsr::Mat4d& xform)
{
    // Make sure far is largest:
    if (radius_far < radius_near) { const double t = radius_near; radius_near = radius_far; radius_far = t; }
    Fsr::Box3d bbox;
    bbox.expand(xform.transform(Fsr::Vec3d(-radius_far, -radius_far, -radius_far)), false/*test_empty*/);
    bbox.expand(xform.transform(Fsr::Vec3d( radius_far, -radius_far, -radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d( radius_far,  radius_far, -radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d(-radius_far,  radius_far, -radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d(-radius_far, -radius_far,  radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d( radius_far, -radius_far,  radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d( radius_far,  radius_far,  radius_far)));
    bbox.expand(xform.transform(Fsr::Vec3d(-radius_far,  radius_far,  radius_far)));
    return bbox;
}


/*virtual*/
inline bool
SphereVolume::intersect(RayShaderContext& stx)
{
    SurfaceIntersection I(std::numeric_limits<double>::infinity());
    return (getFirstIntersection(stx, I) != Fsr::RAY_INTERSECT_NONE);
}


/*virtual*/
inline Fsr::RayIntersectionType
SphereVolume::getFirstIntersection(RayShaderContext&    stx,
                                   SurfaceIntersection& I)
{
    // Find the motion-step this frame_time falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);

    Fsr::RayContext spRtx(stx.Rtx);
    Fsr::RayIntersectionType hit;
    double tmin, tmax;
    static Fsr::Vec3d center(0.0, 0.0, 0.0);
    if (motion_mode == MOTIONSTEP_START)
    {
        const Sample& s0 = m_motion_spheres[motion_step];
        spRtx.transform(s0.inv_xform);
        hit = Fsr::intersectSphere(center, s0.radius_far, spRtx, tmin, tmax);
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        const Sample& s1 = m_motion_spheres[motion_step+1];
        spRtx.transform(s1.inv_xform);
        hit = Fsr::intersectSphere(center, s1.radius_far, spRtx, tmin, tmax);
    }
    else
    {
        const Sample& s0 = m_motion_spheres[motion_step  ];
        const Sample& s1 = m_motion_spheres[motion_step+1];
        spRtx.transform(s0.inv_xform,
                        s1.inv_xform,
                        motion_step_t); // transform ray to volume's space
        hit = Fsr::intersectSphere(center,
                                   Fsr::lerp(s0.radius_far, s1.radius_far, motion_step_t),
                                   spRtx,
                                   tmin, tmax);
    }

    if (hit == Fsr::RAY_INTERSECT_NONE)
        return Fsr::RAY_INTERSECT_NONE;

    I.object      = static_cast<RenderPrimitive*>(this);
    I.object_type = ZprSphereVolumePrim;
    I.object_ref  = 1; // one hit

    I.t           = tmin;
    I.PW          = spRtx.getPositionAt(I.t);
    I.N           = Fsr::Vec3d(0,0,1); // TODO get normal at intersection!

    return Fsr::RAY_INTERSECT_POINT;
}


/*virtual*/
inline void
SphereVolume::getIntersections(RayShaderContext&        stx,
                               SurfaceIntersectionList& I_list,
                               double&                  tmin,
                               double&                  tmax)
{
    //if (R.show_debug_info) std::cout << "    Sphere::getIntersections()" << std::endl;

#if 0
    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;

    double r;
    Fsr::RayContext Rtx(stx.Rtx);
    if (stx.mb_enabled && twin())
    {
        if (stx.frame_time <= m_frame_time)
        {
            r = m_radius_far;
            Rtx.transform(m_motion_inv_xforms[motion_step]); // transform ray to volume's space
        }
        else if (stx.frame_time >= twin()->m_frame_time)
        {
            r = twin()->m_radius_far;
            Rtx.transform(twin()->m_motion_inv_xforms[motion_step]); // transform ray to volume's space
        }
        else
        {
            // Interpolate:
            const double t = (stx.frame_time - m_frame_time) / (twin()->m_frame_time - m_frame_time);
            const double invt = (1.0 - t);
            r = m_radius_far*invt + twin()->m_radius_far*t;
            Fsr::Mat4d interp;
            interp.interpolate(m_motion_inv_xforms[motion_step], twin()->m_motion_inv_xforms[motion_step], t);
            Rtx.transform(interp);
        }
    }
    else
    {
        r = m_radius_far;
        Rtx.transform(m_motion_inv_xforms[motion_step]); // transform ray to volume's space
    }

    //const Fsr::Vec3d p_minus_r(R.origin - m_xform.getTranslation());
    const double a = Rtx.dir().lengthSquared();
    const double b = 2.0 * Rtx.dir().dot(Rtx.origin);
    const double c = Rtx.origin.lengthSquared() - r*r;
    const double discrm = b*b - 4.0*a*c;

    if (discrm >= std::numeric_limits<double>::epsilon())
    {
        const double l = std::sqrt(discrm);
        SurfaceIntersection t_enter((-b - l) / (2.0 * a), this);
        SurfaceIntersection  t_exit((-b + l) / (2.0 * a), this);

        //if (stx.show_debug_info) std::cout << "  hit: tmin=" << t_enter.t << ", tmax=" << t_exit.t << std::endl;
        if (t_exit < t_enter)
            return; // sphere was missed completely
        if (t_exit < stx.Rtx.mindist)
            return; // sphere is behind ray

        t_enter.PW         = stx.Rtx.getPositionAt(t_enter.t);
        t_enter.N          = Fsr::Vec3f(0,0,1); // TODO get normal at intersection!
        t_enter.object_ref = 2; // two hits
        t_exit.PW          = stx.Rtx.getPositionAt(t_exit.t);
        t_exit.N           = Fsr::Vec3f(0,0,1); // TODO get normal at intersection!
        t_exit.object_ref  = -1; // relative offset to first hit
        addIntersectionToList(t_enter, I_list);
        addIntersectionToList(t_exit,  I_list);

        if (t_enter.t < tmin)
            tmin = t_enter.t;
        if (t_exit.t > tmax)
            tmax = t_exit.t;

        return;
    }

    if (::fabs(discrm) < std::numeric_limits<double>::epsilon())
    {
        // Ray is tangent to sphere:
        SurfaceIntersection t_enter(-b / (2.0 * a), this);
        //if (stx.show_debug_info) std::cout << "  tangent hit: t=" << t_enter.t << std::endl;
        if (t_enter < stx.Rtx.mindist)
            return;

        t_enter.object_ref = 1; // only 1 hit
        addIntersectionToList(t_enter, I_list);

        if (t_enter.t < tmin)
            tmin = t_enter.t;

        return;
    }
#endif

    // No hits...
    //if (R.show_debug_info) std::cout << "  no hit" << std::endl;
}


/*virtual*/
inline void
SphereVolume::printInfo() const
{
   std::cout << "Volume::Sphere";
   //std::cout << "Volume::Sphere[" << m_xform.getTranslation();
   //std::cout << " near=" << m_radius_near << ", far=" << m_radius_far << "]";
}


/*virtual*/
inline Fsr::Box3d
SphereVolume::getBBoxAtTime(double frame_time)
{
    Fsr::Box3d bbox;
#if 0
    if (!twin() || frame_time <= m_frame_time)
    {
        getSphereBbox(m_radius_near, m_radius_far, m_xform, bbox);
    }
    else if (frame_time >= twin()->m_frame_time)
    {
        getSphereBbox(twin()->m_radius_near, twin()->m_radius_far, twin()->m_xform, bbox);
    }
    else
    {
        // Interpolate:
        const float t = float((frame_time - m_frame_time) / (twin()->m_frame_time - m_frame_time));
        const float invt = (1.0f - t);
        //
        Fsr::Mat4d time_xform;
        time_xform.interpolate(m_xform, twin()->m_xform, t);
        //
        getSphereBbox(m_radius_near*invt + twin()->m_radius_near*t,
                      m_radius_far*invt  + twin()->m_radius_far*t,
                      time_xform,
                      bbox);
    }
    //std::cout << "sphere" << bbox << " t=" << frame_time << std::endl;
#endif
    return bbox;
}


} // namespace zpr

#endif

// end of zprender/SphereVolume.h

//
// Copyright 2020 DreamWorks Animation
//
