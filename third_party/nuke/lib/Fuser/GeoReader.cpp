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

/// @file Fuser/GeoReader.cpp
///
/// @author Jonathan Egstad

#include "GeoReader.h"

#include "RayContext.h"

#include <DDImage/gl.h>
#include <DDImage/PrimitiveContext.h>
#include <DDImage/Thread.h> // for Lock
#include <DDImage/plugins.h>
#include <DDImage/Application.h>
#include <DDImage/Knob.h>
#include <DDImage/Scene.h>
#include <DDImage/Enumeration_KnobI.h>


//#define TRY_PRIMITIVE_PICKING 1


using namespace DD::Image;

namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

static const char* default_attribute_mappings =
   "color=Cf Cd=Cf\n"
   "UV=uv\n"
#ifdef DWA_INTERNAL_BUILD
   "pscale=size\n"
   "render_part_dwa_mm_part_enum=\n"
   "subd::lo=subd_lo  subd::hi=subd_hi  subd::display=subd_display\n"
#endif
;

// This needs to stick around after the reader is deleted:
static Fsr::StringSet attrib_const_strings;

enum { SUBD_OFF, SUBD_LO, SUBD_HI, SUBD_DISPLAY, SUBD_1, SUBD_2, SUBD_3, SUBD_4, SUBD_5 };
const char* const subd_levels[] = { "off", "subd_lo", "subd_hi", "subd_display", "1", "2", "3", "4", 0 };

enum { SUBD_TESSELLATOR_OPENSUBDIV, SUBD_TESSELLATOR_SIMPLESUBDIV };
const char* const subd_tessellators[] = { "OpenSubdiv", "SimpleSubdiv", 0 };

enum { POINTS_ARE_POINTCLOUD_SPHERES, POINTS_ARE_POINTCLOUD_DISCS, POINTS_ARE_PARTICLES };
const char* const points_modes[] = { "pointclouds-spheres", "pointcloud-discs", "particles", 0 };

#define SUBD_KNOB_HELP \
    "<ul>" \
    "<li><i>off</i> - No subdivision (level 0)</li>" \
    "<li><i>subd_lo</i> - Use value of the 'subd_lo' attribute (typically level 1.)  If attribute is " \
    "missing then this defaults to level 1.</li>" \
    "<li><i>subd_hi</i> - Use value of the 'subd_hi' attribute (typically level 2.)  If attribute is " \
    "missing then this defaults to level 2.</li>" \
    "<li><i>subd_display</i> - Use value of the 'subd_display' attribute - this is typically a high value, " \
    "but it can also be a low value.  If attribute is missing then this defaults to level 3.</li>" \
    "<li><i>1</i> - level 1</li>" \
    "<li><i>2</i> - level 2</li>" \
    "<li><i>3</i> - level 3</li>" \
    "<li><i>4</i> - level 4</li>" \
    "<li><i>5</i> - level 5</li>"


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

enum
{
    SURFACE_MASK_KNOB,
    ATTRIBUTE_MAPPINGS_KNOB,
    VELOCITY_SCALE_KNOB,
    //
    FRAME_OFFSET_KNOB,
    FRAME_ORIGIN_KNOB,
    FPS_KNOB,
    //
    SUBD_IMPORT_LEVEL_KNOB,
    SUBD_RENDER_LEVEL_KNOB,
    SUBD_FORCE_ENABLE_KNOB,
    SUBD_SNAP_TO_LIMIT_KNOB,
    SUBD_TESSELLATOR_KNOB,
    //
    POINTS_MODE_KNOB,
    USE_COLORS_KNOB,
    COLOR_FACESETS_KNOB,
    COLOR_OBJECTS_KNOB,
    APPLY_XFORMS_KNOB,
    CREATION_MODE_KNOB,
    //
    PREVIEW_LOD_KNOB,
    RENDER_LOD_KNOB,
    //
    DEBUG_KNOB,
    DEBUG_ATTRIBS_KNOB
};

