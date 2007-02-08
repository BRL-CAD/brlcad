#                      D R A W B S P . P Y
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
###

import Blender
import string
from Blender import NMesh
from string import split

def createNode(min, max):
  bspNode = NMesh.GetRaw()

  ## Create the Cubes 8 verts
  vert = NMesh.Vert(min[0], min[1], min[2]) ## Pt 0
  bspNode.verts.append(vert)
  vert = NMesh.Vert(min[0], max[1], min[2]) ## Pt 1
  bspNode.verts.append(vert)
  vert = NMesh.Vert(max[0], max[1], min[2]) ## Pt 2
  bspNode.verts.append(vert)
  vert = NMesh.Vert(max[0], min[1], min[2]) ## Pt 3
  bspNode.verts.append(vert)
  vert = NMesh.Vert(min[0], min[1], max[2]) ## Pt 4
  bspNode.verts.append(vert)
  vert = NMesh.Vert(min[0], max[1], max[2]) ## Pt 5
  bspNode.verts.append(vert)
  vert = NMesh.Vert(max[0], max[1], max[2]) ## Pt 6
  bspNode.verts.append(vert)
  vert = NMesh.Vert(max[0], min[1], max[2]) ## Pt 7
  bspNode.verts.append(vert)

  ## Face 1
  face = NMesh.Face()
  face.v.append(bspNode.verts[0])
  face.v.append(bspNode.verts[1])
  face.v.append(bspNode.verts[2])
  face.v.append(bspNode.verts[3])
  bspNode.faces.append(face)

  ## Face 2
  face = NMesh.Face()
  face.v.append(bspNode.verts[0])
  face.v.append(bspNode.verts[4])
  face.v.append(bspNode.verts[5])
  face.v.append(bspNode.verts[1])
  bspNode.faces.append(face)

  ## Face 3
  face = NMesh.Face()
  face.v.append(bspNode.verts[0])
  face.v.append(bspNode.verts[4])
  face.v.append(bspNode.verts[7])
  face.v.append(bspNode.verts[3])
  bspNode.faces.append(face)

  ## Face 4
  face = NMesh.Face()
  face.v.append(bspNode.verts[2])
  face.v.append(bspNode.verts[3])
  face.v.append(bspNode.verts[7])
  face.v.append(bspNode.verts[6])
  bspNode.faces.append(face)

  ## Face 5
  face = NMesh.Face()
  face.v.append(bspNode.verts[2])
  face.v.append(bspNode.verts[6])
  face.v.append(bspNode.verts[5])
  face.v.append(bspNode.verts[1])
  bspNode.faces.append(face)

  ## Face 6
  face = NMesh.Face()
  face.v.append(bspNode.verts[4])
  face.v.append(bspNode.verts[5])
  face.v.append(bspNode.verts[6])
  face.v.append(bspNode.verts[7])
  bspNode.faces.append(face)

  NMesh.PutRaw(bspNode)


## Open file, for each line read in 6 floats (3 for min, 3 for max)
## defining a bounding box

fh = open("bspData.txt", "r")
for line in fh.readlines():
  MinMax = split(line, ' ')
  Min = [ float(MinMax[0]), float(MinMax[1]), float(MinMax[2]) ]
  Max = [ float(MinMax[3]), float(MinMax[4]), float(MinMax[5]) ]
  ## print Min
  ## print Max
  createNode(Min, Max)

# Local Variables:
# mode: Python
# tab-width: 8
# c-basic-offset: 4
# python-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
