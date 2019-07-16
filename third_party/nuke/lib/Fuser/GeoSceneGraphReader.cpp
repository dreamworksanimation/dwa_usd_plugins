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

/// @file Fuser/GeoSceneGraphReader.cpp
///
/// @author Jonathan Egstad

#include "GeoSceneGraphReader.h"
#include "ExecuteTargetContexts.h" // for ScenePathFilters and SceneNodeDescriptions

#include <DDImage/Application.h>
#include <DDImage/Enumeration_KnobI.h>
#include <DDImage/SceneView_KnobI.h>

#include <mutex> // for std::mutex


//#define TRY_PRIMITIVE_PICKING 1
//#define TRY_LIMITING_SCENEGRAPH_UPDATES 1
//#define TRY_CONTEXT_CLEANUP 1


using namespace DD::Image;

namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*static*/ const char* GeoSceneFileArchiveContext::name = "GeoSceneFileArchiveContext";


//! The extra spaces helps set the initial width of the enumeration knob. */
static const char* initial_surface_names[] = { "none                        ", 0 };

enum
{
    SURFACE_MASK_KNOB
};

static const Fsr::KnobMap knob_map[] =
{
    /*  FuserGeoReader knob,   Fsr::NodePrimitive attrib */
    { "surface_mask",         "reader:surface_mask"},
    { 0, 0 }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


typedef std::unordered_map<uint64_t, GeoSceneFileArchiveContext*> GeoSceneFileArchiveContextMap;
static GeoSceneFileArchiveContextMap m_archive_context_map;
static std::mutex                    m_archive_lock;


/*!
*/
GeoSceneFileArchiveContext::GeoSceneFileArchiveContext() :
    cache_data(NULL),
    global_topology_variance(Fsr::Node::ConstantTopology)
{
#ifdef TRY_CONTEXT_CLEANUP
    // Initialize times:
    gettimeofday(&creation_time, NULL/*timezone*/);
    last_access_time = creation_time;
#endif
}


/*!
*/
/*static*/ GeoSceneFileArchiveContext*
GeoSceneFileArchiveContext::findArchiveContext(uint64_t hash)
{
    std::lock_guard<std::mutex> guard(m_archive_lock);
    const GeoSceneFileArchiveContextMap::const_iterator it = m_archive_context_map.find(hash);
    return (it != m_archive_context_map.end()) ? it->second : NULL;
}


/*! This does not check if there's an existing context with the same hash!
    TODO: if there is an existing cache with the same hash but different
    pointers, what do we do? Error? Replace it and delete the old one?
*/
/*static*/ void
GeoSceneFileArchiveContext::addArchiveContext(GeoSceneFileArchiveContext* context,
                                              uint64_t                    hash)
{
    std::lock_guard<std::mutex> guard(m_archive_lock); // lock while we update values
    assert(context);
    m_archive_context_map[hash] = context;
}


#ifdef TRY_CONTEXT_CLEANUP
/*!
*/
void
GeoSceneFileArchiveContext::updateAccessTime()
{
    gettimeofday(&last_access_time, NULL/*timezone*/);
}


/*!
*/
double
GeoSceneFileArchiveContext::getTimeSinceLastAccess()
{
    struct timeval time_now;
    gettimeofday(&time_now, NULL/*timezone*/);
    const double tStart = double(last_access_time.tv_sec) + (double(last_access_time.tv_usec)/1000000.0);
    const double tEnd   = double(time_now.tv_sec        ) + (double(time_now.tv_usec        )/1000000.0);
    //std::cout << "   seconds since last access=" << (tEnd - tStart) << std::endl;
    return (tEnd - tStart);
}
#endif


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! This custom knob's updateUI() method forces the GeoReader's knob_changed()
    to be called to reliably update the SceneGraph UI.
*/
class SceneArchiveUIHelperKnob : public DD::Image::Knob
{
  public:
    /*virtual*/ const char* Class() const { return "SceneArchiveUIHelper"; }

    SceneArchiveUIHelperKnob(DD::Image::Knob_Closure* kc,
                             void*                    parent,
                             const char*              name) :
        DD::Image::Knob(kc, name)
    {
        //std::cout << "SceneArchiveUIHelperKnob::ctor(" << this << ")" << std::endl;
    }

    /*! updateUI gets called often and we can check if the knob is visible
        to indicate whether to update the SceneGraph knob.
    */
    /*virtual*/
    void updateUI(const DD::Image::OutputContext& context)
    {
        //std::cout << "SceneArchiveUIHelperKnob::updateUI(): panel_visible=" << panel_visible() << std::endl;

        if (!panel_visible())
            return; // don't bother if panel is not open

        // Force GeoReader's knob_changed() to get called.
        // TODO: this is wicked unreliable! Sometimes the parent GeoRead will not pass this
        // on to its GeoReader which means the GeoSceneGraphReader never gets told that
        // the node panel tabs have been changed. I've been unable to determine what's
        // stopping the GeoRead from passing the message one, including invalidate(),
        // force_validate(), changing a knob value, etc. These all seem to cache knob changes
        // until something like panel close or another knob GUI gets changed, and then
        // the changes are passed on.
        // So we're forced to make the scenegraph knob update on panel open even if
        // the knob's GUI is not visible...
        DD::Image::Knob::changed();
    }


#ifdef TRY_PRIMITIVE_PICKING
    /*virtual*/
    bool build_handle(DD::Image::ViewerContext* ctx)
    {
        //std::cout << "SceneArchiveUIHelperKnob::build_handle()" << std::endl;
        if (ctx->transform_mode() <= DD::Image::VIEWER_2D)
            return false;

        if (!isVisible())
            return false;

        //ctx->expand_bbox(node_selected(), matrix_.a03, matrix_.a13, matrix_.a23);

        return true;
    }


    static bool select_cb(DD::Image::ViewerContext* ctx,
                          DD::Image::Knob*          k,
                          int                       index)
    {
        DD::Image::GeoOp* geo = dynamic_cast<DD::Image::GeoOp*>(k->op());
        if (!geo)
            return false; // just in case...

        if (!geo->scene())
            geo->setupScene();

        const GeometryList& object_list = *geo->scene()->object_list();
        const uint32_t nObjects = geo->objects();
        if (nObjects == 0)
            return true;

        //std::cout << "select_cb(" << ctx->mouse_x() << " " << ctx->mouse_y() << ")";
        //std::cout << " viewport[" << ctx->viewport().x() << " " << ctx->viewport().y() << " " << ctx->viewport().r() << " " << ctx->viewport().t() << "]" << std::endl;

        // Intersect ray against GeoInfo bbox:

#if 0
        // TODO: this unproject method likely does not work with ortho views!
        // Ray origin is Viewer cam origin which is its inverse:
        const Fsr::Vec3f p0(ctx->cam_matrix().inverse().translation());

        // Unproject mouse location back through viewer projection & xform.
        Fsr::Vec3f p1;
        const DD::Image::Matrix4 c2w = ctx->proj_matrix() * ctx->cam_matrix();
        DD::Image::GLUnproject(ctx->x()+0.5f, ctx->y()+0.5f, -1.0f, c2w, ctx->viewport(), &p1.x, &p1.y, &p1.z);
        Fsr::Vec3f dir(p1 - p0); dir.normalize();
        const Fsr::RayContext Rtx(p0, dir);
#else
        // Use the DD::Image::Raycast construct method as my unproject method
        // doesn't work properly in ortho mode:
        DD::Image::Ray ddRay;
        DD::Image::GetScreenToWorldRay(ctx, ctx->mouse_x(), ctx->mouse_y(), ddRay);
        // Convert to Fuser Ray:
        const Fsr::RayContext Rtx(Fsr::Vec3d(ddRay.src), Fsr::Vec3d(ddRay.dir));
#endif
        //std::cout << "   Rtx" << Rtx << std::endl;

        for (uint32_t j=0; j < nObjects; ++j)
        {
            const DD::Image::GeoInfo& info = object_list[j];
            Fsr::GeoInfoCacheRef geo_cache(j, object_list);
            //if (!geo_cache.isValid())
            //    continue;

            double tmin, tmax;
            if (Fsr::intersectAABB(Fsr::Box3d(info.bbox()), Rtx, tmin, tmax))
            {
                std::cout << "  geo(" << geo->node_name() << ") object " << j << " bbox" << geo_cache.bbox;
                std::cout << " - HIT tmin=" << tmin << ", tmax=" << tmax << std::endl;

                // Check each primitive inside GeoInfo:
                const size_t nPrims = geo_cache.primitives_list->size();
                for (size_t i=0; i < nPrims; ++i)
                {
                    Fsr::Primitive* prim = geo_cache.getFuserPrimitive(i);

                }
            }
        }

        return true;
    }


    /*virtual*/
    void draw_handle(DD::Image::ViewerContext* ctx)
    {
        //std::cout << "SceneArchiveUIHelperKnob::draw_handle()" << std::endl;
        begin_handle(ANYWHERE_MOUSEMOVES, ctx, select_cb, 0/*index*/, 0.0f, 0.0f, 0.0f,
                     DD::Image::ViewerContext::kAddPointCursor);
        end_handle(ctx);
    }
#endif

};


#if 0
/*! At the moment we don't really care about any callbacks as we don't want
    the user to edit the scenegraph at all. However there doesn't appear to
    be a way of stopping the user from fiddling with it accidentally or not,
    so we use this cb as a way to indicate the scenegraph needs to be
    reloaded to wipe out any user edits.

    Clearly we just need to make a new Qt SceneGraph widget...
*/
void scenegraph_cb(SceneView_KnobI::CallbackReason            reason,
                   Knob*                                      k,
                   SceneView_KnobI::WidgetEventCallbackParam& item_values,
                   const char*                                item_name)
{
    DD::Image::SceneView_KnobI* scene_knob = k->sceneViewKnob();
    assert(scene_knob); // shouldn't happen...
    std::cout << "  cb scene_knob=" << scene_knob << " reason=" << reason << std::endl;

    if (reason == SceneView_KnobI::eCustomMenuOptionSelected)
    {
        assert(item_values.size() == 1);
        const size_t item_index = item_values[0];
        std::cout << "   eCustomMenuOptionSelected: index=" << item_index << std::endl;
    }
    else if (reason == SceneView_KnobI::eItemNameChanged)
    {
        assert(item_name != NULL);
        const size_t item_index = item_values[0];
        std::cout << "   eItemNameChanged: index=" << item_index << ", name='" << item_name << "'" << std::endl;
    }
    else if (reason == SceneView_KnobI::eItemMoved)
    {
        assert(item_values.size() > 1);
        const size_t item_index = item_values[0];
        const size_t nItems = (item_values.size()-1);
        std::cout << "   eItemMoved: index=" << item_index << ", nItems=" << nItems << std::endl;
    }
#if 0
    // These are only useful if the Layer +/- buttons are turned on (Knob::SHOW_BUTTONS is set)
    else if (reason == SceneView_KnobI::eItemAdded)
    {
        assert(item_values.size() == 1);
        const size_t item_index = item_values[0];
        std::cout << "   eItemAdded: index=" << item_index << std::endl;
    }
    else if (reason == SceneView_KnobI::eItemRemoved)
    {
        assert(item_values.size() == 1);
        const size_t item_index = item_values[0];
        std::cout << "   eItemRemoved: index=" << item_index << std::endl;
    }
#endif
}
#endif


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
GeoSceneGraphReaderFormat::GeoSceneGraphReaderFormat(DD::Image::ReadGeo* geo) :
    FuserGeoReaderFormat(geo)
{
    //k_object_path          = "";
    k_surface_mask         = defaultSurfaceMask();
    //
    k_scenegraph_scope     = "/";
    k_scenegraph_max_depth = 5;
    //
    k_debug_archive        = false;
}


/*!
*/
/*virtual*/ void
GeoSceneGraphReaderFormat::knobs(DD::Image::Knob_Callback f) 
{
    //std::cout << "GeoSceneGraphReaderFormat::knobs()" << std::endl;
    //String_knob(f, &k_object_path, "object_path", "object path");
    //   SetFlags(f, Knob::EARLY_STORE);

    addTimeOptionsKnobs(f);

    Divider(f);
    Multiline_String_knob(f, &k_surface_mask, knob_map[SURFACE_MASK_KNOB].readerKnob, "surface mask", 3/*lines*/);
        SetFlags(f, Knob::EARLY_STORE);
        ClearFlags(f, Knob::GRANULAR_UNDO); // doesn't appear to do anything for Multiline...
        Tooltip(f,  "Patterns to match object names to using standard glob-style "
                    "wildcards '*', '?'.  There can multiple mask patterns "
                    "separated by spaces.\n"
                    "Turn off objects by preceding the pattern with '-' or '^'. Priority order "
                    "is left to right so if an object is turned off by one mask it can be turned "
                    "on again by an additional mask to the right.\n"
                    "\n"
                    "Examples:\n"
                    " <b>* ^*.ref*</b>  Select all but turn off ones with '.ref'.\n"
                    " <b>*skin_0/m_skin ^*.ref*</b>  Only select the skin mesh.\n"
                    "");

    Newline(f);
    int dummy_int=0;
    Enumeration_knob(f, &dummy_int, initial_surface_names, "object_selection", "mask results");
        SetFlags(f, Knob::DO_NOT_WRITE/* | Knob::DISABLED*/);
        Tooltip(f,  "Result of surface mask selection.\n"
                    "\n"
                    "NOTE - THIS MENU IS JUST FOR REFERENCE, "
                    "SELECTING ITEMS AFFECTS NOTHING");

    Bool_knob(f, &k_debug_archive, "debug_archive", "debug scene file loading");
        SetFlags(f, Knob::STARTLINE);
        Tooltip(f, "Prints scene file archive loading info to the console.");


    Divider(f);
    addImportOptionsKnobs(f);

    Divider(f);
    addPrimOptionsKnobs(f);

    // Gracefully handle the stock Foundry knobs we don't support:
    Obsolete_knob(f, "scene_view", 0);
}


/*!
*/
/*virtual*/ void
GeoSceneGraphReaderFormat::extraKnobs(DD::Image::Knob_Callback f)
{
    //std::cout << "GeoSceneGraphReaderFormat::extraKnobs()" << std::endl;

    Tab_knob(f, "SceneGraph");
    addSceneGraphKnobs(f);

    FuserGeoReaderFormat::extraKnobs(f);
}


/*!
*/
/*virtual*/ void
GeoSceneGraphReaderFormat::addSceneGraphKnobs(DD::Image::Knob_Callback f)
{
    int dummy_int = 0;
    const char* dummy_string = "";
    const char* empty_labels[] = { "<empty>", 0 };

    //------------------------------------------------------------------------------------
    // This custom knob's updateUI() method causes the scene archive knobs to reliably update:
    CustomKnob1(SceneArchiveUIHelperKnob, f, this, "scene_archive_ui_helper");
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE   |
                               DD::Image::Knob::HANDLES_ANYWAY |
                               DD::Image::Knob::KNOB_CHANGED_ALWAYS);

    //------------------------------------------------------------------------------------
    DD::Image::Script_knob(f, "knob scenegraph_scope /[join [lrange [file split [value scenegraph_scope]] 1 end-1] \"/\"]", "Up");
        DD::Image::Tooltip(f, "Moves scope path up one level.");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
    DD::Image::Int_knob(f, &k_scenegraph_max_depth, "scenegraph_max_depth", "max depth");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    	DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_MULTIVIEW |
                               DD::Image::Knob::NO_RERENDER);
        DD::Image::Tooltip(f, "Restricts the maximum node hierarchy depth, starting at the "
                              "end of the scope path, to improve scene loading speed.\n"
                              "\n"
                              "Nuke's scenegraph viewer does not allow progressive expansion "
                              "as a user gradually opens the hierarchy, so to speed up the "
                              "loading of large scenes this control helps limit the number "
                              "of nodes being loaded and potentially not displayed.");
    DD::Image::Help_knob(f, "Poor-man's scenegraph browser visualizing the hierarchy of nodes "
                            "inside a scene file.\n"
                            "(this very rough gui will be improved in the near future...)\n"
                            "\n"
                            "Selecting Object Nodes or Paths:\n"
                            "Selecting a geometry node (a Mesh, Points, etc) will append "
                            "that node's full path to the surface mask. If you want everything at "
                            "current scope level and below to be added to the surface mask push "
                            "the '+' button.\n"
                            "\n"
                            "Browsing Hierarchy & Extending Browsing Scope:\n"
                            "Pushing the '+' buttons next to a node name will open that level "
                            "of the hierarchy showing its contents. You can continue opening "
                            "'directory' nodes down to the 'max depth' level, which are indicated "
                            "by '...' on the end of their name. These nodes have further contents "
                            "underneath them but are hidden.\n"
                            "\n"
                            "Extend the path by selecting the name of the directory node rather "
                            "than the '+' button. This places the selected node path into the "
                            "'scope' path and reloads the scenegraph starting at that level. "
                            "This will cause any parallel directory nodes above this level "
                            "to disappear as they are no longer 'in scope'.\n"
                            "\n"
                            "By progressively selecting lower node names this will take you "
                            "down the hierarchy to the bottom of the scope branch.\n"
                            "\n"
                            "You can manually edit the scope path if you know the destination "
                            "path.");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    	DD::Image::SetFlags(f, DD::Image::Knob::ENDLINE);
    //
    DD::Image::String_knob(f, &k_scenegraph_scope, "scenegraph_scope", "scope");
    	DD::Image::SetFlags(f, /*DD::Image::Knob::DO_NOT_WRITE |*/
                               DD::Image::Knob::STARTLINE | 
                               DD::Image::Knob::NO_MULTIVIEW |
                               DD::Image::Knob::NO_RERENDER);
    DD::Image::Button(f, "append_scope_to_mask", "   +   ");
        DD::Image::Tooltip(f, "Appends the current scope path to the surface mask, including all "
                              "objects underneath it by a '*' tacked on the end.");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    DD::Image::Button(f, "remove_scope_from_mask", "   -   ");
        DD::Image::Tooltip(f, "Removes or adds as subtractive the current scope path to the surface mask, "
                              "including all objects underneath it by a '*' tacked on the end.");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    	DD::Image::SetFlags(f, DD::Image::Knob::ENDLINE);

    //---------------------------------------------------------------------
    DD::Image::SceneView_knob(f, &dummy_int, empty_labels, "scenegraph_nodes", "");
        // SceneView knob options:
        //      SINGLE_SELECTION_ONLY - Knob only allows one item to be selected at a time
        //      SHOW_BUTTONS          - Show Add Layer/Delete Layer buttons
        DD::Image::SetFlags(f, DD::Image::Knob::SINGLE_SELECTION_ONLY);
    	DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE |
                               DD::Image::Knob::ENDLINE |
                               DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::KNOB_CHANGED_ALWAYS |
                               DD::Image::Knob::NO_RERENDER);
        DD::Image::Tooltip(f, "(usage instructions are on the ? button above)");

    //---------------------------------------------------------------------
    DD::Image::String_knob(f, &dummy_string, "scenegraph_selection", DD::Image::INVISIBLE);
    	DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_RERENDER);
    //DD::Image::Text_knob(f, "");
    DD::Image::Button(f, "append_node_to_mask", "    +    ");
        DD::Image::Tooltip(f, "Adds the selected node to the surface mask.");
    //DD::Image::Spacer(f, 20);
    DD::Image::Button(f, "remove_node_from_mask", "    -    ");
        DD::Image::Tooltip(f, "Removes or adds as subtractive the selected node from/to the surface mask.");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    	DD::Image::SetFlags(f, DD::Image::Knob::ENDLINE);

    //---------------------------------------------------------------------
    DD::Image::Divider(f, "Object Mask");
    DD::Image::Link_knob(f, "object_selection", "object_selection_link", "");
    	DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
    DD::Image::Text_knob(f, "  matched results");
    	DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
    	DD::Image::SetFlags(f, DD::Image::Knob::ENDLINE);
    DD::Image::Link_knob(f, "surface_mask", "surface_mask_link", "");
    	DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE |
                               DD::Image::Knob::ENDLINE);
        //DD::Image::ClearFlags(f, DD::Image::Knob::ENDLINE);



}


