#                          D A T A U T I L S . T C L
# BRL-CAD
#
# Copyright (c) 1998-2010 United States Government as represented by
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
#	The DataUtils class provides utility functions for manipulating
#       data_arrow, data_axes, data_labels and data_lines.
#

::itcl::class DataUtils {
    constructor {args} {}
    destructor {}

    public {
	proc dataPick {_ged _archer _group
		       _pdata _data_arrows_name _sdata_arrows_name
		       _data_axes_name _sdata_axes_name
		       _data_labels_name _sdata_labels_name
		       _data_lines_name _sdata_lines_name}
    }
}

################################### Public Section ###################################

::itcl::body DataUtils::dataPick {_ged _archer _group
    _pdata _data_arrows_name _sdata_arrows_name
    _data_axes_name _sdata_axes_name
    _data_labels_name _sdata_labels_name
    _data_lines_name _sdata_lines_name} {

    if {$_pdata == ""} {
	return
    }

    set dcmd [lindex $_pdata 0]
    set dindex [lindex $_pdata 1]
    set dpoint [lindex $_pdata 2]

    # skip past the group name in the _GLOBAL attribute
    incr dindex

    switch -- $dcmd {
	"data_arrows" {
	    if {[catch {$_ged attr get _GLOBAL $_data_arrows_name} dal]} {
		set dal {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_arrows_name} sdal]} {
		set sdal {}
	    }

	    # Get the regular/unselected list for the current group
	    set i [lsearch -index 0 $dal $_group]
	    if {$i != -1} {
		set da [lindex $dal $i]
	    } else {
		# This should never happen
		return
	    }

	    # Remove arrow points from regular/unselected list for the current group
	    if {[expr {$dindex % 2}]} {
		set j [expr {$dindex + 1}]
		set points [lrange $da $dindex $j]
		set da [lreplace $da $dindex $j]
	    } else {
		set j [expr {$dindex - 1}]
		set points [lrange $da $j $dindex]
		set da [lreplace $da $j $dindex]
	    }
	    set dal [lreplace $dal $i $i $da]

	    # Get the selected list for the current group and add points to it
	    set i [lsearch -index 0 $sdal $_group]
	    if {$i != -1} {
		set sda [lindex $sdal $i]
		eval lappend sda $points
		set sdal [lreplace $sdal $i $i $sda]
	    } else {
		lappend sdal [eval list $_group $points]
		set sda [lindex $sdal end]
	    }

	    $_archer attr set _GLOBAL $_data_arrows_name $dal
	    $_archer attr set _GLOBAL $_sdata_arrows_name $sdal

	    $_ged refresh_off
	    $_ged data_arrows points [lrange $da 1 end]
	    $_ged sdata_arrows points [lrange $sda 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"data_axes" {
	    if {[catch {$_ged attr get _GLOBAL $_data_axes_name} dal]} {
		set dal {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_axes_name} sdal]} {
		set sdal {}
	    }

	    # Get the regular/unselected list for the current group
	    set i [lsearch -index 0 $dal $_group]
	    if {$i != -1} {
		set da [lindex $dal $i]
	    } else {
		# This should never happen
		return
	    }

	    # Save the data for later
	    set saved_data [lindex $da $dindex]

	    # Remove point from regular/unselected list for the current group
	    set da [lreplace $da $dindex $dindex]
	    set dal [lreplace $dal $i $i $da]

	    # Get the selected list for the current group and add point to it
	    set i [lsearch -index 0 $sdal $_group]
	    if {$i != -1} {
		set sda [lindex $sdal $i]
		lappend sda $saved_data
		set sdal [lreplace $sdal $i $i $sda]
	    } else {
		lappend sdal [list $_group $saved_data]
		set sda [lindex $sdal end]
	    }

	    $_archer attr set _GLOBAL $_data_axes_name $dal
	    $_archer attr set _GLOBAL $_sdata_axes_name $sdal

	    $_ged refresh_off
	    $_ged data_axes points [lrange $da 1 end]
	    $_ged sdata_axes points [lrange $sda 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"data_labels" {
	    if {[catch {$_ged attr get _GLOBAL $_data_labels_name} dll]} {
		set dll {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_labels_name} sdll]} {
		set sdll {}
	    }

	    # Get the regular/unselected list for the current group
	    set i [lsearch -index 0 $dll $_group]
	    if {$i != -1} {
		set dl [lindex $dll $i]
	    } else {
		# This should never happen
		return
	    }

	    # Save the data for later
	    set saved_data [lindex $dl $dindex]

	    # Remove label from regular/unselected list for the current group
	    set dl [lreplace $dl $dindex $dindex]
	    set dll [lreplace $dll $i $i $dl]

	    #dpoint is really a label and a point (i.e. {{some label} {0 0 0}})

	    # Get the selected list for the current group and add label to it
	    set i [lsearch -index 0 $sdll $_group]
	    if {$i != -1} {
		set sdl [lindex $sdll $i]
		lappend sdl $saved_data
		set sdll [lreplace $sdll $i $i $sdl]
	    } else {
		lappend sdll [list $_group $saved_data]
		set sdl [lindex $sdll end]
	    }

	    $_archer attr set _GLOBAL $_data_labels_name $dll
	    $_archer attr set _GLOBAL $_sdata_labels_name $sdll

	    $_ged refresh_off
	    $_ged data_labels labels [lrange $dl 1 end]
	    $_ged sdata_labels labels [lrange $sdl 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"data_lines" {
	    if {[catch {$_ged attr get _GLOBAL $_data_lines_name} dll]} {
		set dll {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_lines_name} sdll]} {
		set sdll {}
	    }

	    # Get the regular/unselected list for the current group
	    set i [lsearch -index 0 $dll $_group]
	    if {$i != -1} {
		set dl [lindex $dll $i]
	    } else {
		# This should never happen
		return
	    }

	    # Remove line points from regular/unselected list for the current group
	    if {[expr {$dindex % 2}]} {
		set j [expr {$dindex + 1}]
		set points [lrange $dl $dindex $j]
		set dl [lreplace $dl $dindex $j]
	    } else {
		set j [expr {$dindex - 1}]
		set points [lrange $dl $j $dindex]
		set dl [lreplace $dl $j $dindex]
	    }
	    set dll [lreplace $dll $i $i $dl]

	    # Get the selected list for the current group and add point to it
	    set i [lsearch -index 0 $sdll $_group]
	    if {$i != -1} {
		set sdl [lindex $sdll $i]
		eval lappend sdl $points
		set sdll [lreplace $sdll $i $i $sdl]
	    } else {
		lappend sdll [eval list $_group $points]
		set sdl [lindex $sdll end]
	    }

	    $_archer attr set _GLOBAL $_data_lines_name $dll
	    $_archer attr set _GLOBAL $_sdata_lines_name $sdll

	    $_ged refresh_off
	    $_ged data_lines points [lrange $dl 1 end]
	    $_ged sdata_lines points [lrange $sdl 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"sdata_arrows" {
	    if {[catch {$_ged attr get _GLOBAL $_data_arrows_name} dal]} {
		set dal {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_arrows_name} sdal]} {
		set sdal {}
	    }

	    # Get the selected list for the current group
	    set i [lsearch -index 0 $sdal $_group]
	    if {$i != -1} {
		set sda [lindex $sdal $i]
	    } else {
		# This should never happen
		return
	    }

	    # Remove arrow points from the selected list for the current group
	    if {[expr {$dindex % 2}]} {
		set j [expr {$dindex + 1}]
		set points [lrange $sda $dindex $j]
		set sda [lreplace $sda $dindex $j]
	    } else {
		set j [expr {$dindex - 1}]
		set points [lrange $sda $j $dindex]
		set sda [lreplace $sda $j $dindex]
	    }
	    set sdal [lreplace $sdal $i $i $sda]

	    # Get the regular/unselected list for the current group and add point to it
	    set i [lsearch -index 0 $dal $_group]
	    if {$i != -1} {
		set da [lindex $dal $i]
		eval lappend da $points
		set dal [lreplace $dal $i $i $da]
	    } else {
		lappend dal [eval list $_group $points]
		set da [lindex $dal end]
	    }

	    $_archer attr set _GLOBAL $_data_arrows_name $dal
	    $_archer attr set _GLOBAL $_sdata_arrows_name $sdal

	    $_ged refresh_off
	    $_ged data_arrows points [lrange $da 1 end]
	    $_ged sdata_arrows points [lrange $sda 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"sdata_axes" {
	    if {[catch {$_ged attr get _GLOBAL $_data_axes_name} dal]} {
		set dal {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_axes_name} sdal]} {
		set sdal {}
	    }

	    # Get the selected list for the current group
	    set i [lsearch -index 0 $sdal $_group]
	    if {$i != -1} {
		set sda [lindex $sdal $i]
	    } else {
		# This should never happen
		return
	    }

	    # Save the data for later
	    set saved_data [lindex $sda $dindex]

	    # Remove point from regular/unselected list for the current group
	    set sda [lreplace $sda $dindex $dindex]
	    set sdal [lreplace $sdal $i $i $sda]

	    # Get the regular/unselected list for the current group and add point to it
	    set i [lsearch -index 0 $dal $_group]
	    if {$i != -1} {
		set da [lindex $dal $i]
		lappend da $saved_data
		set dal [lreplace $dal $i $i $da]
	    } else {
		lappend dal [list $_group $saved_data]
		set da [lindex $dal end]
	    }

	    $_archer attr set _GLOBAL $_data_axes_name $dal
	    $_archer attr set _GLOBAL $_sdata_axes_name $sdal

	    $_ged refresh_off
	    $_ged data_axes points [lrange $da 1 end]
	    $_ged sdata_axes points [lrange $sda 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"sdata_labels" {
	    if {[catch {$_ged attr get _GLOBAL $_data_labels_name} dll]} {
		set dll {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_labels_name} sdll]} {
		set sdll {}
	    }

	    # Get the selected list for the current group
	    set i [lsearch -index 0 $sdll $_group]
	    if {$i != -1} {
		set sdl [lindex $sdll $i]
	    } else {
		# This should never happen
		return
	    }

	    # Save the data for later
	    set saved_data [lindex $sdl $dindex]

	    # Remove label from regular/unselected list for the current group
	    set sdl [lreplace $sdl $dindex $dindex]
	    set sdll [lreplace $sdll $i $i $sdl]

	    #dpoint is really a label and a point (i.e. {{some label} {0 0 0}})

	    # Get the regular/unselected list for the current group and add label to it
	    set i [lsearch -index 0 $dll $_group]
	    if {$i != -1} {
		set dl [lindex $dll $i]
		lappend dl $saved_data
		set dll [lreplace $dll $i $i $dl]
	    } else {
		lappend dll [list $_group $saved_data]
		set dl [lindex $dll end]
	    }

	    $_archer attr set _GLOBAL $_data_labels_name $dll
	    $_archer attr set _GLOBAL $_sdata_labels_name $sdll

	    $_ged refresh_off
	    $_ged data_labels labels [lrange $dl 1 end]
	    $_ged sdata_labels labels [lrange $sdl 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	"sdata_lines" {
	    if {[catch {$_ged attr get _GLOBAL $_data_lines_name} dll]} {
		set dll {}
	    }

	    if {[catch {$_ged attr get _GLOBAL $_sdata_lines_name} sdll]} {
		set sdll {}
	    }

	    # Get the selected list for the current group
	    set i [lsearch -index 0 $sdll $_group]
	    if {$i != -1} {
		set sdl [lindex $sdll $i]
	    } else {
		# This should never happen
		return
	    }

	    # Remove line points from regular/unselected list for the current group
	    if {[expr {$dindex % 2}]} {
		set j [expr {$dindex + 1}]
		set points [lrange $sdl $dindex $j]
		set sdl [lreplace $sdl $dindex $j]
	    } else {
		set j [expr {$dindex - 1}]
		set points [lrange $sdl $j $dindex]
		set sdl [lreplace $sdl $j $dindex]
	    }
	    set sdll [lreplace $sdll $i $i $sdl]

	    # Get the regular/unselected list for the current group and add point to it
	    set i [lsearch -index 0 $dll $_group]
	    if {$i != -1} {
		set dl [lindex $dll $i]
		eval lappend dl $points
		set dll [lreplace $dll $i $i $dl]
	    } else {
		lappend dll [eval list $_group $points]
		set dl [lindex $dll end]
	    }

	    $_archer attr set _GLOBAL $_data_lines_name $dll
	    $_archer attr set _GLOBAL $_sdata_lines_name $sdll

	    $_ged refresh_off
	    $_ged data_lines points [lrange $dl 1 end]
	    $_ged sdata_lines points [lrange $sdl 1 end]
	    $_ged refresh_on
	    $_ged refresh_all
	}
	default {
	    return
	}
    }
}

################################### End Public Section ###################################


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
