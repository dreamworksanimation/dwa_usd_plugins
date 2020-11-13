//
// Copyright 2020 DreamWorks Animation
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

/// @file TransformGeo2.cpp
///
/// @author Jonathan Egstad


#include <Fuser/SceneXform.h>
#include <Fuser/SceneLoader.h>
#include <Fuser/ObjectFilterKnob.h>

#include <DDImage/GeoOp.h>
#include <DDImage/AxisOp.h>
#include <DDImage/Knobs.h>

using namespace DD::Image;


/*! Fuser replacement for the stock Nuke TransformGeo plugin that adds
    scene file loading capabilities (usd/abc/fbx/etc.)
*/
class TransformGeo2 : public GeoOp,
                      public Fsr::SceneXform,
                      public Fsr::SceneLoader
{
  protected:
    Fsr::ObjectFilter k_object_filter;


  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "Modify or assign the transform of the incoming geometry objects, "
        "optionally using the filter to select one or more objects to affect.\n"
        "\n"
        "The default is to affect the transform of all input objects.";
    }

    /*virtual*/ const char* displayName() const { return "TransformGeo"; }

    /*!
    */
    TransformGeo2(Node* node) :
        GeoOp(node),
        Fsr::SceneXform(),
        Fsr::SceneLoader()
    {
        // Lookat aim location from pivot location is better for geometry:
        k_look_vals.setToDefault(Fsr::LookatVals::AIM_FROM_PIVOT/*aim_location_mode*/);
    }

    //------------------------------------------------------------
    // SceneXform virtual methods:

    //! SceneXform:: Return the parenting input number, or -1 if the parenting source is local. Must implement.
    /*virtual*/ int parentingInput() const { return 1; }

    //! SceneXform:: Return the lookat input number, or -1 if the lookat source is local. Must implement.
    /*virtual*/ int lookatInput() const { return 2; }


    //------------------------------------------------------------
    // SceneExtender/SceneLoader virtual methods:

    //! SceneExtender:: Should return 'this'. Must implement.
    /*virtual*/ Op*    sceneOp() { return this; }

    //! SceneExtender:: If extender is attached to an GeoOp subclass return 'this'.
    /*virtual*/ GeoOp* asGeoOp() { return this; }

    //! Allow subclasses to gain access to sibling functions:
    /*virtual*/ SceneXform*  asSceneXform()   { return this; }
    /*virtual*/ SceneLoader* asSceneLoader()  { return this; }


    //------------------------------------------------------------
    // DD::Image::Op/GeoOp virtual methods.

    /*virtual*/ int minimum_inputs() const { return 3; }
    /*virtual*/ int maximum_inputs() const { return 3; }


    //! Only GeoOp allowed on input 0, only AxisOp allowed on input 1
    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if      (input == 0)
            return GeoOp::test_input(input, op);
        else if (input == 1)
            return dynamic_cast<AxisOp*>(op) != NULL;
        else if (input == 2)
            return dynamic_cast<AxisOp*>(op) != NULL;
        return false;
    }


    //! Return a default GeoOp for input 0, and a NULL for input 1.
    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0)
           return GeoOp::default_input(input);
        return NULL;
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buffer) const
    {
        if      (input == 0)
            return "";
        else if (input == 1)
            return "axis";
        else if (input == 2)
            return "look";
        return NULL;
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //=====================================================================
        Fsr::SceneLoader::addSceneLoaderKnobs(f,
                                              true/*group_open*/,
                                              false/*show_xform_knobs*/,
                                              true/*show_hierarchy*/);

        //---------------------------------------------

        DD::Image::BeginGroup(f, "object_filter", "object filter");
        {
            SetFlags(f, DD::Image::Knob::CLOSED | Knob::DO_NOT_WRITE);
            Fsr::ObjectFilter_knob(f, &k_object_filter, "material_filter", "object filter:");
            Divider(f);
        }
        DD::Image::EndGroup(f);

        GeoOp::knobs(f);
        bool dummy_bool = false;
        Bool_knob(f, &dummy_bool, "transform_normals", "transform normals");
            Tooltip(f, "Apply the transform to the normals in all selected GeoInfo"
                       "\n"
                       "Disabled: it's not necessary to transform the normals unless "
                       "the point locations are being baked, which is not an option "
                       "right now");
            SetFlags(f, Knob::DISABLED);
        Newline(f);

        //---------------------------------------------
        Fsr::SceneXform::addParentingKnobs(f, true/*group_open*/);
        Newline(f);

        Fsr::SceneXform::_addGeoOpTransformKnobs(f);
        Fsr::SceneXform::addLookatKnobs(f);
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        int call_again = 0;

        // Let interfaces handle their changes:
        call_again =  Fsr::SceneXform::knobChanged(k, call_again);
        call_again = Fsr::SceneLoader::knobChanged(k, call_again);
        if (call_again)
            return call_again;

        return GeoOp::knob_changed(k);
    }


    //!
    /*virtual*/
    void _validate(bool for_real)
    {
        Op::_validate(for_real); // validate the inputs

        Fsr::SceneLoader::validateSceneLoader(for_real); // check for any loader errors

        // This will update the input, parent, etc matrices:
        Fsr::SceneXform::_validateGeoOpMatrices(for_real);

        update_geometry_hashes(); // calls get_geometry_hash()
    }


    //! Hash the matrix so that any change causes the points to be invalid.
    /*virtual*/
    void get_geometry_hash()
    {
        GeoOp::get_geometry_hash();

        // Controls that affect the Object matrices:
        k_object_filter.append(geo_hash[Group_Matrix]);
        geo_hash[Group_Matrix].append(m_world_matrix.array(), sizeof(Fsr::Mat4d));
    }


    //! Apply the concat matrix to all the GeoInfos.
    /*virtual*/
    void geometry_engine(Scene&        scene,
                         GeometryList& out)
    {
        GeoOp::geometry_engine(scene, out);
        //std::cout << "TransformGeo2::geometry_engine(" << node_name() << "):" << std::endl;
        //std::cout << "    input_matrix" << m_input_matrix  << std::endl;
        //std::cout << "   parent_matrix" << m_parent_matrix << std::endl;
        //std::cout << "     axis_matrix" << m_axis_matrix   << std::endl;
        //std::cout << "    local_matrix" << m_local_matrix  << std::endl;
        //std::cout << "    world_matrix" << m_world_matrix  << std::endl;

        if (!m_world_matrix.isIdentity())
        {
            // Apply the matrix to selected objects:
            const size_t nObjects = out.size();
            for (size_t obj=0; obj < nObjects; ++obj)
            {
                GeoInfo& info = out[obj];
                if (!k_object_filter.matchObject(info))
                    continue;
                info.matrix = (m_world_matrix * Fsr::Mat4d(info.matrix)).asDDImage();
            }
        }
    }


    /*! The default GeoOp::build_handles will build a Scene object and draw
        it. This is not needed by TransformGeo, as it can just change the OpenGL
        transform and then ask the input to draw. Also the Axis knobs do not draw
        right unless it sets the transform for any parent transform.
    */
    /*virtual*/
    void build_handles(ViewerContext* vtx)
    {
        // Don't display at all if Viewer is in 2D *transform* mode:
        if (vtx->transform_mode() == DD::Image::VIEWER_2D)
            return;

        DD::Image::Matrix4         saved_matrix    = vtx->modelmatrix;
        DD::Image::ViewerConnected saved_connected = vtx->connected();

        // Go up the inputs asking them to build their handles.
        // Do this first so that other ops always have a chance to draw!

        // Parent and look inputs draw in current world space:
        DD::Image::Op::add_input_handle(1, vtx);
        DD::Image::Op::add_input_handle(2, vtx);

        // Draw the geometry if the node's enabled:
        if (!node_disabled())
        {
            this->validate(false); // get transforms up to date

            // If Viewer not in 2D display mode and it's asking to show objects
            // we take ownership of connection so objects only draw once:
            if (vtx->viewer_mode() > DD::Image::VIEWER_2D && vtx->connected() >= SHOW_OBJECT)
            {
                // GeoOp::add_draw_geometry() will construct the output geometry
                // and add callbacks to draw it in the viewer.
                // See notes in GeoOp.h about prep steps.
                DD::Image::GeoOp::add_draw_geometry(vtx);

                // We're the ones drawing objects:
                vtx->connected(DD::Image::CONNECTED);
            }
        }

        // Let other GeoOps draw their knobs, but they shouldn't draw geometry if
        // we're enabled and CONNECTED:
        DD::Image::Op::add_input_handle(0, vtx);

        // Draw our knobs?
        if (k_editable)
        {
            // Our Axis_knob is drawn/manipulated in the parent-space context,
            // so mult in just the parent xform. vtx->modelmatrix will be saved
            // in each build-knob entry:
            vtx->modelmatrix = (Fsr::Mat4d(saved_matrix) * m_input_matrix * m_parent_matrix).asDDImage();

            // Build the local-space handles (Axis_knob):
            DD::Image::Op::build_knob_handles(vtx);
        }

        vtx->modelmatrix = saved_matrix; // don't leave matrix messed up
        vtx->connected(saved_connected); // don't leave connected state messed up
    }


    //! Select just the filtered objects.
    /*virtual*/
    void select_geometry(ViewerContext* vtx,
                         GeometryList&  out)
    {
        // Pass it on so the upstream nodes can do their selections:
        static_cast<GeoOp*>(Op::input0())->select_geometry(vtx, out);
        if (!node_selected())
            return; // no changes if the node's not selected

        // Select only the objects that are filtered:
        const size_t nObjects = out.size();
        for (size_t obj=0; obj < nObjects; ++obj)
        {
            GeoInfo& info = out[obj];
            if (info.selectable && info.display3d > DISPLAY_OFF &&
                k_object_filter.matchObject(info))
            {
                info.selected   = true;
                info.select_geo = this;
            }
            else
            {
                info.selected   = false;
                info.select_geo = NULL;
            }
        }
    }

};


static Op* build(Node* node) { return new TransformGeo2(node); }
const Op::Description TransformGeo2::description("TransformGeo2", build);


// end of TransformGeo2.cpp

//
// Copyright 2020 DreamWorks Animation
//
