#
# Copyright 2019 DreamWorks Animation
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
# StereoCam plugin Python support code
#

import nuke
import nukescripts

#
# Copy a stereo camera to a new non-stereo, non-animated 'projector' camera.
#
def copyToProjector(node=None, frame=None, gui=nuke.GUI):
    if not node:
        node = nuke.thisNode()
    if not frame:
        frame = nuke.root()['frame'].value()

    if node.Class() not in ['StereoCam', 'StereoCam2', 'Camera', 'Camera2']:
        m = 'this node is not a supported camera type, unable to convert to projector'
        if gui:
            nuke.message(m)
        nuke.error(m)
        return

    saved_frame = nuke.root()['frame'].value()
    nuke.root()['frame'].setValue(frame)

    # Turn off all selected nodes, otherwise they mess up the node paste:
    for n in nuke.selectedNodes():
        n.knob('selected').setValue(False)

    # Now select this node then copy and paste it:
    node['selected'].setValue(True)
    nukescripts.node_copypaste()
    proj = nuke.selectedNode()

    # Name/label new node:
    new_name = 'projector_cam'
    if node.knob('shot') is not None:
        if node['shot'].getText() != '':
            new_name += '_%s_' % proj['shot'].getText().replace('.', '_')
    new_name += 'fr%d_' % nuke.frame()
    # De-duplicate the new name:
    counter = 1
    while 1:
        new_name2 = new_name + '%d' % counter
        if nuke.toNode(new_name2) is None:
            new_name = new_name2
            break
        counter += 1
    proj['name'].setValue(new_name)

    l = proj['label'].getText()
    if l != '' and not l.endswith('\\n'):
        l += '\\n'
    l += 'frame %d' % nuke.frame()
    proj['label'].setValue(l)

    # Offset its position in the DAG:
    xpos = node['xpos'].value() ; proj['xpos'].setValue(xpos+100)
    ypos = node['ypos'].value() ; proj['ypos'].setValue(ypos+100)

    # Unsplit all knobs (remove views):
    vs = nuke.views()
    if len(vs) > 1:
        for name, knob in proj.knobs().items():
            if issubclass(knob.__class__, nuke.Array_Knob):
                #print 'knob %s: unsplitting view %s' % (knob.name(), vs[1])
                knob.unsplitView(view=vs[1])

    # Clear animations from all knobs:
    for name, knob in proj.knobs().items():
        if knob.isAnimated():
            knob.clearAnimated()

    # Disable updating:
    if proj.knob('read_from_file') is not None:
        proj['read_from_file'].setValue(False)

    nuke.root()['frame'].setValue(saved_frame)


#
# Copyright 2019 DreamWorks Animation
#
