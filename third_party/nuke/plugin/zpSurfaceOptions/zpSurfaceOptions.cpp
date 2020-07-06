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

/// @file zpSurfaceOptions.cpp
///
/// @author Jonathan Egstad


#include <zprender/SurfaceMaterialOp.h>

#include "DDImage/VertexContext.h"
#include "DDImage/ViewerContext.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/gl.h"


using namespace DD::Image;

namespace zpr {


class zpSurfaceOptions : public SurfaceMaterialOp
{
  public:


  public:
    static const Description description;
    const char* Class()     const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "Change shader options for the shader context going UP the shader tree.  "
        "Any changes are reversed on the way DOWN the tree.";
    }


    zpSurfaceOptions(::Node* node) :
        SurfaceMaterialOp(node)
    {
        //
    }


    /*virtual*/
    RayShader* _createOutputSurfaceShader(const RenderContext&     rtx,
                                          std::vector<RayShader*>& shaders)
    {
        return NULL;
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
    }

};


static Op* build(Node* node) { return new zpSurfaceOptions(node); }
const Op::Description zpSurfaceOptions::description("zpSurfaceOptions", 0, build);

#ifdef DWA_INTERNAL_BUILD
// Map old plugin name to new:
static const Op::Description old_description("SurfaceOptions", build);
#endif

} // namespace zpr

// end of zpSurfaceOptions.cpp

//
// Copyright 2020 DreamWorks Animation
//
