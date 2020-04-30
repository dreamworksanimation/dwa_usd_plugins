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

/// @file FuserUsdNode.h
///
/// @author Jonathan Egstad


#ifndef FuserUsdNode_h
#define FuserUsdNode_h

#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/XformableNode.h>
#include <Fuser/Primitive.h> // for SceneNodesContext
#include <Fuser/NukeKnobInterface.h>


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including USD headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <pxr/usd/usd/attribute.h>
#  include <pxr/usd/usd/stage.h>
#  include <pxr/usd/usd/prim.h>

#  include <pxr/usd/usdGeom/primvar.h>

#  pragma GCC diagnostic pop
#endif

//**********************************************************************
//**********************************************************************
// Map the Pixar namespace to something easier to use so we
// can wrap everything in the Fsr namespace:

namespace Pxr = PXR_INTERNAL_NS;

//**********************************************************************
//**********************************************************************


#include <type_traits> // for is_same


namespace Fsr {


//-------------------------------------------------------------------------------


/*! Used to simplify the attribute get methods.
*/
class AttribDoubles : public Fsr::ArrayKnobDoubles
{
  public:
    AttribDoubles() : Fsr::ArrayKnobDoubles() {}
    AttribDoubles(const Pxr::UsdAttribute& attrib,
                  bool                     allow_animation=true) { getFromAttrib(attrib, allow_animation); }

    //! Cast to UsdTimeCode access - unclamped!
    Pxr::UsdTimeCode timeCode(size_t i) const { return Pxr::UsdTimeCode(times[i]); }


    //! Extract doubles from a UsdAttrib. Returns false if attrib does not support doubles.
    bool getFromAttrib(const Pxr::UsdAttribute& attrib,
                       bool                     allow_animation=true);
};


//-------------------------------------------------------------------------------


/*! USD node wrapper.
*/
class FuserUsdNode
{
  protected:
    Pxr::UsdStageRefPtr m_stage;        //!< Stage reference-counted cache pointer
    double              m_time;         //!< Node's current time (frame / fps)


  protected:
    //! Return the owning stage.
    const Pxr::UsdStageRefPtr& getStage() const { return m_stage; }

    //! Return the wrapped USD prim.
    virtual Pxr::UsdPrim getPrim()=0;


  public:
    //!
    FuserUsdNode(const Pxr::UsdStageRefPtr& stage);

    //!
    virtual ~FuserUsdNode();


    //! Import node attributes into a Nuke Op.
    virtual void importSceneOp(DD::Image::Op*     op,
                               const Fsr::ArgSet& args) {}

    //-------------------------------------------------------------------------------


    /*! Make sure the prim is Loaded, and is Valid, Defined, and Active.
        Returns false if prim is not Valid, not Active, not Defined, or
        it failed to Load.
    */
    static bool isLoadedAndUseablePrim(const Pxr::UsdPrim& prim);

    /*! Make sure the prim is Loaded, and is Valid, Defined, and Active.
        Returns NULL if no error otherwise an allocated ErrorNode with more error info.
    */
    static Fsr::ErrorNode* isLoadedAndUseablePrim(const char*         fsr_builder_class,
                                                  const Pxr::UsdPrim& prim,
                                                  const char*         prim_load_path,
                                                  bool                debug_loading=false);


    //! Is the prim able to be rendered (rasterized)?
    static bool isRenderablePrim(const Pxr::UsdPrim& prim);

    //! Does the prim support bounds (a bounding-box)?
    static bool isBoundablePrim(const Pxr::UsdPrim& prim);

    //! Is the prim a usdShade prim?
    static bool isShadingPrim(const Pxr::UsdPrim& prim);


    //-------------------------------------------------------------------------------


    //!
    static void printPrimAttributes(const char*         prefix,
                                    const Pxr::UsdPrim& prim,
                                    bool                verbose,
                                    std::ostream&);

    //!
    static bool isPrimAttribVarying(const Pxr::UsdAttribute& attrib,
                                    double                   time);

    //! Returns false if times[] contains a single UsdTimeCode::Default() entry, ie. is not animated.
    static bool getPrimAttribTimeSamples(const Pxr::UsdAttribute& attrib,
                                         std::vector<double>&     times);

    //! If not animated UsdTimeCode::Default() is added to set.
    static void concatenatePrimAttribTimeSamples(const Pxr::UsdAttribute&  attrib,
                                                 std::set<Fsr::TimeValue>& concat_times);

    //-------------------------------------------------------------------------------


