# E N G I N E E R I N G _ M A T E R I A L S . T C L
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

namespace eval art_engineering {
    variable palette [dict create \
        shell {0.20 0.40 0.60} \
        amber {0.85 0.40 0.045} \
        blue {0.025 0.32 0.65} \
        cyan {0.025 0.55 0.65} \
        orange {0.85 0.09 0.035} \
        purple {0.38 0.06 0.60} \
        green {0.055 0.50 0.16} \
        yellow {0.85 0.65 0.06} \
        steel {0.23 0.29 0.36} \
        graphite {0.055 0.075 0.11}]

    proc make_material {name color_key objects {transparency 0}} {
        variable palette
        foreach object $objects {
            if {![exists $object]} {error "Missing engineering object: $object"}
        }
        if {![llength $objects]} {error "Empty engineering material selection: $name"}
        art_demo::make_material art.engineering.$name.material art_engineering [list \
            in_color [linsert [dict get $palette $color_key] 0 color] \
            in_opacity [list float [expr {1.0 - $transparency}]]] $objects
        if {$transparency > 0} {
            material set art.engineering.$name.material optical OSL.param.in_contour_color {color 0.45 0.7 0.95}
        }
    }
}
