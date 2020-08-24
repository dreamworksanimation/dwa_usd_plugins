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
#include <Fuser/ExecuteTargetContexts.h> // for MeshTessellateContext

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

class OsdFloat
{
  public:
    void Clear() { val = 0.0f; }
    void AddWithWeight(OsdFloat const& b, float weight) { val += b.val*weight; }
  protected:
    float val;
};

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
    /*virtual*/ void _validateState(const Fsr::NodeContext& exec_ctx,
                                    bool                    for_real)
    {
        //std::cout << "      FuserOpenSubdiv::_validateState(" << this << ")" << std::endl;
        Fsr::Node::_validateState(exec_ctx, for_real);
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

        if (strcmp(target_name, Fsr::MeshTessellateContext::name)==0)
        {
            Fsr::MeshTessellateContext* tessellate_ctx =
                reinterpret_cast<Fsr::MeshTessellateContext*>(target);

            // Any null pointers throw a coding error:
            if (!tessellate_ctx)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            subdivideGenericMesh(target_context.args(), *tessellate_ctx);

            return 0; // success
        }
        else if (strcmp(target_name, Fsr::MeshPrimitive::TessellateContext::name)==0)
        {
            Fsr::MeshPrimitive::TessellateContext* tessellate_ctx =
                reinterpret_cast<Fsr::MeshPrimitive::TessellateContext*>(target);

            // Any null pointers throw a coding error:
            if (!tessellate_ctx || !tessellate_ctx->vbuffers)
                return error("null objects in target '%s'. This is likely a coding error", target_name);

            subdivideVertexBuffer(target_context.args(), *tessellate_ctx->vbuffers);

            return 0; // success
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
    OpenSubdiv::Far::TopologyRefiner* getRefiner(const Fsr::ArgSet&                         exec_args,
                                                 int32_t                                    nRefinementLevels,
                                                 const OpenSubdiv::Far::TopologyDescriptor& desc);


    //!
    void subdivideGenericMesh(const Fsr::ArgSet&          exec_args,
                              Fsr::MeshTessellateContext& tess_ctx);

    //!
    void subdivideVertexBuffer(const Fsr::ArgSet&                       exec_args,
                               Fsr::PointBasedPrimitive::VertexBuffers& vbuffers);

};


//-------------------------------------------------------------------------


/*! Return the appropriate refiner object for the given arguments.
*/
OpenSubdiv::Far::TopologyRefiner*
FuserOpenSubdiv::getRefiner(const Fsr::ArgSet&                         exec_args,
                            int32_t                                    nRefinementLevels,
                            const OpenSubdiv::Far::TopologyDescriptor& desc)
{
    const std::string scheme = exec_args.getString("subd:scheme", "catmullclark"/*default*/);
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
        OpenSubdiv::Far::TopologyRefiner::UniformOptions refine_options(nRefinementLevels);
        refine_options.orderVerticesFromFacesFirst = false;
        refine_options.fullTopologyInLastLevel     = false;

        refiner->RefineUniform(refine_options);
    }
    else
    {
        // TODO: these are set to DWA defaults, also check primvars copied in from file meshes:
        OpenSubdiv::Far::TopologyRefiner::AdaptiveOptions refine_options(nRefinementLevels);
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
FuserOpenSubdiv::subdivideGenericMesh(const Fsr::ArgSet&          exec_args,
                                      Fsr::MeshTessellateContext& tess_ctx)

{
    if (!tess_ctx.verts_per_face            ||
        !tess_ctx.vert_position_indices     ||
        tess_ctx.position_lists.size() == 0 ||
        !tess_ctx.position_lists[0])
        return; // don't crash...

    const int32_t nSrcFaces  = (int32_t)tess_ctx.verts_per_face->size();
    const int32_t nSrcVerts  = (int32_t)tess_ctx.vert_position_indices->size();
    const int32_t nSrcPoints = (int32_t)tess_ctx.position_lists[0]->size();
    if (nSrcFaces == 0 || nSrcVerts == 0 || nSrcPoints == 0)
        return; // don't crash...

    const int32_t current_subd_level = exec_args.getInt("subd:current_level", 0/*default*/);
    const int32_t  target_subd_level = exec_args.getInt("subd:target_level",  0/*default*/);
    //const std::string subd_scheme   = exec_args.getString("subd:scheme");
    const int nRefinementLevels = (target_subd_level - current_subd_level);

    //std::cout << "  FuserOpenSubdiv::subdivide()";
    //std::cout << ", nSrcFaces=" << nSrcFaces;
    //std::cout << ", nSrcVerts=" << nSrcVerts;
    //std::cout << ", nSrcPoints=" << nSrcPoints;
    //std::cout << ", current_subd_level=" << current_subd_level;
    //std::cout << ", target_subd_level=" << target_subd_level;
    //std::cout << std::endl;
#if 0
    std::cout << "src faces:" << std::endl;
    for (size_t p=0; p < tess_ctx.verts_per_face->size(); ++p)
        std::cout << "      " << p << ": " << (*tess_ctx.verts_per_face)[p] << std::endl;

    std::cout << "src point indices:" << std::endl;
    for (size_t p=0; p < tess_ctx.vert_position_indices->size(); ++p)
        std::cout << "      " << p << ": " << (*tess_ctx.vert_position_indices)[p] << std::endl;

    for (size_t j=0; j < tess_ctx.position_lists.size(); ++j)
    {
        Fsr::Vec3fList& P_list = *tess_ctx.position_lists[j];
        std::cout << "src points[" << j << "]:" << std::endl;
        for (size_t i=0; i < P_list.size(); ++i)
            std::cout << "      " << i << P_list[i] << std::endl;
    }
    //for (size_t j=0; j < tess_ctx.vert_vec3_attribs.size(); ++j)
    //{
    //    Fsr::Vec3fList& vec3_list = *tess_ctx.vert_vec3_attribs[j];
    //    std::cout << "src vec3s[" << j << "]:" << std::endl;
    //    for (size_t i=0; i < vec3_list.size(); ++i)
    //        std::cout << "      " << i << vec3_list[i] << std::endl;
    //}
#endif

    if (nRefinementLevels <= 0)
        return; // no need to further subdivide

    // Attribs are already expanded to vertex rate so the primvar index arrays
    // will all point at the same indice array:
    std::vector<int32_t> fvarIndices(nSrcVerts);
    for (int32_t i=0; i < nSrcVerts; ++i)
        fvarIndices[i] = i;

    // Get number of Fvars and set up channel list:
    const int32_t nFvarChans = (int32_t)(tess_ctx.vert_float_attribs.size() +
                                         tess_ctx.vert_vec2_attribs.size()  +
                                         tess_ctx.vert_vec3_attribs.size()  +
                                         tess_ctx.vert_vec4_attribs.size());

    OpenSubdiv::Far::TopologyDescriptor::FVarChannel primvar_channels[nFvarChans];
    for (size_t i=0; i < nFvarChans; ++i)
    {
        primvar_channels[i].numValues    = nSrcVerts;
        primvar_channels[i].valueIndices = fvarIndices.data();
    }

    OpenSubdiv::Far::TopologyDescriptor desc;
    desc.numVertices        = nSrcPoints; // points count, not verts!
    desc.numFaces           = nSrcFaces;
    desc.numVertsPerFace    = reinterpret_cast<const int32_t*>(tess_ctx.verts_per_face->data());
    desc.vertIndicesPerFace = reinterpret_cast<const int32_t*>(tess_ctx.vert_position_indices->data()); // at per-vert rate, not per-face!!!!
    //
    desc.numFVarChannels    = nFvarChans;
    desc.fvarChannels       = primvar_channels;

    // Create a FarTopologyRefiner from the descriptor:
    OpenSubdiv::Far::TopologyRefiner* refiner = getRefiner(exec_args, nRefinementLevels, desc);

    OpenSubdiv::Far::PrimvarRefiner primvarRefiner(*refiner);
    //const size_t nTotalRefinedFaces  = refiner->GetNumFacesTotal();
    const size_t nTotalRefinedVerts  = (nFvarChans > 0) ?
                                            refiner->GetNumFVarValuesTotal(0/*fvarChannelIdx*/) : 0;
    const size_t nTotalRefinedPoints = refiner->GetNumVerticesTotal();
    //std::cout << "    nTotalRefinedFaces=" << refiner->GetNumFacesTotal();
    //std::cout << ", nTotalRefinedPoints=" << nTotalRefinedPoints;
    //std::cout << ", nTotalRefinedVerts=" << nTotalRefinedVerts;
    //std::cout << std::endl;

    // Source pointer lists to swap during refinement loop:
    // TODO: make a struct to hold these...
    std::vector<OsdVec3f*> srcPositions(tess_ctx.position_lists.size());
    std::vector<OsdFloat*> srcFloatAttribs(tess_ctx.vert_float_attribs.size());
    std::vector<OsdVec2f*> srcVec2Attribs(tess_ctx.vert_vec2_attribs.size());
    std::vector<OsdVec3f*> srcVec3Attribs(tess_ctx.vert_vec3_attribs.size());
    std::vector<OsdVec4f*> srcVec4Attribs(tess_ctx.vert_vec4_attribs.size());
    // Destination buffers to accommodate the new verts/points being added:
    std::vector<Fsr::Vec3fList> refined_position_lists(tess_ctx.position_lists.size());       // point rate
    std::vector<Fsr::FloatList> refined_vert_float_attribs(tess_ctx.vert_float_attribs.size()); // face-vertex rate
    std::vector<Fsr::Vec2fList> refined_vert_vec2_attribs(tess_ctx.vert_vec2_attribs.size()); // face-vertex rate
    std::vector<Fsr::Vec3fList> refined_vert_vec3_attribs(tess_ctx.vert_vec3_attribs.size()); // face-vertex rate
    std::vector<Fsr::Vec4fList> refined_vert_vec4_attribs(tess_ctx.vert_vec4_attribs.size()); // face-vertex rate

    //------------------------------------------
    // Initialize positions buffers:
    for (size_t i=0; i < refined_position_lists.size(); ++i)
    {
#if DEBUG
        assert(tess_ctx.position_lists[i] != NULL);
        assert(tess_ctx.position_lists[i]->size() == nSrcPoints);
#endif
        srcPositions[i] = reinterpret_cast<OsdVec3f*>(tess_ctx.position_lists[i]->data());
        refined_position_lists[i].resize(nTotalRefinedPoints);
    }
    if (nFvarChans > 0)
    {
        //------------------------------------------
        // Initialize fvars buffers:
        for (size_t i=0; i < refined_vert_float_attribs.size(); ++i)
        {
#if DEBUG
            assert(tess_ctx.vert_float_attribs[i] != NULL);
            assert(tess_ctx.vert_float_attribs[i]->size() == nSrcVerts);
#endif
            srcFloatAttribs[i] = reinterpret_cast<OsdFloat*>(tess_ctx.vert_float_attribs[i]->data());
            refined_vert_float_attribs[i].resize(nTotalRefinedVerts);
        }
        for (size_t i=0; i < refined_vert_vec2_attribs.size(); ++i)
        {
#if DEBUG
            assert(tess_ctx.vert_vec2_attribs[i] != NULL);
            assert(tess_ctx.vert_vec2_attribs[i]->size() == nSrcVerts);
#endif
            srcVec2Attribs[i] = reinterpret_cast<OsdVec2f*>(tess_ctx.vert_vec2_attribs[i]->data());
            refined_vert_vec2_attribs[i].resize(nTotalRefinedVerts);
        }
        for (size_t i=0; i < refined_vert_vec3_attribs.size(); ++i)
        {
#if DEBUG
            assert(tess_ctx.vert_vec3_attribs[i] != NULL);
            assert(tess_ctx.vert_vec3_attribs[i]->size() == nSrcVerts);
#endif
            srcVec3Attribs[i] = reinterpret_cast<OsdVec3f*>(tess_ctx.vert_vec3_attribs[i]->data());
            refined_vert_vec3_attribs[i].resize(nTotalRefinedVerts);
        }
        for (size_t i=0; i < refined_vert_vec4_attribs.size(); ++i)
        {
#if DEBUG
            assert(tess_ctx.vert_vec4_attribs[i] != NULL);
            assert(tess_ctx.vert_vec4_attribs[i]->size() == nSrcVerts);
#endif
            srcVec4Attribs[i] = reinterpret_cast<OsdVec4f*>(tess_ctx.vert_vec4_attribs[i]->data());
            refined_vert_vec4_attribs[i].resize(nTotalRefinedVerts);
        }
    }

    // Refine mesh attributes to desired level:
    for (int srcLevel=0; srcLevel < nRefinementLevels; ++srcLevel)
    {
        const int dstLevel = (srcLevel + 1);
        //std::cout << "       srcLevel " << srcLevel << " dstLevel " << dstLevel << std::endl;

        //------------------------------------------
        // Positions:
        for (size_t i=0; i < refined_position_lists.size(); ++i)
        {
            OsdVec3f* dst = (srcLevel == 0) ?
                                reinterpret_cast<OsdVec3f*>(refined_position_lists[i].data()) :
                                srcPositions[i];
            dst += refiner->GetLevel(srcLevel).GetNumVertices();
            primvarRefiner.Interpolate(dstLevel, srcPositions[i], dst);
            srcPositions[i] = dst;
        }

        if (nFvarChans > 0)
        {
            //------------------------------------------
            // FVars:
            uint32_t fvar_chan = 0;
            for (size_t i=0; i < refined_vert_float_attribs.size(); ++i, ++fvar_chan)
            {
                const size_t nSrcFVars = refiner->GetLevel(srcLevel).GetNumFVarValues(fvar_chan);
                OsdFloat* src = srcFloatAttribs[i];
                OsdFloat* dst = ((srcLevel == 0) ?
                                    reinterpret_cast<OsdFloat*>(refined_vert_float_attribs[i].data()) :
                                    src) + nSrcFVars;
                primvarRefiner.InterpolateFaceVarying(dstLevel, src, dst, fvar_chan);
                srcFloatAttribs[i] = dst;
            }
            for (size_t i=0; i < refined_vert_vec2_attribs.size(); ++i, ++fvar_chan)
            {
                const size_t nSrcFVars = refiner->GetLevel(srcLevel).GetNumFVarValues(fvar_chan);
                OsdVec2f* src = srcVec2Attribs[i];
                OsdVec2f* dst = ((srcLevel == 0) ?
                                    reinterpret_cast<OsdVec2f*>(refined_vert_vec2_attribs[i].data()) :
                                    src) + nSrcFVars;
                primvarRefiner.InterpolateFaceVarying(dstLevel, src, dst, fvar_chan);
                srcVec2Attribs[i] = dst;
            }
            for (size_t i=0; i < refined_vert_vec3_attribs.size(); ++i, ++fvar_chan)
            {
                const size_t nSrcFVars = refiner->GetLevel(srcLevel).GetNumFVarValues(fvar_chan);
                OsdVec3f* src = srcVec3Attribs[i];
                OsdVec3f* dst = ((srcLevel == 0) ?
                                    reinterpret_cast<OsdVec3f*>(refined_vert_vec3_attribs[i].data()) :
                                    src) + nSrcFVars;
                primvarRefiner.InterpolateFaceVarying(dstLevel, src, dst, fvar_chan);
                srcVec3Attribs[i] = dst;
            }
            for (size_t i=0; i < refined_vert_vec4_attribs.size(); ++i, ++fvar_chan)
            {
                const size_t nSrcFVars = refiner->GetLevel(srcLevel).GetNumFVarValues(fvar_chan);
                OsdVec4f* src = srcVec4Attribs[i];
                OsdVec4f* dst = ((srcLevel == 0) ?
                                    reinterpret_cast<OsdVec4f*>(refined_vert_vec4_attribs[i].data()) :
                                    src) + nSrcFVars;
                primvarRefiner.InterpolateFaceVarying(dstLevel, src, dst, fvar_chan);
                srcVec4Attribs[i] = dst;
            }
        }
    }

    // Copy refined point/vert data.
    // We need to expand out the point/vert indices back to the flattened point-rate/vertex-rate:
    const OpenSubdiv::Far::TopologyLevel& refLastLevel = refiner->GetLevel(nRefinementLevels);

    const int32_t nLevelFaces  = refLastLevel.GetNumFaces();
    const int32_t nLevelVerts  = nLevelFaces*4; // always quads
    const int32_t nLevelPoints = refLastLevel.GetNumVertices();
    const int32_t nLevelFVars  = (nFvarChans > 0) ?
                                    refLastLevel.GetNumFVarValues(0/*fvarChannelIdx*/) : 0;

    // Gets the starting point & facevert index in the highest refinement level:
    const int32_t levelPointsStart = (refiner->GetNumVerticesTotal() - nLevelPoints);
    const int32_t levelFVarsStart  = (nFvarChans > 0) ?
                                        (refiner->GetNumFVarValuesTotal(0/*fvarChannelIdx*/) - nLevelFVars) : 0;
    //std::cout << "    final level " << current_subd_level+nRefinementLevels << ": nLevelFaces=" << nLevelFaces;
    //std::cout << ", nLevelPoints=" << nLevelPoints;
    //std::cout << ", nLevelFVars=" << nLevelFVars;
    //std::cout << ", levelPointsStart=" << levelPointsStart;
    //std::cout << ", levelFVarsStart=" << levelFVarsStart;
    //std::cout << std::endl;

    // Point data is copied straight over:
#if DEBUG
    assert((levelPointsStart + nLevelPoints) <= nTotalRefinedPoints);
#endif
    for (size_t i=0; i < tess_ctx.position_lists.size(); ++i)
    {
        tess_ctx.position_lists[i]->resize(nLevelPoints);
        memcpy(tess_ctx.position_lists[i]->data(), refined_position_lists[i].data()+levelPointsStart, sizeof(Fsr::Vec3f)*nLevelPoints);
    }

    if (nFvarChans > 0)
    {
        for (size_t i=0; i < tess_ctx.vert_float_attribs.size(); ++i)
            tess_ctx.vert_float_attribs[i]->resize(nLevelVerts);
        for (size_t i=0; i < tess_ctx.vert_vec2_attribs.size(); ++i)
            tess_ctx.vert_vec2_attribs[i]->resize(nLevelVerts);
        for (size_t i=0; i < tess_ctx.vert_vec3_attribs.size(); ++i)
            tess_ctx.vert_vec3_attribs[i]->resize(nLevelVerts);
        for (size_t i=0; i < tess_ctx.vert_vec4_attribs.size(); ++i)
            tess_ctx.vert_vec4_attribs[i]->resize(nLevelVerts);
    }

    // 

    tess_ctx.all_quads = false;
    tess_ctx.all_tris  = false;

    uint32_t nFaceVerts;
    switch (refiner->GetSchemeType())
    {
        default:
        case OpenSubdiv::Sdc::SCHEME_CATMARK:  nFaceVerts = 4; tess_ctx.all_quads = true; break;
        case OpenSubdiv::Sdc::SCHEME_LOOP:     nFaceVerts = 3; tess_ctx.all_tris  = true; break;
        case OpenSubdiv::Sdc::SCHEME_BILINEAR: nFaceVerts = 4; tess_ctx.all_quads = true; break;
    }

    // Face-vert attrib data needs to be de-referenced via face indices:
    tess_ctx.verts_per_face->resize(nLevelFaces);
    tess_ctx.vert_position_indices->resize(nLevelVerts);

    uint32_t face_vert_start = 0; // global vert count
    for (int32_t f=0; f < nLevelFaces; ++f)
    {
        const OpenSubdiv::Far::ConstIndexArray facevert_position_indices = refLastLevel.GetFaceVertices(f);

#if DEBUG
        assert(facevert_position_indices.size() == nFaceVerts); // all refined faces should be same
#endif
        (*tess_ctx.verts_per_face)[f] = nFaceVerts; // refined face vert count

        for (uint32_t v=0; v < nFaceVerts; ++v)
            (*tess_ctx.vert_position_indices)[face_vert_start+v] = facevert_position_indices[v];

        // Flatten-copy the primvar values back the vert-rate buffers:
        if (nFvarChans > 0)
        {
            //------------------------------------------
            // FVars:
            uint32_t fvar_chan = 0;
            for (size_t i=0; i < tess_ctx.vert_float_attribs.size(); ++i, ++fvar_chan)
            {
                const OpenSubdiv::Far::ConstIndexArray face_fvar_indices = refLastLevel.GetFaceFVarValues(f, fvar_chan);
                //
                const float* src = refined_vert_float_attribs[i].data()   + levelFVarsStart;
                float*       dst = tess_ctx.vert_float_attribs[i]->data() + face_vert_start;
                for (int v=0; v < nFaceVerts; ++v)
                    *dst++ = src[face_fvar_indices[v]];
            }
            for (size_t i=0; i < tess_ctx.vert_vec2_attribs.size(); ++i, ++fvar_chan)
            {
                const OpenSubdiv::Far::ConstIndexArray face_fvar_indices = refLastLevel.GetFaceFVarValues(f, fvar_chan);
                //
                const Fsr::Vec2f* src = refined_vert_vec2_attribs[i].data()   + levelFVarsStart;
                Fsr::Vec2f*       dst = tess_ctx.vert_vec2_attribs[i]->data() + face_vert_start;
                for (int v=0; v < nFaceVerts; ++v)
                    *dst++ = src[face_fvar_indices[v]];
            }
            for (size_t i=0; i < tess_ctx.vert_vec3_attribs.size(); ++i, ++fvar_chan)
            {
                const OpenSubdiv::Far::ConstIndexArray face_fvar_indices = refLastLevel.GetFaceFVarValues(f, fvar_chan);
                //
                const Fsr::Vec3f* src = refined_vert_vec3_attribs[i].data()   + levelFVarsStart;
                Fsr::Vec3f*       dst = tess_ctx.vert_vec3_attribs[i]->data() + face_vert_start;
                for (int v=0; v < nFaceVerts; ++v)
                    *dst++ = src[face_fvar_indices[v]];
            }
            for (size_t i=0; i < tess_ctx.vert_vec4_attribs.size(); ++i, ++fvar_chan)
            {
                const OpenSubdiv::Far::ConstIndexArray face_fvar_indices = refLastLevel.GetFaceFVarValues(f, fvar_chan);
                //
                const Fsr::Vec4f* src = refined_vert_vec4_attribs[i].data()   + levelFVarsStart;
                Fsr::Vec4f*       dst = tess_ctx.vert_vec4_attribs[i]->data() + face_vert_start;
                for (int v=0; v < nFaceVerts; ++v)
                    *dst++ = src[face_fvar_indices[v]];
            }
        }

        face_vert_start += nFaceVerts;
    }

}


/*!
*/
void
FuserOpenSubdiv::subdivideVertexBuffer(const Fsr::ArgSet&                       exec_args,
                                       Fsr::PointBasedPrimitive::VertexBuffers& vbuffers)
{
    const int32_t nSrcPoints = (int32_t)vbuffers.numPoints();
    const int32_t nSrcVerts  = (int32_t)vbuffers.numVerts();
    const int32_t nSrcFaces  = (int32_t)vbuffers.numFaces();
    if (nSrcPoints == 0 || nSrcVerts == 0 || nSrcFaces == 0)
        return; // don't crash...

    const int32_t current_subd_level = exec_args.getInt("subd:current_level", 0/*default*/);
    const int32_t  target_subd_level = exec_args.getInt("subd:target_level",  0/*default*/);
    const int nRefinementLevels = (target_subd_level - current_subd_level);

    //std::cout << "  FuserOpenSubdiv::subdivideVertexBuffer()";
    //std::cout << ", nSrcVerts=" << nSrcVerts;
    //std::cout << ", nSrcFaces=" << nSrcFaces;
    //std::cout << ", current_subd_level=" << current_subd_level;
    //std::cout << ", target_subd_level=" << target_subd_level;
    //std::cout << std::endl;

    if (nRefinementLevels <= 0)
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
    OpenSubdiv::Far::TopologyRefiner* refiner = getRefiner(exec_args, nRefinementLevels, desc);

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
    for (int level=1; level <= nRefinementLevels; ++level)
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
    const OpenSubdiv::Far::TopologyLevel& refLastLevel = refiner->GetLevel(nRefinementLevels);

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
