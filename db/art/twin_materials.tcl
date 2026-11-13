# T W I N _ M A T E R I A L S . T C L
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

source studio_materials.tcl
art_demo::make_material art.airframe.material as_plastic {
    in_color {color 0.65 0.7 0.75}
    in_specular_weight {float 0.9}
    in_specular_roughness {float 0.22}
} all
set blue_parts {Vertical_Tail Port_Wing__Upper_Surface Starboard_Wing__Upper_Surface
    Port_Wing__Lower_Surface Starboard_Wing__Lower_Surface
    Port_Lower_FWD_Fuselage_Skin Starboard_Lower_FWD_Fuselage_Skin
    Port_FWD_Lower_Skin Starboard_FWD_Lower_Skin}
lappend blue_parts {*}[lmap path [search /all -name *Engine*Skin*] {file tail $path}]
art_demo::make_material art.blue.material as_plastic {
    in_color {color 0.015 0.04 0.12}
    in_specular_weight {float 1}
    in_specular_roughness {float 0.18}
} [lsort -unique $blue_parts]
art_demo::make_material art.glazing.material as_glass {
    in_ior {float 1.5}
    in_roughness {float 0}
    in_transmittance_amount {float 1}
    in_surface_transmittance {color 1 1 1}
    in_volume_transmittance {color 0.8 0.9 1}
    in_volume_transmittance_distance {float 1000}
} Wind_Screen
art_demo::make_material art.rubber.material as_plastic {
    in_color {color 0.02 0.025 0.03}
    in_specular_weight {float 0.25}
    in_specular_roughness {float 0.4}
} {LMLG___Inboard_Tire LMLG___Outboard_Tire RMLG___Inboard_Tire RMLG___Outboard_Tire
    Nose_Gear___Left_Tire Nose_Gear___Right_Tire}
art_demo::make_material art.metal.material as_metal {
    in_face_reflectance {color 0.75 0.8 0.85}
    in_roughness {float 0.18}
    in_energy_compensation {float 0}
} {LMLG___Inboard_Tire_Rim LMLG___Outboard_Tire__Rim
    RMLG___Inboard_Tire_Rim RMLG___Outboard_Tire__Rim
    Nose_Gear__Rim_Right__Tire Nose_Gear___Rim_Left_Tire
    No__1_Engine_Fan_Blades No__1_Engine_Fan_Disk
    No__2_Engine_Fan_Blades No__2_Engine_Fan_Disk}
puts {art twin materials ready}
