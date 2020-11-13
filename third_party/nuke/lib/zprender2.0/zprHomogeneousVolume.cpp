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

/// @file zprender/zprHomogeneousVolume.cpp
///
/// @author Jonathan Egstad


#include "zprHomogeneousVolume.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "LightMaterial.h"

#include <DDImage/Knobs.h>
#include <DDImage/noise.h>


using namespace DD::Image;


namespace zpr {


//----------------------------------------------------------------------------

/* DD::Image::CurveDescription:
    const char*   name;          //!< name of curve (should be short). NULL ends the table
    std::string   defaultValue;  //!< string to parse to get the default curve
    BuildCallback buildCallback; //!< Ony for internal use, callback used to build the curve
    int           flags;         //!< [eNormal = 0, eReadOnly = 1]
    const char*   tooltip;
*/
static const DD::Image::CurveDescription falloff_defaults[] =
{
    // Give it all the values to silence 'missing-field-initializers' compiler warning.
    {"X", "y C 1.0 1.0", NULL, 0, "X range falloff"/*tooltip*/},
    {"Y", "y C 1.0 0.0", NULL, 0, "Y range falloff"/*tooltip*/},
    {"Z", "y C 1.0 1.0", NULL, 0, "Z range falloff"/*tooltip*/},
    { 0, "", NULL, 0, ""}
};

/*static*/ const char* const zprHomogeneousVolume::noise_types[] = { "fBm", "turbulence", 0 };

//----------------------------------------------------------------------------


/*! Need this just for the DD::Image::LookupCurves() initializer...
*/
zprHomogeneousVolume::InputParams::InputParams() :
    k_falloff_lut(falloff_defaults)
{
    //
}


/*!
*/
zprHomogeneousVolume::zprHomogeneousVolume() :
    VolumeShader()
{
    inputs.k_ray_step              = 0.1;
    inputs.k_ray_step_min          = 0.001;
    inputs.k_ray_step_count_min    = 10;
    inputs.k_ray_step_count_max    = 1000;
    inputs.k_preview_max_ray_steps = 10;
    inputs.k_atmospheric_density   = 0.1;
    inputs.k_density_base          = 0.0;
    inputs.k_volume_illum_factor   = 1.0;
    inputs.k_light_absorption      = true;
    //
    inputs.k_noise_enabled         = false;
    inputs.k_num_noise_functions   = 0;
    inputs.k_noise_xform.setToIdentity();
    //
    inputs.k_falloff_enabled       = false;
    inputs.k_falloff_bbox.set(0.0f, 0.0f, 0.0f,
                              1.0f, 1.0f, 1.0f);

    memset(m_noise_modules, 0, NUM_NOISE_FUNC*sizeof(VolumeNoise*));
    for (uint32_t j=0; j < NUM_NOISE_FUNC; ++j)
    {
        m_noise_modules[j] = new VolumeNoise();

        VolumeNoise& nmod = *m_noise_modules[j];

        char buf[128];
        sprintf(buf, "noise%u", j+1); // tab name
        nmod.knob_names[0] = strdup(buf);
        sprintf(buf, "noise_enable%u", j);
        nmod.knob_names[1] = strdup(buf);
        sprintf(buf, "noise_type%u", j);
        nmod.knob_names[2] = strdup(buf);
        sprintf(buf, "noise_octaves%u", j);
        nmod.knob_names[3] = strdup(buf);
        sprintf(buf, "noise_lacunarity%u", j);
        nmod.knob_names[4] = strdup(buf);
        sprintf(buf, "noise_gain%u", j);
        nmod.knob_names[5] = strdup(buf);
        sprintf(buf, "noise_mix%u", j);
        nmod.knob_names[6] = strdup(buf);
        //
        sprintf(buf, "noise_translate%u", j);
        nmod.knob_names[7] = strdup(buf);
        sprintf(buf, "noise_rotate%u", j);
        nmod.knob_names[8] = strdup(buf);
        sprintf(buf, "noise_scale%u", j);
        nmod.knob_names[9] = strdup(buf);
        sprintf(buf, "noise_uniform_scale%u", j);
        nmod.knob_names[10] = strdup(buf);
        //
        nmod.k_enabled        = false;
        nmod.k_type           = NOISE_FBM;
        nmod.k_octaves        = 10;
        nmod.k_lacunarity     = 2.0;
        nmod.k_gain           = 1.0;
        nmod.k_mix            = 1.0;
        nmod.k_translate.set(0.0f, 0.0f, 0.0f);
        nmod.k_rotate.set(0.0f, 0.0f, 0.0f);
        nmod.k_scale.set(1.0f, 1.0f, 1.0f);
        nmod.k_uniform_scale  = 1.0f;
        //
        nmod.m_xform.setToIdentity();
    }
}


/*!
*/
zprHomogeneousVolume::zprHomogeneousVolume(const InputParams& input_params) :
    VolumeShader(),
    inputs(input_params)
{
    //
}


/*!
*/
zprHomogeneousVolume::~zprHomogeneousVolume()
{
    for (uint32_t j=0; j < NUM_NOISE_FUNC; ++j)
    {
        if (!m_noise_modules[j])
            continue;
        VolumeNoise& nmod = *m_noise_modules[j];
        for (uint32_t i=0; i < 11; ++i)
            free(nmod.knob_names[i]);
        delete m_noise_modules[j];
    }
}


/*! Initialize any uniform vars prior to rendering.
    This may be called without a RenderContext from the legacy shader system.
*/
/*virtual*/
void
zprHomogeneousVolume::updateUniformLocals(double  frame,
                                          int32_t view)
{
    //std::cout << "  zprHomogeneousVolume::updateUniformLocals()"<< std::endl;
    VolumeShader::updateUniformLocals(frame, view);

    // Clamp some controls to reasonable limits:
    m_density      = (float)std::max(0.0001, inputs.k_atmospheric_density);
    m_density_base = (float)std::max(0.0,    inputs.k_density_base);

    if (inputs.k_falloff_enabled)
    {
        // Init falloff bbox:
        m_falloff_bbox.setMin(std::min(inputs.k_falloff_bbox.min.x, inputs.k_falloff_bbox.max.x),
                              std::min(inputs.k_falloff_bbox.min.y, inputs.k_falloff_bbox.max.y),
                              std::min(inputs.k_falloff_bbox.min.z, inputs.k_falloff_bbox.max.z));
        m_falloff_bbox.setMax(std::max(inputs.k_falloff_bbox.min.x, inputs.k_falloff_bbox.max.x),
                              std::max(inputs.k_falloff_bbox.min.y, inputs.k_falloff_bbox.max.y),
                              std::max(inputs.k_falloff_bbox.min.z, inputs.k_falloff_bbox.max.z));
    }

    if (inputs.k_noise_enabled)
    {
        const Fsr::Mat4d noise_xform(inputs.k_noise_xform);
        for (uint32_t j=0; j < NUM_NOISE_FUNC; ++j)
        {
            if (!m_noise_modules[j])
                continue;

            VolumeNoise& nmod = *m_noise_modules[j];
            if (!nmod.k_enabled || nmod.k_mix < std::numeric_limits<double>::epsilon())
                continue;

            if (j == 0)
            {
                nmod.m_xform = noise_xform.inverse();
            }
            else
            {
                Fsr::Mat4d m;
                m.setToIdentity();
                m.setToScale(nmod.k_scale*float(nmod.k_uniform_scale));
                m.rotateY(radiansf(nmod.k_rotate.y));
                m.rotateX(radiansf(nmod.k_rotate.x));
                m.rotateZ(radiansf(nmod.k_rotate.z));
                m.translate(nmod.k_translate);
                m *= noise_xform;
                nmod.m_xform = m.inverse();
            }
        }
    }

}


/*!
*/
/*virtual*/
bool
zprHomogeneousVolume::volumeMarch(zpr::RayShaderContext&                stx,
                                  double                                tmin,
                                  double                                tmax,
                                  double                                depth_min,
                                  double                                depth_max,
                                  float                                 surface_Z,
                                  float                                 surface_alpha,
                                  const Volume::VolumeIntersectionList& vol_intersections,
                                  Fsr::Pixel&                           color_out,
                                  Traceable::DeepIntersectionList*      deep_out) const
{
#if DEBUG
    assert(stx.rtx); // shoudldn't happen...
#if 0
    assert(stx.lighting_scene); // need a lighting scene!
#endif
#endif
    //std::cout << "zprHomogeneousVolume::volumeMarch(" << this << "): Rtx[" << stx.Rtx << "]";
    //std::cout << " vol_intersections=" << vol_intersections.size() << std::endl;

    //-----------------------------------------------------------------------
    // Ray march params:
    //-----------------------------------------------------------------------

    // Clamp tmin to minimum starting offset from camera:
    tmin = std::max(0.0, tmin);

    double ray_step_incr = clamp(fabs(inputs.k_ray_step    ), 0.0001, 100.0);
    double ray_step_min  = clamp(fabs(inputs.k_ray_step_min), 0.0001, double(ray_step_incr));

    // Scale ray step down to make minimum number of steps:
    if ((depth_min / ray_step_incr) < double(inputs.k_ray_step_count_min))
    {
        ray_step_incr = depth_min / double(inputs.k_ray_step_count_min);
    }
    else if (inputs.k_ray_step_count_max > inputs.k_ray_step_count_min &&
             (depth_max / ray_step_incr) > double(inputs.k_ray_step_count_max))
    {
        ray_step_incr = depth_max / double(inputs.k_ray_step_count_max);
    }

    // Possibly change step size depending on preview mode:
    if (stx.rtx->k_preview_mode && inputs.k_preview_max_ray_steps > 0)
    {
        // Keep ray step from exceeding max count:
        ray_step_incr = std::max(ray_step_incr, (tmax - tmin) / double(inputs.k_preview_max_ray_steps));
    }
    else
    {
        // Stop high-quality renders from blowing up:
        if (ray_step_incr < ray_step_min)
            ray_step_incr = ray_step_min;
    }

    if (stx.rtx->k_show_diagnostics == RenderContext::DIAG_VOLUMES)
    {
        color_out.color().set(float(tmin), float(tmax), float(tmax - tmin));
        color_out.alpha() = 0.0f;
        color_out.cutoutAlpha() = 0.0f;
        return true;
    }

    // Dummy VertexContext for light shaders....
    //DD::Image::VertexContext vtx;

    Fsr::Pixel& lt_color = stx.thread_ctx->illum_color;

    Fsr::Vec3f illum;
    Fsr::Vec3f voxel_opacity;
    //Fsr::Pixel lt_color(DD::Image::Mask_RGB);
    //Fsr::Pixel shad(DD::Image::Mask_RGB);

    const DD::Image::ChannelSet rgba_channels(DD::Image::Mask_RGBA);
    Traceable::DeepIntersection vI(rgba_channels);
    vI.I.object = NULL; // null object indicates a volume sample!
    vI.spmask = Dcx::SPMASK_FULL_COVERAGE;
    vI.count  = 1; // always 1 (no combining)

    double firstZ = -1.0; // set this to the first non-transparent volume Z
    // Starting Zf:
    double Zf = std::numeric_limits<double>::epsilon() + tmin;
    double Zb = Zf;

    //------------------------------------------------------
    // RAY MARCH THROUGH VOLUMES
    //------------------------------------------------------
    //std::cout << "ray march: tmin=" << tmin << " tmax=" << tmax << std::endl;

    uint32_t abort_check = 0;
    int step = 1;
    bool step_enabled = true;
    while (step_enabled)
    {
        if (++abort_check > 100)
        {
            if (stx.rtx->aborted())
                return false;
            abort_check = 0;
        }

        // Update Zb:
        Zb = std::numeric_limits<double>::epsilon() + tmin + (double(step)*ray_step_incr);
        if (Zb >= tmax)
        {
            Zb = tmax;
            if ((Zb - Zf) < std::numeric_limits<float>::epsilon())
                break;
            step_enabled = false; // stop after this step
        }
        //std::cout << "step=" << step << " Zf=" << Zf << ", Zb=" << Zb << std::endl;


        // Update the worldspace point location in shader context:
        stx.PW = stx.Rtx.getPositionAt(Zb);


        //---------------------------------------------------
        // Starting voxel density
        //
        double density = m_density;


#if 0
        //---------------------------------------------------
        // Vary density in turbulence field
        //
        if (inputs.k_noise_enabled)
        {
            bool apply_noise = false;
            double noise_density = 0.0;
            for (uint32_t i=0; i < NUM_NOISE_FUNC; ++i)
            {
                if (m_noise_modules[i] == NULL)
                    continue;

                const VolumeNoise& nmod = *m_noise_modules[i];
                if (!nmod.k_enabled || nmod.k_mix < 0.00001)
                   continue;

                apply_noise = true;

                Fsr::Vec3d PWn = nmod.m_xform.transform(PW);
                double noise;
                if (nmod.k_type == NOISE_FBM)
                {
                    noise = DD::Image::fBm(PWn.x, PWn.y, PWn.z,
                                           nmod.k_octaves,
                                           nmod.k_lacunarity,
                                           nmod.k_gain*0.5);
                }
                else
                {
                    noise = DD::Image::turbulence(PWn.x, PWn.y, PWn.z,
                                                  nmod.k_octaves,
                                                  nmod.k_lacunarity,
                                                  nmod.k_gain);
                }
                noise *= clamp(nmod.k_mix);
                noise += 1.0;
                noise  = clamp(noise, 0.0, 2.0);
                noise_density = std::max(noise, noise_density);
            }

            if (apply_noise)
                density *= noise_density;
        }


        //---------------------------------------------------
        // Vary density by spatial falloff
        //
        if (inputs.k_falloff_enabled)
        {
            // Get point location inside falloff bbox:
            double fx = std::min(std::max(PW.x, m_falloff_bbox.x()), m_falloff_bbox.r());
            fx = (fx - m_falloff_bbox.x()) / m_falloff_bbox.w();

            double fy = std::min(std::max(PW.y, m_falloff_bbox.y()), m_falloff_bbox.t());
            fy = (fy - m_falloff_bbox.y()) / m_falloff_bbox.h();

            double fz = std::min(std::max(PW.z, m_falloff_bbox.z()), m_falloff_bbox.f());
            fz = (fz - m_falloff_bbox.z()) / m_falloff_bbox.d();
            density *= clamp(inputs.k_falloff_lut.getValue(0, fx)); // X
            density *= clamp(inputs.k_falloff_lut.getValue(1, fy)); // Y
            density *= clamp(inputs.k_falloff_lut.getValue(2, fz)); // Z
        }
#endif

        //---------------------------------------------------
        // Add user density bias:
        density += inputs.k_density_base;


        // TODO: if falloff and noise are off then we can calculated the overall
        // density from the current ray origin to the first volume Zf. This
        // allows us to only ray march within the volume ranges.



        //------------------------------------------------------------------------
        // Calculate the aborption factor for this voxel's density (Beer-Lambert):
        //
        //   absorption calc is:
        //     absorption = 1.0 - exp(-density * dPdz)
        //   and the inverse is:
        //     density = -log(1.0 - absorption) / dPdz
        //
        const float absorption = float(1.0 - exp(-density * ray_step_incr));
        // Opacity is always solid (1.0) which is then attenuated by the absorption
        // factor just like the RGB color:
        voxel_opacity.set(absorption, absorption, absorption/*1.0f * absorption*/);

    
        // Get all light illumination at this point in space:
        illum.set(0,0,0);
        const uint32_t nVolumes = (uint32_t)vol_intersections.size();
        for (uint32_t v=0; v < nVolumes; ++v)
        {
            const Volume::VolumeIntersection& vI = vol_intersections[v];
            // Skip the volumes not intersected with this z:
            if (Zb < vI.tmin || Zb > vI.tmax)
                continue;

            // Call zpr::LightShaders instead of legacy LightOp methods:
#if DEBUG
            assert(vI.object); // shouldn't happen...
#endif
            stx.rprim = static_cast<zpr::RenderPrimitive*>(vI.object);
            MaterialContext* material_ctx = stx.rprim->getMaterialContext();
#if DEBUG
            assert(material_ctx);
            assert(material_ctx->raymaterial);
#endif
            LightMaterial* lt_material = static_cast<LightMaterial*>(material_ctx->raymaterial);
#if DEBUG
            assert(lt_material->getLightShader());
#endif

            Fsr::RayContext Rlight; // ray from volume point to light, for shadowing, etc.
            float direct_pdfW;
            if (!lt_material->getLightShader()->illuminate(stx, Rlight, direct_pdfW, lt_color))
                continue; // not affecting this point in space

            lt_color.rgb() *= direct_pdfW;
            if (lt_color.rgb().isZero())
                continue;

            // Get shadowing factor for light (0=shadowed, 1=no shadow):
            //float shadow = 1.0f;
            RayShaderContext Rshadow_stx(stx,
                                         Rlight,
                                         Fsr::RayContext::shadowPath()/*ray_type*/,
                                         RenderContext::SIDES_BOTH/*sides_mode*/);
            Traceable::SurfaceIntersection Ishadow(std::numeric_limits<double>::infinity());
            if (stx.rtx->objects_bvh.getFirstIntersection(Rshadow_stx, Ishadow) > Fsr::RAY_INTERSECT_NONE &&
                 Ishadow.t < Rlight.maxdist)
            {
#if 1
                continue;
#else
                //std::cout << "D=" << D << ", t=" << Ishadow.t << std::endl;
                // Shadowed - make it fall off the farther the occluder is from surface(hack!!!):
                shadow = (Ishadow.t / D);//powf(float(1.0 - (Ishadow.t / D)), 2.0f);
                if (shadow <= 0.0f)
                {
                    continue;
                }
#endif
            }

            // Only consider the light if its contribution is non-zero:
            if (lt_color.rgb().notZero())
            {
                // Further attenuate the light by the density of the medium:
                if (inputs.k_light_absorption)
                {
                    //------------------------------------------------------------------------
                    // Calculate the light's aborption factor to this point (Beer-Lambert):
                    //
                    //   absorption calc is:
                    //     absorption = 1.0 - exp(-density * dPdz)
                    const float transmission = float(exp(-density * (Rlight.maxdist - Rlight.mindist)));
                    lt_color.rgb() *= transmission;
                }

                illum += lt_color.rgb()*float(inputs.k_volume_illum_factor);
            }

        } // loop nVolumes

        // Further attenuate it if it's past the front surface Z point and surface alpha is < 1.0:
        if (surface_Z < std::numeric_limits<float>::infinity() &&
            Zb > surface_Z &&
            surface_alpha < 0.999)
        {
            const float a = 1.0f - surface_alpha;
            illum *= a;
            voxel_opacity *= a;
        }

        // Accumulate if there's some density:
        if (illum.x > 0.0f ||
            illum.y > 0.0f ||
            illum.z > 0.0f)
        {
            if (deep_out)
            {
                //vI.color[PERCEPTIVE_WEIGHT_CHANNEL] = 
                vI.color[DD::Image::Chan_Red  ] = illum.x*voxel_opacity.x;
                vI.color[DD::Image::Chan_Green] = illum.y*voxel_opacity.y;
                vI.color[DD::Image::Chan_Blue ] = illum.z*voxel_opacity.z;
                vI.color[DD::Image::Chan_Alpha] = voxel_opacity.x;

                vI.color[DD::Image::Chan_DeepFront] = float(Zf);
                vI.color[DD::Image::Chan_DeepBack ] = float(Zb);
                vI.color[DD::Image::Chan_Z        ] = float(Zb);

                deep_out->push_back(vI);
            }
            else
            {
                // UNDER the illumination for this voxel:
                const float iBa = 1.0f - color_out[DD::Image::Chan_Alpha];
                color_out[DD::Image::Chan_Red  ] += illum.x*voxel_opacity.x * iBa;
                color_out[DD::Image::Chan_Green] += illum.y*voxel_opacity.y * iBa;
                color_out[DD::Image::Chan_Blue ] += illum.z*voxel_opacity.z * iBa;
                color_out[DD::Image::Chan_Alpha] += voxel_opacity.x * iBa;

                // saturated alpha, stop marching
                //if (color_out[Chan_Alpha] >= 1.0f)
                //   break;
            }
            if (firstZ < 0.0)
                firstZ = Zb;

        }
        else if (!step_enabled && deep_out)
        {
            // Always write out last deep sample, even if it's black:
            vI.color[DD::Image::Chan_Red  ] = 0.0f;
            vI.color[DD::Image::Chan_Green] = 0.0f;
            vI.color[DD::Image::Chan_Blue ] = 0.0f;
            vI.color[DD::Image::Chan_Alpha] = voxel_opacity[DD::Image::Chan_Red];

            vI.color[DD::Image::Chan_DeepFront] = float(Zf);
            vI.color[DD::Image::Chan_DeepBack ] = float(Zb);
            vI.color[DD::Image::Chan_Z        ] = float(Zb);

            deep_out->push_back(vI);
        }

        Zf = Zb;

        ++step;

    } // Ray march loop

    // All samples transparent?
    if (firstZ < 0.0)
        return true;

    color_out.cutoutAlpha() = color_out.alpha();

    // Set output Z to first non-transparent sample:
    color_out.Z() = float(firstZ);

    return true;
}


} // namespace zpr


// end of zprender/zprHomogeneousVolume.cpp

//
// Copyright 2020 DreamWorks Animation
//
