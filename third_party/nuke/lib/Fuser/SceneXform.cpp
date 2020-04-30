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

/// @file Fuser/FuserSceneXform.cpp
///
/// @author Jonathan Egstad

#include "SceneXform.h"
#include "NukeKnobInterface.h" // for getVec3Knob
#include "AxisOp.h"
#include "CameraOp.h"
#include "LightOp.h"

#include <DDImage/ViewerContext.h> // for display3d_names_source string list
#include <DDImage/ArrayKnobI.h> // for ArrayKnobI::ValueProvider

// Do we want to expose parenting scale?
//#define ENABLE_PARENT_SCALE 1


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

namespace Fsr {


/*! 'class WorldMatrixProvider' is declared in DDImage::AxisOp.h as an opaque
    pointer and is likely implemented inside the AxisOp class, but since we
    can't see its implementation from outside we need to reimplement it here...

    This may fail miserably but we'll attempt to override the one in the
    AxisOp base class since the pointer is exposed as protected. From checking
    the value of that pointer it appears to be set during the AxisOp::knobs()
    method, so we'll do the same in SceneXform::_addAxisOpKnobs().

    At least it's a double-precision interface so we can provide values
    from a Fsr::Mat4d!  :)
*/
class WorldMatrixProvider : public DD::Image::ArrayKnobI::ValueProvider
{
  protected:
    Fsr::SceneXform*      m_xform;
    Fsr::AxisKnobWrapper* m_xform_knob;


  public:
    //!
    WorldMatrixProvider(Fsr::SceneXform*      xform,
                        Fsr::AxisKnobWrapper* xform_knob) :
        m_xform(xform),
        m_xform_knob(xform_knob)
    {
        //
    }

    /*! This function should return true if the knob is presently an output knob.
        This allows this functionality to be toggled on/off without (with a bool knob for example)
        without having to call ->setValueProvider() after the initial setup.
    */
    /*virtual*/ bool provideValuesEnabled(const DD::Image::ArrayKnobI*,
                                          const DD::Image::OutputContext&) const { return true; }

