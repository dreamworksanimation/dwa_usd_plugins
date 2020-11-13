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

/// @file Fuser/Node.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Node_h
#define Fuser_Node_h

#include "ArgConstants.h"
#include "ArgSet.h"
#include "AttributeTypes.h"
#include "NodeContext.h"

#include <DDImage/Description.h>

#include <string>
#include <vector>
#include <map>
#include <assert.h>


namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


class Node;
class NodeContext;

typedef std::vector<Fsr::Node*> NodeList;
typedef std::vector<Fsr::Box3d> BBoxList;


/*!
*/
struct FSR_EXPORT NodeFilterPattern
{
    std::string     name_expr;      //!< Matches node name or path
    std::string     type_expr;      //!< Matches node type/class

    NodeFilterPattern() {}

    NodeFilterPattern(const std::string& _name_expr,
                      const std::string& _type_expr) : name_expr(_name_expr), type_expr(_type_expr) {}

    NodeFilterPattern(const NodeFilterPattern& b) : name_expr(b.name_expr), type_expr(b.type_expr) {}
    NodeFilterPattern& operator = (const NodeFilterPattern& b)
        { name_expr = b.name_expr; type_expr = b.type_expr; return *this; }

};
typedef std::vector<NodeFilterPattern> NodeFilterPatternList;


/*! Extend this as needed to add more description params.
*/
struct FSR_EXPORT NodeDescription
{
    std::string     path;       //!< Full scene path ex. '/Scene/Foo/Bar'
    std::string     type;       //!< Type/Class of node ex. 'Camera'
    std::string     note;       //!< Extra descriptive info ex. 'Invisible', 'Hidden', 'Inactive'

    NodeDescription() {}

    NodeDescription(const std::string& _path,
                    const std::string& _type,
                    const std::string& _note="") : path(_path), type(_type), note(_note) {}

    NodeDescription(const NodeDescription& b) : path(b.path), type(b.type), note(b.note) {}
    NodeDescription& operator = (const NodeDescription& b) { path = b.path; type = b.type; note = b.note; return *this; }

};
typedef std::vector<NodeDescription>           NodeDescriptionList;
// Keep this ordered so that the paths are automatically sorted:
typedef std::map<std::string, NodeDescription> NodeDescriptionMap;


/*! A structure holding selection sets of node paths separated by type.
    TODO: add other fundamental types here?
*/
struct FSR_EXPORT NodePathSelections
{
    std::set<std::string> objects;      //!< List of enabled object node paths
    std::set<std::string> materials;    //!< List of enabled material node paths
    std::set<std::string> lights;       //!< List of enabled light node paths

    //!
    void clear() { objects.clear(); materials.clear(); lights.clear(); }
    //!
    bool isEmpty() const { return (objects.empty() && materials.empty() && lights.empty()); }
};


//-------------------------------------------------------------------------

/*! \class Fsr::Node

    \brief The abstract base class of all Fuser Nodes, which are themselves abstract
    containers that can execute arbitrary functionality.

    Also provides plugin loading functionality that leverages the DD::Image plugin
    system, so Fuser plugins can (must..) appear in the NUKE_PATH. For Fuser-specific
    vs. DD::Image-specific plugins to be found their filenames must begin with 'fsr'
    followed by the Node class name as returned by Fsr::Node::fuserNodeClass().

    For example a compiled & linked plugin for the Fuser Node class 'MyFuserNode'
    must be named exactly 'fsrMyFuserNode', ie '/foo/bar/fsrMyFuserNode.so' on linux,
    for the Fuser plugin system to find it. This naming convention helps separate
    the DD::Image (Op, Reader & Writer) plugins from being confused with Fuser
    plugins.

    .tcl alias files can be used to redirect the names or to point multiple file
    names to the same plugin dso. This follows standard Nuke (DD::Image) rules for
    what's inside the .tcl file - for example 'fsrMyNodeAlias.tcl' must only contain
    the line 'load fsrMyFuserNode' which causes the plugin 'fsrMyFuserNode' to
    be searched for and loaded.
    This is most handy for handling multiple file extensions that map to the same
    file IO plugin. For example USD files can be named differently due to their
    encodings, so '.usd', '.usda', and '.usdc' all refer to USD files and should
    cause the same fsrUsdIO plugin to be loaded. .tcl alias files are used for this
    by creating 'fsrUsdaIO.tcl' and 'fsrUsdcIO.tcl' files that both point
    to the primary 'fsrUsdIO' plugin. ie:
        fsrUsdaIO.tcl:
          load fsrUsdIO

        fsrUsdcIO.tcl:
          load fsrUsdIO

    IMPORTANT: .tcl aliasing files should be matched to aliasing Descriptions
    defined inside the compiled plugin so the alias class names are added to the
    Description map and found quickly. Otherwise every time an aliased Node class
    is instantiated a plugin search is performed.
    In the above USD example the fsrUsdIO.so plugin would contain three Descriptions,
    one for the primary Node class and then two for the Node class aliases. ie:
        static const Fsr::Node::Description  registerUsdIONode( "UsdIO", Fsr::buildUsdNode);
        static const Fsr::Node::Description registerUsdaIONode("UsdaIO", Fsr::buildUsdNode);
        static const Fsr::Node::Description registerUsdcIONode("UsdcIO", Fsr::buildUsdNode);

    All three Descriptions call the same Node build routine 'buildUsdNode()' which
    is implemented inside the same fsrUsdIO.so plugin.

*/
class FSR_EXPORT Node
{
  public:
    //! Error state returned from execution methods.
    struct FSR_EXPORT ErrCtx
    {
        int         state;
        std::string msg;

