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

/// @file zprender/RenderPrimitive.cpp
///
/// @author Jonathan Egstad


#include "RenderPrimitive.h"
#include "RenderContext.h"
#include "ThreadContext.h"


namespace zpr {


/*!
*/
RenderPrimitive::RenderPrimitive(const MaterialContext* material_ctx,
                                 double                 motion_time) :
    m_material_ctx(const_cast<MaterialContext*>(material_ctx))
    //surface_ctx(stx),
    //index(0)
{
#if DEBUG
    assert(m_material_ctx);
#endif
    m_motion_times.resize(1, motion_time);
}


/*!
*/
RenderPrimitive::RenderPrimitive(const MaterialContext* material_ctx,
                                 const Fsr::DoubleList& motion_times) :
    m_material_ctx(const_cast<MaterialContext*>(material_ctx)),
    m_motion_times(motion_times)
    //surface_ctx(stx),
    //index(0)
{
#if DEBUG
    assert(m_material_ctx);
    assert(m_motion_times.size() > 0);
#endif
}


/*!
*/
const SurfaceContext*
RenderPrimitive::getSurfaceContext() const
{
#if DEBUG
    assert(m_material_ctx);
    assert(m_material_ctx->surface_ctx);
#endif
    return m_material_ctx->surface_ctx;
}


/*!
*/
const GeoInfoContext*
RenderPrimitive::getGeoInfoContext() const
{
#if DEBUG
    assert(m_material_ctx);
    assert(m_material_ctx->surface_ctx);
    assert(m_material_ctx->surface_ctx->parent_object_ctx);
#endif
    return m_material_ctx->surface_ctx->parent_object_ctx->asGeoObject();
}


/*!
*/
const LightVolumeContext*
RenderPrimitive::getLightVolumeContext() const
{
#if DEBUG
    assert(m_material_ctx);
    assert(m_material_ctx->surface_ctx);
    assert(m_material_ctx->surface_ctx->parent_object_ctx);
#endif
    return m_material_ctx->surface_ctx->parent_object_ctx->asLightVolume();
}


/*! Which subd level to displace to.
*/
/*virtual*/
int
RenderPrimitive::getDisplacementSubdivisionLevel() const
{
#if DEBUG
    assert(m_material_ctx);
#endif
    return m_material_ctx->displacement_subdivision_level;
}

/*! Return a maximum displacement vector for this prim.
*/
/*virtual*/
Fsr::Vec3f
RenderPrimitive::getDisplacementBounds() const
{
#if DEBUG
    assert(m_material_ctx);
#endif
    return m_material_ctx->displacement_bounds;
}


} // namespace zpr

// end of zprender/RenderPrimitive.cpp

//
// Copyright 2020 DreamWorks Animation
//