    /*! This function should return the values to be displayed in the output knob.
        The vector must match the array_size() of the Array_Knob.
    */
    /*virtual*/ std::vector<double> provideValues(const DD::Image::ArrayKnobI*,
                                                  const DD::Image::OutputContext& context) const
    {
        // Get the concatenated world matrix at the OutputContext:
        assert(m_xform);
        const Fsr::Mat4d m = m_xform->getWorldTransformAt(context);

        // Swap row/column order:
        std::vector<double> array(16);
        for (int i=0; i < 16; ++i)
            array[i] = m[i%4][i/4];
        return array;
    }

};


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

// TODO: putting these implementations here for now:


// These match the order of enums in Fsr::Mat4 and Fsr::Vec3.
// The strings defined also match the corresponding string arrays in DD::Image
// so that Enumeration_Knobs using these save the thing as stock Nuke:
/*extern*/ const char* xform_orders[]    = { "SRT", "STR", "RST", "RTS", "TSR", "TRS", 0 };
/*extern*/ const char* axis_directions[] = {  "-X",  "+X",  "-Y",  "+Y",  "-Z",  "+Z", 0 };
/*extern*/ const char* rotation_orders[] = { "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX", 0 };


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


#ifdef FUSER_USE_KNOB_RTTI
const char* SceneXformRTTIKnob = "FsrSceneXform";
#endif


/*!
*/
SceneXform::SceneXform() :
    SceneOpExtender(),
    kParentTranslate(NULL),
    kParentRotate(NULL),
    kParentScale(NULL),
    kAxisKnob(NULL),
    kFsrAxisKnob(NULL)
{
    //
    //std::cout << "SceneXform::ctor(" << this << ")" << std::endl;
}


/*! Returns true if Op is a Fuser SceneXform.

    For a statically-linked Fuser lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/
bool
SceneXform::isSceneXform(DD::Image::Op* op)
{
#ifdef FUSER_USE_KNOB_RTTI
    // HACK!!!!: Test for dummy knob so we can test for class without using RTTI...:
    return (op && op->knob(SceneXformRTTIKnob) != NULL);
#else
    // TODO: this probably does not work due to multiple-inheritance...:
    return (dynamic_cast<SceneXform*>(op) != NULL);
#endif
}


/*! Returns op cast to Fuser SceneXform if possible, otherwise NULL.

    For a statically-linked Fuser lib this is a hack - we test for a
    dummy knob so we can test the class without using RTTI which
    fails when dso plugins are statically linked to this class.
*/
/*static*/
SceneXform*
SceneXform::asSceneXform(DD::Image::Op* op)
{
    if (!op || !isSceneXform(op))
        return NULL;

    //-----------------------------------------------------------------------------
    // This crap is required to get around the wonky way we're multiple-inheriting
    // from DD::Image::AxisOp/CameraOp/LightOp and the SceneXform classes.
    // We can't static_cast directly to a SceneXform as we need the fundamental
    // class first, ie FuserAxisOp/FuserCameraOp/FuserLightOp:
    //-----------------------------------------------------------------------------
    FuserAxisOp* axis = FuserAxisOp::asFuserAxisOp(op);
    if (axis)
        return static_cast<Fsr::SceneXform*>(axis);

    FuserCameraOp* camera = FuserCameraOp::asFuserCameraOp(op);
    if (camera)
        return static_cast<Fsr::SceneXform*>(camera);

    FuserLightOp* light = FuserLightOp::asFuserLightOp(op);
    if (light)
        return static_cast<Fsr::SceneXform*>(light);

    return NULL;
}


//!
/*virtual*/
int
SceneXform::xformInputs() const
{
    const int parent_input = parentingInput();
    const int lookat_input = lookatInput();

    // Add parent and lookat inputs if valid:
    int inputs = (parent_input >= 0)?1:0;
    if (lookat_input >= 0 && lookat_input != parent_input)
        ++inputs;

    return inputs;
}


//!
/*virtual*/
bool
SceneXform::testInput(int             input,
                       DD::Image::Op* op) const
{
    const int parent_input = parentingInput();
    const int lookat_input = lookatInput();

    if (input == parent_input)
    {
        // Allow *only* AxisOp connections on parent input:
        DD::Image::AxisOp* axis = dynamic_cast<DD::Image::AxisOp*>(op);
        if (axis)
            return true;
    }
    else if (input == lookat_input)
    {
        // Lookat input only supports AxisOps for now:
        DD::Image::AxisOp* axis = dynamic_cast<DD::Image::AxisOp*>(op);
        if (axis)
            return true;

        // TODO: allow lookat to support objects in GeometryList!
        //DD::Image::GeoOp* geo = dynamic_cast<DD::Image::GeoOp*>(op);
        //if (geo)
        //    return true;
    }

    return false;
}


/*! Return the Op to connect to this input if the arrow is disconnected in Nuke,
    or if the Op Nuke tries fails the test_input() test.
*/
/*virtual*/
DD::Image::Op*
SceneXform::defaultInput(int input) const
{
    const int parent_input = parentingInput();
    const int lookat_input = lookatInput();

    if (input == parent_input)
        return NULL; // allow null Op on parent input
    else if (input == lookat_input)
        return NULL; // allow null Op on lookat input

    return NULL;
}


//!
/*virtual*/
const char* 
SceneXform::inputLabel(int   input,
                       char* buffer) const
{
    const int parent_input = parentingInput();
    const int lookat_input = lookatInput();

    if (input == parent_input)
        return "axis";
    else if (input == lookat_input)
        return "look";

    return buffer;
}


//------------------------------------------------------------------------------------


/*! Call this from owner (FuserAxisOp, FuserCameraOp, FuserLightOp)::knobs() to
    replace the AxisKnob knobs.
    Adds the local transform knobs matching the typical AxisKnob ones.
    This is valid as of Nuke 12.
*/
void
SceneXform::_addOpTransformKnobs(DD::Image::Knob_Callback f,
                                 DD::Image::Matrix4*      localtransform)
{
    assert(localtransform);

    // Add the stock single-precision Axis_knob. We still need to create it since there's
    // internal Nuke logic that crashes if this doesn't exist on the Op. The Axis_knob
    // creates the child knobs 'translate', 'rotate', 'scaling', etc:
    kAxisKnob = DD::Image::Axis_knob(f, localtransform, "transform", NULL/*label*/);

    // Add our double-precision Fsr::AxisKnob which calculates a parallel double-precision
    // matrix from the same child knobs as the stock Axis_knob. This relies on the
    // DD::Image::Axis_knob macro being called out separately:
    kFsrAxisKnob = static_cast<Fsr::AxisKnobWrapper*>(Fsr::AxisKnobWrapper_knob(f, &m_xform_matrix, SceneXformRTTIKnob));
}


/*! Call this from owner (FuserAxisOp, FuserCameraOp, FuserLightOp)::knobs() to
    replace the AxisOp baseclass' knobs() implementation.

    Adds the local transform knobs matching the AxisOp base class.

    If the AxisOp class gets additional knob vars added in newer Nuke versions
    this will need to be updated! This is valid as of Nuke 11.3.

    In AxisOp.h:
        Matrix4 localtransform_;    //!< Local matrix that Axis_Knob fills in
        Matrix4 local_;             //!< Local matrix after look at performed
        Matrix4 matrix_;            //!< Object matrix - local&parent
        Matrix4 imatrix_;           //!< Inverse object matrix
        bool    inversion_updated;  //!< Whether imatrix_ is valid

        Axis_KnobI* axis_knob;      //!< reference to the transformation knob

        WorldMatrixProvider* _worldMatrixProvider;
*/
void
SceneXform::_addAxisOpTransformKnobs(DD::Image::Knob_Callback         f,
                                     DD::Image::Matrix4*              localtransform,
                                     DD::Image::Axis_KnobI**          axis_knob,
                                     DD::Image::WorldMatrixProvider** worldMatrixProvider)
{
    assert(localtransform);
    assert(axis_knob);
    assert(worldMatrixProvider);

    _addOpTransformKnobs(f, localtransform);

    // Assign the Axis_KnobI interface pointer on the AxisOp base class:
    if (f.makeKnobs())
        *axis_knob = kAxisKnob->axisKnob();

    DD::Image::BeginGroup(f, "", "World matrix");
    {
        DD::Image::SetFlags(f, DD::Image::Knob::CLOSED);

        // Create the world matrix output array - these are doubles internally!
        DD::Image::Knob* kWorldMatrixKnob = DD::Image::Array_knob(f, NULL/*array*/, 4/*w*/, 4/*h*/, "world_matrix", "");
            DD::Image::SetFlags(f, DD::Image::Knob::STARTLINE | DD::Image::Knob::DO_NOT_WRITE);
            DD::Image::Tooltip(f, "The world matrix is read-only and allows you to see and expression link "
                                  "to the completely concatenated (world) matrix of this op.");
            DD::Image::SetFlags(f, DD::Image::Knob::ENDLINE);

        // Create the output value provider for the matrix knob.
        // Only override base class with our Fsr::Mat4d provider class if
        // it's not already assigned to avoid any possible conflict/crash:
        if (kWorldMatrixKnob && f.makeKnobs() && *worldMatrixProvider == NULL)
        {
            Fsr::WorldMatrixProvider* wmp = new WorldMatrixProvider(this, kFsrAxisKnob);
            assert(wmp);

            // Assign the value provider pointer on the AxisOp base class:
            DD::Image::ArrayKnobI* array = dynamic_cast<DD::Image::ArrayKnobI*>(kWorldMatrixKnob);
            assert(array);
            array->setValueProvider(wmp);

            // Store into AxisOp cast to DD::Image::WorldMatrixProvider - hopefully this works!
            *worldMatrixProvider = reinterpret_cast<DD::Image::WorldMatrixProvider*>(wmp);
        }

    }
    DD::Image::EndGroup(f);
}


//------------------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
SceneXform::addParentingKnobs(DD::Image::Knob_Callback f,
                              bool                     group_open)
{
    //std::cout << "  SceneXform::addParentingKnobs(" << this << ") makeKnobs=" << f.makeKnobs() << std::endl;

    //DD::Image::Divider(f, "Parent constraint");
    //----------------------------------------
    //DD::Image::BeginGroup(f, "", "@b;Parent constraint");
    {
        //if (group_open)
        //    DD::Image::ClearFlags(f, DD::Image::Knob::CLOSED);
        //else
        //    DD::Image::SetFlags(f, DD::Image::Knob::CLOSED);

        // XYZ_knob is always floats but we don't want to store floats, so
        // point the knobs at a dummy value and later use Knob::store() to get
        // the underlying doubles:
        DD::Image::Vector3 dflt(0.0f, 0.0f, 0.0f);
        kParentTranslate = DD::Image::XYZ_knob(f, &dflt.x, "parent_translate", "parent translate");
            DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
            DD::Image::Tooltip(f, "This translate is applied prior to the local transform allowing a "
                                  "parenting hierarchy to be kept separate from the local transform.\n"
                                  "\n"
                                  "When loading xform node data from a scene file the node's parent "
                                  "transform can be placed here.\n");
        kParentRotate = DD::Image::XYZ_knob(f, &dflt.x, "parent_rotate", "parent rotate");
            DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
            DD::Image::Tooltip(f, "This rotate is applied prior to the local transform allowing a "
                                  "parenting hierarchy to be kept separate from the local transform.\n"
                                  "\n"
                                  "When loading xform node data from a scene file the node's parent "
                                  "transform can be placed here.\n");
#ifdef ENABLE_PARENT_SCALE
        kParentScale = DD::Image::XYZ_knob(f, &dflt.x, "parent_scale", "parent scale");
            DD::Image::SetFlags(f, DD::Image::Knob::NO_HANDLES);
            DD::Image::Tooltip(f, "This scale is applied prior to the local transform allowing a "
                                  "parenting hierarchy to be kept separate from the local transform.\n"
                                  "\n"
                                  "When loading xform node data from a scene file the node's parent "
                                  "transform can be placed here.\n");
#else
        // Create a dummy knob so that scripts load without failure and the scene loaders don't fail.
        // But since we're not setting kParentScale the transform code will not fail either.
        DD::Image::XYZ_knob(f, &dflt.x, "parent_scale", DD::Image::INVISIBLE);
            DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE | DD::Image::Knob::NO_ANIMATION | DD::Image::Knob::NO_RERENDER);
#endif
    }
    //DD::Image::EndGroup(f);
    //----------------------------------------

#if 0
    //DD::Image::Bool_knob(f, &k_parent_xform_enable, "parent_transform_enable", "parent enable");
    //    DD::Image::SetFlags(f, DD::Image::Knob::EARLY_STORE);
    //    DD::Image::Tooltip(f, "If enabled this transform is applied prior to the local transform, "
    //                          "allowing the parenting hierarchy to be kept separate from the local "
    //                          "transform.\n"
    //                          "\n"
    //                          "When loading xform node data from a scene file the node's parent transform "
    //                          "is placed here when this is enabled. If not enabled the parent transform "
    //                          "is combined with the node's local transform.\n");
    //DD::Image::Bool_knob(f, &k_parent_xform_live, "parent_transform_live", "update from input");
    //    DD::Image::SetFlags(f, DD::Image::Knob::EARLY_STORE);
    //    DD::Image::Tooltip(f, "If enabled the parent transform knobs are updated live from the current parenting "
    //                          "connection of this node.\n");
#endif
}


