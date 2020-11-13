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

/// @file zprender/LightMaterialOp.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightMaterialOp_h
#define zprender_LightMaterialOp_h

#include <Fuser/LightOp.h>

#include "LightMaterial.h"
#include "ColorMapKnob.h"


namespace zpr {


/*! Extension interface for LightOps to help export their ray-tracing
    LightMaterial and LightShader/RayShader networks.

    This needs to be a subclass of Fuser::LightOp so that we can cast
    the DD::Image::LightOp pointer to access the createMaterial() method.
*/
class ZPR_EXPORT LightMaterialOp : public Fsr::FuserLightOp
{
  protected:
    std::vector<uint16_t>   m_input_binding_type;   //!< Input binding type - Constant, RayShader, Material, Iop, or Op
    //
    DD::Image::Hash         m_shader_hash;          //!< Whether input shaders need to be rebuilt
    std::vector<RayShader*> m_shaders;              //!< List of allocated shaders *not including* output LightShader


  public:
    //!
    LightMaterialOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~LightMaterialOp();


    //---------------------------------------------------------------------


    //!
    static const char* zpClass();

    /*! !!HACK ALERT!! This adds an invisible 'zpLightMaterialOp' knob
        that's used to identify a LightMaterial-interfaced LightOp to other
        plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to LightMaterialOp*.

        Atm if this knob doesn't exist then the evaluate*() methods will
        not be called since the node will not be recognized as a LightMaterialOp!
    */
    void addLightMaterialOpIdKnob(DD::Image::Knob_Callback f);

    //! Returns op cast to LightMaterialOp if possible, otherwise NULL.
    static LightMaterialOp* getOpAsLightMaterialOp(DD::Image::Op* op);


    //---------------------------------------------------------------------


    //! Return the input number to use for the OpenGL texture display, if appropriate.
    virtual int32_t getGLTextureInput() const { return -1; }


    /*! Return a pointer to the InputBinding object inside the shader instance
        mapped to an input connection.
        If the result is NULL then there's no way to connect the Op input to
        the shader.
        Base class returns NULL.
    */
    virtual InputBinding*  getInputBindingForOpInput(uint32_t op_input) { return NULL; }

    /*! Return the Op input for a shader input, or -1 if binding is not exposed.
        Base class returns -1.
    */
    virtual int32_t        getOpInputForShaderInput(uint32_t shader_input) { return -1; }


    /*! Create the shaders for one input, returning the output RayShader.
        Input shaders to a LightShader are generally RayShaders.
        RenderContext is optional.
    */
    virtual RayShader* createInputShader(uint32_t                 input,
                                         const RenderContext*     rtx,
                                         std::vector<RayShader*>& shaders);

    /*! Allocate a list of RayShaders this object produces, and returns the
        connection LightShader point.
        Calling object takes ownership of all returned pointers.
        RenderContext is optional.
    */
    LightShader* createShaders(const RenderContext*     rtx,
                               const Fsr::DoubleList&   motion_times,
                               const Fsr::Mat4dList&    motion_xforms,
                               std::vector<RayShader*>& shaders);


    /*! Allocate and return a RayMaterial filled with all the RayShader comprising
        the shader tree and its input connections. Calling object takes ownership.
        Base class calls createShaders() on each LightMaterialOp InputBinding
        or creates a specific Material and Shaders depending on the InputBinding type.
        RenderContext is optional.
    */
    LightMaterial* createMaterial(const RenderContext*   rtx,
                                  const Fsr::DoubleList& motion_times,
                                  const Fsr::Mat4dList&  motion_xforms);


  protected:
    /*! Create the shaders for one input, returning the output surface shader.

        Implement this to return a custom shader for an input. If not implemented the
        standard InputBinding logic is used.

        Base class does nothing.
    */
    virtual LightShader* _createInputShader(uint32_t                 input,
                                            const RenderContext*     rtx,
                                            std::vector<RayShader*>& shaders) { return NULL; }


    /*! Create and return the output light shader for this object.
        RenderContext is optional.
        Base class does nothing.
    */
    virtual LightShader* _createOutputLightShader(const RenderContext*     rtx,
                                                  const Fsr::DoubleList&   motion_times,
                                                  const Fsr::Mat4dList&    motion_xforms,
                                                  std::vector<RayShader*>& shaders) { return NULL; }


    /*! For legacy shading system.
        Return the local LightShader object which the LightMaterialOp stores
        its knobs into.
        If this LightShader is non-null it will be called in the legacy
        get_L_vector(), get_shadowing(), and get_color() methods.
    */
    virtual LightShader* _getOpOutputLightShader() { return NULL; }


  public:
    //------------------------------------------------------------------
    // From DD::Image ComplexLightOp
    //------------------------------------------------------------------

    /*virtual*/ void  knobs(DD::Image::Knob_Callback f);
    /*virtual*/ void  _validate(bool for_real);

    /*! Handle channel requests.  Base class does nothing, but Lights
        that read imagery such as environment maps will need to
        implement this.
    */
    /*virtual*/ void request(DD::Image::ChannelMask channels,
                             int                    count);

    /*!
    */
    /*virtual*/ int lightType() const;

    /*! Whether the light has a delta distribution (point/spot/direct lights)

        LightMaterialOp forwards this to the output LightShader.
    */
    /*virtual*/ bool is_delta_light() const;


    /*!
    */
    /*virtual*/ double   hfov() const;
    /*virtual*/ double   vfov() const;
    /*virtual*/ double aspect() const;


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
    void get_L_vector(DD::Image::LightContext&  ltx,
                      const DD::Image::Vector3& surfP,
                      const DD::Image::Vector3& lobeN,
                      DD::Image::Vector3&       lightNOut,
                      float&                    lightDistOut) const;


    /*! Return the amount of shadowing the light creates at surface point surfP,
        and optionally copies the shadow mask to a channel in shadowChanOut.

        If the passed-in LightContext is castable to RayLightContext then
        this method is being called from zpRender but via a legacy shader
        so we'll retrieve the cached results in the zpr::ThreadContext
        referenced by the zpr::Scene lighting scene in the LightContext.
    */
    /*virtual*/
    float get_shadowing(const DD::Image::LightContext&  ltx,
                        const DD::Image::VertexContext& vtx,
                        const DD::Image::Vector3&       surfP,
                        DD::Image::Pixel&               shadowChanOut);


    /*! Returns the color of the light (possibly) using the current
        surface point and normal to calculate attenuation and penumbra.

        If the passed-in LightContext is castable to RayLightContext then
        this method is being called from zpRender but via a legacy shader
        so we'll retrieve the cached results in the zpr::ThreadContext
        referenced by the zpr::Scene lighting scene in the LightContext.
     */
    /*virtual*/
    void get_color(DD::Image::LightContext&  ltx,
                   const DD::Image::Vector3& surfP,
                   const DD::Image::Vector3& lobeN,
                   const DD::Image::Vector3& lightN,
                   float                     lightDist,
                   DD::Image::Pixel&         colorChansOut);

};


} // namespace zpr

#endif

// end of zprender/LightMaterial.h

//
// Copyright 2020 DreamWorks Animation
//
