set(PXR_PACKAGE ViewGeoAttributes)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        ViewGeoAttributes.cpp
)
