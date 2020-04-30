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

/// @file Fuser/SceneXform.h
///
/// @author Jonathan Egstad

#ifndef Fuser_SceneXform_h
#define Fuser_SceneXform_h

#include "SceneOpExtender.h"
#include "AxisKnob.h"
#include "Lookat.h"
#include "Mat4.h"

#include <DDImage/AxisOp.h>


namespace Fsr {

// Use these in a Op::node_help() method like so:
// const char* node_help() const { return __DATE__ " " __TIME__ " "
//    "My cool Op's description.\n"
//    "\n"
//    SCENE_LOADER_HELP"\n"
//    "\n"
//    SCENE_XFORM_HELP;
// }
//
#define SCENE_XFORM_HELP \
"While it can only read in one of those nodes at a time, by enabling \
'separate parent xform' it can optionally encapsulate the node's parenting hierarchy \
into the knobs 'parent translate', 'parent rotate', and 'parent scale'. This keeps \
local transform controls seperate and more easily manipulated in a local context."



/*! \class Fsr::SceneXform

    \brief Interface class adding double-precision functionality to DD::Image::AxisOp
    and DD::Image::GeoOp classes with Axis knobs.

    This is an interface to extend the functionality of existing DD::Image::AxisOp
    and DD::Image::GeoOp subclasses without requiring a ton of rewriting.

    Out of neccessity we need to replicate / reverse-engineer several of the existing
    built in DD::Image::AxisOp functions like lookat and parenting to allow those
    to support double-precision matrices.

    Extending the existing DDImage classes with this interface is also clumsy due
    to multiple-inheritance rules that makes it difficult to dynamic/static cast
    a pointer to a SceneXform type to access its functions. You first need to cast
    to the base type (FuserAxisOp/FuserCameraOp/FuserLightOp) and *then* cast to
    SceneXform, and this requires testing a pointer against each of the types in
    succession. Another reason why this is not the ideal method of adding
    double-precision support.

    Once Foundry (if ever...) updates Nuke to support double-precision matrices
    this extension class can probably be deprecated.

*/
class FSR_EXPORT SceneXform : public Fsr::SceneOpExtender
{
  protected:
    Fsr::LookatVals  m_lookat;          //!< Replicates the DD::ImageLookAt functionality

    Fsr::Mat4d       m_input_matrix;    //!< Input transform from parent input source - identity if no input
    Fsr::Mat4d       m_parent_matrix;   //!< Parent transform from local parent knobs
    Fsr::Mat4d       m_xform_matrix;    //!< AxisKnob fills this in
    Fsr::Mat4d       m_local_matrix;    //!< Also contains lookat transform!
    Fsr::Mat4d       m_world_matrix;    //!< parent * local

    DD::Image::Knob* kParentTranslate;  //!< If not NULL apply parent_translate
    DD::Image::Knob* kParentRotate;     //!< If not NULL apply parent_rotate
    DD::Image::Knob* kParentScale;      //!< If not NULL apply parent_scale

    DD::Image::Knob*      kAxisKnob;    //!< The stock single-precision AxisKnob
    Fsr::AxisKnobWrapper* kFsrAxisKnob; //!< Double-precision AxisKnob wrapper


  protected:
    /*! Call this from owner (FuserAxisOp, FuserCameraOp, FuserLightOp)::knobs() to
        replace the AxisKnob knobs.
        Adds the local transform knobs matching the typical AxisKnob ones.
        This is valid as of Nuke 12.
    */
    void _addOpTransformKnobs(DD::Image::Knob_Callback f,
                              DD::Image::Matrix4*      localtransform);

    /*! Call this from owner (FuserAxisOp, FuserCameraOp, FuserLightOp)::knobs() to
        replace the AxisOp baseclass' knobs() implementation.
        Adds the local transform knobs matching the typical AxisOp base class.
        If the AxisOp class gets additional knob vars added in newer Nuke versions
        this will need to be updated! This is valid as of Nuke 12.
    */
    void _addAxisOpTransformKnobs(DD::Image::Knob_Callback         f,
                                  DD::Image::Matrix4*              localtransform,
                                  DD::Image::Axis_KnobI**          axis_knob,
                                  DD::Image::WorldMatrixProvider** worldMatrixProvider);

    /*! Call this from owner (AxisOp-subclass)::_validate() to replace the AxisOp
        baseclass' _validate() implementation.
        Builds the double-precision matrices replacing the stock single-precision ones.
        If the AxisOp class gets additional transform vars added in newer Nuke versions
        this will need to be updated! This is valid as of Nuke 12.
    */
    void _validateAxisOpMatrices(bool for_real,
                                 DD::Image::Matrix4* localtransform,
                                 DD::Image::Matrix4* local,
                                 DD::Image::Matrix4* matrix,
                                 bool*               inversion_updated);


