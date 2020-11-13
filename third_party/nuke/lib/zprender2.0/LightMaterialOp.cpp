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

/// @file zprender/LightMaterialOp.cpp
///
/// @author Jonathan Egstad


#include "LightMaterialOp.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "Sampling.h"
#include "VolumeShader.h"

// Remove these when shader creation changed to RayShader::create() calls
#include "zprIopUVTexture.h"
#include "zprAttributeReader.h"


namespace zpr {


/*!
*/
LightMaterialOp::LightMaterialOp(::Node* node) :
    Fsr::FuserLightOp(node)
{
    //
}


/*!
*/
/*virtual*/
LightMaterialOp::~LightMaterialOp()
{
    //
}


//!
/*static*/ const char* LightMaterialOp::zpClass() { return "zpLightMaterialOp"; }

/*!
*/
void
LightMaterialOp::addLightMaterialOpIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, LightMaterialOp::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
LightMaterialOp::knobs(DD::Image::Knob_Callback f)
{
    addLightMaterialOpIdKnob(f);
    Fsr::FuserLightOp::knobs(f);
}


/*! _validate() is called first by RenderContext as it's validating objects.

    So we need to assign the InputBindings now before they get copied into
    the local RayShader vars that get copied to the spawned RayShader.

*/
/*virtual*/
void
LightMaterialOp::_validate(bool for_real)
{
    //std::cout << "LightMaterialOp::validate(" << this << ")"<< std::endl;
    Fsr::FuserLightOp::_validate(for_real);

    // Validate the light shader to use for legacy shading calls:
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader && DD::Image::Op::hash() != m_shader_hash)
    {
        m_shader_hash = DD::Image::Op::hash();
        // Force the shader to revalidate its locals since controls have changed:
        ltshader->invalidate();

        // Delete any rayshaders connected to the output LightShader:
        for (size_t i=0; i < m_shaders.size(); ++i)
            delete m_shaders[i];
        m_shaders.clear();

        // Assign xforms now so that they can be locally fiddled with:
        Fsr::DoubleList motion_times(1, outputContext().frame());
        Fsr::Mat4dList  motion_xforms(1, Fsr::Mat4d(DD::Image::LightOp::matrix()));
        ltshader->setMotionXforms(motion_times, motion_xforms);
        ltshader->validateShader(for_real, NULL/*rtx*/, &outputContext()/*op_ctx*/);
    }
}


/*! Handle channel requests.  Base class does nothing, but Lights
    that read imagery such as environment maps will need to
    implement this.
*/
/*virtual*/
void
LightMaterialOp::request(DD::Image::ChannelMask channels,
                         int                    count)
{
#if 0
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
    }
#endif
    Fsr::FuserLightOp::request(channels, count);
}


/*!
*/
/*virtual*/
int
LightMaterialOp::lightType() const
{
    return DD::Image::LightOp::eOtherLight;
}


/*! Whether the light has a delta distribution (point/spot/direct lights)

    LightMaterialOp forwards this to the output LightShader.
*/
/*virtual*/
bool
LightMaterialOp::is_delta_light() const
{
#if 0
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
    }
#endif
    return Fsr::FuserLightOp::is_delta_light();
}


/*!
*/
/*virtual*/
double
LightMaterialOp::hfov() const
{
#if 0
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
    }
#endif
    return Fsr::FuserLightOp::hfov();
}


/*virtual*/
double
LightMaterialOp::vfov() const
{
#if 0
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
    }
#endif
    return Fsr::FuserLightOp::vfov();
}


/*virtual*/
double
LightMaterialOp::aspect() const
{
#if 0
    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
    }
#endif
    return Fsr::FuserLightOp::aspect();
}


//-------------------------------------------------------------------------


