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


#include <zprender/SurfaceShaderOp.h>
#include <zprender/RenderContext.h>
#include <zprender/Sampling.h>

#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>


using namespace DD::Image;

namespace zpr {


class zpOcclusion : public SurfaceShaderOp
{
    bool        k_amb_ocl_enabled;
    bool        k_refl_ocl_enabled;
    double      k_amb_ocl_mindist, k_amb_ocl_maxdist, k_amb_ocl_cone_angle;
    double      k_refl_ocl_mindist, k_refl_ocl_maxdist, k_refl_ocl_cone_angle;
    double      k_gi_scale;
    //
    Channel     k_amb_ocl_output;       //!< AOV channel to route ambient occlusion contribution to
    Channel     k_refl_ocl_output;      //!< AOV channel to route reflection occlusion contribution to

    float       m_amb_ocl_cone_angle, m_refl_ocl_cone_angle;
    double      m_amb_ocl_mindist, m_refl_ocl_mindist;
    double      m_amb_ocl_maxdist, m_refl_ocl_maxdist;


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "Simple ambient and reflection occlusion shader.\n";
    }


    //!
    zpOcclusion(::Node* node) :
        SurfaceShaderOp(node)
    {
        k_amb_ocl_enabled     = true;
        k_refl_ocl_enabled    = false;
        k_amb_ocl_mindist     =    0.0;
        k_amb_ocl_maxdist     = 1000.0;
        k_amb_ocl_cone_angle  =  180.0; // 180 degree cone
        k_refl_ocl_mindist    =    0.0;
        k_refl_ocl_maxdist    = 1000.0;
        k_refl_ocl_cone_angle =   20.0; // 20 degree cone
        k_gi_scale            =    1.0;
        //
        k_amb_ocl_output      = Chan_Black;
        k_refl_ocl_output     = Chan_Black;
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
        Bool_knob(f, &k_amb_ocl_enabled, "amb_ocl_enabled", "ambient occlusion enable");
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
        Double_knob(f, &k_amb_ocl_mindist, "amb_ocl_mindist", "min/max");
            ClearFlags(f, Knob::SLIDER); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces closer than this value.");
        Double_knob(f, &k_amb_ocl_maxdist, "amb_ocl_maxdist", "");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces farther than this value.");
        Double_knob(f, &k_amb_ocl_cone_angle, "amb_ocl_cone_angle", "cone angle");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Diffuse distribution cone width angle - in degrees.  180 is a full hemisphere");
        Channel_knob(f, &k_amb_ocl_output, 1, "amb_ocl_output", "output");
            Tooltip(f, "Route this shader component to these output channels.");
        //
        Divider(f);
        Bool_knob(f, &k_refl_ocl_enabled, "refl_ocl_enabled", "reflection occlusion enable");
            Tooltip(f, "Enable global reflection-occlusion.\n"
                       "This calculates the reflection angle off the surface from each camera ray and spawns "
                       "glossy rays (using the glossy samples count,) stochastically distributed over a "
                       "hemispherical cone that's between 0-180deg - set by the 'cone' value.  Each glossy ray "
                       "is intersected against all objects to determine if the ray hits any objects.  If it does "
                       "then it's considered shadowed.\n"
                       "The final shadowing value is multiplied against the surface color.  This is done *after* "
                       "the surface shader is called so this will incorrectly attenuate specular highlights.");
        Double_knob(f, &k_refl_ocl_mindist, "refl_ocl_mindist", "min/max");
            ClearFlags(f, Knob::SLIDER); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces closer than this value.");
        Double_knob(f, &k_refl_ocl_maxdist, "refl_ocl_maxdist", "");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Ignore surfaces farther than this value.");
        Double_knob(f, &k_refl_ocl_cone_angle, "refl_ocl_cone_angle", "cone angle");
            ClearFlags(f, Knob::SLIDER | Knob::STARTLINE); SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
            Tooltip(f, "Glossy distribution cone width angle - in degrees.  180 is a full hemisphere");
        Channel_knob(f, &k_refl_ocl_output, 1, "refl_ocl_output", "output");
            Tooltip(f, "Route this shader component to these output channels.");
        //
        Divider(f);
        Double_knob(f, &k_gi_scale, IRange(0.001, 10.0), "gi_scale", "gi scale");
            ClearFlags(f, Knob::STARTLINE);
            SetFlags(f, Knob::LOG_SLIDER | Knob::NO_MULTIVIEW);
            Tooltip(f, "Scales the calculated distances between objects to bias the distance weights.\n"
                       "To make an object 'feel' smaller decrease gi_scale below 1.0, and increase it "
                       "it above 1.0 to make objects 'feel' larger.");
    }


