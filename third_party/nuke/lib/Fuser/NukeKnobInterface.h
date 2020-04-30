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

/// @file Fuser/NukeKnobInterface.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NukeKnobInterface_h
#define Fuser_NukeKnobInterface_h

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//
// Helper methods to abstract access to Nuke's DD::Image::Knobs.
//
// Also provides convenience methods to translate to/from Fuser
// Vector/Matrix classes.
// 
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#include "NodeContext.h" // for isAnimated()
#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"
#include "Mat4.h"
#include "Box3.h"

#include <DDImage/Knob.h>
#include <DDImage/ArrayKnobI.h>
#include <DDImage/OutputContext.h>
#include <DDImage/Op.h>


namespace Fsr {


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

//! Helper to set knob to a color via label html tags.
FSR_EXPORT
void setKnobLabel(DD::Image::Knob* k,
                  const char*      label,
                  const char*      color="");


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Helper class to pass to get/set methods that deal with DD::Image::ArrayKnobs.
*/
class FSR_EXPORT ArrayKnobDoubles
{
  public:
    std::vector<double> values;             //!< List of doubles from attribute
    unsigned            doubles_per_value;  //!< Stride
    std::vector<double> times;              //!< List of times. If non-animated isNotAnimated(times[0])==true.


  public:
    ArrayKnobDoubles() : doubles_per_value(1) {}

    //! Must have a virtual destructor!
    virtual ~ArrayKnobDoubles() {}


    size_t size() const { return times.size(); }
    size_t nTimes() const { return times.size(); }
    size_t nValues() const { return values.size(); }


  public:
    //! Value access - unclamped!
    double        value(size_t j, size_t i=0) const { return values[i + j*doubles_per_value]; }
    double* operator[] (size_t j) const { return const_cast<double*>(values.data())+(j*doubles_per_value); }


    //! Time access - unclamped!
    double time(size_t i) const { return times[i]; }


    //!
    void   concatenateTimes(std::set<Fsr::TimeValue> concat_times)
    {
        if (times.size() > 0)
        {
            concat_times.erase(Fsr::defaultTimeValue());
            const size_t nSamples = times.size();
            for (size_t i=0; i < nSamples; ++i)
                concat_times.insert(times[i]);
        }
        else
        {
            concat_times.insert(Fsr::defaultTimeValue());
        }
    }

    //! State
    bool   isValid() const { return (times.size() > 0 && times.size()*doubles_per_value == values.size()); }
    bool   isAnimated() const { return Fsr::isAnimated(times); }
    bool   isNotAnimated() const { return Fsr::isNotAnimated(times); }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Helper function to get the number of elements in an ArrayKnob.
    Returns 0 if it's not an array knob.
*/
FSR_EXPORT
int getNumKnobDoubles(DD::Image::Knob* k);


/*! Helper template function to extract a value from a DD::Image::Knob.
*/
template<typename T>
FSR_EXPORT
void getKnobValue(DD::Image::Knob*                k,
                  const DD::Image::OutputContext& context,
                  DD::Image::StoreType            store_type,
                  T*                              value);


//!
inline
FSR_EXPORT
bool getBoolValue(DD::Image::Knob* k) { return ((k)?(k->get_value() > 0.5):false); }


//-------------------------------------------------------------------------


/*  Typed get Knob value specializations.

    The pain here is that the store-type size should match the knob definition and that's not
    easily retrievable from the Knob itself, so we need to implement each type.

    Luckily all Array_Knob subclasses internally store doubles even if their external wrappers
    only store into floats (like XYZ_knob), so we leverage that to copy into Fuser
    double-precision Vector/Matrix classes.


    TODO: add default value args to all these!
*/

FSR_EXPORT
void getStringKnob(DD::Image::Knob*                k,
                   const DD::Image::OutputContext& context,
                   const char*&                    value);
FSR_EXPORT
void getStringKnob(const char*                     name,
                   const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context,
                   const char*&                    value);

FSR_EXPORT
void getStringKnob(DD::Image::Knob*                k,
                   const DD::Image::OutputContext& context,
                   std::string&                    value);
FSR_EXPORT
void getStringKnob(const char*                     name,
                   const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context,
                   std::string&                    value);

//-------------------------------------------------------------------------

FSR_EXPORT
void getDoubleKnob(DD::Image::Knob*                k,
                   const DD::Image::OutputContext& context,
                   double&                         value);
FSR_EXPORT
void getDoubleKnob(const char*                     name,
                   const DD::Image::Op*            op,
                   const DD::Image::OutputContext& context,
                   double&                         value);

FSR_EXPORT
void getFloatKnob(DD::Image::Knob*                k,
                  const DD::Image::OutputContext& context,
                  float&                          value);
FSR_EXPORT
void getFloatKnob(const char*                     name,
                  const DD::Image::Op*            op,
                  const DD::Image::OutputContext& context,
                  float&                          value);

FSR_EXPORT
void getIntKnob(DD::Image::Knob*                k,
                const DD::Image::OutputContext& context,
                int&                            value);
FSR_EXPORT
void getIntKnob(const char*                     name,
                const DD::Image::Op*            op,
                const DD::Image::OutputContext& context,
                int&                            value);

FSR_EXPORT
void getUnsignedIntKnob(DD::Image::Knob*                k,
                        const DD::Image::OutputContext& context,
                        uint32_t&                       value);
FSR_EXPORT
void getUnsignedIntKnob(const char*                     name,
                        const DD::Image::Op*            op,
                        const DD::Image::OutputContext& context,
                        uint32_t&                       value);

FSR_EXPORT
void getBoolKnob(DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 bool&                           value);
FSR_EXPORT
void getBoolKnob(const char*                     name,
                 const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context,
                 bool&                           value);

//-------------------------------------------------------------------------

//! Copy a 2-float knob to a Fsr::Vec2d.
FSR_EXPORT
void getVec2Knob(DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec2d&                     value);
FSR_EXPORT
void getVec2Knob(const char*                     name,
                 const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec2d&                     value);

//! Copy a 3-float knob to a Fsr::Vec3d.
FSR_EXPORT
void getVec3Knob(DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec3d&                     value);
FSR_EXPORT
void getVec3Knob(const char*                     name,
                 const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec3d&                     value);

//! Copy a 4-float knob to a Fsr::Vec4d.
FSR_EXPORT
void getVec4Knob(DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec4d&                     value);
FSR_EXPORT
void getVec4Knob(const char*                     name,
                 const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context,
                 Fsr::Vec4d&                     value);

//-------------------------------------------------------------------------

//! Copy a 16-float knob to a Fsr::Mat4d.
FSR_EXPORT
void getMat4Knob(DD::Image::Knob*                k,
                 const DD::Image::OutputContext& context,
                 Fsr::Mat4d&                     value);
FSR_EXPORT
void getMat4Knob(const char*                     name,
                 const DD::Image::Op*            op,
                 const DD::Image::OutputContext& context,
                 Fsr::Mat4d&                     value);


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*  Typed store Knob specializations.

