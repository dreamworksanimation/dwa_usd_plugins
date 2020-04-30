set(PXR_PACKAGE zprender)

pxr_static_library(${PXR_PACKAGE}
    LIBRARIES
        Fuser
        ${NUKE_DDIMAGE_LIBRARY}

    INCLUDE_DIRS
        ${NUKE_INCLUDE_DIRS}

    PUBLIC_HEADERS
        api.h
        Bvh.h
        ColorMapKnob.h
        ConeVolume.h
        Disc.h
        LightShader.h
        Mesh.h
        Plane.h
        Points.h
        RayCamera.h
        RayPerspectiveCamera.h
        RayShaderContext.h
        RayShader.h
        RaySphericalCamera.h
        RenderContext.h
        RenderPrimitive.h
        Sampling.h
        Scene.h
        SphereVolume.h
        SurfaceHandler.h
        SurfaceShaderOp.h
        ThreadContext.h
        Traceable.h
        Volume.h
        VolumeShader.h
        VolumeShaderOp.h


    CPPFILES
        ColorMapKnob.cpp
        LightShader.cpp
        Mesh.cpp
        Points.cpp
        RayCamera.cpp
        RayShader.cpp
        RenderContext.cpp
        RenderPrimitive.cpp
        Scene.cpp
        SurfaceShaderOp.cpp
        VolumeShader.cpp
        VolumeShaderOp.cpp
)
