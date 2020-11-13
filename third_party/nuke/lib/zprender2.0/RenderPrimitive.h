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

/// @file zprender/RenderPrimitive.h
///
/// @author Jonathan Egstad


#ifndef zprender_RenderPrimitive_h
#define zprender_RenderPrimitive_h

#include "Traceable.h"
#include "Bvh.h"

#include <Fuser/Box3.h>
#include <Fuser/AttributeTypes.h> // for Uint32List, etc
#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <vector>

namespace zpr {


//-------------------------------------------------------------------------
// Utility functions for getting the index and interpolation
// offset inside a motion step.
//

enum { MOTIONSTEP_START, MOTIONSTEP_MID, MOTIONSTEP_END };

//! Get the motion step that frame_time falls inside from the motion_times list.
ZPR_EXPORT uint32_t getMotionStep(const Fsr::DoubleList& motion_times,
                                  double                 frame_time);
//! Get the motion step that frame_time falls inside and interpolation offset within the step.
ZPR_EXPORT int32_t  getMotionStep(const Fsr::DoubleList& motion_times,
                                  double                 frame_time,
                                  uint32_t&              motion_step,
                                  float&                 motion_step_t);


/*! Get a matrix, possibly interpolated, at frame_time.
    If frame_time is between keys then matrix is linear interpolated using
    Fsr::Mat4 lerp().
*/
ZPR_EXPORT Fsr::Mat4d getMotionXformAt(const Fsr::DoubleList& motion_times,
                                       double                 frame_time,
                                       const Fsr::Mat4dList&  motion_xforms);
/*! Get two xforms at once, saving an additional motion step calculation.
    This is usually used when getting a matrix and its inverse at the same
    time. Both are linearly interpolated vs. deriving the first then
    inverting it.
*/
ZPR_EXPORT void       getMotionXformsAt(const Fsr::DoubleList& motion_times,
                                        double                 frame_time,
                                        const Fsr::Mat4dList&  motion_xformsA,
                                        const Fsr::Mat4dList&  motion_xformsB,
                                        Fsr::Mat4d&            xformA,
                                        Fsr::Mat4d&            xformB);



//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*! Clip mask codes.
    TODO: deprecate!
*/
enum
{
    CLIP_RIGHT  = 0x0001,   // Right-edge of screen
    CLIP_LEFT   = 0x0002,   // Left-edge of screen
    CLIP_TOP    = 0x0004,   // Top-edge of screen
    CLIP_BOTTOM = 0x0008,   // Bottom-edge of screen
    CLIP_NEAR   = 0x0010,   // Near camera plane
    CLIP_USER0  = 0x0020,   // usually defaulted to far camera plane
    CLIP_USER1  = 0x0040,
    CLIP_USER2  = 0x0080,
    //
    CLIP_PLANES = 8
};

static Fsr::Vec4f default_grey(0.18f, 0.18f, 0.18f, 1.0f);


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// zpr::RenderPrim enumeration used for SurfaceIntersection::object_type:
static const uint32_t  ZprRenderPrim  =  100;


class Surface;
class Volume;
class LightEmitter;
class MaterialContext;
class SurfaceContext;
class GeoInfoContext;
class LightVolumeContext;



/*!
*/
class ZPR_EXPORT RenderPrimitive
{
  protected:
    MaterialContext*    m_material_ctx;         //!< Material parameters
    Fsr::DoubleList     m_motion_times;         //!< Frame time for each motion-sample


  public:
    //!
    RenderPrimitive(const MaterialContext* material_ctx,
                    double                 motion_time);
    //!
    RenderPrimitive(const MaterialContext* material_ctx,
                    const Fsr::DoubleList& motion_times);

    //!
    virtual ~RenderPrimitive() {}

    //! Return the class string of render primitive.
    virtual const char* getClass() const =0;


    //---------------------------------------------


    //! If this is a traceable primitive return this cast to Traceable.
    virtual Traceable*    isTraceable() { return NULL; }

    //! If this is a surface primitive return this cast to Surface.
    virtual Surface*      isSurface() { return NULL; }

