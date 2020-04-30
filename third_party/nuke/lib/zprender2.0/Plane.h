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

/// @file zprender/Plane.h
///
/// @author Jonathan Egstad


#ifndef zprender_Plane_h
#define zprender_Plane_h

#include "Traceable.h"


namespace zpr {

// zpr::Points enumeration used for SurfaceIntersection::object_type:
static const uint32_t  ZprPlanePrim  =  131;


/*!
*/
class ZPR_EXPORT Plane : public Traceable
{
  public:
    double A, B, C, D;


  public:
    Plane() : Traceable() {}
    Plane(double a, double b, double c, double d) : Traceable(), A(a), B(b), C(c), D(d) {}

    //! If this is a traceable primitive return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return this; }

    /*virtual*/ const char* getClass() const { return "Plane"; }

    //--------------------------------------------------------------------------------- 

    //!
    const Fsr::Vec3d& normal() const              { return *(reinterpret_cast<const Fsr::Vec3d*>(&A)); }
    void              normal(const Fsr::Vec3d& n) { A = n.x; B = n.y; C = n.z; }
    //!
    void              setD(const Fsr::Vec3d& v) { D = v.dot(normal()); }

    //!
    double intersection(const Fsr::RayContext& Rtx) const { return Rtx.origin.dot(normal()) + D; }

    //--------------------------------------------------------------------------------- 

    /*! Intersect a ray with this plane.
        Returns: 0 = disjoint (no intersect)
                 1 = intersect in unique point I.t
    */
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

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


/*virtual*/
inline bool
Plane::intersect(RayShaderContext& stx)
{
    if (::fabs(stx.Rtx.dir().dot(normal())) < std::numeric_limits<double>::epsilon())
    {
        if (::fabs(stx.Rtx.origin.dot(normal()) + D) < std::numeric_limits<double>::epsilon())
            return true; // Ray is contained inside the plane
        return false; // Ray is parallel to plane
    }
    return true;
}


/* Intersect a ray with this plane.
   Returns: 0 = disjoint (no intersect)
            1 = intersect in unique point I.t
*/
/*virtual*/
inline Fsr::RayIntersectionType
Plane::getFirstIntersection(RayShaderContext&    stx,
                            SurfaceIntersection& I)
{
    const double rd_dot_n = stx.Rtx.dir().dot(normal());
    if (::fabs(rd_dot_n) < std::numeric_limits<double>::epsilon())
    {
        I.t = intersection(stx.Rtx);
        if (::fabs(I.t) < std::numeric_limits<double>::epsilon())
        {
            // Ray is contained inside the plane:
            I.t  = 0.0;
            I.Ng = I.Ns = I.N = normal();
            return Fsr::RAY_INTERSECT_POINT;
        }
        // Ray is parallel to plane:
        I.t = std::numeric_limits<double>::infinity();
        return Fsr::RAY_INTERSECT_NONE;
    }

    I.object      = static_cast<RenderPrimitive*>(this);
    I.object_type = ZprPlanePrim;
    I.object_ref  = 1; // only one hit

    I.t           = -intersection(stx.Rtx) / rd_dot_n;
    I.PW          = stx.Rtx.getPositionAt(I.t);
    I.Ng          = I.Ns = I.N = normal();

    return Fsr::RAY_INTERSECT_POINT;
}


/*! Intersect a ray with all surfaces of this object. Returns the type of intersection code.
*/
/*virtual*/
inline void
Plane::getIntersections(RayShaderContext&        stx,
                        SurfaceIntersectionList& I_list,
                        double&                  tmin,
                        double&                  tmax)
{
    SurfaceIntersection I(std::numeric_limits<double>::infinity());
    if (getFirstIntersection(stx, I) > Fsr::RAY_INTERSECT_NONE)
        I_list.push_back(I);
}


/*! Get the AABB for this primitive at an optional time. */
/*virtual*/
inline Fsr::Box3d
Plane::getBBoxAtTime(double frame_time)
{
    // TODO: implement!
    return Fsr::Box3d();
}


/*virtual*/
inline void
Plane::printInfo() const
{
    std::cout << "[" << A << " " << B << " " << C << " " << D << "]";
}


} // namespace zpr

#endif

// end of zprender/Plane.h

//
// Copyright 2020 DreamWorks Animation
//
