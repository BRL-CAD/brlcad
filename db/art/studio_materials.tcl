# S T U D I O _ M A T E R I A L S . T C L
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
    proc make_material {name shader parameters objects} {
        material create $name $name
        material set $name optical OSL $shader
        material set $name optical OSL.output out_outColor
        foreach {parameter value} $parameters {
            material set $name optical OSL.param.$parameter $value
        }
        foreach object $objects {
            material assign $object $name
        }
    }
}
