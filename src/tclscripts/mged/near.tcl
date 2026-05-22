#          N E A R . T C L
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
#
# Description -
#       Find regions near a specified object/path or point.
#

namespace eval ::near_ns {
    variable ray_serial 0
}


proc ::near_ns::require_commands {} {
    set extern_commands [list \
	bb \
	bu_get_value_by_keyword \
	bu_units_conversion \
	rt_gettrees \
	search \
	units]

    foreach cmd $extern_commands {
	catch {auto_load $cmd}
	if {[info commands $cmd] eq ""} {
	    error "[info script]: Application fails to provide command '$cmd'"
	}
    }
}

proc ::near_ns::normalize_path {path} {
    if {$path eq ""} {
	error "empty path specification"
    }

    if {[string index $path 0] eq "/"} {
	return $path
    }

    return "/$path"
}

proc ::near_ns::normalize_name {name} {
    return [regsub {^/} [string trim $name] {}]
}

proc ::near_ns::same_name {left right} {
    return [expr {[normalize_name $left] eq [normalize_name $right]}]
}

proc ::near_ns::distance_to_base {value} {
    global local2base

    set trimmed [string trim $value]

    if {[regexp {^([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)\s*([A-Za-z].*)$} $trimmed -> mag unit]} {
	return [expr {double($mag) * [bu_units_conversion [string trim $unit]]}]
    }

    if {![string is double -strict $trimmed]} {
	error "bad distance '$value' - use a number in current local units or an explicit unit suffix such as 1.0mm or 0.25in"
    }

    return [expr {double($trimmed) * $local2base}]
}

proc ::near_ns::region_paths {root} {
    set root [normalize_path $root]

    if {[catch {set paths [search $root -type region]} msg]} {
	error "search failed for '$root': $msg"
    }

    return [lsort -dictionary -unique $paths]
}

proc ::near_ns::bbox {path} {
    set cmd [list bb -q -e $path]

    if {[catch {set raw [uplevel #0 $cmd]}]} {
	return ""
    }

    if {![regexp {min \{([^\}]*)\} max \{([^\}]*)\}} $raw -> minpt maxpt]} {
	return ""
    }

    return [list [split $minpt] [split $maxpt]]
}

proc ::near_ns::bbox_distance {bbox1 bbox2} {
    set lo1 [lindex $bbox1 0]
    set hi1 [lindex $bbox1 1]
    set lo2 [lindex $bbox2 0]
    set hi2 [lindex $bbox2 1]

    set dx 0.0
    set dy 0.0
    set dz 0.0

    foreach axis {0 1 2} var {dx dy dz} {
	set lo_gap [expr {[lindex $lo1 $axis] - [lindex $hi2 $axis]}]
	set hi_gap [expr {[lindex $lo2 $axis] - [lindex $hi1 $axis]}]
	set gap 0.0

	if {$lo_gap > 0.0} {
	    set gap $lo_gap
	} elseif {$hi_gap > 0.0} {
	    set gap $hi_gap
	}

	set $var $gap
    }

    return [expr {sqrt(($dx * $dx) + ($dy * $dy) + ($dz * $dz))}]
}

proc ::near_ns::combined_bbox {bboxes} {
    if {![llength $bboxes]} {
	return ""
    }

    set first [lindex $bboxes 0]
    set combo_lo [lindex $first 0]
    set combo_hi [lindex $first 1]

    foreach bbox [lrange $bboxes 1 end] {
	set lo [lindex $bbox 0]
	set hi [lindex $bbox 1]
	foreach axis {0 1 2} {
	    lset combo_lo $axis [expr {min([lindex $combo_lo $axis], [lindex $lo $axis])}]
	    lset combo_hi $axis [expr {max([lindex $combo_hi $axis], [lindex $hi $axis])}]
	}
    }

    return [list $combo_lo $combo_hi]
}

proc ::near_ns::bbox_diagonal {bbox} {
    if {$bbox eq ""} {
	return 0.0
    }

    set lo [lindex $bbox 0]
    set hi [lindex $bbox 1]
    set dx [expr {[lindex $hi 0] - [lindex $lo 0]}]
    set dy [expr {[lindex $hi 1] - [lindex $lo 1]}]
    set dz [expr {[lindex $hi 2] - [lindex $lo 2]}]
    return [expr {sqrt(($dx * $dx) + ($dy * $dy) + ($dz * $dz))}]
}

proc ::near_ns::resolve_ray_spacing {spacing_spec bbox max_dist} {
    if {$spacing_spec ne "auto"} {
	return $spacing_spec
    }

    set spacing [expr {[bbox_diagonal $bbox] / 100.0}]
    if {$spacing <= 0.0} {
	set spacing $max_dist
    }

    return $spacing
}

proc ::near_ns::ray_axis_limits {lo hi axis max_dist} {
    set axis_min [lindex $lo $axis]
    set axis_max [lindex $hi $axis]
    set span [expr {$axis_max - $axis_min}]
    set margin [expr {(($span > $max_dist) ? $span : $max_dist) + $max_dist + 1.0}]
    return [list $axis_min $axis_max [expr {$axis_min - $margin}] [expr {$axis_max + $margin}]]
}

proc ::near_ns::record_spacing {values_var spacing} {
    upvar 1 $values_var values
    if {$spacing ne ""} {
	lappend values $spacing
    }
}

proc ::near_ns::bbox_point_distance {bbox point} {
    set lo [lindex $bbox 0]
    set hi [lindex $bbox 1]
    set dx 0.0
    set dy 0.0
    set dz 0.0

    foreach axis {0 1 2} var {dx dy dz} {
	set p [lindex $point $axis]
	set gap 0.0
	if {$p < [lindex $lo $axis]} {
	    set gap [expr {[lindex $lo $axis] - $p}]
	} elseif {$p > [lindex $hi $axis]} {
	    set gap [expr {$p - [lindex $hi $axis]}]
	}
	set $var $gap
    }

    return [expr {sqrt(($dx * $dx) + ($dy * $dy) + ($dz * $dz))}]
}

proc ::near_ns::is_distance_token {value} {
    set trimmed [string trim $value]
    return [expr {[string is double -strict $trimmed] || [regexp {^[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?\s*[A-Za-z].*$} $trimmed]}]
}

proc ::near_ns::expanded_overlap {lo1 hi1 lo2 hi2 pad} {
    set lo [expr {max($lo1 - $pad, $lo2 - $pad)}]
    set hi [expr {min($hi1 + $pad, $hi2 + $pad)}]

    if {$lo > $hi} {
	return ""
    }

    return [list $lo $hi]
}

proc ::near_ns::sample_values {minv maxv spacing max_samples} {
    if {$spacing <= 0.0} {
	error "ray spacing must be greater than zero"
    }

    set span [expr {$maxv - $minv}]
    if {$span <= 1.0e-9} {
	return [list [expr {($minv + $maxv) * 0.5}]]
    }

    set samples [expr {int(ceil($span / $spacing)) + 1}]
    if {$samples < 2} {
	set samples 2
    }
    if {$samples > $max_samples} {
	set samples $max_samples
    }

    set step [expr {$span / double($samples - 1)}]
    set values {}
    for {set i 0} {$i < $samples} {incr i} {
	lappend values [expr {$minv + ($i * $step)}]
    }

    return $values
}

proc ::near_ns::axis_name {axis} {
    return [lindex {X Y Z} $axis]
}

proc ::near_ns::axis_specs {} {
    return {
	{0 1 2}
	{1 0 2}
	{2 0 1}
    }
}

proc ::near_ns::usage {} {
    return "Usage: near \[-scope comb\] \[-ray_spacing distance|\"auto\"\] \[-max_samples n\] \[-quiet 0|1\] {object | x y z} \[distance\]"
}

proc ::near_ns::axis_point {axis aval bval cval} {
    switch -- $axis {
	0 { return [list $aval $bval $cval] }
	1 { return [list $bval $aval $cval] }
	2 { return [list $bval $cval $aval] }
    }

    error "bad axis index '$axis'"
}

proc ::near_ns::point_midpoint {left right} {
    return [list \
	[expr {([lindex $left 0] + [lindex $right 0]) * 0.5}] \
	[expr {([lindex $left 1] + [lindex $right 1]) * 0.5}] \
	[expr {([lindex $left 2] + [lindex $right 2]) * 0.5}]]
}

proc ::near_ns::make_ray {objects} {
    variable ray_serial

    incr ray_serial
    set ray_name ".nearray$ray_serial"

    rt_gettrees $ray_name -i -u {*}$objects

    $ray_name prep 1
    $ray_name no_bool 0
    $ray_name onehit 0

    return $ray_name
}

proc ::near_ns::ray_intervals {partitions target candidate} {
    set target_intervals {}
    set candidate_intervals {}

    foreach partition $partitions {
	if {$partition eq ""} {
	    continue
	}

	if {[catch {set region [bu_get_value_by_keyword region $partition]}] ||
	    [catch {set in_hit [bu_get_value_by_keyword in $partition]}] ||
	    [catch {set out_hit [bu_get_value_by_keyword out $partition]}] ||
	    [catch {set in_dist [bu_get_value_by_keyword dist $in_hit]}] ||
	    [catch {set out_dist [bu_get_value_by_keyword dist $out_hit]}] ||
	    [catch {set in_point [bu_get_value_by_keyword point $in_hit]}] ||
	    [catch {set out_point [bu_get_value_by_keyword point $out_hit]}]} {
	    continue
	}

	set interval [list $in_dist $out_dist $in_point $out_point]
	if {[same_name $region $target]} {
	    lappend target_intervals $interval
	} elseif {[same_name $region $candidate]} {
	    lappend candidate_intervals $interval
	}
    }

    return [list $target_intervals $candidate_intervals]
}

proc ::near_ns::ray_relation {target_intervals candidate_intervals max_dist} {
    set best_gap ""
    set best_gap_point ""
    set best_overlap ""
    set best_overlap_point ""

    foreach tint $target_intervals {
	set tin [lindex $tint 0]
	set tout [lindex $tint 1]
	set tin_point [lindex $tint 2]
	set tout_point [lindex $tint 3]

	foreach cint $candidate_intervals {
	    set cin [lindex $cint 0]
	    set cout [lindex $cint 1]
	    set cin_point [lindex $cint 2]
	    set cout_point [lindex $cint 3]

	    set overlap_start [expr {($tin > $cin) ? $tin : $cin}]
	    set overlap_end [expr {($tout < $cout) ? $tout : $cout}]
	    set overlap_depth [expr {$overlap_end - $overlap_start}]
	    if {$overlap_depth > 0.0} {
		if {$tin > $cin} {
		    set start_point $tin_point
		} else {
		    set start_point $cin_point
		}

		if {$tout < $cout} {
		    set end_point $tout_point
		} else {
		    set end_point $cout_point
		}

		if {$best_overlap eq "" || $overlap_depth > $best_overlap} {
		    set best_overlap $overlap_depth
		    set best_overlap_point [point_midpoint $start_point $end_point]
		}
		continue
	    }

	    if {$tout <= $cin} {
		set gap [expr {$cin - $tout}]
		set gap_point [point_midpoint $tout_point $cin_point]
	    } elseif {$cout <= $tin} {
		set gap [expr {$tin - $cout}]
		set gap_point [point_midpoint $cout_point $tin_point]
	    } else {
		continue
	    }

	    if {$gap <= $max_dist && ($best_gap eq "" || $gap < $best_gap)} {
		set best_gap $gap
		set best_gap_point $gap_point
	    }
	}
    }

    if {$best_overlap ne ""} {
	return [list overlap $best_overlap $best_overlap_point]
    }

    if {$best_gap ne ""} {
	return [list gap $best_gap $best_gap_point]
    }

    return [list none "" ""]
}

proc ::near_ns::sample_pair {target candidate bbox1 bbox2 max_dist ray_spacing max_samples} {
    set ray_name ""
    set best_gap ""
    set best_gap_point ""
    set best_gap_axis ""
    set best_overlap ""
    set best_overlap_point ""
    set best_overlap_axis ""

    set lo1 [lindex $bbox1 0]
    set hi1 [lindex $bbox1 1]
    set lo2 [lindex $bbox2 0]
    set hi2 [lindex $bbox2 1]

    set combo_lo {}
    set combo_hi {}
    foreach axis {0 1 2} {
	set lo [expr {min([lindex $lo1 $axis], [lindex $lo2 $axis])}]
	set hi [expr {max([lindex $hi1 $axis], [lindex $hi2 $axis])}]
	lappend combo_lo $lo
	lappend combo_hi $hi
    }

    set ray_spacing [resolve_ray_spacing $ray_spacing [combined_bbox [list $bbox1 $bbox2]] $max_dist]

    set ray_name [make_ray [list $target $candidate]]

    set status [catch {
	foreach pair [axis_specs] {
	    lassign $pair axis baxis caxis

	    set brange [expanded_overlap \
		[lindex $lo1 $baxis] [lindex $hi1 $baxis] \
		[lindex $lo2 $baxis] [lindex $hi2 $baxis] \
		$max_dist]
	    set crange [expanded_overlap \
		[lindex $lo1 $caxis] [lindex $hi1 $caxis] \
		[lindex $lo2 $caxis] [lindex $hi2 $caxis] \
		$max_dist]

	    if {$brange eq "" || $crange eq ""} {
		continue
	    }

	    set bvals [sample_values [lindex $brange 0] [lindex $brange 1] $ray_spacing $max_samples]
	    set cvals [sample_values [lindex $crange 0] [lindex $crange 1] $ray_spacing $max_samples]

	    lassign [ray_axis_limits $combo_lo $combo_hi $axis $max_dist] axis_min axis_max start_axis stop_axis

	    foreach bval $bvals {
		foreach cval $cvals {
		    set start [axis_point $axis $start_axis $bval $cval]
		    set stop [axis_point $axis $stop_axis $bval $cval]

		    set partitions [$ray_name shootray -R $start at $stop]
		    lassign [ray_intervals $partitions $target $candidate] target_intervals candidate_intervals
		    if {![llength $target_intervals] || ![llength $candidate_intervals]} {
			continue
		    }

		    lassign [ray_relation $target_intervals $candidate_intervals $max_dist] relation dist point
		    switch -- $relation {
			overlap {
			    if {$best_overlap eq "" || $dist > $best_overlap} {
				set best_overlap $dist
				set best_overlap_point $point
				set best_overlap_axis [axis_name $axis]
			    }
			}
			gap {
			    if {$best_gap eq "" || $dist < $best_gap} {
				set best_gap $dist
				set best_gap_point $point
				set best_gap_axis [axis_name $axis]
			    }
			}
		    }
		}
	    }
	}
    } result]

    catch {rename $ray_name ""}
    if {$status} {
	error $result
    }

    if {$best_overlap ne ""} {
	return [list 1 overlap $best_overlap $best_overlap_point $best_overlap_axis $ray_spacing]
    }

    if {$best_gap ne ""} {
	return [list 1 gap $best_gap $best_gap_point $best_gap_axis $ray_spacing]
    }

    return [list 0 none "" "" "" $ray_spacing]
}

proc ::near_ns::ray_point_relation {candidate_intervals point_axis_value max_dist point} {
    set best_gap ""
    set best_gap_point ""

    foreach cint $candidate_intervals {
	set cin [lindex $cint 0]
	set cout [lindex $cint 1]
	set cin_point [lindex $cint 2]
	set cout_point [lindex $cint 3]

	if {$point_axis_value >= $cin && $point_axis_value <= $cout} {
	    return [list overlap 0.0 $point]
	}

	if {$point_axis_value < $cin} {
	    set gap [expr {$cin - $point_axis_value}]
	    set gap_point [point_midpoint $point $cin_point]
	} else {
	    set gap [expr {$point_axis_value - $cout}]
	    set gap_point [point_midpoint $point $cout_point]
	}

	if {$gap <= $max_dist && ($best_gap eq "" || $gap < $best_gap)} {
	    set best_gap $gap
	    set best_gap_point $gap_point
	}
    }

    if {$best_gap ne ""} {
	return [list gap $best_gap $best_gap_point]
    }

    return [list none "" ""]
}

proc ::near_ns::sample_point_candidate {point candidate bbox max_dist ray_spacing max_samples} {
    set ray_name ""
    set best_gap ""
    set best_gap_point ""
    set best_gap_axis ""
    set best_overlap ""
    set best_overlap_point ""
    set best_overlap_axis ""

    set ray_spacing [resolve_ray_spacing $ray_spacing $bbox $max_dist]

    set lo [lindex $bbox 0]
    set hi [lindex $bbox 1]
    set ray_name [make_ray [list $candidate]]

    set status [catch {
	foreach pair [axis_specs] {
	    lassign $pair axis baxis caxis
	    set bval [lindex $point $baxis]
	    set cval [lindex $point $caxis]

	    if {$bval < ([lindex $lo $baxis] - $max_dist) || $bval > ([lindex $hi $baxis] + $max_dist) ||
		$cval < ([lindex $lo $caxis] - $max_dist) || $cval > ([lindex $hi $caxis] + $max_dist)} {
		continue
	    }

	    lassign [ray_axis_limits $lo $hi $axis $max_dist] axis_min axis_max start_axis stop_axis
	    set start [axis_point $axis $start_axis $bval $cval]
	    set stop [axis_point $axis $stop_axis $bval $cval]

	    set partitions [$ray_name shootray -R $start at $stop]
	    lassign [ray_intervals $partitions __near_point__ $candidate] unused candidate_intervals
	    if {![llength $candidate_intervals]} {
		continue
	    }

	    # shootray distances are measured from the ray start, not in world-axis coordinates.
	    set point_ray_dist [expr {[lindex $point $axis] - $start_axis}]
	    lassign [ray_point_relation $candidate_intervals $point_ray_dist $max_dist $point] relation dist hit_point
	    switch -- $relation {
		overlap {
		    if {$best_overlap eq "" || $dist > $best_overlap} {
			set best_overlap $dist
			set best_overlap_point $hit_point
			set best_overlap_axis [axis_name $axis]
		    }
		}
		gap {
		    if {$best_gap eq "" || $dist < $best_gap} {
			set best_gap $dist
			set best_gap_point $hit_point
			set best_gap_axis [axis_name $axis]
		    }
		}
	    }
	}
    } result]

    catch {rename $ray_name ""}
    if {$status} {
	error $result
    }

    if {$best_overlap ne ""} {
	return [list 1 overlap $best_overlap $best_overlap_point $best_overlap_axis $ray_spacing]
    }

    if {$best_gap ne ""} {
	return [list 1 gap $best_gap $best_gap_point $best_gap_axis $ray_spacing]
    }

    return [list 0 none "" "" "" $ray_spacing]
}

proc ::near_ns::format_local_distance {distance units_name} {
    global base2local
    return "[format %.6g [expr {$distance * $base2local}]] $units_name"
}

proc ::near_ns::format_spacing {spacing_spec spacing_values units_name} {
    if {$spacing_spec ne "auto"} {
	return [format_local_distance $spacing_spec $units_name]
    }

    if {![llength $spacing_values]} {
	return "auto (no ray checks)"
    }

    set sorted [lsort -real $spacing_values]
    set min_spacing [lindex $sorted 0]
    set max_spacing [lindex $sorted end]
    if {abs($max_spacing - $min_spacing) <= 1.0e-9} {
	return "[format_local_distance $min_spacing $units_name] (auto)"
    }

    return "[format_local_distance $min_spacing $units_name] .. [format_local_distance $max_spacing $units_name] (auto)"
}

proc ::near_ns::format_results {target_label distance spacing results stats} {
    global base2local

    set b2l $base2local
    set units_name [units -s]
    array set st $stats
    set lines {}
    set spacing_values {}
    if {[info exists st(spacing_values)]} {
	set spacing_values $st(spacing_values)
    }

    lappend lines "Target: $target_label"
    lappend lines "Distance: [format %.6g [expr {$distance * $b2l}]] $units_name"
    lappend lines "Ray spacing: [format_spacing $spacing $spacing_values $units_name]"
    if {[info exists st(target_regions)]} {
	lappend lines "Target regions: $st(target_regions)"
    }
    lappend lines "Regions checked -> bbox candidates -> near: $st(screened_candidates) -> $st(bbox_candidate_pairs) -> [llength $results]"

    if {![llength $results]} {
	lappend lines "No nearby regions found."
	return [join $lines "\n"]
    }

    lappend lines "Nearby regions:"
    foreach result $results {
	array unset row
	array set row $result

	if {$row(relation) eq "overlap"} {
	    set dist_label "overlap/inside"
	} else {
	    set dist_label "gap"
	}

	set bbox_gap [format %.6g [expr {$row(bbox_gap) * $b2l}]]
	set ray_gap [format %.6g [expr {$row(distance) * $b2l}]]
	set point [join $row(point) ", "]

	if {[info exists row(target)]} {
	    lappend lines "  $row(target) <-> $row(candidate) : $dist_label $ray_gap $units_name (bbox $bbox_gap $units_name, axis $row(axis), point {$point})"
	} else {
	    lappend lines "  $row(candidate) : $dist_label $ray_gap $units_name (bbox $bbox_gap $units_name, axis $row(axis), point {$point})"
	}
    }

    return [join $lines "\n"]
}

proc near {args} {
    ::near_ns::require_commands

    set scope [list "/"]
    set ray_spacing auto
    set max_samples 100
    set quiet 0
    set usage [::near_ns::usage]

    while {[llength $args] > 0 && [string match -* [lindex $args 0]]} {
	set option [lindex $args 0]
	set args [lrange $args 1 end]

	switch -- $option {
	    -scope {
		if {![llength $args]} {
		    error "near: -scope requires one path or a Tcl list of paths"
		}
		set scope [lindex $args 0]
		set args [lrange $args 1 end]
	    }
	    -ray_spacing {
		if {![llength $args]} {
		    error "near: -ray_spacing requires a distance or 'auto'"
		}
		set ray_spacing [lindex $args 0]
		set args [lrange $args 1 end]
	    }
	    -max_samples {
		if {![llength $args]} {
		    error "near: -max_samples requires a positive integer"
		}
		set max_samples [lindex $args 0]
		set args [lrange $args 1 end]
	    }
	    -quiet {
		if {![llength $args]} {
		    error "near: -quiet requires 0 or 1"
		}
		set quiet [lindex $args 0]
		set args [lrange $args 1 end]
	    }
	    -- {
		break
	    }
	    default {
		error $usage
	    }
	}
    }

    if {![string is integer -strict $max_samples] || $max_samples < 1} {
	error "near: -max_samples must be a positive integer"
    }

    if {$quiet ni {0 1}} {
	error "near: -quiet must be 0 or 1"
    }

    if {$ray_spacing ne "auto"} {
	set ray_spacing [::near_ns::distance_to_base $ray_spacing]
	if {$ray_spacing <= 0.0} {
	    error "near: ray spacing must be greater than zero"
	}
    }

    set argc [llength $args]
    if {$argc < 1 || $argc > 4} {
	error $usage
    }

    set point_mode 0
    if {($argc == 3 || $argc == 4) &&
	[::near_ns::is_distance_token [lindex $args 0]] &&
	[::near_ns::is_distance_token [lindex $args 1]] &&
	[::near_ns::is_distance_token [lindex $args 2]]} {
	set point_mode 1
    }

    set candidate_regions {}
    foreach root $scope {
	set candidate_regions [concat $candidate_regions [::near_ns::region_paths $root]]
    }
    set candidate_regions [lsort -dictionary -unique $candidate_regions]

    array set bbox_cache {}
    set candidate_bboxes {}
    foreach candidate $candidate_regions {
	set cbbox [::near_ns::bbox $candidate]
	if {$cbbox eq ""} {
	    continue
	}
	set bbox_cache($candidate) $cbbox
	lappend candidate_bboxes $cbbox
    }

    if {$point_mode} {
	set point [list \
	    [::near_ns::distance_to_base [lindex $args 0]] \
	    [::near_ns::distance_to_base [lindex $args 1]] \
	    [::near_ns::distance_to_base [lindex $args 2]]]
	set target_label "point {[join $point {, }]}"

	if {$argc == 4} {
	    set distance [::near_ns::distance_to_base [lindex $args 3]]
	} else {
	    set distance [expr {[::near_ns::bbox_diagonal [::near_ns::combined_bbox $candidate_bboxes]] * 0.10}]
	}
	if {$distance <= 0.0} {
	    error "near: max_distance must be greater than zero"
	}

	set results {}
	set spacing_values {}
	set bbox_candidate_pairs 0
	set screened_candidates 0
	foreach candidate $candidate_regions {
	    if {![info exists bbox_cache($candidate)]} {
		continue
	    }
	    incr screened_candidates
	    set bbox_gap [::near_ns::bbox_point_distance $bbox_cache($candidate) $point]
	    if {$bbox_gap > $distance} {
		continue
	    }
	    incr bbox_candidate_pairs

	    lassign [::near_ns::sample_point_candidate \
		$point \
		$candidate \
		$bbox_cache($candidate) \
		$distance \
		$ray_spacing \
		$max_samples] \
		found relation dist hit_point axis used_spacing
	    ::near_ns::record_spacing spacing_values $used_spacing

	    if {!$found} {
		continue
	    }

	    lappend results [list \
		candidate $candidate \
		relation $relation \
		distance $dist \
		bbox_gap $bbox_gap \
		point $hit_point \
		axis $axis]
	}

	if {!$quiet} {
	    puts [::near_ns::format_results \
		$target_label \
		$distance \
		$ray_spacing \
		$results \
		[list screened_candidates $screened_candidates bbox_candidate_pairs $bbox_candidate_pairs spacing_values $spacing_values]]
	    return ""
	}
	return $results
    }

    if {$argc != 1 && $argc != 2} {
	error $usage
    }

    set object [lindex $args 0]
    set target_regions [::near_ns::region_paths $object]
    if {![llength $target_regions]} {
	error "near: '$object' does not resolve to any regions"
    }

    foreach target $target_regions {
	set idx [lsearch -exact $candidate_regions $target]
	if {$idx != -1} {
	    set candidate_regions [lreplace $candidate_regions $idx $idx]
	}
    }

    set target_bboxes {}
    foreach target $target_regions {
	set tbbox [::near_ns::bbox $target]
	if {$tbbox eq ""} {
	    error "near: failed to compute a bounding box for '$target'"
	}
	set bbox_cache($target) $tbbox
	lappend target_bboxes $tbbox
    }

    if {$argc == 2} {
	set distance [::near_ns::distance_to_base [lindex $args 1]]
    } else {
	set distance [expr {[::near_ns::bbox_diagonal [::near_ns::combined_bbox $target_bboxes]] * 0.10}]
    }
    if {$distance <= 0.0} {
	error "near: max_distance must be greater than zero"
    }

    set results {}
    set spacing_values {}
    set bbox_candidate_pairs 0
    set screened_candidates 0

    foreach candidate $candidate_regions {
	if {![info exists bbox_cache($candidate)]} {
	    continue
	}
	incr screened_candidates

	foreach target $target_regions {
	    set bbox_gap [::near_ns::bbox_distance $bbox_cache($target) $bbox_cache($candidate)]
	    if {$bbox_gap > $distance} {
		continue
	    }

	    incr bbox_candidate_pairs
	    lassign [::near_ns::sample_pair \
		$target \
		$candidate \
		$bbox_cache($target) \
		$bbox_cache($candidate) \
		$distance \
		$ray_spacing \
		$max_samples] \
		found relation dist point axis used_spacing
	    ::near_ns::record_spacing spacing_values $used_spacing

	    if {!$found} {
		continue
	    }

	    lappend results [list \
		target $target \
		candidate $candidate \
		relation $relation \
		distance $dist \
		bbox_gap $bbox_gap \
		point $point \
		axis $axis]
	}
    }

    if {!$quiet} {
	puts [::near_ns::format_results \
	    $object \
	    $distance \
	    $ray_spacing \
	    $results \
	    [list target_regions [llength $target_regions] screened_candidates $screened_candidates bbox_candidate_pairs $bbox_candidate_pairs spacing_values $spacing_values]]
	return ""
    }

    return $results
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
