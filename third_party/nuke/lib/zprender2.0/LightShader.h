//
// Copyright 2020 DreamWorks Animation
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

/// @file zprender/LightShader.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightShader_h
#define zprender_LightShader_h

#include "RayShader.h"

namespace zpr {

class LightVolume;
class MaterialContext;


/*! Base class of ray-tracing light shaders.
    Currently only the transform supports motionblur.

    TODO: add Sample struct to support animating color, etc?
*/
class ZPR_EXPORT LightShader : public RayShader
{
  public:
    /*! Input values set by local knobs or input connections.
    */
    struct BaseInputParams
    {
        Fsr::Vec3f k_color;                     //!<
        float      k_intensity;                 //!<
        bool       k_illuminate_atmosphere;     //!<
    };

    Fsr::Pixel m_color;             //!< Pre-calculated global output color - usually k_color*k_intensity


  protected:
    Fsr::DoubleList m_motion_times;     //!< Frame time for each motion-sample
    Fsr::Mat4dList  m_motion_xforms;    //!< Motion xform matrices
    Fsr::Mat4dList  m_motion_ixforms;   //!< Inverse motion xform matrices created from m_motion_xforms
    //
    bool            m_enabled;          //!< Light can illuminate scene (usually cacl'd from m_color > 0)


  public:
    //! Sets color & intensity to 1.0
    LightShader();
    //!
    LightShader(const InputKnobList&  inputs,
                const OutputKnobList& output);
    //!
    LightShader(const Fsr::DoubleList& motion_times,
                const Fsr::Mat4dList&  motion_xforms);
    //!
    LightShader(const InputKnobList&   inputs,
                const OutputKnobList&  output,
                const Fsr::DoubleList& motion_times,
                const Fsr::Mat4dList&  motion_xforms);

    //! Must have a virtual destructor!
    virtual ~LightShader() {}


    //! Print the shader knob values to stream.
    /*virtual*/ void print(std::ostream&) const;
    friend std::ostream& operator << (std::ostream& o, const LightShader& b) { b.print(o); return o; }


    //---------------------------------------------------------


    //!
    static const char* zpClass();

    //! Returns the class name, must implement.
    /*virtual*/ const char* zprShaderClass() const { return "LightShader"; }

    static const ShaderDescription description;


    //---------------------------------------------------------


    //!
    /*virtual*/ LightShader*  isLightShader() { return this; }

    /*! Quick test if light can illuminate scene.
        Usually valid only after updateUniformLocals() has been called.
    */
    bool       isEnabled() const { return m_enabled; }

    /*! Assign the motion transforms matrices. The inverses will be automatically created.
        Subclasses can add add'l transforms before storage.
    */
    virtual void setMotionXforms(const Fsr::DoubleList& motion_times,
                                 const Fsr::Mat4dList&  motion_xforms);

    // Access to list of motion times and xforms. They should have the same count!
    uint32_t          numMotionTimes()  const { return (uint32_t)m_motion_times.size(); }
    uint32_t          numMotionXforms() const { return (uint32_t)m_motion_xforms.size(); }

    //! Get a time from the list of motion times. No range checking!
    double            getMotionTime(uint32_t index) const { return m_motion_times[index]; }
    //! Get a matrix from the list of motion xforms. No range checking!
    const Fsr::Mat4d& getMotionXform(uint32_t index) const { return m_motion_xforms[index]; }


    //! Get a possibly-interpolated matrix at frame_time.
    Fsr::Mat4d getMotionXformAt(double frame_time) const;
    //!
    Fsr::Mat4d getInverseMotionXformAt(double frame_time) const;
    //! Get both at once (saves a motion step calculation.)
    void       getMotionXformsAt(double      frame_time,
                                 Fsr::Mat4d& xform,
                                 Fsr::Mat4d& ixform) const;


    //---------------------------------------------------------


    /*! Can this light shader produce a LightVolume?
        Simple light types can produce fundamental volume shapes that surround
        their maximum area of illumination while others are either too complex
        in nature.

        LightVolumes are intended for simplistic (fast) simulation of a uniform
        (homogeneous) atmosphere and not 'real' volume rendering, so simple
        volume shapes be intersected and ray marched very quickly.

        For example a point light volume is a sphere, direct light a cylinder,
        spot light a cone, and a rectangle light is a frustum.
    */
    virtual bool canGenerateLightVolume() { return false; }

    /*! Return the entire motion bbox enclosing the LightVolume that
        this shader can create during createLightVolume().
        Base class returns an empty bbox.
    */
    virtual Fsr::Box3d getLightVolumeMotionBbox() { return Fsr::Box3d(); }

    /*! Create a LightVolume primitive appropriate for this LightShader.
        Calling function takes ownership.
        Base class does nothing.
    */
    virtual LightVolume* createLightVolume(const MaterialContext* mtx) { return NULL; }


    //---------------------------------------------------------


    //! Return a pointer to the input uniform parameter structure. Must implement.
    virtual BaseInputParams* uniformInputs()=0;


