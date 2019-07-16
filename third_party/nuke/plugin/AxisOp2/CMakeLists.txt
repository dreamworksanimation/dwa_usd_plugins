set(PXR_PACKAGE Axis2)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        AxisOp2.cpp
)