    //!
    static double getPrimAttribDouble(const Pxr::UsdPrim&     prim,
                                      const char*             attrib_name,
                                      const Pxr::UsdTimeCode& time,
                                      unsigned                element_index=0);
    //!
    static double getPrimAttribDouble(const Pxr::UsdAttribute& attrib,
                                      const Pxr::UsdTimeCode&  time,
                                      unsigned                 element_index=0);


    //!
    static bool   getPrimAttribDoubles(const Pxr::UsdAttribute& attrib,
                                       AttribDoubles&           attrib_data);


    //! Copies all the keys from the attrib to the Nuke Knob. Returns false if error occured.
    static bool   copyAttribToKnob(const Pxr::UsdAttribute& attrib,
                                   bool                     allow_animation,
                                   DD::Image::Knob*         knob,
                                   int                      view,
                                   double                   scale=1.0,
                                   double                   offset=0.0);


    //! Copies all the keys from the stereo attrib to a split Nuke Knob. Returns false if error occured.
    static bool   copyAttribToStereoKnob(const Pxr::UsdAttribute& attrib,
                                         bool                     allow_animation,
                                         DD::Image::Knob*         k,
                                         const std::vector<int>&  views);


    //! Copies a Usd primvar array attribute to a typed std::vector. Returns false if copy did not happen.
    template <typename T, typename S>
    static bool   getArrayPrimvar(const Pxr::UsdGeomPrimvar& primvar,
                                  const Pxr::UsdTimeCode&    time,
                                  std::vector<S>&            out,
                                  const Pxr::TfToken&        scope_mask=Pxr::TfToken(""),
                                  bool                       debug=false);


};


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------


struct PrimvarRef
{
    bool                  is_array;
    bool                  is_int;
    bool                  is_half;
    bool                  is_float;
    bool                  is_double;
    int                   bytes_per_element;
    int                   num_elements;
    DD::Image::AttribType nk_attrib;

    PrimvarRef() {}
    PrimvarRef(bool is_array_type,
               bool is_int_type,
               bool is_float_type,
               int  _bytes_per_element,
               int  _num_elements,
               DD::Image::AttribType _nk_attrib) :
        is_array(is_array_type),
        is_int(is_int_type),
        bytes_per_element(_bytes_per_element),
        num_elements(_num_elements),
        nk_attrib(_nk_attrib)
    {
        if (is_float_type)
        {
            is_half   = (bytes_per_element == 2);
            is_float  = (bytes_per_element == 4);
            is_double = (bytes_per_element == 8);
        }
        else
            is_half = is_float = is_double = false;
    }


