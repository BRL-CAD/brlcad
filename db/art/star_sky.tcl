# S T A R _ S K Y . T C L
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

# Require the known constant-blue sky rather than silently translating a
# different checker pattern or a physical atmosphere.
set sky [get clouds.r shader]
if {[lindex $sky 0] ne "checker" || [lindex $sky 1] ne "a 0/0/150 b 0/0/150"} {
    error "Expected star.g's constant-blue clouds.r checker shader"
}

# Authoring a light proxy can draw it in mged.  Save a subject-only camera
# before creating those objects so their positions cannot affect framing.
Z
draw ncc1701 shuttle
autoview
ae 35 25
zoom 1.6
saveview -e art -o star.png [file join $::env(ART_DEMO_OUTPUT_DIR) star-camera.sh]

# Keep subject instances and their region colors while polishing the surface.
source studio_materials.tcl
art_demo::make_material star.surface.material as_plastic {
    in_specular_weight {float 1}
    in_specular_roughness {float 0.2}
} {ncc1701 shuttle}

# The studio coordinates use BRL-CAD's millimeter base units.  Leave the old
# shell and light available for inspection, outside the art illumination.
units mm
source studio_lights.tcl
art_demo::point_lights {
    key {300000 -150000 350000} 255/241/220 1
    rim {0 200000 160000} 190/215/255 0.4
} 100
attr set light.r shader {light {bright 0}}
comb art.demo.g u ncc1701 u shuttle u light.r u art.key.r u art.rim.r
puts {art sky demo ready}
