# T W I N _ S T U D I O . T C L
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

units mm
# The source windshield is a centered plate.  Refraction needs explicit entry
# and exit boundaries, so convert a copy without simplifying its original faces.
if {[get 21.1.t0 mode] ne "plate" || [regexp {[1-9a-fA-F]} [get 21.1.t0 fm]]} {
    error "Expected Generic_Twin's centered plate windscreen"
}
bot extrude --max-area-delta 0 -i 21.1.t0
if {[get 21.1.t0 mode] ne "volume" || [get 21.1.t0 orient] ne "rh" ||
    [bot check solid 21.1.t0] ne "1"} {
    error "The converted windscreen is not an oriented closed volume"
}
Z
draw all
autoview
ae 35 20
center -19800 0 8500
size 40000
saveview -e art -o twin.png [file join $::env(ART_DEMO_OUTPUT_DIR) twin-camera.sh]
center -4300 0 6200
size 8500
ae 35 15
saveview -e art -o cockpit.png [file join $::env(ART_DEMO_OUTPUT_DIR) twin-detail-camera.sh]

# Keep the model's authored position and put the floor below its lowest wheel.
in art.floor.s rpp -120000 80000 -100000 100000 2400 2535
r art.floor.r u art.floor.s
source studio_materials.tcl
art_demo::make_material art.floor.material as_plastic {
    in_color {color 0.08 0.09 0.10}
    in_specular_weight {float 0.3}
    in_specular_roughness {float 0.35}
} art.floor.r
comb art.demo.g u all u art.floor.r
puts {art twin studio ready}
