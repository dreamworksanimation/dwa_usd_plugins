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

/// @file Fuser/Vec3.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Vec3_H
#define Fuser_Vec3_H

#include "Vec2.h"

#include <DDImage/Vector3.h> // for DD::Image compatibility convenience
#include <DDImage/Hash.h>    // for DD::Image compatibility convenience

#include <cmath> // For sqrt, etc
#include <iostream>
#include <limits> // for numeric_limits<T>


namespace Fsr {

template <typename T> class Vec4;
template <typename T> class Mat4;


// These match the rotation order enums in DD::Image (Axis_KnobI, Matrix4)
enum RotationOrder
{ 
    XYZ_ORDER=0, XZY_ORDER=1,
    YXZ_ORDER=2, YZX_ORDER=3,
    ZXY_ORDER=4, ZYX_ORDER=5
};
extern FSR_EXPORT const char* rotation_orders[];


/*!
*/
template <typename T>
class FSR_EXPORT Vec3
{
  public:
    // 
    enum Axis
    {
        X=0, Y=1, Z=2
    };


  public:
    T   x, y, z;    //!< the data

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! The default constructor leaves garbage in x,y,z!
    Vec3() {}

    //! Copy constructor.
    template<typename S>
    explicit Vec3(const Vec3<S>& v) : x(T(v.x)), y(T(v.y)), z(T(v.z)) {}

    //! Constructor that normalizes vector after copy if 'n' is > 0.
    template<typename S>
    explicit Vec3(const Vec3<S>& v,
                  float          n);

    //! Constructor that sets all components.
    Vec3(T _x, T _y , T _z) : x(_x), y(_y), z(_z) {}

    //! Constructor from an array of numbers.
    Vec3(const T v[3]) : x(v[0]), y(v[1]), z(v[2]) {}

    // Compatibility with other vector sizes.
    explicit Vec3(const Vec2<T>& v, T vz=(T)0) : x(v.x), y(v.y), z(vz) {}

    //! Constructor that sets all components to a single value.
    explicit Vec3(T d) : x(d), y(d), z(d) {}

    //! For DD::Image compatibility convenience.
    explicit Vec3(const DD::Image::Vector3& b) { *this = b; }

    //! Returns value as a specific type:
    operator Vec3<float>()  const;
    operator Vec3<double>() const;
    operator Vec3<int>()    const;
    operator DD::Image::Vector3() const;

    Vec3<float>  asVec3f() const;
    Vec3<double> asVec3d() const;
    Vec3<int>    asVec3i() const;


    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    T&       operator [] (int n)       { return *(&x + n); }
    const T& operator [] (int n) const { return *(&x + n); }

    //! Returns a pointer to the first element.
    T*       array()       { return &x; }
    const T* array() const { return &x; }

    Vec2<T>  xy()  const { return Vec2<T>(x, y); }


    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    //! Set all components to a single value.
    void set(T d) { x = y = z = d; }
    //! Set all components.
    void set(T _x, T _y , T _z) { x = _x; y = _y; z = _z; }
    //! Set components to 0 or 1.
    void setToZero() { x = y = z = (T)0; }
    void setToOne()  { x = y = z = (T)1; }

    //! Type-specific clear. Set all components to 0.
    void clear() { setToZero(); }

    template<typename S>
    Vec3& operator =  (const Vec2<S>& v) { x=T(v.x); y=T(v.y); z=(T)0; return *this; }
    template<typename S>
    Vec3& operator =  (const Vec3<S>& v) { x=T(v.x); y=T(v.y); z=T(v.z); return *this; }
    template<typename S>
    Vec3& operator =  (const Vec4<S>& v) { x=T(v.x); y=T(v.y); z=T(v.z); return *this; }
    Vec3& operator =  (const DD::Image::Vector3& b) { x = T(b.x); y = T(b.y); z = T(b.z); return *this; }

    //! Explicit copy from/to a DD::Image::Vector3.
    void               fromDDImage(const DD::Image::Vector3& b) { x = T(b.x); y = T(b.y); z = T(b.z); }
    void               toDDImage(DD::Image::Vector3& out) const;
    DD::Image::Vector3 asDDImage() const;