static const KnobMap knob_map[] =
{
    /*  FuserGeoReader knob,   Fsr::NodePrimitive attrib */
    { "surface_mask",         "reader:surface_mask"       }, // SURFACE_MASK_KNOB,
    { "attribute_mappings",   "reader:attribute_mappings" }, // ATTRIBUTE_MAPPINGS_KNOB,
    { "velocity_scale",       "reader:velocity_scale"     }, // VELOCITY_SCALE_KNOB,
    //
    { "frame_offset",         "reader:frame_offset"       }, // FRAME_OFFSET_KNOB
    { "frame_origin",         "reader:frame_origin"       }, // FRAME_ORIGIN_KNOB,
    { "frame_rate",           "reader:fps"                }, // FPS_KNOB,
    //
    { "subd_import_level",    "reader:subd_import_level"  }, // SUBD_IMPORT_LEVEL_KNOB,
    { "subd_render_level",    "reader:subd_render_level"  }, // SUBD_RENDER_LEVEL_KNOB,
    { "subd_force_enable",    "reader:subd_force_enable"  }, // SUBD_FORCE_ENABLE_KNOB,
    { "subd_snap_to_limit",   "reader:subd_snap_to_limit" }, // SUBD_SNAP_TO_LIMIT_KNOB,
    { "subd_tessellator",     "reader:subd_tessellator"   }, // SUBD_TESSELLATOR_KNOB,
    //
    { "point_render_mode",    "reader:point_render_mode"  }, // POINTS_MODE_KNOB,
    { "use_geometry_colors",  "reader:use_geometry_colors"}, // USE_COLORS_KNOB,
    { "color_facesets",       "reader:color_facesets"     }, // COLOR_FACESETS_KNOB,
    { "color_objects",        "reader:color_objects"      }, // COLOR_OBJECTS_KNOB,
    { "apply_xforms",         "reader:apply_xforms"       }, // APPLY_XFORMS_KNOB,
    { "prim_creation_mode",   "reader:creation_mode"      }, // CREATION_MODE_KNOB,
    //
    { "proxy_lod_mode",       "reader:proxy_lod"          }, // PREVIEW_LOD_KNOB,
    { "render_lod_mode",      "reader:render_lod"         }, // RENDER_LOD_KNOB,
    //
    { "debug",                "reader:debug"              }, // DEBUG_KNOB,
    { "debug_attribs",        "reader:debug_attribs"      }, // DEBUG_ATTRIBS_KNOB
    //
    { 0, 0 }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
FuserGeoReaderFormat::FuserGeoReaderFormat(ReadGeo* geo)
{
    k_ignore_unrenderable   = true;
    k_translate_render_parts = true; // TODO: DEPRECATE!
    k_attribute_mappings    = default_attribute_mappings;
    //
    k_lock_read_frame       = false;
    k_read_frame            = 1.0;
    k_sub_frame             = true;
    k_velocity_scale        = 1.0;
    k_frame_offset          = 0.0;
    k_frame_origin          = 0.0;
    k_frames_per_second     = 24.0; // Can we default this from the root somehow...?
    //
    k_points_mode           = POINTS_ARE_POINTCLOUD_SPHERES;
    //
    k_subd_import_level     = SUBD_OFF;
    k_subd_render_level     = SUBD_OFF;
    k_subd_force_enable     = false;
    k_subd_snap_to_limit    = false;
    k_subd_tessellator      = SUBD_TESSELLATOR_OPENSUBDIV;
    //
    k_use_colors            = true;
    k_color_facesets        = false;
    k_color_objects         = false;
    //
    k_apply_xforms          = true;
    k_prim_creation_mode    = Fsr::NodePrimitive::LOAD_IMMEDIATE;
    k_proxy_lod_mode        = Fsr::NodePrimitive::LOD_PROXY;
    //
    k_debug                 = false;
    k_debug_attribs         = false;
}


/*! These go on the first tab.
*/
/*virtual*/ void
FuserGeoReaderFormat::knobs(Knob_Callback f)
{
    //std::cout << "FuserGeoReaderFormat::knobs()" << std::endl;
    addTimeOptionsKnobs(f);

    Divider(f);
    addImportOptionsKnobs(f);

    Divider(f);
    addPrimOptionsKnobs(f);
}


/*!
*/
/*virtual*/ void
FuserGeoReaderFormat::addTimeOptionsKnobs(Knob_Callback f)
{
    Bool_knob(f, &k_lock_read_frame, "lock_read_frame", "lock read frame:");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY);
        Tooltip(f, "If enabled lock the reader to use the manually-set frame number.");
    Double_knob(f, &k_read_frame, "read_frame", "");
        SetFlags(f, Knob::DISABLED |
                    Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::NO_MULTIVIEW);
        ClearFlags(f, Knob::SLIDER |
                      Knob::STARTLINE);
        Tooltip(f, "Use this frame number when 'lock read frame' is enabled.\n"
                   "This control can be animated to read any arbitrary frame speed curve.");
    Obsolete_knob(f, "lock_frame", "knob read_frame $value");
    Newline(f);

    Double_knob(f, &k_frame_origin, knob_map[FRAME_ORIGIN_KNOB].readerKnob, "frame: origin");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::NO_MULTIVIEW |
                    Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER);
        Tooltip(f, "Origin of the incoming frame range. Any frame rate change is scaled from this point.");
    Double_knob(f, &k_frames_per_second, IRange(1,96), knob_map[FPS_KNOB].readerKnob, "rate");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::NO_MULTIVIEW |
                    Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER |
                      Knob::STARTLINE);
        Tooltip(f, "This is the frame rate (frames per second) used to sample the geometry file.\n"
                   "If this rate is lower than the rate encoded in the file the effect is to "
                   "slow down the animation. For example if the file was animated at 24 fps and "
                   "frame_rate is set to 12, the animation will read at half speed.");
    Double_knob(f, &k_frame_offset, knob_map[FRAME_OFFSET_KNOB].readerKnob, "output offset");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::NO_MULTIVIEW |
                    Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER |
                      Knob::STARTLINE);
        Tooltip(f, "Offset the incoming frame range, applied after any frame rate change");
    Bool_knob(f, &k_sub_frame, "sub_frame", "sub-frame interp");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f,  "If true allow non-integer frame samples to be read from file.\n"
                    "Only available if we're not manually setting the frame.");
}


/*!
*/
/*virtual*/ void
FuserGeoReaderFormat::addImportOptionsKnobs(Knob_Callback f)
{
    Enumeration_knob(f, &k_prim_creation_mode, Fsr::NodePrimitive::load_modes, knob_map[CREATION_MODE_KNOB].readerKnob, "prim creation");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::STARTLINE);
        Tooltip(f,  "Geometry data creation mode:\n"
                    " <b>immediate</b> - add Nuke geometry primitives, immediately loading all vertex and point data.\n"
                    " <b>deferred</b> - add Fuser primitives with only object attributes loaded (no vertex or point data)\n"
                    "");

    //------------------------------------
    Bool_knob(f, &k_ignore_unrenderable, "ignore_unrenderable", "ignore unrenderable");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::STARTLINE);
SetFlags(f, Knob::DISABLED);
        Tooltip(f,  "Don't show unrenderable objects as available in object lists.  The current "
                    "node types considered unrenderable are:\n"
                    " <b>Xform</b>\n"
                    " <b>Camera</b>\n"
                    " <b>Light</b>\n"
                    " <b>Curves</b>\n"
                    " <b>NuPatch(alembic)</b>\n"
                    );
    Bool_knob(f, &k_apply_xforms, knob_map[APPLY_XFORMS_KNOB].readerKnob, "apply xforms");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY);
        Tooltip(f,  "Enable/disble the application of transform matrices to objects.\n"
                    "Objects with a transform hierarchy will usually end up at the "
                    "origin (0,0,0) when this is off.\n"
                    "\n"
                    "Note - this may not work in the current ReadGeo system which does "
                    "not appear to allow geometry readers this level of control.");
    Obsolete_knob(f, "apply_matrix", "knob apply_xforms $value");

#ifdef DWA_INTERNAL_BUILD
    //------------------------------------
    // TODO: DEPRECATE!
    Bool_knob(f, &k_translate_render_parts, "translate_render_parts", "translate render parts to UDIMs");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY);
SetFlags(f, Knob::DISABLED);
        Tooltip(f,  "Translate legacy render-part enums to UDIM-style UV faceset offsets.\n"
                    "Use the 'UVTile' material node to assign a texture to a faceset (render-part.) "
                    "Multiple facesets can be assigned by using the 'MergeMat' node where each "
                    "material input to the MergeMat has a UVTile node addressing a difference faceset.");
#endif

    //------------------------------------
    Enumeration_knob(f, &k_proxy_lod_mode, Fsr::NodePrimitive::lod_modes, knob_map[PREVIEW_LOD_KNOB].readerKnob, "proxy lod mode");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::STARTLINE);
SetFlags(f, Knob::DISABLED);
        Tooltip(f,  "In deferred mode how to display geometry:\n"
                    "bbox - display the bounding-box extents\n"
                    " <b>standin</b> - TODO\n"
                    " <b>low</b> - TODO\n"
                    " <b>light</b> - TODO\n"
                    "");

}


