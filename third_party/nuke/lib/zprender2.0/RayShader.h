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

/// @file zprender/RayShader.h
///
/// @author Jonathan Egstad


#ifndef zprender_RayShader_h
#define zprender_RayShader_h

#include "Scene.h"
#include "Traceable.h"
#include "RayShaderContext.h"

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel

#include <DDImage/Material.h>
#include <DDImage/Knobs.h>


namespace zpr {

class LightShader;
class VolumeShader;



/*! Base interface class of ray-tracing shaders.
*/
class ZPR_EXPORT RayShader
{
  public:
    //! Frame clamp modes.
    enum
    {
        FRAME_CLAMP_NONE,
        FRAME_FWD_RND_UP,
        FRAME_FWD_RND_DOWN,
        FRAME_REV_RND_UP,
        FRAME_REV_RND_DOWN
    };
    static const char* frame_clamp_modes[];


  public:
    /*!
    */
    struct MapBinding
    {
        /*! Map binding type.
            Note that it's a bitmask - if it's an input then it can be
            multiple types at the same time - i.e. it can be an Op, Iop,
            Material & RayShader all at once.
        */
        enum
        {
            NONE       = 0x00,   //!< No binding
            ATTRIB     = 0x01,   //!< Attribute binding
            OP         = 0x02,   //!< Op input
            IOP        = 0x04,   //!< Iop input - generic 2D operator, a texture
            MATERIAL   = 0x08,   //!< Material input - legacy Nuke shader
            SURFACEOP  = 0x10    //!< SurfaceShaderOp input
        };
        // Flags:
        enum
        {
            IS_TEXTURE = 0x01,   //!< Binding is a texture source
            HAS_ALPHA  = 0x02,   //!< Input image has an alpha channel
            IS_MONO    = 0x04    //!< Input image is single-channel
        };

        uint16_t type;          //!< Type of input
        uint16_t flags;         //!< Input flags

        MapBinding() : type(0x00), flags(0x00) {}

        //! Convenience methods:
        inline bool noBinding()         const { return (type == NONE      ); }
        inline bool isAttrib()          const { return (type & ATTRIB     ); }
        inline bool isOp()              const { return (type & OP         ); }
        inline bool isIop()             const { return (type & IOP        ); }
        inline bool isMaterial()        const { return (type & MATERIAL   ); }
        inline bool isSurfaceShaderOp() const { return (type & SURFACEOP  ); }
        //
        inline bool isTexture()   const { return (flags & IS_TEXTURE); }
        inline bool hasAlpha()    const { return (flags & HAS_ALPHA ); }
        inline bool isMono()      const { return (flags & IS_MONO   ); }
    };

    //! Returns the MapBinding for input n.
    const MapBinding& getInputType(int n) const { return m_input_type[n]; }


  protected:
    std::vector<MapBinding> m_input_type;  //!< Type of input - RayShader, Material, Iop, or Op

    int  k_sides_mode;               //!< Which side this material applies to (default is SIDES_BOTH)
    bool k_camera_visibility;        //!< Is this shader visible to camera rays?
    bool k_shadow_visibility;        //!< Is this shader visible to shadow rays?
    bool k_specular_visibility;      //!< Is this shader visible to specular rays?
    bool k_diffuse_visibility;       //!< Is this shader visible to diffuse rays?
    bool k_transmission_visibility;  //!< Is this shader visible to transmitted rays?
    int  k_frame_clamp_mode;         //!< How this shader uses the frame number from below.


  public:
    //!
    RayShader();

    //! Must have a virtual destructor!
    virtual ~RayShader() {}


    //! Returns 'zpRayShader'.
    static const char* zpClass();

    /*! !!HACK ALERT!! This adds an invisible 'zpRayShader' knob that's
        used to identify a RayShader-derived Op to other plugins.

        If the zprender lib is built static then dynamic_casting fails,
        so we can test for this knob instead and then static_cast the
        pointer to RayShader*.

        Atm if this knob doesn't exist then the _evaluate*() methods will
        not be called since the node will not be recognized as a RayShader!
    */
    void addRayShaderIdKnob(DD::Image::Knob_Callback f);

    //!
    virtual LightShader*  isLightShader()  { return NULL; }
    //!
    virtual VolumeShader* isVolumeShader() { return NULL; }


    //! Get the binding info for an input Op.
    static MapBinding getOpMapBinding(DD::Image::Op*     op,
                                      DD::Image::Channel alpha_chan=DD::Image::Chan_Alpha);


    //!
    virtual void addRayControlKnobs(DD::Image::Knob_Callback f);

    //! Initialize any vars prior to rendering.
    virtual void  validateShader(bool for_real);