/*!
*/
/*virtual*/
void
SceneXform::addLookatKnobs(DD::Image::Knob_Callback f)
{
    //std::cout << "  SceneXform::addLookatKnobs() makeKnobs=" << f.makeKnobs() << std::endl;

    //DD::Image::Divider(f, "@b;Aim (Look) Constraint");
    //----------------------------------------
    DD::Image::BeginGroup(f, "lookat", "@b;Aim (Look) Constraint");
    {
        DD::Image::SetFlags(f, DD::Image::Knob::CLOSED);
        m_lookat.addLookatKnobs(f, "aim_constraint"/*label*/);
    }
    DD::Image::EndGroup(f);
    //----------------------------------------
}


//------------------------------------------------------------------------------------


/*!
*/
/*virtual*/
int
SceneXform::knobChanged(DD::Image::Knob* k,
                        int              call_again)
{
    //std::cout << "SceneXform::knobChanged('" << k->name() << "')" << std::endl;
    DD::Image::Op* op = sceneOp();
#ifdef DEBUG
    assert(op);
#endif

#if 0
    if (k == &DD::Image::Knob::showPanel)
    {
        const int parent_input = parentingInput();
        if (parent_input < 0)
            enableParentTransformKnobs(true);
        else
            enableParentTransformKnobs(dynamic_cast<DD::Image::AxisOp*>(op->input(parent_input)));

        //enableLocalTransformKnobs(k_scene.read_enabled);

        call_again = 1; // we want to be called again

    }
    else if (k == &DD::Image::Knob::inputChange)
    {
        // Note - this only gets called if the panel is open, don't use
        // it to check for a general input change:
        //std::cout << "inputChanged input0=" << op->input(0) << std::endl;
        enableParentTransformKnobs(k_parent_xform_enable);

        call_again = 1; // we want to be called again

    }
#endif

    int ca = m_lookat.knobChanged(op, k);
    if (ca)
        return ca;

    return call_again;
}


