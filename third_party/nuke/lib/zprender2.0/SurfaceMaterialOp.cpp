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

/// @file zprender/SurfaceMaterialOp.cpp
///
/// @author Jonathan Egstad


#include "SurfaceMaterialOp.h"
#include "RayShader.h"

// Remove these when shader creation changed to RayShader::create() calls
#include "zprIopUVTexture.h"
#include "zprAttributeReader.h"


namespace zpr {


//-----------------------------------------------------------------------------


/*static*/ const char* SurfaceMaterialOp::frame_clamp_modes[] =
{
    "none",
    "fwd-round-up",
    "fwd-round-down",
    "rev-round-up",
    "rev-round-down",
    0
};


/*! Default the shader channels to RGB.
*/
SurfaceMaterialOp::SurfaceMaterialOp(::Node* node) :
    DD::Image::Material(node),
    k_frame_clamp_mode(FRAME_CLAMP_NONE)
{
    //
}


//!
/*static*/ const char* SurfaceMaterialOp::zpClass() { return "zpSurfaceMaterialOp"; }

/*!
*/
void
SurfaceMaterialOp::addSurfaceMaterialOpIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, SurfaceMaterialOp::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


/*!
*/
/*virtual*/
void
SurfaceMaterialOp::addRayControlKnobs(DD::Image::Knob_Callback f)
{
    DD::Image::Enumeration_knob(f, &k_visibility.k_sides_mode, RenderContext::sides_modes, "sides_mode", "visibility");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Shader is applied to the front or back face, or both.");
    DD::Image::Bool_knob(f, &k_visibility.k_camera_visibility,       "camera_visibility",       "camera");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to camera rays.");
    DD::Image::Bool_knob(f, &k_visibility.k_shadow_visibility,       "shadow_visibility",       "shadow");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to shadow occlusion rays.");
    DD::Image::Bool_knob(f, &k_visibility.k_specular_visibility,     "specular_visibility",     "spec");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to specular reflection rays.");
    DD::Image::Bool_knob(f, &k_visibility.k_diffuse_visibility,      "diffuse_visibility",      "diff");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to diffuse reflection rays.");
    DD::Image::Bool_knob(f, &k_visibility.k_transmission_visibility, "transmission_visibility", "trans");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "This shader is visible to transmissed (or refracted) rays.");
    DD::Image::Newline(f);
    DD::Image::Enumeration_knob(f, &k_frame_clamp_mode, frame_clamp_modes, "frame_clamp_mode", "frame clamp");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Modify the frame number for the shader, none, round-up or round-down.");
    DD::Image::Newline(f);
}


//-----------------------------------------------------------------------------


/*! Allow only RayShaders on input 0.
*/
/*virtual*/
bool
SurfaceMaterialOp::test_input(int            input,
                            DD::Image::Op* op) const
{
    // Dynamic_cast fails...so use knob() test instead:
#ifdef ZPR_USE_KNOB_RTTI
    if (input == 0 && op && op->knob(SurfaceMaterialOp::zpClass())!=NULL)
        return true;
#else
    if (input == 0 && dynamic_cast<SurfaceMaterialOp*>(op))
        return true;
#endif
    return DD::Image::Material::test_input(input, op);
}


/*! Change frame clamp mode for inputs.

    Return the context to use for the input connected to input(n, offset). The
    most common thing to do is to change the frame number.

    The default version returns outputContext().

    You can use \a scratch as a space to construct the context and
    return it.

    This cannot look at input \a n or above, as they have not been
    created yet.  Often though it is useful to look at these inputs, for
    instance to get the frame range to make a time-reversing operator. If
    you want to do this you use node_input() to generate a "likely"
    op. You can examine any data in it that you know will not depend on
    the frame number.
*/
/*virtual*/
const DD::Image::OutputContext&
SurfaceMaterialOp::inputContext(int                       input,
                              int                       offset,
                              DD::Image::OutputContext& context) const
{
#if 1
    // This implementation probably not required in SurfaceMaterialOp as setOutputContext() sets the
    // frame number for the entire Op, including the inputs since this is called
    // after setOutputContext() is.
    return DD::Image::Material::inputContext(input, offset, context);
#else
    std::cout << "SurfaceMaterialOp(" << this->node_name() << ")::inputContext(): frame=" << context.frame() << std::endl;
    if (k_frame_clamp_mode == FRAME_CLAMP_NONE)
        return DD::Image::Material::inputContext(input, offset, context);

    // Copy the context from the calling op:
    DD::Image::OutputContext c1 = context;
    switch (k_frame_clamp_mode)
    {
        default:
        case FRAME_CLAMP_NONE: break;
        case FRAME_CLAMP_UP:   c1.setFrame(ceil(context.frame())); break;
        case FRAME_CLAMP_DOWN: c1.setFrame(floor(context.frame())); break;
    }
    return c1;
#endif
}


