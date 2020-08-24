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

/// @file Fuser/XformableNode.cpp
///
/// @author Jonathan Egstad

#include "XformableNode.h"


namespace Fsr {


//-----------------------------------------------------------------------------

void testMat4()
{
    Fsr::Mat4f m44f;
    std::cout << "  m44f" << m44f << " isIdentity=" << m44f.isIdentity() << std::endl;
    m44f.setToIdentity();
    std::cout << " im44f" << m44f << " isIdentity=" << m44f.isIdentity() << std::endl;
    Fsr::Mat4d m44d;
    std::cout << "  m44d" << m44d << " isIdentity=" << m44d.isIdentity() << std::endl;
    m44d.setToIdentity();
    std::cout << " im44d" << m44d << " isIdentity=" << m44d.isIdentity() << std::endl;
}

//-----------------------------------------------------------------------------

/*!
*/
XformableNode::XformableNode(Node* parent) :
    Fsr::Node(parent),
    m_xform(Fsr::Mat4d::getIdentity()),
    m_have_xform(false)
{
    //std::cout << "  XformableNode::ctor(" << this << ")" << std::endl;
    //memset(m_gl_draw_lists, 0, 3*sizeof(int));
}

/*!
*/
XformableNode::XformableNode(const ArgSet& args,
                             Node*         parent) :
    Fsr::Node(args, parent),
    m_xform(Fsr::Mat4d::getIdentity()),
    m_have_xform(false)
{
    //std::cout << "  XformableNode::ctor(" << this << ")" << std::endl;
    //memset(m_gl_draw_lists, 0, 3*sizeof(int));
}


/*! This empty dtor is necessary to avoid GCC 'undefined reference to `vtable...' link error.
    Must be in implemenation file, not header.
*/
XformableNode::~XformableNode()
{
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
XformableNode::_validateState(const Fsr::NodeContext& exec_ctx,
                              bool                    for_real)
{
    // Get the time value up to date:
    Fsr::Node::_validateState(exec_ctx, for_real);

    if (0)//(debug())
    {
        std::cout << "============================================================================================" << std::endl;
        std::cout << "Fsr::XformableNode::_validateState(" << this << "): for_real=" << for_real;
        std::cout << ", m_local_bbox=" << m_local_bbox;
        std::cout << ", m_have_xform=" << m_have_xform;
        if (m_have_xform)
            std::cout << ", xform" << m_xform;
        if (debugAttribs())
            std::cout << ", args[" << m_args << "]";
        std::cout << std::endl;
    }
}


/*! Destroy the Node's contents. Base class calls destroyContents() on all children.
*/
/*virtual*/
void
XformableNode::_destroyContents()
{
    Node::_destroyContents();

    m_xform.setToIdentity();
    m_have_xform = false;
    m_local_bbox.setToEmptyState();

    //memset(m_gl_draw_lists, 0, 3*sizeof(int));
}


//---------------------------------------------------------------------------------------


#if 0
/*! Base class does nothing.
*/
/*virtual*/
void
XformableNode::appendGlHash(DD::Image::Hash& hash)
{
    hash.append((int)m_children.size());
}


/*! Base class sets up drawing of bbox.
*/
/*virtual*/
void
XformableNode::buildGL()
{
    validateState(ArgSet()/*args*/, false/*for_real*/);

    // Check the gl hash, if it's changed we need to rebuild the call lists:
    DD::Image::Hash new_hash;
    appendGlHash(new_hash);
    //std::cout << "  points hash=" << std::hex << new_hash.value() << std::dec << std::endl;
    if (new_hash != m_gl_draw_hash)
    {
        std::cout << "  build call lists" << std::endl;
        m_gl_draw_hash = new_hash;
        //
        for (unsigned i=0; i < NodeContext::DRAW_GL_LASTMODE; ++i)
        {
            if (m_gl_draw_lists[i] == 0)
                m_gl_draw_lists[i] = glGenLists(1);
        }
        //
        if (m_gl_draw_lists[NodeContext::DRAW_GL_WIREFRAME] > 0)
        {
            glNewList(m_gl_draw_lists[NodeContext::DRAW_GL_WIREFRAME], GL_COMPILE);
            {
                drawWireframe();
            }
            glEndList();
        }
        //
        if (m_gl_draw_lists[NodeContext::DRAW_GL_SOLID] > 0)
        {
            glNewList(m_gl_draw_lists[NodeContext::DRAW_GL_SOLID], GL_COMPILE);
            {
                drawSolid();
            }
            glEndList();
        }
    }
}

/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::drawGL(int draw_mode)
{
    assert(draw_mode >= 0);
    // Call the default draw lists:
    if (draw_mode == NodeContext::DRAW_GL_WIREFRAME)
    {
        // Wireframe:
        if (m_gl_draw_lists[NodeContext::DRAW_GL_WIREFRAME] > 0)
            glCallList(m_gl_draw_lists[NodeContext::DRAW_GL_WIREFRAME]);
    }
    else
    {
        // Solid / Textured:
        if (m_gl_draw_lists[NodeContext::DRAW_GL_SOLID] > 0)
            glCallList(m_gl_draw_lists[NodeContext::DRAW_GL_SOLID]);
    }
}


/*! First calls drawWireframe() on all chaildren, then the
    virtual _drawWireframe() on this node.
*/
void
XformableNode::drawWireframe()
{
    const unsigned nChildren = (unsigned)m_children.size();
    for (unsigned i=0; i < nChildren; ++i)
        m_children[i]->drawWireframe();

    _drawWireframe();
}

/*! First calls drawSolid() on all chaildren, then the
    virtual _drawSolid() on this node.
*/
void
XformableNode::drawSolid()
{
    const unsigned nChildren = (unsigned)m_children.size();
    for (unsigned i=0; i < nChildren; ++i)
        m_children[i]->drawSolid();

    _drawSolid();
}

/*! First calls drawTextured() on all chaildren, then the
    virtual _drawTextured() on this node.
*/
void
XformableNode::drawTextured()
{
    const unsigned nChildren = (unsigned)m_children.size();
    for (unsigned i=0; i < nChildren; ++i)
        m_children[i]->drawTextured();

    _drawTextured();
}

/*! Base class draws bbox.
*/
void
XformableNode::drawBbox()
{
    const unsigned nChildren = (unsigned)m_children.size();
    for (unsigned i=0; i < nChildren; ++i)
        m_children[i]->drawBbox();

    _drawBbox();
}


/*! Base class draws bbox.
*/
void
XformableNode::drawIcons()
{
    const unsigned nChildren = (unsigned)m_children.size();
    for (unsigned i=0; i < nChildren; ++i)
        m_children[i]->drawIcons();

    _drawIcons();
}


/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::_drawWireframe()
{
    //std::cout << "Fsr::XformableNode::drawWireframe(" << this << ")" << std::endl;
    const Fsr::Box3d bbox = getWorldBbox();
    const Fsr::Vec3d& a = bbox.min;
    const Fsr::Vec3d& b = bbox.max;

    glPushAttrib(GL_LINE_BIT);
    glLineWidth(1);
    {
        glBegin(GL_LINE_STRIP);
        {
            glVertex3d(a.x, a.y, b.z);
            glVertex3d(a.x, b.y, b.z);
            glVertex3d(b.x, b.y, b.z);
            glVertex3d(b.x, a.y, b.z);
            glVertex3d(a.x, a.y, b.z);
            glVertex3d(a.x, a.y, a.z);
            glVertex3d(a.x, b.y, a.z);
            glVertex3d(b.x, b.y, a.z);
            glVertex3d(b.x, a.y, a.z);
            glVertex3d(a.x, a.y, a.z);
        }
        glEnd();
        glBegin(GL_LINES);
        {
            glVertex3d(a.x, b.y, a.z);
            glVertex3d(a.x, b.y, b.z);
            glVertex3d(b.x, b.y, a.z);
            glVertex3d(b.x, b.y, b.z);
            glVertex3d(b.x, a.y, a.z);
            glVertex3d(b.x, a.y, b.z);
        }
        glEnd();
    }
    glPopAttrib(); // GL_LINE_BIT
}


/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::_drawSolid()
{
    //std::cout << "Fsr::XformableNode::drawSolid(" << this << ")" << std::endl;
    const Fsr::Box3d bbox = getWorldBbox();
    const Fsr::Vec3d& a = bbox.min;
    const Fsr::Vec3d& b = bbox.max;

    glBegin(GL_QUADS);
    {
        // Draw a flattened version if bbox is too thin in one or more directions:
        if ((b.x - a.x) < 0.0001f)
        {
            // Flat in x:
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, b.y, b.z);

        }
        else if ((b.y - a.y) < 0.0001f)
        {
            // Flat in y:
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(a.x, b.y, b.z);

        }
        else if ((b.z - a.z) < 0.0001f)
        {
            // Flat in z:
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(a.x, b.y, a.z);

        }
        else
        {
            // Back:
            glNormal3f( 0.0f, 0.0f, 1.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glVertex3d(a.x, b.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glVertex3d(b.x, b.y, b.z);
            // Right:
            glNormal3f( 1.0f, 0.0f, 0.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glVertex3d(b.x, b.y, a.z);
            // Front:
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glVertex3d(a.x, b.y, a.z);
            // Left:
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3d(a.x, b.y, b.z);
            // Top:
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glVertex3d(a.x, b.y, b.z);
            // Bottom:
            glNormal3f( 0.0f,-1.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
        }
    }
    glEnd();
}


/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::_drawTextured()
{
    //std::cout << "Fsr::XformableNode::drawTextured(" << this << ")" << std::endl;
    const Fsr::Box3d bbox = getWorldBbox();
    const Fsr::Vec3d& a = bbox.min;
    const Fsr::Vec3d& b = bbox.max;

    glBegin(GL_QUADS);
    {
        // Draw a flattened version if bbox is too thin in one or more directions:
        if ((b.x - a.x) < 0.0001f)
        {
            // Flat in x:
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, b.z);

        }
        else if ((b.y - a.y) < 0.0001f)
        {
            // Flat in y:
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, b.z);

        }
        else if ((b.z - a.z) < 0.0001f)
        {
            // Flat in z:
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, a.z);

        }
        else
        {
            // Back:
            glNormal3f( 0.0f, 0.0f, 1.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(a.x, b.y, b.z);
            glNormal3f( 0.0f, 0.0f, 1.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(b.x, b.y, b.z);
            // Right:
            glNormal3f( 1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(b.x, b.y, a.z);
            // Front:
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 0.0f,-1.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, a.z);
            // Left:
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(a.x, a.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, b.z);
            // Top:
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, b.y, a.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, b.y, b.z);
            glNormal3f( 0.0f, 1.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, b.y, b.z);
            // Bottom:
            glNormal3f( 0.0f,-1.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3d(a.x, a.y, b.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3d(b.x, a.y, b.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3d(b.x, a.y, a.z);
            glNormal3f( 0.0f,-1.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3d(a.x, a.y, a.z);
        }
    }
    glEnd();
}


/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::_drawBbox()
{
    //std::cout << "Fsr::XformableNode::drawBbox(" << this << ")" << std::endl;
}


/*! Base class draws bbox.
*/
/*virtual*/
void
XformableNode::_drawIcons()
{
    //std::cout << "Fsr::XformableNode::drawIcons(" << this << ")" << std::endl;

}
#endif

} // namespace Fsr


// end of Fuser/XformableNode.cpp

//
// Copyright 2019 DreamWorks Animation
//
