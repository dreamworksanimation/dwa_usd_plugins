Nuke Plugins
==============

This section contains libraries and plugins for Nuke. 

* Requires the cmake/modules/FindNuke.cmake module which uses NUKE_LOCATION
  defined in build_scripts/build_usd.py.
* cmake/defaults/Options.cmake and cmake/defaults/Packages.cmake also need
  to have Nuke entries to kick off the FindNuke module.


List of added files:
<src-top>/
    third_party/
        nuke/
            CMakeLists.txt
            README.md
            lib/
                CMakeLists.txt
                Fuser/
                    CMakeLists.txt
                    api.h
                    ArgConstants.h
                    ArgSet.cpp
                    ArgSet.h
                    Attribute.cpp
                    Attribute.h
                    AttributeTypes.h
                    AxisKnob.cpp
                    AxisKnob.h
                    AxisOp.cpp
                    AxisOp.h
                    Box2.h
                    Box3.h
                    CameraOp.cpp
                    CameraOp.h
                    CameraRigOp.cpp
                    CameraRigOp.h
                    ExecuteTargetContexts.h
                    GeoReader.cpp
                    GeoReader.h
                    GeoSceneFileArchiveContext.h
                    GeoSceneGraphReader.cpp
                    GeoSceneGraphReader.h
                    HalfEdge.h
                    LightOp.cpp
                    LightOp.h
                    Lookat.h
                    Mat4.h
                    MeshPrimitive.cpp
                    MeshPrimitive.h
                    NodeContext.h
                    Node.cpp
                    Node.h
                    NodeIOInterface.cpp
                    NodeIOInterface.h
                    NodePrimitive.cpp
                    NodePrimitive.h
                    NukeGeoInterface.cpp
                    NukeGeoInterface.h
                    NukeKnobInterface.cpp
                    NukeKnobInterface.h
                    PointBasedPrimitive.cpp
                    PointBasedPrimitive.h
                    Primitive.h
                    RayContext.h
                    SceneLoader.cpp
                    SceneLoader.h
                    SceneOpExtender.h
                    SceneXform.cpp
                    SceneXform.h
                    Time.h
                    Vec2.h
                    Vec3.h
                    Vec4.h
                    XformableNode.cpp
                    XformableNode.h

                    icons/
                        Fuser.png

            plugin/
                CMakeLists.txt
                FuserUsd/
                    CMakeLists.txt
                    fsrUsdaIO.tcl
                    fsrUsdcIO.tcl
                    fsrUsdIO.cpp
                    FuserUsdArchiveIO.cpp
                    FuserUsdArchiveIO.h
                    FuserUsdCamera.cpp
                    FuserUsdCamera.h
                    FuserUsdLight.cpp
                    FuserUsdLight.h
                    FuserUsdMesh.cpp
                    FuserUsdMesh.h
                    FuserUsdNode.cpp
                    FuserUsdNode.h
                    FuserUsdShader.cpp
                    FuserUsdShader.h
                    FuserUsdXform.cpp
                    FuserUsdXform.h

                usdReader/
                    CMakeLists.txt
                    usdaReader.tcl
                    usdcReader.tcl
                    usdReader.cpp

                usdReader/
                    CMakeLists.txt
                    usdaReader.tcl

                AxisOp2/
                    CMakeLists.txt
                    AxisOp2.cpp

                CameraOp2/
                    CMakeLists.txt
                    CameraOp2.cpp

                LightOp2/
                    CMakeLists.txt
                    LightOp2.cpp

                TransformGeo2/
                    CMakeLists.txt
                    TransformGeo2.cpp

                EnvLight/
                    CMakeLists.txt
                    EnvLight.cpp

                StereoCam2/
                    CMakeLists.txt
                    StereoCam2.cpp

                ViewGeoAttributes/
                    CMakeLists.txt
                    ViewGeoAttributes.cpp
