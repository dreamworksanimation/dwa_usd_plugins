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

/// @file Fuser/Vec4.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Vec4_H
#define Fuser_Vec4_H

#include "Vec2.h"
#include "Vec3.h"

#include <DDImage/Vector4.h> // for DD::Image compatibility convenience
#include <DDImage/Hash.h>    // for DD::Image compatibility convenience

#include <cmath> // For sqrt, etc
#include <iostream>
#include <limits> // for numeric_limits<T>


namespace Fsr {

template <typename T> class Mat4;


/*!
*/
template <typename T>
class FSR_EXPORT Vec4
{
  public:
    T   x, y, z, w;     //!< the data

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! The default constructor leaves garbage in x,y,z,w!
    Vec4() {}

    //! Copy constructor.
    template<typename S>
    explicit Vec4(const Vec4<S>& v) : x(T(v.x)), y(T(v.y)), z(T(v.z)), w(T(v.w)) {}

    //! Constructor that sets all components.
    Vec4(T _x, T _y , T _z=(T)0, T _w=(T)1) : x(_x), y(_y), z(_z), w(_w) {}

    //! Constructor from an array of 4 numbers.
    Vec4(const T v[4]) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}

    // Compatibility with other vector sizes.
    explicit Vec4(const Vec2<T>& v, T vz=(T)0, T vw=(T)1) : x(v.x), y(v.y), z(vz), w(vw) {}
    explicit Vec4(const Vec3<T>& v, T vw=(T)1) : x(v.x), y(v.y), z(v.z), w(vw) {}

    //! Constructor that sets components to a single value.
    explicit Vec4(T d) : x(d), y(d), z(d), w(d) {}
    explicit Vec4(T dxyz, T dw) : x(dxyz), y(dxyz), z(dxyz), w(dw) {}

    //! For DD::Image compatibility convenience.
    explicit Vec4(const DD::Image::Vector4& b) { *this = b; }

    //! Transmogrify as a specific type:
    operator Vec4<float>()        const;
    operator Vec4<double>()       const;
    operator Vec4<int>()          const;
    operator DD::Image::Vector4() const;


    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    T&       operator [] (int n)       { return *(&x + n); }
    const T& operator [] (int n) const { return *(&x + n); }

    //! Returns a pointer to the first element.
    T*       array()       { return &x; }
    const T* array() const { return &x; }

    Vec2<T>  xy()  const { return Vec2<T>(x, y); }
    Vec3<T>  xyz() const { return Vec3<T>(x, y, z); }


    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    //! Set xyz to a single value and w to 1.
    void set(T d) { x = y = z = d; w = (T)1; }
    //! Set all components, w defaults to 1.
    void set(T _x, T _y , T _z, T _w=(T)1) { x = _x; y = _y; z = _z; w = _w; }
    //! Set xyz and w separately.
    void set(const Vec3<T>& v, T vw=(T)1) { this->set(v.x, v.y, v.z, vw); }
    //! Set components to 0 or 1.
    void setToZero() { x = y = z = w = (T)0; }
    void setToOne()  { x = y = z = w = (T)1; }

    //! Type-specific clear. Set xyz to 0 and w to 1.
    void clear() { x = y = z = (T)0; w = (T)1; }

    template<typename S>
    Vec4& operator =  (const Vec2<S>& v) { x=T(v.x); y=T(v.y); z=(T)0;   w=(T)1;   return *this; }
    template<typename S>
    Vec4& operator =  (const Vec3<S>& v) { x=T(v.x); y=T(v.y); z=T(v.z); w=(T)1;   return *this; }
    template<typename S>
    Vec4& operator =  (const Vec4<S>& v) { x=T(v.x); y=T(v.y); z=T(v.z); z=T(v.w); return *this; }
    Vec4& operator =  (const DD::Image::Vector4& b) { x = T(b.x); y = T(b.y); z = T(b.z); w = T(b.w); return *this; }

    //! Explicit copy from/to a DD::Image::Vector4.
    void               fromDDImage(const DD::Image::Vector4& b) { x = T(b.x); y = T(b.y); z = T(b.z); w = T(b.w); }
    void               toDDImage(DD::Image::Vector4& out) const;
    DD::Image::Vector4 asDDImage() const;

