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

/// @file Fuser/NukeGeoInterface.cpp
///
/// @author Jonathan Egstad

#include "NukeGeoInterface.h"
#include "NodePrimitive.h"

#include <DDImage/GeoOp.h>
#include <DDImage/ParticlesSprite.h>
#include <DDImage/gl.h>


namespace Fsr {

//-----------------------------------------------------------------------------

/*static*/ const char* GeoOpGeometryEngineContext::name = "GeoOpGeometryEngine";

//-----------------------------------------------------------------------------


/*! Copy a string pointer to a constant one stored in a static map.

    This is primarily used for DD::Image::Attribute names which need
    to stick around so that the const char*s used to reference the
    names don't suddenly disappear!
*/
static const char*
getConstStr(const char* var,
            bool        lock=true)
{
    static std::set<std::string> attrib_const_strings;
    static DD::Image::Lock       attrib_const_lock;

    if (lock)
        attrib_const_lock.lock();
    std::pair<std::set<std::string>::const_iterator, bool> it = attrib_const_strings.insert(std::string(var));
    if (lock)
        attrib_const_lock.unlock();

    return it.first->c_str();
}


//-----------------------------------------------------------------------------


/*! Return the GeoInfo's point array cast to a Vec3f. *** Not thread safe! ***
*/
const Fsr::Vec3f*
getObjectPointArray(const DD::Image::GeoInfo& info)
{
    return reinterpret_cast<const Fsr::Vec3f*>(info.point_array());
}


/*! Get an attribute from the object level of a GeoInfo. *** Not thread safe! ***
*/
std::string
getObjectString(const DD::Image::GeoInfo& info,
                const char*               attrib_name,
                const std::string&        dflt_val)
{
    const DD::Image::AttribContext* ctx = info.get_group_attribcontext(DD::Image::Group_Object, attrib_name);
    if (!ctx || ctx->empty() || !ctx->attribute)
    {
        //std::cout << "warning, object attrib '" << attrib_name << "' not found!" << std::endl;
        return dflt_val;
    }

    if (ctx->type == DD::Image::STRING_ATTRIB)
        return std::string(ctx->attribute->string(0));
    else if (ctx->type == DD::Image::STD_STRING_ATTRIB)
        return ctx->attribute->stdstring(0);

    return dflt_val;
}

/*!
*/
int32_t
getObjectInt(const DD::Image::GeoInfo& info,
             const char*               attrib_name,
             int32_t                   dflt_val)
{
    const DD::Image::AttribContext* ctx = info.get_typed_group_attribcontext(DD::Image::Group_Object,
                                                                             attrib_name,
                                                                             DD::Image::INT_ATTRIB);
    if (!ctx || ctx->empty() || !ctx->attribute)
    {
        //std::cout << "warning, object attrib '" << attrib_name << "' not found!" << std::endl;
        return dflt_val;
    }
    return int32_t(ctx->attribute->integer(0));
}
bool
getObjectBool(const DD::Image::GeoInfo& info,
              const char*               attrib_name,
              bool                      dflt_val)
{
    return (getObjectInt(info, attrib_name, 0) > 0);
}


/*!
*/
float
getObjectFloat(const DD::Image::GeoInfo& info,
               const char*               attrib_name,
               float                     dflt_val)
{
    const DD::Image::AttribContext* ctx = info.get_typed_group_attribcontext(DD::Image::Group_Object,
                                                                             attrib_name,
                                                                             DD::Image::FLOAT_ATTRIB);
    if (!ctx || ctx->empty() || !ctx->attribute)
    {
        //std::cout << "warning, object attrib '" << attrib_name << "' not found!" << std::endl;
        return dflt_val;
    }
    return ctx->attribute->flt(0);
}

/*!
*/
bool
hasObjectAttrib(const DD::Image::GeoInfo& info,
                const char*               attrib_name)
{
    const DD::Image::AttribContext* ctx = info.get_group_attribcontext(DD::Image::Group_Object, attrib_name);
    return (ctx && !ctx->empty() && ctx->attribute);
}

/*!
*/
void*
getAttribData(const DD::Image::GeoInfo& info,
              DD::Image::GroupType      attrib_group,
              const char*               attrib_name,
              DD::Image::AttribType     attrib_type)
{
    const DD::Image::AttribContext* ctx = info.get_typed_group_attribcontext(attrib_group, attrib_name, attrib_type);
    if (!ctx || ctx->empty() || !ctx->attribute)
    {
        //std::cout << "warning, object attrib '" << attrib_name << "' not found!" << std::endl;
        return NULL;
    }
    return ctx->attribute->array();
}


//-----------------------------------------------------------------------------


/*! Set an Attribute value at the 'Group_Object' level of a GeoInfo. *** Not thread safe! ***
*/
void
setObjectString(const char*              attrib_name,
                const std::string&       attrib_value,
                uint32_t                 obj_index,
                DD::Image::GeometryList& geometry_list)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        geometry_list.writable_attribute(obj_index,
                                         DD::Image::Group_Object,
                                         getConstStr(attrib_name),
                                         DD::Image::STD_STRING_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->stdstring(0) = attrib_value;
}

/*!
*/
void
setObjectInt(const char*              attrib_name,
             int32_t                  attrib_value,
             uint32_t                 obj_index,
             DD::Image::GeometryList& geometry_list)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        geometry_list.writable_attribute(obj_index,
                                         DD::Image::Group_Object,
                                         getConstStr(attrib_name),
                                         DD::Image::INT_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->integer(0) = attrib_value;
}

/*!
*/
void
setObjectFloat(const char*              attrib_name,
               float                    attrib_value,
               uint32_t                 obj_index,
               DD::Image::GeometryList& geometry_list)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        geometry_list.writable_attribute(obj_index,
                                         DD::Image::Group_Object,
                                         getConstStr(attrib_name),
                                         DD::Image::FLOAT_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->flt(0) = attrib_value;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/*!
*/
GeoInfoCacheRef::GeoInfoCacheRef() :
    obj(-1),
    points_list(NULL),
    primitives_list(NULL),
    attributes_list(NULL)
{
    //
}

/*!
*/
GeoInfoCacheRef::GeoInfoCacheRef(const GeoInfoCacheRef& b) :
    obj(b.obj),
    bbox(b.bbox),
    points_list(b.points_list),
    primitives_list(b.primitives_list),
    m_attributes_list(b.m_attributes_list)
{
    attributes_list = &m_attributes_list; // point to the new local copy
}


/*!
*/
GeoInfoCacheRef::GeoInfoCacheRef(uint32_t          obj_index,
                                 DD::Image::GeoOp* geo) :
    obj(obj_index),
    points_list(NULL),
    primitives_list(NULL),
    attributes_list(NULL)
{
    if (geo)
    {
        const std::vector<DD::Image::GeoInfo::Cache>& geo_cache_list = geo->getCacheList();
        if (obj_index < (uint32_t)geo_cache_list.size())
            updateFromGeoInfoCache(const_cast<DD::Image::GeoInfo::Cache*>(&geo_cache_list[obj_index]));
    }
}


/*!
*/
GeoInfoCacheRef::GeoInfoCacheRef(uint32_t                       obj_index,
                                 const DD::Image::GeometryList& geometry_list) :
    obj(obj_index),
    points_list(NULL),
    primitives_list(NULL),
    attributes_list(NULL)
{
    updateFromGeometryList(geometry_list);
}

/*!
*/
GeoInfoCacheRef&
GeoInfoCacheRef::operator = (const GeoInfoCacheRef& b)
{
    obj             = b.obj;
    bbox            = b.bbox;
    points_list     = b.points_list;
    primitives_list = b.primitives_list;

    m_attributes_list = b.m_attributes_list;
    attributes_list   = &m_attributes_list; // point to the new local copy

    return *this;
}


/*! Update the geometry data pointers with the GeoInfo::Cache for this object index.

    !!Acquire write_lock before calling!!

    Note the local m_attributes_list points to the same Attribute allocations
    as the ones in the GeoInfo::Cache, so the entire list can be copied locally.
*/
void
GeoInfoCacheRef::updateFromGeometryList(const DD::Image::GeometryList& geometry_list)
{
    if (obj < 0 || obj >= (int)geometry_list.size())
    {
        bbox.setToEmptyState();
        points_list     = NULL;
        primitives_list = NULL;
        attributes_list = NULL;
        return;
    }
    updateFromGeoInfoCache(const_cast<DD::Image::GeoInfo::Cache*>(geometry_list[obj].get_cache_pointer()));
}


/*! Update the geometry data pointers from a GeoInfo::Cache.

    !!Acquire write_lock before calling!!

    Note the local m_attributes_list points to the same Attribute allocations
    as the ones in the GeoInfo::Cache, so the entire list can be copied locally.
*/
void
GeoInfoCacheRef::updateFromGeoInfoCache(const DD::Image::GeoInfo::Cache* geoinfo_cache)
{
    if (!geoinfo_cache)
    {
        bbox.setToEmptyState();
        points_list     = NULL;
        primitives_list = NULL;
        attributes_list = NULL;
        return;
    }

    DD::Image::GeoInfo::Cache* writable_cache = const_cast<DD::Image::GeoInfo::Cache*>(geoinfo_cache);

    bbox = writable_cache->bbox;

    // Copy the pointers to the underlying vectors rather than the wrapper structure
    // as the underlying pointers won't change as the GeoInfo mem locations move
    // around as threads append objects to the GeometryList:
    points_list     = (writable_cache->points    ) ? &( *writable_cache->points            ) : NULL;
    primitives_list = (writable_cache->primitives) ? &((*writable_cache->primitives).data()) : NULL;

    // Make a local copy the entire AttribContextList vector as the Attribute
    // pointers in each AttribContext will remain the same but the mem location
    // of the list will move around as threads add objects to the GeometryList:
    m_attributes_list = writable_cache->attributes;
    attributes_list = &m_attributes_list;
}


/*!
*/
DD::Image::Primitive*
GeoInfoCacheRef::getPrimitive(size_t i)
{
    if (!primitives_list || i >= primitives_list->size())
        return NULL;
    return const_cast<DD::Image::Primitive*>((*primitives_list)[i]);
}

/*!
*/
Fsr::FuserPrimitive*
GeoInfoCacheRef::getFuserPrimitive(size_t i)
{
    return dynamic_cast<Fsr::FuserPrimitive*>(getPrimitive(i));
}

/*!
*/
Fsr::NodePrimitive*
GeoInfoCacheRef::getFuserNodePrimitive(size_t i)
{
    return dynamic_cast<Fsr::NodePrimitive*>(getPrimitive(i));
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//! Ctor is not thread safe! It assumes no worker threads are active!
GeoOpGeometryEngineContext::GeoOpGeometryEngineContext(int                      num_threads,
                                                       DD::Image::GeoOp*        _geo,
                                                       DD::Image::GeometryList* _geometry_list,
                                                       DD::Image::Scene*        _scene) :
    geo(_geo),
    geometry_list(_geometry_list),
    scene(_scene),
    geoop_context(NULL),
    m_multithreaded(num_threads > 1),
    m_write_owner(0)
{
    assert(geo); // shouldn't happen...

#ifdef DWA_INTERNAL_BUILD
    typedef std::map<DD::Image::GeoOp*, GeoOpGeometryEngineContext::GeoOpContext*> GeoOpContextMap;
#else
    typedef std::unordered_map<DD::Image::GeoOp*, GeoOpGeometryEngineContext::GeoOpContext*> GeoOpContextMap;
#endif
    static GeoOpContextMap geoop_context_map;

    const GeoOpContextMap::const_iterator it = geoop_context_map.find(geo);
    if (it == geoop_context_map.end())
    {
        geoop_context = new GeoOpContext();
        geoop_context_map[geo] = geoop_context;
    }
    else
    {
        geoop_context = it->second;
    }
    assert(geoop_context);
}


//-----------------------------------------------------------------------------


/*! Acquire the write lock, this will spin until write_owner becomes 0,
    then sets write_owner to the current thread id.

    Does nothing if 'm_multithreaded' is false.
*/
void
GeoOpGeometryEngineContext::acquireWriteLock()
{
    if (!m_multithreaded)
        return;

    //m_write_lock.lock();
    //std::cout << "            acquireWriteLock(" << std::hex << DD::Image::Thread::GetThreadId() << std::dec << ")" << std::endl;
    //m_write_lock.unlock();

    // Wait for the write_owner var to free up.
    // (using a condition_variable here is more effecient than
    //   my old method of using a short delay in a loop):
    std::unique_lock<std::mutex> lock(m_write_lock);

    while (m_write_owner != 0)
        m_write_lock_cv.wait(lock); // Atomically release m_write_lock, wait for notify, relock

    m_write_owner = DD::Image::Thread::GetThreadId();
}


/*! Release the write lock - sets m_write_owner to 0 then
    notifies the other threads that they can try aquiring
    the lock.

    Does nothing if 'm_multithreaded' is false.
*/
void
GeoOpGeometryEngineContext::releaseWriteLock()
{
    if (!m_multithreaded)
        return;

    m_write_lock.lock();
    if (m_write_owner != 0)
    {
        //std::cout << "                releaseWriteLock(" << std::hex << m_write_owner << std::dec << ")" << std::endl;

        // Release owner and notify waiting threads in the
        // m_write_lock_cv.wait(lock) loop above in acquireWriteLock():
        m_write_owner = 0;
        m_write_lock.unlock();

        m_write_lock_cv.notify_all();
    }
    else
        m_write_lock.unlock();
}


//-----------------------------------------------------------------------------


/*! Empty the object id map for this GeoOpContext.
    This is not thread safe, it assumes no worker threads are active!
*/
void
GeoOpGeometryEngineContext::clearObjectIds()
{
    assert(geoop_context); // shouldn't happen...

    geoop_context->object_id_map.clear();
}


/*! Reserve the next available object index, keyed to object_id string.
    !!Acquire write_lock before calling!!
*/
int
GeoOpGeometryEngineContext::addObjectIndexFromId(const std::string& object_id)
{
    assert(geoop_context); // shouldn't happen...

    const int next_index = (int)geometry_list->size();
    geoop_context->object_id_map[object_id] = next_index; // next available object slot
    return next_index;
}


/*! Returns -1 if there's object matching that name in the map.
    !!Acquire write_lock before calling!!
*/
int
GeoOpGeometryEngineContext::getObjectIndexFromId(const std::string& object_id)
{
    assert(geoop_context); // shouldn't happen...

    const ObjectIndexMap::const_iterator it = geoop_context->object_id_map.find(object_id);
    return (it == geoop_context->object_id_map.end()) ? -1 : it->second;
}


//-----------------------------------------------------------------------------


/*!
*/
Fsr::NodePrimitive*
GeoOpGeometryEngineContext::createFuserNodePrimitiveThreadSafe(const char*        node_class,
                                                               const Fsr::ArgSet& args)
{
    Fsr::NodePrimitive* fprim = NULL;

    acquireWriteLock();
    {
        fprim = new Fsr::NodePrimitive(node_class,
                                       args,
                                       Fsr::defaultTimeValue());
    }
    releaseWriteLock();

    return fprim;

}


//-----------------------------------------------------------------------------


/*! Thread-safe retrieve an object from the GeometryList, filling in the GeoInfoCacheRef.
    Returns -1 if not found.
*/
int
GeoOpGeometryEngineContext::getObjectThreadSafe(const std::string& object_id,
                                                GeoInfoCacheRef&   geoinfo_cache)
{
    geoinfo_cache.obj = -1;
    if (!geometry_list)
        return -1; // don't crash...

    acquireWriteLock();
    {
        geoinfo_cache.obj = getObjectIndexFromId(object_id);
        if (geoinfo_cache.obj >= 0)
            geoinfo_cache.updateFromGeometryList(*geometry_list);
    }
    releaseWriteLock();

    return geoinfo_cache.obj;
}


/*! Append an object to the GeometryList, filling in the GeoInfoCacheRef. Thread-safe.

    'object_id' *must* be unique for not only this entire GeometryList but also for
    the object identifier to work across multiple GeoOp instances (due to frame or
    view differences.)

    For example this is imperative in allowing object matching to work for motionblur
    to allow a renderer to find the same object across multiple frames.

    An example of a *bad* object id string would be 'cube', while a good example
    would be '/Scene/envir/room1/Geometry/box2/cube', where the absolute object
    path helps stop multiple 'cube' objects from conflicing.
*/
void
GeoOpGeometryEngineContext::addObjectThreadSafe(const std::string& object_id,
                                                GeoInfoCacheRef&   geoinfo_cache)
{
    geoinfo_cache.obj = -1;
    if (!geometry_list)
        return; // don't crash...

    acquireWriteLock();
    {
        // Does this object already exist in list?
        geoinfo_cache.obj = getObjectIndexFromId(object_id);
        const bool is_new_object = (geoinfo_cache.obj < 0);

        // No, create a new object index:
        if (is_new_object)
            geoinfo_cache.obj = addObjectIndexFromId(object_id);

        assert(geoinfo_cache.obj >= 0); // shouldn't happen...

        //****************************************************************************
        // Always call out.add_object(obj) even if we're just replacing points,
        // otherwise the GeoInfos won't properly validate and the bboxes won't
        // be up to date:
        geometry_list->add_object(geoinfo_cache.obj);
        //****************************************************************************

        if (is_new_object)
        {
            //---------------------------------------------------------------------------
            // The src_id() and out_id() of the resulting GeoInfo cache entry needs to be
            // unique so a multithreaded (likely random) creation order will always
            // produce the same ID hash.
            //
            // The current DD::Image::GeometryList::add_object() method will append the
            // object's index to the hash causing it to be different when there's different
            // object creation orders.
            // ie if the object is index 5 for the GeoOp at frame 100 it may be index 2 when
            // the GeoOp is created at frame 98.5. This causes motionblur object matching code
            // to fail as matching out_id hashes can come from different objects!
            //
            // Here's the current DD::Image GeometryList::add_object() hash creation logic:
            //    DD::Image::Hash obj_id;
            //    obj_id.reset()
            //
            //    void* node_address = geo_->node();
            //    obj_id.append(&node_address, sizeof(void*));
            //
            //    obj_id.append(obj_index + 1); <<<<<<<<< BAD IDEA!!!
            //
            //    out_cache.src_id = obj_id;
            //    out_cache.out_id = obj_id;
            //    out_cache.out_id.append(out_cache.version); // bump by geometry_engine() run
            //
            // Replacement hash logic without object index:
            assert(geo);
            DD::Image::Hash obj_id_hash;
            {
                // The GeoOp's Node* will always be the same for all GeoOp instances but
                // different for separate legs of the GeoOp tree. This allows us to make a
                // similar object *created* in one GeoOp as uniquely different from another:
                void* node_address = geo->node();
                obj_id_hash.append(&node_address, sizeof(void*));

                // 'object_id' is guaranteed to be unique for this GeometryList and
                // across different frame/view GeoOp instances:
                obj_id_hash.append(object_id.c_str());
            }

            // Update the GeoInfo::Cache in the GeoOp's cache list:
            std::vector<DD::Image::GeoInfo::Cache>& geo_cache_list = geo->getCacheList();
            assert(geoinfo_cache.obj < (int)geo_cache_list.size());
            DD::Image::GeoInfo::Cache& geo_cache = geo_cache_list[geoinfo_cache.obj];
            geo_cache.src_id = obj_id_hash;
            geo_cache.out_id = obj_id_hash;
            geo_cache.out_id.append(geo_cache.version); // bump by geometry_engine() run

            // And also update the GeoInfo's copy:
            DD::Image::GeoInfo& info = (*geometry_list)[geoinfo_cache.obj];
            DD::Image::GeoInfo::Cache* cache =
                const_cast<DD::Image::GeoInfo::Cache*>(info.get_cache_pointer());
            assert(cache);
            cache->src_id = geo_cache.src_id;
            cache->out_id = geo_cache.out_id;
            //---------------------------------------------------------------------------
        }

        geoinfo_cache.updateFromGeometryList(*geometry_list);

#if 0
        std::cout << "          GeoOpGeometryEngineContext::addObjectThreadSafe(" << std::hex << m_write_owner << std::dec << ")";
        std::cout << " obj=" << geoinfo_cache.obj;
        std::cout << ", id='" << object_id << "'";
        std::cout << ", points=" << geoinfo_cache.points_list;
        std::cout << ", prims=" << geoinfo_cache.primitives_list;
        //if (geoinfo_cache.attributes_list)
        //    std::cout << ", attribs=" << geoinfo_cache.attributes_list->data();
        //else
        //    std::cout << ", attribs=NULL";
        DD::Image::GeoInfo& info = (*geometry_list)[geoinfo_cache.obj];
        std::cout << ", src_id=0x" << std::hex << info.src_id().value() << std::dec;
        std::cout << ", out_id=0x" << std::hex << info.out_id().value() << std::dec;
        std::cout << std::endl; 
#endif

    }
    releaseWriteLock();
}


//-----------------------------------------------------------------------------


/*! Thread-safe add a primitive to the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
*/
void
GeoOpGeometryEngineContext::appendNewPrimitiveThreadSafe(GeoInfoCacheRef&      geoinfo_cache,
                                                         DD::Image::Primitive* prim,
                                                         size_t                num_verts)
{
    if (!prim || !geometry_list ||
        geoinfo_cache.obj < 0 || geoinfo_cache.obj >= (int)geometry_list->size())
        return; // don't crash...

    acquireWriteLock();
    {
        geometry_list->writable_points(geoinfo_cache.obj);
        geometry_list->add_primitive(geoinfo_cache.obj, prim);

        //==========================================================================================
        //==========================================================================================
        // HACK ALERT: Fix bug where GeometryList::GeoInfo::Cache::vertices_ is not kept up to date
        // with GeoOp::Cache::vertices_...:
        // This is required to create vertex attributes correctly...:
        //
        DD::Image::GeoInfo& info = (*geometry_list)[geoinfo_cache.obj];
        info.setVertexCount((uint32_t)num_verts);
        //==========================================================================================
        //==========================================================================================

        geoinfo_cache.updateFromGeometryList(*geometry_list);
    }
    releaseWriteLock();
}


/*! Thread-safe create a writable PointList in the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
    Returns a DD::Image::PointList pointer.
*/
DD::Image::PointList*
GeoOpGeometryEngineContext::createWritablePointsThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                           size_t           num_points)
{
    if (!geometry_list ||
        geoinfo_cache.obj < 0 || geoinfo_cache.obj >= (int)geometry_list->size())
        return NULL; // don't crash...

    DD::Image::PointList* points = NULL;

    acquireWriteLock();
    {
        points = geometry_list->writable_points(geoinfo_cache.obj);
        geoinfo_cache.updateFromGeometryList(*geometry_list);
    }
    releaseWriteLock();

    assert(points);
    points->resize(num_points);

    return points;
}


/*! Thread-safe create a writable Attribute in the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
    Returns a pointer to the newly created Attribute, or NULL if error creating.
*/
DD::Image::Attribute*
GeoOpGeometryEngineContext::createWritableAttributeThreadSafe(GeoInfoCacheRef&      geoinfo_cache,
                                                              DD::Image::GroupType  attrib_group,
                                                              const char*           attrib_name,
                                                              DD::Image::AttribType attrib_type)
{
    if (!geometry_list ||
        geoinfo_cache.obj < 0 || geoinfo_cache.obj >= (int)geometry_list->size())
        return NULL; // don't crash...

    DD::Image::Attribute* attrib = NULL;

    acquireWriteLock();
    {
        attrib = geometry_list->writable_attribute(geoinfo_cache.obj,
                                                   attrib_group,
                                                   attrib_name,
                                                   attrib_type);
        geoinfo_cache.updateFromGeometryList(*geometry_list);
    }
    releaseWriteLock();

    return attrib;
}


//-----------------------------------------------------------------------------


/*! Calc a bbox from current PointList, updating the one in the GeoInfo cache and our copy.
*/
void
GeoOpGeometryEngineContext::updateBBoxThreadSafe(GeoInfoCacheRef& geoinfo_cache)
{
    Fsr::Box3f bbox;
    if (geoinfo_cache.points_list && geoinfo_cache.points_list->size() > 0)
        bbox.set(reinterpret_cast<Fsr::Vec3f*>(geoinfo_cache.points_list->data()), geoinfo_cache.points_list->size());
    else
        bbox.setToEmptyState();

    setBBoxThreadSafe(geoinfo_cache, bbox);
}

/*! Set the bbox in the GeoInfo cache and our copy.
*/
void
GeoOpGeometryEngineContext::setBBoxThreadSafe(GeoInfoCacheRef&  geoinfo_cache,
                                              const Fsr::Box3f& bbox)
{
    if (!geometry_list ||
        geoinfo_cache.obj < 0 || geoinfo_cache.obj >= (int)geometry_list->size())
        return; // don't crash...

    acquireWriteLock();
    {
        DD::Image::GeoInfo::Cache* writable_cache =
            const_cast<DD::Image::GeoInfo::Cache*>((*geometry_list)[geoinfo_cache.obj].get_cache_pointer());
        writable_cache->bbox = bbox.asDDImage();
    }
    releaseWriteLock();

    geoinfo_cache.bbox = bbox;
}


//-----------------------------------------------------------------------------


/*! Set an attribute at the Group_Object level of a GeoInfo.
*/
void
GeoOpGeometryEngineContext::setObjectStringThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                                      const char*        attrib_name,
                                                      const std::string& attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Object,
                                          getConstStr(attrib_name),
                                          DD::Image::STD_STRING_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->stdstring(0) = attrib_value;
}

void
GeoOpGeometryEngineContext::setObjectIntThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                   const char*      attrib_name,
                                                   int32_t          attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Object,
                                          getConstStr(attrib_name),
                                          DD::Image::INT_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->integer(0) = attrib_value;
}

