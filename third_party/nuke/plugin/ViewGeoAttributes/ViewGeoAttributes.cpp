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

/// @file ViewGeoAttributes.cpp
///
/// @author Jonathan Egstad
///
/// @brief plugin to display GeoInfo contents with support for Fuser prims

/*
    TODO: this is pretty spartan and simple. Needs a GUI upgrade and better
    ways of selecting GeoInfos and prims.
*/


#include <DDImage/GeoOp.h>
#include <DDImage/Scene.h>
#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>
#include <DDImage/List_KnobI.h>

#include <Fuser/NodePrimitive.h>

using namespace DD::Image;


//------------------------------------------------------

// Can't use DD::Image::Attribute::type_string() as of course Foundry
// didn't update it for the latest attribute types...sigh...  :(
const char* attrib_types[] =
{
    "float",
    "vector2", "vector3", "vector4", "normal3",
    "int", "string", "string", "pointer",
    "matrix3", "matrix4",
    0
};

const char* scope_types[] = { "prims", "verts", "points", "object", "xform", "attribs", 0 };


const DD::Image::GroupType group_sort_order[] =
{
    DD::Image::Group_Object,
    DD::Image::Group_Matrix,
    DD::Image::Group_Primitives,
    DD::Image::Group_Vertices,
    DD::Image::Group_Points,
    DD::Image::Group_Attributes
};


