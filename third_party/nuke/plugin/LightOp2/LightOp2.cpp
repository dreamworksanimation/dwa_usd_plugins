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

/// @file Light2.cpp
///
/// @author Jonathan Egstad
///


#include <zprender/LightMaterialOp.h>
#include <zprender/zprPointLight.h>


using namespace DD::Image;

/*! Fuser replacement for the stock Nuke Light2 plugin that adds
    scene file loading capabilities (usd/abc/fbx/etc.)
*/
class LightOp2 : public zpr::LightMaterialOp
{
  public:
    zpr::zprPointLight zprShader;       //!< Local shader allocation for knobs to write into


  public:
    static const Description description;
    /*virtual*/ const char* Class() const { return description.name; }

    LightOp2(Node *node) : zpr::LightMaterialOp(node) {}

    /*virtual*/ const char* displayName() const { return "Light"; }


    /*virtual*/
    void addLightKnobs(DD::Image::Knob_Callback f)
    {
        //LightMaterialOp::addLightKnobs(f); // don't want the near/far controls from LightOp

        Double_knob(f, &zprShader.inputs.k_near, IRange(0.001,  10.0), "near");
        Double_knob(f, &zprShader.inputs.k_far,  IRange(1.0, 10000.0), "far" );
    }


    /*virtual*/
    void _validate(bool for_real)
    {
        // Copy values from the LighOp to the InputParam:
        zprShader.inputs.k_color.set(DD::Image::LightOp::color_[DD::Image::Chan_Red  ],
                                     DD::Image::LightOp::color_[DD::Image::Chan_Green],
                                     DD::Image::LightOp::color_[DD::Image::Chan_Blue ]);
        zprShader.inputs.k_intensity = DD::Image::LightOp::intensity_;
        zprShader.inputs.k_illuminate_atmosphere = Fsr::FuserLightOp::k_illuminate_atmosphere;

        // Updates the legacy-mode output LightShader:
        LightMaterialOp::_validate(for_real);
    }


    //------------------------------------------------------------------
    // From LightMaterialOp
    //------------------------------------------------------------------


    /*! Create the shaders for one input, returning the output surface shader.
    */
    /*virtual*/
    zpr::LightShader* _createOutputLightShader(const zpr::RenderContext*     rtx,
                                               const Fsr::DoubleList&        motion_times,
                                               const Fsr::Mat4dList&         motion_xforms,
                                               std::vector<zpr::RayShader*>& shaders)

    {
        // Create a zprPointLight by default and set its color and intensity
        // values:
        zpr::LightShader* ltshader = new zpr::zprPointLight(zprShader.inputs,
                                                            motion_times,
                                                            motion_xforms);

        shaders.push_back(ltshader);
        return ltshader;
    }

};


static Op* buildLightOp2(Node *node) {return new LightOp2(node);}
const Op::Description LightOp2::description("Light2", buildLightOp2);

// end of LightOp2.cpp

//
// Copyright 2019 DreamWorks Animation
//
