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


/*!
*/
FuserUsdShadeShaderNode::FuserUsdShadeShaderNode(const Pxr::UsdStageRefPtr& stage,
                                                 const Pxr::UsdPrim&        shader_prim,
                                                 const Fsr::ArgSet&         args,
                                                 Fsr::Node*                 parent) :
    FuserUsdNode(stage),
    Fsr::ShaderNode(args, parent)
{
    //std::cout << "  FuserUsdShadeShaderNode::ctor(" << this << ") '" << prim.GetPath() << "'" << std::endl;

    // Make sure it's a UsdShadeShader:
    if (shader_prim.IsValid() && shader_prim.IsA<Pxr::UsdShadeShader>())
    {
        m_shader_schema = Pxr::UsdShadeShader(shader_prim);

        if (debug())
        {
            std::cout << "  --------------------------------------------------" << std::endl;
            std::cout << "  UsdShader('" << shader_prim.GetName() << "'):";
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

    // Create any child Shader or ShadeNodeGraph nodes:
    for (auto child=TfMakeIterator(nodegraph_prim.GetAllChildren()); child; ++child)
    {
        if (child->IsValid() && FuserUsdNode::isShadingPrim(*child))
        {
            // The material creation args are slimmed down:
            Fsr::ArgSet args;
            args.setString(Arg::node_name, child->GetName().GetString());
            //args.setString(Arg::node_path,   );
            args.setString(Arg::Scene::path, child->GetPath().GetString());
            if (getInt(Arg::node_debug, 0) > 0)
                args.setInt(Arg::node_debug, 1/*DEBUG_1*/);

            this->addChild(new FuserUsdShadeShaderNode(getStage(), *child, args, this/*parent*/));
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
