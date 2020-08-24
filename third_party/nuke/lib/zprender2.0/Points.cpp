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

/// @file zprender/Points.cpp
///
/// @author Jonathan Egstad


#include "Points.h"
#include "RenderContext.h"
#include "ThreadContext.h"

#include <DDImage/Point.h>  // for Point type enumerations
#include <DDImage/Thread.h> // for Lock, sleepFor

static DD::Image::Lock my_lock;


namespace zpr {


/*!
*/
Points::Points(SurfaceContext*        stx,
               const Fsr::DoubleList& motion_times,
               const Fsr::Mat4dList&  motion_xforms,
               uint32_t               numPoints,
               const Fsr::Vec3f**     P_arrays,
               const Fsr::Vec3f**     N_arrays,
               const Fsr::Vec3f**     velocity_arrays,
               const float**          radii_arrays,
               const Fsr::Vec4f*      Cf_array) :
    RenderPrimitive(stx, motion_times),
    m_mode(POINT_POINTS),
    m_status(SURFACE_NOT_DICED),
    m_P_offset(0.0, 0.0, 0.0)
{
    // No go without points...
    if (motion_times.size() == 0 || numPoints == 0 || !P_arrays)
    {
        std::cerr << "Points::ctor(): warning, zero points, disabling." << std::endl;
        return;
    }

    //---------------------------------------------------------
    // Size the motion ptc samples list and fill them:
    m_motion_ptcs.resize(m_motion_times.size());

    //---------------------------------------------------------
    // Determine global offset from first motion sample only
    // by building the world-space bbox.
    //
    // Build world-space bbox (with l2w xform applied), and
    // take the global offset from its rounded-off center:
#if DEBUG
    assert(P_arrays[0] != NULL);
#endif
    Fsr::Box3d bbox(P_arrays[0], numPoints, motion_xforms[0]);
    const Fsr::Vec3d bbox_center = bbox.getCenter();
    m_P_offset.set(floor(bbox_center.x),
                   floor(bbox_center.y),
                   floor(bbox_center.z));

    //std::cout << "Points(): numPoints=" << numPoints << std::endl;
    //std::cout << "  bbox" << bbox << ", xform" << motion_xforms[0] << std::endl;
    //std::cout << "  m_P_offset[" << m_P_offset.x << " " << m_P_offset.y << " " << m_P_offset.z << "]" << std::endl;


    for (size_t i=0; i < m_motion_ptcs.size(); ++i)
    {
        // Copy point data with the global offset included in l2w xform:
        Sample& mesh = m_motion_ptcs[i];

        // Subtract offset from xform before baking it into points:
        Fsr::Mat4d xform = motion_xforms[i];
        xform.translate(-m_P_offset.x, -m_P_offset.y, -m_P_offset.z);

        // Bake the xform into the points during copy:
#if DEBUG
        assert(P_arrays[i] != NULL);
#endif
        mesh.P_list.resize(numPoints);
        xform.transform(mesh.P_list.data()/*dst*/, P_arrays[i]/*src*/, numPoints);

        // Build the motion sample local-space bbox:
        mesh.bbox.set(mesh.P_list.data(), numPoints);

        //---------------------------------------------------------
        // Copy animating velocity data:
        if (velocity_arrays)
        {
#if DEBUG
            assert(velocity_arrays[i] != NULL);
#endif
            mesh.vel_list.resize(numPoints);
            memcpy(mesh.vel_list.data(), velocity_arrays[i], numPoints*sizeof(Fsr::Vec3f));
        }

        //---------------------------------------------------------
        // Copy animating radius data:
        if (radii_arrays)
        {
#if DEBUG
            assert(radii_arrays[i] != NULL);
#endif
            mesh.r_list.resize(numPoints);
            memcpy(mesh.r_list.data(), radii_arrays[i], numPoints*sizeof(float));
        }

        //---------------------------------------------------------
        // Copy animating normal data:
        if (N_arrays)
        {
#if DEBUG
            assert(N_arrays[i] != NULL);
#endif
            mesh.N_list.resize(numPoints);
            memcpy(mesh.N_list.data(), N_arrays[i], numPoints*sizeof(Fsr::Vec3f));
        }
    }


    //---------------------------------------------------------
    // Copy non-animating Cf data:
    if (Cf_array)
    {
        m_Cf_list.resize(numPoints);
        memcpy(&m_Cf_list[0], Cf_array, numPoints*sizeof(Fsr::Vec4f));
    }
}


/*!
*/
Points::Points(SurfaceContext*           stx,
               const Fsr::DoubleList&    motion_times,
               const Points::SampleList& motion_ptcs,
               const Fsr::Vec4f*         Cf_array) :
    RenderPrimitive(stx, motion_times),
    m_motion_ptcs(motion_ptcs)
{
#if DEBUG
    assert(m_motion_ptcs.size() == m_motion_times.size());
#endif

    if (motion_ptcs.size() == 0)
        return;

    const uint32_t numPoints = (uint32_t)motion_ptcs[0].P_list.size();

    //---------------------------------------------------------
    // Copy non-animating Cf data:
    if (Cf_array)
    {
        m_Cf_list.resize(numPoints);
        memcpy(&m_Cf_list[0], Cf_array, numPoints*sizeof(Fsr::Vec4f));
    }
}


//------------------------------------------------------------------------------


/*!
*/
SpherePoints::SpherePoints(SurfaceContext*        stx,
                           const Fsr::DoubleList& motion_times,
                           const Fsr::Mat4dList&  motion_xforms,
                           uint32_t               numPoints,
                           const Fsr::Vec3f**     P_arrays,
                           const float**          radii_arrays,
                           const Fsr::Vec4f*      Cf_array) :
    Points(stx,
           motion_times,
           motion_xforms,
           numPoints,
           P_arrays,
           NULL/*N_arrays*/,
           NULL/*vel_arrays*/,
           radii_arrays,
           Cf_array)
{
    m_mode = SPHERE_POINTS;
}


/*!
*/
DiscPoints::DiscPoints(SurfaceContext*        stx,
                       const Fsr::DoubleList& motion_times,
                       const Fsr::Mat4dList&  motion_xforms,
                       uint32_t               numPoints,
                       const Fsr::Vec3f**     P_arrays,
                       const Fsr::Vec3f**     N_arrays,
                       const float**          radii_arrays,
                       const Fsr::Vec4f*      Cf_array) :
    Points(stx,
           motion_times,
           motion_xforms,
           numPoints,
           P_arrays,
           N_arrays,
           NULL/*vel_arrays*/,
           radii_arrays,
           Cf_array)
{
    m_mode = DISC_POINTS;
}


/*!
*/
CardPoints::CardPoints(SurfaceContext*        stx,
                       const Fsr::DoubleList& motion_times,
                       const Fsr::Mat4dList&  motion_xforms,
                       uint32_t               numPoints,
                       const Fsr::Vec3f**     P_arrays,
                       const Fsr::Vec3f**     N_arrays,
                       const float**          width_array,
                       const float*           aspect_array,
                       const Fsr::Vec4f*      Cf_array) :
    Points(stx,
           motion_times,
           motion_xforms,
           numPoints,
           P_arrays,
           N_arrays,
           NULL/*vel_arrays*/,
           width_array/*radii_arrays*/,
           Cf_array)
{
    m_mode = CARD_POINTS;

    //---------------------------------------------------------
    // Copy non-animating aspect data:
    if (aspect_array)
    {
        m_aspect_list.resize(numPoints);
        memcpy(&m_aspect_list[0], aspect_array, numPoints*sizeof(float));
    }
}


//------------------------------------------------------------------------------


/*! Build the bvhs, one for each motion step.
*/
void
Points::buildBvh(const RenderContext& rtx,
                 bool                 force)
{
    if (m_motion_bvhs.size() > 0 && !force)
        return;

    const uint32_t nMotionSamples = (uint32_t)m_motion_ptcs.size();
#if DEBUG
    assert(nMotionSamples > 0);
#endif

    //std::cout << "Points::buildBvh(" << this << ") nMotionSamples=" << nMotionSamples << std::endl;
    //std::cout << "  rtx.numShutterSamples()=" << rtx.numShutterSamples() << std::endl;

    const uint32_t nPoints = (uint32_t)m_motion_ptcs[0].P_list.size();
    std::vector<PointIndexRef> ref_list(nPoints);

    if (!rtx.isMotionBlurEnabled() || nMotionSamples < 2)
    {
        // No motion-blur:
        m_motion_bvhs.resize(1);
        for (uint32_t i=0; i < nPoints; ++i)
        {
            PointIndexRef& ref = ref_list[i];
            ref.data = i;
            ref.bbox = getPointBBoxLocal(i, 0/*motion_sample*/);
        }
        PointIndexBvh& bvh = m_motion_bvhs[0];
        bvh.setName("Points:PointIndexBvh");
        bvh.build(ref_list, rtx.bvh_max_objects);
        //std::cout << "  bvh" << bvh.bbox() << " depth=" << bvh.maxNodeDepth() << std::endl;
    }
    else
    {
        // Motion-blur:
        //
        // There's always at least one motion sample, and two per motion-step:
        m_motion_bvhs.resize(nMotionSamples-1);

        Fsr::Box3fList prev_bbox(nPoints);

        // Do first sample:
        for (uint32_t i=0; i < nPoints; ++i)
           prev_bbox[i] = getPointBBoxLocal(i, 0/*motion_sample*/);

        // Now the rest:
        for (uint32_t j=0; j < nMotionSamples-1; ++j)
        {
            for (uint32_t i=0; i < nPoints; ++i)
            {
                // Find interpolated primitive bbox at start & end of step:
                Fsr::Box3f bbox = prev_bbox[i];
                prev_bbox[i] = getPointBBoxLocal(i, j+1/*motion_sample*/);
                bbox.expand(prev_bbox[i], false/*test_empty*/);

                PointIndexRef& ref = ref_list[i];
                ref.data = i;
                ref.bbox = bbox;
            }
            PointIndexBvh& bvh = m_motion_bvhs[j];
            bvh.setName("Points:PointIndexBvh");
            bvh.build(ref_list, rtx.bvh_max_objects);
            //std::cout << "  " << j << ": mb bvh" << bvh.bbox() << " depth=" << bvh.maxNodeDepth() << std::endl;
        }
    }
}


/*! Build the Bvhs in a thread-safe loop.
*/
bool
Points::expand(const RenderContext& rtx)
{
    if (m_status == SURFACE_DICED)
        return true;

    // Creating the Bvhs must be done thread-safe to avoid another ray thread
    // from intersecting before they exist:
    uint32_t limit_count = 6000; // 0.01*6000 = 60seconds
    while (1)
    {
        if (m_status == SURFACE_DICED)
           return true;
        if (m_status == SURFACE_NOT_DICED)
        {
           my_lock.lock();
           if (m_status == SURFACE_NOT_DICED)
           {
              // Ok, this thread takes ownership of Bvh creation:
              m_status = SURFACE_DICING;
              my_lock.unlock();
              //std::cout << "Points::getIntersections(" << this << "): limit_count=" << limit_count << std::endl;
              buildBvh(rtx, false/*force*/);
              // Done, let the intersection test finish below:
              m_status = SURFACE_DICED;
              return true;

           } else {
              my_lock.unlock();
           }
        }

        // Pause briefly then try again:
        DD::Image::sleepFor(0.01/*seconds*/);
        if (--limit_count == 0)
        {
           std::cerr << "  Points::getIntersections() limit count reached!  This is likely the result of a bug." << std::endl;
           return false;
        }

    } // while loop

    //return false;
}


//--------------------------------------------------------------------------
// From RenderPrimitive:


/*! Get the AABB for this primitive at an optional shutter time. */
/*virtual*/
Fsr::Box3d
Points::getBBoxAtTime(double frame_time)
{
#if DEBUG
    assert(m_motion_ptcs.size() > 0);
#endif

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_ptcs.size());
#endif

