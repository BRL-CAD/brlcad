# M 3 5 _ S T U D I O . T C L
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
# These existing glass regions are not referenced by component or all.g.
# Include them in both versions so only the material treatment changes.
set glazing {r682 r683 r684 r685}
Z
draw component {*}$glazing
autoview
ae 35 20
# Frame the actual truck rather than its larger Boolean operand bounds.
center 1532 0 1309
size 8000
saveview -e art -o m35.png [file join $::env(ART_DEMO_OUTPUT_DIR) m35-camera.sh]
center 3100 0 1550
size 4200
ae 35 12
saveview -e art -o cab.png [file join $::env(ART_DEMO_OUTPUT_DIR) m35-detail-camera.sh]

in art.floor.s rpp -30000 40000 -30000 30000 -100 -10
r art.floor.r u art.floor.s
source studio_materials.tcl
art_demo::make_material art.floor.material as_plastic {
    in_color {color 0.08 0.09 0.10}
    in_specular_weight {float 0.3}
    in_specular_roughness {float 0.35}
} art.floor.r
set members [list u component]
foreach window $glazing {
    lappend members u $window
}
comb art.demo.g {*}$members u art.floor.r
puts {art m35 studio ready}
