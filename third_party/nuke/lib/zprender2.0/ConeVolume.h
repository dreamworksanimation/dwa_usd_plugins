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

/// @file zprender/ConeVolume.h
///
/// @author Jonathan Egstad


#ifndef zprender_ConeVolume_h
#define zprender_ConeVolume_h

#include "Volume.h"
#include "Disc.h"


namespace zpr {

// zpr::ConeVolumePrim enumeration used for VolumeIntersection::object_type:
static const uint32_t  ZprConeVolumePrim  =  520;



/*! Ray-traceable cone volume.
    Hack method using six planes to define the bounding volume of the cone,
    then test distance from center to determine if we're inside the cone volume
    itself (like a spotlight calc.)
    Problem with this hack is that we start ray-marching from the plane surface
    rather than the cone surface which produces a different sampling pattern
    depending on the rotation of the frustum to camera...
*/
class ZPR_EXPORT ConeVolume : public RenderPrimitive,
                              public Volume
{
  public:
    /*!
    */
    struct Sample
    {
        double     near;        //!< Distance to near end of cone from base
        double     near_radius; //!< Radius at near end cap
        double     far;         //!< Distance to far end of cone from base
        double     far_radius;  //!< Radius at far end cap
        Fsr::Mat4d xform;       //!< Center point is getTranslation(), dir vec is getZAxis()
        Fsr::Mat4d inv_xform;   //!< Base of cone is 0,0,0

        double m_far_radius_sqr_by_far_sqr; //!< For intersection speedup


        //! Default ctor leaves junk in vars.
        Sample() {}

        //!
        Sample(const Fsr::Mat4d& _xform,
               double            _angle,
               double            _near,
               double            _far)
        {
            set(_xform, _angle, _near, _far);
        }

        //!
        void set(const Fsr::Mat4d& _xform,
                 double            _angle,
                 double            _near,
                 double            _far)
        {
            xform = _xform;
            // Rotate cone to Y-up orientation:
            inv_xform.setToRotationX(radians(90.0));
            inv_xform *= _xform.inverse();

            near = std::min(::fabs(_near), ::fabs(_far));
            far  = std::max(::fabs(_near), ::fabs(_far));
            ConeVolume::getConeRadii(::fabs(_angle), near, far, near_radius, far_radius); // calc cap radii:

            // Precalc for intersection speedup:
            m_far_radius_sqr_by_far_sqr = (far_radius*far_radius) / (far*far);
        }

    };
    typedef std::vector<Sample> SampleList;


  public:
    //!
    ConeVolume(SurfaceContext*   stx,
               double            motion_time,
               const Fsr::Mat4d& xform,
               double            angle,
               double            near,
               double            far);

    //!
    ConeVolume(SurfaceContext*         stx,
               const Fsr::DoubleList&  motion_times,
               const ConeVolume::SampleList& motion_cones);

    /*virtual*/ const char* getClass() const { return "ConeVolume"; }


    //! If this is a volume primitive return this cast to Volume.
    /*virtual*/ Volume* isVolume() { return this; }

    //! This is a traceable primitive, so return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return this; }


    //--------------------------------------------------------------------------------- 

    //! Intersect a ray with this disc.
    /*virtual*/ bool intersect(RayShaderContext& stx);


    //! Intersect a ray with the plane.
    /*virtual*/ Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&    stx,
                                                              SurfaceIntersection& I);

    //! Intersect a ray with this plane, adding one intersection at most.
    /*virtual*/ void getIntersections(RayShaderContext&        stx,
                                      SurfaceIntersectionList& I_list,
                                      double&                  tmin,
                                      double&                  tmax);

    //! Get the AABB for this primitive at an optional time.
    /*virtual*/ Fsr::Box3d getBBoxAtTime(double frame_time);

    //! Print some information about this object.
    /*virtual*/ void printInfo() const;

    //--------------------------------------------------------------------------------- 

    //! Calc radius of near/far end caps.
    static void getConeRadii(double  angle,
                             double  near,
                             double  far,
                             double& near_radius,
                             double& far_radius);


    static Fsr::Box3d getConeBbox(double            angle,
                                  double            near,
                                  double            far,
                                  const Fsr::Mat4d& xform);

    static Fsr::Box3d getConeBbox(double            near,
                                  double            far,
                                  double            near_radius,
                                  double            far_radius,
                                  const Fsr::Mat4d& xform);

    static Fsr::Box3d getConeBbox(const Sample& cone);


  protected:
    SampleList m_motion_cones;      //! Per motion sample list of cone samples

};


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

inline
ConeVolume::ConeVolume(SurfaceContext*   stx,
                       double            motion_time,
                       const Fsr::Mat4d& xform,
                       double            angle,
                       double            near,
                       double            far) :
    RenderPrimitive(stx, motion_time),
    Volume(2/*nSurfaces*/)
{
    m_motion_cones.resize(1, Sample(xform, angle, near, far));
}

