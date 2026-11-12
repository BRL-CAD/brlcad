# M O S S _ M A T E R I A L S . T C L
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

namespace eval art_demo {
    proc make_material {name shader parameters regions} {
        material create $name $name
        material set $name optical OSL $shader
        material set $name optical OSL.output out_outColor
        foreach {parameter value} $parameters {
            material set $name optical OSL.param.$parameter $value
        }
        foreach region $regions {
            material assign $region $name
        }
    }
}
art_demo::make_material slate.material as_plastic {
    in_color {color 0.045 0.055 0.075}
    in_specular_weight {float 0.7}
    in_specular_roughness {float 0.3}
} platform.r
art_demo::make_material enamel.material as_plastic {
    in_specular_weight {float 1}
    in_specular_roughness {float 0.14}
} {box.r cone.r}
art_demo::make_material gold.material as_metal {
    in_face_reflectance {color 0.85 0.55 0.12}
    in_roughness {float 0.16}
    in_energy_compensation {float 0}
} tor.r
art_demo::make_material glass.material as_glass {
    in_ior {float 1.5}
    in_roughness {float 0}
    in_transmittance_amount {float 1}
    in_surface_transmittance {color 1 1 1}
    in_volume_transmittance {color 0.65 0.9 1}
    in_volume_transmittance_distance {float 20}
} ellipse.r
puts {art demo materials ready}
