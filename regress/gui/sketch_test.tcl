#               S K E T C H _ T E S T . T C L
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
# Shared sketch segment checks used by Archer and MGED.

proc ::gui::test::sketch_editor_stub {command args} {
    switch -- $command {
	get_scale {
	    return 1.0
	}
	get_vlist {
	    return {{0 0} {10 0} {5 5}}
	}
	set_radius {
	    return
	}
	default {
	    error "unexpected sketch editor command: $command"
	}
    }
}

proc ::gui::test::require_unit_vector {vector description} {
    lassign $vector x y
    set magnitude [expr {sqrt($x * $x + $y * $y)}]
    require {abs($magnitude - 1.0) < 1.0e-9} "$description is not a unit vector: $vector"
}

proc ::gui::test::exercise_sketch_segments {
    arc_class bezier_class application object_namespace
} {
    set canvas .gui_sketch_segments
    catch {destroy $canvas}
    canvas $canvas

    set editor ::gui::test::sketch_editor_stub
    set arc ${object_namespace}::arc_probe
    $arc_class $arc $editor $canvas {S 0 E 1 R 10 L 0 O 0}
    set start_tangent [$arc get_tangent_at_vertex 0]
    set end_tangent [$arc get_tangent_at_vertex 1]
    require_unit_vector $start_tangent "$application sketch arc start tangent"
    require_unit_vector $end_tangent "$application sketch arc end tangent"
    require {[$arc get_tangent_at_vertex 2] eq "0 0"} "$application sketch arc returned a tangent for an unrelated vertex"
    $arc draw {}
    require {[llength [$canvas find all]] == 1} "$application sketch arc did not draw on its canvas"
    $arc reverse_orientation
    set reversed_tangent [$arc get_tangent_at_vertex 0]
    lassign $start_tangent sx sy
    lassign $reversed_tangent rx ry
    require {
	abs($sx + $rx) < 1.0e-9 && abs($sy + $ry) < 1.0e-9
    } "$application sketch arc orientation did not reverse its tangent"
    ::itcl::delete object $arc

    set circle ${object_namespace}::circle_probe
    $arc_class $circle $editor $canvas {S 0 E 1 R -1 L 0 O 0}
    require_unit_vector [$circle get_tangent_at_vertex 0] "$application full-circle tangent"
    require {[$circle get_tangent_at_vertex 1] eq "0 0"} "$application full circle reported a tangent at its center"
    ::itcl::delete object $circle

    set bezier ${object_namespace}::bezier_probe
    set item_count [llength [$canvas find all]]
    $bezier_class $bezier $editor $canvas {D 2 P {0 2 1}}
    $bezier draw {}
    require {
	[llength [$canvas find all]] == $item_count + 1
    } "$application sketch Bezier did not draw on its canvas"
    require {
	[string match {*bezier D 2 P*} [$bezier serialize 1.0]]
    } "$application sketch Bezier did not serialize its control points"
    ::itcl::delete object $bezier
    destroy $canvas
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
