################
## REQUIRES PYTHON 2.3 RIGHT NOW
## CHANGE THESE TO WHERE YOUR PYTHON 2.3 IS LOCATED
import sys
sys.path.append('/usr/local/lib/python2.3')
sys.path.append('/usr/local/lib/python2.3/lib-dynload')
################

import commands
import Blender
import math
import struct
import os
from Blender import BGL, Draw, NMesh, Object, Camera, Lamp, Scene, Types
from math import log
from struct import *
#################
meshVal = 1
materVal = 1
frameVal = 1
sceneVal = 1
aniVal = 0
rppVal = 2
rppBtn = 0
layers = []
layerBase = 40
i = 0
while i < 20:
    layers.append(1)
    i += 1
layerBits = 0
layerBtns = []
propertiesList = []
projectName = "proj"
projectButton = 0
#################


def event(evt, val):
  if evt == Draw.ESCKEY:
    Draw.Exit()
    return
  else:
    return
  Draw.Register(drawGUI, event, buttonEvent)


def NMeshDump(of, obj, mesh):
  global	propertiesList

  myName = obj.getName()

  if not mesh.materials:
    ## Write length of Name
    of.write(pack('b', len(myName)))
    of.write(myName)
    of.write(pack('b', 0))
  else:
    matName = mesh.materials[0].getName()
    ## Write length of Name
    of.write(pack('b', len(myName)))
    of.write(myName)
    of.write(pack('b', len(matName)))
    of.write(matName)

    found = -1
    try:
      found = propertiesList.index(matName)
    except ValueError:
      pass


  ## Write Number of Triangles
  num = 0
  for f in mesh.faces:
    if len(f.v) == 4:
      num = num + 2
    if len(f.v) == 3:
      num = num + 1
  of.write(pack('i', num))

  ## Write Triangles
  for f in mesh.faces:
    if len(f.v) >= 3:
      if f.smooth:
        smooth = chr(1)
      else:
        smooth = chr(0)
      of.write(pack('c', smooth))
      of.write(pack('fffffffff', mesh.verts[f.v[0].index].co[0], mesh.verts[f.v[0].index].co[1], mesh.verts[f.v[0].index].co[2],
                                 mesh.verts[f.v[1].index].co[0], mesh.verts[f.v[1].index].co[1], mesh.verts[f.v[1].index].co[2],
                                 mesh.verts[f.v[2].index].co[0], mesh.verts[f.v[2].index].co[1], mesh.verts[f.v[2].index].co[2]))

      ## Smooth Normals
      if f.smooth:
        of.write(pack('fffffffff', mesh.verts[f.v[0].index].no[0], mesh.verts[f.v[0].index].no[1], mesh.verts[f.v[0].index].no[2],
                                   mesh.verts[f.v[1].index].no[0], mesh.verts[f.v[1].index].no[1], mesh.verts[f.v[1].index].no[2],
                                   mesh.verts[f.v[2].index].no[0], mesh.verts[f.v[2].index].no[1], mesh.verts[f.v[2].index].no[2]))

      if len(f.v) == 4:
        of.write(pack('c', smooth))
        of.write(pack('fffffffff', mesh.verts[f.v[0].index].co[0], mesh.verts[f.v[0].index].co[1], mesh.verts[f.v[0].index].co[2],
                                   mesh.verts[f.v[2].index].co[0], mesh.verts[f.v[2].index].co[1], mesh.verts[f.v[2].index].co[2],
                                   mesh.verts[f.v[3].index].co[0], mesh.verts[f.v[3].index].co[1], mesh.verts[f.v[3].index].co[2]))
        if f.smooth:
          of.write(pack('fffffffff', mesh.verts[f.v[0].index].no[0], mesh.verts[f.v[0].index].no[1], mesh.verts[f.v[0].index].no[2],
                                     mesh.verts[f.v[2].index].no[0], mesh.verts[f.v[2].index].no[1], mesh.verts[f.v[2].index].no[2],
                                     mesh.verts[f.v[3].index].no[0], mesh.verts[f.v[3].index].no[1], mesh.verts[f.v[3].index].no[2]))



def dumpMeshes():
  global exportStatus, layerBits
  objList = Blender.Object.Get()

  of = open(projectName + "/mesh.db", "w")
  ## Endian
  of.write(pack('h', 1))


  objCount = 0
  for obj in objList:
      if (obj.Layer & layerBits) == 0:
          continue
      if type(obj.getData()) == Types.NMeshType:
          objCount += 1

  i = 0
  for obj in objList:
      if (obj.Layer & layerBits) == 0:
          continue
      data = obj.getData()
      if type(data) == Types.NMeshType:
          exportStatus = "Mesh %d of %d %s" % \
                         (i, objCount, obj.getName())
          print exportStatus
          Draw.Draw()
          NMeshDump(of, obj, data)
          i += 1


