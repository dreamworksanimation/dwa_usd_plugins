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

/// @file zprender/zprHomogeneousVolume.h
///
/// @author Jonathan Egstad


#ifndef zprender_zprHomogeneousVolume_h
#define zprender_zprHomogeneousVolume_h

#include "VolumeShader.h"


#include <DDImage/Material.h>
#include <DDImage/LookupCurves.h>


namespace zpr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// Max number of volume noise controls in one shader:
#define NUM_NOISE_FUNC 3

/*! Volume noise types. */
enum
{
    VOLUME_NOISE_FBM,
    VOLUME_NOISE_TURBULENCE
};


/*! 3D Noise parameters for volume marching.
*/
struct ZPR_EXPORT VolumeNoise
{
    bool       k_enabled;           //!< Is the noise module enabled?
    int        k_type;              //!< Noise type - FBM or Turbulence
    int        k_octaves;           //!< 
    double     k_lacunarity;        //!< 
    double     k_gain;              //!< Multiplier
    double     k_mix;               //!< 

    Fsr::Vec3f k_translate;         //!< Translate the noise field
    Fsr::Vec3f k_rotate;            //!< Rotate the noise field
    Fsr::Vec3f k_scale;             //!< Scale the noise field
    double     k_uniform_scale;     //!< Uniform scale the noise field

    char*      knob_names[11];     //!< Name strings for knobs

    Fsr::Mat4d m_xform;             //!< Derived from xform controls and global xform
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Base class of ray-tracing volume shader Ops.

    TODO: this should no longer be subclassed off DD::Image::Material since it's not
          going to be a separate Iop Node in the graph but controlled from the zpRender
          panel itself or from a GeoOp in the geometry input. I suppose we could have
          ApplyMaterial use this as an input but it can't attach this Iop as a material
          to any GeoInfo, so what's the point...

    TODO: move most of these vars into a subclass rather than having them in the base class.


*/
class ZPR_EXPORT zprHomogeneousVolume : public VolumeShader
{
  public:
    enum
    {
        NOISE_FBM,
        NOISE_TURBULENCE
    };
    static const char* const noise_types[];


  public:
    // Knob-driven values:
    struct InputParams
    {
        double  k_ray_step;                         //!< Initial ray-step size
        double  k_ray_step_min;                     //!< Minimum step size (to keep march from exploding)
        int     k_ray_step_count_min;               //!< Do at least this many steps per-volume
        int     k_ray_step_count_max;               //!< Do at most this many steps
        int     k_preview_max_ray_steps;
        double  k_atmospheric_density;              //!< Overall atmospheric density factor (what units is this really...?)
        double  k_density_base;                     //!< ?
        double  k_volume_illum_factor;              //!< Illumination volume global multiplier
        double  k_absorption_factor;                //!< 
        bool    k_light_absorption;
        //
        bool                    k_noise_enabled;        //!< Master noise enable
        int                     k_num_noise_functions;  //!<
        Fsr::Mat4f              k_noise_xform;          //!< Master noise xform
        //
        bool                    k_falloff_enabled;      //!< Enable falloff
        Fsr::Box3f              k_falloff_bbox;         //!< Global-space falloff bbox
        DD::Image::LookupCurves k_falloff_lut;          //!< Falloff curves within falloff bbox


        //! Need a ctor for DD::Image::LookupCurves() initializer...
        InputParams();
    };


  public:
    InputParams inputs;

    // Derived values:
    VolumeNoise* m_noise_modules[NUM_NOISE_FUNC];
    Fsr::Box3d   m_falloff_bbox;
    float        m_density;
    float        m_density_base;


  public:
    //!
    zprHomogeneousVolume();
    zprHomogeneousVolume(const InputParams& input_params);

    //! Must have a virtual destructor to subclass!
    virtual ~zprHomogeneousVolume();


    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);


    /*! Default homogenous ray march through a set of light volumes.
        If it returns false there's been a user-abort.
    */
    /*virtual*/ bool volumeMarch(zpr::RayShaderContext&                stx,
                                 double                                tmin,
                                 double                                tmax,
                                 double                                depth_min,
                                 double                                depth_max,
                                 float                                 surface_Z,
                                 float                                 surface_alpha,
                                 const Volume::VolumeIntersectionList& vol_intersections,
                                 Fsr::Pixel&                           color_out,
                                 Traceable::DeepIntersectionList*      deep_out=NULL) const;
};


} // namespace zpr

#endif

// end of zprender/zprHomogeneousVolume.h

//
// Copyright 2020 DreamWorks Animation
//
