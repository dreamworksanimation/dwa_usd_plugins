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

/// @file Fuser/Primitive.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Primitive_h
#define Fuser_Primitive_h

#include "XformableNode.h"
#include "Box3.h"

#include <DDImage/PrimitiveContext.h>
#include <DDImage/GeoInfo.h> // for utility convenience methods

// Extend the hardcoded Foundry primitive enumerations.
// These are in one place to make it easier to add new ones.
#define FUSER_NODE_PRIMITIVE_TYPE       (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+123) // Fsr::NodePrim
#define FUSER_SCENEGRAPH_PRIMITIVE_TYPE (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+124) // Fsr::SceneGraphPrimitive
#define FUSER_MESH_PRIMITIVE_TYPE       (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+125) // Fsr::MeshPrimitive
#define FUSER_POINTCLOUD_PRIMITIVE_TYPE (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+126) // Fsr::PointCloudPrim
#define FUSER_CURVESET_PRIMITIVE_TYPE   (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+127) // Fsr::CurveSetPrim
#define FUSER_INSTANCE_PRIMITIVE_TYPE   (DD::Image::PrimitiveType)(DD::Image::ePrimitiveTypeCount+128) // Fsr::InstancePrim


namespace Fsr {


/*! DD::Image::Primitive + Fsr::XformableNode wrapper adding frame and
    double-precision support.

*/
class FSR_EXPORT FuserPrimitive : public Fsr::XformableNode,
                                  public DD::Image::Primitive
{
  protected:
    double      m_frame;            //!< Absolute scene frame number


  protected:
    //! Required method to support DD::Image::Primitive::duplicate()
    void copy(const FuserPrimitive* b)
    {
        DD::Image::Primitive::copy(this);
        // In Fsr::Node:
        m_parent        = b->m_parent; // bad idea...
        m_children      = b->m_children;
        m_args          = b->m_args;
        m_is_valid      = b->m_is_valid;
        m_status        = b->m_status;
        // In Fsr::XformableNode:
        m_xform         = b->m_xform;
        m_have_xform    = b->m_have_xform;
        //
        m_frame         = b->m_frame;
    }


  public:
    /*! Node execution context structure passed as target data to Fsr::Node::execute()
        methods, containing info normally passed to DD::Image::Primitive::tessellate()
        method which outputs DD::Image::rPrimitives to a render DD::Image::Scene.

        For DD::Image::RenderScene/ScanlineRender use only, not for generic tessellation
        use!
    */
    struct FSR_EXPORT DDImageRenderSceneTessellateContext
    {
        static const char* name; // "DDImageRenderSceneTessellate"

        const FuserPrimitive*        primitive;     //!< Source Fsr::FuserPrimitive
        DD::Image::PrimitiveContext* ptx;           //!< Parent GeoInfo of Primitive
        DD::Image::Scene*            render_scene;  //!< Output rendering scene

        DDImageRenderSceneTessellateContext(const FuserPrimitive*        _primitive,
                                            DD::Image::PrimitiveContext* _ptx,
                                            DD::Image::Scene*            _render_scene) :
            primitive(_primitive),
            ptx(_ptx),
            render_scene(_render_scene)
        {
            //
        }

        //!
        bool isValid() const { return !(primitive        == NULL ||
                                        ptx              == NULL ||
                                        ptx->geoinfo()   == NULL ||
                                        ptx->primitive() == NULL ||
                                        render_scene     == NULL); }

    };


  public:
    //!
    FuserPrimitive(double frame=defaultFrameValue()) :
        Fsr::XformableNode(NULL/*parent*/),
        DD::Image::Primitive(),
        m_frame(frame)
    {
        //
    }

    //!
    FuserPrimitive(const ArgSet&      args,
                   double             frame=defaultFrameValue()) :
        Fsr::XformableNode(args, NULL/*parent*/),
        DD::Image::Primitive(),
        m_frame(frame)
    {
        //
    }


    /*! Must have a virtual destructor!
        Dtor necessary to avoid GCC 'undefined reference to `vtable...' link error.
    */
    virtual ~FuserPrimitive();


    //--------------------------------------------------------------------------------- 


    //! Absolute scene frame number of prim
    double frame() const { return m_frame; }

    //! Set the frame number. Virtual so subclasses can update their internals.
    virtual void setFrame(double frame) { m_frame = frame; }

    //! Get the matrix from the parent GeoInfo (ie the parent xform of this primitive) as a Fuser Mat4d.
    static Fsr::Mat4d getParentXform(const DD::Image::GeoInfo& info) { return Fsr::Mat4d(info.matrix); }


    //! Return the local-space transform matrix of the node.
    /*virtual*/ Fsr::Mat4d getLocalTransform() { return m_xform; }
    //! Return the world-space transform matrix of the node.
    /*virtual*/ Fsr::Mat4d getWorldTransform() { return m_xform; }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------


    /*! Base class returns the read-only vector of point locations from a GeoInfo PointList
        cast to a Fuser Vec3f vector.
    */
    virtual const std::vector<Fsr::Vec3f>& getPointLocations(const DD::Image::PointList* geoinfo_points) const;

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


/*virtual*/ inline const std::vector<Fsr::Vec3f>&
FuserPrimitive::getPointLocations(const DD::Image::PointList* geoinfo_points) const
{
#if DEBUG
    assert(geoinfo_points);
#endif
    static std::vector<Fsr::Vec3f> empty_points = std::vector<Fsr::Vec3f>();
    if (!geoinfo_points)
        return empty_points;

    // First cast the PointList to its explicit std::vector inheritance.
    // Note that this must exactly match the PointList definition in GeoInfo.h!
    const std::vector<DD::Image::Vector3, DD::Image::STL3DAllocator<DD::Image::Vector3> >* ddvec3 =
        static_cast<const std::vector<DD::Image::Vector3, DD::Image::STL3DAllocator<DD::Image::Vector3> >*>(geoinfo_points);
    // Then cast the vector to a Fsr::Vec3f:
    return reinterpret_cast<const std::vector<Fsr::Vec3f>&>(*ddvec3);
}


} // namespace Fsr

#endif

// end of Fuser/Primitive.h

//
// Copyright 2019 DreamWorks Animation
//
