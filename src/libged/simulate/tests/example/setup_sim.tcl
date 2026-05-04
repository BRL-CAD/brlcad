#                   S E T U P _ S I M . T C L
# BRL-CAD
#
# Published in 2026 by the United States Government.
# This work is in the public domain.
#
#
###
# setup_sim.tcl - Create the truck-on-terrain simulation database
# 
# Run from share/db directory (so terra.bin is accessible):
#   cd /path/to/brlcad-build/share/db
#   mged -c output.g < setup_sim.tcl
#
# Geometry facts (all in mm):
#   Truck (m35 component) bounding box:
#     min: {-1827, -1250, 0}  max: {4892, 1250, 2618}
#     center X ~ 1532, center Y = 0
#     nose (+X, front) = 4892 mm, rear (-X) = -1827 mm
#   Terrain center: X=12800000, Y=12800000
#   Terrain center elevation: ~1237625 mm
#   Truck placed 500m (500000 mm) above terrain center elevation
#
# Initial truck placement:
#   The truck is pitched 20 degrees nose-down (rotation around world Y-axis),
#   so that the front wheels (+X end) hit the terrain before the rear wheels.
#   After 20 deg rotation:
#     Front bottom approx. world Z = 1735952 mm (~498 m above terrain)
#     Rear bottom approx. world Z  = 1738250 mm (~501 m above terrain)
#   Translation: Tx=12798468, Ty=12800000, Tz=1737625

dbconcat terra.g terra_
dbconcat m35.g m35_

# Static terrain body (mass=0 means it won't move)
comb terrain.sim u terra_ground.r
attr set terrain.sim simulate::type region
attr set terrain.sim simulate::mass 0.0
attr set terrain.sim simulate::roi_proxy 1

# Dynamic truck body (2000 kg)
comb truck.sim u m35_component
attr set truck.sim simulate::type region
attr set truck.sim simulate::mass 2000.0

# Scene combination with gravity
comb scene.c u terrain.sim u truck.sim
attr set scene.c simulate::gravity {<0,0,-9.80665>}

# Position truck above terrain center with a 20-degree pitch rotation.
# Rotation: +20 deg around Y axis (right-hand rule: nose/+X side tilts down,
# rear/-X side tilts up) so front wheels hit terrain first.
# Combined rotation + translation matrix (row-major 4x4):
#   cos20  0  sin20  Tx
#     0    1    0    Ty
#  -sin20  0  cos20  Tz
#     0    0    0     1
# where cos20=0.93969262, sin20=0.34202014
# Tx=12798468, Ty=12800000, Tz=1737625
arced scene.c/truck.sim matrix rarc 0.93969262 0 0.34202014 12798468 0 1 0 12800000 -0.34202014 0 0.93969262 1737625 0 0 0 1
