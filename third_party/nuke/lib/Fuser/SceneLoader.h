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

/// @file Fuser/SceneLoader.h
///
/// @author Jonathan Egstad

#ifndef Fuser_SceneLoader_h
#define Fuser_SceneLoader_h

#include "NodeIOInterface.h"
#include "SceneOpExtender.h"
#include "Node.h" // for NodeDescriptionMap

#include <DDImage/Knob.h>


namespace Fsr {


// Use these in a Op::node_help() method like so:
// const char* node_help() const { return __DATE__ " " __TIME__ " "
//    "My cool Op's description.\n"
//    "\n"
//    SCENE_LOADER_HELP"\n";
// }
//
#define SCENE_LOADER_HELP \
"This Op can read from scene file formats like Alembic and USD containing scene nodes \
with transform info such as cameras, lights, models and locators. On script load if \
'read from file' is true the Op will automatically attempt to load the named node \
in the 'scene path' knob from within the file pointed to by the 'file path' knob. If \
'scene path' is empty then the first valid node in file will be loaded."


//--------------------------------------------------------------------------------------


/*! \class Fsr::SceneLoader

    \brief Interface class adding 3D scene node loading support for AxisOp
    and GeoOp classes.

    Note that this class is not intended for loading geometry, only
    scene level data like transforms and scene node attributes.

    For SceneLoader to read the scene node on initial script load
    the 'LoadHelperKnob' CustomKnob is added in addSceneLoaderKnobs()
    which is guaranteed to have its store() method called when the
    script loads.
    It's important to do this early because loading the scene data
    changes the values of various knobs, and this must be done before
    Op::validate(), and preferably before Op::append().

    This should be more reliable than requiring all the owner Ops to
    implement append() and have the script load happen then.

    i.e. Nuke calls the owner 'myop' like this on initial script load:
        myop->knobs(<create>);  // create the knob objects
        myop->knobs(<save>);    // save knob values into local vars
        myop->append(<hash>);   // allow Op to change hash
        myop->validate();       // figure stuff out from knob vars

    And after script load since the ops are already created Nuke
    no longer does knob creation and does this sequence:
        myop->knobs(<save>);    // save knob values into local vars
        myop->append(<hash>);   // allow Op to change hash
        myop->validate();       // figure stuff out from knob vars

    Basically we want SceneLoader to act as if the file values saved
    into the Op's transform knobs were saved by local Knobs, not an
    external file, so we want to save the values *before*
    Op::validate() (and preferably before Op::append()) gets called.

    *****************************************************************


*/
class FSR_EXPORT SceneLoader : public Fsr::NodeIOInterface,
                               public Fsr::SceneOpExtender
{
  public:
    struct SceneControls
    {
        bool        read_enabled;           //!< 
        const char* file;                   //!< Path to scene file
        int         file_version;           //!< Incremented when reload button is pushed
        //
        const char* node_path;              //!< Currently selected node
        //
        double      set_frame;              //!< Manually set the frame number to read
        double      frames_per_second;      //!<
        //
        int         decompose_xform_order;  //!< Preferred decompose xform order
        int         decompose_rot_order;    //!< Preferred decompose rotation order
        bool        T_enable;               //!< Enable loading of translations
        bool        R_enable;               //!< Enable loading of rotations
        bool        S_enable;               //!< Enable loading of scale
        bool        euler_filter_enable;    //!< Enable euler filter on rotations
        bool        parent_extract_enable;  //!< Split the parent xform out from the local
        //
        bool        k_copy_specular;
        int         k_hero_view;
        //
        int         k_cam_frame_mode;
        double      k_cam_frame_offset;
        //
        bool        read_debug;             //!< Print debug info during file loading
        bool        archive_debug;          //!< Print debug info during archive file handling
    };


  protected:
    SceneControls    k_scene_ctls;          //!< 
    bool             k_scene_loaded_legacy; //!< Enabled on first load if legacy knobs are detected
    bool             k_editable;            //!< Can this node be edited? Turned off on scene load.

    DD::Image::Hash  m_load_hash;           //!< Has the scene node been loaded yet?
    //int              m_scene_read_error;    //!< Has last read failed?

    DD::Image::Knob* kSceneView;            //!<

    bool             m_loader_error;        //!< Error was thrown attempting to load a scene node
    std::string      m_loader_error_msg;    //!< Error msg saved



  protected:
    //!
    virtual std::string _findDefaultNode(const std::string& scene_file_path,
                                         const std::string& fuser_plugin_type,
                                         const std::string& default_node_type,
                                         bool               debug);

    //! Implementation-specific readSceneNode() method. Base class executes the Fuser IO module.
    virtual bool _readSceneNode(const std::string& scene_file_path,
                                const std::string& expanded_node_path,
                                const std::string& fuser_plugin_type,
                                bool               debug);

    //! Enable the loader error state and fill in the error msg string.
    void setLoadError(const char* msg, ...);
    //! Disable the error state and clear the error message.
    void clearLoadError();
    //! Pop up a warning dialog the user must acknowledge.
    void showUserWarning(const char* msg, ...);


  public:
    //!
    SceneLoader(bool read_enabled=false);

    //! Must have a virtual destructor!
    virtual ~SceneLoader() {}


    //! Returns true if Op is a Fuser SceneLoader.
    static bool isSceneLoader(DD::Image::Op* op);

    //! Is scene loader enabled? Base class returns the state of the 'read_from_file' knob.
    virtual bool isSceneLoaderEnabled();


    //---------------------------------------------------------------------


    //! Call this from owner Op::knobs(). Adds the file options & scene node knobs.
    virtual void addSceneLoaderKnobs(DD::Image::Knob_Callback f,
                                     bool                     group_open=true,
                                     bool                     show_xform_knobs=true,
                                     bool                     show_hierarchy=true);


    //! Adds additional OpenGL display option controls. Currently it just adds the 'editable' switch.
    virtual void addDisplayOptionsKnobs(DD::Image::Knob_Callback);


    //! Call this from owner Op::knob_changed(). Updates loader gui and does node data reloads.
    /*virtual*/ int knobChanged(DD::Image::Knob* k,
                                int              call_again=0);


    //! Call this from owner Op::_validate(). Sets an error on owner Op if there's a loader error.
    virtual void validateSceneLoader(bool for_real);



    //---------------------------------------------------------------------


    //! Returns the file path to the scene file, or an empty string if 'scene_read_enabled' is off.
    std::string getSceneFilePath();


    //! Returns the scene path for the selected node, or an empty string if 'scene_read_enabled' is off.
    std::string getSceneNodePath();


    //---------------------------------------------------------------------

    //! Check if 'read_from_file' is true, 'scene_file' string is not empty but 'scene_node' IS empty.
    void checkForValidNodePath();

    /*! Possibly load a scene node into the loaderOp() using values at current outputContext().
        If force_update == true then the hash is *always* updated and may cause a load.
        If force_load == true then loadSceneNode() is *always* called.
    */
    void updateSceneNode(bool force_update=false,
                         bool force_load=false);

    /*! Possibly load a scene node into the loaderOp(). A hash is built
        from knobs that affect the load state and if it has changed since
        last load then loadSceneNode() is called.

        If force_update == true then the hash is *always* updated and may cause a load.
        If force_load == true then loadSceneNode() is *always* called.
    */
    void updateSceneNode(DD::Image::Hash&                hash,
                         const DD::Image::OutputContext* context=NULL,
                         bool                            force_update=false,
                         bool                            force_load=false);

    //! Try to load a scene node into the loaderOp(). Returns true on success. This will call readSceneNode().
    bool loadSceneNode(const SceneControls& scene_ctrls);


    //---------------------------------------------------------------------

    //! Update the scenegraph display knob (gets the node descriptions first.)
    void updateSceneGraph();
    //! Update the scenegraph display knob with the list of scene file nodes.
    void updateSceneGraph(const Fsr::NodeDescriptionMap& node_descriptions,
                          const char*                    no_nodes_message="");

    //! Enable/disable knobs filled in by the node read.
    virtual void enableSceneLoaderKnobs(bool scene_loader_enabled);
    virtual void enableSceneLoaderExtraKnobs(bool read_enabled);

    //! Get a list of node descriptions from the scene file.
    bool getNodeDescriptions(const char*              file,
                             Fsr::NodeDescriptionMap& node_descriptions,
                             bool                     debug=false);

    //!
    std::string findDefaultNode(const char* file,
                                bool        debug=false);

    //! Import a scene node.
    bool readSceneNode(const char* file,
                       const char* node_path,
                       bool        debug=false);

};


} // namespace Fsr


#endif

// end of FuserSceneLoader.h


//
// Copyright 2019 DreamWorks Animation
//
