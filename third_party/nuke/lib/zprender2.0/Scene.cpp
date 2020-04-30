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

/// @file zprender/Scene.cpp
///
/// @author Jonathan Egstad


#include "Scene.h"
#include "RenderContext.h"

#include <DDImage/Iop.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/Primitive.h>
#include <DDImage/GeometryList.h>
#include <DDImage/PrimitiveContext.h>


namespace zpr {


/*! Default constructor set the parent & context to NULL.
*/
Scene::Scene() :
    DD::Image::Scene(),
    frame(0.0),
    motion_step(0),
    m_parent(0),
    m_rtx(0),
    m_camera_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_geometry_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_material_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_lighting_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_changed_mask(0x0)
{
    _time = 0.0f;
    // TODO: build the materialBoxes map in validate():
    //std::map<Iop*, Box> _materialBoxes; //!< 2D bounding boxes for each material input
    //std::cout << "zpr::Scene::ctor(" << this << ")" << std::endl;
}


/*!
*/
Scene::Scene(DD::Image::Op* parent,
             RenderContext* rtx,
             int            mb_step,
             double         fr) :
    DD::Image::Scene(),
    frame(fr),
    motion_step(mb_step),
    m_parent(parent),
    m_rtx(rtx),
    m_camera_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_geometry_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_material_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_lighting_hash(0xf0f0f0f0f0f0f0f0ULL),
    m_changed_mask(0x0)
{
    _time = float(fr);
    // TODO: build the materialBoxes map in validate():
    //std::map<Iop*, Box> _materialBoxes; //!< 2D bounding boxes for each material input
    // Use the _id var as motion-sample indicator.  If it's negative then
    // the motion-step is back in time from frame0:
    _id = mb_step;
    //std::cout << "zpr::Scene::ctor(" << this << ")" << std::endl;
}


/*!
*/
/*virtual*/ 
Scene::~Scene()
{
    //std::cout << "zpr::Scene::dtor(" << this << ")" << std::endl;
}


//-----------------------------------------------------------------------------


/*! Copy the info out of the source scene, but don't copy actual geometry.
*/
void
Scene::copyInfo(const Scene* b)
{
#if 1
    // This should copy all the vars, including the lights:
    DD::Image::Scene::copyInfo(b);
#else
    // Copy scene vars:
    geo_               = b->geo_;
    //object_list_ = 
    //
    mb_type_           = b->mb_type_;
    mb_scene_          = b->mb_scene_;
    //
    quadtree_ = 0;
    render_primitives_.clear();
    mb_render_primitives_.clear();
    object_transforms_.clear();
    //
    transforms_        = b->transforms_;
    format_            = b->format_;
    //
    projection_mode_   = b->projection_mode_;
    max_tessellation_  = b->max_tessellation_;
    transparency_      = b->transparency_;
    //
    screen_bbox_       = b->screen_bbox_;
    channels_          = b->channels_;
    shadow_channels_   = b->shadow_channels_;
    filter_            = b->filter_;
    shadow_scene_      = false;
    //
    camera             = b->camera;
    lens_func          = b->lens_func;
    cam_vectors        = b->cam_vectors;
    //
    lights.clear();
    light_transforms.clear();
    light_renderers.clear();
    ambient            = b->ambient;
#endif

    // Copy zpr::Scene subclass vars:
    m_parent           = b->m_parent;
    m_rtx              = b->m_rtx;
    //
    m_object_map.clear(); // don't copy geometry info
    //
    m_camera_hash      = b->m_camera_hash;
    m_geometry_hash.reset(); // don't copy geometry info
    m_material_hash    = b->m_material_hash;
    m_lighting_hash    = b->m_lighting_hash;
    //
    m_changed_mask     = b->m_changed_mask;
}


//-----------------------------------------------------------------------------


/*! Find matching object id hash in the object map.
    Returns -1 if not found.
*/
int
Scene::findObject(const uint64_t& obj_id)
{
    const uint32_t nObjects = object_list_.size();
    if (nObjects == 0)
        return -1;

    if (m_object_map.empty())
    {
        // Build the map of output id hashes.  This is needed for fast searching
        // of matching objects in separate scenes for motion-blur purposes.
        //
        // Go through each GeoInfo adding its out_id hash to the map referencing the
        // object's index in the object_list_.  This way we can quickly find objects
        // with matching out_ids.
        for (uint32_t j=0; j < nObjects; ++j)
            m_object_map[object(j).out_id().value()] = j;
    }

    std::map<uint64_t, uint32_t>::iterator it = m_object_map.find(obj_id);
    if (it == m_object_map.end())
        return -1;
    return it->second;
}


/*! Find matching object id in the object map and return the object pointer.
*/
DD::Image::GeoInfo*
Scene::getObject(const uint64_t& obj_id)
{
    const int obj = findObject(obj_id);
    if (obj < 0 || obj >= (int)object_list_.size())
        return NULL;
    return &object_list_[obj];
}


//-----------------------------------------------------------------------------


/*! Sample index is not required since we use the absolute frame time instead.
*/
/*virtual*/
void
Scene::validate(int /*sample*/)
{
    //std::cout << "    Scene(" << this << ")::validate(): rtx=" << m_rtx << " motion_step=" << motion_step << " frame=" << frame << std::endl;
    //DD::Image::Scene::validate(sample);

    // Pre-compute world-space eigen vectors for camera:
    //std::cout << "      camera " << camera->matrix() << std::endl;
    if (camera)
    {
        const DD::Image::Matrix4& m = camera->matrix();
        cam_vectors.p.set(m.a03, m.a13, m.a23); // set the origin
        cam_vectors.x.set(m.a00, m.a10, m.a20); // X axis
        cam_vectors.y.set(m.a01, m.a11, m.a21); // Y axis
        cam_vectors.z.set(m.a02, m.a12, m.a22); // Z axis
        cam_vectors.x.normalize();
        cam_vectors.y.normalize();
        cam_vectors.z.normalize();
    }
    else
    {
        // No camera yet, clear the vectors:
        cam_vectors.p.set(0, 0, 0); // set the origin
        cam_vectors.x.set(0, 0, 0); // X axis
        cam_vectors.y.set(0, 0, 0); // Y axis
        cam_vectors.z.set(0, 0, 0); // Z axis
    }

    // Reset bboxes:
    DD::Image::Box3::clear();
    screen_bbox_.clear();
    channels_        = DD::Image::Mask_None;
    shadow_channels_ = DD::Image::Mask_None;

    // Null the object matrix:
    transforms_.set_object_matrix(DD::Image::Matrix4::identity());

    m_changed_mask = 0x0;
    DD::Image::Hash geometry_hash;
    DD::Image::Hash material_hash;
    DD::Image::Hash lighting_hash;
    DD::Image::Hash camera_hash;

    m_material_refs.clear();

    // If there's no geometry, and if there's no lights and atmospherics mode is on, bail quick:
    const uint32_t nObjects = objects();
    const uint32_t nLights = (uint32_t)lights.size();
    const bool     atmo_enabled = (m_rtx && m_rtx->k_atmospherics_enabled && m_rtx->k_direct_lighting_enabled);
    if (nObjects == 0 && (atmo_enabled && nLights==0))
        return;

    const DD::Image::Matrix4& world2screen = transforms_.matrix(WORLD_TO_SCREEN);
    // This is the maximum 2D bbox we can allow, otherwise some weird numerical
    // problems occur:
    static DD::Image::Box max_format_bbox(-1000000, -1000000, 1000000, 1000000);

    // Merge all primitive's bboxes together, and merge the channels of all materials:
    m_material_refs.resize(nObjects);
    for (uint32_t obj=0; obj < nObjects; ++obj)
    {
        DD::Image::GeoInfo& info = object(obj);

        MaterialRef& material_ref = m_material_refs[obj];
        material_ref.material = NULL;
        material_ref.type     = -1;

        // Skip object if render mode is off:
        if (info.render_mode == DD::Image::RENDER_OFF &&
            info.display3d   == DD::Image::DISPLAY_OFF)
            continue;

        // Combine the GeoInfo hashes together:
        geometry_hash.append(info.out_id());

        // Make sure primitives and attribute references are up-to-date:
        info.validate();

        // Get object bbox, but don't use the GeoInfo::update_bbox() method:
        //info.update_bbox();
        {
            Fsr::Box3f pbbox;
            if (info.point_list() && info.point_list()->size() > 0)
                pbbox.set(reinterpret_cast<const Fsr::Vec3f*>(info.point_list()->data()), info.point_list()->size());
            DD::Image::GeoInfo::Cache* writable_cache =
                const_cast<DD::Image::GeoInfo::Cache*>(info.get_cache_pointer());
            writable_cache->bbox = pbbox.asDDImage();
        }

// TODO: at this point in the scene validate we want to find and track any
// materials and shaders on Fuser Prims - how do we do this...? And is this the
// best place to do it?  I think so since we need to validate any texture Readers
// and get the channel list so that we can properly request() them later and make them
// available at the renderer's output.
        if (info.material && info.material != DD::Image::Iop::default_input(info.material->outputContext()))
        {


// We can detect if a GeoInfo does not have an active material assignment
// by comparing the pointer to the Iop::default_input(), which should be
// assigned to all non-connected inputs, or if it's NULL:
            if (info.material != DD::Image::Iop::default_input(info.material->outputContext()))
            {
                // Only validate the material if it's not a default black Iop from a dangling input:
                info.material->validate();
                channels_ += info.material->channels();

                material_ref.material        = info.material;
                material_ref.output_channels = info.material->channels();
                material_ref.hash            = info.material->hash();
                material_ref.type            = 0; // Iop

                // Combine the material hashes together:
                material_hash.append(info.material->hash());
            }
        }
        else if (info.primitives())
        {
            // Even if no material assignment output rgba:
            channels_ += DD::Image::Mask_RGBA;
        }


        DD::Image::Box3 bbox = info.bbox();
        //std::cout << "      " << obj << ": info=" << &info << ", bbox=" << Fsr::Box3f(info.bbox()) << std::endl;

        const DD::Image::Primitive** prim_array = info.primitive_array();
        if (prim_array)
        {
            const uint32_t nPrims = info.primitives();
            for (uint32_t j=0; j < nPrims; ++j)
            {
                const DD::Image::Primitive* prim = *prim_array++;

                // Do the primitives inside the GeoInfo expand the bbox further than the
                // point values imply? This is material displacement that's done below.
                // Example is a PointCloud with point radii that expand the points into
                // spheres, discs or cards.
                //
                // We only check custom zpr prims.
                //
                // TODO: finish this!!! Support the other types.
                if (prim->getPrimitiveType() > DD::Image::ePrimitiveTypeCount ||
                    prim->getPrimitiveType() == DD::Image::eParticlesSprite)
                {
                    bbox.expand(prim->get_bbox(&info));
                }

#if 0
                // TODO: we're ignoring per-prim material assignments - verify this is ok!!
                DD::Image::Iop* material = (*prim_array++)->material();
                if (material)
                    material->validate();
                channels_ += material->channels();

                // Combine the material hashes together:
                material_hash.append(material->hash());
#endif
            }
        }


        // Find the screen projected bbox of this object.
        // TODO: This should go into the RenderContext which manages the camera projections:
        if (projection_mode() == DD::Image::CameraOp::LENS_PERSPECTIVE ||
            projection_mode() == DD::Image::CameraOp::LENS_ORTHOGRAPHIC)
        {
            if (!info.bbox().empty())
            {
                // Transform it to world-space:
                bbox.transform(info.matrix);
                // Expand it by displacement bounds:
                float displace = 0.0f;
                if (info.material)
                    displace = info.material->displacement_bound();
                if (displace > std::numeric_limits<float>::epsilon())
                {
                    // Apply in world-space by scaling displacement:
                    const DD::Image::Vector3 dp = info.matrix.vtransform(DD::Image::Vector3(displace, displace, displace));
                    bbox.set(bbox.min() - dp, bbox.max() + dp);
                }
                //std::cout << "     " << obj << ": bbox[" << bbox.x() << " " << bbox.y() << " " << bbox.r() << " " << bbox.t() << "]" << std::endl;

                // Expand the Scene's world-space bbox:
                DD::Image::Box3::expand(bbox);

                // Check if camera is inside the object's bbox, because we
                // can't project a bbox that's surrounding the camera, so
                // set the projection size to the maximum:
                if (bbox.inside(cam_vectors.p))
                {
                    // Camera inside, set to maximum projection:
                    screen_bbox_ = max_format_bbox;
                }
                else
                {
                    // Project the object's bbox into screen space:
                    DD::Image::Box sbbox;
                    bbox.project(world2screen, sbbox);
                    // Clip to screen sides:
                    if (sbbox.x() >= format_->width()  || sbbox.r() < 0 ||
                        sbbox.y() >= format_->height() || sbbox.t() < 0)
                    {
                        // don't include object in screen bbox
                    }
                    else
                    {
                        // Clamp sbbox to max format values:
                        sbbox.intersect(max_format_bbox);
                        screen_bbox_.merge(sbbox);
                    }
                }
            }
        }
        else
        {
            // Non-linear projection mode, set to maximum projection:
            screen_bbox_ = max_format_bbox;
        }
    }


    // Get the shadow channels for any legacy lights:
    for (uint32_t j=0; j < nLights; j++)
    {
#if DEBUG
        assert(lights[j]);
        assert(lights[j]->light());
#endif
        const DD::Image::LightOp* light = lights[j]->light();
        shadow_channels_ += light->getShadowMaskChannel();

        // Combine the light hashes together:
        lighting_hash.append(light->hash());
    }


    // If a light can illuminate atmosphere then it becomes
    // a physical object of a certain size. Find that size.
    if (m_rtx && m_rtx->k_atmospherics_enabled)
    {
        for (uint32_t j=0; j < nLights; ++j)
        {
            // Get bbox from light prim:
            const DD::Image::LightOp* light = lights[j]->light();
            Fsr::Box3d lt_bbox;
            if (m_rtx->getVolumeLightTypeAndBbox(light, lt_bbox) == UNRECOGNIZED_PRIM)
                continue;

            const DD::Image::Box3 bbox(lt_bbox.asDDImage());

            // Expand the Scene's bbox:
            DD::Image::Box3::expand(bbox);

            // Check if camera is inside the object's bbox, because we
            // can't project a bbox that's surrounding the camera, so
            // set the projection size to the maximum:
            if (bbox.inside(cam_vectors.p))
            {
                // Camera inside, set to maximum projection:
                screen_bbox_ = max_format_bbox;
            }
            else
            {
                DD::Image::Box sbbox;
                bbox.project(world2screen, sbbox);
                // Clip to screen sides:
                if (sbbox.x() >= format_->width()  || sbbox.r() < 0 ||
                    sbbox.y() >= format_->height() || sbbox.t() < 0)
                {
                    // don't include object in screen bbox
                }
                else
                {
                    // Clamp sbbox to max format values:
                    sbbox.intersect(max_format_bbox);
                    screen_bbox_.merge(sbbox);
                }
            }
        }
    }
    //std::cout << "      screen_bbox[" << screen_bbox_.x() << " " << screen_bbox_.y() << " " << screen_bbox_.r() << " " << screen_bbox_.t() << "]" << std::endl;

    // Build changed mask:
    if (m_geometry_hash != geometry_hash)
        m_changed_mask |= GeometryFlag;
    if (m_material_hash != material_hash)
        m_changed_mask |= MaterialsFlag;
    if (m_lighting_hash != lighting_hash)
        m_changed_mask |= LightsFlag;
    if (m_camera_hash   != camera_hash  )
        m_changed_mask |= CameraFlag;
    m_geometry_hash = geometry_hash;
    m_material_hash = material_hash;
    m_lighting_hash = lighting_hash;
    m_camera_hash   = camera_hash;
    //std::cout << "     changed_mask=" << std::hex << m_changed_mask << std::dec << std::endl;
}


/*!
*/
/*virtual*/
void
Scene::request(const DD::Image::ChannelSet& channels,
               int                          count)
{
    //std::cout << "  zpr::Scene(" << this << ")::request(): channels=" << channels << ", count=" << count;
    //std::cout << ", changed_mask=0x" << std::hex << m_changed_mask << std::dec << std::endl;

    // Skip the request pass (it can be expensive) if the change mask is 0:
    //if (m_changed_mask == 0x0)
    //    return;

    // Something broke in 7.0v1 that is not letting the materials request properly,
    // so we're re-implementing the whole thing here:
    //
    DD::Image::MatrixArray tmp_transforms = transforms_;
    DD::Image::PrimitiveContext tmp_ptx;
    DD::Image::VArray tmp_varray;

    // Request each object's material:
    const uint32_t nObjects = object_list_.size();
    for (uint32_t i=0; i < nObjects; ++i)
    {
        DD::Image::GeoInfo& info = object_list_[i];
        DD::Image::Iop* material = info.material;
        // Don't bother if no material or we're not rendering the object:
        if (!material || info.render_mode == DD::Image::RENDER_OFF)
            continue;

        // Make sure primitives and attribute references are up-to-date:
        // info.validate(); not needed, validate() does this

        // The UV extents:
        Fsr::Vec2f uv_min( std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity());
        Fsr::Vec2f uv_max(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

        if (projection_mode_ <= DD::Image::CameraOp::LENS_ORTHOGRAPHIC || !camera)
        {
            /* Find min/max uv's for every material.  This is used to limit
               the amount of requested image cache when the camera only sees
               a small section of an object.

               WARNING:
               The current code only works well on tessellated objects with a
               standard perspective projection.
               If a simple object with a 2x2 mesh has a huge texture map
               attached to it, this code doesn't save much because there's not
               enough points to reduce the texture request area.
               A better implemtation may be to put a method on Primitive so that
               each primitive class can trivially intersect itself with the screen,
               finding its bounds using a cheaper method than intersecting each
               scanline.
               Alternatively we can wait until the renderer's engine call to
               intersect with the screen.  I'm not sure how efficient repeatedly
               calling 'cache_new_pass()' for each primitive.
            */

            tmp_transforms.set_object_matrix(info.matrix);
            //const Matrix4 m0 = tmp_transforms.matrix(LOCAL_TO_CLIP);

            const DD::Image::Primitive** prim_array = info.primitive_array();
            const uint32_t nPrims = info.primitives();
            for (uint32_t j=0; j < nPrims; ++j)
            {
                const DD::Image::Primitive* p = *prim_array++;
                //
                tmp_ptx.set_geoinfo(&info, /*mb*/0);
                tmp_ptx.set_transforms(&tmp_transforms, /*mb*/0);
                tmp_ptx.setPrimitive(const_cast<DD::Image::Primitive*>(p));
                tmp_ptx.setPrimitiveIndex(j);
#if 1
                // Nasty hack to get around private var - relies on C++
                // var packing to be sequential:
                uint32_t* face_clipmask = reinterpret_cast<uint32_t*>(const_cast<DD::Image::Box3*>(&tmp_ptx.face_uv_bbox() + 1));
                *face_clipmask = 0x00;
#else
                tmp_ptx.face_clipmask_ = 0x00;
#endif
            
                // Do all faces in the primitive:
                uint32_t tmp_face_vertices[1000];
                const uint32_t nFaces = p->faces();
                for (uint32_t f=0; f < nFaces; ++f)
                {
                    const uint32_t nVerts = p->face_vertices(f);
                    p->get_face_vertices(f, tmp_face_vertices);
                    // Copying the prim context keeps the clipmask & uv_bbox empty
                    // since we don't have public access to them...sigh:
                    DD::Image::PrimitiveContext tmp_ptx1 = tmp_ptx;

                    // Call the vertex shader for each vertex:
                    for (uint32_t v=0; v < nVerts; ++v)
                       p->vertex_shader(tmp_face_vertices[v], this, &tmp_ptx, tmp_varray);

                    // If all the face verts are clipped, skip those uvs:
                    if      (tmp_ptx.face_clipmask() == 0x01) continue; // Right
                    else if (tmp_ptx.face_clipmask() == 0x02) continue; // Left
                    else if (tmp_ptx.face_clipmask() == 0x04) continue; // Top
                    else if (tmp_ptx.face_clipmask() == 0x08) continue; // Bottom
                    else if (tmp_ptx.face_clipmask() == 0x10) continue; // Near

                    uv_min.x = std::min(uv_min.x, tmp_ptx.face_uv_bbox().x());
                    uv_min.y = std::min(uv_min.y, tmp_ptx.face_uv_bbox().y());
                    uv_max.x = std::max(uv_max.x, tmp_ptx.face_uv_bbox().r());
                    uv_max.y = std::max(uv_max.y, tmp_ptx.face_uv_bbox().t());
                }
            }
        }

        DD::Image::Box bbox;
        if (uv_min.x < uv_max.x && uv_min.y < uv_max.y)
        {
            // We have a valid uv extent, use it:
            const DD::Image::Format& f = material->format();
            const float fX = float(f.x());
            const float fY = float(f.y());
            const float fW = float(f.w());
            const float fH = float(f.h());
            const DD::Image::Box& b = material->info();

            // In some cases, the conversion from float to int to calculate X,Y,R,T can have overflow errors
            // To avoid it be sure to clamp s,t inside material info box
            float s1 = uv_min.x;
            float t1 = uv_min.y;
            float Xf = (fW * s1 + fX) + 0.01f;
            float Yf = (fH * t1 + fY) + 0.01f;
            int X = int( floorf( clamp( Xf, b.x(), b.x()+b.w() ) ) );
            int Y = int( floorf( clamp( Yf, b.y(), b.y()+b.h() ) ) );

            float s2 = uv_max.x;
            float t2 = uv_max.y;
            float Rf = (fW * s2 + fX) + 0.01f;
            float Tf = (fH * t2 + fY) + 0.01f;
            int R = int( floorf( clamp( Rf, b.x(), b.x()+b.w() ) ) )+1;
            int T = int( floorf( clamp( Tf, b.y(), b.y()+b.h() ) ) )+1;

            bbox.set(X, Y, R, T);

        }
        else
        {
            // No uv extents, use max:
            bbox = material->info();
        }
        // Call request() on the material:
        material->request(bbox, channels, count);
        //std::cout << "material " << material << " channels=" << material->requested_channels();
        //std::cout << " bbox[" << material->requestedBox().x() << " " << material->requestedBox().y();
        //std::cout << " " <<  material->requestedBox().r() << " " << material->requestedBox().t() << "]";
        //std::cout << std::endl;
    }

    // Request RGB from each light:
    DD::Image::ChannelSet light_channels(DD::Image::Mask_RGBA);
    const size_t nLights = lights.size();
    for (size_t i=0; i < nLights; ++i)
    {
        const DD::Image::LightContext* ltx = lights[i];
        assert(ltx); // shouldn't happen...
        DD::Image::LightOp* l = ltx->light();
        assert(l); // shouldn't happen...
        if (l->node_disabled())
            continue;

        l->request(light_channels, count);
    }

}


//-----------------------------------------------------------------------------


/*virtual*/
void
Scene::add_light(DD::Image::LightOp* light)
{
    //std::cout << "  zpr::Scene::add_light(" << this << ") - UNUSED" << std::endl;
    DD::Image::Scene::add_light(light);
}

/*virtual*/
void
Scene::add_lights(const std::vector<DD::Image::LightOp*>* light_list)
{
    //std::cout << "  zpr::Scene::add_lights(" << this << ") - UNUSED" << std::endl;
    DD::Image::Scene::add_lights(light_list);
}

/*virtual*/
void
Scene::clear_lights()
{
    //std::cout << "  zpr::Scene::clear_lights(" << this << ") - UNUSED" << std::endl;
    DD::Image::Scene::clear_lights();
}

/*virtual*/
bool
Scene::evaluate_lights()
{
    //std::cout << "  zpr::Scene::evaluate_lights(" << this << ") - UNUSED" << std::endl;
    // Base class will evaluate transforms and validate lights:
    return DD::Image::Scene::evaluate_lights();
}

/*virtual*/
void
Scene::delete_light_context()
{
    //std::cout << "  zpr::Scene::delete_light_context(" << this << ") - UNUSED" << std::endl;
    DD::Image::Scene::delete_light_context();
}


//-----------------------------------------------------------------------------


/*! Disable this method - unused. */
/*virtual*/
void
Scene::add_render_primitive(DD::Image::rPrimitive*       prim,
                            DD::Image::PrimitiveContext* ptx)
{
    std::cout << "  zpr::Scene::add_render_primitive(" << prim << ") - UNUSED" << std::endl;
    delete prim;
}

/*! Disable this method - unused. */
/*virtual*/
void
Scene::add_clipped_render_primitive(DD::Image::rPrimitive* prim)
{
    std::cout << "  zpr::Scene::add_clipped_render_primitive(" << prim << ") - UNUSED" << std::endl;
    delete prim;
}

/*! Disable this method - unused. */
/*virtual*/
void
Scene::add_clipped_displacement_render_primitive(DD::Image::rPrimitive* prim)
{
    std::cout << "  zpr::Scene::add_clipped_render_primitive(" << prim << ") - UNUSED" << std::endl;
    delete prim;
}


//-----------------------------------------------------------------------------


/*virtual*/
bool
Scene::generate_render_primitives()
{
    //std::cout << "  zpr::Scene::generate_render_primitives(" << this << ") - UNUSED" << std::endl;
    return true;
}

/*virtual*/
void
Scene::delete_render_primitives()
{
    //std::cout << "    zpr::Scene::delete_render_primitives(" << this << ") - UNUSED" << std::endl;
}


} // namespace zpr

// end of zprender/Scene.cpp

//
// Copyright 2020 DreamWorks Animation
//
