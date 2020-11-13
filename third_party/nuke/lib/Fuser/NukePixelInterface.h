//
// Copyright 2019 DreamWorks Animation
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

/// @file Fuser/NukePixelInterface.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NukePixelInterface_h
#define Fuser_NukePixelInterface_h

#include "AttributeTypes.h"

#include <DDImage/Pixel.h>
#include <DDImage/Channel3D.h>


//-------------------------------------------------------------


namespace DD { namespace Image {

// Use very unlikely channels (for a renderer) to store cutout alpha & Z in:
static const Channel Chan_Cutout_Alpha = Chan_RotoPaint_Mask;
static const Channel Chan_Cutout_Depth = Chan_Mask_PlanarTrack;

}}


//-------------------------------------------------------------


namespace Fsr {

/*! Helper class for doing quick iterative lookups of channels within a
    ChannelSet, to speed up the foreach() macro.

    Kinda-sorta replacement for DD::Image ChannelMap which has limited
    access (can't get at the list of packed channels directly) and can't
    subclass from it since the vars are private.

    The channels are packed so that the first channel in the list is at
    m_idx[0] and the last channel is at m_idx[m_num_chans-1], and are
    guaranteed to be in the same order as a ChannelSet has them defined.

    The index list is a fixed-size array to save on new/delete cost of
    a std::vector and keep this POD.
*/
class FSR_EXPORT ChannelList
{
  protected:
    DD::Image::Channel m_idx[DD::Image::Chan_Last + 1]; //!< Array of Channel indices
    uint32_t           m_num_chans;                     //!< Number of assigned indices


  public:
    //! Default ctor is an empty set
    ChannelList() : m_num_chans(0) {}

    //! Copy from a ChannelSet
    explicit ChannelList(const DD::Image::ChannelSet& channels) { this->set(channels); }

    //! Number of channels in list
    uint32_t size()    const { return m_num_chans; }
    bool     isEmpty() const { return (m_num_chans == 0); }

    //! Set from a ChannelSet.
    ChannelList& operator = (const DD::Image::ChannelSet& channels) { this->set(channels); return *this; }

    //!
    void clear()                                    { m_num_chans = 0; }
    void set(const DD::Image::ChannelSet& channels) { this->clear(); this->add(channels); }
    void set(DD::Image::Channel chan)               { this->clear(); this->add(chan); }
    void add(const DD::Image::ChannelSet& channels);
    void add(DD::Image::Channel chan)               { m_idx[m_num_chans++] = chan; }

    //! Read/write access to channel indices
    DD::Image::Channel  operator [](uint32_t i) const { return m_idx[i]; }
    DD::Image::Channel& operator [](uint32_t i)       { return m_idx[i]; }

    //! Access to entire Channel array
    DD::Image::Channel* array() { return m_idx; }
};


//-------------------------------------------------------------


/*! Extension wrapper for the DD::Image::Pixel class which adds convenient access
    methods for standard GeoInfo attributes matching the DD::Image::VArray interface,
    but cast to Fuser Vec types.

    We wrap Pixel rather than VArray since it has 1024(currently) float channels
    and is intended only for passing data between methods, not storage. VArray
    started life as Pixel but since it was also being used for per-vertex attribute
    storage the large size of Pixel was just too big and was trimmed down for VArray.

    Warning, 'all' channel mode is unsupported!

    A ray tracer doesn't need a structure like VArray for vertex attribute storage since
    it's not storing & interpolating temporary scanline Spans.
*/
class FSR_EXPORT Pixel : public DD::Image::Pixel
{
  protected:
    ChannelList m_chan_indices; //!< Array of Channel indices


  public:
    //!
    Pixel();

    //! Copy from a ChannelSet
    explicit Pixel(const DD::Image::ChannelSet& channels);


    //----------------------------------------------------------


    //! Assign a ChannelSet
    void setChannels(const DD::Image::ChannelSet& channels);

    //! Assign some ChannelSet presets.
    void setToRGBChannels()   { setChannels(DD::Image::Mask_RGB ); }
    void setToRGBAChannels()  { setChannels(DD::Image::Mask_RGBA); }
    void setToRGBAZChannels() { setChannels(DD::Image::Mask_RGBA | DD::Image::Mask_Z); }

    //! Read access to channel indices
    DD::Image::Channel getIdx(uint32_t i) const { return m_chan_indices[i]; }

