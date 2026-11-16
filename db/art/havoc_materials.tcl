# H A V O C _ M A T E R I A L S . T C L
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
art_demo::make_material art.paint.material as_plastic {
    in_color {color 0.055 0.075 0.03}
    in_specular_weight {float 0.8}
    in_specular_roughness {float 0.28}
} havoc
art_demo::make_material art.glazing.material as_glass {
    in_ior {float 1.5}
    in_roughness {float 0}
    in_transmittance_amount {float 1}
    in_surface_transmittance {color 1 1 1}
    in_volume_transmittance {color 0.8 0.95 0.9}
    in_volume_transmittance_distance {float 100}
} {cannopy_glass laser_sight_glss}
art_demo::make_material art.blades.material as_plastic {
    in_color {color 0.022 0.028 0.035}
    in_specular_weight {float 0.5}
    in_specular_roughness {float 0.32}
} {mr_blade1 mr_blade101 mr_blade201 mr_blade301 mr_blade401 tail_rot_blades}
art_demo::make_material art.rubber.material as_plastic {
    in_color {color 0.02 0.025 0.03}
    in_specular_weight {float 0.25}
    in_specular_roughness {float 0.4}
} {2_wipe1_rubber 3_wipe1_rubber l_lgear_tire r_lgear_tire t_lgear_tire}
puts {art havoc materials ready}