void
GeoOpGeometryEngineContext::setObjectFloatThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                     const char*      attrib_name,
                                                     float            attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Object,
                                          getConstStr(attrib_name),
                                          DD::Image::FLOAT_ATTRIB);
    assert(attrib); // shouldn't happen...
    attrib->resize(1); // just in case...
    attrib->flt(0) = attrib_value;
}


//-----------------------------------------------------------------------------



/*! Set an attribute at the Group_Primitive level of a GeoInfo.
*/
void
GeoOpGeometryEngineContext::setPrimitiveStringThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                                         uint32_t           prim_index,
                                                         const char*        attrib_name,
                                                         const std::string& attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Primitives,
                                          getConstStr(attrib_name),
                                          DD::Image::STD_STRING_ATTRIB);
    assert(attrib); // shouldn't happen...
    if (prim_index >= (uint32_t)attrib->size())
        attrib->resize(prim_index+1); // just in case...
    attrib->stdstring(prim_index) = attrib_value;
}

void
GeoOpGeometryEngineContext::setPrimitiveIntThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                      uint32_t         prim_index,
                                                      const char*      attrib_name,
                                                      int32_t          attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Primitives,
                                          getConstStr(attrib_name),
                                          DD::Image::INT_ATTRIB);
    assert(attrib); // shouldn't happen...
    if (prim_index >= (uint32_t)attrib->size())
        attrib->resize(prim_index+1); // just in case...
    attrib->integer(prim_index) = attrib_value;
}

void
GeoOpGeometryEngineContext::setPrimitiveFloatThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                        uint32_t         prim_index,
                                                        const char*      attrib_name,
                                                        float            attrib_value)
{
    if (!attrib_name || !attrib_name[0])
        return;
    DD::Image::Attribute* attrib =
        createWritableAttributeThreadSafe(geoinfo_cache,
                                          DD::Image::Group_Primitives,
                                          getConstStr(attrib_name),
                                          DD::Image::FLOAT_ATTRIB);
    assert(attrib); // shouldn't happen...
    if (prim_index >= (uint32_t)attrib->size())
        attrib->resize(prim_index+1); // just in case...
    attrib->flt(prim_index) = attrib_value;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace Fsr


// end of Fuser/NukeGeoInterface.cpp

//
// Copyright 2019 DreamWorks Animation
//