    The pain here is that the store-type size should match the knob definition and that's not
    easily retrievable from the Knob itself, so we need to implement each type.

    For storing we allow an offset into the internal double array so that we can more easily
    support things like placing transparency into the 4th element of a Color_knob.

    Luckily all Array_Knob subclasses internally store doubles even if their external wrappers
    only store into floats (like XYZ_knob), so we leverage that to copy from Fuser
    double-precision Vector/Matrix classes.

*/

FSR_EXPORT
void storeArrayOfDoublesInKnob(const double*                   values,
                               unsigned                        size,
                               DD::Image::Knob*                k,
                               const DD::Image::OutputContext& context,
                               unsigned                        element_offset=0);

//! Helper functions to store a value into a knob at an optional time and view (in OutputContext).
FSR_EXPORT
void storeDoubleInKnob(double                          value,
                       DD::Image::Knob*                k,
                       const DD::Image::OutputContext& context,
                       unsigned                        element_offset=0);

FSR_EXPORT
void storeDoubleInKnob(double                          value,
                       DD::Image::Knob*                k,
                       double                          frame,
                       int                             view=-1,
                       unsigned                        element_offset=0);

//-------------------------------------------------------------------------

FSR_EXPORT
void storeIntInKnob(int                             value,
                    DD::Image::Knob*                k,
                    const DD::Image::OutputContext& context,
                    unsigned                        element_offset=0);

FSR_EXPORT
void storeIntInKnob(int                             value,
                    DD::Image::Knob*                k,
                    double                          frame,
                    int                             view=-1,
                    unsigned                        element_offset=0);

//-------------------------------------------------------------------------

FSR_EXPORT
void storeVec2dInKnob(const Fsr::Vec2d&               value,
                      DD::Image::Knob*                k,
                      const DD::Image::OutputContext& context,
                      unsigned                        element_offset=0);
FSR_EXPORT
void storeVec3dInKnob(const Fsr::Vec3d&               value,
                      DD::Image::Knob*                k,
                      const DD::Image::OutputContext& context,
                      unsigned                        element_offset=0);
FSR_EXPORT
void storeVec4dInKnob(const Fsr::Vec4d&               value,
                      DD::Image::Knob*                k,
                      const DD::Image::OutputContext& context,
                      unsigned                        element_offset=0);

//-------------------------------------------------------------------------

FSR_EXPORT
void storeMat4dInKnob(const Fsr::Mat4d&               value,
                      DD::Image::Knob*                k,
                      const DD::Image::OutputContext& context,
                      unsigned                        element_offset=0);


//-------------------------------------------------------------------------

//! Helper functions to store vectors of animated values into a knob at an optional view.
FSR_EXPORT
void storeDoublesInKnob(DD::Image::Knob*        k,
                        const ArrayKnobDoubles& vals,
                        int                     element_offset,
                        int                     view);
FSR_EXPORT
void storeDoublesInKnob(DD::Image::Knob*           k,
                        const std::vector<double>& values,
                        int                        doubles_per_value,
                        const std::vector<double>& times,
                        int                        element_offset,
                        int                        view);

//-------------------------------------------------------------------------

FSR_EXPORT
void storeVec2dsInKnob(DD::Image::Knob*               k,
                       const std::vector<Fsr::Vec2d>& values,
                       const std::vector<double>&     times,
                       int                            view);
FSR_EXPORT
void storeVec3dsInKnob(DD::Image::Knob*               k,
                       const std::vector<Fsr::Vec3d>& values,
                       const std::vector<double>&     times,
                       int                            view);
FSR_EXPORT
void storeVec4dsInKnob(DD::Image::Knob*               k,
                       const std::vector<Fsr::Vec4d>& values,
                       const std::vector<double>&     times,
                       int                            view);



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

inline int
getNumKnobDoubles(DD::Image::Knob* k)

{
    if (!k || !k->arrayKnob())
        return 0; // don't crash...
    return (int)k->arrayKnob()->array_size();
}

template<typename T>
inline void
getKnobValue(DD::Image::Knob*                k,
             const DD::Image::OutputContext& context,
             DD::Image::StoreType            store_type,
             T*                              value)
{
    // Always check for null so missing knob names won't cause a crash:
    if (!k)
        return;
#ifdef DEBUG
    assert(value);
#endif
    DD::Image::Hash dummy_hash;
    k->store(store_type, value, dummy_hash, context);
}

//-------------------------------------------------------------------------

inline void getStringKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, const char*& value)
    { getKnobValue<const char*>(k, context, DD::Image::StringPtr, &value); }

inline void getStringKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, const char*& value)
    { getStringKnob(op->knob(name), context, value); }

