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

/// @file Fuser/AxisKnob.cpp
///
/// @author Jonathan Egstad

#include "AxisKnob.h"
#include "Lookat.h"
#include "NukeKnobInterface.h" // for getVec3Knob

#include <DDImage/TransformGeo.h>
#include <DDImage/Knobs.h>
#include <DDImage/ViewerContext.h>
#include <DDImage/gl.h>

#include <sstream> // for print_name

namespace Fsr {


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


/*! Print values out.
*/
void
AxisKnobVals::print(const char*   prefix,
                    std::ostream& o) const
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(20);
    o << prefix;
    o << "xform_order=" << xform_order;
    o << ", rot_order=" << rot_order;
    o << ", translate"  << translate;
    o << ", rotate"     << rotate;
    o << ", scaling"    << scaling;
    o << ", uniform_scale=" << uniform_scale;
    o << ", skew"       << skew;
    o << ", pivot"      << pivot;
    o << ", useMatrix=" << use_matrix;
    if (use_matrix)
        o << ", matrix" << matrix;
    o << ", parent_translate" << parent_translate;
    o << ", parent_rotate"    << parent_rotate;
    o << ", parent_scale"     << parent_scale;
    o << " ]";
}


/*! Build a matrix from the current values.
*/
Fsr::Mat4d
AxisKnobVals::getMatrix() const
{
    if (use_matrix)
        return matrix;

    Fsr::Mat4d m;
    m.setToTransform(xform_order,
                     rot_order,
                     translate,
                     rotate,
                     totalScaling(),
                     skew,
                     pivot);
    return m;
}



/*!
*/
/*static*/
void
AxisKnobVals::applyEulerFilter(Fsr::RotationOrder         target_rot_order,
                               std::vector<AxisKnobVals>& vals,
                               bool                       sort)
{
    if (sort)
        std::sort(vals.begin(), vals.end());

    const size_t nSamples = vals.size();
    std::vector<Vec3d> rotation_angles(nSamples);
    for (size_t j=0; j < nSamples; ++j)
        rotation_angles[j] = vals[j].rotate;

    Fsr::eulerFilterRotations(rotation_angles, target_rot_order);

    for (size_t j=0; j < nSamples; ++j)
        vals[j].rotate = rotation_angles[j];
}


static Fsr::Vec3d default_translate(0.0, 0.0, 0.0);
static Fsr::Vec3d    default_rotate(0.0, 0.0, 0.0);
static Fsr::Vec3d     default_scale(1.0, 1.0, 1.0);
static Fsr::Vec3d      default_skew(0.0, 0.0, 0.0);
static Fsr::Vec3d     default_pivot(0.0, 0.0, 0.0);


/*! Assigns standard default values to all params.
*/
void
AxisKnobVals::setToDefault(double _time)
{
    time          = _time;
    xform_order   = Fsr::SRT_ORDER;
    rot_order     = Fsr::XYZ_ORDER;
    use_matrix    = false;
    //
    setLocalXformValsToDefault();
    setParentXformValsToDefault();
}


/*! Assigns standard default values to transform params (rotate, scale, etc.)
*/
void
AxisKnobVals::setLocalXformValsToDefault()
{
    translate = default_translate;
    rotate    = default_rotate;
    scaling   = default_scale;
    uniform_scale = 1.0;
    skew      = default_skew;
    pivot     = default_pivot;
    matrix.setToIdentity();
}
/*! Return true if xform vals are at default settings.
*/
bool
AxisKnobVals::isLocalXformValsDefault() const
{
    if (use_matrix)
        return matrix.isIdentity();
    return !(translate     != default_translate ||
             rotate        != default_rotate    ||
             scaling       != default_scale     ||
             uniform_scale != 1.0               ||
             skew          != default_skew      ||
             pivot         != default_pivot);
}


/*! Assigns standard default values to transform params (rotate, scale, etc.)
*/
void
AxisKnobVals::setParentXformValsToDefault()
{
    parent_enable = false;
    parent_translate = default_translate;
    parent_rotate    = default_rotate;
    parent_scale     = default_scale;
}
/*! Return true if xform vals are at default settings.
*/
bool
AxisKnobVals::isParentXformValsDefault() const
{
    return !(parent_translate != default_translate ||
             parent_rotate    != default_rotate    ||
             parent_scale     != default_scale);
}