    /*virtual*/
    void _validate(bool for_real) 
    {
        //std::cout << "zpOcclusion::_validate(" << for_real << ")" << std::endl;
        SurfaceShaderOp::_validate(for_real);

        // Precalculate and clamp some shader params:
        m_amb_ocl_cone_angle = float(clamp(k_amb_ocl_cone_angle, 0.0, 180.0));
        m_amb_ocl_mindist = MAX(0.001, fabs(k_amb_ocl_mindist));
        m_amb_ocl_maxdist = MIN(fabs(k_amb_ocl_maxdist), std::numeric_limits<double>::infinity());
        //
        m_refl_ocl_cone_angle = float(clamp(k_refl_ocl_cone_angle, 0.0, 180.0));
        m_refl_ocl_mindist = MAX(0.001, fabs(k_refl_ocl_mindist));
        m_refl_ocl_maxdist = MIN(fabs(k_refl_ocl_maxdist), std::numeric_limits<double>::infinity());

        // Enable AOV output channels:
        info_.turn_on(k_amb_ocl_output);
        info_.turn_on(k_refl_ocl_output);
    }


    //----------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------


    /*! The ray-tracing shader call.
    */
    /*virtual*/
    void _evaluateShading(RayShaderContext& stx,
                          Fsr::Pixel&       out)
    {
        //std::cout << "zpOcclusion::_evaluateShading() [" << stx.x << " " << stx.y << "]" << std::endl;
        float amb_ocl_weight  = 1.0f;
        float refl_ocl_weight = 1.0f;
        if (k_amb_ocl_enabled)
        {
             amb_ocl_weight = getOcclusion(stx,
                                           Fsr::RayContext::DIFFUSE,
                                           m_amb_ocl_mindist,
                                           m_amb_ocl_maxdist,
                                           m_amb_ocl_cone_angle,
                                           float(k_gi_scale));
        }

        if (k_refl_ocl_enabled)
        {
             refl_ocl_weight = getOcclusion(stx,
                                            Fsr::RayContext::GLOSSY,
                                            m_refl_ocl_mindist,
                                            m_refl_ocl_maxdist,
                                            m_refl_ocl_cone_angle,
                                            float(k_gi_scale));
        }

        // Get the input shading result after (just in case stx gets messed with):
        SurfaceShaderOp::_evaluateShading(stx, out);

        // Apply occlusion weights:
        if (k_amb_ocl_enabled)
        {
            const float wt = (1.0f - amb_ocl_weight);
            out[Chan_Red  ] *= wt;
            out[Chan_Green] *= wt;
            out[Chan_Blue ] *= wt;

            // Copy AOVs only if they're not overwriting RGBA:
            if (k_amb_ocl_output > Chan_Alpha)
            {
                out.channels += k_amb_ocl_output;
                out[k_amb_ocl_output] = wt;
            }
        }
        if (k_refl_ocl_enabled)
        {
            const float wt = (1.0f - refl_ocl_weight);
            out[Chan_Red  ] *= wt;
            out[Chan_Green] *= wt;
            out[Chan_Blue ] *= wt;

            // Copy AOVs only if they're not overwriting RGBA:
            if (k_refl_ocl_output > Chan_Alpha)
            {
                out.channels += k_refl_ocl_output;
                out[k_refl_ocl_output] = wt;
            }
        }
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
