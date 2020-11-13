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
#if kDDImageVersionMajorNum < 12
namespace DD { namespace Image {
typedef Quaternion Quaternion4d;
}}
#endif

#include <limits> // for numeric_limits<T>
#include <cmath>


namespace Fsr {

//-------------------------------------------------------------------------

/*! Interface class providing lookat aim-constraint functionality.

*/
class FSR_EXPORT LookatVals
{
  public:
    enum { USE_VECTORS, USE_QUATS };                //!< Rotation calculation methods.
    static const char* const method_list[];         //!< List of method names

    enum { AIM_USE_LOCAL_XFORM, AIM_FROM_PIVOT };   //!< Aim location modes
    static const char* const aim_location_modes[];  //!< List of mode for handling aim rotation location


  public:
    bool       k_lookat_enable;         //!< Global enable
    int        k_lookat_axis;           //!< Axis to align
    bool       k_lookat_do_rx;          //!< Enable X lookat rotation
    bool       k_lookat_do_ry;          //!< Enable Y lookat rotation
    bool       k_lookat_do_rz;          //!< Enable Z lookat rotation
    bool       k_lookat_use_point;      //!< Use the user-specified point rather than the input connection
    Fsr::Vec3f k_lookat_point;          //!< User-assigned world-space lookat point
    bool       k_lookat_method;         //!< Which method to use - vectors(false) or quaternions(true)
    int        k_lookat_aim_location;   //!< Which aim location mode to use
    double     k_lookat_mix;            //!< Lookat mix


  public:
    //!
    LookatVals();
    //! Extracts values from an Op at a specific OutputContext.
    LookatVals(const DD::Image::Op*            op,
               const DD::Image::OutputContext& context) { getValsAt(op, context); }

    //! Assigns standard default values to all params.
    void setToDefault(uint32_t aim_location_mode=AIM_USE_LOCAL_XFORM);

    //!
    void addLookatKnobs(DD::Image::Knob_Callback f,
                        const char*              label="aim_constraint");

    //!
    void appendLookatHash(DD::Image::Hash& hash) const;

    //!
    int  knobChanged(const DD::Image::Op* op,
                     DD::Image::Knob*     k);

    //!
    void enableLookatKnobs(const DD::Image::Op* op,
                           bool                 lookat_enabled);

    //!
    bool getValsAt(const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context);

    //!
    static bool store(DD::Image::Op* op,
                      LookatVals     vals);



    //!
    Fsr::Mat4d getLookatXform(const Fsr::Mat4d& parent_matrix,
                              const Fsr::Mat4d& local_matrix);


    /*! Assumes a normalized quaternion and an output rotation of ZXY.
        Output rotations are in degrees!
    */
    template <typename T>
    static void quatToRotations(const DD::Image::Quaternion4d& quat,
                                Fsr::Vec3<T>&                  rotations_in_degrees);


    /*! Calculate rotations to align point P to point lookatP.
        Return true if rotations have been affected.
        Output rotations are in degrees!
    */
    bool lookatPoint(const Fsr::Vec3d& P,
                     const Fsr::Vec3d& lookatP,
                     Fsr::Vec3d&       rotations,
                     RotationOrder&    rotation_order) const;


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
                                  Fsr::Vec3d&        rotations,
                                  RotationOrder&     rotation_order);

};



//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
// INLINE METHODS:
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

template <typename T>
/*static*/ inline void
LookatVals::quatToRotations(const DD::Image::Quaternion4d& quat,
                            Fsr::Vec3<T>&                  rotations_in_degrees)
{
#define FSR_M_PI_2 1.57079632679489661923
    const double tilt = quat.vx*quat.vy + quat.vz*quat.s;
    if (tilt > 0.4999)
    {
        // Singularity at north pole:
        rotations_in_degrees.y = (T)2 * ::atan2(quat.vx, quat.s);
        rotations_in_degrees.x = (T)0;
        rotations_in_degrees.z = M_PI_2;
    }
    else if (tilt < -0.4999)
    {
        // Singularity at south pole:
        rotations_in_degrees.y = (T)-2 * ::atan2(quat.vx, quat.s);
        rotations_in_degrees.x = (T)0;
        rotations_in_degrees.z = -M_PI_2;
    }
    else
    {
        const double zz = quat.vz*quat.vz;
        rotations_in_degrees.y = ::atan2((T)2*quat.vy*quat.s - (T)2*quat.vx*quat.vz,
                                            (T)1 - (T)2*quat.vy*quat.vy - (T)2*zz);
        rotations_in_degrees.x = ::atan2((T)2*quat.vx*quat.s - (T)2*quat.vy*quat.vz,
                                            (T)1 - (T)2*quat.vx*quat.vx - (T)2*zz);
        rotations_in_degrees.z =  ::asin((T)2*tilt);
    }
    rotations_in_degrees.toDegrees();
#undef FSR_M_PI_2
}


