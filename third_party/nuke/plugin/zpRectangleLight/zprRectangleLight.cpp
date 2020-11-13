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

/// @file zprRectangleLight.cpp
///
/// @author Jonathan Egstad


#include "zprRectangleLight.h"

#include <zprender/ThreadContext.h>
#include <zprender/LightMaterial.h>


namespace zpr {


static RayShader* shaderBuilder() { return new zprRectangleLight(); }
/*static*/ const RayShader::ShaderDescription zprRectangleLight::description("RectangleLight", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprRectangleLight::input_defs =
{
    {InputKnob("map",       PIXEL_KNOB)}, // MAP1
    //{InputKnob("color",     COLOR3_KNOB, "1 1 1")},
    //{InputKnob("intensity", FLOAT_KNOB,  "1"    )},
};
/*static*/ const RayShader::OutputKnobList zprRectangleLight::output_defs =
{
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("r",       FLOAT_KNOB )},
    {OutputKnob("g",       FLOAT_KNOB )},
    {OutputKnob("b",       FLOAT_KNOB )},
};


/*!
*/
zprRectangleLight::zprRectangleLight() :
    LightShader(input_defs, output_defs)
{
    //std::cout << "zprRectangleLight::ctor(" << this << ")" << std::endl;
    inputs.k_color.set(1.0f);
    inputs.k_intensity         = 1.0f;
    //
    inputs.k_lens_in_focal     = 1.0;
    inputs.k_lens_in_haperture = 1.0;
    inputs.k_z                 = 0.0;
    //
    inputs.k_single_sided      = false;
    inputs.k_map_enable        = true;
    for (int z=0; z < 4; ++z)
        inputs.k_map_channel[z] = DD::Image::Channel(DD::Image::Chan_Red + z);
    inputs.k_filter_size       = 2.0;

    // TODO: Switch to bindInput() after adding InputKnob defs
    setInputKnobTarget("color",     &inputs.k_color);
    setInputKnobTarget("intensity", &inputs.k_intensity);
}


/*!
*/
zprRectangleLight::zprRectangleLight(const InputParams&     input_params,
                                     const Fsr::DoubleList& motion_times,
                                     const Fsr::Mat4dList&  motion_xforms) :
    LightShader(input_defs, output_defs, motion_times, motion_xforms),
    inputs(input_params)
{
    //std::cout << "zprRectangleLight::ctor(" << this << ")" << std::endl;
    setInputKnobTarget("color",     &inputs.k_color);
    setInputKnobTarget("intensity", &inputs.k_intensity);
}


/*virtual*/
InputBinding*
zprRectangleLight::getInputBinding(uint32_t input)
{
    assert(input < NUM_INPUTS);
    return &inputs.k_bindings[input];
}


/*!
*/
/*virtual*/
void
zprRectangleLight::setMotionXforms(const Fsr::DoubleList& motion_times,
                                   const Fsr::Mat4dList&  motion_xforms)
{
    m_motion_times = motion_times;
    m_motion_xforms = motion_xforms;
#if DEBUG
    assert(m_motion_times.size() > 0);
    assert(m_motion_xforms.size() == m_motion_times.size());
#endif
    m_motion_ixforms.resize(m_motion_xforms.size());
    for (size_t i=0; i < m_motion_xforms.size(); ++i)
    {
        m_motion_xforms[i].translate(0.0, 0.0, inputs.k_z);
        m_motion_ixforms[i] = m_motion_xforms[i].inverse();
    }
    //std::cout << "zprRectangleLight::setMotionXforms(" << this << ")" << std::endl;
    //std::cout << "  xform" << m_motion_xforms[0] << ", ixform" << m_motion_ixforms[0] << std::endl;
}


/*! Initialize any uniform vars prior to rendering.
    This may be called without a RenderContext from the legacy shader system.
*/
/*virtual*/
void
zprRectangleLight::updateUniformLocals(double  frame,
                                       int32_t view)
{
    //std::cout << "  zprRectangleLight::updateUniformLocals()"<< std::endl;
    LightShader::updateUniformLocals(frame, view); // update m_color

    double lens = (inputs.k_lens_in_haperture / inputs.k_lens_in_focal);
    if (std::fabs(inputs.k_z) > 0.0)
        lens *= 1.0f + std::fabs(inputs.k_z);
    m_width_half  = float(lens / 2.0);
    m_height_half = float(lens / 2.0);

    const InputBinding& map0 = inputs.k_bindings[MAP0];
    if (map0.isTextureIop() && map0.isEnabled())
    {
        DD::Image::Iop* map_iop = map0.asTextureIop();
        map_iop->validate(true);

        const DD::Image::Format& f = map_iop->format();
        const float fW = float(f.w());
        const float fH = float(f.h());
        m_height_half = m_width_half / ((fW / fH) / float(f.pixel_aspect()));

        m_filter_dx.set(float(inputs.k_filter_size)/fW, 0.0f);
        m_filter_dy.set(0.0f, float(inputs.k_filter_size)/fH);
    }
    else
    {
        m_filter_dx.set(0.0f, 0.0f);
        m_filter_dy.set(0.0f, 0.0f);
    }
}


/*!
*/
/*virtual*/
void
zprRectangleLight::validateShader(bool                            for_real,
                                  const RenderContext*            rtx,
                                  const DD::Image::OutputContext* op_ctx)
{
    LightShader::validateShader(for_real, rtx, op_ctx); // validate inputs, update uniforms
    //std::cout << "zprRectangleLight::validateShader() map1=" << getInputShader(MAP0) << std::endl;

    m_texture_channels = DD::Image::Mask_None;
    m_output_channels  = DD::Image::Mask_None;

    const InputBinding& map0 = inputs.k_bindings[MAP0];
    if (map0.isTextureIop())
    {
        m_texture_channels = map0.asTextureIop()->channels();
        m_output_channels  = DD::Image::Mask_RGBA;
        m_output_channels += m_texture_channels;
    }
}


/*!
*/
/*virtual*/
void
zprRectangleLight::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    LightShader::getActiveTextureBindings(texture_bindings); // < get the inputs
    //std::cout << "zprRectangleLight::getActiveTextureBindings() map1=" << getInputShader(MAP0) << std::endl;

