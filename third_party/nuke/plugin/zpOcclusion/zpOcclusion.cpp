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

/// @file zpOcclusion.cpp
///
/// @author Jonathan Egstad


#include "zprOcclusion.h"

#include <zprender/SurfaceMaterialOp.h>
#include <zprender/RenderContext.h>

#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>


using namespace DD::Image;

namespace zpr {


class zpOcclusion : public SurfaceMaterialOp
{
  protected:
    zprOcclusion::InputParams k_inputs;
    zprOcclusion::LocalVars   m_locals;


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "Simple ambient and reflection occlusion shader.\n";
    }


    //!
    zpOcclusion(::Node* node) : SurfaceMaterialOp(node) {}


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        RayShader* output = new zprOcclusion(k_inputs);
        shaders.push_back(output);
        return output;
    }


    //! Return the InputBinding for an input.
    /*virtual*/
    InputBinding* getInputBinding(uint32_t input)
    {
        if (input == 0) return &k_inputs.k_bindings[zprOcclusion::BG0];
        return NULL;
    }


    /*virtual*/
    void _validate(bool for_real) 
    {
        //std::cout << "zpOcclusion::_validate(" << for_real << ")" << std::endl;
        // Call base class first to get InputBindings assigned:
        SurfaceMaterialOp::_validate(for_real);

        zprOcclusion::updateLocals(k_inputs, m_locals);

        // Enable AOV output channels:
        info_.turn_on(k_inputs.k_amb_ocl_output);
        info_.turn_on(k_inputs.k_refl_ocl_output);
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

        InputOp_knob(f, &k_inputs.k_bindings[zprOcclusion::BG0], 0/*input_num*/);

        //----------------------------------------------------------------------------------------------
        Divider(f);
        Bool_knob(f, &k_inputs.k_amb_ocl_enabled, "amb_ocl_enabled", "ambient occlusion enable");
            Tooltip(f, "Enable global ambient-occlusion. (fyi this is confusingly termed 'exposure' at Dreamworks...)\n"
                       "This calculates the diffuse angle off the surface for each camera ray and spawns "
                       "diffuse rays (using the diffuse samples count,) stochastically distributed over a "
                       "hemispherical cone that's between 0-180deg - set by the 'cone' value.  Each diffuse ray "
                       "is intersected against all objects to determine if the ray hits any objects.  If it does "
                       "then it's considered shadowed.  The weight of the shadowing is attenuated by distance "
                       "so that close objects produce more shadowing than distant objects.  The 'gi scale' control "
                       "scales the distances to bias the appearance.\n"
                       "The final shadowing value is multiplied against the surface color.  This is done *after* "
                       "the surface shader is called so this will incorrectly attenuate specular highlights.");
        Double_knob(f, &k_inputs.k_amb_ocl_mindist, "amb_ocl_mindist", "min/max");
            ClearFlags(f, Knob::SLIDER); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces closer than this value.");
        Double_knob(f, &k_inputs.k_amb_ocl_maxdist, "amb_ocl_maxdist", "");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces farther than this value.");
        Double_knob(f, &k_inputs.k_amb_ocl_cone_angle, "amb_ocl_cone_angle", "cone angle");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Diffuse distribution cone width angle - in degrees.  180 is a full hemisphere");
        Channel_knob(f, &k_inputs.k_amb_ocl_output, 1, "amb_ocl_output", "output");
            Tooltip(f, "Route this shader component to these output channels.");
        //
        Divider(f);
        Bool_knob(f, &k_inputs.k_refl_ocl_enabled, "refl_ocl_enabled", "reflection occlusion enable");
            Tooltip(f, "Enable global reflection-occlusion.\n"
                       "This calculates the reflection angle off the surface from each camera ray and spawns "
                       "glossy rays (using the glossy samples count,) stochastically distributed over a "
                       "hemispherical cone that's between 0-180deg - set by the 'cone' value.  Each glossy ray "
                       "is intersected against all objects to determine if the ray hits any objects.  If it does "
                       "then it's considered shadowed.\n"
                       "The final shadowing value is multiplied against the surface color.  This is done *after* "
                       "the surface shader is called so this will incorrectly attenuate specular highlights.");
        Double_knob(f, &k_inputs.k_refl_ocl_mindist, "refl_ocl_mindist", "min/max");
            ClearFlags(f, Knob::SLIDER); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces closer than this value.");
        Double_knob(f, &k_inputs.k_refl_ocl_maxdist, "refl_ocl_maxdist", "");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces farther than this value.");
        Double_knob(f, &k_inputs.k_refl_ocl_cone_angle, "refl_ocl_cone_angle", "cone angle");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Glossy distribution cone width angle - in degrees.  180 is a full hemisphere");
        Channel_knob(f, &k_inputs.k_refl_ocl_output, 1, "refl_ocl_output", "output");
            Tooltip(f, "Route this shader component to these output channels.");
        //
        Divider(f);
        Double_knob(f, &k_inputs.k_gi_scale, IRange(0.001, 10.0), "gi_scale", "gi scale");
            ClearFlags(f, Knob::STARTLINE);
            SetFlags(f, Knob::LOG_SLIDER | Knob::NO_MULTIVIEW);
            Tooltip(f, "Scales the calculated distances between objects to bias the distance weights.\n"
                       "To make an object 'feel' smaller decrease gi_scale below 1.0, and increase it "
                       "it above 1.0 to make objects 'feel' larger.");
    }


};


static Op* build(Node* node) {return new zpOcclusion(node);}
const Op::Description zpOcclusion::description("zpOcclusion", build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("SurfaceOcclusion", build);
#endif

} // namespace zpr

// end of zpOcclusion.cpp

//
// Copyright 2020 DreamWorks Animation
//
