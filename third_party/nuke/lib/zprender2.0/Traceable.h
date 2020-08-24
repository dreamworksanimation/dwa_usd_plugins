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

/// @file zprender/Traceable.h
///
/// @author Jonathan Egstad


#ifndef zprender_Traceable_h
#define zprender_Traceable_h

#include "RayShaderContext.h"

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel



// TODO: switch to OpenDCX lib for this stuff? We're deprecating the 8x8 support...
#if 0
#  include "DeepPixelHandler.h" // for SpMask8
#else
namespace Dcx {
typedef uint64_t SpMask8;
static const SpMask8 SPMASK_ZERO_COVERAGE = 0x0ull;
static const SpMask8 SPMASK_FULL_COVERAGE = 0xffffffffffffffffull;
}
#endif



namespace zpr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

//!
ZPR_EXPORT Fsr::Vec3f
getTriGeometricNormal(const Fsr::Vec3f& p0,
                      const Fsr::Vec3f& p1,
                      const Fsr::Vec3f& p2);


//!
ZPR_EXPORT Fsr::Vec3f
getQuadGeometricNormal(const Fsr::Vec3f& p0,
                       const Fsr::Vec3f& p1,
                       const Fsr::Vec3f& p2,
                       const Fsr::Vec3f& p3);

//!
ZPR_EXPORT Fsr::Vec3f
getNormalAtBbox(const Fsr::Box3f& bbox,
                const Fsr::Vec3f& P);


/*! Find the st coordinate of 2D coord vP inside 2D triangle corners.
    Returns false if uv are outside triangle bounds.
*/
ZPR_EXPORT bool
getStCoordInsideTriangleAt(const Fsr::Vec2d& vP,
                           const Fsr::Vec2d& v0,
                           const Fsr::Vec2d& v1,
                           const Fsr::Vec2d& v2,
                           Fsr::Vec2f&       st_out);


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// zpr::Traceable prim enumerations start with this one. Used for SurfaceIntersection::object_type:
static const uint32_t  ZprTracable  =  10;



/*! Add ray tracing capabilities to an object.
*/
class ZPR_EXPORT Traceable
{
  public:
    //--------------------------------------------------------------------------------- 

    /*! Stores surface intersection information that may be motion-blurred.
    */
    struct SurfaceIntersection
    {
        //Fsr::RayIntersectionType type;  //! Type of intersection
        double   t;             //!< Distance from R.origin to intersection point vtx.PW
        //
        void*    object;        //!< Object pointer for this intersection
        uint32_t object_type;   //!< Object type used to cast the object pointer
        int32_t  object_ref;    //!< If positive: the number of intersections of same object, if negative: offset to first intersection
        //
        int32_t  part_index;    //!< Object part index used for a face index, point index, etc. -1 indicates unused
        int32_t  subpart_index; //!< Part subpart index used for part of a face, etc. -1 indicates unused

        /* Surface Params: */
        Fsr::Vec2f st;          //!< Primitive's parametric coordinate at intersection
        Fsr::Vec2f Rxst;        //!< X-offset parametric coordinate at intersection
        Fsr::Vec2f Ryst;        //!< Y-offset parametric coordinate at intersection
        //
        Fsr::Vec3d PW;          //!< Surface point w/displacement
        Fsr::Vec3d RxPW;        //!< PW at x-derivative offset
        Fsr::Vec3d RyPW;        //!< PW at y-derivative offset
        Fsr::Vec3d PWg;         //!< Geometric surface point (no displacement)
        //
        Fsr::Vec3f N;           //!< Interpolated surface normal (vertex normal) possibly with bump
        Fsr::Vec3f RxN;         //!< N at x-derivative offset
        Fsr::Vec3f RyN;         //!< N at y-derivative offset
        Fsr::Vec3f Ni;          //!< Interpolated surface normal
        Fsr::Vec3f Ng;          //!< Geometric surface normal


        // Default ctor leaves junk in var.
        SurfaceIntersection() {}

        //!
        SurfaceIntersection(double _t,
                            void*  _object= NULL) :
            t(_t),
            object(_object),
            object_type(0/*no type*/),
            object_ref(1/*one hit*/),
            part_index(-1/*no part*/),
            subpart_index(-1/*no subpart*/)
        {
            //
        }

        /* Comparison operators test the 't's of the intersection: */
        bool operator <  (const SurfaceIntersection& i) const { return (t <  i.t); }
        bool operator <= (const SurfaceIntersection& i) const { return (t <= i.t); }
        bool operator >  (const SurfaceIntersection& i) const { return (t >  i.t); }
        bool operator >= (const SurfaceIntersection& i) const { return (t >= i.t); }
        bool operator == (const SurfaceIntersection& i) const { return (::fabs(t - i.t) <  std::numeric_limits<double>::epsilon()); }
        bool operator != (const SurfaceIntersection& i) const { return (::fabs(t - i.t) >= std::numeric_limits<double>::epsilon()); }
        bool operator <  (double v) const { return (t <  v); }
        bool operator <= (double v) const { return (t <= v); }
        bool operator >  (double v) const { return (t >  v); }
        bool operator >= (double v) const { return (t >= v); }
        bool operator == (double v) const { return (::fabs(t - v) <  std::numeric_limits<double>::epsilon()); }
        bool operator != (double v) const { return (::fabs(t - v) >= std::numeric_limits<double>::epsilon()); }


