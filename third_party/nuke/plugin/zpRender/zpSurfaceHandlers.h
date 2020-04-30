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

/// @file zpSurfaceHandlers.h
///
/// @author Jonathan Egstad


#ifndef zprender_SurfaceHandlers_h
#define zprender_SurfaceHandlers_h

#include <zprender/RenderContext.h>
#include <zprender/SurfaceHandler.h>
#include <zprender/Points.h>
#include <zprender/Mesh.h>

#include <Fuser/NukeGeoInterface.h> // for getObjectString(), getObjectInt(), getAttribData()
#include <Fuser/NodePrimitive.h>
#include <Fuser/MeshPrimitive.h>
#include <Fuser/ExecuteTargetContexts.h> // for MeshTessellateContext


#include <DDImage/Polygon.h>
#include <DDImage/Mesh.h>
#include <DDImage/ParticlesSprite.h>
#include <DDImage/PolyMesh.h>


// Uncomment to turn on info prints
#define DEBUG_HANDLERS 1


namespace zpr {


//-------------------------------------------------------------------------


/*! Looks at assigned subd-related GeoInfo attributes.
    Returns true if subdivision is enabled and fill subd_args with the
    appropriate values for Fuser subdividers to use.

    GeoInfo attributes. Not all readers set these!
        int        subd_current_level =    Fsr::getObjectInt(info, "subd:current_level", 0);
        int        subd_render_level  =    Fsr::getObjectInt(info, "subd:render_level",  0);
        std:string subd_tessellator   = Fsr::getObjectString(info, "subd:tessellator",   "OpenSubdiv");
        std:string subd_scheme        = Fsr::getObjectString(info, "subd:scheme",        "catmullclark");
        bool       subd_snap_to_limit =   Fsr::getObjectBool(info, "subd:snap_to_limit", false);
        bool       subd_force_enable  =   Fsr::getObjectBool(info, "subd:force_enable",  false);

    Fuser::OpenSubdiv supports:
        subd_args.getInt(   "subd:current_level", 0);
        subd_args.getInt(   "subd:target_level",  0);
        subd_args.getString("subd:scheme",        "catmullclark");
        subd_args.getBool(  "subd:snap_to_limit", false);

    Fuser::SimpleSubdiv supports:
        subd_args.getInt("subd:current_level", 0);
        subd_args.getInt("subd:target_level",  0);

    TODO: these arg constants *really* need to be moved to a common definition in Fuser!
*/
inline bool
getSubdArgs(const DD::Image::GeoInfo& info,
            Fsr::ArgSet&              subd_args)
{
    const int  subd_current_level =  Fsr::getObjectInt(info, "subd:current_level", 0/*default*/);
    const int  subd_render_level  =  Fsr::getObjectInt(info, "subd:render_level",  0/*default*/);
    const bool subd_force_meshes  = Fsr::getObjectBool(info, "subd:force_enable",  false/*default*/);
    if (subd_render_level > subd_current_level ||
        subd_force_meshes)
    {
        // Copy info attributes to subd args:
        subd_args.setString("subd_tessellator",   Fsr::getObjectString(info, "subd:tessellator", "OpenSubdiv"/*default*/));
        subd_args.setInt(   "subd:current_level", subd_current_level);
        subd_args.setInt(   "subd:target_level",  subd_render_level);
        subd_args.setString("subd:scheme",        Fsr::getObjectString(info, "subd:scheme", "catmullclark"/*default*/));
        subd_args.setBool(  "subd:snap_to_limit", Fsr::getObjectBool(info, "subd:snap_to_limit", false));

        return true;
    }

    return false; // no subdivision required
}


/*! Handles the standard DDImage Primitives like Mesh and PolyMesh
    as well as special-cased support for Fuser::MeshPrimitive.
*/
inline void
convertDDImagePrimitiveToMesh(RenderContext&     rtx,
                              SurfaceContext&    stx,
                              GeoInfoContext&    gptx,
                              bool               enable_subdivision,
                              const Fsr::ArgSet& subd_args)
{
#if DEBUG
    assert(stx.prim_index >= 0); // shouldn't happen...
    assert(gptx.getGeoInfoSample(0).info); // shouldn't happen...
#endif
    const DD::Image::GeoInfo& info0 = *gptx.getGeoInfoSample(0).info;

    //std::cout << "----------------------------------------------" << std::endl;
    //std::cout << "convertDDImagePrimitiveToMesh('" << Fsr::getObjectString(info0, "name") << "')" << std::endl;

    const uint32_t nGeoMotionSamples = gptx.numMotionSamples();
    assert(nGeoMotionSamples > 0);

    const bool haveNs  = (info0.N_ref  && info0.N_ref->attribute );
    const bool haveUVs = (info0.UV_ref && info0.UV_ref->attribute);
    const bool haveCfs = (info0.Cf_ref && info0.Cf_ref->attribute);

    Fsr::Mat4dList                 motion_xforms(nGeoMotionSamples);
    Fsr::Uint32List                vertsPerFace;
    Fsr::Uint32List                vertIndicesList;
    //
    std::vector<Fsr::Vec3fList>    P_lists(nGeoMotionSamples); //< only used if subdividing
    std::vector<Fsr::Vec3fList>    N_lists(nGeoMotionSamples);
    Fsr::Vec2fList                 UV_list;
    Fsr::Vec4fList                 Cf_list;
    //
    std::vector<const Fsr::Vec3f*> P_arrays(nGeoMotionSamples, NULL);
    std::vector<const Fsr::Vec3f*> N_arrays(nGeoMotionSamples, NULL);

    uint32_t nPoints      = 0;
    uint32_t nPrimFaces   = 0;
    uint32_t nPrimVerts   = 0;
    uint32_t nOutVerts    = 0;
    uint32_t maxFaceVerts = 0;

    DD::Image::VArray tmpV;
    uint32_t attrib_indices[DD::Image::Group_Last];
    memset(attrib_indices, 0, DD::Image::Group_Last*sizeof(uint32_t));
    attrib_indices[DD::Image::Group_Object    ] = stx.obj_index;
    attrib_indices[DD::Image::Group_Primitives] = stx.prim_index;


    DD::Image::PrimitiveContext ptx;
    for (uint32_t j=0; j < nGeoMotionSamples; ++j)
    {
        const GeoInfoContext::Sample& gtx = gptx.getGeoInfoSample(j);
#if DEBUG
        assert(gtx.info); // shouldn't happen...
        assert(gtx.info->primitive(stx.prim_index)); // shouldn't happen...
#endif
        const DD::Image::GeoInfo&   info = *gtx.info;
        const DD::Image::Primitive& prim = *info.primitive(stx.prim_index);

        // Get prim topology at first motion sample only:
        if (j == 0)
        {
            nPoints    = info.points();
            nPrimVerts = prim.vertices();
            nPrimFaces = prim.faces();
            if (nPoints == 0 || nPrimVerts == 0 || nPrimFaces == 0)
                return; // can't render it...!

            // Find the total face vert count:
            vertsPerFace.reserve(nPrimFaces);
            for (uint32_t f=0; f < nPrimFaces; ++f)
            {
                const uint32_t nFaceVerts = prim.face_vertices(f);
                if (nFaceVerts < 3)
                    continue;
                vertsPerFace.push_back(nFaceVerts);
                nOutVerts += nFaceVerts;
                maxFaceVerts = std::max(maxFaceVerts, nFaceVerts);
            }

            // Copy the verts and non-animating vert attribs now that we know the totals:
            vertIndicesList.resize(nOutVerts);
            if (haveUVs)
                UV_list.reserve(nOutVerts);
            if (haveCfs)
                Cf_list.reserve(nOutVerts);

            const uint32_t vert_offset_start = prim.vertex_offset();
            uint32_t  face_vert_indices[maxFaceVerts];
            uint32_t* vp = vertIndicesList.data();
            for (uint32_t f=0; f < nPrimFaces; ++f)
            {
                const uint32_t nFaceVerts = prim.face_vertices(f);
                if (nFaceVerts < 3)
                    continue;

                // This DD::Image::Primitive method copies the face vert
                // indices to a memory location, so we point it at our
                // now-allocated verts array:
                prim.get_face_vertices(f, face_vert_indices);

                for (uint32_t fv=0; fv < nFaceVerts; ++fv)
                {
                    const uint32_t vindex = face_vert_indices[fv];
                    const uint32_t pindex = prim.vertex(vert_offset_start + vindex);
                    *vp++ = pindex;

                    attrib_indices[DD::Image::Group_Vertices] = vindex;
                    attrib_indices[DD::Image::Group_Points  ] = pindex;

                    if (haveUVs)
                    {
                        info0.UV_ref->copy_to_channels(attrib_indices, tmpV);
                        const Fsr::Vec4f& uv = reinterpret_cast<const Fsr::Vec4f&>(tmpV.UV());
                        UV_list.push_back(Fsr::Vec2f(uv.x/uv.w, uv.y/uv.w));
                    }

                    if (haveCfs)
                    {
                        info0.Cf_ref->copy_to_channels(attrib_indices, tmpV);
                        Cf_list.push_back(reinterpret_cast<const Fsr::Vec4f&>(tmpV.Cf()));
                    }

                }
            }
        }
        else
        {
            // Double-check that the rest of the Mesh prims are topologically the same:
            assert(info.points()   == nPoints   );
            assert(prim.vertices() == nPrimVerts);
            assert(prim.faces()    == nPrimFaces);
        }

        motion_xforms[j] = info.matrix;
        P_arrays[j]      = Fsr::getObjectPointArray(info);

        if (haveNs)
        {
            const DD::Image::GeoInfo& info = *gptx.getGeoInfoSample(j).info;
#if DEBUG
            assert(info.N_ref);
#endif

            Fsr::Vec3fList& N_list = N_lists[j];
            N_list.resize(vertIndicesList.size());
            Fsr::Vec3f* Np = N_list.data();

            const uint32_t vert_offset_start = prim.vertex_offset();
            uint32_t  face_vert_indices[maxFaceVerts];
            for (uint32_t f=0; f < nPrimFaces; ++f)
            {
                const uint32_t nFaceVerts = prim.face_vertices(f);
                if (nFaceVerts < 3)
                    continue;

                // This DD::Image::Primitive method copies the face vert
                // indices to a memory location, so we point it at our
                // now-allocated verts array:
                prim.get_face_vertices(f, face_vert_indices);

                for (uint32_t fv=0; fv < nFaceVerts; ++fv)
                {
                    const uint32_t vindex = face_vert_indices[fv];
                    const uint32_t pindex = prim.vertex(vert_offset_start + vindex);

                    attrib_indices[DD::Image::Group_Vertices] = vindex;
                    attrib_indices[DD::Image::Group_Points  ] = pindex;

                    info.N_ref->copy_to_channels(attrib_indices, tmpV);
                    *Np++ = reinterpret_cast<const Fsr::Vec3f&>(tmpV.N());
                }
            }

            N_arrays[j] = const_cast<const Fsr::Vec3f*>(N_list.data());
        }

    } // nGeoMotionSamples

    // Build a zpr::Mesh RenderPrimitive:
    if (nPoints > 0 && vertIndicesList.size() > 0 && vertsPerFace.size() > 0)
    {
        zpr::Mesh* mesh = new zpr::Mesh(&stx,
                                        enable_subdivision,
                                        subd_args,
                                        rtx.shutter_times,
                                        motion_xforms,
                                        nPoints/*numPoints*/,
                                        P_arrays.data(),
                                        (haveNs)?const_cast<const Fsr::Vec3f**>(N_arrays.data()):NULL/*N_arrays*/,
                                        (uint32_t)vertsPerFace.size()/*numFaces*/,
                                        vertsPerFace.data(),
                                        vertIndicesList.data(),
                                        (haveUVs)?UV_list.data():NULL/*UV_list*/,
                                        (haveCfs)?Cf_list.data():NULL/*Cf_list*/);
        mesh->index = gptx.addPrim(mesh);
    }

}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! DD::Image::Triangle and DD::Image::Polygon surface translator.
*/
class DDImagePolysoupHandler : public SurfaceHandler
{
  public:
    /*virtual*/ const char* Class() const { return "DDImagePolysoupHandler"; }

