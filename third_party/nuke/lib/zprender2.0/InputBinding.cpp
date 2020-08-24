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

/// @file zprender/InputBinding.cpp
///
/// @author Jonathan Egstad


#include "InputBinding.h"
#include "RayShader.h"
#include "RayMaterial.h"
#include "SurfaceMaterialOp.h"
#include "RenderContext.h"
#include "ThreadContext.h"

#include <Fuser/api.h> // for stringTrim
#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/Knobs.h>
#include <DDImage/Material.h>
#include <DDImage/AxisOp.h>
#include <DDImage/noise.h>


using namespace DD::Image;

namespace zpr {

/*
    USD defines these connection types in the Sdr lib (Shader Definition Registry.)
    https://graphics.pixar.com/usd/docs/api/sdr_page_front.html

    We'll use this as a guide for what types of inputs to support.

    // Non interpolating:
    Int,      "int"
    String,   "string"

    // Interpolateable (per-texel, ie texture-mappable)
    Float,    "float"
    Color,    "color"
    Point,    "point"
    Normal,   "normal"
    Vector,   "vector"
    Matrix,   "matrix"

    // Abstract types:
    Struct,   "struct"
    Terminal, "terminal"
    Vstruct,  "vstruct"
    Unknown,  "unknown"
*/


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------


inline void copy_attrib2f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 2*sizeof(float)); out.z = 0.0f; out.w = 1.0f; }
inline void copy_attrib3f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 3*sizeof(float)); out.w = 1.0f; }
inline void copy_attrib4f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 4*sizeof(float)); }

inline void copy_attrib2d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = 0.0f;          out.w = 1.0f; }
inline void copy_attrib3d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = float(*src++); out.w = 1.0f; }
inline void copy_attrib4d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = float(*src++); out.w = float(*src); }


static void handler_null(  const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { memset(&out.x, 0, 4*sizeof(float)); }
//----------------------------------------
static void handler_const( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out = etx.const_val; }
static void handler_white( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set( 1.0f,  1.0f,  1.0f, 1.0f); }
static void handler_black( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set( 0.0f,  0.0f,  0.0f, 1.0f); }
static void handler_grey18(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set(0.18f, 0.18f, 0.18f, 1.0f); }
static void handler_grey50(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set( 0.5f,  0.5f,  0.5f, 1.0f); }
static void handler_inf(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set(std::numeric_limits<float>::infinity(),
                                                                                            std::numeric_limits<float>::infinity(),
                                                                                            std::numeric_limits<float>::infinity(),
                                                                                            1.0f); }
//----------------------------------------
static void handler_V(    const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec3d V(-etx.stx->Rtx.dir());
    out.set(float(V.x), float(V.y), float(V.z), 1.0f);
}
static void handler_Z(    const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out.set(float(etx.stx->distance),
                                                                                           float(etx.stx->distance),
                                                                                           float(etx.stx->distance), 1.0f); }
//----------------------------------------
static void handler_PW(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->PW.array(),    out); }
static void handler_dPWdx(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dPWdx.array(), out); }
static void handler_dPWdy(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dPWdy.array(), out); }
static void handler_PWg(  const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->PWg.array(),   out); }
static void handler_PL(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    if (!etx.stx->w2l) {
        copy_attrib3d(etx.stx->PW.array(), out);
    } else {
        const Fsr::Vec3d PL = etx.stx->w2l->transform(etx.stx->PW);
        copy_attrib3d(PL.array(), out);
    }
}
//----------------------------------------
static void handler_N(    const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->N.array(),    out); }
static void handler_dNdx( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dNdx.array(), out); }
static void handler_dNdy( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dNdy.array(), out); }
static void handler_Nf(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Nf.array(),   out); }
static void handler_Ni(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Ni.array(),   out); }
static void handler_Ng(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Ng.array(),   out); }
//----------------------------------------
static void handler_st(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->st.array(),   out); }
static void handler_dstdx(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec2f d(etx.stx->Rxst - etx.stx->st);
    copy_attrib2f(d.array(), out);
}
static void handler_dstdy(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec2f d(etx.stx->Ryst - etx.stx->st);
    copy_attrib2f(d.array(), out);
}
//----------------------------------------
static void handler_UV(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->UV.array(),    out); }
static void handler_dUVdx(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->dUVdx.array(), out); }
static void handler_dUVdy(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->dUVdy.array(), out); }
//----------------------------------------
static void handler_Cf(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->Cf;    }
static void handler_dCfdx(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->dCfdx; }
static void handler_dCfdy(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->dCfdy; }
//----------------------------------------
static void handler_t(   const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float f = float(etx.stx->frame_time);
    out.set(f, f, f, 1.0f);
}
static void handler_dtdx(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { handler_null(etx, out); }
static void handler_dtdy(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) { handler_null(etx, out); }
//----------------------------------------
static void handler_VdotN( const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float v = float(etx.stx->N.dot(-etx.stx->Rtx.dir()));
    out.set(v, v, v, 1.0f);
}
static void handler_VdotNg(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float v = float(etx.stx->Ng.dot(-etx.stx->Rtx.dir()));
    out.set(v, v, v, 1.0f);
}
static void handler_VdotNf(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float v = float(etx.stx->Nf.dot(-etx.stx->Rtx.dir()));
    out.set(v, v, v, 1.0f);
}

//----------------------------------------
/*  noise.h:
    DDImage_API double noise(double x);
    DDImage_API double noise(double x, double y);
    DDImage_API double noise(double x, double y, double z);
    DDImage_API double fBm(double x, double y, double z,
                           int octaves, double lacunarity, double gain);
    DDImage_API double turbulence(double x, double y, double z,
                                  int octaves, double lacunarity, double gain);
    DDImage_API double p_random(int x);
    DDImage_API double p_random(int x, int y);
    DDImage_API double p_random(int x, int y, int z);
    DDImage_API double p_random(double x);
    DDImage_API double p_random(double x, double y);
    DDImage_API double p_random(double x, double y, double z);
*/
static void handler_noise_PW(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::noise(float(etx.stx->PW.x), float(etx.stx->PW.y), float(etx.stx->PW.z)));
    out.set(n, n, n, 1.0f);
}
static void handler_random_PW(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::p_random(float(etx.stx->PW.x), float(etx.stx->PW.y), float(etx.stx->PW.z)));
    out.set(n, n, n, 1.0f);
}
static void handler_noise_UV(const InputBinding::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::noise(etx.stx->UV.x, etx.stx->UV.y));
    out.set(n, n, n, 1.0f);
}


