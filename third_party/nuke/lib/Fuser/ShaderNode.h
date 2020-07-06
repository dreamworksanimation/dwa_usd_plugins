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

/// @file Fuser/ShaderNode.h
///
/// @author Jonathan Egstad

#ifndef Fuser_ShaderNode_h
#define Fuser_ShaderNode_h

#include "Node.h"
#include "AttributeTypes.h"


namespace Fsr {

typedef std::map<std::string, int32_t> NamedInputMap;


/*! A ShaderNode contains attributes and ShaderNode inputs and outputs.

    In the scope of Fuser it's simply a storage node that may have
    additional ShaderNode children and stores the attributes from
    a (usually) imported Shader from USD or other scenegraph
    system.

    This node and its children are translated into real shader
    implementations for whatever system is trying to use them.

    TODO: continue fleshing this concept out
*/
class FSR_EXPORT ShaderNode : public Fsr::Node
{
  public:
    /*!
    */
    struct InputBinding
    {
        std::string name;
        std::string type;
        std::string value;
        ShaderNode* source_shader;
        std::string source_output_name;

        //!
        InputBinding() : source_shader(NULL) {}
        InputBinding(const InputBinding& b)
        {
            name               = b.name;
            type               = b.type;
            value              = b.value;
            source_shader      = b.source_shader;
            source_output_name = b.source_output_name;
        }
    };


  protected:
    std::vector<InputBinding> m_inputs;             //!< Input connections
    NamedInputMap             m_input_name_map;     //!< Map of input names to m_inputs index
    std::vector<ShaderNode*>  m_outputs;


  protected:
    //! Subclass implementation of connectInput().
    virtual void _connectInput(uint32_t    input_index,
                               ShaderNode* to_shader,
                               const char* to_shader_output_name);


  public:
    //!
    ShaderNode(Node* parent=NULL);

    //!
    ShaderNode(const ArgSet& args,
               Node*         parent=NULL);

    /*! Must have a virtual destructor!
        Dtor necessary to avoid GCC 'undefined reference to `vtable...' link error.
    */
    virtual ~ShaderNode();

    //! Specialized to return child cast to ShaderNode.
    ShaderNode* getChild(unsigned index) const { assert(index < m_children.size()); return static_cast<ShaderNode*>(m_children[index]); }
    //! Returns NULL if named node not in child list. Specialized to return child cast to ShaderNode.
    ShaderNode* getChildByName(const char*        child_name) const;
    ShaderNode* getChildByName(const std::string& child_name) const { return this->getChildByName(child_name.c_str()); }
    //! Returns NULL if a node with the path is not found in child list. Specialized to return child cast to ShaderNode.
    ShaderNode* getChildByPath(const char*        child_path) const;
    ShaderNode* getChildByPath(const std::string& child_path) const { return this->getChildByPath(child_path.c_str()); }


    //! Returns the number of inputs.
    uint32_t numInputs() const { return (uint32_t)m_inputs.size(); }
    //! Sets the number of inputs of this shader. New inputs beyond the current are set to NULL.
    void     setNumInputs(uint32_t numInputs);

    //! Returns binding for input. No range checking!
    const InputBinding& getInput(uint32_t input)     const { return m_inputs[input]; }
    //! Return a named input's index or -1 if not found.
    int32_t             getInputByName(const char* input_name) const;

    //! Return the input name if assigned. No range checking!
    const char*         getInputName(uint32_t input) const { return m_inputs[input].name.c_str(); }
    //! Return the input name if assigned. No range checking!
    const char*         getInputType(uint32_t input) const { return m_inputs[input].type.c_str(); }
    //! Returns shader pointer for input. No range checking!
    ShaderNode*         getInputConnection(uint32_t input) const { return m_inputs[input].source_shader; }

    //! Assign an input's values but don't connect it.
    void setInput(uint32_t    input,
                  const char* name,
                  const char* type,
                  const char* value="");

    //!
    void setInputValue(uint32_t    input,
                       const char* value);


    //! Returns true if input can connect to shader.
    virtual bool canConnect(uint32_t    input,
                            ShaderNode* to_shader,
                            const char* to_shader_output_name);

    //! Assign the input ShaderNode pointer for input, returning true if connection was made.
    bool         connectInput(uint32_t    input,
                              ShaderNode* to_shader,
                              const char* to_shader_output_name="rgb");

    //! Print some info about shader settings.
    void printInfo(std::ostream&, const char* prefix="") const;

};


} // namespace Fsr

#endif

// end of Fuser/ShaderNode.h

//
// Copyright 2019 DreamWorks Animation
//
