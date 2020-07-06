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

/// @file FuserUsdShader.cpp
///
/// @author Jonathan Egstad


#include "FuserUsdShader.h"

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/Primitive.h> // for SceneNodesContext
#include <Fuser/NukeKnobInterface.h>



namespace Fsr {

static const Fsr::KeyValueMap usd_to_knob_type =
{
    {"int",          "int"   },
    {"float",        "double"},
    {"double",       "double"},
    //
    {"token",        "string"},
    {"asset",        "file"  },
    //
    {"float2",       "vec2"  },
    {"double2",      "vec2"  },
    {"color2",       "vec2"  },
    {"color2f",      "vec2"  },
    {"color2d",      "vec2"  },
    //
    {"float3",       "vec3"  },
    {"double3",      "vec3"  },
    {"color3",       "vec3"  },
    {"color3f",      "vec3"  },
    {"color3d",      "vec3"  },
    //
    {"normal",       "vec3"  },
    {"normal3",      "vec3"  },
    {"normal3f",     "vec3"  },
    {"normal3d",     "vec3"  },
    //
    {"float4",       "vec4"  },
    {"double4",      "vec4"  },
    {"color4",       "vec4"  },
    {"color4f",      "vec4"  },
    {"color4d",      "vec4"  },
    //
    {"float2[]",     "vec2[]"},
    {"double2[]",    "vec2[]"},
    {"color2[]",     "vec2[]"},
    {"color2f[]",    "vec2[]"},
    {"color2d[]",    "vec2[]"},
    //
    {"float3[]",     "vec3[]"},
    {"double3[]",    "vec3[]"},
    {"color3[]",     "vec3[]"},
    {"color3f[]",    "vec3[]"},
    {"color3d[]",    "vec3[]"},
    //
    {"float4[]",     "vec4[]"},
    {"double4[]",    "vec4[]"},
    {"color4[]",     "vec4[]"},
    {"color4f[]",    "vec4[]"},
    {"color4d[]",    "vec4[]"},
    //
    {"matrix4",      "mat4"  },
    {"matrix4d",     "mat4"  },
};

const std::string&
getShaderKnobType(const std::string& usd_type)
{
    Fsr::KeyValueMap::const_iterator it = usd_to_knob_type.find(usd_type);
    if (it != usd_to_knob_type.end())
        return it->second;
    return Fsr::empty_string;
}



/*!
*/
FuserUsdShadeShaderNode::FuserUsdShadeShaderNode(const Pxr::UsdStageRefPtr&  stage,
                                                 const Pxr::UsdPrim&         shader_prim,
                                                 const Fsr::ArgSet&          args,
                                                 FuserUsdShadeNodeGraphNode* group,
                                                 Fsr::Node*                  parent) :
    FuserUsdNode(stage),
    Fsr::ShaderNode(args, parent)
{
    //std::cout << "  FuserUsdShadeShaderNode::ctor(" << this << ") '" << shader_prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdShadeShader:
    if (shader_prim.IsValid() && shader_prim.IsA<Pxr::UsdShadeShader>())
    {
        m_shader_schema = Pxr::UsdShadeShader(shader_prim);

        // Shader 'Class' name is called the id token:
        Pxr::TfToken shader_class;
        if (!m_shader_schema.GetShaderId(&shader_class))
            shader_class = Pxr::TfToken("unknown");
        this->setString("shader:class", shader_class.GetString());

        if (debug())
        {
            std::cout << "  --------------------------------------------------" << std::endl;
            std::cout << "  UsdShader('" << shader_prim.GetName() << "') class='" << shader_class << "':";
            printPrimAttributes(" ", shader_prim, true/*verbose*/, std::cout);
            std::cout << std::endl;
            std::cout << "    inputs[";
            const std::vector<Pxr::UsdShadeInput> inputs(m_shader_schema.GetInputs());
            for (size_t i=0; i < inputs.size(); ++i)
            {
                const Pxr::UsdShadeInput& input = inputs[i];
                std::cout << " " << i << ":'" << input.GetBaseName() << "'[" << input.GetTypeName().GetType() << "]";
            }
            std::cout << " ]" << std::endl;
            std::cout << "    outputs[";
            const std::vector<Pxr::UsdShadeOutput> outputs(m_shader_schema.GetOutputs());
            for (size_t i=0; i < outputs.size(); ++i)
            {
                const Pxr::UsdShadeOutput& output = outputs[i];
                std::cout << " " << i << ":'" << output.GetBaseName() << "'[" << output.GetTypeName().GetType() << "]";
            }
            std::cout << " ]" << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "  FuserUsdShadeShader::ctor(" << this << "): ";
            std::cerr << "warning, node '" << shader_prim.GetPath() << "'(" << shader_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }


    const std::vector<Pxr::UsdShadeInput> inputs(m_shader_schema.GetInputs());
    const uint32_t nInputs = (uint32_t)inputs.size();
    setNumInputs(nInputs);
    for (uint32_t input_index=0; input_index < nInputs; ++input_index)
    {
        const Pxr::UsdShadeInput& input = inputs[input_index];
        const std::string input_name = input.GetBaseName();
        const std::string input_type = input.GetTypeName().GetType().GetTypeName();

        const std::string& fsr_type = getShaderKnobType(input.GetTypeName().GetAsToken().GetString());

        //std::cout << " " << input_index << ":'" << input_name << "'";
        //std::cout << "[" << input_type << "]";
        //std::cout << " typename=" << input.GetTypeName();
        //std::cout << " fsr_type=" << fsr_type;
        //std::cout << std::endl;

        // Configure the input:
        // TODO: get metadata out of inputs so we can capture colorspace hints, etc.
        setInput(input_index,
                 input_name.c_str()/*name*/,
                 fsr_type.c_str()/*type*/,
                 ""/*value*/);


        Pxr::UsdShadeConnectableAPI source;
        Pxr::TfToken                sourceName;
        Pxr::UsdShadeAttributeType  sourceType;
        if (Pxr::UsdShadeConnectableAPI::GetConnectedSource(input,
                                                            &source,
                                                            &sourceName,
                                                            &sourceType) &&
            sourceType == Pxr::UsdShadeAttributeType::Output)
        {
            // Recursively walk up the input tree creating connected shaders:
            Pxr::UsdPrim      input_prim      = source.GetPrim();
            const std::string input_prim_name = input_prim.GetName().GetString();
            //std::cout << "    ----------------------------------------" << std::endl;
            //std::cout << "    source " << input_index << ":'" << input.GetBaseName() << "'[" << input.GetTypeName().GetType() << "]";
            //std::cout << " '" << input_prim_name << "'[" << sourceName << "]" << std::endl;

            // The shader creation args are slimmed down:
            Fsr::ArgSet sdr_args;

            // TODO: make a wrapper method for building these standard node args:
            {
                sdr_args.setString(Arg::node_name, input_prim_name);

                // Usd scene path:
                sdr_args.setString(Arg::Scene::path, input_prim.GetPath().GetString());

                // Local Fsr node path:'fsr:node:path' is the node + child node path:
                const std::string fsr_node_path = Fsr::buildPath(Fsr::Node::getPath(), input_prim_name);
                sdr_args.setString(Arg::node_path, fsr_node_path);

                if (getInt(Arg::node_debug, 0) > 0)
                    sdr_args.setInt(Arg::node_debug, 1/*DEBUG_1*/);
            }

            // If there's a group only create the node if not already in it:
            Fsr::ShaderNode* input_shadernode = NULL;
            if (group)
            {
                // Does it already exist in the group?
                input_shadernode = group->getChildByName(input_prim_name);
                if (!input_shadernode)
                {
                    input_shadernode = new FuserUsdShadeShaderNode(getStage(),
                                                                   input_prim,
                                                                   sdr_args,
                                                                   group,
                                                                   group/*parent*/);
                    group->addChild(input_shadernode);
                }
            }
            else
            {
                input_shadernode = new FuserUsdShadeShaderNode(getStage(),
                                                               input_prim,
                                                               sdr_args,
                                                               NULL/*group*/,
                                                               this/*parent*/);
                this->addChild(input_shadernode);
            }
#if DEBUG
            assert(input_shadernode); // shouldn't happen...
#endif

            connectInput(input_index/*input_index*/,
                         input_shadernode/*to_shader*/,
                         sourceName.GetString().c_str()/*to_shader_output_name*/);

            
        }
        else
        {
            // Assign local controls (knobs) not connected to inputs.
            //    (we'll use Nuke nomenclature here for no good reason... :) )
            Pxr::UsdAttribute attr = input.GetAttr();

            std::stringstream valstr;

            std::vector<double> times;
            if (!attr.GetTimeSamples(&times) || times.size() == 0)
                times.resize(1, Pxr::UsdTimeCode::Default().GetValue());

            // TODO: support animated Fuser:ShaderNode values!!!!!
            const uint32_t nSamples = 1;//(uint32_t)times.size();
            for (uint32_t sample=0; sample < nSamples; ++sample)
            {
                const Pxr::UsdTimeCode time = times[sample];

                Pxr::VtValue vt;
                if (!attr.Get(&vt, time))
                    continue; // error getting value, skip this input

                //std::cout << "    ";
                //if (group)
                //    std::cout << group->getName() << ".";
                //std::cout << getName() << "." << knob_name;
                //const Pxr::TfType knob_type = input.GetTypeName().GetType();
                //std::cout << "[" << knob_type.GetTypeName() << "]";

                /*
                    USD defines these connection types in the Sdr lib (Shader Definition Registry.)
                    https://graphics.pixar.com/usd/docs/api/sdr_page_front.html

                    // Non interpolating:
                    Int,      "int"
                    String,   "string"

                    // Interpolateable (per-texel, ie texture-mappable)
                    Float,    "float", "float2", "float3"
                    Color,    "color", "color2", "color3"
                    Point,    "point"
                    Normal,   "normal"
                    Vector,   "vector"
                    Matrix,   "matrix"

                    // Abstract types:
                    Struct,   "struct"
                    Terminal, "terminal"
                    Vstruct,  "vstruct"
                    Unknown,  "unknown"

                    MaterialX has these (from pxr/usd/plugin/usdMtlx/utils.cpp)
                        static const auto table =
                            std::unordered_map<std::string, UsdMtlxUsdTypeInfo>{
                               { "boolean",       TUPLEX(Bool,          true,  noMatch) },
                               { "integer",       TUPLE3(Int,           true,  Int)     },
                               { "integerarray",  TUPLE3(IntArray,      true,  Int)     },
                               { "color2array",   TUPLEX(Float2Array,   false, noMatch) },
                               { "color2",        TUPLEX(Float2,        false, noMatch) },
                               { "color3array",   TUPLE3(Color3fArray,  true,  Color)   },
                               { "color3",        TUPLE3(Color3f,       true,  Color)   },
                               { "color4array",   TUPLEX(Color4fArray,  true,  noMatch) },
                               { "color4",        TUPLEX(Color4f,       true,  noMatch) },
                               { "filename",      TUPLE3(Asset,         true,  String)  },
                               { "floatarray",    TUPLE3(FloatArray,    true,  Float)   },
                               { "float",         TUPLE3(Float,         true,  Float)   },
                               { "geomnamearray", TUPLEX(StringArray,   false, noMatch) },
                               { "geomname",      TUPLEX(String,        false, noMatch) },
                               { "matrix33",      TUPLEX(Matrix3d,      true,  noMatch) },
                               { "matrix44",      TUPLE3(Matrix4d,      true,  Matrix)  },
                               { "stringarray",   TUPLE3(StringArray,   true,  String)  },
                               { "string",        TUPLE3(String,        true,  String)  },
                               { "vector2array",  TUPLEX(Float2Array,   false, noMatch) },
                               { "vector2",       TUPLEX(Float2,        false, noMatch) },
                               { "vector3array",  TUPLE3(Vector3fArray, true,  Vector)  },
                               { "vector3",       TUPLE3(Vector3f,      true,  Vector)  },
                               { "vector4array",  TUPLEX(Float4Array,   false, noMatch) },
                               { "vector4",       TUPLEX(Float4,        false, noMatch) },
                            };


                */

                if      (vt.IsHolding<bool>())
                {
                    const bool v = vt.Get<bool>();
                    valstr << ((v) ? "1" : "0");
                }
                else if (vt.IsHolding<int>())
                {
                    const int v = vt.Get<int>();
                    valstr << v;
                }
                else if (vt.IsHolding<float>())
                {
                    const float v = vt.Get<float>();
                    char buf[64];
                    std::snprintf(buf, 64, "%.20g", v);
                    valstr << buf;
                }
                else if (vt.IsHolding<double>())
                {
                    const double v = vt.Get<double>();
                    char buf[64];
                    std::snprintf(buf, 64, "%.20g", v);
                    valstr << buf;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::GfVec2f>())
                {
                    const Fsr::Vec2f v = reinterpret_cast<const Fsr::Vec2f&>(vt.Get<Pxr::GfVec2f>());
                    valstr << v.x << " " << v.y;
                }
                else if (vt.IsHolding<Pxr::GfVec2d>())
                {
                    const Fsr::Vec2d v = reinterpret_cast<const Fsr::Vec2d&>(vt.Get<Pxr::GfVec2d>());
                    valstr << v.x << " " << v.y;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::GfVec3f>())
                {
                    const Fsr::Vec3f v = reinterpret_cast<const Fsr::Vec3f&>(vt.Get<Pxr::GfVec3f>());
                    valstr << v.x << " " << v.y << " " << v.z;
                }
                else if (vt.IsHolding<Pxr::GfVec3d>())
                {
                    const Fsr::Vec3d v = reinterpret_cast<const Fsr::Vec3d&>(vt.Get<Pxr::GfVec3d>());
                    valstr << v.x << " " << v.y << " " << v.z;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::GfVec4f>())
                {
                    const Fsr::Vec4f v = reinterpret_cast<const Fsr::Vec4f&>(vt.Get<Pxr::GfVec4f>());
                    valstr << v.x << " " << v.y << " " << v.z << " " << v.w;
                }
                else if (vt.IsHolding<Pxr::GfVec4d>())
                {
                    const Fsr::Vec4d v = reinterpret_cast<const Fsr::Vec4d&>(vt.Get<Pxr::GfVec4d>());
                    valstr << v.x << " " << v.y << " " << v.z << " " << v.w;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::GfMatrix4d>())
                {
                    const Fsr::Mat4d v = reinterpret_cast<const Fsr::Mat4d&>(vt.Get<Pxr::GfMatrix4d>());
                    valstr << v;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::TfToken>())
                {
                    const std::string v = vt.Get<Pxr::TfToken>().GetString();
                    valstr << v;
                }
                else if (vt.IsHolding<Pxr::SdfAssetPath>())
                {
                    const std::string v = vt.Get<Pxr::SdfAssetPath>().GetResolvedPath();
                    valstr << v;
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec2f> >())
                {
                    const Pxr::VtArray<Pxr::GfVec2f> vals = vt.Get<Pxr::VtArray<Pxr::GfVec2f> >();
                    //std::vector<float> out;
                    //memcpy(out.data(), vals.data(), vals.size()*sizeof(float));
std::cout << "[vec2farray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec2d> >())
                {
                    const Pxr::VtArray<Pxr::GfVec2d> vals = vt.Get<Pxr::VtArray<Pxr::GfVec2d> >();
std::cout << "[vec2darray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec3f> >())
                {
                    const Pxr::VtArray<Pxr::GfVec3f> vals = vt.Get<Pxr::VtArray<Pxr::GfVec3f> >();
std::cout << "[vec3farray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec3d> >())
                {
                    const Pxr::VtArray<Pxr::GfVec3d> vals = vt.Get<Pxr::VtArray<Pxr::GfVec3d> >();
std::cout << "[vec3darray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                //---------------------------------------------------------------------------
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec4f> >())
                {
                    const Pxr::VtArray<Pxr::GfVec4f> vals = vt.Get<Pxr::VtArray<Pxr::GfVec4f> >();
std::cout << "[vec4farray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                else if (vt.IsHolding<Pxr::VtArray<Pxr::GfVec4d> >())
                {
                    const Pxr::VtArray<Pxr::GfVec4d> vals = vt.Get<Pxr::VtArray<Pxr::GfVec4d> >();
std::cout << "[vec4darray]=[";
for (size_t i=0; i < vals.size(); ++i)
    std::cout << " " << vals[i];
std::cout << " ]";
                }
                else
                {
                    //std::cout << "[UNHANDLED]";
                }
                //std::cout << std::endl;
            }

            setInputValue(input_index, valstr.str().c_str());
        }
    }
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdShadeShaderNode::_validateState(const Fsr::NodeContext& args,
                                        bool                    for_real)
{
    Fsr::ShaderNode::_validateState(args, for_real);
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdShadeShaderNode::_execute(const Fsr::NodeContext& target_context,
                                  const char*             target_name,
                                  void*                   target,
                                  void*                   src0,
                                  void*                   src1)
{
    // We need a context and a target name to figure out what to do:
    if (!target_name || !target_name[0])
        return -1; // no context target!

    if (debug())
    {
        std::cout << "  FuserUsdShadeShaderNode::_execute(" << this << ") target='" << target_name << "'";
        std::cout << " Shader";
        std::cout << " '" << getString(Arg::Scene::path) << "'";
        std::cout << std::endl;
    }

    return 0; // no error
}


//--------------------------------------------------------------------------


/*!
*/
FuserUsdShadeNodeGraphNode::FuserUsdShadeNodeGraphNode(const Pxr::UsdStageRefPtr& stage,
                                                       const Pxr::UsdPrim&        nodegraph_prim,
                                                       const Fsr::ArgSet&         args,
                                                       Fsr::Node*                 parent) :
    FuserUsdNode(stage),
    Fsr::MaterialNode(args, parent)
{
    //std::cout << "  FuserUsdShadeNodeGraphNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdShadeNodeGraph:
    if (nodegraph_prim.IsValid() && nodegraph_prim.IsA<Pxr::UsdShadeNodeGraph>())
    {
        m_nodegraph_schema = Pxr::UsdShadeNodeGraph(nodegraph_prim);

        if (debug())
        {
            std::cout << "  --------------------------------------------------" << std::endl;
            std::cout << "  UsdNodeGraph('" << nodegraph_prim.GetName() << "'):";
            printPrimAttributes(" ", nodegraph_prim, true/*verbose*/, std::cout);
            std::cout << std::endl;
            std::cout << "    inputs[";
            const std::vector<Pxr::UsdShadeInput> inputs(m_nodegraph_schema.GetInputs());
            for (size_t i=0; i < inputs.size(); ++i)
            {
                const Pxr::UsdShadeInput& input = inputs[i];
                std::cout << " " << i << ":'" << input.GetBaseName() << "'[" << input.GetTypeName().GetType() << "]";
            }
            std::cout << " ]" << std::endl;
            std::cout << "    outputs[";
            const std::vector<Pxr::UsdShadeOutput> outputs(m_nodegraph_schema.GetOutputs());
            for (size_t i=0; i < outputs.size(); ++i)
            {
                const Pxr::UsdShadeOutput& output = outputs[i];
                std::cout << " " << i << ":'" << output.GetBaseName() << "'[" << output.GetTypeName().GetType() << "]";
            }
            std::cout << " ]" << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "  FuserUsdShadeNodeGraph::ctor(" << this << "): ";
            std::cerr << "warning, node '" << nodegraph_prim.GetPath() << "'(" << nodegraph_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }
}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdShadeNodeGraphNode::_validateState(const Fsr::NodeContext& args,
                                           bool                    for_real)
{
    Fsr::MaterialNode::_validateState(args, for_real);
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdShadeNodeGraphNode::_execute(const Fsr::NodeContext& target_context,
                                     const char*             target_name,
                                     void*                   target,
                                     void*                   src0,
                                     void*                   src1)
{
    // We need a context and a target name to figure out what to do:
    if (!target_name || !target_name[0])
        return -1; // no context target!

    if (debug())
    {
        std::cout << "  FuserUsdShadeNodeGraphNode::_execute(" << this << ") target='" << target_name << "'";
        std::cout << " Shader";
        std::cout << " '" << getString(Arg::Scene::path) << "'";
        std::cout << std::endl;
    }

    return 0; // no error
}


//--------------------------------------------------------------------------


/*!
*/
FuserUsdShadeMaterialNode::FuserUsdShadeMaterialNode(const Pxr::UsdStageRefPtr& stage,
                                                     const Pxr::UsdPrim&        material_prim,
                                                     const Fsr::ArgSet&         args,
                                                     Fsr::Node*                 parent) :
    FuserUsdShadeNodeGraphNode(stage, material_prim, args, parent)
{
    //std::cout << "  FuserUsdShadeMaterialNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdShadeMaterial:
    if (material_prim.IsValid() && material_prim.IsA<Pxr::UsdShadeMaterial>())
    {
        m_material_schema = Pxr::UsdShadeMaterial(material_prim);

        if (debug())
        {
            const Pxr::UsdShadeOutput& surface_output  = m_material_schema.GetSurfaceOutput();
            const Pxr::UsdShadeOutput& displace_output = m_material_schema.GetDisplacementOutput();
            const Pxr::UsdShadeOutput& volume_output   = m_material_schema.CreateVolumeOutput();
            std::cout << "     surface_output('" << surface_output.GetBaseName() << "')[" << surface_output.GetTypeName().GetType() << "]" << std::endl;
            std::cout << "    displace_output('" << displace_output.GetBaseName() << "')[" << displace_output.GetTypeName().GetType() << "]" << std::endl;
            std::cout << "      volume_output('" << volume_output.GetBaseName() << "')[" << volume_output.GetTypeName().GetType() << "]" << std::endl;
        }
    }
    else
    {
        if (debug())
        {
            std::cerr << "  FuserUsdShadeMaterial::ctor(" << this << "): ";
            std::cerr << "warning, node '" << material_prim.GetPath() << "'(" << material_prim.GetTypeName() << ") ";
            std::cerr << "is invalid or wrong type";
            std::cerr << std::endl;
        }
    }

    //std::cout << "  -----------------------------------------------------" << std::endl;
    //std::cout << "  UsdMaterialNode('" << material_prim.GetName() << "'):" << std::endl;
    const std::vector<Pxr::UsdShadeOutput> outputs(m_material_schema.GetOutputs());
    const uint32_t nOutputs = (uint32_t)outputs.size();
    for (uint32_t i=0; i < nOutputs; ++i)
    {
        const Pxr::UsdShadeOutput& output = outputs[i];
        if (!output.HasConnectedSource())
            continue; // skip unconnected outputs

        const std::string output_name = output.GetBaseName().GetString();
        const std::string output_type = output.GetTypeName().GetType().GetTypeName();
        //std::cout << "    output " << i << ": '" << output_name << "'[" << output_type << "]" << std::endl;

        Pxr::UsdShadeConnectableAPI source;
        Pxr::TfToken                sourceName;
        Pxr::UsdShadeAttributeType  sourceType;
        if (output.GetConnectedSource(&source,
                                      &sourceName,
                                      &sourceType))
        {
            if (sourceType != Pxr::UsdShadeAttributeType::Output)
                continue; // skip non-explicit outputs

            Pxr::UsdPrim      input_prim      = source.GetPrim();
            const std::string input_prim_name = input_prim.GetName().GetString();
            //std::cout << "      source='" << input_prim_name << "'[" << sourceName << "]" << std::endl;

            // Create the shader tree connected to this output.
            // Each FuserUsdShadeShaderNode will recursively create its inputs
            // adding them to the group (this node):
            std::string output_label = "usd:";
            output_label += output_name;
            Fsr::ArgSet sdr_args;
            sdr_args.setString("material:output", output_label);

            // TODO: make a wrapper method for building these standard node args:
            {
                sdr_args.setString(Arg::node_name, input_prim_name);

                // Usd scene path:
                sdr_args.setString(Arg::Scene::path, input_prim.GetPath().GetString());

                // Local Fsr node path:'fsr:node:path' is the node + child node path:
                const std::string fsr_node_path = Fsr::buildPath(Fsr::Node::getPath(), input_prim_name);
                sdr_args.setString(Arg::node_path, fsr_node_path);

                if (getInt(Arg::node_debug, 0) > 0)
                    sdr_args.setInt(Arg::node_debug, 1/*DEBUG_1*/);
            }

            FuserUsdShadeShaderNode* output_shader = new FuserUsdShadeShaderNode(getStage(),
                                                                                 input_prim,
                                                                                 sdr_args,
                                                                                 this/*group*/,
                                                                                 NULL/*parent*/);
            this->addChild(output_shader);

            // Add this output node to the material's output connections lists:
            m_surface_outputs.push_back(output_shader);
            //m_displacement_outputs.push_back(output_shader);
            //m_volume_outputs.push_back(output_shader);
        }
    }

    if (1)//debug())
    {
        if (numChildren() > 0)
        {
            std::cout << "  Fsr::MaterialNode('" << getName() << "'):" << std::endl;
            std::cout << "    material children:" << std::endl;
            for (unsigned j=0; j < numChildren(); ++j)
            {
                getChild(j)->printInfo(std::cout, "      ");
                std::cout << std::endl;
            }
        }
    }

}


/*! Called before execution to allow node to update local data from args.
*/
/*virtual*/ void
FuserUsdShadeMaterialNode::_validateState(const Fsr::NodeContext& args,
                                          bool                    for_real)
{
    FuserUsdShadeNodeGraphNode::_validateState(args, for_real);
}


/*! Return abort (-1) on user-interrupt so processing can be interrupted.
*/
/*virtual*/ int
FuserUsdShadeMaterialNode::_execute(const Fsr::NodeContext& target_context,
                                    const char*             target_name,
                                    void*                   target,
                                    void*                   src0,
                                    void*                   src1)
{
    // We need a context and a target name to figure out what to do:
    if (!target_name || !target_name[0])
        return -1; // no context target!

    if (debug())
    {
        std::cout << "  FuserUsdShadeMaterialNode::_execute(" << this << ") target='" << target_name << "'";
        std::cout << " Shader";
        std::cout << " '" << getString(Arg::Scene::path) << "'";
        std::cout << std::endl;
    }

    return 0; // no error
}


} // namespace Fsr


// end of FuserUsdShader.cpp

//
// Copyright 2019 DreamWorks Animation
//
