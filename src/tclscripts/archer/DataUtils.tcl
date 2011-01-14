#                          D A T A U T I L S . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
	proc appendGlobalData {_ged _archer _group
			       _attr_name _data_cmd _data_subcmd
			       _data _index_begin _index_end}
	proc dataPick {_ged _archer _group
		       _pdata _data_arrows_name _sdata_arrows_name
		       _data_axes_name _sdata_axes_name
		       _data_labels_name _sdata_labels_name
		       _data_lines_name _sdata_lines_name}
	proc deleteGlobalData {_ged _archer _group _attr_name}
	proc measureLastDataPoints {_ged _archer _group _attr_name _pindex {_sindex -1}}
	proc updateData {_ged _archer _group
			 _attr_name _data_cmd _data_subcmd}
	proc updateGlobalData {_ged _archer _group _attr_name _data_cmd _data_subcmd}
    }
}

################################### Public Section ###################################

::itcl::body DataUtils::appendGlobalData {_ged _archer _group
                                          _attr_name _data_cmd _data_subcmd
                                          _data _index_begin _index_end} {
    if {$_group == ""} {
	$_archer putString "Please select a group before appending to $_attr_name."
	return
    }
    if {[catch {$_ged attr get _GLOBAL $_attr_name} dataList]} {
	set dataList {}
    }

    set i [lsearch -index 0 $dataList $_group]

    if {$i != -1} {
	set subDataList [lindex $dataList $i]
	eval lappend subDataList $_data
	set dataList [lreplace $dataList $i $i $subDataList]
    } else {
	lappend dataList [eval list [list $_group] $_data]
	set subDataList [lindex $dataList end]
    }

    set dataList [lsort -index 0 -dictionary $dataList]
    $_archer attr set _GLOBAL $_attr_name $dataList

    $_ged refresh_off
    set plist {}
    foreach item [lrange $subDataList 1 end] {
	lappend plist [lrange $item $_index_begin $_index_end]
    }
    catch {$_ged $_data_cmd $_data_subcmd $plist} msg
    $_ged refresh_on
    $_ged refresh_all
}

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
		lappend sdal [eval list [list $_group] $points]
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
		lappend sdll [eval list [list $_group] $points]
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
		lappend dal [eval list [list $_group] $points]
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
		lappend dll [eval list [list $_group] $points]
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

::itcl::body DataUtils::deleteGlobalData {_ged _archer _group _attr_name} {
    if {$_group == ""} {
	$_archer putString "Please select a group before deleting from $_attr_name."
	return
    }

    if {[catch {$_ged attr get _GLOBAL $_attr_name} dl]} {
	return
    }

    # Delete the data list for the specified group
    set i [lsearch -index 0 $dl $_group]
    if {$i == -1} {
	return
    }

    set dl [lreplace $dl $i $i]
    $_archer attr set _GLOBAL $_attr_name $dl
}

::itcl::body DataUtils::measureLastDataPoints {_ged _archer _group _attr_name _pindex {_sindex -1}} {
    if {[catch {$_ged attr get _GLOBAL $_attr_name} dl]} {
	return
    }

    # Get the data list for the specified group
    set i [lsearch -index 0 $dl $_group]
    if {$i != -1} {
	set gd [lindex $dl $i]
    } else {
	return
    }

    # Strip off the group name
    set data [lrange $gd 1 end]

    # If a valid index is specified for indicating selection
    # of data then collect the selected data.
    if {[string is digit $_sindex]} {
	set all_data $data
	set data {}
	foreach item $all_data {
	    if {[lindex $item $_sindex]} {
		lappend data $item
	    }
	}
    }

    set last_index [expr {[llength $data] - 1}]
    if {$last_index < 1} {
	return
    }

    set pindex_end [expr {$_pindex + 2}]

    # Make sure dataA has enough values
    set dataA [lindex $data end-1]
    set last_index [expr {[llength $dataA] - 1}]
    if {$last_index < $pindex_end} {
	return
    }

    # Make sure dataB has enough values
    set dataB [lindex $data end]
    set last_index [expr {[llength $dataB] - 1}]
    if {$last_index < $pindex_end} {
	return
    }

    if {[catch {
	set ptA [lrange $dataA $_pindex $pindex_end]
	set ptB [lrange $dataB $_pindex $pindex_end]
	set dist [vmagnitude [vsub2 $ptB $ptA]]
	set dist [expr {$dist * [$_ged base2local]}]
        } msg]} {
	return
    }
    
    $_archer putString "Measured distance between data points: $dist [$_ged units -s]."
    $_archer setStatusString "Measured distance between data points: $dist [$_ged units -s]."
}

::itcl::body DataUtils::updateData {_ged _archer _group
                                    _attr_name _data_cmd _data_subcmd} {
    if {[catch {$_ged attr get _GLOBAL $_attr_name} dal]} {
	set dal {}
    }

    set i [lsearch -index 0 $dal $_group]
    if {$i != -1} {
	set da [lindex $dal $i]
    } else {
	set da {}
    }

    $_ged refresh_off
    $_ged $_data_cmd $_data_subcmd [lrange $da 1 end]
    $_ged refresh_on
}

::itcl::body DataUtils::updateGlobalData {_ged _archer _group _attr_name _data_cmd _data_subcmd} {
    if {[catch {$_ged attr get _GLOBAL $_attr_name} gdl]} {
	set gdl {}
    }

    # Get the list for the current group
    set i [lsearch -index 0 $gdl $_group]
    if {$i == -1} {
	# This should never happen
	return
    }

    set data [$_ged $_data_cmd $_data_subcmd]
    set gdl [lreplace $gdl $i $i [eval list [list $_group] $data]]
    $_ged refresh_off
    catch {$_archer attr set _GLOBAL $_attr_name $gdl}
    $_ged refresh_on
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