        ErrCtx(int _state=0) : state(_state) {}
        ErrCtx(int _state, const char* _msg) : state(_state), msg((_msg)?_msg:"") {}
        ErrCtx(const ErrCtx& b) { *this = b; }
        ErrCtx& operator = (const ErrCtx& b) { state = b.state; msg = b.msg; return *this; }
        
    };


    //! Node expansion status. TODO: need this anymore...?
    enum
    {
        DISABLED              = -1,     //!< Node not enabled (do not expand or process)
        NOT_EXPANDED          =  0,     //!< Not yet expanded
        EXPANSION_IN_PROGRESS =  1,     //!< A thread is actively expanding it
        EXPANSION_COMPLETE    =  2      //!< Expansion is complete
    };


    /*! These are similar to Alembic's topology enums but the
        terminology is hopefully less obscure...
        TODO: move this to the abstract PointBased Node class.
    */
    enum
    {
        ConstantTopology         = 0x0,     //!< Nothing about the object is changing
        XformVaryingTopology     = 0x1,     //!< The transform of the object changes over time
        PointVaryingTopology     = 0x2,     //!< The point locations change over time
        PrimitiveVaryingTopology = 0x4      //!< The primitive structre changes over time
    };


  public:
    /*! This structure creates a subclass of Fsr::Node.
        The constructor builds these into a list that Fsr::Node primitives
        search to create an instance of a Fsr::Node class.
    */
    class FSR_EXPORT Description : public DD::Image::Description
    {
      private:
        const char* m_node_class;

        /*! Method type defined in DD::Image::Description.h: (*f)(Description*)
            Called when the plugin .so is first loaded.
        */
        static void pluginBuilderCallback(DD::Image::Description* desc);


      public:
        //! Constructor method definition used for 'build()' methods in plugins.
        typedef Node* (*PluginBuilder)(const char*   builder_class,
                                       const ArgSet& args,
                                       Fsr::Node*    parent);
        PluginBuilder builder_method; // <<< Call this to construct a Fsr::Node object.


      public:
        //! Constructor sets name and label to same value.
        Description(const char*   node_class,
                    PluginBuilder builder);

        //!
        const char* fuserNodeClass() const { return m_node_class; }

        //! Find a dso description by name.
        static const Description* find(const char* node_class);
    };


  protected:
    Node*           m_parent;           //!< Parent Node
    NodeList        m_children;         //!< List of child Nodes
    //
    ArgSet          m_args;             //!< Set of key/value arg pairs
    bool            m_is_valid;         //!< Has validate been called to update the args state?
    //
    int             m_status;           //!< State flags (not expanded, etc) TODO: need this anymore...change to arg...?


  protected:
    //! Called before execution to allow node to update local data from args. Base class does nothing.
    virtual void _validateState(const Fsr::NodeContext& exec_ctx,
                                bool                    for_real=false) { }

    //! Return abort (-1) on user-interrupt so processing can be interrupted.
    virtual int _execute(const Fsr::NodeContext& target_context,
                         const char*             target_name,
                         void*                   target,
                         void*                   src0=NULL,
                         void*                   src1=NULL);


    //! Expand the Node possibly creating additional internal Nodes. Returns false on user abort.
    virtual bool _expandContents(const char* node_mask) { return false; }

    //! Destroy the Node's contents. Base class calls destroyContents() on all children.
    virtual void _destroyContents();


  public:
    //!
    Node(Node* parent=NULL);

