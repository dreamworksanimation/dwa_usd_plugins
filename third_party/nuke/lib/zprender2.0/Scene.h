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

/// @file zprender/Scene.h
///
/// @author Jonathan Egstad


#ifndef zprender_Scene_h
#define zprender_Scene_h

#include "api.h"

#include <DDImage/Scene.h>

#include <string>


namespace zpr {

class RenderContext;
class MaterialContext;


/*! Extension of the default DDImage Scene class to fill in that
    class' gaps.

    TODO: We really only need this class so that we can pass it to the
    legacy shader system for lights.
*/
class ZPR_EXPORT Scene : public DD::Image::Scene
{
  public:
    //! Scene part masks.
    enum
    {
        GeometryFlag    = 0x00000001,
        MaterialsFlag   = 0x00000002,
        LightsFlag      = 0x00000004,
        CameraFlag      = 0x00000008,
        //
        AllPartsFlag  = GeometryFlag | MaterialsFlag | LightsFlag | CameraFlag
    };

    double frame;                   //!< This scene's absolute frame number
    int    motion_step;             //!< Which motion step this scene represents


    /*!
    */
    struct MaterialRef
    {
        DD::Image::Iop*       material;
        DD::Image::ChannelSet output_channels;
        DD::Image::Hash       hash;
        uint32_t              type;
    };


  protected:
    DD::Image::Op* m_parent;                    //!< Which op is our parent (renderer)
    RenderContext* m_rtx;                       //!< Render context

    std::map<uint64_t, uint32_t> m_object_map;      //!< Object-ID -> object-index map
    std::vector<MaterialRef>     m_material_refs;   //!< Per-object material contexts

    DD::Image::Hash         m_camera_hash;      //!< Hash value of current camera
    DD::Image::Hash         m_geometry_hash;    //!< Hash value of all geometric params
    DD::Image::Hash         m_material_hash;    //!< Hash value of all materials
    DD::Image::Hash         m_lighting_hash;    //!< Hash value of all lights
    DD::Image::GeometryMask m_changed_mask;     //!< If a part changed it's bit is set (after validate() is called)


  public:
    Scene();
    Scene(DD::Image::Op* parent,
          RenderContext* rtx,
          int            mb_step=0,
          double         fr=0.0);

    /*virtual*/ ~Scene();


    //! Returns the RenderContext object owned by the parent renderer.
    RenderContext& rtx() { return *m_rtx; }

    //! Copy the info out of the source scene, but don't copy actual geometry.
    void copyInfo(const Scene* b);

    //!
    void clearChangedMask() { m_changed_mask = 0x0; }

    //===========================================================

    //! Find matching object id hash in object map. Returns -1 if not found.
    int findObject(const uint64_t& obj_id);
    int findObject(const char* id_string) { return findObject(strtoull(id_string, 0, 16)); }

    //! Find matching object id in object map and return the object pointer.
    DD::Image::GeoInfo* getObject(const uint64_t& obj_id);
    DD::Image::GeoInfo* getObject(const char* id_string) { return getObject(strtoull(id_string, 0, 16)); }


    //===========================================================
    // Methods to expose private vars in the DD::Image::Scene class:

    std::vector<DD::Image::MatrixArray>& object_transforms_list() { return object_transforms_; }

    void setGeoOp(DD::Image::GeoOp* geo) { geo_ = geo; }
    void setMotionblurScene(DD::Image::Scene* s) { mb_scene_ = s; }
    void setFormat(const DD::Image::Format* f) { format_ = f; }
    void setProjectionMode(int v) { projection_mode_ = v; }
    void setMaxTessellation(int v) { max_tessellation_ = v; }
#if 0
    void setScreenBbox(const DD::Image::Box& bbox) { screen_bbox_ = bbox; }
    void setChannels(const DD::Image::ChannelSet& chans) { channels_ = chans; }

    void clearObjectTransforms() { object_transforms_.clear(); }
    void reserveObjectTransforms(uint32_t n) { object_transforms_.reserve(n); }
    void addObjectTransforms(DD::Image::MatrixArray* m) { object_transforms_.push_back(*m); }
    void setObjectTransforms(int i,
                             DD::Image::MatrixArray* m) { object_transforms_[i] = *m; }
#endif
    //===========================================================


    //===========================================================
    // From DD::Image::Scene class:

    /*virtual*/ void validate(int /*sample*/);
    /*virtual*/ void request(const DD::Image::ChannelSet& channels,
                             int count);

    /*virtual*/ void add_light(DD::Image::LightOp* light);
    /*virtual*/ void add_lights(const std::vector<DD::Image::LightOp*>* light_list);
    /*virtual*/ void clear_lights();
    /*virtual*/ bool evaluate_lights();
    /*virtual*/ void delete_light_context();
    /*virtual*/ void delete_render_primitives();
    /*virtual*/ bool generate_render_primitives();
    /*virtual*/ void add_render_primitive(DD::Image::rPrimitive*,
                                          DD::Image::PrimitiveContext*);
    /*virtual*/ void add_clipped_render_primitive(DD::Image::rPrimitive*);
    /*virtual*/ void add_clipped_displacement_render_primitive(DD::Image::rPrimitive*);
    //===========================================================

};


} // namespace zpr


#endif

// end of zprender/Scene.h

//
// Copyright 2020 DreamWorks Animation
//