inline
ConeVolume::ConeVolume(SurfaceContext*               stx,
                       const Fsr::DoubleList&        motion_times,
                       const ConeVolume::SampleList& motion_cones) :
    RenderPrimitive(stx, motion_times),
    Volume(2/*nSurfaces*/),
    m_motion_cones(motion_cones)
{
#if DEBUG
    assert(m_motion_cones.size() == m_motion_times.size());
#endif
}


/*! Calc radius of near/far end caps.
*/
/*static*/
inline void
ConeVolume::getConeRadii(double  angle,
                         double  near,
                         double  far,
                         double& near_radius,
                         double& far_radius)
{
    const double lens = (1.0 / ::tan(radians(angle / 2.0))) / 2.0;
    near_radius = (near >= std::numeric_limits<double>::epsilon()) ? (near / lens / 2.0) : 0.0;
    far_radius  = (near >= std::numeric_limits<double>::epsilon()) ? ( far / lens / 2.0) : 0.0;
}


/*static*/
inline Fsr::Box3d
ConeVolume::getConeBbox(double            near,
                        double            far,
                        double            near_radius,
                        double            far_radius,
                        const Fsr::Mat4d& xform)
{
    Fsr::Box3d bbox;
    if (far < std::numeric_limits<double>::epsilon()          ||
        (far - near) < std::numeric_limits<double>::epsilon() ||
        near_radius < std::numeric_limits<double>::epsilon()  ||
        far_radius < std::numeric_limits<double>::epsilon())
        return bbox;
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius, -near_radius, -near)), false/*test_empty*/);
    bbox.expand(xform.transform(Fsr::Vec3d( -far_radius,  -far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius,  near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d( near_radius, -near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d( near_radius,  near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(  far_radius,   far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d( near_radius, -near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(  far_radius,  -far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius, -near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius,  near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d( -far_radius,   far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d( near_radius,  near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius, -near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(-near_radius,  near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d( near_radius, -near_radius, -near)));
    bbox.expand(xform.transform(Fsr::Vec3d(  far_radius,  -far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d(  far_radius,   far_radius,  -far)));
    bbox.expand(xform.transform(Fsr::Vec3d( -far_radius,  -far_radius,  -far)));
    return bbox;
}

/*static*/
inline Fsr::Box3d
ConeVolume::getConeBbox(double            angle,
                        double            near,
                        double            far,
                        const Fsr::Mat4d& xform)
{
    // Make sure far is largest:
    if (far < near)
        std::swap(near, far);
    double near_radius, far_radius;
    getConeRadii(angle, near, far, near_radius, far_radius);
    return getConeBbox(near, far, near_radius, far_radius, xform);
}

/*static*/
inline Fsr::Box3d
ConeVolume::getConeBbox(const Sample& cone)
{
    return getConeBbox(cone.near, cone.far, cone.near_radius, cone.far_radius, cone.xform);
}



/*!
*/
inline bool
intersectCone(const Fsr::Vec3d&         Ro,
              const Fsr::Vec3d&         Rd,
              const ConeVolume::Sample& cone,
              double&                   tmin,
              double&                   tmax)
{
    double t0 =  0.0; // t distance to intersected surface
    double y0 = -1.0; // distance from base to intersected surface

    // Test against end caps first as it's cheaper:
    if (::fabs(Rd.y) >= std::numeric_limits<double>::epsilon())
    {
        // Test far cap disc:
        const double     tfar = (cone.far - Ro.y) / Rd.y;
        const Fsr::Vec3d Pfar = Ro + (Rd*tfar);
        const double     dfar = (Pfar.x*Pfar.x + Pfar.z*Pfar.z);
        if (dfar <= (cone.far_radius*cone.far_radius))
        {
            // Far cap hit:
            t0 = tfar;
            y0 = Pfar.y;
        }

        // Test near cap disc if it has size:
        if (cone.near_radius >= std::numeric_limits<double>::epsilon())
        {
            const double     tnear = (cone.near - Ro.y) / Rd.y;
            const Fsr::Vec3d Pnear = Ro + (Rd*tnear);
            const double     dnear = (Pnear.x*Pnear.x + Pnear.z*Pnear.z);
            if (dnear <= (cone.near_radius*cone.near_radius))
            {
                // If the far cap has been hit then both have been:
                if (y0 >= 0.0)
                {
                    tmin = tnear;
                    tmax = tfar;
                    return true; // both caps hit, no need to test conic section
                }
                // Only near cap hit, on to conic section:
                t0 = tnear;
                y0 = Pnear.y;
            }
        }
    }

    // Conic section test:
    const double a = (Rd.x*Rd.x + Rd.z*Rd.z - Rd.y*Rd.y*cone.m_far_radius_sqr_by_far_sqr);
    const double b = (Ro.x*Rd.x + Ro.z*Rd.z - Ro.y*Rd.y*cone.m_far_radius_sqr_by_far_sqr)*2.0;
    const double c = (Ro.x*Ro.x + Ro.z*Ro.z - Ro.y*Ro.y*cone.m_far_radius_sqr_by_far_sqr);
    double       d = (b*b) - (4.0*a*c);
    if (d <= std::numeric_limits<double>::epsilon()) 
        return false; // miss, or only a point hit

    d = std::sqrt(d);

    // We're guaranteed to have two conic section hits - find the ones
    // between the end caps by comparing the heights (in Y) of the hits:
    double t1 = (-b + d) / (2.0*a);
    double y1 = (Ro + (Rd*t1)).y;
    if (y1 >= cone.near && y1 <= cone.far)
    {
        if (y0 >= 0.0)
        {
            tmin = t0;
            tmax = t1;
            return true; // two hits all done
        }
        t0 = t1;
        y0 = y1;
    }

    t1 = (-b - d) / (2.0*a);
    y1 = (Ro + (Rd*t1)).y;
    if (y1 >= cone.near && y1 <= cone.far)
    {
        if (y0 >= 0.0)
        {
            tmin = t0;
            tmax = t1;
            return true; // two hits all done
        }
    }

    return false;// miss!
}


/*virtual*/
inline bool
ConeVolume::intersect(RayShaderContext& stx)
{
    return false;
}


/*virtual*/
inline Fsr::RayIntersectionType
ConeVolume::getFirstIntersection(RayShaderContext&    stx,
                                 SurfaceIntersection& I)
{
    return Fsr::RAY_INTERSECT_NONE;
}


/*virtual*/
inline void
ConeVolume::getIntersections(RayShaderContext&        stx,
                             SurfaceIntersectionList& I_list,
                             double&                  tmin,
                             double&                  tmax)
{
#if DEBUG
    assert(m_motion_cones.size() > 0);
#endif
    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_cones.size());
#endif

    // Transform the ray origin and direction by the cone's inverse xform:
    if (motion_mode == MOTIONSTEP_START)
    {
        // No interpolation first sample:
        const Sample& cone0 = m_motion_cones[motion_step];
        if (cone0.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, cone is too small

        const Fsr::Vec3d Ro = cone0.inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = cone0.inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectCone(Ro, Rd, cone0, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        // No interpolation second sample:
        const Sample& cone1 = m_motion_cones[motion_step+1];
        if (cone1.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, cone is too small

        const Fsr::Vec3d Ro = cone1.inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = cone1.inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectCone(Ro, Rd, cone1, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }
    else
    {
        // Interpolate cone inv_xforms:
        const Sample& cone0 = m_motion_cones[motion_step  ];
        const Sample& cone1 = m_motion_cones[motion_step+1];
        if (cone0.far_radius < std::numeric_limits<double>::epsilon() ||
            cone1.far_radius < std::numeric_limits<double>::epsilon())
            return; // miss, cone is too small
#if DEBUG
        assert((motion_step+1) < m_motion_cones.size());
#endif
        const Fsr::Mat4d interp_inv_xform(Fsr::lerp(cone0.inv_xform, cone1.inv_xform, motion_step_t));
        const Fsr::Vec3d Ro = interp_inv_xform.transform(stx.Rtx.origin);
        const Fsr::Vec3d Rd = interp_inv_xform.vecTransform(stx.Rtx.dir());
        double t0, t1;
        if (intersectCone(Ro, Rd, cone0, t0, t1))
            Volume::addVolumeIntersection(t0, t1, this, stx.Rtx, I_list, tmin, tmax);
    }

}


/*virtual*/
inline Fsr::Box3d
ConeVolume::getBBoxAtTime(double frame_time)
{
#if DEBUG
    assert(m_motion_cones.size() > 0);
#endif

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_cones.size());
#endif

    if (motion_mode == MOTIONSTEP_START)
        return getConeBbox(m_motion_cones[motion_step]);
    else if (motion_mode == MOTIONSTEP_END)
        return getConeBbox(m_motion_cones[motion_step+1]);

#if DEBUG
    assert((motion_step+1) < m_motion_cones.size());
#endif
    // Only support motionblurred xform:
    const Sample& cone0 = m_motion_cones[motion_step  ];
    const Sample& cone1 = m_motion_cones[motion_step+1];
    return getConeBbox(cone0.near, cone0.far, cone0.near_radius, cone0.far_radius,
                       Fsr::lerp(cone0.xform, cone1.xform, motion_step_t));
}


/*virtual*/
inline void
ConeVolume::printInfo() const
{
    //std::cout << "ConeVolume[" << " angle=" << m_angle << " near=" << m_near << ", far=" << m_far;
}



} // namespace zpr

#endif

// end of zprender/ConeVolume.h

//
// Copyright 2020 DreamWorks Animation
//
