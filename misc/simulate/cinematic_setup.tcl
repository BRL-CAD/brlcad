# cinematic_setup.tcl - Build simulation database with cinematic materials
#
# Extends the basic setup with:
#   - Earthy brown terrain shader (low shininess, high diffuse)
#   - Olive drab military truck shader
#   - Both shaders propagate via inherit=1 to all child regions
#
# Run from share/db directory (so terra.bin is accessible):
#   cd /path/to/brlcad-build/share/db
#   mged -c output.g < cinematic_setup.tcl
#
# Geometry facts (all in mm):
#   Truck (m35 component) bounding box:
#     min: {-1827, -1250, 0}  max: {4892, 1250, 2618}
#     center X ~ 1532, center Y = 0, height = 2618 mm
#   Terrain center: X=12800000, Y=12800000
#   Terrain center elevation: ~1237625 mm
#
# Camera (used by cinematic_render.py):
#   Fixed world position at:
#     X = 12800000  (terrain center X)
#     Y = 12650000  (150 m south of landing zone)
#     Z = 1240625   (terrain elev + 3000 mm = 3 m above ground)
#   Looks at truck center each frame → naturally shows horizon when
#   truck is at or below ~150 m above terrain (lower half of the fall).
#
# Initial truck placement: 20-degree nose-down pitch, 500 m above terrain.
# (same as setup_sim.tcl)

dbconcat terra.g terra_
dbconcat m35.g m35_

# ----------------------------------------------------------------
# Terrain: static body, earthy-brown ground shader
#   plastic: sh=2 (low specular highlight size), sp=0.05 (5% specular),
#            di=0.95 (95% diffuse) → looks like packed dirt/earth
#   RGB(95, 75, 50) = warm brown dirt
#   inherit=1 so all child regions use this material
# ----------------------------------------------------------------
comb terrain.sim u terra_ground.r
attr set terrain.sim simulate::type region
attr set terrain.sim simulate::mass 0.0
attr set terrain.sim simulate::roi_proxy 1
mater terrain.sim "plastic {sh 2 sp .05 di .95}" 95 75 50 1

# ----------------------------------------------------------------
# Truck: dynamic body, olive-drab military paint shader
#   plastic: sh=12 (moderate highlight), sp=0.25 (25% specular),
#            di=0.75 (75% diffuse) → painted sheet metal
#   RGB(56, 72, 30) = US Army olive drab (close to FS 34087)
#   inherit=1 so all child regions use this material
# ----------------------------------------------------------------
comb truck.sim u m35_component
attr set truck.sim simulate::type region
attr set truck.sim simulate::mass 2000.0
mater truck.sim "plastic {sh 12 sp .25 di .75}" 56 72 30 1

# Scene combination with gravity
comb scene.c u terrain.sim u truck.sim
attr set scene.c simulate::gravity {<0,0,-9.80665>}

# Position truck: 20-degree nose-down pitch, 500 m above terrain center.
# Matrix: cos20=0.93969262, sin20=0.34202014
# Tx=12798468, Ty=12800000, Tz=1737625
arced scene.c/truck.sim matrix rarc 0.93969262 0 0.34202014 12798468 0 1 0 12800000 -0.34202014 0 0.93969262 1737625 0 0 0 1