/*!
*/
class ViewGeoAttributes : public GeoOp
{
    int  k_select_obj;
    Hash m_geo_hash;

  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }
    /*virtual*/ const char* node_help() const { return __DATE__ " " __TIME__ "\n"
       "Inspect the geometry attributes.";
    }


    ViewGeoAttributes(Node* node) :
        GeoOp(node)
    {
        k_select_obj = 0;
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        Int_knob(f, &k_select_obj, "select_obj", "obj");
            Tooltip(f, "Select an objbect to display primitive contents.");

        std::vector<std::vector<std::string> > dummy_contents;
        List_knob(f, &dummy_contents, "attribute_contents", "");
            // Set knob to be output only:
            SetFlags(f, Knob::OUTPUT_ONLY | Knob::NO_KNOB_CHANGED | Knob::NO_UNDO | Knob::DO_NOT_WRITE | Knob::NO_RERENDER | Knob::STARTLINE);
            SetFlags(f, Knob::STARTLINE); // removes left gutter
            Tooltip(f, "Contents of attributes");

        List_knob(f, &dummy_contents, "fuser_prim_args", "");
            // Set knob to be output only:
            SetFlags(f, Knob::OUTPUT_ONLY | Knob::NO_KNOB_CHANGED | Knob::NO_UNDO | Knob::DO_NOT_WRITE | Knob::NO_RERENDER | Knob::STARTLINE);
#if 0
ClearFlags(f, Knob::STARTLINE);
SetFlags(f, Knob::ENDLINE);
#else
            SetFlags(f, Knob::STARTLINE); // removes left gutter
#endif
            Tooltip(f, "Contents of Fuser prims");
    }


    /*virtual*/
    int knob_changed(Knob* k)
    {
        // Make sure any knob changes force an update:
        if (Op::panel_visible())
            this->updateUI(outputContext());
        return 1;
    }


    /*virtual*/
    void _validate(bool for_real)
    {
        GeoOp::_validate(for_real);

        // Has anything changed?
        Hash geo_hash;
        for (uint32_t i=0; i < DD::Image::Group_Last; ++i)
            geo_hash.append(GeoOp::hash(i));
        if (geo_hash != m_geo_hash)
        {
            m_geo_hash = geo_hash;
            if (Op::panel_visible())
                updateUI(outputContext());
        }
    }


    /*! Can be overriden by ops to update their UI when their control panel is open.
        If you override it return true, otherwise it will not be called again.

        Note that this doesn't reliably work on GeoOps...so we use _validate() to
        force an update when geometry params change.
    */
    /*virtual*/
    bool updateUI(const OutputContext& context)
    {
        if (!input0())
            return true; // call this again

        // Let the GeoOp base class fill in the scene var:
        if (!scene_)
            scene_ = new Scene(); // Allocate a local scene
        assert(scene_);
        build_scene(*scene_);
        const unsigned nObjects = scene_->objects();

        char buf[1024];

        {
            List_KnobI* list = knob("attribute_contents")->listKnob();
            assert(list);
            list->clearColumns();
            list->setColumn(0, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "obj"  ));
            list->setColumn(1, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "name" ));
            list->setColumn(2, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "scope"));
            list->setColumn(3, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "type" ));
            list->setColumn(4, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "value"));

            list->deleteAllItemsNoChanged();
            list->setMinItems(nObjects);

            int row = 0;
            for (unsigned obj=0; obj < nObjects; ++obj)
            {
                const GeoInfo& info = scene_->object(obj);

                if (obj > 0)
                {
                    list->cell(row, 0) = "----------";
                    list->cell(row, 1) = "";
                    list->cell(row, 2) = "";
                    list->cell(row, 3) = "";
                    list->cell(row, 4) = "";
                    ++row;
                }

                snprintf(buf, 1024, "%d", obj); list->cell(row, 0) = buf; // obj index

                const unsigned nAttribs = info.get_attribcontext_count();
                // Put the 'name' attribute first:
                int name_attrib = -1;
                for (unsigned i=0; i < nAttribs; ++i)
                {
                    assert(info.get_attribcontext(i));
                    const AttribContext& attrib = *info.get_attribcontext(i);
                    if (attrib.empty())
                        continue;
                    if (strcmp(attrib.name, DD::Image::kNameAttrName)==0)
                    {
                        name_attrib = i;
                        list->cell(row, 1) = attrib.name; // name
                        list->cell(row, 2) = scope_types[attrib.group]; // scope
                        if (attrib.attribute->size() == 1)
                            list->cell(row, 3) = attrib_types[attrib.type]; // type
                        else
                        {
                            snprintf(buf, 1024, "%s[%d]", attrib_types[attrib.type], attrib.attribute->size());
                            list->cell(row, 3) = buf; // type
                        }
                        if (attrib.type == STRING_ATTRIB)
                            snprintf(buf, 1024, "'%s'", attrib.attribute->string(0));
                        else if (attrib.type == STD_STRING_ATTRIB)
                            snprintf(buf, 1024, "'%s'", attrib.attribute->stdstring(0).c_str());
                        else
                            snprintf(buf, 1024, "[error]");
                        list->cell(row, 4) = buf;
                        ++row;

                        break;
                    }
                }

                // Sort attributes by group:
                for (unsigned j=0; j < DD::Image::Group_Last; ++j)
                {
                    const DD::Image::GroupType do_group = group_sort_order[j];

                    for (unsigned i=0; i < nAttribs; ++i)
                    {
                        if (i == name_attrib)
                            continue; // already done!

                        assert(info.get_attribcontext(i));
                        const AttribContext& attrib = *info.get_attribcontext(i);
                        if (attrib.empty() || attrib.group != do_group)
                            continue;
                        //std::cout << "    "; attrib.print_info(); std::cout << std::endl;

                        const unsigned nVals = attrib.attribute->size();

                        if (i > 0 || name_attrib >= 0)
                            list->cell(row, 0) = ""; // obj index
                        list->cell(row, 1) = attrib.name; // name
                        list->cell(row, 2) = scope_types[attrib.group]; // scope
                        if (nVals == 1)
                            list->cell(row, 3) = attrib_types[attrib.type]; // type
                        else
                        {
                            snprintf(buf, 1024, "%s[%d]", attrib_types[attrib.type], nVals);
                            list->cell(row, 3) = buf; // type
                        }

                        // value
                        buf[0] = 0;
                        if (attrib.type == STRING_ATTRIB)
                        {
                            if (nVals == 1)
                                snprintf(buf, 1024, "'%s'", attrib.attribute->string(0));
                            else
                                snprintf(buf, 1024, "'%s', ...", attrib.attribute->string(0));
                        }
                        else if (attrib.type == STD_STRING_ATTRIB)
                        {
                            if (nVals == 1)
                                snprintf(buf, 1024, "'%s'", attrib.attribute->stdstring(0).c_str());
                            else
                                snprintf(buf, 1024, "'%s', ...", attrib.attribute->stdstring(0).c_str());
                        }
                        else if (attrib.type == INT_ATTRIB)
                        {
                            if (nVals == 1)
                                snprintf(buf, 1024, "%d", attrib.attribute->integer(0));
                            else
                                snprintf(buf, 1024, "%d, ...", attrib.attribute->integer(0));
                        }
                        else if (attrib.type == POINTER_ATTRIB)
                        {
                            if (nVals == 1)
                                snprintf(buf, 1024, "0x%p [mem]", attrib.attribute->pointer(0));
                            else
                                snprintf(buf, 1024, "0x%p [mem], ...", attrib.attribute->pointer(0));
                        }
                        else if (attrib.type == FLOAT_ATTRIB)
                        {
                            if (nVals == 1)
                                snprintf(buf, 1024, "%g", attrib.attribute->flt(0));
                            else
                                snprintf(buf, 1024, "%g, ...", attrib.attribute->flt(0));
                        }
                        else if (attrib.type == VECTOR2_ATTRIB)
                        {
                            if (nVals == 1)
                            {
                                const Vector2& v = attrib.attribute->vector2(0);
                                snprintf(buf, 1024, "[%g %g]", v.x, v.y);
                            }
                            else
                            {
                                const Vector2& v = attrib.attribute->vector2(0);
                                snprintf(buf, 1024, "[%g %g], ...", v.x, v.y);
                            }
                        }
                        else if (attrib.type == VECTOR3_ATTRIB)
                        {
                            if (nVals == 1)
                            {
                                const Vector3& v = attrib.attribute->vector3(0);
                                snprintf(buf, 1024, "[%g %g %g]", v.x, v.y, v.z);
                            }
                            else
                            {
                                const Vector3& v = attrib.attribute->vector3(0);
                                snprintf(buf, 1024, "[%g %g %g], ...", v.x, v.y, v.z);
                            }
                        }
                        else if (attrib.type == NORMAL_ATTRIB)
                        {
                            if (nVals == 1)
                            {
                                const Vector3& v = attrib.attribute->normal(0);
                                snprintf(buf, 1024, "[%g %g %g]", v.x, v.y, v.z);
                            }
                            else
                            {
                                const Vector3& v = attrib.attribute->normal(0);
                                snprintf(buf, 1024, "[%g %g %g], ...", v.x, v.y, v.z);
                            }
                        }
                        else if (attrib.type == VECTOR4_ATTRIB)
                        {
                            if (nVals == 1)
                            {
                                const Vector4& v = attrib.attribute->vector4(0);
                                snprintf(buf, 1024, "[%g %g %g %g]", v.x, v.y, v.z, v.w);
                            }
                            else
                            {
                                const Vector4& v = attrib.attribute->vector4(0);
                                snprintf(buf, 1024, "[%g %g %g %g], ...", v.x, v.y, v.z, v.w);
                            }
                        }
                        else if (attrib.type == MATRIX3_ATTRIB)
                            ;//
                        else if (attrib.type == MATRIX4_ATTRIB)
                            ;//
                        else
                            snprintf(buf, 1024, "[support this type!]");
                        list->cell(row, 4) = buf;
                        ++row;

                    } // attrib look

                } // group loop

            }
            list->knob().changed();
        }

        {
            List_KnobI* list = knob("fuser_prim_args")->listKnob();
            assert(list);

            list->clearColumns();
            list->setColumn(0, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "prim"));
            list->setColumn(1, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "name"));
            list->setColumn(2, DD::Image::List_KnobI::Column(List_KnobI::String_Column, "value"));

            list->deleteAllItemsNoChanged();

            if (k_select_obj >= 0 && k_select_obj < nObjects)
            {
                const GeoInfo& info = scene_->object(k_select_obj);

                const Primitive** PRIMS = info.primitive_array();
                const unsigned nPrims = info.primitives();

                int row = 0;
                int prim_cnt = 0;
                for (unsigned j=0; j < nPrims; ++j)
                {
                    const Primitive* prim = *PRIMS++;
                    assert(prim);

                    if (prim_cnt > 0)
                    {
                        list->cell(row, 0) = "----------";
                        list->cell(row, 1) = "";
                        list->cell(row, 2) = "";
                        ++row;
                    }

                    snprintf(buf, 1024, "%d", j); list->cell(row, 0) = buf; // prim index

                    const Fsr::NodePrimitive* fsr_nprim = dynamic_cast<const Fsr::NodePrimitive*>(prim);
                    const Fsr::Node*          fsr_node  = dynamic_cast<const Fsr::Node*>(prim);
                    if (fsr_nprim && fsr_nprim->node())
                    {
                        // Fuser node, get arg set sorted alphabetically:
                        list->cell(row, 1) = "<Class>";
                        sprintf(buf, "%s[%s]", fsr_nprim->Class(), fsr_nprim->node()->fuserNodeClass());
                        list->cell(row, 2) = buf;
                        ++row;

                        // Get arg set sorted alphabetically:
                        Fsr::KeyValueSortedMap sorted_args;
                        fsr_nprim->node()->args().getAsSorted(sorted_args);
                        for (Fsr::KeyValueSortedMap::const_iterator it=sorted_args.begin(); it != sorted_args.end(); ++it)
                        {
                            list->cell(row, 0) = "";         // prim index
                            list->cell(row, 1) = it->first;  // name
                            list->cell(row, 2) = it->second; // value
                            ++row;
                        }
                        ++prim_cnt;
                    }
                    else if (fsr_node)
                    {
                        // Fuser node:
                        list->cell(row, 1) = "<Class>";
                        list->cell(row, 2) = prim->Class();
                        ++row;

                        // Get arg set sorted alphabetically:
                        Fsr::KeyValueSortedMap sorted_args;
                        fsr_node->args().getAsSorted(sorted_args);
                        for (Fsr::KeyValueSortedMap::const_iterator it=sorted_args.begin(); it != sorted_args.end(); ++it)
                        {
                            list->cell(row, 0) = "";         // prim index
                            list->cell(row, 1) = it->first;  // name
                            list->cell(row, 2) = it->second; // value
                            ++row;
                        }
                        ++prim_cnt;
                    }
                    else
                    {
                        list->cell(row, 1) = "<Class>";
                        list->cell(row, 2) = prim->Class();
                        ++row;
                    }
                }
            }
            list->knob().changed();
        }

        return true; // call this again
    }

};

static Op* build(Node* node) { return new ViewGeoAttributes(node); }
const Op::Description ViewGeoAttributes::description("ViewGeoAttributes", build);

// end of ViewGeoAttributes.cpp

//
// Copyright 2019 DreamWorks Animation
//
