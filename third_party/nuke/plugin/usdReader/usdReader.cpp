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
    k_surface_mask          = defaultSurfaceMask(); // virtual calls don't work in parent-class constructors!
    k_attribute_mappings    = default_usd_attribute_mappings;
    k_show_inactive_prims   = false;
    k_enable_inactive_prims = false;
    k_inactive_mask         = "*";
}


/*virtual*/
void
usdReaderFormat::knobs(DD::Image::Knob_Callback f) 
{
    GeoSceneGraphReaderFormat::knobs(f);
}


/*!
*/
/*virtual*/ void
usdReaderFormat::addObjectSelectionKnobs(DD::Image::Knob_Callback f)
{
    GeoSceneGraphReaderFormat::addObjectSelectionKnobs(f);
}


/*virtual*/
void
usdReaderFormat::addImportOptionsKnobs(DD::Image::Knob_Callback f)
{
    GeoSceneGraphReaderFormat::addImportOptionsKnobs(f);
}


/*!
*/
/*virtual*/ void
usdReaderFormat::addSceneGraphKnobs(DD::Image::Knob_Callback f)
{
    GeoSceneGraphReaderFormat::addSceneGraphKnobs(f);

    DD::Image::Divider(f, "Inactive Prims");
    Bool_knob(f, &k_show_inactive_prims, "show_inactive_prims", "show inactive prims");
        Tooltip(f, "If enabled, inactive prims show up in the scenegraph noted as 'INACTIVE'"
                   "and none of its children are made visible.\n"
                   "If the prim is enabled via the 'inactive mask' below then the prim and "
                   "its children will appear in the scenegraph no longer noted as inactive.");
    Bool_knob(f, &k_enable_inactive_prims, "enable_inactive_prims", "enable inactive prims");
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, "If enabled, inactive prims matching the 'inactive mask' below are made "
                   "active and loadable.\n"
                   "Change the make patterns to enable specific prim paths using the same "
                   "expression matching as 'surface mask'.");
    Newline(f);
    Multiline_String_knob(f, &k_inactive_mask, "inactive_mask", "inactive mask", 2/*lines*/);
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f,  "Enable inactive prims paths using the same expression matching as 'surface mask'.\n"
                    "\n"
                    "Patterns to match inactive prim names using standard glob-style "
                    "wildcards '*', '?'.  There can multiple mask patterns separated by spaces.\n"
                    "Turn off objects by preceding the pattern with '-' or '^'. Priority order "
                    "is left to right so if an object is turned off by one mask it can be turned "
                    "on again by an additional mask to the right.\n"
                    "\n"
                    "Examples:\n"
                    " <b>* ^*.ref*</b>  Select all but turn off ones with '.ref'.\n"
                    " <b>*skin_0/m_skin ^*.ref*</b>  Only select the skin mesh.\n"
                    "");
}


/*virtual*/
void
usdReaderFormat::extraKnobs(DD::Image::Knob_Callback f)
{
    GeoSceneGraphReaderFormat::extraKnobs(f);
}


/*virtual*/
void
usdReaderFormat::append(DD::Image::Hash& hash)
{
    hash.append(k_show_inactive_prims);
    hash.append(k_enable_inactive_prims);
    hash.append(k_inactive_mask);

    GeoSceneGraphReaderFormat::append(hash);
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


/*! Add controls that affect the set of inactive prims.
    This is used several times to make sure the scene file is reloaded,
    the gui refreshed, and the geometry selection updated.
*/
void
usdReader::appendInactivePrimControls(DD::Image::Hash& hash)
{
    const usdReaderFormat* options = dynamic_cast<usdReaderFormat*>(geo->handler());
    if (options)
    {
        hash.append(options->k_show_inactive_prims);
        hash.append(options->k_enable_inactive_prims);
        hash.append(options->k_inactive_mask);
    }
}


/*! Make sure inactive prim selection changes force a stage refresh.
*/
void
usdReader::_getFileHash(DD::Image::Hash& hash)
{
    appendInactivePrimControls(hash);
    GeoSceneGraphReader::_getFileHash(hash);
}


/*! Make sure inactive prim selection changes force a GUI refresh.
*/
void
usdReader::_getReaderUIHash(DD::Image::Hash& hash)
{
    appendInactivePrimControls(hash);
    GeoSceneGraphReader::_getReaderUIHash(hash);
}


/*! Make sure inactive prim selection changes force a geo reload.
*/
/*virtual*/ void
usdReader::get_geometry_hash(DD::Image::Hash* geo_hashes)
{
    GeoSceneGraphReader::get_geometry_hash(geo_hashes);
    appendInactivePrimControls(geo_hashes[Group_Object]);
}


/*virtual*/ void
usdReader::_appendNodeContextArgs(ArgSet& node_args)
{
    const usdReaderFormat* options = dynamic_cast<usdReaderFormat*>(geo->handler());
    if (options)
    {
        // Add USD-specific knob options to pass to fsrUsdIO plugin:
        //if (options->k_show_inactive_prims)
        //    node_args.setBool("UsdIO:show_inactive_prims", true);
    }
    GeoSceneGraphReader::_appendNodeContextArgs(node_args);
}


/*virtual*/ void
usdReader::_appendExecuteContextArgs(const Fsr::ArgSet& node_args,
                                     Fsr::NodeContext&  exec_ctx)
{
    const usdReaderFormat* options = dynamic_cast<usdReaderFormat*>(geo->handler());
    if (options)
    {
        // Add USD-specific knob options to pass to FuserUsdArchiveIO execute method:
        if (options->k_show_inactive_prims)
            exec_ctx.setBool("UsdIO:show_inactive_prims", true);
        if (options->k_enable_inactive_prims &&
            options->k_inactive_mask && options->k_inactive_mask[0])
        {
            exec_ctx.setString("UsdIO:inactive_mask", options->k_inactive_mask);
        }
    }
    GeoSceneGraphReader::_appendExecuteContextArgs(node_args, exec_ctx);
}


/*! Knob changed callbacks need to be handled in the GeoReader, not the GeoReaderFormat.
*/
/*virtual*/ int
usdReader::knob_changed(DD::Image::Knob* k)
{
    Knob* kEnable = geo->knob("enable_inactive_prims");
    Knob* kMask   = geo->knob("inactive_mask");
    if ((kEnable && kMask) && (k == kEnable || kMask->isVisible()))
    {
        kMask->enable(kEnable->get_value() > 0.5);
        if (k == kEnable)
            return 1;
    }

    return GeoSceneGraphReader::knob_changed(k);
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
