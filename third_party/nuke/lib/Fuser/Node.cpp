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

/// @file Fuser/Node.cpp
///
/// @author Jonathan Egstad

#include "Node.h"
#include "ExecuteTargetContexts.h"
#include "Primitive.h"

#include <DDImage/plugins.h>
#include <DDImage/Thread.h> // for Lock

#include <fstream>  // for pin
#include <stdarg.h> // for va_start, va_end
#include <dlfcn.h>  // for dlerror

//#define CHECK_LOAD_TIMES 1
#ifdef CHECK_LOAD_TIMES
#   include <sys/time.h>
#endif

namespace Fsr {

//-----------------------------------------------------------------------------

/*static*/ const char* SceneArchiveContext::name    = "SceneArchive";
/*static*/ const char* ScenePathFilters::name       = "ScenePathFilters";
/*static*/ const char* SceneNodeDescriptions::name  = "SceneNodeDescriptions";
/*static*/ const char* SelectedSceneNodePaths::name = "SelectedSceneNodePaths";

/*static*/ const char* SceneOpImportContext::name   = "SceneOpImport";
/*static*/ const char* PrimitiveViewerContext::name = "drawGL";

/*static*/ const char* MeshTessellateContext::name  = "MeshTessellate"; // generic mesh version

/*static*/ const char* FuserPrimitive::DDImageRenderSceneTessellateContext::name = "DDImageRenderSceneTessellate";

//-----------------------------------------------------------------------------

static DD::Image::Lock expand_lock;

/*!
*/
Node::Node(Node* parent) :
    m_parent(parent),
    m_is_valid(false),
    m_status(NOT_EXPANDED)
{
    //std::cout << "  Node::ctor(" << this << ")" << std::endl;
}

/*!
*/
Node::Node(const ArgSet& args,
           Node*         parent) :
    m_parent(parent),
    m_args(args),
    m_is_valid(false),
    m_status(NOT_EXPANDED)
{
    //std::cout << "  Node::ctor(" << this << ")" << std::endl;
}

/*!
*/
/*virtual*/
Node::~Node()
{
    //std::cout << "    Node::dtor(" << this << ")" << std::endl;
    for (unsigned i=0; i < m_children.size(); ++i)
        delete m_children[i];
}


//---------------------------------------------------------------------------------------

//! Convenience functions testing 'fsr:node:error' and 'fsr:node:error_msg' args.
bool               Node::hasError()     const { return (m_args.getInt(Arg::node_error_state) <= -2); }
int                Node::errorState()   const { return m_args.getInt(Arg::node_error_state); }
const std::string& Node::errorMessage() const { return m_args.getString(Arg::node_error_msg); }
bool               Node::hasAborted()   const { return (m_args.getInt(Arg::node_error_state) == -1); }


/*! Sets the error state to -2 and assigns the error message.
    Returns -2 as a convenience.
*/
int
Node::error(const char* msg, ...)
{
    const int error_state = this->errorState();
    // Avoid setting it again:
    if (error_state > -2)
    {
        // Expand the va list:
        char buf[2048];
        va_list args;
        va_start(args, msg);
        vsnprintf(buf, 2048, msg, args);
        va_end(args);

        m_args.setInt(Arg::node_error_state, -2);
        m_args.setString(Arg::node_error_msg, buf);
    }

    return -2;
}

/*! Sets the error state to -1 and clears the error message.
    Returns -1 as a convenience.
*/
int
Node::abort()
{
    m_args.setInt(Arg::node_error_state, -1);
    m_args.remove(Arg::node_error_msg);

    return -1;
}

/*! Remove the error state and clear the error message.
*/
void
Node::clearError()
{
    m_args.remove(Arg::node_error_state);
    m_args.remove(Arg::node_error_msg);
}


/*!
*/
ErrorNode::ErrorNode(const char* builder_class,
                     int         error_state,
                     const char* error_msg, ...) :
    Fsr::Node(NULL/*parent*/)
{
    if (error_state == -1)
    {
        abort();
    }
    else if (error_state <= -2)
    {
        //! Unfortunately can't forward va_lists to other methods:
        char buf[2048];
        va_list args;
        va_start(args, error_msg);
        vsnprintf(buf, 2048, error_msg, args);
        va_end(args);

        error("%s: %s", builder_class, buf);
    }
}


/*static*/ const char* NodeContext::debug_modes[] = { "off", "1", "2", "3", 0 };

//! Convenience functions testing 'fsr:node:debug' and 'fsr:node:debug_attrib' args.
int  Node::debug()        const { return m_args.getInt(Arg::node_debug); }
int  Node::debugAttribs() const { return m_args.getInt(Arg::node_debug_attribs); }
bool Node::debugOff()     const { return (debug() == NodeContext::DEBUG_OFF); }
bool Node::debug1()       const { return (debug() == NodeContext::DEBUG_1  ); }
bool Node::debug2()       const { return (debug() == NodeContext::DEBUG_2  ); }
bool Node::debug3()       const { return (debug() == NodeContext::DEBUG_3  ); }


//-----------------------------------------------------------------------------


/*! Add a child node, this node take ownership of pointer.
*/
unsigned
Node::addChild(Node* node)
{
    m_children.push_back(node);
    return (unsigned)(m_children.size()-1);
}


//---------------------------------------------------------------------------------------


/*! Called before execution to allow node to update local data from args.
    This method calls _validateState() on itself first, then calls validateState() on all its children.
    Since the rNode tree is inverted the top-most rNode must be validated first before the
    dependent children are.
*/
void
Node::validateState(const Fsr::NodeContext& args,
                    bool                    for_real,
                    bool                    force)
{
    //std::cout << "-------------------------------------------------------------------" << std::endl;
    //std::cout << "Fsr::Node::validateState('" << fuserNodeClass() << "') args[" << args.args() << "]" << std::endl;

    // Validate parent first:
    if (m_parent)
        m_parent->validateState(args, for_real, force);

    if (force)
        m_is_valid = false;
    else
    {
        // Compare values of args and if anything's changed invalidate:
        // TODO: move this to the ArgSet class
        //std::cout << "  -------------------------------------------------------------------" << std::endl;
        //std::cout << "  Fsr::Node::validateState(" << this << ") current[" << m_args << "]" << std::endl;

        const ArgSet& new_args = args.args();
        if (new_args.size() == 0)
        {
            m_is_valid = true; // no args to change
        }
        else
        {
            //std::cout << "  Fsr::Node::validateState(" << this << ") new[" << new_args << "]" << std::endl;
            for (ArgSet::const_iterator it=new_args.begin(); it != new_args.end(); ++it)
            {
                if (!hasArg(it->first) || getArg(it->first) != it->second)
                {
                    m_args.set(it->first, it->second);
                    m_is_valid = false; // new arg or different value, invalidate
                    //std::cout << "    arg['" << it->first << "'] changed to '" << it->second << "', invalidating node." << std::endl;
                }
            }
        }
    }

    if (!m_is_valid)
    {
        // Get our local vars up to date:
        _validateState(args, for_real);
        m_is_valid = true;
    }
}


/*! Returns abort (-1) on user-interrupt so processing can be interrupted, 0 if no
    error, or -2 if an error occured.

    This method calls validateState() on the node then _execute().

    Use errorState() and errorMessage() to retrieve full execution results.
*/
int
Node::execute(const Fsr::NodeContext& target_context,
              const char*             target_name,
              void*                   target,
              void*                   src0,
              void*                   src1)
{
    //std::cout << "  *****************************************************************" << std::endl;
    //std::cout << "  *****************************************************************" << std::endl;
    //std::cout << "  Fsr::Node::execute('" << fuserNodeClass() << "')";
    //std::cout << " - execute_target_name='" << target_name << "'";
    //std::cout << ", target args[" << target_context.args() << "]" << std::endl;

    // Validate the node then execute it:
    validateState(target_context, true/*for_real*/, false/*force*/);

    clearError();
    int ret = _execute(target_context,
                       target_name,
                       target,
                       src0,
                       src1);
    if (ret == 0)
    {
        //std::cout << "     ...no error" << std::endl;
        return 0;
    }
    else if (ret == -1)
    {
        //std::cout << "     ...user-abort" << std::endl;
        return -1;
    }
    else if (ret <= -2)
    {
        //std::cout << "     ...error=" << node->errorState() << ": '" << node->errorMessage() << "'" << std::endl;
        return ret;
    }
    return ret;
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
    Base class returns an unrecognized-target error.
*/
/*virtual*/ int
Node::_execute(const Fsr::NodeContext& target_context,
               const char*             target_name,
               void*                   target,
               void*                   src0,
               void*                   src1)
{
    return error("unrecognized target '%s'. This is likely a coding error", target_name);
}


/*! Creates, executes, then deletes a Fsr::Node instance.

    node_parent is passed to the created node's builder method.

    If a ErrorNode was returned from the create() method its error state and message
    are used, otherwise an unspecific error message is formulated.

    The resulting error state and any error message are in the returned ErrorContext.
*/
/*static*/ Node::ErrCtx
Node::executeImmediate(const char*             node_class,
                       const ArgSet&           node_args,
                       Node*                   node_parent,
                       const Fsr::NodeContext& execute_target_context,
                       const char*             execute_target_name,
                       void*                   execute_target,
                       void*                   execute_src0,
                       void*                   execute_src1)
{
    //std::cout << "*****************************************************************" << std::endl;
    //std::cout << "*****************************************************************" << std::endl;
    //std::cout << "Fsr::Node::executeImmediate() node_class='" << node_class << "'";
    //std::cout << " - execute_target_name='" << execute_target_name << "'" << std::endl;

    ErrCtx ret;

    Node* node = Node::create(node_class, node_args, node_parent);
    if (!node)
    {
        // Formulate a simple error message since there's a NULL node returned:
        ret.state = -2;
        ret.msg  = "cannot create Fsr::Node of class type '";
        ret.msg += node_class;
        ret.msg += "'";
        return ret;
    }
    //std::cout << "   ...create('" << node_class << "') error=" << node->errorState() << ": '" << node->errorMessage() << "'" << std::endl;

    int result = node->errorState();
    if (result == -1)
    {
        ret.state = -1; // user-abort
    }
    else if (result <= -2)
    {
        // Copy out error message before deleting node:
        ret.state = result;
        ret.msg   = node->errorMessage();
        //std::cout << "   ...create() error " << ret.state << ": '" << ret.msg << "' occured" << std::endl;
    }
    else
    {
        // execute() will call validateState() on the Node:
        result = node->execute(execute_target_context,
                               execute_target_name,
                               execute_target,
                               execute_src0,
                               execute_src1);

        if (result == -1)
        {
            ret.state = -1; // user-abort
        }
        else if (result <= -2)
        {
            // Copy out error message before deleting node:
            ret.state = node->errorState();
            ret.msg   = node->errorMessage();
            //std::cout << "   ...error -2 occured '" << ret.msg << "'" << std::endl;
        }
        else
        {
            //std::cout << "   ...no error" << std::endl;
        }
    }

    delete node;

    return ret;
}


//---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------


/*! Destroy the Node's contents.
    Base class calls destroy on child nodes.
*/
void
Node::destroyContents()
{
    for (unsigned i=0; i < m_children.size(); ++i)
        m_children[i]->destroyContents();
    _destroyContents();
}


/*! Destroy the Node's contents. Base class calls destroyContents() on all children.
*/
/*virtual*/
void
Node::_destroyContents()
{
    m_status = NOT_EXPANDED;
    m_is_valid = false;

    for (unsigned i=0; i < m_children.size(); ++i)
        delete m_children[i];
    m_children.clear();
}


//---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------


/*! Expand the Node possibly creating additional internal Nodes.
    Returns false on user abort.
    This handles the threading lock loop.
    If the node needs's expanding _expandContents() will be called on the subclass.

    TODO: switch this wait loop to std::unique_lock condition var as
          found in GeoOpGeometryEngineContext
*/
bool
Node::expandContents(const char* node_mask)
{
    //std::cout << "Node(" << this << ")::expandContents()" << std::endl;
    if (isComplete())
       return true; // Return immediately if done

    // Check if node's available to be expanded.
    // We do this in a loop to make sure dependant threads don't
    // continue until this object has been fully expanded:
    //unsigned limit_count = 6000; // 0.01*6000 = 60seconds
    while (1)
    {
        // If another thread has finished it we're done:
        if (isComplete())
            return true;

        // Check for not-expanded twice to avoid a race condition:
        if (notExpanded())
        {
            expand_lock.lock();
            if (notExpanded())
            {
                // Grab it and unlock so other threads can continue:
                setInProgress();
                expand_lock.unlock();
                if (!_expandContents(node_mask))
                    return false; // user-abort

                // And we're done:
                setComplete();
                return false;
            }
            else
                expand_lock.unlock();
        }

        // Pause briefly then try again:
        DD::Image::sleepFor(0.01/*seconds*/);

        //if (--limit_count == 0) {
        //   std::cerr << "  rContext::expand_objects() limit count reached!  This is likely the result of a bug." << std::endl;
        //   break;
        //}
    } // while (1) loop

    // This is an error case as the loop should never be
    // exited unless finished or aborted...
    // return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


/*  Map of already loaded Fsr::Node::Descriptions to speed up lookups.

    Use of this static singleton class allows m_dso_map to be shared between
    plugins that statically link against libFuser.

    Not clear why this is any functionally different than declaring a static
    NodeDescMap, but I'm sure smarter folk than I know. Probably something
    to do with when the static object is created in the process memory.
*/
class DsoMap
{
  private:
#ifdef DWA_INTERNAL_BUILD
    typedef std::map<std::string, const Node::Description*> NodeDescMap;
#else
    typedef std::unordered_map<std::string, const Node::Description*> NodeDescMap;
#endif
    NodeDescMap m_dso_map;


  public:
    //!
    DsoMap()
    {
        //std::cout << "DsoMap::ctor(): NodeDescMap singleton=" << &m_dso_map << std::endl;
        //m_dso_map[std::string("Foo")] = NULL;
    }

    /*! Return the static DsoMap singleton.

        For some magical reason this works across statically linked plugins, but putting
        'static NodeDescMap m_dso_map;' as a global in the cpp file doesn't...
        Probably something to do with when the static object is created in the process memory.
    */
    static NodeDescMap* dsoMap()
    {
        static DsoMap m_instance;
        //std::cout << "  DsoMap::instance(): singleton=" << &m_instance.m_dso_map << std::endl;
        return &m_instance.m_dso_map;
    }

    //!
    static const Node::Description* find(const std::string& node_class)
    {
        if (node_class.empty())
            return NULL;
        DD::Image::Guard guard(expand_lock); // just in case...
        NodeDescMap::const_iterator it = dsoMap()->find(node_class);
        if (it != dsoMap()->end())
            return it->second;
        return NULL; // not found
    }
    
    //!
    static void add(const std::string&       node_class,
                    const Node::Description* desc)
    {
        if (node_class.empty() || !desc)
            return;
        DD::Image::Guard guard(expand_lock); // just in case...
        (*dsoMap())[node_class] = desc;
    }

};


//--------------------------------------------------------------------------------------------------

#if 0
// TODO: don't need this any longer but it's good to refer to
#ifdef FUSER_STATIC_LIB

// Which plugin extension to use depends on operating system. This is
// only used for the static lib support find routine:
static const char* const plugin_extensions[] =
{
#ifdef _WIN32
    ".dll",
#elif defined __APPLE__
    ".dylib",
#else
    ".so",
#endif
    ".tcl", // allow .tcl alias files
    0
};

#include <dlfcn.h>

#endif
#endif

//-----------------------------------------------------------------------------


/*static*/
Node*
Node::create(const Description& node_description,
             const ArgSet&      args,
             Node*              parent)
{
    return create(node_description.fuserNodeClass(), args, parent);
}


/*! Create a Node instance based on the type name ('abcProcedural', 'PerspectiveCamera', etc.)
    Calling code takes ownership of returned pointer.
*/
/*static*/
Node*
Node::create(const char*   node_class,
             const ArgSet& args,
             Node*         parent)
{
    if (!node_class || !node_class[0])
        return NULL;
    //std::cerr << "Fsr::Node::create('" << node_class << "')" << std::endl;

#ifdef CHECK_LOAD_TIMES
    struct timeval time_0;
    gettimeofday(&time_0, NULL/*timezone*/);
#endif

    // Get the description by name:
    const Node::Description* desc = find(node_class);
    if (!desc)
        return NULL; // can't find plugin...

    // Allocate a new one and return it:
    Node* dso = desc->builder_method(node_class, args, parent);
    if (!dso)
    {
        //std::cerr << "Fsr::Node::create(): error, cannot allocate new primitive of type '" << node_class << "'" << std::endl;
        return NULL;
    }
    //std::cerr << "loaded description '" << desc->fuserNodeClass() << "', dso=" << dso << std::endl;

#ifdef CHECK_LOAD_TIMES
    struct timeval time_1;
    gettimeofday(&time_1, NULL/*timezone*/);
    const double tStart = double(time_0.tv_sec) + (double(time_0.tv_usec)/1000000.0);
    const double tEnd   = double(time_1.tv_sec) + (double(time_1.tv_usec)/1000000.0);
    const double tSecs = (tEnd - tStart);
    std::cout << "Fsr::Node::create() load delay=" << tSecs << std::endl;
#endif

    return dso;
}



/*! Constructor sets name and label to same value.
*/
Node::Description::Description(const char*   node_class,
                               PluginBuilder builder) :
    m_node_class(node_class),
    builder_method(builder)
{
    //std::cout << "  Fsr::Node::Description::ctor(" << node_class << ")" << std::endl;

    // DD::Image::Description.h:
    //  const char* compiled;   // Date and DD_IMAGE_VERSION_LONG this was compiled for
    //  const char* plugin;     // Set to the plugin filename
    //  License*    license;    // If non-null, license check is run

    // No need for license checks, although this could be leveraged to stop
    // DD::Image from loading a Fuser plugin accidentally:
    DD::Image::Description::license = NULL;

    // Register the plugin callback - this is called when the plugin is loaded:
    Description::ctor(pluginBuilderCallback);

    // Update compiled string to use Fuser version rather than kDDImageVersion:
    DD::Image::Description::compiled = __DATE__ " for Fuser-" FuserVersion;
}


/*! Called when the plugin .so is first loaded.
    This adds the plugin class to the map of loaded dsos so that we don't need
    to search or load the .so again.
*/
/*static*/
void
Node::Description::pluginBuilderCallback(DD::Image::Description* desc)
{
    if (!desc)
        return; // don't crash...

    const Node::Description* dso_desc = static_cast<const Node::Description*>(desc);

    const char* node_class = dso_desc->fuserNodeClass();
    assert(node_class && node_class[0]);

    //std::cout << "  Fsr::Node::Description::pluginBuilderCallback(" << dso_desc << "):";
    //std::cout << " node_class='" << node_class << "'" << std::endl;

    // Add to dso map if it doesn't already exist.
    // Statically linked plugins will cause the libFuser built in descriptions
    // to call this repeatedly, so ignore any repeats:
    if (!DsoMap::find(std::string(node_class)))
    {
        DsoMap::add(std::string(node_class), dso_desc);
        //std::cout << "    (pluginBuilderCallback) adding '" << node_class << "'=" << dso_desc << std::endl;
    }
}


/*! Find a dso description by name.

    If it's been loaded before it quickly returns an existing cached
    Description, otherwise it prepends 'fsr' to the start of the name
    (ie 'fsrMyFuserClass')  before searching the plugin paths for a
    matching plugin filename.

    Returns NULL if not found.
*/
/*static*/
const Node::Description*
Node::Description::find(const char* node_class)
{
    if (!node_class || !node_class[0])
        return NULL;  // just in case...
    const std::string dso_name(node_class);

    //std::cout << "Fsr::Node::Description::find('" << dso_name << "') dso_map=" << DsoMap::dsoMap() << std::endl;

    // Search for existing dso using the base fuserNodeClass() name
    // (ie UsdIO, UsdaIO, MeshPrim, etc)
    const Node::Description* dso_desc = DsoMap::find(dso_name);
    if (dso_desc)
        return dso_desc;

    // Not found, prepend 'fsr' to name and search the plugin paths for
    // the plugin dso file (ie fsrUsdIO.so, fsrUsdaIO.tcl, fsrMeshPrim.so, etc)
    std::string plugin_name("fsr");
    plugin_name += dso_name;

    // Use the stock DDImage plugin load method, which supports .tcl redirectors.
    // It's important because we're relying on .tcl directors to handle aliasing
    // in several IO plugins:
    // NOTE: DD::Image::plugin_load() says that it returns NULL if a plugin is
    // not loaded but that does not appear to be the case. It returns the path
    // to the plugin it *attempted* to load, but only by checking plugin_error()
    // can we tell if dlopen() failed and what was returned in dlerror()
    const char* plugin_path = DD::Image::plugin_load(plugin_name.c_str());
    if (!plugin_path || !plugin_path[0])
    {
        std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
        std::cerr << "error: plugin not found." << std::endl;
        return NULL;  // plugin not found!
    }
    // Was there a dlerror() on load?
    if (DD::Image::plugin_error())
    {
        std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
        std::cerr << "error: plugin not loaded, dlopen error '" << DD::Image::plugin_error() << "'" << std::endl;
        return NULL;  // plugin not found!
    }

    // Plugin found and loaded, return the pointer that was added to the map:
    dso_desc = DsoMap::find(dso_name);
    if (!dso_desc)
    {
        // Error - the plugin should have been found! If not then the plugin
        // likely does not have defined Descriptions matching 'plugin_name':
        std::cerr << "Fsr::Node::Description::find('" << dso_name << "') ";
        std::cerr << "error: plugin did not define a Fsr::Node::Description matching ";
        std::cerr << "the plugin name - this is likely a coding error.";
        if (dlerror())
            std::cerr << " '" << dlerror() << "'";
        std::cerr << std::endl;
        return NULL;  // plugin not found!
    }

    return dso_desc;

#if 0
    // Replacement for DD::Image::plugin_load()
    //
    // We only support one level of .tcl alias indirection, more would require
    // making this routine a loop.
    //
    // TODO: don't need this replacement code any longer but it's good to
    //       keep around and refer to.
    //
    const char* filepath = DD::Image::plugin_find(plugin_name.c_str(), plugin_extensions);
    if (filepath == NULL || filepath[0] == NULL)
    {
       std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
       std::cerr << "error: plugin not found!" << std::endl;
       return NULL;
    }
    std::string plugin_path(filepath);
    //std::cout << "  plugin_path='" << plugin_path << "'" << std::endl;

    // If it's a .tcl alias file parse it to find the dso name and repeat
    // the plugin search:
    if (plugin_path.rfind(".tcl") != std::string::npos)
    {
        // Load the .tcl file contents and parse the simple expression inside.
        // We only support the expression 'load <plugin_name>', where <plugin_name>
        // is the full name of the actual dso plugin to load without extension,
        // i.e. 'load fsrUsdIO' or 'load fsrAbcIO'.
        bool found_alias = false;
        std::ifstream pin(filepath);
        std::string line;
        while (pin)
        {
            std::getline(pin, line);
            size_t a = line.find_first_not_of(" \t\r"); // skip leading whitespace
            if (a == std::string::npos || line[a] == '#')
                continue; // skip empty lines or comments
            a = line.find("load ", a);
            if (a != std::string::npos)
            {
                plugin_name = line.substr(a + 5, std::string::npos);
                found_alias = true;
                break;
            }
        }
        if (!found_alias)
        {
           std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
           std::cerr << "error: unable to read or parse tcl alias, plugin not found!" << std::endl;
           return NULL;
        }
        //std::cout << "    new plugin name ='" << plugin_name << "'" << std::endl;

        // Try to find the new plugin name:
        filepath = DD::Image::plugin_find(plugin_name.c_str(), plugin_extensions);
        if (filepath == NULL || filepath[0] == NULL)
        {
           std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
           std::cerr << "error with tcl alias: plugin not found!" << std::endl;
           return NULL;
        }
        plugin_path = filepath;
    }

    // Found plugin file path.
    // Load it and attempt to find the symbol identifying this plugin as a FuserNode plugin:

    dlerror(); // clear any existing error
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW);
    if (!handle)
    {
        std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
        std::cerr << "error: plugin not loaded, dlopen error '" << dlerror() << "'" << std::endl;
        return NULL;
    }

    // Find the getDsoDescriptionPtr symbol:
    const char* symbol_name = "FUSER_GET_DESCRIPTION";
    FUSER_GET_DESCRIPTION get_desc = (FUSER_GET_DESCRIPTION)dlsym(handle, symbol_name);
    if (!get_desc)
    {
        std::cerr << "Fsr::Node::Description::find('" << plugin_name << "') ";
        std::cerr << "error: plugin did not define symbol '" << symbol_name << "'" << std::endl;
        return NULL;
    }

    // Get the Description pointer from the plugin lib:
    dso_desc = get_desc();
    assert(dso_desc); // shouldn't happen...

    // Add it to this .so's copy of the dso_map:
    DsoMap::add(std::string(dso_desc->fuserNodeClass()), dso_desc);

    return dso_desc;
#endif

}


} // namespace Fsr


// end of Fuser/Node.cpp

//
// Copyright 2019 DreamWorks Animation
//
