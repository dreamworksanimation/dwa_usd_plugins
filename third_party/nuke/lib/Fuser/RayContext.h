//
// Copyright 2019 DreamWorks Animation
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

/// @file Fuser/RayContext.h
///
/// @author Jonathan Egstad

#ifndef Fuser_RayContext_h
#define Fuser_RayContext_h

#include "Vec2.h"
#include "Vec3.h"
#include "Mat4.h"
#include "Box3.h"
#include "Time.h" // for defaultTimeValue

#include <DDImage/Raycast.h> // for DD::Image compatibility convenience


namespace Fsr {

//-------------------------------------------------------------------------


/*!
*/
enum RayIntersectionType
{
    RAY_INTERSECT_ABORT    = -2,    //!< User-abort ocurred during intersection testing
    RAY_INTERSECT_ERROR    = -1,    //!< Object cannot be traced against (degenerate, no spatial info, etc)
    RAY_INTERSECT_NONE     =  0,    //!< No intersection
    RAY_INTERSECT_POINT    =  1,    //!< Intersection is a single point
    RAY_INTERSECT_SEGMENT  =  2,    //!< Intersection is a linear segment (two points)
    RAY_INTERSECT_PLANE    =  3,    //!< Intersection is a plane (one point)
    RAY_INTERSECT_RAY      =  4,    //!< Intersection is a ray (origin & direction)
    RAY_INTERSECT_MULTIPLE =  5,    //!< Multiple intersections (volume)
    RAY_INTERSECT_BVH_LEAF =  6     //!< Intersection is a BVH leaf node
};


//-------------------------------------------------------------------------


/*! \class Fsr::RayContext

    All values are double-precision to reduce runtime float->double conversions during
    intersection tests.

*/
class FSR_EXPORT RayContext
{
  public:
    Fsr::Vec3d      origin;     //! Ray's origin location (xyz position in space)
    //
    Fsr::TimeValue  time;       //!< Ray's current time. If value is Fsr::isNotAnimated(time) then time is unspecified (the default.)
    double          mindist;    //!< Ray start distance - intersection tests should fail if intersection is nearer than this
    double          maxdist;    //!< Ray end distance   - intersection tests should fail if intersection is farther than this
    //
    uint32_t        type_mask;  //!< Ray type mask


  public:
    // Ray types:
    enum RayTypeMask
    {
        CAMERA       = 0x00000001,      //!< Ray coming from camera (to camera actually...)
        SHADOW       = 0x00000002,      //!< Ray from surface to light
        REFLECTION   = 0x00000004,      //!< Ray reflected off a surface
        TRANSMISSION = 0x00000008,      //!< Ray refraced or transmitted through a surface
        //
        DIFFUSE      = 0x00000010,      //!<
        GLOSSY       = 0x00000020       //!< 
    };


  protected:
    // Ray direction is protected to keep inv_direction and slope indicators up to date.
    Fsr::Vec3d  m_dir;                  //!< Ray's direction (a normal) double-precision to reduce conversions
    Fsr::Vec3d  m_inv_dir;              //!< Direction reciprocal, for intersection test speedups
    bool        m_slope_positive[3];    //!< xyz positive slope indicator - true if slope is positive for an axis

    //! Recalcs the speedup vars - do this after any change to ray direction.
    void _updateSlopes();


  public:
    //! Base constructor doesn't initialize the contents.
    RayContext() {}

    //! TODO: do we need the templating anymore?
    template <typename T>
    RayContext(const Fsr::Vec3<T>& _origin,
               const Fsr::Vec3<T>& _dir,
               double              _mindist=std::numeric_limits<double>::epsilon(),
               double              _maxdist=std::numeric_limits<double>::infinity());
    //! TODO: do we need the templating anymore?
    template <typename T>
    RayContext(const Fsr::Vec3<T>& _origin,
               const Fsr::Vec3<T>& _dir,
               double              _time,
               double              _mindist=std::numeric_limits<double>::epsilon(),
               double              _maxdist=std::numeric_limits<double>::infinity());


    //! Construct from DD::Image::Ray. For DD::Image compatibility convenience.
    explicit RayContext(const DD::Image::Ray& b) { *this = b; }