/*static*/ inline bool
LookatVals::vectorToRotations(int                method,
                              const Fsr::Vec3d&  dir_vec,
                              Fsr::AxisDirection align_axis,
                              bool               do_rx,
                              bool               do_ry,
                              bool               do_rz,
                              double             lookat_strength,
                              Fsr::Vec3d&        rotations,
                              RotationOrder&     rotation_order)
{
    if (lookat_strength <= 0.0)
        return false; // don't bother

    // Which technique do we use?
    if (method == USE_QUATS)
    {
        Fsr::Vec3d dir(dir_vec);
        const double len = dir.normalize();
        if (len < std::numeric_limits<double>::epsilon())
            return false; // can't perform lookat

        Fsr::Vec3d look_rotations(0.0);

        // TODO: get quaternion lookat working! I think we need to refactor all
        // this to support the aim pivot mode:

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

        DD::Image::Quaternion4d rotation_quat(startv, dir.asDDImage());
        if (1)//(lookat_strength >= 1.0)
        {
           quatToRotations(rotation_quat, look_rotations); // no blending
        }
        else
        {
           // Blend between quats:
           // TODO: this doesn't work, need to convert input rotations into the start_quat...?:
           const DD::Image::Quaternion4d start_quat(dir.asDDImage(), dir.asDDImage());
           const DD::Image::Quaternion4d end_quat = start_quat.slerp(rotation_quat, lookat_strength);
           quatToRotations(end_quat, look_rotations);
        }

        if (do_rx) rotations.x = look_rotations.x;
        if (do_ry) rotations.y = look_rotations.y;
        if (do_rz) rotations.z = look_rotations.z;
    }
    else
    {
        // Use vector math:
        Fsr::Vec3d look_rotations(0.0);
        if (Fsr::Mat4d::vectorToRotations(dir_vec, align_axis, do_rx, do_ry, do_rz,
                                            look_rotations, rotation_order))
        {
            look_rotations.toDegrees();
            if (lookat_strength < 1.0f)
            {
                // Interpolate between parent rotation and look rotation:
                if (do_rx) rotations.x = ::lerp(rotations.x, look_rotations.x, lookat_strength);
                if (do_ry) rotations.y = ::lerp(rotations.y, look_rotations.y, lookat_strength);
                if (do_rz) rotations.z = ::lerp(rotations.z, look_rotations.z, lookat_strength);
            }
            else
            {
                // Use max rotations:
                if (do_rx) rotations.x = look_rotations.x;
                if (do_ry) rotations.y = look_rotations.y;
                if (do_rz) rotations.z = look_rotations.z;
            }
        }
    }

    return true;
}


/*! Calculate rotations to align point P to point lookatP.
    Return true if rotations have been affected.
*/
inline bool
LookatVals::lookatPoint(const Fsr::Vec3d& P,
                        const Fsr::Vec3d& lookatP,
                        Fsr::Vec3d&       rotations,
                        RotationOrder&    rotation_order) const
{
   if (!k_lookat_enable || k_lookat_mix <= 0.0)
        return false;
   return vectorToRotations(k_lookat_method,
                            (lookatP - P),
                            (Fsr::AxisDirection)k_lookat_axis,
                            k_lookat_do_rx,
                            k_lookat_do_ry,
                            k_lookat_do_rz,
                            k_lookat_mix,
                            rotations,
                            rotation_order);
}


} // namespace Fsr


#endif

// end of Fuser/Lookat.h

//
// Copyright 2019 DreamWorks Animation
//