/*!
*/
/*virtual*/ void
FuserGeoReaderFormat::addPrimOptionsKnobs(Knob_Callback f)
{
    //------------------------------------
    Newline(f, "subds:");
    Enumeration_knob(f, &k_subd_import_level, subd_levels, knob_map[SUBD_IMPORT_LEVEL_KNOB].readerKnob, "import level");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f,  "Subdivision level to use for <b>importing</b>\n"
                    "In immediate load mode this will create mesh primitives with subdivided faces/verts.\n"
                    "In deferred mode this will affect the OpenGL preview display.\n"
                    SUBD_KNOB_HELP);
    Enumeration_knob(f, &k_subd_render_level, subd_levels, knob_map[SUBD_RENDER_LEVEL_KNOB].readerKnob, "render level");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f,  "Subdivision level to use for <b>rendering</b> (will not affect OpenGL display)\n"
                    SUBD_KNOB_HELP);
    Bool_knob(f, &k_subd_force_enable, knob_map[SUBD_FORCE_ENABLE_KNOB].readerKnob, "all meshes");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f, "Enable subdivision on meshes even though they may not be tagged as subds in the file.");
    Bool_knob(f, &k_subd_snap_to_limit, knob_map[SUBD_SNAP_TO_LIMIT_KNOB].readerKnob, "snap to limit");
SetFlags(f, Knob::DISABLED);
        SetFlags(f, Knob::MODIFIES_GEOMETRY |
                    Knob::ENDLINE);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f,  "After subdividing to the target level snap the resulting points to the limit surface.\n"
                    "\n"
                    "The will make the resulting mesh more accurate to the ideal subd surface profile "
                    "(the 'limit surface') but will not allow the mesh to be further subdivided properly "
                    "since the point locations are no longer aligned with the original cage.");
    Enumeration_knob(f, &k_subd_tessellator, subd_tessellators, knob_map[SUBD_TESSELLATOR_KNOB].readerKnob, "tessellator");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        ClearFlags(f, Knob::STARTLINE);
        Tooltip(f,  "Tessellator scheme to use for subdividing\n"
                    "OpenSubdiv (default, uses the OpenSubdiv library)\n"
                    "SimpleSubdiv (buggy, naive, kinda-sorta-works, use as fallback only)\n"
                    "\n");
    Obsolete_knob(f, "import_subd_level", "knob subd_import_level $value");
    Obsolete_knob(f, "render_subd_level", "knob subd_render_level $value");
    //------------------------------------
    Enumeration_knob(f, &k_points_mode, points_modes, knob_map[POINTS_MODE_KNOB].readerKnob, "render points as");
SetFlags(f, Knob::DISABLED);
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        Tooltip(f,  "Sets the preferred render mode attribute 'point_render_mode' for point cloud primitives.\n"
                    "Note - this may not be supported by all renderers.");
    //------------------------------------
    Double_knob(f, &k_velocity_scale, knob_map[VELOCITY_SCALE_KNOB].readerKnob, "velocity scale");
        SetFlags(f, Knob::EARLY_STORE |
                    Knob::MODIFIES_GEOMETRY |
                    Knob::NO_MULTIVIEW);
        ClearFlags(f, Knob::STARTLINE |
                      Knob::SLIDER);
        Tooltip(f,  "If the geometry contains point velocity data, apply this scale factor to it.\n"
                    "\n"
                    "Point velocity vectors are often used to produce motionblur for geometry that has "
                    "topology varying frame to frame causing point/vertex counts to change.\n"
                    "\n"
                    "Use -1.0 to invert them. Sometimes they must be inverted to get the motionblur to "
                    "go in the correct direction, depending on how they were originally generated.\n"
                    "\n"
                    "By default velocities are scaled down by the file's frames-per-second rate (frame rate) "
                    "as velocity magnitude is interpreted as distance-per-second (default when generated "
                    "from Houdini) but other packages may use distance-per-frame.\n"
                    "If the motionblur is too short try setting scale to 24.0 (or whatever the frame rate is) "
                    "and this may correct the length.");
    Obsolete_knob(f, "invert_velocities", "if {$value==true} {knob velocity_scale -1.0}");
    //------------------------------------
    Newline(f);
    Bool_knob(f, &k_use_colors, knob_map[USE_COLORS_KNOB].readerKnob, "use geometry colors");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        Tooltip(f, "");
    Bool_knob(f, &k_color_facesets, knob_map[COLOR_FACESETS_KNOB].readerKnob, "color facesets");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
SetFlags(f, Knob::DISABLED);
        Tooltip(f, "Set the color of the faces in each faceset to a random color for identification.");
    Bool_knob(f, &k_color_objects, knob_map[COLOR_OBJECTS_KNOB].readerKnob, "color objects");
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        Tooltip(f,  "Set the color of each object to a random color for identification.\n"
                    "To see the colors in the OpenGL 3D display set the 'display' knob below to 'solid'.");
    //------------------------------------
    Newline(f);
    Bool_knob(f, &k_debug, knob_map[DEBUG_KNOB].readerKnob, "prim debug info");
        Tooltip(f, "Prints primitive loading info to the console.");
    Bool_knob(f, &k_debug_attribs, knob_map[DEBUG_ATTRIBS_KNOB].readerKnob, "attribs debug info");
        Tooltip(f, "Prints attribute loading info to the console.");
}


/*! These knobs go on additional tabs.
*/
/*virtual*/ void
FuserGeoReaderFormat::extraKnobs(Knob_Callback f)
{
    //std::cout << "FuserGeoReaderFormat::extraKnobs()" << std::endl;
    Tab_knob(f, "AttributeMap");
    Text_knob(f, "mapping syntax: '<file attrib name>=<out attrib name>'");
    Newline(f);
    Multiline_String_knob(f, &k_attribute_mappings, knob_map[ATTRIBUTE_MAPPINGS_KNOB].readerKnob, "attribute mappings", 10/*lines*/);
        SetFlags(f, Knob::MODIFIES_GEOMETRY);
        SetFlags(f, Knob::EARLY_STORE);
}


/*!
*/
/*virtual*/ int
FuserGeoReaderFormat::knob_changed(Knob* k)
{
    //std::cout << "FuserGeoReaderFormat::knob_changed()" << std::endl;
    // This does nothing as the parent GeoReader gets all the
    // callbacks.
    return 0; // don't call again
}


