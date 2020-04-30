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

/// @file Fuser/GeoReader.h
///
/// @author Jonathan Egstad

#ifndef Fuser_GeoReader_h
#define Fuser_GeoReader_h

#include "NodeIOInterface.h"
#include "NodePrimitive.h"
#include "GeoSceneFileArchiveContext.h"

#include <DDImage/ddImageVersion.h>
#if kDDImageVersionMajorNum >= 9
//#if kDDImageVersionInteger > 90000
# undef VERSION
#endif
#include <DDImage/GeoReader.h>
#include <DDImage/GeoReaderDescription.h>

#include <map> // for multimap


namespace Fsr {

//--------------------------------------------------------------------------------------

// We use namespaces to echo the namespacing in the attribute names:
namespace NukeGeo {

// The standard attrib names Nuke uses for some important GeoInfo attribs:
const char* const uvs_attrib_name       = DD::Image::kUVAttrName;       // 'uvs'
const char* const normals_attrib_name   = DD::Image::kNormalAttrName;   // 'N'
const char* const colors_attrib_name    = DD::Image::kColorAttrName;    // 'Cf'
const char* const opacities_attrib_name = "Of";
const char* const velocity_attrib_name  = DD::Image::kVelocityAttrName; // 'vel'

}


//! Knob -> Fsr::NodePrimitive attribute mappings:
struct FSR_EXPORT KnobMap
{
    const char* readerKnob;
    const char* fuserPrimAttrib;
};


//--------------------------------------------------------------------------------------


/*!
    Fsr::FuserGeoReaderFormat may be a little redundant but it's easier to keep straight.
*/
class FSR_EXPORT FuserGeoReaderFormat : public DD::Image::GeoReaderFormat
{
  public:
    bool        k_ignore_unrenderable;      //!<
    bool        k_translate_render_parts;   //!< Translate render-part enums to UDIM offsets TODO: DEPRECATE! 
    const char* k_attribute_mappings;       //!< Map file attrib names to nuke attrib names
    //
    bool        k_read_on_each_frame;       //!< 
    bool        k_sub_frame;                //!< 
    double      k_velocity_scale;           //!< Scale the velocity channels - can also be used to invert them.
    double      k_set_frame;                //!< Manually set the frame number
    double      k_frames_per_second;        //!< Change the speed of the incoming geometry
    //
    int         k_points_mode;              //!< Translate points to what primitive type
    //
    int         k_subd_import_level;        //!< Subd level for importing meshes
    int         k_subd_render_level;        //!< Subd level for rendering
    bool        k_subd_snap_to_limit;       //!< Snap to limit surface after subdivision
    bool        k_subd_force_enable;        //!< Enable subd mode on all meshes
    int         k_subd_tessellator;         //!< Tessellator scheme to use
    //
    bool        k_use_colors;               //!< Copy color attribute to the vertex color
    bool        k_color_facesets;           //!< Color the vertices in a faceset with random colors
    bool        k_color_objects;            //!< Color the separate meshes with random colors
    //
    bool        k_apply_matrix;             //!<
    int         k_prim_creation_mode;       //!< What types of prims to export?
    int         k_proxy_lod_mode;           //!< In deferred-mode how to display geometry
    //
    bool        k_debug;
    bool        k_debug_attribs;


  protected:
    //!
    virtual void addTimeOptionsKnobs(DD::Image::Knob_Callback);

    //!
    virtual void addImportOptionsKnobs(DD::Image::Knob_Callback);

    //!
    virtual void addPrimOptionsKnobs(DD::Image::Knob_Callback);


  public:
    FuserGeoReaderFormat(DD::Image::ReadGeo* geo);

    //! Must have a virtual destructor!
    virtual ~FuserGeoReaderFormat() {}


    //================================================================
    // From FileHandler (DD::Image::FileOp.h):
    //================================================================

    /*virtual*/ void knobs(DD::Image::Knob_Callback);

    /*virtual*/ void extraKnobs(DD::Image::Knob_Callback);

    /*virtual*/ int  knob_changed(DD::Image::Knob*);

    //================================================================
    // From ReaderFormat (DD::Image::Reader.h):
    //================================================================

    /*virtual*/ void append(DD::Image::Hash& hash);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!

    Fsr::FuserGeoReader may be a little redundant but it's easier to keep straight.