/*!
*/
/*virtual*/ void
GeoSceneGraphReaderFormat::append(DD::Image::Hash& hash)
{
    //std::cout << "GeoSceneGraphReaderFormat::append(" << this << ")" << std::endl;
    FuserGeoReaderFormat::append(hash);
    //hash.append(k_object_path);
    hash.append(k_surface_mask);
    hash.append(k_debug_archive);
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
GeoSceneGraphReader::GeoSceneGraphReader(ReadGeo* geo, int fd) :
    Fsr::FuserGeoReader(geo, fd)
{
#if 0
    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());
    const char*  surface_mask      = (options)?options->k_surface_mask:"";
    const bool   debug             = (options)?options->k_debug:false;
#endif

#if 0
    if (debug)
    {
        std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
        std::cout << "GeoSceneGraphReader::ctor(" << this << "): reading scene file '" << filePathForReader() << "'";
        std::cout << std::endl;
    }
#endif

#if 0
    // In deferred mode use Fuser::Node creation method to pre-inform
    // the fsrAbcNode class about the archive opening.
    if (options && options->k_prim_creation_mode == Fsr::NodePrimitive::LOAD_DEFERRED)
    {
        Fsr::ArgSet args;
        args.setString(Arg::NukeGeo::file,  filePathForReader());
        args.setString("fsrAbcIO:node:class", "OpenSceneFile");
        Fsr::Node* dummy = Fsr::Node::create("AbcIO", NULL/*parent-node*/, args);
        delete dummy;
    }
#endif

}


