# M 3 5 _ M A T E R I A L S . T C L
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
# An unspecified in_color retains the database's table and explicit colors.
art_demo::make_material art.paint.material as_plastic {
    in_specular_weight {float 0.9}
    in_specular_roughness {float 0.22}
} component
set rubber [lsort -unique [lmap path [search /component -attr material_id=18] {file tail $path}]]
if {[llength $rubber] == 0} {
    error "Expected m35 rubber regions with material_id=18"
}
art_demo::make_material art.rubber.material as_plastic {
    in_color {color 0.02 0.025 0.03}
    in_specular_weight {float 0.25}
    in_specular_roughness {float 0.4}
} $rubber
art_demo::make_material art.chrome.material as_metal {
    in_face_reflectance {color 0.75 0.8 0.85}
    in_roughness {float 0.18}
    in_energy_compensation {float 0}
} {r737 r739 r741}
art_demo::make_material art.mirrors.material as_metal {
    in_face_reflectance {color 0.95 0.95 0.95}
    in_roughness {float 0}
    in_energy_compensation {float 0}
} {r872 r891}
art_demo::make_material art.glazing.material as_glass {
    in_ior {float 1.5}
    in_roughness {float 0}
    in_transmittance_amount {float 1}
    in_surface_transmittance {color 1 1 1}
    in_volume_transmittance {color 0.8 0.9 1}
    in_volume_transmittance_distance {float 100}
} {r682 r683 r684 r685}
puts {art m35 materials ready}
