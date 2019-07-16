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

/// @file Fuser/NukeGeoInterface.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NukeGeoInterface_h
#define Fuser_NukeGeoInterface_h

#include "ArgSet.h"
#include "Box3.h"

#include <DDImage/GeoOp.h>
#include <DDImage/Scene.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/GeometryList.h>

#include <mutex> // for std::mutex
#include <condition_variable> // for std::condition_variable


//-------------------------------------------------------------------------
/*! Need to redeclare this since it's private in DD::Image::GeoInfo.h
    PrimitiveList and yet it's used in the public interface of that
    class... wth Foundry?
*/
namespace DD { namespace Image {
    typedef std::vector<Primitive*, STL3DAllocator<Primitive*> > PrimList;
}}
//-------------------------------------------------------------------------


namespace Fsr {


//-------------------------------------------------------------------------


/*! These utility functions are not thread safe! Only use them if you're
    sure you're not within a GeoOp::geometry_engine() context that's
    managed by a GeoOpGeometryEngineContext.

    For example you can use these during rendering in Primitive methods
    like tessellat() since the GeometryList object pointers are stable
    by that point.
*/

//! Get an Attribute value from the Group_Object level of a GeoInfo. *** Not thread safe! ***
FSR_EXPORT std::string getObjectString(const DD::Image::GeoInfo& info,
                                       const char*               name);

FSR_EXPORT int         getObjectInt(const DD::Image::GeoInfo& info,
                                    const char*               name);

FSR_EXPORT float       getObjectFloat(const DD::Image::GeoInfo& info,
                                      const char*               name);

//! Does the object have a named attribute. *** Not thread safe! ***
FSR_EXPORT bool        hasObjectAttrib(const DD::Image::GeoInfo& info,
                                       const char*               name);

//! Get the data array of a DD::Image::Attribute, or NULL if not found. *** Not thread safe! ***
FSR_EXPORT void*       getAttribData(const DD::Image::GeoInfo& info,
                                     DD::Image::GroupType      group,
                                     const char*               name,
                                     DD::Image::AttribType     type);


//-------------------------------------------------------------------------


class FSR_EXPORT FuserPrimitive;
class FSR_EXPORT NodePrimitive;


/*! Wrapper for the DD::Image::GeoInfo object.
*/
class FSR_EXPORT GeoInfo
{
  public:
    //!
    GeoInfo(DD::Image::GeoInfo* info);
    //!
    GeoInfo(int               obj_index,
            DD::Image::GeoOp* geo);
    //!
    GeoInfo(int                            obj_index,
            const DD::Image::GeometryList& geometry_list);


  public:
#if 0
    //!
    std::string getObjectString(const char* attrib) const;
    int         getObjectInt(const char* attrib) const;
    float       getObjectFloat(const char* attrib) const;

    //!
    bool  hasObjectAttrib(const char* attrib) const;
    //!
    void* getAttribData(const char*           attrib,
                        DD::Image::GroupType  group,
                        DD::Image::AttribType type) const;
#endif


  private:
    DD::Image::GeoInfo* m_info;

};


//-------------------------------------------------------------------------


/*! Wrapper for the DD::Image::GeoInfo::Cache object to provide a stable
    reference to the underlying geometry data allocations despite the memory
    location of the GeoInfo::Cache object possibly moving around as the
    GeometryList object inserts GeoInfos in a multi-threaded context.

    Note the local m_attributes_list points to the same Attribute allocations
    as the ones in the GeoInfo::Cache, so the entire list can be copied locally
    in updateFromGeometryList().

    TODO: should we hide the pointers away and just expose via methods...?

*/
class FSR_EXPORT GeoInfoCacheRef
{
  public:
    int                             obj;                //!< Object index inside GeometryList
    Fsr::Box3f                      bbox;               //!< Copy of cache bbox
    DD::Image::PointList*           points_list;        //!< Points list
    DD::Image::PrimList*            primitives_list;    //!< Primitives list
    DD::Image::AttribContextList*   attributes_list;    //!< Normally points to the *local* copy of AttribContextList


  public:
    //! Default ctor leaves everything invalid/NULL.
    GeoInfoCacheRef();

    //! Copy ctor copies private AttribContextList, updating pointer.
    GeoInfoCacheRef(const GeoInfoCacheRef& b);

