#                      P T C L O U D . P Y
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation.
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



## Open file, for each line read in 6 floats (3 for min, 3 for max)
## defining a bounding box

mesh = NMesh.GetRaw()
fh = open("/home/justin/ADRT/adrt/scripts/pts.txt", "r")
for line in fh.readlines():
  pt = split(line, ' ')
  pt = [ float(pt[0]), float(pt[1]), float(pt[2]) ]
  vert = NMesh.Vert(pt[0], pt[1], pt[2]) ## Pt 0
  mesh.verts.append(vert)
NMesh.PutRaw(mesh)

# Local Variables:
# mode: Python
# tab-width: 8
# c-basic-offset: 4
# python-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