    //!
    Node(const ArgSet& args,
         Node*         parent=NULL);

    //! Must have a virtual destructor!
    virtual ~Node();


    //--------------------------------------------------------------------------------- 


    //! Convenience functions testing 'fsr:node:debug' and 'fsr:node:debug_attribs' args.
    int  debug()        const;
    int  debugAttribs() const;
    bool debugOff() const;
    bool debug1()   const;
    bool debug2()   const;
    bool debug3()   const;


    //--------------------------------------------------------------------------------- 


    //! Returns the class name, must implement.
    virtual const char* fuserNodeClass() const=0;

    //! Create a Fsr::Node instance based on the class name ('AbcIO', 'UsdIO', 'PerspCamera', 'Spotlight', etc.)
    static Node* create(const char*   node_class,
                        const ArgSet& args,
                        Node*         parent=NULL);

    static Node* create(const Description& node_description,
                        const ArgSet&      args,
                        Node*              parent=NULL);

    //! Find a Fsr::Node::Description by Fuser class name ('AbcIO', 'UsdIO', 'PerspCamera', 'Spotlight', etc.)
    static const Description* find(const char* node_class) { return Description::find(node_class); }


    //--------------------------------------------------------------------------------- 


    //! Some standard args:
    virtual const std::string& getName() const { return m_args[Arg::node_name]; }
    virtual const std::string& getPath() const { return m_args[Arg::node_path]; }
    virtual const std::string& getType() const { return m_args[Arg::node_type]; }


    //! Returns true if Node has a non-identity transform - default returns false.
    virtual bool       haveTransform() const { return false; }
    //! Return the local-space transform matrix of the node - default returns identity.
    virtual Fsr::Mat4d getLocalTransform() { return Fsr::Mat4d::getIdentity(); }
    //! Return the world-space transform matrix of the node - default returns identity.
    virtual Fsr::Mat4d getWorldTransform() { return Fsr::Mat4d::getIdentity(); }


    //! Returns true if local bbox is empty - default returns true.
    virtual bool       isLocalBboxEmpty() { return true; }
    //! Nodes can implement this to return a custom local-space bbox - default returns an empty bbox.
    virtual Fsr::Box3d getLocalBbox() { return Fsr::Box3d(); }
    //! Nodes can implement this to return a custom world-space bbox - default returns an empty bbox.
    virtual Fsr::Box3d getWorldBbox() { return Fsr::Box3d(); }


    //--------------------------------------------------------------------------------- 


    //! Will this node generate additional Fsr::Node children?
    virtual bool willProduceChildren() const { return false; }

    //! Add a child node, this node takes ownership of pointer.
    unsigned addChild(Node* node);
    //!
    unsigned numChildren() const { return (unsigned)m_children.size(); }
    //! Returns NULL if index is outside range.
    Node*    getChild(unsigned index) const { assert(index < m_children.size()); return m_children[index]; }
    //! Returns -1 if not in child list.
    int      getChild(Node* node) const;
    //! Returns NULL if named node not in child list.
    Node*    getChildByName(const char*        child_name) const;
    Node*    getChildByName(const std::string& child_name) const { return getChildByName(child_name.c_str()); }
    //! Returns NULL if a node with the path is not found in child list.
    Node*    getChildByPath(const char*        child_path) const;
    Node*    getChildByPath(const std::string& child_path) const { return getChildByPath(child_path.c_str()); }
    //!
    const std::vector<Node*>& getChildren() const { return m_children; }

    //! Return the Node that's the parent of this one.
    Node* parent() const { return m_parent; }


    //--------------------------------------------------------------------------------- 


    //!< Is the node up to date?
    bool isValid() const { return m_is_valid; }

    //! Called before execution to allow node to update local data from args.
    void validateState(const ArgSet&           node_args,
                       const Fsr::NodeContext& exec_ctx,
                       bool                    for_real,
                       bool                    force=false);

    //! Turn off the valid flag so that node validates itself again.
    void invalidateState() { m_is_valid = false; }

    //! Expand the Node possibly creating additional internal Nodes. Returns false on user abort.
    bool expandContents(const char* node_mask="*");


    //--------------------------------------------------------------------------------- 


    /*! Creates, executes, then deletes a Fsr::Node instance.
        The resulting error state and any error message is in the returned ErrorContext.
    */
    static ErrCtx executeImmediate(const char*             node_class,
                                   const ArgSet&           node_args,
                                   Node*                   node_parent,
                                   const Fsr::NodeContext& execute_target_context,
                                   const char*             execute_target_name,
                                   void*                   execute_target=NULL,
                                   void*                   execute_src0=NULL,
                                   void*                   execute_src1=NULL);