inline void getStringKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, std::string& value)
    { getKnobValue<std::string>(k, context, DD::Image::StlStringPtr, &value); }

inline void getStringKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, std::string& value)
    { getStringKnob(op->knob(name), context, value); }

//-------------------------------------------------------------------------

inline void getDoubleKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, double& value)
    { getKnobValue<double>(k, context, DD::Image::DoublePtr, &value); }

inline void getDoubleKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, double& value)
    { getDoubleKnob(op->knob(name), context, value); }

inline void getFloatKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, float& value)
    { getKnobValue<float>(k, context, DD::Image::FloatPtr, &value); }

inline void getFloatKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, float& value)
    { getFloatKnob(op->knob(name), context, value); }

inline void getIntKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, int& value)
    { getKnobValue<int>(k, context, DD::Image::IntPtr, &value); }

inline void getIntKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, int& value)
    { getIntKnob(op->knob(name), context, value); }

inline void getUnsignedIntKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, uint32_t& value)
    { getKnobValue<uint32_t>(k, context, DD::Image::UnsignedIntPtr, &value); }

inline void getUnsignedIntKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, uint32_t& value)
    { getUnsignedIntKnob(op->knob(name), context, value); }

inline void getBoolKnob(DD::Image::Knob* k, const DD::Image::OutputContext& context, bool& value)
    { getKnobValue<bool>(k, context, DD::Image::BoolPtr, &value); }

inline void getBoolKnob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, bool& value)
    { getBoolKnob(op->knob(name), context, value); }

//-------------------------------------------------------------------------

inline void getVec2Knob(DD::Image::Knob* k, const DD::Image::OutputContext& context, Fsr::Vec2d& value)
    { getKnobValue<double>(k, context, DD::Image::DoublePtr, value.array()); }

inline void getVec2Knob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, Fsr::Vec2d& value)
    { getVec2Knob(op->knob(name), context, value); }

inline void getVec3Knob(DD::Image::Knob* k, const DD::Image::OutputContext& context, Fsr::Vec3d& value)
    { getKnobValue<double>(k, context, DD::Image::DoublePtr, value.array()); }

inline void getVec3Knob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, Fsr::Vec3d& value)
    { getVec3Knob(op->knob(name), context, value); }

inline void getVec4Knob(DD::Image::Knob* k, const DD::Image::OutputContext& context, Fsr::Vec4d& value)
    { getKnobValue<double>(k, context, DD::Image::DoublePtr, value.array()); }

inline void getVec4Knob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, Fsr::Vec4d& value)
    { getVec4Knob(op->knob(name), context, value); }

//-------------------------------------------------------------------------

inline void getMat4Knob(DD::Image::Knob* k, const DD::Image::OutputContext& context, Fsr::Mat4d& value)
{
#if 1
    getKnobValue<double>(k, context, DD::Image::DoublePtr, value.array());
#else
    float data[16];
    getKnobValue<float>(k, context, DD::Image::MatrixPtr, data);
    for (unsigned i=0; i < 16; ++i)
        value.element(i) = double(data[i]);
#endif
}
inline void getMat4Knob(const char* name, const DD::Image::Op* op, const DD::Image::OutputContext& context, Fsr::Mat4d& value)
    { getMat4Knob(op->knob(name), context, value); }


} // namespace Fsr

#endif

// end of Fuser/NukeKnobInterface.h

//
// Copyright 2019 DreamWorks Animation
//