    //! Constructs the contents from the obj_index's GeoInfo::Cache in a GeoOp's cache list.
    GeoInfoCacheRef(int               obj_index,
                    DD::Image::GeoOp* geo);

    //! Constructs the contents from the obj_index's GeoInfo::Cache in the GeometryList.
    GeoInfoCacheRef(int                            obj_index,
                    const DD::Image::GeometryList& geometry_list);

    //! Copy operator copies private AttribContextList, updating pointer.
    GeoInfoCacheRef& operator = (const GeoInfoCacheRef& b);


  public:
    //! If any pointers are NULL the cache ref is invalid.
    bool isValid() const { return !(obj < 0 || !points_list || !primitives_list || !attributes_list); }

    //! Convenience function returns the Primitive at index 'i'.
    DD::Image::Primitive* getPrimitive(size_t i);

    //! Convenience function casts Primitive to a Fuser Primitive, returning NULL if it's not one.
    Fsr::FuserPrimitive*  getFuserPrimitive(size_t i=0);

    //! Convenience function casts Primitive to a Fuser NodePrimitive, returning NULL if it's not one.
    Fsr::NodePrimitive*   getFuserNodePrimitive(size_t i=0);


  protected:
    friend class GeoOpGeometryEngineContext;

    //! Update the geometry data pointers with the GeoInfo::Cache for this object index. !!Acquire write_lock before calling!!
    void updateFromGeometryList(const DD::Image::GeometryList& geometry_list);

    //! Update the geometry data pointers from a GeoInfo::Cache. !!Acquire write_lock before calling!!
    void updateFromGeoInfoCache(const DD::Image::GeoInfo::Cache* geoinfo_cache);

    //! Local copy of DD::Image::Attribute references (underlying Attribute pointers are still valid!)
    DD::Image::AttribContextList m_attributes_list;

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Node execution context structure passed as target data to Fsr::Node::execute()
    methods.

    This can be shared between threads filling the same DD::Image::GeometryList
    so writing thread info and locks are provided.

    WARNING: this does not allow multiple threads to write to the SAME GeoInfo
    data simultaneously. Each thread must be operating on a SEPARATE GeoInfo
    in the DD::Image::GeometryList.
    
*/
class FSR_EXPORT GeoOpGeometryEngineContext
{
  public:
    typedef std::unordered_map<std::string, int> ObjectIndexMap;

    /*! These contexts are stored in a static map keyed to the pointer of the GeoOp
        where they were first assigned/created.
    */
    struct GeoOpContext
    {
        ObjectIndexMap object_id_map;   //!< Order-independent mapping to GeomtryList object index
    };


  public:
    static const char* name; // "GeoOpGeometryEngine"

    DD::Image::GeoOp*           geo;                //!< The owner GeoOp
    DD::Image::GeometryList*    geometry_list;      //!< The list of GeoInfo object containers and their data pointers
    DD::Image::Scene*           scene;              //!< This is normally never used...
    //
    GeoOpContext*               geoop_context;      //!< Pointer to a GeoOpContext, stored in static GeoOpContext map


  protected:
    bool                        m_multithreaded;    //!< If true write_lock must be acquired prior to geo cache access
    DD::Image::ThreadId         m_write_owner;      //!< The thread that owns the write_lock
    std::mutex                  m_write_lock;       //!< Shared lock
    std::condition_variable     m_write_lock_cv;    //!< Does this need to be a class member?


    //! Acquire the write lock, this will spin until write_owner becomes 0.
    void acquireWriteLock();
    //! Release the write lock - sets write_owner to 0.
    void releaseWriteLock();


  public:
    //! Ctor is not thread safe! It assumes no worker threads are active!
    GeoOpGeometryEngineContext(int                      num_threads,
                               DD::Image::GeoOp*        _geo,
                               DD::Image::GeometryList* _geometry_list,
                               DD::Image::Scene*        _scene=NULL);


    //! Return the GeoOpContext keyed to the parent GeoOp pointer.
    GeoOpContext* getGeoOpContext() const { return geoop_context; }


    //! Reserve the next available object index, keyed to object_id string. !!Acquire write_lock before calling!!
    int  addObjectIndexFromId(const std::string& object_id);

    //! Returns -1 if there's object matching that name in the map. !!Acquire write_lock before calling!!
    int  getObjectIndexFromId(const std::string& object_id);

