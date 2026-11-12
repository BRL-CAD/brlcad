# M O S S _ S T U D I O . T C L
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

# The studio coordinates use BRL-CAD's millimeter base units.
units mm

# Keep the light centers inside the original model bounds for auto-framing.
foreach {name position color brightness} {
    key {-35 -35 60} 255/237/220 1
    fill {65 -45 50} 200/222/255 0.3
    rim {55 45 55} 255/255/255 0.6
} {
    in art.$name.s sph {*}$position 0.25
    r art.$name.r u art.$name.s
    attr set art.$name.r shader [list light [list bright $brightness]] color $color
}
attr set light.r shader {light {bright 0}}
comb art.demo.g u all.g u art.key.r u art.fill.r u art.rim.r
puts {art demo studio ready}
