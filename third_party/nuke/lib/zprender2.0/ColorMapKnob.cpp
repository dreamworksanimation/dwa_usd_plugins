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

/// @file zprender/ColorMapKnob.cpp
///
/// @author Jonathan Egstad


#include "ColorMapKnob.h"
#include "SurfaceShaderOp.h"

#include <Fuser/api.h> // for stringTrim
#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/Knobs.h>
#include <DDImage/noise.h>


using namespace DD::Image;

namespace zpr {


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------


inline void copy_attrib2f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 2*sizeof(float)); out.z = 0.0f; out.w = 1.0f; }
inline void copy_attrib3f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 3*sizeof(float)); out.w = 1.0f; }
inline void copy_attrib4f(const float* src, Fsr::Vec4f& out) { memcpy(&out.x, src, 4*sizeof(float)); }

inline void copy_attrib2d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = 0.0f;          out.w = 1.0f; }
inline void copy_attrib3d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = float(*src++); out.w = 1.0f; }
inline void copy_attrib4d(const double* src, Fsr::Vec4f& out) { out.x = float(*src++); out.y = float(*src++); out.z = float(*src++); out.w = float(*src); }


static void handler_null(  const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { memset(&out.x, 0, 4*sizeof(float)); }
//----------------------------------------
static void handler_const( const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out = etx.const_val; }
static void handler_white( const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set( 1.0f,  1.0f,  1.0f, 1.0f); }
static void handler_black( const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set( 0.0f,  0.0f,  0.0f, 1.0f); }
static void handler_grey18(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set(0.18f, 0.18f, 0.18f, 1.0f); }
static void handler_grey50(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set( 0.5f,  0.5f,  0.5f, 1.0f); }
static void handler_inf(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set(std::numeric_limits<float>::infinity(),
                                                                                            std::numeric_limits<float>::infinity(),
                                                                                            std::numeric_limits<float>::infinity(),
                                                                                            1.0f); }
//----------------------------------------
static void handler_V(    const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec3d V(-etx.stx->Rtx.dir());
    out.set(float(V.x), float(V.y), float(V.z), 1.0f);
}
static void handler_Z(    const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out.set(float(etx.stx->distance),
                                                                                           float(etx.stx->distance),
                                                                                           float(etx.stx->distance), 1.0f); }
//----------------------------------------
static void handler_PW(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->PW.array(),    out); }
static void handler_dPWdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dPWdx.array(), out); }
static void handler_dPWdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dPWdy.array(), out); }
static void handler_PWg(  const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->PWg.array(),   out); }
static void handler_PL(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    if (!etx.stx->w2l) {
        copy_attrib3d(etx.stx->PW.array(), out);
    } else {
        const Fsr::Vec3d PL = etx.stx->w2l->transform(etx.stx->PW);
        copy_attrib3d(PL.array(), out);
    }
}
//----------------------------------------
static void handler_N(    const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->N.array(),     out); }
static void handler_Nf(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Nf.array(),    out); }
static void handler_Ng(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Ng.array(),    out); }
static void handler_Ngf(  const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Ngf.array(),   out); }
static void handler_Ns(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->Ns.array(),    out); }
static void handler_dNsdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dNsdx.array(), out); }
static void handler_dNsdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib3d(etx.stx->dNsdy.array(), out); }
//----------------------------------------
static void handler_st(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->st.array(),    out); }
static void handler_dstdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec2f d(etx.stx->Rxst - etx.stx->st);
    copy_attrib2f(d.array(), out);
}
static void handler_dstdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const Fsr::Vec2f d(etx.stx->Ryst - etx.stx->st);
    copy_attrib2f(d.array(), out);
}
//----------------------------------------
static void handler_UV(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->UV.array(),    out); }
static void handler_dUVdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->dUVdx.array(), out); }
static void handler_dUVdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { copy_attrib2f(etx.stx->dUVdy.array(), out); }
//----------------------------------------
static void handler_Cf(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->Cf;    }
static void handler_dCfdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->dCfdx; }
static void handler_dCfdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { out = etx.stx->dCfdy; }
//----------------------------------------
static void handler_t(   const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float f = float(etx.stx->frame_time);
    out.set(f, f, f, 1.0f);
}
static void handler_dtdx(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { handler_null(etx, out); }
static void handler_dtdy(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) { handler_null(etx, out); }
//----------------------------------------
static void handler_VdotN( const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float v = float(etx.stx->N.dot(-etx.stx->Rtx.dir()));
    out.set(v, v, v, 1.0f);
}
static void handler_VdotNg(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float v = float(etx.stx->Ng.dot(-etx.stx->Rtx.dir()));
    out.set(v, v, v, 1.0f);
}
static void handler_VdotNf(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
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
static void handler_noise_PW(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::noise(float(etx.stx->PW.x), float(etx.stx->PW.y), float(etx.stx->PW.z)));
    out.set(n, n, n, 1.0f);
}
static void handler_random_PW(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::p_random(float(etx.stx->PW.x), float(etx.stx->PW.y), float(etx.stx->PW.z)));
    out.set(n, n, n, 1.0f);
}
static void handler_noise_UV(const ColorMapKnob::ExprContext& etx, Fsr::Vec4f& out) {
    const float n = float(DD::Image::noise(etx.stx->UV.x, etx.stx->UV.y));
    out.set(n, n, n, 1.0f);
}


