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

/// @file Fuser/Box3.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Box3_h
#define Fuser_Box3_h

#include "Vec3.h"
#include "Vec4.h"
#include "Box2.h"

#include <DDImage/Box3.h> // for DD::Image compatibility convenience

#include <string.h> // For memset, memcmp & memcpy
#include <iostream>


namespace Fsr {


/*! \class Fsr::Box3
    Axis-aligned bounding box (AABB)

*/
template <typename T>
class FSR_EXPORT Box3
{
  public:
    enum Corner
    {
        MinMinMin=0, MaxMinMin=1, MaxMaxMin=2, MinMaxMin=3,
        MinMinMax=4, MaxMinMax=5, MaxMaxMax=6, MinMaxMax=7,
        NumCorners=8
    };
    enum Face
    {
        Front =0, Back =1,
        Bottom=2, Top  =3,
        Left  =4, Right=5,
        NumFaces=6
    };


  public:
    // These are public for ease of access.
    Vec3<T>  min;       //!< "Lower-left"
    Vec3<T>  max;       //!< "Upper-right"

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! Default ctor makes an empty-state bbox where min=infinity & max=-infinity
    Box3();

    //! Copy constructor
    template<typename S>
    Box3(const Box3<S>& b) : min(T(b.min.x), T(b.min.y), T(b.min.z)),
                             max(T(b.max.x), T(b.max.y), T(b.max.z)) {}

    Box3(const T array[6]) { memcpy(&this->min.x, array, 6*sizeof(T)); }

    Box3(T x, T y, T z) : min(x, y, z), max(x, y, z) {}

    Box3(T x, T y, T z,
         T r, T t, T f) : min(x, y, z), max(r, t, f) {}

    Box3(const Vec3<T>& _min,
         const Vec3<T>& _max) : min(_min), max(_max) {}

    explicit Box3(const Vec3<T>& v) : min(v), max(v) {}

    explicit Box3(const Vec3<T>* points,
                  size_t         nPoints) { set(points, nPoints); }
    template<typename S>
    explicit Box3(const Vec3<S>* points,
                  size_t         nPoints,
                  const Mat4<T>& xform) { set(points, nPoints, xform); }

    //! For DD::Image compatibility convenience.
    explicit Box3(const DD::Image::Box3& b) { *this = b; }

    //! Transmogrify as a specific type:
    operator Box3<float>()     const;
    operator Box3<double>()    const;
    operator Box3<int>()       const;
    operator DD::Image::Box3() const;


    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    void set(T x, T y, T z,
             T r, T t, T f)       { this->min.set(x, y, z); this->max.set(r, t, f); }
    void set(const Vec3<T>& _min,
             const Vec3<T>& _max) { this->min = _min; this->max = _max; }
    void set(const T array[6])    { memcpy(&this->min.x, array, 6*sizeof(T)); }
    //!
    void set(T x, T y, T z)    { this->min.set(x, y, z); this->max.set(x, y, z); }
    void set(const Vec3<T>& v) { this->min = v; this->max = v; }
    //!
    void set(const Box3<T>& b) { *this = b; }
    //!
    void set(const Vec3<T>* points,
             size_t         nPoints);
    template<typename S>
    void set(const Vec3<S>* points,
             size_t         nPoints,
             const Mat4<T>& xform);

    //! Set all components to 0 or 1.
    void setToZero() { memset(min.array(), 0, 6*sizeof(T)); }
    void setToOne();

    void setMin(const Vec3<T>& v) { this->min = v; }
    void setMin(T x, T y, T z)    { this->min.set(x, y, z); }
    void setMax(const Vec3<T>& v) { this->max = v; }
    void setMax(T x, T y, T z)    { this->max.set(x, y, z); }

    Box3<T>& operator =  (const DD::Image::Box3& b);

    //! Sets box to empty state where min=infinity & max=-infinity.
    void setToEmptyState();

    //! Return true if the box is in an empty state (min=infinity & max=-infinity.)
    bool isEmpty() const;