/*! Calculate a normalized direction vector 'lightNOut' and distance
    'lightDistOut' from the light to surface point 'surfP'.

    Normalized vector 'lobeN' is passed to allow lights like area lights
    to simulate a large emission surface. 'lobeN' is usually the surface
    normal when querying the diffuse surface contribution and the
    reflection vector off the surface when querying specular contribution.

    If the passed-in LightContext is castable to RayLightContext then
    this method is being called from zpRender but via a legacy shader
    so this call is reformulated into a LightShader::illuminate() call to
    the output LightShader, storing the results in the zpr::ThreadContext
    referenced by the zpr::Scene lighting scene in the LightContext.

    We can calc the entire thing in LightShader::illuminate() and
    pass the results to the other LightOp shader calls by
    caching the results in the thread-safe ThreadContext and
    relying on the LightOp shading method order of:
        LightOp::get_L_vector()
        LightOp::get_shadowing()
        LightOp::get_color()
*/
/*virtual*/
void
LightMaterialOp::get_L_vector(DD::Image::LightContext&  ltx,
                              const DD::Image::Vector3& surfP,
                              const DD::Image::Vector3& lobeN,
                              DD::Image::Vector3&       lightNOut,
                              float&                    lightDistOut) const
{
    //std::cout << "LightMaterialOp::get_L_vector(" << this << ")" << std::endl;
    zpr::RayLightContext* rltx = zpr::RayLightContext::isRayLightContext(&ltx);
    if (rltx && rltx->enabled())
    {
        //std::cout << "LightMaterialOp(" << node_name() << ")::get_L_vector()";
        //std::cout << " rltx=" << rltx;
        //std::cout << std::endl;
#if DEBUG
        assert(rltx->ttx);
        assert(rltx->light_material);
        assert(rltx->light_material->getLightShader());
#endif
        // Add a new shader context updated with the vectors passed to this
        // method which are likely modifed from the ones in the current
        // surface stx:
        RayShaderContext& stx = rltx->ttx->pushShaderContext(rltx->ttx->currentShaderContext(),
                                                             Fsr::Vec3d(lobeN)/*Rdir*/,
                                                             std::numeric_limits<double>::epsilon()/*tmin*/,
                                                             std::numeric_limits<double>::infinity()/*tmax*/,
                                                             Fsr::RayContext::shadowPath()/*ray_type*/,
                                                             RenderContext::SIDES_BOTH/*sides_mode*/,
                                                             NULL/*Rdif*/);
        // Update the ray origin and surface point values.
        // Light shaders do not need all the other surface parameters up to date:
        stx.Rtx.origin = Fsr::Vec3d(surfP);
        stx.PW = stx.Rtx.origin;
        stx.N  = stx.Rtx.dir();

        Fsr::RayContext&  Rlight      = rltx->ttx->Rlight;
        float&            direct_pdfW = rltx->ttx->direct_pdfW;
        Fsr::Pixel&       illum_color = rltx->ttx->illum_color;
        if (rltx->light_material->getLightShader()->illuminate(stx, Rlight, direct_pdfW, illum_color))
        {
            lightNOut = -Rlight.dir().asDDImage();
            lightDistOut = float(Rlight.maxdist);
        }
        else
        {
            lightNOut = lobeN;
            lightDistOut = /*DD::Image::*/INFINITY; // no illum
        }

        rltx->ttx->popShaderContext(); // restore previous context

        return;
    }

    LightShader* ltshader = const_cast<LightMaterialOp*>(this)->_getOpOutputLightShader();
    if (ltshader)
    {
        // Called from a legacy renderer (ScanlineRender, RayRender):
        ltshader->getLightVector(ltx, surfP, lobeN, lightNOut, lightDistOut);
    }
    else
    {
        // Can't shade without a shader!
        // TODO: print a warning? Throw an Op error?
    }
}


/*! Return the amount of shadowing the light creates at surface point surfP,
    and optionally copies the shadow mask to a channel in shadowChanOut.

    If the passed-in LightContext is castable to RayLightContext then
    this method is being called from zpRender but via a legacy shader
    so we'll retrieve the cached results in the zpr::ThreadContext
    referenced by the zpr::Scene lighting scene in the LightContext.
*/
/*virtual*/
float
LightMaterialOp::get_shadowing(const DD::Image::LightContext&  ltx,
                               const DD::Image::VertexContext& vtx,
                               const DD::Image::Vector3&       surfP,
                               DD::Image::Pixel&               shadowChanOut)
{
    zpr::RayLightContext* rltx = zpr::RayLightContext::isRayLightContext(&ltx);
    if (rltx && rltx->enabled())
    {
        //std::cout << "LightMaterialOp(" << node_name() << ")::get_shadowing()";
        //std::cout << " rltx=" << rltx;
        //std::cout << std::endl;
#if DEBUG
        assert(rltx->ttx);
#endif
        RayShaderContext& stx    = rltx->getShaderContext();
        Fsr::RayContext&  Rlight = rltx->ttx->Rlight;

        // Get shadowing factor for light (0=shadowed, 1=no shadow):
        float shadowFactor = 1.0f; // full illumination
        RayShaderContext Rshadow_stx(stx,
                                     Rlight,
                                     Fsr::RayContext::shadowPath()/*ray_type*/,
                                     RenderContext::SIDES_BOTH/*sides_mode*/);

        // TODO: implement soft shadow loop here!
        Traceable::SurfaceIntersection Ishadow(std::numeric_limits<double>::infinity());
        if (stx.rtx->objects_bvh.getFirstIntersection(Rshadow_stx, Ishadow) > Fsr::RAY_INTERSECT_NONE &&
             Ishadow.t < Rlight.maxdist)
        {
            shadowFactor = 0.0f; // fully-shadowed
        }

        // Copy the shadowing factor to the output shadowmask channel
        // if light has one assigned:
        if (_shadowMaskChannel != DD::Image::Chan_Black)
            shadowChanOut[_shadowMaskChannel] = shadowFactor;

        return shadowFactor;

    }

    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
        // Called from a legacy renderer (ScanlineRender, RayRender):
        return ltshader->getShadowing(ltx, vtx, surfP, shadowChanOut);
    }

    // Can't shade without a shader!
    // TODO: print a warning? Throw an Op error?
    return 1.0f; // no shadowing
}


