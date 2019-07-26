Nuke Plugins
==============

This section contains libraries and plugins to help Nuke load scene files like USD, Alembic, FBX, etc.

The approach to this project was to be as holistic as possible and allow scene file support throughout Nuke's node set by leveraging an abstract I/O plugin architecture. This architecture allows all Nuke node types to be extended, where it makes sense, to import or export data to and from scene files. For example while a Camera scene node is an obvious case for importing camera data, a node that performs defocus/bokeh image processing may want to import camera data directly from a scene file.

The current project code takes the form of a utility library called 'Fuser', a plugin specifically for Usd I/O, a set of plugins for extending/replacing several stock Nuke nodes, and several custom plugins not in stock Nuke.

* `lib/Fuser` - helps abstract and wrap Nuke API quirkiness and provides a plugin architecture to share USD I/O support across multiple plugins without those plugins needing to be built or linked specifically against scene file support libraries. libFuser can be used to produce I/O plugins for Alembic, FBX or any custom scene file format and is only dependent on Nuke's DDImage lib and standard system libraries for maximum interchangeability. ie no boost, tbb, etc.

* `plugin/FuserUsd` (fsrUsdIO) - provides USD I/O functionality and is discovered and loaded by libFuser using the standard Nuke plugin subsystem so it can live in the same directories as other Nuke plugins. Any Fuser Node can automatically load its own named plugin, and the `fsr<ext>IO` naming convention is a specialization to simplify finding an I/O plugin by file extension.

* `plugin/usdReader` - provides the interface to the Fuser I/O USD plugin and provides a standardized scene file interface. Geometry reading is currently handled through the stock ReadGeo/GeoReader interface. 

* `plugin/AxisOp2`, `plugin/CameraOp2`, `plugin/LightOp2` - replace the stock Nuke scene nodes which do not support additional scene file formats beyond Alembic/FBX. These replacements are functional equivalents providing standardized FuserIO interfaces. Currently only the basic Axis, Camera, Light nodes are replaced, with other scene nodes in progress.

* `plugin/StereoCam` - this custom CameraOp supports typical stereo camera rig functionality but in a single node.

* `plugin/ViewGeoAttributes` - a simple debugging tool to introspect the primitives flowing down the geometry tree. This would be really improved with a custom Qt gui.

* `plugin/TransformGeo2` - an unfinished replacement for TransformGeo.


Caveats:

* This first release of the code is beta-grade internal-development code and **should not be relied on for production work**!
* This code is very much a work in progress, here's a number of unfinished code stubs in the release and many TODOs to complete.
* While the API itself should not change significantly the feature set and several plugins are incomplete
* It works fine on Nuke 10 & 11 and should work on the upcoming Nuke 12
* At the moment it has only been built on Linux64 and there likely needs to be (hopefully small) changes to get it building on Win/OSX.
* When using the replacement AxisOp2/CameraOp2/LightOp2 nodes you will **lose Alembic and FBX support** as there are no Fuser IO plugins provided for these file formats. Internally at DWA we have an Alembic Fuser I/O plugin but are not releasing it through this project initially.
* Only import functionality is currently supported, exporting is a TODO.
