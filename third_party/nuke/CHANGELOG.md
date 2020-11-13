# Change Log

##---------------------------------------------------------------------------------
## Nov 13, 2020

- Fixed crashes with motionblurred, animating meshes
- Fixed crashes in zpRender initialization

### USD plugin (ReadGeo, Axis, Camera, Light):
- usdReader now supports the activation and loading of inactive prims using prim path expressions. Intended for cases when a USD prim is in the scene for utility use and should not end up rendering into the main scene by default.

### zpRender plugin:
- Many refinements and fixes to new shader system. Getting there but still needs work.
- Light shader system is working again with support for legacy lighting calls. Atm only point lights and rect (card) light shaders have been added.
- Volume atmosphere shading support is partially back.


### TODOs:
- Support USD exporting via WriteGeo node.
- 'SimpleSubdiv' plugin is not yet released. This is a simple uniform catmull-clark subdivider that can be used if OpenSubdiv is unavailable.
- UsdMesh GeomSubsets are not yet supported
- UsdPointInstancer is still not supported
- Add more Light shaders - Spot, Cylinder, Env, etc.
- Continue refining atmosphere volume rendering


#### Known issues:
- When using the replacement AxisOp2/CameraOp2/LightOp2 nodes **you will lose FBX support** as there are no Fuser IO plugins provided for this file format.


##---------------------------------------------------------------------------------
## August 20, 2020