    DDImagePolysoupHandler() : SurfaceHandler() {}


  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for DDImagePolysoup primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }
#if DEBUG
        assert(stx.prim_index == -1);
        assert(gptx->getGeoInfoSample(0).info); // shouldn't happen...
#endif

        const uint32_t nSoupPrims = (uint32_t)stx.polysoup_prims.size();
        if (nSoupPrims == 0)
            return; // shouldn't happen...

        const DD::Image::GeoInfo& info0 = *gptx->getGeoInfoSample(0).info;
#if DEBUG
        assert(nSoupPrims <= info0.primitives());
#endif

        const uint32_t nGeoMotionSamples = gptx->numMotionSamples();
        assert(nGeoMotionSamples > 0);
        //std::cout << "  DDImagePolysoupHandler()::generateRenderPrims()";
        //std::cout << " nGeoMotionSamples=" << nGeoMotionSamples << ", nSoupPrims=" << nSoupPrims << std::endl;

        const bool haveNs  = (info0.N_ref  && info0.N_ref->attribute );
        const bool haveUVs = (info0.UV_ref && info0.UV_ref->attribute);
        const bool haveCfs = (info0.Cf_ref && info0.Cf_ref->attribute);

        Fsr::Mat4dList                 motion_xforms(nGeoMotionSamples);
        Fsr::Uint32List                vertsPerFace;
        Fsr::Uint32List                vertIndicesList;
        //
        std::vector<Fsr::Vec3fList>    P_lists(nGeoMotionSamples); //< only used if subdividing
        std::vector<Fsr::Vec3fList>    N_lists(nGeoMotionSamples);
        Fsr::Vec2fList                 UV_list;
        Fsr::Vec4fList                 Cf_list;
        //
        std::vector<const Fsr::Vec3f*> P_arrays(nGeoMotionSamples, NULL);
        std::vector<const Fsr::Vec3f*> N_arrays(nGeoMotionSamples, NULL);

