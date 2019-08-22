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

#include <pxr/pxr.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/trace/trace.h>
#include <pxr/base/arch/fileSystem.h>

#include "pxr/usd/usdat/usdatFileFormat.h"

#include <boost/xpressive/xpressive.hpp>

#include <string>
#include <cstdio>

using namespace boost;
using namespace xpressive;

namespace
{
    struct pa_replace
    {
        typedef std::map<std::string, std::string> stringmap;
        stringmap lut;

        template<typename Out>
        Out operator()(smatch const &var, Out out) const {
            stringmap::const_iterator lutIter = lut.find(var[1]);
            if(lutIter != lut.end()) {
                std::copy(lutIter->second.begin(), lutIter->second.end(), out);
            }
            else {
                // leave variables in tact if no match
                std::string orig = "${";
                orig += var[1];
                orig += "}";
                std::copy(orig.begin(), orig.end(), out);
            }

            return out;
        }

    };
}

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(UsdUsdatFileFormatTokens, USD_USDAT_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdUsdatFileFormat, SdfTextFileFormat);
}

UsdUsdatFileFormat::UsdUsdatFileFormat()
    : SdfTextFileFormat(UsdUsdatFileFormatTokens->Id,
                        UsdUsdatFileFormatTokens->Version,
                        UsdUsdatFileFormatTokens->Target)
{
    // Do Nothing.
}

UsdUsdatFileFormat::~UsdUsdatFileFormat()
{
    // Do Nothing.
}

bool
UsdUsdatFileFormat::CanRead(const std::string& filePath) const {
    auto extension = TfGetExtension(filePath);
    if (extension.empty()) {
        return false;
    }

    if (extension != this->GetFormatId()) {
        return false;
    }

    return SdfTextFileFormat::CanRead(filePath);
}

bool
UsdUsdatFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const
{
    TRACE_FUNCTION();

    std::FILE* fp = ArchOpenFile(resolvedPath.c_str(), "rb");
    if (!fp) {
        return false;
    }

    // Read file into a string
    std::string fileText;
    std::fseek(fp, 0, SEEK_END);
    fileText.resize(std::ftell(fp));
    std::rewind(fp);
    std::fread(&fileText[0], 1, fileText.size(), fp);
    std::fclose(fp);

    return ReadFromString(layer, fileText);
}

bool
UsdUsdatFileFormat::ReadFromString(
    SdfLayer* layer,
    const std::string& str) const
{
    pa_replace formatter;
    formatter.lut = layer->GetFileFormatArguments();

    sregex templateRegex = "${" >> (s1 = +_w) >> "}";
    const std::string replaced = regex_replace(str, templateRegex, formatter);

    return SdfTextFileFormat::ReadFromString(layer, replaced);
}

PXR_NAMESPACE_CLOSE_SCOPE

//
// Copyright 2019 DreamWorks Animation
//

