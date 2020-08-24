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

/// @file Fuser/XformableNode.h
///
/// @author Jonathan Egstad

#ifndef Fuser_XformableNode_h
#define Fuser_XformableNode_h

#include "Node.h"


namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! A Fuser transformable Node contains a 4x4 transform matrix and
    is considered a drawable or renderable object.

    TODO: put local_bbox at this level?
*/
class FSR_EXPORT XformableNode : public Fsr::Node
{
  protected:
    Fsr::Mat4d      m_xform;            //!<
    bool            m_have_xform;       //!< Matrix is not identity
    Fsr::Box3d      m_local_bbox;       //!< Local-space bbox

    //int             m_gl_draw_lists[NodeContext::DRAW_GL_LASTMODE]; //!< OpenGL drawlists for different draw modes.
    //DD::Image::Hash m_gl_draw_hash;     //!< State of OpenGL drawlists.


  protected:
    //! Called before execution to allow node to update local data from args. Base class does nothing.
    /*virtual*/ void _validateState(const Fsr::NodeContext& exec_ctx,
                                    bool                    for_real);

    //! Destroy the Node's contents - Xformable clears the xform and bbox.
    /*virtual*/ void _destroyContents();


#if 0
    //! OpenGL drawing calls. Called by draw*().
    virtual void _drawWireframe();
    virtual void _drawSolid();
    virtual void _drawTextured();
    virtual void _drawBbox();
    virtual void _drawIcons();
#endif


  public:
    //!
    XformableNode(Node* parent=NULL);

    //!
    XformableNode(const ArgSet& args,
                  Node*         parent=NULL);

    /*! Must have a virtual destructor!
        Dtor necessary to avoid GCC 'undefined reference to `vtable...' link error.
    */
    virtual ~XformableNode();


    //! Returns true if Node has a non-identity transform.
    /*virtual*/ bool       haveTransform() const { return m_have_xform; }
    //! Return the local-space transform matrix of the node.
    /*virtual*/ Fsr::Mat4d getLocalTransform() { return m_xform; }
    //! Return the world-space transform matrix of the node.
    /*virtual*/ Fsr::Mat4d getWorldTransform() { return m_xform; }


    //! Get/set the local-space transform matrix.
    const Fsr::Mat4d& getTransform() const { return m_xform; }
    void              setTransform(const Fsr::Mat4d& xform) { m_xform = xform; m_have_xform = !xform.isIdentity(); }


    //! Returns true if local bbox is empty.
    /*virtual*/ bool       isLocalBboxEmpty() { return m_local_bbox.isEmpty(); }
    //! Nodes can implement this to return a custom local-space bbox.
    /*virtual*/ Fsr::Box3d getLocalBbox() { return m_local_bbox; }
    //! Nodes can implement this to return a custom world-space bbox.
    /*virtual*/ Fsr::Box3d getWorldBbox() { return m_xform.transform(m_local_bbox); }


    //--------------------------------------------------------------------------------- 


#if 0
    //! Base class does nothing.
    virtual void appendGlHash(DD::Image::Hash&);
    //! Base class sets up drawing of bbox.
    virtual void buildGL();
    //! Base class draws bbox.
    virtual void drawGL(int draw_mode);

    //! Top level OpenGL drawing calls. Calls draw on all children first, then the node draws.
    void drawWireframe();
    void drawSolid();
    void drawTextured();
    void drawBbox();
    void drawIcons();
#endif


}; // XformableNode


} // namespace Fsr

#endif

// end of Fuser/XformableNode.h

//
// Copyright 2019 DreamWorks Animation
//