/*! Extract transform knob values from an Axis_Knob at an OutputContext.
    Returns false if not possible.

    Depending on the connections and knob settings we can either
    export the transform arguments as trans/rot/scale values or
    we need to output a explicit matrix.

    We want to use the raw knob values as much as possible to
    retain double-precision of the source knobs, even if we're
    exporting a matrix since the concatenated DD::Image::Matrix4s
    are always single-precision...  :(

    At the moment the only reason to export a single-precision
    source Matrix4 is either when an explicit matrix is set in the
    Axis_Knob ('use_matrix'), or when Lookat mode is enabled and we
    can't preserve the translation location due to xform order.
*/
bool
AxisKnobVals::getValsAt(const DD::Image::Op*            op,
                        const DD::Image::Knob*          axis_knob,
                        const DD::Image::OutputContext& context)
{
    //std::cout << "      AxisKnobVals:getValsAt()" << std::endl;
    if (!op || !axis_knob/* || !const_cast<DD::Image::Knob*>(axis_knob)->axisKnob()*/)
        return false; // don't crash...

    /*  Standard set of Axis transform knobs that we care
        about and their defaults:
            xform_order   SRT
            rot_order     ZXY
            translate     0 0 0
            rotate        0 0 0
            scaling       1 1 1
            uniform_scale 1
            skew          0 0 0
            pivot         0 0 0
            useMatrix     false     (use the 'matrix' knob below)
            matrix        1 0 0 0
                          0 1 0 0
                          0 0 1 0
                          0 0 0 1   (is this knob usually filled in by file reading?)
    */

    setToDefault(context.frame()/*time*/);

    // Get raw values from Axis_Knob sub-knobs through the DD::Image::StoreType
    // interface.
    // Can't use DD::Image::AxisOp::getAxis() methods as we need access to underlying
    // Array_knob data as doubles...!

    DD::Image::Knob* k;
    k = op->knob("xform_order");
    if (k)
    {
        int v;
        getIntKnob(k, context, v);
        xform_order = (Fsr::XformOrder)v;
    }
    else
        xform_order = Fsr::SRT_ORDER;
    k = op->knob("rot_order");
    if (k)
    {
        int v;
        getIntKnob(k, context, v);
        rot_order = (Fsr::RotationOrder)v;
    }
    else
        rot_order = Fsr::XYZ_ORDER;
    k = op->knob("translate"    ); if (k)   getVec3Knob(k, context, translate    ); else translate.set(0.0);
    k = op->knob("rotate"       ); if (k)   getVec3Knob(k, context, rotate       ); else rotate.set(0.0);
    k = op->knob("scaling"      ); if (k)   getVec3Knob(k, context, scaling      ); else scaling.set(1.0);
    k = op->knob("uniform_scale"); if (k) getDoubleKnob(k, context, uniform_scale); else uniform_scale = 1.0;
    k = op->knob("skew"         ); if (k)   getVec3Knob(k, context, skew         ); else skew.set(0.0);
    k = op->knob("pivot"        ); if (k)   getVec3Knob(k, context, pivot        ); else pivot.set(0.0);
    //
    k = op->knob("useMatrix"    ); if (k)   getBoolKnob(k, context, use_matrix   ); else use_matrix = false;
    if (use_matrix)
    {
        k = op->knob("matrix"   ); if (k) getMat4Knob(k, context, matrix         ); else matrix.setToIdentity();
    }
    //
    if (parent_enable)
    {
        k = op->knob("parent_translate"); if (k) getVec3Knob(k, context, parent_translate); else parent_translate.set(0.0);
        k = op->knob("parent_rotate"   ); if (k) getVec3Knob(k, context, parent_rotate   ); else parent_rotate.set(0.0);
        k = op->knob("parent_scale"    ); if (k) getVec3Knob(k, context, parent_scale    ); else parent_scale.set(1.0);
    }

#if 0
    std::cout << "      AxisKnobVals::getValsAt() knob vals:";
    std::cout << " xform_order=" << xform_order;
    std::cout << ", rot_order=" << rot_order;
    std::cout << ", translate"  << translate;
    std::cout << ", rotate"     << rotate;
    std::cout << ", scaling"    << scaling;
    std::cout << ", uniform_scale=" << uniform_scale;
    std::cout << ", skew"       << skew;
    std::cout << ", pivot"      << pivot;
    std::cout << ", useMatrix=" << use_matrix;
    if (use_matrix)
        std::cout << ", matrix" << matrix;
    std::cout << std::endl;
    std::cout << ", parent_translate" << parent_translate;
    std::cout << ", parent_rotate"    << parent_rotate;
    std::cout << ", parent_scale"     << parent_scale;
#endif

    return true;
}


