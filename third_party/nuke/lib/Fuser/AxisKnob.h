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


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


class AxisKnobVals;
typedef std::vector<AxisKnobVals> AxisKnobValsList;


/*! Encapsulates all the parameters in an Axis_Knob in double-precision.

*/
class AxisKnobVals
{
  public:
    double              time;               //!< Sample time
    //
    Fsr::XformOrder     xform_order;        //!< (matches the DD::Image::Axis_KnobI enums)
    Fsr::RotationOrder  rot_order;          //!< (matches the DD::Image::Axis_KnobI enums)
    Fsr::Vec3d          translate;
    Fsr::Vec3d          rotate;             //!< Rotation angles *in degrees*
    Fsr::Vec3d          scaling;
    double              uniform_scale;
    Fsr::Vec3d          skew;
    Fsr::Vec3d          pivot;
    //
    bool                use_matrix;         //!< Ignore separate transform parms above, use an explicit matrix
    Fsr::Mat4d          matrix;             //!< If 'use_matrix' should be enabled on Axis_Knob.
    //
    bool                parent_enable;      //!< Are parent transform values being used?
    Fsr::Vec3d          parent_translate;   //!<
    Fsr::Vec3d          parent_rotate;      //!< Rotation angles *in degrees*
    Fsr::Vec3d          parent_scale;       //!<


  public:
    //! Default ctor leaves junk in values.
    AxisKnobVals() {}
    //! Assigns standard default values (time arg is just to make ctor unique.)
    AxisKnobVals(double time) { setToDefault(time); }
    //! Extracts values from Knob axis_knob on op.
    AxisKnobVals(const DD::Image::Op*            op,
                 const DD::Image::Knob*          axis_knob,
                 const DD::Image::OutputContext& context) { get(op, axis_knob, context); }
    //! Extracts values from an Axis_Knob named 'transform' on Op.
    AxisKnobVals(const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context) { get(op, context); }

    //----------------------------------------------------------------------

    //! Compares time value. Used by the sort routine.
    bool operator < (const AxisKnobVals& b) const { return (time < b.time); }


    //! Print values out.
    void print(const char*   prefix,
               std::ostream& o) const;

    //----------------------------------------------------------------------

    //! Build a matrix from the current values.
    Fsr::Mat4d getMatrix() const;

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

    //! Extract transform values from an Axis_Knob at an OutputContext. Returns false if not possible.
    bool get(const DD::Image::Op*            op,
             const DD::Image::Knob*          axis_knob,
             const DD::Image::OutputContext& context);

    //! Extract transform values from an AxisOp at an OutputContext. Returns false if not possible.
    bool get(const DD::Image::Op*            op,
             const DD::Image::OutputContext& context);

    //!
    bool extractFromMatrix(const Fsr::Mat4d&  m,
                           bool               T_enable,
                           bool               R_enable,
                           bool               S_enable,
                           Fsr::RotationOrder decompose_rot_order,
                           bool               apply_to_parent=false);

    //----------------------------------------------------------------------

    //!
    static bool store(DD::Image::Op*   op,
                      AxisKnobValsList axis_vals_list);

    //!
    static void applyEulerFilter(Fsr::RotationOrder         target_rot_order,
                                 std::vector<AxisKnobVals>& vals,
                                 bool                       sort=false);

    //----------------------------------------------------------------------

    //! Clears any animation from the knobs we will store() into.
    static void clearAnimation(DD::Image::Op*                  op,
                               const DD::Image::OutputContext& context);

    //!
    bool store(DD::Image::Op*                  op,
               const DD::Image::OutputContext& context) const;

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Knob construction/store callback 'macro' similar to the ones
    defined in Knobs.h. It declares a DD::Image::CUSTOM_KNOB
    enumeration and a DD::Image::Custom data type.

    This knob stores a 3D transformation into a double-precision
    Fsr::Mat4d. It relies on the existence of a companion Axis_Knob
    being present on the same parent Op so that the 'translate',
    'rotate', 'scaling', etc knobs exist.

    Add this knob *after* the Axis_Knob so we're confident the
    Axis_Knob constructs/stores before this one (shouldn't
    really matter though.)
*/
DD::Image::Knob* AxisKnobWrapper_knob(DD::Image::Knob_Callback f,
                                      Fsr::Mat4d*              matrix,
                                      const char*              name);


/*! \class Fsr::AxisKnob

    \brief DD::Image::Knob companion 'wrapper' for Nuke's AxisKnob class.

    Not a true wrapper but more of a companion Knob so that we can
    augment the stock single-precision AxisKnob that comes with Nuke.

    This knob relies on an Axis_Knob already being present on the parent
    Op and it creating the child knobs 'translate', 'rotate', 'scaling', etc.

    All this knob does is implement a store() routine that builds a
    double-precision Fuser Matd from those knobs in the same manner
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

    //! Get a Fsr::Mat4d matrix built at the specified output context. Updates the hash if provided.
    Fsr::Mat4d getMatrixAt(const DD::Image::OutputContext& context,
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
