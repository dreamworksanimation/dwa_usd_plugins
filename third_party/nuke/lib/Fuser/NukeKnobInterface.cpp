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

/// @file Fuser/NukeKnobInterface.cpp
///
/// @author Jonathan Egstad

#include "NukeKnobInterface.h"

namespace Fsr {


/*!
*/
void
storeDoubleInKnob(double                          value,
                  DD::Image::Knob*                k,
                  double                          frame,
                  int                             view,
                  unsigned                        element_offset)
{
    if (!k)
        return;

    const int nKnobElements = getNumKnobDoubles(k);
    if (nKnobElements == 0 || (int)element_offset >= nKnobElements)
        return; // don't bother...

    if (view <= 1)
    {
        if (Fsr::isAnimated(frame))
            k->set_value_at(value, frame, element_offset);
        else
            k->set_value(value, element_offset);
    }
    else
    {
        // Set a specific view:
        if (Fsr::isAnimated(frame))
            k->set_value_at_view(value, frame, view, element_offset);
        else
            k->set_value_at_view(value, 0.0, view, element_offset); // set uniform value, need dummy 0.0 time...?
    }
}


/*!
*/
void
storeDoubleInKnob(double                          value,
                  DD::Image::Knob*                k,
                  const DD::Image::OutputContext& context,
                  unsigned                        element_offset)
{
    storeDoubleInKnob(value, k, context.frame(), context.view(), element_offset);
}


/*!
*/
void
storeIntInKnob(int                             value,
               DD::Image::Knob*                k,
               double                          frame,
               int                             view,
               unsigned                        element_offset)
{
    storeDoubleInKnob(double(value), k, frame, view, element_offset);
}

/*!
*/
void
storeIntInKnob(int                             value,
               DD::Image::Knob*                k,
               const DD::Image::OutputContext& context,
               unsigned                        element_offset)
{
    storeIntInKnob(value, k, context.frame(), context.view(), element_offset);
}


//  a b c d |       nKnobElements == 4 (abcd), nInElements == 3
//  0 1 2   |       element_offset == 0
//    0 1 2 |       element_offset == 1
//      0 1 | 2     element_offset == 2
//        0 | 1 2   element_offset == 3
//          | 0 1 2 element_offset == 4
//

//  a b |             nKnobElements == 2 (ab), nInElements == 4
//  0 1 | 2 3         element_offset == 0
//    0 | 1 2 3       element_offset == 1
//      | 0 1 2 3     element_offset == 2
//


/*!
    TODO: change this to handle strides (packed arrays of vec2, vec3, matrix4, etc):
*/
void
storeArrayOfDoublesInKnob(const double*                   values,
                          unsigned                        size,
                        /*unsigned                        stride,*/
                          DD::Image::Knob*                k,
                          const DD::Image::OutputContext& context,
                          unsigned                        element_offset)
{
    if (!k || size == 0)
        return;

    const int nKnobElements = getNumKnobDoubles(k);
    const int nInElements = std::min((int)size, (nKnobElements - (int)element_offset));
    //std::cout << "storeArrayOfDoublesInKnob('" << k->name() << "'): size=" << size;
    //std::cout << ", nKnobElements=" << nKnobElements;
    //std::cout << ", nInElements=" << nInElements;
    //std::cout << ", element_offset=" << element_offset;
    //std::cout << ", view=" << context.view();
    //std::cout << ", time=" << context.frame();
    //std::cout << std::endl;

    if (nKnobElements <= 0 || nInElements <= 0)
        return; // don't bother...

    if (context.view() <= 1)
    {
        // Default (no specific) view:
        if (Fsr::isAnimated(context.frame()))
        {
            k->set_animated(-1); // enable animation on all the sub-knobs
            for (int i=0; i < nInElements; ++i)
                k->set_value_at(values[i], context.frame(), element_offset+i);
        }
        else
        {
            for (int i=0; i < nInElements; ++i)
                k->set_value(values[i], element_offset+i);
        }
    }
    else
    {
        // Set a specific view:
        if (Fsr::isAnimated(context.frame()))
        {
            k->set_animated_view(context.view(), -1); // enable animation on all the sub-knobs
            for (int i=0; i < nInElements; ++i)
                k->set_value_at_view(values[i], context.frame(), context.view(), element_offset+i);
        }
        else
        {
            for (int i=0; i < nInElements; ++i)
                k->set_value_at_view(values[i], 0.0, context.view(), element_offset+i); // set uniform value, need dummy 0.0 time...?
        }
    }
}


//--------------------------------------------------------------------------------------------------


/*! Helper function to store an array of doubles in knob values.
*/
void
storeDoublesInKnob(DD::Image::Knob*        k,
                   const ArrayKnobDoubles& vals,
                   int                     knob_index_start,
                   int                     view)
{
    storeDoublesInKnob(k, vals.values, vals.doubles_per_value, vals.times, knob_index_start, view);
}


/*! Helper function to store an array of doubles in knob values.
*/
void
storeDoublesInKnob(DD::Image::Knob*           k,
                   const std::vector<double>& values,
                   int                        doubles_per_value,
                   const std::vector<double>& times,
                   int                        knob_index_start,
                   int                        view)
{
    if (!k || doubles_per_value <= 0 || values.size() < (size_t)doubles_per_value)
        return; // don't bother...

    const int nKnobDoubles = getNumKnobDoubles(k);
    if (nKnobDoubles == 0 || knob_index_start >= nKnobDoubles)
        return; // don't bother...

    if (knob_index_start < 0)
        knob_index_start = 0;
    // Clamp the number of doubles to copy:
    const int nCopyDoubles = ((knob_index_start + doubles_per_value) >= nKnobDoubles) ?
                                (nKnobDoubles - knob_index_start) : doubles_per_value;

    const bool is_animated = (Fsr::isAnimated(times) && times.size() == (values.size()*doubles_per_value));

    if (view < 0)
    {
        // No view:
        for (int i=knob_index_start; i < nCopyDoubles; ++i)
            k->clear_animated(i/*index*/); // clear any existing keys

        if (is_animated)
        {
            // Enable animation on channels:
            for (int i=knob_index_start; i < nCopyDoubles; ++i)
                k->set_animated(i/*index*/);

            // Set keys:
            for (size_t j=0; j < values.size(); j+=doubles_per_value)
            {
                const double t = times[j];
                size_t vi = j;
                for (int i=knob_index_start; i < nCopyDoubles; ++i)
                    k->set_value_at(values[vi++], t, i/*index*/);
            }
        }
        else
        {
            // Set a uniform value:
            size_t vi = 0;
            for (int i=knob_index_start; i < nCopyDoubles; ++i)
                k->set_value(values[vi++], i/*index*/);
        }
    }
    else
    {
        // Set doubles at a particular view:
        for (int i=knob_index_start; i < nCopyDoubles; ++i)
            k->clear_animated_view(view, i/*index*/); // clear any existing keys

        if (is_animated)
        {
            // Enable animation on channels:
            for (int i=knob_index_start; i < nCopyDoubles; ++i)
                k->set_animated_view(view, i/*index*/);

            // Set keys:
            for (size_t j=0; j < values.size(); j+=doubles_per_value)
            {
                const double t = times[j];
                size_t vi = j;
                for (int i=knob_index_start; i < nCopyDoubles; ++i)
                    k->set_value_at_view(values[vi++], t, view, i/*index*/);
            }
        }
        else
        {
            // Set a uniform value:
            size_t vi = 0;
            for (int i=knob_index_start; i < nCopyDoubles; ++i)
                k->set_value_at_view(values[vi++], 0.0, view, i/*index*/); // need dummy 0.0 time...?
        }
    }
}


//--------------------------------------------------------------------------------------------------


void
storeVec2dInKnob(const Fsr::Vec2d&               value,
                 DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 unsigned                        element_offset)
    { storeArrayOfDoublesInKnob(value.array(), 2, k, context, element_offset); }

void
storeVec3dInKnob(const Fsr::Vec3d&               value,
                 DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 unsigned                        element_offset)
    { storeArrayOfDoublesInKnob(value.array(), 3, k, context, element_offset); }

void
storeVec4dInKnob(const Fsr::Vec4d&               value,
                 DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 unsigned                        element_offset)
    { storeArrayOfDoublesInKnob(value.array(), 4, k, context, element_offset); }

void
storeMat4dInKnob(const Fsr::Mat4d&               value,
                 DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 unsigned                        element_offset)
    { storeArrayOfDoublesInKnob(value.array(), 16, k, context, element_offset); }


//--------------------------------------------------------------------------------------------------


/*! Helper function to store Vec2ds in knob values with optional
    scale/offset applied (in that order.)
*/
void
storeVec2dsInKnob(DD::Image::Knob*               k,
                  const std::vector<Fsr::Vec2d>& values,
                  const std::vector<double>&     times,
                  int                            view)
{
    if (!k)
        return;
#if 0
    // TODO: change this to handle stride (packed arrays of vec2, vec3, matrix4, etc):
    storeArrayOfDoublesInKnob(static_cast<double*>(values.data()), values.size(), 2/*stride*/,
                              k, context,
                              0/*element_offset*/);
#else
    const int nElements = std::min(getNumKnobDoubles(k), 2);
    if (nElements == 0 || values.size() == 0)
        return; // don't bother...

    if (view < 0)
    {
        // No view:
        k->clear_animated(-1); // clear any existing keys on all the sub-knobs
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated(-1); // enable animation on all the sub-knobs
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec2d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at(v[i], t, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec2d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value(v[i], i/*index*/); // set uniform value
        }
    }
    else
    {
        // Set a particular view:
        k->clear_animated_view(view, -1); // clear any existing keys on all the sub-knobs
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated_view(view, -1); // enable animation on all the sub-knobs
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec2d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at_view(v[i], t, view, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec2d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value_at_view(v[i], 0.0, view, i/*index*/); // set uniform value, need dummy 0.0 time...?
        }
    }
#endif
}


//--------------------------------------------------------------------------------------------------


/*! Helper function to store Vec3ds in knob values.
*/
void
storeVec3dsInKnob(DD::Image::Knob*               k,
                  const std::vector<Fsr::Vec3d>& values,
                  const std::vector<double>&     times,
                  int                            view)
{
    if (!k)
        return;
#if 0
    // TODO: change this to handle stride (packed arrays of vec2, vec3, matrix4, etc):
    storeArrayOfDoublesInKnob(static_cast<double*>(values.data()), values.size(), 3/*stride*/,
                              k, context,
                              0/*element_offset*/);
#else
    const int nElements = std::min(getNumKnobDoubles(k), 3);
    if (nElements == 0 || values.size() == 0)
        return; // don't bother...

    if (view < 0)
    {
        // No view:
        k->clear_animated(-1); // clear any existing keys on all the sub-knobs
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated(-1); // enable animation on all the sub-knobs
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec3d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at(v[i], t, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec3d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value(v[i], i/*index*/); // set uniform value
        }
    }
    else
    {
        // Set a particular view:
        k->clear_animated_view(view, -1); // clear any existing keys
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated_view(view, -1); // enable animation
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec3d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at_view(v[i], t, view, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec3d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value_at_view(v[i], 0.0, view, i/*index*/); // set uniform value, need dummy 0.0 time...?
        }
    }
#endif
}


//--------------------------------------------------------------------------------------------------


/*! Helper function to store Vec4ds in knob values.
*/
void
storeVec4dsInKnob(DD::Image::Knob*               k,
                  const std::vector<Fsr::Vec4d>& values,
                  const std::vector<double>&     times,
                  int                            view)
{
    if (!k)
        return;
#if 0
    // TODO: change this to handle stride (packed arrays of vec2, vec3, matrix4, etc):
    storeArrayOfDoublesInKnob(static_cast<double*>(values.data()), values.size(), 4/*stride*/,
                              k, context,
                              0/*element_offset*/);
#else
    const int nElements = std::min(getNumKnobDoubles(k), 4);
    if (nElements == 0 || values.size() == 0)
        return; // don't bother...

    if (view < 0)
    {
        // No view:
        k->clear_animated(-1); // clear any existing keys on all the sub-knobs
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated(-1); // enable animation on all the sub-knobs
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec4d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at(v[i], t, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec4d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value(v[i], i/*index*/); // set uniform value
        }
    }
    else
    {
        // Set a particular view:
        k->clear_animated_view(view, -1); // clear any existing keys on all the sub-knobs
        if (Fsr::isAnimated(times) && times.size() == values.size())
        {
            // Set keys:
            k->set_animated_view(view, -1); // enable animation on all the sub-knobs
            for (size_t j=0; j < values.size(); ++j)
            {
                const double t = times[j];
                const Fsr::Vec4d& v = values[j];
                for (int i=0; i < nElements; ++i)
                    k->set_value_at_view(v[i], t, view, i/*index*/);
            }
        }
        else
        {
            const Fsr::Vec4d& v = values[0];
            for (int i=0; i < nElements; ++i)
                k->set_value_at_view(v[i], 0.0, view, i/*index*/); // set uniform value, need dummy 0.0 time...?
        }
    }
#endif
}


} // namespace Fsr


// end of Fuser/NukeKnobInterface.cpp

//
// Copyright 2019 DreamWorks Animation
//