    //! Add this to a DD::Image::Hash object, for DD::Image compatibility convenience.
    void append(DD::Image::Hash& hash) const;


    /*---------------------------*/
    /*   Matrix Multiplication   */
    /*---------------------------*/
    Vec4  operator *  (const Mat4<T>& m) const { Vec4<T> o; return m.transform(*this, o); }
    Vec4& operator *= (const Mat4<T>& m) { Vec4<T> o = *this; return m.transform(o, *this); }

    /*---------------------------*/
    /*      Multiplication       */
    /*---------------------------*/
    Vec4  operator *  (T d)           const { return Vec4(x*d, y*d, z*d, w*d); }
    Vec4  operator *  (const Vec4& v) const { return Vec4(x*v.x, y*v.y, z*v.z, w*v.w); }
    Vec4& operator *= (T d)           { x *= d; y *= d; z *= d; w *= d; return *this; }
    Vec4& operator *= (const Vec4& v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }

    /*---------------------------*/
    /*          Division         */
    /*---------------------------*/
    Vec4  operator /  (T d)           const { return (*this)*((T)1/d); }
    Vec4  operator /  (const Vec4& v) const { return Vec4(x/v.x, y/v.y, z/v.z, w/v.w); }
    Vec4& operator /= (T d)           { return (*this)*=((T)1/d); }
    Vec4& operator /= (const Vec4& v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }

    /*---------------------------*/
    /*          Addition         */
    /*---------------------------*/
    Vec4  operator +  (T d)           const { return Vec4(x+d, y+d, z+d, w+d); }
    Vec4  operator +  (const Vec4& v) const { return Vec4(x+v.x, y+v.y, z+v.z, w+v.w); }
    Vec4& operator += (T d)           { x += d; y += d; z += d; w += d; return *this; }
    Vec4& operator += (const Vec4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }

    /*---------------------------*/
    /*        Subtraction        */
    /*---------------------------*/
    Vec4  operator -  (T d)           const { return Vec4(x-d, y-d, z-d, w-d); }
    Vec4  operator -  (const Vec4& v) const { return Vec4(x-v.x, y-v.y, z-v.z, w-v.w);}
    Vec4& operator -= (T d)           { x -= d; y -= d; z -= d; w -= d; return *this; }
    Vec4& operator -= (const Vec4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }

    /*---------------------------*/
    /*        Negation           */
    /*---------------------------*/
    Vec4  operator -  () const { return Vec4(-x, -y, -z, -w); }
    void negate()              { x = -x; y = -y; z = -z; w = -w; }

    /*---------------------------*/
    /*         Equality          */
    /*---------------------------*/
    bool operator == (const Vec4& v) const { return x==v.x && y==v.y && z==v.z && w==v.w; }
    bool operator != (const Vec4& v) const { return x!=v.x || y!=v.y || z!=v.z || w!=v.w; }
    bool operator == (T d)           const { return x==d && y==d && z==d && w==d; }
    bool operator != (T d)           const { return x!=d || y!=d || z!=d || w!=d; }


    /*---------------------------*/
    /*      Vector Functions     */
    /*---------------------------*/

    //! Divide xyz by w.
    Vec4&   wNormalize()        { T d=(T)1/w; x*=d; y*=d; z*=d; w=(T)1; return *this; }
    Vec3<T> wNormalized() const { T d=(T)1/w; return Vec3<T>(x*d, y*d, z*d); }

    //! Also known as the absolute value or magnitude of the vector.
    T    length() const { return std::sqrt(x*x + y*y + z*z); }

    //! Same as this dot this, length() squared.
    T    lengthSquared() const { return (x*x + y*y + z*z); }

    //! Same as (this-v).length()
    T    distanceBetween(const Vec4& v) const { return std::sqrt((x-v.x)*(x-v.x) + (y-v.y)*(y-v.y) + (z-v.z)*(z-v.z)); }

    //! Same as (this-v).lengthSquared()
    T    distanceSquared(const Vec4& v) const { return (x-v.x)*(x-v.x) + (y-v.y)*(y-v.y) + (z-v.z)*(z-v.z); }