/*!
*/
/*virtual*/ void
FuserGeoReaderFormat::append(DD::Image::Hash& hash)
{
    //std::cout << "FuserGeoReaderFormat::append(" << this << ")" << std::endl;
    hash.append(k_translate_render_parts); // TODO: DEPRECATE!
    hash.append(k_ignore_unrenderable);
    hash.append(k_attribute_mappings);
    //
    hash.append(k_lock_read_frame);
    hash.append(k_sub_frame);
    hash.append(k_velocity_scale);
    hash.append(k_read_frame);
    hash.append(k_frame_offset);
    hash.append(k_frame_origin);
    hash.append(k_frames_per_second);
    //
    hash.append(k_subd_import_level);
    hash.append(k_subd_render_level);
    hash.append(k_subd_force_enable);
    hash.append(k_subd_snap_to_limit);
    hash.append(k_subd_tessellator);
    //
    hash.append(k_points_mode);
    //
    hash.append(k_use_colors);
    hash.append(k_color_facesets);
    hash.append(k_color_objects);
    //
    hash.append(k_apply_xforms);
    hash.append(k_prim_creation_mode);
    hash.append(k_proxy_lod_mode);
    //
    hash.append(k_debug);
    hash.append(k_debug_attribs);
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
FuserGeoReader::FuserGeoReader(ReadGeo* geo, int fd) :
    GeoReader(geo),
    Fsr::NodeIOInterface(),
    m_filename_for_reader(Arg::invalid_token)
{
#if 0
    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    const double frames_per_second = (options)?options->k_frames_per_second:24.0;
    const bool   debug             = (options)?options->k_debug:false;
    const bool   debug_attribs     = (options)?options->k_debug_attribs:false;
#endif

#if 0
    if (debug)
    {
        std::cout << "FuserGeoReader::ctor(" << this << "): reading geo file '" << filePathForReader() << "'";
        std::cout << std::endl;
    }
#endif

#if 0
    if (frame_start <= frame_end)
    {
        //std::cout << "  frame range=" << frame_start << "-" << frame_end << std::endl;
        geo->knob("range_first")->set_value(frame_start);
        geo->knob("range_last")->set_value(frame_end);
    }
#endif

}


/*!
*/
FuserGeoReader::~FuserGeoReader()
{
#if 0
    if (debug)
    {
        std::cout << "         FuserGeoReader::dtor(" << this << ")" << std::endl;
        std::cout << "......................................................................................" << std::endl;
    }
#endif
}


/*! Returned the trimmed filename without any leading extension tokens.

    For whatever reason geo->filename() nor geo->fname() do not reliably
    return the trimmed name, so we're storing it locally.
*/
/*virtual*/ const char*
FuserGeoReader::filePathForReader()
{
    assert(geo); // shouldn't happen...

    // Possibly update it if empty:
    if (m_filename_for_reader == Arg::invalid_token)
    {
        // Get trimmed path. Sometimes geo->fname() does not contain
        // a trimmed path - not sure why:
        m_filename_for_reader = NodeIOInterface::getTrimmedPath(geo->fname());
    }

    //std::cout << "FuserGeoReader::filePathForReader() filename=" << m_filename_for_reader << std::endl;
    return m_filename_for_reader.c_str();
}


/*! enable the axis knob or knot for the current plugin
    This is a temporary hack fix until per-mesh animation over-rides
    can be introduced.
*/
/*virtual*/ bool
FuserGeoReader::enableAxisKnob()
{
    //std::cout << "FuserGeoReader::enableAxisKnob()" << std::endl;
    // Don't show the Axis knob:
    return false;
}


/*!
*/
/*virtual*/ int
FuserGeoReader::knob_changed(Knob* k)
{
    //std::cout << "FuserGeoReader(" << geo << ")::knob_changed(" << ((k)?k->name():"<null>") << ")" << std::endl;
    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());

    //*****************************************************************************
    //*  DO NOT CALL THE BASE CLASS GeoReader::knob_changed() in here, this will  *
    //*  cause a symbol problem since there's no base-class implementation...     *
    //*****************************************************************************

    const bool show_panel = (k == &Knob::showPanel);

    int ret = 0;

    if (show_panel ||
        k->name() == "lock_read_frame")
    {
        const bool lock_read_frame = (options) ? options->k_lock_read_frame : false;
        geo->knob("read_frame")->enable(lock_read_frame);
        geo->knob(knob_map[FRAME_ORIGIN_KNOB].readerKnob)->enable(!lock_read_frame);
        geo->knob(knob_map[FPS_KNOB         ].readerKnob)->enable(!lock_read_frame);
        geo->knob("sub_frame")->enable(!lock_read_frame);
        ret = 1; // we want to be called again
    }

    return ret;
}


/*! Default material
*/
/*virtual*/ Iop*
FuserGeoReader::default_material_iop() const
{
    //std::cout << "FuserGeoReader::default_material_iop()" << std::endl;
    return DD::Image::GeoReader::default_material_iop();
}


/*!
*/
/*virtual*/ void
FuserGeoReader::append(DD::Image::Hash& hash)
{
    //std::cout << "FuserGeoReader::append(" << this << ")" << std::endl;

    //*****************************************************************************
    //*  DO NOT CALL THE BASE CLASS GeoReader::append() in here, this will        *
    //*  cause a symbol problem since there's no base-class implementation...     *
    //*****************************************************************************

    // Make sure we rebuild on frame changes since the filename normally
    // doesn't change with a scene file:
    hash.append(geo->outputContext().frame());
    //std::cout << "FuserGeoReader::append() frame=" << geo->outputContext().frame() << ", hash=" << std::hex << hash.value() << std::dec << std::endl;
}


/*!
*/
/*virtual*/ void
FuserGeoReader::get_geometry_hash(DD::Image::Hash* geo_hashes)
{
    //std::cout << "FuserGeoReader::get_geometry_hash(" << this << ")" << std::endl;

    //*********************************************************************************
    //*  DO NOT CALL THE BASE CLASS GeoReader::get_geometry_hash() in here, this will *
    //*  cause a symbol problem since there's no base-class implementation...         *
    //*********************************************************************************

    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    if (options)
    {
        Hash knob_hash;
        //
        knob_hash.append(geo->knob("version")->get_value()); // 'Reload' button increments this
        //
        knob_hash.append(options->k_translate_render_parts); // TODO: DEPRECATE!
        knob_hash.append(options->k_ignore_unrenderable);
        //
        knob_hash.append(options->k_lock_read_frame);
        knob_hash.append(options->k_read_frame);
        knob_hash.append(options->k_sub_frame);
        knob_hash.append(options->k_velocity_scale);
        knob_hash.append(options->k_frame_offset);
        knob_hash.append(options->k_frame_origin);
        knob_hash.append(options->k_frames_per_second);
        //
        knob_hash.append(options->k_subd_import_level);
        knob_hash.append(options->k_subd_render_level);
        knob_hash.append(options->k_subd_force_enable);
        knob_hash.append(options->k_subd_snap_to_limit);
        knob_hash.append(options->k_subd_tessellator);
        //
        knob_hash.append(options->k_points_mode);
        //
        knob_hash.append(options->k_use_colors);
        knob_hash.append(options->k_color_facesets);
        knob_hash.append(options->k_color_objects);
        //
        knob_hash.append(options->k_apply_xforms);
        knob_hash.append(options->k_prim_creation_mode);
        knob_hash.append(options->k_proxy_lod_mode);
        //
        knob_hash.append(options->k_debug);
        knob_hash.append(options->k_debug_attribs);
        knob_hash.append(options->k_attribute_mappings);
        //
        if (!options->k_lock_read_frame)
        {
            // Make sure we rebuild points or geometry on frame changes since the filename
            // normally doesn't change with an alembic file.
            //
            // TODO: atm if any of the nodes have animating matrices (rigid body transforms)
            // we have to reload the points since we're applying the matrix to the points...
            // When we don't have to do that we can just set the Group_Matrix hash to indicate
            // an animating matrix.

            const uint32_t global_topology_variance = getGlobalTopologyVariance();

            // Just points or matrix animating:
            if ((global_topology_variance & Fsr::Node::XformVaryingTopology) ||
                (global_topology_variance & Fsr::Node::PointVaryingTopology))
                geo_hashes[Group_Points].append(geo->outputContext().frame());

            // Primitives are animating:
            if (global_topology_variance & Fsr::Node::PrimitiveVaryingTopology)
            {
                geo_hashes[Group_Points].append(geo->outputContext().frame());
                geo_hashes[Group_Primitives].append(geo->outputContext().frame());
            }

            _getTopologyHash(geo_hashes[Group_Primitives]);
        }
        geo_hashes[Group_Vertices  ].append(knob_hash);
        geo_hashes[Group_Primitives].append(knob_hash);
        geo_hashes[Group_Object    ].append(knob_hash);
        geo_hashes[Group_Attributes].append(knob_hash);
    }
    //std::cout << "  hash out=" << std::hex << geo_hashes[Group_Points].value() << std::dec << std::endl;
}