/*!
*/
GeoSceneGraphReader::~GeoSceneGraphReader()
{
#if 0
    std::cout << "         GeoSceneGraphReader::dtor(" << this << ")" << std::endl;
    std::cout << "......................................................................................" << std::endl;
#endif
}


/*! Does a lot of gunk to update the scenegraph ui.
    TODO: move this stuff to a custom knob!
*/
/*virtual*/ int
GeoSceneGraphReader::knob_changed(DD::Image::Knob* k)
{
    //std::cout << "  " << fuserIOClass() << "(" << geo << ")::knob_changed(" << ((k)?k->name():"<null>") << ")" << std::endl;
    int ret = -1;

    // Did the user change the scenegraph interface knobs or does the
    // knob view need to be refreshed?
    if (DD::Image::Application::gui)
    {
        const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());
        //const bool   debug         = (options)?options->k_debug:false;
        const bool   debug_archive = (options)?options->k_debug_archive:false;

        // SceneArchiveUIHelper knob has KNOB_CHANGED_ALWAYS enabled so
        // we can be assured of getting UI events like tab selections:
        if (k == &DD::Image::Knob::showPanel || k->name() == "scene_archive_ui_helper")
        {
            updateReaderUI();

            return 1; // SceneArchiveUIHelper always wants to be called again
        }
 
        // Force a reload of the scene file, likely caused by the user pushed the
        // 'reload' button. This usually means invalidating any scene file caching
        // in the IO plugin to force the file to be re-read:
        if (k->name() == "version")
        {
            // Execute but only send a cache-invalidate command:
            Fsr::NodeContext node_ctx;
            Fsr::NodeContext target_ctx;
            //
            node_ctx.setString(Arg::node_directive, Arg::Scene::file_archive_invalidate);
            node_ctx.setString(Arg::Scene::file,    filePathForReader());
            //
            target_ctx.setString(Arg::Scene::file,             filePathForReader());
            target_ctx.setBool(Arg::Scene::file_archive_debug, debug_archive);
            //
            Fsr::Node::executeImmediate(fuserIOClass(),                 /*node_class*/
                                        node_ctx.args(),                /*node_attribs*/
                                        target_ctx,                     /*target_context*/
                                        Fsr::SceneArchiveContext::name  /*target_name*/);

            updateReaderUI();

            return 1;
        }

        // TODO: move all this stuff to a custom scenegraph Qt knob!
        DD::Image::Knob* kScenegraph = geo->knob("scenegraph_nodes");
        if (kScenegraph && kScenegraph->isVisible())
        {
            DD::Image::SceneView_KnobI* scene_knob = kScenegraph->sceneViewKnob();
            assert(scene_knob); // shouldn't happen...

            DD::Image::Knob* kScopePath = geo->knob("scenegraph_scope");
            assert(kScopePath);
            DD::Image::Knob* kNodePath = geo->knob("scenegraph_selection");
            assert(kNodePath);

            // Should only be 1 item if Knob::SINGLE_SELECTION_ONLY is enabled:
            //std::vector<std::string> highlighted_items;
            //scene_knob->getHighlightedItems(highlighted_items);
            //for (size_t i=0; i < highlighted_items.size(); ++i)
            //    std::cout << "  " << i << ": highlighted item '" << highlighted_items[i] << "'" << std::endl;

            //std::vector<uint32_t> imported_items;
            //scene_knob->getImportedItems(imported_items);
            //for (size_t i=0; i < imported_items.size(); ++i)
            //    std::cout << "  " << i << ": imported item '" << imported_items[i] << "'" << std::endl;

            // Always check for a currently hilighted item and clear the selection if none:
            std::string item = scene_knob->getHighlightedItem();
            //std::cout << "----------------------------------------------------------" << std::endl;
            //std::cout << "GeoSceneGraphReader::knob_changed(" << this << ") k='" << k->name() << "'" << std::endl;
            //std::cout << "  selected scene item '" << item << "'" << std::endl;

            bool selected_node = false;

            if (k->name() == "scenegraph_nodes")
            {
                // User selected an item in the scene graph, grab the string
                // and set the scope or node path to it:
                if (!item.empty())
                {
                    std::string path, type;

                    // If path ends in '...' then it's a truncated path:
                    const bool truncated = (item.rfind("...") != std::string::npos);

                    // Find trailing '(<type>)' or trailing '/':
                    char* p = const_cast<char*>(item.c_str());
                    while (*p && *p != ' ' && *p != '(')
                        ++p;
                    if (p == item.c_str())
                    {
                        // root dir:
                        path = "/";
                        type = "Dir";
                    }
                    else
                    {
                        // Extract the path and type:
                        size_t a = (p - item.c_str());
                        path.reserve(item.size()+1);
                        path = "/";
                        path += item.substr(0, a);

                        // Determine type - if path ends in '/' then it's a directory:
                        a = item.find('(', a);
                        if (a != std::string::npos)
                        {
                            if (truncated)
                                type = "Dir"; // ignore type if truncated path
                            else
                            {
                                ++a;
                                size_t b = item.find(')', a);
                                if (b != std::string::npos && b > a)
                                    type = item.substr(a, b-a);
                            }
                        }
                        else if (path[path.size()-1] == '/')
                        {
                            type = "Dir";
                            path[path.size()-1] = 0; // trim '/' off end
                        }
                    }
                    //std::cout << "    path='" << path << "', type='" << type << "'" << std::endl;

                    // Directory types extend the scope path:
                    if (type == "Dir" || type == "Scope" || type == "Xform")
                    {
                        // Restrict the path:
                        kScopePath->set_text(path.c_str());
                        kScopePath->changed();
                        updateReaderUI();

                        kNodePath->set_text("");
                        selected_node = false;
                    }
                    else
                    {
                        // Append path to mask list:
                        kNodePath->set_text(path.c_str());
                        selected_node = true;
                    }
                }
                ret = 1; // we want to be called again

            }
            else if (k->name() == "append_scope_to_mask")
            {
                // Append path to mask list:
                std::string scope = kScopePath->get_text();

                if (scope.empty() || scope == "/")
                    scope = "*";
                else if (scope[scope.size()-1] == '/')
                    scope += "*";
                else
                    scope += "/*";

                editSurfaceMaskKnob(scope,
                                    std::string("<na>")/*type*/,
                                    false/*remove_mode*/);
                selected_node = false;
                ret = 1; // we want to be called again

            }
            else if (k->name() == "remove_scope_from_mask")
            {
                // Append path to mask list:
                std::string scope = kScopePath->get_text();

                if (scope.empty() || scope == "/")
                    scope = "*";
                else if (scope[scope.size()-1] == '/')
                    scope += "*";
                else
                    scope += "/*";

                editSurfaceMaskKnob(scope,
                                    std::string("<na>")/*type*/,
                                    true/*remove_mode*/);
                selected_node = false;
                ret = 1; // we want to be called again

            }
            else if (k->name() == "append_node_to_mask")
            {
                // Append path to mask list:
                editSurfaceMaskKnob(std::string(kNodePath->get_text()),
                                    std::string("<na>")/*type*/,
                                    false/*remove_mode*/);
                selected_node = true;
                ret = 1; // we want to be called again

            }
            else if (k->name() == "remove_node_from_mask")
            {
                // Remove path from mask list, or mark as subtractive:
                editSurfaceMaskKnob(std::string(kNodePath->get_text()),
                                    std::string("<na>")/*type*/,
                                    true/*remove_mode*/);
                selected_node = true;
                ret = 1; // we want to be called again

            }

            geo->knob("append_node_to_mask")->enable(selected_node);
            geo->knob("remove_node_from_mask")->enable(selected_node);

        } // scenegraph visible

    } // Application::gui

    // If not handled call parent:
    if (ret == -1)
        ret = FuserGeoReader::knob_changed(k);

    return ret;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Update the scenegraph and selected object knobs.