    //! Copy from DD::Image::Ray. For DD::Image compatibility convenience.
    RayContext& operator =  (const DD::Image::Ray& b);


  public:
    //! Ray's direction vector
    const Fsr::Vec3d& dir()     const { return m_dir; }
    const Fsr::Vec3d& inv_dir() const { return m_inv_dir; }

    bool isXSlopePositive() const { return m_slope_positive[0]; }
    bool isYSlopePositive() const { return m_slope_positive[1]; }
    bool isZSlopePositive() const { return m_slope_positive[2]; }

    //! TODO: do we need the templating anymore?
    template <typename T>
    void set(const Fsr::Vec3<T>& _origin,
             const Fsr::Vec3<T>& _dir,
             double              _mindist=std::numeric_limits<double>::epsilon(),
             double              _maxdist=std::numeric_limits<double>::infinity());

    //!
    void    setOrigin(const Fsr::Vec3d& _origin) { origin = _origin; }
    void setDirection(const Fsr::Vec3d& _dir)    { m_dir = _dir; _updateSlopes(); }

    //!
    Fsr::Vec3d getPositionAt(double t) const { return origin + (m_dir*t); }

    //! Transform the ray origin and direction by a matrix.
    void transform(const Fsr::Mat4d& m);

};



//-------------------------------------------------------------------------



/*! \class Fsr::RayContextDif

    Adds ray-differential direction vectors.
*/
class FSR_EXPORT RayContextDif : public RayContext
{
  protected:
    // Directions are protected to keep inv_direction and slope indicators up to date:
    Fsr::Vec3d  m_xdir;         //!< Ray x-differential direction normal
    Fsr::Vec3d  m_ydir;         //!< Ray y-differential direction normal


  public:
    //! Base constructor doesn't initialize the contents.
    RayContextDif() {}

    //! Ctor sets both differential direction normals to primary direction.
    template <typename T>
    RayContextDif(const Fsr::Vec3<T>& _origin,
                  const Fsr::Vec3<T>& _dir,
                  double              _mindist=std::numeric_limits<double>::epsilon(),
                  double              _maxdist=std::numeric_limits<double>::infinity());
    template <typename T>
    RayContextDif(const Fsr::Vec3<T>& _origin,
                  const Fsr::Vec3<T>& _dir,
                  double              _time,
                  double              _mindist=std::numeric_limits<double>::epsilon(),
                  double              _maxdist=std::numeric_limits<double>::infinity());


    //! X/Y differential direction vectors
    const Fsr::Vec3d& xdir() const { return m_xdir; }
    const Fsr::Vec3d& ydir() const { return m_ydir; }


