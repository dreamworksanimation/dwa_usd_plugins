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

/// @file Fuser/GeoSceneGraphReader.h
///
/// @author Jonathan Egstad

#ifndef Fuser_GeoSceneGraphReader_h
#define Fuser_GeoSceneGraphReader_h

#include "GeoReader.h"


namespace Fsr {


//--------------------------------------------------------------------------------------


/*! Adds scene graph specific controls.
*/
class FSR_EXPORT GeoSceneGraphReaderFormat : public FuserGeoReaderFormat
{
  public:
    //const char* k_object_path;              //!< Primary object path
    const char* k_surface_mask;             //!< Object & faceset mask

    // Scenegraph UI (TODO: move to its own knob class!)
    const char* k_scenegraph_scope;         //!< Path to start loading the scene at
    int         k_scenegraph_max_depth;     //!< Maximum node subdirs to reduce load times on large scenes

    bool        k_debug_archive;            //!< Show archive loading info


  public:
    GeoSceneGraphReaderFormat(DD::Image::ReadGeo* geo);

    //! Must have a virtual destructor!
    virtual ~GeoSceneGraphReaderFormat() {}


    //!
    virtual const char* defaultSurfaceMask() const { return ""; }

    //! Add knobs specific to scene graph reading.
    virtual void addSceneGraphKnobs(DD::Image::Knob_Callback f);


    //================================================================
    // From FileHandler (DD::Image::FileOp.h):
    //================================================================

    /*virtual*/ void knobs(DD::Image::Knob_Callback f);

    /*virtual*/ void extraKnobs(DD::Image::Knob_Callback f);

    //================================================================
    // From ReaderFormat (DD::Image::Reader.h):
    //================================================================

    /*virtual*/ void append(DD::Image::Hash& hash);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
class FSR_EXPORT GeoSceneGraphReader : public FuserGeoReader
{
  protected:
    DD::Image::Hash        m_reader_ui_hash;        //!< If this changes update scene graph
    DD::Image::Hash        m_scenegraph_ui_hash;    //!< Separate from reader ui hash as scenegraph can update randomly

    //std::vector<NodeRef>   m_objects;               //!<
    //std::set<unsigned>     m_enabled_objects;       //!<
    //DD::Image::Hash        m_objects_hash;          //!<


  protected:
    /*! A GeoSceneGraphReader subclass can implement this to return an
        archive context from customized storage.

        Base class returns a GeoSceneFileArchiveContext from the default
        static archive context map.
    */
    virtual GeoSceneFileArchiveContext* findArchiveContext(uint64_t hash);


    /*! Return a pointer to the implementation's statically-stored GeoSceneFileArchiveContext
        object, created by the createArchiveContext() method and assigned by the
        updateArchiveContext() method.

        Normally an implementation will define a subclass of GeoSceneFileArchiveContext
        containing any data that's specific to that implementation. Usually it would
        points to some static data that reused between geometry_engine() runs and
        possibly between multiple GeoReaders.

        GeoReaders are often deleted unexpectedly and rebuilt by the parent ReadGeo
        so this is usually handled by creating a unique hash for the archive context
        and having a static map to store the reusable data keyed to the hash.

        Must implement.
    */
    virtual GeoSceneFileArchiveContext* sceneFileArchiveContext() const=0;


    /*! Create a new GeoSceneFileArchiveContext to be associated with an archive
        context hash.
        This is called by GeoSceneGraphReader::_validate() if no previous context
        matching that hash was found.

        A GeoSceneGraphReader subclass must implement this method to create
        a GeoSceneFileArchiveContext or its own custom subclass.
        The hash value can be ignored if a custom subclass doesn't need it.

        ie 'return new MyCustomArchiveContextClass()' or just
        'return new GeoSceneFileArchiveContext()'.

        Must implement.
    */
    virtual GeoSceneFileArchiveContext* createArchiveContext(uint64_t hash)=0;


    /*! Store the archive context in the GeoSceneGraphReader subclass.
        Return false on type mismatch.
        
        This is called by GeoSceneGraphReader::_validate() after the context
        has been found or created.

        A GeoSceneGraphReader subclass must implement this method to locally
        save a GeoSceneFileArchiveContext or its own custom subclass.
        The hash value can be ignored if a custom subclass doesn't need it.

        It's best to dynamically test that the passed context matches the
        expected type before storing it! Return false to avoid a crash.

        Must implement.
    */
    virtual bool updateArchiveContext(GeoSceneFileArchiveContext* context,
                                      uint64_t                    hash)=0;



    /*! Add an archive context to a storage cache.

        A GeoSceneGraphReader subclass can implement this method to manage the
        storage itself.

        Base class adds it to the default static archive context map.
    */
    virtual void addArchiveContext(GeoSceneFileArchiveContext* context,
                                   uint64_t                    hash);


    //------------------------------------------------------------


    //! Append params that affect the file state - used to invalidate caches.
    /*virtual*/ void _getFileHash(DD::Image::Hash&);

    //! Return the global topology variance flags from the scene archive.
    /*virtual*/ uint32_t _getGlobalTopologyVariance();

    //! Append params that affect the geometry topology state - used to invalidate primitives.
    /*virtual*/ void _getTopologyHash(DD::Image::Hash&);