*/
/*virtual*/ void
GeoSceneGraphReader::_updateReaderUI()
{
    if (!DD::Image::Application::gui)
        return;

    DD::Image::Hash reader_ui_hash = getReaderUIHash();

    // We're trying to only refresh the scenegraph UI when it's visible and the
    // scene state has changed, so we store a hash just for the scenegraph's
    // ui state separate from the overall reader ui state:
    if (m_scenegraph_ui_hash != reader_ui_hash)
    {
#ifdef TRY_LIMITING_SCENEGRAPH_UPDATES
        // Only update if the scenegraph UI is visible, which SHOULD be as simple as
        // checking the visibility of the scenegraph knob.
        // HOWEVER - GeoReaderFormat-created knobs' isVisible() method don't appear to
        // reliably respect whether their Tab is visible or not which means we only
        // reliably know if the panel is open or not.
        // To work around this we also check the visibility status of some ReadGeo knobs 
        // so we know if the first or last tabs are open and update the SceneGraph if they
        // *aren't* on the assumption the user has switched away to another Tab:
        DD::Image::Knob* kDisplay    = geo->knob("display"); // a ReadGeo knob on first tab
        assert(kDisplay); // shouldn't happen...

        DD::Image::Knob* kScenegraph = geo->knob("scenegraph_nodes"); // on second tab
        assert(kScenegraph); // shouldn't happen...

        DD::Image::Knob* kLabel      = geo->knob("label"); // on last tab
        assert(kLabel); // shouldn't happen...

        if (kScenegraph->isVisible() &&
            (!kDisplay->isVisible() && !kLabel->isVisible()))
#else
        // Screw it - can't get the knob GUI state to report reliably, have the scenegraph
        // update as long as the node panel is open. Due to the hash state checking this
        // only really happens on actual knob changes:
#endif
        {
            // Make sure archive is up to date (this is *not* fast for repeat calls):
            acquireSceneFileArchiveContext();

            //std::cout << "GeoSceneGraphReader::_updateReaderUI(" << this << ")";
            //std::cout << " archive_ctx=" << sceneFileArchiveContext();
            //std::cout << std::endl;

            _updateSceneGraph();

            m_scenegraph_ui_hash = reader_ui_hash;
        }

    }

    // Always refresh the selected objects list since the selected paths are cached:
    if (m_reader_ui_hash != reader_ui_hash)
    {
        // Make sure scene file's been opened (this is fast for repeat calls):
        openSceneFile();

        const std::set<std::string>& selected_paths = getObjectPathsForReader();
        _updateSelectedObjectsMenu(selected_paths);

        m_reader_ui_hash = reader_ui_hash;
    }

}


/*! Fill the scenegraph knob.
    TODO: move this stuff to a custom knob!
*/
/*virtual*/ void
GeoSceneGraphReader::_updateSceneGraph()
{
    //std::cout << "  GeoSceneGraphReader::_updateSceneGraph(" << this << ")" << std::endl;
    if (!DD::Image::Application::gui)
        return;

    DD::Image::Knob* kScenegraph = geo->knob("scenegraph_nodes");
    assert(kScenegraph); // shouldn't happen...

    DD::Image::SceneView_KnobI* scene_knob = kScenegraph->sceneViewKnob();
    assert(scene_knob); // shouldn't happen...

    std::string scope;
    DD::Image::Knob* kScope = geo->knob("scenegraph_scope");
    if (kScope)
        scope = kScope->get_text();
    else
        scope = "/";

    uint32_t path_max_depth = 5;
    if (geo->knob("scenegraph_max_depth"))
        path_max_depth = std::max(1, int(geo->knob("scenegraph_max_depth")->get_value()));

    //std::cout << "    GeoSceneGraphReader::_updateSceneGraph() scope='" << scope << ", max_depth=" << path_max_depth << std::endl;

    std::vector<std::string> node_paths;
    Fsr::NodeDescriptionMap node_descriptions;
    if (!_getNodeDescriptions(filePathForReader(),
                              scope.c_str(),
                              path_max_depth,
                              node_descriptions,
                              false/*debug*/))
        return; // user-abort

    node_paths.reserve(node_descriptions.size());

    // Create the name list for the menu:
    char path[2048];
    for (Fsr::NodeDescriptionMap::const_iterator it=node_descriptions.begin(); it != node_descriptions.end(); ++it)
    {
        const std::string& desc_id = it->first;
        if (desc_id.empty() || desc_id == "/")
            continue; // skip root

        const NodeDescription& desc = it->second;
        if (desc.type.empty())
        {
            if (desc.path == "...")
                snprintf(path, 2048, "%s ...", desc_id.c_str());
            else
                snprintf(path, 2048, "%s", desc_id.c_str());
        }
        else
        {
            if (desc.path == "...")
                snprintf(path, 2048, "%s  (%s) ...", desc_id.c_str(), desc.type.c_str());
            else
                snprintf(path, 2048, "%s  (%s)", desc_id.c_str(), desc.type.c_str());
        }
        node_paths.push_back(std::string(path));
        //std::cout << "  ('" << desc_id << "'['" << desc.type << "') " << path << "'" << std::endl;
    }

    if (node_paths.size() == 0)
    {
        //if (no_nodes_message && no_nodes_message[0])
        //    node_paths.push_back(std::string(no_nodes_message));
        //else
            node_paths.push_back(std::string("<invalid scene path>")); // default message
    }

    //scene_knob->registerWidgetEventCallback(scenegraph_cb, kScenegraph);

    scene_knob->setSelectedItems(std::vector<unsigned int>()); // clear any existing selection
    scene_knob->setImportedItems(std::vector<unsigned int>()); // clear any existing imported list

    scene_knob->setColumnHeader(fileNameForReader());//"file node hierarchy");
    scene_knob->setSelectionMode(DD::Image::SceneView_KnobI::eSelectionModeHighlight);
    scene_knob->viewAllNodes(true);
    scene_knob->autoSelectItems(false);

    scene_knob->menu(node_paths);
    scene_knob->autoExpand(); // this only sometimes works...  :(
    //kScenegraph->changed();
}


//-------------------------------------------------------------------------


/*!
*/
/*virtual*/ void
GeoSceneGraphReader::_updateSelectedObjectsMenu(const std::set<std::string>& selected_paths)
{
    //std::cout << "  GeoSceneGraphReader::_updateSelectedObjectsMenu(" << this << ")" << std::endl;
    if (!DD::Image::Application::gui)
        return;

    std::string buf; buf.reserve(1024);

    // Update file object menu knob:
    std::vector<std::string> name_list;
    name_list.reserve(selected_paths.size());
    std::string display_name;
    display_name.reserve(1024);
    std::string last_parent_path;

    if (selected_paths.empty())
    {
        // Reset it back to initial default (this keeps the knob size wide):
        name_list.push_back(initial_surface_names[0]);
    }
    else
    {
        for (std::set<std::string>::const_iterator it=selected_paths.begin(); it != selected_paths.end(); ++it)
        {
            const std::string& path = *it;
            if (path.empty()) 
                continue;

            // Try to match up parent paths so we can strip them out
            // onto separate lines while leaving the object names on
            // their own:
            std::string parent_path, name;
            Fsr::splitPath(path, parent_path, name);
            if (!parent_path.empty() && parent_path != last_parent_path)
            {
                // Add the parent path as its own line:
                std::string s(parent_path);
                stringReplaceAll(s, " ", "_");
                name_list.push_back(s);
                last_parent_path = parent_path;
            }
#if 1
            // TODO: get info about each node so we can add type prefix:
            buf = "     ";
            buf += name;
            buf += "     ";
            name_list.push_back(buf);
#else
            addObjectToMenu(ref, display_name, true/*only_enabled_facesets*/, name_list);
#endif
        }
    }

    Knob* k = geo->knob("object_selection");
    if (k && k->enumerationKnob())
    {
        //int saved_selection_value = (int)k->get_value();
        k->enumerationKnob()->menu(name_list);
        k->set_value(0);
    }
}