    //! Initialize the differentials from two angles (in radians).
    void initializeFromAngle(const Fsr::RayContext& primary,
                             double angleX,
                             double angleY);
};



/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/


//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const RayContext& r);


//----------------------------------------------------------------------


//! Basic ray-AABB (Axis-Aligned-Bounding-Box) intersect test.
template<typename T>
FSR_EXPORT
bool intersectAABB(const Fsr::Box3<T>& bbox,
                   const Fsr::Vec3d&   ray_origin,
                   const Fsr::Vec3d&   ray_dir,
                   double&             tmin,
                   double&             tmax);
template<typename T>
FSR_EXPORT
bool intersectAABB(const Fsr::Box3<T>& bbox,
                   const Fsr::Vec3d&   ray_origin,
                   const Fsr::Vec3d&   ray_dir);

//! Accelerated ray-AABB (Axis-Aligned-Bounding-Box) intersect test.
template<typename T>
FSR_EXPORT
bool intersectAABB(const Fsr::Box3<T>&    bbox,
                   const Fsr::RayContext& Rtx,
                   double&                tmin,
                   double&                tmax);
template<typename T>
FSR_EXPORT
bool intersectAABB(const Fsr::Box3<T>&    bbox,
                   const Fsr::RayContext& Rtx);


//-------------------------------------------------------------------------


/*! Ray-sphere intersect test.
*/
FSR_EXPORT
bool intersectSphere(const Fsr::Vec3d&      P,
                     float                  radius,
                     const Fsr::RayContext& Rtx);
FSR_EXPORT
RayIntersectionType intersectSphere(const Fsr::Vec3d&      P,
                                    float                  radius,
                                    const Fsr::RayContext& Rtx,
                                    double&                tmin,
                                    double&                tmax);


//-------------------------------------------------------------------------


/*! Ray-plane intersect test.
*/
FSR_EXPORT
bool intersectPlane(const Fsr::Vec4d&      planeXYZD,
                    const Fsr::RayContext& Rtx);
FSR_EXPORT
bool intersectPlane(const Fsr::Vec4d&      planeXYZD,
                    const Fsr::RayContext& Rtx,
                    double&                t);


//-------------------------------------------------------------------------


/*! Ray-triangle intersect test against either front or back sides.
    'vert_origin' is required to cheaply offset ray origin into vert-local
    space without pre-converting each vert or modifying the RayContext.
*/
FSR_EXPORT
bool intersectTriangle(const Fsr::Vec3d&      vert_origin,
                       const Fsr::Vec3f&      v0,
                       const Fsr::Vec3f&      v1,
                       const Fsr::Vec3f&      v2,
                       const Fsr::RayContext& Rtx,
                       Fsr::Vec2f&            uv,
                       double&                t);
FSR_EXPORT
bool intersectTriangle(const Fsr::Vec3d&         vert_origin,
                       const Fsr::Vec3f&         v0,
                       const Fsr::Vec3f&         v1,
                       const Fsr::Vec3f&         v2,
                       const Fsr::RayContextDif& Rdtx,
                       Fsr::Vec2f&               uv,
                       Fsr::Vec2f&               uvdx,
                       Fsr::Vec2f&               uvdy,
                       double&                   t);

/*! Ray-triangle intersect test against one of the front/back sides.
    'vert_origin' is required to cheaply offset ray origin into vert-local
    space without pre-converting each vert or modifying the RayContext.
*/
FSR_EXPORT
bool intersectTriangleSided(bool                   front_side,
                            const Fsr::Vec3d&      vert_origin,
                            const Fsr::Vec3f&      v0,
                            const Fsr::Vec3f&      v1,
                            const Fsr::Vec3f&      v2,
                            const Fsr::RayContext& Rtx,
                            Fsr::Vec2f&            uv,
                            double&                t);
FSR_EXPORT
bool intersectTriangleSided(bool                      front_side,
                            const Fsr::Vec3d&         vert_origin,
                            const Fsr::Vec3f&         v0,
                            const Fsr::Vec3f&         v1,
                            const Fsr::Vec3f&         v2,
                            const Fsr::RayContextDif& Rdtx,
                            Fsr::Vec2f&               uv,
                            Fsr::Vec2f&               uvdx,
                            Fsr::Vec2f&               uvdy,
                            double&                   t);


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


//!
template<typename T>
inline
RayContext::RayContext(const Fsr::Vec3<T>& _origin,
                       const Fsr::Vec3<T>& _dir,
                       double              _time,
                       double              _mindist,
                       double              _maxdist) :
    time(_time),
    mindist(_mindist),
    maxdist(_maxdist),
    type_mask(CAMERA)
{
    setOrigin(_origin);
    setDirection(_dir);
}
template<typename T>
inline
RayContext::RayContext(const Fsr::Vec3<T>& _origin,
                       const Fsr::Vec3<T>& _dir,
                       double              _mindist,
                       double              _maxdist) :
    time(Fsr::defaultTimeValue()),
    mindist(_mindist),
    maxdist(_maxdist),
    type_mask(CAMERA)
{
    setOrigin(_origin);
    setDirection(_dir);
}


//! Copy from DD::Image::Ray. For DD::Image compatibility convenience.
inline RayContext&
RayContext::operator =  (const DD::Image::Ray& b)
{
    origin  = b.src;
    m_dir   = b.dir;
    mindist = double(b.minT);
    maxdist = double(b.maxT);
    _updateSlopes();
    return *this;
}

inline std::ostream& operator << (std::ostream& o, const RayContext& r)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << r.origin << r.dir() << " " << r.mindist << "..." << r.maxdist;
    return o ;
}

