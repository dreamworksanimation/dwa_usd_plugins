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

/// @file Fuser/Time.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Time_h
#define Fuser_Time_h

#include "api.h"

#include <limits> // for quiet_NaN
#include <cmath>  // for isnan()
#include <vector>


//-------------------------------------------------------------------------
//
// These helper types/functions are intended to be compatible with the
// UsdTimeCode::Default() time value, but are intended to be generally
// useful without being dependent on the Usd libs.
//
// TODO: I don't think these types need to be classes as we can keep the
// time methods in the global Fuser namespace for ease of use, or at the
// most wrap them in a 'Time' namespace.
//------------------------------------------------------------------------


namespace Fsr {

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! A TimeValue is an abstract, absolute time reference that's unlocked from
    the frame rate that most animation keys are defined on. ie a key at
    frame 105 in a 24 fps project is at a different absolute time value
    than frame 105 in a 48 fps project (4.375 seconds vs. 2.1875 seconds.)

    TimeValues are fractional seconds where 1.0 = one second, 2.0 =
    2 seconds, 1.5 = one-and-a-half seconds, etc.
*/
typedef double TimeValue;


/*! A 'not-animated' time value (either frame or time) represents 'no time',
    eg a non-animated value or keyframe.

    This special value is *not supported* by Nuke's keyframe system so setting
    a Nuke knob keyframe to this will likely result in unexpected behavior.

    This is the same as doing 'time = std::numeric_limits<double>::quiet_NaN()'.
*/
FSR_EXPORT
inline TimeValue notAnimatedTimeValue() { return std::numeric_limits<TimeValue>::quiet_NaN(); }

FSR_EXPORT
inline TimeValue defaultTimeValue() { return notAnimatedTimeValue(); }


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! A FrameValue is an absolute frame number that's dependent on a
    companion frames-per-second rate (24.0 fps, 30.0 fps, 48.0 fps, etc.)

    The only point when a TimeValue and a FrameValue are ever equal is
    at exactly value 0.0.

    Conversion to/from a TimeValue is trivial as you just need the
    frames-per-second rate:
        TimeValue  = FrameValue / frameRate;
        FrameValue = TimeValue * frameRate;

*/
typedef double FrameValue;


//! Same as notAnimatedTimeValue().
FSR_EXPORT
inline FrameValue notAnimatedFrameValue() { return notAnimatedTimeValue(); }

//! Same as defaultTimeValue().
FSR_EXPORT
inline FrameValue defaultFrameValue() { return defaultTimeValue(); }


//-----------------------------------------


//! Convert a FrameValue to an absolute TimeValue. Requires frames-per-second.
FSR_EXPORT
inline TimeValue  getAbsoluteTime(const FrameValue& frame,
                                  double            frames_per_second) { return (frame / frames_per_second); }

//! Convert an absolute TimeValue to a FrameValue. Requires frames-per-second.
FSR_EXPORT
inline FrameValue getFrameValue(const TimeValue& time,
                                double           frames_per_second) { return (time * frames_per_second); }

FSR_EXPORT
inline void setNotAnimated(TimeValue& time) { time = notAnimatedTimeValue(); }

FSR_EXPORT
inline void setNotAnimated(std::vector<TimeValue>& times) { times.resize(1); times[0] = notAnimatedTimeValue(); }


//-----------------------------------------


/*! Does the time value represent 'no time', i.e. a non-animated value?
    This is the same as doing std::isnan(time).
*/
FSR_EXPORT
inline bool isAnimated(const TimeValue& time) { return !std::isnan(time); }

FSR_EXPORT
inline bool isNotAnimated(const TimeValue& time) { return !isAnimated(time); }


FSR_EXPORT
inline bool isAnimated(const std::vector<TimeValue>& times) { return (times.size() > 0 && isAnimated(times[0])); }

FSR_EXPORT
inline bool isNotAnimated(const std::vector<TimeValue>& times) { return !isAnimated(times); }


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr

#endif

// end of Time.h


//
// Copyright 2019 DreamWorks Animation
//
