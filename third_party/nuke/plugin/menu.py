import nuke

nodebar = nuke.menu("Nodes")

#==============================================================================
# Add Fuser & zpRender items to 3D menu:

menu = nodebar.findItem("3D")
if menu is not None:
    menu.addCommand("Stereo Camera", "nuke.createNode('StereoCam2')", icon="Camera.png")

    gmenu = menu.findItem("Geometry")
    if gmenu is not None:
        gmenu.addCommand("ViewGeoAttributes", "nuke.createNode('ViewGeoAttributes')", icon="Modify.png")

    #ltmenu = menu.findItem("Lights")

    #modmenu = menu.findItem("Modify")
    #if modmenu is not None:
    #    modmenu.addCommand("TransformGeo", "nuke.createNode('TransformGeo2')", icon="Modify.png")

    shdmenu = menu.findItem("Shader")
    if shdmenu is not None:
        zpmenu = shdmenu.addMenu("zpRender")
        if zpmenu is not None:
            zpmenu.addCommand("zpBaseMaterial",   "nuke.createNode('zpBaseMaterial')"  )
            zpmenu.addCommand("zpCutout",         "nuke.createNode('zpCutout')"        )
            zpmenu.addCommand("zpOcclusion",      "nuke.createNode('zpOcclusion')"     )
            zpmenu.addCommand("zpProject",        "nuke.createNode('zpProject')"       )
            zpmenu.addCommand("zpSurfaceOptions", "nuke.createNode('zpSurfaceOptions')")
            zpmenu.addCommand("zpSurfaceModify",  "nuke.createNode('zpSurfaceModify')" )

    zpmenu = menu.addMenu("zpRender")
    if zpmenu is not None:
        zpmenu.addCommand("zpRender", "nuke.createNode('zpRender')")