inline void
RayContext::_updateSlopes()
{
    m_inv_dir.set(1.0/m_dir.x, 1.0/m_dir.y, 1.0/m_dir.z);
    m_slope_positive[0] = (m_inv_dir.x < 0.0);
    m_slope_positive[1] = (m_inv_dir.y < 0.0);
    m_slope_positive[2] = (m_inv_dir.z < 0.0);
}

template<typename T>
inline void
RayContext::set(const Fsr::Vec3<T>& _origin,
                const Fsr::Vec3<T>& _dir,
                double              _mindist,
                double              _maxdist)
{
    setOrigin(_origin);
    setDirection(_dir);
    mindist = _mindist;
    maxdist = _maxdist;
}

inline void
RayContext::transform(const Fsr::Mat4d& m)
{
#if 1
    // TODO: verify the dir transform math here
    origin = m.transform(origin);
    m_dir  = m.vecTransform(m_dir);
#else
    m_dir  = m.transform(origin + m_dir);
    origin = m.transform(origin);
    m_dir -= origin;
    m_dir.normalize();
#endif
    _updateSlopes();
}

//-----------------------------------------------------------

template <typename T>
RayContextDif::RayContextDif(const Fsr::Vec3<T>& _origin,
                             const Fsr::Vec3<T>& _dir,
                             double              _time,
                             double              _mindist,
                             double              _maxdist) :
    RayContext(_origin, _dir, _time, _mindist, _maxdist),
    m_xdir(_dir),
    m_ydir(_dir)
{
    //
}

template <typename T>
RayContextDif::RayContextDif(const Fsr::Vec3<T>& _origin,
                             const Fsr::Vec3<T>& _dir,
                             double              _mindist,
                             double              _maxdist) :
    RayContext(_origin, _dir, Fsr::defaultTimeValue(), _mindist, _maxdist),
    m_xdir(_dir),
    m_ydir(_dir)
{
    //
}

inline void
RayContextDif::initializeFromAngle(const Fsr::RayContext& primary,
                                   double                 angleX,
                                   double                 angleY)
{
    angleX = ::fabs(angleX);
    if (angleX > std::numeric_limits<double>::epsilon())
    {
        if (angleX > M_PI)
            angleX = M_PI;
        const double dx = std::sin(angleX);
        const double dz = std::cos(angleX);
        m_xdir.set(dx, 0.0, dz);
        m_xdir.orientAroundNormal(primary.dir());
    }
    else
        m_xdir = primary.dir();

    angleY = ::fabs(angleY);
    if (angleY > std::numeric_limits<double>::epsilon())
    {
        if (angleY > M_PI)
            angleY = M_PI;
        const double dy = std::sin(angleY);
        const double dz = std::cos(angleY);
        m_ydir.set(0.0, dy, dz);
        m_ydir.orientAroundNormal(primary.dir());
    }
    else
        m_ydir = primary.dir();
}

//-----------------------------------------------------------

/*! Basic ray-AABB (Axis-Aligned-Bounding-Box) intersect test.
    Uses a slower intersection alogrithm that must calculate a slope per plane.
*/
template<typename T>
inline bool
intersectAABB(const Fsr::Box3<T>& bbox,
              const Fsr::Vec3d&   ray_origin,
              const Fsr::Vec3d&   ray_dir,
              double&             tmin,
              double&             tmax)
{
    tmin = -std::numeric_limits<double>::infinity();
    tmax =  std::numeric_limits<double>::infinity();
    // Test against each xyz near/far planes:
    for (int i=0; i < 3; ++i)
    {
        const double origin = ray_origin[i];
        const double dir    = ray_dir[i];
        if (std::fabs(dir) < std::numeric_limits<T>::epsilon())
        {
            if (origin < double(bbox.min[i]) ||
                origin > double(bbox.max[i]))
                return false;
        }
        else
        {
            const double t0 = (double(bbox.min[i]) - origin) / dir;
            const double t1 = (double(bbox.max[i]) - origin) / dir;
            if (t0 < t1)
            {
                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);
            }
            else
            {
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t0);
            }
            if (tmin > tmax)
                return false;
        }
    }
}
template<typename T>
inline bool
intersectAABB(const Fsr::Box3<T>& bbox,
              const Fsr::Vec3d&   ray_origin,
              const Fsr::Vec3d&   ray_dir)
{
    double tmin, tmax;
    return intersectAABB(bbox, ray_origin, ray_dir, tmin, tmax);
}