/*! Extract transform knob values from an Op at an OutputContext.
    Returns false if not possible.

    Depending on the connections and knob settings we can either
    export the transform arguments as trans/rot/scale values or
    we need to output a explicit matrix.

    We want to use the raw knob values as much as possible to
    retain double-precision of the source knobs, even if we're
    exporting a matrix since the concatenated DD::Image::Matrix4s
    are always single-precision...  :(

    At the moment the only reason to export a single-precision
    source Matrix4 is either when an explicit matrix is set in the
    Axis_Knob ('use_matrix'), or when Lookat mode is enabled and we
    can't preserve the translation location due to xform order.
*/
bool
AxisKnobVals::getValsAt(const DD::Image::Op*            op,
                        const DD::Image::OutputContext& context)
{
    //std::cout << "      AxisKnobVals:getValsAt()" << std::endl;
    if (!op)
        return false; // don't crash...

    // Get the explicit knob vs. using the AxisOp::axisKnob() method:
    DD::Image::Knob* axis_knob = op->knob("transform");
    if (!axis_knob || !axis_knob->axisKnob())
    {
        std::cerr << "AxisKnobVals:getValsAt(): error, no 'transform' knob on AxisOp." << std::endl;
        return false;
    }

    // Extract the local transform values at this context:
    this->getValsAt(op, axis_knob, context);

#if 0
    // Now, possibly modify the rotations by the standard AxisOp lookat logic:

    /*  These lookat knobs are only on Axis2, Camera2, Light2 etc plugins,
        check for their existence before accessing them!
            look_axis            +Z
            look_rotate_x        true
            look_rotate_y        true
            look_rotate_z        true
            look_strength        1
            look_use_quaternions false

    */

    // Is the 'look' input connection connected (and thus enabled)?
    // At the moment only AxisOp and TransformGeo are subclasses of LookAt:
    const DD::Image::AxisOp*       lookat_axis = NULL;
    const DD::Image::AxisOp*       axis_op = dynamic_cast<const DD::Image::AxisOp*>(op);
    const DD::Image::TransformGeo* tgeo_op = dynamic_cast<const DD::Image::TransformGeo*>(op);
    if (axis_op)
        lookat_axis = dynamic_cast<DD::Image::AxisOp*>(axis_op->lookat_input());
    else if (tgeo_op)
        lookat_axis = dynamic_cast<DD::Image::AxisOp*>(tgeo_op->lookat_input());

    // Also check if lookat is enabled and we can't trivially derive the translation
    // of the xform to use as a lookat source point the also output matrix:
    if (lookat_axis != NULL && !use_matrix)
    {
        // In the special (and common) case of SRT and RST xform order the translation
        // point is always the translation value, so we can simply overwrite the rotations
        // with the derived ones.
        if (xform_order == Fsr::SRT_ORDER || xform_order == Fsr::RST_ORDER)
            use_matrix = false;
        else
            use_matrix = true; // enable matrix mode for all other transform orders
        //std::cout << "        lookat enabled, op='" << lookat_axis->node_name() << "', use_matrix=" << use_matrix << std::endl;
    }

    // Write a matrix if 'useMatrix' (specify matrix) is enabled on the AxisOp,
    // or lookat has forced it:
    if (use_matrix)
    {
        // Use the Axis_KnobI interface to get the user-defined matrix without
        // the lookat matrix mixed in:
        DD::Image::Matrix4 m = axis_knob->axisKnob()->matrix(context);
        if (lookat_axis != NULL)
        {
            // Apply lookat transform:
            if (axis_op)
                const_cast<DD::Image::AxisOp*>(axis_op)->lookMatrixAt(context, m);
            else if (tgeo_op)
                const_cast<DD::Image::TransformGeo*>(tgeo_op)->lookMatrixAt(context, m);
        }
        matrix = m;

    }
    else
    {
        // In the special-case lookat mode where the final translation point is always
        // the translation knob value we can simply overwrite the rotations with the
        // derived lookat ones:
        if (lookat_axis != NULL)
        {
            Fsr::AxisDirection look_axis = Fsr::AXIS_Z_PLUS;
            bool               look_rotate_x = true;
            bool               look_rotate_y = true;
            double             look_rotate_z = true;
            bool               look_strength = 1.0;
            bool               look_use_quaternions = false;
            //
            DD::Image::Knob* k;
            DD::Image::Hash dummy_hash;
            k = op->knob("look_axis"           ); if (k) k->store(DD::Image::IntPtr,    &look_axis,     dummy_hash, context);
            k = op->knob("look_rotate_x"       ); if (k) k->store(DD::Image::BoolPtr,   &look_rotate_x, dummy_hash, context);
            k = op->knob("look_rotate_y"       ); if (k) k->store(DD::Image::BoolPtr,   &look_rotate_y, dummy_hash, context);
            k = op->knob("look_rotate_z"       ); if (k) k->store(DD::Image::BoolPtr,   &look_rotate_z, dummy_hash, context);
            k = op->knob("look_strength"       ); if (k) k->store(DD::Image::DoublePtr, &look_strength, dummy_hash, context);
            k = op->knob("look_use_quaternions"); if (k) k->store(DD::Image::BoolPtr,   &look_use_quaternions, dummy_hash, context);
            //std::cout << "        look_axis=" << look_axis;
            //std::cout << ", look_rotate_x=" << look_rotate_x << ", look_rotate_y=" << look_rotate_y << ", look_rotate_z=" << look_rotate_z;
            //std::cout << ", look_strength=" << look_strength << ", look_use_quaternions=" << look_use_quaternions;
            //std::cout << std::endl;

            // Remap look axis enum from Nuke's to Fuser's (should do nothing...!):
            switch (look_axis)
            {
                // Stoopid DDImage LookAt class has protected enums...
                case 0/*DD::Image::LookAt::kAxisZPlus*/:  look_axis = Fsr::AXIS_Z_PLUS;  break;
                case 1/*DD::Image::LookAt::kAxisZMinus*/: look_axis = Fsr::AXIS_Z_MINUS; break;
                case 2/*DD::Image::LookAt::kAxisYPlus*/:  look_axis = Fsr::AXIS_Y_PLUS;  break;
                case 3/*DD::Image::LookAt::kAxisYMinus*/: look_axis = Fsr::AXIS_Y_MINUS; break;
                case 4/*DD::Image::LookAt::kAxisXPlus*/:  look_axis = Fsr::AXIS_X_PLUS;  break;
                case 5/*DD::Image::LookAt::kAxisXMinus*/: look_axis = Fsr::AXIS_X_MINUS; break;
            }

            // Decompose lookat vector into rotations, replacing the ones from AxisOp.
            // Build vector from point to lookat point and convert to rotations:
            const Fsr::Vec3d axisP((axis_op) ? axis_op->matrix().translation() :
                                   (tgeo_op) ? tgeo_op->matrix().translation() : Fsr::Vec3d(0.0));
            const Fsr::Vec3d lookP(lookat_axis->matrix().translation());
            //std::cout << "        axisP=" << axisP << std::endl;
            //std::cout << "        lookP=" << lookP << std::endl;
            Fsr::Vec3d look_rotations;
            Fsr::LookatVals::vectorToRotations((look_use_quaternions) ? Fsr::LookatVals::USE_QUATS :
                                                                    Fsr::LookatVals::USE_VECTORS,
                                           (lookP - axisP),
                                           look_axis,
                                           look_rotate_x,
                                           look_rotate_y,
                                           look_rotate_z,
                                           look_strength,
                                           look_rotations);
            // In lookat mode we always use ZXY order:
            rot_order = Fsr::ZXY_ORDER;
            //std::cout << "          new rotations" << look_rotations << std::endl;
        }

    }
#endif

    return true;
}


