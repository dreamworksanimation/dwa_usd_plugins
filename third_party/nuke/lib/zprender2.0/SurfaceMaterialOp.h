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

/// @file zprender/SurfaceMaterialOp.h
///
/// @author Jonathan Egstad


#ifndef zprender_SurfaceMaterialOp_h
#define zprender_SurfaceMaterialOp_h

#include "RayMaterial.h"
#include "ColorMapKnob.h"
#include "RenderContext.h"

#include <DDImage/Material.h>
#include <DDImage/Knobs.h>


namespace zpr {


/*! Base class of ray-tracing material Ops.

*/
class ZPR_EXPORT SurfaceMaterialOp : public DD::Image::Material
{
  public:
    //! Frame clamp modes.
    enum
    {
        FRAME_CLAMP_NONE,
        FRAME_FWD_RND_UP,
        FRAME_FWD_RND_DOWN,
        FRAME_REV_RND_UP,
        FRAME_REV_RND_DOWN
    };
    static const char* frame_clamp_modes[];


  protected:
    RayMaterial::Visibility k_visibility;
    int                     k_frame_clamp_mode;     //!< How this shader uses the frame number from below.


  protected:
    std::vector<uint16_t> m_input_binding_type;     //!< Input binding type - Constant, RayShader, Material, Iop, or Op

    //! Create and return the output surface shader for this Op. Base class does nothing.
    virtual RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                                  std::vector<RayShader*>& shaders) { return NULL; }



  public:
    //! Default the shader channels to RGB.
    SurfaceMaterialOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~SurfaceMaterialOp() {}


    //!
    static const char* zpClass();

    /*! !!HACK ALERT!! This adds an invisible 'zpSurfaceMaterialOp' knob
        that's used to identify a SurfaceMaterialOp-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to SurfaceMaterialOp*.

        Atm if this knob doesn't exist then the evaluate*() methods will
        not be called since the node will not be recognized as a SurfaceMaterialOp!
    */
    void addSurfaceMaterialOpIdKnob(DD::Image::Knob_Callback f);


    //! Allow only RayShaders on input 0.
    /*virtual*/ bool        test_input(int input, Op* op) const;
    /*virtual*/ const char* node_shape() const { return "(|"; }

    //! All material operators default to a orangish color.
    /*virtual*/ uint32_t    node_color() const { return 0xffffffff; }


    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    virtual int32_t getGLTextureInput() const;

    //! Add the ray visibility knobs.
    virtual void addRayControlKnobs(DD::Image::Knob_Callback f);


    /*! Return a pointer to the InputBinding object inside the shader instance
        mapped to an input connection.
        If the result is NULL then there's no way to connect the Op input to
        the shader.
        Base class returns NULL.
    */
    virtual InputBinding* getInputBinding(uint32_t input) { return NULL; }


    //! Create the shaders for one input, returning the output surface shader.
    virtual RayShader* createInputSurfaceShaders(uint32_t                 input,
                                                 const RenderContext&     rtx,
                                                 std::vector<RayShader*>& shaders);

    /*! Allocate a list of RayShaders this Op produces, and returns the output
        connection point.
        Calling object takes ownership of all returned pointers.
    */
    RayShader* createSurfaceShaders(const RenderContext&     rtx,
                                    std::vector<RayShader*>& shaders);


    /*! Allocate and return a RayMaterial filled with all the RayShader comprising
        the shader tree and its input connections. Calling object takes ownership.
        Base class calls createSurfaceShaders() on each SurfaceMaterialOp InputBinding
        or creates a specific Material and Shaders depending on the InputBinding type.
    */
    RayMaterial* createMaterial(const RenderContext& rtx);


  public:
    //------------------------------------------------------------------
    // From DD::Image Iop & Material
    //------------------------------------------------------------------

    /*virtual*/ void  setOutputContext(const DD::Image::OutputContext& context);
    /*virtual*/ const DD::Image::OutputContext& inputContext(int                       input,
                                                             int                       offset,
                                                             DD::Image::OutputContext& context) const;

    /*virtual*/ void append(DD::Image::Hash&);
    /*virtual*/ void  knobs(DD::Image::Knob_Callback f);
    /*virtual*/ void  _validate(bool for_real);

    //------------------------------------
    // Shading / Rendering
    //------------------------------------

    //! Modify the vertex of any 3D geometry that this image is being applied as a shader to. Base class does nothing.
    /*virtual*/ void  vertex_shader(DD::Image::VertexContext&);

    //! Change the value of the out Pixel as though the result of the surface shading. Base class does nothing.
    /*virtual*/ void  fragment_shader(const DD::Image::VertexContext& vtx,
                                      DD::Image::Pixel&               out);
    //! Do the displacement. Base class does nothing.
    /*virtual*/ void  displacement_shader(const DD::Image::VertexContext& vtx,
                                          DD::Image::VArray&              out);

    //! Return the maximum displacement bound. Base class does nothing.
    /*virtual*/ float displacement_bound() const;

    /*virtual*/ void  blending_shader(const DD::Image::Pixel& in,
                                      DD::Image::Pixel&       out);

    //! Change the internal render state of a geoinfo.
    /*virtual*/ void  render_state(DD::Image::GeoInfoRenderState& state);

    /*virtual*/ bool  set_texturemap(DD::Image::ViewerContext* ctx,
                                     bool                      gl);
    /*virtual*/ bool  shade_GL(DD::Image::ViewerContext* ctx,
                               DD::Image::GeoInfo&       info);
    /*virtual*/ void  unset_texturemap(DD::Image::ViewerContext* ctx);

};


} // namespace zpr

#endif

// end of zprender/SurfaceMaterialOp.h

//
// Copyright 2020 DreamWorks Animation
//
