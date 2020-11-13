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

/// @file usdReader.h
///
/// @author Jonathan Egstad
///
/// @brief Nuke GeoReader plugin to load USD files (.usd*) via fsrUsdIO plugin


#include <Fuser/GeoSceneGraphReader.h>


namespace Fsr {


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! Subclass the Fuser archive context class to store USD-specific info.

    This is passed to the fsrUsdIO FuserNode plugin via its execute() portal.
*/
class UsdArchiveContext : public GeoSceneFileArchiveContext
{
  public:
    UsdArchiveContext() : GeoSceneFileArchiveContext() { }
};


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*!
*/
class usdReaderFormat : public GeoSceneGraphReaderFormat
{
  public:
    bool        k_show_inactive_prims;      //!< Inactive prims will show up in scenegraph
    bool        k_enable_inactive_prims;    //!< Allow the enabling of prims via masks
    const char* k_inactive_mask;            //!< Which inactive prims to enable


  public:
    static DD::Image::GeoReaderFormat* usdBuild(DD::Image::ReadGeo* geo) { return new usdReaderFormat(geo); }

    //!
    usdReaderFormat(DD::Image::ReadGeo* geo);

    //! USD files can take huge amounts of time to scan, let's avoid this by default.
    /*virtual*/ const char* defaultSurfaceMask() const { return ""; }


    //================================================================
    // From FileHandler (FileOp.h):
    //================================================================

    /*virtual*/
    const char* help() { return "usdReader"; }

    /*virtual*/
    void knobs(DD::Image::Knob_Callback f);

    /*virtual*/
    void extraKnobs(DD::Image::Knob_Callback f);

    //================================================================
    // From GeoSceneGraphReaderFormat:
    //================================================================

    /*virtual*/
    void addObjectSelectionKnobs(DD::Image::Knob_Callback f);

    /*virtual*/
    void addImportOptionsKnobs(DD::Image::Knob_Callback f);

    /*virtual*/
    void addSceneGraphKnobs(DD::Image::Knob_Callback f);

    //================================================================
    // From ReaderFormat (Reader.h):
    //================================================================

    /*virtual*/
    void append(DD::Image::Hash& hash);

};


//-------------------------------------------------------------------------------


/*!
*/
class usdReader : public GeoSceneGraphReader
{
  protected:
    UsdArchiveContext*  m_stage_cache_ctx;  //!< Contains the stage cache id value

    //!
    void appendInactivePrimControls(DD::Image::Hash&);


  public:
    static const DD::Image::GeoDescription usdDescription;
    static DD::Image::GeoReader* buildUsdReader(DD::Image::ReadGeo*  op,
                                                int                  fd,
                                                const unsigned char* block,
                                                int                  length);

    //!
    usdReader(DD::Image::ReadGeo* geo, int fd);


    /*! Return the class(plugin) name of fuser IO node to load.
        This, in conjunction with the 'usdaReader.tcl' and 'usdcReader.tcl' alias files
        direct the Fuser plugin finder to the correct plugin filename 'fsrUsdIO' to
        load (the leading 'fsr' is added by the Fsr::Node plugin code.)
    */
    /*virtual*/ const char* fuserIOClass() const { return "UsdIO"; }


    //! Create a new GeoSceneFileArchiveContext to be associated with an archive context hash.
    /*virtual*/ GeoSceneFileArchiveContext* createArchiveContext(uint64_t hash) { return new UsdArchiveContext(); }

    //! Store the archive context in the GeoSceneGraphReader subclass. Return false on type mismatch.
    /*virtual*/ bool updateArchiveContext(GeoSceneFileArchiveContext* context,
                                          uint64_t                    hash);

    //! Return a pointer to the implementation's GeoSceneFileArchiveContext object.
    /*virtual*/ GeoSceneFileArchiveContext* sceneFileArchiveContext() const { return m_stage_cache_ctx; }

    /*virtual*/ void _getFileHash(DD::Image::Hash&);
    /*virtual*/ void _getReaderUIHash(DD::Image::Hash&);

    /*virtual*/ void get_geometry_hash(DD::Image::Hash* geo_hashes);

    //! Add or modify args to pass to node or execution contexts.
    /*virtual*/ void _appendNodeContextArgs(ArgSet& node_args);
    /*virtual*/ void _appendExecuteContextArgs(const Fsr::ArgSet& node_args,
                                               Fsr::NodeContext&  exec_ctx);

    //! Knob changed callbacks need to be handled in the GeoReader, not the GeoReaderFormat.
    /*virtual*/
    int knob_changed(DD::Image::Knob*);


}; // usdReader


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

//
// Copyright 2019 DreamWorks Animation
//
