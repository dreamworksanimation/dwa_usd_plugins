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

/// @file Fuser/Lookat.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Lookat_h
#define Fuser_Lookat_h

#include "Vec3.h"
#include "Mat4.h"

#include <DDImage/AxisOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/Quaternion.h>

#include <limits> // for numeric_limits<T>
#include <cmath>
#ifndef M_PI
#  define M_PI      3.14159265358979323846
#endif
#ifndef M_PI_2
#  define M_PI_2    1.57079632679489661923
#endif


namespace Fsr {

//-------------------------------------------------------------------------

/*! Interface class providing lookat functionality.

*/
class FSR_EXPORT Lookat
{
  public:
    enum { USE_VECTORS, USE_QUATS };        //!< Rotation calculation methods.
    static const char* const method_list[]; //!< List of method names


  public:
    bool   k_lookat_enable;     //!< Global enable
    int    k_lookat_axis;       //!< Axis to align
    bool   k_lookat_do_rx;      //!< Enable X lookat rotation
    bool   k_lookat_do_ry;      //!< Enable Y lookat rotation
    bool   k_lookat_do_rz;      //!< Enable Z lookat rotation
    int    k_lookat_method;     //!< Which method to use - vectors or quaternions
    double k_lookat_mix;        //!< Lookat mix


  public:
    Lookat();


    /*! */
    void addLookatKnobs(DD::Image::Knob_Callback f,
                        const char*              label="lookat");

    /*!
    */
    void appendLookatHash(DD::Image::Hash& hash) const;


    /*! Assumes a normalized quaternion and an output rotation of ZXY.
        Output rotations are in degrees!
    */
    template <typename T>
    static void quatToRotations(const DD::Image::Quaternion& quat,
                                Fsr::Vec3<T>&                rotations);


    /*! Calculate rotations to align point P to point lookatP.
        Return true if rotations have been affected.
        Output rotations are in degrees!
    */
    bool lookatPoint(const Fsr::Vec3d& P,
                     const Fsr::Vec3d& lookatP,
                     bool              use_quats,
                     Fsr::Vec3d&       rotations);


