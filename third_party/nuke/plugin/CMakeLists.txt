add_subdirectory(FuserUsd)
add_subdirectory(usdReader)

add_subdirectory(ApplyMaterial)
add_subdirectory(AxisOp2)
add_subdirectory(CameraOp2)
#add_subdirectory(EnvLight)
add_subdirectory(LightOp2)
#add_subdirectory(MergeGeo2)
#add_subdirectory(TransformGeo2)

add_subdirectory(StereoCam2)
add_subdirectory(FuserOpenSubdiv)
add_subdirectory(ViewGeoAttributes)

add_subdirectory(zpBaseMaterial)
add_subdirectory(zpCutout)
add_subdirectory(zpOcclusion)
add_subdirectory(zpProject)
add_subdirectory(zpRectangleLight)
add_subdirectory(zpRender)
add_subdirectory(zpSurfaceModify)
add_subdirectory(zpSurfaceOptions)

install(
    FILES
        menu.py
        sceneloader_support.py
    DESTINATION
        ${PXR_INSTALL_SUBDIR}/plugin
)
