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

/// @file zprender/RayShaderContext.h
///
/// @author Jonathan Egstad


#ifndef zprender_RayShaderContext_h
#define zprender_RayShaderContext_h

#include "api.h"
#include <Fuser/RayContext.h>

#include <DDImage/Iop.h>

#include <sys/time.h>


namespace DD {namespace Image {
class TextureFilter;
}}


namespace zpr {


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


class RenderContext;
class RayShader;
class LightShader;
class VolumeShader;
class RenderPrimitive;
class Scene;
class AOVLayer;
class StochasticSampleSet;
class ThreadContext;

typedef std::vector<zpr::Scene*>     LightingSceneList;
typedef std::vector<LightShader*>    LightShaderList;
typedef std::vector<LightShaderList> LightShaderLists;

//-------------------------------------------------------------------------


/*! Shading context structure passed to RayShaders containing all the info a RayShader
    needs, including direct access to the geometry scene.

    Note that the normals are stored double-precision since the RayContext origin and
    dir are double-precision and keeping the shader normals the same data type reduces
    any back and forth conversions.

    Avoid adding any non-POD types in here so copying this class is a simple memcpy.
*/
struct ZPR_EXPORT RayShaderContext
{
    Fsr::RayContext       Rtx;              //!< Current ray
    Fsr::RayDifferentials Rdif;             //!< Current ray XY differentials
    bool                  use_differentials;//!< Whether differentials should be used - false if just point sampling
    //
    Fsr::Vec3d         heroCamOrigin;       //!< Hero-view camera origin - for specular copying
    bool               use_heroV_for_spec;  //!< If enabled camera ray uses heroV for reflection vector
    //
    double             distance;            //!< Distance from last intersection/camera
    //
    RenderPrimitive*   rprim;               //!< Current primitive being evaluated (intersected/shaded)
    const Fsr::Mat4d*  w2l;                 //!< World-to-local matrix for current primitive - NULL if identity
    const Fsr::Mat4d*  l2w;                 //!< Local-to-world matrix for current primitive - NULL if identity
    //
    RayShader*         surface_shader;      //!< Current RayShader being evaluated (NULL if legacy material)
    RayShader*         displacement_shader; //!< Current RayShader being evaluated (NULL if legacy material)
    VolumeShader*      atmosphere_shader;   //!< Current atmospheric VolumeShader being evaluated

    DD::Image::TextureFilter* texture_filter;       //!< Filter to use for texture mapping
    //
    bool               direct_lighting_enabled;     //!< Enable direct scene lighting (shadowed)
    bool               indirect_lighting_enabled;   //!< Enable indirect scene lighting (bounce)
    //
    LightShaderList*   master_light_shaders;        //!< List of all light shaders in scene
    LightShaderLists*  per_object_light_shaders;    //!< Per-object list of light shaders
    //--------------------------------------------------------------------------------
    DD::Image::Iop*    material;                    //!< Current material on primitive - legacy!
    DD::Image::Iop*    displacement_material;       //!< Current displacement material on primitive - legacy!
    zpr::Scene*        master_lighting_scene;       //!< List of lights are in this scene - legacy!
    LightingSceneList* per_object_lighting_scenes;  //!< Per-object-context list of lights - legacy!
    //--------------------------------------------------------------------------------
    //
    int32_t  depth;                         //!< Current depth
    int32_t  diffuse_depth;                 //!< Current diffuse recursion depth
    int32_t  glossy_depth;                  //!< Current glossy recursion depth
    int32_t  reflection_depth;              //!< Current reflection recursion depth
    int32_t  refraction_depth;              //!< Current refraction recursion depth
    double   index_of_refraction;           //!< Current index of refraction
    //
    uint32_t sides_mode;                    //!< Which sides to intersect against (SIDES_BOTH, SIDES_FRONT, SIDES_BACK)
    //
    int32_t  x, y;                          //!< Current output screen coords
    double   sx, sy;                        //!< Current output subpixel screen coords
    int32_t  si;                            //!< Current subsample index
    //
    const StochasticSampleSet* sampler;     //!< Sampler to use
    //
    double   frame_time;                    //!< Absolute frame time (i.e. 101.0, 155.0, etc)
    double   frame_time_offset;             //!< Shutter time offset from global frame time (i.e. -0.5, -0.35, 0.0, +0.5, etc)
    bool     mb_enabled;                    //!< Whether to interpolate time (for convenience, same as frame_time_offset != 0.0)
    uint32_t frame_shutter_step;            //!< Global-context motion-step index for this frame_time
    //
    RenderContext*          rtx;            //!< Global rendering context - this contains the global geometry environment
    const RayShaderContext* previous_stx;   //!< Previous RayShaderContext, normally the last surface intersected/shaded
    int32_t                 thread_index;   //!< Index of current thread, starting at 0
    ThreadContext*          thread_ctx;
    //
    struct timeval     start_time;          //!< The time when this context was instantiated
    //
    DD::Image::Channel cutout_channel;      //!< Channel to use for cutout mask value


    bool show_debug_info;                   //!< For debugging