//----------------------------------------------------------------------------------


typedef std::map<std::string, InputBinding::Handler> HandlerMap;

struct AttribHandlers
{
    HandlerMap map;

    AttribHandlers()
    {
        // Add all the build-in handlers:
        map[std::string("white" )] = handler_white;
        map[std::string("black" )] = handler_black;
        map[std::string("grey"  )] = handler_grey18;
        map[std::string("grey18")] = handler_grey18;
        map[std::string("grey50")] = handler_grey50;
        map[std::string("inf"   )] = handler_inf;
        //
        map[std::string("v"     )] = handler_V;
        map[std::string("z"     )] = handler_Z;
        //
        map[std::string("pw"    )] = handler_PW;
        map[std::string("dpwdx" )] = handler_dPWdx;
        map[std::string("dpwdy" )] = handler_dPWdy;
        map[std::string("pwg"   )] = handler_PWg;
        map[std::string("pl"    )] = handler_PL;
        //
        map[std::string("vdotn" )] = handler_VdotN;
        map[std::string("vdotng")] = handler_VdotNg;
        map[std::string("vdotnf")] = handler_VdotNf;
        //
        map[std::string("n"     )] = handler_N;
        map[std::string("dNdx"  )] = handler_dNdx;
        map[std::string("dNdy"  )] = handler_dNdy;
        map[std::string("nf"    )] = handler_Nf;
        map[std::string("ni"    )] = handler_Ni;
        map[std::string("ng"    )] = handler_Ng;
        //
        map[std::string("st"    )] = handler_st;
        map[std::string("dstdx" )] = handler_dstdx;
        map[std::string("dstdy" )] = handler_dstdy;
        //
        map[std::string("uv"    )] = handler_UV;
        map[std::string("duvdx" )] = handler_dUVdx;
        map[std::string("duvdy" )] = handler_dUVdy;
        //
        map[std::string("t"     )] = handler_t;
        map[std::string("dtdx"  )] = handler_dtdx;
        map[std::string("dtdy"  )] = handler_dtdy;
        //
        map[std::string("cf"    )] = handler_Cf;
        map[std::string("dcfdx" )] = handler_dCfdx;
        map[std::string("dcfdy" )] = handler_dCfdy;
        //
        map[std::string("t"     )] = handler_t;
        map[std::string("time"  )] = handler_t;
        //
        map[std::string("noisePW" )] = handler_noise_PW;
        map[std::string("randomPW")] = handler_random_PW;
        map[std::string("noiseUV" )] = handler_noise_UV;
    }
};
static AttribHandlers attrib_handlers;


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------