    //! Number of channels in Pixel. If 'all' channels this will return DD::Image::Chan_Last+1.
    uint32_t getNumChans()  const { return m_chan_indices.size(); }
    bool     isEmpty()      const { return m_chan_indices.isEmpty(); }


    //----------------------------------------------------------

    //! Sets assigned channels to zero.
    void clear();

    //! Does an explicit memset on all channels. Like erase().
    void clearAllChannels() { memset(chan, 0, sizeof(float)*size_t(DD::Image::Chan_Last+1)); }

    //! Convenience color functions
    void setRGBAToBlack() { rgb().set(0.0f); alpha() = 1.0f; }
    void setRGBAToWhite() { rgba().set(1.0f); }

    void setRGBToBlack()  { rgb().set(0.0f); }
    void setRGBToWhite()  { rgb().set(1.0f); }

    void setOpacityToFull() { opacity() = 1.0f; }
    void setOpacityToNone() { opacity() = 0.0f; }


    //----------------------------------------------------------
    // ChannelSet ops, accelerated using m_idx packed list.

    Pixel& operator *= (const Pixel& b);
    Pixel& operator *= (float v);
    Pixel& operator += (const Pixel& b);
    Pixel& operator += (float v);


    // Read/write vector attribute access convenience methods.
    // Unfortunately need to duplicate each accessor due to C++ complaining
    // when the non-const ones are using in a const method:
    Fsr::Vec4f&       P()       { return reinterpret_cast<      Fsr::Vec4f&>(chan[DD::Image::Chan_P_]); }
    const Fsr::Vec4f& P() const { return reinterpret_cast<const Fsr::Vec4f&>(chan[DD::Image::Chan_P_]); }
    float&            x()       { return P().x; }
    const float&      x() const { return P().x; }
    float&            y()       { return P().y; }
    const float&      y() const { return P().y; }
    float&            z()       { return P().z; }
    const float&      z() const { return P().z; }
    float&            w()       { return P().w; }
    const float&      w() const { return P().w; }

    float&            Z()       { return chan[DD::Image::Chan_Z        ]; }
    const float&      Z() const { return chan[DD::Image::Chan_Z        ]; }
    float&           Zf()       { return chan[DD::Image::Chan_DeepFront]; }
    const float&     Zf() const { return chan[DD::Image::Chan_DeepFront]; }
    float&           Zb()       { return chan[DD::Image::Chan_DeepBack ]; }
    const float&     Zb() const { return chan[DD::Image::Chan_DeepBack ]; }