    if (inputs.k_bindings[MAP0].isActiveTexture())
        texture_bindings.push_back(&inputs.k_bindings[MAP0]);
}


//-------------------------------------------------------------------------------


/*! This is intentionally implemented as an inline function
    to eliminate an add'l call in virtual shader methods, vs. making
    a macro.
    Compiler should collapse it down due to all the reference arguments.
*/
inline bool
intersectRect(const zprRectangleLight& lt,
              const Fsr::Mat4d&        xform,
              const Fsr::Mat4d&        ixform,
              const Fsr::RayContext&   Rtx,
              Fsr::RayContext&         illum_ray,
              Fsr::Vec2f&              Iuv)
{
    // Plane equation comes from xform:
    const Fsr::Vec3d planeP(xform.getTranslation());
    const Fsr::Vec3d planeN(xform.getZAxis(), 1.0f/*normalize*/);
    const Fsr::Vec3d      L(planeP - Rtx.origin, 1.0f/*normalize*/);

    // Light facing away from surface point?
    if (lt.inputs.k_single_sided && L.dot(-planeN) < 0.0)
        return false;

    // Intersect plane and update D to intersection distance:
    double D = std::numeric_limits<double>::infinity();
    if (!Fsr::intersectPlane(planeP, planeN, Rtx, D))
        return false; // plane not hit!

    // Update output light ray with direction and updated distance:
    illum_ray = Rtx;
    illum_ray.maxdist = D;

    // See if intersection point is within light's rectangle:
    const Fsr::Vec3d st = ixform.transform(illum_ray.getPositionAt(D));
    if (st.x < -lt.m_width_half  || st.x > lt.m_width_half  ||
        st.y < -lt.m_height_half || st.y > lt.m_height_half)
        return false; // nope

    Iuv.set(float(st.x / lt.m_width_half )*0.5f + 0.5f,
            float(st.y / lt.m_height_half)*0.5f + 0.5f);

    return true;
}


/*! Intersect the surface vector with the card plane and find the distance
    which is returned in illum_ray. If the vector doesn't intersect the card
    or is outside the rectangle return false.

    We are doing extra work in this routine to get an accurate Z distance
    (rather than the trivial distance to the center of the light) so that
    surface shaders that care can Z-order the lights prior to calling the
    more expensive get_color() in order to layer light colors properly.
*/
/*virtual*/
bool
zprRectangleLight::illuminate(RayShaderContext& stx,
                              Fsr::RayContext&  illum_ray,
                              float&            direct_pdfW_out,
                              Fsr::Pixel&       illum_color_out)
{
    //std::cout << "zprRectangleLight::illuminate(" << this << ")" << std::endl;
    Fsr::Mat4d xform, ixform;
    zpr::getMotionXformsAt(m_motion_times,
                           stx.frame_time,
                           m_motion_xforms,
                           m_motion_ixforms,
                           xform,
                           ixform);
//std::cout << "zprRectangleLight::illuminate(" << this << ")" << std::endl;
//std::cout << "  xform" << xform << ", ixform" << ixform << std::endl;

    // If illum_ray intersects rectangle outside w/h return false:
    Fsr::Vec2f Iuv;
    if (!intersectRect(*this,
                       xform,
                       ixform,
                       stx.Rtx,
                       illum_ray,
                       Iuv))
        return false;

    // Calc power falloff factor:
    direct_pdfW_out = 1.0f;//float(D*D);

    Fsr::Pixel& map_color = stx.thread_ctx->binding_color;
    InputBinding& map0 = inputs.k_bindings[MAP0];
    if (map0.isActiveColor())
    {
        map0.sampleTexture(Iuv,
                           m_filter_dx/*dUVdx*/,
                           m_filter_dy/*dUVdy*/,
                           &inputs.k_map_filter,
                           map_color);
        //map0.getValue(stx, map_color);
        illum_color_out.rgb() = map_color.rgb()*m_color.rgb();
        illum_color_out.a()   = 1.0f;
    }
    else
    {
        illum_color_out.rgb() = m_color.rgb();
        illum_color_out.a()   = 1.0f;
    }

    return true;
}


