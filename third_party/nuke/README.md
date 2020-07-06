Nuke Plugins
==============

This section contains libraries and plugins to help Nuke load scene files like USD, Alembic, FBX, etc.

The approach to this project was to be as holistic as possible and allow scene file support throughout Nuke's node set by leveraging an abstract I/O plugin architecture. This architecture allows all Nuke node types to be extended, where it makes sense, to import or export data to and from scene files. For example while a Camera scene node is an obvious case for importing camera data, a node that performs defocus/bokeh image processing may want to import camera data directly from a scene file.

Additional Info
----------------

* [CHANGELOG](CHANGELOG.md)

Files
------------------------

The current project code takes the form of a utility library called 'Fuser', a rendering library 'zprender', a plugin specifically for Usd I/O, a set of plugins for extending/replacing several stock Nuke nodes, and several custom plugins not in stock Nuke.

- `lib/Fuser` - helps abstract and wrap Nuke API quirkiness and provides a plugin architecture to share USD I/O support across multiple plugins without those plugins needing to be built or linked specifically against scene file support libraries. libFuser can be used to produce I/O plugins for Alembic, FBX or any custom scene file format and is only dependent on Nuke's DDImage lib and standard system libraries for maximum interchangeability. ie no boost, tbb, etc.

- `lib/zprender` - provides the 'zpr' ray-trace rendering api that zpRender, zpOcclusion, etc plugins are built on top of.

- `plugin/FuserUsd` (fsrUsdIO) - provides USD I/O functionality and is discovered and loaded by libFuser using the standard Nuke plugin subsystem so it can live in the same directories as other Nuke plugins. Any Fuser Node can automatically load its own named plugin, and the `fsr<ext>IO` naming convention is a specialization to simplify finding an I/O plugin by file extension.

- `plugin/usdReader` - provides access to the Fuser I/O USD plugin with a standardized scene file interface. Geometry reading is currently handled through the stock ReadGeo/GeoReader plugin interfaces so there's limitations to what can be done in the UI and with the global transform (which is disabled.)

- `plugin/AxisOp2`, `plugin/CameraOp2`, `plugin/LightOp2` - these replace the stock Nuke scene nodes which do not support additional scene file formats beyond Alembic/FBX. These replacements are functional equivalents providing standardized FuserIO interfaces. Currently only the basic Axis, Camera, Light nodes are replaced, with other scene nodes in progress.

- `plugin/StereoCam` - this custom CameraOp supports typical stereo camera rig functionality but in a single node.

- `plugin/ViewGeoAttributes` - a simple debugging tool to introspect the primitives flowing down the geometry tree. This would be really improved with a custom Qt gui.

- `plugin/FuserOpenSubdiv` - Fuser plugin providing subdivision functionality from the OpenSubdiv library.

- `plugin/TransformGeo2` - a replacement for TransformGeo (unfinished).

- `plugin/zpRender` - ray-trace renderer alternative for ScanlineRender or RayRender. Supports ray-tracing shaders, subdivision surfaces, motionblur, etc.

- `plugin/zpBaseMaterial` - Ray-trace lighting shader providing some simple functionality - not a GI shader!

- `plugin/zpCutout` - Ray-trace shader allowing an object to be marked as a cutout

- `plugin/zpOcclusion` - Ray-trace shader providing simple ambient and reflection occlusion

- `plugin/zpProject` - Ray-trace replacement for Project material node.

- `plugin/zpSurfaceModify` - Ray-trace shader allows modifications of the shading context.

- `plugin/zpSurfaceOptions` - Ray state control shader
