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

/// @file zprender/SurfaceShaderOp.h
///
/// @author Jonathan Egstad


#ifndef zprender_SurfaceShaderOp_h
#define zprender_SurfaceShaderOp_h

#include "RayShader.h"

#include <DDImage/Material.h>
#include <DDImage/Knobs.h>


namespace zpr {


/*! Base class of ray-tracing shader Ops.
    DD::Image::Op::Iop::Material
*/
class ZPR_EXPORT SurfaceShaderOp : public RayShader,
                                   public DD::Image::Material
{
  public:
    //! Default the shader channels to RGB.
    SurfaceShaderOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~SurfaceShaderOp() {}


    //!
    static const char* zpClass();

    /*! !!HACK ALERT!! This adds an invisible 'zpSurfaceShaderOp' knob
        that's used to identify a SurfaceShaderOp-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to SurfaceShaderOp*.

        Atm if this knob doesn't exist then the _evaluate*() methods will
        not be called since the node will not be recognized as a SurfaceShaderOp!
    */
    void addSurfaceShaderOpIdKnob(DD::Image::Knob_Callback f);


    //! Allow only RayShaders on input 0.
    /*virtual*/ bool        test_input(int input, Op* op) const;
    /*virtual*/ const char* node_shape() const { return "(|"; }

    //! All material operators default to a orangish color.
    /*virtual*/ uint32_t    node_color() const { return 0xffffffff; }


    //! Returns input pointer cast to Op if input is Op not NULL.
    DD::Image::Op*       opInput(int n) const;
    //! Returns input pointer cast to Iop if input is an Iop subclass.
    DD::Image::Iop*      iopInput(int n) const;
    //! Returns input pointer cast to Material if input is a Material subclass.
    DD::Image::Material* materialInput(int n) const;
    //! Returns input pointer cast to RayShader if input is a RayShader subclass.
    SurfaceShaderOp*   rayShaderInput(int n) const;


  protected:
    //------------------------------------------------------------------
    // Subclasses implement these calls to modify the shading.
    // Called from base class high-level methods like getIllumination().
    //------------------------------------------------------------------

    //! The surface evaluation shader call.  If doing final displacement implement _evaluateDisplacement() instead.
    /*virtual*/ void _evaluateGeometricShading(RayShaderContext& stx,
                                               RayShaderContext& out);

    //! The ray-tracing surface shader evaluation call.
    /*virtual*/ void _evaluateShading(RayShaderContext& stx,
                                      Fsr::Pixel&       out);

    //! The ray-tracing displacement shader evaluation call.
    /*virtual*/ void _evaluateDisplacement(RayShaderContext& stx,
                                           Fsr::Pixel&       out);


  public:
    //------------------------------------------------------------------
    // From DD::Image Iop & Material
    //------------------------------------------------------------------

    /*virtual*/ void  setOutputContext(const DD::Image::OutputContext& context);
    /*virtual*/ const DD::Image::OutputContext& inputContext(int                       input,
                                                             int                       offset,
                                                             DD::Image::OutputContext& context) const;
    /*virtual*/ void  knobs(DD::Image::Knob_Callback f);

    //
    /*virtual*/ void  _validate(bool for_real);

    //------------------------------------
    // Shading / Rendering
    //------------------------------------

    //! Modify the vertex of any 3D geometry that this image is being applied as a shader to. Base class does nothing.
    /*virtual*/ void  vertex_shader(DD::Image::VertexContext&) {/*do nothing*/}

    //! Change the value of the out Pixel as though the result of the surface shading. Base class does nothing.
    /*virtual*/ void  fragment_shader(const DD::Image::VertexContext& vtx,
                                      DD::Image::Pixel&               out) { out.erase();/*do nothing*/}
    //! Do the displacement. Base class does nothing.
    /*virtual*/ void  displacement_shader(const DD::Image::VertexContext& vtx,
                                          DD::Image::VArray&              out) {/*do nothing*/}

    //! Return the maximum displacement bound. Base class does nothing.
    /*virtual*/ float displacement_bound() const { return 0.0f; }

    /*virtual*/ void  blending_shader(const DD::Image::Pixel& in,
                                      DD::Image::Pixel&       out) {/*do nothing*/}

    //! Change the internal render state of a geoinfo.
    /*virtual*/ void  render_state(DD::Image::GeoInfoRenderState& state) { /*base class does nothing*/ }

    /*virtual*/ bool  set_texturemap(DD::Image::ViewerContext* ctx,
                                     bool                      gl) { return input0().set_texturemap(ctx, gl); }
    /*virtual*/ bool  shade_GL(DD::Image::ViewerContext* ctx,
                               DD::Image::GeoInfo&       info) { return input0().shade_GL(ctx, info); }
    /*virtual*/ void  unset_texturemap(DD::Image::ViewerContext* ctx) { input0().unset_texturemap(ctx); }

};


} // namespace zpr

#endif

// end of zprender/SurfaceShaderOp.h

//
// Copyright 2020 DreamWorks Animation
//