//-------------------------------------------------------------------------


/*! Add/remove a node path to/from the mask list.
    Returns true if the knob was changed.
*/
bool
GeoSceneGraphReader::editSurfaceMaskKnob(const std::string& path,
                                         const std::string& type,
                                         bool               remove_mode)
{
    if (!geo || path.empty())
        return false;

    DD::Image::Knob* kMasks = geo->knob("surface_mask");
    if (!kMasks)
        return false; // shouldn't happen...

    //std::cout << "editSurfaceMaskKnob() path='" << path << "', type='" << type << "'" << std::endl;

    // Append to object mask string:
    std::string mask_text((kMasks->get_text()) ? kMasks->get_text() : "");

    // No existing masks, trivially add it:
    if (mask_text.empty())
    {
        kMasks->set_text(path.c_str());
        return true;
    }

    // If there's existing masks search for a match for this
    // path so we don't add duplicates:
    std::vector<std::string> masks;
    Fsr::splitString(mask_text, " \t\r\n", masks);

    //std::cout << "  find '" << path << "'" << std::endl;

    // Search for a matching mask, ignoring any '-' in front.
    // Search bottom to top to find to most relevant entry:
    bool found   = false;
    bool changed = false;
    for (int i=(int)masks.size()-1; i >= 0; --i)
    {
        const std::string& mask = masks[i];
        const char* s = mask.c_str();
        while (*s == '-' || *s == '+' || *s == '^')
            ++s;

        //std::cout << "    mask'" << s << "'";
        if (path == s)
        {
            // Match, turn it on or off:
            //std::cout << " FOUND";
            found = true;
            const size_t a = mask_text.find(mask);
            assert(a != std::string::npos); // shouldn't happen...
            if (remove_mode && (mask[0] != '-' && mask[0] != '^'))
            {
                //std::cout << ", remove entry" << std::endl;
#if 1
                mask_text.erase(a, mask.size()+1);
#else
                mask_text.insert(mask_text.begin()+a, '-'); // mark as subtracted
#endif
                changed = true;
            }
            else if (!remove_mode && (mask[0] == '-' || mask[0] == '^'))
            {
                //std::cout << ", add entry" << std::endl;
                mask_text.erase(a, 1); // remove leading '-'
                changed = true;
            }
            //else
            //    std::cout << ", no change" << std::endl;
        }
        //else
        //    std::cout << " not found" << std::endl;
    }

    // Not in masks, append it:
    if (!found)
    {
        const size_t len = mask_text.size();
        if (len > 0 && mask_text[len-1] != '\n')
            mask_text += '\n';
        if (remove_mode)
            mask_text += '-';
        mask_text += path;
        changed = true;
    }
    //std::cout << "new mask='" << std::endl << mask_text << "'" << std::endl;

    if (changed)
        kMasks->set_text(mask_text.c_str());

    return changed;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
/*virtual*/ void
GeoSceneGraphReader::append(DD::Image::Hash& hash)
{
    //std::cout << "  GeoSceneGraphReader::append(" << this << ")" << std::endl;

#if 1
    FuserGeoReader::append(hash);
#else
    // TODO: determine if the archive has global animation or not, this
    // will determine if we need to add in the outputcontext's frame.

    // Make sure we rebuild on frame changes since the filename normally
    // doesn't change with a scene file:
    if (m_has_global_animation)
        hash.append(geo->outputContext().frame());
    //std::cout << "GeoSceneGraphReader::append() frame=" << geo->outputContext().frame() << ", hash=" << std::hex << hash.value() << std::dec << std::endl;
#endif
}


/*!
*/
/*virtual*/ void
GeoSceneGraphReader::get_geometry_hash(DD::Image::Hash* geo_hashes)
{
    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());

    //std::cout << "  GeoSceneGraphReader::get_geometry_hash(" << this << ")";
    //std::cout << " global topology-variance=" << (int)m_global_topology_variance;
    //std::cout << ", global is-xform-constant=" << m_global_is_constant_xform;
    //std::cout << std::endl;

    FuserGeoReader::get_geometry_hash(geo_hashes);

    if (options)
    {
        Hash object_hash;
        // TODO: this should take the *selected nodes* into account, not the raw mask!!!!
        //object_hash.append(options->k_object_path);
        object_hash.append(options->k_surface_mask);

        // Change everything if object set change:
        geo_hashes[Group_Vertices  ].append(object_hash);
        geo_hashes[Group_Primitives].append(object_hash);
        geo_hashes[Group_Object    ].append(object_hash);
        geo_hashes[Group_Attributes].append(object_hash);
    }
    //std::cout << "  hash out=" << std::hex << geo_hashes[Group_Points] << std::dec << std::endl;
}


/*!
*/
/*virtual*/ void
GeoSceneGraphReader::_getFileHash(DD::Image::Hash& hash)
{
    FuserGeoReader::_getFileHash(hash);
}


/*! Return the global topology variance flags from the scene archive.
*/
/*virtual*/ uint32_t
GeoSceneGraphReader::_getGlobalTopologyVariance()
{
    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());

    // If there's objects to load prescan them to get their animation capabilites
    // so that we set the global_topology_variance mask before _validate() gets called,
    // otherwise frame to frame read performance may be very bad.
    if (options->k_surface_mask && options->k_surface_mask[0] != 0)
        openSceneFile();

    Fsr::GeoSceneFileArchiveContext* archive_ctx = sceneFileArchiveContext();
    if (archive_ctx)
        return archive_ctx->global_topology_variance;

    return FuserGeoReader::_getGlobalTopologyVariance();
}


/*! Append params that affect the geometry topology state - used to invalidate primitives.
*/
/*virtual*/ void
GeoSceneGraphReader::_getTopologyHash(DD::Image::Hash& hash)
{
    FuserGeoReader::_getTopologyHash(hash);
}


/*! Append params that affect the reader's UI state - used to cause updateReaderUI() to be called.
*/
/*virtual*/ void
GeoSceneGraphReader::_getReaderUIHash(DD::Image::Hash& hash)
{
    FuserGeoReader::_getReaderUIHash(hash);

    // Has anything changed that requires the object set to be rebuilt?
    // TODO: this should take the selected nodes into account, not the raw mask!!!!
    // TODO: if this required anymore?

    hash.append(getFileHash());
    hash.append(geo->knob("surface_mask")->get_text());
    hash.append(geo->knob("ignore_unrenderable")->get_value());
    //
    hash.append(geo->knob("scenegraph_scope")->get_text());
    hash.append(geo->knob("scenegraph_max_depth")->get_value());
    //std::cout << "GeoSceneGraphReader::_getReaderUIHash() hash=0x" << std::hex << hash.value() << std::dec << std::endl;
}


//-------------------------------------------------------------------------


/*! A GeoSceneGraphReader subclass can implement this to return an
    archive context from customized storage.

    Base class returns a GeoSceneFileArchiveContext from the default
    static archive map.
*/
/*virtual*/ GeoSceneFileArchiveContext*
GeoSceneGraphReader::findArchiveContext(uint64_t hash)
{
    return GeoSceneFileArchiveContext::findArchiveContext(hash);
}


/*! Create a new GeoSceneFileArchiveContext to be associated with an archive context hash.
    This is called by GeoSceneGraphReader::_validate() if no previous context
    matching that hash was found.

    A GeoSceneGraphReader subclass can implement this method to can return
    a custom GeoSceneFileArchiveContext subclass. The hash value can be ignored
    if the custom subclass doesn't need it.

    ie 'return new MyCustomArchiveContextClass()'.

    Base class does 'return new GeoSceneFileArchiveContext()'.
*/
/*virtual*/ GeoSceneFileArchiveContext*
GeoSceneGraphReader::createArchiveContext(uint64_t hash)
{
    return new GeoSceneFileArchiveContext();
}


/*! Add an archive context to a storage cache.

    A GeoSceneGraphReader subclass can implement this method to manage the
    storage itself.

    Base class adds it to the default static archive context map.
*/
/*virtual*/ void
GeoSceneGraphReader::addArchiveContext(GeoSceneFileArchiveContext* context,
                                       uint64_t                    hash)
{
    GeoSceneFileArchiveContext::addArchiveContext(context, hash);
}


