set(PXR_PACKAGE StereoCam2)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        StereoCam2.cpp
)

install(
    FILES
        stereocam2_support.py
    DESTINATION
        ${PXR_INSTALL_SUBDIR}/plugin
)
