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

/// @file Fuser/Box2.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Box2_h
#define Fuser_Box2_h

#include "Vec2.h"

#include <DDImage/Box.h> // for DD::Image compatibility convenience

#include <string.h> // For memset, memcmp & memcpy
#include <iostream>


namespace Fsr {


/*! \class Fsr::Box2
*/
template <typename T>
class FSR_EXPORT Box2
{
  public:
    // These are public for ease of access.
    Vec2<T>  min;       //!< "Lower-left"
    Vec2<T>  max;       //!< "Upper-right"

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! Default ctor makes an empty-state bbox where min=<T>max() & max=-<T>max()
    Box2();

    //! Copy constructor
    template<typename S>
    explicit Box2(const Box2<S>& b) : min(T(b.min.x), T(b.min.y)),
                                      max(T(b.max.x), T(b.max.y)) {}

    Box2(const T array[4]) { memcpy(&this->min.x, array, 4*sizeof(T)); }

    Box2(T x, T y) : min(x, y), max(x, y) {}

    Box2(T x, T y,
         T r, T t) : min(x, y), max(r, t) {}

    Box2(const Vec2<T>& _min,
         const Vec2<T>& _max) : min(_min), max(_max) {}

    explicit Box2(const Vec2<T>& v) : min(v), max(v) {}

    //! The resulting Box2 is the intersection of all the source points.
    explicit Box2(const Vec2<T>* points,
                  size_t         nPoints) { set(points, nPoints); }

    //! For DD::Image compatibility convenience.
    explicit Box2(const DD::Image::Box& b) { *this = b; }

    //! Transmogrify as a specific type:
    operator Box2<float>()    const;
    operator Box2<double>()   const;
    operator Box2<int>()      const;
    operator DD::Image::Box() const;


    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    void set(T x, T y,
             T r, T t)            { this->min.set(x, y); this->max.set(r, t); }
    void set(const Vec2<T>& _min,
             const Vec2<T>& _max) { this->min = _min; this->max = _max; }
    void set(const T array[4])    { memcpy(&this->min.x, array, 4*sizeof(T)); }
    //!
    void set(T x, T y)         { this->min.set(x, y); this->max.set(x, y); }
    void set(const Vec2<T>& v) { this->min = v; this->max = v; }
    //!
    void set(const Box2<T>& b) { *this = b; }

    //! The resulting Box2 is the intersection of all the source points.
    void set(const Vec2<T>* points,
             size_t         nPoints);

    //! Set all components to 0 or 1.
    void setToZero() { memset(min.array(), 0, 4*sizeof(T)); }
    void setToOne();

    void setMin(const Vec2<T>& v) { this->min = v; }
    void setMin(T x, T y)         { this->min.set(x, y); }
    void setMax(const Vec2<T>& v) { this->max = v; }
    void setMax(T x, T y)         { this->max.set(x, y); }

    Box2<T>& operator =  (const DD::Image::Box& b);

    //! Sets box to empty state where min=<T>max() & max=-<T>max().
    void setToEmptyState();

    //! Return true if the box is in an empty state (min=<T>max() & max=-<T>max().)
    bool isEmpty() const;

    //! Type-specific clear. Sets box to empty state where min=<T>max() & max=-<T>max().
    void clear() { setToEmptyState(); }

    //! Copy from/to a DD::Image::Box
    void                  fromDDImage(const DD::Image::Box& in);
    void                  toDDImage(DD::Image::Box& out) const;
    inline DD::Image::Box asDDImage() const;

    //! Add this to a DD::Image::Hash object, for DD::Image compatibility convenience.
    void append(DD::Image::Hash& hash) const;


    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    //! Return a pointer to min.x
    T*       array()       { return &this->min.x; }
    const T* array() const { return &this->min.x; }

    T  x() const { return this->min.x; }                        //!< left   - min X
    T  y() const { return this->min.y; }                        //!< bottom - min Y
    T  r() const { return this->max.x; }                        //!< right  - max X
    T  t() const { return this->max.y; }                        //!< top    - max Y
    T  w() const { return (this->max.x - this->min.x); }        //!< width
    T  h() const { return (this->max.y - this->min.y); }        //!< height

    T cx() const { return (this->min.x + this->max.x) / (T)2; } //!< center X
    T cy() const { return (this->min.y + this->max.y) / (T)2; } //!< center Y

    //! Return a Vec2 with width/height in it.
    Vec2<T> getDimensions() const { return Vec2<T>(this->w(), this->h()); }

    //! Return the xy coordinate of the bbox center.
    Vec2<T> getCenter() const { return (this->min + this->max) / (T)2; }

