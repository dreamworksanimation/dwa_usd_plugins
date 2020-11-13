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

/// @file MergeGeo2.cpp
///
/// @author Jonathan Egstad


#include <Fuser/ObjectFilterKnob.h>

#include <DDImage/GeoOp.h>
#include <DDImage/AxisOp.h>
#include <DDImage/CameraOp.h>
#include <DDImage/LightOp.h>
#include <DDImage/Scene.h>
#include <DDImage/Knobs.h>

using namespace DD::Image;


/*! Fuser replacement for the stock Nuke MergeGeo plugin that handles
    Fuser objects and eliminates the slowdown associated with animating
    lights.

    Originally MergeGeo was supposed to also allow the actual merging of
    GeoInfos together rather simply combining lists. That proved too
    difficult as originally Primitives in the GeoInfos tended to be poly
    soup (individually allocated Triangles and Polygons with no connection
    info) rather than logically combined into meshes.

*/
class MergeGeo2 : public GeoOp
{
  protected:
    Fsr::ObjectFilter k_object_filter;


  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "Merge all input geometry into a single list optionally using the "
        "filters to skip or include objects.\n"
        "\n"
        "The default is to merge all input objects.";
    }

    /*virtual*/ const char* displayName() const { return "MergeGeo"; }

    /*!
    */
    MergeGeo2(Node* node) :
        GeoOp(node)
    {
        //
    }

    //------------------------------------------------------------
    // DD::Image::Op/GeoOp virtual methods.

    /*virtual*/ int minimum_inputs() const { return   1; }
    /*virtual*/ int maximum_inputs() const { return 500; }


    //! Allow GeoOps and AxisOps on any input.
    /*virtual*/
    bool test_input(int input,
                    Op* op) const { return (dynamic_cast<GeoOp*>(op)  != NULL ||
                                            dynamic_cast<AxisOp*>(op) != NULL); }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        GeoOp::knobs(f);

        Divider(f);
        Fsr::ObjectFilter_knob(f, &k_object_filter, "merge_filter", "object filter:");
    }


    /*! Combine the hashes from all the inputs.
    */
    /*virtual*/
    void get_geometry_hash()
    {
        // Visit each input GeoOp, validate it to get its geometry
        // hashes up to date, then append each hash 'group' to
        // this GeoOp's:
        for (uint32_t i=0; i < DD::Image::Group_Last; ++i)
            geo_hash[i].reset();

        const uint32_t nInputs = Op::inputs();
        for (uint32_t input=0; input < nInputs; ++input)
        {
            Op* input_op = Op::input(input);
            if (!input_op)
                continue; // skip NULL connections

            input_op->validate(false); // building hashes don't need for_read=true

            // If input's a GeoOp append the hash groups:
            GeoOp* input_geo = dynamic_cast<GeoOp*>(input_op);
            if (input_geo)
            {
                for (uint32_t i=0; i < DD::Image::Group_Last; ++i)
                    geo_hash[i].append(input_geo->hash(i));
                
                continue;
            }

            //********************************************************
            // Don't change Group_Object hash for any other Op type!!!
            //********************************************************

            // Changing Group_Object forces all GeoInfos to rebuild
            // which really slows things down.
            //
            // This has been in Nuke for quite a while and I don't
            // recall the reason why I added it in the first place -
            // possibly because I thought that the surfacing of the
            // object would need to change, but I think that was before
            // changing Group_Object caused a GeoInfo rebuild...  :(

            // Camera or a Light? Test for light first since it's a subclass
            // of CameraOp.
            LightOp* input_light = dynamic_cast<LightOp*>(input_op);
            if (input_light)
            {
                continue;
            }

            CameraOp* input_cam = dynamic_cast<CameraOp*>(input_op);
            if (input_cam)
            {
                continue;
            }

        }

        k_object_filter.append(geo_hash[Group_Object]);
    }


    /*! Combine the GeoInfos from multiple inputs, plus Cameras, Lights
        and Fuser objects.

        Each input GeoOp will add its objects to the GeometryList with
        a range offset the MergeGeo manages so that the input GeoOp's
        not directly aware that what it thinks is 'object 0' is
        actually the 10th, 20th, 55th, etc object in the GeometryList.
      
        This is the purpose of the GeometryList push_range(),
        set_range() and pop_range(). So starting with the first input's
        offset of 0 we call each input GeoOp's geometry_engine() which
        will (usually) add objects starting at the current offset.
      
        I don't quite remember my rational for this but likely I was
        trying to keep the object indices stable for each run through
        a GeoOp's geometry engine, although I can't think of a reason
        why an absolute offset would be a problem...ah well, lost in
        the fog of time...
    */
    /*virtual*/
    void geometry_engine(Scene&        scene,
                         GeometryList& out)
    {
        //std::cout << "MergeGeo2::geometry_engine(" << node_name() << "):" << std::endl;

        // Remember the current range so we can restore it later. We could
        // have done this using local vars since this acts as a recursive
        // function...
        out.push_range();

        // Initialize current range to the GeometryList's offset and object count:
        uint32_t output_offset = out.offset();
        uint32_t added_objects = out.size();

        CameraOp* shooting_camera = NULL;
        const uint32_t nInputs = Op::inputs();
        for (uint32_t input=0; input < nInputs; ++input)
        {
            Op* input_op = Op::input(input);
            if (!input_op)
                continue;  // skip NULL connections

            // If input's a GeoOp let it append its GeoInfos and AxisOps:
            GeoOp* input_geo = dynamic_cast<GeoOp*>(input_op);
            if (input_geo)
            {
                // Assign the range the GeoOp input will start adding
                // objects at, then get the geometry from input:
                out.set_range(output_offset, 0/*nObjects*/);
                //------------------
                input_geo->get_geometry(scene, out);
                //------------------
                const uint32_t nObjects = out.size(); // objects added to current range
                added_objects += nObjects;
                output_offset += nObjects;

                // Update the GeoOp pointers in each GeoInfo to reflect
                // whether it's before or after a merge.
                for (uint32_t obj=0; obj < nObjects; ++obj)
                {
                    GeoInfo& info = out[obj];
                    /* In GeoInfo.h:
                        GeoOp* source_geo;        //!< Last non-merge GeoOp
                        GeoOp* final_geo;         //!< Last GeoOp before a merge
                        GeoOp* select_geo;        //!< Last selectable GeoOp
                        GeoOp* recursion_geo;     //!< GeoOp which started a recursing loop
                    */
                    // If source_geo is not assigned yet set it to the input_geo:
                    if (info.source_geo == NULL)
                        info.source_geo = input_geo;
                }

                continue;
            }

            // Camera or a Light? Test for light first since it's a subclass
            // of CameraOp.
            LightOp* input_light = dynamic_cast<LightOp*>(input_op);
            if (input_light)
            {
                scene.add_light(input_light);
                continue;
            }

            // These are added to the output Scene rather than
            // GeometryList.
            CameraOp* input_cam = dynamic_cast<CameraOp*>(input_op);
            if (input_cam && !shooting_camera)
            {
                scene.camera = shooting_camera = input_cam;
                continue;
            }
        }

        out.pop_range(); // restore the GeometryList's range
        // Offset the new output range to include the total added objects:
        out.set_range(out.offset(), added_objects);
    }


    //! Don't do anything special for build_handles() yet.
    /*virtual*/
    void build_handles(ViewerContext* vtx)
    {
        GeoOp::build_handles(vtx);
    }


    /*! Search up the tree to set the selected nodes and bounding box
        based on whether user has nodes selected and/or open.

        MergeGeo calls each of its input GeoOps with the appropriate
        object range offsets.
    */
    /*virtual*/
    void select_geometry(ViewerContext* vtx,
                         GeometryList&  out)
    {
        // Remember the current range so we can restore it later. We could
        // have done this using local vars since this acts as a recursive
        // function...
        out.push_range();

        // Initialize current range to the GeometryList's offset:
        uint32_t output_offset  = out.offset();

        const uint32_t nInputs = Op::inputs();
        for (uint32_t input=0; input < nInputs; ++input)
        {
            GeoOp* input_geo = dynamic_cast<GeoOp*>(Op::input(input));
            if (!input_geo)
                continue; // skip NULL & non-GeoOps

            // Let input GeoOp do selection:
            input_geo->select_geometry(vtx, out);

            const uint32_t nObjects = input_geo->objects();
            if (nObjects > 0)
            {
                // Offset range:
                out.set_range(output_offset, 0/*nObjects*/);

                output_offset  += nObjects;
            }
        }

        out.pop_range(); // restore the GeometryList's range

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


static Op* MergGeo2_build(Node* node) { return new MergeGeo2(node); }
const Op::Description MergeGeo2::description("MergeGeo2", MergGeo2_build);


//-------------------------------------------------------------------------------


/*! Replacement for Scene node which is just a MergeGeo with a different
    node shape.

    Originally I intended the Scene node to handle other chores such as
    unifying scales from different inputs but in the end that all got
    tossed in favor of it just being a combiner.
*/
class Scene2 : public MergeGeo2
{
  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "Merge all input geometry into a single list optionally using the "
        "filters to skip or include objects.\n"
        "\n"
        "The default is to merge all input objects."
        "\n"
        "(Scene is functionally identical to MergeGeo node just drawn with "
        "round node graph shape.)";
    }

    /*virtual*/ const char* node_shape() const { return "O"; }
    /*virtual*/ const char* displayName() const { return "Scene"; }

    //!
    Scene2(Node* node) : MergeGeo2(node) {}

    /*virtual*/
    void knobs(Knob_Callback f)
    {
        MergeGeo2::knobs(f);
    }

};


static Op* Scene2_build(Node* node) { return new Scene2(node); }
const Op::Description Scene2::description("Scene2", Scene2_build);


// end of MergeGeo2.cpp

//
// Copyright 2020 DreamWorks Animation
//