    /*! Return true if the vertex_shader() method is implemented and should be called.
        The vertex_shader() call is required by some shaders to perturb normals, uvs, or
        point locations, but the Primitive::vertex_shader() method is expensive to call on
        big models, so this method allows us to skip calling it unless required by the
        RayShader.
    */
    virtual bool vertexShaderActive() { return false; }

    //! Which uniform subd level to displace to.
    virtual int getDisplacementSubdivisionLevel() const { return 0; }


    //! Current modes for this shader.
    int  sides_mode()              const { return k_sides_mode; }
    bool camera_visibility()       const { return k_camera_visibility; }
    bool shadow_visibility()       const { return k_shadow_visibility; }
    bool specular_visibility()     const { return k_specular_visibility; }
    bool diffuse_visibility()      const { return k_diffuse_visibility; }
    bool transmission_visibility() const { return k_transmission_visibility; }


  public:
    //! This copies info from the Intersection structure into the RayShaderContext structure.
    static void updateShaderContextFromIntersection(const Traceable::SurfaceIntersection& I,
                                                    RayShaderContext&                     stx);


    //! Abstracted illumination entry point.
    static void getIllumination(RayShaderContext&                stx,
                                Fsr::Pixel&                      out,
                                Traceable::DeepIntersectionList* deep_out=0);

    //----------------------------------

    /*! Abstracted surface shader entry point allows either legacy fragment shader
        or new ray-traced shader methods to be called.
        Shader being called is contained in the RayShaderContext.
    */
    static void doShading(RayShaderContext& stx,
                          Fsr::Pixel&       out);

    /*! Abstracted displacement entry point allows legacy displacement shader or new ray-traced
        shader methods to be called.  Shader being called is contained in the RayShaderContext.
    */
    static void doDisplacement(RayShaderContext& stx,
                               Fsr::Pixel&       out);


    //! Top-level ray-tracing geometric surface shader evaluation call.
    void doGeometricShading(RayShaderContext& stx,
                            RayShaderContext& out);

    //----------------------------------

    /*! Top-level ray-tracing surface shader evaluation call.
        This checks global-level params before calling the virtual subclass version.
    */
    void evaluateShading(RayShaderContext& stx,
                         Fsr::Pixel&       out);

    /*! Top-level ray-tracing displacement shader evaluation call.
        This checks global-level params before calling the virtual subclass version.
    */
    void evaluateDisplacement(RayShaderContext& stx,
                              Fsr::Pixel&       out);


  protected:
    //------------------------------------------------------------------
    // Subclasses implement these calls to modify the shading.
    // Called from base class high-level methods like getIllumination().
    //------------------------------------------------------------------

    /*! The surface evaluation shader call. Base class does nothing.
        If doing final displacement implement _evaluateDisplacement() instead.
    */
    virtual void _evaluateGeometricShading(RayShaderContext& stx,
                                           RayShaderContext& out) {}

    //! The ray-tracing surface shader evaluation call. Base class does nothing.
    virtual void _evaluateShading(RayShaderContext& stx,
                                  Fsr::Pixel&       out) {}

    //! The ray-tracing displacement shader evaluation call. Base class does nothing.
    virtual void _evaluateDisplacement(RayShaderContext& stx,
                                       Fsr::Pixel&       out) {}


  public:
    //------------------------------------------------------------------
    // Simepl default integrators, for convenience.
    //------------------------------------------------------------------

    /*! Return the indirect diffuse illumination for surface point with normal N.
        Indirect diffuse means only rays that hit objects will contribute to the surface color.
    */
    bool getIndirectDiffuse(RayShaderContext& stx,
                            const Fsr::Vec3d& N,
                            double            roughness,
                            Fsr::Pixel&       out);

    /*! Return the indirect specular illumination for surface point with normal N.
        Indirect diffuse means only reflected rays that hit objects will contribute to the surface color.
    */
    bool getIndirectGlossy(RayShaderContext& stx,
                           const Fsr::Vec3d& N,
                           double            roughness,
                           Fsr::Pixel&       out);

    /*! Return the transmitted illumination for surface point with normal N.
        Transmission means only refracted rays that pass through objects will contribute to the surface color.
    */
    bool getTransmission(RayShaderContext& stx,
                         const Fsr::Vec3d& N,
                         double            eta,
                         double            roughness,
                         Fsr::Pixel&       out);

    /*! Get the occlusion of this surface point.

        For ambient occlusion set 'occlusion_ray_type' to DIFFUSE and
        for reflection(ambient) occlusion use GLOSSY or REFLECTION, and
        TRANSMISSION for refraction-occlusion.

        The value returned is between 0.0 and 1.0, where 0.0 means no
        occlusion (ie the point is completely exposed to the environment)
        and 1.0 is full-occlusion where the point has no exposure to the
        environment.
    */
    static float getOcclusion(RayShaderContext& stx,
                              uint32_t          occlusion_ray_type,
                              double            mindist,
                              double            maxdist,
                              double            cone_angle=180.0,
                              double            gi_scale=1.0);


