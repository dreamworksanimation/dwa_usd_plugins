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

/// @file zprender/RenderContext.h
///
/// @author Jonathan Egstad


#ifndef zprender_Context_h
#define zprender_Context_h

#include "Bvh.h"
#include "RayCamera.h"
#include "RayShaderContext.h"
#include "RenderPrimitive.h"
#include "Texture2dSampler.h"

#include <Fuser/Box3.h>
#include <Fuser/Mat4.h>

#include <DDImage/Matrix4.h>
#include <DDImage/Shutter.h>
#include <DDImage/Format.h>
#include <DDImage/Filter.h>
#include <DDImage/LightContext.h>

#include <map>
#include <deque>

#include <sys/time.h>


namespace zpr {


//-------------------------------------------------------------------------


/*! Pre-defined AOV output types.  These are used to quickly
    access/copy aov values without having to test name strings.
*/
enum AOVType
{
    AOV_ATTRIBUTE,          //!< Vertex attribute
    //
    AOV_Z,                  //!< Ray depth (distance) from camera
    AOV_Zl,                 //!< Linearly projected depth from camera
    //
    AOV_PW,                 //!< Displaced shading point in world-space
    AOV_dPWdx,              //!< PW x-derivative
    AOV_dPWdy,              //!< PW y-derivative
    AOV_PL,                 //!< Shading point in local-space
    AOV_PWg,                //!< Geometric surface point (no displacement)
    //
    AOV_st,                 //!< Primitive's barycentric coordinates at R intersection
    AOV_dstdx,              //!< Primitive's barycentric coordinates at Rx intersection
    AOV_dstdy,              //!< Primitive's barycentric coordinates at Ry intersection
    //
    AOV_N,                  //!< Shading normal (interpolated & bumped vertex normal)
    AOV_Nf,                 //!< Face-forward shading normal
    AOV_Ng,                 //!< Geometric surface normal
    AOV_Ngf,                //!< Face-forward geometric normal
    AOV_Ns,                 //!< Interpolated surface normal (same as N but with no bump)
    AOV_dNdx,               //!< N x-derivative
    AOV_dNdy,               //!< N y-derivative
    //
    AOV_UV,                 //!< Surface texture coordinate
    AOV_dUVdx,              //!< UV x-derivative
    AOV_dUVdy,              //!< UV y-derivative
    //
    AOV_time,
    AOV_dtdx,
    AOV_dtdy,
    //
    AOV_surf_id,
    //
    AOV_Cf,                 //!< Vertex color
    AOV_dCfdx,              //!< Cf x-derivative
    AOV_dCfdy,              //!< Cf y-derivative
    //
    AOV_V,                  //!< View vector
    AOV_VdotN,              //!< View vector dot N - facing ratio
    AOV_VdotNg,             //!< View vector dot Ng - facing ratio
    AOV_VdotNf,             //!< View vector dot Nf - facing ratio
    //
    AOV_LAST_TYPE
};


class AOVBuiltIn;

/*! 
*/
class ZPR_EXPORT AOVLayer
{
  public:
    //!
    enum
    { 
        AOV_MERGE_PREMULT_UNDER,
        AOV_MERGE_UNDER,
        AOV_MERGE_PLUS,
        AOV_MERGE_MIN,
        AOV_MERGE_MID,
        AOV_MERGE_MAX
    };
    static const char* aov_merge_modes[];

    //!
    enum
    {
        AOV_UNPREMULT_BY_COVERAGE,
        AOV_UNPREMULT_BY_ALPHA,
        AOV_NO_UNPREMULT
    };
    //static const char* aov_unpremult_modes[];

    typedef void (*Handler)(const RayShaderContext& stx, const AOVLayer& aov, Fsr::Pixel& out);


  public:
    std::string                     name;           //!< Layer name - 'P', 'N', 'Ns', etc.
    AOVType                         type;           //!< AOV type
    std::vector<DD::Image::Channel> channel;        //!< List of output Pixel channels
    DD::Image::ChannelSet           mask;           //!< Channel mask
    bool                            enabled;        //!< AOV enabled
    char                            unpremult;      //!< Unpremult by coverage mode
    int                             merge_mode;     //!< Merging mode
    AOVLayer::Handler               handler;        //!< Handler routine


  public:
    AOVLayer() :
        type(AOV_ATTRIBUTE),
        mask(DD::Image::Mask_None),
        enabled(true),
        unpremult(AOV_UNPREMULT_BY_COVERAGE),
        merge_mode(AOV_MERGE_UNDER)
    {
        //
    }