        uint32_t nPoints    = 0;
        uint32_t nSoupVerts = 0;
#if DEBUG
        uint32_t nFaces     = 0;
        uint32_t nVerts     = 0;
#endif

        DD::Image::VArray tmpV;
        uint32_t attrib_indices[DD::Image::Group_Last];
        memset(attrib_indices, 0, DD::Image::Group_Last*sizeof(uint32_t));
        attrib_indices[DD::Image::Group_Object] = stx.obj_index;

        DD::Image::PrimitiveContext ptx;
        for (uint32_t j=0; j < nGeoMotionSamples; ++j)
        {
            const GeoInfoContext::Sample& gtx = gptx->getGeoInfoSample(j);
#if DEBUG
            assert(gtx.info); // shouldn't happen...
#endif
            DD::Image::GeoInfo& info = *gtx.info;
            //std::cout << "    " << j << ": info=" << gtx.info << ":" << std::endl;
            //info.print_info(std::cout);

            // Get prim topology at first motion sample only:
            if (j == 0)
            {
                // Copy the verts for prims that have only 1 face (Triangle and Polygon):
                vertsPerFace.reserve(nSoupPrims);
                for (uint32_t i=0; i < nSoupPrims; ++i)
                {
#if DEBUG
                    assert(stx.polysoup_prims[i] < info.primitives());
                    assert(info.primitive(stx.polysoup_prims[i])); // shouldn't happen...
#endif
                    const DD::Image::Primitive& prim = *info.primitive(stx.polysoup_prims[i]);
                    const uint32_t nPrimVerts = prim.vertices();
                    const uint32_t nPrimFaces = prim.faces();
                    if (nPrimVerts < 3 || nPrimFaces != 1)
                        continue; // invalid, skip

                    nSoupVerts += nPrimVerts;
                    vertsPerFace.push_back(nPrimVerts);
                }

                nPoints = info.points();
#if DEBUG
                nVerts  = nSoupVerts;
                nFaces  = (uint32_t)vertsPerFace.size();
                //std::cout << "      nPoints=" << nPoints << ", nVerts=" << nVerts << ", nFaces=" << nFaces << std::endl;
#endif

                // Copy the vert indices and non-animating vert attribs now that we know the total:
                if (haveUVs)
                    UV_list.reserve(nSoupVerts);
                if (haveCfs)
                    Cf_list.reserve(nSoupVerts);

                vertIndicesList.resize(nSoupVerts);
                uint32_t* vp = vertIndicesList.data();
                for (uint32_t i=0; i < nSoupPrims; ++i)
                {
                    const uint32_t prim_index = stx.polysoup_prims[i];
                    const DD::Image::Primitive& prim = *info.primitive(prim_index);
                    const uint32_t nPrimVerts = prim.vertices();
                    const uint32_t nPrimFaces = prim.faces();
                    if (nPrimVerts < 3 || nPrimFaces != 1)
                        continue; // invalid, skip

                    const uint32_t vert_offset_start = prim.vertex_offset();
                    for (uint32_t v=0; v < nPrimVerts; ++v)
                    {
                        const uint32_t vindex = vert_offset_start + v;
                        const uint32_t pindex = prim.vertex(v);
                        //std::cout << "  " << i << ": v" << v << "[" << vindex << "=" << pindex << "]" << std::endl;
                        *vp++ = pindex;

                        attrib_indices[DD::Image::Group_Primitives] = prim_index;
                        attrib_indices[DD::Image::Group_Vertices  ] = vindex;
                        attrib_indices[DD::Image::Group_Points    ] = pindex;

                        if (haveUVs)
                        {
                            info0.UV_ref->copy_to_channels(attrib_indices, tmpV);
                            const Fsr::Vec4f& uv = reinterpret_cast<const Fsr::Vec4f&>(tmpV.UV());
                            UV_list.push_back(Fsr::Vec2f(uv.x/uv.w, uv.y/uv.w));
                        }

                        if (haveCfs)
                        {
                            info0.Cf_ref->copy_to_channels(attrib_indices, tmpV);
                            Cf_list.push_back(reinterpret_cast<const Fsr::Vec4f&>(tmpV.Cf()));
                        }

                    }
                }

            }
            else
            {
                // Double-check that the rest of the Mesh prims are topologically the same:
                uint32_t nSoupVerts = 0;
                uint32_t nSoupFaces = 0;
                for (uint32_t i=0; i < nSoupPrims; ++i)
                {
#if DEBUG
                    assert(stx.polysoup_prims[i] < info.primitives());
                    assert(info.primitive(stx.polysoup_prims[i])); // shouldn't happen...
#endif
                    const DD::Image::Primitive& prim = *info.primitive(stx.polysoup_prims[i]);
                    const uint32_t nPrimVerts = prim.vertices();
                    const uint32_t nPrimFaces = prim.faces();
                    if (nPrimVerts < 3 || nPrimFaces == 0)
                        continue; // invalid, skip

                    nSoupVerts += nPrimVerts;
                    ++nSoupFaces;
                }
                //std::cout << "      info.points=" << nPoints << ", nSoupVerts=" << nSoupVerts << ", nSoupFaces=" << nSoupFaces << std::endl;

                assert(info.points() == nPoints);
#if DEBUG
                assert(nSoupVerts    == nVerts);
                assert(nSoupFaces    == nFaces);
#endif
            }

            motion_xforms[j] = info.matrix;
            P_arrays[j]      = Fsr::getObjectPointArray(info);
#if DEBUG
            assert(P_arrays[j] != NULL);
#endif


            if (haveNs)
            {
                const DD::Image::GeoInfo& info = *gptx->getGeoInfoSample(j).info;
#if DEBUG
                assert(info.N_ref);
#endif

                Fsr::Vec3fList& N_list = N_lists[j];
                N_list.resize(vertIndicesList.size());
                Fsr::Vec3f* Np = N_list.data();
                for (uint32_t i=0; i < nSoupPrims; ++i)
                {
                    const uint32_t prim_index = stx.polysoup_prims[i];
                    const DD::Image::Primitive& prim = *info.primitive(prim_index);
                    const uint32_t nPrimVerts = prim.vertices();
                    const uint32_t nPrimFaces = prim.faces();
                    if (nPrimVerts < 3 || nPrimFaces == 0)
                        continue; // invalid, skip

                    const uint32_t vert_offset_start = prim.vertex_offset();
                    for (uint32_t v=0; v < nPrimVerts; ++v)
                    {
                        const uint32_t vindex = vert_offset_start + v;
                        const uint32_t pindex = prim.vertex(v);
                        //std::cout << "  " << i << ": v" << v << "[" << vindex << "=" << pindex << "]" << std::endl;

                        attrib_indices[DD::Image::Group_Primitives] = prim_index;
                        attrib_indices[DD::Image::Group_Vertices  ] = vindex;
                        attrib_indices[DD::Image::Group_Points    ] = pindex;

                        info.N_ref->copy_to_channels(attrib_indices, tmpV);
                        *Np++ = reinterpret_cast<const Fsr::Vec3f&>(tmpV.N());
                    }

                }

                N_arrays[j] = const_cast<const Fsr::Vec3f*>(N_list.data());
            }

        } // nGeoMotionSamples

