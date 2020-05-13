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
/// @brief Nuke GeoReader plugin to load USD files (.usd*) via fsrUsdIO plugin


#include "usdReader.h"


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

static const char* default_usd_attribute_mappings =
    "st=uv, st_0=uv, uv=uv\n"
    "normals=N\n"
    "displayColor=Cf\n"
    "displayOpacity=Of\n"
    "velocities=VEL\n"
;


/*!
*/
usdReaderFormat::usdReaderFormat(DD::Image::ReadGeo* geo) :
    GeoSceneGraphReaderFormat(geo)
{
    k_surface_mask = defaultSurfaceMask(); // virtual calls don't work in parent-class constructors!

    k_attribute_mappings = default_usd_attribute_mappings;
}


//-------------------------------------------------------------------------------


/*!
*/
usdReader::usdReader(ReadGeo* geo, int fd) :
    GeoSceneGraphReader(geo, fd),
    m_stage_cache_ctx(NULL)
{
    //std::cout << "usdReader::ctor(" << this << "): reading USD file '" << fileNameForReader() << "'" << std::endl;

    // TODO: This is not needed anymore. fuserIOClass() method is used instead along with the .tcl aliases.
    //Fsr::NodeIOInterface::addExtensionMappings("usd,usda,usdc:UsdIO");
}


//! Store the archive context in the GeoSceneGraphReader subclass. Return false on type mismatch.
/*virtual*/ bool
usdReader::updateArchiveContext(GeoSceneFileArchiveContext* context,
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


//-------------------------------------------------------------------------


/*static*/ GeoReader*
usdReader::buildUsdReader(ReadGeo*  op,
                          int                  fd,
                          const unsigned char* block,
                          int                  length)
{
    return new usdReader(op, fd);
}



//! Return true if magic numbers match up.
static bool
testUsdFiles(int                  fd,
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

const GeoDescription usdReader::usdDescription(usd_file_extensions,
                                               buildUsdReader /*ctor*/,
                                               usdReaderFormat::usdBuild /*format ctor*/,
                                               testUsdFiles /*test method*/,
                                               NULL /*license*/,
                                               true /*needFd*/);

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

//
// Copyright 2019 DreamWorks Animation
//
