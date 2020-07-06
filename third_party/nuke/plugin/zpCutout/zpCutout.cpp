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


#include "zprCutout.h"

#include <zprender/SurfaceMaterialOp.h>

#include "DDImage/VertexContext.h"
#include "DDImage/ViewerContext.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/gl.h"

//#define TRY_CUTOUT_MAP 1


using namespace DD::Image;

namespace zpr {


/*!
*/
class zpCutout : public SurfaceMaterialOp
{
  protected:
    zprCutout::InputParams k_inputs;


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "This shader cuts out the object in all channels.";
    }

    //!
    zpCutout(::Node* node) : SurfaceMaterialOp(node) {}


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        RayShader* output = new zprCutout(k_inputs);
        shaders.push_back(output);
        return output;
    }


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBinding(uint32_t input)
    {
#ifdef TRY_CUTOUT_MAP
        if (input == 1) return &k_inputs.k_map;
#endif
        return NULL;
    }


#ifdef TRY_CUTOUT_MAP
    //! Return the input number to use for the OpenGL texture display, usually the diffuse.
    /*virtual*/
    int32_t getGLTextureInput() const { return 1; }
#endif


    //----------------------------------------------------------------------------------


    /*virtual*/
    void _validate(bool for_real)
    {
        // Call base class first to get InputBindings assigned:
        SurfaceMaterialOp::_validate(for_real);
        info_.turn_on(k_inputs.k_cutout_channel);
    }


    /*virtual*/
    void knobs(Knob_Callback f)
    {
        //---------------------------------------------------------------------------------
        // This adds the 'zpSurfaceMaterialOp' knob that's used to identify a SurfaceMaterialOp
        // to other plugins (because dynamic_cast-ing fails).  Atm if this doesn't
        // exist then the _evaluate*() methods will not be called since the node
        // will not be recognized as a RayShader type:
        addSurfaceMaterialOpIdKnob(f);
        //---------------------------------------------------------------------------------
        // The top line of ray controls:
        addRayControlKnobs(f);

        InputOp_knob(f, &k_inputs.k_bindings[zprCutout::BG0], 0/*input_num*/);

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Channel_knob(f, &k_inputs.k_cutout_channel, 1/*channels*/, "cutout_channel", "cutout channel");
            Tooltip(f, "Use this channel to write cutout value to.  This will need to match the "
                       "renderer's setting so that front-to-back rendering order is handled "
                       "properly.");
#ifdef TRY_CUTOUT_MAP
        Newline(f);
        ColorMap_knob(f, &k_inputs.k_cutout_map, 1/*input*/, 1/*num_chans*/, "opacity", "opacity map");
#endif
    }


    /*virtual*/
    void fragment_shader(const VertexContext& vtx,
                         DD::Image::Pixel&    out)
    {
        // Base class call will pass it on up to input0:
        SurfaceMaterialOp::fragment_shader(vtx, out);

        // Clear the output channels *EXCEPT* alpha:
        const float a = out[Chan_Alpha];
        out.erase();
        out[Chan_Alpha] = a;

        // Indicate that this surface is completely cutout:
        out[k_inputs.k_cutout_channel] = 1.0f;
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
