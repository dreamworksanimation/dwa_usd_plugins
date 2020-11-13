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

/// @file ApplyMaterial.cpp
///
/// @author Jonathan Egstad


#include <Fuser/ObjectFilterKnob.h>

#include <DDImage/GeoOp.h>
#include <DDImage/Material.h>
#include <DDImage/Knobs.h>


using namespace DD::Image;


/*!
*/
class ApplyMaterial : public GeoOp
{
  protected:
    Fsr::ObjectFilter k_object_filter;


  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
        "Apply a material/shader from input 'mat' to the incoming geometry objects, "
        "optionally using a filter to select one or more objects to affect.\n"
        "\n"
        "The default is to affect the material assignment of all input objects.";
    }


    //!
    ApplyMaterial(Node* node) : GeoOp(node) {}


    //!
    Iop* inputIop() const { return static_cast<Iop*>(Op::input1()); }


    /*virtual*/ int minimum_inputs() const { return 2; }
    /*virtual*/ int maximum_inputs() const { return 2; }


    //! Only GeoOp allowed on input 0, only Iop allowed on input 1.
    /*virtual*/
    bool test_input(int input,
                    Op* op) const
    {
        if (input == 0)
            return dynamic_cast<GeoOp*>(op) != NULL;
        if (input == 1)
            return dynamic_cast<Iop*>(op) != NULL;
        return false;
    }


    //! Return a default GeoOp on input 0 and a default Iop on input 1.
    /*virtual*/
    Op* default_input(int input) const
    {
        if (input == 0)
           return GeoOp::default_input(input);
        if (input == 1)
           return Iop::default_input(this);
        return NULL;
    }


    /*virtual*/
    const char* input_label(int   input,
                            char* buffer) const
    {
        if (input == 0)
            return "";
        if (input == 1)
            return "mat";
        return NULL;
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        GeoOp::knobs(f);

        Divider(f);
        Fsr::ObjectFilter_knob(f, &k_object_filter, "material_filter", "object filter:");

        Obsolete_knob(f, "object_attrib", "knob material_filter_attrib $value");
        Obsolete_knob(f, "object_mask",   "knob material_filter_mask   $value");
    }


    /*virtual*/
    void get_geometry_hash()
    {
        GeoOp::get_geometry_hash(); // Get input0's hashes up-to-date

        k_object_filter.append(geo_hash[Group_Object]);

        // Make the input node address cause a hash change:
        const ::Node* node = input0()->node();
        geo_hash[Group_Object].append(&node, sizeof(void*));

        // Input material always affects Group_Object:
        geo_hash[Group_Object].append(inputIop()->hash());

        // If input's a material, add its geometry hashes which may change other hash groups:
        //Material* mat = dynamic_cast<Material*>(inputIop());
        //if (mat)
        //    mat->get_geometry_hash(geo_hash);
    }


    //! Apply material to object matching filter.
    /*virtual*/
    void geometry_engine(Scene& scene, GeometryList& out)
    {
        GeoOp::geometry_engine(scene, out);
        //std::cout << "ApplyMaterial::geometry_engine(" << this << "):";
        //std::cout << " rebuild_mask=0x" << std::hex << rebuild_mask() << std::dec << std::endl;

        // Assign the material to selected objects.
        // We don't need to create a local GeoOp cache for the modification as
        // it affects to the GeoInfo object on the way back down the tree:
        Iop* iop = inputIop();
        const size_t nObjects = out.size();
        for (size_t obj=0; obj < nObjects; ++obj)
        {
            GeoInfo& info = out[obj];
            if (!k_object_filter.matchObject(info))
                continue;

            info.material = iop;
        }
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


static Op* ApplyMaterial_build(Node* node) { return new ApplyMaterial(node); }
const Op::Description ApplyMaterial::description("ApplyMaterial", ApplyMaterial_build);


// end of ApplyMaterial.cpp

//
// Copyright 2020 DreamWorks Animation
//