/*! Return a hash indicating the file state.
    Subclasses can change this to cause file invalidation and reload.
*/
DD::Image::Hash
FuserGeoReader::getFileHash()
{
    DD::Image::Hash hash;
    _getFileHash(hash);
    hash.append(filename()); // use the raw filename() vs. possibly trimmed geo->fname()
    hash.append(geo->knob("version")->get_value()); // 'Reload' button increments this
    return hash;
}


/*! Return the global topology variance flags.
*/
uint32_t
FuserGeoReader::getGlobalTopologyVariance()
{
    return _getGlobalTopologyVariance();
}


/*! Return a hash indicating the geometry topology state.
    Subclasses can change this to cause the primitive topology to be invalidated.
*/
DD::Image::Hash
FuserGeoReader::getTopologyHash()
{
    DD::Image::Hash hash;
    _getTopologyHash(hash);
    return hash;
}

/*! Return a hash indicating the reader's UI state.
*/
DD::Image::Hash
FuserGeoReader::getReaderUIHash()
{
    DD::Image::Hash hash;
    _getReaderUIHash(hash);
    hash.append(geo->knob("version")->get_value()); // 'Reload' button increments this
    return hash;
}


/*!
*/
/*virtual*/ void
FuserGeoReader::_validate(const bool for_real)
{
    //std::cout << "FuserGeoReader::_validate(" << this << ") for_real=" << for_real << std::endl;

    //***************************************************************************
    //*  DO NOT CALL THE BASE CLASS GeoReader::_validate() in here, this will   *
    //*  cause a symbol problem since there's no base-class implementation...   *
    //***************************************************************************

    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    if (options)
    {
        //std::cout << "FuserGeoReader(" << this << ")::_validate(): for_real=" << for_real << ", frame=" << geo->outputContext().frame();
        //std::cout << " file='" << geo->getFilename() << "'" << std::endl;
    }

    DD::Image::Hash file_hash = getFileHash();
    if (file_hash != m_file_hash)
    {
        m_file_hash = file_hash;
        m_filename_for_reader = Arg::invalid_token; // reset trimmed filename to invalid
    }

}


/*! Get the list of object names(paths) to read in during geometry_engine.
    Base class returns an empty set.
*/
/*virtual*/
const std::set<std::string>&
FuserGeoReader::getObjectPathsForReader()
{
    static std::set<std::string> empty_set;
    return empty_set;
}


/*! Get the list of material names(paths) to read in during geometry_engine.
    Base class returns an empty set.
*/
/*virtual*/
const std::set<std::string>&
FuserGeoReader::getMaterialPathsForReader()
{
    static std::set<std::string> empty_set;
    return empty_set;
}


/*! Get the list of light names(paths) to read in during geometry_engine.
    Base class returns an empty set.
*/
/*virtual*/
const std::set<std::string>&
FuserGeoReader::getLightPathsForReader()
{
    static std::set<std::string> empty_set;
    return empty_set;
}


/*! Calls _updateReaderUI() if Nuke is in GUI mode.
*/
void
FuserGeoReader::updateReaderUI()
{
    if (DD::Image::Application::gui)
    {
        this->_validate(false);
        _updateReaderUI();
    }
}


/*! Extract the to/from attribute name mappings from a text entry.

    Name mapping syntax: <file-attrib-name>=<out-attrib-name>'

    If out-attrib-name is empty then the file attrib is ignored.
    Examples:
       color=Cf, Cd=Cf, UV=uv, st=uv  (more than one mapping on a line)
       pscale=size
       render_part_dwa_mm_part_enum=  (empty mapping, file attrib is ignored)
*/
/*static*/
void
FuserGeoReader::buildAttributeMappings(const char*            txt,
                                       Fsr::KeyValueMap&      file_to_nuke_map,
                                       Fsr::KeyValueMultiMap& nuke_to_file_map)
{
    //std::cout << "FuserGeoReader::buildAttributeMappings('" << txt << "')" << std::endl;
    file_to_nuke_map.clear();
    nuke_to_file_map.clear();

    if (!txt || !txt[0])
        return;

    std::vector<std::string> mappings; mappings.reserve(10);
    Fsr::stringSplit(txt, ";, \t\n\r", mappings);

    for (unsigned i=0; i < mappings.size(); ++i)
    {
        const std::string& mapping = mappings[i];

        // Split mapping into from/to:
        size_t a = mapping.find_first_of('=');
        if (a == std::string::npos)
            continue; // no '=' sign, skip it

        const std::string file_attrib = stringTrim(mapping.substr(0, a));
        const std::string nuke_attrib = stringTrim(mapping.substr(a+1, std::string::npos));

        // Skip empty mappings:
        if (file_attrib.empty() || nuke_attrib.empty())
            continue;

        file_to_nuke_map[file_attrib] = nuke_attrib;
        nuke_to_file_map.insert(std::pair<std::string, std::string>(nuke_attrib, file_attrib));
        //std::cout << "  '" << mapping << "': file'" << file_attrib << "'->nuke'" << nuke_attrib << "'" << std::endl;
    }
}


/*! If the file_attrib exists in the attrib_map return the nuke attrib mapped name.
*/
/*static*/
std::string
FuserGeoReader::getFileToNukeAttribMapping(const char*             file_attrib,
                                           const Fsr::KeyValueMap& file_to_nuke_map)
{
    const Fsr::KeyValueMap::const_iterator it = file_to_nuke_map.find(std::string(file_attrib));
    if (it != file_to_nuke_map.end())
        return it->second;
    return std::string();
}


