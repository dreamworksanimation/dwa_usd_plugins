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
        InputBinding.h
        Mesh.h
        Plane.h
        Points.h
        RayCamera.h
        RayCylindricalCamera.h
        RayMaterial.h
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
        SurfaceMaterialOp.h
        Texture2dSampler.h
        ThreadContext.h
        Traceable.h
        Volume.h
        VolumeShader.h
        VolumeMaterialOp.h
        zprAttributeReader.h
        zprIopUVTexture.h
        zprPreviewSurface.h
        zprReadUVTexture.h


    CPPFILES
        ColorMapKnob.cpp
        InputBinding.cpp
        LightShader.cpp
        Mesh.cpp
        Points.cpp
        RayCamera.cpp
        RayMaterial.cpp
        RayShader.cpp
        RenderContext.cpp
        RenderPrimitive.cpp
        Scene.cpp
        SurfaceMaterialOp.cpp
        Texture2dSampler.cpp
        VolumeShader.cpp
        VolumeMaterialOp.cpp
        zprAttributeReader.cpp
        zprIopUVTexture.cpp
        zprReadUVTexture.cpp
        zprPreviewSurface.cpp
)
