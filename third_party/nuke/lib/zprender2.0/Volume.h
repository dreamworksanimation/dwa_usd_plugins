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

/// @file zprender/Volume.h
///
/// @author Jonathan Egstad


#ifndef zprender_Volume_h
#define zprender_Volume_h

#include "Traceable.h"
#include "RenderPrimitive.h"

#include <Fuser/Mat4.h>
#include <Fuser/AttributeTypes.h>

#include <DDImage/GeoInfo.h>


namespace zpr {

// zpr::Volume prim enumerations start with this one. Used for VolumeIntersection::object_type:
static const uint32_t  ZprVolume  =  500;


class LightEmitter;


/*!
*/
class ZPR_EXPORT Volume : public Traceable
{
  public:
    int                 surfaces;               //!< Number of 
    DD::Image::GeoInfo* geoinfo;                //!< Parent geoinfo (if it's geometry)
    int                 primitive_index;        //!< Primitive index in geoinfo (if it's geometry)


  public:
    /*!
    */
    class VolumeIntersection
    {
      public:
        double   tmin, tmax;        //!< Thickness of volume
        //
        void*    object;            //!< Object pointer for this intersection
        uint32_t object_type;       //!< Object type used to cast the object pointer
        //
        int32_t  part_index;        //!< Part index in primitive (if it's geometry)
        int32_t  subpart_index;     //!< Part sub-index (if it's geometry)
        float    coverage;          //!< TODO: this used anymore??
        //
        Fsr::Vec3d PWmin;           //!< Surface point at tmin
        Fsr::Vec3d PWmax;           //!< Surface point at tmax


        //! Default ctor leaves junk in vars.
        VolumeIntersection() {}

        //!
        VolumeIntersection(double tmin_,
                           double tmax_,
                           void*  objptr=NULL) :
            tmin(tmin_),
            tmax(tmax_),
            object(objptr),
            object_type(0/*no type*/),
            part_index(-1/*no part*/),
            subpart_index(-1/*no subpart*/),
            coverage(0.0f)
        {}


        /*! Print information about this intersection. */
        friend std::ostream& operator << (std::ostream& o, const VolumeIntersection& i)
        {
            o << " [tmin=" << i.tmin << " tmax=" << i.tmax << ", coverage=" << i.coverage;
            o << ", object=" << i.object << ", subpart=" << i.subpart_index << "]";
            return o;
        }
    };
    typedef std::vector<Volume::VolumeIntersection> VolumeIntersectionList;


  public:
    //!
    Volume(int nSurfaces=2) :
        Traceable(),
        surfaces(nSurfaces),
        geoinfo(NULL),
        primitive_index(-1)
    {
        //
    }


    //!
    static void addVolumeIntersection(double                              t0,
                                      double                              t1,
                                      void*                               object,
                                      Fsr::RayContext&                    Rtx,
                                      Traceable::SurfaceIntersectionList& I_list,
                                      double&                             tmin,
                                      double&                             tmax);



};

typedef std::vector<Volume*> VolumePtrList;



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


//!
/*static*/
inline void
Volume::addVolumeIntersection(double                   t0,
                              double                   t1,
                              void*                    object,
                              Fsr::RayContext&         Rtx,
                              SurfaceIntersectionList& I_list,
                              double&                  tmin,
                              double&                  tmax)
{
    SurfaceIntersection t_enter, t_exit;
    if (t0 < t1)
    {
        tmin = std::min(tmin, t0);
        t_enter.t          = t0;
        t_enter.object     = static_cast<RenderPrimitive*>(object);
        t_enter.PW         = Rtx.getPositionAt(t0);
        t_enter.N          = Fsr::Vec3f(0,0,1); // TODO get normal at intersection?
        t_enter.object_ref = 2; // two hits
        //
        tmax = std::max(tmax, t1);
        t_exit.t           = t1;
        t_exit.object      = static_cast<RenderPrimitive*>(object);
        t_exit.PW          = Rtx.getPositionAt(t1);
        t_exit.N           = Fsr::Vec3f(0,0,1); // TODO get normal at intersection?
        t_exit.object_ref  = -1; // relative offset to first hit
    }
    else
    {
        tmin = std::min(tmin, t1);
        t_enter.t          = t1;
        t_enter.object     = static_cast<RenderPrimitive*>(object);
        t_enter.PW         = Rtx.getPositionAt(t1);
        t_enter.N          = Fsr::Vec3f(0,0,1); // TODO get normal at intersection?
        t_enter.object_ref = 2; // two hits
        //
        tmax = std::max(tmax, t0);
        t_exit.t           = t0;
        t_exit.object      = static_cast<RenderPrimitive*>(object);
        t_exit.PW          = Rtx.getPositionAt(t0);
        t_exit.N           = Fsr::Vec3f(0,0,1); // TODO get normal at intersection?
        t_exit.object_ref  = -1; // relative offset to first hit
    }
    addIntersectionToList(t_enter, I_list);
    addIntersectionToList(t_exit,  I_list);
    //std::cout << " tmin=" << t_enter.t << ", tmax=" << t_exit.t << std::endl;
}


} // namespace zpr


#endif

// end of zprender/Volume.h

//
// Copyright 2020 DreamWorks Animation
//
