# T W I N _ S T R U C T U R E . T C L
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
art_engineering::make_material frames amber {all 7xxx_series}
art_engineering::make_material skin shell 0xxx_series 0.72
art_engineering::make_material engines orange 1xxx_series
art_engineering::make_material crew steel 2xxx_series 0.85
art_engineering::make_material controls cyan 3xxx_series
art_engineering::make_material fuel purple 4xxx_series
art_engineering::make_material gear green 5xxx_series
art_engineering::make_material electrical yellow 8xxx_series
art_engineering::make_material cockpit steel 9xxx_series
set spars [lsort -unique [lmap path [search /all -name *spar* -or -name *Spar*] {file tail $path}]]
art_engineering::make_material spars blue $spars
art_engineering::make_material tank purple {Left_Wing_Fuel Right_Wing_Fuel Fuel_in_Center_Tank Center_Fuel_Tank} 0.92
art_engineering::make_material floor shell floor 0.85
art_engineering::make_material cases shell {No__1_Engine__Core_Case No__1_Engine__Fan_Case No__2_Engine_Core_Case No__2_Engine__Fan_Case} 0.85
comb art.context.g u all
# Copying the source tree retains member transforms in the skin-off view.
cp all art.structure.g
comb art.structure.g rm 0xxx_series 2xxx_series
Z
draw all
autoview
center -19800 0 8500
size 40000
ae 35 25
saveview -e art -o overview.png [file join $::env(ART_DEMO_OUTPUT_DIR) twin-structure-overview-camera.sh]
center -18000 0 7500
size 26500
ae 35 35
saveview -e art -o detail.png [file join $::env(ART_DEMO_OUTPUT_DIR) twin-structure-detail-camera.sh]
puts {art twin structure ready}