    Fsr::Box3d bbox;
    if (motion_mode == MOTIONSTEP_START)
        bbox = Fsr::Box3d(m_motion_ptcs[motion_step].bbox);
    else if (motion_mode == MOTIONSTEP_END)
        bbox = Fsr::Box3d(m_motion_ptcs[motion_step+1].bbox);
    else
        bbox = Fsr::lerp(Fsr::Box3d(m_motion_ptcs[motion_step  ].bbox),
                         Fsr::Box3d(m_motion_ptcs[motion_step+1].bbox),
                         motion_step_t);

    bbox.shift(m_P_offset); // to world-space

    //std::cout << "    bbox" << bbox << std::endl;
    return bbox;
}


/*! Interpolate varying vertex attributes at SurfaceIntersection, no derivatives.
*/
/*virtual*/
void
Points::getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                           const DD::Image::ChannelSet& mask,
                                           Fsr::Pixel&                  v) const
{
#if DEBUG
    assert(I.part_index < (int32_t)numPoints());
#endif
#if 0
    const uint32_t point_index = I.part_index;

    v.PW() = I.PW;

    v.N() = I.N;

    v.UV().set(0.5,0.5,0,1);

    v.Cf() = (point_index < m_Cf.size())?m_Cf[point_index]:DD::Image::Vector4(1,1,1,1);
#endif
}