/*! Map a nuke attrib name to possible multiple file attrib names.
    The order of preference is likely alphabetical.
*/
/*static*/
uint32_t
FuserGeoReader::getNukeToFileAttribMappings(const char*                  nuke_attrib,
                                            const Fsr::KeyValueMultiMap& nuke_to_file_map,
                                            std::vector<std::string>&    mappings)
{
    const uint32_t nVals = (uint32_t)nuke_to_file_map.count(std::string(nuke_attrib));
    mappings.clear();
    if (nVals > 0)
    {
        mappings.reserve(nVals);
        Fsr::KeyValueMultiMap::const_iterator it = nuke_to_file_map.find(std::string(nuke_attrib));
        for (uint32_t i=0; i < nVals; ++i, ++it)
            mappings.push_back(it->second);
    }
    return nVals;
}


//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------


/*! This is passed to the static thread_proc_cb() callback below and is
    shared between the threads.
*/
struct GeometryEngineThreadContext
{
    FuserGeoReader*                 reader;         //!<
    Fsr::ArgSet                     node_args;      //!< Args for Fuser node creation
    Fsr::NodeContext                exec_ctx;       //!< Execution context args
    Fsr::GeoOpGeometryEngineContext geo_ctx;        //!<

    std::set<std::string>::const_iterator next;     //!< Next object id to operate on
    std::set<std::string>::const_iterator end;      //!< Last object id

    //!
    GeometryEngineThreadContext(FuserGeoReader*   geo_reader,
                                int               num_threads,
                                GeoOp*            geo,
                                GeometryList*     geometry_list,
                                DD::Image::Scene* scene) :
        reader(geo_reader),
        geo_ctx(num_threads, geo, geometry_list, scene)
    {
        //
    }

};


#if 0
/*! Returns the number of real cpus on system.
*/
unsigned getCPUsForSystem()
{
#ifdef _WIN32
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    return SysInfo.dwNumberOfProcessors;
#elif defined(_SC_NPROC_CONF)
    return sysconf(_SC_NPROC_CONF);
#elif defined(_SC_NUM_PROCESSORS)
    return sysconf(_SC_NUM_PROCESSORS);
#elif defined(_SC_NPROCESSORS_CONF)
    return sysconf(_SC_NPROCESSORS_CONF);
#else
    return 0;
#endif
}


//! Return the current number of threads that DDImage will use to render.
unsigned getThreads()
{
    return DD::Image::Thread::numThreads;
}
#endif


/*! DD::Image::Thread spawn callback function to iterate through the object list.

    typedef void (*ThreadFunction)(unsigned index, unsigned nThreads, void* userData);

    If multithreaded each thread tries to grab the next available object
    to process, leapfrog-like.
*/
static void thread_proc_cb(unsigned thread_index, unsigned num_threads, void* p)
{
    GeometryEngineThreadContext* otx = reinterpret_cast<GeometryEngineThreadContext*>(p);
    assert(otx && otx->reader);

    //otx->geo_ctx.lock();
    //std::cout << "  thread_proc_cb(" << otx << ") thread_index=" << thread_index;
    //std::cout << ", pthread=" << std::hex << DD::Image::Thread::GetThreadId() << std::dec;
    //std::cout << std::endl;
    //otx->geo_ctx.unlock();

    // Get the next object to work on, otherwise bail:
    while (1)
    {
        if (otx->next == otx->end)
            return; // all done, bail

        // Aquire a write lock if we're multithreaded:
        if (num_threads > 0)
        {
            otx->geo_ctx.lock();
            // Check again to avoid race condition:
            if (otx->next == otx->end)
            {
                otx->geo_ctx.unlock();
                return; // all done, bail
            }

            const std::set<std::string>::const_iterator it = otx->next++;
            otx->geo_ctx.unlock();

            assert(it != otx->end); // shouldn't happen...
            //std::cout << "  thread_proc_cb(" << otx << ") thread_index=" << thread_index;
            //std::cout << ", pthread=" << std::hex << DD::Image::Thread::GetThreadId() << std::dec;
            //std::cout << ", path='" << *it << "'" << std::endl;

            // Make a local node_ctx copy to avoid other threads conflicting:
            Fsr::ArgSet      node_args(otx->node_args);
            Fsr::NodeContext exec_ctx(otx->exec_ctx);
            otx->reader->readObject(*it, node_args, exec_ctx, otx->geo_ctx);
        }
        else
        {
            const std::set<std::string>::const_iterator it = otx->next++;
            //std::cout << "  path='" << *it << "'" << std::endl;
            otx->reader->readObject(*it, otx->node_args, otx->exec_ctx, otx->geo_ctx);
        }
    }
}


/*! Thread-safe object loader entry point called by ThreadedGeometryEngine instance.
    Returns false on user-abort.
*/
bool
FuserGeoReader::readObject(const std::string&               path,
                           Fsr::ArgSet&                     node_args,
                           Fsr::NodeContext&                exec_ctx,
                           Fsr::GeoOpGeometryEngineContext& geo_ctx)
{
    //std::cout << "      FuserGeoReader::readObject(" << this << ") thread=" << std::hex << DD::Image::Thread::GetThreadId() << std::dec;
    //std::cout << ", path='" << path << "'" << std::endl;

    //if (debug)
    //    std::cout << "       " << obj << ": path='" << path << "'" << std::endl;

    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    if (!options)
        return true;  // don't crash...
    const int prim_creation_mode = options->k_prim_creation_mode;

    node_args.setString(Arg::node_name,   Fsr::fileNameFromPath(path));
    node_args.setString(Arg::node_path,   path); // TODO: this path may change to be different than Scene::path
    node_args.setString(Arg::Scene::path, path);

#if 0
    // Add the arguments that controls what kind of Fuser Scene Node will get
    // constructed by the fsrAbcIO plugin.
    //
    // Some of these node class names are semi-Alembic specific, like 'Subd' and 'NuPatch',
    // while most are generic like 'Camera', 'Xform', and 'Light':

    node_args.setString("fsrUsdIO:node:class", ref.nodeName());
#endif

    //std::cout << "FuserGeoReader: args: " << node_args << std::endl;

    // TODO: shouldn't need to pass in plugin name 'AbcIO' here, it should be extracted
    // from 'scene:file' and cached by the Fsr::NodePrimitive.
    // Adds DD::Image::Primitives to GeometryList, and updates animating info:
    int added_objects = Fsr::NodePrimitive::addGeometryToScene(fuserIOClass(),     /* .so plugin name */
                                                               prim_creation_mode, /* immediate/deferred */
                                                               node_args,
                                                               exec_ctx,
                                                               geo_ctx);
    if (added_objects < 0)
    {
        //throw geo error
        return true;
    }

    return true;
}