    //! Append params that affect the reader's UI state - used to cause _updateSceneGraph() to be called.
    /*virtual*/ void _getReaderUIHash(DD::Image::Hash&);

    /*! Call this to refresh a reader's custom knobs. Subclasses can implement
        this to make sure their knobs are kept up to date.
    */
    /*virtual*/ void _updateReaderUI();


    //! Fill the scenegraph knob.
    virtual void _updateSceneGraph();

    //!
    virtual void _updateSelectedObjectsMenu(const std::set<std::string>& node_paths);


    //------------------------------------------------------------


    /*! Get a list of node descriptions from the scene file starting at
        a specific path level.

        The base class calls the FuserIO plugin with the Arg::Scene::node_type_contents
        directive and a SceneNodeDescriptions object as a target.
    */
    virtual bool _getNodeDescriptions(const char*              file,
                                      const char*              start_path_at,
                                      uint32_t                 max_depth,
                                      Fsr::NodeDescriptionMap& node_descriptions,
                                      bool                     debug=false);


    /*! Fill in the selected paths from the node filter args.
        Returns false on user-abort.

        Base class implementation constructs 
    */
    virtual void _getSelectedNodePaths(const Fsr::NodeFilterPatternList& node_filter_patterns,
                                       const DD::Image::Hash&            node_filter_hash,
                                       Fsr::NodePathSelections&          selected_paths,
                                       DD::Image::Hash*                  selected_paths_hash);


    //------------------------------------------------------------


    //! Add/remove a node path to/from the mask list. Returns true if the knob was changed.
    bool editSurfaceMaskKnob(const std::string& path,
                             const std::string& type,
                             bool               remove_mode=false);


    //------------------------------------------------------------


    //! Add or modify args to pass to FuserNodePrimitive ctors.
    /*virtual*/ void _appendNodeContextArgs(Fsr::NodeContext& node_ctx);


    /*! Open a scene file in preparation for reading data.
        Returns fast if open has already happened (checks state hashes.)
        Returns false on user-abort.

        For readers that are monolithic 'archives' like Alembic & USD (many frames
        of data stored in a single 'file' instance,) we can save tons of overhead
        by only opening the object once and reusing it as we extract nodes.
        USD in particular can be composed of many, many individual files and
        reading/parsing them can be time consuming.

        The GeoSceneFileArchiveContext object stores the identifier used to find the
        correct object cache during multiple node instanatiations and must be
        created by the implementation and returned by sceneFileArchiveContext().

        Normally an implementation will define a subclass of GeoSceneFileArchiveContext
        containing any data that's specific to that implementation.

        The default implementation calls the FuserIO plugin with the Arg::Scene::file_archive_open
        directive and the GeoSceneFileArchiveContext as a target.
    */
    /*virtual*/ bool _openSceneFile();


  public:
    //!
    GeoSceneGraphReader(DD::Image::ReadGeo* geo,
                        int                 fd);

    //! Must have a virtual destructor!
    virtual ~GeoSceneGraphReader();


    /*! Handle the acquisition or re-acquisition of a scene file archive cache.

        This can be tricky as the GeoReader is often destroyed and re-allocated by
        the parent ReadGeo but GeoOp geometry rebuild flags are not changed making
        it difficult to easily know what we need to do, and we don't want to be
        forced to always reload the prims.

        This method calculates a hash from several sources like the file name and
        object masks to come with the 'archive hash' which is passed to a subclass'
        findArchiveContext() method to return a GeoSceneFileArchiveContext*.
        If one does not yet exist then it's created via createArchiveContext(),
        otherwise the subclass' locally-stored pointer is updated via
        updateArchiveContext().

        This is primarily called by GeoSceneGraphReader::_validate() and during
        UI updates when the scenegraph & selections knobs need filling.
    */
    GeoSceneFileArchiveContext* acquireSceneFileArchiveContext();


    /*! Get the list of object names(paths) to read in during geometry_engine.
        Returns the current archive context's 'selected_node_paths.objects' string set.
    */
    /*virtual*/ const std::set<std::string>& getObjectPathsForReader();

    /*! Get the list of material names(paths) to read in during geometry_engine.
        Returns the current archive context's 'selected_node_paths.materials' string set.
    */
    /*virtual*/ const std::set<std::string>& getMaterialPathsForReader();

    /*! Get the list of light names(paths) to read in during geometry_engine.
        Returns the current archive context's 'selected_node_paths.lights' string set.
    */
    /*virtual*/ const std::set<std::string>& getLightPathsForReader();


    /*! Build a list of mask patterns from a arbitrary mask string.
        Base class implementation splits patterns at whitespace
        separators.
    */
    virtual void buildNodeMasks(const char*               surface_mask,
                                std::vector<std::string>& mask_patterns,
                                DD::Image::Hash*          mask_hash);


  public:
    //------------------------------------------------------------
    // DD::Image::GeoReader virtual methods.

    /*virtual*/ void append(DD::Image::Hash& hash);

    /*virtual*/ void get_geometry_hash(DD::Image::Hash* geo_hashes);

    /*virtual*/ void _validate(const bool for_real);

    /*virtual*/ int  knob_changed(DD::Image::Knob*);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

#endif

// end of Fuser/GeoSceneGraphReader.h

//
// Copyright 2019 DreamWorks Animation
//