def dumpProperties():
  propertiesFileName = projectName + "/properties.db"
  print propertiesFileName
  propertiesFile = open(propertiesFileName, "w")

  objCount = 0
  objCount = len(Blender.Material.Get())

  i = 0
  for m in Blender.Material.Get():
    myName = m.getName()
    exportStatus = "Properties for %s (%d of %d)" % (myName, i, objCount)
    print exportStatus

    propertiesFile.write("properties,%s\n" % (myName))

    propertiesFile.write("color,%f,%f,%f\n" % \
            (m.rgbCol[0], m.rgbCol[1], m.rgbCol[2]))

    i += 1
  propertiesFile.close()


def dumpFrames():
#  global animVal
#  print "dumpFrames"
  cur = Blender.Get('curframe')
  frameFileName = projectName + "/frame.db"
  frameFile = open(frameFileName, "w")

  if aniVal == 0:
    dumpOneFrame(frameFile, cur)
  else:
    sta = Blender.Get('staframe')
    end = Blender.Get('endframe')
    for frame in range (sta, end+1):
      Blender.Set('curframe', frame)
      Blender.Window.RedrawAll()
      dumpOneFrame(frameFile, frame)
  frameFile.close()


def dumpCamera(of, obj, cam):
  focus = [0, 0, 0]

  of.write("camera")

  loc = obj.matrix[3]
  look = obj.matrix[2]
  upvec = obj.matrix[1]

  # Generate Focus/Look Vector
  focus[0] = loc[0] - look[0]
  focus[1] = loc[1] - look[1]
  focus[2] = loc[2] - look[2]

  of.write(",%f,%f,%f" % (loc[0], loc[1], loc[2])) # position
  of.write(",%f,%f,%f" % (focus[0], focus[1], focus[2])) # focus
  of.write(",0.0") # tilt


  # Vertical FoV = 2 * atan(height/(2*lens_mm)) * pi / 180
  # Horiz FoV = 2 * atan(width/(2*lens_mm)) * pi / 180
  #
  # simulate a 35mm camera w 24x36mm image plane
  # Blender uses the image width for this calcuation

  FoV = math.atan(35.0 / (2.0 * cam.getLens())) * 180 / math.pi * 0.75

  of.write(",%f,%f\n" % (FoV, 0.0))


def dumpOneFrame(of, n):
  print "exporting frame: %d" % n
  of.write("frame,%d\n" % n)

  objList = Blender.Object.Get()
  m_identity = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]

  for obj in objList:
    if (obj.Layer & layerBits) == 0:
      continue
    data = obj.getData()
    if type(data) == Types.CameraType:
#      exportStatus = "Camera"
#      Draw.Draw()
      dumpCamera(of, obj, data)
    if type(data) == Types.NMeshType:
      objName = obj.getName()
#      exportStatus = "Transform Mesh %s" % objName
#      print exportStatus
#      Draw.Draw()
      m = obj.matrix

      same = 1
      for y in range(4):
        for x in range(4):
          if m[x][y] != m_identity[x][y]:
            same = 0

      if same != 1:
        of.write("transform_mesh,%s" % objName)
        of.write(",%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n" % (m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3], m[3][0], m[3][1], m[3][2], m[3][3]))