    //!
    bool build(AOVBuiltIn*         built_ins,
               const char*         _name,
               int                 nChans,
               DD::Image::Channel* chans);

};


/*! 
*/
struct AOVBuiltIn
{
    const char*       tokens;
    AOVLayer::Handler handler;

    //!
    void set(const char* t, AOVLayer::Handler h) { tokens = t; handler = h; }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


class RenderContext;
class ObjectContext;
class GeoInfoContext;
class LightVolumeContext;
class SurfaceHandler;
class ThreadContext;
class RayMaterial;
class Texture2dSampler;
class InputBinding;
class Scene;


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Works on gcc 4.1.2+.
*/
class AtomicCount32
{
    int32_t val;
  public:
    AtomicCount32(int32_t v=0) : val(v) {}

    inline operator int32_t() { return val; }

    inline int32_t operator ++ (int unused) { return __sync_fetch_and_add(&val, 1); } //   postfix++
    inline int32_t operator -- (int unused) { return __sync_fetch_and_sub(&val, 1); } //   postfix++
    inline int32_t operator ++ ()           { return __sync_add_and_fetch(&val, 1); } // ++prefix
    inline int32_t operator -- ()           { return __sync_sub_and_fetch(&val, 1); } // ++prefix
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! TODO: deprecate? Don't think we need this whole thing anymore.
*/
struct ShutterSceneRef
{
    int op_input_index;                 //!< Op input number (split input number)
    //
    zpr::Scene*          scene;         //!< Pointer to geometry Scene (owned by this struct!)
    DD::Image::CameraOp* camera;        //!< Pointer to view camera (should be same as one in scene pointer)
    DD::Image::CameraOp* hero_camera;   //!< Pointer to hero view camera, or null if not set
    //
    uint32_t shutter_sample;            //!< Index to shutter_times entry
    //
    double   frame0;                    //!< Absolute frame 0 (render context output frame)
    double   frame;                     //!< Absolute frame number for this time sample
    double   frame0_offset;             //!< Offset from frame0 for this time sample


    //! Used by the sort routine.
    bool operator < (const ShutterSceneRef& b) const { return (frame < b.frame); }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Supported source primitive types.
    This list is used to index an array of SurfaceHandler structures
    or for fast comparison since testing just the Class() pointer fails
    since zprender is statically linked and you're forced to string compare...
*/
enum SourcePrimitiveType
{
    UNRECOGNIZED_PRIM=-1,
    //
    // Surface Primitives:
    FN_POLYSOUP_PRIM=0,         //!< DD::Image::Triangle or DD::Image::Polygon
    FN_MESH_PRIM,               //!< DD::Image::Mesh
    FN_POLYMESH_PRIM,           //!< DD::Image::PolyMesh
    FN_POINT_PRIM,              //!< DD::Image::Point
    FN_PARTICLE_SPRITE_PRIM,    //!< DD::Image::ParticleSprite
    //
    FUSER_NODEPRIM,             //!< Fuser::NodePrim
    FUSER_MESHPRIM,             //!< Fuser::MeshPrim
    FUSER_POINTPRIM,            //!< Fuser::PointPrim (TODO: NYI)
    //
    // Light primitives:
    LIGHTSPHERE_PRIM,           //!< PointLight  primitive (zpr::SphereVolume)
    LIGHTCONE_PRIM,             //!< SpotLight   primitive (zpr::ConeVolume)
    LIGHTCYLINDER_PRIM,         //!< DirectLight primitive (zpr::CylinderVolume) (TODO: NYI)
    LIGHTCARD_PRIM,             //!< AreaLight   primitive (zpr::Card) (TODO: NYI)
    //
    LAST_SOURCE_PRIM_TYPE
};

//-------------------------------------------------------------------------

//! Scene part masks.
enum
{
    GeometryFlag    = 0x00000001,
    MaterialsFlag   = 0x00000002,
    LightsFlag      = 0x00000004,
    CameraFlag      = 0x00000008,
    //
    AllPartsFlag  = GeometryFlag | MaterialsFlag | LightsFlag | CameraFlag
};


/*! Surface dicing status.
*/
enum
{
    SURFACE_NOT_DICED    =  0,  //!<
    SURFACE_DICING       =  1,  //!< 
    SURFACE_DICED        =  2   //!< 
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

typedef std::map<DD::Image::Iop*, uint32_t>          TextureSampleIndexMap;
typedef std::map<DD::Image::Iop*, DD::Image::Box>    TextureBBoxMap;
typedef std::map<DD::Image::Iop*, Texture2dSampler*> Texture2dSamplerMap;

/*!
*/
struct TextureSamplerContext
{
    uint32_t          index;    //!< Index in global texture list
    Texture2dSampler* sampler;  //!<