    //--------------------------------------------------------------------------------- 


    /*! Returns -1 on user-interrupt so processing can be interrupted, 0 if no
        error, or -2 if an error occured.
        Use errorState() and errorMessage() to retrieve full error results.
    */
    int execute(const ArgSet&           node_args,
                const Fsr::NodeContext& target_context,
                const char*             target_name,
                void*                   target=NULL,
                void*                   src0=NULL,
                void*                   src1=NULL);

    //! Destroy the Node's contents. Base class calls destroy on child nodes.
    void destroyContents();


    //--------------------------------------------------------------------------------- 


    //! Sets the error state to -2 and assigns the error message. Returns -2.
    int  error(const char* msg, ...);
    //! Sets the error state to -1 and clears the error message. Returns -1.
    int  abort();
    //! Removes the error state and clears the error message.
    void clearError();


    //! Convenience functions testing 'fsr:node:error_state' and 'fsr:node:error_msg' args.
    bool               hasError()     const;
    int                errorState()   const;
    const std::string& errorMessage() const;

    /*! Typically enabled when an execute() method detects a DDImage user-abort.
        This can be tested occasionally to interrupt heavy processing loops and
        improve user response.
    */
    bool               hasAborted()   const;


    //--------------------------------------------------------------------------------- 


    // TODO: deprecate all the status stuff? Or convert to an arg?
    //!
    unsigned status() const { return m_status; }
    void     setStatus(unsigned status) { m_status = status; }
    //!
    bool disabled()    const { return (m_status == DISABLED); }
    bool notExpanded() const { return (m_status == NOT_EXPANDED); }
    bool inProgress()  const { return (m_status == EXPANSION_IN_PROGRESS); }
    bool isComplete()  const { return (m_status == EXPANSION_COMPLETE); }
    //!
    void setNotExpanded() { m_status = NOT_EXPANDED; }
    void setInProgress()  { m_status = EXPANSION_IN_PROGRESS; }
    void setComplete()    { m_status = EXPANSION_COMPLETE; }
    void disable()        { _destroyContents(); m_status = DISABLED; }
    void enable()         { m_status = NOT_EXPANDED; }


    //--------------------------------------------------------------------------------- 


    //! Read/write access to arg set.
    const ArgSet& args() const { return m_args; }
    ArgSet&       args()       { return m_args; }

    //
    bool hasArg(const std::string& key) const { return m_args.has(key); }
    bool hasArg(const char*        key) const { return m_args.has(key); }
    //
    const std::string& getArg(const std::string& key) const { return m_args.get(key); }
    const std::string& getArg(const char*        key) const { return m_args.get(key); }
    //
    const std::string& operator[] (const std::string& key) const { return m_args[key]; }
    const std::string& operator[] (const char*        key) const { return m_args[key]; }