    //! Empty the object id map for this GeoOpContext. !!Acquire write_lock before calling!!
    void clearObjectIds();


    //----------------------------------------------------------------------


    //! Is the context in multithreaded mode? i.e. we need to acquire write lock before changing GeometryList.
    bool multithreaded() const { return m_multithreaded; }

    //!
    DD::Image::ThreadId writeOwner() const { return m_write_owner; }

    //! Access to the shared mutex.
    void lock()   { m_write_lock.lock(); }
    void unlock() { m_write_lock.unlock(); }


    //----------------------------------------------------------------------


    //! 
    Fsr::NodePrimitive* createFuserNodePrimitiveThreadSafe(const char*        node_class,
                                                           const Fsr::ArgSet& args);


    //----------------------------------------------------------------------


    //!  Thread-safe retrieve an object from the GeometryList, filling in the GeoInfoCacheRef. Returns -1 if not found.
    int  getObjectThreadSafe(const std::string& object_id,
                             GeoInfoCacheRef&   geoinfo_cache);


    //!  Thread-safe add an object to the GeometryList, filling in the GeoInfoCacheRef.
    void addObjectThreadSafe(const std::string& object_id,
                             GeoInfoCacheRef&   geoinfo_cache);


    //----------------------------------------------------------------------


    //! Thread-safe add a primitive to the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
    void appendNewPrimitiveThreadSafe(GeoInfoCacheRef&      geoinfo_cache,
                                      DD::Image::Primitive* prim,
                                      size_t                num_verts=0);

    //! Thread-safe create a writable PointList in the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
    DD::Image::PointList* createWritablePointsThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                                         size_t           num_points=0);

    //! Thread-safe create a writable Attribute in the GeoInfo referenced by the GeoInfoCacheRef, and updating it.
    DD::Image::Attribute* createWritableAttributeThreadSafe(GeoInfoCacheRef&      geoinfo_cache,
                                                            DD::Image::GroupType  attrib_group,
                                                            const char*           attrib_name,
                                                            DD::Image::AttribType attrib_type);


    //----------------------------------------------------------------------


    //! Set an Attribute value at the Group_Object level of a GeoInfo.
    void setObjectStringThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                   const char*        attrib_name,
                                   const std::string& attrib_value);
    void    setObjectIntThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                   const char*      attrib_name,
                                   int              attrib_value);
    void  setObjectFloatThreadSafe(GeoInfoCacheRef& geoinfo_cache,
                                   const char*      attrib_name,
                                   float            attrib_value);

    //---------------------------------------------

    //! Set an attribute value at the Group_Primitive level of a GeoInfo.
    void setPrimitiveStringThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                      uint32_t           prim_index,
                                      const char*        attrib_name,
                                      const std::string& attrib_value);
    void    setPrimitiveIntThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                      uint32_t           prim_index,
                                      const char*        attrib_name,
                                      int                attrib_value);
    void  setPrimitiveFloatThreadSafe(GeoInfoCacheRef&   geoinfo_cache,
                                      uint32_t           prim_index,
                                      const char*        attrib_name,
                                      float              attrib_value);


};





/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

//!
inline
GeoInfo::GeoInfo(DD::Image::GeoInfo* info) :
    m_info(info)
{
    assert(m_info);
}
//!
inline
GeoInfo::GeoInfo(int               obj_index,
                 DD::Image::GeoOp* geo) :
    m_info(NULL)
{
    if (geo && geo->scene() && geo->scene()->object_list())
    {
        DD::Image::GeometryList& geometry_list = *geo->scene()->object_list();
        assert(obj_index >= 0 && obj_index < (int)geometry_list.size());
        m_info = &geometry_list[obj_index];
    }
    assert(m_info);
}
//!
inline
GeoInfo::GeoInfo(int                            obj_index,
                 const DD::Image::GeometryList& geometry_list) :
    m_info(NULL)
{
    assert(obj_index >= 0 && obj_index < (int)geometry_list.size());
    m_info = const_cast<DD::Image::GeoInfo*>(&geometry_list[obj_index]);
}


} // namespace Fsr

#endif

// end of Fuser/NukeGeoInterface.h

//
// Copyright 2019 DreamWorks Animation
//