  public:
    //!
    SceneXform();

    //! Must have a virtual destructor!
    virtual ~SceneXform() {}


    //! Returns true if Op is a Fuser SceneXform.
    static bool        isSceneXform(DD::Image::Op* op);

    //! Returns op cast to Fuser SceneXform if possible, otherwise NULL.
    static SceneXform* asSceneXform(DD::Image::Op* op);


    //--------------------------------------------------------------------
    // Must implement these pure virtuals:
    //--------------------------------------------------------------------


    //! Return the parenting input number, or -1 if the parenting source is local. Must implement.
    virtual int                parentingInput() const=0;

    //! Return the lookat input number, or -1 if the lookat source is local. Must implement.
    virtual int                lookatInput() const=0;


    //---------------------------------------------------------------------

    //!
    virtual int xformInputs() const;

    //!
    virtual bool testInput(int            input,
                           DD::Image::Op* op) const;

    //! Return the Op to connect to this input if the arrow is disconnected in Nuke.
    virtual DD::Image::Op* defaultInput(int input) const;

    //!
    virtual const char* inputLabel(int   input,
                                   char* buffer) const;


    //! Call this from owner Op::knobs(). Adds the parent transform knobs.
    virtual void addParentingKnobs(DD::Image::Knob_Callback f,
                                   bool                     group_open=true);

    //! Call this from owner Op::knobs(). Adds the lookat knobs.
    virtual void addLookatKnobs(DD::Image::Knob_Callback);

    //! Call this from owner Op::knob_changed(). Updates loader gui and does node data reloads.
    /*virtual*/ int knobChanged(DD::Image::Knob* k,
                                int              call_again=0);

    //! Call this from owner Op::build_handles(). Replaces the AxisOp::build_handles() implementation.
    /*virtual*/ void buildHandles(DD::Image::ViewerContext* ctx);


    //! Get the Knob pointers for parent transform controls. If a knob is NULL is doesn't get applied.
    DD::Image::Knob* parentTranslateKnob() const { return kParentTranslate; }
    DD::Image::Knob* parentRotateKnob()    const { return kParentRotate;    }
    DD::Image::Knob* parentScaleKnob()     const { return kParentScale;     }

    //! Get the Knob pointers for local transform controls. Should never be NULL!
    DD::Image::Knob*      axisKnob()    const { return kAxisKnob;    }
    Fsr::AxisKnobWrapper* fsrAxisKnob() const { return kFsrAxisKnob; }


    //! Enable/disable knobs filled in by the node read.
    virtual void enableParentTransformKnobs(bool parent_xform_enabled);
    virtual void enableLocalTransformKnobs(bool read_enabled);
    virtual void enableSceneXformExtraKnobs(bool read_enabled);


    //---------------------------------------------------------------------


    // Return the matrices constructed at the last validated OutputContext()
    const Fsr::Mat4d& getInputParentTransform()      const { return m_input_matrix;  }
    const Fsr::Mat4d& getParentConstraintTransform() const { return m_parent_matrix; }
    const Fsr::Mat4d& getXformTransform()            const { return m_xform_matrix;  }
    const Fsr::Mat4d& getLocalTransform()            const { return m_local_matrix;  }
    const Fsr::Mat4d& getWorldTransform()            const { return m_world_matrix;  }


    //! Builds the input transform matrix. Will be identity if no input.
    virtual Fsr::Mat4d getInputParentTransformAt(const DD::Image::OutputContext& context) const;

    //! Builds the parent contraint matrix from the parent constraint knobs.
    virtual Fsr::Mat4d getParentConstraintTransformAt(const DD::Image::OutputContext& context) const;

    //! Builds the local 'xform' matrix from the local transform knobs. Does not include lookat!
    virtual Fsr::Mat4d getLocalTransformAt(const DD::Image::OutputContext& context) const;

    //! Build the local xform with lookat applied. Requires world transform up to local matrix to find origin.
    virtual Fsr::Mat4d getLocalTransformWithLookatAt(const Fsr::Mat4d&               parent_matrix,
                                                     const DD::Image::OutputContext& context,
                                                     Fsr::Mat4d*                     xform_matrix=NULL) const;

    //! Builds the entire transform matrix. Includes parent, local and lookat.
    virtual Fsr::Mat4d getWorldTransformAt(const DD::Image::OutputContext& context) const;


};


} // namespace Fsr


#endif

// end of FuserSceneXform.h


//
// Copyright 2019 DreamWorks Animation
//
