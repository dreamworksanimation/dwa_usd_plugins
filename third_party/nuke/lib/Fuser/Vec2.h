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

/// @file Fuser/Vec2.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Vec2_H
#define Fuser_Vec2_H

#include "api.h"

#include <DDImage/Vector2.h> // for DD::Image compatibility convenience

#include <cmath> // For sqrt, etc
#include <iostream>
#include <limits> // for numeric_limits<T>


namespace Fsr {

template <typename T> class Vec3;
template <typename T> class Vec4;
template <typename T> class Mat4;


/*!
*/
template <typename T>
class FSR_EXPORT Vec2
{
  public:
    T   x, y;       //!< the data

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! The default constructor leaves garbage in x,y!
    Vec2() {}

    //! Copy constructor.
    template<typename S>
    Vec2(const Vec2<S>& v) : x(T(v.x)), y(T(v.y)) {}

    //! Constructor that sets all components.
    Vec2(T _x, T _y) : x(_x), y(_y) {}

    //! Constructor from an array of numbers.
    Vec2(const T v[2]) : x(v[0]), y(v[1]) {}

    //! Constructor that sets all components to a single value.
    explicit Vec2(T d) : x(d), y(d) {}

    //! For DD::Image compatibility convenience.
    explicit Vec2(const DD::Image::Vector2& b) { *this = b; }

    //! Returns value as a specific type:
    operator Vec2<float>()  const;
    operator Vec2<double>() const;
    operator Vec2<int>()    const;
    operator DD::Image::Vector2() const;


    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    T&       operator [] (int n)       { return *(&x + n); }
    const T& operator [] (int n) const { return *(&x + n); }

    //! Returns a pointer to the first element.
    T*       array()       { return &x; }
    const T* array() const { return &x; }


    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    //! Set all components to a single value.
    void set(T d) { x = y = d; }
    //! Set all components.
    void set(T _x, T _y) { x = _x; y = _y; }
    //! Set components to 0 or 1.
    void setToZero() { x = y = (T)0; }
    void setToOne()  { x = y = (T)1; }

    //! Type-specific clear. Sets all components to 0.
    void clear() { setToZero(); }

    template<typename S>
    Vec2& operator =  (const Vec2<S>& v) { x=T(v.x); y=T(v.y); return *this; }
    template<typename S>
    Vec2& operator =  (const Vec3<S>& v) { x=T(v.x); y=T(v.y); return *this; }
    template<typename S>
    Vec2& operator =  (const Vec4<S>& v) { x=T(v.x); y=T(v.y); return *this; }
    Vec2& operator =  (const DD::Image::Vector2& b) { x = T(b.x); y = T(b.y); return *this; }

    //! Explicit copy from/to a DD::Image::Vector2.
    void               fromDDImage(const DD::Image::Vector2& b) { x = T(b.x); y = T(b.y); }
    void               toDDImage(DD::Image::Vector2& out) const;
    DD::Image::Vector2 asDDImage() const;


    /*---------------------------*/
    /*   Matrix Multiplication   */
    /*---------------------------*/
    //Vec2  operator *  (const Matr4<T>& m) const { Vec2<T> o; return m.transform(*this, o); }
    //Vec2& operator *= (const Mat4<T>& m)        { Vec2<T> o = *this; return m.transform(o, *this); }

    /*---------------------------*/
    /*      Multiplication       */
    /*---------------------------*/
    Vec2  operator *  (T d)           const { return Vec2(x*d, y*d); }
    Vec2  operator *  (const Vec2& v) const { return Vec2(x*v.x, y*v.y); }
    Vec2& operator *= (T d)           { x *= d; y *= d; return *this; }
    Vec2& operator *= (const Vec2& v) { x *= v.x; y *= v.y; return *this; }

    /*---------------------------*/
    /*          Division         */
    /*---------------------------*/
    Vec2  operator /  (T d)           const { return (*this * ((T)1/d)); }
    Vec2  operator /  (const Vec2& v) const { return Vec2(x/v.x, y/v.y); }
    Vec2& operator /= (T d)           { return *this *=((T)1/d); }
    Vec2& operator /= (const Vec2& v) { x /= v.x; y /= v.y; return *this; }

    /*---------------------------*/
    /*          Addition         */
    /*---------------------------*/
    Vec2  operator +  (T d)           const { return (*this + d); }
    Vec2  operator +  (const Vec2& v) const { return Vec2(x+v.x, y+v.y); }
    Vec2& operator += (T d)           { x += d; y += d; return *this; }
    Vec2& operator += (const Vec2& v) { x += v.x; y += v.y; return *this; }

    /*---------------------------*/
    /*        Subtraction        */
    /*---------------------------*/
    Vec2  operator -  (T d)           const { return Vec2(x-d, y-d); }
    Vec2  operator -  (const Vec2& v) const { return Vec2(x-v.x, y-v.y); }
    Vec2& operator -= (T d)           { x -= d; y -= d; return *this; }
    Vec2& operator -= (const Vec2& v) { x -= v.x; y -= v.y; return *this; }