def mat_mul(b, a):
  m = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
  mat_print("    a", a)
  mat_print("    b", b)
  for j in range(4):
    for i in range(4):
      m[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + \
                a[i][2] * b[2][j] + a[i][3] * b[3][j]
  mat_print("    m", m)
  return m


def mat_print(s, m):
  print s
  print "\t%f %f %f %f" % (m[0][0], m[0][1], m[0][2], m[0][3])
  print "\t%f %f %f %f" % (m[1][0], m[1][1], m[1][2], m[1][3])
  print "\t%f %f %f %f" % (m[2][0], m[2][1], m[2][2], m[2][3])
  print "\t%f %f %f %f" % (m[3][0], m[3][1], m[3][2], m[3][3])


def doExport():
  global	layerBits

  print "Exporting...\n"

  ## Built Layer Bit Mask
  i = 0
  layerBits = 0
  while i < 20:
    if layers[i] != 0:
      layerBits |= (1 << i)
    i += 1

  if sceneVal == 1:
    ## Make Project Directory
    commands.getoutput("mkdir %s" % projectName)

    envFileName = projectName + "/env.db"
    print envFileName
    envFile = open(envFileName, "w")

    envFile.write("image_size,%d,%d\n" % (640,480))
    envFile.write("rendering_method,normal\n")

    envFile.close();

    textureFileName = projectName + "/texture.db"
    print textureFileName
    textureFile = open(textureFileName, "w")
    textureFile.close();


  if meshVal == 1:
    ## Remove and Create all active layer folders
    dumpMeshes()
#      exportStatus = "Meshes"
#      Draw.Draw()


  if materVal == 1:
#    exportStatus = "Properties"
#    Draw.Draw()
    dumpProperties()

  if frameVal == 1:
    exportStatus = "Frames"
    Draw.Draw()
    dumpFrames()


def buttonEvent(evt):
  global	aniVal, rppVal, meshVal, materVal, frameVal, sceneVal, rppBtn, layerBtns, layers
  global	rppBtn, layerBtns, layers, projectName, projectButton


  if evt == 8:
    projectName = projectButton.val
  if evt == 1:
    doExport()
  if evt == 2:
    print "Rendering...\n"
  elif evt == 4:
    aniVal = 1 - aniVal
  elif evt == 5:
    rppVal = rppBtn.val
  elif evt == 6:
    for i in range(20):
      layers[i] = 1
      layerBtns[i].val = 1
  elif evt == 7:
    for i in range(20):
      layers[i] = 0
      layerBtns[i].val = 0
  elif evt == 10:
    sceneVal = 1 - sceneVal
  elif evt == 11:
    meshVal = 1 - meshVal
  elif evt == 12:
    materVal = 1 - materVal
  elif evt == 13:
    frameVal = 1 - frameVal
  elif evt >= layerBase and evt < layerBase + 20:
    layerNumber = evt - layerBase
#    print "layer %d" % (layerNumber)
    layers[layerNumber] = 1 - layers[layerNumber]

  Draw.Redraw(1)


def drawGUI():
  global	rppBtn, layers, layerBtns, projectName, projectButton

  ## Set background color
  BGL.glClearColor(0.7, 0.7, 0.6, 1)
  BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
  ## Set text color
  BGL.glColor3f(0, 0, 0)

  xPad = 10
  height = 18
  yLoc = xPad + 10
  yDelta = height


  binWidth = Draw.GetStringWidth("Binary Format", 'normal') + 30
  aniWidth = Draw.GetStringWidth("Save All Frames", 'normal') + 30
  expWidth = Draw.GetStringWidth("Export", 'normal') + 40

  Draw.Button("Export", 1, xPad, yLoc, expWidth, 2*height, "Export Scene to Disk")

  projectButton = Draw.String("Project Name:", 8, expWidth + xPad, yLoc,
                              binWidth + aniWidth, height, projectName, 32, "Project Name")

  yLoc += yDelta

  Draw.Toggle("Save All Frames", 4, expWidth + binWidth + xPad, yLoc, aniWidth, height, aniVal, "Select Binary Format")

  yLoc += yDelta

  Draw.Button("Render", 2, xPad, yLoc, expWidth, height, "Render exported scene")
  rppBtn = Draw.Number("RPP:", 5, expWidth + xPad, yLoc, binWidth + aniWidth, height, rppVal, 1, 8, "Number of rays (squared) per pixel")

  yLoc += yDelta + 1
  xVal = 61
  Draw.Toggle("Scene", 10, xPad + 0 * xVal, yLoc, xVal, height, sceneVal)
  Draw.Toggle("Meshes", 11, xPad + 1 * xVal, yLoc, xVal, height, meshVal)
  Draw.Toggle("Materials", 12, xPad + 2 * xVal, yLoc, xVal, height, materVal)
  Draw.Toggle("Frames", 13, xPad + 3 * xVal, yLoc, xVal, height, frameVal)

  yLoc += yDelta + 1

  # Draw the layer buttons
  layerBtn = 0 # start with the bottom row
  btnSize = height

  for row in range(2):
    ypos = yLoc + (1-row) * btnSize
    xpos = xPad
    for col in range(10):
      layerBtns.append(Draw.Toggle(" ",  layerBase + layerBtn, xpos, ypos, btnSize, btnSize, layers[layerBtn]))
      if (col+1) % 5:
        xpos += btnSize
      else:
        xpos += btnSize * 2
      layerBtn += 1

  Draw.Button("All", 6, 12*btnSize + xPad, yLoc, btnSize*5, height, "Deselect all Layers")
  Draw.Button("None", 7, 12*btnSize + xPad, yLoc+btnSize, btnSize*5, height, "Select all Layers")


##########################################
## Begin Event Handling
##########################################
Draw.Register(drawGUI, event, buttonEvent)
##########################################