    //! Add this to a DD::Image::Hash object, for DD::Image compatibility convenience.
    void append(DD::Image::Hash& hash) const;


    /*---------------------------*/
    /*   Matrix Multiplication   */
    /*---------------------------*/
    Vec3  operator *  (const Mat4<T>& m) const { Vec3<T> o; return m.transform(*this, o); }
    Vec3& operator *= (const Mat4<T>& m)       { Vec3<T> o = *this; return m.transform(o, *this); }

    /*---------------------------*/
    /*      Multiplication       */
    /*---------------------------*/
    Vec3  operator *  (T d)           const { return Vec3(x*d, y*d, z*d); }
    Vec3  operator *  (const Vec3& v) const { return Vec3(x*v.x, y*v.y, z*v.z); }
    Vec3& operator *= (T d)           { x *= d; y *= d; z *= d; return *this; }
    Vec3& operator *= (const Vec3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }

    /*---------------------------*/
    /*          Division         */
    /*---------------------------*/
    Vec3  operator /  (T d)           const { return (*this * ((T)1/d)); }
    Vec3  operator /  (const Vec3& v) const { return Vec3(x/v.x, y/v.y, z/v.z); }
    Vec3& operator /= (T d)           { return *this *=((T)1/d); }
    Vec3& operator /= (const Vec3& v) { x /= v.x; y /= v.y; z /= v.z; return *this; }

    /*---------------------------*/
    /*          Addition         */
    /*---------------------------*/
    Vec3  operator +  (T d)           const { return Vec3(x+d, y+d, z+d); }
    Vec3  operator +  (const Vec3& v) const { return Vec3(x+v.x, y+v.y, z+v.z); }
    Vec3& operator += (T d)           { x += d; y += d; z += d; return *this; }
    Vec3& operator += (const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }

    /*---------------------------*/
    /*        Subtraction        */
    /*---------------------------*/
    Vec3  operator -  (T d)           const { return Vec3(x-d, y-d, z-d); }
    Vec3  operator -  (const Vec3& v) const { return Vec3(x-v.x, y-v.y, z-v.z); }
    Vec3& operator -= (T d)           { x -= d; y -= d; z -= d; return *this; }
    Vec3& operator -= (const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }

    /*---------------------------*/
    /*        Negation           */
    /*---------------------------*/
    Vec3  operator -  () const { return Vec3(-x, -y, -z); }
    void negate()              { x = -x; y = -y; z = -z; }