/*!
*/
bool
AxisKnobVals::extractFromMatrix(const Fsr::Mat4d&  m,
                                bool               T_enable,
                                bool               R_enable,
                                bool               S_enable,
                                Fsr::RotationOrder decompose_rot_order,
                                bool               apply_to_parent)
{
    bool decompose_ok = true;
    Fsr::Vec3d scale0(1.0), skew0(0.0), rotate0(0.0), translate0(0.0);
    if (!m.isIdentity())
    {
        // TODO: add control for parent rot order?
        decompose_ok = m.extractSHRT(scale0,
                                     skew0,
                                     rotate0,
                                     translate0,
                                     decompose_rot_order);
        if (T_enable)
            translate0.roundIfNearlyZero();
        if (R_enable)
        {
            skew0.roundIfNearlyZero();
            rotate0.roundIfNearlyZero();
        }
        if (S_enable)
            scale0.roundIfNearlyOne();
    }

    if (apply_to_parent)
    {
        setParentXformValsToDefault();
        if (S_enable)
            parent_scale     = scale0;
        if (R_enable)
            parent_rotate    = rotate0;
        if (T_enable)
            parent_translate = translate0;
        //std::cout << "    parent: parent_rotate" << parent_rotate;
        //std::cout << ", parent_scale" << parent_scale;
        //std::cout << ", parent_translate" << parent_translate;
        //std::cout << std::endl;
    }
    else
    {
        setLocalXformValsToDefault();
        if (S_enable)
            scaling   = scale0;
        if (R_enable)
        {
            skew      = skew0;
            rotate    = rotate0;
        }
        if (T_enable)
            translate = translate0;
        //std::cout << "    local: rotate" << rotate;
        //std::cout << ", scaling" << scaling;
        //std::cout << ", skew" << skew;
        //std::cout << ", translate" << translate;
        //std::cout << std::endl;
    }

    return decompose_ok;
}