/*! Accelerated ray-AABB (Axis-Aligned-Bounding-Box) intersect test.
    Requires the precalculated xyz slopes in RayContext to be up to date.
*/
template<typename T>
inline bool
intersectAABB(const Fsr::Box3<T>&    bbox,
              const Fsr::RayContext& Rtx,
              double&                tmin,
              double&                tmax)
{
    // Speedy code - accelerate plane tests using precalculated ray slope values
    double tmin0, tmax0, tmin1, tmax1;
    // X ---------------------------------------------------------------
    if (Rtx.isXSlopePositive())
    {
       tmin0 = (double(bbox.max.x) - Rtx.origin.x)*Rtx.inv_dir().x;
       tmax0 = (double(bbox.min.x) - Rtx.origin.x)*Rtx.inv_dir().x;
    }
    else
    {
       tmin0 = (double(bbox.min.x) - Rtx.origin.x)*Rtx.inv_dir().x;
       tmax0 = (double(bbox.max.x) - Rtx.origin.x)*Rtx.inv_dir().x;
    }
    // Y ---------------------------------------------------------------
    if (Rtx.isYSlopePositive())
    {
       tmin1 = (double(bbox.max.y) - Rtx.origin.y)*Rtx.inv_dir().y; if (tmin1 > tmax0) return false;
       tmax1 = (double(bbox.min.y) - Rtx.origin.y)*Rtx.inv_dir().y; if (tmax1 < tmin0) return false;
    }
    else
    {
       tmin1 = (double(bbox.min.y) - Rtx.origin.y)*Rtx.inv_dir().y; if (tmin1 > tmax0) return false;
       tmax1 = (double(bbox.max.y) - Rtx.origin.y)*Rtx.inv_dir().y; if (tmax1 < tmin0) return false;
    }
    if (tmin1 > tmin0) tmin0 = tmin1;
    if (tmax1 < tmax0) tmax0 = tmax1;
    // Z ---------------------------------------------------------------
    if (Rtx.isZSlopePositive())
    {
       tmin1 = (double(bbox.max.z) - Rtx.origin.z)*Rtx.inv_dir().z; if (tmin1 > tmax0) return false;
       tmax1 = (double(bbox.min.z) - Rtx.origin.z)*Rtx.inv_dir().z; if (tmax1 < tmin0) return false;
    }
    else
    {
       tmin1 = (double(bbox.min.z) - Rtx.origin.z)*Rtx.inv_dir().z; if (tmin1 > tmax0) return false;
       tmax1 = (double(bbox.max.z) - Rtx.origin.z)*Rtx.inv_dir().z; if (tmax1 < tmin0) return false;
    }
    tmin = (tmin1 > tmin0) ? tmin1 : tmin0;
    tmax = (tmax1 < tmax0) ? tmax1 : tmax0;

    return (tmax >= Rtx.mindist && tmin <= Rtx.maxdist);
}
template<typename T>
inline bool
intersectAABB(const Fsr::Box3<T>&    bbox,
              const Fsr::RayContext& Rtx)
{
    double tmin, tmax;
    return intersectAABB(bbox, Rtx, tmin, tmax);
}

//-----------------------------------------------------------

inline bool
intersectSphere(const Fsr::Vec3d&      P,
                float                  radius,
                const Fsr::RayContext& Rtx)
{
    const Fsr::Vec3d v(Rtx.origin - P);
    const double B = v.dot(Rtx.dir());
    return ((B*B - (v.lengthSquared() - double(radius*radius))) > std::numeric_limits<double>::epsilon());
}