    /*---------------------------*/
    /*         Equality          */
    /*---------------------------*/
    bool operator == (const Vec3& v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator != (const Vec3& v) const { return x!=v.x || y!=v.y || z!=v.z; }
    bool operator == (T d)           const { return x==d && y==d && z==d; }
    bool operator != (T d)           const { return x!=d || y!=d || z!=d; }


    /*---------------------------*/
    /*      Vector Functions     */
    /*---------------------------*/

    //! Also known as the absolute value or magnitude of the vector.
    T    length() const { return std::sqrt(x*x + y*y + z*z); }

    //! Same as this dot this, length() squared.
    T    lengthSquared() const { return (x*x + y*y + z*z); }

    //! Same as (this-v).length()
    T    distanceBetween(const Vec3& v) const { return std::sqrt((x-v.x)*(x-v.x) + (y-v.y)*(y-v.y) + (z-v.z)*(z-v.z)); }

    //! Same as (this-v).lengthSquared()
    T    distanceSquared(const Vec3& v) const { return (x-v.x)*(x-v.x) + (y-v.y)*(y-v.y) + (z-v.z)*(z-v.z); }

    //! Return the scalar distance to the plane defined by ABCD.
    T    distanceFromPlane(T A, T B, T C, T D) const { return (A*x + B*y + C*z + D); }

    //! Dot product. Twice the area of the triangle between the vectors.
    T    dot(const Vec3& v) const { return (x*v.x + y*v.y + z*v.z); }

    //! Cross product. Returns a vector at right angles to the vectors.
    Vec3 cross(const Vec3& v) const { return Vec3(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x); }

    //! Change the vector to be unit length. Returns the original length.
    T    normalize() { T d = this->length(); if (d > (T)0) *this /= d; return d; }

    //! Return a vector of this one reflected around a normal vector.
    Vec3 reflect(const Vec3& N) const { return (N * (this->dot(N) * (T)2) - *this); }

    //! Negate (flip) vector if it points in the opposite direction of N.
    void faceForward(const Vec3& N) { if (this->dot(N) < (T)0) { x=-x; y=-y; z=-z; } }

    //! Returns the minimum element.
    T    minimum()              const { return std::min(x, std::min(y, z)); }
    Vec3 minimum(const Vec3& v) const { return Vec3(std::min(v.x, x), std::min(v.y, y), std::min(v.z, z)); }

    //! Returns the maximum element.
    T    maximum()              const { return std::max(x, std::max(y, z)); }
    Vec3 maximum(const Vec3& v) const { return Vec3(std::max(v.x, x), std::max(v.y, y), std::max(v.z, z)); }

    //! Returns the absolute value of the largest element.
    T    largestAxis() const { return std::max(::fabs(x), std::max(::fabs(y), ::fabs(z))); }

    //! Linear-interpolate between this Vec3 and another at t, where t=0..1.
    template<typename S>
    Vec3<T> interpolateTo(const Vec3<T>& b,
                          S              t) const;
    template<typename S>
    Vec3<T>        lerpTo(const Vec3<T>& b,
                          S              t) const;

    //! Convert to/from radians/degrees.
    void toRadians()       { x = radians(x); y = radians(y); z = radians(z); }
    Vec3 asRadians() const { return Vec3(radians(x), radians(y), radians(z)); }
    void toDegrees()       { x = degrees(x); y = degrees(y); z = degrees(z); }
    Vec3 asDegrees() const { return Vec3(degrees(x), degrees(y), degrees(z)); }

    bool isZero()          const { return !notZero(); }
    bool notZero()         const { return (x != (T)0 || y != (T)0 || z != (T)0); }
    bool greaterThanZero() const { return !(x <= (T)0 || y <= (T)0 || z <= (T)0); }

    //! Round off each element if nearly one or zero.
    void roundIfNearlyZero();
    void roundIfNearlyOne();

    /*! Orient a vector relative to a normal's frame.
        The +Z axis of the input vector is rotated to line up with the normal.
        If N.z is negative then the up orientation of the resulting vector is
        flipped to avoid the degenerate case where N.z gets near -1.0 and there's
        no rotation solution.
    */
    void orientAroundNormal(Vec3 N,
                            bool auto_flip=true);

    //! Rotate a vector by an angle around a center axis vector.
    void rotateAroundAxis(T           angle,
                          const Vec3& axis);


  private:
    template <typename R, typename S> struct isSameType       { enum {value = 0}; };
    template <typename R>             struct isSameType<R, R> { enum {value = 1}; };
};


/*----------------------------------*/
/* Typedefs for standard data types */
/*----------------------------------*/
typedef Vec3<float>  Vec3f;
typedef Vec3<double> Vec3d;
typedef Vec3<int>    Vec3i;


/*----------------------------------*/
/*        Static operations         */
/*----------------------------------*/

//! Print out components to a stream.
template<typename T>
std::ostream& operator << (std::ostream& o, const Vec3<T>& v);


/*! Apply an euler rotation filter to a series of rotation keyframes (in degrees).
    rot_order can be 'XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', or 'ZYX'.  Default is 'ZXY'.
*/
template<typename T>
void eulerFilterRotations(std::vector<Vec3<T> >& rotations_in_degrees,
                          Fsr::RotationOrder     rot_order=Fsr::ZXY_ORDER);


//! Linear-interpolate between two Vec3s at t, where t=0..1.
template<typename T, typename S>
Vec3<T> lerp(const Vec3<T>& v0,
             const Vec3<T>& v1,
             S              t);
//! Linear-interpolate between two Vec3s at t, where t=0..1, and inv is 1-t.
template<typename T, typename S>
Vec3<T> lerp(const Vec3<T>& v0,
             const Vec3<T>& v1,
             S              t,
             S              invt);

//! Interpolate between three Vec3s at barycentric coord st.
template<typename T, typename S>
Vec3<T> interpolateAtBaryCoord(const Fsr::Vec3<T>& v0,
                               const Fsr::Vec3<T>& v1,
                               const Fsr::Vec3<T>& v2,
                               const Fsr::Vec2<S>& st);
//! Interpolate between three Vec3s at barycentric coord st, with derivatives.
template<typename T, typename S>
void    interpolateAtBaryCoord(const Fsr::Vec3<T>& v0,
                               const Fsr::Vec3<T>& v1,
                               const Fsr::Vec3<T>& v2,
                               const Fsr::Vec2<S>& st,
                               const Fsr::Vec2<S>& stdx,
                               const Fsr::Vec2<S>& stdy,
                               Fsr::Vec3<T>&       out,
                               Fsr::Vec3<T>&       duout,
                               Fsr::Vec3<T>&       dvout);


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Vec3<T>& v)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '[' << v.x << ' ' << v.y << ' ' << v.z << ']';
    return o;
}

