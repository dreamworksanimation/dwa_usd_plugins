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

/// @file Fuser/AxisKnob.h
///
/// @author Jonathan Egstad

#ifndef Fuser_AxisKnob_h
#define Fuser_AxisKnob_h

#include "Mat4.h"

#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>


namespace Fsr {


/*
    *************************************************************

    For Knob classes and macros for use in Op::knobs() routines
    see the AxisKnobVals and AxisKnobWrapper classes down below
    AxisVals.

    *************************************************************
*/


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


class LookatVals;
class AxisVals;

//! Typically used for animation curves
typedef std::vector<AxisVals> AxisValsList;


/*! Encapsulates all the parameters in an Axis_Knob in double-precision.

*/
class FSR_EXPORT AxisVals
{
  public:
    double              time;                   //!< Sample time

    // Parent xform:
    bool                parent_enable;          //!< Are parent transform values being used?
    Fsr::Vec3d          parent_translate;       //!<
    Fsr::Vec3d          parent_rotate;          //!< Rotation angles *in degrees*
    Fsr::Vec3d          parent_scale;           //!<

    // Local xform:
    Fsr::XformOrder     xform_order;            //!< (matches the DD::Image::Axis_KnobI enums)
    Fsr::RotationOrder  rot_order;              //!< (matches the DD::Image::Axis_KnobI enums)
    Fsr::Vec3d          translate;
    Fsr::Vec3d          rotate;                 //!< Rotation angles *in degrees*
    Fsr::Vec3d          scaling;
    double              uniform_scale;
    Fsr::Vec3d          skew;
    Fsr::Vec3d          pivot;
    //
    bool                use_matrix;             //!< Ignore separate transform parms above, use an explicit matrix
    Fsr::Mat4d          matrix;                 //!< If 'use_matrix' should be enabled on Axis_Knob.


  public:
    //! Default ctor leaves junk in values.
    AxisVals();

    //! Assigns standard default values (time arg is just to make ctor unique.)
    AxisVals(double time,
             bool   _parent_enable);

    //! Extracts values from Knob axis_knob on op.
    AxisVals(const DD::Image::Op*            op,
             const DD::Image::Knob*          axis_knob,
             const DD::Image::OutputContext& context,
             bool                            _parent_enable);

    //! Extracts values from an Axis_Knob named 'transform' on Op.
    AxisVals(const DD::Image::Op*            op,
             const DD::Image::OutputContext& context,
             bool                            _parent_enable);

    //----------------------------------------------------------------------

    //! Compares time value. Used by the sort routine.
    bool operator < (const AxisVals& b) const { return (time < b.time); }


    //! Print values out.
    void print(const char*   prefix,
               std::ostream& o) const;

    //----------------------------------------------------------------------

    //! Enable the parent knobs so that they are stored and sampled.
    void enableParentXformVals(bool enable=true) { parent_enable = enable; }

    //! Build a matrix from the current parent or local TRS values.
    Fsr::Mat4d getParentMatrix() const;
    Fsr::Mat4d getLocalMatrix()  const;

    //! Build a matrix from the current values and lookat params - requires a parent matrix.
    Fsr::Mat4d getMatrixWithLookat(const LookatVals& lookat,
                                   const Fsr::Mat4d& parent_matrix,
                                   const Fsr::Vec3d& lookatP) const;

    //! Get the total scale as a vector3.
    Fsr::Vec3d totalScaling() const { return scaling*uniform_scale; }

    //----------------------------------------------------------------------

    //! Assigns standard default values to all params.
    void setToDefault(double _time=0.0);

    //! Assigns standard default values to transform params (rotate, scale, etc.)
    void setLocalXformValsToDefault();
    void setParentXformValsToDefault();

    //! Return true if xform vals are at default settings.
    bool isLocalXformValsDefault() const;
    bool isParentXformValsDefault() const;

    //----------------------------------------------------------------------

    //! Extract transform knob values from an Axis_Knob at an OutputContext. Returns false if not possible.
    bool getValsAt(const DD::Image::Op*            op,
                   const DD::Image::Knob*          axis_knob,
                   const DD::Image::OutputContext& context);

    //! Extract transform knob values from an AxisOp at an OutputContext. Returns false if not possible.
    bool getValsAt(const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context);

    //!
    bool extractFromMatrix(const Fsr::Mat4d&  m,
                           bool               T_enable,
                           bool               R_enable,
                           bool               S_enable,
                           Fsr::RotationOrder decompose_rot_order,
                           bool               apply_to_parent=false);

    //----------------------------------------------------------------------

    //! Store a list of AxisVals into the Op knobs as animation.
    static bool store(const AxisValsList& axis_vals_list,
                      DD::Image::Op*      op);

    //!
    static void applyEulerFilter(Fsr::RotationOrder     target_rot_order,
                                 std::vector<AxisVals>& vals,
                                 bool                   sort=false);

    //----------------------------------------------------------------------

    //! Clears any animation from the knobs we will store() into.
    static void clearAnimation(DD::Image::Op*                  op,
                               const DD::Image::OutputContext& context);

