import nuke

nodebar = nuke.menu("Nodes")

#==============================================================================
# Add Fuser & zpRender items to 3D menu:

tmenu = nodebar.findItem("3D")
if tmenu is not None:
    gmenu = nodebar.findItem("Geometry")
    if gmenu is not None:
        gmenu.addCommand("ViewGeoAttributes", "nuke.createNode('ViewGeoAttributes')")

    #lmenu = gmenu.findItem("Lights")

    #mmenu = gmenu.findItem("Modify")

    smenu = tmenu.findItem("Shader")
    if smenu is not None:
        zmenu = smenu.addMenu("zpRender")
        if zmenu is not None:
            zmenu.addCommand("zpBaseMaterial",   "nuke.createNode('zpBaseMaterial')"  )
            zmenu.addCommand("zpCutout",         "nuke.createNode('zpCutout')"        )
            zmenu.addCommand("zpOcclusion",      "nuke.createNode('zpOcclusion')"     )
            zmenu.addCommand("zpProject",        "nuke.createNode('zpProject')"       )
            zmenu.addCommand("zpSurfaceOptions", "nuke.createNode('zpSurfaceOptions')")
            zmenu.addCommand("zpSurfaceModify",  "nuke.createNode('zpSurfaceModify')" )

    tmenu.addCommand("Stereo Camera", "nuke.createNode('StereoCam2')", icon="Camera.png")

    zmenu = tmenu.addMenu("zpRender")
    if zmenu is not None:
        zmenu.addCommand("zpRender", "nuke.createNode('zpRender')")