//-----------------------------------------------------------

// Copy and normalize in same ctor.
template<typename T>
template<typename S>
inline Vec3<T>::Vec3(const Vec3<S>& v,
                     float          n) :
    x(T(v.x)),
    y(T(v.y)),
    z(T(v.z))
{
    if (n > 0.0f)
    {
        T d = this->length();
        if (d > (T)0)
        {
            d = (T)1 / d;
            x *= d;
            y *= d;
            z *= d;
        }
    }
}

// Copy to/from a DD::Image::Vector3.
template<typename T>
inline void
Vec3<T>::toDDImage(DD::Image::Vector3& out) const { out.x = float(x); out.y = float(y); out.z = float(z); }

template<typename T>
inline DD::Image::Vector3
Vec3<T>::asDDImage() const { return DD::Image::Vector3(float(x), float(y), float(z)); }

template<typename T>
inline Vec3<T>::operator DD::Image::Vector3() const
{
    if (isSameType<float,T>::value)
        return *reinterpret_cast<DD::Image::Vector3*>(const_cast<Vec3<T>* >(this));
    else
        return this->asDDImage();
}

template<typename T>
inline void
Vec3<T>::append(DD::Image::Hash& hash) const
{
    hash.append(this->array(), 3*sizeof(T));
}

//-----------------------------------------------------------

template<typename T>
inline Vec3<float>
Vec3<T>::asVec3f() const
{
    if (isSameType<float,T>::value)
        return *this;
    else
        return Vec3<float>(float(x), float(y), float(z));
}
template<typename T>
inline Vec3<T>::operator Vec3<float>() const { return this->asVec3f(); }

template<typename T>
inline Vec3<double>
Vec3<T>::asVec3d() const
{
    if (isSameType<double,T>::value)
        return *this;
    else
        return Vec3<double>(double(x), double(y), double(z));
}
template<typename T>
inline Vec3<T>::operator Vec3<double>() const { return this->asVec3d(); }

template<typename T>
inline Vec3<int>
Vec3<T>::asVec3i() const
{
    if (isSameType<int,T>::value)
        return *this;
    else
        return Vec3<int>(int(x), int(y), int(z));
}
template<typename T>
inline Vec3<T>::operator Vec3<int>() const { return this->asVec3i(); }

//-----------------------------------------------------------

template<typename T>
template<typename S>
inline Vec3<T>
Vec3<T>::interpolateTo(const Vec3<T>& b,
                       S              t) const
{
    if (t < std::numeric_limits<S>::epsilon())
        return *this; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return b; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Vec3<T>(this->x*invtT + b.x*tT, this->y*invtT + b.y*tT, this->z*invtT + b.z*tT);
}
template<typename T>
template<typename S>
inline Vec3<T>
Vec3<T>::lerpTo(const Vec3<T>& b, S t) const { return this->interpolateTo(b, t); }