    //! Store the AxisVals into Op knobs.
    bool store(DD::Image::Op*                  op,
               const DD::Image::OutputContext& context) const;

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Used in Knob::store() to separate the parent and local matrices rather
    than storing the concatenated result.
*/
struct AxisKnobVals
{
    AxisVals   vals;                //!< 

    // Derived matrices from the AxisVals:
    Fsr::Mat4d parent_matrix;       //!< Built from k_axis_vals parent TRS vals
    Fsr::Mat4d local_matrix;        //!< Built from k_axis_vals local TRS vals


    //! Sets parent_enable true and all values to their normal defaults.
    AxisKnobVals();
};



/*! Knob construction/store callback 'macro' similar to the ones
    defined in Knobs.h. It declares a DD::Image::CUSTOM_KNOB
    enumeration and a DD::Image::Custom data type.

    This knob stores the 3D transformation into double-precision
    Fsr::Mat4d matrices along with double-precision versions of the
    separate axis controls.

    It relies on the existence of a companion Axis_Knob being
    present on the same parent Op so that the 'translate',
    'rotate', 'scaling', etc knobs exist.

    Add this knob *after* the Axis_Knob so we're confident the
    Axis_Knob constructs/stores before this one (shouldn't
    really matter though.)
*/
DD::Image::Knob* AxisKnobWrapper_knob(DD::Image::Knob_Callback f,
                                      Fsr::AxisKnobVals*       axis_knob_vals,
                                      const char*              name);


/*! \class Fsr::AxisKnob

    \brief DD::Image::Knob companion 'wrapper' for Nuke's AxisKnob class.

    Not a true wrapper but more of a companion Knob so that we can
    augment the stock single-precision AxisKnob that comes with Nuke.

    This knob relies on an Axis_Knob already being present on the parent
    Op and it creating the child knobs 'translate', 'rotate', 'scaling', etc.

    All this knob does is implement a store() routine that builds
    double-precision Fuser Matds from those knobs in the same manner
    the stock Axis_Knob does. It doesn't save or load anything to a
    script or cause a hash change.

    A child AxisOp connecting to the parent Op can check for the existence
    of this knob to directly access its Mat4d, or more typically would
    check if the Op is a Fsr::SceneXform type and get the Mat4d from
    that interface so it can get the double-precision parent and world
    transforms as well.

*/
class FSR_EXPORT AxisKnobWrapper : public DD::Image::Knob
{
  protected:
    // Assigned in the first getValsAt() call:
    DD::Image::Knob* kParentTranslate;
    DD::Image::Knob* kParentRotate;
    DD::Image::Knob* kParentScale;
    //
    DD::Image::Knob* kXformOrder;
    DD::Image::Knob* kRotOrder;
    DD::Image::Knob* kTranslate;
    DD::Image::Knob* kRotate;
    DD::Image::Knob* kScale;
    DD::Image::Knob* kUniformScale;
    DD::Image::Knob* kSkew;
    DD::Image::Knob* kPivot;
    DD::Image::Knob* kUseMatrix;
    DD::Image::Knob* kMatrix;


  public:
    //! Ctor does not require a data pointer since it does not have a separate default.
    AxisKnobWrapper(DD::Image::Knob_Closure* cb,
                    const char*              name);

    //! Add the parent TRS knobs.
    static void addParentTRSKnobs(DD::Image::Knob_Callback f);


    //! Get a Fsr::Mat4d matrix built at the specified output context. Updates the hash if provided.
    Fsr::Mat4d getMatrixAt(const DD::Image::OutputContext& context,
                           DD::Image::Hash*                hash=NULL);

    //! Get AxisKnobVals & matrices filled in at the specified output context. Updates the hash if provided.
    void       getValsAt(const DD::Image::OutputContext& context,
                         Fsr::AxisKnobVals&              axis_knob_vals,
                         DD::Image::Hash*                hash=NULL);

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    // From DD::Image::Knob:

    //!
    /*virtual*/ const char* Class() const { return "FsrAxisKnob"; }

    //!
    /*virtual*/ DD::Image::Axis_KnobI* axisKnob() { return NULL; }

    //! Don't make an interface. TODO: this causes build problems...
    ///*virtual*/ WidgetPointer make_widget(const DD::Image::WidgetContext&) { return NULL; }

    //! Don't do anything since the wrapper knob should never be written to a script file.
    /*virtual*/ bool from_script(const char*) { return true; }

    //! Do nothing since we're not a 'real' knob.
    /*virtual*/ void reset_to_default() {}

    //! Don't affect the hash since we're not a 'real' knob.
    /*virtual*/ void append(DD::Image::Hash&,
                            const DD::Image::OutputContext*) {}

    //! Stores the transform into a double-precision Fuser Mat4d. This does affect the hash.
    /*virtual*/ void store(DD::Image::StoreType            type,
                           void*                           p,
                           DD::Image::Hash&                hash,
                           const DD::Image::OutputContext& context);

};


} // namespace Fsr



#endif

// end of FuserAxisKnob.h


//
// Copyright 2019 DreamWorks Animation
//