    //! Retrieve a reference object to a primvar type.
    static const PrimvarRef* get(const Pxr::UsdGeomPrimvar&);

};


/*----------------------------------*/
/*        Inline Functions          */
/*----------------------------------*/


template <typename S>
bool isInt(const S&)
{
    return (std::is_same<int,        S>::value ||
            std::is_same<Fsr::Box3i, S>::value ||
            std::is_same<Fsr::Vec3i, S>::value ||
            std::is_same<Fsr::Vec3i, S>::value ||
            std::is_same<Fsr::Vec4i, S>::value);
}

template <typename S>
bool isFloat(const S&)
{
    return (std::is_same<float,      S>::value ||
            std::is_same<Fsr::Box3f, S>::value ||
            std::is_same<Fsr::Mat4f, S>::value ||
            std::is_same<Fsr::Vec3f, S>::value ||
            std::is_same<Fsr::Vec3f, S>::value ||
            std::is_same<Fsr::Vec4f, S>::value);
}

template <typename S>
bool isDouble(const S&)
{
    return (std::is_same<double,     S>::value ||
            std::is_same<Fsr::Box3d, S>::value ||
            std::is_same<Fsr::Mat4d, S>::value ||
            std::is_same<Fsr::Vec3d, S>::value ||
            std::is_same<Fsr::Vec3d, S>::value ||
            std::is_same<Fsr::Vec4d, S>::value);
}

//! Copy data arrays.
template <typename T, typename S>
void
copyArrays(const T* IN, unsigned in_vals, S* OUT, unsigned out_vals)
{
    if (in_vals < out_vals)
    {
        T* p = OUT;
        for (unsigned i=0; i < in_vals; ++i)
            *p++ = float(*IN++);
        // Fill extra out values with 0:
        memset((OUT+in_vals), 0, (out_vals - in_vals)*sizeof(T));

    }
    else
    {
        // Ignore any extra input values:
        for (unsigned i=0; i < out_vals; ++i)
            *OUT++ = float(*IN++);
    }
}


//-------------------------------------------------------------------------------


/*! Copies a Usd primvar array attribute to a typed std::vector.
    Returns false if copy did not happen.

    TODO: make this handle more types and output to a typed Fsr::Attribute.

*/
template <typename T, typename S>
/*static*/ inline bool
FuserUsdNode::getArrayPrimvar(const Pxr::UsdGeomPrimvar& primvar,
                              const Pxr::UsdTimeCode&    time,
                              std::vector<S>&            out,
                              const Pxr::TfToken&        scope_mask,
                              bool                       debug)
{
    if (!primvar/* || !primvar.GetAttr().HasAuthoredValueOpinion()*/)
        return false; // invalid primvar

    if (debug)
    {
        std::cout << "    getUsdPrimvar('" << primvar.GetName() << "'): scope=" << primvar.GetInterpolation();
        std::cout << ", type=" << primvar.GetTypeName() << ", indexed=" << primvar.IsIndexed();
        std::cout << ", element_size=" << primvar.GetElementSize();
    }

    const PrimvarRef* ref = PrimvarRef::get(primvar);
    if (!ref)
    {
        if (debug)
            std::cout << " - warning, can't translate to Nuke3D attrib!" << std::endl;
        return false; // can't translate
    }

    if (debug)
    {
        std::cout << " - is_array=" << ref->is_array;
        std::cout << ", num_elements=" << ref->num_elements;
        std::cout << ", bytes_per_element=" << ref->bytes_per_element;
        std::cout << ", nk_attrib=" << DD::Image::Attribute::type_string(ref->nk_attrib);
        std::cout << " - OUT isInt=" << isInt(S());
        std::cout << ", isFloat=" << isFloat(S());
        std::cout << ", isDouble=" << isDouble(S());
        std::cout << std::endl;
    }

    Pxr::VtArray<T> vals;
    if (!primvar.Get(&vals, time) || vals.empty())
    {
        if (debug)
            std::cerr << "Primvar::Get() failed, type mismatch" << std::endl;
        return false; // incompatible type or no data
    }

    const Pxr::TfToken& scope = primvar.GetInterpolation();
    if (!scope_mask.IsEmpty() && scope_mask != scope)
        return false; // not the requested scope

    if      (scope == Pxr::UsdGeomTokens->vertex)
    {
        // Per-point attribute (not per face-vertex despite the name):
        assert(sizeof(T) == sizeof(S));
        out.resize(vals.size());
        memcpy(out.data(), vals.data(), vals.size()*sizeof(S));
        if (debug)
        {
            std::cout << "      vertex size=" << out.size();
            std::cout << std::endl;
        }

    }
    else if (scope == Pxr::UsdGeomTokens->faceVarying)
    {
        // Per-vertex attribute (not per-face despite the name!) i.e. *varying* is
        // the key word here:
        primvar.ComputeFlattened(&vals, time);
        out.resize(vals.size());
        memcpy(out.data(), vals.data(), vals.size()*sizeof(S));
        if (debug)
        {
            std::cout << "      faceVarying size=" << out.size();
            std::cout << std::endl;
        }

    }
    else if (scope == Pxr::UsdGeomTokens->uniform)
    {
        //
        // uniform: One value remains constant for each uv patch segment of 
        //  the surface primitive (which is a face for meshes). 
        //
        assert(sizeof(T) == sizeof(S));
        out.resize(vals.size());
        memcpy(out.data(), vals.data(), vals.size()*sizeof(S));
        if (debug)
        {
            std::cout << "      uniform(face) size=" << out.size();
            std::cout << std::endl;
        }

    }
    else if (scope == Pxr::UsdGeomTokens->constant)
    {
        //
        // One value remains constant over the entire surface primitive.
        //
        assert(sizeof(T) == sizeof(S));
        out.resize(1);
        memcpy(out.data(), vals.data(), sizeof(S));
        if (debug)
        {
            std::cout << "      constant size=" << out.size();
            std::cout << std::endl;
        }

    }
    else
    {
        if (debug)
            std::cerr << "FsrUsd: warning, can't support primvar with '" << scope << "' scope." << std::endl;
        return false;
    }

    if (debug)
    {
        //for (size_t i=0; i < out.size(); ++i)
        //    std::cout << i << out[i] << std::endl;
    }

    return true;

} // getArrayPrimvar()


} // namespace Fsr


#endif

// end of FuserUsdNode.h

//
// Copyright 2019 DreamWorks Animation
//
