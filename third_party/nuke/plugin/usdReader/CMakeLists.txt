set(PXR_PACKAGE usdReader)

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
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        usdReader.cpp

)

install(
    FILES
        usdaReader.tcl
        usdcReader.tcl
        usdzReader.tcl
    DESTINATION
        ${PXR_INSTALL_SUBDIR}/plugin
)