    /*! Calculate rotations to align with direction vector.
        Return true if rotations have been affected.
        Output rotations are in degrees!
    */
    static bool vectorToRotations(int                method,
                                  const Fsr::Vec3d&  dir_vec,
                                  Fsr::AxisDirection align_axis,
                                  bool               do_rx,
                                  bool               do_ry,
                                  bool               do_rz,
                                  double             lookat_strength,
                                  Fsr::Vec3d&        rotations);

};



//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
// INLINE METHODS:
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

template <typename T>
/*static*/ inline void
Lookat::quatToRotations(const DD::Image::Quaternion& quat,
                        Fsr::Vec3<T>&                rotations)
{
    const double tilt = quat.vx*quat.vy + quat.vz*quat.s;
    if (tilt > 0.4999)
    {
        // Singularity at north pole:
        rotations.y = (T)2 * ::atan2(quat.vx, quat.s);
        rotations.x = (T)0;
        rotations.z = M_PI_2;
    }
    else if (tilt < -0.4999)
    {
        // Singularity at south pole:
        rotations.y = (T)-2 * ::atan2(quat.vx, quat.s);
        rotations.x = (T)0;
        rotations.z = -M_PI_2;
    }
    else
    {
        const double zz = quat.vz*quat.vz;
        rotations.y = ::atan2((T)2*quat.vy*quat.s - (T)2*quat.vx*quat.vz,
                              (T)1 - (T)2*quat.vy*quat.vy - (T)2*zz);
        rotations.x = ::atan2((T)2*quat.vx*quat.s - (T)2*quat.vy*quat.vz,
                              (T)1 - (T)2*quat.vx*quat.vx - (T)2*zz);
        rotations.z =  ::asin((T)2*tilt);
    }
    rotations.toDegrees();
}

/*static*/ inline bool
Lookat::vectorToRotations(int                method,
                          const Fsr::Vec3d&  dir_vec,
                          Fsr::AxisDirection align_axis,
                          bool               do_rx,
                          bool               do_ry,
                          bool               do_rz,
                          double             lookat_strength,
                          Fsr::Vec3d&        rotations)
{
    Fsr::Vec3d dir(dir_vec);
    const double len = dir.normalize();
    if (lookat_strength <= 0.0 || len < std::numeric_limits<double>::epsilon())
        return false; // can't perform lookat

    Fsr::Vec3d look_rotations(0.0);

    // Which technique do we use?
    if (method == USE_QUATS)
    {
        // Use quaternions:
        DD::Image::Vector3 startv;
        switch (align_axis)
        {
        case Fsr::AXIS_X_PLUS : startv.set(-1, 0, 0); break;
        case Fsr::AXIS_X_MINUS: startv.set( 1, 0, 0); break;
        case Fsr::AXIS_Y_PLUS : startv.set( 0,-1, 0); break;
        case Fsr::AXIS_Y_MINUS: startv.set( 0, 1, 0); break;
        case Fsr::AXIS_Z_PLUS : startv.set( 0, 0,-1); break;
        case Fsr::AXIS_Z_MINUS: startv.set( 0, 0, 1); break;
        }
        DD::Image::Quaternion rotation_quat(startv, dir.asDDImage());
#if 1
        quatToRotations(rotation_quat, look_rotations);
#else
        if (lookat_strength < 1.0)
        {
           // Interpolate between quats:
           // TODO: this doesn't work, need to convert input rotations into the start_quat...?:
           DD::Image::Quaternion start_quat(dir.asDDImage(), dir.asDDImage());
           DD::Image::Quaternion dest_quat = start_quat.slerp(rotation_quat, lookat_strength);
           quatToRotations(dest_quat, look_rotations);
        }
        else
           quatToRotations(rotation_quat, look_rotations);
#endif
    }
    else
    {
        // Use vector math.
        // We calculate the primary rotation first then the second, and which rotation axis we
        // change is determined by the align axis:
        double d;
        switch (align_axis)
        {
        case Fsr::AXIS_X_MINUS:
            if (do_ry) { look_rotations.y = ::atan2(-dir.z, dir.x); d = std::sqrt(dir.z*dir.z + dir.x*dir.x); } else { d =  dir.x; }
            if (do_rz) { look_rotations.z = ::atan2( dir.y, d); }
            break;
        case Fsr::AXIS_X_PLUS:
            if (do_ry) { look_rotations.y = ::atan2( dir.z,-dir.x); d = std::sqrt(dir.z*dir.z + dir.x*dir.x); } else { d = -dir.x; }
            if (do_rz) { look_rotations.z =-::atan2( dir.y, d); }
            break;
        //
        case Fsr::AXIS_Y_MINUS:
            if (do_rx) { look_rotations.x = ::atan2( dir.z, dir.y); d = std::sqrt(dir.z*dir.z + dir.y*dir.y); } else { d =  dir.y; }
            if (do_rz) { look_rotations.z =-::atan2( dir.x, d); }
            break;
        case Fsr::AXIS_Y_PLUS:
            if (do_rx) { look_rotations.x = ::atan2(-dir.z,-dir.y); d = std::sqrt(dir.z*dir.z + dir.y*dir.y); } else { d = -dir.y; }
            if (do_rz) { look_rotations.z = ::atan2( dir.x, d); }
            break;
        //
        case Fsr::AXIS_Z_MINUS:
            if (do_ry) { look_rotations.y = ::atan2( dir.x, dir.z); d = std::sqrt(dir.x*dir.x + dir.z*dir.z); } else { d =  dir.z; }
            if (do_rx) { look_rotations.x =-::atan2( dir.y, d); }
            break;
        case Fsr::AXIS_Z_PLUS:
            if (do_ry) { look_rotations.y = ::atan2(-dir.x,-dir.z); d = std::sqrt(dir.x*dir.x + dir.z*dir.z); } else { d = -dir.z; }
            if (do_rx) { look_rotations.x = ::atan2( dir.y, d); }
            break;
        }
        look_rotations.toDegrees();
    }

    if (lookat_strength < 1.0f)
    {
        // Interpolate between parent rotation and look rotation:
        if (do_rx) rotations.x = ::lerp(rotations.x, look_rotations.x, lookat_strength);
        if (do_ry) rotations.y = ::lerp(rotations.y, look_rotations.y, lookat_strength);
        if (do_rz) rotations.z = ::lerp(rotations.z, look_rotations.z, lookat_strength);

    }
    else
    {
        // Max rotations:
        if (do_rx) rotations.x = look_rotations.x;
        if (do_ry) rotations.y = look_rotations.y;
        if (do_rz) rotations.z = look_rotations.z;
    }

    return true;
}


/*! Calculate rotations to align point P to point lookatP.
    Return true if rotations have been affected.
*/
inline bool
Lookat::lookatPoint(const Fsr::Vec3d& P,
                    const Fsr::Vec3d& lookatP,
                    bool              use_quats,
                    Fsr::Vec3d&       rotations)
{
   if (!k_lookat_enable || k_lookat_mix <= 0.0)
        return false;
   return vectorToRotations(k_lookat_method,
                            (P - lookatP),
                            (Fsr::AxisDirection)k_lookat_axis,
                            k_lookat_do_rx,
                            k_lookat_do_ry,
                            k_lookat_do_rz,
                            k_lookat_mix,
                            rotations);
}


} // namespace Fsr


#endif

// end of Fuser/Lookat.h

//
// Copyright 2019 DreamWorks Animation
//
