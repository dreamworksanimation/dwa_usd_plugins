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

#pragma once

#include <GT/GT_GEOPrimCollect.h>
#include <gusd/GU_PackedUSD.h>

/// This holds a set of GU_PackedUSD prims so they can all be rendered at once.
/// This object simply keeps an array of pointers to them, all the real work is
/// done by GR_PrimHydra.
class GT_PrimHydra : public GT_Primitive
{
public:
    GT_PrimHydra() {}; // creates an empty one

    std::vector<const GU_PrimPacked*> prims;
    UT_Array<GA_Offset> pids; // Houdini part ids or "map offset" for each prim
    UT_BoundingBox bbox; // merged bounding box of the prims

    bool empty() const { return prims.empty(); }
    size_t size() const { return prims.size(); }
    void collect(const GEO_Primitive*);

    // implement the pure virtual functions
    const char* className() const override;
    int	getPrimitiveType() const override { return mTypeId; }
    GT_PrimitiveHandle doSoftCopy() const override;
    void enlargeBounds(UT_BoundingBox boxes[], int nsegments) const override;
    int getMotionSegments() const override;
    int64 getMemoryUsage() const override;

    static int mTypeId;
    static bool install() // returns true if this is first time it was called
    {
        if (mTypeId == GT_PRIM_UNDEFINED) {
            mTypeId = createPrimitiveTypeId();
            return true;
        } else {
            return false;
        }
    }
    static int typeId() { return mTypeId; }
};

/// Builds a GT_PrimHydra object from several GU_PackedUSD prims
class GT_PrimHydraCollect : public GT_GEOPrimCollect
{
public:
    // see hydra.cc for what constructs this
    GT_PrimHydraCollect() { }

    /// Creates the GT_PrimHydra
    GT_GEOPrimCollectData* beginCollecting(
        const GT_GEODetailListHandle&, const GT_RefineParms*) const override;

    /// Adds to the GT_PrimHydra
    GT_PrimitiveHandle collect(
        const GT_GEODetailListHandle&,
        const GEO_Primitive* const* prim_list, int nsegments,
        GT_GEOPrimCollectData *data) const override;

    /// Return the new GT_PrimHydra
    GT_PrimitiveHandle endCollecting(
        const GT_GEODetailListHandle&, GT_GEOPrimCollectData*) const override;
};

//
// Copyright 2019 DreamWorks Animation
//
