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

/// @file zprender/SurfaceShaderOp.cpp
///
/// @author Jonathan Egstad


#include "SurfaceShaderOp.h"
#include "RenderContext.h"


namespace zpr {


//-----------------------------------------------------------------------------


/*! Default the shader channels to RGB.
*/
SurfaceShaderOp::SurfaceShaderOp(::Node* node) :
    RayShader(),
    DD::Image::Material(node)
{
    //
}


//!
/*static*/ const char* SurfaceShaderOp::zpClass() { return "zpSurfaceShaderOp"; }

/*!
*/
void
SurfaceShaderOp::addSurfaceShaderOpIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, SurfaceShaderOp::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-----------------------------------------------------------------------------


DD::Image::Op*
SurfaceShaderOp::opInput(int n) const
{
    return (m_input_type[n].isOp()) ?
            static_cast<DD::Image::Op*>(Op::input(n)) :
                NULL;
}

DD::Image::Iop*
SurfaceShaderOp::iopInput(int n) const
{
    return (m_input_type[n].isIop()) ?
            static_cast<DD::Image::Iop*>(Op::input(n)) :
                NULL;
}

DD::Image::Material*
SurfaceShaderOp::materialInput(int n) const
{
    return (m_input_type[n].isMaterial()) ?
            static_cast<DD::Image::Material*>(Op::input(n)) :
                NULL;
}

SurfaceShaderOp*
SurfaceShaderOp::rayShaderInput(int n) const
{
    return (m_input_type[n].isSurfaceShaderOp()) ?
            static_cast<SurfaceShaderOp*>(Op::input(n)) :
                NULL;
}


//-----------------------------------------------------------------------------


/*! The surface evaluation shader call.  If doing final displacement
   implement evaluateDisplacement instead.
   Base-class version passes it up to input0.
*/
/*virtual*/
void
SurfaceShaderOp::_evaluateGeometricShading(RayShaderContext& stx,
                                           RayShaderContext& out)
{
    // Pass it on up if input 0 is another RayShader:
    SurfaceShaderOp* ray_shader = rayShaderInput(0);
    if (ray_shader)
        ray_shader->doGeometricShading(stx, out);
    else
        out = stx; // no input, copy source stx to output
}


/*!
*/
/*virtual*/
void
SurfaceShaderOp::_evaluateShading(RayShaderContext& stx,
                                  Fsr::Pixel&       out)
{
    // Pass it on up if input 0 is another SurfaceShaderOp:
    SurfaceShaderOp* ray_shader = rayShaderInput(0);
    if (ray_shader)
    {
        ray_shader->evaluateShading(stx, out);
    }
    else if (iopInput(0))
    {
        // Call legacy shader:
        DD::Image::VertexContext vtx; //!< Contains surface attribs
        updateDDImageShaderContext(stx, vtx);
        //------------------------------------------
        //------------------------------------------
        iopInput(0)->fragment_shader(vtx, out);
    }
}


/*!
*/
/*virtual*/
void
SurfaceShaderOp::_evaluateDisplacement(RayShaderContext& stx,
                                       Fsr::Pixel&       out)
{
    // Pass it on up if input 0 is another SurfaceShaderOp:
    SurfaceShaderOp* ray_shader = rayShaderInput(0);
    if (ray_shader)
    {
        out.PW() = stx.PW;
        out.N()  = stx.Ns;
        // Pass it on up:
        ray_shader->evaluateDisplacement(stx, out);
    }
    else if (iopInput(0))
    {
        // Call legacy shader:
    }
}


//-----------------------------------------------------------------------------


/*! Allow only RayShaders on input 0.
*/
/*virtual*/
bool
SurfaceShaderOp::test_input(int            input,
                            DD::Image::Op* op) const
{
    // Dynamic_cast fails...so use knob() test instead:
#ifdef ZPR_USE_KNOB_RTTI
    if (input == 0 && op && op->knob(SurfaceShaderOp::zpClass())!=NULL)
        return true;
#else
    if (input == 0 && dynamic_cast<SurfaceShaderOp*>(op))
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
SurfaceShaderOp::inputContext(int                       input,
                              int                       offset,
                              DD::Image::OutputContext& context) const
{
#if 1
    // This implementation probably not required in SurfaceShaderOp as setOutputContext() sets the
    // frame number for the entire Op, including the inputs since this is called
    // after setOutputContext() is.
    return DD::Image::Material::inputContext(input, offset, context);
#else
    std::cout << "SurfaceShaderOp(" << this->node_name() << ")::inputContext(): frame=" << context.frame() << std::endl;
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
SurfaceShaderOp::setOutputContext(const DD::Image::OutputContext& context)
{
    // Op's implementation simply copies the passed-in context to the
    // Op::outputContext_ variable, so modify the context that we pass
    // up to our parent class:
#if 1
    //std::cout << "SurfaceShaderOp(" << this->node_name() << ")::setOutputContext(): frame=" << context.frame() << std::endl;
    int frame_clamp_mode = int(knob("frame_clamp_mode")->get_value());
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
#else
    DD::Image::Material::setOutputContext(context);
#endif
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
SurfaceShaderOp::knobs(DD::Image::Knob_Callback f)
{
    addSurfaceShaderOpIdKnob(f);
    addRayControlKnobs(f);
}


/*!
*/
/*virtual*/
void
SurfaceShaderOp::_validate(bool for_real)
{
#if 1
    // This validates all inputs:
    DD::Image::Op::_validate(for_real);

    // Do a copy_info() - it's not really needed as Materials
    // don't need to provide a format or bbox unless they're
    // the 2D source for a texture map (which they never are since
    // that doesn't make much sense...)
    copy_info();
#else
    // This appears to just copy_info() on input 0 but doesn't
    // validate all the inputs:
    DD::Image::Material::_validate(for_real);
#endif

    // Build input map bindings:
    m_input_type.clear();
    if (for_real)
    {
        const uint32_t nInputs = DD::Image::Op::inputs();
        m_input_type.reserve(nInputs);
        for (uint32_t i=0; i < nInputs; ++i)
        {
            DD::Image::Op* op = DD::Image::Op::input(i);
            if (op)
                m_input_type.push_back(RayShader::getOpMapBinding(op));
        }
    }

    validateShader(for_real);

    // Always output rgba & z:
    info_.turn_on(DD::Image::Mask_RGBA);
    info_.turn_on(DD::Image::Mask_Z);
}


} // namespace zpr

// end of zprender/SurfaceShaderOp.cpp

//
// Copyright 2020 DreamWorks Animation
//