    //! Return the scalar distance to the plane defined by ABCD.
    T    distanceFromPlane(T A, T B, T C, T D) const { return A*x + B*y + C*z + D; }

    //! Dot product. Twice the area of the triangle between the vectors.
    T    dot(const Vec4& v) const { return x*v.x + y*v.y + z*v.z; }

    //! Cross product - returns vector at right angles to the vectors.
    Vec4 cross(const Vec4& v) const { return Vec4(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x); }

    //! Approximate normalization, returns approximate length.
    T    fastNormalize();

    //! Return a vector of this one reflected around the passed normal vector.
    Vec4 reflect(const Vec4& N) { Vec4 r = N * (dot(N) * 2) - this; return r;}

    //! Returns the minimum XYZ element - w is ignored.
    T    minimum()              const { return std::min(x, std::min(y, z)); }
    Vec4 minimum(const Vec4& v) const { return Vec4(std::min(v.x, x), std::min(v.y, y), std::min(v.z, z)); }

    //! Returns the maximum XYZ element - w is ignored.
    T    maximum()              const { return std::max(x, std::max(y, z)); }
    Vec4 maximum(const Vec4& v) const { return Vec4(std::max(v.x, x), std::max(v.y, y), std::max(v.z, z)); }

    //! Returns the absolute value of the largest XYZ element - w is ignored.
    T    largestAxis() const { return std::max(::fabs(x), std::max(::fabs(y), ::fabs(z))); }

    //! Linear-interpolate between this Vec4 and another at t, where t=0..1.
    template<typename S>
    Vec4<T> interpolateTo(const Vec4<T>& b,
                          S              t) const;
    template<typename S>
    Vec4<T>        lerpTo(const Vec4<T>& b,
                          S              t) const;


  private:
    template <typename R, typename S> struct isSameType       { enum {value = 0}; };
    template <typename R>             struct isSameType<R, R> { enum {value = 1}; };
};


/*----------------------------------*/
/* Typedefs for standard data types */
/*----------------------------------*/
typedef Vec4<float>  Vec4f;
typedef Vec4<double> Vec4d;
typedef Vec4<int>    Vec4i;



/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/

//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const Vec4<T>& v);


//! Linear-interpolate between two Vec3s at t, where t=0..1.
template<typename T, typename S>
Vec4<T> lerp(const Vec4<T>& v0,
             const Vec4<T>& v1,
             S              t);
//! Linear-interpolate between two Vec3s at t, where t=0..1, and inv is 1-t.
template<typename T, typename S>
Vec4<T> lerp(const Vec4<T>& v0,
             const Vec4<T>& v1,
             S              t,
             S              invt);

//! Interpolate between three Vec3s at barycentric coord st.
template<typename T, typename S>
Vec4<T> interpolateAtBaryCoord(const Fsr::Vec4<T>& v0,
                               const Fsr::Vec4<T>& v1,
                               const Fsr::Vec4<T>& v2,
                               const Fsr::Vec2<S>& st);
//! Interpolate between three Vec4s at barycentric coord st, with derivatives.
template<typename T, typename S>
void    interpolateAtBaryCoord(const Fsr::Vec4<T>& v0,
                               const Fsr::Vec4<T>& v1,
                               const Fsr::Vec4<T>& v2,
                               const Fsr::Vec2<S>& st,
                               const Fsr::Vec2<S>& stdx,
                               const Fsr::Vec2<S>& stdy,
                               Fsr::Vec4<T>&       out,
                               Fsr::Vec4<T>&       duout,
                               Fsr::Vec4<T>&       dvout);


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Vec4<T>& v)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '[' << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w <<']';
    return o;
}

//-----------------------------------------------------------

//! Copy to/from a DD::Image::Vector4.
template<typename T>
inline void
Vec4<T>::toDDImage(DD::Image::Vector4& out) const { out.x = float(x); out.y = float(y); out.z = float(z);  out.w = float(w); }

template<typename T>
inline DD::Image::Vector4
Vec4<T>::asDDImage() const { return DD::Image::Vector4(float(x), float(y), float(z), float(w)); }