/*! Interpolate varying vertex attributes at SurfaceIntersection. This also calculates derivatives.
*/
/*virtual*/
void
Points::getAttributesAtSurfaceIntersection(const SurfaceIntersection&   I,
                                           const DD::Image::ChannelSet& mask,
                                           Fsr::Pixel&                  v,
                                           Fsr::Pixel&                  vdu,
                                           Fsr::Pixel&                  vdv) const
{
#if DEBUG
    assert(I.part_index < (int32_t)numPoints());
#endif
#if 0
    const uint32_t point_index = I.part_index;

    v.PW() = I.PW;

    vdu.PW().set(0,0,0);
    vdv.PW().set(0,0,0);

    v.N() = I.N;
    vdu.N().set(0,0,0);
    vdv.N().set(0,0,0);

    v.UV().set(0.5,0.5,0,1);
    vdu.UV().set(0,0,0,0);
    vdv.UV().set(0,0,0,0);

    v.Cf() = (point_index < m_Cf.size())?m_Cf[point_index]:DD::Image::Vector4(1,1,1,1);
    vdu.Cf().set(0,0,0,0);
    vdv.Cf().set(0,0,0,0);
#endif
}


//--------------------------------------------------------------------------


/*! Intersect an individual point.
*/
/*virtual*/
Fsr::RayIntersectionType
Points::pointIntersect(uint32_t             point,
                       int                  motion_mode,
                       uint32_t             motion_step,
                       float                motion_step_t,
                       RayShaderContext&    stx,
                       SurfaceIntersection& I)
{
    // TODO: what should we do here?
    return Fsr::RAY_INTERSECT_NONE;
}


