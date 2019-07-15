#
# Copyright 2019 DreamWorks Animation
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#

# Find an installed Nuke Development Kit (NDK)
#
# Variables that will be defined:
# NUKE_INCLUDE_DIRS     Path to the NDK include directory
# NUKE_LIB_DIRS         Path to the NDK library directory
# NUKE_DDIMAGE_LIBRARY
# NUKE_VERSION          Full Nuke version, 10.5.7 for example
# NUKE_MAJOR_VERSION
# NUKE_MINOR_VERSION
# NUKE_RELEASE_VERSION

#message("********************************************************************************")
#message("*********************        BUILDING NUKE PLUGIN          *********************")
#message("********************************************************************************")

find_path(NUKE_INCLUDE_DIRS 
    DDImage/DDImage_API.h
    HINTS
        "${NUKE_LOCATION}"
        "$ENV{NUKE_LOCATION}"        
        "${NUKE_BASE_DIR}"
    PATH_SUFFIXES
        include/
    DOC
        "Nuke NDK Header Path"
)

find_library(NUKE_DDIMAGE_LIBRARY
    NAMES
        DDImage
    PATH_SUFFIXES
        .
    HINTS
        "${NUKE_LOCATION}"
        "$ENV{NUKE_LOCATION}"        
        "${NUKE_BASE_DIR}"
    )

#set(NUKE_VERSION         ${NUKE_FULL_VERSION})
#set(NUKE_MAJOR_VERSION   ${NUKE_FULL_VERSION})
#set(NUKE_MINOR_VERSION   ${NUKE_FULL_VERSION})
#set(NUKE_RELEASE_VERSION ${NUKE_FULL_VERSION})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Nuke
    REQUIRED_VARS
        NUKE_INCLUDE_DIRS
        NUKE_DDIMAGE_LIBRARY
)
#    VERSION_VAR
#        NUKE_VERSION