    TextureSamplerContext() : index(0), sampler(NULL) {}
};



/*! There's one of these for each object in the primary Scene, even if
    it doesn't render.
*/
struct ZPR_EXPORT ObjectMaterialRef
{
    RayMaterial*          raymaterial;              //!<
    //
    DD::Image::Iop*       material;                 //!<
    DD::Image::Iop*       displacement_material;    //!< Legacy displacement shader to use if RayMaterial not available
    //
    DD::Image::Hash       hash;                     //!< 
    DD::Image::ChannelSet texture_channels;         //!< All the channels from all texture samplers
    DD::Image::ChannelSet output_channels;          //!< All the channels this material outputs
    float                 displacement_max;         //!< Max displacement bounds

    std::vector<InputBinding*> texture_bindings;    //!< Set of all texture bindings in Material

    //!
    ObjectMaterialRef() :
        raymaterial(NULL),
        material(NULL),
        displacement_max(0.0f)
    {
        //
    }

    //!
    ObjectMaterialRef(const ObjectMaterialRef& b) :
        raymaterial(b.raymaterial),
        material(b.material),
        hash(b.hash),
        texture_channels(b.texture_channels),
        output_channels(b.output_channels),
        displacement_max(b.displacement_max),
        texture_bindings(b.texture_bindings)
    {
        //
    }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Shaders are assigned from filled-in ObjectMaterialRefs.
*/
class ZPR_EXPORT SurfaceContext
{
  public:
    SurfaceHandler*     handler;                        //!< Source primitive converter
    ObjectContext*      parent_object_ctx;              //!< ObjectContext
    int32_t             status;                         //!< Surface state indicator (unexpanded, etc)
    //
    int32_t             obj_index;                      //!< Copied from parent ObjectSample index
    int32_t             prim_index;                     //!< Primitive index in GeoInfo if not polysoup or light
    Fsr::Uint32List     polysoup_prims;                 //!< List of tris/polys prim indices in GeoInfo

    // Ray shading:
    RayMaterial*        raymaterial;                    //!< RayMaterial to call shaders on
    //
    bool                displacement_enabled;           //!< Is displacement enabled?
    int                 displacement_subdivision_level; //!< What recursion level to subdivide to
    Fsr::Vec3f          displacement_bounds;            //!< Displacement bounds scaled by local-to-world matrix

    // Legacy shading:
    DD::Image::Iop*     material;                       //!< Legacy shader to use if RayMaterial not available
    DD::Image::Iop*     displacement_material;          //!< Legacy displacement shader to use if RayMaterial not available


  public:
    /*!
    */
    SurfaceContext(ObjectContext* _parent_object_ctx) :
        handler(NULL),
        parent_object_ctx(_parent_object_ctx),
        status(SURFACE_NOT_DICED),
        //
        obj_index(-1),
        prim_index(-1),
        //
        raymaterial(NULL),
        //
        displacement_enabled(false),
        displacement_subdivision_level(0),
        displacement_bounds(0.0f, 0.0f, 0.0f),
        //
        material(NULL),
        displacement_material(NULL)
    {
        //
    }


    //! Returns the parent_object_context cast to a GeoInfoContext.
    GeoInfoContext*     getGeoInfoContext();
    //! Returns the parent_object_context cast to a LightVolumeContext.
    LightVolumeContext* getLightVolumeContext();

    //! Return the zpr::Scene* from the parent ObjectContext.
    zpr::Scene* getScene(uint32_t sample=0);
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Abstract base class for geometric objects that the renderer is handling.

    There's one of these created for each geometric object in the GeometryList
    input to the renderer. Note that 'geometric object' can be lights, cameras
    as well as geometry, so this class is further specialized with the
    GeoInfoContext subclass for geometry surfaces, and LightVolumeContext
    for atmospheric volumetric lights.

    
*/
class ZPR_EXPORT ObjectContext
{
  public:
    /*! There's one of these for each Object motion sample.
    */
    struct ObjectSample
    {
        zpr::Scene* scene;      //!< Scene containing this object.
        uint32_t    index;      //!< Object index in Scene lists (GeoInfo index or LightContext index)


        //
        ObjectSample() : scene(NULL), index(0) {}
        //!
        ObjectSample(zpr::Scene* _scene,
                     uint32_t    _index) : scene(_scene), index(_index) {}
    };