//----------------------------------------------------------------------------------


typedef std::map<std::string, ColorMapKnob::Handler> HandlerMap;

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
        map[std::string("nf"    )] = handler_Nf;
        map[std::string("ng"    )] = handler_Ng;
        map[std::string("ngf"   )] = handler_Ngf;
        //
        map[std::string("ns"    )] = handler_Ns;
        map[std::string("dNsdx" )] = handler_dNsdx;
        map[std::string("dNsdy" )] = handler_dNsdy;
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


/*!
*/
ColorMapKnob::ColorMapKnob(DD::Image::Iop*    parent,
                           int                input_num,
                           int                num_chans,
                           DD::Image::Channel first_chan) :
    k_enable(true),
    k_binding_expr("map"),
    m_parent(parent),
    m_handler(handler_null),
    m_uv_tile_offset(0.0f, 0.0f)
{
    setInput(input_num, num_chans, first_chan);
    m_knob_names[0] = m_knob_names[1] = 0;
    m_constant.set(1,1,1,1);
}


/*virtual*/
ColorMapKnob::~ColorMapKnob()
{
    //
}


/*!
*/
void
ColorMapKnob::setInput(int                input_num,
                       int                num_chans,
                       DD::Image::Channel first_chan)
{
    m_input = input_num;
    m_num_channels = num_chans;
    for (int z=0; z < m_num_channels; ++z)
        k_map_chans[z] = DD::Image::Channel(first_chan + z);
    m_channels.clear();
    m_channels.insert(k_map_chans, m_num_channels);
}