inline RayIntersectionType
intersectSphere(const Fsr::Vec3d&      P,
                float                  radius,
                const Fsr::RayContext& Rtx,
                double&                tmin,
                double&                tmax)
{
    const Fsr::Vec3d p_minus_r(Rtx.origin - P);
    const double a = Rtx.dir().lengthSquared();
    const double b = 2.0 * Rtx.dir().dot(p_minus_r);
    const double c = p_minus_r.lengthSquared() - radius*radius;

    const double discrm = b*b - 4.0*a*c;
    if (discrm >= std::numeric_limits<double>::epsilon())
    {
        const double l = std::sqrt(discrm);
        tmin = (-b - l) / (2.0 * a);
        tmax = (-b + l) / (2.0 * a);
        if (tmin < 0.0 && tmax < 0.0)
            return RAY_INTERSECT_NONE; // sphere behind origin
        return RAY_INTERSECT_SEGMENT;
    }
    if (::fabs(discrm) < std::numeric_limits<double>::epsilon())
    {
        // Ray is tangent to sphere:
        tmin = tmax = -b / (2.0 * a);
        if (tmin < 0.0)
            return RAY_INTERSECT_NONE; // sphere behind origin
        return RAY_INTERSECT_POINT;
    }
    return RAY_INTERSECT_NONE;
}

//-----------------------------------------------------------

inline bool
intersectPlane(const Fsr::Vec4d&      planeXYZD,
               const Fsr::RayContext& Rtx)
{
    const Fsr::Vec3d& N = (const Fsr::Vec3d&)planeXYZD;
    if (::fabs(Rtx.dir().dot(N)) < std::numeric_limits<double>::epsilon())
    {
        if (::fabs(Rtx.origin.dot(N) + planeXYZD.w) < std::numeric_limits<double>::epsilon())
            return true; // ray is contained inside the plane
        return false; // ray is parallel to plane
    }
    return true;
}

inline bool
intersectPlane(const Fsr::Vec4d&      planeXYZD,
               const Fsr::RayContext& Rtx,
               double&                t)
{
    const Fsr::Vec3d& N = (const Fsr::Vec3d&)planeXYZD;
    const double rd_dot_n = Rtx.dir().dot(N);
    if (::fabs(rd_dot_n) < std::numeric_limits<double>::epsilon())
    {
        t = Rtx.origin.dot(N) + planeXYZD.w;
        if (::fabs(t) < std::numeric_limits<double>::epsilon())
        {
            t = 0.0;
            return true; // ray origin is on plane
        }
        t = std::numeric_limits<double>::infinity();
        return false; // ray is parallel to plane
    }
    t = Rtx.origin.dot(N) + planeXYZD.w;
    return true;
}

//-----------------------------------------------------------

inline bool
intersectDisc(const Fsr::Vec3d&      P,
              const Fsr::Vec3d&      N,
              double                 radius,
              const Fsr::RayContext& Rtx)
{
#if 1
    return false;
#else
    // TODO: finish implementation!
#endif
}

inline bool
intersectDisc(const Fsr::Vec3d&      P,
              const Fsr::Vec3d&      N,
              double                 radius,
              const Fsr::RayContext& Rtx,
              double&                t)
{
#if 1
    return false;
#else
    // TODO: finish implementation!

    const double rd_dot_n = Rtx.dir().dot(N);

    // Is ray contained inside the disk plane or is parallel to plane?
    if (::fabsf(rd_dot_n) < std::numeric_limits<double>::epsilon())
     {
        t = Rtx.origin.dot(N) + plane.w;
        if (::fabs(t) < std::numeric_limits<double>::epsilon())
        {
            t = 0.0;
            return true; // ray origin is on plane
        }
        t = std::numeric_limits<double>::infinity();
        return false; // ray is parallel to plane
    }
    t = Rtx.origin.dot(N) + plane.w;
    return true;
#if 0
    // Distance from intersection to P:
    const double D = -P.dot(N); // distance to origin (plane's D value)
    const double t = -(R.origin.dot(N) + D) / rd_dot_n;
    const DD::Image::Vector3 iP = R.get_point_at(t / rd_dot_n);

    return (iP.distanceSquared(P) > radius*radius);
#endif
#endif
}

//-----------------------------------------------------------

