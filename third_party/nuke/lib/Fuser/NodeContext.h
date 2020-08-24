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

/// @file Fuser/NodeContext.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NodeContext_h
#define Fuser_NodeContext_h

#include "ArgSet.h"
#include "Time.h"

#include <DDImage/Hash.h>
#include <DDImage/OutputContext.h>


namespace Fsr {


/*!
    TODO: this is currently a catchall and is only being used as a 
          abstract container for the ArgSet.
    Either delete it or make it more useful.
    It's also forcing an ArgSet copy since we can't pass ArgSet to
    Node ctors directly...

*/
class FSR_EXPORT NodeContext
{
  public:
    //! OpenGL drawlist modes (solid and textured share the same list.)
    enum
    {
        DRAW_GL_BBOX,
        DRAW_GL_WIREFRAME,
        DRAW_GL_SOLID,
        DRAW_GL_TEXTURED,
        //
        DRAW_GL_LASTMODE
    };

    enum
    {
        DEBUG_OFF,
        DEBUG_1,
        DEBUG_2,
        DEBUG_3
    };
    static const char* debug_modes[];


  public://protected:
    // Leave these public to allow messing with
    ArgSet   m_args;

    // TODO: do we want to have these vars vs. just the arg set?
    //double      m_frame;
    //double      m_fps;


  protected:
    //void*       m_user_data;


  public:
    //!
    NodeContext()// :
        //m_frame(0.0),
        //m_fps(24.0)
    {
        //
    }

    //! Copy ctors.
    NodeContext(const NodeContext& b) : m_args(b.m_args) {}
    NodeContext(const ArgSet& args) : m_args(args) {}

    //! Replaces the contents with another NodeContext.
    NodeContext& operator= (const NodeContext& b) { m_args = b.m_args; return *this; }

    //! Must have a virtual destructor!
    virtual ~NodeContext() {}

    //! Unique identifier string for context - must implement.
    //virtual const char* identifier() const=0;

    //! Purpose of context. Optional hint.
    //virtual const char* purpose() const { return ""; }


    bool hasArg(const std::string& key) const { return m_args.has(key); }
    bool hasArg(const char*        key) const { return m_args.has(key); }

    //! Allow the ArgSet to be messed with.
    const ArgSet& args() const { return m_args; }
    ArgSet&       args()       { return m_args; }

    //! Set/get the blind user data of the NodeContext.
    //void  setUserData(void* p) { m_user_data = p; }
    //void* userData() const { return m_user_data; }


    //!
    double frame() const { return m_args.getDouble("frame"); }
    void   setFrame(double frame) { m_args.setDouble("frame", frame); }
    //!
    double fps() const { return m_args.getDouble("fps"); }
    void   setFps(double fps) { m_args.setDouble("fps", fps); }
    //!
    //double time() const { return m_frame / m_fps; }
    void   setTime(double frame,
                   double fps) { setFrame(frame); setFps(fps); }
    //
    bool   isAnimated() const { return Fsr::isAnimated(frame()); }
    bool   isNotAnimated() const { return Fsr::isNotAnimated(frame()); }


