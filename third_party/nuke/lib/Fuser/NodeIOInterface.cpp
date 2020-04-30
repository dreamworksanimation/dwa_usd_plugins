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

/// @file Fuser/NodeIOInterface.cpp
///
/// @author Jonathan Egstad

#include "NodeIOInterface.h"
#include "ArgSet.h" // for empty_string

#include <DDImage/Thread.h> // for Lock

#include <algorithm> // for std::transform
#include <locale>    // for std::tolower

//#include <fstream>  // for pin

namespace Fsr {


// Map of explicit extension-to-plugin mappings. ex. 'usd'->'UsdIO', 'usda'->'UsdIO'.
static KeyValueMap m_extensions_map;
static DD::Image::Lock lock;


/*!
*/
NodeIOInterface::NodeIOInterface()
{
    //
}


/*! Extract the filename extension from the file path which may
    be at the front like 'ext:foo' or at the end like 'foo.ext'.

    Returns the trimmed path when there's a leading extension,
    or just src_path if not. ie 'ext:foo' will return 'foo'.

    Leading extension text up to the colon must not contain any
    path symbols like '/\.' otherwise it's ignored.

    If the 'ext' var pointer is provided this will contain any
    found extension string.
*/
/*static*/
std::string
NodeIOInterface::getTrimmedPath(const char*  src_path,
                                std::string* ext)
{
    if (!src_path || !src_path[0])
        return empty_string;

    std::string trimmed_path;
    bool have_path_chars = false;
    const char* e = NULL;
    for (const char* s=src_path; *s; ++s)
    {
        if (*s==':' && !have_path_chars)
        {
            // ext at front:
            if (s <= src_path)
                return empty_string;
            trimmed_path = (s+1);
            if (ext != NULL)
                *ext = s;
            return trimmed_path;
        }
        else if (*s=='.')
        {
            e = (s + 1); // ext at end
            have_path_chars = true;
        }
        else if (*s=='/' || *s=='\\')
        {
            e = NULL; // reset
            have_path_chars = true;
        }
    }
    trimmed_path = src_path;
    if (ext != NULL)
        *ext = (e != NULL)?e:"";

    return trimmed_path;
}


/*! Add explicit extension mappings used to map files to plugin names.

    Each mapping can be in the form 'n=s' or 'n,m,o=s', such as
    'abc=AbcIO' or 'usd,usda,usdc=UsdIO'

    All mappings are converted to lower case so there's no difference
    between 'Usda' and 'usda'.
*/
void
NodeIOInterface::addExtensionMappings(const char* mappings_list)
{
    if (!mappings_list || !mappings_list[0])
        return;

    std::vector<std::string> mappings;
    Fsr::stringSplit(mappings_list, " \t\n", mappings);

    const size_t nMappings = mappings.size();
    for (size_t j=0; j < nMappings; ++j)
    {
        const std::string& mapping = mappings[j];
        if (mapping.size() == 0)
            continue; // shouldn't happen...
        //std::cout << j << ": parse mapping '" << mapping << "'" << std::endl;

        // Split at '=':
        size_t a = mapping.find_first_of('=', 0);
        if (a == 0 || a == std::string::npos)
            continue; // need at least two params

        // Plugin name:
        std::string plugin_name(mapping.substr(a+1, std::string::npos));

        // Add list of extensions to map:
        std::vector<std::string> extensions;
        Fsr::stringSplit(mapping.substr(0, a), ",", extensions);
        const size_t nExtensions = extensions.size();
        lock.lock();
        for (size_t i=0; i < nExtensions; ++i)
        {
            std::string& ext = extensions[i];
            //std::cout << "  " << i << "'" << ext << "'" << std::endl;
            if (ext.size() > 0)
            {
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // to lower-case
                m_extensions_map[ext] = plugin_name;
            }
        }
        lock.unlock();
    }

}


/*! Extract the file path and extension to use for file I/O and plugin loading,
    updating file_path and plugin_type.

    'plugin_type' is the base plugin type ('UsdIO', 'AbcIO', etc) used to look for
    the I/O plugin to load for the file type, built from the camel-cased file
    extension with 'plugin_class' appended to the end.

    For loading Fuser::Node plugins the plugin_type string is further prepended
    with 'fsr' to form the final plugin name 'fsrExtIO'.

    This is virtual so subclasses can completely change the logic.
*/
/*virtual*/ void
NodeIOInterface::_buildFilePathAndPluginType(const char*  path,
                                             const char*  plugin_class,
                                             std::string& file_path,
                                             std::string& plugin_type)
{
#if 1
    plugin_type.clear();

    std::string ext;
    file_path = getTrimmedPath(path, &ext);
#else
    file_path.clear();
    plugin_type.clear();

    std::string ext = getExtension(path, &file_path);
#endif
    if (ext.empty())
    {
        // TODO: figure out how to traverse all the Fuser IO plugins
        // searching for an appropriate one to load this file.
        // For now we bail...
        return;
    }

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // to lower-case

    // Is the extension in the extensions map?
    lock.lock();
    KeyValueMap::const_iterator it = m_extensions_map.find(ext);
    if (it != m_extensions_map.end())
        plugin_type = it->second; // in map, get plugin name
    else
    {
        // Not in map, convert the extension 'foo' to 'FooIO':
        plugin_type.reserve(128);
        plugin_type += (char)::toupper(ext[0]); // Camel-case it
        plugin_type += (ext.data()+1);
        if (plugin_class && plugin_class[0])
            plugin_type += plugin_class;
        m_extensions_map[ext] = plugin_type;
    }
    lock.unlock();
}


} // namespace Fsr


// end of Fuser/NodeIOInterface.cpp


//
// Copyright 2019 DreamWorks Animation
//