/*! Handle the acquisition or re-acquisition of a scene file archive cache.
*/
GeoSceneFileArchiveContext*
GeoSceneGraphReader::acquireSceneFileArchiveContext()
{
    //std::cout << "  GeoSceneGraphReader::acquireSceneFileArchiveContext(" << this << ")";

    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());
    const char* surface_mask = (options)?options->k_surface_mask:"";
    //std::cout << ", surface_mask='" << surface_mask << "'";

    // Get the file hash but don't change the m_file_hash state:
    DD::Image::Hash archive_hash = getFileHash();

    // Extract the path pattern masks from the user surface_mask entry.
    // buildNodeMasks will append each extracted pattern string to archive_hash:
    std::vector<std::string> mask_patterns;
    DD::Image::Hash          mask_pattern_hash;
    GeoSceneGraphReader::buildNodeMasks(surface_mask,
                                        mask_patterns,
                                        &mask_pattern_hash);
    archive_hash.append(mask_pattern_hash);
    //std::cout << "  mask_patterns[";
    //for (size_t j=0; j < mask_patterns.size(); ++j)
    //    std::cout << " '" << mask_patterns[j] << "'";
    //std::cout << " ]" << std::endl;

    // Archive file loading can be time consuming due to scene complexity and
    // not-so-great hierarchy construction.
    //
    // To make this as fast as possible we pre-build a 'stage mask' from the
    // object surface masks to restrict the areas of the scene we want the
    // stage to contain.
    //
    // Build stage mask-paths from the beginnings of each surface pattern
    // mask, up until a wildcard character.
    //
    // Each resulting mask addition is appended to the stage hash.
    //
    // examples:
    //  '/foo/bar'                         -> [/foo/bar]
    //  '/foo/bar*'                        -> [/foo]
    //  '/foo/bar/abab* ^*baba /foo2/bar2' -> [/foo/bar, /foo2/bar2]
    //  '*foo*'                            -> [/]
    //
    std::vector<std::string> populate_path_masks;
    {
        // TODO: move this logic to the FuserArchiveIO class!
        if (mask_patterns.size() > 0)
        {
            populate_path_masks.reserve(5);
            std::vector<std::string> segments; segments.reserve(10);
            std::string parent_path; parent_path.reserve(2048);
            for (size_t j=0; j < mask_patterns.size(); ++j)
            {
                const std::string& mask = mask_patterns[j];
                if (mask.size() == 0 || mask[0] == '-')
                    continue;
                else if (mask[0] == '*')
                {
                    //std::cout << "      populate_path_masks += '/'" << std::endl;
                    populate_path_masks.push_back("/");
                    archive_hash.append("/");
                    continue;
                }
                else if (mask[0] == '+')
                {
                    segments.clear();
                    Fsr::stringSplit(mask.substr(1, std::string::npos), "/", segments);
                }
                else
                {
                    segments.clear();
                    Fsr::stringSplit(mask, "/", segments);
                }

                parent_path.clear();
                for (size_t i=0; i < segments.size(); ++i)
                {
                    const std::string& segment = segments[i];
                    //std::cout << "    " << i << ":'" << segment << "'" << std::endl;
                    if (segment.find_first_of("*?") != std::string::npos)
                    {
                        if (i == 0)
                            parent_path = '/';
                        break;
                    }
                    parent_path += '/';
                    parent_path += segment;
                }
                //std::cout << "      populate_path_masks += '" << parent_path << "'" << std::endl;
                populate_path_masks.push_back(parent_path);
                archive_hash.append(parent_path);
            }
        }
    }
    //std::cout << "  populate_path_masks[";
    //for (size_t j=0; j < populate_path_masks.size(); ++j)
    //    std::cout << " '" << populate_path_masks[j] << "'";
    //std::cout << " ]" << std::endl;

    //std::cout << "  new archive hash=" << std::hex << archive_hash.value();

    // Does a context matching this archive hash already exist?
    // Note that the resulting archive context can be different than the one
    // just retrieved via sceneFileArchiveContext():
    // TODO: wrap this all in a lock? Don't think we need to since _validate() is normally unthreaded
    GeoSceneFileArchiveContext* archive_ctx = findArchiveContext(archive_hash.value());
    if (archive_ctx)
    {
        //std::cout << ", found EXISTING archive context=" << archive_ctx;
    }
    else
    {
        // No match, create and add the new context.
        archive_ctx = createArchiveContext(archive_hash.value());
        assert(archive_ctx); // shouldn't happen!
        addArchiveContext(archive_ctx, archive_hash.value());
        //std::cout << ", created NEW archive context=" << archive_ctx;

        {
            /* GeoSceneFileArchiveContext:
                std::string                 scene_file;             //!< File path to scene
                std::string                 scene_context_name;     //!< Arbitrary name for this context
                //
                Fsr::NodeFilterPatternList  node_filter_patterns;   //!< List of filter expressions for node matching
                DD::Image::Hash             node_filter_hash;       //!< Hash value of filter masks
                //
                std::vector<std::string>    populate_path_masks;    //!< Archive path population mask patterns
                //
                std::set<std::string>       selected_paths;         //!< List of enabled node paths
                DD::Image::Hash             selected_paths_hash;    //!< Hash values of selected paths
                //
                std::string                 archive_context_id;     //!< Archive context identifier string
                DD::Image::Hash             archive_context_hash;     //!< Hash value for archive cache
                //
                void*                       cache_data;             //!< Unmanaged pointer to arbitrary subclass data
            */

            archive_ctx->scene_file          = filePathForReader();
            archive_ctx->scene_context_name  = geo->node_name();
            //
            // Copy mask_patterns into NodeFilterPatterns:
            if (mask_patterns.size() == 0)
            {
#if 1
                // Leave the patterns empty, this avoids the archive pre-loading
                // anything:
                archive_ctx->node_filter_patterns.clear();
#else
                // Don't bother with separate mask paths:
                archive_ctx->node_filter_patterns.resize(1);
                Fsr::NodeFilterPattern& node_filter = archive_ctx->node_filter_patterns[0];
                node_filter.name_expr = "/";
                //node_filter.type_expr = ;
#endif
            }
            else
            {
                archive_ctx->node_filter_patterns.resize(mask_patterns.size());
                for (size_t j=0; j < mask_patterns.size(); ++j)
                {
                    // TODO: finish this, 'type_expr' is not being set.
                    Fsr::NodeFilterPattern& node_filter = archive_ctx->node_filter_patterns[j];
                    node_filter.name_expr = mask_patterns[j];
                    //node_filter.type_expr = ;
                }
            }
            archive_ctx->node_filter_hash     = mask_pattern_hash;
            //
            archive_ctx->populate_path_masks  = populate_path_masks;
            //
            archive_ctx->selected_paths.clear();
            archive_ctx->selected_paths_hash.reset();
            //
            archive_ctx->archive_context_id   = "";
            archive_ctx->archive_context_hash = archive_hash;
            //
            archive_ctx->cache_data           = NULL;
            //
            archive_ctx->global_topology_variance = Fsr::Node::ConstantTopology;
        }
    }
    assert(archive_ctx);
#ifdef TRY_CONTEXT_CLEANUP
    archive_ctx->updateAccessTime();
#endif
    //std::cout << "    archive_context_hash=" << std::hex << archive_ctx->archive_context_hash.value() << std::dec;
    //std::cout << std::endl;

    // Update the archive context in the reader and remember the last state:
    updateArchiveContext(archive_ctx, archive_hash.value());

    return archive_ctx;
}


//-------------------------------------------------------------------------


