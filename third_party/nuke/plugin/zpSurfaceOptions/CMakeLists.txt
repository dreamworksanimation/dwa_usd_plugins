set(PXR_PACKAGE zpSurfaceOptions)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        zprender
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        zpSurfaceOptions.cpp
)
