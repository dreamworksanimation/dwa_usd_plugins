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
    void knobs(DD::Image::Knob_Callback f) 
    {
        GeoSceneGraphReaderFormat::knobs(f);
    }

    /*virtual*/
    void extraKnobs(DD::Image::Knob_Callback f)
    {
        GeoSceneGraphReaderFormat::extraKnobs(f);
    }

    //================================================================
    // From ReaderFormat (Reader.h):
    //================================================================

    /*virtual*/
    void append(DD::Image::Hash& hash)
    {
        GeoSceneGraphReaderFormat::append(hash);
    }

};


//-------------------------------------------------------------------------------


/*!
*/
class usdReader : public GeoSceneGraphReader
{
  protected:
    UsdArchiveContext*  m_stage_cache_ctx;  //!< Contains the stage cache id value


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

}; // usdReader


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

//
// Copyright 2019 DreamWorks Animation
//
