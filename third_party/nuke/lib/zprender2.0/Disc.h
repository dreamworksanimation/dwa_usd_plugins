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

/// @file zprender/Disc.h
///
/// @author Jonathan Egstad


#ifndef zprender_Disc_h
#define zprender_Disc_h

#include "Traceable.h"
#include "RenderPrimitive.h"

namespace zpr {

// zpr::Points enumeration used for SurfaceIntersection::object_type:
static const uint32_t  ZprDiscPrim  =  141;


/*!
*/
class ZPR_EXPORT Disc : public Traceable,
                        public RenderPrimitive
{
    /*!
    */
    struct Sample
    {
        Fsr::Vec3d P;       //!< Center point
        Fsr::Vec3d N;       //!<
        double     radius;  //!<

        //! Default ctor leaves junk in vars.
        Sample() {}

        //!
        Sample(const Fsr::Vec3d& _P,
               const Fsr::Vec3d& _N,
               double            _radius) :
            P(_P),
            N(_N),
            radius(_radius)
        {
            //
        }
    };
    typedef std::vector<Sample> SampleList;


  public:
    //!
    Disc(SurfaceContext*   stx,
         double            motion_time,
         const Fsr::Vec3d& P,
         const Fsr::Vec3d& N,
         double            radius);

    //!
    Disc(SurfaceContext*         stx,
         const Fsr::DoubleList&  motion_times,
         const Disc::SampleList& motion_discs);

    /*virtual*/ const char* getClass() const { return "Disc"; }

    //! If this is a traceable primitive return this cast to Traceable.
    virtual Traceable* isTraceable() { return this; }

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

  protected:
    SampleList  m_motion_discs;     //! Per motion sample list of disc samples

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline
Disc::Disc(SurfaceContext*   stx,
           double            motion_time,
           const Fsr::Vec3d& P,
           const Fsr::Vec3d& N,
           double            radius) :
    Traceable(),
    RenderPrimitive(stx, motion_time)
{
    m_motion_discs.resize(1, Sample(P, N, radius));
}

inline
Disc::Disc(SurfaceContext*         stx,
           const Fsr::DoubleList&  motion_times,
           const Disc::SampleList& motion_discs) :
    Traceable(),
    RenderPrimitive(stx, motion_times),
    m_motion_discs(motion_discs)
{
#if DEBUG
    assert(m_motion_discs.size() == m_motion_times.size());
#endif
}

/*virtual*/
inline bool
Disc::intersect(RayShaderContext& stx)
{
    //return ((stx.Rtx.origin.dot(m_N) + m_D) > std::numeric_limits<double>::epsilon());
    return false;
}


/*virtual*/
inline Fsr::RayIntersectionType
Disc::getFirstIntersection(RayShaderContext&    stx,
                           SurfaceIntersection& I)
{
    // Find the motion-step this frame_time falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);

    Fsr::RayIntersectionType hit = Fsr::RAY_INTERSECT_NONE;
    double tmin;
    Fsr::Vec3d iPW;
    if (motion_mode == MOTIONSTEP_START)
    {
        const Sample& s0 = m_motion_discs[motion_step];
        if (Fsr::intersectDisc(s0.P, s0.N, s0.radius, stx.Rtx, tmin))
        {
            // Determine distance from center:
            iPW = stx.Rtx.getPositionAt(tmin);
            if (iPW.distanceSquared(s0.P) > s0.radius*s0.radius)
                hit = Fsr::RAY_INTERSECT_POINT;
        }
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        const Sample& s1 = m_motion_discs[motion_step+1];
        if (Fsr::intersectDisc(s1.P, s1.N, s1.radius, stx.Rtx, tmin))
        {
            // Determine distance from center:
            iPW = stx.Rtx.getPositionAt(tmin);
            if (iPW.distanceSquared(s1.P) > s1.radius*s1.radius)
                hit = Fsr::RAY_INTERSECT_POINT;
        }
    }
    else
    {
        const Sample& s0 = m_motion_discs[motion_step  ];
        const Sample& s1 = m_motion_discs[motion_step+1];
        const Fsr::Vec3d P = Fsr::lerp(s0.P, s1.P, motion_step_t);
        const Fsr::Vec3d N = Fsr::lerp(s0.N, s1.N, motion_step_t);
        const double     r = Fsr::lerp(s0.radius, s1.radius, motion_step_t);
        if (Fsr::intersectDisc(P, N, r, stx.Rtx, tmin))
        {
            // Determine distance from center:
            iPW = stx.Rtx.getPositionAt(tmin);
            if (iPW.distanceSquared(P) > r*r)
                hit = Fsr::RAY_INTERSECT_POINT;
        }
    }

    if (hit == Fsr::RAY_INTERSECT_NONE)
        return Fsr::RAY_INTERSECT_NONE;

    I.t           = tmin;
    I.object      = static_cast<RenderPrimitive*>(this);
    I.object_type = ZprDiscPrim;
    I.object_ref  = 1;    // one hit
    I.PW          = iPW;
    I.N           = Fsr::Vec3d(0,0,1); // TODO get normal at intersection!

    return Fsr::RAY_INTERSECT_POINT;
}
#if 0
    // First do a plane intersection:
    const double rd_dot_n = stx.Rtx.dir().dot(m_N);
    const double t = stx.Rtx.origin.dot(m_N) + m_D;
    if (::fabs(rd_dot_n) < std::numeric_limits<double>::epsilon())
    {
        I.t = t;
        if (::fabs(I.t) < std::numeric_limits<double>::epsilon())
        {
            // Ray is contained inside the disk plane:
            I.t           = 0.0;
            I.PW          = stx.Rtx.origin;
            I.N = I.Ns    = m_N;
            I.object      = static_cast<RenderPrimitive*>(this);
            I.object_type = ZprDiscPrim;
            I.object_ref  = 1;    // one hit
            return Fsr::RAY_INTERSECT_POINT;
        }
        // Ray is parallel to disk plane:
        I.t = std::numeric_limits<double>::infinity();
        return Fsr::RAY_INTERSECT_NONE;
    }

    I.t  = -t / rd_dot_n;
    I.PW = stx.Rtx.getPositionAt(I.t);

    // Determine distance from center:
    if (I.PW.distanceSquared(m_P) > m_radius*m_radius)
        return Fsr::RAY_INTERSECT_NONE;

    I.Ng          = I.N = I.Ns = m_N;
    I.object      = static_cast<RenderPrimitive*>(this);
    I.object_type = ZprDiscPrim;
    I.object_ref  = 1;    // one hit
#endif


/*virtual*/
inline void
Disc::getIntersections(RayShaderContext&        stx,
                       SurfaceIntersectionList& I_list,
                       double&                  tmin,
                       double&                  tmax)
{
    SurfaceIntersection I(std::numeric_limits<double>::infinity());
    if (getFirstIntersection(stx, I) > Fsr::RAY_INTERSECT_NONE)
        I_list.push_back(I);
}


/*virtual*/
inline Fsr::Box3d
Disc::getBBoxAtTime(double frame_time)
{
    // TODO: implement!
    return Fsr::Box3d();
}


/*virtual*/
inline void
Disc::printInfo() const
{
    std::cout << "Volume::Disc";
}


} // namespace zpr

#endif

// end of zprender/Disc.h

//
// Copyright 2020 DreamWorks Animation
//
