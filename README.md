# DreamWorks Animation USD Plugins

This repository contains plugins for USD, and plugins for 
third-party software to support a USD pipeline. These plugins are
developed and maintained by [DreamWorks Animation](https://www.dreamworks.com).

#### Hydra integration for Houdini

Enables drawing packed prims from Pixar's USD Import in the viewer using Hydra,
which is significantly faster for drawing and playback. Same code as this PR:

https://github.com/PixarAnimationStudios/USD/pull/723

[Documentation](third_party/houdini/plugin/Hydra/README.md)

#### USD Reader plugins for Nuke

A suite of plugins that can import USD geometry, cameras, and lights
into Nuke. This is currently a beta release.

[Documentation](third_party/nuke/README.md)

# License

This code is released under the 
[Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0), 
which is a free, open-source, and detailed software license developed and maintained 
by the Apache Software Foundation.

# Installing

This repository is somewhat unusual in that the install process 
requires merging its contents with the USD repository first, and then
using USD's build_usd.py to build and install the plugins. We provide a small
utility to streamline the merge process. This structure allows us to use
Pixar's CMake utilities in our CMakeLists.txt files, and while not ideal,
is hopefully sufficient until a proper solution for delivering 
USD plugins is devised.
   
## Requirements

USD-19.7 (other versions may work but will require a manual merge process)

Houdini-16.5 (for Hydra integration)

Nuke-10 or Nuke-11 (for Nuke plugins)

## Install process

Run merge_plugins.py and supply a path to your local USD repository,
and specify which plugins you would like to merge as arguments.

```bash
python merge_plugins.py /path/to/USD (--nuke) (--houdini_hydra)
```

This will merge and / or copy the relevant files from this repository to
the USD repository. If this results in merge conflicts (if you have modified 
your local USD repo), you will need to resolve them manually.

Once this is complete, you can use USD's build_usd.py from the USD repository
location to build and install these plugins.

#### Hydra integration for Houdini
```bash
python build_scripts/build_usd.py --houdini --houdini-location /path/to/houdini16.5
```

#### USD Reader plugins for Nuke
```bash
python build_scripts/build_usd.py --nuke --nuke-location /path/to/nuke10_or_11
```

