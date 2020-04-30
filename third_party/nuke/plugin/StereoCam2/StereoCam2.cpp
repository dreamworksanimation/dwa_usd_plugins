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

/// @file StereoCam2.cpp
///
/// @author Jonathan Egstad
///
/// @brief Fuser CameraRigOp plugin with stereo controls

#include <Fuser/CameraRigOp.h>

#include <DDImage/gl.h>

using namespace DD::Image;


//----------------------------------------------------------------------------------


/* Python script to copy the stereo camera to a new non-stereo, non-animated 'projector' camera.
*/
const char* const py_copy_to_projector =
    // Access an external Python file for the functions rather than hardcoding it all:
    "import nuke\n"
    "try:\n"
    "    import stereocam2_support\n"
    "    stereocam2_support.copyToProjector(nuke.thisNode())\n"
    ""
    "except (ImportError), e:\n"
    "    print 'Unable to import StereoCam support module'\n"
    "";


//----------------------------------------------------------------------------------


/*!
*/
class StereoCam2 : public Fsr::CameraRigOp
{
  public:
    //! Local baked down window & stereo parameters used primarily for OpenGL display
    struct StereoKnobParams : BakedKnobParams
    {
        double interaxial;
        double convergence_filmback_offset;
        double convergence_angle;
    };


  protected:
    std::pair<int, int> k_stereo_views;     //!< Views to use for stereo left and right
    //
    double k_interaxial;                    //!< Stereo interaxial value  - local x translation
    double k_convergence_filmback_offset;   //!< Stereo filmback offset - in millimeters (added to win_trans.x)
    double k_convergence_angle;             //!< Stereo convergence rotation - local Y-rotation angle in degrees
    //
    double k_stereo_near_plane_distance;    //!< Stereo near plane
    double k_stereo_near_plane_shift;       //!< Stereo near plane shift amount
    double k_stereo_far_plane_distance;     //!< Stereo far plane
    double k_stereo_far_plane_shift;        //!< Stereo far plane shift amount
    //
    bool   k_gl_stereo_planes;              //!< 

    std::vector<StereoKnobParams> m_knob_params;  //!< Baked-down knob values


  public:
    static const Description description;
    const char* Class() const { return description.name; }
    const char* node_help() const { return __DATE__ " " __TIME__ " "
        "Stereo camera with interaxial and convergence support.  See the tooltips on the knobs for additional help."
        "\n"
        SCENE_LOADER_HELP"\n"  // scene file loading
        "\n"
        SCENE_XFORM_HELP; // parenting
    }


    StereoCam2(::Node* node) :
        Fsr::CameraRigOp(node),
        k_stereo_views(1, 2)
    {
        m_knob_params.resize(2);
        //
#ifdef DWA_INTERNAL_BUILD
        k_world_to_meters             =  0.03; // legacy decifoot->meters default (should be 0.03048 to be more precise)
#else
        k_world_to_meters             =  1.0;
#endif
        //
        k_interaxial                  =  0.0;
        k_convergence_filmback_offset =  0.0;
        k_convergence_angle           =  0.0;
        //
        k_stereo_near_plane_distance  = -1.0;
        k_stereo_near_plane_shift     =  0.0;
        k_stereo_far_plane_distance   = -1.0;
        k_stereo_far_plane_shift      =  0.0;
        //
        k_gl_stereo_planes            = false;
    }


    /*virtual*/ const char* displayName() const { return "StereoCam"; }

    //! Fsr::CameraRigOp method. Return the identification name of the stereo rig.
    /*virtual*/ const char* rigName() const { return "StereoRig"; }


    //! DD::Image::Op method.
    /*virtual*/
    void knobs(Knob_Callback f)
    {
        Fsr::CameraRigOp::knobs(f);
    }


    //! Adds the OpenGL display option controls. Adds stereo display options.
    /*virtual*/
    void addDisplayOptionsKnobs(Knob_Callback f)
    {
        Fsr::CameraRigOp::addDisplayOptionsKnobs(f);
        Bool_knob(f, &k_gl_stereo_planes, "gl_stereo_planes", "show stereo planes");
    }