template<typename T>
inline Vec4<T>::operator DD::Image::Vector4() const
{
    if (isSameType<float,T>::value)
        return *reinterpret_cast<DD::Image::Vector4*>(const_cast<Vec4<T>* >(this));
    else
        return this->asDDImage();
}

template<typename T>
inline void
Vec4<T>::append(DD::Image::Hash& hash) const
{
    hash.append(this->array(), 4*sizeof(T));
}

//-----------------------------------------------------------

template<typename T>
inline Vec4<T>::operator Vec4<float>() const
{
    if (isSameType<float,T>::value)
        return *this;
    else
        return Vec4<float>(float(x), float(y), float(z), float(w));
}
template<typename T>
inline Vec4<T>::operator Vec4<double>() const
{
    if (isSameType<double,T>::value)
        return *this;
    else
        return Vec4<double>(double(x), double(y), double(z), double(w));
}
template<typename T>
inline Vec4<T>::operator Vec4<int>() const
{
    if (isSameType<int,T>::value)
        return *this;
    else
        return Vec4<int>(int(x), int(y), int(z), int(w));
}

//-----------------------------------------------------------

template<typename T>
template<typename S>
inline Vec4<T>
Vec4<T>::interpolateTo(const Vec4<T>& b,
                       S              t) const
{
    if (t < std::numeric_limits<S>::epsilon())
        return *this; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return b; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Vec4<T>(this->x*invtT + b.x*tT, this->y*invtT + b.y*tT, this->z*invtT + b.z*tT, this->w*invtT + b.w*tT);
}
template<typename T>
template<typename S>
inline Vec4<T>
Vec4<T>::lerpTo(const Vec4<T>& b, S t) const { return this->interpolateTo(b, t); }

template<typename T, typename S>
inline Vec4<T>
lerp(const Vec4<T>& v0,
     const Vec4<T>& v1,
     S              t)
{
    if (t < std::numeric_limits<S>::epsilon())
        return v0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return v1; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Vec4<T>(v0.x*invtT + v1.x*tT, v0.y*invtT + v1.y*tT, v0.z*invtT + v1.z*tT);
}
template<typename T, typename S>
inline Vec4<T>
lerp(const Vec4<T>& v0,
     const Vec4<T>& v1,
     S              t,
     S              invt)
{
    if (t < std::numeric_limits<S>::epsilon())
        return v0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return v1; // at or after last
    const T tT    = T(t);
    const T invtT = T(invt);
    return Vec4<T>(v0.x*invtT + v1.x*tT, v0.y*invtT + v1.y*tT, v0.z*invtT + v1.z*tT, v0.w*invtT + v1.w*tT);
}

// Interpolate between three Vec4s at barycentric coord st.
template<typename T, typename S>
inline Vec4<T>
interpolateAtBaryCoord(const Fsr::Vec4<T>& v0,
                       const Fsr::Vec4<T>& v1,
                       const Fsr::Vec4<T>& v2,
                       const Fsr::Vec2<S>& st)
{
    return Vec4<T>(v0 + ((v1 - v0)*T(st.x)) + ((v2 - v0)*T(st.y)));
}

// Interpolate between three Vec3s at barycentric coord st, with derivatives.
template<typename T, typename S>
inline void
interpolateAtBaryCoord(const Fsr::Vec4<T>& v0,
                       const Fsr::Vec4<T>& v1,
                       const Fsr::Vec4<T>& v2,
                       const Fsr::Vec2<S>& st,
                       const Fsr::Vec2<S>& stdx,
                       const Fsr::Vec2<S>& stdy,
                       Fsr::Vec4<T>&       out,
                       Fsr::Vec4<T>&       duout,
                       Fsr::Vec4<T>&       dvout)
{
    const Fsr::Vec4<T> e01 = (v1 - v0);
    const Fsr::Vec4<T> e02 = (v2 - v0);
    const Fsr::Vec4<T> dt = (e01*T(st.x)) + (e02*T(st.y));
    out   = v0 + dt;
    duout = (e01*T(stdx.x)) + (e02*T(stdx.y)) - dt;
    dvout = (e01*T(stdy.x)) + (e02*T(stdy.y)) - dt;
}

} //namespace Fsr

#endif

// end of Fuser/Vec4.h

//
// Copyright 2019 DreamWorks Animation
//