//!
std::ostream& operator << (std::ostream& o, const InputBinding& b)
{
    o << "[";
    switch (b.type)
    {
        default:
        case InputBinding::NONE:              o << "<none>";       break;
        case InputBinding::CONSTANT:          o << "Constant";     break;
        case InputBinding::ATTRIB:            o << "Attribute";    break;
        case InputBinding::RAYSHADER:         o << "RayShader";    break;
        case InputBinding::SURFACEMATERIALOP: o << "SurfaceMaterialOp"; break;
        case InputBinding::MATERIALIOP:       o << "Material";     break;
        case InputBinding::TEXTUREIOP:        o << "Iop";          break;
        case InputBinding::AXISOP:            o << "AxisOp";       break;
        case InputBinding::CAMERAOP:          o << "CameraOp";     break;
        case InputBinding::LIGHTOP:           o << "LightOp";      break;
    }
    if (!b.isEnabled())
        return o;

    if (b.isNukeOp() || b.isRayShader())
    {
        o << " op=" << b.input_object;
        if (b.isTextureIop())
            o << " (udim" << b.uv_tile_offset << ")";
    }
    else if (b.isAttrib())
        o << " hdlr=" << (void*)b.handler;
    else if (b.isConstant())
        o << b.constant_value;

    o << " chans=" << b.num_channels;
    if (b.hasRGB())
        o << " rgb[" << b.rgb_chans[0] << " " << b.rgb_chans[1] << " " << b.rgb_chans[2] << "]";
    if (b.hasAlpha())
        o << " opc=" << b.opacity_chan;
    o << "]";

    return o;
}



//!
InputBinding::InputBinding(uint16_t _type) :
    type(_type),
    flags(0x00),
    constant_value(1.0f),
    input_object(NULL),
    opacity_chan(DD::Image::Chan_Black),
    num_channels(0),
    uv_tile_offset(0.0f, 0.0f),
    handler(handler_null)
{
    rgb_chans[0] = rgb_chans[1] = rgb_chans[2] = DD::Image::Chan_Black;
}


/*! Builds a ChannelSet on the fly from the chans.
*/
DD::Image::ChannelSet
InputBinding::getChannels() const
{
    DD::Image::ChannelSet channels(rgb_chans, 3);
    channels += opacity_chan;
    return channels;
}


//!
void
InputBinding::setActiveChannels(DD::Image::Channel red_chan,
                                DD::Image::Channel green_chan,
                                DD::Image::Channel blue_chan,
                                DD::Image::Channel _opacity_chan)
{
    num_channels = 0;
    if (red_chan != DD::Image::Chan_Black)
    {
        rgb_chans[num_channels++] = red_chan;
    }
    if (green_chan != DD::Image::Chan_Black)
    {
        rgb_chans[num_channels++] = green_chan;
    }
    if (blue_chan != DD::Image::Chan_Black)
    {
        rgb_chans[num_channels++] = blue_chan;
    }
    if (num_channels == 3)
        flags |= (uint16_t)HAS_RGB;
    else
        flags &= (uint16_t)~HAS_RGB;

    if (num_channels == 1)
        flags |= (uint16_t)IS_MONO;
    else
        flags &= (uint16_t)~IS_MONO;

    if (_opacity_chan != DD::Image::Chan_Black)
    {
        opacity_chan = _opacity_chan;
        flags |= (uint16_t)HAS_ALPHA;
        ++num_channels;
    }
    else
        flags &= (uint16_t)~HAS_ALPHA;
}


