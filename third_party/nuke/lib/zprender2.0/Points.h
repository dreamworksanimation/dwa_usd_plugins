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

/// @file zprender/Points.h
///
/// @author Jonathan Egstad


#ifndef zprender_Points_h
#define zprender_Points_h

#include "Traceable.h"
#include "RenderPrimitive.h"
#include "Bvh.h"

#define MIN_RADIUS 0.01f


namespace zpr {

// zpr::Points enumeration used for SurfaceIntersection::object_type:
static const uint32_t  ZprPointsPrim  =  131;


typedef Bvh<uint32_t>       PointIndexBvh;
typedef BvhObjRef<uint32_t> PointIndexRef;


/*!
*/
class ZPR_EXPORT Points : public Traceable,
                          public RenderPrimitive
{
  public:
    enum PointMode
    {
        SPHERE_POINTS,  //!< Sphere
        DISC_POINTS,    //!< Flat circular dics
        CARD_POINTS,    //!< Flat rectangular card - uses 'aspect' to define rectangle shape
        POINT_POINTS,   //!< Fixed-size circle in sceen space
    };

    PointMode m_mode;   //!< How to draw each point


  public:
    /*!
    */
    struct Sample
    {
        Fsr::Vec3fList P_list;          //!< Per-point position
        Fsr::Vec3fList N_list;          //!< Per-point normal
        Fsr::Vec3fList B_list;          //!< Per-point binormal
        Fsr::Vec3fList vel_list;        //!< Per-point velocity
        Fsr::FloatList r_list;          //!< Per-point radius
        Fsr::Box3f     bbox;            //!< Derived bbox of all points in P_list
    };
    typedef std::vector<Sample> SampleList;

    // Per motion-sample data, public so it can be tweaked after construction if need be:
    SampleList  m_motion_ptcs;          //! Per motion sample list of mesh samples


  protected:
    uint32_t        m_status;           //!< Surface state flags (unexpanded, etc)
    Fsr::Vec3d      m_P_offset;         //!< Positional offset for position data

    // Per-vertex, non-animating attributes:
    Fsr::Vec4fList  m_Cf_list;          //!< Per-point color
    Fsr::FloatList  m_aspect_list;      //!< Per-point aspect ratio (for card mode)

    // Per motion-step:
    std::vector<PointIndexBvh>  m_motion_bvhs;  //!< BVH for points, one per motion-STEP (ie 1 less than motion-samples)


  protected:
    //! Build the BVHs in a thread-safe loop.
    bool expand(const RenderContext& rtx);


  public:
    //!
    Points(SurfaceContext*        stx,
           const Fsr::DoubleList& motion_times,
           const Fsr::Mat4dList&  motion_xforms,
           uint32_t               numPoints,
           const Fsr::Vec3f**     P_arrays,
           const Fsr::Vec3f**     N_arrays=NULL,
           const Fsr::Vec3f**     velocity_arrays=NULL,
           const float**          radii_arrays=NULL,
           const Fsr::Vec4f*      Cf_array=NULL);

    //!
    Points(SurfaceContext*           stx,
           const Fsr::DoubleList&    motion_times,
           const Points::SampleList& motion_ptcs,
           const Fsr::Vec4f*         Cf_array=NULL);

    //! Number of points.
    uint32_t numPoints() const { return (uint32_t)m_motion_ptcs[0].P_list.size(); }


    //!
    Fsr::Vec3f getPoint(uint32_t point) const { return m_motion_ptcs[0].P_list[point]; }
    Fsr::Vec3f getPointMB(uint32_t point,
                          uint32_t motion_step,
                          float    motion_step_t);

    //! Intersect an individual point.
    virtual Fsr::RayIntersectionType pointIntersect(uint32_t             point,
                                                    int                  motion_mode,
                                                    uint32_t             motion_step,
                                                    float                motion_step_t,
                                                    RayShaderContext&    stx,
                                                    SurfaceIntersection& I) =0;


    //! Build the bvh, returns quickly if it's already been built.
    void buildBvh(const RenderContext& rtx,
                  bool                 force=false);

    //! Return the world-space bbox for a point (no offset to origin.)
    Fsr::Box3d getPointBBox(uint32_t point,
                            uint32_t motion_sample=0) const;

    //! Return the local-space bbox for a point (offset to origin.)
    virtual Fsr::Box3f getPointBBoxLocal(uint32_t point,
                                         uint32_t motion_sample=0) const;

    //! Return the world-space bbox for motion sample (not offset to origin.)
    Fsr::Box3d getBBox(uint32_t motion_sample=0) const;

    //! Return the local-space bbox for motion sample (offset to origin.)
    Fsr::Box3f getBBoxLocal(uint32_t motion_sample=0) const;


    /*====================================================*/
    /*               From RenderPrimitive                 */
    /*====================================================*/

    /*virtual*/ const char* getClass() const { return "Points"; }

    //! If this is a traceable primitive return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return static_cast<Traceable*>(this); }

    //! Get the AABB for this primitive at an optional shutter time.
    /*virtual*/ Fsr::Box3d getBBoxAtTime(double frame_time);


    //! Interpolate varying vertex attributes at SurfaceIntersection.
    /*virtual*/ void getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                                        const DD::Image::ChannelSet& mask,
                                                        Fsr::Pixel&                  v) const;
    //! Interpolate varying vertex attributes at SurfaceIntersection. This also calculates derivatives.
    /*virtual*/ void getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                                        const DD::Image::ChannelSet& mask,
                                                        Fsr::Pixel&                  v,
                                                        Fsr::Pixel&                  vdu,
                                                        Fsr::Pixel&                  vdv) const;


    /*====================================================*/
    /*                 From Traceable                     */
    /*====================================================*/

    /*virtual*/ bool intersect(RayShaderContext& stx);
    /*virtual*/ Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&    stx,
                                                              SurfaceIntersection& I);
    /*virtual*/ void getIntersections(RayShaderContext&        stx,
                                      SurfaceIntersectionList& I_list,
                                      double&                  tmin,
                                      double&                  tmax);
};


