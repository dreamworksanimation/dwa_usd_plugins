set(PXR_PACKAGE zpRender)

pxr_plugin(${PXR_PACKAGE}
    LIBRARIES
        zprender
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    CPPFILES
        AxisManipulator.cpp
        zpRender.cpp
        zpManipulator.cpp
        zpTracer.cpp
)
