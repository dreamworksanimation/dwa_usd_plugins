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

/// @file zprender/VolumeMaterialOp.cpp
///
/// @author Jonathan Egstad


#include "VolumeMaterialOp.h"
#include "RenderContext.h"
#include "ThreadContext.h"

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

const char* const noise_types[] = { "fBm", "turbulence", 0 };

//----------------------------------------------------------------------------


/*!
*/
VolumeMaterialOp::VolumeMaterialOp(::Node* node) :
    VolumeShader(),
    DD::Image::Material(node),
    k_falloff_lut(falloff_defaults)
{
    k_ray_step              = 0.1;
    k_ray_step_min          = 0.001;
    k_ray_step_count_min    = 10;
    k_ray_step_count_max    = 1000;
    k_preview_max_ray_steps = 10;
    k_atmospheric_density   = 0.1;
    k_density_base          = 0.0;
    k_volume_illum_factor   = 1.0;
    k_light_absorption      = true;
    //
    k_noise_enabled         = false;
    k_num_noise_functions   = 0;
    k_noise_xform.setToIdentity();
    //
    k_falloff_enabled       = false;
    k_falloff_bbox.set(0.0f, 0.0f, 0.0f,
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
VolumeMaterialOp::~VolumeMaterialOp()
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


//-----------------------------------------------------------------------------


//!
/*static*/ const char* VolumeMaterialOp::zpClass() { return "zpVolumeMaterialOp"; }

/*!
*/
void
VolumeMaterialOp::addVolumeMaterialOpIdKnob(DD::Image::Knob_Callback f)
{
#ifdef ZPR_USE_KNOB_RTTI
    // HACK!!!! Define a hidden knob that can be tested instead of dynamic_cast:
    int dflt=0;
    Int_knob(f, &dflt, VolumeMaterialOp::zpClass(), DD::Image::INVISIBLE);
        DD::Image::SetFlags(f, DD::Image::Knob::DO_NOT_WRITE |
                               DD::Image::Knob::NO_ANIMATION |
                               DD::Image::Knob::NO_RERENDER);
#endif
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
VolumeMaterialOp::knobs(DD::Image::Knob_Callback f)
{
    addVolumeKnobs(f);
    Divider(f);
    addFalloffKnobs(f);

    //--------------------------------------------------------------------------
    // Noise tabs:
    addNoiseKnobs(f);
}


/*!
*/
/*virtual*/
void
VolumeMaterialOp::addVolumeKnobs(DD::Image::Knob_Callback f)
{
    addVolumeMaterialOpIdKnob(f);

    Int_knob(f, &k_preview_max_ray_steps, "preview_max_steps", "max steps");
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER);
        Tooltip(f, "The max number of ray steps to use in preview mode.  The lower the amount the faster "
                   "the preview but of course the quality also drops.");
    Int_knob(f, &k_ray_step_count_max, "max_steps", "full-quality:");
        SetFlags(f, Knob::NO_MULTIVIEW | Knob::NO_ANIMATION);
        ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
        Tooltip(f, "Limit the total number of ray steps to this.");
    Newline(f);
    Int_knob(f, &k_ray_step_count_min, IRange(5, 100), "min_steps", "min steps");
        SetFlags(f, Knob::SLIDER);
        Tooltip(f, "Where volumes are thin (like the start of a spotlight cone,) do at least this number of ray steps.\n"
                   "Ignored in preview mode.");
    Double_knob(f, &k_ray_step, IRange(0.0, 10.0), "step", "step size");
        SetFlags(f, Knob::LOG_SLIDER);
        Tooltip(f, "Full-quality step size.  Smaller value yields higher-quality but dramatically slows down the render.\n"
                   "Ignored in preview mode.");
    Double_knob(f, &k_ray_step_min, IRange(0.0, 10.0), "step_min", "min step size");
        //Clear_knob(f, Knob::SLIDER);
        Tooltip(f, "Don't go smaller that this step size (stops render times from blowing up.)\n"
                   "Ignored in preview mode.");

    //--------------------------------------------------------------------------
    Divider(f);
    Double_knob(f, &k_atmospheric_density, IRange(0.0, 5.0), "density", "atmospheric density");
    SetFlags(f, Knob::LOG_SLIDER);
        Tooltip(f, "Density per world-scale unit.  In other words, the density of the medium "
                   "through the thickness of one unit of space (i.e. 1 meter or 1 shreckle.)\n"
                   "When this increases the influence of the illumination sources on the atmosphere increases "
                   "(higher density means there's more particles in the air to scatter the light.)\n"
                   "Higher values may mean the illumination gain must be increased for the light to get "
                   "through the fog.");
    Double_knob(f, &k_density_base, IRange(0.0, 1.0), "density_base", "base density");
    SetFlags(f, Knob::LOG_SLIDER);
        Tooltip(f, "The ambient base level density.");
    Newline(f);
    Bool_knob(f, &k_light_absorption, "enable_light_absorption", "atmosphere attenuates light sources");
        Tooltip(f, "Additional density falloff over the light's reach (near->far).\n"
                   "This is separate from the falloff of the light itself which is assumed to be due to energy "
                   "dispersal over distance.\n"
                   "\n"
                   "When enabled this additional absorption may cause the light beam to look incorrectly "
                   "attenuated where it overlaps objects due to object surfaces still being illuminated "
                   "with the full light's strength. This is by design however since the volume rendering "
                   "algorithm is not using 'real' volumes and is not integrated with the surface "
                   "calculations.\n"
                   "\n"
                   "True volume rendering support is TBD.\n");
    Divider(f);
    Double_knob(f, &k_volume_illum_factor, IRange(0.01, 5.0), "illum_gain", "illumination factor");
       Tooltip(f, "Light additional gain.");
}


/*!
*/
/*virtual*/
void
VolumeMaterialOp::addFalloffKnobs(DD::Image::Knob_Callback f)
{
    Bool_knob(f, &k_falloff_enabled, "falloff_enable", "atmo falloff enable");
        Tooltip(f, "Enable atmospheric falloff.  This is confined inside the cube area defined on the "
                   "'bbox' control.\n"
                   "The X,Y & Z curves define the falloff in each axis respectively.  The default is for "
                   "the atmosphere in Y to be most dense at the bottom of the cube and least dense at the top.  "
                   "Changing the slope of the curves changes the rate of the falloff in that direction.");
    Box3_knob(f, k_falloff_bbox.array(), "falloff_bbox", "bbox");
        Tooltip(f, "Defines the XYZ cubic space containing the falloff curves.");
    LookupCurves_knob(f, &k_falloff_lut, "falloff_profile", "falloff profile");
        Tooltip(f, "Slope of a curve changes the rate the falloff in its respective direction.");
}


/*!
*/
/*virtual*/
void
VolumeMaterialOp::addNoiseKnobs(DD::Image::Knob_Callback f)
{
    Bool_knob(f, &k_noise_enabled, "noise_enable", "atmo noise master enable");
    Divider(f);
    for (uint32_t j=0; j < NUM_NOISE_FUNC; ++j)
    {
        if (!m_noise_modules[j])
            continue;
        VolumeNoise& nmod = *m_noise_modules[j];

        Tab_knob(f, nmod.knob_names[0]);

        Bool_knob(f, &nmod.k_enabled, nmod.knob_names[1], "enable");
        Enumeration_knob(f, &nmod.k_type, noise_types, nmod.knob_names[2], "noise");
        Int_knob(f, &nmod.k_octaves, IRange(1, 10), nmod.knob_names[3], "octaves");
            SetFlags(f, Knob::SLIDER);
        Double_knob(f, &nmod.k_lacunarity, IRange(1.0, 10.0), nmod.knob_names[4], "lacunarity");
            ClearFlags(f, Knob::LOG_SLIDER);
        Double_knob(f, &nmod.k_gain, IRange(-10.0, 10.0), nmod.knob_names[5], "gain");
            ClearFlags(f, Knob::LOG_SLIDER);
        Double_knob(f, &nmod.k_mix, IRange(0.0, 1.0), nmod.knob_names[6], "mix");
            ClearFlags(f, Knob::LOG_SLIDER);

        if (j == 0)
        {
            Divider(f, "Master Transform");
            Axis_knob(f, reinterpret_cast<DD::Image::Matrix4*>(k_noise_xform.array()), "noise_xform", "transform");
                SetFlags(f, Knob::NO_HANDLES);
        }
        else
        {
            Divider(f);
            XYZ_knob(f, nmod.k_translate.array(), nmod.knob_names[7], "translate");
                SetFlags(f, Knob::NO_HANDLES);
            XYZ_knob(f, nmod.k_rotate.array(), nmod.knob_names[8], "rotate");
                SetFlags(f, Knob::NO_HANDLES);
            XYZ_knob(f, nmod.k_scale.array(), nmod.knob_names[9], "scale");
                SetFlags(f, Knob::NO_HANDLES);
            Float_knob(f, &nmod.k_uniform_scale, nmod.knob_names[10], "scale");
                SetFlags(f, Knob::NO_HANDLES);
        }
    }
}


/*! Initialize any vars prior to rendering.
*/
/*virtual*/
void
VolumeMaterialOp::_validate(bool for_real)
{
    if (!for_real)
        return;

    // Clamp some controls to reasonable limits:
    m_density      = (float)std::max(0.0001, k_atmospheric_density);
    m_density_base = (float)std::max(0.0,    k_density_base);

    if (k_falloff_enabled)
    {
        // Init falloff bbox:
        m_falloff_bbox.setMin(std::min(k_falloff_bbox.min.x, k_falloff_bbox.max.x),
                              std::min(k_falloff_bbox.min.y, k_falloff_bbox.max.y),
                              std::min(k_falloff_bbox.min.z, k_falloff_bbox.max.z));
        m_falloff_bbox.setMax(std::max(k_falloff_bbox.min.x, k_falloff_bbox.max.x),
                              std::max(k_falloff_bbox.min.y, k_falloff_bbox.max.y),
                              std::max(k_falloff_bbox.min.z, k_falloff_bbox.max.z));
    }

    if (k_noise_enabled)
    {
        const Fsr::Mat4d noise_xform(k_noise_xform);
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

    //validateShader(for_real);
}


/*!
*/
bool
VolumeMaterialOp::getVolumeIntersections(zpr::RayShaderContext&          stx,
                                         Volume::VolumeIntersectionList& vol_intersections,
                                         double&                         vol_tmin,
                                         double&                         vol_tmax,
                                         double&                         vol_segment_min,
                                         double&                         vol_segment_max) const
{
#if DEBUG
    assert(stx.rtx); // shouldn't happen...
#endif
    vol_intersections.clear();
    vol_segment_min = std::numeric_limits<double>::infinity();
    vol_segment_max = 0.0;

    // Get list of light volume intersections:
    Traceable::SurfaceIntersectionList& I_vol_list = stx.thread_ctx->I_vol_list;
    I_vol_list.clear();

    vol_tmin =  std::numeric_limits<double>::infinity(); // Nearest volume intersection (may be behind camera!)
    vol_tmax = -std::numeric_limits<double>::infinity(); // Farthest volume intersection
    stx.rtx->lights_bvh.getIntersections(stx, I_vol_list, vol_tmin, vol_tmax);
    //std::cout << "VolumeMaterialOp::getVolumeIntersections(" << this << "): Rtx[" << stx.Rtx << "]";
    //std::cout << " I_vol_list=" << I_vol_list.size() << std::endl;

    // Volume intersections should always have two intersections, even
    // if they're behind the camera:
    const uint32_t nVolumes = (I_vol_list.size()%2 == 0) ?
                                (uint32_t)I_vol_list.size() / 2 :
                                    0;
    if (nVolumes == 0)
        return false;

    if (isnan(vol_tmin) || isnan(vol_tmax) || vol_tmin >= vol_tmax)
        return false; // invalid distances

    // Build the list of volume intersections:
    uint32_t I_index = 0;
    Volume::VolumeIntersection vI;
    for (uint32_t i=0; i < nVolumes; ++i)
    {
        // Make a single intersection for the back of the volume
        // and a volume intersection to define the entire range:
        const Traceable::SurfaceIntersection& I_enter = I_vol_list[I_index++];
        const Traceable::SurfaceIntersection& I_exit  = I_vol_list[I_index++];
        if (I_enter.object != I_exit.object)
            continue; // shouldn't happen...

        const double segment_size = (I_exit.t - I_enter.t);
        if (::fabs(segment_size) < std::numeric_limits<double>::epsilon())
            continue; // too small in depth, skip it

        // Find the min/max volume depths:
        vol_segment_min = std::min(segment_size, vol_segment_min);
        vol_segment_max = std::max(vol_segment_max, segment_size);

        // Build volume intersection:
        vI.tmin          = I_enter.t;
        vI.tmax          = I_exit.t;
        vI.object        = I_enter.object;
        vI.subpart_index = -1; // legacy, remove!
        vI.coverage      = 0.0f; // legacy, remove!

        vol_intersections.push_back(vI);
    }

    return true;
}


/*!
*/
/*virtual*/
bool
VolumeMaterialOp::volumeMarch(zpr::RayShaderContext&                stx,
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
    //std::cout << "VolumeMaterialOp::volumeMarch(" << this << "): Rtx[" << stx.Rtx << "]";
    //std::cout << " vol_intersections=" << vol_intersections.size() << std::endl;

    //-----------------------------------------------------------------------
    // Ray march params:
    //-----------------------------------------------------------------------

    // Clamp tmin to minimum starting offset from camera:
    tmin = std::max(0.0, tmin);

    double ray_step_incr = clamp(fabs(k_ray_step    ), 0.0001, 100.0);
    double ray_step_min  = clamp(fabs(k_ray_step_min), 0.0001, double(ray_step_incr));

    // Scale ray step down to make minimum number of steps:
    if ((depth_min / ray_step_incr) < double(k_ray_step_count_min))
    {
        ray_step_incr = depth_min / double(k_ray_step_count_min);
    }
    else if (k_ray_step_count_max > k_ray_step_count_min &&
             (depth_max / ray_step_incr) > double(k_ray_step_count_max))
    {
        ray_step_incr = depth_max / double(k_ray_step_count_max);
    }

    // Possibly change step size depending on preview mode:
    if (stx.rtx->k_preview_mode && k_preview_max_ray_steps > 0)
    {
        // Keep ray step from exceeding max count:
        ray_step_incr = std::max(ray_step_incr, (tmax - tmin) / double(k_preview_max_ray_steps));
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
    DD::Image::VertexContext vtx;

    Fsr::Vec3f illum;
    Fsr::Vec3f voxel_opacity;
    Fsr::Pixel lt_color(DD::Image::Mask_RGB);
    Fsr::Pixel shad(DD::Image::Mask_RGB);

    const DD::Image::ChannelSet rgba_channels(DD::Image::Mask_RGBA);
    Traceable::DeepIntersection vI(rgba_channels);
    vI.I.object = 0; // null object indicates a volume sample!
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
            if (Op::aborted())
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


        // The point in worldspace:
        const Fsr::Vec3d PW = stx.Rtx.getPositionAt(Zb);


        //---------------------------------------------------
        // Starting voxel density
        //
        double density = m_density;


#if 0
        //---------------------------------------------------
        // Vary density in turbulence field
        //
        if (k_noise_enabled)
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
        if (k_falloff_enabled)
        {
            // Get point location inside falloff bbox:
            double fx = std::min(std::max(PW.x, m_falloff_bbox.x()), m_falloff_bbox.r());
            fx = (fx - m_falloff_bbox.x()) / m_falloff_bbox.w();

            double fy = std::min(std::max(PW.y, m_falloff_bbox.y()), m_falloff_bbox.t());
            fy = (fy - m_falloff_bbox.y()) / m_falloff_bbox.h();

            double fz = std::min(std::max(PW.z, m_falloff_bbox.z()), m_falloff_bbox.f());
            fz = (fz - m_falloff_bbox.z()) / m_falloff_bbox.d();
            density *= clamp(k_falloff_lut.getValue(0, fx)); // X
            density *= clamp(k_falloff_lut.getValue(1, fy)); // Y
            density *= clamp(k_falloff_lut.getValue(2, fz)); // Z
        }
#endif

        //---------------------------------------------------
        // Add user density bias:
        density += k_density_base;


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

#if 0
            // TODO: Implement RayLightShader calls instead of just legacy lights

#else
            RenderPrimitive* rprim = static_cast<zpr::RenderPrimitive*>(vI.object);
#if DEBUG
            assert(rprim);
            assert(rprim->surface_ctx);
#endif
            const uint32_t scene_light_index = rprim->surface_ctx->obj_index;
#if DEBUG
            assert(scene_light_index < stx.master_lighting_scene->lights.size());
#endif

            DD::Image::LightContext* ltx = stx.master_lighting_scene->lights[scene_light_index];
#if DEBUG
            assert(ltx);
            assert(ltx->light());
#endif
            DD::Image::LightOp* light = ltx->light();

            //---------------------------------------------------
            // Light color/shadowing
            //---------------------------------------------------

            // Build light vectors:
            DD::Image::Vector3 L;
            float D;

            // Fake the surface normal:
            DD::Image::Vector3 N = ltx->p() - PW;
            N.normalize();

            // Get light color:
            light->get_L_vector(*ltx, PW, N, L, D);
            double Dlt = double(D);
            light->get_color(*ltx, PW, -L/*normal*/, L, D, lt_color);

            if (light->lightType() == DD::Image::LightOp::eSpotLight)
            {
                // Attenuate light by shadowing:
                lt_color *= light->get_shadowing(*ltx, vtx, PW, shad);
                //lt_color *= (1.0f - shad[Chan_Alpha]);
            }

            // Only consider the light if its contribution is non-zero:
            if (lt_color.color().notZero())
            {
                // Further attenuate the light by the density of the medium:
                if (k_light_absorption)
                {
                    //------------------------------------------------------------------------
                    // Calculate the light's aborption factor to this point (Beer-Lambert):
                    //
                    //   absorption calc is:
                    //     absorption = 1.0 - exp(-density * dPdz)
                    const float transmission = float(exp(-density * (Dlt - light->Near())));

                    lt_color *= transmission;
                }

                illum += lt_color.color()*float(k_volume_illum_factor);
            }
#endif

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


// end of zprender/VolumeMaterialOp.cpp

//
// Copyright 2020 DreamWorks Animation
//