/*! Intersect an individual sphere.
*/
/*virtual*/
Fsr::RayIntersectionType
SpherePoints::pointIntersect(uint32_t             point,
                             int                  motion_mode,
                             uint32_t             motion_step,
                             float                motion_step_t,
                             RayShaderContext&    stx,
                             SurfaceIntersection& I)
{
#if DEBUG
    assert(motion_step < (uint32_t)m_motion_ptcs.size());
#endif

    Fsr::Vec3f P;
    float radius;
    double tmin, tmax;
    if (motion_mode == MOTIONSTEP_START)
    {
        const Sample& ptc = m_motion_ptcs[motion_step];
        P      = ptc.P_list[point];
        radius = ptc.r_list[point];
    }
    else if (motion_mode == MOTIONSTEP_END)
    {
        const Sample& ptc = m_motion_ptcs[motion_step+1];
        P      = ptc.P_list[point];
        radius = ptc.r_list[point];
    }
    else
    {
        const Sample& ptc0 = m_motion_ptcs[motion_step  ];
        const Sample& ptc1 = m_motion_ptcs[motion_step+1];
        P      = Fsr::lerp(ptc0.P_list[point], ptc1.P_list[point], motion_step_t);
        radius = (point < ptc0.r_list.size()) ?
                    Fsr::lerp(ptc0.r_list[point], ptc1.r_list[point], motion_step_t) :
                        MIN_RADIUS;
    }

    Fsr::RayIntersectionType Itype = Fsr::intersectSphere(P, radius, stx.Rtx, tmin, tmax);
    if (Itype > Fsr::RAY_INTERSECT_NONE)
    {
        I.t  = tmin;
        I.PW = I.PWg = stx.Rtx.getPositionAt(tmin);
        I.N  = (I.PW - P.asVec3d());
        I.N.fastNormalize();
        I.Ng = I.Ni = I.N;
    }

    return Itype;
}