    T    radius() const { return (this->max - this->min).length() / (T)2; }
    T    minDim() const { return std::min(this->min.minimum(), this->max.minimum()); }
    T    maxDim() const { return std::max(this->min.maximum(), this->max.maximum()); }

    //! Returns true if point is inside the box.
    bool pointIsInside(const Vec2<T>& p) const;
    bool pointIsInside(T x, T y)         const;

    //! Expand or contract the box by a set amount.
    void pad(T d)              { this->min -= d; this->max += d; }
    void pad(const Vec2<T>& p) { this->min -= p; this->max += p; }
    void pad(T x, T y)         { pad(Vec2<T>(x, y)); }

    //! Shift the position of the box.
    void shift(const Vec2<T>& p) { this->min += p; this->max += p; }
    void shift(T x, T y)         { shift(Vec2<T>(x, y)); }
    void shiftMin(T x, T y)      { this->min += Vec2<T>(x, y); }
    void shiftMax(T x, T y)      { this->max += Vec2<T>(x, y); }

    // TODO: these currently shift, is it more natural for them to do pad...?
    Box2<T>  operator +  (const Vec2<T>& v) const { return Box2<T>(min+v, max+v); }
    Box2<T>& operator += (const Vec2<T>& v)       { min += v; max += v; return *this; }
    Box2<T>  operator -  (const Vec2<T>& v) const { return Box2<T>(min-v, max-v); }
    Box2<T>& operator -= (const Vec2<T>& v)       { min -= v; max -= v; return *this; }

    //! Union the box with another. If this one is empty the other (non-empty) box is copied.
    void    expand(const Box2<T>& b,
                   bool           test_empty=true);
    //! Expand the box to contain a point. If empty the box is set to a zero-size at the point.
    void    expand(const Vec2<T>& p,
                   bool           test_empty=true);
    void    expand(T x, T y,
                   bool test_empty=true) { expand(Vec2<T>(x, y), test_empty); }
    void    expand(const DD::Image::Box& b,
                   bool                  test_empty=true);

    //! Find the union between the bbox and another.
    Box2<T> intersect(const Box2<T>& v) const;

    //! Project the corners of the bbox by a 4x4 matrix. If all corners project behind origin return false and empty box.
    //bool project(const Mat4<T>& m,
    //             Box2<T>&       bbox) const;

    //! Interpolate between this Box2 and another at t, where t=0..1.
    template<typename S>
    Box2<T> interpolateTo(const Box2<T>& b,
                          S              t) const;
    template<typename S>
    Box2<T>        lerpTo(const Box2<T>& b,
                          S              t) const;


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
typedef Box2<float>  Box2f;
typedef Box2<double> Box2d;
typedef Box2<int>    Box2i;


/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/


//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const Box2<T>& b);



//! Linear-interpolate between two Box2s at t, where t=0..1.
template<typename T, typename S>
Box2<T> lerp(const Box2<T>& v0,
             const Box2<T>& v1,
             S              t);
//! Linear-interpolate between two Box2s at t, where t=0..1, and inv is 1-t.
template<typename T, typename S>
Box2<T> lerp(const Box2<T>& v0,
             const Box2<T>& v1,
             S              t,
             S              invt);



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Box2<T>& b)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '[' << b.min.x << ' ' << b.min.y << ", " << b.max.x << ' ' << b.max.y << ']';
    return o;
}

//-----------------------------------------------------------

template<typename T>
inline
Box2<T>::Box2() :
    min( std::numeric_limits<T>::max()),
    max(-std::numeric_limits<T>::max())
{
    //
}

template<typename T>
inline void
Box2<T>::setToEmptyState()
{
    this->min.set( std::numeric_limits<T>::max());
    this->max.set(-std::numeric_limits<T>::max());
}

template<typename T>
inline void
Box2<T>::setToOne()
{
    min.x = min.y = (T)1;
    max.x = max.y = (T)1;
}

//-----------------------------------------------------------

template<typename T>
inline void
Box2<T>::fromDDImage(const DD::Image::Box& b)
{
    this->min.set(b.x(), b.y());
    this->max.set(b.r(), b.t());
}
template<typename T>
inline Box2<T>&
Box2<T>::operator =  (const DD::Image::Box& b)
{
    this->fromDDImage(b);
    return *this;
}

template<typename T>
inline void
Box2<T>::toDDImage(DD::Image::Box& out) const
{
    if (isSameType<int,T>::value)
        out.set(this->min.x, this->min.y,
                this->max.x, this->max.y);
    else
        out.set(int(this->min.x), int(this->min.y),
                int(this->max.x), int(this->max.y));
}
template<typename T>
inline DD::Image::Box
Box2<T>::asDDImage() const
{
    DD::Image::Box b;
    this->toDDImage(b);
    return b;
}

