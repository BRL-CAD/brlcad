#        X M I N _ G U I _ P I P E _ O R A C L E . T C L
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
###
set tolerance 1.0e-9
set expected_attributes {
    V0 {0 0 0} O0 1 I0 0.5 R0 2
    V1 {4 0 2} O1 1 I1 0.5 R1 2
    V2 {6 2 2} O2 1 I2 0.5 R2 2
    V3 {8 4 4} O3 1 I3 0.5 R3 2
    V4 {12 4 6} O4 1 I4 0.5 R4 2
}

if {![catch {get gui.pipe V5}]} {
    error "pipe split produced more than five control points"
}

foreach {attribute expected} $expected_attributes {
    set actual [get gui.pipe $attribute]
    if {[llength $actual] != [llength $expected]} {
	error "pipe attribute $attribute is {$actual}, expected {$expected}"
    }
    foreach actual_value $actual expected_value $expected {
	if {abs(double($actual_value) - double($expected_value)) > $tolerance} {
	    error "pipe attribute $attribute is {$actual}, expected {$expected}"
	}
    }
}

puts "PASS: pipe split stored the expected five-point geometry"

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