/*!
*/
/*virtual*/
void
SceneXform::enableParentTransformKnobs(bool parent_xform_enabled)
{
    //DD::Image::Op* op = sceneOp();
#ifdef DEBUG
    //assert(op);
#endif
    //DD::Image::Knob* k; k = op->knob("parent_transform_live"); if (k) k->enable(parent_xform_enabled);

    if (kParentTranslate) kParentTranslate->enable(parent_xform_enabled);
    if (kParentRotate   ) kParentRotate->enable(parent_xform_enabled);
    if (kParentScale    ) kParentScale->enable(parent_xform_enabled);
}

/*!
*/
/*virtual*/
void
SceneXform::enableLocalTransformKnobs(bool read_enabled)
{
    DD::Image::Op* op = sceneOp();
#ifdef DEBUG
    assert(op);
#endif
    DD::Image::Knob* k;

    // turn on local controls if not reading from file:
    const bool local_enabled = (!read_enabled);

    k = op->knob("transform"    ); if (k) k->enable(local_enabled);

    k = op->knob("uniform_scale"); if (k) k->visible(local_enabled);
    k = op->knob("skew"         ); if (k) k->visible(local_enabled);
    k = op->knob("pivot"        ); if (k) k->visible(local_enabled);
}


/*!
*/
/*virtual*/
void
SceneXform::enableSceneXformExtraKnobs(bool read_enabled)
{
    // base class does nothing
}


