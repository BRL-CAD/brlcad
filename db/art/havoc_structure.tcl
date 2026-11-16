# H A V O C _ S T R U C T U R E . T C L
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
source engineering_materials.tcl
art_engineering::make_material context steel {havoc havoc_middle havoc_front}
art_engineering::make_material rotor amber {main_rotor tail_rotor}
art_engineering::make_material gear green landing_gear
art_engineering::make_material weapons orange weapons
art_engineering::make_material blades graphite {mr_blade1 mr_blade101 mr_blade201 mr_blade301 mr_blade401 tail_rot_blades}
art_engineering::make_material sensors purple {front_antennae radar_warn_ant tail_antennae laser_sight}
set controls {}
foreach path [search /main_rotor -type comb] {
    set name [file tail $path]
    if {[regexp {^mr_(phorn|plnk|plnkpin|damp|dampbot)} $name]} {lappend controls $name}
}
lappend controls tail_rot_links tail_rot_xhead
art_engineering::make_material links cyan [lsort -unique $controls]
art_engineering::make_material covers shell {mr_cover mr_lcover cannopy_glass laser_sight_glss lt_pylon_skin rt_pylon_skin lt_launch_case rt_launch_case 30mm_ammo_box 30mm_turret} 0.75
comb art.context.g u havoc
# Keep the source placements while isolating the rotor-head context.
cp havoc art.structure.g
comb art.structure.g rm havoc_tail landing_gear weapons
Z
draw havoc
autoview
ae 35 20
zoom 1.1
saveview -e art -o overview.png [file join $::env(ART_DEMO_OUTPUT_DIR) havoc-structure-overview-camera.sh]
center 11830 0 2900
size 4300
ae 35 25
saveview -e art -o detail.png [file join $::env(ART_DEMO_OUTPUT_DIR) havoc-structure-detail-camera.sh]
puts {art havoc structure ready}
