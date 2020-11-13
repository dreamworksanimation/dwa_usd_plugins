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

/// @file zprender/LightMaterial.h
///
/// @author Jonathan Egstad


#ifndef zprender_LightMaterial_h
#define zprender_LightMaterial_h

#include "RayMaterial.h"
#include "LightShader.h"


namespace zpr {

class MaterialContext;


/*! LightMaterial is subclassed from RayMaterial solely so it can be
    handled in the SurfaceContexts which are used to translate both
    GeoInfos and LightOps from DD::Image.

    Most of the RayMaterial interface can be ignored in the light shader
    case and just the interface in LightMaterial used. TODO: a better
    way to do this would be to have RayMaterial be abstract and create
    an ObjectMaterial that has the hard-surface interface in it.

    The LightMaterial interface provides a translation layer for converting
    DD::Image::LightOps and Fuser::LightOps to zpr LightShaders.

    TODO: make the translators be Fuser plugins so we can extend the
    layer later on without changing the zpRender lib.
*/
class ZPR_EXPORT LightMaterial : public RayMaterial
{
  protected:
    LightShader*    m_light_shader;         //!< Output light shader
    Fsr::Box3d      m_light_volume_bbox;    //!< LightVolume bbox for all motion samples

    Fsr::DoubleList m_motion_times;         //!< Frame time for each motion-sample
    Fsr::Mat4dList  m_motion_xforms;        //!< May be modified when copied into LightShader


  public:
    //! Leaves all shaders assigments empty.
    LightMaterial();
    //!
    LightMaterial(const Fsr::DoubleList&  motion_times,
                  const Fsr::Mat4dList&   motion_xforms,
                  std::vector<RayShader*> shaders,
                  LightShader*            output_light_shader);

    //! Deletes any LightShader children.
    virtual ~LightMaterial();


    //---------------------------------------------------------


    //!
    void          setLightShader(LightShader* shader) { m_light_shader = shader; }
    LightShader*  getLightShader() const { return m_light_shader; }

    //! Worldspace bbox for all motion samples.
    const Fsr::Box3d& getLightVolumeBbox() const { return m_light_volume_bbox; }

    //---------------------------------------------------------


    //! Initialize any vars prior to rendering.
    /*virtual*/ void validateMaterial(bool                 for_real,
                                      const RenderContext& rtx);

    //! Fill in a list with pointers to the *active* texture bindings this shader and its inputs has.
    /*virtual*/ void getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings);


    //---------------------------------------------------------


    /*! Create a LightVolume primitive appropriate for the assigned LightShader.
        Calling function takes ownership.
        MaterialContext is passed for use in the Volume ctors.

        Calls createLightVolume() on the assigned output light shader.
    */
    LightVolume* createLightVolume(const MaterialContext* material_ctx);


    //---------------------------------------------------------


    /*! Evaluate the light's contribution to a surface intersection.
        Returns false if light does not contribute to surface illumination.

        Calls illuminate() on the assigned output light shader.
    */
    bool illuminate(RayShaderContext& stx,
                    Fsr::RayContext&  light_ray,
                    float&            direct_pdfW_out,
                    Fsr::Pixel&       light_color_out);


    //---------------------------------------------------------

    /*!
    */
    static LightMaterial* createLightMaterial(const RenderContext&   rtx,
                                              DD::Image::LightOp*    light,
                                              const Fsr::DoubleList& motion_times,
                                              const Fsr::Mat4dList&  motion_xforms);

};


} // namespace zpr

#endif

// end of zprender/LightMaterial.h

//
// Copyright 2020 DreamWorks Animation
//