        // Build a zpr::Mesh RenderPrimitive:
        if (nPoints > 0 && vertIndicesList.size() > 0 && vertsPerFace.size() > 0)
        {
            zpr::Mesh* mesh = new zpr::Mesh(&stx,
                                            false/*enable_subdivision*/,
                                            Fsr::ArgSet()/*subd_args*/,
                                            rtx.shutter_times,
                                            motion_xforms,
                                            nPoints/*numPoints*/,
                                            P_arrays.data(),
                                            (haveNs)?const_cast<const Fsr::Vec3f**>(N_arrays.data()):NULL/*N_arrays*/,
                                            (uint32_t)vertsPerFace.size()/*numFaces*/,
                                            vertsPerFace.data(),
                                            vertIndicesList.data(),
                                            (haveUVs)?UV_list.data():NULL/*UV_list*/,
                                            (haveCfs)?Cf_list.data():NULL/*Cf_list*/);
            mesh->index = gptx->addPrim(mesh);
        }

    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! DD::Image::Mesh surface translator.
*/
class DDImageMeshHandler : public SurfaceHandler
{
  public:
    /*virtual*/ const char* Class() const { return "DDImageMeshHandler"; }

    DDImageMeshHandler() : SurfaceHandler() {}


  public:
    // TODO: we need to check the point normals to determine how to create the winding order.
    // Unfortunately if the Card node is in YZ or ZX mode the normals are reversed from
    // the std winding order...

    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for DDImageMesh primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }
        //std::cout << "  DDImageMeshHandler(" << stx.prim_index << ")::generateRenderPrims()" << std::endl;

#if DEBUG
        assert(gptx->getGeoInfoSample(0).info); // shouldn't happen...
#endif
        Fsr::ArgSet subd_args;
        const bool enable_subdivision = getSubdArgs(*gptx->getGeoInfoSample(0).info, subd_args);
        convertDDImagePrimitiveToMesh(rtx, stx, *gptx, enable_subdivision, subd_args);
    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! DD::Image::PolyMesh surface translator.
    Assumes we're building for Nuke version 7+.
*/
class DDImagePolyMeshHandler : public SurfaceHandler
{
  public:
    /*virtual*/ const char* Class() const { return "DDImagePolyMeshHandler"; }

