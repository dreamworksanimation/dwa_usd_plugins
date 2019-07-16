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

/// @file FuserOpenSubdiv.cpp
///
/// @author Jonathan Egstad


#include <Fuser/ArgConstants.h> // for attrib names constants
#include <Fuser/Node.h>
#include <Fuser/MeshPrimitive.h>

//#include <DDImage/PrimitiveContext.h>


#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#else
// Turn off -Wconversion warnings when including Opensubdiv headers:
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"

#  include <opensubdiv/far/topologyDescriptor.h>
#  include <opensubdiv/far/primvarRefiner.h>

#  pragma GCC diagnostic pop
#endif


namespace Fsr {


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

// Wrap Fuser vector classes for OpenSubdiv use.
// See opensubdiv/far/primvarRefiner.h and opensubdiv/hbr/fvarData.h:
// <snip>
//    Interpolation methods template both the source and destination
//    data buffer classes. Client-code is expected to provide interfaces
//    that implement the functions specific to its primitive variable
//    data layout. Template APIs must implement the following:
//
//    class MySource {
//        MySource & operator[](int index);
//    };
//
//    class MyDestination {
//        void Clear();
//        void AddWithWeight(MySource const & value, float weight);
//        void AddWithWeight(MyDestination const & value, float weight);
//    };
// </snip>
//
// Fsr::Vec classes already have [] defined so we just need to expose
// setToZero() and implement AddWithWeight().

class OsdVec2f : public Fsr::Vec2f
{
  public:
    void Clear() { setToZero(); }
    void AddWithWeight(OsdVec2f const& b, float weight) { *this += b*weight; }
};

class OsdVec3f : public Fsr::Vec3f
{
  public:
    void Clear() { setToZero(); }
    void AddWithWeight(OsdVec3f const& b, float weight) { *this += b*weight; }
};

class OsdVec4f : public Fsr::Vec4f
{
  public:
    void Clear() { setToZero(); }
    void AddWithWeight(OsdVec4f const& b, float weight) { *this += b*weight; }
};


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------


/*! 
*/
class FuserOpenSubdiv : public Fsr::Node
{
  public:
    //! For create() method to instantiate this node by name.
    static const Fsr::Node::Description description;
    //! Returns the class name, must implement.
    /*virtual*/ const char* fuserNodeClass() const { return description.fuserNodeClass(); }

    //!
    FuserOpenSubdiv(const Fsr::ArgSet& args,
                    Fsr::Node*         parent) :
        Fsr::Node(args, parent)
    {
        //std::cout << "      FuserOpenSubdiv::ctor(" << this << ")" << std::endl;
#if 0
        if (m_debug)
        {
            std::cout << "      FuserOpenSubdiv::ctor(" << this << ")" << std::endl;
        }
#endif
    }


    //!
    ~FuserOpenSubdiv()
    {
        //std::cout << "      FuserOpenSubdiv::dtor(" << this << ")" << std::endl;
    }


    /*! Called before execution to allow node to update local data from args.
        Updates time value and possibly local matrix transform.
    */
    /*virtual*/ void _validateState(const Fsr::NodeContext& args,
                                    bool                    for_real)
    {
        //std::cout << "      FuserOpenSubdiv::_validateState(" << this << ")" << std::endl;
        Fsr::Node::_validateState(args, for_real);
    }


    //! Prints an unrecognized-target warning in debug mode and returns 0 (success).
    /*virtual*/ int _execute(const Fsr::NodeContext& target_context,
                             const char*             target_name,
                             void*                   target,
                             void*                   src0,
                             void*                   src1)
    {
        // We need a context and a target name to figure out what to do:
        if (!target_name || !target_name[0])
            return -1; // no context target!

        //std::cout << " FuserOpenSubdiv::execute(" << this << ") target='" << target_name << "'";
        //std::cout << " [" << target_context.args() << " ]" << std::endl;

        if (strcmp(target_name, Fsr::MeshPrimitive::TessellateContext::name)==0)
        {
            Fsr::MeshPrimitive::TessellateContext* tessellate_ctx =
                reinterpret_cast<Fsr::MeshPrimitive::TessellateContext*>(target);

            // Any null pointers throw a coding error:
            if (!tessellate_ctx || !tessellate_ctx->vbuffers)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            subdivideVertexBuffer(target_context.args(), *tessellate_ctx->vbuffers);

            return 0; // success
        }
        else if (strcmp(target_name, Fsr::MeshPrimitive::TessellateContext2::name)==0)
        {
            Fsr::MeshPrimitive::TessellateContext2* tessellate_ctx =
                reinterpret_cast<Fsr::MeshPrimitive::TessellateContext2*>(target);

            // Any null pointers throw a coding error:
            if (!tessellate_ctx)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            subdivide(target_context.args(), *tessellate_ctx);

            return 0; // success
        }
        else if (strcmp(target_name, Fsr::FuserPrimitive::RenderSceneTessellateContext::name)==0)
        {
            //
        }

        // Don't throw an error on an unrecognized target:
        if (debug())
        {
            std::cerr << fuserNodeClass() << ": warning, cannot handle target type '" << target_name << "'";
            std::cerr << ", ignoring." << std::endl;
        }
        return 0; // no user-abort
    }


