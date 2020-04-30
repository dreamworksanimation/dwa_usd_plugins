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

/// @file AxisManipulator.h
///
/// @author Jonathan Egstad


#ifndef zprender_AxisManipulator_h
#define zprender_AxisManipulator_h

#include <zprender/RenderContext.h>

#include <DDImage/Knob.h>
#include <DDImage/Scene.h>
#include <DDImage/ViewerContext.h>

//#define DEBUG_MANIPULATOR 1

namespace zpr {


/*! Axis/Camera/Light placement interface for renderer nodes.
*/
class AxisManipulator
{
  protected:
    //! Placement interaction modes
    enum
    {
        PLACE_OFF=0,
        PLACE_ON_REFLECTION,
        PLACE_ON_NORMAL,
        PLACE_ON_SURFACE,
        ORIENT_LIGHT_TO_SURFACE
    };

    /*! Enables callbacks in Nuke's Viewer
    */
    class GlueKnob :
        public DD::Image::Knob
    {
        AxisManipulator* parent;

      public:
        const char* Class() const { return "AxisManipulatorGlue"; }

        GlueKnob(DD::Image::Knob_Closure* kc,
                 AxisManipulator*         p,
                 const char*              n) :
            DD::Image::Knob(kc, n) { parent = p; }

        bool build_handle(DD::Image::ViewerContext*);
        void draw_handle(DD::Image::ViewerContext*);
    };


    int    k_place_light;
    double k_place_distance;
    bool   k_place_maintain_size;
    DD::Image::Knob *kViewerLightNames;
    DD::Image::Knob *kPlaceLightOnSurface, *kPlaceLightOnReflection;
    DD::Image::Knob *kPlaceLightOnNormal,  *kOrientLightToSurface;

    std::string       m_current_light;
    DD::Image::Hash   m_light_list_hash;


  public:
    // Value used for all manipulators - only one can be active at a time!
    static int              global_placement_mode;
    static AxisManipulator* global_placement_manipulator;

  public:
    //!
    AxisManipulator();

    //!
    virtual DD::Image::Op* manipulatorOp()=0;

    //!
    virtual DD::Image::Scene* manipulatorScene()=0;

    //!
    void updateManipulatorMenu();

    //!
    void addManipulatorKnobs(DD::Image::Knob_Callback f,
                             bool                     in_viewer=true);

    //!
    int knobChanged(DD::Image::Knob* k);

    //!
    void setManipulatorMode(int              mode,
                            DD::Image::Knob* k_changed);

    //!
    bool handleManipulatorClick(DD::Image::ViewerContext* ctx);

    //!
    virtual bool intersectScene(DD::Image::ViewerContext* ctx,
                                Fsr::Vec3d&               camPW,
                                Fsr::Vec3d&               camV,
                                Fsr::Vec3d&               surfPW,
                                Fsr::Vec3d&               surfN) =0;

    //!
    void drawManipulatorIcons(DD::Image::ViewerContext* ctx);


  protected:
    virtual ~AxisManipulator() {}
};


} // namespace zpr

#endif

// end of AxisManipulator.h

//
// Copyright 2020 DreamWorks Animation
//