    //! If this is a volume primitive return this cast to Volume.
    virtual Volume*       isVolume() { return NULL; }

    //! Return true if primitive is 'watertight', ie is an enclosing boundary. Usually cards(single-sided) & volumes are not manifold.
    virtual bool          isManifold() { return false; }

    //! Returns a pointer to the LightEmitter object if this primitive can emit light.
    virtual LightEmitter* isLightEmitter() { return NULL; }


    //---------------------------------------------


    //! Get the MaterialContext info structure - should never be NULL!
    MaterialContext*          getMaterialContext() { return m_material_ctx; }

    //! Get the parent SurfaceContext structure - should never be NULL!
    const SurfaceContext*     getSurfaceContext() const;

    //! Get the parent DD::Image::GeoInfo context structure, if generated from one.
    const GeoInfoContext*     getGeoInfoContext() const;

    //! Get the parent DD::Image::LightOp context structure, if generated from one.
    const LightVolumeContext* getLightVolumeContext() const;


    //---------------------------------------------


    //! Number of motion times, must be >= 1!
    uint32_t numMotionTimes() const { return (uint32_t)m_motion_times.size(); }

    //! Get a motion sample time. No range checking!
    double   getMotionTime(uint32_t i) const { return m_motion_times[i]; }

    //! A motion step covers two motion samples. ie step 0=samples 0..1, step 1=samples 1..2, etc.
    uint32_t numMotionSteps() const { return (numMotionTimes()-1); }

    //! Get the AABB for this primitive at an optional shutter time.
    virtual Fsr::Box3d getBBoxAtTime(double frame_time) =0;


    //---------------------------------------------


    //! How many subd level to displace to.
    virtual int        getDisplacementSubdivisionLevel() const;

    //! Return a maximum displacement vector for this prim.
    virtual Fsr::Vec3f getDisplacementBounds() const;

    //!
    virtual void doAutoBump(RayShaderContext&               stx,
                            Traceable::SurfaceIntersection& I) const {}


    //=======================================================
    // Attributes

    //! Interpolate varying vertex attributes at SurfaceIntersection.
    virtual void getAttributesAtSurfaceIntersection(const Traceable::SurfaceIntersection& I,
                                                    const DD::Image::ChannelSet&          mask,
                                                    Fsr::Pixel&                           v) const {}

    //! Interpolate varying vertex attributes at SurfaceIntersection. This also calculates derivatives.
    virtual void getAttributesAtSurfaceIntersection(const Traceable::SurfaceIntersection& I,
                                                    const DD::Image::ChannelSet&          mask,
                                                    Fsr::Pixel&                           v,
                                                    Fsr::Pixel&                           vdu,
                                                    Fsr::Pixel&                           vdv) const {}


    //=======================================================
    // Misc

    //! Print some information about this object.
    virtual void printInfo() const { std::cerr << "RenderPrimitive::printInfo() not implemented" << std::endl; }


