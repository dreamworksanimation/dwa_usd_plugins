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

/// @file TransformGeo2.cpp
///
/// @author Jonathan Egstad


#include <Fuser/SceneXform.h>
#include <Fuser/SceneLoader.h>

#include <DDImage/TransformGeo.h>
#include <DDImage/Knobs.h>

using namespace DD::Image;


/*! Fuser replacement for the stock Nuke TranformGeo plugin that adds
    scene file loading capabilities (usd/abc/fbx/etc.)
*/
class TransformGeo2 : public TransformGeo,
                      public Fsr::SceneXform,
                      public Fsr::SceneLoader
{
  protected:
    bool    k_transform_normals;    //!< Apply the xform to the normals in all GeoInfo

    // From DD::Image::TransformGeo:
    //Matrix4 matrix_;            //!< Object matrix - parent * local
    //Matrix4 concat_matrix_;     //!< Concatented input matrix * matrix_
    //GeoOp*  concat_input_;      //!< Op this one concatenates its matrix with

    // From DD::Image::LookAt:
    //Matrix4    my_local;        //!< For the Axis_Knob to store into
    //bool       my_transform_normals;
    //int        my_lookat_axis;
    //bool       my_rotate_x;
    //bool       my_rotate_y;
    //bool       my_rotate_z;
    //bool       my_lookat_use_quat;
    //double     my_lookat_strength;


  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }


    /*!
    */
    TransformGeo2(Node* node) :
        TransformGeo(node),
        Fsr::SceneXform(),
        Fsr::SceneLoader(),
        k_transform_normals(false)
    {
        //
    }

    //------------------------------------------------------------
    // SceneXform virtual methods:

    //! SceneXform:: Return the parenting input number, or -1 if the parenting source is local. Must implement.
    /*virtual*/ int parentingInput() const { return 1; }

    //! SceneXform:: Return the lookat input number, or -1 if the lookat source is local. Must implement.
    /*virtual*/ int lookatInput() const { return 2; }

    //! SceneXform:: If attached Op has an Axis_knob to fill in for the local transform, return it. Must implement.
    ///*virtual*/ Knob* localTransformKnob() const { return this->knob("transform"); }


    //------------------------------------------------------------
    // SceneExtender/SceneLoader virtual methods:

    //! SceneExtender:: Should return 'this'. Must implement.
    /*virtual*/ Op*    sceneOp() { return this; }

    //! SceneExtender:: If extender is attached to an GeoOp subclass return 'this'.
    /*virtual*/ GeoOp* asGeoOp() { return this; }


    //------------------------------------------------------------
    // DD::Image::Op/GeoOp/TransformGeo virtual methods.

    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //=====================================================================
        Fsr::SceneLoader::addSceneLoaderKnobs(f,
                                              true/*group_open*/,
                                              false/*show_xform_knobs*/,
                                              true/*show_hierarchy*/);

        //---------------------------------------------
        Divider(f);
        GeoOp::knobs(f);

        Bool_knob(f, &my_transform_normals, "transform_normals", "transform normals");
            Tooltip(f, "Apply the transform to the normals in all selected GeoInfo");

        //---------------------------------------------
        addParentingKnobs(f, true/*group_open*/);
        DD::Image::Newline(f);

#if 0
        /* Allow protected Op knobs to be set by SceneXform interface by passing
           their target vars in.
        */
        SceneXform::_addOpTransformKnobs(f, &this->my_local);
        //SceneXform::addLookatKnobs(f);
#else
        Axis_knob(f, &my_local, "transform"); // 'my_local' in LookAt class
#endif

        //=====================================================================
        LookAt::knobs(f); // makes a 'Look' tab
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        int call_again = 0;

        // Let interfaces handle their changes:
        //call_again =  Fsr::SceneXform::knobChanged(k, call_again);
        call_again = Fsr::SceneLoader::knobChanged(k, call_again);

        // Let base class handle their changes:
        if (TransformGeo::knob_changed(k))
            call_again = 1;

        return call_again;
    }


    /*! Validate our parent axis first, if any, then apply our local
        transform to that.
    */
    /*virtual*/
    void _validate(bool for_real)
    {
        // Check for any loader errors:
        Fsr::SceneLoader::validateSceneLoader(for_real);

        TransformGeo::_validate(for_real);

#if 0
        // Concatenate scenegraph parent matrix from input1:
        AxisOp* parent = axis_input();
        if (parent)
            matrix_ = parent->matrix() * my_local;
        else
            matrix_ = my_local;

        // See if we can concatenate with input0, do so:
        TransformGeo* previous = dynamic_cast<TransformGeo*>(input0());
        if (!previous)
        {
            // No concatenate:
            concat_input_ = input0();
            concat_matrix_ = matrix_;
        }
        else
        {
            // Concatenate local matrix * parent matrix:
            concat_input_ = previous->concat_input();
            concat_matrix_ = matrix_ * previous->concat_matrix();
        }

        if (lookat_input())
        {
            lookat_input()->validate(for_real);
            perform_lookat();
        }

        // Calculate the geometry hashes:
        update_geometry_hashes();
#endif
    }

    //! Hash the matrix so that any change causes the points to be invalid.
    /*virtual*/
    void get_geometry_hash()
    {
#if 1
        TransformGeo::get_geometry_hash();
#else
        // Get hashes from input0:
        GeoOp::get_geometry_hash();

        // TODO: get the hash from the double-precision matrices instead
        matrix_.append(geo_hash[Group_Matrix]);
        if (lookat_input())
            lookat_matrix_.append(geo_hash[Group_Matrix]);

        geo_hash[Group_Attributes].append(k_transform_normals);
#endif
    }

    //! Apply the concat matrix to all the GeoInfos.
    /*virtual*/
    void geometry_engine(Scene&        scene,
                         GeometryList& out)
    {
#if 1
        TransformGeo::geometry_engine(scene, out);
#else
        // Get the geometry from the concat input GeoOp:
        concat_input_->get_geometry(scene, out);

        // TODO: apply the double-precision matrices instead
        const uint32_t nObjects = out.size();
        for (uint32_t i=0; i < nObjects; i++)
        {
            // TODO: apply object filtering!
            GeoInfo& info = out[i];

	        info.matrix = (concat_matrix_ * info.matrix);

#if 0
            // TODO: what does this even mean unless we're baking the xform into the point locations
            //       as well....?
            const DD::Image::AttribContext* N = info.get_typed_attribcontext("N",  DD::Image::NORMAL_ATTRIB);
            if (N)
#endif
                
        }
#endif
    }

    /*! The default GeoOp::build_handles will build a Scene object and draw
        it. This is not needed by TransformGeo, as it can just change the OpenGL
        transform and then ask the input to draw. Also the Axis knobs do not draw
        right unless it sets the transform for any parent transform.
    */
    /*virtual*/
    void build_handles(ViewerContext* vtx)
    {
#if 1
        TransformGeo::build_handles(vtx);
#else
        // TODO: apply the double-precision matrix in OpenGL instead
#endif
    }
};


static Op* build(Node* node) { return new TransformGeo2(node); }
const Op::Description TransformGeo2::description("TransformGeo3", build);


// end of TransformGeo2.cpp

//
// Copyright 2019 DreamWorks Animation
//
