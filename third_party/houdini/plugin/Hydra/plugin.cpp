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

#include "GT_PrimHydra.h"
#include "GR_PrimHydra.h"
#include <DM/DM_RenderTable.h>
#include <UT/UT_DSOVersion.h> // necessary to get plugin to load

void
newGeometryPrim(GA_PrimitiveFactory* factory)
{
    // Must initialize the USD Import first for the typeId to be allocated
    pxr::GusdGU_PackedUSD::install(*factory);
}

void
newRenderHook(DM_RenderTable *dm_table)
{
    if (const char* s = getenv("HYDRA_HOUDINI_DISABLE")) {
        if (s[0] == '2') // 2 == use RE_Geometry
            GR_PrimHydra::disable = 2;
        else if (s[0] != '0') // 0 = do not disable, any other value = disable
            return;
    }
    if (const char* s = getenv("HYDRA_HOUDINI_POSTPASS"))
        GR_PrimHydra::postpass = (s[0] != '0');
    if (GT_PrimHydra::install()) {
        // add a collector for PackedUSD prims (replaces one defined by pxr)
        (new GT_PrimHydraCollect)->bind(pxr::GusdGU_PackedUSD::typeId().get());
        // add converter to GR_PrimHydra which renders them
        dm_table->registerGTHook(
            new GR_PrimHydraHook,
            GT_PrimitiveType(GT_PrimHydra::typeId()),
            10000 // priority
        );
    }
}

//
// Copyright 2019 DreamWorks Animation
//
