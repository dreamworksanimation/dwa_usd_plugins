set(PXR_PACKAGE Light2)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        LightOp2.cpp
)