  private:
    //! Disabled copy constructor.
    RenderPrimitive(const RenderPrimitive&);
    //! Disabled copy operator.
    RenderPrimitive& operator=(const RenderPrimitive&);

};


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


/*! Get the motion step that frame_time falls inside from the motion_times list.
    Step is clamped to start or end motion time.
*/
inline uint32_t
getMotionStep(const Fsr::DoubleList& motion_times,
              double                 frame_time)
{
    const size_t nMotionSamples = motion_times.size();
#if DEBUG
    assert(nMotionSamples > 0);
#endif
    if (nMotionSamples == 1)
        return 0; // no motionblur

    // Find the motion step frame_time falls inside.
    // sample: 0             1             2             3
    //   step: |......0......|......1......|......2......|
    if (frame_time < motion_times[0])
    {
        return 0; // before the first motion step
    }
    else
    {
        // Search the rest:
        for (size_t sample=1; sample < nMotionSamples; ++sample)
            if (frame_time < motion_times[sample])
                return uint32_t(sample-1); // done!
        return uint32_t(nMotionSamples-1); // past last motion step
    }
}


/*! Get the motion step that frame_time falls inside and interpolation offset within the step.
    Step is clamped to start or end motion time.
    Returns a hint to whether interpolation is required and whether to use the start
    or end of the motion_step (t=0 or 1).
    MOTIONSTEP_START = use start of motion_step, no interpolation
    MOTIONSTEP_MID   = interpolate with motion_step_t
    MOTIONSTEP_END   = use end of motion_step, no interpolation
*/
inline int32_t
getMotionStep(const Fsr::DoubleList& motion_times,
              double                 frame_time,
              uint32_t&              motion_step,
              float&                 motion_step_t)
{
    const size_t nMotionSamples = motion_times.size();
#if DEBUG
    assert(nMotionSamples > 0);
#endif
    if (nMotionSamples == 1)
    {
        // No motionblur:
        motion_step   = 0;
        motion_step_t = 0.0f;
        return MOTIONSTEP_START;
    }

    // Find the motion step frame_time falls inside.
    // sample: 0             1             2             3
    //   step: |......0......|......1......|......2......|
    if (frame_time <= motion_times[0])
    {
        // Before the first motion step:
        motion_step   = 0;
        motion_step_t = 0.0f;
        return MOTIONSTEP_START;
    }

    // Search the rest:
    for (size_t sample=1; sample < nMotionSamples; ++sample)
    {
        if (frame_time < motion_times[sample])
        {
            motion_step   = uint32_t(sample-1);
            motion_step_t = float((frame_time - motion_times[motion_step]) /
                                    (motion_times[motion_step+1] - motion_times[motion_step]));
            return MOTIONSTEP_MID;
        }
    }

    // Past last motion step:
    motion_step   = uint32_t(nMotionSamples-2);
    motion_step_t = 1.0f;
    return MOTIONSTEP_END;
}


inline Fsr::Mat4d
getMotionXformAt(const Fsr::DoubleList& motion_times,
                 double                 frame_time,
                 const Fsr::Mat4dList&  motion_xforms)
{
    // Don't crash, just return identity():
    if (motion_xforms.size() == 0)
        return Fsr::Mat4d::getIdentity();

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(motion_times, frame_time, motion_step, motion_step_t);
#if DEBUG
    assert(motion_step < motion_xforms.size());
#endif

    if (motion_mode == MOTIONSTEP_START)
        return motion_xforms[motion_step ];
    else if (motion_mode == MOTIONSTEP_END)
        return motion_xforms[motion_step+1];

    return Fsr::lerp(motion_xforms[motion_step], motion_xforms[motion_step+1], motion_step_t);
}

inline void
getMotionXformsAt(const Fsr::DoubleList& motion_times,
                  double                 frame_time,
                  const Fsr::Mat4dList&  motion_xformsA,
                  const Fsr::Mat4dList&  motion_xformsB,
                  Fsr::Mat4d&            xformA,
                  Fsr::Mat4d&            xformB)
{
    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(motion_times, frame_time, motion_step, motion_step_t);

    if (motion_step >= motion_xformsA.size())
    {
        // Don't crash, just return identity():
        xformA = Fsr::Mat4d::getIdentity();
    }
    else
    {
        if (motion_mode == MOTIONSTEP_START)
            xformA = motion_xformsA[motion_step ];
        else if (motion_mode == MOTIONSTEP_END)
            xformA = motion_xformsA[motion_step+1];

        xformA = Fsr::lerp(motion_xformsA[motion_step], motion_xformsA[motion_step+1], motion_step_t);
    }

    if (motion_step >= motion_xformsB.size())
    {
        // Don't crash, just return identity():
        xformB = Fsr::Mat4d::getIdentity();
    }
    else
    {
        if (motion_mode == MOTIONSTEP_START)
            xformB = motion_xformsB[motion_step ];
        else if (motion_mode == MOTIONSTEP_END)
            xformB = motion_xformsB[motion_step+1];

        xformB = Fsr::lerp(motion_xformsB[motion_step], motion_xformsB[motion_step+1], motion_step_t);
    }
}


} // namespace zpr

#endif

// end of zprender/RenderPrimitive.h

//
// Copyright 2020 DreamWorks Animation
//