//---------------------------------------------------------------------------------------


/*! Renders points as spheres.
*/
class SpherePoints : public Points
{
  public:
    //!
    SpherePoints(SurfaceContext*        stx,
                 const Fsr::DoubleList& motion_times,
                 const Fsr::Mat4dList&  motion_xforms,
                 uint32_t               numPoints,
                 const Fsr::Vec3f**     P_arrays,
                 const float**          radii_arrays=NULL,
                 const Fsr::Vec4f*      Cf_array=NULL);

    //! Intersect an individual sphere.
    /*virtual*/ Fsr::RayIntersectionType pointIntersect(uint32_t             point,
                                                        int                  motion_mode,
                                                        uint32_t             motion_step,
                                                        float                motion_step_t,
                                                        RayShaderContext&    stx,
                                                        SurfaceIntersection& I);

    /*virtual*/ const char* getClass() const { return "SpherePoints"; }
};


//---------------------------------------------------------------------------------------


/*! Same as SpherePoints but renders as oriented discs.
*/
class DiscPoints : public Points
{
  public:
    //!
    DiscPoints(SurfaceContext*        stx,
               const Fsr::DoubleList& motion_times,
               const Fsr::Mat4dList&  motion_xforms,
               uint32_t               numPoints,
               const Fsr::Vec3f**     P_arrays,
               const Fsr::Vec3f**     N_arrays=NULL,
               const float**          radii_arrays=NULL,
               const Fsr::Vec4f*      Cf_array=NULL);

    //! Intersect an individual disc.
    /*virtual*/ Fsr::RayIntersectionType pointIntersect(uint32_t             point,
                                                        int                  motion_mode,
                                                        uint32_t             motion_step,
                                                        float                motion_step_t,
                                                        RayShaderContext&    stx,
                                                        SurfaceIntersection& I);