    /*! Initialize any uniform vars prior to rendering.

        LightShader base class calls calculates 'm_color' from
        'k_color' and 'k_intensity'.
    */
    /*virtual*/ void updateUniformLocals(double  frame,
                                         int32_t view=-1);

    /*! Initialize any vars prior to rendering.

        LightShader base class calls RayShader::validateShader() which
        calls updateUniformLocals(). This will call the LightShader subclass
        which sets any uniform local vars, and importantly calculates the
        global m_color var.

        After RayShader::validateShader() returns m_enabled is set true if
        m_color.rgb() is non-zero.

        RenderContext is optional so that this can be called by a legacy shading
        context, passing an Op OutputContext instead.
    */
    /*virtual*/ void validateShader(bool                            for_real,
                                    const RenderContext*            rtx,
                                    const DD::Image::OutputContext* op_ctx=NULL);


    /*! Emit radiance from the light, used for general light emission falling
        on any point in space not just a surface.
        The point illuminated is stx.PW but the other surface parameters like
        normal, uv, etc are ignored.
        Returns false if light does not contribute to point illumination.
        Must implement.

        'illum_ray' is built in the LightShader and normally points from surface to
        light origin and can be used for shadowing, specular angle, etc.

        illum_ray.maxdist should be the distance between surface point and light,
        for shadow intersection and falloff determination.
        illum_ray.mindist should be set to 0 or an epsilon bias off surface.

        direct_pdfW_out is the direct lighting power distribution function weight
        of the light for illum_ray.
    */
    //virtual bool emit(RayShaderContext& stx,
    //                  Fsr::RayContext&  emission_ray,
    //                  float&            emissive_pdfW_out,
    //                  Fsr::Pixel&       emissive_color_out)=0;


    /*! Evaluate the light's contribution to a surface intersection.
        Returns false if light does not contribute to surface illumination.
        Must implement.

        stx.PW is the point being illuminated.

        'illum_ray' is built in the LightShader and normally points from surface to
        light origin and can be used for shadowing, specular angle, etc.

        'illum_ray.mindist' should be set to 0 or an epsilon bias off light 'surface'
        clamped to the light's near value (if it has one.)

        'illum_ray.maxdist' should be the distance between surface point and light,
        for shadow intersection and falloff determination, clamped to the light's
        far value (if it has one.)

        direct_pdfW_out is the direct lighting power distribution function weight
        of the light for illum_ray.
    */
    virtual bool illuminate(RayShaderContext& stx,
                            Fsr::RayContext&  illum_ray,
                            float&            direct_pdfW_out,
                            Fsr::Pixel&       illum_color_out)=0;


    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //  DDImage legacy shading support methods for ScanlineRender, RayRender.
    /*
        We need these on the shader interface as the DD::Image::LightOp API
        splits the illumination calculation into three parts:
          get_L_vector()
          get_shadowing()
          get_color()

        These methods are expected to be called from surface shaders and are
        split to allow the lighting vectors to be manipulated by the shaders.
        For example calling get_shadowing() multiple times to create soft
        shadows or fiddling with the normal to create some special effect.

        Since there's no thread-state continuity between the three calls
        we can't cache the results from illuminate() like we do
        when spoofing legacy shading from a zpRender RayShader.

        Unfortunately this means re-implementing the light shader code
        for legacy vs. zpRender shading...  :(

        Hoeever - we still want to use the spoofing method with RayShaders
        so that we get motionblurred lights, etc, so in practice a
        LightShader should split its functionality into inline methods
        that can be shared as much as possible between the methods.
    */


    /*! Calculate a normalized direction vector 'lightNOut' and distance
        'lightDistOut' from the light to surface point 'surfP'.

        Normalized vector 'lobeN' is passed to allow lights like area lights
        to simulate a large emission surface. 'lobeN' is usually the surface
        normal when querying the diffuse surface contribution and the
        reflection vector off the surface when querying specular contribution.
    */
    virtual void getLightVector(const DD::Image::LightContext& ltx,
                                const DD::Image::Vector3&      surfP,
                                const DD::Image::Vector3&      lobeN,
                                DD::Image::Vector3&            lightNOut,
                                float&                         lightDistOut) const {}


    /*! Return the amount of shadowing the light creates at surface point surfP,
        and optionally copies the shadow mask to a channel in shadowChanOut.
    */
    virtual float getShadowing(const DD::Image::LightContext&  ltx,
                               const DD::Image::VertexContext& vtx,
                               const DD::Image::Vector3&       surfP,
                               DD::Image::Pixel&               shadowChanOut) const { return 1.0f; }


    /*! Returns the color of the light (possibly) using the current
        surface point and normal to calculate attenuation and penumbra.
    */
    virtual void getColor(const DD::Image::LightContext& ltx,
                          const DD::Image::Vector3&      surfP,
                          const DD::Image::Vector3&      lobeN,
                          const DD::Image::Vector3&      lightN,
                          float                          lightDist,
                          DD::Image::Pixel&              colorChansOut) const {}

};


} // namespace zpr

#endif

// end of zprender/LightShader.h

//
// Copyright 2020 DreamWorks Animation
//