    int32_t             status;         //!< Object state indicator (unexpanded, etc)
    Fsr::Box3d          bbox;           //!< Entire bbox including all motion samples
    DD::Image::Hash     hash;           //!< All the geo hashes together
    struct timeval      last_access;    //!< The last time the object was probed

    // Animating:
    std::vector<ObjectSample>     motion_objects;   //!< List of Object motion sample Scene references
    Fsr::DoubleList               motion_times;     //!< Frame time for each active Object motion sample

    // Non-animating:
    std::vector<SurfaceContext*>  surface_list; //!< List of surfaces generated by this object
    std::vector<RenderPrimitive*> prim_list;    //!< List of primitives generated from surfaces


  public:
    //!
    ObjectContext();
    //!
    ObjectContext(zpr::Scene* scene,
                  uint32_t    index);

    //!
    virtual ~ObjectContext();

    //!
    virtual GeoInfoContext*     asGeoObject()   { return NULL; }
    virtual LightVolumeContext* asLightVolume() { return NULL; }

    //!
    bool     isMotionBlurred()  const { return (numMotionSamples() > 1); }
    uint32_t numMotionSamples() const { return (uint32_t)motion_times.size(); }


  public:
    //!
    SurfaceContext* addSurface()
    {
        // If the additional primitive is greater than the memory reserve, double the reserve amount:
        if (surface_list.size() && surface_list.size() >= surface_list.capacity())
            surface_list.reserve(surface_list.capacity() * 2);
        surface_list.push_back(new SurfaceContext(this));
        return surface_list[surface_list.size()-1];
    }

    //!
    uint32_t numSurfaces() const { return (uint32_t)surface_list.size(); }

    //
    SurfaceContext& getSurface(int i) { return *surface_list[i]; }


  public:
    uint32_t numPrims() const { return (uint32_t)prim_list.size(); }


    //! Add a primitive p to the list.  Returns the index of the added prim.
    uint32_t addPrim(RenderPrimitive* prim)
    {
        if (prim_list.size() && prim_list.size() >= prim_list.capacity())
            prim_list.reserve(prim_list.capacity() * 2);
        prim_list.push_back(prim);
        return (uint32_t)prim_list.size()-1;
    }


    //!
    void clearSurfacesAndRenderPrims()
    {
        const size_t nSurfs = surface_list.size();
        for (size_t i=0; i < nSurfs; ++i)
            delete surface_list[i];
        surface_list.clear();

        const size_t nPrims = prim_list.size();
        for (size_t i=0; i < nPrims; ++i)
            delete prim_list[i];
        prim_list.clear();

        gettimeofday(&last_access, 0);
    }


  private:
    //! Disabled copy constructor.
    ObjectContext(const ObjectContext&);

};


/*! A GeoInfo abstracted to be simply an 'object' which contains a series
    of 'surfaces' (primitives.)
*/
class ZPR_EXPORT GeoInfoContext : public ObjectContext
{
  public:
    /*! Motion sample for a GeoInfo-based object.
    */
    struct Sample
    {
        DD::Image::GeoInfo*     info;               //!< GeoInfo pointer to separate time-sampled GeometryLists.
        DD::Image::MatrixArray* transforms;         //!< TODO: deprecate?
        //
        Fsr::Mat4d              l2w;                //!< TODO: replace transforms with this?
        Fsr::Mat4d              w2l;
        bool                    xform_is_identity;  //!< 
    };

    /*! Motion step indices */
    std::vector<Sample> motion_geoinfos;        //!< GeoInfo motion samples
    std::set<uint32_t>  enabled_lights;         //!< List of enabled lights illuminating object


  public:
    //!
    GeoInfoContext() : ObjectContext() {}
    //!
    GeoInfoContext(zpr::Scene* scene,
                   uint32_t    obj_index) :
        ObjectContext(scene, obj_index)
    {
        //
    }

    //!
    /*virtual*/ GeoInfoContext* asGeoObject() { return this; }

    //!
    const Sample& getGeoInfoSample(uint32_t sample) const { return motion_geoinfos[sample]; }
    //!
    Sample&       addGeoInfoSample(zpr::Scene* scene,
                                   uint32_t    obj_index);

};


/*!
*/
class ZPR_EXPORT LightVolumeContext : public ObjectContext
{
  public:
    /*! Motion sample for a LightVolume object.
    */
    struct Sample
    {
        DD::Image::LightContext* lt_ctx;            //!< TODO: do we really need the whole LightContext?
        //
        Fsr::Mat4d               l2w;               //!< TODO: replace LightContext transforms with this?
        Fsr::Mat4d               w2l;               //!<
        bool                     xform_is_identity; //!<
    };