//!
void
InputBinding::setToConstantBinding(const Fsr::Vec3f& constant)
{
    constant_value.set(constant.x, constant.y, constant.z);
    type           = CONSTANT;
    handler        = handler_const;
    setActiveChannels(DD::Image::Chan_Red,
                      DD::Image::Chan_Green,
                      DD::Image::Chan_Blue,
                      DD::Image::Chan_Black);
}

//!
void
InputBinding::setToConstantBinding(const Fsr::Vec4f& constant)
{
    constant_value = constant;
    type           = CONSTANT;
    handler        = handler_const;
    setActiveChannels(DD::Image::Chan_Red,
                      DD::Image::Chan_Green,
                      DD::Image::Chan_Blue,
                      DD::Image::Chan_Alpha);
}


/*! Parses the binding expression and return a configured InputBinding.
*/
/*static*/
InputBinding
InputBinding::buildFromBindExpression(const char* expr)
{
    InputBinding binding; // default to none binding

    if (expr == NULL || expr[0] == 0)
        return binding; // no valid binding

    std::string s(Fsr::stringTrim(std::string(expr)));
    std::transform(s.begin(), s.end(), s.begin(), ::tolower); 

    // First check for input or map strings:
    if (Fsr::stringStartsWith(s, "map") || s.empty())
    {
        // Is there a UDIM tileID after 'map'?
        const uint32_t udim = ::atoi(s.c_str()+3);
        if (udim >= 1001 && udim <= 9999)
        {
            // Yep! ex. 1011 = 1000+(0(u) + 1(v))+(1(v) * 10)
            const uint32_t utile = (udim - 1001) % 10;
            const uint32_t vtile = (udim - 1001) / 10;
            //std::cout << "'" << s << "' u=" << utile << ", v=" << vtile << std::endl;
            binding.uv_tile_offset.set(float(utile), float(vtile));
        }

        binding.type = InputBinding::TEXTUREIOP;
    }
    else if (Fsr::stringStartsWith(s, "attr"))
    {
        // TODO: finish this thought....
        binding.type = ATTRIB;
        binding.setActiveChannels(DD::Image::Chan_Red,
                                  DD::Image::Chan_Green,
                                  DD::Image::Chan_Blue,
                                  DD::Image::Chan_Black);
    }
    else
    {
        // See if attrib type is in the map:
        HandlerMap::const_iterator it = attrib_handlers.map.find(s);
        if (it != attrib_handlers.map.end())
        {
            // yep, retrieve handler:
            binding.type    = ATTRIB;
            binding.handler = it->second;
            binding.setActiveChannels(DD::Image::Chan_Red,
                                      DD::Image::Chan_Green,
                                      DD::Image::Chan_Blue,
                                      DD::Image::Chan_Black);
            //std::cout << "bound to " << s << "attrib" << std::endl;
        }
        else
        {
            // Check if it's a numerical constant:
            Fsr::Vec4f vals;
            int n = sscanf(expr, "%f %f %f %f", &vals.x, &vals.y, &vals.z, &vals.w);
            //std::cout << "'" << expr << "': " << n << ", [" << vals.x << " " << vals.y << " " << vals.z << " " << vals.w << "]" << std::endl;
            if (n == 1)
                binding.setToConstantBinding(Fsr::Vec3f(vals.x, vals.x, vals.x));
            else if (n == 2)
                binding.setToConstantBinding(Fsr::Vec4f(vals.x, vals.x, vals.x, vals.y));
            else if (n == 3)
                binding.setToConstantBinding(Fsr::Vec3f(vals.x, vals.y, vals.z));
            else if (n == 4)
                binding.setToConstantBinding(vals);
        }
    }

    return binding;
}