    TODO: support facesets in object selection parser.

*/
class FSR_EXPORT FuserGeoReader : public DD::Image::GeoReader,
                                  public Fsr::NodeIOInterface
{
  protected:
    std::string            m_filename_for_reader;   //!< Trimmed filename (no leading extension)
    DD::Image::Hash        m_file_hash;             //!< If this changes reload the scene file, update UI


  protected:
    // Call by the base class ctor to allow the subclass to get ready to open a geometry file.
    //virtual void _initializeForLoad() {}


    /*! Subclasses can append params that affect the file state - invalidates file cache.
        Base class does nothing.
    */
    virtual void _getFileHash(DD::Image::Hash&) { }


    /*! Return the global topology variance flags.
        Subclasses can implement this to return a custom set.
    */
    virtual uint32_t _getGlobalTopologyVariance() { return Fsr::Node::ConstantTopology; }


    /*! Subclasses can append params that affect the geometry topology state - invalidates primitives.
        Base class does nothing (assumes only point locations are changing frame to frame.)
    */
    virtual void _getTopologyHash(DD::Image::Hash&) { }


    /*! Subclasses can append params that affect the reader's UI state.
        Base class does nothing.
    */
    virtual void _getReaderUIHash(DD::Image::Hash&) { }


    /*! Subclasses can implement this to make sure their knobs are kept up to date.
        Only gets called if in GUI mode.
        Base class does nothing.
    */
    virtual void _updateReaderUI() { }


    /*! Subclasses can implement this to customize the opening of a scene file archive
        in preparation for reading data.
        Returns false on user-abort.
        Base class does nothing.
    */
    virtual bool _openSceneFile() { return true;/*no user-abort*/ }


  public:
    //!
    FuserGeoReader(DD::Image::ReadGeo* geo,
                   int                 fd);

    //! Must have a virtual destructor!
    virtual ~FuserGeoReader();

    /*! Return the class(plugin) name of Fuser IO node to load, ie. 'UsdIO' or 'AbcIO'.

        This is used instead of Fsr::NodeIOInterface::_buildFilePathAndPluginType() to build the
        plugin name as DD::Image already trims the filename for us and the subclass implementation
        knows the IO plugin name.

        If the subclass *doesn't* know the plugin name it can always use
        Fsr::NodeIOInterface::_buildFilePathAndPluginType() to build it, but may need to pass in the
        raw filename string from the knob.
    */
    virtual const char* fuserIOClass() const=0;


    /*! Return the trimmed file path without any leading extension tokens.

        For whatever reason geo->filename() nor geo->fname() do not reliably
        return the trimmed name, so we're storing it locally.
    */
    virtual const char* filePathForReader();

    //! Return the name of the file being read without the leading path.
    std::string         fileNameForReader() { return fileNameFromPath(filePathForReader()); }


    /*! Return a hash indicating the file state.
        Base class appends raw file name and 'version' knob.
        Subclasses can change this to cause file invalidation and reloading.
    */
    DD::Image::Hash getFileHash();

    //! Return the global topology variance flags.
    uint32_t        getGlobalTopologyVariance();

    /*! Return a hash indicating the geometry topology state.
        Subclasses can change this to cause the primitive topology to be invalidated.
    */
    DD::Image::Hash getTopologyHash();

    /*! Return a hash indicating the reader's UI state.

        Subclasses should include values that may need to cause UI updates
        then check this resulting hash in knob_changed() to know if something
        needs refreshing.
    */
    DD::Image::Hash getReaderUIHash();


    /*! Call this to refresh a reader's custom knobs.
        Only does something if Nuke is in GUI mode.

        Readers can have complex UIs with scenegraphs, attribute lists, etc
        and due to the awkward state updating of GeoReaders (GeoOp's _validate()
        is often used to execute geometry_engine()) sometimes we need to force
        the UI knobs to be updated outside the normal Nuke Op update sequence.
    */
    void updateReaderUI();



    /*! Open a scene file in preparation for reading data.
        Returns false on user-abort.

        For readers that are monolithic 'archives' like Alembic & USD (many frames
        of data stored in a single 'file' instance,) we can save tons of overhead
        by only opening the object once and reusing it as we extract nodes.

        The GeoSceneFileArchiveContext object stores the identifier used to find the
        correct object cache during multiple node instanatiations and must be
        created by the implementation and returned by sceneFileArchiveContext().

        Normally an implementation will define a subclass of GeoSceneFileArchiveContext
        containing any data that's specific to that implementation.

        This is called from FuserGeoReader::geometry_engine().
    */
    bool openSceneFile();


    //! Get the list of object names(paths) to read in during geometry_engine. Base class returns an empty set.
    virtual const std::set<std::string>& getObjectPathsForReader();

    //! Get the list of material names(paths) to read in during geometry_engine. Base class returns an empty set.
    virtual const std::set<std::string>& getMaterialPathsForReader();

    //! Get the list of light names(paths) to read in during geometry_engine. Base class returns an empty set.
    virtual const std::set<std::string>& getLightPathsForReader();


    //! Thread-safe object loader entry point called by a ThreadedGeometryEngine instance.
    bool readObject(const std::string&               path,
                    Fsr::NodeContext&                node_ctx,
                    Fsr::GeoOpGeometryEngineContext& geo_ctx);


    //! Thread-safe object loader entry point called by a ThreadedGeometryEngine instance.
    bool readMaterial(const std::string&               path,
                      Fsr::NodeContext&                node_ctx,
                      Fsr::GeoOpGeometryEngineContext& geo_ctx);


  public:
    //! Extract the from-to attribute name mappings from a text entry. Optional to-from mappings at same time.
    static void     buildAttributeMappings(const char*            txt,
                                           Fsr::KeyValueMap&      file_to_nuke_map,
                                           Fsr::KeyValueMultiMap& nuke_to_file_map);

    //! If the file_attrib exists in the attrib_map return the nuke attrib mapped name
    std::string     getFileToNukeAttribMapping(const char*             file_attrib,
                                               const Fsr::KeyValueMap& file_to_nuke_map);

    //! Map a nuke attrib name to possible multiple file attrib names.
    static uint32_t getNukeToFileAttribMappings(const char*                  nuke_attrib,
                                                const Fsr::KeyValueMultiMap& nuke_to_file_map,
                                                std::vector<std::string>&    mappings);





  public:
    //------------------------------------------------------------
    // DD::Image::GeoReader virtual methods.

    //! Enable the axis knob or knot for the current plugin
    /*virtual*/ bool enableAxisKnob();

    /*virtual*/ int  knob_changed(DD::Image::Knob* k);

    /*virtual*/ DD::Image::Iop* default_material_iop() const;

    /*virtual*/ void append(DD::Image::Hash& hash);

    /*virtual*/ void get_geometry_hash(DD::Image::Hash* geo_hashes);

    /*virtual*/ void _validate(const bool for_real);

    //! afaict this never gets called by ReadGeo.
    /*virtual*/ void _open();

    /*virtual*/ void geometry_engine(DD::Image::Scene&        scene,
                                     DD::Image::GeometryList& out);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

#endif

// end of Fuser/GeoReader.h

//
// Copyright 2019 DreamWorks Animation
//