    /*! Motion step indices */
    std::vector<Sample> motion_lights;                  //!< LightVolume motion samples


  public:
    //!
    LightVolumeContext() : ObjectContext() {}
    //!
    LightVolumeContext(zpr::Scene* scene,
                       uint32_t    lt_index) :
        ObjectContext(scene, lt_index)
    {
        //
    }

    /*virtual*/ LightVolumeContext* asLightVolume() { return this; }

    //!
    const Sample& getLightVolumeSample(uint32_t sample) const { return motion_lights[sample]; }
    //!
    Sample&       addLightVolumeSample(zpr::Scene* scene,
                                       uint32_t    lt_index);

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


typedef Bvh<ObjectContext*>       ObjectContextBvh;
typedef BvhObjRef<ObjectContext*> ObjectContextRef;


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

// Bvh specializations
// Declarations of specialized methods must be outside the class definition:

template<>
/*virtual*/
void
ObjectContextBvh::getIntersections(RayShaderContext&        stx,
                                   SurfaceIntersectionList& I_list,
                                   double&                  tmin, 
                                   double&                  tmax);

template<>
/*virtual*/
Fsr::RayIntersectionType
ObjectContextBvh::getFirstIntersection(RayShaderContext&    stx,
                                       SurfaceIntersection& I);

template<>
/*virtual*/
int
ObjectContextBvh::intersectLevel(RayShaderContext& stx,
                                 int               level,
                                 int               max_level);

template<>
/*virtual*/
void
ObjectContextBvh::getIntersectionsWithUVs(RayShaderContext&          stx,
                                          const Fsr::Vec2f&          uv0,
                                          const Fsr::Vec2f&          uv1,
                                          UVSegmentIntersectionList& I_list);


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Map stereoRole strings to Nuke view names.

    TODO: this logic is hardcoded to map stereoRole string to DWA-specific
    LFT', 'RGT' strings. This info should come from the Stage defaults
    instead? Or can we extract this from Nuke itself?
*/
struct ZPR_EXPORT DWAStereoViews
{
  private:
    std::map<std::string, std::string> views_map;

  public:
    DWAStereoViews();

    static const std::map<std::string, std::string>& viewsMap();
};


//-------------------------------------------------------------------------


/*! Context structure passed as target data to Fuser execute methods.
*/
struct GenerateRenderPrimsContext
{
    static const char* name; // "GenerateRenderPrims"

    RenderContext*  rtx;
    ObjectContext*  otx;
    SurfaceContext* stx;

    std::vector<DD::Image::PrimitiveContext>* ptx_list;


    //! Ctor sets everything to invalid values.
    GenerateRenderPrimsContext() : rtx(NULL), otx(NULL), stx(NULL), ptx_list(NULL) {}
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! Render task context.

    This is shared between threads!!
*/
class ZPR_EXPORT RenderContext
{
  public:
    // Camera projection types:
    enum
    {
        CAMERA_PROJECTION_PERSPECTIVE,
        CAMERA_PROJECTION_ORTHOGRAPHIC,
        CAMERA_PROJECTION_UV,
        CAMERA_PROJECTION_SPHERICAL,
        CAMERA_PROJECTION_CYLINDRICAL
    };

    //! Stereo camera types.
    enum
    {
        CAMERA_SEPARATE,
        CAMERA_COMBINED
    };
    static const char* camera_modes[];

    //! Shading types.
    enum
    {
        SHADING_OFF,
        SHADING_CONSTANT,
        SHADING_SMOOTH
    };
    static const char* shading_interpolation_names[];

    //! Pixel sampling modes.
    enum
    {
        SAMPLING_1x1, SAMPLING_2x2, SAMPLING_3x3,
        SAMPLING_4x4, SAMPLING_5x5, SAMPLING_8x8,
        SAMPLING_12x12, SAMPLING_16x16,
        SAMPLING_32x32, SAMPLING_64x64,
        SAMPLING_CUSTOM
    };
    static const char* sampling_modes[];

    //! Output bbox modes.
    enum
    {
        BBOX_SCENE_SIZE,
        BBOX_CLAMP_TO_FORMAT
    };
    static const char* output_bbox_modes[];

    //! Surface side modes.
    enum
    {
        SIDES_BOTH,
        SIDES_FRONT,
        SIDES_BACK
    };
    static const char* sides_modes[];

    //! Debug levels.
    enum
    {
        DEBUG_NONE,
        DEBUG_LOW,
        DEBUG_MEDIUM,
        DEBUG_HIGH
    };
    static const char* debug_names[];

