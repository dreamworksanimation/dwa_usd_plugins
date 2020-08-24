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

/// @file Fuser/NodeIOInterface.h
///
/// @author Jonathan Egstad

#ifndef Fuser_NodeIOInterface_h
#define Fuser_NodeIOInterface_h

#include "api.h"

#include <string>

namespace Fsr {

class ArgSet;


/*! \class Fsr::NodeIOInterface

    \brief Interface class adding standardized Fsr::Node file I/O functionality.

*/
class FSR_EXPORT NodeIOInterface
{
  public:
    // Need this anymore? Don't think it's being used anywhere.
    enum Errors
    {
        NO_ERROR = 0,
        EMPTY_FILE_PATH,
        FILE_IS_UNREADABLE,
        CANNOT_LOAD_FILE,
        FILE_INTERNAL_ERROR
    };



  protected:
    //! Add explicit extension mappings in the form 'n=s' or 'n,m,o=s', such as 'abc=AbcIO' or 'usd,usda,usdc=UsdIO'
    void         addExtensionMappings(const char* mappings_list);

    //! Extract the file path and extension to use for file I/O and plugin loading, updating file_path and plugin_type.
    virtual void _buildFilePathAndPluginType(const char*  path,
                                             const char*  plugin_class,
                                             std::string& file_path,
                                             std::string& plugin_type);

    //! Add or modify args to pass to Fsr::Node ctors. Base class does nothing.
    virtual void _appendNodeContextArgs(Fsr::ArgSet& node_args) {}


  public:
    //!
    NodeIOInterface();

    //! Must have a virtual destructor!
    virtual ~NodeIOInterface() {}


  public:
    //! Extract extension string from file path. Look for 'ext:foo' at beginning and 'foo.ext' at end.
    static std::string getTrimmedPath(const char*  src_path,
                                      std::string* ext=NULL);

};


} // namespace Fsr


#endif

// end of Fuser/NodeIOInterface.h


//
// Copyright 2019 DreamWorks Animation
//