    //! Type-specific clear. Sets box to empty state where min=infinity & max=-infinity.
    void clear() { setToEmptyState(); }

    //! Copy from/to a DD::Image::Box3
    void                   fromDDImage(const DD::Image::Box3& in);
    void                   toDDImage(DD::Image::Box3& out) const;
    inline DD::Image::Box3 asDDImage() const;


    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    //! Return a pointer to min.x
    T*       array()       { return &this->min.x; }
    const T* array() const { return &this->min.x; }

    T  x() const { return this->min.x; }                        //!< left   - min X
    T  y() const { return this->min.y; }                        //!< bottom - min Y
    T  z() const { return this->min.z; }                        //!< near   - min Z
    T  r() const { return this->max.x; }                        //!< right  - max X
    T  t() const { return this->max.y; }                        //!< top    - max Y
    T  f() const { return this->max.z; }                        //!< far    - max Z
    T  w() const { return (this->max.x - this->min.x); }        //!< width
    T  h() const { return (this->max.y - this->min.y); }        //!< height
    T  d() const { return (this->max.z - this->min.z); }        //!< depth

    T cx() const { return (this->min.x + this->max.x) / (T)2; } //!< center X
    T cy() const { return (this->min.y + this->max.y) / (T)2; } //!< center Y
    T cz() const { return (this->min.z + this->max.z) / (T)2; } //!< center Z

    //! Return the xyz coordinate of the bbox center.
    Vec3<T> getCenter() const { return (this->min + this->max) / (T)2; }

    //! Return the xyz coordinate of one of the corners.
    Vec3<T> getCorner(Corner corner_index);

    T    radius() const { return (this->max - this->min).length() / (T)2; }
    T    minDim() const { return std::min(this->min.minimum(), this->max.minimum()); }
    T    maxDim() const { return std::max(this->min.maximum(), this->max.maximum()); }

    //! Returns true if point is inside the box.
    bool isInside(const Vec3<T>& p) const;
    bool isInside(T x, T y, T z) const;

    //! Expand or contract the box by a set amount.
    void pad(T d)              { this->min -= d; this->max += d; }
    void pad(const Vec3<T>& p) { this->min -= p; this->max += p; }
    void pad(T x, T y, T z)    { pad(Vec3<T>(x, y, z)); }

    //! Shift the position of the box.
    void shift(const Vec3<T>& p) { this->min += p; this->max += p; }
    void shift(T x, T y, T z)    { shift(Vec3<T>(x, y, z)); }
    void shiftMin(T x, T y, T z) { this->min += Vec3<T>(x, y, z); }
    void shiftMax(T x, T y, T z) { this->max += Vec3<T>(x, y, z); }

    //! Union the box with another. If this one is empty the other (non-empty) box is copied.
    void    expand(const Box3<T>& b,
                   bool           test_empty=true);
    //! Expand the box to contain a point. If empty the box is set to a zero-size at the point.
    void    expand(const Vec3<T>& p,
                   bool           test_empty=true);
    void    expand(T x, T y, T z,
                   bool test_empty=true) { expand(Vec3<T>(x, y, z), test_empty); }
    void    expand(const DD::Image::Box3& b,
                   bool                   test_empty=true);

    //! Find the union between the bbox and another.
    Box3<T> intersect(const Box3<T>& v) const;

    //! Project the corners of the bbox by a 4x4 matrix. If all corners project behind origin return false and empty box.
    bool project(const Mat4<T>& m,
                 Box2<T>&       box2D_out) const;

    //! Returns 1 if point projects behind origin, ie an 'outside corner'.
    static uint32_t projectCorner(T x, T y, T z,
                                  const Mat4<T>& m,
                                  Box2<T>&       box2D_out);

    //! Returns 1 if point projects behind origin, ie an 'outside corner'.
    uint32_t        projectCorner(Corner         corner_index,
                                  const Mat4<T>& m,
                                  Box2<T>&       box2D_out) const;

