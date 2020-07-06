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

#include <mutex> // for std::mutex


namespace zpr {


/*!
*/
Scene::Scene(int32_t _shutter_step,
             double  _frame) :
    DD::Image::Scene(),
    frame(_frame),
    shutter_step(_shutter_step)
{
    _time = float(frame);

    // Use the _id var as motion-sample indicator.  If it's negative then
    // the motion-step is back in time from frame0:
    _id = shutter_step;
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
    // This should copy all the vars, including the lights:
    DD::Image::Scene::copyInfo(b);

    // Copy zpr::Scene subclass vars:
    frame        = b->frame;
    shutter_step = b->shutter_step;
    m_object_map.clear(); // don't copy geometry info
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

/*virtual*/ void
Scene::validate(int /*sample*/)
{
    // do nothing, don't call base class!
}

/*virtual*/ void
Scene::request(const DD::Image::ChannelSet& channels,
               int                          count)
{
    // do nothing, don't call base class!
}

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
