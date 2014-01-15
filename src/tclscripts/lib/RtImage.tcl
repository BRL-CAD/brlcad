#                          R T I M A G E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2014 United States Government as represented by
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

namespace eval cadwidgets {
proc rtimage {rtimage_dict} {
    global tcl_platform
    global env
    set necessary_vars [list _dbfile _port _w _n _viewsize _orientation \
    _eye_pt _perspective _bgcolor _ecolor _necolor _occmode _gamma]
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

    set ar [ expr $_w.0 / $_n.0 ]

    if {$tcl_platform(platform) == "windows"} {
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
    set tgi [file join $dir $pid\_ghost.pix]
    set tfci [file join $dir $pid\_fc.pix]
    set tgfci [file join $dir $pid\_ghostfc.pix]
    set tmi [file join $dir $pid\_merge.pix]
    set tmi2 [file join $dir $pid\_merge2.pix]
    set tbw [file join $dir $pid\_bw.bw]
    set tmod [file join $dir $pid\_bwmod.bw]
    set tbwpix [file join $dir $pid\_bwpix.pix]

    set binpath [bu_brlcad_root "bin"]

    if {[llength $_color_objects]} {
	set have_color_objects 1
	set cmd [list [file join $binpath rt] -w $_w -n $_n \
		     -F $_port \
		     -V $ar \
		     -R \
		     -A 0.9 \
		     -p $_perspective \
		     -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
		     -c [list viewsize $_viewsize] \
		     -c [eval list orientation $_orientation] \
		     -c [eval list eye_pt $_eye_pt] \
		     $_dbfile]

	foreach obj $_color_objects {
	    lappend cmd $obj
	}

	#
	# Run rt to generate the color insert
	#
	catch {eval exec $cmd}

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
		    set coMode "-c {set ov=1}"
		    set bgMode [list set bg=[lindex $_bgcolor 0],[lindex $_bgcolor 1],[lindex $_bgcolor 2]]

		    set cmd [concat [file join $binpath rtedge] -w $_w -n $_n \
				 -F $_port \
				 -V $ar \
				 -R \
				 -A 0.9 \
				 -p $_perspective \
				 -c [list $fgMode] \
				 -c [list $bgMode] \
				 $coMode \
				 -c [list [list viewsize $_viewsize]] \
				 -c [list [eval list orientation $_orientation]] \
				 -c [list [eval list eye_pt $_eye_pt]] \
				 $_dbfile]

		    foreach obj $ce_objects {
			lappend cmd $obj
		    }
		}

		#
		# Run rtedge to generate the full-color with edges
		#
		catch {eval exec $cmd}

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
	set cmd [list [file join $binpath rt] -w $_w -n $_n \
		     -o $tgi \
		     -V $ar \
		     -R \
		     -A 0.9 \
		     -p $_perspective \
		     -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
		     -c [list viewsize $_viewsize] \
		     -c [eval list orientation $_orientation] \
		     -c [eval list eye_pt $_eye_pt] \
		     $_dbfile]

	foreach obj $_ghost_objects {
	    lappend cmd $obj
	}

	#
	# Run rt to generate the full-color version of the ghost image
	#
	catch {eval exec $cmd}

	set cmd [list [file join $binpath rt] -w $_w -n $_n \
		     -o $tgfci \
		     -V $ar \
		     -R \
		     -A 0.9 \
		     -p $_perspective \
		     -C [lindex $_bgcolor 0]/[lindex $_bgcolor 1]/[lindex $_bgcolor 2] \
		     -c [list viewsize $_viewsize] \
		     -c [eval list orientation $_orientation] \
		     -c [eval list eye_pt $_eye_pt] \
		     $_dbfile]

	foreach obj $occlude_objects {
	    lappend cmd $obj
	}

	#
	# Run rt to generate the full-color version of the occlude_objects (i.e. color and ghost)
	#
	catch {eval exec $cmd}

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
	    set coMode "-c {set ov=1}"
	    set bgMode [list set bg=[lindex $_bgcolor 0],[lindex $_bgcolor 1],[lindex $_bgcolor 2]]
	}

	set cmd [concat [list [file join $binpath rtedge]] -w $_w -n $_n \
		     -F $_port \
		     -V $ar \
		     -R \
		     -A 0.9 \
		     -p $_perspective \
		     -c [list $fgMode] \
		     -c [list $bgMode] \
		     $coMode \
		     -c [list [list viewsize $_viewsize]] \
		     -c [list [eval list orientation $_orientation]] \
		     -c [list [eval list eye_pt $_eye_pt]] \
		     [list $_dbfile]]

	foreach obj $_edge_objects {
	    lappend cmd $obj
	}

	#
	# Run rtedge to generate the full-color version of the ghost image
	#
	catch {eval exec $cmd}
    }

    catch {file delete -force $tgi}
    catch {file delete -force $tfci}
    catch {file delete -force $tgfci}
    catch {file delete -force $tmi}
    catch {file delete -force $tmi2}
    catch {file delete -force $tbw}
    catch {file delete -force $tmod}
    catch {file delete -force $tbwpix}
}
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