/*! Intersect an individual disc.
*/
/*virtual*/
Fsr::RayIntersectionType
DiscPoints::pointIntersect(uint32_t             point,
                           int                  motion_mode,
                           uint32_t             motion_step,
                           float                motion_step_t,
                           RayShaderContext&    stx,
                           SurfaceIntersection& I)
{
#if DEBUG
    assert(motion_step < (uint32_t)m_motion_ptcs.size());
#endif

#if 0
    const Fsr::Vec3f P = (!stx.mb_enabled || m_motion_times.size() == 0) ?
                            getPoint(point) :
                            getPointMB(point, motion_step, motion_step_t);
    const Fsr::Vec3f N = (m_N.size() > 0) ? m_N[point] : -stx.Rtx.dir().asVec3f();
    const float radius = (point < m_r.size()) ? m_r[point] : MIN_RADIUS;

    double tmin;
    if (Fsr::intersectDisc(P, N, radius, stx.Rtx, tmin))
    {
        I.t  = tmin;
        I.PW = I.PWg = stx.Rtx.getPositionAt(tmin);
        I.N  = (I.PW - P.asVec3d());
        I.N.fastNormalize();
        I.Ng = I.Ni = I.N;
        return Fsr::RAY_INTERSECT_POINT;
    }
#endif
    return Fsr::RAY_INTERSECT_NONE;
}


/*! Intersect an individual card.
*/
/*virtual*/
Fsr::RayIntersectionType
CardPoints::pointIntersect(uint32_t             point,
                           int                  motion_mode,
                           uint32_t             motion_step,
                           float                motion_step_t,
                           RayShaderContext&    stx,
                           SurfaceIntersection& I)
{
#if DEBUG
    assert(motion_step < (uint32_t)m_motion_ptcs.size());
#endif

#if 0
    const Fsr::Vec3f P = (!stx.mb_enabled || m_motion_times.size() == 0) ?
                            getPoint(point) :
                            getPointMB(point, motion_step, motion_step_t);
    const Fsr::Vec3f N = (m_N.size() > 0) ? m_N[point] : -stx.Rtx.dir().asVec3f();
    const float radius = (point < m_r.size()) ? m_r[point] : MIN_RADIUS;

#if 0
    double tmin;
    if (Fsr::intersectCard(P, N, radius, stx.Rtx, tmin))
    {
        I.t  = tmin;
        I.PW = I.PWg = stx.Rtx.getPositionAt(tmin);
        I.N  = (I.PW - P.asVec3d());
        I.N.fastNormalize();
        I.Ng = I.Ni = I.N;
        return Fsr::RAY_INTERSECT_POINT;
    }
#endif
#endif
    return Fsr::RAY_INTERSECT_NONE;
}


