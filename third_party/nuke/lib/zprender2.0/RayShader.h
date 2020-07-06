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
#include "InputBinding.h"

#include <DDImage/Description.h>
#include <DDImage/Knobs.h>


namespace zpr {

class LightShader;
class VolumeShader;


/*! Base interface class of ray-tracing shaders.
*/
class ZPR_EXPORT RayShader
{
  private:
    //! Disabled copy constructor.
    RayShader(const RayShader&);
    //! Disabled assignment operator.
    RayShader& operator = (const RayShader&);


  public:
    /*! This structure creates a subclass of zpr::RayShader.
    */
    class FSR_EXPORT ShaderDescription : public DD::Image::Description
    {
      private:
        const char* m_shader_class;

        /*! Method type defined in DD::Image::Description.h: (*f)(Description*)
            Called when the plugin .so is first loaded.
        */
        static void pluginBuilderCallback(DD::Image::Description* desc);


      public:
        //! Constructor method definition used for 'build()' methods in plugins.
        typedef RayShader* (*PluginBuilder)(void);
        PluginBuilder builder_method; // <<< Call this to construct a zpr::RayShader object.


      public:
        //! Constructor sets name and label to same value.
        ShaderDescription(const char*   shader_class,
                          PluginBuilder builder);

        //!
        const char* shaderClass() const { return m_shader_class; }

        //! Find a dso description by name.
        static const ShaderDescription* find(const char* shader_class);
    };


  public:
    /*! Limited data types for RayShader knob inputs.
       (we'll use Nuke nomenclature here for no good reason... :) )
    */
    enum KnobType
    {
        EMPTY_KNOB,
        //
        STRING_KNOB,    //!< std::string
        //
        INT_KNOB,       //!< Also used for boolean
        DOUBLE_KNOB,    //!< Not bothering with separate float type
        //
        VEC2_KNOB,      //!< Fsr::Vec2d
        VEC3_KNOB,      //!< Fsr::Vec3d
        VEC4_KNOB,      //!< Fsr::Vec4d
        MAT4_KNOB,      //!< Fsr::Mat4d
        //
        COLOR2_KNOB,    //!< Fsr::Vec2d - mono with alpha
        COLOR3_KNOB,    //!< Fsr::Vec3d - rgb
        COLOR4_KNOB,    //!< Fsr::Vec4d - rgba
        //
        VEC2ARRAY_KNOB,
        VEC3ARRAY_KNOB,
        VEC4ARRAY_KNOB,
        //
        PIXEL_KNOB,     //!< Fsr::Pixel (also contains a ChannelSet)
        //
        NUM_KNOB_TYPES
    };


    /*! Shader input. Similar to an Op knob except dedicated to RayShader
        use with support for external binding to another RayShader's output
        knob.
    */
    struct InputKnob
    {
        const char* name;           //!<
        KnobType    type;           //!<
        void*       data;           //!< Pointer to local data, cast to type
        RayShader*  shader;         //!< Non-NULL if knob is bound to another RayShader's output
        int32_t     output_index;   //!< Output index of another RayShader


        //!
        InputKnob() : name(""), type(EMPTY_KNOB), data(NULL), shader(NULL), output_index(-1) {}
        //!
        InputKnob(const char* _name,
                  KnobType    _type) :
            name(_name),
            type(_type),
            data(NULL),
            shader(NULL),
            output_index(-1)
        {
            //
        }

        //!
        void setValue(const char* value);
    };

    typedef std::vector<InputKnob> InputKnobList;


    /*!
    */
    struct OutputKnob
    {
        const char* name;               //!<
        KnobType    type;               //!<


        //!
        OutputKnob() : name(""), type(EMPTY_KNOB) {}
        //!
        OutputKnob(const char* _name,
                   KnobType    _type) :
            name(_name),
            type(_type)
        {
            //
        }
    };

    typedef std::vector<OutputKnob> OutputKnobList;


  protected:
    std::string             m_name;             //!< Shader name
    //
    InputKnobList           m_inputs;           //!< Input knobs, copied and updated from the static list
    OutputKnobList          m_outputs;          //!< Output knobs, copied from the static list
    //
    bool                    m_valid;            //!< validateShader() has been called
    DD::Image::ChannelSet   m_texture_channels; //!< Set of channels output by all texture bindings
    DD::Image::ChannelSet   m_output_channels;  //!< Set of all output channels


    //! Subclass implementation of connectInput().
    virtual void _connectInput(uint32_t    input,
                               RayShader*  shader,
                               const char* output_name);


  public:
    //!
    RayShader();
    //!
    RayShader(const InputKnobList&  inputs,
              const OutputKnobList& output);

    //! Must have a virtual destructor!
    virtual ~RayShader() {}


    //---------------------------------------------------------


    //! Returns 'zpRayShader'.
    static const char* zpClass();


    //! Returns the class name, must implement.
    virtual const char* zprShaderClass() const=0;


    //! Create a zpr::RayShader instance based on the shader class name or existing shader description.
    static RayShader* create(const char* shader_class);
    static RayShader* create(const ShaderDescription& shader_description);