    /*virtual*/ const char* getClass() const { return "DiscPoints"; }
};


//---------------------------------------------------------------------------------------


/*! Same as SpherePoints but renders as oriented discs.
*/
class CardPoints : public Points
{
  public:
    //!
    CardPoints(SurfaceContext*        stx,
               const Fsr::DoubleList& motion_times,
               const Fsr::Mat4dList&  motion_xforms,
               uint32_t               numPoints,
               const Fsr::Vec3f**     P_arrays,
               const Fsr::Vec3f**     N_arrays=NULL,
               const float**          width_arrays=NULL,
               const float*           aspect_array=NULL,
               const Fsr::Vec4f*      Cf_array=NULL);

    //! Intersect an individual card.
    /*virtual*/ Fsr::RayIntersectionType pointIntersect(uint32_t             point,
                                                        int                  motion_mode,
                                                        uint32_t             motion_step,
                                                        float                motion_step_t,
                                                        RayShaderContext&    stx,
                                                        SurfaceIntersection& I);

    /*virtual*/ const char* getClass() const { return "CardPoints"; }
};


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


//!
inline Fsr::Vec3f
Points::getPointMB(uint32_t point,
                   uint32_t motion_step,
                   float    motion_step_t)
{
    return m_motion_ptcs[motion_step].P_list[point].interpolateTo(m_motion_ptcs[motion_step+1].P_list[point], motion_step_t);
}

//--------------------------------------------------------------------------

/*! Return the world bbox for point i.
*/
inline Fsr::Box3d
Points::getPointBBox(uint32_t point,
                     uint32_t motion_sample) const
{
    Fsr::Box3f local_bbox = getPointBBoxLocal(point, motion_sample);
    return Fsr::Box3d(local_bbox.min/* + m_P_offset*/,
                      local_bbox.max/* + m_P_offset*/);
}

/*! Return the local bbox for point i.
*/
/*virtual*/
inline Fsr::Box3f
Points::getPointBBoxLocal(uint32_t point,
                          uint32_t motion_sample) const
{
#if DEBUG
    assert(point < numPoints() && motion_sample < (uint32_t)m_motion_ptcs.size());
#endif

    Fsr::Box3f bbox;
#if 0
    const Fsr::Vec3f& P = m_motion_P[motion_sample][point];
    const float r = (point < m_r.size())?m_r[point]:MIN_RADIUS;
    const Fsr::Vec3f R(r, r, r);
    //std::cout << point << ": r=" << r << std::endl;

    //Fsr::Vec3f scaling = Fsr::Vec3f(1,1,1);
    //float scale = scaling.length();
    bbox.set(P-R, P+R);
    //bbox.expand(points[*vp++]);
#endif

    return bbox;
}

/*! Return the bbox for motion sample.
*/
inline Fsr::Box3d
Points::getBBox(uint32_t motion_sample) const
{
    Fsr::Box3d bbox;
    const uint32_t nPoints = numPoints();
    if (nPoints > 0)
    {
        bbox = getPointBBox(0/*point*/, motion_sample);
        for (uint32_t i=1; i < nPoints; ++i)
            bbox.expand(getPointBBox(i/*point*/, motion_sample));
    }
    return bbox;
}
/*! Return the local-space bbox for motion sample.
*/
inline Fsr::Box3f
Points::getBBoxLocal(uint32_t motion_sample) const
{
    Fsr::Box3f bbox;
    const uint32_t nPoints = numPoints();
    if (nPoints > 0)
    {
        bbox = getPointBBoxLocal(0/*point*/, motion_sample);
        for (uint32_t i=1; i < nPoints; ++i)
            bbox.expand(getPointBBoxLocal(i/*point*/, motion_sample));
    }
    return bbox;
}


} // namespace zpr

#endif

// end of zprender/Points.h

//
// Copyright 2020 DreamWorks Animation
//