//--------------------------------------------------------------------------
// From Traceable:


/*virtual*/
void
Points::getIntersections(RayShaderContext&        stx,
                         SurfaceIntersectionList& I_list,
                         double&                  tmin,
                         double&                  tmax)
{
    //if (stx.x == 630 && stx.y == 732)
    //   std::cout << "Points::getIntersections(" << this << ") frame_time=" << stx.frame_time << std::endl;

    // Make sure Bvhs are created:
    if (!expand(*stx.rtx))
        return; // error in expand

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_bvhs.size());
#endif

#if 1
    // Intersect against the correct motion_step bvh and get the list of Bvh
    // leaf nodes intersected:
    const PointIndexBvh& bvh = m_motion_bvhs[motion_step];

    std::vector<const BvhNode*>& bvh_leafs = stx.thread_ctx->bvh_leafs;
    if (!bvh.getIntersectedLeafs(stx.Rtx, bvh_leafs))
        return; // no intersected leafs!

    // Test each leaf node's face list:
    const uint32_t nNodes = (uint32_t)bvh_leafs.size();
    for (uint32_t j=0; j < nNodes; ++j)
    {
#if DEBUG
        assert(bvh_leafs[j]);
#endif
        const BvhNode& node = *bvh_leafs[j];
        //std::cout << "  " << j << ": itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
#if DEBUG
        assert(node.itemStart() < bvh.numItems());
        assert((node.itemStart() + node.numItems()-1) < bvh.numItems());
#endif

        uint32_t p = node.itemStart();
        const uint32_t lastPoint = p + node.numItems();
        if (stx.mb_enabled)
        {
            //std::cout << "    mb_enabled, test nPoints=" << (lastPoint-p) << std::endl;
            SurfaceIntersection I;
            for (; p < lastPoint; ++p)
            {
                const uint32_t findex = bvh.getItem(p);
                if (pointIntersect(findex, motion_mode, motion_step, motion_step_t, stx, I) != Fsr::RAY_INTERSECT_NONE)
                {
                    //std::cout << "      " << findex << " hit!" << std::endl;
                    addIntersectionToList(I, I_list);
                    if (I.t < tmin)
                        tmin = I.t;
                    if (I.t > tmax)
                        tmax = I.t;
                }
            }

        }
        else
        {
            // No motion-blur:
            //std::cout << "    no motion-blur, test nPoints=" << (lastPoint-p) << std::endl;
            SurfaceIntersection I;
            for (; p < lastPoint; ++p)
            {
                const uint32_t findex = bvh.getItem(p);
                if (pointIntersect(findex, 0, 0, 0.0f, stx, I) != Fsr::RAY_INTERSECT_NONE)
                {
                    //std::cout << "      " << findex << " hit!" << std::endl;
                    addIntersectionToList(I, I_list);
                    if (I.t < tmin)
                        tmin = I.t;
                    if (I.t > tmax)
                        tmax = I.t;
                }
            }
        }
    }