    //! Add rig-specific knobs. Add stereo controls.
    /*virtual*/
    void addRigKnobs(Knob_Callback f,
                     const char*   rig_name)
    {
        Divider(f, rig_name);
        ViewPair_knob(f, &k_stereo_views, "views", "views");
            Tooltip(f, "Selects which views are used in the camera.\n"
                       "The second view is drawn in OpenGL with dashed lines for easier identification.");
        Obsolete_knob(f, "gl_views", "knob views $value");
        Newline(f);
        Double_knob(f, &k_interaxial, IRange(-5,5), "interaxial");
            ClearFlags(f, Knob::SLIDER | Knob::RESIZABLE);
            Tooltip(f, "Local-space x-translation to separate the stereo cameras.\n"
                       "Also termed 'interocular' - the distance between your eyes.");
        Spacer(f, 5);
        Double_knob(f, &k_convergence_filmback_offset, IRange(0,89), "convergence_filmback_offset", "fb-offset");
            ClearFlags(f, Knob::STARTLINE | Knob::SLIDER | Knob::RESIZABLE);
            Tooltip(f, "Filmback horizontal offset of stereo camera, in horizontal-aperture units (millimeters).");
        Spacer(f, 5);
        Double_knob(f, &k_convergence_angle, "convergence_angle", "toe-in");
            ClearFlags(f, Knob::STARTLINE | Knob::SLIDER | Knob::RESIZABLE);
            SetFlags(f, Knob::ENDLINE);
            Tooltip(f, "Local-space Y-rotation angle (in degrees) of stereo camera to focus on a point.\n"
                       "Also termed 'toe-in' - the rotation of your eyes to converge on a point in space.");

        //Divider(f, "");
        Double_knob(f, &k_stereo_near_plane_distance, "stereo_near_plane_distance", "near plane dist");
            SetFlags(f, Knob::NO_MULTIVIEW);
            ClearFlags(f, Knob::SLIDER | Knob::RESIZABLE);
        Double_knob(f, &k_stereo_near_plane_shift, "stereo_near_plane_shift", "near plane shift");
            SetFlags(f, Knob::NO_MULTIVIEW);
            ClearFlags(f, Knob::SLIDER | Knob::RESIZABLE | Knob::STARTLINE);
        Double_knob(f, &k_stereo_far_plane_distance, "stereo_far_plane_distance", "far plane dist");
            SetFlags(f, Knob::NO_MULTIVIEW);
            ClearFlags(f, Knob::SLIDER | Knob::RESIZABLE);
        Double_knob(f, &k_stereo_far_plane_shift, "stereo_far_plane_shift", "far plane shift");
            SetFlags(f, Knob::NO_MULTIVIEW);
            ClearFlags(f, Knob::SLIDER | Knob::RESIZABLE | Knob::STARTLINE);

        PyScript_knob(f, py_copy_to_projector, "generate_projector", "  Generate Projector Cam  ");
            SetFlags(f, Knob::STARTLINE);
            Tooltip(f, "Copies the stereo camera at the current frame to a new non-stereo, "
                       "non-animated 'projector' camera.");
    }


    //------------------------------------------------------------------------------


    /*! DD::Image::CameraOp method.
        Adds projection knobs normally put on 'Projection' tab.
    */
    /*virtual*/
    void projection_knobs(Knob_Callback f)
    {
        Fsr::CameraRigOp::projection_knobs(f);
    }