        //! Print information about this intersection.
        friend std::ostream& operator << (std::ostream& o, const SurfaceIntersection& i)
        {
            o << "[t=" << i.t;
            o << ", st[" << i.st.x << " " << i.st.y << "]";
            o << ", Rxst[" << i.Rxst.x << " " << i.Rxst.y << "]";
            o << ", Ryst[" << i.Ryst.x << " " << i.Ryst.y << "]";
            o << ", object=" << i.object << "]";
            return o;
        }
    };
    typedef std::vector<Traceable::SurfaceIntersection> SurfaceIntersectionList;


    //! Compare two SurfaceIntersection depths.
    struct CompareALessB { bool operator()(const SurfaceIntersection& a, const SurfaceIntersection& b) const { return (a < b); } };
    struct CompareBLessA { bool operator()(const SurfaceIntersection& a, const SurfaceIntersection& b) const { return (b < a); } };

    //!
    static size_t addIntersectionToList(const SurfaceIntersection& I,
                                        SurfaceIntersectionList&   list);
    //!
    static void   sortIntersections(SurfaceIntersectionList& list);


    //--------------------------------------------------------------------------------- 


    /*! An intersection segment in UV space across the face of the primitive.
    */
    struct UVSegmentIntersection
    {
        void* object;           //!< Object pointer for this intersection
        int   object_type;      //!< General use index, often used for face or vertex reference
        //
        Fsr::Vec2f st0;         //!< Barycentric coordinate at seqment start
        Fsr::Vec2f st1;         //!< Barycentric coordinate at seqment end
        Fsr::Vec2f uv0;         //!< UV at segment start
        Fsr::Vec2f uv1;         //!< UV at segment end
    };
    typedef std::vector<Traceable::UVSegmentIntersection> UVSegmentIntersectionList;


    //--------------------------------------------------------------------------------- 


    /*! This is old and needs to be replaced.
    */
    struct DeepIntersection
    {
        Traceable::SurfaceIntersection I;          //!< Intersection info
        Fsr::Pixel                     color;      //!< Sample color
        Dcx::SpMask8                   spmask;     //!< Subpixel mask
        uint32_t                       count;      //!< Normalization count - only normalize if count is > 1

        DeepIntersection(const DD::Image::ChannelSet& chans) : color(chans), spmask(Dcx::SPMASK_ZERO_COVERAGE), count(0) {}
        DeepIntersection(const Fsr::Pixel& c) : color(c), spmask(Dcx::SPMASK_ZERO_COVERAGE), count(0) {}
        DeepIntersection(const Traceable::SurfaceIntersection& i,
                         const Fsr::Pixel&                     c,
                         const Dcx::SpMask8&                   sp) : I(i), color(c), spmask(sp), count(1) {}
    };

    //! List of DeepIntersections
    typedef std::vector<Traceable::DeepIntersection> DeepIntersectionList;

    /*! A surface can overlap itself causing the same surface ID to show up multiple times in the
        same deep intersection list, but we don't want to always combine them if the surface
        intersections are facing away from each other or are not close in Z.
    */
    //! List of same-surface DeepIntersection indices
    typedef std::vector<uint32_t>                                   DeepSurfaceIntersectionList;
    typedef std::map<void*, Traceable::DeepSurfaceIntersectionList> DeepIntersectionMap;


    //--------------------------------------------------------------------------------- 


  public:
    //! Intersect a ray with this object.  This does not return any additional info.
    virtual bool intersect(RayShaderContext& stx) =0;