    //! Find a RayShader::ShaderDescription by shader class name.
    static const ShaderDescription* find(const char* node_class) { return ShaderDescription::find(node_class); }


    //---------------------------------------------------------


    //! Whether shader is ready to be evaluated. True after validateShader() has been called.
    bool isValid() const { return m_valid; }
    //! Cause validateShader() to be called during next evaluation.
    void invalidate() { m_valid = false; }

    //! Return the name identifier if assigned.
    const std::string& getName() const { return m_name; }
    //! Assign a name identifier.
    void               setName(const std::string& name) { m_name = name; }


    //! Return a static list of input knobs for this shader. Base class returns an empty list.
    virtual const InputKnobList&  getInputKnobDefinitions() const;

    //! Return a static list of output knobs for this shader. Base class returns only the 'primary' output.
    virtual const OutputKnobList& getOutputKnobDefinitions() const;


    //---------------------------------------------------------


    //! Returns the number of input knobs.
    uint32_t numInputs() const { return (uint32_t)m_inputs.size(); }
    //! Returns the number of output knobs.
    uint32_t numOutputs() const { return (uint32_t)m_outputs.size(); }

    //! Returns output knob by index. If there's no knob an empty InputKnob is returned.
    const InputKnob&  getInputKnob(uint32_t input) const;
    //! Returns output knob by index. If there's no knob an empty OutputKnob is returned.
    const OutputKnob& getOutputKnob(uint32_t output) const;


    //! Return a named input's index or -1 if not found.
    int32_t    getInputByName(const char* input_name) const;
    //! Return a named output's index or -1 if not found.
    int32_t    getOutputByName(const char* output_name) const;


    //! Returns shader pointer for input. May be NULL if there's no input or no connection.
    RayShader* getInput(uint32_t input) const;


    //! Returns an InputBinding object for an input.
    virtual InputBinding* getInputBinding(uint32_t input) { return NULL; }


    //! Returns true if input can be connected to another RayShader's named output.
    virtual bool canConnectInputTo(uint32_t    input,
                                   RayShader*  shader,
                                   const char* output_name="surface");

    //! Assign the input RayShader pointer for input. No range checking!
    bool         connectInput(uint32_t    input,
                              RayShader*  shader,
                              const char* output_name="surface");

    //!
    void setInputValue(uint32_t    input,
                       const char* value);
    void setInputValue(const char* input_name,
                       const char* value);

    //---------------------------------------------------------


    //!
    virtual LightShader*  isLightShader()  { return NULL; }
    //!
    virtual VolumeShader* isVolumeShader() { return NULL; }


    //! Initialize any vars prior to rendering.
    virtual void validateShader(bool                 for_real,
                                const RenderContext& rtx);

    //! Fill in a list with pointers to the *active* texture bindings this shader and its inputs has.
    virtual void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);


    /*! Return true if the vertex_shader() method is implemented and should be called.
        The vertex_shader() call is required by some shaders to perturb normals, uvs, or
        point locations, but the Primitive::vertex_shader() method is expensive to call on
        big models, so this method allows us to skip calling it unless required by the
        RayShader.
    */
    virtual bool vertexShaderActive() { return false; }


    //! Return the channels output by all the textures in this shader, and any inputs.
    virtual DD::Image::ChannelSet getTextureChannels() { return m_texture_channels; }

    //! Return the channels output by this shader, and any inputs.
    virtual DD::Image::ChannelSet getChannels() { return m_output_channels; }


  public:
    /*! Surface evaluation returns the radiance and aovs from this RayShader
        given an intersection point and incoming ray in the RayShaderContext.
        Base class sets color to 18% grey, full opacity.
    */
    virtual void evaluateSurface(RayShaderContext& stx,
                                 Fsr::Pixel&       out);

    /*! Surface displacement evaluation call.
        TODO: implement this, the Pixel output is likely wrong.
        Base class does nothing.
    */
    virtual void evaluateDisplacement(RayShaderContext& stx,
                                      Fsr::Pixel&       out);


  public:
    //------------------------------------------------------------------
    // Simple default integrators, for convenience.
    //------------------------------------------------------------------

    /*! Return the indirect diffuse illumination for surface point with normal N.
        Indirect diffuse means only rays that hit objects will contribute to the surface color.
    */
    static bool getIndirectDiffuse(RayShaderContext& stx,
                                    const Fsr::Vec3d& N,
                                    double            roughness,
                                    Fsr::Pixel&       out);

    /*! Return the indirect specular illumination for surface point with normal N.
        Indirect diffuse means only reflected rays that hit objects will contribute to the surface color.
    */
    static bool getIndirectGlossy(RayShaderContext& stx,
                                  const Fsr::Vec3d& N,
                                  double            roughness,
                                  Fsr::Pixel&       out);

    /*! Return the transmitted illumination for surface point with normal N.
        Transmission means only refracted rays that pass through objects will contribute to the surface color.
    */
    static bool getTransmission(RayShaderContext& stx,
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
