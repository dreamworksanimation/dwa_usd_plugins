set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE usdat)

pxr_plugin(usdat
    LIBRARIES
        sdf
        ar
        plug
        tf
        trace
        usd

    INCLUDE_DIRS
        ${Boost_INCLUDE_DIRS}

    PUBLIC_CLASSES
        usdatFileFormat

    RESOURCE_FILES
        plugInfo.json
)