    //! Interpolate between two bboxes.
    Box3<T> interpolate(const Box3<T>& b,
                        T              t) const;
    Box3<T>        lerp(const Box3<T>& b,
                        T              t) const;


  private:
    template <typename R, typename S>
    struct isSameType
    {
        enum {value = 0};
    };

    template <typename R>
    struct isSameType<R, R>
    {
        enum {value = 1};
    };

};


/*----------------------------------*/
/* Typedefs for standard data types */
/*----------------------------------*/
typedef Box3<float>  Box3f;
typedef Box3<double> Box3d;
typedef Box3<int>    Box3i;


/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/


//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const Vec3<T>& v);


//! Build untransformed and transformed bboxes in one pass.
template<typename T, typename S>
void getLocalAndTransformedBbox(const Vec3<T>*      points,
                                size_t              nPoints,
                                Fsr::Box3<T>&       local_bbox,
                                const Fsr::Mat4<S>& xform,
                                Fsr::Box3<S>&       xformed_bbox);




/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline
Box3<T>::Box3() :
    min( std::numeric_limits<T>::infinity()),
    max(-std::numeric_limits<T>::infinity())
{
    //
}

template<typename T>
inline void
Box3<T>::setToEmptyState()
{
    this->min.set( std::numeric_limits<T>::infinity());
    this->max.set(-std::numeric_limits<T>::infinity());
}

template<typename T>
inline void
Box3<T>::setToOne()
{
    min.x = min.y = min.z = (T)1;
    max.x = max.y = max.z = (T)1;
}

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Box3<T>& b)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '[' << b.min.x << ' ' << b.min.y << ' ' << b.min.z << ", "
             << b.max.x << ' ' << b.max.y << ' ' << b.max.z << ']'
      << '(' << b.w() << ' ' << b.h() << ' ' << b.d() << ')';
    return o ;
}

//-----------------------------------------------------------

template<typename T>
inline void
Box3<T>::fromDDImage(const DD::Image::Box3& b)
{
    this->min.fromDDImage(b.min());
    this->max.fromDDImage(b.max());
}
template<typename T>
inline Box3<T>&
Box3<T>::operator =  (const DD::Image::Box3& b)
{
    this->fromDDImage(b);
    return *this;
}

template<typename T>
inline void
Box3<T>::toDDImage(DD::Image::Box3& out) const
{
    float* outp = const_cast<float*>(&out.min().x);
    if (isSameType<float,T>::value)
        memcpy(outp, &this->min.x, 6*sizeof(float));
    else
    {
        const T* inp = &this->min.x;
        for (int i=0; i < 6; ++i)
            *outp++ = float(*inp++);
    }
}
template<typename T>
inline DD::Image::Box3
Box3<T>::asDDImage() const
{
    DD::Image::Box3 b;
    this->toDDImage(b);
    return b;
}

template<typename T>
inline Box3<T>::operator DD::Image::Box3() const
{
    if (isSameType<float,T>::value)
        return reinterpret_cast<DD::Image::Box3&>(const_cast<Box3<T>&>(*this));
    else
        return this->asDDImage();
}

//-----------------------------------------------------------

template<typename T>
inline Box3<T>::operator Box3<float>() const
{
    if (isSameType<float,T>::value)
        return *this;
    else
        return Box3<float>(float(min.x), float(min.y), float(min.z),
                           float(max.x), float(max.y), float(max.z));
}
template<typename T>
inline Box3<T>::operator Box3<double>() const
{
    if (isSameType<double,T>::value)
        return *this;
    else
        return Box3<double>(double(min.x), double(min.y), double(min.z),
                            double(max.x), double(max.y), double(max.z));
}
template<typename T>
inline Box3<T>::operator Box3<int>() const
{
    if (isSameType<int,T>::value)
        return *this;
    else
        return Box3<int>(int(min.x), int(min.y), int(min.z),
                         int(max.x), int(max.y), int(max.z));
}

//-----------------------------------------------------------

