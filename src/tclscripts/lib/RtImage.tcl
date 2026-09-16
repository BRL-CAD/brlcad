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

proc rtimage_exec_log {cmd log_file} {
    if {[catch {exec {*}$cmd >& $log_file} msg]} {
	return -code error "rtimage command failed: $cmd\n$msg"
    }
}

proc rtimage_cut_animation {rtimage_dict} {
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
    if {![info exists ::RtWizard::wizard_state(output_filename)] ||
	$::RtWizard::wizard_state(output_filename) eq ""} {
	return -code error "rtwizard: cutting-plane animation requires a file output"
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
    set frame_count $::RtWizard::wizard_state(cut_steps)
    if {$frame_count < 2} {
	return -code error "rtwizard: animation frame count must be at least 2"
    }

    dict set rtimage_dict _animate 0
    set animation_frames {}
    set frame_dir [file dirname $::RtWizard::wizard_state(output_filename)]
    try {
	for {set frame_no 0} {$frame_no < $frame_count} {incr frame_no} {
	    set fraction [expr {double($frame_no) / double($frame_count - 1)}]
	    set plane_dist [expr {$sweep_first + $fraction * ($sweep_last - $sweep_first)}]
	    set plane_pt [vscale $cut_dir $plane_dist]
	    dict set rtimage_dict _cut_plane [format "%.15g,%.15g,%.15g,%.15g,%.15g,%.15g" \
		[lindex $plane_pt 0] [lindex $plane_pt 1] [lindex $plane_pt 2] \
		[lindex $cut_dir 0] [lindex $cut_dir 1] [lindex $cut_dir 2]]
	    puts "Rendering frame [expr {$frame_no + 1}]/$frame_count"
	    catch {exec [file join [bu_dir bin] fbclear] -F $_port \
		[lindex $_bgcolor 0] [lindex $_bgcolor 1] [lindex $_bgcolor 2]}
	    rtimage $rtimage_dict
	    set frame_file [file join $frame_dir [format ".rtwizard-%d-%06d.pix" [pid] $frame_no]]
	    exec [file join [bu_dir bin] fb-pix] -w $_w -n $_n -F $_port $frame_file 2>@1
	    lappend animation_frames $frame_file
	}
	rtwizard_anim_write $::RtWizard::wizard_state(output_filename) $_w $_n \
	    $::RtWizard::wizard_state(animation_fps) {*}$animation_frames
    } finally {
	foreach frame_file $animation_frames { catch {file delete -force $frame_file} }
    }
    return 1
}

proc rtimage {rtimage_dict} {
    global tcl_platform
    global env
    set necessary_vars [list _dbfile _port _w _n _viewsize _orientation \
    _eye_pt _perspective _bgcolor _ecolor _necolor _occmode _gamma _benchmark_mode \
    _cut_plane _ao_samples _ao_radius _animate]
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
	return [rtimage_cut_animation $rtimage_dict]
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
	    -c "eye_pt $_eye_pt" \
	    $_dbfile

	foreach obj $_color_objects {
	    lappend cmd $obj
	}

	#puts "RT (with fullcolor): $cmd"

	#
	# Run rt to generate the color insert
	#
	rtimage_exec_log $cmd $_log_file

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
			-c "eye_pt $_eye_pt" \
			$_dbfile

		    foreach obj $ce_objects {
			lappend cmd $obj
		    }
		}

		# !!! FIXME: this runs rt in regress-D ...
		#puts "RTEDGE (with fullcolor): $cmd"
		
		#
		# Run rtedge to generate the full-color with edges
		#
		rtimage_exec_log $cmd $_log_file
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
	    -c "eye_pt $_eye_pt" \
	    $_dbfile

	foreach obj $_ghost_objects {
	    lappend cmd $obj
	}

	#puts "RT (ghosted): $cmd"

	#
	# Run rt to generate the full-color version of the ghost image
	#
	rtimage_exec_log $cmd $_log_file

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
	    -c "eye_pt $_eye_pt" \
	    $_dbfile

	foreach obj $occlude_objects {
	    lappend cmd $obj
	}

	#puts "RT (occluded): $cmd"
	
	#
	# Run rt to generate the full-color version of the occlude_objects (i.e. color and ghost)
	#
	rtimage_exec_log $cmd $_log_file

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
	    -c "eye_pt $_eye_pt" \
	    $_dbfile
	foreach obj $_edge_objects {
	    lappend cmd $obj
	}

	#puts "RTEDGE: $cmd"
	
	#
	# Run rtedge to generate the full-color version of the ghost image
	# !!! manually write an rtedge log
	rtimage_exec_log $cmd rtedge.log
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