template<typename T, typename S>
inline Vec3<T>
lerp(const Vec3<T>& v0,
     const Vec3<T>& v1,
     S              t)
{
    if (t < std::numeric_limits<S>::epsilon())
        return v0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return v1; // at or after last
    const T tT    = T(t);
    const T invtT = (T)1 - tT;
    return Vec3<T>(v0.x*invtT + v1.x*tT, v0.y*invtT + v1.y*tT, v0.z*invtT + v1.z*tT);
}
template<typename T, typename S>
inline Vec3<T>
lerp(const Vec3<T>& v0,
     const Vec3<T>& v1,
     S              t,
     S              invt)
{
    if (t < std::numeric_limits<S>::epsilon())
        return v0; // before or at first
    else if (t > ((S)1 - std::numeric_limits<S>::epsilon()))
        return v1; // at or after last
    const T tT    = T(t);
    const T invtT = T(invt);
    return Vec3<T>(v0.x*invtT + v1.x*tT, v0.y*invtT + v1.y*tT, v0.z*invtT + v1.z*tT);
}

// Interpolate between three Vec3s at barycentric coord st.
template<typename T, typename S>
inline Vec3<T>
interpolateAtBaryCoord(const Fsr::Vec3<T>& v0,
                       const Fsr::Vec3<T>& v1,
                       const Fsr::Vec3<T>& v2,
                       const Fsr::Vec2<S>& st)
{
    return Vec3<T>(v0 + ((v1 - v0)*T(st.x)) + ((v2 - v0)*T(st.y)));
}

// Interpolate between three Vec3s at barycentric coord st, with derivatives.
template<typename T, typename S>
inline void
interpolateAtBaryCoord(const Fsr::Vec3<T>& v0,
                       const Fsr::Vec3<T>& v1,
                       const Fsr::Vec3<T>& v2,
                       const Fsr::Vec2<S>& st,
                       const Fsr::Vec2<S>& stdx,
                       const Fsr::Vec2<S>& stdy,
                       Fsr::Vec3<T>&       out,
                       Fsr::Vec3<T>&       duout,
                       Fsr::Vec3<T>&       dvout)
{
    const Fsr::Vec3<T> e01 = (v1 - v0);
    const Fsr::Vec3<T> e02 = (v2 - v0);
    const Fsr::Vec3<T> dt = (e01*T(st.x)) + (e02*T(st.y));
    out   = v0 + dt;
    duout = (e01*T(stdx.x)) + (e02*T(stdx.y)) - dt;
    dvout = (e01*T(stdy.x)) + (e02*T(stdy.y)) - dt;
}

//-----------------------------------------------------------

template<typename T>
inline void
Vec3<T>::roundIfNearlyZero()
{
    if (::fabs(x) < std::numeric_limits<T>::epsilon()) x = (T)0;
    if (::fabs(y) < std::numeric_limits<T>::epsilon()) y = (T)0;
    if (::fabs(z) < std::numeric_limits<T>::epsilon()) z = (T)0;
}

template<typename T>
inline void
Vec3<T>::roundIfNearlyOne()
{
    if (::fabs((T)1-x) <= std::numeric_limits<T>::epsilon()) x = (T)1;
    if (::fabs((T)1-y) <= std::numeric_limits<T>::epsilon()) y = (T)1;
    if (::fabs((T)1-z) <= std::numeric_limits<T>::epsilon()) z = (T)1;
}

template<typename T>
inline void
Vec3<T>::orientAroundNormal(Vec3<T> N,
                            bool    auto_flip)
{
    Vec3<T> in(*this);
    // Flip normal to solve degenerate case of N.z near -1.0, then flip result back:
    if (N.z < 0.0f && auto_flip)
    {
        const Vec3<T> iN = -N;
        const T s = (T)1 / ((T)1 + N.z);
        x = -(in.x*( iN.z + ( iN.y*iN.y*s)) + in.y*(        (-iN.x*iN.y*s)) + in.z*iN.x),
        y = -(in.x*(        (-iN.x*iN.y*s)) + in.y*( iN.z + ( iN.x*iN.x*s)) + in.z*iN.y),
        z = -(in.x*(-iN.x                 ) + in.y*(-iN.y                 ) + in.z*iN.z);
    }
    // No flipping required:
    const T s = (T)1 / ((T)1 + N.z);
    x = in.x*( N.z + ( N.y*N.y*s)) + in.y*(       (-N.x*N.y*s)) + in.z*N.x;
    y = in.x*(       (-N.x*N.y*s)) + in.y*( N.z + ( N.x*N.x*s)) + in.z*N.y;
    z = in.x*(-N.x               ) + in.y*(-N.y               ) + in.z*N.z;
}