/*! Change what is in outputContext(). Nuke calls this for you.
    Subclasses can override this, but they must call the base class with
    exactly the same context. This method is a convenient place to do
    calculations that are needed before any of the following methods work:
       - int split_input(int) const;
       - float uses_input(int) const;
       - const OutputContext& inputContext(int n, int m, OutputContext&) const;
       - Op* defaultInput(int n, const OutputContext&) const;
    The knob values have been stored at this point, but no inputs
    have been created.
*/
/*virtual*/
void
SurfaceMaterialOp::setOutputContext(const DD::Image::OutputContext& context)
{
    // Op's implementation simply copies the passed-in context to the
    // Op::outputContext_ variable, so modify the context that we pass
    // up to our parent class:
#if 1
    //std::cout << "SurfaceMaterialOp(" << this->node_name() << ")::setOutputContext(): frame=" << context.frame() << std::endl;
    DD::Image::Knob* k = knob("frame_clamp_mode");
    if (k)
    {
        int frame_clamp_mode = int(k->get_value());
        if (frame_clamp_mode == FRAME_CLAMP_NONE)
        {
            DD::Image::Material::setOutputContext(context);
        }
        else
        {
            // Copy the context from the calling op:
            DD::Image::OutputContext c1 = context;
            switch (frame_clamp_mode)
            {
                default: break;
                case FRAME_FWD_RND_UP:   c1.setFrame(floor(context.frame())+1.0); break;
                case FRAME_FWD_RND_DOWN: c1.setFrame(floor(context.frame())); break;
                case FRAME_REV_RND_UP:   c1.setFrame(ceil(context.frame())); break;
                case FRAME_REV_RND_DOWN: c1.setFrame(ceil(context.frame())-1.0); break;
            }
            //std::cout << "  frame=" << c1.frame() << std::endl;
            DD::Image::Material::setOutputContext(c1);
        }
    }
    else
    {
        DD::Image::Material::setOutputContext(context);
    }
#else
    DD::Image::Material::setOutputContext(context);
#endif
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
SurfaceMaterialOp::append(DD::Image::Hash& hash)
{
    DD::Image::Material::append(hash);
    //std::cout << "SurfaceMaterialOp::append(" << this << ")";
    //std::cout << "hash=0x" << std::hex << hash.value() << std::dec << std::endl;
}


/*!
*/
/*virtual*/
void
SurfaceMaterialOp::knobs(DD::Image::Knob_Callback f)
{
    addSurfaceMaterialOpIdKnob(f);
}


/*! _validate() is called first by RenderContext as it's validating objects.

    So we need to assign the InputBindings now before they get copied into
    the local RayShader vars that get copied to the spawned RayShader.

*/
/*virtual*/
void
SurfaceMaterialOp::_validate(bool for_real)
{
    // This validates all inputs whic is important to get connected input
    // SurfaceMaterialOp to build their local InputBindings:
    DD::Image::Op::_validate(for_real);

    // Do a copy_info() - it's not really needed as Materials
    // don't need to provide a format or bbox unless they're
    // the 2D source for a texture map (which they never are since
    // that doesn't make much sense...)
    copy_info();

    // Always output rgba:
    info_.turn_on(DD::Image::Mask_RGBA);
}


/*! Create the shaders for one input, adding them to the shaders list and
    returning the output surface shader to connect RayShader input to.
*/
/*virtual*/
RayShader*
SurfaceMaterialOp::createInputSurfaceShaders(uint32_t                 input,
                                             const RenderContext&     rtx,
                                             std::vector<RayShader*>& shaders)
{
    if (input >= (uint32_t)DD::Image::Op::inputs())
        return NULL;

    // Skip input if it's not another SurfaceMaterialOp:
    InputBinding* binding = getInputBinding(input);
    if (!binding || !binding->isSurfaceMaterialOp() || !binding->input_object)
        return NULL;

    SurfaceMaterialOp* input_material = static_cast<SurfaceMaterialOp*>(binding->input_object);
    //std::cout << "  " <<  node_name() << "::createInputSurfaceShaders() input " << input << " input_material=" << input_material << " '" << input_material->node_name() << "'" << std::endl;

    // Create the input shader tree and return the output shader:
    return input_material->createSurfaceShaders(rtx, shaders);
}


/*! Allocate a list of RayShaders this Op produces, and return the output
    connection point.
    Calling object takes ownership of all returned pointers.

    Creates the output shader by calling the virtual subclass
    _createOutputSurfaceShader() method then creating and connecting up all
    SurfaceMaterialOp inputs.
*/
RayShader*
SurfaceMaterialOp::createSurfaceShaders(const RenderContext&     rtx,
                                        std::vector<RayShader*>& shaders)
{
    //std::cout << node_name() << "::createSurfaceShaders():" << std::endl;
    RayShader* output_shader = _createOutputSurfaceShader(rtx, shaders);
    //std::cout << "  output_shader=" << output_shader << std::endl;

    // If nothing created try connecting to input0:
    if (!output_shader)
        return createInputSurfaceShaders(0, rtx, shaders);

    // Use the name of the Op as the shader name:
    std::string output_shader_name(node_name());
    output_shader_name += "_shader";
    output_shader->setName(output_shader_name.c_str());

    // Assign the shader's input and output knobs:
    const uint32_t nInputKnobs = output_shader->numInputs();
    for (uint32_t input=0; input < nInputKnobs; ++input)
    {
        InputBinding* input_binding = output_shader->getInputBinding(input);
        if (!input_binding)
            continue; // skip any null bindings

        const RayShader::InputKnob* k_input = output_shader->getInputKnob(input);
        assert(k_input);

        //std::cout << "    '" << output_shader_name << "': input=" << input << ", binding" << *input_binding;
        //std::cout << ", knob " << *k_input << std::endl;
        /*
            const char* name;           //!<
            KnobType    type;           //!<
            void*       data;           //!< Pointer to local data, cast to type
            RayShader*  shader;         //!< Non-NULL if knob is bound to another RayShader's output
            int32_t     output_index;   //!< Output index of another RayShader
        */
        if (k_input->type == RayShader::PIXEL_KNOB)
        {
            // Try to connect the input channel set to the input binding.
            // If the binding has an object pointer then it's attached to another
            // object, usually an Op, so handle those.
            //std::cout << "    '" << output_shader_name << "': input#" << input << " PIXEL_KNOB binding=" << input_binding << *input_binding << std::endl;
            if (input_binding->isSurfaceMaterialOp())
            {
                SurfaceMaterialOp* input_material = input_binding->asSurfaceMaterialOp();

                RayShader* input_shader = createInputSurfaceShaders(input, rtx, shaders);
                //std::cout << "      input_shader=" << input_shader << std::endl;

                if (input_shader)
                {
                    //const RayShader::OutputKnob* k_input_shader_output = input_shader->getOutputKnob(0);
                    //std::cout << "      '" << output_shader_name << "': connect input '" << k_input->name << "' to input shader's output '" << k_input_shader_output->name << "'" << std::endl;

                    if (output_shader->connectInput(input, input_shader, "surface"/*output_name*/))
                    {
                        // Connected - update the InputBinding to point at the input RayShader:
                        input_binding->type = InputBinding::RAYSHADER;
                        input_binding->input_object = (void*)input_shader;
                        input_binding->setActiveChannels(DD::Image::Chan_Red,
                                                         DD::Image::Chan_Green,
                                                         DD::Image::Chan_Blue,
                                                         DD::Image::Chan_Alpha);
                    }
                    else
                    {
                        // Couldn't connect, clear the binding:
                        *input_binding = InputBinding();
                    }

                    if (k_input->data != NULL)
                    {
                        std::stringstream chan_text;
                        chan_text << input_material->channels();
                        output_shader->setInputValue(input, chan_text.str().c_str());
                    }
                }
                else
                {
                    //std::cout << "      cannot connect, no input shader to connect to!" << std::endl;
                    std::cerr << node_name() << "::createSurfaceShaders()";
                    std::cerr << " warning cannot connect input '" << k_input->name << "'";
                    std::cerr << ", no shader to connect to.";
                    std::cerr << std::endl;
                    output_shader->setInputValue(input, "");
                }

            }
            else if (input_binding->isTextureIop())
            {
                // Create a shader that gets called to sample the Iop:
                DD::Image::Iop* input_iop = input_binding->asTextureIop();
                //std::cout << "    '" << output_shader_name << "': input#" << input << " TEXTURE" << std::endl;

                // Change this to a RayShader::create() call:
#if 0
                RayShader* input_shader = RayShader::create("IopUVTexture");
                assert(input_shader); // shouldn't happen...
                RayShader::InputKnob* k = input_shader->knob("iop");
                assert(k); // shouldn't happen...
                k->setPointer(input_iop);
#else
                RayShader* input_shader = new zprIopUVTexture(input_iop);
                assert(input_shader); // shouldn't happen...
#endif

                std::string input_shader_name(input_iop->node_name());
                input_shader_name += "_shader";
                input_shader->setName(input_shader_name.c_str());

                //const RayShader::OutputKnob& k_input_shader_output = input_shader->getOutputKnob(0);
                //std::cout << "      connect to input shader's output'" << k_input_shader_output.name << "'" << std::endl;

                output_shader->connectInput(input, input_shader, "rgba"/*output_name*/);

                if (k_input->data != NULL)
                {
                    std::stringstream chan_text;
                    chan_text << input_iop->channels();
                    output_shader->setInputValue(input, chan_text.str().c_str());
                }
            }
        }

        //std::cout << "        '" << output_shader_name << "': input=" << input << ", binding" << *input_binding << std::endl;
    }

    return output_shader;
}


/*! InputBinding type:
        zprUVTexture for a texture InputBinding, a zprConstant for constant InputBinding,
    or zprAttrib for an attribute InputBinding.
*/
RayMaterial*
SurfaceMaterialOp::createMaterial(const RenderContext& rtx)
{
    //std::cout << "SurfaceMaterialOp('" << node_name() << "')::createMaterial(" << this << ")" << std::endl;

    std::vector<RayShader*> all_shaders;
    all_shaders.reserve(50);

    RayShader* output_surface_shader      = createSurfaceShaders(rtx, all_shaders);
    RayShader* output_displacement_shader = NULL;//createDisplacementShaders(rtx, all_shaders);
    RayShader* output_volume_shader       = NULL;//createVolumeShaders(rtx, all_shaders);

    if (!output_surface_shader || all_shaders.size() == 0)
        return NULL;

    //for (size_t i=0; i < all_shaders.size(); ++i)
    //    std::cout << "  " << i << ": '" << all_shaders[i]->zprShaderClass() << "'" << std::endl;

    // Create a new material and built its shader tree:
    RayMaterial* material = new RayMaterial(all_shaders,
                                            output_surface_shader,
                                            output_displacement_shader,
                                            output_volume_shader);

    return material;
}


//-----------------------------------------------------------------------


/*! Return the input number to use for the OpenGL texture display, usually the diffuse.
    Defaults to an invalid input -1.
*/
/*virtual*/
int32_t
SurfaceMaterialOp::getGLTextureInput() const
{
    return -1;
}


/*virtual*/
void 
SurfaceMaterialOp::vertex_shader(DD::Image::VertexContext& vtx)
{
    // do nothing
}


/*virtual*/
void 
SurfaceMaterialOp::fragment_shader(const DD::Image::VertexContext& vtx,
                                   DD::Image::Pixel&               out)
{
    out.erase(); // do nothing
}


/*virtual*/
void 
SurfaceMaterialOp::displacement_shader(const DD::Image::VertexContext& vtx,
                                       DD::Image::VArray&              out)
{
    // do nothing
}


/*virtual*/
float
SurfaceMaterialOp::displacement_bound() const
{
    return 0.0f;
}


/*virtual*/
void
SurfaceMaterialOp::blending_shader(const DD::Image::Pixel& in,
                                   DD::Image::Pixel&       out)
{
    // do nothing
}


/*virtual*/
void
SurfaceMaterialOp::render_state(DD::Image::GeoInfoRenderState& state)
{
    // base class does nothing
}


/*virtual*/
bool
SurfaceMaterialOp::set_texturemap(DD::Image::ViewerContext* ctx,
                                  bool                      gl)
{
    const int32_t ogltex_input = getGLTextureInput();
    if (ogltex_input >= 0)
        return input(ogltex_input)->set_texturemap(ctx, gl);
    return false;
}


/*virtual*/
bool
SurfaceMaterialOp::shade_GL(DD::Image::ViewerContext* ctx,
                            DD::Image::GeoInfo&       geo)
{
    const int32_t ogltex_input = getGLTextureInput();
    if (ogltex_input >= 0)
        return input(ogltex_input)->shade_GL(ctx, geo);
    return true;
}


/*virtual*/
void
SurfaceMaterialOp::unset_texturemap(DD::Image::ViewerContext* ctx)
{
    const int32_t ogltex_input = getGLTextureInput();
    if (ogltex_input >= 0)
        return input(ogltex_input)->unset_texturemap(ctx);
}


} // namespace zpr

// end of zprender/SurfaceMaterialOp.cpp

//
// Copyright 2020 DreamWorks Animation
//
