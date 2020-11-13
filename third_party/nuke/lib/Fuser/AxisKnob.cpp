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
#include <DDImage/Convolve.h> // for ConvolveArray

#include <sstream> // for print_name

namespace Fsr {


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


/*! Sets parent_enable true and all values to their normal defaults.
*/
AxisKnobVals::AxisKnobVals()
{
    // Enable the parent TRS knobs:
    vals.parent_enable = true;

    // Make sure axis and parent knobs are initialized:
    vals.setToDefault();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

static Fsr::Vec3d default_translate(0.0, 0.0, 0.0);
static Fsr::Vec3d    default_rotate(0.0, 0.0, 0.0);
static Fsr::Vec3d     default_scale(1.0, 1.0, 1.0);
static Fsr::Vec3d      default_skew(0.0, 0.0, 0.0);
static Fsr::Vec3d     default_pivot(0.0, 0.0, 0.0);


/*! Default ctor leaves junk in values.
*/
AxisVals::AxisVals()
{
    //
}


/*! Assigns standard default values (time arg is just to make ctor unique.)
*/
AxisVals::AxisVals(double time,
                   bool   _parent_enable) :
    parent_enable(_parent_enable)
{
    setToDefault(time);
}


/*! Extracts values from Knob axis_knob on op.
*/
AxisVals::AxisVals(const DD::Image::Op*            op,
                   const DD::Image::Knob*          axis_knob,
                   const DD::Image::OutputContext& context,
                   bool                            _parent_enable) :
    parent_enable(_parent_enable)
{
    getValsAt(op, axis_knob, context);
}


/*! Extracts values from an Axis_Knob named 'transform' on Op.
*/
AxisVals::AxisVals(const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context,
                   bool                            _parent_enable) :
    parent_enable(_parent_enable)
{
    getValsAt(op, context);
}


/*! Assigns standard default values to all params.
*/
void
AxisVals::setToDefault(double _time)
{
    time          = _time;
    xform_order   = Fsr::SRT_ORDER;
    rot_order     = Fsr::XYZ_ORDER;
    use_matrix    = false;

    setLocalXformValsToDefault();
    setParentXformValsToDefault();
}


/*! Assigns standard default values to transform params (rotate, scale, etc.)
*/
void
AxisVals::setLocalXformValsToDefault()
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
AxisVals::isLocalXformValsDefault() const
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
AxisVals::setParentXformValsToDefault()
{
    parent_translate = default_translate;
    parent_rotate    = default_rotate;
    parent_scale     = default_scale;
}
/*! Return true if xform vals are at default settings.
*/
bool
AxisVals::isParentXformValsDefault() const
{
    return !(parent_translate != default_translate ||
             parent_rotate    != default_rotate    ||
             parent_scale     != default_scale);
}


/*! Print values out.
*/
void
AxisVals::print(const char*   prefix,
                std::ostream& o) const
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(20);
    o << prefix;
    if (parent_enable)
    {
        o << "[ parent_translate" << parent_translate;
        o << ", parent_rotate"    << parent_rotate;
        o << ", parent_scale"     << parent_scale;
        o << " ] ";
    }
    o << "[ xform_order=" << xform_order;
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
    o << " ]]";
}


/*! Build a matrix from the current parent TRS values.

    Transform order is always SRT and rotation order is always XYZ.
*/
Fsr::Mat4d
AxisVals::getParentMatrix() const
{
    if (!parent_enable)
        return Fsr::Mat4d::getIdentity();

    Fsr::Mat4d m;
    m.setToTranslation(parent_translate);
    m.rotate(Fsr::XYZ_ORDER, parent_rotate.asRadians());
    m.scale(parent_scale);
    return m;
}

/*! Build a matrix from the current local TRS values.
*/
Fsr::Mat4d
AxisVals::getLocalMatrix() const
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


/*! Build a matrix from the current values and lookat params.

    Requires the world-space parent matrix to find the look rotation
    position in space.

    Uses the aim location mode in LookatVals to determine which lookat
    rotation mode to use.

    Including the pivot during Lookat means the lookat rotations are
    not centered at the the pivot location but at the final xform origin.

    This is intuitively logical to a user when manipulating scene
    objects like Axis, Cameras, etc, but doesn't make as much sense
    when applying to geometry xforms as the geometry will not rotate
    about the pivot as the geometry does not have an intuitive 'center'
    like an Axis, Camera, Light does.
*/
Fsr::Mat4d
AxisVals::getMatrixWithLookat(const LookatVals& lookat,
                              const Fsr::Mat4d& parent_matrix,
                              const Fsr::Vec3d& lookatP) const
{
    Fsr::Mat4d local = this->getLocalMatrix();

    // Handle different look aim location modes:
    switch (lookat.k_lookat_aim_location)
    {
        default:
        case LookatVals::AIM_USE_LOCAL_XFORM:
        {
            // Respect pivot rotation/scale translation offset.
            // Rotating with a pivot offset moves the nominal 'center' of
            // the xform when a rotation or scale is applied, and we want
            // the lookat rotation to be from this center and not the
            // pivot itself.
            //
            // This requires fully decomposing in world-space so we have
            // the complete xform including any pivot offsets:
            const Fsr::Mat4d world_matrix(parent_matrix * local);

            // Decompose the world xform matrix:
            Fsr::Vec3d scale0(1.0), skew0(0.0), rotate0(0.0), translate0(0.0);
            if (!world_matrix.extractSHRT(scale0,
                                          skew0,
                                          rotate0,
                                          translate0,
                                          Fsr::ZXY_ORDER))
                return local; // skip lookat if local xform is degenerate

            // Build vector from pivot point to lookat point and convert to
            // rotations angles, with blending:
            //std::cout << "  ---------------------------" << std::endl;
            //std::cout << "  rotate0=" << rotate0 << std::endl;
            //std::cout << "    local=" << local << std::endl;
            //std::cout << "    pivot=" << pivot << std::endl;
            //std::cout << "     rotP=" << world_matrix.getTranslation() << std::endl;
            //std::cout << "  lookatP=" << lookatP << std::endl;
            Fsr::RotationOrder look_rotation_order;
            if (!lookat.lookatPoint(world_matrix.getTranslation(),
                                    lookatP,
                                    rotate0,
                                    look_rotation_order))
                return local; // skip lookat if rotations can't be derived
            //std::cout << "  rotate0=" << rotate0 << std::endl;

            // Build local matrix with fixed SRT and ZXY orders:
            // TODO: should we use the suggested rotation order from the lookatPoint() method?
            local = parent_matrix.inverse();
            local.translate(translate0);
            local.rotate(Fsr::ZXY_ORDER/*look_rotation_order*/, rotate0.asRadians());
            local.skew(skew0);
            local.scale(scale0);

            return local;
        }

        case LookatVals::AIM_FROM_PIVOT:
        {
            // In this mode we do the lookat rotations & scale from the pivot
            // location. This assumes there's no nominal 'center' so it's
            // best used for geometry aim contraints.

            // Transform lookatP into parent-relative space so the rotations
            // blend in the same coordinate frame:
            const Fsr::Vec3d aimP = parent_matrix.inverse().transform(lookatP);

            // Build vector from pivot point to lookat point and convert to
            // rotations angles, with blending.
            Fsr::Vec3d         look_rotations      = rotate;
            Fsr::RotationOrder look_rotation_order = rot_order;
            if (!lookat.lookatPoint(pivot,
                                    aimP,
                                    look_rotations,
                                    look_rotation_order))
                return local; // skip lookat if rotations can't be derived

            local.setToTransform(xform_order,
                                 look_rotation_order,
                                 translate,
                                 look_rotations,
                                 totalScaling(),
                                 skew,
                                 pivot);
            return local;
        }

    }

    //return local;
}


/*!
*/
/*static*/
void
AxisVals::applyEulerFilter(Fsr::RotationOrder     target_rot_order,
                           std::vector<AxisVals>& vals,
                           bool                   sort)
{
    const size_t nSamples = vals.size();
    if (nSamples == 0)
        return; // don't bother...

    if (sort)
        std::sort(vals.begin(), vals.end());

    std::vector<Vec3d> rotation_angles(nSamples);

    // Filter local rotations:
    {
        for (size_t j=0; j < nSamples; ++j)
            rotation_angles[j] = vals[j].rotate;

        Fsr::eulerFilterRotations(rotation_angles, target_rot_order);

        for (size_t j=0; j < nSamples; ++j)
            vals[j].rotate = rotation_angles[j];
    }

    // Filter parent rotations:
    if (vals[0].parent_enable)
    {
        for (size_t j=0; j < nSamples; ++j)
            rotation_angles[j] = vals[j].parent_rotate;

        // Parent rotation is always XYZ:
        Fsr::eulerFilterRotations(rotation_angles, Fsr::XYZ_ORDER);

        for (size_t j=0; j < nSamples; ++j)
            vals[j].parent_rotate = rotation_angles[j];
    }
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
AxisVals::getValsAt(const DD::Image::Op*            op,
                    const DD::Image::Knob*          axis_knob,
                    const DD::Image::OutputContext& context)
{
    //std::cout << "      AxisVals:getValsAt()" << std::endl;
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

    // Note - the get*Knob methods use Knob::store() in an efficient manner:
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
        k = op->knob("matrix"   ); if (k)   getMat4Knob(k, context, matrix       ); else matrix.setToIdentity();
    }
    //
    if (parent_enable)
    {
        k = op->knob("parent_translate"); if (k) getVec3Knob(k, context, parent_translate); else parent_translate.set(0.0);
        k = op->knob("parent_rotate"   ); if (k) getVec3Knob(k, context, parent_rotate   ); else parent_rotate.set(0.0);
        k = op->knob("parent_scale"    ); if (k) getVec3Knob(k, context, parent_scale    ); else parent_scale.set(1.0);
    }

#if 0
    std::cout << "      AxisVals::getValsAt() knob vals:";
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
AxisVals::getValsAt(const DD::Image::Op*            op,
                    const DD::Image::OutputContext& context)
{
    //std::cout << "      AxisVals:getValsAt()" << std::endl;
    if (!op)
        return false; // don't crash...

    // Get the explicit knob vs. using the AxisOp::axisKnob() method:
    DD::Image::Knob* axis_knob = op->knob("transform");
    if (!axis_knob || !axis_knob->axisKnob())
    {
        std::cerr << "AxisVals:getValsAt(): error, no 'transform' knob on AxisOp." << std::endl;
        return false;
    }

    // Extract the local transform values at this context:
    this->getValsAt(op, axis_knob, context);

    return true;
}


/*!
*/
bool
AxisVals::extractFromMatrix(const Fsr::Mat4d&  m,
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
        enableParentXformVals(); // make sure the parent knobs are enabled
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
AxisVals::clearAnimation(DD::Image::Op*                  op,
                         const DD::Image::OutputContext& context)
{
    if (!op)
        return; // don't crash...

    const bool sync_parent_xform_knobs = getBoolValue(op->knob("sync_parent_xform"), true);
    const bool sync_local_xform_knobs  = getBoolValue(op->knob("sync_local_xform" ), true);

    if (sync_parent_xform_knobs)
    {
        DD::Image::Knob* k;
        k = op->knob("parent_translate"); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
        k = op->knob("parent_rotate"   ); if (k) { k->clear_animated(-1); k->set_value(0.0, -1); }
        k = op->knob("parent_scale"    ); if (k) { k->clear_animated(-1); k->set_value(1.0, -1); }
    }

    if (sync_local_xform_knobs)
    {
        DD::Image::Knob* k;
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
}


/*!
*/
bool
AxisVals::store(DD::Image::Op*                  op,
                const DD::Image::OutputContext& context) const
{
    //std::cout << "      AxisVals:store('" << op->node_name() << "')" << std::endl;
    if (!op)
        return false; // don't crash...

    const bool sync_parent_xform_knobs = getBoolValue(op->knob("sync_parent_xform"), true);
    const bool sync_local_xform_knobs  = getBoolValue(op->knob("sync_local_xform" ), true);

    // TODO: these enables should be on the AxisVals class, or something like it.
    const bool do_translate = getBoolValue(op->knob("translate_enable"), true);
    const bool do_rotation  = getBoolValue(op->knob("rotate_enable"   ), true);
    const bool do_scaling   = getBoolValue(op->knob("scale_enable"    ), true);

#if 1
    if (do_translate)
    {
        if (sync_parent_xform_knobs && parent_enable)
            storeVec3dInKnob(parent_translate, op->knob("parent_translate"), context, 0/*offset*/);
        if (sync_local_xform_knobs)
            storeVec3dInKnob(translate, op->knob("translate"), context, 0/*offset*/);
    }
    if (do_rotation)
    {
        if (sync_parent_xform_knobs && parent_enable)
            storeVec3dInKnob(parent_rotate, op->knob("parent_rotate"), context, 0/*offset*/);
        if (sync_local_xform_knobs)
            storeVec3dInKnob(rotate, op->knob("rotate"), context, 0/*offset*/);
    }
    if (do_scaling)
    {
        if (sync_parent_xform_knobs && parent_enable)
            storeVec3dInKnob(parent_scale, op->knob("parent_scale"), context, 0/*offset*/);
        if (sync_local_xform_knobs)
            storeVec3dInKnob(scaling, op->knob("scaling"), context, 0/*offset*/);
        //storeDoubleInKnob(uniform_scale, op->knob("uniform_scale"), context, 0/*offset*/);
    }
    //storeVec3dsInKnob(skew,  op->knob("skew" ), context, 0/*offset*/);
    //storeVec3dsInKnob(pivot, op->knob("pivot"), context, 0/*offset*/);
#else
    if (do_translate)
    {
        if (sync_parent_xform_knobs && do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_translate"), parent_translate, times, -1/*view*/);
        if (sync_local_xform_knobs)
            Fsr::storeVec3dsInKnob(op->knob("translate"), translate, times, -1/*view*/);
    }
    if (do_rotation)
    {
        if (sync_parent_xform_knobs && do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_rotate"), parent_rotate, times, -1/*view*/);
        if (sync_local_xform_knobs)
            Fsr::storeVec3dsInKnob(op->knob("rotate"), rotate, times, -1/*view*/);
    }
    if (do_scaling)
    {
        if (sync_parent_xform_knobs && do_parent_extract)
            Fsr::storeVec3dsInKnob(op->knob("parent_scale"), parent_scale, times, -1/*view*/);
        if (sync_local_xform_knobs)
            Fsr::storeVec3dsInKnob(op->knob("scaling"), scaling, times, -1/*view*/);
        //Fsr::storeDoublesInKnob(op->knob("uniform_scale"), uniform_scale, times, -1/*view*/);
    }
    //Fsr::storeVec3dsInKnob(op->knob("skew" ), skew,  times, -1/*view*/);
    //Fsr::storeVec3dsInKnob(op->knob("pivot"), pivot, times, -1/*view*/);
#endif

    return true;
}


/*!
*/
/*static*/ bool
AxisVals::store(const AxisValsList& axis_vals_list,
                DD::Image::Op*      op)
{
    if (!op)
        return false; // don't crash...

    DD::Image::OutputContext context;
    context.setView(-1);

    // Creating a KnobChangeGroup causes Nuke to batch up knobChanged messages,
    // sending only one upon destruction:
    { DD::Image::KnobChangeGroup change_group;

        AxisVals::clearAnimation(op, context);

        const size_t nSamples = axis_vals_list.size();
        if (nSamples > 0)
        {
            // Store separate xform parameters.
            for (size_t j=0; j < nSamples; ++j)
            {
                const AxisVals& axis_vals = axis_vals_list[j];
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
    kParentTranslate(NULL),
    kParentRotate(NULL),
    kParentScale(NULL),
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
                     Fsr::AxisKnobVals*       axis_knob_vals,
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
        // OutputContext and fills in 'axis_vals'. It should return
        // the same knob pointer created above for the same Op.
        k = f(DD::Image::CUSTOM_KNOB/*knob type enum*/,
              DD::Image::Custom/*datatype*/,
              axis_knob_vals,
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


/*! Add the parent TRS knobs.

    XYZ_knob always stores floats but we don't want floats, so point the knobs
    at a dummy value and later use Knob::store() to get the underlying doubles.
*/
/*static*/ void
AxisKnobWrapper::addParentTRSKnobs(DD::Image::Knob_Callback f)
{
    //std::cout << "  AxisKnobWrapper::addParentTRSKnobs(" << this << ") makeKnobs=" << f.makeKnobs() << std::endl;

    Fsr::Vec3f dflt_zero(0.0f);
    DD::Image::XYZ_knob(f, &dflt_zero.x, "parent_translate", "parent translate");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
        DD::Image::Tooltip(f, "This translate is applied prior to the local transform allowing a "
                              "parenting hierarchy to be kept separate from the local transform.\n"
                              "\n"
                              "Applied in fixed SRT transform order and XYZ rotation order.\n"
                              "\n"
                              "When loading xform node data from a scene file the node's parent "
                              "transform can be placed here.\n");
    DD::Image::XYZ_knob(f, &dflt_zero.x, "parent_rotate", "parent rotate");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
        DD::Image::Tooltip(f, "This rotate is applied prior to the local transform allowing a "
                              "parenting hierarchy to be kept separate from the local transform.\n"
                              "\n"
                              "Applied in fixed SRT transform order and XYZ rotation order.\n"
                              "\n"
                              "When loading xform node data from a scene file the node's parent "
                              "transform can be placed here.\n");
    Fsr::Vec3f dflt_one(1.0f);
    DD::Image::XYZ_knob(f, &dflt_one.x, "parent_scale", "parent scale");
        DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
        DD::Image::Tooltip(f, "This scale is applied prior to the local transform allowing a "
                              "parenting hierarchy to be kept separate from the local transform.\n"
                              "\n"
                              "Applied in fixed SRT transform order and XYZ rotation order.\n"
                              "\n"
                              "When loading xform node data from a scene file the node's parent "
                              "transform can be placed here.\n");
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
    AxisKnobVals* axis_knob_vals = reinterpret_cast<AxisKnobVals*>(p);
#if 1//DEBUG
    assert(axis_knob_vals);
    assert(type == DD::Image::Custom);
#endif

    getValsAt(context, *axis_knob_vals, &hash);
}


/*! Get AxisKnobVals filled in at the specified output context, updating
    the matrices as well.
    Updates the hash if provided.
*/
void
AxisKnobWrapper::getValsAt(const DD::Image::OutputContext& context,
                           AxisKnobVals&                   axis_knob_vals,
                           DD::Image::Hash*                hash)
{
    //std::cout << "    AxisKnobWrapper(" << name() << ")::getValsAt() frame=" << context.frame();
    //std::cout << ", view=" << context.view();

    AxisVals& vals = axis_knob_vals.vals;

    // Get the Knob pointers to the local transform knobs that were created
    // by the Axis_Knob - there is an Axis_Knob to wrap, yes?
    if (kXformOrder == NULL && kMatrix == NULL)
    {
        assert(Knob::op());
        DD::Image::Op* op = DD::Image::Knob::op()->firstOp();
        // We need all the knobs so throw an assert if any are missing:
        if (vals.parent_enable)
        {
            kParentTranslate = op->knob("parent_translate"); assert(kParentTranslate);
            kParentRotate    = op->knob("parent_rotate"   ); assert(kParentRotate);
            kParentScale     = op->knob("parent_scale"    ); assert(kParentScale);
        }
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
    {
        // Look in Convolve.h for explanation on how to use ConvolveArray store.
        // It's too bad it only supports floats, but for this purpose I think it's ok:
        float m[16];
        DD::Image::ConvolveArray ca; ca.width = ca.height = 4; ca.array = m;
        kMatrix->store(DD::Image::ConvolveArrayPtr, &ca, *hash, context);
        const float* src = m;
        double* dst = vals.matrix.array();
        for (int i=0; i < 16; ++i)
            *dst++ = double(*src++);
        vals.matrix.transpose();
        //std::cout << "class=" << kMatrix->Class() << ", matrix=" << vals.matrix<< std::endl;
        axis_knob_vals.local_matrix = vals.matrix;
    }
    else
    {
        axis_knob_vals.local_matrix = vals.getLocalMatrix();
    }

    // If parent knobs have been created store them and build the parent matrix:
    if (kParentTranslate && kParentRotate && kParentScale)
    {
        kParentTranslate->store(DD::Image::DoublePtr, vals.parent_translate.array(), *hash, context);
        kParentRotate->store(DD::Image::DoublePtr,    vals.parent_rotate.array(),    *hash, context);
        kParentScale->store(DD::Image::DoublePtr,     vals.parent_scale.array(),     *hash, context);
        
        axis_knob_vals.parent_matrix = vals.getParentMatrix();
    }
    else
    {
        vals.parent_translate.set(0.0);
        vals.parent_rotate.set(0.0);
        vals.parent_scale.set(1.0);

        axis_knob_vals.parent_matrix.setToIdentity();
    }

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
    AxisKnobVals axis_knob_vals;
    getValsAt(context, axis_knob_vals, hash);
    return (axis_knob_vals.parent_matrix * axis_knob_vals.local_matrix);
}


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


// These match the strings in DD::Image::LookAt so Enumeration_Knobs using these
// save the same thing as stock Nuke:
/*static*/ const char* const LookatVals::method_list[]        = { "vectors", "quaternions", 0 };

/*static*/ const char* const LookatVals::aim_location_modes[] = { "use-local-xform", "from-pivot", 0 };

/*!
*/
LookatVals::LookatVals()
{
    setToDefault();
}




/*! Assigns standard default values to all params.
*/
void
LookatVals::setToDefault(uint32_t aim_location_mode)
{
    k_lookat_enable       = true;
    k_lookat_axis         = (int)Fsr::AXIS_Z_MINUS;
    k_lookat_do_rx        = true;
    k_lookat_do_ry        = true;
    k_lookat_do_rz        = true;
    k_lookat_use_point    = false;
    k_lookat_point.set(0.0, 0.0, 0.0);
    k_lookat_method       = USE_VECTORS;
    k_lookat_aim_location = aim_location_mode;
    k_lookat_mix          = 1.0f;
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
    DD::Image::Enumeration_knob(f, &k_lookat_aim_location, aim_location_modes, "look_aim_location", "");
        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
        DD::Image::Tooltip(f, "How to place the aim rotation point:\n"
                              "\n"
                              "<b>use-local-xform</b>: Aim location is placed at the origin of the "
                              "local transform <b>after</b> the TRS controls are applied. This is best "
                              "for scene objects like Cameras, Axis, and Lights which have an obvious "
                              "'origin' point which is the intuitive rotation location.\n"
                              "\n"
                              "<b>from-pivot</b>: Aim location is always the pivot location and the "
                              "local rotation controls do not affect it. This mode is best for "
                              "orienting geometry which may not have an obvious origin location to "
                              "rotate about. Setting the pivot location to the center of the geometry "
                              "bounding-box is often best and may need to be animated.\n");
    DD::Image::Bool_knob(f, &k_lookat_do_rx, "look_rotate_x", "x rot");
    DD::Image::Bool_knob(f, &k_lookat_do_ry, "look_rotate_y", "y rot");
    DD::Image::Bool_knob(f, &k_lookat_do_rz, "look_rotate_z", "z rot");
    DD::Image::Bool_knob(f, &k_lookat_method, "look_use_quaternions", "use quaternions");
// TODO: fix the quaternion look function and re-enable:
DD::Image::SetFlags(f, DD::Image::Knob::DISABLED);
    //DD::Image::Enumeration_knob(f, &k_lookat_method, method_list, "look_use_quaternions", "method");

    //---------------------------------------------------
    // TODO: enable these new lookat point controls

    DD::Image::Bool_knob(f, &k_lookat_use_point, "look_use_point", "");
DD::Image::SetFlags(f, DD::Image::Knob::INVISIBLE);
//        DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE);
//        DD::Image::Tooltip(f, "Use a user-specified point to look at, ignoring the lookat input connection.");
    DD::Image::XYZ_knob(f, k_lookat_point.array(), "lookat_point", "lookat point");
DD::Image::SetFlags(f, DD::Image::Knob::INVISIBLE);
//        DD::Image::ClearFlags(f, DD::Image::Knob::STARTLINE);
//        DD::Image::Tooltip(f, "User-specified point to look at, ignoring the lookat input connection.");

    //---------------------------------------------------

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
    hash.append(k_lookat_aim_location);
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
// TODO: fix the quaternion look function and re-enable:
//    k = op->knob("look_use_quaternions"); if (k) k->enable(lookat_enabled);
    k = op->knob("look_aim_location"   ); if (k) k->enable(lookat_enabled);
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
    k = op->knob("look_aim_location"   ); if (k)    getIntKnob(k, context, k_lookat_aim_location);
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
std::cout << ", k_lookat_mix=" << k_lookat_mix;
std::cout << ", k_lookat_method=" << k_lookat_method;
std::cout << ", k_lookat_aim_location=" << k_lookat_aim_location;
std::cout << std::endl;

    const Fsr::Vec3d axisP(parent_matrix.getTranslation());
    const Fsr::Vec3d lookP(local_matrix.getTranslation());
std::cout << "        axisP=" << axisP << std::endl;
std::cout << "        lookP=" << lookP << std::endl;

    // Decompose lookat vector into ZXY rotations:
    Fsr::Vec3d         look_rotations;
    Fsr::RotationOrder look_rotation_order;
    lookatPoint(axisP, lookP, look_rotations, look_rotation_order);

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
