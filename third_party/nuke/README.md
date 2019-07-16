Nuke Plugins
==============

This section contains libraries and plugins to help Nuke load scene files like USD, Alembic, FBX, etc, and an IO plugin for USD.


This first release of the code is beta internal-development code and shouldn't be relied on for production work. It works fine on Nuke 10 & 11 and should work on Nuke 12.

At the moment it's only been built on Linux 64 and there likely needs to be many (hopefully small) changes to get it building on Win/OSX.


   docs are work-in-progress - add Fuser lib explanation here


List of currently-added files:
src-top/
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

                StereoCam2/
                    CMakeLists.txt
                    StereoCam2.cpp

                ViewGeoAttributes/
                    CMakeLists.txt
                    ViewGeoAttributes.cpp
