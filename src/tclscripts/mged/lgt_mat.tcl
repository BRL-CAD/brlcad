#                     L G T _ M A T . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
proc process_mdb { mdb_file array_name } {
    upvar $array_name mdb_array

    # open material database file
    if { [string length $mdb_file] == 0 } {
	puts "ERROR: missing name of material database"
	exit
    }

    if { [catch {open $mdb_file} fdb] } {
	puts "ERROR opening database file ($mdb_file)"
	puts $fdb
	exit
    }

    #read material database
    set mdb [split [read $fdb] "\n"]

    # process MDB into an array

    set max_index 0
    set index 0
    set len [llength $mdb]

    while { $index < $len } {
	set m_name [string trim [lindex $mdb $index]]
	incr index
	if { $index >= $len } break
	set m_index [string trim [lindex $mdb $index]]
	incr index
	set m_shine [string trim [lindex $mdb $index]]
	incr index
	set m_spec_wgt [string trim [lindex $mdb $index]]
	incr index
	set m_diff_wgt [string trim [lindex $mdb $index]]
	incr index
	set m_trans [string trim [lindex $mdb $index]]
	incr index
	set m_refl [string trim [lindex $mdb $index]]
	incr index
	set m_ri [string trim [lindex $mdb $index]]
	incr index
	set m_rgb [string trim [lindex $mdb $index]]
	incr index
	set m_mode [string trim [lindex $mdb $index]]
	incr index

	if { $m_name == "(null)" } continue

	set mdb_array($m_index) [list $m_name "sh" $m_shine "sp" $m_spec_wgt "di" $m_diff_wgt "tr" $m_trans "re" $m_refl "ri" $m_ri $m_rgb $m_mode]

    }
}

proc apply_lgt_mat { args } {

    # set defaults
    set mode 0
    set mdb_file ""

    # mode: Tcl
    #    0 - initially apply lgt material database to model
    #    1 - reapply lgt material database (using info stored in attributes)
    #    2 - undo (revert to old shader parameters using saved attributes)

    # process options
    set argc [llength $args]
    for { set index 0 } { $index < $argc } { incr index } {
	set argv [lindex $args $index ]
	switch -exact -- $argv {
	    "-r" {
		set mode 1
	    }
	    "-u" {
		set mode 2
	    }
	    default {
		if { [string index $argv 0] == "-" } {
		    puts "Illegal option $argv"
		    puts "\tChoices are:"
		    puts "\t\t no options - apply lgt material database"
		    puts "\t\t -r         - reapply lgt material database"
		    puts "\t\t -u         - undo (remove lgt database info)"
		    exit
		} else {
		    set mdb_file $argv
		}
	    }
	}
    }

    if { $mode < 2 } {
	process_mdb $mdb_file "mdb_array"
    }


    foreach comb [ls -c] {

	if { $mode == 0 } {
	    # initial application, check shader params for "mid"
	    set shader [db get $comb shader]
	} elseif { $mode == 1 } {
	    # reapply, find "mid" in attribute "lgt_mdb_params"
	    if { [catch {attr get $comb "lgt_mdb_params"} shader] } continue
	} elseif { $mode == 2 } {
	    # undo, restore the saved parameters from attributes
	    if { [catch {attr get $comb "lgt_mdb_params"} shader] } continue
	    if { [catch {attr get $comb "old_inherit"} inherit] } continue
	    if { [catch {attr get $comb "old_rgb"} rgb] } {set rgb "none"}
	    puts " restoring saved values to $comb"
	    db adjust $comb shader $shader inherit $inherit rgb $rgb
	    continue
	}
	if { [llength $shader] < 2 } continue

	set shader_name [lindex $shader 0]
	set shader_params [lindex $shader 1]
	set index [lsearch -exact $shader_params "mid"]
	if { $index == -1 } continue

	incr index
	set mid [lindex $shader_params $index]

	if { [info exists mdb_array($mid)] } {
	    set mdb_entry $mdb_array($mid)

	    puts " Adjusting $comb"
	    if { $mode == 0 } {
		# initial application, save the current settings in attributes
		if { ![catch {attr get $comb lgt_mdb_params} old_attr ] } {
		    if { [string compare $old_attr $shader] != 0 } {
			puts "WARNING: $comb already has LGT parameters saved"
			puts "\tand they are different from the current settings"
			puts "\tthe old saved settings will be overwritten"
			puts "\t old settings: $old_attr"
		    }
		}
		set old_rgb [db get $comb "rgb"]
		if { [string compare $old_rgb "invalid"] == 0 } {
		    attr set $comb "lgt_mdb_params" $shader \
			    "old_inherit" [db get $comb "inherit"]
		} else {
		    attr set $comb "lgt_mdb_params" $shader "old_rgb"\
			    $old_rgb "old_inherit" [db get $comb "inherit"]
		}
	    }

	    # apply the lgt material properties
	    db adjust $comb shader [list "plastic" [lrange $mdb_entry 1 12]]
	    db adjust $comb inherit yes rgb [lindex $mdb_entry 13]
	}
    }
}

proc make_lgt_light { args } {
    set num_args [llength $args]
    if { $num_args != 1 && $num_args != 4 } {
	puts "ERROR: usage:"
	puts "\tmake_lgt_light light_region [x y z]"
	return
    }

    set light [lindex $args 0]
    if { $num_args == 4 } {
	set x [lindex $args 1]
	set y [lindex $args 2]
	set z [lindex $args 3]
    } else {
	set x 2500
	set y 4330
	set z 8660
    }

    if { [catch {db get $light tree} light_tree] } {
	# light object does not exist, create a new one
	set light_solid [make_name s.light@]
	db put $light_solid sph V "$x $y $z" A { 0.1 0 0 } \
		B { 0 0.1 0 } C { 0 0 0.1 }
	db put $light comb region yes id 1 shader light tree "l $light_solid"
    } else {
	# move existing light to the LGT default position
	set mged_state [status state]

	if { $mged_state != "VIEWING" } {
	    puts "This command cannot be run in $mged_state state, must be viewing"
	    return
	}

	set operator [lindex $light_tree 0]
	while { $operator != "l" } {
	    set light_tree [lindex $light_tree 1]
	    set operator [lindex $light_tree 0]
	}

	set member_name [lindex $light_tree 1]

	draw $light
	oed $light $member_name
	translate $x $y $z
	press accept
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
