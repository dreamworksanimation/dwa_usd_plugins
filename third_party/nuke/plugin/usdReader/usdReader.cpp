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

/// @file usdReader.cpp
///
/// @author Jonathan Egstad
///
/// @brief Nuke GeoReader plugin to load USD files (.usd) via fsrUsdIO plugin


#include <Fuser/GeoSceneGraphReader.h>


using namespace DD::Image;

namespace Fsr {


//------------------------------------------------------------
//------------------------------------------------------------
//
// Keep this extension list in sync with the TCL redirector
// files. ie for 'usdzReader.tcl' you need the 'usdz\0' entry
// below, otherwise Nuke will not recognize the extension
// properly.
//
static const char* usd_file_extensions =
    "usd\0"
    "usda\0"
    "usdc\0"
    "usdz\0";


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
    static GeoReaderFormat* build(ReadGeo* geo) { return new usdReaderFormat(geo); }

    usdReaderFormat(ReadGeo* geo) :
        GeoSceneGraphReaderFormat(geo)
    {
        //
    }

    //! USD files can take huge amounts of time to scan, let's avoid this by default.
    /*virtual*/ const char* defaultSurfaceMask() const { return ""; }


    //================================================================
    // From FileHandler (FileOp.h):
    //================================================================

    /*virtual*/
    const char* help() { return "usdReader"; }

    /*virtual*/
    void knobs(Knob_Callback f) 
    {
        GeoSceneGraphReaderFormat::knobs(f);
    }

    /*virtual*/
    void extraKnobs(Knob_Callback f)
    {
        GeoSceneGraphReaderFormat::extraKnobs(f);
    }

    //================================================================
    // From ReaderFormat (Reader.h):
    //================================================================

    /*virtual*/
    void append(Hash& hash)
    {
        GeoSceneGraphReaderFormat::append(hash);
    }

};


//-------------------------------------------------------------------------------


/*!
*/
class usdReader : public GeoSceneGraphReader
{
    UsdArchiveContext*  m_stage_cache_ctx;  //!< Contains the stage cache id value


  public:
    static const GeoDescription description;
    static GeoReader* build(ReadGeo*  op,
                            int                  fd,
                            const unsigned char* block,
                            int                  length) { return new usdReader(op, fd); }

    /*!
    */
    usdReader(ReadGeo* geo, int fd) :
       GeoSceneGraphReader(geo, fd),
       m_stage_cache_ctx(NULL)
    {
        //std::cout << "usdReader::ctor(" << this << "): reading USD file '" << fileNameForReader() << "'" << std::endl;

        // TODO: This is not needed anymore. fuserIOClass() method is used instead along with the .tcl aliases.
        //Fsr::NodeIOInterface::addExtensionMappings("usd,usda,usdc:UsdIO");
    }


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
                                          uint64_t                    hash)
    {
        //std::cout << "      usdReader::updateArchiveContext(" << this << ")";
        //std::cout << " m_stage_cache_ctx=" << m_stage_cache_ctx << ", archive_ctx=" << context;
        //std::cout << std::endl;

        if (context == m_stage_cache_ctx)
            return true; // already up to date
        UsdArchiveContext* ctx = dynamic_cast<UsdArchiveContext*>(context);
        if (!ctx)
            return false; // shouldn't happen...don't crash
        m_stage_cache_ctx = ctx;
        return true;
    }

    //! Return a pointer to the implementation's GeoSceneFileArchiveContext object.
    /*virtual*/ GeoSceneFileArchiveContext* sceneFileArchiveContext() const { return m_stage_cache_ctx; }

}; // usdReader


//-------------------------------------------------------------------------


//! Return true if magic numbers match up.
static bool
test(int                  fd,
     const unsigned char* block,
     int                  length)
{
    // Several different header formulations:
    // usda:       23 75 73 64 61 20 31 2e  |#usda 1.|
    // usdc:       50 58 52 2d 55 53 44 43  |PXR-USDC|
    // usdz (zip): 50 4b 03 04 0a 00 00 00  |PK......|
    return (strncmp((const char*)block, "#usda",    5)==0 ||
            strncmp((const char*)block, "PXR-USDC", 8)==0 ||
            (block[0] == 0x50 && block[1] == 0x4b && block[2] == 0x03 && block[3] == 0x04));
}

const GeoDescription usdReader::description(usd_file_extensions,
                                            build /*ctor*/,
                                            usdReaderFormat::build /*format ctor*/,
                                            test /*test method*/,
                                            NULL /*license*/,
                                            true /*needFd*/);

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

//
// Copyright 2019 DreamWorks Animation
//
