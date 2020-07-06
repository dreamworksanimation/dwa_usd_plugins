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

/// @file Fuser/ShaderNode.cpp
///
/// @author Jonathan Egstad

#include "ShaderNode.h"

#include <mutex> // for std::mutex

namespace Fsr {


/*!
*/
ShaderNode::ShaderNode(Node* parent) :
    Fsr::Node(parent)
{
    //std::cout << "  ShaderNode::ctor(" << this << ")" << std::endl;
    m_inputs.reserve(10);
    m_outputs.reserve(4);
}

/*!
*/
ShaderNode::ShaderNode(const ArgSet& args,
                       Node*         parent) :
    Fsr::Node(args, parent)
{
    //std::cout << "  ShaderNode::ctor(" << this << ")" << std::endl;
    if (debug())
    {
        static std::mutex m_lock; std::lock_guard<std::mutex> guard(m_lock); // lock to make the output print cleanly

        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "Fsr::ShaderNode('" << getName() << "')";
        std::cout << " args[" << m_args << "]" << std::endl;
    }
    m_inputs.reserve(10);
    m_outputs.reserve(4);
}


/*! This empty dtor is necessary to avoid GCC 'undefined reference to `vtable...' link error.
    Must be in implemenation file, not header.
*/
ShaderNode::~ShaderNode()
{
    //std::cout << "  ShaderNode::dtor(" << this << ")" << std::endl;
}


/*!
*/
void
ShaderNode::printInfo(std::ostream& o,
                      const char*   prefix) const
{
    o << prefix;
    o << "'" << getName() << "'";

    if (m_args.size() > 0)
    {
        o << " knobs[";
        for (unsigned i=0; i < numInputs(); ++i)
        {
            const InputBinding& binding = m_inputs[i];
            if (binding.name.empty() || binding.source_shader)
                continue;
            o << " " << binding.name << "(" << binding.type << ")";
            o << "=[" << binding.value << "]";
        }
        o << " ]";

        o << ", inputs[";
        for (unsigned i=0; i < numInputs(); ++i)
        {
            const InputBinding& binding = m_inputs[i];
            if (binding.name.empty() || !binding.source_shader)
                continue;
            o << " " << binding.name << "(" << binding.type << ")=";
            if (binding.source_shader)
                o << binding.source_shader->getName() << "(" << binding.source_output_name << ")";
            else
                o << "none";
        }
        o << " ]";
    }
}


/*! Returns NULL if named node not in child list.
    Specialized to return child cast to ShaderNode.
*/
ShaderNode*
ShaderNode::getChildByName(const char* child_name) const
{
    Fsr::Node* node = Node::getChildByName(child_name);
    if (node)
        return static_cast<ShaderNode*>(node);
    return NULL;
}


/*!
*/
ShaderNode*
ShaderNode::getChildByPath(const char* child_path) const
{
    Fsr::Node* node = Node::getChildByPath(child_path);
    if (node)
        return static_cast<ShaderNode*>(node);
    return NULL;
}


/*! Set the number of inputs of this ShaderNode.  Initially they are set to
	the defaultConnection() for that input number.
*/
void
ShaderNode::setNumInputs(uint32_t numInputs)
{
    if (numInputs <= (uint32_t)m_inputs.size())
        return;

    m_inputs.reserve(numInputs);
    for (uint32_t input=(uint32_t)m_inputs.size(); input < numInputs; ++input)
        m_inputs.push_back(InputBinding());
}


/*! Assign an input's values but don't connect it.
*/
void
ShaderNode::setInput(uint32_t    input,
                     const char* name,
                     const char* type,
                     const char* value)
{
    setNumInputs(input+1);

    InputBinding& binding = m_inputs[input];
    if (name)
    {
        binding.name = name;
        m_input_name_map[binding.name] = input;
    }
    if (type)
        binding.type = type;
    else
        binding.type = "none";

    binding.source_shader      = NULL;
    binding.source_output_name = "";
}


/*!
*/
void
ShaderNode::setInputValue(uint32_t    input,
                          const char* value)
{
    if (!value || input >= m_inputs.size())
        return;

    InputBinding& binding = m_inputs[input];
    binding.value = value;
}


/*! Returns true if input \b n can connect to ShaderNode.

    Base class return false.

	Subclass ShaderNode types should do a dynamic_case on the input ShaderNode to determine
	if the type is allowed and return true if so.
*/
/*virtual*/
bool
ShaderNode::canConnect(uint32_t    input,
                       ShaderNode* to_shader,
                       const char* to_shader_output_name)
{
    return true;
}


/*! Attempt to connect input \b i to ShaderNode \b shader.
    Returns true if connection was made.

    The virtual method \b canConnect() is called on this shader which
    returns true if the connection is allowed.

    If connection is allowed, the virtual method _connectInput() is
    called to allow sublasses to do special things with the ShaderNode
    input like hook up additional shaders to the input.
*/
bool
ShaderNode::connectInput(uint32_t    input,
                         ShaderNode* to_shader,
                         const char* to_shader_output_name)
{
    if (!to_shader || !canConnect(input,
                                  to_shader,
                                  to_shader_output_name))
        return false;

    InputBinding& binding = m_inputs[input];
    if (to_shader_output_name)
        binding.source_output_name = to_shader_output_name;
    else
        binding.source_output_name = "none";

    // Connect it to source shader and add this to the source shader's
    // output connections:
    binding.source_shader = to_shader;

    // Add to output list of connected shader if it's not already connected:
    bool is_connected = false;
    const size_t nConnectedShaderOutputs = to_shader->m_outputs.size();
    for (size_t i=0; i < nConnectedShaderOutputs; ++i)
        if (to_shader->m_outputs[i] == this)
            is_connected = true;
    if (!is_connected)
        to_shader->m_outputs.push_back(this);

    // Allow subclass to do their own connections:
    _connectInput(input, to_shader, to_shader_output_name);

    return true;
}


/*! Subclass implementation of connectInput().
	Base class does nothing.
*/
/*virtual*/
void
ShaderNode::_connectInput(uint32_t    input,
                          ShaderNode* to_shader,
                          const char* to_shader_output_name)
{
    /* Do nothing */
}


/*! Return a named input's index or -1 if not found.
*/
int32_t
ShaderNode::getInputByName(const char* input_name) const
{
    if (!input_name || !input_name[0])
        return -1;
    NamedInputMap::const_iterator it = m_input_name_map.find(std::string(input_name));
    return (it !=  m_input_name_map.end()) ? it->second : -1;
}


} // namespace Fsr


// end of Fuser/ShaderNode.cpp

//
// Copyright 2019 DreamWorks Animation
//