/*! Rotate a vector by an angle around a center axis vector.
    ex. Nnew = rotateAroundAxis(dir, -acosf(N.z), Vec3f(N.y, -N.x, 0.0f));
*/
template<typename T>
inline void
Vec3<T>::rotateAroundAxis(T              angle,
                          const Vec3<T>& axis)
{
    const T s  = std::sin(angle);
    const T c  = std::cos(angle);
    const T ic = ((T)1 - c);
    Vec3<T> in(*this);
    x = in.x*(axis.x*axis.x*ic + c         ) + in.y*(axis.y*axis.x*ic - (axis.z*s)) + in.z*(axis.z*axis.x*ic + (axis.y*s));
    y = in.x*(axis.x*axis.y*ic + (axis.z*s)) + in.y*(axis.y*axis.y*ic + c         ) + in.z*(axis.z*axis.y*ic - (axis.x*s));
    z = in.x*(axis.x*axis.z*ic - (axis.y*s)) + in.y*(axis.y*axis.z*ic + (axis.x*s)) + in.z*(axis.z*axis.z*ic + c         );
}


//! Align a rotation angle to another angle by iteritively 'unwinding' it.
template<typename T>
inline T alignAngle(T angle,
                    T to)
{
    while (::fabs(to - angle) > (T)180)
    {
        if (angle > to)
            angle -= (T)360;
        else
            angle += (T)360;
    }
    return angle;
}


template<typename T>
inline void
eulerFilterRotations(std::vector<Vec3<T> >& rotations_in_degrees,
                     Fsr::RotationOrder     rot_order)
{
    const size_t nKeys = rotations_in_degrees.size();
    if (nKeys < 2)
        return; // don't bother...

    // Get the axis order to rotate about:
    int axis0, axis1, axis2;
    switch (rot_order)
    {
        default:
        case XYZ_ORDER: axis0 = 0; axis1 = 1; axis2 = 2; break;
        case XZY_ORDER: axis0 = 0; axis1 = 2; axis2 = 1; break;
        case YXZ_ORDER: axis0 = 1; axis1 = 0; axis2 = 2; break;
        case YZX_ORDER: axis0 = 1; axis1 = 2; axis2 = 0; break;
        case ZXY_ORDER: axis0 = 2; axis1 = 0; axis2 = 1; break;
        case ZYX_ORDER: axis0 = 2; axis1 = 1; axis2 = 0; break;
    }

    // Find rotation keys with an angle change exceeding 180 degrees
    // and flip them by 180 if so:
    Vec3<T> prev, cur, flip;
    prev = rotations_in_degrees[0];
    for (size_t i=1; i < nKeys; ++i)
    {
        Vec3<T>& rotation = rotations_in_degrees[i];
        cur.x = alignAngle(rotation.x, prev.x);
        cur.y = alignAngle(rotation.y, prev.y);
        cur.z = alignAngle(rotation.z, prev.z);
        flip[axis0] = cur[axis0] + (T)180;
        flip[axis1] =-cur[axis1] + (T)180;
        flip[axis2] = cur[axis2] + (T)180;
        flip.x = alignAngle(flip.x, prev.x);
        flip.y = alignAngle(flip.y, prev.y);
        flip.z = alignAngle(flip.z, prev.z);
        // Update the rotation value from the smallest angle delta:
        const double dRot  = ::fabs(prev.x -  cur.x) + ::fabs(prev.y -  cur.y) + ::fabs(prev.z -  cur.z);
        const double dFlip = ::fabs(prev.x - flip.x) + ::fabs(prev.y - flip.y) + ::fabs(prev.z - flip.z);
        rotation = (dFlip < dRot)?flip:cur;
        prev = cur;
    }
}


} //namespace Fsr


#endif

// end of Fuser/Vec3.h

//
// Copyright 2019 DreamWorks Animation
//
