#                         A D R T . P Y
# BRL-CAD / ADRT
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#  Justin Shumaker
#
################
## REQUIRES PYTHON 2.3 RIGHT NOW
## CHANGE THESE TO WHERE YOUR PYTHON 2.2 IS LOCATED
#import sys
#sys.path.append('/usr/local/lib/python2.2')
#sys.path.append('/usr/local/lib/python2.2/lib-dynload')
################

import commands
import Blender
import math
import os
import struct
from struct import pack
from Blender import BGL, Draw, NMesh, Object, Camera, Lamp, Scene, Types
from math import log


framework_name = "scene"
layer_mask = 3
export_all_frames = 0
framework_button = 0

def export_meshes():
  fh = open(framework_name + ".adrt", "wb")

  ## Endian
  fh.write(pack('h', 1))

  ## Version
  fh.write(pack('h', 2))

  ## Calculate total number of triangles
  obj_list = Blender.Object.Get()

  num = 0
  for obj in obj_list:
    if (obj.Layer & layer_mask) == obj.Layer:
      if(type(obj.getData()) == Types.NMeshType):
        for f in obj.getData().faces:
          if len(f.v) == 4:
            num = num + 2
          if len(f.v) == 3:
            num = num + 1

  fh.write(pack('i', num))
  print "\nWriting %d triangles..." % num

  ## Pack each mesh
  for obj in obj_list:
    if (obj.Layer & layer_mask) == obj.Layer:
      if(type(obj.getData()) == Types.NMeshType):
        ## Mesh Name Length
        fh.write(pack('B', len(obj.getName())+1))
        ## Mesh Name
        fh.write(obj.getName())
        fh.write(pack('B', 0))
        ## Vertice Total
        fh.write(pack('I', len(obj.getData().verts)))
        ## Write Vertices
        for v in obj.getData().verts:
          fh.write(pack('fff', v.co[0], v.co[1], v.co[2]))
        ## Write Faces
        num = 0
        for f in obj.getData().faces:
          if len(f.v) == 4:
            num = num + 2
          if len(f.v) == 3:
            num = num + 1

        if(len(obj.getData().faces) < 1<<16):
          fh.write(pack('B', 0))
          fh.write(pack('H', num))
          for f in obj.getData().faces:
            if len(f.v) >= 3:
              fh.write(pack('HHH', f.v[0].index, f.v[1].index, f.v[2].index))
            if len(f.v) == 4:
              fh.write(pack('HHH', f.v[0].index, f.v[2].index, f.v[3].index))
        else:
          fh.write(pack('B', 1))
          fh.write(pack('I', num))
          for f in obj.getData().faces:
            if len(f.v) >= 3:
              fh.write(pack('III', f.v[0].index, f.v[1].index, f.v[2].index))
            if len(f.v) == 4:
              fh.write(pack('III', f.v[0].index, f.v[2].index, f.v[3].index))



  ## close the file handle
  fh.close()


def export_properties():
  fh = open(framework_name + ".properties", "w")

  print "Writing Properties..."

  ## default property
  fh.write("properties,%s\n" % "default")
  fh.write("color,%f,%f,%f\n" % (0.8, 0.8, 0.8))
  fh.write("gloss,%f\n" % (0.2))
  fh.write("emission,%f\n" % (0.0))

  for m in Blender.Material.Get():
    fh.write("properties,%s\n" % m.getName())
    fh.write("color,%f,%f,%f\n" % (m.rgbCol[0], m.rgbCol[1], m.rgbCol[2]))
    if(m.getEmit() > 0.0):
      fh.write("emission,%f\n" % (m.getEmit()))
  fh.close()


def export_textures():
  fh = open(framework_name + ".textures", "w")

  print "Writing Textures..."
  fh.close()


def export_mesh_map():
  fh = open(framework_name + ".map", "wb")

  print "Writing Mesh Map..."
  obj_list = Blender.Object.Get()
  for obj in obj_list:
    if (obj.Layer & layer_mask) == obj.Layer:
      if(type(obj.getData()) == Types.NMeshType):
        if(len(obj.getData().materials)) > 0:
          fh.write(pack('B', len(obj.getName())+1))
          fh.write(obj.getName());
          fh.write(pack('B', 0))

          fh.write(pack('B', len(obj.getData().materials[0].getName())+1))
          fh.write(obj.getData().materials[0].getName());
          fh.write(pack('B', 0))
        else:
          fh.write(pack('B', len(obj.getName())+1))
          fh.write(obj.getName());
          fh.write(pack('B', 0))

          fh.write(pack('B', len("default")+1))
          fh.write("default");
          fh.write(pack('B', 0))



  fh.close()


