# H A V O C _ S T U D I O . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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

# Frame the helicopter before adding a floor that extends beyond its bounds.
units mm
Z
draw havoc
autoview
ae 35 20
zoom 1.1
saveview -e art -o havoc.png [file join $::env(ART_DEMO_OUTPUT_DIR) havoc-camera.sh]
center 14100 0 1900
size 6500
ae 35 12
saveview -e art -o canopy.png [file join $::env(ART_DEMO_OUTPUT_DIR) havoc-detail-camera.sh]

# The ground sits just below the original model's lowest wheel point.
in art.floor.s rpp -30000 40000 -30000 30000 -1100 -900
r art.floor.r u art.floor.s
source studio_materials.tcl
art_demo::make_material art.floor.material as_plastic {
    in_color {color 0.12 0.13 0.15}
    in_specular_weight {float 0.3}
    in_specular_roughness {float 0.35}
} art.floor.r
comb art.demo.g u havoc u art.floor.r
puts {art havoc studio ready}