//------------------------------------------------------------------------------------


/*! Call this from owner (AxisOp-subclass)::_validate() to replace the AxisOp
    baseclass' _validate() implementation.

    Builds the double-precision matrices replacing the stock single-precision ones,
    then saves that result in the single-precision ones so that built in code still
    works. Since the concatenation of the world matrix is done in double-precision
    there's a better chance that the final single-precision ones aren't as badly
    degraded. Any code that knows about the Fsr::SceneXform class can get direct
    access to the double-precision ones.

    If the AxisOp class gets additional transform vars added in newer Nuke versions
    this will need to be updated! This is valid as of Nuke 11.3.

    In AxisOp.h:
        Matrix4 localtransform_;    //!< Local matrix that Axis_Knob fills in
        Matrix4 local_;             //!< Local matrix after look at performed
        Matrix4 matrix_;            //!< Object matrix - local&parent
        Matrix4 imatrix_;           //!< Inverse object matrix
        bool    inversion_updated;  //!< Whether imatrix_ is valid
*/
void
SceneXform::_validateAxisOpMatrices(bool                for_real,
                                    DD::Image::Matrix4* localtransform,
                                    DD::Image::Matrix4* local,
                                    DD::Image::Matrix4* matrix,
                                    bool*               inversion_updated)
{
#if 1//def DEBUG
    assert(localtransform);
    assert(local);
    assert(matrix);
    assert(inversion_updated);
#endif

    DD::Image::Op* op = const_cast<SceneXform*>(this)->sceneOp();
#ifdef DEBUG
    assert(op);
#endif
    if (op->node_disabled())
        return; // don't bother...

    //std::cout << "-------------------------------------------------------" << std::endl;
    //std::cout << "SceneXform('" << op->node_name() << "' " << this << ")::_validateAxisOpMatrices()";
    //std::cout << " frame=" << op->outputContext().frame() << ", view=" << op->outputContext().view() << std::endl;

    // This logic is also implemented in getInputParentTransformAt(), but this
    // one assumes the input Op has the same OutputContext:
    const int parent_input = parentingInput();
    if (parent_input < 0)
    {
        // Locally defined parent source
        m_input_matrix.setToIdentity();
        // TODO: what to do here? Likely need to call a virtual function on subclass.
    }
    else
    {
        DD::Image::AxisOp* parent_axis = dynamic_cast<DD::Image::AxisOp*>(op->input(parent_input));
        if (parent_axis)
        {
            parent_axis->validate(for_real);
            const SceneXform* input_xform = asSceneXform(parent_axis);
            //std::cout << "  parent='" << op->input(parent_input)->node_name() << "' input_xform=" << input_xform << std::endl;
            if (input_xform)
                m_input_matrix = input_xform->getWorldTransform();
            else
                m_input_matrix = Fsr::Mat4d(parent_axis->matrix()); // single-precision parent
        }
        else
            m_input_matrix.setToIdentity();
    }

    m_world_matrix = m_input_matrix;

    // Extract the local transform from the Axis_Knob knobs, build the parent
    // transform and lookat rotations, then produce double-precision matrices
    // from the lot to use:
    m_parent_matrix = getParentConstraintTransformAt(op->outputContext());
    m_world_matrix *= m_parent_matrix;

    // ** m_xform_matrix is filled in by AxisKnobWrapper **

    // Lookat function uses the local xform to get the world-space P
    // locale, then builds a translation + rotation matrix:
    m_local_matrix = getLocalTransformWithLookatAt(m_world_matrix, op->outputContext());
    m_world_matrix *= m_local_matrix;

    // Update the single-precision matrices in the AxisOp base class:
    m_xform_matrix.toDDImage(*localtransform); // overwrite AxisKnob...?
    m_local_matrix.toDDImage(*local); // (with lookat)
    m_world_matrix.toDDImage(*matrix);
    *inversion_updated = false; // invalidate the inverted matrix.

    //std::cout << "    input_matrix" << m_input_matrix  << std::endl;
    //std::cout << "   parent_matrix" << m_parent_matrix << std::endl;
    //std::cout << "    xform_matrix" << m_xform_matrix  << std::endl;
    //std::cout << "    local_matrix" << m_local_matrix  << std::endl;
    //std::cout << "    world_matrix" << m_world_matrix  << std::endl;
    //std::cout << " localtransform_" << *localtransform << std::endl;
    //std::cout << "          local_" << *local << std::endl;
    //std::cout << "         matrix_" << *matrix << std::endl;
    //std::cout << "         inversion_updated=" << *inversion_updated << std::endl;

}