#else
    // Get list of Bvh leaf nodes intersected:
    std::vector<const PointIndexBvhLeaf*> nodes;
    nodes.reserve(100);
    if (!m_motion_bvhs[motion_step].getIntersectedLeafs(stx.Rtx, nodes))
        return;

    // Some intersected nodes, intersect each node's Disc list:
    const uint32_t nNodes = (uint32_t)nodes.size();
    for (uint32_t j=0; j < nNodes; ++j)
    {
        const PointIndexBvhLeaf* node = nodes[j];
        assert(node);
        const std::vector<uint32_t>& points_list = node->data();
        //
        if (stx.mb_enabled)
        {
            //std::cout << "Points::getIntersections(): mb_enabled" << std::endl;

            const uint32_t nPoints = (uint32_t)points_list.size();
            SurfaceIntersection I;
            for (uint32_t i=0; i < nPoints; ++i)
            {
                const uint32_t point_index = points_list[i];
#if 1
                if (pointIntersect(index, motion_mode, motion_step, motion_step_t, stx, I) > Fsr::RAY_INTERSECT_NONE &&
                        I.t > std::numeric_limits<double>::epsilon())
#else
                Fsr::Vec3d P = P_array[index] + VEL_array[index];
                //
                const float radius = (r_array)?r_array[index]:MIN_RADIUS;
                if (pointIntersect(index, P, radius, stx, I) > Fsr::RAY_INTERSECT_NONE &&
                        I.t > std::numeric_limits<double>::epsilon())
#endif
                {
                    // Have an intersection, add it to the list:
                    I.object        = static_cast<RenderPrimitive*>(this);
                    I.object_type   = ZprPointsPrim;
                    I.object_ref    = 1;    // one hit
                    I.part_index    = point_index;
                    I.subpart_index = -1;

                    I.st.set(0.0f, 0.0f);
                    I.Rxst.set(0.0f, 0.0f);
                    I.Ryst.set(0.0f, 0.0f);

                    addIntersectionToList(I, I_list);
                    if (I.t < tmin)
                        tmin = I.t;
                    if (I.t > tmax)
                        tmax = I.t;
                }
            }
        }
        else
        {
            // No motion-blur:
            const uint32_t nPoints = (uint32_t)points_list.size();
            SurfaceIntersection I;
            for (uint32_t i=0; i < nPoints; ++i)
            {
                const uint32_t point_index = points_list[i];
#if 1
                if (pointIntersect(index, motion_mode, motion_step, motion_step_t, stx, I) > Fsr::RAY_INTERSECT_NONE &&
                        I.t > std::numeric_limits<double>::epsilon())
#else
                const float radius = (r_array)?r_array[index]:MIN_RADIUS;
                if (pointIntersect(index, P_array[index], radius, stx, I) > Fsr::RAY_INTERSECT_NONE &&
                        I.t > std::numeric_limits<double>::epsilon())
#endif
                {
                    // Have an intersection, add it to the list:
                    I.object        = static_cast<RenderPrimitive*>(this);
                    I.object_type   = ZprPointsPrim;
                    I.object_ref    = 1;    // one hit
                    I.part_index    = point_index;
                    I.subpart_index = -1;

                    I.st.set(0.0f, 0.0f);
                    I.Rxst.set(0.0f, 0.0f);
                    I.Ryst.set(0.0f, 0.0f);

                    addIntersectionToList(I, I_list);
                    if (I.t < tmin)
                       tmin = I.t;
                    if (I.t > tmax)
                       tmax = I.t;
                }
            }
        }
    }
#endif
}


/*virtual*/
bool
Points::intersect(RayShaderContext& stx)
{
    SurfaceIntersection I(std::numeric_limits<double>::infinity());
    return (getFirstIntersection(stx, I) != Fsr::RAY_INTERSECT_NONE);
}