//-------------------------------------------------------------------------------


/*! Calculate a normalized direction vector 'lightNOut' and distance
    'lightDistOut' from the light to surface point 'surfP'.

    Normalized vector 'lobeN' is passed to allow lights like area lights
    to simulate a large emission surface. 'lobeN' is usually the surface
    normal when querying the diffuse surface contribution and the
    reflection vector off the surface when querying specular contribution.
*/
/*virtual*/
void
zprRectangleLight::getLightVector(const DD::Image::LightContext& ltx,
                                  const DD::Image::Vector3&      surfP,
                                  const DD::Image::Vector3&      lobeN,
                                  DD::Image::Vector3&            lightNOut,
                                  float&                         lightDistOut) const
{
//std::cout << "zprRectangleLight::getLightVector(" << this << ")" << std::endl;
//std::cout << "  xform" << m_motion_xforms[0] << ", ixform" << m_motion_ixforms[0] << std::endl;
    const Fsr::RayContext Rtx(Fsr::Vec3d(surfP), Fsr::Vec3d(lobeN), 0.0/*time*/);
    Fsr::RayContext illum_ray;
    Fsr::Vec2f      Iuv;
    if (intersectRect(*this,
                      m_motion_xforms[0],
                      m_motion_ixforms[0],
                      Rtx,
                      illum_ray,
                      Iuv))
    {
        lightNOut    = -Rtx.dir().asDDImage();
        lightDistOut = float(illum_ray.maxdist);
    }
    else
    {
        lightNOut    = lobeN; // reverse causes failure
        lightDistOut = std::numeric_limits<float>::infinity();
    }
    //std::cout << "      lightNOut" << Fsr::Vec3f(lightNOut) << std::endl;
    //std::cout << "  lightDistOut=" << lightDistOut << std::endl;
}


/*! Return the amount of shadowing the light creates at surface point surfP,
    and optionally copies the shadow mask to a channel in shadowChanOut.
*/
/*virtual*/
float
zprRectangleLight::getShadowing(const DD::Image::LightContext&  ltx,
                                const DD::Image::VertexContext& vtx,
                                const DD::Image::Vector3&       surfP,
                                DD::Image::Pixel&               shadowChanOut) const
{
    // TODO: I don't think we can implement anything here.
    return 1.0f;
}


/*! Returns the color of the light (possibly) using the current
    surface point and normal to calculate attenuation and penumbra.
*/
/*virtual*/
void
zprRectangleLight::getColor(const DD::Image::LightContext& ltx,
                            const DD::Image::Vector3&      surfP,
                            const DD::Image::Vector3&      lobeN,
                            const DD::Image::Vector3&      lightN,
                            float                          lightDist,
                            DD::Image::Pixel&              colorChansOut) const
{
    //std::cout << "zprRectangleLight::getColor(" << this << ")" << std::endl;

    // Unfortunately we need to calculate all this again...:
    const Fsr::RayContext Rtx(Fsr::Vec3d(surfP), Fsr::Vec3d(lobeN), 0.0/*time*/);
    Fsr::RayContext illum_ray;
    Fsr::Vec2f      Iuv;
    if (intersectRect(*this,
                      m_motion_xforms[0],
                      m_motion_ixforms[0],
                      Rtx,
                      illum_ray,
                      Iuv))
    {
        const InputBinding& map0 = inputs.k_bindings[MAP0];
        if (map0.isActiveColor())
        {
            Fsr::Pixel map_color;
            map0.sampleTexture(Iuv,
                               m_filter_dx/*dUVdx*/,
                               m_filter_dy/*dUVdy*/,
                               &inputs.k_map_filter,
                               map_color);
            colorChansOut[DD::Image::Chan_Red  ] = map_color.r()*m_color.r();
            colorChansOut[DD::Image::Chan_Green] = map_color.g()*m_color.g();
            colorChansOut[DD::Image::Chan_Blue ] = map_color.b()*m_color.b();
            colorChansOut[DD::Image::Chan_Alpha] = 1.0f;//map_color.a()*m_color.a();
        }
        else
        {
            colorChansOut[DD::Image::Chan_Red  ] = m_color.r();
            colorChansOut[DD::Image::Chan_Green] = m_color.g();
            colorChansOut[DD::Image::Chan_Blue ] = m_color.b();
            colorChansOut[DD::Image::Chan_Alpha] = 1.0f;//m_color.a();
        }

    }
    else
    {
        colorChansOut[DD::Image::Chan_Red  ] = 0.0f;
        colorChansOut[DD::Image::Chan_Green] = 0.0f;
        colorChansOut[DD::Image::Chan_Blue ] = 0.0f;
        colorChansOut[DD::Image::Chan_Alpha] = 0.0f;
    }

}


} // namespace zpr

// end of zprRectangleLight.cpp

//
// Copyright 2020 DreamWorks Animation
//