/*! Clears any animation from the knobs we will store() into.
*/
/*static*/
void
AxisKnobVals::clearAnimation(DD::Image::Op*                  op,
                             const DD::Image::OutputContext& context)
{
    if (!op)
        return; // don't crash...

    DD::Image::Knob* k;
    DD::Image::Hash dummy_hash;

    k = op->knob("parent_translate"); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
    k = op->knob("parent_rotate"   ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
    k = op->knob("parent_scale"    ); if (k) { k->clear_animated(-1); k->set_value(1.0, -1); }

    k = op->knob("translate"       ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
    k = op->knob("rotate"          ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
    k = op->knob("scaling"         ); if (k) { k->clear_animated(-1); k->set_value(1.0, -1); }
    k = op->knob("uniform_scale"   ); if (k) { k->clear_animated(-1); k->set_value(1.0); }
    k = op->knob("skew"            ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
    k = op->knob("pivot"           ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }

    k = op->knob("rot_order"       ); if (k) k->set_value(double(Fsr::XYZ_ORDER));
    k = op->knob("xform_order"     ); if (k) k->set_value(double(Fsr::SRT_ORDER));
    k = op->knob("usdMatrix"       ); if (k) k->set_value(0.0);
}


/*!
*/
bool
AxisKnobVals::store(DD::Image::Op*                  op,
                    const DD::Image::OutputContext& context) const
{
    //std::cout << "      AxisKnobVals:store('" << op->node_name() << "')" << std::endl;
    if (!op)
        return false; // don't crash...

    DD::Image::Knob* k;
    DD::Image::Hash dummy_hash;

    // TODO: these enables should be on the AxisKnobVals class, or something like it.
    bool do_translate = true;
    bool do_rotation  = true;
    bool do_scaling   = true;
    k = op->knob("translate_enable" ); if (k) k->store(DD::Image::BoolPtr, &do_translate, dummy_hash, context);
    k = op->knob("rotate_enable"    ); if (k) k->store(DD::Image::BoolPtr, &do_rotation,  dummy_hash, context);
    k = op->knob("scale_enable"     ); if (k) k->store(DD::Image::BoolPtr, &do_scaling,   dummy_hash, context);

#if 1
    if (do_translate)
    {
        storeVec3dInKnob(translate, op->knob("translate"), context, 0/*offset*/);
        if (parent_enable)
            storeVec3dInKnob(parent_translate, op->knob("parent_translate"), context, 0/*offset*/);
    }
    if (do_rotation)
    {
        storeVec3dInKnob(rotate, op->knob("rotate"), context, 0/*offset*/);
        if (parent_enable)
            storeVec3dInKnob(parent_rotate, op->knob("parent_rotate"), context, 0/*offset*/);
    }
    if (do_scaling)
    {
        storeVec3dInKnob(scaling, op->knob("scaling"), context, 0/*offset*/);
        //storeDoubleInKnob(uniform_scale, op->knob("uniform_scale"), context, 0/*offset*/);
        if (parent_enable)
            storeVec3dInKnob(parent_scale, op->knob("parent_scale"), context, 0/*offset*/);
    }
    //storeVec3dsInKnob(skew,  op->knob("skew" ), context, 0/*offset*/);
    //storeVec3dsInKnob(pivot, op->knob("pivot"), context, 0/*offset*/);
#else
    if (do_translate)
    {
        Fsr::storeVec3dsInKnob(op->knob("translate"), translate, times, -1/*view*/);
        if (do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_translate"), parent_translate, times, -1/*view*/);
    }
    if (do_rotation)
    {
        Fsr::storeVec3dsInKnob(op->knob("rotate"), rotate, times, -1/*view*/);
        if (do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_rotate"), parent_rotate, times, -1/*view*/);
    }
    if (do_scaling)
    {
        Fsr::storeVec3dsInKnob(op->knob("scaling"), scaling, times, -1/*view*/);
        //Fsr::storeDoublesInKnob(op->knob("uniform_scale"), uniform_scale, times, -1/*view*/);
        if (do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_scale"), parent_scale, times, -1/*view*/);
    }
    //Fsr::storeVec3dsInKnob(op->knob("skew" ), skew,  times, -1/*view*/);
    //Fsr::storeVec3dsInKnob(op->knob("pivot"), pivot, times, -1/*view*/);
#endif

    return true;
}


/*!
*/
/*static*/ bool
AxisKnobVals::store(DD::Image::Op*   op,
                    AxisKnobValsList axis_vals_list)
{
    if (!op)
        return false; // don't crash...

    DD::Image::OutputContext context;
    context.setView(-1);

    // Creating a KnobChangeGroup causes Nuke to batch up knobChanged messages,
    // sending only one upon destruction:
    { DD::Image::KnobChangeGroup change_group;

        // Store separate xform parameters.
        AxisKnobVals::clearAnimation(op, context);

        const size_t nSamples = axis_vals_list.size();
        if (nSamples > 0)
        {
            for (size_t j=0; j < nSamples; ++j)
            {
                const AxisKnobVals& axis_vals = axis_vals_list[j];
                //std::cout << "           time=" << axis_vals.time << std::endl;
                //std::cout << "          rotate" << axis_vals.rotate << std::endl;
                //std::cout << "         scaling" << axis_vals.scaling << std::endl;
                //std::cout << "       translate" << axis_vals.translate << std::endl;
                //std::cout << "            skew" << axis_vals.skew << std::endl;
                //std::cout << "  parent_enable=" << axis_vals.parent_enable << std::endl;
                //std::cout << "parent_translate" << axis_vals.parent_translate << std::endl;
                //std::cout << "   parent_rotate" << axis_vals.parent_rotate << std::endl;
                //std::cout << "    parent_scale" << axis_vals.parent_scale << std::endl;

                context.setFrame(axis_vals.time);
                axis_vals.store(op, context);
            }

            // Set the rotation order to match the decompose order:
            DD::Image::Knob* k;
            k = op->knob("rot_order"  ); if (k) k->set_value(double(axis_vals_list[0].rot_order  ));
            k = op->knob("xform_order"); if (k) k->set_value(double(axis_vals_list[0].xform_order));
        }

    } // DD::Image::KnobChangeGroup

    return true;
}


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


/*! The ctor should only get called when Knob_Closure has makeKnobs()==true.
    Ctor does not require a data pointer since it does not have a separate default.
*/
AxisKnobWrapper::AxisKnobWrapper(DD::Image::Knob_Closure* cb,
                                 const char*              name) :
    DD::Image::Knob(cb, name),
    kXformOrder(NULL),
    kRotOrder(NULL),
    kTranslate(NULL),
    kRotate(NULL),
    kScale(NULL),
    kUniformScale(NULL),
    kSkew(NULL),
    kPivot(NULL),
    kUseMatrix(NULL),
    kMatrix(NULL)
{
    //std::cout << "    AxisKnobWrapper::ctor(" << this << ")" << std::endl;
    // We don't want the knob getting written into script files or being visible:
    this->set_flag(DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::INVISIBLE);
}


/*! Knob construction/store callback 'macro' similar to the ones defined in
    Knobs.h. It declares a DD::Image::CUSTOM_KNOB enumeration and a
    DD::Image::Custom data type.
*/
DD::Image::Knob*
AxisKnobWrapper_knob(DD::Image::Knob_Callback f,
                     Fsr::Mat4d*              matrix,
                     const char*              name)
{
    // TODO: no idea if this bool is needed, it matches the logic in the the CustomKnob macros.
    // This is false if the knob will be filtered out by name (used only for custom knobs.)
    const bool filter_name = f.filter(name);

    DD::Image::Knob* k = NULL;
    if (f.makeKnobs() && filter_name)
    {
        // Create the AxisKnob wrapper knob:
        DD::Image::Knob* axis_wrapper = new Fsr::AxisKnobWrapper(&f, name);
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              NULL/*data*/,
              name,
              NULL/*label*/,
              axis_wrapper/*extra*/);
        //std::cout << "  AxisKnobWrapper_knob(" << name << ")::Knob_Callback:makeKnobs, knob=" << k << std::endl;
    }
    else
    {
        // Store the knob. This callback calls the store() method
        // below which in turn calls getMatrixAt() at the correct
        // OutputContext and fills in 'matrix'. It should return
        // the same knob pointer created above for the same Op.
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              matrix,
              name,
              NULL/*label*/,
              NULL/*extra*/);
        //std::cout << "  AxisKnobWrapper_knob(" << name << ")::Knob_Callback:store, knob=" << k << std::endl;
    }
#if DEBUG
    assert(k);
#endif
    return k;
}


/*! Stores the transform into a double-precision Fuser Mat4d.
*/
/*virtual*/ void
AxisKnobWrapper::store(DD::Image::StoreType            type,
                       void*                           p,
                       DD::Image::Hash&                hash,
                       const DD::Image::OutputContext& context)
{
    //std::cout << "    AxisKnobWrapper::store(" << this << ")" << std::endl;
    Fsr::Mat4d* matrix = reinterpret_cast<Fsr::Mat4d*>(p);
#if 1//DEBUG
    assert(matrix);
    assert(type == DD::Image::Custom);
#endif

    *matrix = getMatrixAt(context);
    hash.append(matrix->array(), sizeof(Fsr::Mat4d));
}


/*! Get AxisVals filled in at the specified output context.
    Updates the hash if provided.
*/
void
AxisKnobWrapper::getValsAt(const DD::Image::OutputContext& context,
                           AxisKnobVals&                   vals,
                           DD::Image::Hash*                hash)
{
    //std::cout << "    AxisKnobWrapper(" << name() << ")::getValsAt() frame=" << context.frame();
    //std::cout << ", view=" << context.view();

    // Get the Knob pointers to the local transform knobs that were created
    // by the Axis_Knob - there is an Axis_Knob to wrap, yes?
    if (kXformOrder == NULL && kMatrix == NULL)
    {
        assert(Knob::op());
        DD::Image::Op* op = DD::Image::Knob::op()->firstOp();
        // We need all the knobs so throw an assert if any are missing:
        kXformOrder   = op->knob("xform_order"  ); assert(kXformOrder  );
        kRotOrder     = op->knob("rot_order"    ); assert(kRotOrder    );
        kTranslate    = op->knob("translate"    ); assert(kTranslate   );
        kRotate       = op->knob("rotate"       ); assert(kRotate      );
        kScale        = op->knob("scaling"      ); assert(kScale       );
        kUniformScale = op->knob("uniform_scale"); assert(kUniformScale);
        kSkew         = op->knob("skew"         ); assert(kSkew        );
        kPivot        = op->knob("pivot"        ); assert(kPivot       );
        kUseMatrix    = op->knob("useMatrix"    ); assert(kUseMatrix   );
        kMatrix       = op->knob("matrix"       ); assert(kMatrix      );
    }

    // Point to a dummy hash value if one not provided:
    DD::Image::Hash dummy_hash;
    if (!hash)
        hash = &dummy_hash;

    // Call Knob::store() on all the Axis_Knob child knobs and
    // force all the XYZ knobs to store as DoublePtr:
    {
        int v;
        kXformOrder->store(DD::Image::IntPtr, &v, *hash, context);
        vals.xform_order = (Fsr::XformOrder)v;
    }
    {
        int v;
        kRotOrder->store(DD::Image::IntPtr, &v, *hash, context);
        vals.rot_order = (Fsr::RotationOrder)v;
    }
    kTranslate->store(DD::Image::DoublePtr,    vals.translate.array(), *hash, context);
    kRotate->store(DD::Image::DoublePtr,       vals.rotate.array(),    *hash, context);
    kScale->store(DD::Image::DoublePtr,        vals.scaling.array(),   *hash, context);
    kUniformScale->store(DD::Image::DoublePtr, &vals.uniform_scale,    *hash, context);
    kSkew->store(DD::Image::DoublePtr,         vals.skew.array(),      *hash, context);
    kPivot->store(DD::Image::DoublePtr,        vals.pivot.array(),     *hash, context);

    kUseMatrix->store(DD::Image::BoolPtr, &vals.use_matrix, *hash, context);
    if (vals.use_matrix)
        kMatrix->store(DD::Image::DoublePtr, vals.matrix.array(), *hash, context);

    //vals.print(" ", std::cout); std::cout << std::endl;
}


/*! Get a Fsr::Mat4d matrix built at the specified output context.
    Updates the hash if provided.

    Does *not* apply any lookat modification.

    This method calls Knob::store() on all the Axis_Knob child knobs
    like 'translate', 'rotate', 'scaling', etc so that it can access
    the double-precision values. These are XYZ_Knobs that normally
    store to DD::Image::Vector3 which are only single-precision, so
    so can't access the already stored values.

    It primarily matters for the 'translate' knob since translate offset
    is the cause of most precision problems, but we're doing it for all
    of the XYZ knobs.
*/
Fsr::Mat4d
AxisKnobWrapper::getMatrixAt(const DD::Image::OutputContext& context,
                             DD::Image::Hash*                hash)
{
    //std::cout << "    AxisKnobWrapper(" << name() << ")::getMatrixAt() frame=" << context.frame();
    //std::cout << ", view=" << context.view();
    Fsr::AxisKnobVals axis_vals;
    getValsAt(context, axis_vals, hash);
    return axis_vals.getMatrix();
}


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


// These match the strings in DD::Image::LookAt so Enumeration_Knobs using these
// save the same thing as stock Nuke:
/*static*/ const char* const LookatVals::method_list[] = { "vectors", "quaternions", 0 };


/*!
*/
LookatVals::LookatVals()
{
    setToDefault();
}


/*! Assigns standard default values to all params.
*/
void
LookatVals::setToDefault()
{
    k_lookat_enable    = true;
    k_lookat_axis      = (int)Fsr::AXIS_Z_MINUS;
    k_lookat_do_rx     = true;
    k_lookat_do_ry     = true;
    k_lookat_do_rz     = true;
    k_lookat_use_point = false;
    k_lookat_point.set(0.0, 0.0, 0.0);
    k_lookat_method    = USE_VECTORS;
    k_lookat_mix       = 1.0f;
}


/*!
*/
void
LookatVals::addLookatKnobs(DD::Image::Knob_Callback f,
                           const char*              label)
{
    DD::Image::Bool_knob(f, &k_lookat_enable, "look_enable", "enable");
DD::Image::SetFlags(f, DD::Image::Knob::INVISIBLE);
    //
    DD::Image::Enumeration_knob(f, &k_lookat_axis, axis_directions, "look_axis", "aim axis");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "Selects which axis is oriented(aimed) towards the lookat point.");
    DD::Image::Bool_knob(f, &k_lookat_do_rx, "look_rotate_x", "x rot");
    DD::Image::Bool_knob(f, &k_lookat_do_ry, "look_rotate_y", "y rot");
    DD::Image::Bool_knob(f, &k_lookat_do_rz, "look_rotate_z", "z rot");
    DD::Image::Bool_knob(f, &k_lookat_method, "look_use_quaternions", "use quaternions");
    //DD::Image::Enumeration_knob(f, &k_lookat_method, method_list, "look_use_quaternions", "method");
    //
    DD::Image::Bool_knob(f, &k_lookat_use_point, "look_use_point", "");
DD::Image::SetFlags(f, DD::Image::Knob::INVISIBLE);
//        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
//        DD::Image::Tooltip(f, "Use a user-specified point to look at, ignoring the lookat input connection.");
    DD::Image::XYZ_knob(f, k_lookat_point.array(), "lookat_point", "lookat point");
DD::Image::SetFlags(f, DD::Image::Knob::INVISIBLE);
//        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
//        DD::Image::Tooltip(f, "User-specified point to look at, ignoring the lookat input connection.");
    //
    //
    DD::Image::Double_knob(f, &k_lookat_mix, "look_strength", "mix");
        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE | DD::Image::Knob::NO_MULTIVIEW);
        DD::Image::SetFlags(f, DD::Image::Knob::NO_MULTIVIEW);
        DD::Image::Tooltip(f, "How much the lookat rotations affect the output rotations.");
}


/*!
*/
void
LookatVals::appendLookatHash(DD::Image::Hash& hash) const
{
    if (!k_lookat_enable)
        return;
    hash.append(k_lookat_do_rx);
    hash.append(k_lookat_do_ry);
    hash.append(k_lookat_do_rz);
    hash.append(k_lookat_use_point);
    hash.append(k_lookat_point.array(), 3*sizeof(float));
    hash.append(k_lookat_method);
    hash.append(k_lookat_axis);
    hash.append(k_lookat_mix);
}


/*!
*/
int
LookatVals::knobChanged(const DD::Image::Op* op,
                        DD::Image::Knob*     k)
{
    if (k == &DD::Image::Knob::showPanel || k->name() == "look_enable")
    {
        enableLookatKnobs(op, k_lookat_enable);
        return 1; // we want to be called again
    }
    return 0;
}


//!
void
LookatVals::enableLookatKnobs(const DD::Image::Op* op,
                              bool                 lookat_enabled)
{
    if (!op)
        return; // don't crash...

    DD::Image::Knob* k;
    k = op->knob("look_axis"           ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_rotate_x"       ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_rotate_y"       ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_rotate_z"       ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_use_point"      ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_point"          ); if (k) k->enable(lookat_enabled);
    k = op->knob("look_use_quaternions"); if (k) k->enable(lookat_enabled);
    k = op->knob("look_strength"       ); if (k) k->enable(lookat_enabled);
}


/*!
*/
bool
LookatVals::getValsAt(const DD::Image::Op*            op,
                      const DD::Image::OutputContext& context)
{
    //std::cout << "      Lookat:getValsAt()" << std::endl;
    if (!op)
        return false; // don't crash...

    /*
        bool       k_lookat_enable;     //!< Global enable
        int        k_lookat_axis;       //!< Axis to align
        bool       k_lookat_do_rx;      //!< Enable X lookat rotation
        bool       k_lookat_do_ry;      //!< Enable Y lookat rotation
        bool       k_lookat_do_rz;      //!< Enable Z lookat rotation
        bool       k_lookat_use_point;  //!< Use the user-specified point rather than the input connection
        Fsr::Vec3f k_lookat_point;      //!< User-assigned world-space lookat point
        bool       k_lookat_method;     //!< Which method to use - vectors(true) or quaternions(false)
        double     k_lookat_mix;        //!< Lookat mix
    */

    setToDefault();

    DD::Image::Knob* k;
    k = op->knob("look_enable"         ); if (k)   getBoolKnob(k, context, k_lookat_enable);
    k = op->knob("look_axis"           ); if (k)    getIntKnob(k, context, k_lookat_axis);
    k = op->knob("look_rotate_x"       ); if (k)   getBoolKnob(k, context, k_lookat_do_rx);
    k = op->knob("look_rotate_y"       ); if (k)   getBoolKnob(k, context, k_lookat_do_ry);
    k = op->knob("look_rotate_z"       ); if (k)   getBoolKnob(k, context, k_lookat_do_rz);
    k = op->knob("look_use_point"      ); if (k)   getBoolKnob(k, context, k_lookat_use_point);
    k = op->knob("look_point"          ); if (k) { Fsr::Vec3d val; getVec3Knob(k, context, val); k_lookat_point = val; }
    k = op->knob("look_use_quaternions"); if (k)   getBoolKnob(k, context, k_lookat_method);
    k = op->knob("look_strength"       ); if (k) getDoubleKnob(k, context, k_lookat_mix);

    return true;
}


/*!
*/
Fsr::Mat4d
LookatVals::getLookatXform(const Fsr::Mat4d& parent_matrix,
                           const Fsr::Mat4d& local_matrix)
{
    /*
        bool       k_lookat_enable;     //!< Global enable
        int        k_lookat_axis;       //!< Axis to align
        bool       k_lookat_do_rx;      //!< Enable X lookat rotation
        bool       k_lookat_do_ry;      //!< Enable Y lookat rotation
        bool       k_lookat_do_rz;      //!< Enable Z lookat rotation
        bool       k_lookat_use_point;  //!< Use the user-specified point rather than the input connection
        Fsr::Vec3f k_lookat_point;      //!< User-assigned world-space lookat point
        bool       k_lookat_method;     //!< Which method to use - vectors(true) or quaternions(false)
        double     k_lookat_mix;        //!< Lookat mix
    */


std::cout << "  k_lookat_axis=" << k_lookat_axis;
std::cout << ", k_lookat_mix=" << k_lookat_mix << ", k_lookat_method=" << k_lookat_method;
std::cout << std::endl;

    const Fsr::Vec3d axisP(parent_matrix.getTranslation());
    const Fsr::Vec3d lookP(local_matrix.getTranslation());
std::cout << "        axisP=" << axisP << std::endl;
std::cout << "        lookP=" << lookP << std::endl;

    // Decompose lookat vector into ZXY rotations:
    Fsr::Vec3d look_rotations;
    lookatPoint(axisP, lookP, look_rotations);

    // In lookat mode we always use ZXY order:
    Fsr::Mat4d m;
    m.setToRotations(Fsr::ZXY_ORDER, look_rotations.asRadians());
std::cout << "          new rotations" << look_rotations << std::endl;

    return m;
#if 0
    // Also check if lookat is enabled and we can't trivially derive the translation
    // of the xform to use as a lookat source point the also output matrix:
    if (lookat_axis != NULL && !use_matrix)
    {
        // In the special (and common) case of SRT and RST xform order the translation
        // point is always the translation value, so we can simply overwrite the rotations
        // with the derived ones.
        if (xform_order == Fsr::SRT_ORDER || xform_order == Fsr::RST_ORDER)
            use_matrix = false;
        else
            use_matrix = true; // enable matrix mode for all other transform orders
std::cout << "        lookat enabled, op='" << lookat_axis->node_name() << "', use_matrix=" << use_matrix << std::endl;
    }

    // Write a matrix if 'useMatrix' (specify matrix) is enabled on the AxisOp,
    // or lookat has forced it:
    if (use_matrix)
    {
        // Use the Axis_KnobI interface to get the user-defined matrix without
        // the lookat matrix mixed in:
        DD::Image::Matrix4 m = axis_knob->axisKnob()->matrix(context);
        if (lookat_axis != NULL)
        {
            // Apply lookat transform:
            if (axis_op)
                const_cast<DD::Image::AxisOp*>(axis_op)->lookMatrixAt(context, m);
            else if (tgeo_op)
                const_cast<DD::Image::TransformGeo*>(tgeo_op)->lookMatrixAt(context, m);
        }
        matrix = m;

    }
    else
    {
        // In the special-case lookat mode where the final translation point is always
        // the translation knob value we can simply overwrite the rotations with the
        // derived lookat ones:
        if (lookat_axis != NULL)
        {
std::cout << "  look_axis=" << k_look_axis;
std::cout << ", look_rotate_x=" << k_look_rotate_x << ", look_rotate_y=" << k_look_rotate_y << ", look_rotate_z=" << k_look_rotate_z;
std::cout << ", look_strength=" << k_look_strength << ", look_use_quaternions=" << k_look_use_quaternions;
std::cout << std::endl;

            // Remap look axis enum from Nuke's to Fuser's (should do nothing...!):
            switch (look_axis)
            {
                // Stoopid DDImage LookAt class has protected enums...
                case 0/*DD::Image::LookAt::kAxisZPlus*/:  look_axis = Fsr::AXIS_Z_PLUS;  break;
                case 1/*DD::Image::LookAt::kAxisZMinus*/: look_axis = Fsr::AXIS_Z_MINUS; break;
                case 2/*DD::Image::LookAt::kAxisYPlus*/:  look_axis = Fsr::AXIS_Y_PLUS;  break;
                case 3/*DD::Image::LookAt::kAxisYMinus*/: look_axis = Fsr::AXIS_Y_MINUS; break;
                case 4/*DD::Image::LookAt::kAxisXPlus*/:  look_axis = Fsr::AXIS_X_PLUS;  break;
                case 5/*DD::Image::LookAt::kAxisXMinus*/: look_axis = Fsr::AXIS_X_MINUS; break;
            }

            // Decompose lookat vector into rotations, replacing the ones from AxisOp.
            // Build vector from point to lookat point and convert to rotations:
            const Fsr::Vec3d axisP((axis_op) ? axis_op->matrix().translation() :
                                   (tgeo_op) ? tgeo_op->matrix().translation() : Fsr::Vec3d(0.0));
            const Fsr::Vec3d lookP(lookat_axis->matrix().translation());
std::cout << "        axisP=" << axisP << std::endl;
std::cout << "        lookP=" << lookP << std::endl;
            Fsr::Vec3d look_rotations;
            Fsr::LookatVals::vectorToRotations((look_use_quaternions) ? Fsr::LookatVals::USE_QUATS :
                                                                    Fsr::LookatVals::USE_VECTORS,
                                           (lookP - axisP),
                                           look_axis,
                                           look_rotate_x,
                                           look_rotate_y,
                                           look_rotate_z,
                                           look_strength,
                                           look_rotations);
            // In lookat mode we always use ZXY order:
            rot_order = Fsr::ZXY_ORDER;
std::cout << "          new rotations" << look_rotations << std::endl;
        }

    }
#endif

}


} // namespace Fsr


// end of Fuser/AxisKnob.cpp


//
// Copyright 2019 DreamWorks Animation
//
