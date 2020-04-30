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
#include "Traceable.h" // for SurfaceIntersectionList
#include "Volume.h"    // for VolumeIntersectionList

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/Interest.h> // for InterestRatchet


namespace zpr {

class RenderContext;
class BvhNode;
class TextureSamplerContext;



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
    RenderContext* m_rtx;           //!< Parent RenderContext
    uint32_t       m_index;         //!< Thread index in RenderContext thread list
    pthread_t      m_ID;            //!< pthread ID

    // TODO: deprecate these when no longer supporting legacy lighting shaders:
    zpr::Scene        m_master_lighting_scene;          //!< Lighting contexts set to the frame time
    LightingSceneList m_per_object_lighting_scenes;     //!< Per-object-context lighting scenes (object filtered)

    std::vector<TextureSamplerContext*> m_tex_samplers;     //!< List of active texture samplers for this thread
    std::map<uint64_t, uint32_t>        m_tex_sampler_map;  //!< Texture ID to texture sampler index map


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

    Fsr::Pixel surface_color;
    Fsr::Pixel light_color;
    Fsr::Pixel volume_color;
    DD::Image::InterestRatchet surfaceColorInterestRatchet;
    DD::Image::InterestRatchet lightColorInterestRatchet;
    DD::Image::InterestRatchet volumeColorInterestRatchet;

    // For calling legacy Iop-based materials:
    DD::Image::Scene         dummy_lighting_scene;
    DD::Image::VertexContext vtx;
    DD::Image::VArray        varray;


  public:
    //! Constructor requires an zpr::Context, thread ID and it's index in the thread list.
    ThreadContext(RenderContext* rtx);
    //!
    ~ThreadContext();

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

    //! Get the shader context for index i.
    RayShaderContext& getShaderContext(uint32_t i) { assert(i < m_stx_list.size()); return m_stx_list[i]; }

    //! Add a RayShaderContext to the end of the list, and return its index.
    uint32_t          pushShaderContext(const RayShaderContext* current=0);

    //! Remove a RayShaderContext from the end of the list, and return the new index or -1 if empty.
    int               popShaderContext();

    //! Clear the shader context list but keep the memory allocation.
    void              clearShaderContexts() { m_stx_list.clear(); }

    //! Clears the shader context list and deletes the memory allocation.
    void              destroyShaderContexts() { m_stx_list = std::vector<RayShaderContext>(); }


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