    //! Diagnostic modes.
    enum
    {
        DIAG_OFF,
        DIAG_TIME,
        DIAG_BOUNDS,
        DIAG_BVH_LEAF,
        DIAG_INTERSECTIONS,
        DIAG_VOLUMES,
        DIAG_PATCHES,
        DIAG_RENDER_TIME
    };
    static const char* diagnostics_modes[];


  public:
    DD::Image::Op* m_parent;                    //!< Op that owns this context

    //-------------------------------------------------------
    // Values set by knobs on Renderer Op:
    //-------------------------------------------------------
    int    k_camera_mode;                       //!< Stereo camera mode
    int    k_projection_mode;                   //!< Which render projection to use
    DD::Image::ShutterControls k_shutter;       //!< Shutter params
    double k_shutter_bias;                      //!< Weights the shutter samples towards shutter close with a power function
    //
    int    k_shading_interpolation;             //!< Surface shading mode
    int    k_sides_mode;                        //!< Surface side shading mode
    //
    bool   k_preview_mode;                      //!< Use preview (lower quality) settings
    int    k_debug;                             //!< Debug info level
    int    k_show_diagnostics;                  //!< Display diagnostic info rather than image
    int    k_diagnostics_sample;                //!< Which sample index to grab
    //
    int           k_hero_view;                  //!< Which view drives specular calc
    std::set<int> k_views;                      //!< Views set by view knob
    bool          k_copy_specular;              //!< Copy the specular angle from the hero view
    //
    int    k_pixel_filter;
    float  k_pixel_filter_size[2];
    int    k_spatial_jitter_threshold;
    int    k_output_bbox_mode;                  //!< How to handle the output scene bbox
    //
    bool   k_atmosphere_alpha_blending;         //!<
    bool   k_transparency_enabled;              //!< Enable alpha blending
    float  k_alpha_threshold;                   //!< Below this alpha value a surface no longer affect Z
    //
    bool   k_dof_enabled;
    float  k_dof_max_radius;

    //-------------------------------------------------------
    // Values derived or configured by Renderer Op:
    //-------------------------------------------------------
    std::vector<zpr::Scene*>     input_scenes;      //!< List of motion-sample scenes in Op input-order
    std::vector<ShutterSceneRef> shutter_scenerefs; //!< List of time-sorted ShutterSceneRefs where index 0 is earliest time.
    //
    double                   render_frame;      //!< Current output frame number (from outputContext())
    double                   frame0;            //!< Input frame0 number (possibly offset from render_frame)
    //
    int                      render_view;       //!< Current view (from outputContext())
    std::string              render_view_name;  //!< Current view name
    std::vector<int>         render_views;      //!< Views to render (stripped of crap views)
    //
    std::vector<RayCamera*>  ray_cameras;       //!< List of RayCameras from current view, one per shutter sample
    std::vector<RayCamera*>  hero_ray_cameras;  //!< List of RayCameras from hero view, one per shutter sample
    //
    int                      render_projection; //!< Render projection to use
    //
    Fsr::Box3d               render_bbox;       //!< BBox of entire render scene
    Fsr::Box2i               render_region;     //!< Render screen xyrt area to shoot rays in
    const DD::Image::Format* render_format;     //!< Render format
    DD::Image::ChannelSet    render_channels;   //!< Render channel set - what channels will be filled in
    //
    DD::Image::ChannelSet    texture_channels;  //!< All the output channels from all texture samplers
    DD::Image::ChannelSet    material_channels; //!< All the output materials channels
    DD::Image::ChannelSet    shadow_channels;   //!< Legacy shadow renderer channels
    //
    DD::Image::Filter        pixel_filter;      //!< Output pixel filter - TODO: this should be per-channel!
    //
    DD::Image::ChannelSet    color_channels;    //!< Set of channels that are in color layers (rgba, mask)
    DD::Image::ChannelSet    vector_channels;   //!< Set of channels that are in 'vector' layers (N, P, motion, depth.Z)
    DD::Image::ChannelSet    under_channels;    //!< Channels to merge using standard UNDER
    DD::Image::ChannelSet    aov_channels;      //!< Channels to handle using specific AOV merge method
    //
    std::vector<AOVLayer>           aov_outputs;                //!< List of AOV layers to output - i.e. 'P', 'N', 'Ng', etc.
    std::map<std::string, uint32_t> aov_map;                    //!< Map of aov names to AOVLayer
    AOVBuiltIn                      aov_handler[AOV_LAST_TYPE]; //!< List of assigned AOV handlers