template<typename T>
inline Vec3<T>
Box3<T>::getCorner(Corner corner_index)
{
    switch (corner_index)
    {
    case MinMinMin: return Vec3<T>(this->min.x, this->min.y, this->min.z);
    case MaxMinMin: return Vec3<T>(this->max.x, this->min.y, this->min.z); 
    case MaxMaxMin: return Vec3<T>(this->max.x, this->max.y, this->min.z);
    case MinMaxMin: return Vec3<T>(this->min.x, this->max.y, this->min.z);
    case MinMinMax: return Vec3<T>(this->min.x, this->min.y, this->max.z);
    case MaxMinMax: return Vec3<T>(this->max.x, this->min.y, this->max.z);
    case MaxMaxMax: return Vec3<T>(this->max.x, this->max.y, this->max.z);
    case MinMaxMax: return Vec3<T>(this->min.x, this->max.y, this->max.z);
    }
}

template<typename T>
inline bool
Box3<T>::isEmpty() const
{
    return (this->max.x < this->min.x ||
            this->max.y < this->min.y ||
            this->max.z < this->min.z);
}

template<typename T>
inline bool
Box3<T>::isInside(T x, T y, T z) const
{
    const bool is_outside = (x < this->min.x || x > this->max.x ||
                             y < this->min.y || y > this->max.y ||
                             z < this->min.z || z > this->max.z);
    return !is_outside;
}
template<typename T>
inline bool
Box3<T>::isInside(const Vec3<T>& p) const { return isInside(p.x, p.y, p.z); }

template<typename T>
inline void
Box3<T>::expand(const Vec3<T>& p,
                bool           test_empty)
{
    if (test_empty && this->isEmpty())
        this->min = this->max = p;
    else
    {
#if 1
        if (p.x < this->min.x) this->min.x = p.x; if (p.x > this->max.x) this->max.x = p.x;
        if (p.y < this->min.y) this->min.y = p.y; if (p.y > this->max.y) this->max.y = p.y;
        if (p.z < this->min.z) this->min.z = p.z; if (p.z > this->max.z) this->max.z = p.z;
#else
        this->min.x = std::min(this->min.x, p.x); this->max.x = std::max(this->max.x, p.x);
        this->min.y = std::min(this->min.y, p.y); this->max.y = std::max(this->max.y, p.y);
        this->min.z = std::min(this->min.z, p.z); this->max.z = std::max(this->max.z, p.z);
#endif
    }
}

template<typename T>
inline void
Box3<T>::expand(const Box3<T>& b,
                bool           test_empty)
{
    if (b.isEmpty())
        return; // source bbox is empty, do nothing
    else if (test_empty && this->isEmpty())
        *this = b;
    else
    {
        this->expand(b.min, false/*test_empty*/);
        this->expand(b.max, false/*test_empty*/);
    }
}

template<typename T>
inline void
Box3<T>::expand(const DD::Image::Box3& b,
                bool                   test_empty)
{
    expand(T(b.min().x), T(b.min().y), T(b.min().z), test_empty);
    expand(T(b.max().x), T(b.max().y), T(b.max().z), false);
}

template<typename T>
inline void
Box3<T>::set(const Vec3<T>* points,
             size_t         nPoints)
{
    if (nPoints == 0)
        setToEmptyState();
    else
    {
        this->min = this->max = *points++;
        for (size_t i=1; i < nPoints; ++i)
            expand(*points++, false/*test_empty*/);
    }
}

template<typename T>
template<typename S>
inline void
Box3<T>::set(const Vec3<S>* points,
             size_t         nPoints,
             const Mat4<T>& xform)
{
    if (nPoints == 0)
        setToEmptyState();
    else if (xform.isIdentity())
    {
        this->set(*points++);
        for (size_t i=1; i < nPoints; ++i)
            expand(*points++, false/*test_empty*/);
    }
    else
    {
        this->set(xform.transform(*points++));
        for (size_t i=1; i < nPoints; ++i)
            expand(xform.transform(*points++), false/*test_empty*/);
    }
}