/*! Builds the input transform matrix at a specific OutputContext.
    Will be identity if no input.
*/
/*virtual*/
Fsr::Mat4d
SceneXform::getInputParentTransformAt(const DD::Image::OutputContext& context) const
{
    //std::cout << "  SceneXform('" << const_cast<SceneXform*>(this)->sceneOp()->node_name() << "' " << this << ")::getInputParentTransformAt()" << std::endl;

    const int parent_input = parentingInput();
    if (parent_input < 0)
    {
        // Locally defined parent source:
        // TODO: what to do here? I assume we should call a virtual method to
        // get matrix from subclass.
        return Fsr::Mat4d::getIdentity();
    }

    DD::Image::Op* op = const_cast<SceneXform*>(this)->sceneOp();
#ifdef DEBUG
    assert(op);
#endif

    DD::Image::AxisOp* parent_axis = dynamic_cast<DD::Image::AxisOp*>(op->input(parent_input));
    if (parent_axis)
    {
        parent_axis->validate(false);
        const SceneXform* input_xform = asSceneXform(parent_axis);
        //std::cout << "    parent='" << parent_axis->node_name() << "' input_xform=" << input_xform << std::endl;
        // Check if input is a SceneXform and access the double-precision methods:
        if (input_xform)
            return input_xform->getWorldTransformAt(context);

        // Single-precision parent:
        DD::Image::Matrix4 m; parent_axis->matrixAt(context, m);
        return Fsr::Mat4d(m);
    }

    return Fsr::Mat4d::getIdentity();
}


/*! Builds the local parent transform matrix from the parent knobs
    at a specific OutputContext.
*/
/*virtual*/
Fsr::Mat4d
SceneXform::getParentConstraintTransformAt(const DD::Image::OutputContext& context) const
{
    //std::cout << "  SceneXform('" << const_cast<SceneXform*>(this)->sceneOp()->node_name() << "' " << this << ")::getParentConstraintTransformAt()" << std::endl;

    Fsr::Mat4d m;
    m.setToIdentity();

    // Transform order is always SRT for parent contraint:
    if (kParentTranslate)
    {
        Fsr::Vec3d translate(0.0);
        getVec3Knob(kParentTranslate, context, translate);
        m.translate(translate);
    }
    if (kParentRotate)
    {
        // Rotation order is always XYZ for parent contraint:
        Fsr::Vec3d rotate(0.0);
        getVec3Knob(kParentRotate, context, rotate);
        m.rotate(Fsr::XYZ_ORDER, rotate.asRadians());
    }
    if (kParentScale)
    {
        Fsr::Vec3d scale(1.0);
        getVec3Knob(kParentScale, context, scale);
        m.scale(scale);
    }

    return m;
}


/*! Builds the local 'xform' transform matrix from the AxisKnobWrapper knobs
    at a specific OutputContext.
    Does not include lookat rotations. 
*/
/*virtual*/
Fsr::Mat4d
SceneXform::getLocalTransformAt(const DD::Image::OutputContext& context) const
{
    //std::cout << "  SceneXform('" << const_cast<SceneXform*>(this)->sceneOp()->node_name() << "' " << this << ")::getLocalTransformAt()" << std::endl;
    DD::Image::Op* op = const_cast<SceneXform*>(this)->sceneOp();
#ifdef DEBUG
    assert(op);
#endif
    if (op->node_disabled())
        return Fsr::Mat4d::getIdentity(); // don't bother...

    // Just in case, shouldn't happen but don't crash...
    if (!kFsrAxisKnob)
    {
        std::cerr << "SceneXform('" << op->node_name() << "' " << this << ")::getLocalTransformAt()";
        std::cerr << " warning, kFsrAxisKnob is NULL, likely due to a coding error." << std::endl;
        return Fsr::Mat4d::getIdentity();
    }

    return kFsrAxisKnob->getMatrixAt(context);
}