/*! Basic ray-triangle intersect test.  This is static so that it can be used
    by other classes as a generic triangle test.

    If it returns true then uv contains the barycentric coordinate and t is the
    distance of the intersection from the ray origin.

    'vert_origin' is required to cheaply offset ray origin into vert-local space
    without pre-converting each vert or modifying the RayContext. ie the RayContext
    origin is offset by this value (Rtx.origin - vert_origin) before being used.
    Set to 0,0,0 if unsure about the transformation of point data.

    This technique is used to reduce precision issues with single-precision point
    data by allowing them to be kept close to the origin and storing a single offset
    to locate them in world space.

    The single-precision points are promoted to double-precision within the test to
    increase intersection accuracy, especially for thin (single-sided) geometry
    which is common in Nuke 3D scenes.

    This test is from the "Fast, Minimum Storage Ray/Triangle Intersection" paper
    by Tomas Moller & Ben Trumbore (1997).
*/
inline bool
intersectTriangleSided(bool                   front_side,
                       const Fsr::Vec3d&      vert_origin,
                       const Fsr::Vec3f&      v0,
                       const Fsr::Vec3f&      v1,
                       const Fsr::Vec3f&      v2,
                       const Fsr::RayContext& Rtx,
                       Fsr::Vec2f&            uv,
                       double&                t)
{
    const Fsr::Vec3d e1(v1 - v0); // edge 1
    const Fsr::Vec3d e2(v2 - v0); // edge 2

    const Fsr::Vec3d pvec = Rtx.dir().cross(e2);
    double det = e1.dot(pvec);

    // Front/back test:
    if (front_side)
    {
        if (det < std::numeric_limits<double>::epsilon())
            return false; // hit back, bail
    }
    else
    {
        if (det > -std::numeric_limits<double>::epsilon())
            return false; // hit front, bail
    }

    const Fsr::Vec3d tvec((Rtx.origin - vert_origin) - v0);
    const double u = tvec.dot(pvec); // barycentric u coord
    if (u < 0.0 || (u - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    const Fsr::Vec3d qvec(tvec.cross(e1));
    const double v = Rtx.dir().dot(qvec); // barycentric v coord
    if (v < 0.0 || (u + v - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    det = 1.0 / det;
    t = e2.dot(qvec)*det; // intersection distance from ray origin
    if (t < Rtx.mindist || t > Rtx.maxdist)
        return false; // outside ray's range, bail

    uv.x = float(u*det);
    uv.y = float(v*det);

    return true;
}
inline bool
intersectTriangleSided(bool                      front_side,
                       const Fsr::Vec3d&         vert_origin,
                       const Fsr::Vec3f&         v0,
                       const Fsr::Vec3f&         v1,
                       const Fsr::Vec3f&         v2,
                       const Fsr::RayContextDif& Rdtx,
                       Fsr::Vec2f&               uv,
                       Fsr::Vec2f&               uvdx,
                       Fsr::Vec2f&               uvdy,
                       double&                   t)
{
    const Fsr::Vec3d e1(v1 - v0); // edge 1
    const Fsr::Vec3d e2(v2 - v0); // edge 2

    Fsr::Vec3d pvec(Rdtx.dir().cross(e2));
    double det = e1.dot(pvec);

    // Front/back test:
    if (front_side)
    {
        if (det < std::numeric_limits<double>::epsilon())
            return false; // hit back, bail
    }
    else
    {
        if (det > -std::numeric_limits<double>::epsilon())
            return false; // hit front, bail
    }

    const Fsr::Vec3d tvec((Rdtx.origin - vert_origin) - v0);
    const double u = tvec.dot(pvec); // barycentric u coord
    if (u < 0.0 || (u - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    Fsr::Vec3d qvec(tvec.cross(e1));
    const double v = Rdtx.dir().dot(qvec); // barycentric v coord
    if (v < 0.0 || (u + v - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    det = 1.0 / det;
    t = e2.dot(qvec)*det; // intersection distance from ray origin
    if (t < Rdtx.mindist || t > Rdtx.maxdist)
        return false; // outside ray's range, bail

    uv.x = float(u*det);
    uv.y = float(v*det);

    // du differential:
    pvec = Rdtx.xdir().cross(e2);
    qvec = tvec.cross(e1);
    det  = 1.0 / e1.dot(pvec);
    uvdx.x = float(tvec.dot(pvec)*det);
    uvdx.y = float(Rdtx.xdir().dot(qvec)*det);

    // dv differential:
    pvec = Rdtx.ydir().cross(e2);
    qvec = tvec.cross(e1);
    det  = 1.0 / e1.dot(pvec);
    uvdy.x = float(tvec.dot(pvec)*det);
    uvdy.y = float(Rdtx.ydir().dot(qvec)*det);

    return true;
}

inline bool
intersectTriangle(const Fsr::Vec3d&      vert_origin,
                  const Fsr::Vec3f&      v0,
                  const Fsr::Vec3f&      v1,
                  const Fsr::Vec3f&      v2,
                  const Fsr::RayContext& Rtx,
                  Fsr::Vec2f&            uv,
                  double&                t)
{
    const Fsr::Vec3d e1(v1 - v0); // edge 1
    const Fsr::Vec3d e2(v2 - v0); // edge 2

    const Fsr::Vec3d pvec = Rtx.dir().cross(e2);
    double det = e1.dot(pvec);

    // Check for ray parallel to plane:
    if (fabs(det) < std::numeric_limits<double>::epsilon())
        return false; // edge-on, bail

    const Fsr::Vec3d tvec((Rtx.origin - vert_origin) - v0);
    const double u = tvec.dot(pvec); // barycentric u coord
    if (u < 0.0 || (u - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    const Fsr::Vec3d qvec(tvec.cross(e1));
    const double v = Rtx.dir().dot(qvec); // barycentric v coord
    if (v < 0.0 || (u + v - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    det = 1.0 / det;
    t = e2.dot(qvec)*det; // intersection distance from ray origin
    if (t < Rtx.mindist || t > Rtx.maxdist)
        return false; // outside ray's range, bail

    uv.x = float(u*det);
    uv.y = float(v*det);

    return true;
}
inline bool
intersectTriangle(const Fsr::Vec3d&         vert_origin,
                  const Fsr::Vec3f&         v0,
                  const Fsr::Vec3f&         v1,
                  const Fsr::Vec3f&         v2,
                  const Fsr::RayContextDif& Rdtx,
                  Fsr::Vec2f&               uv,
                  Fsr::Vec2f&               uvdx,
                  Fsr::Vec2f&               uvdy,
                  double&                   t)
{
    const Fsr::Vec3d e1(v1 - v0); // edge 1
    const Fsr::Vec3d e2(v2 - v0); // edge 2

    Fsr::Vec3d pvec(Rdtx.dir().cross(e2));
    double det = e1.dot(pvec);

    // Check for ray parallel to plane:
    if (fabs(det) < std::numeric_limits<double>::epsilon())
        return false; // edge-on, bail

    const Fsr::Vec3d tvec((Rdtx.origin - vert_origin) - v0);
    const double u = tvec.dot(pvec); // barycentric u coord
    if (u < 0.0 || (u - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    Fsr::Vec3d qvec(tvec.cross(e1));
    const double v = Rdtx.dir().dot(qvec); // barycentric v coord
    if (v < 0.0 || (u + v - std::numeric_limits<double>::epsilon()) > det)
        return false; // outside perimeter, bail

    det = 1.0 / det;
    t = e2.dot(qvec)*det; // intersection distance from ray origin
    if (t < Rdtx.mindist || t > Rdtx.maxdist)
        return false; // outside ray's range, bail

    uv.x = float(u*det);
    uv.y = float(v*det);

    // du differential:
    pvec = Rdtx.xdir().cross(e2);
    qvec = tvec.cross(e1);
    det  = 1.0 / e1.dot(pvec);
    uvdx.x = float(tvec.dot(pvec)*det);
    uvdx.y = float(Rdtx.xdir().dot(qvec)*det);

    // dv differential:
    pvec = Rdtx.ydir().cross(e2);
    qvec = tvec.cross(e1);
    det  = 1.0 / e1.dot(pvec);
    uvdy.x = float(tvec.dot(pvec)*det);
    uvdy.y = float(Rdtx.ydir().dot(qvec)*det);

    return true;
}


} // namespace Fsr


#endif

// end of Ray.h

//
// Copyright 2019 DreamWorks Animation
//