  public:
    //-------------------------------------------------------------------------
    // Typed read access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    const std::string& getString(const std::string& key, const std::string& dflt_val=empty_string) const;
    const std::string& getString(const char*        key, const std::string& dflt_val=empty_string) const;
    //
    int                   getInt(const std::string& key, int       dflt_val=0)     const;
    int                   getInt(const char*        key, int       dflt_val=0)     const;
    double             getDouble(const std::string& key, double    dflt_val=0.0)   const;
    double             getDouble(const char*        key, double    dflt_val=0.0)   const;
    bool                 getBool(const std::string& key, bool      dflt_val=false) const;
    bool                 getBool(const char*        key, bool      dflt_val=false) const;
    HashValue            getHash(const std::string& key, HashValue dflt_val=~0ULL) const;
    HashValue            getHash(const char*        key, HashValue dflt_val=~0ULL) const;
    //
    Fsr::Vec2d          getVec2d(const std::string& key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const;
    Fsr::Vec2d          getVec2d(const char*        key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const;
    //
    Fsr::Vec3d          getVec3d(const std::string& key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const;
    Fsr::Vec3d          getVec3d(const char*        key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const;
    //
    Fsr::Vec4d          getVec4d(const std::string& key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const;
    Fsr::Vec4d          getVec4d(const char*        key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const;
    //
    Fsr::Mat4d          getMat4d(const std::string& key, Fsr::Mat4d dflt_val=Fsr::Mat4d(1.0)) const;
    Fsr::Mat4d          getMat4d(const char*        key, Fsr::Mat4d dflt_val=Fsr::Mat4d(1.0)) const;


  public:
    //-------------------------------------------------------------------------
    // Typed read access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    void   setString(const std::string& key, const std::string& value);
    void   setString(const std::string& key, const char*        value);
    void   setString(const char*        key, const std::string& value);
    void   setString(const char*        key, const char*        value);
    //
    void      setInt(const std::string& key, int       value);
    void   setDouble(const std::string& key, double    value);
    void     setBool(const std::string& key, bool      value);
    void     setHash(const std::string& key, HashValue value);
    //
    void    setVec2d(const std::string& key, const Fsr::Vec2d& value);
    void    setVec3d(const std::string& key, const Fsr::Vec3d& value);
    void    setVec4d(const std::string& key, const Fsr::Vec4d& value);
    //
    void    setMat4d(const std::string& key, const Fsr::Mat4d& value);


}; // NodeContext



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

inline const std::string& NodeContext::getString(const std::string& key, const std::string& dflt_val) const { return m_args.getString(key, dflt_val); }
inline const std::string& NodeContext::getString(const char*        key, const std::string& dflt_val) const { return m_args.getString(key, dflt_val); }

inline int          NodeContext::getInt(const std::string& key, int        dflt_val) const { return    m_args.getInt(key, dflt_val); }
inline int          NodeContext::getInt(const char*        key, int        dflt_val) const { return    m_args.getInt(key, dflt_val); }
inline double    NodeContext::getDouble(const std::string& key, double     dflt_val) const { return m_args.getDouble(key, dflt_val); }
inline double    NodeContext::getDouble(const char*        key, double     dflt_val) const { return m_args.getDouble(key, dflt_val); }
inline bool        NodeContext::getBool(const std::string& key, bool       dflt_val) const { return   m_args.getBool(key, dflt_val); }
inline bool        NodeContext::getBool(const char*        key, bool       dflt_val) const { return   m_args.getBool(key, dflt_val); }
inline HashValue   NodeContext::getHash(const std::string& key, HashValue  dflt_val) const { return   m_args.getHash(key, dflt_val); }
inline HashValue   NodeContext::getHash(const char*        key, HashValue  dflt_val) const { return   m_args.getHash(key, dflt_val); }

inline Fsr::Vec2d NodeContext::getVec2d(const std::string& key, Fsr::Vec2d dflt_val) const { return  m_args.getVec2d(key, dflt_val); }
inline Fsr::Vec2d NodeContext::getVec2d(const char*        key, Fsr::Vec2d dflt_val) const { return  m_args.getVec2d(key, dflt_val); }

inline Fsr::Vec3d NodeContext::getVec3d(const std::string& key, Fsr::Vec3d dflt_val) const { return  m_args.getVec3d(key, dflt_val); }
inline Fsr::Vec3d NodeContext::getVec3d(const char*        key, Fsr::Vec3d dflt_val) const { return  m_args.getVec3d(key, dflt_val); }

inline Fsr::Vec4d NodeContext::getVec4d(const std::string& key, Fsr::Vec4d dflt_val) const { return  m_args.getVec4d(key, dflt_val); }
inline Fsr::Vec4d NodeContext::getVec4d(const char*        key, Fsr::Vec4d dflt_val) const { return  m_args.getVec4d(key, dflt_val); }

inline Fsr::Mat4d NodeContext::getMat4d(const std::string& key, Fsr::Mat4d dflt_val) const { return  m_args.getMat4d(key, dflt_val); }
inline Fsr::Mat4d NodeContext::getMat4d(const char*        key, Fsr::Mat4d dflt_val) const { return  m_args.getMat4d(key, dflt_val); }

//-------------------------------------------------------------------------

inline void NodeContext::setString(const std::string& key, const std::string& value) { m_args.setString(key, value); }
inline void NodeContext::setString(const std::string& key, const char*        value) { m_args.setString(key, value); }
inline void NodeContext::setString(const char*        key, const std::string& value) { m_args.setString(key, value); }
inline void NodeContext::setString(const char*        key, const char*        value) { m_args.setString(key, value); }

inline void    NodeContext::setInt(const std::string& key, int                value) {    m_args.setInt(key, value); }
inline void NodeContext::setDouble(const std::string& key, double             value) { m_args.setDouble(key, value); }
inline void   NodeContext::setBool(const std::string& key, bool               value) {   m_args.setBool(key, value); }
inline void   NodeContext::setHash(const std::string& key, HashValue          value) {   m_args.setHash(key, value); }

inline void  NodeContext::setVec2d(const std::string& key, const Fsr::Vec2d&  value) {  m_args.setVec2d(key, value); }
inline void  NodeContext::setVec3d(const std::string& key, const Fsr::Vec3d&  value) {  m_args.setVec3d(key, value); }
inline void  NodeContext::setVec4d(const std::string& key, const Fsr::Vec4d&  value) {  m_args.setVec4d(key, value); }

inline void  NodeContext::setMat4d(const std::string& key, const Fsr::Mat4d&  value) {  m_args.setMat4d(key, value); }



} // namespace Fsr

#endif

// end of Fuser/NodeContext.h

//
// Copyright 2019 DreamWorks Animation
//