/*! Build the local xform with lookat applied at a specific OutputContext.
    Requires world transform up to local matrix (ie the parent xform) to find vector origin.

    It first resolves the local xform to find the translation of the matrix then calculates
    the lookat rotations from that to the lookat point which may come from the input
    connection or user value.

    Make sure the OutputContext matches the one used to get the parent matrix!
*/
/*virtual*/
Fsr::Mat4d
SceneXform::getLocalTransformWithLookatAt(const Fsr::Mat4d&               parent_matrix,
                                          const DD::Image::OutputContext& context,
                                          Fsr::Mat4d*                     xform_matrix) const
{
    //std::cout << "SceneXform('" << const_cast<SceneXform*>(this)->sceneOp()->node_name() << "' " << this << ")::getLookatTransformAt()" << std::endl;
    DD::Image::Op* op = const_cast<SceneXform*>(this)->sceneOp();
#ifdef DEBUG
    assert(op);
#endif
    if (op->node_disabled())
    {
        if (xform_matrix)
            xform_matrix->setToIdentity();
        return Fsr::Mat4d::getIdentity(); // don't bother...
    }

    // Just in case, shouldn't happen but don't crash...
    if (!kFsrAxisKnob)
    {
        std::cerr << "SceneXform('" << op->node_name() << "' " << this << ")::getLocalTransformWithLookatAt()";
        std::cerr << " warning, kFsrAxisKnob is NULL, likely due to a coding error." << std::endl;
        if (xform_matrix)
            xform_matrix->setToIdentity();
        return Fsr::Mat4d::getIdentity();
    }

    Fsr::AxisKnobVals axis_vals;
    kFsrAxisKnob->getValsAt(context, axis_vals);

    Fsr::Mat4d local_matrix = axis_vals.getMatrix();
    if (xform_matrix)
        *xform_matrix = local_matrix;

    // Get lookat knob values at context:
    const LookatVals lookvals(op, context);
    if (!lookvals.k_lookat_enable)
        return local_matrix;

    // Need our world-space position to include parent:
    const Fsr::Mat4d world_matrix(parent_matrix * local_matrix);

    Fsr::Vec3d lookP;
    bool have_lookat_position = false;
    const int lookat_input = this->lookatInput();
    if (lookat_input < 0)
    {
        // Locally defined lookat source:
        //have_lookat_position = getLookatPoint(lookP);
    }
    else
    {
        // Get the position of the input AxisOp:
        // TODO: support lookat connections to GeometryList objects
        DD::Image::AxisOp* lookat_axis = dynamic_cast<DD::Image::AxisOp*>(op->input(lookat_input));
        if (lookat_axis)
        {
            lookat_axis->validate(false);
            const SceneXform* lookat_xform = asSceneXform(lookat_axis);
            //std::cout << "    lookat='" << lookat_axis->node_name() << "' lookat_xform=" << lookat_xform << std::endl;
            // Check if input is a SceneXform and access the double-precision methods:
            if (lookat_xform)
            {
                lookP = lookat_xform->getWorldTransformAt(context).getTranslation();
            }
            else
            {
                // Single-precision parent:
                DD::Image::Matrix4 m; lookat_axis->matrixAt(context, m);
                lookP = m.translation();
            }
            have_lookat_position = true;
        }
    }

    if (have_lookat_position)
    {
        const Fsr::Vec3d localP(local_matrix.getTranslation());
        const Fsr::Vec3d xformP(world_matrix.getTranslation());
        //std::cout << "-------------------------------------------" << std::endl;
        //std::cout << "   parent_matrix" << parent_matrix  << std::endl;
        //std::cout << "    local_matrix" << local_matrix  << std::endl;
        //std::cout << "    world_matrix" << world_matrix  << std::endl;
        //std::cout << "          localP" << localP << std::endl;
        //std::cout << "          xformP" << xformP << std::endl;
        //std::cout << "           lookP" << lookP << std::endl;

        // Build nominal aim vectors where +Z pointing towards lookP:
        double rotZ = 0.0;
        Fsr::Vec3d rX, rY, rZ;
        rY.set(-std::sin(rotZ), std::cos(rotZ), 0.0); rY.normalize(); // 'up' vector
        rZ = (lookP - xformP); rZ.normalize(); // normal
        rX = rY.cross(rZ);     rX.normalize(); // tangent
        rY = rZ.cross(rX);     rY.normalize(); // recalc bitangent

        // Build a rotation matrix with the aim vectors, swapping them around
        // to fit desired aim axis:
        Fsr::Mat4d aim_matrix;
        aim_matrix.setToTranslation(xformP);
        switch (lookvals.k_lookat_axis)
        {
            case AXIS_X_MINUS: aim_matrix.setXYZAxis(-rZ, rY, rX); break;
            case AXIS_X_PLUS:  aim_matrix.setXYZAxis( rZ, rY,-rX); break;
            //
            case AXIS_Y_MINUS: aim_matrix.setXYZAxis( rX,-rZ, rY); break;
            case AXIS_Y_PLUS:  aim_matrix.setXYZAxis( rX, rZ,-rY); break;
            //
            case AXIS_Z_MINUS: aim_matrix.setXYZAxis(-rX, rY,-rZ); break;
            case AXIS_Z_PLUS:  aim_matrix.setXYZAxis( rX, rY, rZ); break;
        }

        // Remove the parent xform from the resulting aim matrix and
        // decompose to rotations:
        aim_matrix = parent_matrix.inverse()*aim_matrix;

        // Interpolate between local rotation and aim rotations:

        //---------------------------------------------------------------
        // TODO: This method of interpoation doesn't work right...
        // I'm missing something w/respect to scaling...
        Fsr::Vec3d scale, shear;
        local_matrix.extractScalingAndShear(scale, shear);

        const double t = Fsr::clamp(lookvals.k_lookat_mix);
        Fsr::Vec3d axisX(Fsr::lerp(local_matrix.getXAxis(), aim_matrix.getXAxis(), t*lookvals.k_lookat_do_rx));
        Fsr::Vec3d axisY(Fsr::lerp(local_matrix.getYAxis(), aim_matrix.getYAxis(), t*lookvals.k_lookat_do_ry));
        Fsr::Vec3d axisZ(Fsr::lerp(local_matrix.getZAxis(), aim_matrix.getZAxis(), t*lookvals.k_lookat_do_rz));

        local_matrix.setToTranslation(aim_matrix.getTranslation());
        local_matrix.setXYZAxis(axisX, axisY, axisZ);
        local_matrix.scale(scale);
        local_matrix.skew(shear);
        //---------------------------------------------------------------
        //std::cout << "    local_matrix out" << local_matrix  << std::endl;
    }

    return local_matrix;
}


