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

/// @file Fuser/Mat4.h
///
/// @author Jonathan Egstad

#ifndef Fuser_Mat4_h
#define Fuser_Mat4_h

#include "Vec3.h"
#include "Vec4.h"
#include "Box3.h"

#include <DDImage/Matrix4.h> // for DD::Image compatibility convenience

#include <string.h> // For memset, memcmp & memcpy
#include <iostream>
#include <limits> // for numeric_limits<T>


namespace Fsr {

// These match the transform order enums in DD::Image (Axis_KnobI, Matrix4)
enum XformOrder
{
    SRT_ORDER=0, STR_ORDER=1,
    RST_ORDER=2, RTS_ORDER=3,
    TSR_ORDER=4, TRS_ORDER=5
};
extern FSR_EXPORT const char* xform_orders[];

// These match the enum order in DD::Image::LookAt:
enum AxisDirection
{
    AXIS_X_MINUS=0, AXIS_X_PLUS=1,
    AXIS_Y_MINUS=2, AXIS_Y_PLUS=3,
    AXIS_Z_MINUS=4, AXIS_Z_PLUS=5
};
extern FSR_EXPORT const char* axis_directions[];


/*! \class Fsr::Mat4

    \brief A 4x4 transformation matrix. You multiply a Vec4 by one of these
    to go from a transformed space to normal space.

    The data is stored packed together in OpenGL order, which is transposed
    from the way used in most modern graphics literature. This affects
    how the array() and [] operator work. You can directly access the
    entries with the aRC members, where R is the row and C is the column.

    For instance matrix.a03 is the top-right corner of the matrix in
    most literature. It is multiplied by the W of a vector to produce
    part of the X of the output vector, and can be considered the X
    translation of the matrix.

    However matrix.a03 is matrix[3][0], and is matrix.array()[12].

    a00, a10, a20, a30,
    a01, a11, a21, a31,
    a02, a12, a22, a32,
    a03, a13, a23, a33
*/
template <typename T>
class FSR_EXPORT Mat4
{
  public:
    // Matrix is stored in transposed order (relies on C++ packing it correctly!):
    T
    /*        row0 row1 row2 row3 */
    /*col 0*/ a00, a10, a20, a30,
    /*col 1*/ a01, a11, a21, a31,
    /*col 2*/ a02, a12, a22, a32,
    /*col 2*/ a03, a13, a23, a33;

    /*---------------------------*/
    /*       Constructors        */
    /*---------------------------*/
    //! Default constructor leaves garbage in contents.
    Mat4() {}

    //! Copy constructor.
    Mat4(const T array[16]) { memcpy(&a00, array, 16*sizeof(T)); }

    //! Initialize to identity matrix with a constant in the diagonal.
    Mat4(T d) { setToZero(); a00=a11=a22=a33=d; }