    //----------------------------------------------
    // Surface params:
    //----------------------------------------------
    Fsr::Vec3d PW;                  //!< Displaced shading point in world-space
    Fsr::Vec3d dPWdx;               //!< PW x-derivative
    Fsr::Vec3d dPWdy;               //!< PW y-derivative
    Fsr::Vec3d PWg;                 //!< Geometric surface point (no displacement)

    Fsr::Vec2f st;                  //!< Primitive's barycentric coordinates at Rtx intersection
    Fsr::Vec2f Rxst;                //!< Primitive's barycentric coordinates at Rtdx intersection
    Fsr::Vec2f Ryst;                //!< Primitive's barycentric coordinates at Rtdy intersection

    Fsr::Vec3d N;                   //!< Shading normal (interpolated & bumped vertex normal)
    Fsr::Vec3d Nf;                  //!< Face-forward shading normal
    Fsr::Vec3d Ng;                  //!< Geometric surface normal
    Fsr::Vec3d Ngf;                 //!< Face-forward geometric normal
    //-------------------------------------------------------------------
    Fsr::Vec3d Ns;                  //!< Interpolated surface normal (same as N but with no bump)
    Fsr::Vec3d dNsdx;               //!< Ns x-derivative
    Fsr::Vec3d dNsdy;               //!< Ns y-derivative

    Fsr::Vec2f UV;                  //!< Surface texture coordinate
    Fsr::Vec2f dUVdx;               //!< UV x-derivative
    Fsr::Vec2f dUVdy;               //!< UV y-derivative

    Fsr::Vec4f Cf;                  //!< Vertex color
    Fsr::Vec4f dCfdx;               //!< Vertex color x-derivative
    Fsr::Vec4f dCfdy;               //!< Vertex color y-derivative


    //! Empty constructor leaves junk in the contents.
    RayShaderContext() {}

    //! Initializes R, leaves junk in the rest.
    RayShaderContext(const Fsr::Vec3d& origin,
                     const Fsr::Vec3d& dir,
                     double            time,
                     double            tmin=std::numeric_limits<double>::epsilon(),
                     double            tmax=std::numeric_limits<double>::infinity());

    //! Copy ctor updates Rtx from current_stx PW, frame_time, etc.
    RayShaderContext(const RayShaderContext&      current_stx,
                     const Fsr::Vec3d&            Rdir,
                     double                       tmin,
                     double                       tmax,
                     uint32_t                     ray_type,
                     int                          sides,
                     const Fsr::RayDifferentials* ray_dif=NULL);


    //! Set the ray, ray type and ray-differential in one step.
    void setRayContext(const Fsr::RayContext&       ray_context,
                       uint32_t                     ray_type,
                       const Fsr::RayDifferentials* ray_dif=NULL);

    //! Copies intersection info into context.
    //void updateFromIntersection(const zpr::Traceable::SurfaceIntersection& I);

    //! Returns 'fake' stereo view-vector or ray view-vector depending on rendering context's stereo mode.
    Fsr::Vec3d getViewVector() const;

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


/*! Initializes Rtx, leaves junk in the rest.
*/
inline
RayShaderContext::RayShaderContext(const Fsr::Vec3d& origin,
                                   const Fsr::Vec3d& dir,
                                   double            time,
                                   double            tmin,
                                   double            tmax) :
    Rtx(origin, dir, time, tmin, tmax),
    frame_time(time)
{
    //
}

//!
inline
RayShaderContext::RayShaderContext(const RayShaderContext&      current_stx,
                                   const Fsr::Vec3d&            Rdir,
                                   double                       tmin,
                                   double                       tmax,
                                   uint32_t                     ray_type,
                                   int                          sides,
                                   const Fsr::RayDifferentials* ray_dif)
{
    if (this != &current_stx)
    {
        memcpy(this, &current_stx, sizeof(RayShaderContext));
        previous_stx = &current_stx;
    }

    Rtx.set(PW, Rdir, frame_time, tmin, tmax);
    Rtx.type_mask = ray_type;
    if (ray_dif)
    {
        Rdif = *ray_dif;
        use_differentials = true;
    }

    sides_mode = sides;
    rprim = NULL;
}


/* Set the ray, ray type and ray-differential in one step.
*/
inline void
RayShaderContext::setRayContext(const Fsr::RayContext&       ray_context,
                                uint32_t                     ray_type,
                                const Fsr::RayDifferentials* ray_dif)
{
    Rtx = ray_context;
    Rtx.time = frame_time;
    Rtx.type_mask = ray_type;
    if (ray_dif)
    {
        Rdif = *ray_dif;
        use_differentials = true;
    }
    else
    {
        use_differentials = false;
    }
}


/*! Returns 'fake' stereo view-vector or ray view-vector depending on
   rendering context's stereo mode.
*/
inline Fsr::Vec3d
RayShaderContext::getViewVector() const
{
    // If it's a camera ray we want to construct a 'fake' view-vector
    // from (hero-cam.origin - shading point) to avoid floating reflections:
    if ((Rtx.type_mask & Fsr::RayContext::CAMERA) && use_heroV_for_spec)
    {
        Fsr::Vec3d V(heroCamOrigin - PW);
        V.normalize();
        return V;
    }

    // No fake stereo, just return the negated ray direction:
    return -Rtx.dir();
}


} // namespace zpr

#endif

// end of zprender/RayShaderContext.h

//
// Copyright 2020 DreamWorks Animation
//