/*!
*/
/*virtual*/
DD::Image::Knob*
ColorMapKnob::addKnobs(DD::Image::Knob_Callback f,
                       const char*              name,
                       const char*              label)
{
    if (m_knob_names[0] == 0)
    {
        // Build knob names:
        char buf[128];
        snprintf(buf, 128, "%s_source", name);
        m_knob_names[0] = strdup(buf);
        snprintf(buf, 128, "%s_layer", name);
        m_knob_names[1] = strdup(buf);
    }
    Newline(f, label);
    Bool_knob(f, &k_enable, "");
    String_knob(f, &k_binding_expr, m_knob_names[0], "");
        ClearFlags(f, Knob::STARTLINE);
        SetFlags(f, Knob::EARLY_STORE);
        Tooltip(f, "Text entry defining where to source the map value from.\n"
                   "Supports a limited set of keywords:\n"
                   "<b>Input arrow connection:</b>"
                         "<ul>"
                         "<li><i>map, (empty string)</i> - Sample a 2D texture input (or another shader)</li>"
                         "</ul>"
                   "<b>Set map value to a constant color with an alpha of 1:</b>"
                         "<ul>"
                         "<li>type in a color value - 1,3&4 vals supported</li>"
                         "<li><i>white</i>  - 1</li>"
                         "<li><i>black</i>  - 0</li>"
                         "<li><i>grey, grey18</i> - 18% grey</li>"
                         "<li><i>grey50</i> - 50% grey</li>"
                         "<li><i>inf</i>    - infinity</li>"
                         "</ul>"
                   "<b>Hardcoded shading attributes:</b>"
                         "<ul>"
                         "<li><i>V</i>      - View-vector from surface point to camera origin (normalized)</li>"
                         "<li><i>Z</i>      - Ray depth (distance) from camera</li>"
                         //"<li><i>Zl</i>     - Linearly projected depth from camera</li>"
                         "<li><i>PW</i>     - Displaced shading point in world-space</li>"
                         "<li><i>dPWdx</i>  - PW x-derivative</li>"
                         "<li><i>dPWdy</i>  - PW y-derivative</li>"
                         "<li><i>PL</i>     - Shading point in local-space</li>"
                         "<li><i>PWg</i>    - Geometric surface point (no displacement)</li>"
                         "<li><i>st</i>     - Primitive's barycentric coordinates</li>"
                         "<li><i>dstdx</i>  - st x-derivative</li>"
                         "<li><i>dstdy</i>  - st y-derivative</li>"
                         "<li><i>N</i>      - Shading normal (interpolated & bumped vertex normal)</li>"
                         "<li><i>Nf</i>     - Face-forward shading normal</li>"
                         "<li><i>Ng</i>     - Geometric surface normal</li>"
                         "<li><i>Ngf</i>    - Face-forward geometric normal</li>"
                         "<li><i>Ns</i>     - Interpolated surface normal (same as N but with no bump)</li>"
                         "<li><i>dNsdx</i>  - Ns x-derivative</li>"
                         "<li><i>dNsdy</i>  - Ns y-derivative</li>"
                         "<li><i>UV</i>     - Surface texture coordinate</li>"
                         "<li><i>dUVdx</i>  - UV x-derivative</li>"
                         "<li><i>dUVdy</i>  - UV y-derivative</li>"
                         "<li><i>Cf</i>     - vertex color (stands for 'Color front')</li>"
                         "<li><i>dCfdx</i>  - Cf x-derivative</li>"
                         "<li><i>dCfdy</i>  - Cf y-derivative</li>"
                         "<li><i>t, time</i> - frame time</li>"
                         "</ul>"
                   "<b>Shading calculations:</b>"
                         "<ul>"
                         "<li><i>VdotN</i>  - Facing-ratio of shading normal</li>"
                         "<li><i>VdotNg</i> - Facing-ratio of geometric normal</li>"
                         "<li><i>VdotNf</i> - Facing-ratio of face-forward shading normal</li>"
                         "</ul>");

    Knob* chan_knob = Input_Channel_knob(f, k_map_chans, m_num_channels/*count*/, m_input/*input*/, m_knob_names[1], "layer:");
        ClearFlags(f, Knob::STARTLINE);
        SetFlags(f, Knob::NO_CHECKMARKS);
        if (m_num_channels < 4)
            SetFlags(f, Knob::NO_ALPHA_PULLDOWN);
        Tooltip(f, "Map source layer to use");


    if (f.makeKnobs())
    {
        //===============================================================================
        // MAKE KNOBS
        //===============================================================================
        // Construct knobs:
        // You can't access any knobs at this point, they don't exist and the 'knob()' call
        // will return 0.
        //std::cout << " <makeKnobs>" << std::endl;

    }
    else
    {
        //===============================================================================
        // STORE KNOBS
        //===============================================================================
        // Store knobs.  This is called on every user change of the tree, so if we check
        // this after all the knobs have stored themselves we can make early decisions:
        //std::cout << " <store>: expr='" << k_binding_expr << "'" << std::endl;
    }

    return chan_knob;
}


