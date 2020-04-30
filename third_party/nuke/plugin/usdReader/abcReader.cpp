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

/// @file abcReader.cpp
///
/// @author Jonathan Egstad
///
/// @brief Nuke GeoReader plugin to load Alembic files (.abc) via fsrUsdIO plugin


#include "usdReader.h"

#ifdef DWA_INTERNAL_BUILD
#  include "../environmentDWA.hpp" // to configure default plugin paths
#endif


using namespace DD::Image;

namespace Fsr {


//------------------------------------------------------------
//------------------------------------------------------------
//
// Keep this extension list in sync with any TCL redirector
// files. ie for 'fooReader.tcl' you need the 'foo\0' entry
// below, otherwise Nuke will not recognize the extension
// properly.
//
static const char* abc_file_extensions =
    "abc\0";


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


/*! The only difference between this and usdReader is the default surface mask
    pattern.
*/
class abcReaderFormat : public usdReaderFormat
{
  public:
    static DD::Image::GeoReaderFormat* abcBuild(DD::Image::ReadGeo* geo) { return new abcReaderFormat(geo); }

    abcReaderFormat(ReadGeo* geo) :
        usdReaderFormat(geo)
    {
        k_surface_mask = defaultSurfaceMask(); // virtual calls don't work in parent-class constructors!
    }

    //! Alembic files are usually not huge in their node count, so default to importing everything.
    /*virtual*/ const char* defaultSurfaceMask() const { return "*"; }


    //================================================================
    // From FileHandler (FileOp.h):
    //================================================================

    /*virtual*/
    const char* help() { return "abcReader"; }

};


//-------------------------------------------------------------------------------


/*!
*/
class abcReader : public usdReader
{
  public:
    static const GeoDescription abcDescription;
    static GeoReader* abcReaderBuilder(ReadGeo*  op,
                                       int                  fd,
                                       const unsigned char* block,
                                       int                  length) { return new abcReader(op, fd); }

    /*!
    */
    abcReader(ReadGeo* geo, int fd) :
       usdReader(geo, fd)
    {
        //std::cout << "abcReader::ctor(" << this << "): reading Abc file '" << fileNameForReader() << "'" << std::endl;

#ifdef DWA_INTERNAL_BUILD
        // TODO: set this bool via a runtime variable that only gets set when in a show env
        const bool is_show_env = false;

        findAndRegisterDefaultUsdFolios(is_show_env);
#endif

        // TODO: This is not needed anymore. fuserIOClass() method is used instead along with the .tcl aliases.
        //Fsr::NodeIOInterface::addExtensionMappings("abc:UsdIO");
    }

}; // abcReader


//-------------------------------------------------------------------------


//! Return true if magic numbers match up.
static bool
testAbcFiles(int                  fd,
             const unsigned char* block,
             int                  length)
{
    // Several different header formulations:
    // abc ogawa:  4f 67 61 77 61 ff 00 01  |Ogawa....|
    // abc hdf:    89 48 44 46 0d 0a 1a 0a  |.HDF.....|
    return (strncmp((const char*)block, "Ogawa",    5)==0 ||
           (block[0] == 0x89 && strncmp((const char*)block+1, "HDF", 3)==0));
}


const GeoDescription abcReader::abcDescription(abc_file_extensions,
                                               abcReaderBuilder /*ctor*/,
                                               abcReaderFormat::abcBuild /*format ctor*/,
                                               testAbcFiles /*test method*/,
                                               NULL /*license*/,
                                               true /*needFd*/);

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

//
// Copyright 2019 DreamWorks Animation
//