    //-------------------------------------------------------
    // Shutter time sample info:
    //-------------------------------------------------------
    uint32_t        num_shutter_steps;          //<! Set by renderer. Number of shutter steps where 0steps = 1sample, 1step = 2samples, etc.

    // Derived from num_shutter_steps:
    Fsr::DoubleList shutter_times;              //!< List of time-sorted shutter times where index 0 is earliest time sample.
    uint32_t        frame0_shutter_sample;      //!< Which shutter sample represents frame0
    float           shutter_open_offset;        //!< Shutter open relative to frame0
    float           shutter_close_offset;       //!< Shutter close relative to frame0
    float           shutter_length;             //!< Direction of shutter and its length (ex -0.5, 0.0, 0.5)


    //-------------------------------------------------------
    // Render state:
    //-------------------------------------------------------
    DD::Image::Hash camera_hash;                //!< Hash value of current camera
    DD::Image::Hash geometry_hash;              //!< Hash value of all geometric params
    DD::Image::Hash material_hash;              //!< Hash value of all materials
    DD::Image::Hash lighting_hash;              //!< Hash value of all lights
    DD::Image::Hash hash;                       //!< All of the other hashes together
    bool            objects_initialized;        //!< If false call generate_render_primitives()


    //-------------------------------------------------------
    // Ray stuff:
    //-------------------------------------------------------
    int ray_max_depth;                          //!< Max ray recursion depth
    int ray_diffuse_max_depth;
    int ray_glossy_max_depth;
    int ray_reflection_max_depth;
    int ray_refraction_max_depth;

    int ray_single_scatter_samples;             //!< Camera ray samples
    int ray_diffuse_samples;
    int ray_glossy_samples;
    int ray_refraction_samples;


    //-------------------------------------------------------
    // Rendering stuff:
    //-------------------------------------------------------
    int render_version;                         //!< This is incremented every render pass

    Fsr::Mat4d global_xform;                    //!< Transform the entire world by this matrix
    Fsr::Vec3d global_offset;                   //!< Same as global_xform but single-precision for offset

    //! List of threads working on this render context.
    std::vector<ThreadContext*>   thread_list;  //!< List of thread contexts
    std::map<pthread_t, uint32_t> thread_map;   //!< Thread map - pthread ID -> thread context index

    double Near, Far;                           //!< Near/Far camera clipping panes

    //! List of object and light contexts, one per object or light.
    std::vector<GeoInfoContext*>     object_context;
    std::vector<LightVolumeContext*> light_context;

    //! Per-object material references
    std::vector<ObjectMaterialRef>     object_materials;    //!< Per-object material references
    //std::vector<TextureSamplerContext> texture_samplers;    //!< Flattened list of all textures for ColorMapKnob 

    std::vector<RayMaterial*>          ray_materials;       //!< List of allocated RayMaterials
    Texture2dSamplerMap                texture_sampler_map; //!< Texture ID to texture sampler index map
    //
    //TextureSampleIndexMap              texture_sampler_map; //!< Iop*->index map into texture_samplers map
    TextureBBoxMap                     texture_bbox_map;    //!< Iop*->Box2i map for texture request()

    //! ID -> object/light index map
    std::map<uint64_t, uint32_t>     object_map;
    std::map<uint64_t, uint32_t>     light_map;

    //! Scene level BVHs:
    ObjectContextBvh objects_bvh;
    ObjectContextBvh lights_bvh;
    bool objects_bvh_initialized;               //!< 
    bool lights_bvh_initialized;                //!< 

    int bvh_max_depth;                          //!< TODO: deprecate
    int bvh_max_objects;                        //!< TODO: deprecate


    //-----------------------------------------------------------------
    // Lighting:
    //-----------------------------------------------------------------

    bool              direct_lighting_enabled;      //!< Enable direct scene lighting (shadowed)
    bool              indirect_lighting_enabled;    //!< Enable indirect scene lighting (bounce)
    bool              atmospheric_lighting_enabled; //!< Enable atmospherics lighting

    LightShaderList   master_light_shaders;         //!< Ray-tracing light shaders, for each light in the Scene
    LightShaderLists  per_object_light_shaders;     //!< Per-object list of light shaders



    //-------------------------------------------------------
    // Surface handler array:
    //-------------------------------------------------------
    SurfaceHandler* surface_handler[LAST_SOURCE_PRIM_TYPE];


  public:
    //!
    RenderContext(DD::Image::Op* parent=NULL);
    //!
    ~RenderContext();


    //========================================================
    //! Delete context allocations.
    void destroyAllocations(bool force=false);
    void destroyObjectBVHs(bool force=false);
    void destroyLightBVHs(bool force=false);
    void destroyTextureSamplers();
    void destroyRayMaterials();