/*virtual*/
Fsr::RayIntersectionType
Points::getFirstIntersection(RayShaderContext&    stx,
                             SurfaceIntersection& I)
{
    //if (stx.rtx->k_debug == RenderContext::DEBUG_LOW)
    //   std::cout << "Points::getFirstIntersection(" << this << ") frame_time=" << stx.frame_time << std::endl;

    // Make sure Bvhs are created:
    if (!expand(*stx.rtx))
        return Fsr::RAY_INTERSECT_NONE; // error in expand

    // Find the motion-step this shutter position falls inside:
    uint32_t  motion_step;
    float     motion_step_t;
    const int motion_mode = getMotionStep(m_motion_times, stx.frame_time, motion_step, motion_step_t);
    //std::cout << "  frame_time=" << stx.frame_time << ", motion_step=" << motion_step << ", motion_step_t=" << motion_step_t << std::endl;
#if DEBUG
    assert(motion_step < m_motion_bvhs.size());
#endif

#if 1
    // Intersect against the correct motion_step bvh and get the list of Bvh
    // leaf nodes intersected:
    const PointIndexBvh& bvh = m_motion_bvhs[motion_step];

    std::vector<const BvhNode*>& bvh_leafs = stx.thread_ctx->bvh_leafs;
    if (!bvh.getIntersectedLeafs(stx.Rtx, bvh_leafs))
        return Fsr::RAY_INTERSECT_NONE; // no intersected leafs!

    SurfaceIntersection If;

    I.t = std::numeric_limits<double>::infinity();

    // Test each leaf node's face list:
    const uint32_t nNodes = (uint32_t)bvh_leafs.size();
    for (uint32_t j=0; j < nNodes; ++j)
    {
#if DEBUG
        assert(bvh_leafs[j]);
#endif
        const BvhNode& node = *bvh_leafs[j];
        //std::cout << "  " << j << ": itemStart=" << node.itemStart() << ", numItems=" << node.numItems() << std::endl;
#if DEBUG
        assert(node.itemStart() < bvh.numItems());
        assert((node.itemStart() + node.numItems()-1) < bvh.numItems());
#endif

        // Find the nearest point intersection:
        uint32_t p = node.itemStart();
        const uint32_t lastPoint = p + node.numItems();
        for (; p < lastPoint; ++p)
        {
            const uint32_t pindex = bvh.getItem(p);
            if (pointIntersect(pindex, motion_mode, motion_step, motion_step_t, stx, If) != Fsr::RAY_INTERSECT_NONE &&
                    If.t < I.t)
            {
                I = If;
                //if (stx.rtx->k_debug == RenderContext::DEBUG_LOW)
                //   std::cout << "    node=" << j << " point=" << p << " I.t=" << I.t << std::endl;
            }
        }
    }
    if (I.t < std::numeric_limits<double>::infinity())
        return Fsr::RAY_INTERSECT_POINT;

    return Fsr::RAY_INTERSECT_NONE;
#else
    // Get list of Bvh leaf nodes intersected:
    std::vector<const PointIndexBvhLeaf*> nodes;
    nodes.reserve(100);
    if (!m_motion_bvhs[motion_step].getIntersectedLeafs(stx.Rtx, nodes))
        return Fsr::RAY_INTERSECT_NONE;

    // Some intersected nodes, intersect each node's Disc list:
    const uint32_t nNodes = (uint32_t)nodes.size();
    for (uint32_t j=0; j < nNodes; ++j)
    {
        const PointIndexBvhLeaf* node = nodes[j];
        assert(node);
        const std::vector<uint32_t>& points_list = node->data();

        { // TODO: add back motion-blur test
            // No motion-blur:
            const uint32_t nPoints = (uint32_t)points_list.size();
            for (uint32_t i=0; i < nPoints; ++i)
            {
                const uint32_t point_index = points_list[i];
#if 1
                if (pointIntersect(index, motion_mode, motion_step, motion_step_t, stx, I) > Fsr::RAY_INTERSECT_NONE
                        && I.t > std::numeric_limits<double>::epsilon())
#else
                const float radius = (r_array)?r_array[index]:MIN_RADIUS;
                if (pointIntersect(index, P_array[index], radius, stx, I) > Fsr::RAY_INTERSECT_NONE
                        && I.t > std::numeric_limits<double>::epsilon())
#endif
                {
                    I.object        = static_cast<RenderPrimitive*>(this);
                    I.object_type   = ZprPointsPrim;
                    I.object_ref    = 1;    // one hit
                    I.part_index    = point_index;
                    I.subpart_index = -1;

                    I.st.set(0.0f, 0.0f);
                    I.Rxst.set(0.0f, 0.0f);
                    I.Ryst.set(0.0f, 0.0f);
                    return Fsr::RAY_INTERSECT_POINT;
                }
            }
        }

    }
#endif

    return Fsr::RAY_INTERSECT_NONE;
}


} // namespace zpr

// end of zprender/Points.cpp

//
// Copyright 2020 DreamWorks Animation
//