/*!
*/
/*virtual*/
bool
ColorMapKnob::knobChanged(Knob* k)
{
    //std::cout << "knob_changed('" << k->name() << "')" << std::endl;
    if (k->name() == m_knob_names[0]     ||
        k == &DD::Image::Knob::showPanel ||
        k == &DD::Image::Knob::inputChange)
    {
        // Expression text possibly changed, enable/disable the channel knob:
        this->validateColorMap(false);

        m_parent->knob(m_knob_names[1])->enable(isTexture());
        //std::cout << "  '" << k_binding_expr << "', texture=" << is_texture() << std::endl;

        return true; // call me again
    }
    return false; // don't call me again
}


/*!
*/
uint32_t
ColorMapKnob::parseExpression(const char* expr)
{
    m_handler = handler_null;
    m_hash.reset();
    // If string is empty default to OP input:
    if (expr == NULL || expr[0] == 0)
        return RayShader::MapBinding::OP;

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
            m_uv_tile_offset.set(float(utile), float(vtile));
        }

        return RayShader::MapBinding::OP;
    }

    // See if attrib is in the map:
    HandlerMap::const_iterator it = attrib_handlers.map.find(s);
    if (it != attrib_handlers.map.end())
    {
        m_handler = it->second;
        //std::cout << "bound to " << s << "attrib" << std::endl;
        return RayShader::MapBinding::ATTRIB;
    }

    // Check if it's a numerical constant:
    Fsr::Vec4f vals;
    int n = sscanf(expr, "%f %f %f %f", &vals.x, &vals.y, &vals.z, &vals.w);
    //std::cout << "'" << expr << "': " << n << ", [" << vals.x << " " << vals.y << " " << vals.z << " " << vals.w << "]" << std::endl;
    if (n == 1)
    {
        m_constant.set(vals.x, vals.x, vals.x, 1.0f);
        m_handler = handler_const;
        return RayShader::MapBinding::ATTRIB;
    }
    else if (n == 3)
    {
        m_constant.set(vals.x, vals.y, vals.z, 1.0f);
        m_handler = handler_const;
        return RayShader::MapBinding::ATTRIB;
    }
    else if (n == 4)
    {
        m_constant.set(vals.x, vals.y, vals.z, vals.w);
        m_handler = handler_const;
        return RayShader::MapBinding::ATTRIB;
    }

    return RayShader::MapBinding::NONE;
}


/*!
*/
void
ColorMapKnob::validateColorMap(bool for_real)
{
    m_channels.clear();
    m_binding.type  = 0x00;
    m_binding.flags = 0x00;

    uint32_t type = parseExpression(k_binding_expr);
    if (type == RayShader::MapBinding::NONE)
    {
        // No binding, disable it:
        return;
    }

    if (type == RayShader::MapBinding::ATTRIB)
    {
        // Handle attrib binding:
        m_binding.type = RayShader::MapBinding::ATTRIB;
        m_channels = Mask_RGBA; // Default channel set
        m_binding.flags |= RayShader::MapBinding::HAS_ALPHA;
        return;
    }

    // Handle input texture or shader binding:
    Iop* iop = m_parent->input(m_input);
    if (!iop)
        return; // No iop, disable it

    // Have an Iop input, check if it's valid:
    iop->validate(for_real);
    m_channels.insert(k_map_chans, m_num_channels);
    const int nSelectedChans = m_channels.size();

    // If no selected channels or no available channels, disable it:
    if (iop->channels() == Mask_None)
        return;

    m_binding = RayShader::getOpMapBinding(iop, k_map_chans[3]);

    // Is it mono?
    if (nSelectedChans == 1 || (nSelectedChans == 2 && hasAlpha()))
        m_binding.flags |= RayShader::MapBinding::IS_MONO;

    //std::cout << "input " << m_input << " m_channels=" << m_channels << " nSelectedChans=" << nSelectedChans << " is_mono=" << is_mono() << " has_alpha=" << has_alpha() << std::endl;
}