/*! Returns the color of the light (possibly) using the current
    surface point and normal to calculate attenuation and penumbra.

    If the passed-in LightContext is castable to RayLightContext then
    this method is being called from zpRender but via a legacy shader
    so we'll retrieve the cached results in the zpr::ThreadContext
    referenced by the zpr::Scene lighting scene in the LightContext.
 */
/*virtual*/
void
LightMaterialOp::get_color(DD::Image::LightContext&  ltx,
                           const DD::Image::Vector3& surfP,
                           const DD::Image::Vector3& lobeN,
                           const DD::Image::Vector3& lightN,
                           float                     lightDist,
                           DD::Image::Pixel&         colorChansOut)
{
    zpr::RayLightContext* rltx = zpr::RayLightContext::isRayLightContext(&ltx);
    if (rltx && rltx->enabled())
    {
        //std::cout << "LightMaterialOp(" << node_name() << ")::get_color()";
        //std::cout << " rltx=" << rltx;
        //std::cout << std::endl;
#if DEBUG
        assert(rltx->ttx);
#endif
        Fsr::Pixel& illum_color = rltx->ttx->illum_color;
        float&      direct_pdfW = rltx->ttx->direct_pdfW;

        colorChansOut[DD::Image::Chan_Red  ] = illum_color.r()*direct_pdfW;
        colorChansOut[DD::Image::Chan_Green] = illum_color.g()*direct_pdfW;
        colorChansOut[DD::Image::Chan_Blue ] = illum_color.b()*direct_pdfW;

        return;
    }

    LightShader* ltshader = _getOpOutputLightShader();
    if (ltshader)
    {
        // Called from a legacy renderer (ScanlineRender, RayRender):
        ltshader->getColor(ltx, surfP, lobeN, lightN, lightDist, colorChansOut);
    }
    else
    {
        // Can't shade without a shader!
        // TODO: print a warning? Throw an Op error?
    }
}


//-----------------------------------------------------------------------------


/*! Returns op cast to LightMaterialOp if possible, otherwise NULL.

    For a statically-linked zprender lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/
LightMaterialOp*
LightMaterialOp::getOpAsLightMaterialOp(DD::Image::Op* op)
{
#ifdef ZPR_USE_KNOB_RTTI
    return (op->knob(zpClass())) ? static_cast<LightMaterialOp*>(op) : NULL;
#else
    return (dynamic_cast<LightMaterialOp*>(op) != NULL);
#endif
}


/*! Create the shaders for one input, returning the output RayShader.
    Input shaders to a LightShader are generally RayShaders.
    RenderContext is optional.
*/
/*virtual*/
RayShader*
LightMaterialOp::createInputShader(uint32_t                 input,
                                   const RenderContext*     rtx,
                                   std::vector<RayShader*>& shaders)
{
    if (input >= (uint32_t)DD::Image::Op::inputs())
        return NULL;

    // Let the subclass create a custom input shader:
    RayShader* shader_for_input = _createInputShader(input, rtx, shaders);
    if (shader_for_input)
        return shader_for_input;

    // No custom input shader, use InputBinding logic:

    return NULL;
#if 0
    // Skip input if it's not another SurfaceMaterialOp:
    InputBinding* binding = getInputBindingForOpInput(input);
    if (!binding || !binding->isSurfaceMaterialOp() || !binding->input_object)
        return NULL;

    LightMaterialOp* input_material = static_cast<LightMaterialOp*>(binding->input_object);
    //std::cout << "  " <<  node_name() << "::createInputSurfaceShaders() input " << input << " input_material=" << input_material << " '" << input_material->node_name() << "'" << std::endl;

    // Create the input shader tree and return the output shader:
    return input_material->createShaders(rtx, shaders);
#endif
}


