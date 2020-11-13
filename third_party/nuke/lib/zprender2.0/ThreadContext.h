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

/// @file zprender/ThreadContext.h
///
/// @author Jonathan Egstad


#ifndef zprender_ThreadContext_h
#define zprender_ThreadContext_h

#include "RayShaderContext.h"
#include "Scene.h"
#include "Texture2dSampler.h"
#include "Traceable.h" // for SurfaceIntersectionList
#include "Volume.h"    // for VolumeIntersectionList

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/Interest.h> // for InterestRatchet


namespace zpr {

class RenderContext;
class BvhNode;


/*! The render context (zpr::Context) has one of these for each thread it's
    performing ray-shading calls with.

    It stores thread-safe variables for ray tracing to help avoid per-sample
    allocation/deletion costs for temporary structures.

    Since there's usually very few of these (one per thread) the vars can be
    generous with their memory reserves. ie even at 90 threads (which would
    kill Nuke performance in other ways,) 90 of these objects would only
    occupy a relatively small amount of temporary memory.
*/
class ZPR_EXPORT ThreadContext
{
  private:
    // Thread info:
    RenderContext* m_rtx;               //!< Parent RenderContext
    int            m_render_version;    //!< If different from m_rtx this context must be refreshed
    uint32_t       m_index;             //!< Thread index in RenderContext thread list
    pthread_t      m_ID;                //!< pthread ID

    // TODO: deprecate these when no longer supporting legacy lighting shaders:
    zpr::Scene        m_master_lighting_scene;          //!< Lighting contexts set to the frame time
    LightingSceneList m_per_object_lighting_scenes;     //!< Per-object-context lighting scenes (object filtered)


  private:
    //-------------------------------------------------------
    // Thread-safe variables that change on every ray sample:
    //-------------------------------------------------------
    std::vector<RayShaderContext> m_stx_list;       //!< Ray segment context list - index 0 is always the primary ray


  public:
    //--------------------------------------------------------
    // Thread-safe scratch data to be used during ray-tracing:
    //--------------------------------------------------------
    std::vector<const BvhNode*>          bvh_leafs; // For generic intersections (may not need this anymore)

    Traceable::SurfaceIntersectionList   I_list;
    Traceable::SurfaceIntersectionList   I_vol_list;
    std::vector<uint32_t>                index_list;
    Volume::VolumeIntersectionList       vol_intersections;
    Traceable::UVSegmentIntersectionList uv_intersections;

    Fsr::Pixel texture_color;       //!< Used for sampling texture map Iops
    Fsr::Pixel binding_color;       //!< Used for InputBinding getValue() calls
    Fsr::Pixel surface_color;       //!< Used for RayShader surface evaluation
    Fsr::Pixel illum_color;         //!< Used for LightShader evaluation
    Fsr::Pixel volume_color;        //!< Used for VolumeShader evaluation
    DD::Image::InterestRatchet textureColorInterestRatchet;
    DD::Image::InterestRatchet bindingColorInterestRatchet;
    DD::Image::InterestRatchet surfaceColorInterestRatchet;
    DD::Image::InterestRatchet illumColorInterestRatchet;
    DD::Image::InterestRatchet volumeColorInterestRatchet;

    // For calling legacy Iop-based materials:
    DD::Image::Scene         dummy_lighting_scene;
    DD::Image::VertexContext vtx;
    DD::Image::VArray        varray;

    // For passing to light shading methods:
    Fsr::RayContext Rlight;         //!< Ray from surface to light, filled in by LightShader::illuminate()
    float           direct_pdfW;    //!< Power distribution function weight, filled in by LightShader::illuminate()


  public:
    //! Constructor requires an zpr::Context, thread ID and it's index in the thread list.
    ThreadContext(RenderContext* rtx);
    //!
    ~ThreadContext();

    //!
    RenderContext* getRenderContext() const { return m_rtx; }

    //!
    int  getRenderVersion() const { return m_render_version; }
    void setRenderVersion(int v) { m_render_version = v; }

    //! Assign the thread info.
    void setThreadID(uint32_t  index,
                     pthread_t ID) { m_index = index; m_ID = ID; }

    uint32_t  index() const { return m_index; }
    pthread_t ID()    const { return m_ID; }

    //! Returns a reference to the shader context list.
    const std::vector<RayShaderContext>& shaderContextList() const { return m_stx_list; }


    //-----------------------------------------------------------------
    // Legacy Lighting:
    // TODO: deprecate these when no longer supporting legacy lighting shaders:
    //-----------------------------------------------------------------

    //! Return the lighting scene containing light Ops and their interpolated matrices.
    zpr::Scene& masterLightingScene() { return m_master_lighting_scene; }

    //! Return the lighting scene for a specfic object.
    zpr::Scene& objectLightingScene(uint32_t i) { return *m_per_object_lighting_scenes[i]; }

    //! Return the list of per-object-context lighting scenes (object filtered)
    LightingSceneList& perObjectLightingSceneList() { return m_per_object_lighting_scenes; }

    //! Destroy all the curently assigned lighting scenes.
    void clearLightingScenes();
    //-----------------------------------------------------------------


    //-------------------------------------------------------
    // RayShaderContext management:
    //-------------------------------------------------------

    //! Clear the shader context list but keep the memory allocation.
    void              clearShaderContexts() { m_stx_list.clear(); }

    //! Get the current RayShaderContext (the last in the list.)
    RayShaderContext& currentShaderContext() { assert(m_stx_list.size() > 0); return m_stx_list[m_stx_list.size()-1]; }

    //! Get the shader context for index i.
    RayShaderContext& getShaderContext(uint32_t i) { assert(i < m_stx_list.size()); return m_stx_list[i]; }

    //! Add a RayShaderContext to the end of the list, and return it, copying 'stx' if it's not NULL.
    RayShaderContext& pushShaderContext(const RayShaderContext* src_stx=NULL);

    RayShaderContext& pushShaderContext(const RayShaderContext&          src_stx,
                                        const Fsr::Vec3d&                Rdir,
                                        double                           tmin,
                                        double                           tmax,
                                        uint32_t                         ray_type,
                                        uint32_t                         sides_mode,
                                        const Fsr::RayDifferentials*     Rdif=NULL);

    RayShaderContext& pushShaderContext(const RayShaderContext&          src_stx,
                                        const Fsr::RayContext&           Rtx,
                                        uint32_t                         ray_type,
                                        uint32_t                         sides_mode,
                                        const Fsr::RayDifferentials*     Rdif=NULL);

    //! Remove a RayShaderContext from the end of the list, and return the new index or -1 if empty.
    int               popShaderContext();


  private:
    //! Disabled copy contructor.
    ThreadContext(const ThreadContext&);
    //! Disabled copy operator.
    ThreadContext& operator=(const ThreadContext&);

};


} // namespace zpr

#endif

// end of zprender/ThreadContext.h

//
// Copyright 2020 DreamWorks Animation
//
