set(PXR_PACKAGE fsrUsdIO)

# compiler flags needed for building.
#  silence deprecated warning from usd /pxr/base/tf/hashset.h: '-Wno-deprecated'
#  silence 'unused' warnings from boost: '-Wno-unused-local-typedefs'
if (UNIX)
    add_definitions(
        -Wno-cast-qual
        -Wno-extra
        -Wno-reorder
        -Wno-deprecated
        -Wno-unused-local-typedefs
    )
endif()

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        #
        arch
        gf
        js
        plug
        tf
        trace
        vt
        work
        #
        ar
        kind
        pcp
        sdf
        usd
        usdGeom
        usdLux
        usdShade
        usdUtils
        #
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        fsrUsdIO.cpp
        FuserUsdArchiveIO.cpp
        FuserUsdCamera.cpp
        FuserUsdLight.cpp
        FuserUsdMesh.cpp
        FuserUsdNode.cpp
        FuserUsdShader.cpp
        FuserUsdXform.cpp
        FuserUsdXformableNode.cpp
)

install(
    FILES
        fsrUsdaIO.tcl
        fsrUsdcIO.tcl
        fsrUsdzIO.tcl
        fsrAbcIO.tcl
    DESTINATION
        ${PXR_INSTALL_SUBDIR}/plugin
)