### USD plugin (ReadGeo, Axis, Camera, Light):
- Scenegraph instancing is now supported. While Nuke reads instanced scenegraph prims they are still copied locally and do not remain instanced in the Nuke geometry system since it has no instancing support. (Issue 6)
- Added time offset and scale controls to geometry and scene object loaders.
- Added initial support for zpRender Shader plugins. These shaders can be created by the USD readers or by dedicated Nuke Nodes meaning scenes with UsdPreviewSurface shaders will automatically be translate to work-in-progress Fuser Shader nodes attached to Fuser primitives. If zpRender detects these shaders they will be translated to zpRender shader and used for rendering. Since the Fuser Shader nodes are part of the geometry primitive flowing down the geometry graph they can be manipulated or overriden using GeoOps, although only the ApplyMaterial node can do this at the moment.
- Fixed & cleaned up ReadGeo controls, disabling non-functional ones and making sure enabled knobs execute the correct actions. Some of these controls were left over from the ported Alembic reader and were not functional or their names were not intuitive.  (Issue #10)

### zpRender plugin:
- Completely new shader plugin system added
- Initial support for shader interconnections that pass typed attribute data between shaders
- Added backwards-compatible Nuke Material Ops which instantiate the new shaders rather than running their own embedded shader code. This allows the same shaders to be used by scene loaders and Material Ops.


### TODOs:
- Support USD exporting via WriteGeo node.
- 'SimpleSubdiv' plugin is not yet released. This is a simple uniform catmull-clark subdivider that can be used if OpenSubdiv is unavailable.
- UsdMesh GeomSubsets are not yet supported
- UsdPointInstancer is still not supported
- Finish Light Scene node implementation
- Finish Look input functionality


#### Known issues:
- When using the replacement AxisOp2/CameraOp2/LightOp2 nodes **you will lose FBX support** as there are no Fuser IO plugins provided for this file format.

##---------------------------------------------------------------------------------
## July 8, 2020

- Preliminary support for Materials & Shaders added to Fuser lib with corresponding initial support in FuserUsd and zpRender
- Fixed parent/local xform derivation (Issue #9)
- Fixed a crash when enabling local matrix knob on AxisOp2/CameraOp2/LightOp2 nodes


##---------------------------------------------------------------------------------
## May 13, 2020

### USD plugin (ReadGeo, Axis, Camera, Light):
- Added Alembic file support. This is implemented as file aliasing so the USD lib takes care of translating Alembic data into USD data through its usdAbc plugin, which is then imported into Nuke. Note that FBX file support is still missing.
- Added support for attribute (primvar) name mapping in ReadGeo on the 'AttributeMap' tab. The default mappings translate primvar names 'st', 'st_0' or 'uv' to Nuke's 'uv' attribute. (Issue #4)
- Added support for subdivision surfaces using the OpenSubdiv library. The ReadGeo subd controls 'import level' and 'render level' now function if the 'fsrOpenSubdiv' plugin is built and in the NUKE_PATH. The alternate 'SimpleSubdiv' plugin which does not rely on OpenSubdiv is not yet released.
- Added initial support for Usd Materials and Shaders. They import as Fuser Material and Shader child nodes of the FuserMesh in the Nuke geometry system and render via zpRender but are not currently viewable in OpenGL.
- Fixed reading of point-scope (vertex-scope in USD parlance) primvars into Meshes.
- Fixed reading of UV(st) primvars into Meshes. (Issue #4)
- Fixed reading of USD paths that contain colons (Issue #5)
- Fixed (partially) the lack of Look input support on Scene nodes like Axis and Camera. The look connection works and the aim rotations are calculated but additional features like 'look strength' and xyz-only rotations are only partially working. (Issue #7)

### zpRender plugin (initial release)
- 'zpRender' is a rendering plugin for Nuke that provides high-quality utilitarian ray-trace rendering to complement and/or replace the ScanlineRender plugin.
- zpRender consists of two primary parts:
    1. An api/library `lib/zprender` providing the scene and shader apis
    2. the zpRender plugin itself
- Optionally there are several companion shading plugins that add additional functionality, like ambient occlusion and a simple 'base' shader, with more to come.
- As zpRender is utilitarian in nature it is not intended for 'production' rendering and at DreamWorks Animation is primarily used by matte-painting, lighting and image-finaling departments to enhance and supplement our production renders.
- It's being released with the USD nuke toolkit as it's a good complement for the USD functionality, and the zprender lib is based on the same Fuser lib that's already been released with the USD Nuke toolkit.

### TODOs:
- Support USD exporting via WriteGeo node.
- 'SimpleSubdiv' plugin is not yet released. This is a simple uniform catmull-clark subdivider that can be used if OpenSubdiv is unavailable.
- UsdMesh GeomSubsets are not yet supported
- Add frame/view controls to scene import functions which will allow xforms, meshes, etc to be locked or offset to a specific frame and view.
- Finish Light Scene node implementation
- Finish Look input functionality


#### Known issues:
- This second major release of the code is beta-grade production code, ie it's used in active production but still unfinished with many TODOs yet to complete.
- While the APIs should not change significantly the feature set and several plugins are works in progress
- It works fine on Nuke 10, 11 and 12
- At the moment it has only been built on Linux64 and there likely needs to be (hopefully small) changes to get it building on Win/OSX.
- When using the replacement AxisOp2/CameraOp2/LightOp2 nodes **you will lose FBX support** as there are no Fuser IO plugins provided for this file format.


##---------------------------------------------------------------------------------
## Jul 16, 2019

Initial release

#### Known issues:
- This first release of the code is beta-grade internal-development code and **should not be relied on for production work**!
- This code is very much a work in progress, here's a number of unfinished code stubs in the release and many TODOs to complete.
- While the API itself should not change significantly the feature set and several plugins are incomplete
- It works fine on Nuke 10 & 11 and should work on the upcoming Nuke 12
- At the moment it has only been built on Linux64 and there likely needs to be (hopefully small) changes to get it building on Win/OSX.
- When using the replacement AxisOp2/CameraOp2/LightOp2 nodes you will **lose Alembic and FBX support** as there are no Fuser IO plugins provided for these file formats. Internally at DWA we have an Alembic Fuser I/O plugin but are not releasing it through this project initially.
- Only import functionality is currently supported, exporting is a TODO.
