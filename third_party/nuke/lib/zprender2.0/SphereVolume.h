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

#include "LightVolume.h"

namespace zpr {

// zpr::SphereVolumePrim enumeration used for VolumeIntersection::object_type:
static const uint32_t  ZprSphereVolumePrim  =  510;


/*!
    TODO: finish!
*/
class ZPR_EXPORT SphereVolume : public LightVolume
{
  public:
    /*!
    */
    struct Sample
    {
        Fsr::Mat4d inv_xform;       //!< For transforming ray into sphere's space
        double     near_radius;     //!< Inner shell (optional for point light fakery)
        double     far_radius;      //!< Outside shell

        //! Default ctor leaves junk in vars.
        Sample() {}

        //!
        Sample(const Fsr::Mat4d& _xform,
               double            _near_radius,
               double            _far_radius)
        {
            set(_xform, _near_radius, _far_radius);
        }

        //!
        void set(const Fsr::Mat4d& _xform,
                 double            _near_radius,
                 double            _far_radius)
        {
            inv_xform   = _xform.inverse();
            near_radius = std::min(::fabs(_near_radius), ::fabs(_far_radius));
            far_radius  = std::max(::fabs(_near_radius), ::fabs(_far_radius));
        }

    };
    typedef std::vector<Sample> SampleList;


  public:
    //!
    SphereVolume(const MaterialContext* material_info,
                 double                 motion_time,
                 const Fsr::Mat4d&      xform       =Fsr::Mat4d::getIdentity(),
                 double                 near_radius =0.0,
                 double                 far_radius  =1.0);