  public:
    //------------------------------------------------------------------
    // RayShaderContext adapter for calling DDImage Materials with a
    // DD::Image::VertexContext.
    //------------------------------------------------------------------

    //! Construct a DD::Image::VertexContext that can be passed to a fragment_shader()
    static void updateDDImageShaderContext(const RayShaderContext&   stx,
                                           DD::Image::VertexContext& vtx);


  public:
    //------------------------------------------------------------------
    // Shader utility functions
    // TODO: move outside class?
    //------------------------------------------------------------------

    /*! Returns a normal as-is if a vertex's eye-space position vector points in the
        opposite direction of a geometric normal, otherwise return the negated version of the normal.
    */
    inline static Fsr::Vec3d faceOutward(const Fsr::Vec3d&       N,
                                         const RayShaderContext& stx);


    /*! Returns a normal as-is if a vertex's eye-space position vector points in the
        opposite direction of a geometric normal, otherwise return the negated version of the normal.
    */
    inline static Fsr::Vec3d faceOutward(const Fsr::Vec3d& N,
                                         const Fsr::Vec3d& V,
                                         const Fsr::Vec3d& Ng);


    //! Same as faceOutward() (imho 'faceOutward' is a far more descriptive name for this operation)
    inline static Fsr::Vec3d faceForward(const Fsr::Vec3d& N,
                                         const Fsr::Vec3d& V,
                                         const Fsr::Vec3d& Ng);


    //! Returns the refraction ratio for two index-of-refraction weights, flipping them if neccessary
    static double getRefractionRatio(const Fsr::Vec3d& V,
                                     const Fsr::Vec3d& N,
                                     double            ior_from,
                                     double            ior_to);

    /*! Calcs a refracted incident vector 'I'.
        'eta' is the ratio of the indices-of-refraction (ior) differences between
        two materials, such as the one returned by the getRefractionRatio() method.
        Returns false if total internal reflection.
    */
    static bool refract(const Fsr::Vec3d& I,
                        const Fsr::Vec3d& N,
                        double            eta,
                        Fsr::Vec3d&       out);

    //! Returns the ratio of reflection vs. transmission using Snell's law and Schlick's approximation.
    static float reflectionRatioSnellSchlick(const Fsr::Vec3d& V,
                                             const Fsr::Vec3d& N,
                                             double            ior_from,
                                             double            ior_to,
                                             double            fresnel_power=5.0);

    //! Simplified Oren-Nayer diffuse function (disarded C3 & interreflections ignored).
    static double orenNayerSimplified(const Fsr::Vec3d& V,
                                      const Fsr::Vec3d& N,
                                      const Fsr::Vec3d& lightV,
                                      double            roughnessSquared=(1.0*1.0));

    //! Convenience function to encapsulate the RGB triplet in a Pixel object as a Vec3f.
    static Fsr::Vec3f& vec3fColorInPixel(const Fsr::Pixel& pixel);