    //========================================================
    // Shutter samples:

    //! Relative shutter-open/close offsets from frame_time.
    double   getShutterOpenTime()   const { return shutter_times[0]; }
    double   getShutterCloseTime()  const { return shutter_times[shutter_times.size()-1]; }
    //! Shutter (close - open).
    double   getShutterLength() const { return (getShutterCloseTime() - getShutterOpenTime()); }


    //! Number of shutter samples.  This is numShutterSteps()+1, where  0 steps=1 sample, 2 steps=3 sample, etc.
    uint32_t numShutterSamples() const { return (num_shutter_steps + 1); }

    //! Number of shutter steps. This is numShutterSamples()-1, where 0 indicates no shutter blur.
    uint32_t numShutterSteps() const { return num_shutter_steps; }

    //! Is global motion-blur enabled?
    bool     isMotionBlurEnabled() const { return (numShutterSamples() > 1 && getShutterLength() >= std::numeric_limits<double>::epsilon()); }


    //========================================================


    //!
    void validateObjects(zpr::Scene* scene,
                         bool        for_real=true);

    //! Return false if object disabled.
    bool validateObject(int32_t                    obj,
                        DD::Image::Hash&           obj_geometry_hash,
                        DD::Image::Box3&           obj_bbox,
                        DD::Image::Box&            obj_screen_bbox,
                        RayMaterial*&              ray_material);

    //!
    void doTextureRequests(const DD::Image::ChannelSet& request_channels,
                           int                          request_count);

    //! Return false if object disabled.
    bool requestObject(int32_t                      obj,
                       const DD::Image::ChannelSet& request_channels,
                       int                          request_count,
                       DD::Image::Iop*&             obj_material,
                       DD::Image::Box&              obj_material_bbox);


    //! Update the map of all InputBinding->TextureSamplers.
    void requestTextureSamplers();


    //========================================================


    //! Find an AOVLayer by name.
    const AOVLayer* findAOVLayer(const char* name) const;


    //========================================================


    //! Expand each object into surface context.  This is a thread-safe call. Returns false on user-abort.
    bool expandObject(ObjectContext* otx);

    //! Create SurfaceContext references from an ObjectContext. Returns false on user-abort.
    bool generateSurfaceContextsForObject(ObjectContext* ctx);

    //! Create RenderPrimitives from an ObjectContext's surfaces. Returns false on user-abort.
    bool generateRenderPrimitivesForObject(ObjectContext* ctx);


    //-----------------------------------------------------------------
    // Lighting:
    //-----------------------------------------------------------------

    //! If a light can illuminate atmosphere then it becomes a physical object of a certain size.
    SourcePrimitiveType getVolumeLightTypeAndBbox(const DD::Image::LightOp* light,
                                                  Fsr::Box3d&               bbox) const;

    //!
    void buildLightShaders();

    //!
    void updateLightingScene(const zpr::Scene* ref_scene,
                             zpr::Scene*       lighting_scene) const;

    //!
    void updateLightingScenes(const zpr::Scene* ref_scene,
                              ThreadContext&    ttx) const;

    //! Interpolate LightContext vectors to a shutter time.
    void updateLightingSceneVectorsTo(uint32_t    shutter_step,
                                      float       shutter_step_t,
                                      zpr::Scene* light_scene) const;

    //========================================================
    // RayShaderContext routines:

    //! Start a shader context list owned by thread_index and returning a reference to the first one.
    RayShaderContext& startShaderContext(uint32_t thread_index);
    //! Add a shader context to the end of the list, copying the current context, and returning a reference to it.
    RayShaderContext& pushShaderContext(uint32_t                thread_index,
                                        const RayShaderContext* current);
    //! Add a shader context to the end of the list, copying the current context, and returning a reference to it.
    RayShaderContext& pushShaderContext(uint32_t                         thread_index,
                                        const RayShaderContext*          current,
                                        const Fsr::RayContext&           R,
                                        const Fsr::RayContext::TypeMask& ray_type,
                                        const Fsr::RayDifferentials*     Rdif=NULL);
    //! Remove a RayShaderContext from the end of the list.
    int               popShaderContext(uint32_t thread_index);


  private:
    //! Disabled copy contructor.
    RenderContext(const RenderContext&);
    //! Disabled copy operator.
    RenderContext& operator=(const RenderContext&);

};



} // namespace zpr

#endif

// end of zprender/RenderContext.h

//
// Copyright 2020 DreamWorks Animation
//