    //!
    SphereVolume(const MaterialContext*          material_info,
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
    static Fsr::Box3d getSphereBbox(double            near_radius,
                                    double            far_radius,
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
SphereVolume::SphereVolume(const MaterialContext* material_info,
                           double                 motion_time,
                           const Fsr::Mat4d&      xform,
                           double                 near_radius,
                           double                 far_radius) :
    LightVolume(material_info, motion_time)
{
    m_motion_spheres.resize(1, Sample(xform, near_radius, far_radius));
}


inline
SphereVolume::SphereVolume(const MaterialContext*          material_info,
                           const Fsr::DoubleList&          motion_times,
                           const SphereVolume::SampleList& motion_spheres) :
    LightVolume(material_info, motion_times),
    m_motion_spheres(motion_spheres)
{
#if DEBUG
    assert(m_motion_spheres.size() == m_motion_times.size());
#endif
}


/*static*/
inline Fsr::Box3d
SphereVolume::getSphereBbox(double            near_radius,
                            double            far_radius,
                            const Fsr::Mat4d& xform)
{
    // Make sure far is largest:
    if (far_radius < near_radius) { const double t = near_radius; near_radius = far_radius; far_radius = t; }
    Fsr::Box3d bbox;
    bbox.expand(xform.transform(Fsr::Vec3d(-far_radius, -far_radius, -far_radius)), false/*test_empty*/);
    bbox.expand(xform.transform(Fsr::Vec3d( far_radius, -far_radius, -far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d( far_radius,  far_radius, -far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d(-far_radius,  far_radius, -far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d(-far_radius, -far_radius,  far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d( far_radius, -far_radius,  far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d( far_radius,  far_radius,  far_radius)));
    bbox.expand(xform.transform(Fsr::Vec3d(-far_radius,  far_radius,  far_radius)));
    return bbox;
}


/*!
*/
inline bool
intersectSphere(const Fsr::Vec3d&           Ro,
                const Fsr::Vec3d&           Rd,
                const SphereVolume::Sample& sphere,
                double&                     tmin,
                double&                     tmax)
{
    const double a = Rd.lengthSquared();
    const double b = 2.0 * Rd.dot(Ro);
    const double c = (Ro.lengthSquared() - double(sphere.far_radius*sphere.far_radius));

    const double discrm = b*b - 4.0*a*c;
    if (discrm >= std::numeric_limits<double>::epsilon())
    {
        const double l = std::sqrt(discrm);
        tmin = (-b - l) / (2.0 * a);
        tmax = (-b + l) / (2.0 * a);
        if (tmin < 0.0 && tmax < 0.0)
            return false; // sphere behind origin
        return true;
    }
    if (::fabs(discrm) < std::numeric_limits<double>::epsilon())
    {
        // Ray is tangent to sphere:
        tmin = tmax = -b / (2.0 * a);
        if (tmin < 0.0)
            return false; // sphere behind origin
        return true;
    }

    return false;
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
        hit = Fsr::intersectSphere(center, s0.far_radius, spRtx, tmin, tmax);
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        const Sample& s1 = m_motion_spheres[motion_step+1];
        spRtx.transform(s1.inv_xform);
        hit = Fsr::intersectSphere(center, s1.far_radius, spRtx, tmin, tmax);
    }
    else
    {
        const Sample& s0 = m_motion_spheres[motion_step  ];
        const Sample& s1 = m_motion_spheres[motion_step+1];
        spRtx.transform(s0.inv_xform,
                        s1.inv_xform,
                        motion_step_t); // transform ray to volume's space
        hit = Fsr::intersectSphere(center,
                                   Fsr::lerp(s0.far_radius, s1.far_radius, motion_step_t),
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
    //std::cout << "    Sphere::getIntersections()" << std::endl;

#if DEBUG
    assert(m_motion_spheres.size() > 0);
#endif
    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_spheres.size());
#endif

    // Transform the ray origin and direction by the sphere's inverse xform:
    if (motion_mode == MOTIONSTEP_START)
    {
        // No interpolation first sample:
        const Sample& sphere0 = m_motion_spheres[motion_step];
        if (sphere0.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, sphere is too small

        const Fsr::Vec3d Ro = sphere0.inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = sphere0.inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectSphere(Ro, Rd, sphere0, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        // No interpolation second sample:
        const Sample& sphere1 = m_motion_spheres[motion_step+1];
        if (sphere1.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, sphere is too small

        const Fsr::Vec3d Ro = sphere1.inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = sphere1.inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectSphere(Ro, Rd, sphere1, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }
    else
    {
        // Interpolate sphere inv_xforms:
        const Sample& sphere0 = m_motion_spheres[motion_step  ];
        const Sample& sphere1 = m_motion_spheres[motion_step+1];
        if (sphere0.far_radius < std::numeric_limits<double>::epsilon() ||
            sphere1.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, sphere is too small
#if DEBUG
        assert((motion_step+1) < m_motion_spheres.size());
#endif
        const Fsr::Mat4d interp_inv_xform(Fsr::lerp(sphere0.inv_xform, sphere1.inv_xform, motion_step_t));
        const Fsr::Vec3d Ro = interp_inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = interp_inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectSphere(Ro, Rd, sphere0, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }

#if 0
    double r;
    Fsr::RayContext Rtx(stx.Rtx);
    if (stx.mb_enabled && twin())
    {
        if (stx.frame_time <= m_frame_time)
        {
            r = m_far_radius;
            Rtx.transform(m_motion_inv_xforms[motion_step]); // transform ray to volume's space
        }
        else if (stx.frame_time >= twin()->m_frame_time)
        {
            r = twin()->m_far_radius;
            Rtx.transform(twin()->m_motion_inv_xforms[motion_step]); // transform ray to volume's space
        }
        else
        {
            // Interpolate:
            const double t = (stx.frame_time - m_frame_time) / (twin()->m_frame_time - m_frame_time);
            const double invt = (1.0 - t);
            r = m_far_radius*invt + twin()->m_far_radius*t;
            Fsr::Mat4d interp;
            interp.interpolate(m_motion_inv_xforms[motion_step], twin()->m_motion_inv_xforms[motion_step], t);
            Rtx.transform(interp);
        }
    }
    else
    {
        r = m_far_radius;
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
   //std::cout << " near=" << m_near_radius << ", far=" << m_far_radius << "]";
}


/*virtual*/
inline Fsr::Box3d
SphereVolume::getBBoxAtTime(double frame_time)
{
    Fsr::Box3d bbox;
#if 0
    if (!twin() || frame_time <= m_frame_time)
    {
        getSphereBbox(m_near_radius, m_far_radius, m_xform, bbox);
    }
    else if (frame_time >= twin()->m_frame_time)
    {
        getSphereBbox(twin()->m_near_radius, twin()->m_far_radius, twin()->m_xform, bbox);
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
        getSphereBbox(m_near_radius*invt + twin()->m_near_radius*t,
                      m_far_radius*invt  + twin()->m_far_radius*t,
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