/*! Thread-safe object loader entry point called by ThreadedGeometryEngine instance.
    Returns false on user-abort.
*/
bool
FuserGeoReader::readMaterial(const std::string&               path,
                             Fsr::ArgSet&                     node_args,
                             Fsr::NodeContext&                exec_ctx,
                             Fsr::GeoOpGeometryEngineContext& geo_ctx)
{
    //std::cout << "      FuserGeoReader::readObject(" << this << ") thread=" << std::hex << DD::Image::Thread::GetThreadId() << std::dec;
    //std::cout << ", path='" << path << "'" << std::endl;

    //if (debug)
    //    std::cout << "       " << obj << ": path='" << path << "'" << std::endl;

    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    if (!options)
        return true;  // don't crash...
    const int prim_creation_mode = options->k_prim_creation_mode;

    node_args.setString(Arg::node_name,   Fsr::fileNameFromPath(path));
    node_args.setString(Arg::node_path,   path); // TODO: this path may change to be different than Scene::path
    node_args.setString(Arg::Scene::path, path);

#if 0
    // Add the arguments that controls what kind of Fuser Scene Node will get
    // constructed by the fsrAbcIO plugin.
    //
    // Some of these node class names are semi-Alembic specific, like 'Subd' and 'NuPatch',
    // while most are generic like 'Camera', 'Xform', and 'Light':

    node_args.setString("fsrUsdIO:node:class", ref.nodeName());
#endif

    //std::cout << "FuserGeoReader: args: " << node_args << std::endl;

    // TODO: shouldn't need to pass in plugin name 'AbcIO' here, it should be extracted
    // from 'scene:file' and cached by the Fsr::NodePrimitive.
    // Adds DD::Image::Primitives to GeometryList, and updates animating info:
    int added_objects = Fsr::NodePrimitive::addGeometryToScene(fuserIOClass(),     /* .so plugin name */
                                                               prim_creation_mode, /* immediate/deferred */
                                                               node_args,
                                                               exec_ctx,
                                                               geo_ctx);
    if (added_objects < 0)
    {
        //throw geo error
        return true;
    }

    return true;
}


//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------


/*! I don't think this ever gets called...
*/
/*virtual*/ void
FuserGeoReader::_open()
{
    std::cout << "FuserGeoReader::_open(" << this << ")" << std::endl;
}


/*!
*/
bool
FuserGeoReader::openSceneFile()
{
    //const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    //const bool debug = (options)?options->k_debug:false;

    return _openSceneFile();
}