/*!
*/
void
ColorMapKnob::requestColorMap(int count) const
{
    if (!k_enable || !isTexture())
        return;

    // Request texture map channels:
    DD::Image::ChannelSet c1(m_channels);
    c1 += Mask_Alpha;

    DD::Image::Iop* iop = m_parent->input(m_input);
#if DEBUG
    assert(iop);
#endif

    const DD::Image::Box& b = iop->info();
    iop->request(b.x(), b.y(), b.r(), b.t(), c1, count);
}


/*!
*/
Fsr::Vec3f
ColorMapKnob::sample(RayShaderContext& stx,
                     float*            out_alpha)
{
    if (!k_enable || m_binding.noBinding())
    {
        if (out_alpha)
            *out_alpha = 0.0f;

        return Fsr::Vec3f(0.0f, 0.0f, 0.0f);
    }
    else if (m_binding.isAttrib())
    {
        // Attribute binding, call the handler:
        ExprContext etx;
        etx.stx       = &stx;
        etx.const_val = m_constant;

        Fsr::Vec4f out;
        m_handler(etx, out); // execute handler

        if (out_alpha)
            *out_alpha = out.w;

        return Fsr::Vec3f(out.x, out.y, out.z);
    }

    // Change sampling depending on input Iop type:
    Iop* iop = m_parent->input(m_input);
    Fsr::Pixel   color_out(m_channels);
    Fsr::Pixel opacity_out(m_channels);
    if (m_binding.isSurfaceShaderOp())
    {
        // Call the RayShader:
        static_cast<SurfaceShaderOp*>(iop)->evaluateShading(stx, color_out);
    }
    else if (m_binding.isMaterial())
    {
        DD::Image::VertexContext vtx;
        zpr::RayShader::updateDDImageShaderContext(stx, vtx);

        // Having Pixel be set to black is essential to front-to-back
        // under-ing because the Nuke legacy shader's are doing overs
        // internally:
        color_out.erase(); // this does a memset on the entire Pixel
        iop->fragment_shader(vtx, color_out);
    }
    else
    {
        // It's an Iop texture, sample the image directly:
        const DD::Image::Format& f = iop->format();
        const float fX = float(f.x());
        const float fY = float(f.y());
        const float fW = float(f.w());
        const float fH = float(f.h());
        const Fsr::Vec2f uv(stx.UV - m_uv_tile_offset);
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
                    color_out);
        }
        else
        {
#if 0
            // TODO: see if we can get the mip filter to work!
            DD::Image::TextureMipSample(DD::Image::Vector2(fX + uv.x*fW, fY + uv.y*fH), /*xy*/
                                        DD::Image::Vector2(stx.dUVdx.x*fW,   stx.dUVdx.y*fH  ), /*dU*/
                                        DD::Image::Vector2(stx.dUVdy.x*fW,   stx.dUVdy.y*fH  ), /*dV*/
                                        stx.texture_filter,
                                        Iop** mip_iops,
                                        color_out);
#else
            const Fsr::Vec2f uv(stx.UV - m_uv_tile_offset);
            iop->sample(DD::Image::Vector2(fX + uv.x*fW, fY + uv.y*fH), /*xy*/
                        DD::Image::Vector2(stx.dUVdx.x*fW,   stx.dUVdx.y*fH  ), /*dU*/
                        DD::Image::Vector2(stx.dUVdy.x*fW,   stx.dUVdy.y*fH  ), /*dV*/
                        stx.texture_filter,
                        color_out);
#endif
        }
    }

    if (out_alpha)
    {
        if (useAlpha())
            *out_alpha = color_out[k_map_chans[3]];
        else
            *out_alpha = 1.0f;
    }

    if (m_binding.isMono())
    {
        const float v = color_out[k_map_chans[0]];
        return Fsr::Vec3f(v, v, v);
    }

    return Fsr::Vec3f(color_out[k_map_chans[0]],
                      color_out[k_map_chans[1]],
                      color_out[k_map_chans[2]]);
}


} // namespace zpr

// end of zprender/ColorMapKnob.cpp

//
// Copyright 2020 DreamWorks Animation
//