def write_camera(fh, obj):
  focus = [0, 0, 0]

  fh.write("camera")

  loc = obj.matrix[3]
  look = obj.matrix[2]
  upvec = obj.matrix[1]

  # Generate Focus/Look Vector
  focus[0] = loc[0] - look[0]
  focus[1] = loc[1] - look[1]
  focus[2] = loc[2] - look[2]

  fh.write(",%f,%f,%f" % (loc[0], loc[1], loc[2])) # position
  fh.write(",%f,%f,%f" % (focus[0], focus[1], focus[2])) # focus
  fh.write(",0.0") # tilt


  # Vertical FoV = 2 * atan(height/(2*lens_mm)) * pi / 180
  # Horiz FoV = 2 * atan(width/(2*lens_mm)) * pi / 180
  #
  # simulate a 35mm camera w 24x36mm image plane
  # Blender uses the image width for this calcuation

  FoV = math.atan(35.0 / (2.0 * obj.getData().getLens())) * 180 / math.pi * 0.75

  fh.write(",%f,%f\n" % (FoV, 0.0))


def write_frame(fh, frame):
  print "Writing frame: %d" % frame
  fh.write("frame,%d\n" % frame)

  m_identity = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]

  obj_list = Blender.Object.Get()
  for obj in obj_list:
    if(obj.Layer & layer_mask) == obj.Layer:
      if type(obj.getData()) == Types.CameraType:
        write_camera(fh, obj)

    if type(obj.getData()) == Types.NMeshType:
      obj_name = obj.getName()
      m = obj.matrix

      same = 1
      for y in range(4):
        for x in range(4):
          if m[x][y] != m_identity[x][y]:
            same = 0

      if same == 0:
        fh.write("transform_mesh,%s" % obj_name)
        fh.write(",%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n" % \
                (m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], \
                 m[2][0], m[2][1], m[2][2], m[2][3], m[3][0], m[3][1], m[3][2], m[3][3]))


def export_frames():
  global export_all_frames

  fh = open(framework_name + ".frames", "w")

  cur = Blender.Get('curframe')

  if export_all_frames == 0:
    write_frame(fh, cur)
  else:
    sta = Blender.Get('staframe')
    end = Blender.Get('endframe')
    for frame in range (sta, end+1):
      Blender.Set('curframe', frame)
      Blender.Window.RedrawAll()
      write_frame(fh, frame)


  fh.close()


def export_environment():
  fh = open(framework_name + ".env", "w")
  fh.write("geometry_file," + framework_name + ".adrt\n")
  fh.write("properties_file," + framework_name + ".properties\n")
  fh.write("textures_file," + framework_name + ".textures\n")
  fh.write("mesh_map_file," + framework_name + ".map\n")
  fh.write("frames_file," + framework_name + ".frames\n")
  fh.write("image_size,512,384,128,128\n")
  fh.write("rendering_method,normal\n")
  fh.close()


def event(evt, val):
  if evt == Draw.ESCKEY:
    Draw.Exit()
    return
  else:
    return
  Draw.Register(draw_gui, event, buttonEvent)


def button_event(evt):
  global framework_button, framework_name, layer_mask, export_all_frames

  ## Export ADRT Framework
  if evt == 1:
    ## export mesh data
    export_meshes()

    ## export properties data
    export_properties()

    ## export textures data
    export_textures()

    ## export mesh map
    export_mesh_map()

    ## export frame data
    export_frames()

    ## export an environment file
    export_environment()
    print "Complete."

  if evt == 2:
    framework_name = framework_button.val

  if evt == 3:
    layer_mask = 0xfffff

  if evt == 4:
    layer_mask = 0

  if evt == 5:
    export_all_frames ^= 1

  if evt >= 32:
    layer_mask ^= 1<<(evt - 32)

  Draw.Redraw(1)


def draw_gui():
  global framework_button, framework_name, layer_mask, export_all_frames

  ##########
  Draw.Button("Export ADRT Framework", 1, 0, 0, 160, 20, "Export Scene")
  framework_button = Draw.String("Framework Name:", 2, 160, 0, 240, 20, framework_name, 64, "Framework Name")
  ##########


  ##########
  Draw.Button("Select All", 3, 0, 60, 160, 20, "Deselect all Layers")
  Draw.Button("Select None", 4, 0, 40, 160, 20, "Select all Layers")

  layer_id = 0
  for row in range(2):
    for col in range(10):
      Draw.Toggle(" ", 32+layer_id, 160+col*20, 60-row*20, 20, 20, layer_mask & 1<<layer_id, "")
      layer_id += 1
  ##########

  ##########
  Draw.Toggle("Export All Frames", 5, 0, 100, 160, 20, export_all_frames, "")
  ##########


############################################
## Begin Event Handling
############################################
Draw.Register(draw_gui, event, button_event)
############################################

# Local Variables:
# mode: Python
# tab-width: 8
# c-basic-offset: 4
# python-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
