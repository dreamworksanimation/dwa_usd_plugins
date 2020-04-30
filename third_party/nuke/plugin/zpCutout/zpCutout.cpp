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

/// @file zpCutout.cpp
///
/// @author Jonathan Egstad


#include <zprender/SurfaceShaderOp.h>
#include <zprender/ColorMapKnob.h>

#include "DDImage/VertexContext.h"
#include "DDImage/ViewerContext.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/gl.h"

//#define TRY_CUTOUT_MAP 1


using namespace DD::Image;

namespace zpr {


class zpCutout : public SurfaceShaderOp
{
  public:
    Channel           k_cutout_channel;     //!< Channel to use for cutout logic
#ifdef TRY_CUTOUT_MAP
    ColorMapKnob      k_cutout_map;         //!< Texture map for cutout opacity
#endif


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "This shader cuts out the object in all channels.";
    }


    //!
    zpCutout(::Node* node) : SurfaceShaderOp(node)
#ifdef TRY_CUTOUT_MAP
        , k_cutout_map( this, 1/*input*/, 1/*num_channels*/, Chan_Red/*first_chan*/)
#endif
    {
        k_cutout_channel = Chan_Mask;
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //---------------------------------------------------------------------------------
        // This adds the 'zpSurfaceShaderOp' knob that's used to identify a SurfaceShaderOp
        // to other plugins (because dynamic_cast-ing fails).  Atm if this doesn't
        // exist then the _evaluate*() methods will not be called since the node
        // will not be recognized as a RayShader type:
        addSurfaceShaderOpIdKnob(f);
        //---------------------------------------------------------------------------------
        // The top line of ray controls:
        RayShader::addRayControlKnobs(f);

        Divider(f);
        Channel_knob(f, &k_cutout_channel, 1/*channels*/, "cutout_channel", "cutout channel");
        Tooltip(f, "Use this channel to write cutout value to.  This will need to match the "
                   "renderer's setting so that front-to-back rendering order is handled "
                   "properly.");
#ifdef TRY_CUTOUT_MAP
        Newline(f);
        Texture_knob(f, k_cutout_map, "opacity", "opacity map");
#endif
    }


#ifdef TRY_CUTOUT_MAP
    /*virtual*/
    int knob_changed(Knob* k)
    {
        int ret = 0;
        if (k_cutout_map.knobChanged(k))
            ++ret;
        return (ret > 0);
    }
#endif


    /*virtual*/
    void _validate(bool for_real)
    {
        SurfaceShaderOp::_validate(for_real);
#ifdef TRY_CUTOUT_MAP
        // Validate map knobs:
        k_cutout_map.validateColorMap(for_real);
#endif

        info_.turn_on(k_cutout_channel);
    }


    /*virtual*/
    void _request(int x, int y, int r, int t,
                  ChannelMask channels,
                  int         count)
    {
        // Request map knobs:
        SurfaceShaderOp::_request(x, y, r, t, channels, count);
#ifdef TRY_CUTOUT_MAP
        k_cutout_map.requestColorMap(count);
#endif
    }


    /*virtual*/
    void fragment_shader(const VertexContext& vtx,
                         DD::Image::Pixel&    out)
    {
        // Base class call will pass it on up to input0:
        SurfaceShaderOp::fragment_shader(vtx, out);

        // Clear the output channels *EXCEPT* alpha:
        const float a = out[Chan_Alpha];
        out.erase();
        out[Chan_Alpha] = a;

        // Indicate that this surface is completely cutout:
        out[k_cutout_channel] = 1.0f;
    }


    //! The ray-tracing shader call.
    /*virtual*/
    void _evaluateShading(RayShaderContext& stx,
                          Fsr::Pixel&       out)
    {
        // Base class call will pass it on up to input0:
        SurfaceShaderOp::_evaluateShading(stx, out);

        // Clear the output channels *EXCEPT* alpha:
        const float a = out[Chan_Alpha];
        out.erase();
        out[Chan_Alpha] = a;

#ifdef TRY_CUTOUT_MAP
        // Modulate final color by cutout map:
        if (k_cutout_map.enabled())
        {
            Vector3 op = k_diffuse_map.sample(stx, 0/*alpha_ptr*/);
            if (op.x < 0.0f) {
            } else {
            }

        } else {
            // Indicate that this surface is completely cutout:
            if (k_cutout_channel != stx.cutout_channel)
                out[k_cutout_channel] = 1.0f;
            else
                out[stx.cutout_channel] = 1.0f;
        }
#else
        // Indicate that this surface is completely cutout:
        if (k_cutout_channel != stx.cutout_channel)
            out[k_cutout_channel] = 1.0f;
        else
            out[stx.cutout_channel] = 1.0f;
#endif
    }


    /*virtual*/
    bool shade_GL(ViewerContext* ctx,
                  GeoInfo&       geo)
    {
         glPushAttrib(GL_LIGHTING_BIT);

         glDisable(GL_LIGHTING);

         glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

         return true;
    }


    /*virtual*/
    void unset_texturemap(ViewerContext* ctx)
    {
        SurfaceShaderOp::unset_texturemap(ctx);
        glPopAttrib();
    }

};


static Op* build(Node* node) { return new zpCutout(node); }
const Op::Description zpCutout::description("zpCutout", 0, build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("Cutout", build);
#endif

} // namespace zpr

// end of zpCutout.cpp

//
// Copyright 2020 DreamWorks Animation
//
