set(PXR_PACKAGE zpCutout)

pxr_plugin(zprCutout}
    LIBRARIES
        zprender
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        zprCutout.cpp
)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        zprender
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        zprCutout.cpp
        zpCutout.cpp
)