/*! Builds the entire transform matrix. Includes parent, local and lookat.
*/
/*virtual*/
Fsr::Mat4d
SceneXform::getWorldTransformAt(const DD::Image::OutputContext& context) const
{
    //std::cout << "SceneXform('" << const_cast<SceneXform*>(this)->sceneOp()->node_name() << "' " << this << ")::getWorldTransformAt()" << std::endl;

    // Extract the local transform from the Axis_Knob knobs, build the parent
    // transform and lookat rotations, then produce double-precision matrices
    // from the lot to use:
    Fsr::Mat4d m;
    m  = getInputParentTransformAt(context);
    m *= getParentConstraintTransformAt(context);
    m *= getLocalTransformWithLookatAt(m, context);

    //std::cout << "    world_matrix" << m << std::endl;

    return m;
}


//------------------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
SceneXform::buildHandles(DD::Image::ViewerContext* ctx)
{
#if 0
    DD::Image::AxisOp* axis = asAxisOp();
#ifdef DEBUG
    assert(axis);
#endif

    DD::Image::Matrix4 saved_matrix = ctx->modelmatrix;

    // Go up the inputs asking them to build their handles.
    // Do this first so that other ops always have a chance to draw!
    axis->DD::Image::Op::build_input_handles(ctx);  // inputs are drawn in current world space

    if (axis->node_disabled())
        return;

    // Only draw the camera's icon in 3D view:
    // TODO: what about stereo window controls in 2D?
    if (ctx->transform_mode() == DD::Image::VIEWER_2D)
        return;

    ctx->modelmatrix = saved_matrix;

    axis->validate(false); // get transforms up to date

    // Local knobs are drawn/manipulated in parent's space context,
    // so mult in just parent xform. ctx->modelmatrix will be saved
    // in each build-knob entry:
    ctx->modelmatrix *= m_input_matrix.asDDImage();
    ctx->modelmatrix *= m_parent_matrix.asDDImage();

    // Let op build any of its local-space handles (3D transform, 2D controls, etc):
    DD::Image::Op::build_knob_handles(ctx);

    // Only draw the camera icon if viewer is in 3D mode:
    if (ctx->viewer_mode() > VIEWER_2D && AxisOp::display3d_)
    {
        axis->DD::Image::Op::add_draw_handle(ctx);

        ctx->expand_bbox(node_selected(),
                         local_.a03,
                         local_.a13,
                         local_.a23);
    }

    ctx->modelmatrix = saved_matrix; // don't leave matrix messed up
#endif
}


} // namespace Fsr


// end of FuserSceneXform.cpp


//
// Copyright 2019 DreamWorks Animation
//