/*!
*/
/*virtual*/ void
FuserGeoReader::geometry_engine(DD::Image::Scene&        scene,
                                DD::Image::GeometryList& out)
{
    assert(geo); // shouldn't happen...
    //std::cout << "FuserGeoReader::geometry_engine(" << this << ") rebuild=" << geo->rebuild_mask();
    //std::cout << ", handler=" << geo->handler() << std::endl;

    if (geo->rebuild_mask() == 0x0)
        return; // no changes, don't bother reading anything

    const FuserGeoReaderFormat* options = dynamic_cast<FuserGeoReaderFormat*>(geo->handler());
    if (!options)
    {
        std::cerr << "FuserGeoReader::geometry_engine(): warning, no GeoReaderFormat object, ";
        std::cerr << "this is likely a coding error!" << std::endl;
        return;  // don't crash...
    }

    const bool   lock_read_frame = options->k_lock_read_frame;
    const bool   sub_frame       = options->k_sub_frame;
    const bool   debug           = options->k_debug;

    const bool   reload_points = (geo->rebuild_mask() & Mask_Points);
    const bool   reload_prims  = (geo->rebuild(Mask_Primitives) ||
                                  geo->rebuild(Mask_Vertices  ) ||
                                  geo->rebuild(Mask_Object    ) ||
                                  geo->rebuild(Mask_Attributes));
    const bool   reload_attribs = geo->rebuild(Mask_Attributes);

    if (debug)
    {
        std::cout << "  ======================================================================================================" << std::endl;
        std::cout << "  FuserGeoReader::geometry_engine(" << this << "): rebuild_mask=" << std::hex << geo->rebuild_mask() << std::dec;
        std::cout << ": reload_prims=" << reload_prims << ", reload_points=" << reload_points << ", reload_attribs=" << reload_attribs;
        std::cout << ", frame=" << geo->outputContext().frame();
        std::cout << std::endl;
    }

    if (debug)
    {
        std::cout << "    file='" << filePathForReader() << "'";
        std::cout << ", fuserIOClass='" << fuserIOClass() << "'";
        std::cout << std::endl;
    }

    // We need a frame for the output primitive so that the renderer can
    // interpolate, which must be in the output frame range (non-timewarped):
    double output_frame = geo->outputContext().frame(); // get Op's outputContext frame

    // If not using a subframe value determine whether to round the output frame up or down
    // depending on which shutter scene this is.
    // (we don't bother rounding if the frame is manually set)
    // TODO: check that this scene ID trick still works....!
    if (!sub_frame && !lock_read_frame)
    {
        // If the current frame is not at an integer value then we either need
        // to round up or down:
        const double frameFloor = floor(output_frame);

        if (debug)
            std::cout << "    output_frame=" << output_frame << ", mb_offset=" << (output_frame - frameFloor) << ", scene.sceneId()=" << scene.sceneId() << std::endl;
        if ((output_frame - frameFloor) > 0.0001)
        {
            // Use the Scene's sceneId var as the motion-sample.  If it's negative then
            // the motion-sample is back in time from frame and we should round DOWN
            // the frame #, otherwise if it's positive we should round UP the frame:
            output_frame = frameFloor;
            if (scene.sceneId() > 0)
                output_frame += 1.0; // fwd shutter, round-up
        }
    }

    // Use the output frame for the reader, or the manually set one?
    //
    // This is the frame number used to index the geometry in the file, which
    // may be timewarped by an animation curve:
    double reader_frame = output_frame;
    if (lock_read_frame && options)
        reader_frame = geo->knob("read_frame")->get_value_at(output_frame);

    reader_frame -= options->k_frame_offset;

    // openSceneFile returns fast on repeat calls:
    if (!openSceneFile())
        return; // user-abort

    // 
    const std::set<std::string>& selected_object_paths = getObjectPathsForReader();

    // Figure out if we want to run the engine multithreaded:
    int num_threads = DD::Image::Thread::numThreads;
    if ((int)selected_object_paths.size() < num_threads)
        num_threads = (int)selected_object_paths.size();

    // This gets passed to the worker threads:
    GeometryEngineThreadContext geo_thread_ctx(this,
                                               num_threads,
                                               geo,
                                               &out,
                                               &scene);

    // Assign the selected object range to the context to process:
    geo_thread_ctx.next = selected_object_paths.begin();
    geo_thread_ctx.end  = selected_object_paths.end();

    //----------------------------------------------------------------------------

    //int particle_index = 0; // so that the index is unique across all objects
    if (reload_prims)
    {
        // Clears all objects:
        out.delete_objects();
        int obj = out.objects(); // New object index to use should always be 0
        assert(obj == 0); // should always be 0!

        // Why do we need this here to make the gui update...?  I suppose it's because
        // the geometry_engine() is called from the validate() process...
        //if (DD::Image::Application::gui)
        //    _updateReaderUI();

        geo_thread_ctx.geo_ctx.clearObjectIds();
    }

    if (debug)
    {
        std::cout << "    reader_frame=" << reader_frame << ", output_frame=" << output_frame;
        std::cout << ", selected object nodes=" << selected_object_paths.size();
    }

    if (selected_object_paths.size() == 0)
    {
        if (debug)
            std::cout << std::endl;
        return;
    }

    //----------------------------------------------------------------------------

    // Build context (args) to pass to Fsr::NodePrimitives ctors:
    {
        ArgSet&           node_args = geo_thread_ctx.node_args;
        Fsr::NodeContext& exec_ctx  = geo_thread_ctx.exec_ctx;

        // Fill in the arguments that the Fuser nodes need to build or update:
        node_args.setString(Arg::node_directive,      Arg::NukeGeo::node_type_auto);
        node_args.setString(Arg::Scene::file,         filePathForReader());
        node_args.setDouble("output_frame",           output_frame);
        node_args.setBool(  Arg::NukeGeo::read_debug, debug);

        // Let subclasses add their local args:
        _appendNodeContextArgs(node_args);

        exec_ctx.setTime(reader_frame, options->k_frames_per_second);

        // Get reader-specific options for execution context:
        if (options)
        {
            // Get raw text from String knobs:
            Knob* k;
            if (0)
            {
                std::string mask; k = geo->knob(knob_map[SURFACE_MASK_KNOB].readerKnob);
                if (k && k->get_text())
                    mask = k->get_text();
                stringReplaceAll(mask, "\n", " ");
                exec_ctx.setString(knob_map[SURFACE_MASK_KNOB].fuserPrimAttrib, mask);
            }
            //
            {
                std::string mappings; k = geo->knob(knob_map[ATTRIBUTE_MAPPINGS_KNOB].readerKnob);
                if (k && k->get_text())
                    mappings = k->get_text();
                stringReplaceAll(mappings, "\n", " ");
                exec_ctx.setString(knob_map[ATTRIBUTE_MAPPINGS_KNOB].fuserPrimAttrib, mappings);
            }
            //
            exec_ctx.setString(knob_map[CREATION_MODE_KNOB ].fuserPrimAttrib, Fsr::NodePrimitive::load_modes[options->k_prim_creation_mode]);
            //
            exec_ctx.setDouble(knob_map[VELOCITY_SCALE_KNOB].fuserPrimAttrib, options->k_velocity_scale   );
            //
            //exec_ctx.setDouble(knob_map[FRAME_OFFSET_KNOB  ].fuserPrimAttrib, options->k_frame_offset     );
            exec_ctx.setDouble(knob_map[FRAME_ORIGIN_KNOB  ].fuserPrimAttrib, options->k_frame_origin     );
            exec_ctx.setDouble(knob_map[FPS_KNOB           ].fuserPrimAttrib, options->k_frames_per_second);
            //
            // Map import/render subd level selections to standard level count:
            exec_ctx.setString(knob_map[SUBD_IMPORT_LEVEL_KNOB ].fuserPrimAttrib, subd_levels[options->k_subd_import_level]);
            exec_ctx.setString(knob_map[SUBD_RENDER_LEVEL_KNOB ].fuserPrimAttrib, subd_levels[options->k_subd_render_level]);
            exec_ctx.setBool(  knob_map[SUBD_FORCE_ENABLE_KNOB ].fuserPrimAttrib, options->k_subd_force_enable );
            exec_ctx.setBool(  knob_map[SUBD_SNAP_TO_LIMIT_KNOB].fuserPrimAttrib, options->k_subd_snap_to_limit);
            exec_ctx.setString(knob_map[SUBD_TESSELLATOR_KNOB  ].fuserPrimAttrib, subd_tessellators[options->k_subd_tessellator]);
            //
            exec_ctx.setString(knob_map[POINTS_MODE_KNOB   ].fuserPrimAttrib, points_modes[options->k_points_mode]);
            exec_ctx.setBool(  knob_map[USE_COLORS_KNOB    ].fuserPrimAttrib, options->k_use_colors    );
            exec_ctx.setBool(  knob_map[COLOR_FACESETS_KNOB].fuserPrimAttrib, options->k_color_facesets);
            exec_ctx.setBool(  knob_map[COLOR_OBJECTS_KNOB ].fuserPrimAttrib, options->k_color_objects );
            exec_ctx.setBool(  knob_map[APPLY_XFORMS_KNOB  ].fuserPrimAttrib, options->k_apply_xforms  );
            //
            exec_ctx.setString(knob_map[PREVIEW_LOD_KNOB].fuserPrimAttrib, Fsr::NodePrimitive::lod_modes[options->k_proxy_lod_mode]);
            //exec_ctx.setString(knob_map[RENDER_LOD_KNOB ].fuserPrimAttrib, Fsr::NodePrimitive::lod_modes[options->k_render_lod_mode]);
            //
            exec_ctx.setBool(knob_map[DEBUG_KNOB         ].fuserPrimAttrib, options->k_debug);
            exec_ctx.setBool(knob_map[DEBUG_ATTRIBS_KNOB ].fuserPrimAttrib, options->k_debug_attribs);

            //exec_ctx.setString("reader:enabled_facesets", enabled_facesets);
            //exec_ctx.setInt("reader:faceset_index_offset", faceset_index_offset);
        }

    } // add node_ctx args

    if (debug)
    {
        std::cout << " num_objects=" << selected_object_paths.size();
        std::cout << ", num_threads=" << num_threads;
        std::cout << std::endl;
    }

    if (num_threads <= 1)
    {
        // Pass 0 for num_threads so object loop knows it's not multi-threaded:
        thread_proc_cb(0/*thread_index*/, 0/*num_threads*/, &geo_thread_ctx); // just do one
    }
    else
    {
        // Spawn multiple threads (minus one for this thread to directly execute,) then wait for them to finish:
        DD::Image::Thread::spawn(thread_proc_cb, num_threads-1, &geo_thread_ctx);
        // This thread handles the last one:
        thread_proc_cb(num_threads-1/*thread_index*/, num_threads/*num_threads*/, &geo_thread_ctx); // just do one
        //
        DD::Image::Thread::wait(&geo_thread_ctx);
    }

} // geometry_engine()


} // namespace Fsr


// end of Fuser/GeoReader.cpp

//
// Copyright 2019 DreamWorks Animation
//