template<typename T>
inline Box2<T>::operator DD::Image::Box() const
{
    if (isSameType<int,T>::value)
        return reinterpret_cast<DD::Image::Box&>(const_cast<Box2<T>&>(*this));
    else
        return this->asDDImage();
}

template<typename T>
inline void
Box2<T>::append(DD::Image::Hash& hash) const
{
    hash.append(this->array(), 4*sizeof(T));
}

//-----------------------------------------------------------

template<typename T>
inline Box2<T>::operator Box2<float>() const
{
    if (isSameType<float,T>::value)
        return *this;
    else
        return Box2<float>(float(min.x), float(min.y),
                           float(max.x), float(max.y));
}
template<typename T>
inline Box2<T>::operator Box2<double>() const
{
    if (isSameType<double,T>::value)
        return *this;
    else
        return Box2<double>(double(min.x), double(min.y),
                            double(max.x), double(max.y));
}
template<typename T>
inline Box2<T>::operator Box2<int>() const
{
    if (isSameType<int,T>::value)
        return *this;
    else
        return Box2<int>(int(min.x), int(min.y),
                         int(max.x), int(max.y));
}

//-----------------------------------------------------------

template<typename T>
inline bool
Box2<T>::isEmpty() const
{
    return (this->max.x < this->min.x ||
            this->max.y < this->min.y);
}

template<typename T>
inline bool
Box2<T>::pointIsInside(T x, T y) const
{
    const bool is_outside = (x < this->min.x || x > this->max.x ||
                             y < this->min.y || y > this->max.y);
    return !is_outside;
}
template<typename T>
inline bool
Box2<T>::pointIsInside(const Vec2<T>& p) const { return pointIsInside(p.x, p.y); }

template<typename T>
inline void
Box2<T>::expand(const Vec2<T>& p,
                bool           test_empty)
{
    if (test_empty && this->isEmpty())
        this->min = this->max = p;
    else
    {
#if 1
        if (p.x < this->min.x) this->min.x = p.x; if (p.x > this->max.x) this->max.x = p.x;
        if (p.y < this->min.y) this->min.y = p.y; if (p.y > this->max.y) this->max.y = p.y;
#else
        this->min.x = std::min(this->min.x, p.x); this->max.x = std::max(this->max.x, p.x);
        this->min.y = std::min(this->min.y, p.y); this->max.y = std::max(this->max.y, p.y);
#endif
    }
}

template<typename T>
inline void
Box2<T>::expand(const Box2<T>& b,
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
Box2<T>::expand(const DD::Image::Box& b,
                bool                  test_empty)
{
    expand(T(b.x()), T(b.y()), test_empty);
    expand(T(b.r()), T(b.t()), false);
}

template<typename T>
inline void
Box2<T>::set(const Vec2<T>* points,
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

//-----------------------------------------------------------

template<typename T>
inline Box2<T>
Box2<T>::intersect(const Box2<T>& b) const
{
    return Box2<T>(Vec2<T>(std::max(this->min.x, b.min.x),
                           std::max(this->min.y, b.min.y)),
                   Vec2<T>(std::min(this->max.x, b.max.x),
                           std::min(this->max.y, b.max.y)));
}

//-----------------------------------------------------------

//! Interpolate between two bounding-boxes.
template<typename T>
template<typename S>
inline Box2<T>
Box2<T>::interpolateTo(const Box2<T>& b,
                       S              t) const
{
    if (t < std::numeric_limits<S>::epsilon())
        return *this; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return b; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Box2<T>(b.min*invtT + b.min*tT,
                   b.max*invtT + b.max*tT); // lerp
}
template<typename T>
template<typename S>
inline Box2<T>
Box2<T>::lerpTo(const Box2<T>& b, S t) const { return this->interpolateTo(b, t); }

template<typename T, typename S>
inline Box2<T>
lerp(const Box2<T>& b0,
     const Box2<T>& b1,
     S              t)
{
    if (t < std::numeric_limits<S>::epsilon())
        return b0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return b1; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Box2<T>(b0.min*invtT + b1.min*tT,
                   b0.max*invtT + b1.max*tT);
}
template<typename T, typename S>
inline Box2<T>
lerp(const Box2<T>& b0,
     const Box2<T>& b1,
     S              t,
     S              invt)
{
    if (t < std::numeric_limits<S>::epsilon())
        return b0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return b1; // at or after last
    const T tT    = T(t);
    const T invtT = T(invt);
    return Box2<T>(b0.min*invtT + b1.min*tT,
                   b0.max*invtT + b1.max*tT);
}


} //namespace Fsr


#endif

// end of Fuser/Box2.h

//
// Copyright 2019 DreamWorks Animation
//