/*! Get the binding configuration for an input Op.

    This will not support a connection to a RayShader as it's not an
    Op subclass.
*/
/*static*/
InputBinding
InputBinding::buildInputOpBinding(DD::Image::Op* op)
{
    InputBinding binding;
    if (op)
    {
        // Determine input type:
#ifdef ZPR_USE_KNOB_RTTI
        if (op->knob(SurfaceMaterialOp::zpClass()))
#else
        if (dynamic_cast<SurfaceMaterialOp*>(op))
#endif
        {
            binding.type = SURFACEMATERIALOP;
            binding.input_object = (void*)static_cast<SurfaceMaterialOp*>(op);
        }
        else if (dynamic_cast<DD::Image::Material*>(op))
        {
            // TODO: do we need this anymore...?
            binding.type = MATERIALIOP;
            binding.input_object = (void*)static_cast<DD::Image::Material*>(op);
        }
        else if (dynamic_cast<DD::Image::Iop*>(op))
        {
            // Only allow connection if it's NOT the default Black Iop:
            if (strcmp(op->Class(), "Black") != 0)
            {
                binding.type = TEXTUREIOP;
                binding.input_object = (void*)static_cast<DD::Image::Iop*>(op);
            }
        }
        else if (dynamic_cast<DD::Image::LightOp*>(op))
        {
            binding.type = LIGHTOP;
            binding.input_object = (void*)static_cast<DD::Image::LightOp*>(op);
        }
        else if (dynamic_cast<DD::Image::CameraOp*>(op))
        {
            binding.type = CAMERAOP;
            binding.input_object = (void*)static_cast<DD::Image::CameraOp*>(op);
        }
        else if (dynamic_cast<DD::Image::AxisOp*>(op))
        {
            binding.type = AXISOP;
            binding.input_object = (void*)static_cast<DD::Image::AxisOp*>(op);
        }
    }

    return binding;
}


/*! Get the binding configuration for an input Iop.
*/
/*static*/
InputBinding
InputBinding::buildInputTextureBinding(DD::Image::Iop*    iop,
                                       DD::Image::Channel red_chan,
                                       DD::Image::Channel green_chan,
                                       DD::Image::Channel blue_chan,
                                       DD::Image::Channel _opacity_chan)
{
    if (!iop)
        return InputBinding();

    iop->validate(true);

    InputBinding binding(TEXTUREIOP);
    binding.input_object = (void*)iop;

    // Does input offer the color channels? Do this in
    // rgba order:
    if (red_chan != DD::Image::Chan_Black && iop->channels().contains(red_chan))
    {
        binding.rgb_chans[binding.num_channels++] = red_chan;
    }
    if (green_chan != DD::Image::Chan_Black && iop->channels().contains(green_chan))
    {
        binding.rgb_chans[binding.num_channels++] = green_chan;
    }
    if (blue_chan != DD::Image::Chan_Black && iop->channels().contains(blue_chan))
    {
        binding.rgb_chans[binding.num_channels++] = blue_chan;
    }
    if (binding.num_channels == 3)
        binding.flags |= (uint16_t)HAS_RGB;
    else if (binding.num_channels == 1)
        binding.flags |= (uint16_t)IS_MONO;

    // Does input offer an alpha?
    if (_opacity_chan != DD::Image::Chan_Black && iop->channels().contains(_opacity_chan))
    {
        binding.opacity_chan = _opacity_chan;
        binding.flags |= (uint16_t)HAS_ALPHA;
        ++binding.num_channels;
    }

    return binding;
}