    //! Intersect a ray with this first surface of this object. Returns the type of intersection code.
    virtual Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&    stx,
                                                          SurfaceIntersection& I) =0;

    //! Intersect a ray with all surfaces of this object. Returns the type of intersection code.
    virtual void getIntersections(RayShaderContext&        stx,
                                  SurfaceIntersectionList& I_list,
                                  double&                  tmin,
                                  double&                  tmax)
    {
        SurfaceIntersection I(std::numeric_limits<double>::infinity());
        if (getFirstIntersection(stx, I) > Fsr::RAY_INTERSECT_NONE)
            I_list.push_back(I);
    }

    //! Intersect against a specific depth level, usually for debugging. Returns the depth intersected.
    virtual int intersectLevel(RayShaderContext& stx,
                               int               level,
                               int               max_level)
    {
        //std::cout << "          Traceable::intersectLevel(" << this << "): parent-level=" << level << ", hit=" << intersect(stx) << std::endl;
        return (intersect(stx))?++level:-1;
    }

    //! Intersect a 2D line with the primitive's UV coords and return the intersection.
    virtual void getIntersectionsWithUVs(RayShaderContext&          stx,
                                         const Fsr::Vec2f&          uv0,
                                         const Fsr::Vec2f&          uv1,
                                         UVSegmentIntersectionList& I_list)
    {
        std::cerr << "Traceable::getIntersectionsWithUVs() not implemented!" << std::endl;
    }

    //! Get the ST coord at a UV coord.  Returns true when ST is inside primitive's parameterization bounds.
    virtual bool getStCoordAtUv(const Fsr::Vec2f& uv,
                                Fsr::Vec2f&       st_out)
    {
        std::cerr << "Traceable::getStCoordAtUv() not implemented!" << std::endl;
        return true;
    }


    //! Print some information about this object.
    virtual void printInfo() const { std::cerr << "printInfo() not implemented" << std::endl; }

    static const char* indent_spaces;   //!< For debugging


  protected:
    //!
    virtual ~Traceable() {}

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

/*static*/
inline size_t
Traceable::addIntersectionToList(const SurfaceIntersection& I,
                                 SurfaceIntersectionList&   list)
{
    list.push_back(I);
    return list.size()-1;
}

/*static*/
inline void
Traceable::sortIntersections(SurfaceIntersectionList& list)
{
    static CompareALessB intersection_near;
    sort(list.begin(), list.end(), intersection_near); // first entry is nearest
}


/*!
*/
inline Fsr::Vec3f
getTriGeometricNormal(const Fsr::Vec3f& p0,
                      const Fsr::Vec3f& p1,
                      const Fsr::Vec3f& p2)
{
    Fsr::Vec3f N = (p1 - p0).cross(p2 - p0);
    N.fastNormalize();
    return N;
}


/*!
*/
inline Fsr::Vec3f
getQuadGeometricNormal(const Fsr::Vec3f& p0,
                       const Fsr::Vec3f& p1,
                       const Fsr::Vec3f& p2,
                       const Fsr::Vec3f& p3)
{
    const Fsr::Vec3f diag0(p3 - p1);
    const Fsr::Vec3f diag1(p0 - p2);
    Fsr::Vec3f N(diag0.cross(diag1));
    N.fastNormalize();
    return N;
}


/*!
*/
inline Fsr::Vec3f
getNormalAtBbox(const Fsr::Box3f& bbox,
                const Fsr::Vec3f& P)
{
    const Fsr::Vec3f center(bbox.getCenter());
    const Fsr::Vec3f extents(center - bbox.min);
    const Fsr::Vec3f localP(P - center);

    Fsr::Vec3f N;
    float min_d = std::numeric_limits<float>::infinity();
    float d;
    d = ::fabsf(extents.x - ::fabsf(localP.x));
    if (d < min_d)
    {
        N.set((localP.x < 0.0f)?-1.0f:1.0f, 0.0f, 0.0f);
        min_d = d;
    }
    d = ::fabsf(extents.y - ::fabsf(localP.y));
    if (d < min_d)
    {
        N.set(0.0f, (localP.y < 0.0f)?-1.0f:1.0f, 0.0f);
        min_d = d;
    }
    d = ::fabsf(extents.z - ::fabsf(localP.z));
    if (d < min_d)
        N.set(0.0f, 0.0f, (localP.z < 0.0f)?-1.0f:1.0f);

    return N;
};


/*! Find the st coordinate of 2D coord vP inside 2D triangle corners.
    Returns false if uv are outside triangle bounds.
*/
inline bool
getStCoordInsideTriangleAt(const Fsr::Vec2d& vP,
                           const Fsr::Vec2d& v0,
                           const Fsr::Vec2d& v1,
                           const Fsr::Vec2d& v2,
                           Fsr::Vec2f&       st_out)
{
    // Edge dot products:
    const Fsr::Vec2d e0(v1 - v0);
    const Fsr::Vec2d e1(v2 - v0);
    const Fsr::Vec2d e2(vP - v0);
    const double dot00 = e0.dot(e0);
    const double dot01 = e0.dot(e1);
    const double dot02 = e0.dot(e2);
    const double dot11 = e1.dot(e1);
    const double dot12 = e1.dot(e2);

    // Compute barycentric coordinates:
    const double iden = 1.0 / (dot00*dot11 - dot01*dot01);
    st_out.x = float((dot11*dot02 - dot01*dot12)*iden);
    st_out.y = float((dot00*dot12 - dot01*dot02)*iden);

    return !(st_out.x < 0.0f ||
             st_out.y < 0.0f ||
             (st_out.x + st_out.y) > 1.0f);
}


} // namespace zpr


#endif

// end of zprender/Traceable.h

//
// Copyright 2020 DreamWorks Animation
//
