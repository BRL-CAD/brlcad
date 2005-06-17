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