/*! Handle the acquisition or re-acquisition of a scene file archive cache.

    This can be tricky as the GeoReader is often destroyed and re-allocated by
    the parent ReadGeo but GeoOp geometry rebuild flags are not changed making
    it difficult to easily know what we need to do.

    We don't want to be forced to always reload all the prims.

    So we have to check for both a geometry rebuild event and the all-too-common
    new reader case (ie. sceneFileArchiveContext() == NULL.)

*/
/*virtual*/ void
GeoSceneGraphReader::_validate(const bool for_real)
{
    // Retrieve the locally-stored pointer in the GeoReader subclass.
    // The allocation must be safely stored somewhere else!
    GeoSceneFileArchiveContext* current_archive_ctx = sceneFileArchiveContext();

    // Check for both a geometry rebuild event and the common new reader
    // case (ie. sceneFileArchiveContext() == NULL.)
    const bool missing_context = (current_archive_ctx == NULL);

    // TODO: we're recalculating the archive hash on any topology changes, which
    //       seems unnecessary. Check if we only need to recalc hashes if some knob
    //       values change.

    // Ignore point location changes!
    const bool geo_changes = (geo->rebuild(Mask_Primitives) ||
                              geo->rebuild(Mask_Vertices  ) ||
                              geo->rebuild(Mask_Object    ) ||
                              geo->rebuild(Mask_Attributes));
    // std::cout << "  ----------------------------------------------------------------" << std::endl;
    // std::cout << "  GeoSceneGraphReader::validate(" << this << ") for_real=" << for_real;
    // std::cout << ", rebuild_mask=0x" << std::hex << geo->rebuild_mask() << std::dec;
    // std::cout << ", frame=" << geo->outputContext().frame();
    // std::cout << ", nukeNode(" << geo->node() << "): geo_changes=" << geo_changes;
    // std::cout << ", current_archive_ctx=" << current_archive_ctx;
    // std::cout << std::endl;

    if (missing_context || geo_changes)
    {
        //--------------------------------------------------------------------------
        // Rebuild some geometry or do a retrieval of the archive context. If an
        // existing one matches the calc'd hash retrieve it otherwise create a
        // new context.
        //
        // Often the GeoReader gets destroyed and re-allocated so we keep the
        // GeoSceneFileArchiveContext around in a static map for reuse.
        //
        current_archive_ctx = acquireSceneFileArchiveContext();
        assert(current_archive_ctx); // shouldn't happen!!
 
        //****************************************************************************
        // Force GeometryList to be rebuilt - this is IMPORTANT to getting the
        // rebuilt objects to validate properly after a reader delete/re-allocate:
        if (missing_context)
            geo->set_rebuild(Mask_Primitives);
        //****************************************************************************
 
    }
    else
    {
        // No geometry changes and we already have the archive context.
        //std::cout << ", CURRENT archive context=" << current_archive_ctx;
        //if (current_archive_ctx)
        //    std::cout << ", archive_context_hash=" << std::hex << current_archive_ctx->archive_context_hash.value() << std::dec;
        //std::cout << std::endl;
    }

    // Call the base class validate AFTER acquiring the archive context:
    FuserGeoReader::_validate(for_real);
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Build a list of surface masks from a arbitrary mask string.
    Base class splits on whitespace.
*/
/*virtual*/ void
GeoSceneGraphReader::buildNodeMasks(const char*               surface_mask,
                                    std::vector<std::string>& mask_patterns,
                                    DD::Image::Hash*          mask_hash)
{
    mask_patterns.clear();
    if (!surface_mask || !surface_mask[0])
        return;

    std::vector<std::string> masks;
    Fsr::stringSplit(surface_mask, " \t\n", masks);

    mask_patterns.reserve(masks.size());
    for (size_t i=0; i < masks.size(); ++i)
    {
        const std::string& mask = masks[i];
        // Ignore any commented-out masks:
        if (mask[0] == '#')
            continue;
        mask_patterns.push_back(mask);
        if (mask_hash)
            mask_hash->append(mask);
    }
}


/*! Add or modify args to pass to FuserNodePrimitive ctors.

    It's very important to pass in the archive_context_hash value that the
    Fuser IO plugin needs to find a re-usable archive cache, otherwise
    the scene file will be repeatedly opened.

*/
/*virtual*/ void
GeoSceneGraphReader::_appendNodeContextArgs(Fsr::NodeContext& node_ctx)
{
    Fsr::GeoSceneFileArchiveContext* archive_ctx = sceneFileArchiveContext();
    assert(archive_ctx); // shouldn't happen!

#ifdef TRY_CONTEXT_CLEANUP
    archive_ctx->updateAccessTime();
#endif

    node_ctx.setString(Arg::Scene::file, filePathForReader());

    node_ctx.setHash(Arg::Scene::node_filter_hash,    archive_ctx->node_filter_hash.value()   );
    node_ctx.setHash(Arg::Scene::node_selection_hash, archive_ctx->selected_paths_hash.value());

    node_ctx.setString(Arg::Scene::file_archive_context_id,   archive_ctx->archive_context_id        );
    node_ctx.setHash(  Arg::Scene::file_archive_context_hash, archive_ctx->archive_context_hash.value());
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Return a pointer to a cache context to use, if needed.
    Returns false on user-abort.

    For readers that are monolithic objects like Alembic & USD (many frames of
    data stored in a single 'file' instance,) we can save tons of overhead by
    only opening the object once and reusing it as we extract nodes.

    The GeoSceneFileArchiveContext context stores the identifier used to find the
    correct object cache during multiple node instanatiations:
*/
/*virtual*/
bool
GeoSceneGraphReader::_openSceneFile()
{
    Fsr::GeoSceneFileArchiveContext* archive_ctx = sceneFileArchiveContext();
    if (!archive_ctx)
        return true; // don't crash if reader hasn't been validated yet

    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());
    const bool debug         = (options)?options->k_debug:false;
    const bool debug_archive = (options)?options->k_debug_archive:false;

#ifdef TRY_CONTEXT_CLEANUP
    archive_ctx->updateAccessTime();
#endif

    if (0)
    {
        std::cout << "  GeoSceneGraphReader(" << this << ")::_openSceneFile(): reading scene file '" << filePathForReader() << "'";
        std::cout << ", archive_ctx=" << archive_ctx;
        std::cout << ", archive_context_hash=" << std::hex << archive_ctx->archive_context_hash.value() << std::dec;
        std::cout << ", path_masks[";
        for (size_t j=0; j < archive_ctx->node_filter_patterns.size(); ++j)
            std::cout << " '" << archive_ctx->node_filter_patterns[j].name_expr << "'";
        std::cout << " ]" << std::endl;
    }

    // If it's already been loaded return it fast:
    if (archive_ctx->archive_context_hash != Fsr::defaultHashValue &&
        !archive_ctx->archive_context_id.empty())
    {
        if (debug_archive)
        {
            std::cout << "  " << fuserIOClass() << "::openSceneFile(" << this << ")";
            std::cout << " scene file='" << filePathForReader() << "', archive context=" << archive_ctx;
            std::cout << std::endl;
            std::cout << "    EXISTS with archive_context_id='" << archive_ctx->archive_context_id << "'";
            std::cout << ", archive_context_hash=" << std::hex << archive_ctx->archive_context_hash.value() << std::dec;
            std::cout << std::endl;
        }
        return true; // no user-abort
    }

    if (0)//(debug_archive)
    {
        std::cout << fuserIOClass() << "::openSceneFile(" << this << "):";
        std::cout << " reading scene file '" << filePathForReader() << "'";
        std::cout << std::endl;
    }

    // Do an initial archive load to initialize the archive cache:
    if (archive_ctx->node_filter_patterns.size() == 0)
    {
        if (debug_archive)
        {
            std::cerr << "  " << fuserIOClass() << "::openSceneFile(" << this << ")";
            std::cerr << " scene file='" << filePathForReader() << "'";
            std::cerr << " note - not creating an archive context, no object paths specified" << std::endl;
        }
        archive_ctx->selected_paths.clear();
        archive_ctx->selected_paths_hash.append("<empty-paths>");
    }
    else
    {
        Fsr::NodeContext node_ctx;
        node_ctx.setString(Arg::node_directive, Arg::Scene::file_archive_open);
        _appendNodeContextArgs(node_ctx); // sets filename, hashes, etc
#if 0
        {
            std::string population_masks; population_masks.reserve(4096);
            for (size_t j=0; j < archive_ctx->populate_path_masks.size(); ++j)
            {
                const std::string& mask = archive_ctx->populate_path_masks[j];
                if (mask.size() == 0)
                    continue;
                if (!population_masks.empty())
                    population_masks += ",";
                population_masks += mask;
            }
            target_ctx.setString(buildStr("%s:archive:population_masks", fuserIOClass()), population_masks);
        }
#endif

        Fsr::NodeContext target_ctx;
        target_ctx.setBool(Arg::Scene::read_debug,         debug);
        target_ctx.setBool(Arg::Scene::file_archive_debug, debug_archive);
        //target_ctx.setBool(buildStr("%s:debug_archive_loading", fuserIOClass()), debug_archive);

        archive_ctx->scene_file         = filePathForReader();
        archive_ctx->scene_context_name = "";

        Fsr::Node::ErrCtx err = Fsr::Node::executeImmediate(fuserIOClass(),                        /*node_class*/
                                                            node_ctx.args(),                       /*node_attribs*/
                                                            target_ctx,                            /*target_context*/
                                                            Fsr::GeoSceneFileArchiveContext::name, /*target_name*/
                                                            &archive_ctx->archive_context_id,      /*target*/
                                                            archive_ctx,                           /*src0*/
                                                            &archive_ctx->populate_path_masks      /*src1*/);
        if (err.state < 0)
        {
            if (err.state == -1)
                return false; // user-abort

            geo->error("openSceneFile(): %s", err.msg.c_str());
            if (debug_archive)
            {
                std::cerr << fuserIOClass() << "::openSceneFile(" << this << "):";
                std::cerr << " error, " << err.msg << std::endl;
            }
            return true; // no user-abort
        }
        else if (archive_ctx->archive_context_id.empty())
        {
            geo->error("openSceneFile(): failed to load archive");
            if (debug_archive)
            {
                std::cerr << fuserIOClass() << "::openSceneFile(" << this << "):";
                std::cerr << " error, failed to load archive" << std::endl;
            }
            return true; // no user-abort
        }

        if (debug_archive)
        {
            std::cout << "  " << fuserIOClass() << "::openSceneFile(" << this << ")";
            std::cout << " scene file='" << filePathForReader() << "', archive context=" << archive_ctx;
            std::cout << std::endl;
            std::cout << "    INITIALIZED cache with archive_context_id='" << archive_ctx->archive_context_id << "'";
            std::cout << ", archive_context_hash=" << std::hex << archive_ctx->archive_context_hash.value() << std::dec;
            std::cout << std::endl;
        }

        // Get the selected paths up to date now so the archive isn't repeatedly
        // traversed in geometry_engine():
        archive_ctx->selected_paths.clear();
        archive_ctx->selected_paths_hash.reset();
        _getSelectedNodePaths(archive_ctx->node_filter_patterns,
                              archive_ctx->node_filter_hash,
                              archive_ctx->selected_paths,
                              &archive_ctx->selected_paths_hash);

        // If there's objects to load prescan them to get their animation capabilites
        // so that we set the global_topology_variance mask before _validate() gets called,
        // otherwise frame to frame read performance may be *very* bad due to prims being
        // rebuilt:
#if 0
        SceneGraphPrimitive* scene_prim = archive_ctx->getSceneGraphPrimitive();
        scene_prim->updateGlobalTopologyVariance();
#else
        // TODO: for now we do an execute immediate which causes the node to be
        // created, executed, then destroyed. We should be caching the created
        // nodes in the SceneGraphPrimitive so they can be reused.
        for (std::set<std::string>::const_iterator it=archive_ctx->selected_paths.begin();
                it != archive_ctx->selected_paths.end(); ++it)
        {
            Fsr::NodeContext node_ctx;
            Fsr::NodeContext target_ctx;
            //
            node_ctx.setString(Arg::node_directive, Arg::NukeGeo::node_type_auto);
            node_ctx.setString(Arg::Scene::file,    filePathForReader());
            node_ctx.setString(Arg::node_name,      Fsr::fileNameFromPath(*it)); // not really a 'file' name in this context
            node_ctx.setString(Arg::node_path,      *it); // TODO: this path may change to be different than Scene::path
            node_ctx.setString(Arg::Scene::path,    *it);
            node_ctx.setBool(  Arg::NukeGeo::read_debug, debug);
            //
            target_ctx.setString(Arg::Scene::path,         *it);
            target_ctx.setBool(  Arg::Scene::read_debug,   true);
            target_ctx.setBool(  Arg::NukeGeo::read_debug, true);
            //
            uint32_t topology_variance = Fsr::Node::ConstantTopology;
            Fsr::Node::executeImmediate(fuserIOClass(),                               /*node_class*/
                                        node_ctx.args(),                              /*node_attribs*/
                                        target_ctx,                                   /*target_context*/
                                        Arg::NukeGeo::node_topology_variance.c_str(), /*target_name*/
                                        &topology_variance                            /*target*/);

            archive_ctx->global_topology_variance |= topology_variance;
            //std::cout << "  " << *it << ": topo_variance=" << topology_variance << ", gbl=" << archive_ctx->global_topology_variance << std::endl;
        }
#endif
        //std::cout << "  global_topology_variance=" << archive_ctx->global_topology_variance << std::endl;

    }

    return true; // no user-abort
}


//----------------------------------------------------------------------------------------------


/*! Get a list of node descriptions from the scene file.
    Returns false on user-abort.

    
*/
/*virtual*/
bool
GeoSceneGraphReader::_getNodeDescriptions(const char*              file,
                                          const char*              start_path_at,
                                          uint32_t                 path_max_depth,
                                          Fsr::NodeDescriptionMap& node_descriptions,
                                          bool                     debug)
{
    node_descriptions.clear();
    // Don't bother if no path:
    if (!file || !file[0])
        return true; // no user-abort

    if (debug)
    {
        std::cout << "    GeoSceneGraphReader::getNodeDescriptions() file='" << file << "'";
        std::cout << std::endl;
    }

    // Build context (args) to pass to FuserPrims ctors:
    Fsr::NodeContext node_ctx;
    Fsr::NodeContext target_ctx;
    {
        // Fill in the arguments that the Fuser nodes need to build or update:
        //node_ctx.setTime(reader_frame, m_options->k_frames_per_second);

        node_ctx.setString(Arg::node_directive,          Arg::Scene::node_type_contents);
        node_ctx.setString(Arg::Scene::file,             file);
        node_ctx.setString(Arg::Scene::path,             "/"); // primary node path is root(the archive) in this case
        node_ctx.setBool(  Arg::Scene::read_debug,       debug);
        //node_ctx.setBool(Arg::Scene::file_archive_debug, debug_archive);
        //
        target_ctx.setString(Arg::Scene::path,             (start_path_at) ? start_path_at : "/");
        target_ctx.setInt(   Arg::Scene::path_max_depth,   path_max_depth);
        target_ctx.setBool(  Arg::Scene::read_debug,       debug);
        //target_ctx.setBool(Arg::Scene::file_archive_debug, debug_archive);
    }

    Fsr::ScenePathFilters scene_path_filters;
    scene_path_filters.node_filter_patterns = NULL;
    //
    Fsr::SceneNodeDescriptions scene_node_descriptions;
    scene_node_descriptions.node_description_map = &node_descriptions;

    Fsr::Node::ErrCtx err = Fsr::Node::executeImmediate(fuserIOClass(),               /*node_class*/
                                                        node_ctx.args(),              /*node_args*/
                                                        target_ctx,                   /*target_context*/
                                                        scene_node_descriptions.name, /*target_name*/
                                                        &scene_node_descriptions,     /*target*/
                                                        &scene_path_filters           /*src0*/);
    // Set load error on execute failure, but not on user-abort:
    if (err.state == -1)
    {
        return false; // user-abort
    }
    else if (err.state == -2)
    {
        //if (err == -1)
        //    showUserWarning("GeoSceneGraphReader::getNodeDescriptions('%s') error, unknown file type '%s'",
        //                    file_path.c_str(), plugin_type.c_str());
        std::cerr << "    GeoSceneGraphReader::getNodeDescriptions('" << file << "')";
        std::cerr << " error '" << err.msg << "'" << std::endl;
        return true; // no user-abort
    }

    if (debug)
    {
        std::cout << "    GeoSceneGraphReader::getNodeDescriptions('" << file << "'):" << std::endl;
        for (Fsr::NodeDescriptionMap::const_iterator it=node_descriptions.begin(); it != node_descriptions.end(); ++it)
            std::cout << "      '" << it->first << "': type='" << it->second.type << "', path='" << it->second.path << "'" << std::endl;
    }

    return true; // no user-abort
}


//----------------------------------------------------------------------------------------------


/*! Get the list of object names(paths) to read in during geometry_engine.
    Returns the current archive context's 'selected_paths' string set.
*/
/*virtual*/
const std::set<std::string>&
GeoSceneGraphReader::getObjectPathsForReader()
{
    Fsr::GeoSceneFileArchiveContext* archive_ctx = sceneFileArchiveContext();
    if (archive_ctx)
        return archive_ctx->selected_paths;

    return FuserGeoReader::getObjectPathsForReader(); // empty set
}


/*! Fill in the selected paths from the node filter args.
*/
/*virtual*/
void
GeoSceneGraphReader::_getSelectedNodePaths(const Fsr::NodeFilterPatternList& node_filter_patterns,
                                           const DD::Image::Hash&            node_filter_hash,
                                           std::set<std::string>&            selected_paths,
                                           DD::Image::Hash*                  selected_paths_hash)
{
    //std::cout << "  GeoSceneGraphReader(" << this << ")::_getSelectedNodePaths()" << std::endl;

    // Don't bother if selected paths already filled in:
    if (!selected_paths.empty() && selected_paths_hash && *selected_paths_hash != Fsr::defaultHashValue)
        return;

    // Make sure selected_paths_hash is always non-zero after this:
    if (selected_paths_hash)
        selected_paths_hash->append((int)node_filter_patterns.size()+1);

    const GeoSceneGraphReaderFormat* options = dynamic_cast<GeoSceneGraphReaderFormat*>(geo->handler());
    const bool   debug         = (options)?options->k_debug:false;
    const bool   debug_archive = (options)?options->k_debug_archive:false;

    //std::cout << "  node_filter_patterns[";
    //for (size_t j=0; j < node_filter_patterns.size(); ++j)
    //    std::cout << " '" << node_filter_patterns[j].name_expr << "'";
    //std::cout << " ]" << std::endl;

    // Get selected node list:
    if (node_filter_patterns.size() == 0)
    {
        if (debug)
        {
            std::cerr << fuserIOClass() << "::getSelectedNodePaths(" << this << "):";
            std::cerr << " warning, cannot create archive, no object paths specified" << std::endl;
        }
    }
    else
    {
        Fsr::NodeContext node_ctx;
        node_ctx.setString(Arg::node_directive, Arg::NukeGeo::node_type_contents);
        _appendNodeContextArgs(node_ctx);

        Fsr::NodeContext target_ctx;
        target_ctx.setBool(Arg::Scene::read_debug,         debug);
        target_ctx.setBool(Arg::Scene::file_archive_debug, debug_archive);

        Fsr::NodeFilterPatternList* filter_patterns =
            const_cast<Fsr::NodeFilterPatternList*>(&node_filter_patterns);

        // Save previous hash:
        selected_paths.clear();
        Fsr::Node::ErrCtx err = Fsr::Node::executeImmediate(fuserIOClass(),              /*node_class*/
                                                            node_ctx.args(),             /*node_args*/
                                                            target_ctx,                  /*target_context*/
                                                            Fsr::ScenePathFilters::name, /*target_name*/
                                                            &selected_paths,             /*target*/
                                                            filter_patterns              /*src0*/);
        if (err.state < 0)
        {
            if (err.state == -1)
                return; // user-abort

            geo->error("getSelectedNodePaths(): %s", err.msg.c_str());
            if (debug)
            {
                std::cerr << fuserIOClass() << "::getSelectedNodePaths(" << this << "):";
                std::cerr << " error, " << err.msg << std::endl;
            }
        }
    }

}


} // namespace Fsr


// end of Fuser/GeoSceneGraphReader.cpp

//
// Copyright 2019 DreamWorks Animation
//
