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

/// @file zprender/RayMaterial.h
///
/// @author Jonathan Egstad


#ifndef zprender_RayMaterial_h
#define zprender_RayMaterial_h

#include "RayShader.h"

#include <Fuser/NukePixelInterface.h> // for Fsr::Pixel
#include <Fuser/MaterialNode.h>


namespace zpr {

class LightShader;
class VolumeShader;


/*! Interface class to RayShaders.
*/
class ZPR_EXPORT RayMaterial
{
  public:
    struct Visibility
    {
        int  k_sides_mode;               //!< Which side this material applies to (default is SIDES_BOTH)
        bool k_camera_visibility;        //!< Is this shader visible to camera rays?
        bool k_shadow_visibility;        //!< Is this shader visible to shadow rays?
        bool k_specular_visibility;      //!< Is this shader visible to specular rays?
        bool k_diffuse_visibility;       //!< Is this shader visible to diffuse rays?
        bool k_transmission_visibility;  //!< Is this shader visible to transmitted rays?


        Visibility() :
            k_sides_mode(0/*RenderContext::SIDES_BOTH*/),
            k_camera_visibility(true),
            k_shadow_visibility(true),
            k_specular_visibility(true),
            k_diffuse_visibility(true),
            k_transmission_visibility(true)
        {
            //
        }
    };


  protected:
    std::vector<RayShader*> m_shaders;              //!< List of child shaders
    RayShader*              m_surface_shader;       //!< Output surface shader
    RayShader*              m_displacement_shader;  //!< Output displacement shader
    RayShader*              m_volume_shader;        //!< Output volume shader

    Visibility              m_visibility;

    DD::Image::ChannelSet   m_texture_channels;     //!< Set of channels output by all texture bindings
    DD::Image::ChannelSet   m_output_channels;      //!< Set of all output channels

    //! Fill in a list with pointers to the *active* texture bindings this shader and its inputs has.
    //virtual void _getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);

    //! Initialize any vars prior to rendering.
    //virtual void _validateMaterial(bool                 for_real,
    //                               const RenderContext& rtx);


  public:
    //!
    RayMaterial();
    //!
    RayMaterial(std::vector<RayShader*> shaders,
                RayShader*              output_surface_shader,
                RayShader*              output_displacement_shader=NULL,
                RayShader*              output_volume_shader=NULL);

    //! Deletes any RayShader children.
    ~RayMaterial();


    //! Adds a shader to the group list, taking ownership of shader allocation.
    RayShader* addShader(const char* shader_class);
    RayShader* addShader(RayShader* shader);

    //!
    void setSurfaceShader(RayShader* shader)      { m_surface_shader = shader; }
    void setDisplacementShader(RayShader* shader) { m_displacement_shader = shader; }
    void setVolumeShader(RayShader* shader)       { m_volume_shader = shader; }

    //!
    RayShader* getSurfaceShader()      const { return m_surface_shader; }
    RayShader* getDisplacementShader() const { return m_displacement_shader; }
    RayShader* getVolumeShader()       const { return m_volume_shader; }


    //! Current modes for this material.
    int  getSidesMode()              const { return m_visibility.k_sides_mode; }
    bool getDameraVisibility()       const { return m_visibility.k_camera_visibility; }
    bool getShadowVisibility()       const { return m_visibility.k_shadow_visibility; }
    bool getSpecularVisibility()     const { return m_visibility.k_specular_visibility; }
    bool getDiffuseVisibility()      const { return m_visibility.k_diffuse_visibility; }
    bool getTransmissionVisibility() const { return m_visibility.k_transmission_visibility; }


    //! Initialize any vars prior to rendering.
    void validateMaterial(bool                 for_real,
                          const RenderContext& rtx);

    //! Fill in a list with pointers to the *active* texture bindings this shader and its inputs has.
    void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);


    /*! Return true if the vertex_shader() method is implemented and should be called.
        The vertex_shader() call is required by some shaders to perturb normals, uvs, or
        point locations, but the Primitive::vertex_shader() method is expensive to call on
        big models, so this method allows us to skip calling it unless required by the
        RayShader.
    */
    bool vertexShaderActive() { return false; }

    //! Which uniform subd level to displace to.
    int getDisplacementSubdivisionLevel() const { return 0; }

    //! Return the channels output by all the textures in this shader, and any inputs.
    DD::Image::ChannelSet getTextureChannels() { return m_texture_channels; }

    //! Return the channels output by this shader, and any inputs.
    DD::Image::ChannelSet getChannels() { return m_output_channels; }


  public:
    /*! This copies info from the Intersection structure into the RayShaderContext structure
        in preparation for calling a RayShader evaluation method.
    */
    static void updateShaderContextFromIntersection(const Traceable::SurfaceIntersection& I,
                                                    RayShaderContext&                     stx);

    /*! Construct a DD::Image::VertexContext that can be passed to a fragment_shader()
        RayShaderContext adapter for calling DDImage Materials with a DD::Image::VertexContext.
    */
    static void updateDDImageShaderContext(const RayShaderContext&   stx,
                                           DD::Image::VertexContext& vtx);


  public:
    /*! Build a RayMaterial with the Fuser ShaderNodes converted to zpr RayShader
        equivalents of UsdPreviewSurface shaders.
    */
    static RayMaterial* createUsdPreviewSurface(Fsr::ShaderNode* surface_output);


  public:
    //! Abstracted illumination entry point.
    static void getIllumination(RayShaderContext&                stx,
                                Fsr::Pixel&                      out,
                                Traceable::DeepIntersectionList* deep_out=0);


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


};


} // namespace zpr

#endif

// end of zprender/RayMaterial.h

//
// Copyright 2020 DreamWorks Animation
//