    /*---------------------------*/
    /*        Negation           */
    /*---------------------------*/
    Vec2  operator -  () const { return Vec2(-x, -y); }
    void negate()              { x = -x; y = -y; }

    /*---------------------------*/
    /*         Equality          */
    /*---------------------------*/
    bool operator == (const Vec2& v) const { return x==v.x && y==v.y; }
    bool operator != (const Vec2& v) const { return x!=v.x || y!=v.y; }
    bool operator == (T d)           const { return x==d && y==d; }
    bool operator != (T d)           const { return x!=d || y!=d; }


    /*---------------------------*/
    /*      Vector Functions     */
    /*---------------------------*/

    //! Also known as the absolute value or magnitude of the vector.
    T    length() const { return std::sqrt(x*x + y*y); }

    //! Same as this dot this, length() squared.
    T    lengthSquared() const { return (x*x + y*y); }

    //! Same as (this-v).length()
    T    distanceBetween(const Vec2 &v) const {return std::sqrt((x-v.x)*(x-v.x) + (y-v.y)*(y-v.y)); }

    //! Same as (this-v).lengthSquared()
    T    distanceSquared(const Vec2 &v) const { return (x-v.x)*(x-v.x) + (y-v.y)*(y-v.y); }

    //! Dot product.
    T    dot(const Vec2 &v) const { return (x*v.x + y*v.y); }

    //! Returns the Z component of the cross product, Ux*Vy - Uy*Vx.
    T    cross(const Vec2 &v) const { return (x*v.y - y*v.x); }

    //! Change the vector to be unit length. Returns the original length.
    T    normalize() { T d = length(); if (d) *this *= ((T)1/d); return d; }

    //! Approximate normalization, returns approximate length.
    T    fastNormalize();

    //! Returns the minimum element.
    T    minimum()                 const { return std::min(x, y); }
    Vec2 minimum(const Vec2& v) const { return Vec2(std::min(v.x, x), std::min(v.y, y));}

    //! Returns the maximum element.
    T    maximum()                 const { return std::max(x, y); }
    Vec2 maximum(const Vec2& v) const { return Vec2(std::max(v.x, x), std::max(v.y, y));}

    //! Returns the absolute value of the largest element.
    T    largestAxis() const { return std::max(::fabs(x), ::fabs(y)); }


  private:
    template <typename R, typename S> struct isSameType       { enum {value = 0}; };
    template <typename R>             struct isSameType<R, R> { enum {value = 1}; };
};


/*----------------------------------*/
/* Typedefs for standard data types */
/*----------------------------------*/
typedef Vec2<float>  Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<int>    Vec2i;


/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/

//! Interpolate between two Vec2s.
template<typename T>
Vec2<T> lerp(const Vec2<T>& v0,
             const Vec2<T>& v1,
             T              t);

//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const Vec2<T>& v);


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Vec2<T>& v)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '[' << v.x << ' ' << v.y << ']';
    return o;
}

//-----------------------------------------------------------

//! Copy to/from a DD::Image::Vector2.
template<typename T>
inline void
Vec2<T>::toDDImage(DD::Image::Vector2& out) const { out.x = float(x); out.y = float(y); }

template<typename T>
inline DD::Image::Vector2
Vec2<T>::asDDImage() const { return DD::Image::Vector2(float(x), float(y)); }

template<typename T>
inline Vec2<T>::operator DD::Image::Vector2() const
{
    if (isSameType<float,T>::value)
        return *reinterpret_cast<DD::Image::Vector2*>(const_cast<Vec2<T>* >(this));
    else
        return this->asDDImage();
}

//-----------------------------------------------------------

template<typename T>
inline Vec2<T>::operator Vec2<float>() const
{
    if (isSameType<float,T>::value)
        return *this;
    else
        return Vec2<float>(float(x), float(y));
}
template<typename T>
inline Vec2<T>::operator Vec2<double>() const
{
    if (isSameType<double,T>::value)
        return *this;
    else
        return Vec2<double>(double(x), double(y));
}
template<typename T>
inline Vec2<T>::operator Vec2<int>() const
{
    if (isSameType<int,T>::value)
        return *this;
    else
        return Vec2<int>(int(x), int(y));
}

//-----------------------------------------------------------

template<typename T>
inline Vec2<T>
lerp(const Vec2<T>& v0,
     const Vec2<T>& v1,
     T              t)
{
    const T it = (T)1 - t;
    return Vec2<T>(v0.x*it + v1.x*t, v0.y*it + v1.y*t);
}

} //namespace Fsr

#endif

// end of Fuser/Vec2.h

//
// Copyright 2019 DreamWorks Animation
//