    //! Replacement for the DD::Image::Pixel::under() method which does the wrong thing and doesn't handle alpha.
    static void A_under_B(const Fsr::Pixel&             A,
                          Fsr::Pixel&                   B,
                          const DD::Image::ChannelMask& channels);

};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


/*static*/ inline Fsr::Vec3d
RayShader::faceOutward(const Fsr::Vec3d&       N,
                       const RayShaderContext& stx) { return (-stx.Rtx.dir().dot(stx.Ng) < 0.0f) ? -N : N; }
/*static*/ inline Fsr::Vec3d
RayShader::faceOutward(const Fsr::Vec3d& N,
                       const Fsr::Vec3d& V,
                       const Fsr::Vec3d& Ng) { return (V.dot(Ng) < 0.0f) ? -N : N; }
/*static*/ inline Fsr::Vec3d
RayShader::faceForward(const Fsr::Vec3d& N,
                       const Fsr::Vec3d& V,
                       const Fsr::Vec3d& Ng) { return faceOutward(N, V, Ng); }



/*! Returns the refraction ratio for two index-of-refraction weights, flipping them if neccessary
    if the viewpoint is inside the object as indicated by N.dot(V) >= 0.0.
*/
/*static*/
inline double
RayShader::getRefractionRatio(const Fsr::Vec3d& V,
                              const Fsr::Vec3d& N,
                              double            ior_from,
                              double            ior_to)
{
    if (N.dot(V) >= 0.0)
        return ior_from/ior_to;
    else
        return ior_to/ior_from;
}


/*! Calcs a refracted incident vector 'I'.
    'eta' is the ratio of the indices-of-refraction (ior) differences between
    two materials, such as the one returned by the getRefractionRatio() method.
    Returns false if total internal reflection.
*/
/*static*/
inline bool
RayShader::refract(const Fsr::Vec3d& I,
                   const Fsr::Vec3d& N,
                   double            eta,
                   Fsr::Vec3d&       out)
{
    const double N_dot_I = N.dot(I);
    const double k = 1.0 - (eta*eta)*(1.0 - N_dot_I*N_dot_I);
    if (k < 0.0)
        return false; // total internal reflection

    out = I*eta - N*(eta*N_dot_I + std::sqrt(k)); // bend I
    out.normalize();

    return true;
}




/*! Returns the ratio of reflection vs. transmission for a view-vector and
    shading normal using Snell's law and Schlick's approximation.
*/
/*static*/
inline float
RayShader::reflectionRatioSnellSchlick(const Fsr::Vec3d& V,
                                       const Fsr::Vec3d& N,
                                       double            ior_from,
                                       double            ior_to,
                                       double            fresnel_power)
{
    // Get the refraction ratio (commonly termed eta):
    const double eta = getRefractionRatio(V, N, ior_from, ior_to);

    // Calculate the ratio of reflection vs. refraction (commonly termed f) using Snell's law:
    const double f = ((1.0 - eta)*(1.0 - eta)) / ((1.0 + eta)*(1.0 + eta));

    // Use a simplified fresnel equation (Schlick's approximation):
    float r;
    if (fresnel_power > 1.0)
        r = float(f + (1.0 - f)*::pow(1.0 - N.dot(V), fresnel_power)); // weight it by fresnel
    else
        r = float(f + (1.0 - f)*(1.0 - N.dot(V)));
    if (r < 0.0f)
        return 0.0f; // no reflection, max transmission
    else if (r < 1.0f)
        return r;

    return 1.0f; // max reflection, no transmission
}


/*! Simplified Oren-Nayer diffuse function (disarded C3 & interreflections ignored)
*/
/*static*/
inline double
RayShader::orenNayerSimplified(const Fsr::Vec3d& V,
                               const Fsr::Vec3d& N,
                               const Fsr::Vec3d& lightV,
                               double            roughnessSquared)
{
    // Fast-normlized vectors can sometimes create dot-products > 1.0 or < -1.0
    // so clamp them so the acos() functions don't blow up:
    const double N_dot_L = clamp(N.dot(lightV), -1.0, 1.0);
    const double N_dot_V = clamp(N.dot(V),      -1.0, 1.0);

    const Fsr::Vec3d VN = (V      - N*N_dot_V);
    const Fsr::Vec3d LN = (lightV - N*N_dot_L);

    const double angleNL = ::acos(N_dot_L);
    const double angleNV = ::acos(N_dot_V);

    const double A = 1.0 - 0.50*(roughnessSquared / (roughnessSquared + 0.57));
    const double B =       0.45*(roughnessSquared / (roughnessSquared + 0.09));
    const double C = (angleNV > angleNL) ? ::sin(angleNV)*::tan(angleNL) :
                                           ::sin(angleNL)*::tan(angleNV);
    const double gamma = std::max(0.0, VN.dot(LN));

    return (std::max(0.0, N_dot_L) * (A + B*gamma*C));
}


/*! Convenience function to encapsulate the RGB triplet in a Pixel object as a Vec3f.
*/
/*static*/
inline Fsr::Vec3f&
RayShader::vec3fColorInPixel(const Fsr::Pixel& pixel)
{
    return *(reinterpret_cast<Fsr::Vec3f*>(const_cast<float*>(pixel.chan + DD::Image::Chan_Red)));
}


/*! Replacement for the Pixel::under() method which does the wrong thing and doesn't handle alpha.
    We also handle Chan_Cutout in here.
*/
/*static*/
inline void
RayShader::A_under_B(const Fsr::Pixel&             A,
                     Fsr::Pixel&                   B,
                     const DD::Image::ChannelMask& channels)
{
    // Handle cutouts:
    //if      (B[DD::Image::Chan_Mask] > 0.5f) return; // Don't bother if B's a cutout
    //else if (A[DD::Image::Chan_Mask] > 0.5f)
    //{
    //    return;
    //}
    //
    float a = B[DD::Image::Chan_Alpha];
    if (a < std::numeric_limits<float>::epsilon())
    {
        foreach(z, channels)
            B.chan[z] += A.chan[z];
    }
    else if (a < 1.0f)
    {
        a = 1.0f - a;
        foreach(z, channels)
            B.chan[z] += A.chan[z]*a;
    }
    else
    {
        // saturated alpha - do nothing
    }
    //B[DD::Image::Chan_Mask] = 0.0f;
}


} // namespace zpr

#endif

// end of zprender/RayShader.h

//
// Copyright 2020 DreamWorks Animation
//