    //! Initialize with a00=a, a01=b, a02=c, etc. ie the arguments are given as rows.
    Mat4(T a, T b, T c, T d,   
         T e, T f, T g, T h,
         T i, T j, T k, T l,
         T m, T n, T o, T p)
        { setTo(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }

    //! For DD::Image compatibility convenience.
    explicit Mat4(const DD::Image::Matrix4& b) { *this = b; }
    Mat4& operator =  (const DD::Image::Matrix4& b);

    /*---------------------------*/
    /*     Component Access      */
    /*---------------------------*/
    //! Return a pointer to the start of column 'c'.
    T*       operator [] (int c)       { return &a00 + c*4; }
    const T* operator [] (int c) const { return &a00 + c*4; }

    //! Return a pointer to the first element a00.
    T*       array()       { return &a00; }
    const T* array() const { return &a00; }

    //! Return the value of matrix element a00 + 'i'.
    T&       element(int i)       { return *(&a00 + i); }
    const T& element(int i) const { return *(&a00 + i); }

    //! Return row 0 as a Vec4.
    Vec4<T> row0() const { return Vec4<T>(a00, a01, a02, a03); }
    //! Return row 1 as a Vec4.
    Vec4<T> row1() const { return Vec4<T>(a10, a11, a12, a13); }
    //! Return row 2 as a Vec4.
    Vec4<T> row2() const { return Vec4<T>(a20, a21, a22, a23); }
    //! Return row 3 as a Vec4.
    Vec4<T> row3() const { return Vec4<T>(a20, a21, a22, a33); }
    //! Return column 0 as a Vec4.
    Vec4<T> col0() const { return Vec4<T>(a00, a10, a20, a30); }
    //! Return column 1 as a Vec4.
    Vec4<T> col1() const { return Vec4<T>(a01, a11, a21, a31); }
    //! Return column 2 as a Vec4.
    Vec4<T> col2() const { return Vec4<T>(a02, a12, a22, a32); }
    //! Return column 3 as a Vec4.
    Vec4<T> col3() const { return Vec4<T>(a02, a12, a22, a33); }

    //! Assign row 0, 1, or 2 from a Vec3.
    template<typename S>
    void    setRow0(const Vec3<S>& v) { a00 = T(v.x); a01 = T(v.y); a02 = T(v.z); }
    template<typename S>
    void    setRow1(const Vec3<S>& v) { a10 = T(v.x); a11 = T(v.y); a12 = T(v.z); }
    template<typename S>
    void    setRow2(const Vec3<S>& v) { a20 = T(v.x); a21 = T(v.y); a22 = T(v.z); }

    //! Return col 0, 1 or 2 as a Vec3.
    template<typename S>
    void    getXAxis(Vec3<S>& v) const { v.x = S(a00); v.y = S(a10); v.z = S(a20); }
    Vec3<T> getXAxis()              const { return Vec3<T>(a00, a10, a20); }

    template<typename S>
    void    getYAxis(Vec3<S>& v) const { v.x = S(a01); v.y = S(a11); v.z = S(a21); }
    Vec3<T> getYAxis()              const { return Vec3<T>(a01, a11, a21); }

    template<typename S>
    void    getZAxis(Vec3<S>& v) const { v.x = S(a02); v.y = S(a12); v.z = S(a22); }
    Vec3<T> getZAxis()              const { return Vec3<T>(a02, a12, a22); }

    //! Assign col 0, 1, or 2 from a Vec3.
    template<typename S>
    void    setXAxis(const Vec3<S>& v) { a00 = T(v.x); a10 = T(v.y); a20 = T(v.z); }
    template<typename S>
    void    setYAxis(const Vec3<S>& v) { a01 = T(v.x); a11 = T(v.y); a21 = T(v.z); }
    template<typename S>
    void    setZAxis(const Vec3<S>& v) { a02 = T(v.x); a12 = T(v.y); a22 = T(v.z); }

    //! Return the translations or scale of the matrix as a Vec3.
    template<typename S>
    void    getTranslation(Vec3<S>& v) const { v.x = S(a03); v.y = S(a13); v.z = S(a23); }
    Vec3<T> getTranslation()              const { return Vec3<T>(a03, a13, a23); }

    template<typename S>
    void    getScaleAxis(Vec3<S>& v) const { v.x = S(a00); v.y = S(a11); v.z = S(a22); }
    Vec3<T> getScaleAxis()              const { return Vec3<T>(a00, a11, a22); }

    /*! Extract the rotation angles (in radians) from the matrix.
        The matrix is assumed to have no shear or non-uniform scaling.
    */
    Vec3<T> getRotations(Fsr::RotationOrder order) const;
    void    getRotations(Fsr::RotationOrder order,
                         T&                 radian_rX,
                         T&                 radian_rY,
                         T&                 radian_rZ) const;

    //! Return the scaling of the matrix.
    Vec3<T> getScale() const;

    /*---------------------------*/
    /*         Assignment        */
    /*---------------------------*/
    //! Set each component with a00=a, a01=b, a02=c, etc. ie the arguments are given as rows.
    void setTo(T a, T b, T c, T d,   
               T e, T f, T g, T h,
               T i, T j, T k, T l,
               T m, T n, T o, T p)
    {
        a00=a; a01=b; a02=c; a03=d;
        a10=e; a11=f; a12=g; a13=h;
        a20=i; a21=j; a22=k; a23=l;
        a30=m; a31=n; a32=o; a33=p;
    }
    //! Set all components to 0 or 1.
    void setToZero() { memset(&a00, 0, 16*sizeof(T)); }
    void setToOne();

    //! Type-specific clear. Set all components to 0.
    void clear() { setToZero(); }

    //! Destructive copy operator.
    Mat4& operator =  (const Mat4& b) { memcpy(this, &b, sizeof(b)); return *this; }

    //! Copy from/to a DD::Image::Matrix4.
    void               fromDDImage(const DD::Image::Matrix4& in);
    void               toDDImage(DD::Image::Matrix4& out) const;
    DD::Image::Matrix4 asDDImage() const;

    /*---------------------------*/
    /*      Multiplication       */
    /*---------------------------*/
    Mat4  operator *  (const Mat4& b) const;
    Mat4& operator *= (const Mat4& b);
    // For DD::Image compatibility convenience:
    Mat4  operator *  (const DD::Image::Matrix4& b) const { return (*this *= Mat4<T>(b)); }
    Mat4& operator *= (const DD::Image::Matrix4& b) { *this *= Mat4<T>(b); return *this; }

    /*---------------------------*/
    /*         Equality          */
    /*---------------------------*/
    //! Returns true if all 16 locations are equal.
    bool operator == (const Mat4& b) const { return memcmp(&a00, &b.a00, 16*sizeof(T)) == 0; }
    //! Returns true if any of the 16 locations are different.
    bool operator != (const Mat4& b) const { return memcmp(&a00, &b.a00, 16*sizeof(T)) != 0; }

    /*---------------------------*/
    /*   Vector Multiplication   */
    /*---------------------------*/
    template<typename S, typename R>
    Vec3<R>& transform(const Vec3<S>& in,
                       Vec3<R>&       out) const;
    template<typename S>
    Vec3<S>  operator * (const Vec3<S>& v) const { Vec3<S> o; return transform(v, o); }
    template<typename S>
    Vec3<S>  transform(const Vec3<S>& v) const { return *this * v; }
    template<typename S>
    void transform(Vec3<S>*       dstPoints,
                   const Vec3<S>* srcPoints,
                   size_t         nPoints) const;

    template<typename S, typename R>
    Vec4<R>& transform(const Vec4<S>& in,
                       Vec4<R>&       out) const;
    template<typename S>
    Vec4<S>  operator * (const Vec4<S>& v) const { Vec4<S> o; return transform(v, o); }
    template<typename S>
    Vec4<S>  transform(const Vec4<S>& v) const { return *this * v; }

    //! Transform a vector with no translation applied.
    template<typename S>
    Vec3<S> vecTransform(const Vec3<S>& v) const;
    template<typename S>
    void vecTransform(Vec3<S>*       dstVecs,
                      const Vec3<S>* srcVecs,
                      size_t         nVecs) const;

    //! Transform a normal - same as transpose().transform(n).
    template<typename S>
    Vec3<S> normalTransform(const Vec3<S>& n) const;
    template<typename S>
    void normalTransform(Vec3<S>*       dstNormals,
                         const Vec3<S>* srcNormalss,
                         size_t         nNormals) const;

    template<typename S>
    Box3<S>    transform(const Box3<S>& bbox) const;

    // For DD::Image compatibility convenience:
    DD::Image::Vector3       transform(const DD::Image::Vector3& v) const;
    DD::Image::Vector3    vecTransform(const DD::Image::Vector3& v) const;
    DD::Image::Vector3 normalTransform(const DD::Image::Vector3& v) const;

    /*---------------------------*/
    /*         Inversion         */
    /*---------------------------*/
    //! Return the determinant. Non-zero means the matrix can be inverted.
    T     getDeterminant() const;
    /*! Replace the contents of \a to with the inverse of this, where \a det
        is the already-calculated determinant of this and must be non-zero.
        <i>&to must be a different matrix than this!</i>
    */
    void  invert(Mat4& out,
                 T     determinant) const;
    /*! Replace the contents of \a to with the inverse of this and return
        the determinant.  If this cannot be inverted \a to is unchanged
        and zero is returned.
        <i>&to must be a different matrix than this!</i>
    */
    T     invert(Mat4& out) const;
    //! Invert this matrix in place.
    Mat4& invert() { Mat4 t; invert(t); return *this = t; }
    //! Returns the inverse of this matrix. Returns garbage if this cannot be inverted.
    Mat4  inverse() const { Mat4 t; invert(t); return t; }
    //! Returns the inverse of this matrix (must supply a precomputed non-zero determinant)
    Mat4  inverse(T determinant) const { Mat4 t; invert(t, determinant); return t; }

    /*---------------------------*/
    /*         Identity          */
    /*---------------------------*/
    //! Return the identity matrix object.
    static const Mat4& getIdentity() { return m_identity; }

    //! Set the matrix to the identity.
    void setToIdentity() { *this = Mat4<T>::getIdentity(); }

    //! Returns true if matrix is an identity matrix.
    bool isIdentity()    const { return (*this == Mat4<T>::getIdentity()); }
    bool isNotIdentity() const { return !isIdentity(); }

    /*---------------------------*/
    /*      Scale Assignment     */
    /*---------------------------*/
    //! Set the contents to a uniform scale by \a d.
    Mat4& setToScale(T d);
    //! Set the contents to a scale by \a x,y,z.
    Mat4& setToScale(T sx,
                     T sy,
                     T sz=(T)1);
    template<typename S>
    Mat4& setToScale(const Vec3<S>& s) { return setToScale(T(s.x), T(s.y), T(s.z)); }

    /*---------------------------*/
    /*  Translation Assignment   */
    /*---------------------------*/
    //! Set the contents to a translation by \a x,y,z.
    Mat4& setToTranslation(T tx,
                           T ty,
                           T tz=(T)0);
    template<typename S>
    Mat4& setToTranslation(const Vec3<S>& t) { return setToTranslation(T(t.x), T(t.y), T(t.z)); }

    /*---------------------------*/
    /*     Rotation Assignment   */
    /*---------------------------*/
    //! Set the contents to an angle (in radians) around the X axis.
    Mat4& setToRotationX(T radian_angle);
    //! Set the contents to an angle (in radians) around the Y axis.
    Mat4& setToRotationY(T radian_angle);
    //! Set the contents to an angle (in radians) around the Z axis.
    Mat4& setToRotationZ(T radian_angle);
    //! Set the contents to an angle (in radians) around about the vector x,y,z.
    Mat4& setToRotation(T radian_angle,
                        T x, T y, T z);
    template<typename S>
    Mat4& setToRotation(T              radian_angle,
                        const Vec3<S>& v) { return setToRotation(radian_angle, T(v.x), T(v.y), T(v.z)); }

    /*-----------------------------------------------*/
    /*  Single-step SRT Transform Handling           */
    /*    * rotations are assumed to be in degrees   */
    /*    * skew always follows rotation             */
    /*-----------------------------------------------*/
    template<typename S>
    void applyTransform(Fsr::XformOrder    xformOrder, 
                        Fsr::RotationOrder rotOrder,
                        const Vec3<S>&     translation,
                        const Vec3<S>&     rotations_in_degrees,
                        const Vec3<S>&     scaling,
                        const Vec3<S>&     skewing,
                        const Vec3<S>&     pivot);
    template<typename S>
    void setToTransform(Fsr::XformOrder    xformOrder, 
                        Fsr::RotationOrder rotOrder,
                        const Vec3<S>&     translation,
                        const Vec3<S>&     rotations_in_degrees,
                        const Vec3<S>&     scaling,
                        const Vec3<S>&     skewing,
                        const Vec3<S>&     pivot);

    /*---------------------------*/
    /*    Destructive Modifiers  */
    /*---------------------------*/
    //! Replace the contents with the transposition (reflect through diagonal)
    void transpose();

    //! Scale the transformation by uniform scale \a d.
    void scale(T d);
    //! Scale columns 0,1,2 by x,y,z.
    void scale(T sx,
               T sy,
               T sz=(T)1);
    template<typename S>
    void scale(const Vec3<S>& sv) { scale(T(sv.x), T(sv.y), T(sv.z)); }

    //! Rotate the transformation by an angle (in radians) about the X axis.
    void rotateX(T radian_angle);
    //! Rotate the transformation by an angle (in radians) about the Y axis.
    void rotateY(T radian_angle);
    //! Rotate the transformation by an angle (in radians) about the Z axis.
    void rotateZ(T radian_angle);
    //! Same as rotateZ(a).
    void rotate(T radian_angle) { rotateZ(radian_angle);}
    //! Rotate the transformation by an angle (in radians) about the vector x,y,z.
    void rotate(T radian_angle,
                T x, T y, T z);
    template<typename S>
    void rotate(T              radian_angle,
                const Vec3<S>& v) { rotate(radian_angle, T(v.x), T(v.y), T(v.z)); }

    //! Apply rotations in each axis (in radians) in specific order.
    void rotate(Fsr::RotationOrder order,
                T                  radian_x_angle,
                T                  radian_y_angle,
                T                  radian_z_angle);
    template<typename S>
    void rotate(typename Fsr::RotationOrder order,
                const Vec3<S>&              radian_angles) { rotate(order, T(radian_angles.x), T(radian_angles.y), T(radian_angles.z)); }

    //! Translate the transformation by an x,y,z offset.
    void translate(T x,
                   T y,
                   T z=T(0));
    template<typename S>
    void translate(const Vec3<S>& v) { translate(T(v.x), T(v.y), T(v.z)); }

    //! Skew the transformation by \a a (X positions have a*Y added to them).
    void skew(T a);
    template<typename S>
    void skew(const Vec3<S>& skew);

    //! Component-wise add all the elements.
    Mat4& add(const Mat4& b);
    //! Add a constant to all the elements.
    void add(T t);
    //! Add a constant to all the diagonal elements.
    void addDiagonal(T t);

#if 1
    //! Build orientation rotations.
    template<typename S>
    static void lookAt(const Vec3<S>& eye,
                       const Vec3<S>& interest,
                       AxisDirection  align_axis,
                       bool           do_rx,
                       bool           do_ry,
                       bool           do_rz,
                       T              lookat_strength,
                       Vec3<S>&       rotations_out);
#else
    //! Construct a transform to orient -Z towards the interest point.
    void lookAt(const Vec4<T>& eye,
                const Vec4<T>& interest,
                T              roll,
                bool           left_handed=true);
#endif

    /*! Linear-interpolate two matrices at offset 't' which is between 0.0 and 1.0.
        This only interpolates position and rotation, and rotation is only valid
        within a certain range since it's a linear interpolation of the xyz axes!
    */
    void interpolate(const Mat4<T>& m0,
                     const Mat4<T>& m1,
                     T              t);

    /*---------------------------------------------------------------------*/
    /*          Component extraction (decomposition) routines.             */
    /*         Some come from ImathMatAlgo or Graphics Gems.            */
    /*---------------------------------------------------------------------*/

    //! Modify the transformation matrix to represent the translation component only.
    void translationOnly();
    //! Modify the transformation matrix to represent the rotation component only.
    void rotationOnly();
    //! Modify the transformation matrix to represent the scale component only.
    void scaleOnly();
    //! Modify the transformation matrix to represent the scale and rotation component only.
    void scaleAndRotationOnly();

#if 0
    //!
    bool extractScaling(Vec3<T>& scale) const;

#endif

    //!
    bool extractScalingAndShear(Vec3<T>& scale,
                                Vec3<T>& shear) const;

    //!
    bool extractAndRemoveScalingAndShear(Vec3<T>& scale,
                                         Vec3<T>& shear);

    //! Rotations are in degrees.
    bool extractSHRT(Vec3<T>&           scaling,
                     Vec3<T>&           shearing,
                     Vec3<T>&           rotationAngles,
                     Vec3<T>&           translation,
                     Fsr::RotationOrder order = Fsr::ZXY_ORDER) const;


  private:
    static Mat4<T> m_identity;

    template <typename R, typename S> struct isSameType       { enum {value = 0}; };
    template <typename R>             struct isSameType<R, R> { enum {value = 1}; };
};



/*----------------------------------*/
/* Typedefs for standard data types */
/*----------------------------------*/
typedef Mat4<float>  Mat4f;
typedef Mat4<double> Mat4d;



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

template<typename T>
inline std::ostream& operator << (std::ostream& o, const Mat4<T>& m)
{
    //o.setf(ios::fixed, ios::floatfield);
    //o.precision(7);
    o << '['
        //        col0            col1            col2            col3
        << "[" << m.a00 << " " << m.a01 << " " << m.a02 << " " << m.a03 << "]" // row0
        << "[" << m.a10 << " " << m.a11 << " " << m.a12 << " " << m.a13 << "]" // row1
        << "[" << m.a20 << " " << m.a21 << " " << m.a22 << " " << m.a23 << "]" // row2
        << "[" << m.a30 << " " << m.a31 << " " << m.a32 << " " << m.a33 << "]" // row3
    << ']';
    return o;
}

//-----------------------------------------------------------

template<typename T>
/*static*/ Mat4<T> Mat4<T>::m_identity = Mat4<T>(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

//-----------------------------------------------------------

//! Copy from a DD::Image::Matrix4.
template<typename T>
inline void
Mat4<T>::fromDDImage(const DD::Image::Matrix4& m)
{
    if (isSameType<float,T>::value)
        memcpy(&a00, &m.a00, 16*sizeof(float));
    else
    {
        const float* mIn = &m.a00;
        T* mOut = &a00;
        for (int i=0; i < 16; ++i)
            *mOut++ = T(*mIn++);
    }
}
template<typename T>
inline Mat4<T>&
Mat4<T>::operator =  (const DD::Image::Matrix4& m)
{
    this->fromDDImage(m);
    return *this;
}

//! Copy to a DD::Image::Matrix4.
template<typename T>
inline void
Mat4<T>::toDDImage(DD::Image::Matrix4& out) const
{
    if (isSameType<float,T>::value)
        memcpy(&out.a00, &a00, 16*sizeof(float));
    else
    {
        const T* mIn = &a00;
        float* mOut = &out.a00;
        for (int i=0; i < 16; ++i)
            *mOut++ = float(*mIn++);
    }
}
template<typename T>
inline DD::Image::Matrix4
Mat4<T>::asDDImage() const
{
    DD::Image::Matrix4 m;
    this->toDDImage(m);
    return m;
}

//-----------------------------------------------------------

template<typename T>
inline Mat4<T>
Mat4<T>::operator *  (const Mat4<T>& b) const
{
    Mat4<T> m;
    m.a00 = a00*b.a00 + a01*b.a10 + a02*b.a20 + a03*b.a30;
    m.a10 = a10*b.a00 + a11*b.a10 + a12*b.a20 + a13*b.a30;
    m.a20 = a20*b.a00 + a21*b.a10 + a22*b.a20 + a23*b.a30;
    m.a30 = a30*b.a00 + a31*b.a10 + a32*b.a20 + a33*b.a30;

    m.a01 = a00*b.a01 + a01*b.a11 + a02*b.a21 + a03*b.a31;
    m.a11 = a10*b.a01 + a11*b.a11 + a12*b.a21 + a13*b.a31;
    m.a21 = a20*b.a01 + a21*b.a11 + a22*b.a21 + a23*b.a31;
    m.a31 = a30*b.a01 + a31*b.a11 + a32*b.a21 + a33*b.a31;

    m.a02 = a00*b.a02 + a01*b.a12 + a02*b.a22 + a03*b.a32;
    m.a12 = a10*b.a02 + a11*b.a12 + a12*b.a22 + a13*b.a32;
    m.a22 = a20*b.a02 + a21*b.a12 + a22*b.a22 + a23*b.a32;
    m.a32 = a30*b.a02 + a31*b.a12 + a32*b.a22 + a33*b.a32;

    m.a03 = a00*b.a03 + a01*b.a13 + a02*b.a23 + a03*b.a33;
    m.a13 = a10*b.a03 + a11*b.a13 + a12*b.a23 + a13*b.a33;
    m.a23 = a20*b.a03 + a21*b.a13 + a22*b.a23 + a23*b.a33;
    m.a33 = a30*b.a03 + a31*b.a13 + a32*b.a23 + a33*b.a33;
    return m;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::operator *= (const Mat4<T>& b)
{
    Mat4<T> m;
    m.a00 = a00*b.a00 + a01*b.a10 + a02*b.a20 + a03*b.a30;
    m.a10 = a10*b.a00 + a11*b.a10 + a12*b.a20 + a13*b.a30;
    m.a20 = a20*b.a00 + a21*b.a10 + a22*b.a20 + a23*b.a30;
    m.a30 = a30*b.a00 + a31*b.a10 + a32*b.a20 + a33*b.a30;

    m.a01 = a00*b.a01 + a01*b.a11 + a02*b.a21 + a03*b.a31;
    m.a11 = a10*b.a01 + a11*b.a11 + a12*b.a21 + a13*b.a31;
    m.a21 = a20*b.a01 + a21*b.a11 + a22*b.a21 + a23*b.a31;
    m.a31 = a30*b.a01 + a31*b.a11 + a32*b.a21 + a33*b.a31;

    m.a02 = a00*b.a02 + a01*b.a12 + a02*b.a22 + a03*b.a32;
    m.a12 = a10*b.a02 + a11*b.a12 + a12*b.a22 + a13*b.a32;
    m.a22 = a20*b.a02 + a21*b.a12 + a22*b.a22 + a23*b.a32;
    m.a32 = a30*b.a02 + a31*b.a12 + a32*b.a22 + a33*b.a32;

    m.a03 = a00*b.a03 + a01*b.a13 + a02*b.a23 + a03*b.a33;
    m.a13 = a10*b.a03 + a11*b.a13 + a12*b.a23 + a13*b.a33;
    m.a23 = a20*b.a03 + a21*b.a13 + a22*b.a23 + a23*b.a33;
    m.a33 = a30*b.a03 + a31*b.a13 + a32*b.a23 + a33*b.a33;
    *this = m;
    return *this;
}

//-----------------------------------------------------------

template<typename T>
template<typename S, typename R>
inline Vec3<R>&
Mat4<T>::transform(const Vec3<S>& in,
                   Vec3<R>&       out) const
{
    if (isSameType<S,T>::value)
    {
        out.x = R(a00*in.x + a01*in.y + a02*in.z + a03);
        out.y = R(a10*in.x + a11*in.y + a12*in.z + a13);
        out.z = R(a20*in.x + a21*in.y + a22*in.z + a23);
    }
    else
    {
        const T x = T(in.x);
        const T y = T(in.y);
        const T z = T(in.z);
        out.x = R(a00*x + a01*y + a02*z + a03);
        out.y = R(a10*x + a11*y + a12*z + a13);
        out.z = R(a20*x + a21*y + a22*z + a23);
    }
    return out;
}
//-----------------------------------------------------------
template<typename T>
template<typename S, typename R>
inline Vec4<R>&
Mat4<T>::transform(const Vec4<S>& in,
                   Vec4<R>&       out) const
{
    if (isSameType<S,T>::value)
    {
        out.x = R(a00*in.x + a01*in.y + a02*in.z + a03*in.w);
        out.y = R(a10*in.x + a11*in.y + a12*in.z + a13*in.w);
        out.z = R(a20*in.x + a21*in.y + a22*in.z + a23*in.w);
        out.w = R(a30*in.x + a31*in.y + a32*in.z + a33*in.w);
    }
    else
    {
        const T x = T(in.x);
        const T y = T(in.y);
        const T z = T(in.z);
        const T w = T(in.w);
        out.x = R(a00*x + a01*y + a02*z + a03*w);
        out.y = R(a10*x + a11*y + a12*z + a13*w);
        out.z = R(a20*x + a21*y + a22*z + a23*w);
        out.w = R(a30*x + a31*y + a32*z + a33*w);
    }
    return out;
}

template<typename T>
template<typename S>
void
Mat4<T>::transform(Vec3<S>*       dstPoints,
                   const Vec3<S>* srcPoints,
                   size_t         nPoints) const
{
    if (isIdentity())
        memcpy(dstPoints, srcPoints, nPoints*sizeof(Vec3<S>));
    else
    {
        for (size_t i=0; i < nPoints; ++i)
            transform(*srcPoints++, *dstPoints++);
    }
}

template<typename T>
inline DD::Image::Vector3
Mat4<T>::transform(const DD::Image::Vector3& v) const
{
    const Vec3f& fin = *reinterpret_cast<const Vec3f*>(&v);
    Vec3f fout;
    transform(fin, fout);
    return *reinterpret_cast<DD::Image::Vector3*>(&fout);
}

//-----------------------------------------------------------
template<typename T>
template<typename S>
inline Vec3<S>
Mat4<T>::vecTransform(const Vec3<S>& v) const
{
    if (isSameType<S,T>::value)
    {
        return Vec3<S>(a00*v.x + a01*v.y + a02*v.z,
                       a10*v.x + a11*v.y + a12*v.z,
                       a20*v.x + a21*v.y + a22*v.z);
    }
    else
    {
        const T x = T(v.x);
        const T y = T(v.y);
        const T z = T(v.z);
        return Vec3<S>(a00*x + a01*y + a02*z,
                       a10*x + a11*y + a12*z,
                       a20*x + a21*y + a22*z);
    }
}

template<typename T>
template<typename S>
void
Mat4<T>::vecTransform(Vec3<S>*       dstVecs,
                      const Vec3<S>* srcVecs,
                      size_t         nVecs) const
{
    if (isIdentity())
        memcpy(dstVecs, srcVecs, nVecs*sizeof(Vec3<S>));
    else
    {
        for (size_t i=0; i < nVecs; ++i)
            *dstVecs++ = vecTransform(*srcVecs++);
    }
}

template<typename T>
inline DD::Image::Vector3
Mat4<T>::vecTransform(const DD::Image::Vector3& v) const
{
    const Vec3f& fin = *reinterpret_cast<const Vec3f*>(&v);
    Vec3f fout = vecTransform(fin);
    return *reinterpret_cast<DD::Image::Vector3*>(&fout);
}

//-----------------------------------------------------------

template<typename T>
template<typename S>
inline Vec3<S>
Mat4<T>::normalTransform(const Vec3<S>& n) const
{
    if (isSameType<S,T>::value)
    {
        return Vec3<S>(a00*n.x + a10*n.y + a20*n.z,
                       a01*n.x + a11*n.y + a21*n.z,
                       a02*n.x + a12*n.y + a22*n.z);
    }
    else
    {
        const T x = T(n.x);
        const T y = T(n.y);
        const T z = T(n.z);
        return Vec3<S>(a00*x + a10*y + a20*z,
                       a01*x + a11*y + a21*z,
                       a02*x + a12*y + a22*z);
    }
}

template<typename T>
template<typename S>
void
Mat4<T>::normalTransform(Vec3<S>*       dstNormals,
                         const Vec3<S>* srcNormals,
                         size_t         nNormals) const
{
    if (isIdentity())
        memcpy(dstNormals, srcNormals, nNormals*sizeof(Vec3<S>));
    else
    {
        for (size_t i=0; i < nNormals; ++i)
            *dstNormals++ = normalTransform(*srcNormals++);
    }
}

template<typename T>
inline DD::Image::Vector3
Mat4<T>::normalTransform(const DD::Image::Vector3& v) const
{
    const Vec3f& fin = *reinterpret_cast<const Vec3f*>(&v);
    Vec3f fout = normalTransform(fin);
    return *reinterpret_cast<DD::Image::Vector3*>(&fout);
}

//-----------------------------------------------------------

template<typename T>
inline Vec3<T>
Mat4<T>::getScale() const
{
    return Vec3<T>(std::sqrt(a00*a00 + a10*a10 + a20*a20 + a30*a30),
                   std::sqrt(a01*a01 + a11*a11 + a21*a21 + a31*a31),
                   std::sqrt(a02*a02 + a12*a12 + a22*a22 + a32*a32));
}

//-----------------------------------------------------------

template<typename T>
inline T
Mat4<T>::getDeterminant() const
{
    return a01*a23*a32*a10 - a01*a22*a33*a10 - a23*a31*a02*a10 + a22*a31*a03*a10 -
           a00*a23*a32*a11 + a00*a22*a33*a11 + a23*a30*a02*a11 - a22*a30*a03*a11 -
           a01*a23*a30*a12 + a00*a23*a31*a12 + a01*a22*a30*a13 - a00*a22*a31*a13 -
           a33*a02*a11*a20 + a32*a03*a11*a20 + a01*a33*a12*a20 - a31*a03*a12*a20 -
           a01*a32*a13*a20 + a31*a02*a13*a20 + a33*a02*a10*a21 - a32*a03*a10*a21 -
           a00*a33*a12*a21 + a30*a03*a12*a21 + a00*a32*a13*a21 - a30*a02*a13*a21;
}
template<typename T>
inline void
Mat4<T>::invert(Mat4<T>& out,
                T        determinant) const
{
    const T idet = ((T)1 / determinant);
    out.a00 = (-a23*a32*a11 +a22*a33*a11 +a23*a31*a12 -a22*a31*a13 -a33*a12*a21 +a32*a13*a21) * idet;
    out.a01 = ( a01*a23*a32 -a01*a22*a33 -a23*a31*a02 +a22*a31*a03 +a33*a02*a21 -a32*a03*a21) * idet;
    out.a02 = (-a33*a02*a11 +a32*a03*a11 +a01*a33*a12 -a31*a03*a12 -a01*a32*a13 +a31*a02*a13) * idet;
    out.a03 = ( a23*a02*a11 -a22*a03*a11 -a01*a23*a12 +a01*a22*a13 +a03*a12*a21 -a02*a13*a21) * idet; 

    out.a10 = ( a23*a32*a10 -a22*a33*a10 -a23*a30*a12 +a22*a30*a13 +a33*a12*a20 -a32*a13*a20) * idet;
    out.a11 = (-a00*a23*a32 +a00*a22*a33 +a23*a30*a02 -a22*a30*a03 -a33*a02*a20 +a32*a03*a20) * idet;
    out.a12 = ( a33*a02*a10 -a32*a03*a10 -a00*a33*a12 +a30*a03*a12 +a00*a32*a13 -a30*a02*a13) * idet;
    out.a13 = (-a23*a02*a10 +a22*a03*a10 +a00*a23*a12 -a00*a22*a13 -a03*a12*a20 +a02*a13*a20) * idet; 

    out.a20 = (-a23*a31*a10 +a23*a30*a11 -a33*a11*a20 +a31*a13*a20 +a33*a10*a21 -a30*a13*a21) * idet;
    out.a21 = (-a01*a23*a30 +a00*a23*a31 +a01*a33*a20 -a31*a03*a20 -a00*a33*a21 +a30*a03*a21) * idet;
    out.a22 = (-a01*a33*a10 +a31*a03*a10 +a00*a33*a11 -a30*a03*a11 +a01*a30*a13 -a00*a31*a13) * idet;
    out.a23 = ( a01*a23*a10 -a00*a23*a11 +a03*a11*a20 -a01*a13*a20 -a03*a10*a21 +a00*a13*a21) * idet;

    out.a30 = ( a22*a31*a10 -a22*a30*a11 +a32*a11*a20 -a31*a12*a20 -a32*a10*a21 +a30*a12*a21) * idet;
    out.a31 = ( a01*a22*a30 -a00*a22*a31 -a01*a32*a20 +a31*a02*a20 +a00*a32*a21 -a30*a02*a21) * idet;
    out.a32 = ( a01*a32*a10 -a31*a02*a10 -a00*a32*a11 +a30*a02*a11 -a01*a30*a12 +a00*a31*a12) * idet;
    out.a33 = (-a01*a22*a10 +a00*a22*a11 -a02*a11*a20 +a01*a12*a20 +a02*a10*a21 -a00*a12*a21) * idet;
}
template<typename T>
inline T
Mat4<T>::invert(Mat4<T>& out) const
{
    const T determinant = getDeterminant();
    if (::fabs(determinant) > (T)0)
        invert(out, determinant);
    return determinant;
}

template<typename T>
inline void
Mat4<T>::setToOne()
{
    a00 = a01 = a02 = a03 = \
    a10 = a11 = a12 = a13 = \
    a20 = a21 = a22 = a23 = \
    a30 = a31 = a32 = a33 = (T)1;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::setToScale(T d)
{
    a00 = d; a01 = 0; a02 = 0; a03 = 0;
    a10 = 0; a11 = d; a12 = 0; a13 = 0;
    a20 = 0; a21 = 0; a22 = d; a23 = 0;
    a30 = 0; a31 = 0; a32 = 0; a33 = 1;
    return *this;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::setToScale(T x, T y, T z)
{
    a00 = x; a01 = 0; a02 = 0; a03 = 0;
    a10 = 0; a11 = y; a12 = 0; a13 = 0;
    a20 = 0; a21 = 0; a22 = z; a23 = 0;
    a30 = 0; a31 = 0; a32 = 0; a33 = 1;
    return *this;
}

template<typename T>
inline Mat4<T>&
Mat4<T>::setToTranslation(T x, T y, T z)
{
    a00 = 1; a01 = 0; a02 = 0; a03 = x;
    a10 = 0; a11 = 1; a12 = 0; a13 = y;
    a20 = 0; a21 = 0; a22 = 1; a23 = z;
    a30 = 0; a31 = 0; a32 = 0; a33 = 1;
    return *this;
}

template<typename T>
inline Mat4<T>&
Mat4<T>::setToRotationX(T radian_angle)
{
    const T s = std::sin(radian_angle);
    const T c = std::cos(radian_angle);
    a00 = 1; a01 = 0; a02 =  0; a03 = 0;
    a10 = 0; a11 = c; a12 = -s; a13 = 0;
    a20 = 0; a21 = s; a22 =  c; a23 = 0;
    a30 = 0; a31 = 0; a32 =  0; a33 = 1;
    return *this;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::setToRotationY(T radian_angle)
{
    const T s = std::sin(radian_angle);
    const T c = std::cos(radian_angle);
    a00 =  c; a01 = 0; a02 = s; a03 = 0;
    a10 =  0; a11 = 1; a12 = 0; a13 = 0;
    a20 = -s; a21 = 0; a22 = c; a23 = 0;
    a30 =  0; a31 = 0; a32 = 0; a33 = 1;
    return *this;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::setToRotationZ(T radian_angle)
{
    const T s = std::sin(radian_angle);
    const T c = std::cos(radian_angle);
    a00 = c; a01 = -s; a02 = 0; a03 = 0;
    a10 = s; a11 =  c; a12 = 0; a13 = 0;
    a20 = 0; a21 =  0; a22 = 1; a23 = 0;
    a30 = 0; a31 =  0; a32 = 0; a33 = 1;
    return *this;
}
template<typename T>
inline Mat4<T>&
Mat4<T>::setToRotation(T radian_angle,
                       T rX,
                       T rY,
                       T rZ)
{
    const T L = (T)1/std::sqrt(rX*rX + rY*rY + rZ*rZ);
    const T x = rX*L;
    const T y = rY*L;
    const T z = rZ*L;
    const T s = std::sin(radian_angle);
    const T c = std::cos(radian_angle);
    const T c1 = (T)1-c;
    a00 = x*x*c1+c;   a01 = y*x*c1-z*s; a02 = z*x*c1+y*s; a03 = (T)0;
    a10 = x*y*c1+z*s; a11 = y*y*c1+c;   a12 = z*y*c1-x*s; a13 = (T)0;
    a20 = x*z*c1-y*s; a21 = y*z*c1+x*s; a22 = z*z*c1+c;   a23 = (T)0;
    a30 = (T)0;       a31 = (T)0;       a32 = (T)0;       a33 = (T)1;
    return *this;
}
template<typename T>
inline void
Mat4<T>::rotate(Fsr::RotationOrder order,
                T                  radian_x_angle,
                T                  radian_y_angle,
                T                  radian_z_angle)
{
    switch (order)
    {
        case XYZ_ORDER:
            this->rotateZ(radian_z_angle); this->rotateY(radian_y_angle); this->rotateX(radian_x_angle); break;
        case XZY_ORDER:
            this->rotateY(radian_y_angle); this->rotateZ(radian_z_angle); this->rotateX(radian_x_angle); break;
        case YXZ_ORDER:
            this->rotateZ(radian_z_angle); this->rotateX(radian_x_angle); this->rotateY(radian_y_angle); break;
        case YZX_ORDER:
            this->rotateX(radian_x_angle); this->rotateZ(radian_z_angle); this->rotateY(radian_y_angle); break;
        case ZXY_ORDER:
            this->rotateY(radian_y_angle); this->rotateX(radian_x_angle); this->rotateZ(radian_z_angle); break;
        case ZYX_ORDER:
            this->rotateX(radian_x_angle); this->rotateY(radian_y_angle); this->rotateZ(radian_z_angle); break;
    }
}

template<typename T>
inline void
Mat4<T>::translate(T x,
                   T y,
                   T z)
{
    a03 += x*a00 + y*a01 + z*a02;
    a13 += x*a10 + y*a11 + z*a12;
    a23 += x*a20 + y*a21 + z*a22;
    a33 += x*a30 + y*a31 + z*a32;
}
template<typename T>
inline void
Mat4<T>::translationOnly()
{
    a00 = (T)1; a01 = (T)0; a02 = (T)0;
    a10 = (T)0; a11 = (T)1; a12 = (T)0;
    a20 = (T)0; a21 = (T)0; a22 = (T)1;
    a30 = (T)0; a31 = (T)0; a32 = (T)0; a33 = (T)1;
}

template<typename T>
inline void
Mat4<T>::scaleAndRotationOnly()
{
    a03 = a13 = a23 =
    a30 = a31 = a32 = T(0);
}
template<typename T>
inline void
Mat4<T>::rotationOnly()
{
    scaleAndRotationOnly();
    const Vec3<T> s(getScale());
    scale((T)1/s.x, (T)1/s.y, (T)1/s.z);
}
template<typename T>
inline void
Mat4<T>::scaleOnly()
{
    setToScale(getScale());
}
template<typename T>
inline void
Mat4<T>::transpose()
{
    std::swap(a01, a10);
    std::swap(a02, a20);
    std::swap(a03, a30);
    std::swap(a12, a21);
    std::swap(a13, a31);
    std::swap(a23, a32);
}
template<typename T>
inline void
Mat4<T>::scale(T s)
{
    a00 *= s; a01 *= s; a02 *= s;
    a10 *= s; a11 *= s; a12 *= s;
    a20 *= s; a21 *= s; a22 *= s;
    a30 *= s; a31 *= s; a32 *= s;
}
template<typename T>
inline void
Mat4<T>::scale(T x,
                  T y,
                  T z)
{
    a00 *= x; a01 *= y; a02 *= z;
    a10 *= x; a11 *= y; a12 *= z;
    a20 *= x; a21 *= y; a22 *= z;
    a30 *= x; a31 *= y; a32 *= z;
}
template<typename T>
inline void
Mat4<T>::rotateX(T radian_angle)
{
    if (::fabs(radian_angle) > 0.0)
        *this *= Mat4().setToRotationX(radian_angle);
}
template<typename T>
inline void
Mat4<T>::rotateY(T radian_angle)
{
    if (::fabs(radian_angle) > 0.0)
        *this *= Mat4().setToRotationY(radian_angle);
}
template<typename T>
inline void
Mat4<T>::rotateZ(T radian_angle)
{
    if (::fabs(radian_angle) > 0.0)
        *this *= Mat4().setToRotationZ(radian_angle);
}
template<typename T>
inline void
Mat4<T>::rotate(T radian_angle,
                   T x, T y, T z)
{
    if (::fabs(radian_angle) > 0.0)
        *this *= Mat4().setToRotation(radian_angle, x, y, z);
}
template<typename T>
inline void
Mat4<T>::skew(T d)
{
    if (d == (T)0)
        return;
    Mat4<T> skew;
    skew.identity();
    skew.a01 = d;
    *this *= skew;
}
template<typename T>
template<typename S>
inline void
Mat4<T>::skew(const Vec3<S>& skew)
{
    //TODO: finish this!
/*
    if (d == (T)0)
        return;
    Mat4<T> skew;
    skew.setToIdentity();
    skew.a01 = d;
    *this *= skew;
*/
}

template<typename T>
template<typename S>
inline void
Mat4<T>::applyTransform(Fsr::XformOrder    xformOrder, 
                        Fsr::RotationOrder rotOrder,
                        const Vec3<S>&     translation,
                        const Vec3<S>&     rotations_in_degrees,
                        const Vec3<S>&     scaling,
                        const Vec3<S>&     skewing,
                        const Vec3<S>&     pivot)
{
    translate(pivot); // offset to origin
    switch (xformOrder)
    {
        case SRT_ORDER:
            this->translate(translation); this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); this->scale(scaling); break;
        case STR_ORDER:
            this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); this->translate(translation); this->scale(scaling); break;
        case RST_ORDER:
            this->translate(translation); this->scale(scaling); this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); break;
        case RTS_ORDER:
            this->scale(scaling); this->translate(translation); this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); break;
        case TSR_ORDER:
            this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); this->scale(scaling); this->translate(translation); break;
        case TRS_ORDER:
            this->scale(scaling); this->rotate(rotOrder, rotations_in_degrees.asRadians()); this->skew(skewing); this->translate(translation); break;
    }
    translate(-pivot); // back to pivot location
}
template<typename T>
template<typename S>
inline void
Mat4<T>::setToTransform(Fsr::XformOrder    xformOrder, 
                        Fsr::RotationOrder rotOrder,
                        const Vec3<S>&     translation,
                        const Vec3<S>&     rotations_in_degrees,
                        const Vec3<S>&     scaling,
                        const Vec3<S>&     skewing,
                        const Vec3<S>&     pivot)
{
    this->setToIdentity();
    applyTransform(xformOrder, rotOrder, translation, rotations_in_degrees, scaling, skewing, pivot);
}

template<typename T>
inline Mat4<T>&
Mat4<T>::add(const Mat4<T>& b)
{
    T* ap = &a00;
    const T* bp = &b.a00;
    for (int i=0; i < 16; ++i)
        *ap++ += *bp++;
    return *this;
}
template<typename T>
inline void
Mat4<T>::add(T t)
{
    a00 += t; a10 += t; a20 += t; a30 += t;
    a01 += t; a11 += t; a21 += t; a31 += t;
    a02 += t; a12 += t; a22 += t; a32 += t;
    a03 += t; a13 += t; a23 += t; a33 += t;
}
template<typename T>
inline void
Mat4<T>::addDiagonal(T d)
{
    a00 += d;
        a11 += d;
            a22 += d;
                a33 += d;
}

template<typename T>
template<typename S>
/*static*/ inline void
Mat4<T>::lookAt(const Vec3<S>& eye,
                const Vec3<S>& interest,
                AxisDirection  align_axis,
                bool           do_rx,
                bool           do_ry,
                bool           do_rz,
                T              lookat_strength,
                Vec3<S>&       rotations_out)
{
#if 1
    Vec3<S> dir(interest - eye);
    const S len = dir.normalize();
    if (lookat_strength <= (S)0 || len < std::numeric_limits<S>::epsilon())
        return; // Zero-length vector so no rotation possible

    Vec3<S> look((S)0);

    // Calculate the primary rotation first then the second, and which rotation axis we
    // change is determined by the align axis:
    S d;
    switch (align_axis)
    {
        case AXIS_X_MINUS:
            if (do_ry) { look.y = ::atan2(-dir.z, dir.x); d = std::sqrt(dir.z*dir.z + dir.x*dir.x); } else d =  dir.x;
            if (do_rz) { look.z = ::atan2( dir.y, d); }
            break;
        case AXIS_X_PLUS:
            if (do_ry) { look.y = ::atan2( dir.z,-dir.x); d = std::sqrt(dir.z*dir.z + dir.x*dir.x); } else d = -dir.x;
            if (do_rz) { look.z =-::atan2( dir.y, d); }
            break;
        //
        case AXIS_Y_MINUS:
            if (do_rx) { look.x = ::atan2( dir.z, dir.y); d = std::sqrt(dir.z*dir.z + dir.y*dir.y); } else d =  dir.y;
            if (do_rz) { look.z =-::atan2( dir.x, d); }
            break;
        case AXIS_Y_PLUS:
            if (do_rx) { look.x = ::atan2(-dir.z,-dir.y); d = std::sqrt(dir.z*dir.z + dir.y*dir.y); } else d = -dir.y;
            if (do_rz) { look.z = ::atan2( dir.x, d); }
            break;
        //
        case AXIS_Z_MINUS:
            if (do_ry) { look.y = ::atan2( dir.x, dir.z); d = std::sqrt(dir.x*dir.x + dir.z*dir.z); } else d =  dir.z;
            if (do_rx) { look.x =-::atan2( dir.y, d); }
            break;
        case AXIS_Z_PLUS:
            if (do_ry) { look.y = ::atan2(-dir.x,-dir.z); d = std::sqrt(dir.x*dir.x + dir.z*dir.z); } else d = -dir.z;
            if (do_rx) { look.x = ::atan2( dir.y, d); }
            break;
    }

    if (lookat_strength < (S)1)
    {
        // Interpolate between parent rotation and look rotation:
        if (do_rx) rotations_out.x = ::lerp(rotations_out.x, look.x, lookat_strength);
        if (do_ry) rotations_out.y = ::lerp(rotations_out.y, look.y, lookat_strength);
        if (do_rz) rotations_out.z = ::lerp(rotations_out.z, look.z, lookat_strength);
    }
    else
    {
        // Max rotations:
        if (do_rx) rotations_out.x = look.x;
        if (do_ry) rotations_out.y = look.y;
        if (do_rz) rotations_out.z = look.z;
    }
#else
    // Rotate the Y vector around the roll value:
    const T r = radians(roll);
    Vec4<T> Y(-std::sin(r), std::cos(r), (T)0);
    Y.normalize();
    // Find the reverse view(Z) vector(so that Z points away from the
    // interest point).  For a right-handed system, subtract eye from center:
    Vec4<T> Z;
    if (left_handed)
    {
        Z = (eye - interest);
        Z.normalize();
    }
    else
    {
        Z = (interest - eye);
        Z.normalize();
    }
    // Find the X vector:
    Vec4<T> X = Y.cross(Z);
    X.normalize();
    // Recompute Y(rotated about X):
    Y = Z.cross(X);
    Y.normalize();
    Mat4<T> m;
    m.a00 =  X.x; m.a01 =  Y.x; m.a02 =  Z.x; m.a03 = (T)0;
    m.a10 =  X.y; m.a11 =  Y.y; m.a12 =  Z.y; m.a13 = (T)0;
    m.a20 =  X.z; m.a21 =  Y.z; m.a22 =  Z.z; m.a23 = (T)0;
    m.a30 = (T)0; m.a31 = (T)0; m.a32 = (T)0; m.a33 = (T)1;
    *this *= m;
#endif
}

/*! Faster implementation from Graphics Gems I, page 785 -
    "Transforming Axis-Aligned Bounding Boxes"
*/
template<typename T>
template<typename S>
inline Box3<S>
Mat4<T>::transform(const Box3<S>& bbox) const
{
    // Copy translations out of matrix:
    Vec3<S> tr; getTranslation(tr);
    Box3<S> out(tr);
    for (int i=0; i < 3; ++i)
    {
        for (int j=0; j < 3; ++j)
        {
            const T t = (*this)[j][i];
            const T a = t * T(bbox.min[j]);
            const T b = t * T(bbox.max[j]);
            if (a < b)
            {
                out.min[i] += S(a);
                out.max[i] += S(b);
            }
            else
            {
                out.min[i] += S(b);
                out.max[i] += S(a);
            }
        }
    }
    return out;
}


/*! Extract the rotation angles (in radians) from the matrix.
    The matrix is assumed to have no shear or non-uniform scaling.
    Adapted from ilmbase ImathMatrixAlgo.
*/
template <class T>
inline void
Mat4<T>::getRotations(Fsr::RotationOrder order,
                      T&                 radian_rX,
                      T&                 radian_rY,
                      T&                 radian_rZ) const
{
    // Normalize the local x, y and z axes to remove scaling:
    Vec3<T> i(a00, a01, a02); i.normalize();
    Vec3<T> j(a10, a11, a12); j.normalize();
    Vec3<T> k(a20, a21, a22); k.normalize();
    Mat4<T> m(i.x, i.y, i.z, 0, 
              j.x, j.y, j.z, 0, 
              k.x, k.y, k.z, 0, 
              0,   0,   0,   1);
    switch (order)
    {
        case XYZ_ORDER:
        {
            // Extract the first angle:
            radian_rX = ::atan2(m.a21, m.a22); // Yz,Zz

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateX(-radian_rX);

            // Extract the other two angles:
            const T cosy = std::sqrt(m.a00*m.a00 + m.a10*m.a10); // Xx,Xy
            radian_rY = ::atan2(-m.a20,  cosy); // Xz
            radian_rZ = ::atan2(-m.a01, m.a11); // Yx,Yy
        }
        break;

        case XZY_ORDER:
        {
            // Extract the first angle:
            radian_rX = -::atan2(m.a12, m.a11); // Zy,Yy

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateX(-radian_rX);

            // Extract the other two angles:
            const T cosz = std::sqrt(m.a00*m.a00 + m.a20*m.a20); // Xx,Xz
            radian_rZ = ::atan2( m.a10,  cosz); // Xy
            radian_rY = ::atan2(-m.a20, m.a00); // Xz,Xx
        }
        break;

        case YXZ_ORDER:
        {
            // Extract the first angle:
            radian_rY = -::atan2(m.a20, m.a22); // Xz,Zz

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateY(-radian_rY);

            // Extract the other two angles:
            const T cosx = std::sqrt(m.a01*m.a01 + m.a11*m.a11); // Yx,Yy
            radian_rX = ::atan2( m.a21,  cosx); // Yz
            radian_rZ = ::atan2(-m.a01, m.a11); // Yx,Yy
        }
        break;

        case YZX_ORDER:
        {
            // Extract the first angle:
            radian_rY = ::atan2(m.a02, m.a00); // Zx,Xx

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateY(-radian_rY);

            // Extract the other two angles:
            const T cosx = std::sqrt(m.a11*m.a11 + m.a21*m.a21); // Yy,Yz
            radian_rZ = ::atan2(-m.a01,  cosx); // Yx
            radian_rX = ::atan2( m.a21, m.a11); // Yz,Yy
        }
        break;

        case ZXY_ORDER:
        {
            // Extract the first angle:
            radian_rZ = ::atan2(m.a10, m.a11); // Xy,Yy

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateZ(-radian_rZ);

            // Extract the other two angles:
            const T cosx = std::sqrt(m.a02*m.a02 + m.a22*m.a22); // Zx,Zz
            radian_rX = ::atan2(-m.a12,  cosx); // Zy
            radian_rY = ::atan2( m.a02, m.a22); // Zx,Zz
        }
        break;

        case ZYX_ORDER:
        default:
        {
            // Extract the first angle:
            radian_rZ = -::atan2(m.a01, m.a00); // Yx,Xx

            // Remove the first rotation so that the remaining rotations
            // are only around two axes, and gimbal lock cannot occur:
            m.rotateZ(-radian_rZ);

            // Extract the other two angles:
            const T cosy = std::sqrt(m.a12*m.a12 + m.a22*m.a22); // Zy,Zz
            radian_rY = -::atan2(-m.a02,  cosy); // Zx
            radian_rX = -::atan2(-m.a21, m.a11); // Yz,Yy
        }
        break;

    }
}
template<typename T>
inline Vec3<T>
Mat4<T>::getRotations(Fsr::RotationOrder order) const
{
    Vec3<T> angles;
    this->getRotations(order, angles.x, angles.y, angles.z);
    return angles;
}


/*! Adapted from ilmbase ImathMatrixAlgo.
*/
template <class T> 
inline bool
checkForZeroScaleInRow(const T&       scale, 
                       const Vec3<T>& row)
{
    const T abs_scale = ::fabs(scale);
    if (abs_scale < (T)1)
    {
        const T max_scale = std::numeric_limits<T>::max()*abs_scale;
        if (::fabs(row.x) >= max_scale ||
            ::fabs(row.y) >= max_scale ||
            ::fabs(row.z) >= max_scale)
                    return false;
    }
    return true;
}


/*! Adapted from ilmbase ImathMatrixAlgo.
*/
template <class T>
inline bool
Mat4<T>::extractAndRemoveScalingAndShear(Vec3<T>& scale,
                                         Vec3<T>& shear)
{
    // This implementation follows the technique described in the paper by
    // Spencer W. Thomas in the Graphics Gems II article: "Decomposing a 
    // Mat into Simple Transformations", p. 320.
    Vec3<T> row[3];
    row[0] = Vec3<T>(a00, a01, a02);
    row[1] = Vec3<T>(a10, a11, a12);
    row[2] = Vec3<T>(a20, a21, a22);

    // Find largest amplitude value in rows:
    T maxVal = (T)0;
    for (int i=0; i < 3; ++i)
    {
        const Vec3<T>& v = row[i];
        maxVal = std::max(maxVal, std::max(::fabs(v.x), std::max(::fabs(v.y), ::fabs(v.z))));
    }

    // We normalize the 3x3 matrix here.
    // It was noticed that this can improve numerical stability significantly,
    // especially when many of the upper 3x3 matrix's coefficients are very
    // close to zero; we correct for this step at the end by multiplying the 
    // scaling factors by maxVal at the end (shear and rotation are not 
    // affected by the normalization).
    if (maxVal > (T)0)
    {
        for (int i=0; i < 3; ++i)
        {
            if (!checkForZeroScaleInRow(maxVal, row[i]))
                return false;
            else
                row[i] /= maxVal;
        }
    }

    // Compute X scale factor. 
    scale.x = row[0].length();
    if (!checkForZeroScaleInRow(scale.x, row[0]))
        return false;

    // Normalize first row.
    row[0] /= scale.x;

    // An XY shear factor will shear the X coord as the Y coord changes.
    // There are 6 combinations (XY, XZ, YZ, YX, ZX, ZY), although we only
    // extract the first 3 because we can effect the last 3 by shearing in
    // XY, XZ, YZ combined rotations and scales.
    //
    // shear matrix <   1,  YX,  ZX,  0,
    //                 XY,   1,  ZY,  0,
    //                 XZ,  YZ,   1,  0,
    //                  0,   0,   0,  1 >

    // Compute XY shear factor and make 2nd row orthogonal to 1st.
    shear.x = row[0].dot(row[1]);
    row[1] -= row[0]*shear.x;

    // Now, compute Y scale.
    scale.y = row[1].length();
    if (!checkForZeroScaleInRow(scale.y, row[1]))
        return false;

    // Normalize 2nd row and correct the XY shear factor for Y scaling.
    row[1]  /= scale.y; 
    shear.x /= scale.y;

    // Compute XZ and YZ shears, orthogonalize 3rd row.
    shear.y = row[0].dot(row[2]);
    row[2] -= row[0]*shear.y;
    shear.z = row[1].dot(row[2]);
    row[2] -= row[1]*shear.z;

    // Next, get Z scale.
    scale.z = row[2].length();
    if (!checkForZeroScaleInRow(scale.z, row[2]))
        return false;

    // Normalize 3rd row and correct the XZ and YZ shear factors for Z scaling.
    row[2]  /= scale.z;
    shear.y /= scale.z;
    shear.z /= scale.z;

    // At this point, the upper 3x3 matrix in mat is orthonormal.
    // Check for a coordinate system flip. If the determinant
    // is less than zero, then negate the matrix and the scaling factors.
    if (row[0].dot(row[1].cross(row[2])) < (T)0)
    {
        for (int i=0; i < 3; ++i)
            row[i].negate();
        scale.negate();
    }

    // Copy over the orthonormal rows into the returned matrix.
    // The upper 3x3 matrix in mat is now a rotation matrix.
    setRow0(row[0]);
    setRow1(row[1]);
    setRow2(row[2]);

    // Correct the scaling factors for the normalization step that we 
    // performed above; shear and rotation are not affected by the 
    // normalization.
    scale *= maxVal;

    return true;
}


/*! Adapted from ilmbase ImathMatrixAlgo.
*/
template <class T>
inline bool
Mat4<T>::extractScalingAndShear(Vec3<T>& scale,
                                Vec3<T>& shear) const
{
    Mat4d m1(*this);
    if (!m1.extractAndRemoveScalingAndShear(scale, shear))
        return false;

    return true;
}


/*! Adapted from ilmbase ImathMatrixAlgo.
*/
template <class T>
inline bool 
Mat4<T>::extractSHRT(Vec3<T>&           scaling,
                     Vec3<T>&           shearing,
                     Vec3<T>&           rotationAngles,
                     Vec3<T>&           translation,
                     Fsr::RotationOrder order) const
{
    translation = this->getTranslation();

    // Remove scaling and shearing before extracting rotations:
    Mat4<T> rotm(*this);
    if (!rotm.extractAndRemoveScalingAndShear(scaling, shearing))
        return false;

    rotationAngles = rotm.getRotations(order).asDegrees();

    return true;
}

/*! Interpolate two matrices at 't' which is between 0.0 and 1.0.

    TODO: this should use quaternions instead...or decompose the
    matrices into trans/rot/scale and interpolate those...?
*/
template <class T>
inline void
Mat4<T>::interpolate(const Mat4<T>& m0,
                     const Mat4<T>& m1,
                     T                 t)
{
    if (t <= (T)0)
        *this = m0;
    else if (t >= (T)1)
        *this = m1;
    else
    {
        const T inv_t = ((T)1 - t);

        Vec3<T> axisX(m0.getXAxis()*inv_t + m1.getXAxis()*t);
        Vec3<T> axisY(m0.getYAxis()*inv_t + m1.getYAxis()*t);
        Vec3<T> axisZ(m0.getZAxis()*inv_t + m1.getZAxis()*t);

        // Normalizing the axes returns their lengths to use as scales:
        const Vec3<T> axes_scale(axisX.normalize(),
                                 axisY.normalize(),
                                 axisZ.normalize()); // Use slow normalize as length may be > 1.0

        this->setToTranslation(m0.getTranslation()*inv_t + m1.getTranslation()*t);
        this->setXAxis(axisX);
        this->setYAxis(axisY);
        this->setZAxis(axisZ);
        this->scale(axes_scale);
    }
}

} //namespace Fsr


#endif

// end of Fuser/Mat4.h

//
// Copyright 2019 DreamWorks Animation
//
