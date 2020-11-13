#
# Copyright 2020 DreamWorks Animation
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#

#
# Fuser SceneLoader plugin Python support code
#

import nuke
import nukescripts

#
# This is a standin for what the sceneloader_support.sceneBrowser() method
# should do when implementing a custom scene browser popup window.
#
# The method is intended for custom scene browser GUIs to be used rather
# than the clunky built-in one that's currently in the SceneLoader node
# interfaces (AxisOp2, CameraOp2, LightOp2, etc.)
#
# Basic steps that the sceneBrowser() method should perform:
#   1) Spawn a scene browser UI window.
#   2) Use the provided scene file path to open and access scene file contents. The browser
#      code is expected to handle whether a USD, FBX, Alembic, etc scene file is being
#      loaded, usually by testing the file extension which is what's used by the Fuser
#      file handling layer to find the I/O plugin to load. At the moment it's not
#      possible for the SceneLoader interface to forward its internal info to this Python
#      method as it only has access to the node knobs.
#   3) Display the scene contents in a cool GUI (otherwise what's the point...?)
#   4) If the browser is intended for object selection let the user select only a single
#      object node path, as SceneLoader does not support multiple objects.
#   5) Return the scene object node path as a non-empty string which will be placed in the
#      'scene_node' knob by the code calling the sceneBrowser() method.
#
# The SceneLoader Nuke node is passed in so that this method can do something else beside
# simply return a scene node path. The default behavior of the code calling the
# sceneBrowser() method is to place the returned string into the 'scene_node' knob, however
# if the returned string is empty ('') then the 'scene_node' knob is left unaffected so this
# the sceneBrowser() code can manipulate any node knobs and not affect 'scene_node'. For
# example if the browser is only intended for viewing the scene and not actually selecting
# anything.
#
def sceneBrowser(node=None, path=''):
    if not node:
        node = nuke.thisNode()
    if path == '':
        return ''

    browsePath = nuke.getClipname(default=path, prompt='Unimplemented Scene Browser')
    if (browsePath != None):
        return browsePath

    return ""

#
# Copyright 2020 DreamWorks Animation
#