    //-------------------------------------------------------------------------
    // Typed read access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    const std::string& getString(const std::string& key, const std::string& dflt_val=empty_string) const { return m_args.getString(key, dflt_val); }
    const std::string& getString(const char*        key, const std::string& dflt_val=empty_string) const { return m_args.getString(key, dflt_val); }
    //
    int           getInt(const std::string& key, int        dflt_val=0)     const { return m_args.getInt(key, dflt_val); }
    int           getInt(const char*        key, int        dflt_val=0)     const { return m_args.getInt(key, dflt_val); }
    double     getDouble(const std::string& key, double     dflt_val=0.0)   const { return m_args.getDouble(key, dflt_val); }
    double     getDouble(const char*        key, double     dflt_val=0.0)   const { return m_args.getDouble(key, dflt_val); }
    bool         getBool(const std::string& key, bool       dflt_val=false) const { return m_args.getBool(key, dflt_val); }
    bool         getBool(const char*        key, bool       dflt_val=false) const { return m_args.getBool(key, dflt_val); }
    HashValue    getHash(const std::string& key, HashValue  dflt_val=~0ULL) const { return m_args.getHash(key, dflt_val); }
    HashValue    getHash(const char*        key, HashValue  dflt_val=~0ULL) const { return m_args.getHash(key, dflt_val); }
    //
    Fsr::Vec2d  getVec2d(const std::string& key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const { return m_args.getVec2d(key, dflt_val); }
    Fsr::Vec2d  getVec2d(const char*        key, Fsr::Vec2d dflt_val=Fsr::Vec2d(0.0)) const { return m_args.getVec2d(key, dflt_val); }
    //
    Fsr::Vec3d  getVec3d(const std::string& key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const { return m_args.getVec3d(key, dflt_val); }
    Fsr::Vec3d  getVec3d(const char*        key, Fsr::Vec3d dflt_val=Fsr::Vec3d(0.0)) const { return m_args.getVec3d(key, dflt_val); }
    //
    Fsr::Vec4d  getVec4d(const std::string& key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const { return m_args.getVec4d(key, dflt_val); }
    Fsr::Vec4d  getVec4d(const char*        key, Fsr::Vec4d dflt_val=Fsr::Vec4d(0.0)) const { return m_args.getVec4d(key, dflt_val); }
    //
    Fsr::Mat4d  getMat4d(const std::string& key, Fsr::Mat4d dflt_val=Fsr::Mat4d::getIdentity()) const { return m_args.getMat4d(key, dflt_val); }
    Fsr::Mat4d  getMat4d(const char*        key, Fsr::Mat4d dflt_val=Fsr::Mat4d::getIdentity()) const { return m_args.getMat4d(key, dflt_val); }

    //-------------------------------------------------------------------------
    // Typed write access. These are just naive string conversions!
    //-------------------------------------------------------------------------
    void   setString(const std::string& key, const std::string& value) { m_args.setString(key, value); }
    void   setString(const std::string& key, const char*        value) { m_args.setString(key, value); }
    void   setString(const char*        key, const std::string& value) { m_args.setString(key, value); }
    void   setString(const char*        key, const char*        value) { m_args.setString(key, value); }
    //
    void      setInt(const std::string& key, int       value) { m_args.setInt(key, value); }
    void   setDouble(const std::string& key, double    value) { m_args.setDouble(key, value); }
    void     setBool(const std::string& key, bool      value) { m_args.setBool(key, value); }
    void     setHash(const std::string& key, HashValue value) { m_args.setHash(key, value); }
    //
    void    setVec2d(const std::string& key, const Fsr::Vec2d& value) { m_args.setVec2d(key, value); }
    void    setVec3d(const std::string& key, const Fsr::Vec3d& value) { m_args.setVec3d(key, value); }
    void    setVec4d(const std::string& key, const Fsr::Vec4d& value) { m_args.setVec4d(key, value); }
    void    setMat4d(const std::string& key, const Fsr::Mat4d& value) { m_args.setMat4d(key, value); }


  private:
    //! Disabled copy constructor.
    Node(const Node&);
    //! Disabled copy operator.
    Node& operator=(const Node&);

}; // Node


//-------------------------------------------------------------------------


/*! Empty, deletable (temporary) node containing the error state and message
    returned from methods like create() on failure to create a valid node.

    Node build methods should return one of these to communicate any issue
    back to the create() method, which is the passed back through execute()
    to the calling method.
    ie:
        Node* node = create("FooBar", args);
        if (node)
        {
            std::cerr << "create returned NULL node - no idea why..." << std::endl;
        }
        else if (node->hasError())
        {
            std::cerr << "create failed to instantiate node returning error ";
            std::cerr << node->errorState() << ": '" << node->errorMessage() << "'";
            std::cerr << std::endl;
        }

*/
class FSR_EXPORT ErrorNode : public Fsr::Node
{
  public:
    /*! Ctor requires an error code, message, and the class name of the builder.
        The error message will have the class name prepended to it.
    */
    ErrorNode(const char* builder_class,
              int         error_state,
              const char* error_msg, ...);

    /*virtual*/ const char* fuserNodeClass() const { return "ErrorNode"; }

    //! Called before execution to allow node to update local data from args.
    /*virtual*/ void _validateState(const Fsr::NodeContext& exec_ctx,
                                    bool                    for_real) {}


    //! Do nothing, silence warning.
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        return 0; // success
    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


#if 0
// TODO: don't need this any longer but it's good to refer to
#ifdef FUSER_STATIC_LIB
/*! This procedure needs to be implemented in a Fsr::Node
    plugin so that the Description pointer can be accessed via
    a dlsym() call.  This allows a plugin linked against a static
    version of the Fuser lib to return the Description pointer.
    The method in the plugin needs to be prefaced with 'extern "C"', i.e.:
        extern "C"
        const Fsr::Node::Description* FUSER_GET_DESCRIPTION() { return &myProcedural::description; }

*/
typedef const Node::Description* (*FUSER_GET_DESCRIPTION)(void);
#endif
#endif


} // namespace Fsr

#endif

// end of Fuser/Node.h

//
// Copyright 2019 DreamWorks Animation
//