    DDImagePolyMeshHandler() : SurfaceHandler() {}


  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for DDImagePolyMesh primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }
        //std::cout << "  DDImagePolyMeshHandler(" << stx.prim_index << ")::generateRenderPrims()" << std::endl;

#if DEBUG
        assert(gptx->getGeoInfoSample(0).info); // shouldn't happen...
#endif
        Fsr::ArgSet subd_args;
        const bool enable_subdivision = getSubdArgs(*gptx->getGeoInfoSample(0).info, subd_args);
        convertDDImagePrimitiveToMesh(rtx, stx, *gptx, enable_subdivision, subd_args);
    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*!
*/
class DDImagePointHandler : public SurfaceHandler
{
  public:
    DDImagePointHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "DDImagePointHandler"; }


  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for DDImagePoint primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }
        //std::cout << "  DDImagePointHandler(" << stx.prim_index << ")::generateRenderPrims(), frame=" << rtx.frame0 << std::endl;

        // TODO: finish this!
    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*! DD::Image::ParticleSprite surface translator.
*/
class DDImageParticleSpriteHandler : public SurfaceHandler
{
  public:
    /*virtual*/ const char* Class() const { return "DDImageParticleSpriteHandler"; }

    DDImageParticleSpriteHandler() : SurfaceHandler() {}


  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for DDImageParticleSprite primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }

        //std::cout << "  DDImageParticleSpriteHandler(" << stx.prim_index << ")::generateRenderPrims()" << std::endl;

        // TODO: finish this!
    }
};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*!
*/
class FsrNodePrimitiveHandler : public SurfaceHandler
{
  public:
    FsrNodePrimitiveHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "FsrNodePrimitiveHandler"; }

  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for FsrNode primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }

        //std::cout << "  FsrNodePrimitiveHandler(" << stx.prim_index << ")::generateRenderPrims()" << std::endl;

        // TODO: finish this!
    }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