template<typename T, typename S>
inline void
getLocalAndTransformedBbox(const Vec3<T>*      points,
                           size_t              nPoints,
                           Fsr::Box3<T>&       local_bbox,
                           const Fsr::Mat4<S>& xform,
                           Fsr::Box3<S>&       xformed_bbox)
{
    // Calc both bboxes in one pass:
    if (nPoints == 0)
    {
        local_bbox.setToEmptyState();
        xformed_bbox.setToEmptyState();
    }
    else
    {
        if (xform.isIdentity())
        {
            local_bbox.set(points, nPoints);
            xformed_bbox.set(local_bbox.min, local_bbox.max);
        }
        else
        {
            local_bbox.set(points[0]);
            xformed_bbox.set(xform.transform(points[0]));
            for (size_t i=1; i < nPoints; ++i)
            {
                local_bbox.expand(points[i], false/*test_empty*/);
                xformed_bbox.expand(xform.transform(points[i]), false/*test_empty*/);
            }
        }
    }
}

//-----------------------------------------------------------

template<typename T>
inline Box3<T>
Box3<T>::intersect(const Box3<T>& b) const
{
    return Box3<T>(Vec3<T>(std::max(this->min.x, b.min.x),
                           std::max(this->min.y, b.min.y),
                           std::max(this->min.z, b.min.z)),
                   Vec3<T>(std::min(this->max.x, b.max.x),
                           std::min(this->max.y, b.max.y),
                           std::min(this->max.z, b.max.z)));
}

//-----------------------------------------------------------

//! Returns 1 if xyz point projects behind origin, ie an 'outside corner'.
template<typename T>
/*static*/ inline uint32_t
Box3<T>::projectCorner(T x, T y, T z,
                       const Mat4<T>& m,
                       Box2<T>&       box2D_out)
{
    const Vec4<T> v = m * Vec3<T>(x, y, z);
    if (v.w < (T)0)
        return 1; // corner is behind origin
    box2D_out.expand(v.x/v.w, v.y/v.w, v.w);
    return 0;
}

//! Returns 1 if point projects behind origin, ie an 'outside corner'.
template<typename T>
inline uint32_t
Box3<T>::projectCorner(Corner         corner_index,
                       const Mat4<T>& m,
                       Box2<T>&       box2D_out) const
{
    const Vec4<T> v = m.transform(getCorner(corner_index));
    if (v.w < (T)0)
        return 1; // corner is behind origin
    box2D_out.expand(v.x/v.w, v.y/v.w, v.w);
    return 0;
}

template<typename T>
inline bool
Box3<T>::project(const Mat4<T>& m,
                 Box2<T>&       box2D_out) const
{
    box2D_out.setToEmptyState();
    uint32_t num_outside_corners = 0;
    for (uint32_t i=0; i < NumCorners; ++i)
        num_outside_corners += projectCorner(this->min.x, this->min.y, this->min.z, m, box2D_out);
    if (num_outside_corners == NumCorners)
    {
        box2D_out.setToEmptyState(); // all corners behind origin
        return false;
    }
    return true; // at least one corner in projection
}

//-----------------------------------------------------------

//! Interpolate between two bounding-boxes.
template<typename T>
inline Box3<T>
Box3<T>::interpolate(const Box3<T>& b,
                     T              t) const
{
    if (t < std::numeric_limits<T>::epsilon())
        return *this; // before or at first
    else if (t > ((T)1 - std::numeric_limits<T>::epsilon()))
        return b; // at or after last
    else
        return Box3<T>(b.min*((T)1 - t) + b.min*t,
                       b.max*((T)1 - t) + b.max*t); // lerp
}
template<typename T>
inline Box3<T>
Box3<T>::lerp(const Box3<T>& b, T t) const { return this->interpolate(b, t); }


} //namespace Fsr


#endif

// end of Fuser/Box3.h

//
// Copyright 2019 DreamWorks Animation
//