    Fsr::Vec3f&           PL()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_PL_ ]); }
    const Fsr::Vec3f&     PL() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_PL_ ]); }
    Fsr::Vec3f&           PW()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_PW_ ]); }
    const Fsr::Vec3f&     PW() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_PW_ ]); }
    Fsr::Vec3f&           MB()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_MB_ ]); }
    const Fsr::Vec3f&     MB() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_MB_ ]); }
    Fsr::Vec4f&           UV()       { return reinterpret_cast<      Fsr::Vec4f&>(chan[DD::Image::Chan_UV_ ]); }
    const Fsr::Vec4f&     UV() const { return reinterpret_cast<const Fsr::Vec4f&>(chan[DD::Image::Chan_UV_ ]); }
    Fsr::Vec3f&            N()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_N_  ]); }
    const Fsr::Vec3f&      N() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_N_  ]); }
    Fsr::Vec3f&          VEL()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_VEL_]); }
    const Fsr::Vec3f&    VEL() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_VEL_]); }

    Fsr::Vec4f&           Cf()       { return reinterpret_cast<      Fsr::Vec4f&>(chan[DD::Image::Chan_Red]); }
    const Fsr::Vec4f&     Cf() const { return reinterpret_cast<const Fsr::Vec4f&>(chan[DD::Image::Chan_Red]); }
    Fsr::Vec4f&         rgba()       { return reinterpret_cast<      Fsr::Vec4f&>(chan[DD::Image::Chan_Red]); }
    const Fsr::Vec4f&   rgba() const { return reinterpret_cast<const Fsr::Vec4f&>(chan[DD::Image::Chan_Red]); }
    Fsr::Vec3f&          rgb()       { return reinterpret_cast<      Fsr::Vec3f&>(chan[DD::Image::Chan_Red]); }
    const Fsr::Vec3f&    rgb() const { return reinterpret_cast<const Fsr::Vec3f&>(chan[DD::Image::Chan_Red]); }
    float&                 r()       { return chan[DD::Image::Chan_Red  ]; }
    const float&           r() const { return chan[DD::Image::Chan_Red  ]; }
    float&                 g()       { return chan[DD::Image::Chan_Green]; }
    const float&           g() const { return chan[DD::Image::Chan_Green]; }
    float&                 b()       { return chan[DD::Image::Chan_Blue ]; }
    const float&           b() const { return chan[DD::Image::Chan_Blue ]; }
    float&                 a()       { return chan[DD::Image::Chan_Alpha]; }
    const float&           a() const { return chan[DD::Image::Chan_Alpha]; }
    float&               red()       { return chan[DD::Image::Chan_Red  ]; }
    const float&         red() const { return chan[DD::Image::Chan_Red  ]; }
    float&             green()       { return chan[DD::Image::Chan_Green]; }
    const float&       green() const { return chan[DD::Image::Chan_Green]; }
    float&              blue()       { return chan[DD::Image::Chan_Blue ]; }
    const float&        blue() const { return chan[DD::Image::Chan_Blue ]; }
    float&             alpha()       { return chan[DD::Image::Chan_Alpha]; }
    const float&       alpha() const { return chan[DD::Image::Chan_Alpha]; }

    float&       cutoutAlpha()       { return chan[DD::Image::Chan_Cutout_Alpha]; }
    const float& cutoutAlpha() const { return chan[DD::Image::Chan_Cutout_Alpha]; }
    float&       cutoutDepth()       { return chan[DD::Image::Chan_Cutout_Depth]; }
    const float& cutoutDepth() const { return chan[DD::Image::Chan_Cutout_Depth]; }

    Fsr::Vec3f&        color()       { return   rgb(); }
    const Fsr::Vec3f&  color() const { return   rgb(); }
    float&           opacity()       { return alpha(); }
    const float&     opacity() const { return alpha(); }

    Fsr::Vec3f&       position()       { return  PW(); }
    const Fsr::Vec3f& position() const { return  PW(); }
    Fsr::Vec3f&       velocity()       { return VEL(); }
    const Fsr::Vec3f& velocity() const { return VEL(); }
    Fsr::Vec3f&         normal()       { return   N(); }
    const Fsr::Vec3f&   normal() const { return   N(); }

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


inline
Pixel::Pixel() :
    DD::Image::Pixel(DD::Image::ChannelSetInit(DD::Image::Mask_None))
{
    //
}

inline
Pixel::Pixel(const DD::Image::ChannelSet& _channels) :
    DD::Image::Pixel(_channels)
{
    setChannels(_channels);
}

inline void
Pixel::setChannels(const DD::Image::ChannelSet& _channels)
{
    channels = _channels;
    if (_channels.all())
    {
        // Sorta support 'all' mode...
        for (int32_t z=0; z < (DD::Image::Chan_Last+1); ++z)
            m_chan_indices.add((DD::Image::Channel)z);
    }
    else
    {
        foreach(z, _channels)
            m_chan_indices.add(z);
    }
}

inline void
Pixel::clear()
{
    const uint32_t nChans = this->getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
        chan[this->getIdx(i)] = 0.0f;
}

inline Pixel&
Pixel::operator *= (const Pixel& b)
{
    const uint32_t nChans = b.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = b.getIdx(i);
        chan[z] *= b.chan[z];
    }
    return *this;
}

inline Pixel&
Pixel::operator *= (float v)
{
    const uint32_t nChans = this->getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
        chan[this->getIdx(i)] *= v;
    return *this;
}

inline Pixel&
Pixel::operator += (const Pixel& b)
{
    const uint32_t nChans = b.getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
    {
        const DD::Image::Channel z = b.getIdx(i);
        chan[z] += b.chan[z];
    }
    return *this;
}

inline Pixel&
Pixel::operator += (float v)
{
    const uint32_t nChans = this->getNumChans();
    for (uint32_t i=0; i < nChans; ++i)
        chan[this->getIdx(i)] += v;
    return *this;
}

//---------------------------------

inline void
ChannelList::add(const DD::Image::ChannelSet& channels)
{
    foreach(z, channels)
        m_idx[m_num_chans++] = z;
}

} // namespace Fsr

#endif

// end of Fuser/NukePixelInterface.h

//
// Copyright 2019 DreamWorks Animation
//