    /*! DD::Image::CameraOp method.
        Adds 'lens' knobs normally appearing on 'Projection' tab underneath projection knobs.
        Adds dof controls.
    */
    /*virtual*/
    void lens_knobs(Knob_Callback f)
    {
        Fsr::CameraRigOp::lens_knobs(f); // adds world_scale
        Obsolete_knob(f, "dof_world_scale", "knob world_scale $value");

        Divider(f, "@b;DOF");

        // None of these controls do anything on this class so they only
        // affect dummy vars:
        bool dflt=false; Bool_knob(f, &dflt, "dof_enable", "dof enable");
            Tooltip(f, "Whether depth of field is enabled on this camera.");
        Newline(f);
        //
        double val=0.0;
        Double_knob(f, &val, IRange(0,20),   "dof_extra_focus_depth", "dof extra focus depth");
            SetFlags(f, Knob::NO_MULTIVIEW);
        Double_knob(f, &val, IRange(0,20),   "dof_extra_near_focus",  "dof extra near focus");
            SetFlags(f, Knob::NO_MULTIVIEW);
        Double_knob(f, &val, IRange(0,20),   "dof_extra_far_focus",   "dof extra far focus");
            SetFlags(f, Knob::NO_MULTIVIEW);
        Double_knob(f, &val, IRange(-89,89), "dof_tilt_shift_pan",    "dof tilt-shift pan");
            SetFlags(f, Knob::NO_MULTIVIEW);
        Double_knob(f, &val, IRange(-89,89), "dof_tilt_shift_tilt",   "dof tilt-shift tilt");
            SetFlags(f, Knob::NO_MULTIVIEW);
        val=50.0;
        Double_knob(f, &val, IRange(0, 100), "dof_max_radius",        "dof max radius");
            SetFlags(f, Knob::NO_MULTIVIEW);
    }


    /*! DD::Image::Op method.
    */
    /*virtual*/
    int knob_changed(Knob* k)
    {
        //std::cout << "StereoCam2::knob_changed(" << k->name() << ")" << std::endl;
        int call_again = 0;

        if (k == &Knob::showPanel)
        {
            // Enable the stereo knobs if there's enough views:
            if (outputContext().viewcount() > 2)
            {
                knob("gl_views")->show();
                knob("gl_show_all_rig_cameras")->show();
                knob("gl_stereo_planes")->show();
            }
            else
            {
                knob("gl_views")->hide();
                knob("gl_show_all_rig_cameras")->hide();
                knob("gl_stereo_planes")->hide();
            }
            call_again = 1;
        }

        if (Fsr::CameraRigOp::knob_changed(k))
            call_again = 1;

        return call_again;
    }


    /*! DD::Image::Op method.
    */
    /*virtual*/
    void _validate(bool for_real)
    {
        //std::cout << "  StereoCam2::_validate(" << this->node_name() << ") for_real=" << for_real << std::endl;

        // This will cause the CamParams list to be built:
        Fsr::CameraRigOp::_validate(for_real);

        // Get projection matrix withstereo calculation:
        projection_ = this->projection(LENS_PERSPECTIVE);
    }


    //------------------------------------------------------------------------------


    /*! Fsr::SceneLoader method.
        Enable/disable any knobs that get updated by SceneLoader.
    */
    /*virtual*/
    void enableSceneLoaderExtraKnobs(bool read_enabled)
    {
        CameraRigOp::enableSceneLoaderExtraKnobs(read_enabled);

        // Turn on local controls if not reading from file:
        const bool local_enabled = (!read_enabled);

        Knob* k;
        // Extra stereo knobs:
        k = knob("interaxial"                 ); if (k) k->enable(local_enabled);
        k = knob("convergence_filmback_offset"); if (k) k->enable(local_enabled);
        k = knob("convergence_angle"          ); if (k) k->enable(local_enabled);
    }


