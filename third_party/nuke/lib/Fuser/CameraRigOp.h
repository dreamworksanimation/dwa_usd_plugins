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

/// @file Fuser/CameraRigOp.h
///
/// @author Jonathan Egstad

#ifndef Fuser_CameraRigOp_h
#define Fuser_CameraRigOp_h


#include "CameraOp.h"

#include <DDImage/CameraOp.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>


namespace Fsr {


/*! DD::Image::CameraOp wrapper adding Fuser scene loading and double-precision
    matrix support.
*/
class FSR_EXPORT CameraRigOp : public FuserCameraOp
{
  protected:
    /*! Stores per-view derived values updated in _validate().
        Used primarily for OpenGL display.
    */
    struct CamParams
    {
        int         view;           //!< Sub camera view index
        const char* name;           //!< Name to use for OpenGL display
        uint32_t    gl_color;       //!< Color to use for OpenGL display
        Fsr::Mat4d  local_xform;    //!< Rig-local transform matrix, in world-space
        Fsr::Mat4d  aperture_xform; //!< Rig-local aperture transform matrix, in millimeters

        //! Default ctor leaves junk in vars.
        CamParams() : name("") {}
    };


  protected:
    bool   k_gl_show_all_rig_cameras;           //!< 

    DD::Image::Hash         m_rig_hash;         //!< Hash of knob values affecting CamParams
    std::vector<CamParams>  m_cam_params;       //!< Derived per-view camera params & local xforms


  protected:
    //! Append controls that affect the rig cameras to a hash at a specific OutputContext.
    virtual void _appendRigValuesAt(const DD::Image::OutputContext& context,
                                    DD::Image::Hash&                hash)=0;

    /*! Rebuild the CamParams list at a specific OutputContext.

        A subclass needs to add at least one CamParam to m_cam_params otherwise
        an assert will be thrown.
    */
    virtual void _rebuildCamParamsAt(const DD::Image::OutputContext& context)=0;


  public:
    //!
    CameraRigOp(::Node* node);

    //! Must have a virtual destructor!
    virtual ~CameraRigOp() {}


    //------------------------------------------------------------
    // DD::Image::CameraOp virtual methods.

    /*virtual*/ void append(DD::Image::Hash& hash);
    /*virtual*/ void _validate(bool for_real);
    /*virtual*/ void draw_handle(DD::Image::ViewerContext*);


    //------------------------------------------------------------


    //! Adds the OpenGL display option controls.
    /*virtual*/ void addDisplayOptionsKnobs(DD::Image::Knob_Callback);

    //! Adds addl front-panel knobs. Calls addRigKnobs() with the rigName().
    /*virtual*/ void addExtraFrontPanelKnobs(DD::Image::Knob_Callback);


    //------------------------------------------------------------

    //! Return the identification name of the rig. Must implement.
    virtual const char* rigName() const=0;

    //! Add rig-specific knobs. Must implement.
    virtual void addRigKnobs(DD::Image::Knob_Callback f,
                             const char*              rig_name="CameraRig")=0;

    //! Append controls that affect the rig cameras to a hash at the current OutputContext.
    void appendRigValues(DD::Image::Hash&);

    //! Rebuild the CamParams list at the current OutputContext.
    void rebuildCamParams();

};


} // namespace Fsr

#endif

// end of Fuser/CameraRigOp.h

//
// Copyright 2019 DreamWorks Animation
//
