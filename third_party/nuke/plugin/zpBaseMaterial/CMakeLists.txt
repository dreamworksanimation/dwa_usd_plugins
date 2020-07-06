set(PXR_PACKAGE zpBaseMaterial)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        zprender
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        zprBase.cpp
        zpBaseMaterial.cpp
)
