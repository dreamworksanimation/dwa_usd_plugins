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

/// @file zprReadUVTexture.cpp
///
/// @author Jonathan Egstad


#include "zprReadUVTexture.h"
#include "RenderContext.h"

#include <DDImage/Reader.h>
#include <DDImage/ARRAY.h>
#include <DDImage/Row.h>
#include <DDImage/LUT.h>
#include <DDImage/Thread.h>
#include <DDImage/plugins.h>

#include <sys/stat.h> // to check texture file existence

DD::Image::Lock  op_lock;

namespace zpr {


static RayShader* shaderBuilder() { return new zprReadUVTexture(); }
/*static*/ const RayShader::ShaderDescription zprReadUVTexture::description("ReadUVTexture", shaderBuilder);
/*static*/ const RayShader::InputKnobList zprReadUVTexture::input_defs =
{
    {InputKnob("file",     STRING_KNOB, ""       )},
    {InputKnob("wrapS",    INT_KNOB,    "0"      )},
    {InputKnob("wrapT",    INT_KNOB,    "0"      )},
    {InputKnob("fallback", COLOR4_KNOB, "1 1 1 1")},
    {InputKnob("scale",    COLOR4_KNOB, "1 1 1 1")},
    {InputKnob("bias",     COLOR4_KNOB, "0 0 0 0")},
};
/*static*/ const RayShader::OutputKnobList zprReadUVTexture::output_defs =
{
    {OutputKnob("rgb",     COLOR3_KNOB)},
    {OutputKnob("rgba",    COLOR4_KNOB)},
    {OutputKnob("r",       FLOAT_KNOB )},
    {OutputKnob("g",       FLOAT_KNOB )},
    {OutputKnob("b",       FLOAT_KNOB )},
    {OutputKnob("a",       FLOAT_KNOB )},
};


/*!
*/
zprReadUVTexture::zprReadUVTexture(const char* path) :
    RayShader(input_defs, output_defs),
    m_read(NULL),
    m_file_exists(false),
    m_read_error(true)
{
    //std::cout << "zprReadUVTexture::ctor(" << this << ")" << std::endl;
    // Assign the knobs to their values:
    assert(m_inputs.size() == 6 && m_inputs.size() == input_defs.size());
    assignInputKnob("file",     &k_file);
    assignInputKnob("wrapS",    &k_wrapS);
    assignInputKnob("wrapT",    &k_wrapT);
    assignInputKnob("fallback", &k_fallback);
    assignInputKnob("scale",    &k_scale);
    assignInputKnob("bias",     &k_bias);

    if (path)
        k_file = path;
}


/*!
*/
zprReadUVTexture::~zprReadUVTexture()
{
    //std::cout << "zprReadUVTexture::dtor(" << this << ")" << std::endl;
    delete m_read;
}


/*!
*/
/*virtual*/
void
zprReadUVTexture::validateShader(bool                 for_real,
                                 const RenderContext& rtx)
{
    //std::cout << "zprReadUVTexture::validateShader() file='" << k_file << "'" << std::endl;
    RayShader::validateShader(for_real, rtx);

    m_file_exists = false;
    if (!k_file.empty())
    {
        struct stat st;
        if (stat(k_file.c_str(), &st) == 0)
        {
            m_file_exists = true;
        }
    }


    if (m_file_exists && !m_read)
    {
        //std::cout << "  file exists '" << k_file << "', CREATE READ" << std::endl;
        op_lock.lock();
        {
            m_read = new DD::Image::Read(NULL/*node*/);
            m_read->parent(rtx.m_parent); // set the parent to avoid issues with undo/error system
            assert(m_read);
        }
        op_lock.unlock();

        m_read->filename(k_file.c_str());
        m_read->validate(for_real);
        if (!m_read->hasError())
        {
            //std::cout << "    READ OK read=" << m_read << " '" << k_file << "', channels=" << m_read->channels() << std::endl;
            m_texture_channels = m_read->channels();

            const DD::Image::Channel rchan = (m_texture_channels.contains(DD::Image::Chan_Red  )) ? DD::Image::Chan_Red   : DD::Image::Chan_Black;
            const DD::Image::Channel gchan = (m_texture_channels.contains(DD::Image::Chan_Green)) ? DD::Image::Chan_Green : DD::Image::Chan_Black;
            const DD::Image::Channel bchan = (m_texture_channels.contains(DD::Image::Chan_Blue )) ? DD::Image::Chan_Blue  : DD::Image::Chan_Black;
            const DD::Image::Channel achan = (m_texture_channels.contains(DD::Image::Chan_Alpha)) ? DD::Image::Chan_Alpha : DD::Image::Chan_Black;
            m_binding = zpr::InputBinding::buildInputTextureBinding(m_read, rchan, gchan, bchan, achan);

            m_read_error = false;
        }
        else
        {
            //std::cout << "    ERROR '" << k_file << "'" << std::endl;
        }
    }

    if (m_read_error)
    {
        m_binding = zpr::InputBinding();
        m_texture_channels = DD::Image::Mask_None;
    }

    m_output_channels = m_texture_channels;
    //std::cout << "  channels=" << m_texture_channels << std::endl;
}


/*!
*/
/*virtual*/
void
zprReadUVTexture::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    //std::cout << "zprReadUVTexture::getActiveTextureBindings() m_read=" << m_read << std::endl;
    if (m_binding.isActiveTexture())
        texture_bindings.push_back(&m_binding);
}


/*! TODO: support the uv scale, offset, wrap
*/
/*virtual*/
void
zprReadUVTexture::evaluateSurface(RayShaderContext& stx,
                                  Fsr::Pixel&       out)
{
    //std::cout << "zprReadUVTexture::evaluateSurface() [" << stx.x << " " << stx.y << "]" << std::endl;
    if (m_texture_channels.empty())
    {
        //out.setChannels(getChannels());
        //out[m_binding.rgb_chans[0]] = 0.0f;
        //out[m_binding.rgb_chans[1]] = 0.0f;
        //out[m_binding.rgb_chans[2]] = 0.0f;
        //out[m_binding.opacity_chan] = 0.0f;
        out.rgb().set(0.0f);
        out.alpha() = 1.0f;
        return;
    }

    m_binding.sampleTexture(stx, out);
}


} // namespace zpr

// end of zprReadUVTexture.cpp

//
// Copyright 2020 DreamWorks Animation
//