/*! Allocate a list of RayShaders this Op produces, and returns the output
    connection LightShader point.
    Calling object takes ownership of all returned pointers.
*/
LightShader*
LightMaterialOp::createShaders(const RenderContext*     rtx,
                               const Fsr::DoubleList&   motion_times,
                               const Fsr::Mat4dList&    motion_xforms,
                               std::vector<RayShader*>& shaders)
{
    //std::cout << "  " <<  node_name() << "::createShaders()" << std::endl;
    LightShader* output_light_shader = _createOutputLightShader(rtx,
                                                                motion_times,
                                                                motion_xforms,
                                                                shaders);
    if (!output_light_shader)
        return NULL;

    // Use the name of the Op as the shader name:
    std::string output_light_shader_name(node_name());
    output_light_shader_name += "_shader";
    output_light_shader->setName(output_light_shader_name.c_str());

    // Assign the shader's input and output knobs:
    const uint32_t nInputKnobs = output_light_shader->numInputs();
    for (uint32_t input=0; input < nInputKnobs; ++input)
    {
        //std::cout << "      '" << output_light_shader_name << "': input=" << input << std::endl;
        const int32_t op_input = getOpInputForShaderInput(input);
        if (op_input < 0 || op_input >= Op::inputs())
            continue; // skip, no exposed Op connection

        // Get the InputBinding to configure:
        InputBinding* input_binding = output_light_shader->getInputBinding(input);
        if (!input_binding)
            continue; // skip any null bindings

        const RayShader::InputKnob* k_input = output_light_shader->getInputKnob(input);
        assert(k_input);
        //std::cout << "        binding" << *input_binding << ", knob " << *k_input << std::endl;

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
            //std::cout << "    '" << output_light_shader_name << "': input#" << input << " PIXEL_KNOB binding=" << input_binding << *input_binding << std::endl;
            if (input_binding->isSurfaceMaterialOp())
            {
#if 0
                SurfaceMaterialOp* input_material = input_binding->asSurfaceMaterialOp();

                RayShader* input_shader = createInputShader(input, rtx, shaders);
                //std::cout << "      input_shader=" << input_shader << std::endl;

                if (input_shader)
                {
                    //const RayShader::OutputKnob* k_input_shader_output = input_shader->getOutputKnob(0);
                    //std::cout << "      '" << output_light_shader_name << "': connect input '" << k_input->name << "' to input shader's output '" << k_input_shader_output->name << "'" << std::endl;

                    if (output_light_shader->connectInput(input, input_shader, "surface"/*output_name*/))
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
                        output_light_shader->setInputValue(input, chan_text.str().c_str());
                    }
                }
                else
                {
                    //std::cout << "      cannot connect, no input shader to connect to!" << std::endl;
                    std::cerr << node_name() << "::createSurfaceShaders()";
                    std::cerr << " warning cannot connect input '" << k_input->name << "'";
                    std::cerr << ", no shader to connect to.";
                    std::cerr << std::endl;
                    output_light_shader->setInputValue(input, "");
                }
#endif

            }
            else if (input_binding->isTextureIop())
            {
                // Create a shader that gets called to sample the Iop:
                DD::Image::Iop* input_iop = input_binding->asTextureIop();
                //std::cout << "    '" << output_light_shader_name << "': input#" << input << " TEXTURE" << std::endl;

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

                output_light_shader->connectInput(input, input_shader, "rgba"/*output_name*/);

                if (k_input->data != NULL)
                {
                    std::stringstream chan_text;
                    chan_text << input_iop->channels();
                    output_light_shader->setInputValue(input, chan_text.str().c_str());
                }
            }
        }

        //std::cout << "        '" << output_light_shader_name << "': input=" << input << ", binding" << *input_binding << std::endl;
    }

    return output_light_shader;
}


/*! Allocate and return a LightMaterial filled with all the RayShader comprising
    the shader tree and its input connections. Calling object takes ownership.

    Base class calls createShaders() on each LightMaterialOp InputBinding
    or creates a specific Material and Shaders depending on the InputBinding type.
*/
LightMaterial*
LightMaterialOp::createMaterial(const RenderContext*   rtx,
                                const Fsr::DoubleList& motion_times,
                                const Fsr::Mat4dList&  motion_xforms)
{
    //std::cout << "--------------------------------------------------------------" << std::endl;
    //std::cout << "  " <<  node_name() << "::createMaterial()" << std::endl;
    std::vector<RayShader*> all_shaders;
    all_shaders.reserve(5);

    LightShader* output_light_shader = createShaders(rtx,
                                                     motion_times,
                                                     motion_xforms,
                                                     all_shaders);

    if (!output_light_shader || all_shaders.size() == 0)
        return NULL;

    //for (size_t i=0; i < all_shaders.size(); ++i)
    //    std::cout << "    shader " << i << ": " << *all_shaders[i] << std::endl;

    // Create a new material and built its shader tree:
    LightMaterial* material = new LightMaterial(motion_times,
                                                motion_xforms,
                                                all_shaders,
                                                output_light_shader);

    return material;
}


//-----------------------------------------------------------------------------


} // namespace zpr

// end of zprender/LightMaterial.cpp

//
// Copyright 2020 DreamWorks Animation
//