    /*! Fsr::SceneLoader method.
        Add in knob values for both views to make sure the interface updates
        whenever the values change for either view.
    */
    /*virtual*/
    void _appendRigValuesAt(const OutputContext& context,
                            Hash&                hash)
    {
        //std::cout << "StereoCam2::_appendRigKnobsAt()" << std::endl;

        // Ignore input context for gl_views and world_scale (non-splittable knobs):
        hash.append(k_stereo_views.first);
        hash.append(k_stereo_views.second);

        // Get baked-down knob values at views:
        OutputContext Lctx = context;
        OutputContext Rctx = context;

        const int Lview = k_stereo_views.first;
        const int Rview = k_stereo_views.second;
        if (Rview == 0 || Rview == Lview)
        {
            // Not in stereo mode:
            Lctx.view(Lview);
            Rctx.view(-1);
        }
        else
        {
            if (Lview >= 0)
                Lctx.view(Lview);
            if (Rview >= 0)
                Rctx.view(Rview);
        }

        assert(m_knob_params.size() == 2);

        // TODO: store the Knob pointers so this is faster? I think Knob pointers
        // from Op::firstOp() are better for these kinds of store actions.
        {
            Knob* k;
            k = Op::knob("interaxial");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].interaxial, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].interaxial, hash, Rctx);
            }
            k = Op::knob("convergence_filmback_offset");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].convergence_filmback_offset, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].convergence_filmback_offset, hash, Rctx);
            }
            k = Op::knob("convergence_angle");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].convergence_angle, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].convergence_angle, hash, Rctx);
            }
            k = Op::knob("win_translate");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].win_translate.x, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].win_translate.x, hash, Rctx);
            }
            k = Op::knob("win_scale");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].win_scale.x, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].win_scale.x, hash, Rctx);
            }
            k = Op::knob("winroll");
            if (k)
            {
                if (Lctx.view() >= 0) k->store(DoublePtr, &m_knob_params[0].win_roll, hash, Lctx);
                if (Rctx.view() >= 0) k->store(DoublePtr, &m_knob_params[1].win_roll, hash, Rctx);
            }
        }
        //std::cout << "  hash=" << hash << std::endl;
    }


    //------------------------------------------------------------------------------


    /*! Fsr::CameraRig method.
        Rebuild the CamParams list for the stereo views.

        A subclass needs to add at least one CamParam to m_cam_params otherwise
        an assert will be thrown.

    */
    /*virtual*/
    void _rebuildCamParamsAt(const OutputContext& context)
    {
        // Ignore input context for k_stereo_views and world_scale (non-splittable knobs):
        const int Lview = k_stereo_views.first;
        const int Rview = k_stereo_views.second;
        //std::cout << "Lview=" << Lview << ", Rview=" << Rview << std::endl;
        if (Rview == 0 || Rview == Lview)
        {
            // Not in stereo mode:
            //assert(m_knob_params.size() >= 1);

            // TODO: should we still support stereo weight offsets in this case?
            m_cam_params.resize(1);
            CamParams& cam = m_cam_params[0];
            cam.view     = Lview;
            cam.name     = OutputContext::viewname(Lview).c_str();
            cam.gl_color = 0xffffffff;
            cam.local_xform.setToIdentity();
            cam.aperture_xform.setToIdentity();
        }
        else
        {
            // Always have just two CamParams:
            assert(m_knob_params.size() == 2);

            m_cam_params.resize(2);
            CamParams& Lcam = m_cam_params[0];
            CamParams& Rcam = m_cam_params[1];

            if (Lview <= 0)
            {
                // Default left view to zero offsets:
                // TODO: support the stereo weight param here?
                Lcam.view     = Lview;
                Lcam.name     = OutputContext::viewname(Lview).c_str();
                Lcam.gl_color = 0xffffffff;
                Lcam.local_xform.setToIdentity();
                Lcam.aperture_xform.setToIdentity();
            }
            else
            {
                //std::cout << "Lview='" << OutputContext::viewname(Lview) << "'" << std::endl;
                const StereoKnobParams& param = m_knob_params[0];
                //
                Lcam.view     = Lview;
                Lcam.name     = OutputContext::viewname(Lview).c_str();
                Lcam.gl_color = 0xff3030ff; // Red - TODO: how to get this from Nuke?
                //
                Lcam.local_xform.setToTranslation(param.interaxial, 0.0, 0.0);
                Lcam.local_xform.rotateY(radians(param.convergence_angle));
                //
                // Add convergence offset into win_translate:
                Lcam.aperture_xform.setToTranslation((param.win_translate.x*2.0)*haperture_ +
                                                        param.convergence_filmback_offset,
                                                     (param.win_translate.y*2.0)*vaperture_,
                                                     0.0);
                Lcam.aperture_xform.rotateZ(-radians(param.win_roll));
                Lcam.aperture_xform.scale(param.win_scale.x,
                                          param.win_scale.y,
                                          1.0);
            }

            if (Rview <= 0)
            {
                // Default right view to zero offsets:
                // TODO: support the stereo weight param here?
                Rcam.view     = Rview;
                Rcam.name     = OutputContext::viewname(Rview).c_str();
                Rcam.gl_color = 0xffffffff;
                Rcam.local_xform.setToIdentity();
                Rcam.aperture_xform.setToIdentity();
            }
            else
            {
                //std::cout << "Rview='" << OutputContext::viewname(Rview) << "'" << std::endl;
                const StereoKnobParams& param = m_knob_params[1];
                //
                Rcam.view     = Rview;
                Rcam.name     = OutputContext::viewname(Rview).c_str();
                Rcam.gl_color = 0x30ff30ff; // Green - TODO: how to get this from Nuke?
                //
                Rcam.local_xform.setToTranslation(param.interaxial, 0.0, 0.0);
                Rcam.local_xform.rotateY(radians(param.convergence_angle));
                //
                // Add convergence offset into win_translate:
                Rcam.aperture_xform.setToTranslation((param.win_translate.x*2.0)*haperture_ +
                                                        param.convergence_filmback_offset,
                                                     (param.win_translate.y*2.0)*vaperture_,
                                                     0.0);
                Rcam.aperture_xform.rotateZ(-radians(param.win_roll));
                Rcam.aperture_xform.scale(param.win_scale.x,
                                          param.win_scale.y,
                                          1.0);
            }
        }
    }


    //------------------------------------------------------------------------------


    /*! Builds the local transform matrix, plus the stereo offset.
        Does not include lookat rotations. 
    */
    /*virtual*/
    Fsr::Mat4d getLocalTransformAt(const OutputContext& context) const
    {
        //std::cout << "StereoCam2('" << node_name() << "' " << this << ")::getLocalTransformAt()" << std::endl;
        Fsr::Mat4d m = SceneXform::getLocalTransformAt(context);

        // Apply interaxial to local matrix:
        m.translate(k_interaxial, 0.0, 0.0);
        m.rotateY(radians(k_convergence_angle));

        return m;
    }


    /*! Return camera projection matrix for a particular projection mode.
        It needs to be overridden if subclasses implement a different logic to calculate
        the projection matrix.
    */
    /*virtual*/
    Matrix4 projectionAt(const OutputContext& context)
    {
        // TODO: get this implemented! See need to evaluate the projection knobs
        // the context. We should put the same method on FuserCamera as we're
        // doing with transforms.
        return CameraRigOp::projectionAt(context);
    }


    /*! Returns a transformation to an output image due to the camera lens.
        It needs to be overridden if subclasses implement a different logic to calculate
        the projection matrix, in this case we're applying a stereo filmback shift
        in addition to the win_translate offsets.
    */
    /*virtual*/
    Matrix4 projection(int mode) const
    {
        Matrix4 projection;
        projection.makeIdentity();

        // We're only supporting perspective projections at the moment...:
        if (mode == LENS_PERSPECTIVE)
        {
            projection.rotateZ((float)radians(win_roll_));
            projection.scale(1.0f / win_scale_.x, 1.0f / win_scale_.y, 1.0f);

            float filmback_shift = 0.0f;
            // Apply stereo convergence offset:
            if (fabs(k_convergence_filmback_offset) > 0.0 && haperture_ > 0.0)
            {
                // Old formula:
                // Calculate filmback_shift from interaxial & convergence_distance:
                //   filmback_shift = (interaxial * (focal / convergence_distance)) / (haperture / 2.0)

                // New formula (USD+):
                // Scale convergence offset value from millimeters to aperture window scale:
                filmback_shift = float(k_convergence_filmback_offset / (haperture_ / 2.0));
            }
            projection.translate(-win_translate_.x + filmback_shift, -win_translate_.y, 0.0f);
        }
        // And finally the camera projection itself:
        Matrix4 p;
        p.projection(float(focal_length_ / haperture_), float(near_), float(far_), projection_mode_ == LENS_PERSPECTIVE);
        projection *= p;

        return projection;
    }

}; // class StereoCam2


static Op* build(Node* node) { return new StereoCam2(node); }
const Op::Description StereoCam2::description("StereoCam2", build);

// end of StereoCam2.cpp

//
// Copyright 2019 DreamWorks Animation
//