    //! Return the appropriate refiner object for the given arguments and topology descriptor.
    OpenSubdiv::Far::TopologyRefiner* getRefiner(const Fsr::ArgSet&                         args,
                                                 int32_t                                    target_subd_level,
                                                 const OpenSubdiv::Far::TopologyDescriptor& desc);


    //!
    void subdivide(const Fsr::ArgSet&                      args,
                   Fsr::MeshPrimitive::TessellateContext2& tess_ctx);

    //!
    void subdivideVertexBuffer(const Fsr::ArgSet&                       args,
                               Fsr::PointBasedPrimitive::VertexBuffers& vbuffers);

};


//-------------------------------------------------------------------------


/*! Return the appropriate refiner object for the given arguments.
*/
OpenSubdiv::Far::TopologyRefiner*
FuserOpenSubdiv::getRefiner(const Fsr::ArgSet&                         args,
                            int32_t                                    target_subd_level,
                            const OpenSubdiv::Far::TopologyDescriptor& desc)
{
    const std::string& scheme = args.getString("subd:scheme", "catmullclark"/*default*/);
    OpenSubdiv::Sdc::SchemeType type;
    if      (scheme == "catmullclark") type = OpenSubdiv::Sdc::SCHEME_CATMARK;
    else if (scheme == "loop"        ) type = OpenSubdiv::Sdc::SCHEME_LOOP;
    else if (scheme == "bilinear"    ) type = OpenSubdiv::Sdc::SCHEME_BILINEAR;
    else
    {
        // TODO: throw unrecognized-scheme warning
        type = OpenSubdiv::Sdc::SCHEME_CATMARK;
    }

    // TODO: these are set to DWA defaults, also check primvars copied in from file meshes:
    OpenSubdiv::Sdc::Options options;
    options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
    options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
    options.SetCreasingMethod(OpenSubdiv::Sdc::Options::CREASE_UNIFORM);
    options.SetTriangleSubdivision(OpenSubdiv::Sdc::Options::TRI_SUB_CATMARK);

    // Create a FarTopologyRefiner from the descriptor:
    OpenSubdiv::Far::TopologyRefiner* refiner =
        OpenSubdiv::Far::TopologyRefinerFactory<OpenSubdiv::Far::TopologyDescriptor>::Create(desc,
            OpenSubdiv::Far::TopologyRefinerFactory<OpenSubdiv::Far::TopologyDescriptor>::Options(type, options));
    assert(refiner);

    // Declare how we're doing refinement to the desired target level:
    // TODO: support other refinement types like adaptive?
    if (1)
    {
        // TODO: these are set to DWA defaults, also check primvars copied in from file meshes:
        OpenSubdiv::Far::TopologyRefiner::UniformOptions refine_options(target_subd_level);
        refine_options.orderVerticesFromFacesFirst = false;
        refine_options.fullTopologyInLastLevel     = false;

        refiner->RefineUniform(refine_options);
    }
    else
    {
        // TODO: these are set to DWA defaults, also check primvars copied in from file meshes:
        OpenSubdiv::Far::TopologyRefiner::AdaptiveOptions refine_options(target_subd_level);
        refine_options.secondaryLevel              = 15;
        refine_options.useSingleCreasePatch        = false;
        refine_options.useInfSharpPatch            = false;
        refine_options.considerFVarChannels        = false;
        refine_options.orderVerticesFromFacesFirst = false;

        refiner->RefineAdaptive(refine_options);
    }

    return refiner;
}


//!
void
FuserOpenSubdiv::subdivide(const Fsr::ArgSet&                      args,
                           Fsr::MeshPrimitive::TessellateContext2& tess_ctx)

{
    if (!tess_ctx.P || !tess_ctx.Pidx || !tess_ctx.vertsPerFace)
        return; // don't crash...

    const int32_t nSrcPoints = (int32_t)tess_ctx.P->size();
    const int32_t nSrcVerts  = (int32_t)tess_ctx.Pidx->size();
    const int32_t nSrcFaces  = (int32_t)tess_ctx.vertsPerFace->size();
    if (nSrcPoints == 0 || nSrcVerts == 0 || nSrcFaces == 0)
        return; // don't crash...

    const int32_t current_subd_level = args.getInt("subd:current_level", 0/*default*/);
    const int32_t  target_subd_level = args.getInt("subd:target_level",  0/*default*/);
    //const std::string& subd_scheme   = args.getInt("subd:scheme");

    //std::cout << "  FuserOpenSubdiv::subdivide()";
    //std::cout << ", nSrcVerts=" << nSrcVerts;
    //std::cout << ", nSrcFaces=" << nSrcFaces;
    //std::cout << ", current_subd_level=" << current_subd_level;
    //std::cout << ", target_subd_level=" << target_subd_level;
    //std::cout << ", UV=" << tess_ctx.UV;
    //std::cout << ", Cf=" << tess_ctx.Cf;
    //std::cout << std::endl;

    if (target_subd_level <= current_subd_level)
        return; // no need to further subdivide

    // VertexBuffers are already expanded to vertex rate (point-rate attribs are promoted
    // to vertex-rate) so the primvar index arrays will all point at the same array:
    std::vector<int32_t> fvarIndices(nSrcVerts);
    for (int32_t i=0; i < nSrcVerts; ++i)
        fvarIndices[i] = i;

    // MUST HAVE AT LEAST ONE FVAR SO WE CAN SIZE THE VERTEX-RATE ARRAYS!
    // vbuffer.Pidx is guaranteed to be the vertex-rate count, but after
    // refinement we need to use one of the fvar channels to find the
    // new vertex count since the point count will no longer match.
    // Always enable UV fvar even if it's filled with zeros.

    const int32_t nFvarChans = 1 + 1;
    const int32_t uv_fvar_chan = 0; // TODO: this may be empty, handle this!
    const int32_t Cf_fvar_chan = 1; // TODO: this may be empty, handle this!

    OpenSubdiv::Far::TopologyDescriptor::FVarChannel primvar_channels[nFvarChans];
    {
        // TODO: support more than just UV/Cf primvars? Probably not for ScanlineRender.
        primvar_channels[uv_fvar_chan].numValues    = nSrcVerts;
        primvar_channels[uv_fvar_chan].valueIndices = fvarIndices.data();
        primvar_channels[Cf_fvar_chan].numValues    = nSrcVerts;
        primvar_channels[Cf_fvar_chan].valueIndices = fvarIndices.data();
    }

    OpenSubdiv::Far::TopologyDescriptor desc;
    desc.numVertices        = nSrcPoints; // points count, not verts!
    desc.numFaces           = nSrcFaces;
    desc.numVertsPerFace    = reinterpret_cast<const int32_t*>(tess_ctx.vertsPerFace->data());
    desc.vertIndicesPerFace = reinterpret_cast<const int32_t*>(tess_ctx.Pidx->data()); // at per-vert rate, not per-face!!!!
    //
    desc.numFVarChannels    = nFvarChans;
    desc.fvarChannels       = primvar_channels;

    // Create a FarTopologyRefiner from the descriptor:
    OpenSubdiv::Far::TopologyRefiner* refiner = getRefiner(args, target_subd_level, desc);

    //std::cout << "    GetNumFVarValuesTotal=" << refiner->GetNumFVarValuesTotal(uv_fvar_chan);
    //std::cout << ", GetNumFacesTotal=" << refiner->GetNumFacesTotal();
    //std::cout << ", GetNumVerticesTotal=" << refiner->GetNumVerticesTotal();
    //std::cout << std::endl;

    // Create a destination vertex buffer to accommodate the new verts/points to be added:
    Fsr::PointBasedPrimitive::VertexBuffers refine_vbuffers(refiner->GetNumVerticesTotal(),
                                                            refiner->GetNumFVarValuesTotal(uv_fvar_chan));
    {
        // TODO: this doesn't need to be a VertexBuffer object, just a list of vectors tied
        // to the primvars we want to refine.

        // Copy the current level data:
        memcpy(refine_vbuffers.PL.data(), tess_ctx.P->data(),  sizeof(Fsr::Vec3f)*nSrcPoints);

        if (tess_ctx.UV && tess_ctx.UV->size() == nSrcVerts)
            memcpy(refine_vbuffers.UV.data(), tess_ctx.UV->data(), sizeof(Fsr::Vec4f)*nSrcVerts);
        else
            memset(refine_vbuffers.UV.data(), 0, sizeof(Fsr::Vec4f)*nSrcVerts);

        if (tess_ctx.Cf && tess_ctx.Cf->size() == nSrcVerts)
            memcpy(refine_vbuffers.Cf.data(), tess_ctx.Cf->data(), sizeof(Fsr::Vec4f)*nSrcVerts);
        else
            memset(refine_vbuffers.Cf.data(), 0, sizeof(Fsr::Vec4f)*nSrcVerts);
    }

    // Refine mesh to desired level:
    OpenSubdiv::Far::PrimvarRefiner primvarRefiner(*refiner);
    OsdVec3f* srcPoints = reinterpret_cast<OsdVec3f*>(refine_vbuffers.PL.data());
    OsdVec4f* srcUVs    = reinterpret_cast<OsdVec4f*>(refine_vbuffers.UV.data());
    OsdVec4f* srcColors = reinterpret_cast<OsdVec4f*>(refine_vbuffers.Cf.data());
    for (int level=current_subd_level+1; level <= target_subd_level; ++level)
    {
        OsdVec3f* dstPoints = srcPoints + refiner->GetLevel(level-1).GetNumVertices();
        OsdVec4f* dstUVs    = srcUVs    + refiner->GetLevel(level-1).GetNumFVarValues(uv_fvar_chan);
        OsdVec4f* dstColors = srcColors + refiner->GetLevel(level-1).GetNumFVarValues(Cf_fvar_chan);

        primvarRefiner.Interpolate(level, srcPoints, dstPoints);
        primvarRefiner.InterpolateFaceVarying(level, srcUVs,    dstUVs,    uv_fvar_chan);
        primvarRefiner.InterpolateFaceVarying(level, srcColors, dstColors, Cf_fvar_chan);

        srcPoints = dstPoints;
        srcUVs    = dstUVs;
        srcColors = dstColors;
    }

    // Copy refined point/vert data back to source vbuffer.
    // We need to expand out the point/vert indices back to the flattened point-rate/vertex-rate:
    const OpenSubdiv::Far::TopologyLevel& refLastLevel = refiner->GetLevel(target_subd_level);

    const int32_t nLevelFaces  = refLastLevel.GetNumFaces();
    const int32_t nLevelVerts  = nLevelFaces*4; // always quads
    const int32_t nLevelPoints = refLastLevel.GetNumVertices();
    const int32_t nLevelUVs    = refLastLevel.GetNumFVarValues(uv_fvar_chan);
    const int32_t nLevelColors = refLastLevel.GetNumFVarValues(Cf_fvar_chan);
    //std::cout << "    refLastLevel: nLevelFaces=" << nLevelFaces;
    //std::cout << ", nLevelPoints=" << nLevelPoints;
    //std::cout << ", nLevelUVs=" << nLevelUVs;
    //std::cout << ", nLevelColors=" << nLevelColors;

    // Gets the starting vert index in the highest refinement level:
    const int32_t levelPointsStart = (refiner->GetNumVerticesTotal()               - nLevelPoints);
    const int32_t levelUVsStart    = (refiner->GetNumFVarValuesTotal(uv_fvar_chan) - nLevelUVs   );
    const int32_t levelColorsStart = (refiner->GetNumFVarValuesTotal(Cf_fvar_chan) - nLevelColors);
    //std::cout << ",levelPointsStart=" << levelPointsStart;
    //std::cout << ", levelUVsStart=" << levelUVsStart;
    //std::cout << ", levelColorsStart=" << levelColorsStart;
    //std::cout << std::endl;


    tess_ctx.vertsPerFace->resize(nLevelFaces);
    tess_ctx.P->resize(nLevelPoints);
    tess_ctx.Pidx->resize(nLevelVerts);
    if (tess_ctx.UV)
        tess_ctx.UV->resize(nLevelVerts);
    if (tess_ctx.Cf)
        tess_ctx.Cf->resize(nLevelVerts);
    //if (tess_ctx.N)
    //    tess_ctx.N->resize(nLevelVerts);
    //if (tess_ctx.VEL)
    //    tess_ctx.VEL->resize(nLevelVerts);

    // Point data is copied straight over:
    //std::cout << "   refine_vbuffers: numPoints=" << refine_vbuffers.numPoints();
    //std::cout << ", numVerts=" << refine_vbuffers.numVerts();
    //std::cout << std::endl;
    assert((levelPointsStart + nLevelPoints) <= refine_vbuffers.numPoints());
    memcpy(tess_ctx.P->data(), refine_vbuffers.PL.data()+levelPointsStart, sizeof(Fsr::Vec3f)*nLevelPoints);

    uint32_t v0 = 0; // global vert count
    for (int32_t f=0; f < nLevelFaces; ++f)
    {
        const OpenSubdiv::Far::ConstIndexArray fPidx = refLastLevel.GetFaceVertices(f);
#if DEBUG
        assert(fPidx.size() == 4); // all refined Catmull faces should be quads
#endif
        (*tess_ctx.vertsPerFace)[f] = 4;

        for (int i=0; i < 4; ++i)
            (*tess_ctx.Pidx)[v0+i] = fPidx[i];

        // Flatten-copy the primvar values back the VertexBuffer vert-rate buffers:
        {
            // TODO: these should be in a primvar loop
            if (tess_ctx.Cf)
            {
                const OpenSubdiv::Far::ConstIndexArray fUVidx = refLastLevel.GetFaceFVarValues(f, uv_fvar_chan);
                for (int i=0; i < 4; ++i)
                    (*tess_ctx.UV)[v0+i] = refine_vbuffers.UV[levelUVsStart + fUVidx[i]];
            }

            if (tess_ctx.UV)
            {
                const OpenSubdiv::Far::ConstIndexArray fCfidx = refLastLevel.GetFaceFVarValues(f, Cf_fvar_chan);
                for (int i=0; i < 4; ++i)
                    (*tess_ctx.Cf)[v0+i] = refine_vbuffers.Cf[levelColorsStart + fCfidx[i]];
            }
        }

        v0 += 4;
    }

}


/*!
*/
void
FuserOpenSubdiv::subdivideVertexBuffer(const Fsr::ArgSet&                       args,
                                       Fsr::PointBasedPrimitive::VertexBuffers& vbuffers)
{
    const int32_t nSrcPoints = (int32_t)vbuffers.numPoints();
    const int32_t nSrcVerts  = (int32_t)vbuffers.numVerts();
    const int32_t nSrcFaces  = (int32_t)vbuffers.numFaces();
    if (nSrcPoints == 0 || nSrcVerts == 0 || nSrcFaces == 0)
        return; // don't crash...

    const int32_t current_subd_level = args.getInt("subd:current_level", 0/*default*/);
    const int32_t  target_subd_level = args.getInt("subd:target_level",  0/*default*/);

    //std::cout << "  FuserOpenSubdiv::subdivideVertexBuffer()";
    //std::cout << ", nSrcVerts=" << nSrcVerts;
    //std::cout << ", nSrcFaces=" << nSrcFaces;
    //std::cout << ", current_subd_level=" << current_subd_level;
    //std::cout << ", target_subd_level=" << target_subd_level;
    //std::cout << std::endl;

    if (target_subd_level <= current_subd_level)
        return; // no need to further subdivide

    // VertexBuffers are already expanded to vertex rate (point-rate attribs are promoted
    // to vertex-rate) so the primvar index arrays will all point at the same array:
    std::vector<int32_t> fvarIndices(nSrcVerts);
    for (int32_t i=0; i < nSrcVerts; ++i)
        fvarIndices[i] = i;

    // MUST HAVE AT LEAST ONE FVAR SO WE CAN SIZE THE VERTEX-RATE ARRAYS!
    // vbuffer.Pidx is guaranteed to be the vertex-rate count, but after
    // refinement we need to use one of the fvar channels to find the
    // new vertex count since the point count will no longer match.
    // Always enable UV fvar even if it's filled with zeros.

    const int32_t nFvarChans = 1 + 1;
    const int32_t uv_fvar_chan = 0; // TODO: this may be empty, handle this!
    const int32_t Cf_fvar_chan = 1; // TODO: this may be empty, handle this!

    OpenSubdiv::Far::TopologyDescriptor::FVarChannel primvar_channels[nFvarChans];
    {
        // TODO: support more than just UV/Cf primvars? Probably not for ScanlineRender.
        primvar_channels[uv_fvar_chan].numValues    = nSrcVerts;
        primvar_channels[uv_fvar_chan].valueIndices = fvarIndices.data();
        primvar_channels[Cf_fvar_chan].numValues    = nSrcVerts;
        primvar_channels[Cf_fvar_chan].valueIndices = fvarIndices.data();
    }

    OpenSubdiv::Far::TopologyDescriptor desc;
    desc.numVertices        = nSrcPoints; // points count, not verts!
    desc.numFaces           = nSrcFaces;
    desc.numVertsPerFace    = reinterpret_cast<const int32_t*>(vbuffers.vertsPerFace.data());
    desc.vertIndicesPerFace = reinterpret_cast<const int32_t*>(vbuffers.Pidx.data()); // at per-vert rate, not per-face!!!!
    //
    desc.numFVarChannels    = nFvarChans;
    desc.fvarChannels       = primvar_channels;

    // Create a FarTopologyRefiner from the descriptor:
    OpenSubdiv::Far::TopologyRefiner* refiner = getRefiner(args, target_subd_level, desc);

    //std::cout << "    GetNumFVarValuesTotal=" << refiner->GetNumFVarValuesTotal(uv_fvar_chan);
    //std::cout << ", GetNumFacesTotal=" << refiner->GetNumFacesTotal();
    //std::cout << ", GetNumVerticesTotal=" << refiner->GetNumVerticesTotal();
    //std::cout << std::endl;

    // Create a destination vertex buffer to accommodate the new verts/points to be added:
    Fsr::PointBasedPrimitive::VertexBuffers refine_vbuffers(refiner->GetNumVerticesTotal(),
                                                            refiner->GetNumFVarValuesTotal(uv_fvar_chan));
    {
        // TODO: this doesn't need to be a VertexBuffer object, just a list of vectors tied
        // to the primvars we want to refine.

        // Copy the current level data:
        memcpy(refine_vbuffers.PL.data(), vbuffers.PL.data(), sizeof(Fsr::Vec3f)*nSrcPoints);
        memcpy(refine_vbuffers.UV.data(), vbuffers.UV.data(), sizeof(Fsr::Vec4f)*nSrcVerts );
        memcpy(refine_vbuffers.Cf.data(), vbuffers.Cf.data(), sizeof(Fsr::Vec4f)*nSrcVerts );
    }

    // Refine mesh to desired level:
    OpenSubdiv::Far::PrimvarRefiner primvarRefiner(*refiner);
    OsdVec3f* srcPoints = reinterpret_cast<OsdVec3f*>(refine_vbuffers.PL.data());
    OsdVec4f* srcUVs    = reinterpret_cast<OsdVec4f*>(refine_vbuffers.UV.data());
    OsdVec4f* srcColors = reinterpret_cast<OsdVec4f*>(refine_vbuffers.Cf.data());
    for (int level=current_subd_level+1; level <= target_subd_level; ++level)
    {
        OsdVec3f* dstPoints = srcPoints + refiner->GetLevel(level-1).GetNumVertices();
        OsdVec4f* dstUVs    = srcUVs    + refiner->GetLevel(level-1).GetNumFVarValues(uv_fvar_chan);
        OsdVec4f* dstColors = srcColors + refiner->GetLevel(level-1).GetNumFVarValues(Cf_fvar_chan);

        primvarRefiner.Interpolate(level, srcPoints, dstPoints);
        primvarRefiner.InterpolateFaceVarying(level, srcUVs,    dstUVs,    uv_fvar_chan);
        primvarRefiner.InterpolateFaceVarying(level, srcColors, dstColors, Cf_fvar_chan);

        srcPoints = dstPoints;
        srcUVs    = dstUVs;
        srcColors = dstColors;
    }

    // Copy refined point/vert data back to source vbuffer.
    // We need to expand out the point/vert indices back to the flattened point-rate/vertex-rate:
    const OpenSubdiv::Far::TopologyLevel& refLastLevel = refiner->GetLevel(target_subd_level);

    const int32_t nLevelFaces  = refLastLevel.GetNumFaces();
    const int32_t nLevelVerts  = nLevelFaces*4; // always quads
    const int32_t nLevelPoints = refLastLevel.GetNumVertices();
    const int32_t nLevelUVs    = refLastLevel.GetNumFVarValues(uv_fvar_chan);
    const int32_t nLevelColors = refLastLevel.GetNumFVarValues(Cf_fvar_chan);
    //std::cout << "    refLastLevel: nLevelFaces=" << nLevelFaces;
    //std::cout << ", nLevelPoints=" << nLevelPoints;
    //std::cout << ", nLevelUVs=" << nLevelUVs;
    //std::cout << ", nLevelColors=" << nLevelColors;

    // Gets the starting vert index in the highest refinement level:
    const int32_t levelPointsStart = (refiner->GetNumVerticesTotal()               - nLevelPoints);
    const int32_t levelUVsStart    = (refiner->GetNumFVarValuesTotal(uv_fvar_chan) - nLevelUVs   );
    const int32_t levelColorsStart = (refiner->GetNumFVarValuesTotal(Cf_fvar_chan) - nLevelColors);
    //std::cout << ",levelPointsStart=" << levelPointsStart;
    //std::cout << ", levelUVsStart=" << levelUVsStart;
    //std::cout << ", levelColorsStart=" << levelColorsStart;
    //std::cout << std::endl;

    vbuffers.resizePoints(nLevelPoints);
    vbuffers.resizeVerts(nLevelVerts);
    vbuffers.setAllQuadsMode(); // no need to fill vbuffers.vertsPerFace

    // Point data is copied straight over:
    //std::cout << "   refine_vbuffers: numPoints=" << refine_vbuffers.numPoints();
    //std::cout << ", numVerts=" << refine_vbuffers.numVerts();
    //std::cout << std::endl;
    assert((levelPointsStart + nLevelPoints) <= refine_vbuffers.numPoints());
    memcpy(vbuffers.PL.data(), refine_vbuffers.PL.data()+levelPointsStart, sizeof(Fsr::Vec3f)*nLevelPoints);

    uint32_t v0 = 0; // global vert count
    for (int32_t f=0; f < nLevelFaces; ++f)
    {
        const OpenSubdiv::Far::ConstIndexArray fPidx = refLastLevel.GetFaceVertices(f);
#if DEBUG
        assert(fPidx.size() == 4); // all refined Catmull faces should be quads
#endif
        for (int i=0; i < 4; ++i)
            vbuffers.Pidx[v0+i] = fPidx[i];

        // Flatten-copy the primvar values back the VertexBuffer vert-rate buffers:
        {
            // TODO: these should be in a primvar loop
            const OpenSubdiv::Far::ConstIndexArray fUVidx = refLastLevel.GetFaceFVarValues(f, uv_fvar_chan);
            for (int i=0; i < 4; ++i)
                vbuffers.UV[v0+i] = refine_vbuffers.UV[levelUVsStart + fUVidx[i]];

            const OpenSubdiv::Far::ConstIndexArray fCfidx = refLastLevel.GetFaceFVarValues(f, Cf_fvar_chan);
            for (int i=0; i < 4; ++i)
                vbuffers.Cf[v0+i] = refine_vbuffers.Cf[levelColorsStart + fCfidx[i]];
        }

        v0 += 4;
    }

}


//------------------------------------------------------------------------------

/*! 
*/
static Fsr::Node* buildOpenSubdiv(const char*        builder_class,
                                  const Fsr::ArgSet& args,
                                  Fsr::Node*         parent)
{
    return new FuserOpenSubdiv(args, parent);
}

// Register plugin:
/*static*/ FSR_EXPORT
const Fsr::Node::Description
FuserOpenSubdiv::description("OpenSubdiv"/*plugin name*/, buildOpenSubdiv/*ctor*/);


} // namespace Fsr


// end of FuserOpenSubdiv.cpp

//
// Copyright 2019 DreamWorks Animation
//
