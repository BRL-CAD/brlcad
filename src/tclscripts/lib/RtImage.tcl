#                          R T I M A G E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2026 United States Government as represented by
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
#	This creates rtwizard types of images.
#

package provide cadwidgets::RtImage 1.0

proc ::pid_wait { pid } {
    if {$::tcl_platform(platform) == "windows"} {
	set task_cmd [auto_execok tasklist]
	set task_args [list $task_cmd /FI "PID eq $pid" /FI {STATUS eq running} "/NH"]
	set task_list "$pid"
	while {[string match "*$pid*" $task_list]} {
	    catch {eval exec $task_args} task_list
	    after 50
	}
    } else {
	while {![catch {exec kill -0 $pid} pid_results]} {
	    after 50
	}
    }
}

namespace eval cadwidgets {

proc rtimage_control_word {word} {
    if {[regexp {[;\r\n]} $word]} {
	return -code error "rtwizard: animation object names may not contain command separators"
    }
    return \"[string map [list \\ \\\\ \" \\\"] $word]\"
}

proc rtimage_exec_log {cmd log_file {objects {}} {anim_commands {}}} {
    if {[llength $anim_commands]} {
	set script ""
	foreach anim_cmd $anim_commands { append script $anim_cmd ";\n" }
	append script "tree"
	foreach object $objects { append script " " [rtimage_control_word $object] }
	append script ";\nend;\n"
	# Object arguments make rt take its immediate-render path, which does not
	# consume the control stream.  The script's typed tree command supplies
	# those objects after the database and animations have been loaded.
	set exec_cmd $cmd
	if {[llength $objects]} { set exec_cmd [lrange $cmd 0 end-[llength $objects]] }
	set status [catch {exec {*}$exec_cmd << $script >& $log_file} msg]
    } else {
	set status [catch {exec {*}$cmd >& $log_file} msg]
    }
    if {$status} {
	return -code error "rtimage command failed: $cmd\n$msg"
    }
}

proc rtimage_bounds_center {dbfile objects} {
    foreach {xmin xmax} [rtwizard_cut_bounds $dbfile {1 0 0} {*}$objects] break
    foreach {ymin ymax} [rtwizard_cut_bounds $dbfile {0 1 0} {*}$objects] break
    foreach {zmin zmax} [rtwizard_cut_bounds $dbfile {0 0 1} {*}$objects] break
    return [list [expr {($xmin+$xmax)/2.0}] [expr {($ymin+$ymax)/2.0}] [expr {($zmin+$zmax)/2.0}]]
}

proc rtimage_rotate_vector {v axis angle_degrees} {
    set a [vunitize $axis]
    set r [expr {$angle_degrees * acos(-1.0) / 180.0}]
    set c [expr {cos($r)}]
    set s [expr {sin($r)}]
    set dot [expr {[lindex $a 0]*[lindex $v 0]+[lindex $a 1]*[lindex $v 1]+[lindex $a 2]*[lindex $v 2]}]
    set cross [list \
	[expr {[lindex $a 1]*[lindex $v 2]-[lindex $a 2]*[lindex $v 1]}] \
	[expr {[lindex $a 2]*[lindex $v 0]-[lindex $a 0]*[lindex $v 2]}] \
	[expr {[lindex $a 0]*[lindex $v 1]-[lindex $a 1]*[lindex $v 0]}]]
    return [list \
	[expr {[lindex $v 0]*$c+[lindex $cross 0]*$s+[lindex $a 0]*$dot*(1.0-$c)}] \
	[expr {[lindex $v 1]*$c+[lindex $cross 1]*$s+[lindex $a 1]*$dot*(1.0-$c)}] \
	[expr {[lindex $v 2]*$c+[lindex $cross 2]*$s+[lindex $a 2]*$dot*(1.0-$c)}]]
}

proc rtimage_lookat_quat {direction {up {}}} {
    set forward [vunitize $direction]
    if {![llength $up]} { return [quat_mat2quat [mat_lookat $forward 0]] }
    set right [vcross $forward [vunitize $up]]
    if {[magnitude $right] < 1.0e-12} {
	return [quat_mat2quat [mat_lookat $forward 0]]
    }
    set right [vunitize $right]
    set camera_up [vcross $right $forward]
    set m [list \
	[lindex $right 0] [lindex $right 1] [lindex $right 2] 0 \
	[lindex $camera_up 0] [lindex $camera_up 1] [lindex $camera_up 2] 0 \
	[expr {-[lindex $forward 0]}] [expr {-[lindex $forward 1]}] [expr {-[lindex $forward 2]}] 0 \
	0 0 0 1]
    return [quat_mat2quat $m]
}

proc rtimage_preset_plan {rtimage_dict} {
    foreach param [dict keys $rtimage_dict] {
	set $param [dict get $rtimage_dict $param]
    }
    foreach var {_color_objects _ghost_objects _edge_objects} {
	if {![info exists $var]} { set $var {} }
    }

    set anim_objects [lsort -unique [concat $_color_objects $_ghost_objects $_edge_objects]]
    if {![llength $anim_objects]} {
	return -code error "rtwizard: no objects available for cutting-plane animation bounds"
    }
    set preset [expr {[info exists ::RtWizard::wizard_state(animation_preset)] ?
	$::RtWizard::wizard_state(animation_preset) : "cut"}]
    set fps $::RtWizard::wizard_state(animation_fps)
    set duration $::RtWizard::wizard_state(animation_duration)
    set frame_count $::RtWizard::wizard_state(animation_frames)
    set cyclic [expr {$preset in {orbit turntable} &&
	abs($::RtWizard::wizard_state(${preset}_angle)) >= 360.0}]
    if {[info exists ::RtWizard::wizard_state(animation_cyclic)] &&
	$::RtWizard::wizard_state(animation_cyclic) >= 0} {
	set cyclic $::RtWizard::wizard_state(animation_cyclic)
    }
    if {$frame_count < 2} { set frame_count [expr {round($duration*$fps) + ($cyclic ? 0 : 1)}] }
    if {$frame_count < 2} { return -code error "rtwizard: animation requires at least two frames" }
    set plays $::RtWizard::wizard_state(animation_plays)
    if {$plays < 0} { set plays [expr {$cyclic ? 0 : 1}] }
    set frames {}

    if {$preset eq "cut"} {
      if {[info exists ::RtWizard::wizard_state(cut_steps)] && $::RtWizard::wizard_state(cut_steps) >= 2} {
	set frame_count $::RtWizard::wizard_state(cut_steps)
      }

      if {[info exists ::RtWizard::wizard_state(cut_direction)] &&
	[string trim $::RtWizard::wizard_state(cut_direction)] ne ""} {
	if {[catch {set cut_dir [vunitize $::RtWizard::wizard_state(cut_direction)]}]} {
	    return -code error "rtwizard: cutting direction must be a non-zero XYZ vector"
	}
    } else {
	set omat [quat_quat2mat $_orientation]
	set cut_dir [vunitize [list [expr {-[lindex $omat 8]}] \
	    [expr {-[lindex $omat 9]}] [expr {-[lindex $omat 10]}]]]
    }

      set sweep_bounds [rtwizard_cut_bounds $_dbfile $cut_dir {*}$anim_objects]
    set sweep_min [lindex $sweep_bounds 0]
    set sweep_max [lindex $sweep_bounds 1]
    set sweep_span [expr {$sweep_max - $sweep_min}]
    if {$sweep_span <= 0.0} {
	return -code error "rtwizard: rendered objects have degenerate cutting-plane bounds"
    }
    # Keep the terminal slice large enough to survive rasterization while
    # remaining approximately one output-pixel fraction of the sweep.
    set sweep_eps [expr {max(abs($sweep_span) / double(max($_w, $_n)),
	abs($sweep_span) * 0.05, 1.0e-9)}]
    set sweep_first [expr {$sweep_min - $sweep_eps}]
    set sweep_last [expr {$sweep_max - $sweep_eps}]
      for {set frame_no 0} {$frame_no < $frame_count} {incr frame_no} {
	set fraction [expr {double($frame_no) / double($frame_count - 1)}]
	set plane_dist [expr {$sweep_first + $fraction * ($sweep_last - $sweep_first)}]
	set plane_pt [vscale $cut_dir $plane_dist]
	lappend frames [dict create time [expr {$duration*$fraction}] \
	    viewsize $_viewsize orientation $_orientation eye_pt $_eye_pt perspective $_perspective \
	    cut_plane [format "%.15g,%.15g,%.15g,%.15g,%.15g,%.15g" \
		[lindex $plane_pt 0] [lindex $plane_pt 1] [lindex $plane_pt 2] \
		[lindex $cut_dir 0] [lindex $cut_dir 1] [lindex $cut_dir 2]] anim_commands {}]
      }
    } elseif {$preset eq "orbit"} {
      set axis [vunitize $::RtWizard::wizard_state(orbit_axis)]
      if {[info exists ::RtWizard::wizard_state(orbit_center)]} {
	set center $::RtWizard::wizard_state(orbit_center)
      } else {
	set center [rtimage_bounds_center $_dbfile $anim_objects]
      }
      set radial [list [expr {[lindex $_eye_pt 0]-[lindex $center 0]}] \
	[expr {[lindex $_eye_pt 1]-[lindex $center 1]}] [expr {[lindex $_eye_pt 2]-[lindex $center 2]}]]
      set radius [magnitude $radial]
	if {$radius < 1.0e-12} {
	    if {abs([lindex $axis 2]) < 0.9} { set reference {0 0 1} } else { set reference {1 0 0} }
	    set radius $_viewsize
	    set radial [vscale [vunitize [vcross $axis $reference]] $radius]
	}
      if {[info exists ::RtWizard::wizard_state(orbit_radius)]} {
	set radius $::RtWizard::wizard_state(orbit_radius)
	set radial [vscale [vunitize $radial] $radius]
      }
      if {[info exists ::RtWizard::wizard_state(orbit_elevation)]} {
	set elev [expr {$::RtWizard::wizard_state(orbit_elevation)*acos(-1.0)/180.0}]
	set along [expr {[lindex $radial 0]*[lindex $axis 0]+[lindex $radial 1]*[lindex $axis 1]+[lindex $radial 2]*[lindex $axis 2]}]
	set plane [list [expr {[lindex $radial 0]-$along*[lindex $axis 0]}] \
	    [expr {[lindex $radial 1]-$along*[lindex $axis 1]}] [expr {[lindex $radial 2]-$along*[lindex $axis 2]}]]
	if {[magnitude $plane] < 1.0e-12} {
	    if {abs([lindex $axis 2]) < 0.9} { set reference {0 0 1} } else { set reference {1 0 0} }
	    set plane [vcross $axis $reference]
	}
	set radial [vadd2 [vscale [vunitize $plane] [expr {$radius*cos($elev)}]] \
	    [vscale $axis [expr {$radius*sin($elev)}]]]
      }
      for {set frame_no 0} {$frame_no < $frame_count} {incr frame_no} {
	set fraction [expr {double($frame_no) / double($cyclic ? $frame_count : $frame_count-1)}]
	set rv [rtimage_rotate_vector $radial $axis [expr {$fraction*$::RtWizard::wizard_state(orbit_angle)}]]
	set eye [vadd2 $center $rv]
	set dir [vunitize [list [expr {[lindex $center 0]-[lindex $eye 0]}] \
	    [expr {[lindex $center 1]-[lindex $eye 1]}] [expr {[lindex $center 2]-[lindex $eye 2]}]]]
	set q [rtimage_lookat_quat $dir $axis]
	lappend frames [dict create time [expr {$duration*$fraction}] viewsize $_viewsize \
	    orientation $q eye_pt $eye perspective $_perspective cut_plane "" anim_commands {}]
      }
    } elseif {$preset eq "turntable"} {
      set axis [vunitize $::RtWizard::wizard_state(turntable_axis)]
	if {[regexp {[[:space:];]} $::RtWizard::wizard_state(turntable_object)]} {
	    return -code error "rtwizard: turntable object paths may not contain whitespace or semicolons"
	}
	set turntable_path $::RtWizard::wizard_state(turntable_object)
	if {[string first "/" $turntable_path] < 0} { set turntable_path "/$turntable_path" }
      if {[info exists ::RtWizard::wizard_state(turntable_center)]} {
	set center $::RtWizard::wizard_state(turntable_center)
      } else {
	set center [rtimage_bounds_center $_dbfile [list $::RtWizard::wizard_state(turntable_object)]]
      }
      foreach {x y z} $axis break
      foreach {px py pz} $center break
      for {set frame_no 0} {$frame_no < $frame_count} {incr frame_no} {
	set fraction [expr {double($frame_no) / double($cyclic ? $frame_count : $frame_count-1)}]
	set a [expr {$fraction*$::RtWizard::wizard_state(turntable_angle)*acos(-1.0)/180.0}]
	set c [expr {cos($a)}]; set s [expr {sin($a)}]; set m [expr {1.0-$c}]
	set r00 [expr {$c+$x*$x*$m}]; set r01 [expr {$x*$y*$m-$z*$s}]; set r02 [expr {$x*$z*$m+$y*$s}]
	set r10 [expr {$y*$x*$m+$z*$s}]; set r11 [expr {$c+$y*$y*$m}]; set r12 [expr {$y*$z*$m-$x*$s}]
	set r20 [expr {$z*$x*$m-$y*$s}]; set r21 [expr {$z*$y*$m+$x*$s}]; set r22 [expr {$c+$z*$z*$m}]
	set tx [expr {$px-($r00*$px+$r01*$py+$r02*$pz)}]
	set ty [expr {$py-($r10*$px+$r11*$py+$r12*$pz)}]
	set tz [expr {$pz-($r20*$px+$r21*$py+$r22*$pz)}]
	set cmd [format "anim %s matrix lmul %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g 0 0 0 1" \
	    $turntable_path $r00 $r01 $r02 $tx $r10 $r11 $r12 $ty $r20 $r21 $r22 $tz]
	lappend frames [dict create time [expr {$duration*$fraction}] viewsize $_viewsize \
	    orientation $_orientation eye_pt $_eye_pt perspective $_perspective cut_plane "" anim_commands [list $cmd]]
      }
    } else {
      return -code error "rtwizard: unknown animation preset '$preset'"
    }
    return [dict create duration $duration fps $fps cyclic $cyclic plays $plays frames $frames]
}

proc rtimage_animation {rtimage_dict} {
    foreach param [dict keys $rtimage_dict] { set $param [dict get $rtimage_dict $param] }
    if {[info exists ::RtWizard::wizard_state(animation_file)] &&
	$::RtWizard::wizard_state(animation_file) ne ""} {
	set local2base 1.0
	if {![catch {set unit_name [db units -s]}] && $unit_name ne ""} {
	    set local2base [bu_units_conversion $unit_name]
	}
	set plan [rtwizard_animation_json $::RtWizard::wizard_state(animation_file) \
	    $_viewsize $_orientation $_eye_pt $_perspective \
	    $::RtWizard::wizard_state(animation_duration) $::RtWizard::wizard_state(animation_fps) \
	    $::RtWizard::wizard_state(animation_frames) $::RtWizard::wizard_state(animation_plays) \
	    $::RtWizard::wizard_state(animation_cyclic) $local2base]
    } else {
	set plan [rtimage_preset_plan $rtimage_dict]
    }
    dict set rtimage_dict _animate 0
    set animation_frames {}
    set temp_dir [expr {[info exists ::RtWizard::wizard_state(output_filename)] ?
	[file dirname $::RtWizard::wizard_state(output_filename)] : [pwd]}]
    set keep_dir ""
    if {[info exists ::RtWizard::wizard_state(frame_dir)] && $::RtWizard::wizard_state(frame_dir) ne ""} {
	set keep_dir $::RtWizard::wizard_state(frame_dir)
	file mkdir $keep_dir
    }
    set output_tmp ""
    try {
	set frames [dict get $plan frames]
	set frame_count [llength $frames]
	for {set frame_no 0} {$frame_no < $frame_count} {incr frame_no} {
	    if {$keep_dir ne ""} {
		set frame_file [file join $keep_dir [format "frame-%06d.png" $frame_no]]
		if {[file exists $frame_file]} {
		    if {[info exists ::RtWizard::wizard_state(resume_animation)] &&
			$::RtWizard::wizard_state(resume_animation) &&
			[rtwizard_image_valid $frame_file $_w $_n]} {
			puts "Using existing frame [expr {$frame_no + 1}]/$frame_count"
			lappend animation_frames $frame_file
			continue
		    }
		    if {![info exists ::RtWizard::wizard_state(resume_animation)] ||
			!$::RtWizard::wizard_state(resume_animation)} {
			return -code error "rtwizard: frame already exists: $frame_file (use --resume)"
		    }
		}
	    } else {
		set frame_file [file join $temp_dir [format ".rtwizard-%d-%06d.pix" [pid] $frame_no]]
	    }
	    set frame [lindex $frames $frame_no]
	    foreach key {viewsize orientation eye_pt perspective cut_plane anim_commands} {
		dict set rtimage_dict _$key [dict get $frame $key]
	    }
	    puts "Rendering frame [expr {$frame_no + 1}]/$frame_count"
	    catch {exec [file join [bu_dir bin] fbclear] -F $_port \
		[lindex $_bgcolor 0] [lindex $_bgcolor 1] [lindex $_bgcolor 2]}
	    rtimage $rtimage_dict
	    if {$keep_dir ne ""} {
		set frame_tmp "${frame_file}.tmp-[pid]"
		exec [file join [bu_dir bin] fb-png] -w $_w -n $_n -F $_port $frame_tmp 2>@1
		file rename -force $frame_tmp $frame_file
	    } else {
		exec [file join [bu_dir bin] fb-pix] -w $_w -n $_n -F $_port $frame_file 2>@1
	    }
	    lappend animation_frames $frame_file
	}
        if {[info exists ::RtWizard::wizard_state(output_filename)] && $::RtWizard::wizard_state(output_filename) ne ""} {
	    set output $::RtWizard::wizard_state(output_filename)
	    set output_tmp "${output}.tmp-[pid][file extension $output]"
	    rtwizard_anim_write $output_tmp $_w $_n \
		[dict get $plan fps] [dict get $plan plays] {*}$animation_frames
	    file rename -force $output_tmp $output
	}
    } finally {
	if {$output_tmp ne ""} { catch {file delete -force $output_tmp} }
	if {$keep_dir eq ""} { foreach frame_file $animation_frames { catch {file delete -force $frame_file} } }
    }
    return 1
}

proc rtimage {rtimage_dict} {
    global tcl_platform
    global env
    set necessary_vars [list _dbfile _port _w _n _viewsize _orientation \
    _eye_pt _perspective _bgcolor _ecolor _necolor _occmode _gamma _benchmark_mode \
    _cut_plane _anim_commands _ao_samples _ao_radius _animate]
    set necessary_lists [list _color_objects _ghost_objects _edge_objects]

    # It's the responsibility of the calling function
    # to populate the dictionary with what is needed.
    # Make the variables for local processing.
    foreach param [dict keys $rtimage_dict] {
        set $param [dict get $rtimage_dict $param]
    }

    # Anything we don't already have from the dictionary
    # is assumed empty
    foreach var ${necessary_vars} {
      if {![info exists $var]} { set $var "" }
    }
    foreach var ${necessary_lists} {
      if {![info exists $var]} { set $var {} }
    }

    if {$_animate ne "" && $_animate &&
	[info exists ::RtWizard::wizard_state(make_animation)] &&
	$::RtWizard::wizard_state(make_animation)} {
	return [rtimage_animation $rtimage_dict]
    }

    set ar [ expr $_w.0 / $_n.0 ]

    if {$::tcl_platform(platform) == "windows"} {
	if {[catch {set dir $env(TMP)}]} {
	    return "make_image: env(TMP) does not exist"
	}
    } else {
	set dir "/tmp"

	if {![file exists $dir]} {
	    return "make_image: $dir does not exist"
	}
    }

    set pid [pid]
    set tgi [list [file join $dir $pid\_ghost.pix] ]
    set tfci [list [file join $dir $pid\_fc.pix] ]
    set tgfci [list [file join $dir $pid\_ghostfc.pix] ]
    set tmi [list [file join $dir $pid\_merge.pix] ]
    set tmi2 [list [file join $dir $pid\_merge2.pix] ]
    set tbw [list [file join $dir $pid\_bw.bw] ]
    set tmod [list [file join $dir $pid\_bwmod.bw] ]
    set tbwpix [list [file join $dir $pid\_bwpix.pix] ]

    set binpath [bu_dir bin]

    if {[llength $_color_objects]} {
	set have_color_objects 1

	set cmd [list [file join $binpath rt] -w $_w -n $_n]
	if {$_benchmark_mode != ""} {
	    lappend cmd $_benchmark_mode
	}
	if {$_cut_plane != ""} {
	    lappend cmd -k $_cut_plane
	}
	if {$_ao_samples > 0} {
	    set ao_set "set ambSamples=$_ao_samples"
	    if {$_ao_radius > 0} { append ao_set " ambRadius=$_ao_radius" }
	    lappend cmd -c $ao_set
	}
	lappend cmd -F $_port \
	    -V $ar \
	    -R \
	    -A 0.9 \
	    -p $_perspective \
	    -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
	    -c "viewsize $_viewsize" \
	    -c "orientation $_orientation" \
	    -c "eye_pt $_eye_pt"
	lappend cmd $_dbfile

	foreach obj $_color_objects {
	    lappend cmd $obj
	}

	#puts "RT (with fullcolor): $cmd"

	#
	# Run rt to generate the color insert
	#
	rtimage_exec_log $cmd $_log_file $_color_objects $_anim_commands

	# Look for color objects that also get edges
	if {[llength $_edge_objects] && [llength $_ecolor] == 3} {

	    set r [lindex $_ecolor 0]
	    set g [lindex $_ecolor 1]
	    set b [lindex $_ecolor 2]

	    if {[string is digit $r] && $r <= 255 ||
		[string is digit $g] && $g <= 255 ||
		[string is digit $b] && $b <= 255} {

		set fgMode [list set fg=[lindex $_ecolor 0],[lindex $_ecolor 1],[lindex $_ecolor 2]]

		set ce_objects {}
		set ne_objects {}
		foreach cobj $_color_objects {
		    set i [lsearch $_edge_objects $cobj]
		    if {$i != -1} {
			lappend ce_objects $cobj
		    } else {
			lappend ne_objects $cobj
		    }
		}

		if {[llength $ce_objects]} {
		    set bgMode [list set bg=[lindex $_bgcolor 0],[lindex $_bgcolor 1],[lindex $_bgcolor 2]]

		    set cmd [list [file join $binpath rtedge] -w $_w -n $_n]
		    if {$_benchmark_mode != ""} {
			lappend cmd $_benchmark_mode
		    }
		    if {$_cut_plane != ""} {
			lappend cmd -k $_cut_plane
		    }
		    lappend cmd -F $_port \
			-V $ar \
			-R \
			-A 0.9 \
			-p $_perspective \
			-c $fgMode \
			-c $bgMode \
			-c "set ov=1" \
			-c "viewsize $_viewsize" \
			-c "orientation $_orientation" \
			-c "eye_pt $_eye_pt"
		    lappend cmd $_dbfile

		    foreach obj $ce_objects {
			lappend cmd $obj
		    }
		}

		# !!! FIXME: this runs rt in regress-D ...
		#puts "RTEDGE (with fullcolor): $cmd"
		
		#
		# Run rtedge to generate the full-color with edges
		#
		rtimage_exec_log $cmd $_log_file $ce_objects $_anim_commands
	    }
	}

    } else {
	set have_color_objects 0

	# Put a blank image into the framebuffer
	catch {exec [file join $binpath fbclear] -F $_port [lindex $_bgcolor 0] [lindex $_bgcolor 1] [lindex $_bgcolor 2]}
    }

    set occlude_objects [lsort -unique [concat $_color_objects $_ghost_objects]]

    if {[llength $_ghost_objects]} {

	# Pull the image from the framebuffer
	catch {exec [file join $binpath fb-pix] -w $_w -n $_n -F $_port $tfci}

	set have_ghost_objects 1
	set cmd [list [file join $binpath rt] -w $_w -n $_n]
	if {$_benchmark_mode != ""} {
	    lappend cmd $_benchmark_mode
	}
	if {$_cut_plane != ""} {
	    lappend cmd -k $_cut_plane
	}
	if {$_ao_samples > 0} {
	    set ao_set "set ambSamples=$_ao_samples"
	    if {$_ao_radius > 0} { append ao_set " ambRadius=$_ao_radius" }
	    lappend cmd -c $ao_set
	}
	lappend cmd -o $tgi \
	    -V $ar \
	    -R \
	    -A 0.9 \
	    -p $_perspective \
	    -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
	    -c "viewsize $_viewsize" \
	    -c "orientation $_orientation" \
	    -c "eye_pt $_eye_pt"
	lappend cmd $_dbfile

	foreach obj $_ghost_objects {
	    lappend cmd $obj
	}

	#puts "RT (ghosted): $cmd"

	#
	# Run rt to generate the full-color version of the ghost image
	#
	rtimage_exec_log $cmd $_log_file $_ghost_objects $_anim_commands

	set cmd [list [file join $binpath rt] -w $_w -n $_n]
	if {$_benchmark_mode != ""} {
	    lappend cmd $_benchmark_mode
	}
	if {$_cut_plane != ""} {
	    lappend cmd -k $_cut_plane
	}
	if {$_ao_samples > 0} {
	    set ao_set "set ambSamples=$_ao_samples"
	    if {$_ao_radius > 0} { append ao_set " ambRadius=$_ao_radius" }
	    lappend cmd -c $ao_set
	}
	lappend cmd -o $tgfci \
	    -V $ar \
	    -R \
	    -A 0.9 \
	    -p $_perspective \
	    -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
	    -c "viewsize $_viewsize" \
	    -c "orientation $_orientation" \
	    -c "eye_pt $_eye_pt"
	lappend cmd $_dbfile

	foreach obj $occlude_objects {
	    lappend cmd $obj
	}

	#puts "RT (occluded): $cmd"
	
	#
	# Run rt to generate the full-color version of the occlude_objects (i.e. color and ghost)
	#
	rtimage_exec_log $cmd $_log_file $occlude_objects $_anim_commands

	#
	# Convert to ghost image
	#
	catch {exec [file join $binpath pix-bw] -e crt $tgi > $tbw}
	catch {exec [file join $binpath bwmod] -a 4 -d259 -r$_gamma -m255 $tbw > $tmod}
	catch {exec [file join $binpath bw-pix] $tmod > $tbwpix}

	set bgl "=[lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2]"
	catch {exec [file join $binpath pixmatte] -e $tfci $bgl $tbwpix $tfci > $tmi}
	catch {exec [file join $binpath pixmatte] -e $tgfci $bgl $tfci $tmi > $tmi2}

	# Put the image into the framebuffer
	catch {exec [file join $binpath pix-fb] -w $_w -n $_n -F $_port $tmi2}
    } else {
	set have_ghost_objects 0
    }

    if {[llength $_edge_objects]} {
	set have_edge_objects 1

	if {[llength $_ecolor] != 3} {
	    set fgMode [list set rc=1]
	} else {
	    set r [lindex $_ecolor 0]
	    set g [lindex $_ecolor 1]
	    set b [lindex $_ecolor 2]
	    if {![string is digit $r] || $r > 255 ||
		![string is digit $g] || $g > 255 ||
		![string is digit $b] || $b > 255} {
		set fgMode [list set rc=1]
	    } else {
		set fgMode [list set fg=[lindex $_ecolor 0],[lindex $_ecolor 1],[lindex $_ecolor 2]]
	    }
	}

	if {[llength $occlude_objects]} {
	    set coMode "-c {set om=$_occmode} -c {set oo=\\\"$occlude_objects\\\"}"
	    set bgMode [list set bg=[lindex $_necolor 0],[lindex $_necolor 1],[lindex $_necolor 2]]
	} else {
	    set coMode ""
	    set bgMode [list set bg=[lindex $_bgcolor 0],[lindex $_bgcolor 1],[lindex $_bgcolor 2]]
	}

	set cmd [list [file join $binpath rtedge] -w $_w -n $_n]
	if {$_benchmark_mode != ""} {
	    lappend cmd $_benchmark_mode
	}
	if {$_cut_plane != ""} {
	    lappend cmd -k $_cut_plane
	}
	lappend cmd -F $_port \
	    -V $ar \
	    -R \
	    -A 0.9 \
	    -p $_perspective \
	    -c $fgMode \
	    -c $bgMode
	if {[llength $occlude_objects]} {
	    lappend cmd -c "set om=$_occmode" -c "set oo=\"$occlude_objects\""
	}
	lappend cmd \
	    -c "viewsize $_viewsize" \
	    -c "orientation $_orientation" \
	    -c "eye_pt $_eye_pt"
	lappend cmd $_dbfile
	foreach obj $_edge_objects {
	    lappend cmd $obj
	}

	#puts "RTEDGE: $cmd"
	
	#
	# Run rtedge to generate the full-color version of the ghost image
	# !!! manually write an rtedge log
	rtimage_exec_log $cmd rtedge.log $_edge_objects $_anim_commands
    }

    catch {file delete -force $tgi}
    catch {file delete -force $tfci}
    catch {file delete -force $tgfci}
    catch {file delete -force $tmi}
    catch {file delete -force $tmi2}
    catch {file delete -force $tbw}
    catch {file delete -force $tmod}
    catch {file delete -force $tbwpix}

#end proc rtimage
}

#end namespace cadwidgets
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