/*
*/
class FsrMeshHandler : public SurfaceHandler
{
  public:
    FsrMeshHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "FsrMeshHandler"; }


  public:
    /*
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for FsrMesh primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }

        //std::cout << "  FsrMeshHandler(" << stx.prim_index << ")::generateRenderPrims()" << std::endl;

#if DEBUG
        assert(gptx->getGeoInfoSample(0).info); // shouldn't happen...
#endif
        Fsr::ArgSet subd_args;
        const bool enable_subdivision = getSubdArgs(*gptx->getGeoInfoSample(0).info, subd_args);
        convertDDImagePrimitiveToMesh(rtx, stx, *gptx, enable_subdivision, subd_args);
   }

};


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

/*!
*/
class FsrPointsHandler : public SurfaceHandler
{
  public:
    FsrPointsHandler() : SurfaceHandler() {}

    /*virtual*/ const char* Class() const { return "FsrPointsHandler"; }


  public:
    /*!
    */
    /*virtual*/
    void generateRenderPrims(RenderContext&  rtx,
                             SurfaceContext& stx)
    {
        GeoInfoContext* gptx = stx.getGeoInfoContext();
        if (!gptx)
        {
            std::cerr << "Incorrect ObjectContext type for FsrPoint primitive, ignoring." << std::endl;
            return; // shouldn't happen
        }
        //std::cout << "  FsrPointsHandler(" << stx.prim_index << ")::generateRenderPrims(), frame=" << rtx.frame0 << std::endl;

        // TODO: finish this!
    }

};



} // namespace zpr

#endif

// end of zpSurfaceHandlers.h

//
// Copyright 2020 DreamWorks Animation
//