/*!
*/
void
InputBinding::getValue(RayShaderContext& stx,
                       Fsr::Pixel&       out)
{
    //std::cout << "InputBinding::getValue(" << this << "): " << *this << std::endl;
    if (!isEnabled())
    {
        out.setChannels(DD::Image::Mask_RGBA);
        out.rgba().set(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    else if (type <= ATTRIB)
    {
        // Attribute binding, call the handler:
        out.setChannels(DD::Image::Mask_RGBA);
        //---------------
        handler(ExprContext(&stx, constant_value), out.rgba()); // execute handler
        //---------------
        return;
    }
    else if (isTextureIop())
    {
        // Common texture binding type:
        Fsr::Pixel& texture_color = stx.thread_ctx->texture_color;
        sampleTexture(stx, texture_color);

        out.setChannels(DD::Image::Mask_RGBA);
        out.rgba().set(texture_color[rgb_chans[0]],
                       texture_color[rgb_chans[1]],
                       texture_color[rgb_chans[2]],
                       texture_color[opacity_chan]);
        return;
    }
    else if (isRayShader())
    {
        asRayShader()->evaluateSurface(stx, out);
        return;
    }
    else if (isMaterialIop())
    {
        // Having Pixel be set to black is essential to front-to-back
        // under-ing because the Nuke legacy shader's are doing overs
        // internally:
        out.setChannels(DD::Image::Mask_RGBA);
        out.erase(); // this does a memset on the entire Pixel

        zpr::RayMaterial::updateDDImageShaderContext(stx, stx.thread_ctx->vtx);
        asMaterialIop()->fragment_shader(stx.thread_ctx->vtx, out);
        return;
    }

    // Type not recognized:
    out.setChannels(DD::Image::Mask_RGBA);
    out.rgba().set(0.0f, 0.0f, 0.0f, 0.0f);
}


/*!
*/
Fsr::Vec3f
InputBinding::getValue(RayShaderContext& stx,
                       float*            out_alpha)
{
    if (!isEnabled())
    {
        if (out_alpha)
            *out_alpha = 0.0f;
        return Fsr::Vec3f(0.0f);
    }

    Fsr::Pixel& value = stx.thread_ctx->binding_color;
    value.setChannels(DD::Image::Mask_RGBA);
    getValue(stx, value);

    if (out_alpha)
        *out_alpha = value.alpha();
    return value.rgb();
}


/*!
*/
void
InputBinding::sampleTexture(RayShaderContext& stx,
                            Fsr::Pixel&       tex_color)
{
    //std::cout << "InputBinding::sampleTexture(" << this << "): type=" << type << ", obj=" << input_object << std::endl;
    tex_color.setChannels(getChannels());

    DD::Image::Iop* iop = asTextureIop();
    if (!iop)
    {
        tex_color[rgb_chans[0]] = 0.0f;
        tex_color[rgb_chans[1]] = 0.0f;
        tex_color[rgb_chans[2]] = 0.0f;
        tex_color[opacity_chan] = 0.0f;
        return;
    }

    const DD::Image::Format& f = iop->format();
    const float fX = float(f.x());
    const float fY = float(f.y());
    const float fW = float(f.w());
    const float fH = float(f.h());
    const Fsr::Vec2f uv(stx.UV - uv_tile_offset);

    if (!stx.texture_filter)
    {
        //-------------------------------------------------------------
        // Texture filtering disabled
        //
        // the scene filter is set to null in Render::_validate() for impulse filter
        // the material->sample() will use impulse if the filter is null
        //-------------------------------------------------------------
        iop->at(int32_t(floorf(fX + uv.x*fW)),
                int32_t(floorf(fY + uv.y*fH)),
                tex_color);
    }
    else
    {
        // Get the Texture2dSampler for the texture Iop:
        // TODO: this should not be required anymore!!!
        Texture2dSamplerMap::iterator it = stx.rtx->texture_sampler_map.find(iop);
        Texture2dSampler* tex_sampler = (it != stx.rtx->texture_sampler_map.end()) ? it->second : NULL;
        if (tex_sampler)
        {
            tex_sampler->sampleFilterered(stx.UV - uv_tile_offset, 
                                          stx.dUVdx,
                                          stx.dUVdy,
                                          stx.texture_filter,
                                          tex_color);
        }
        else
        {
            // Fallback to slow Iop::sample():
#if 0
            // TODO: see if we can get the mip filter to work!
            DD::Image::TextureMipSample(DD::Image::Vector2(fX + uv.x*fW, fY + uv.y*fH), /*xy*/
                                        DD::Image::Vector2(stx.dUVdx.x*fW,   stx.dUVdx.y*fH  ), /*dU*/
                                        DD::Image::Vector2(stx.dUVdy.x*fW,   stx.dUVdy.y*fH  ), /*dV*/
                                        stx.texture_filter,
                                        Iop** mip_iops,
                                        tex_color);
#else
            const Fsr::Vec2f uv(stx.UV - uv_tile_offset);
            iop->sample(DD::Image::Vector2(fX + uv.x*fW, fY + uv.y*fH), /*xy*/
                        DD::Image::Vector2(stx.dUVdx.x*fW,   stx.dUVdx.y*fH  ), /*dU*/
                        DD::Image::Vector2(stx.dUVdy.x*fW,   stx.dUVdy.y*fH  ), /*dV*/
                        stx.texture_filter,
                        tex_color);
#endif
        }
    }

}


} // namespace zpr

// end of zprender/InputBinding.cpp

//
// Copyright 2020 DreamWorks Animation
//
