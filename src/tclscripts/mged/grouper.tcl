#                        G R O U P E R . T C L
# BRL-CAD
#
# Copyright (c) 2005-2014 United States Government as represented by
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
# Allows primitives to be added or removed from a group by selecting
# them on the MGED display using a selection box (rubber band).
# Drawing the selection box (left to right), only primitives completely
# in the box are selected. Drawing the selection box (right to left),
# primitives completely and partly in the box are selected. Primitives
# are identified by the location of their vertices on the display.
# For example, a portion of a primitive may be within the selection box
# but if none of its vertices are then it is not considered within the
# section box. To start grouper use the command 'grouper' or 'gr' and
# to finish use 'done_grouper' or 'dg'. The center mouse button is used
# to create the selection box. Primitives in the group will be
# displayed in yellow. The '-p' places the primitive parent in the group
# instead of the primitive. With the '-p' option, if the primitive does
# not have a parent (i.e. is in the root) it will be ignored.
#
# NOTE: This command will not function as expected if a selected object
#       has matrices applied. The xpush command can resolve this
#       limitation.
#

namespace eval Grouper {
    set GroupNameGlobal ""
    set GrouperRunning 0
    set draw_orig ""
    set pos_orig ""
    set dim_orig ""
    set fb_orig ""
    set fb_all_orig ""
    set listen_orig ""
    set objs {}
    set erase_status ""
    set parent_only 0
    set center_orig ""
    set size_orig ""
    set ae_orig ""

    proc remdup { GroupName } {
	set ItemList [search $GroupName -maxdepth 1]
	set ItemList [lrange $ItemList 1 end]
	set ItemList [lsort -unique $ItemList]

	set found_tmp 0
	while { !$found_tmp } {
	    set tmp_grp [expr rand()]
	    if { [search -name $tmp_grp] == "" } {
		set found_tmp 1
	    }
	}
	eval g $tmp_grp $ItemList
	kill $GroupName
	mv $tmp_grp $GroupName
    }

    proc getObjInRectangle {} {
	variable parent_only
	set objs2 {}
	set size [dm size]
	set sizeX [lindex $size 0].0
	set sizeY [lindex $size 1].0
	set adjX [expr {($sizeX / 2.0) - 1.0}]
	set adjY [expr {($sizeY / 2.0) - 1.0}]
	set dim [rset r dim]
	set dimX [lindex $dim 0].0
	set dimY [lindex $dim 1].0

	if { $dimX == 0.0 || $dimY == 0.0 } {
	    return "invalid_selection"
	}

	set pos [rset r pos]
	set posX [lindex $pos 0].0
	set posY [lindex $pos 1].0
	set posX [expr ($posX - $adjX) / $adjX]
	set posY [expr ($posY - $adjY) / $adjY]
	set dimX [expr ($dimX / $adjX)]
	set dimY [expr ($dimY / $adjY)]

	if { $dimX > 0 } {
	    # Rectangle was created left-to-right. Select only
	    # objects completely in the rectangle.
	    set objs [select $posX $posY $dimX $dimY]
	} else {
	    # Rectangle was created right-to-left. Select everything
	    # completely and partly in the rectangle.
	    set objs [select -p $posX $posY $dimX $dimY]
	}

	if { $objs == "" } {
	    return "no_result"
	}

	if { $parent_only != 1 } {
	    foreach obj $objs {
		set obj2 [file tail $obj]
		lappend objs2 $obj2
	    }
	} else {
	    foreach obj $objs {
		set obj2 [file tail [file dirname $obj]]
		# skip objects with no parent
		if { $obj2 != "" } {
		    lappend objs2 $obj2
		}
	    }
	}

	if { $objs2 == {} } {
	    return "no_result"
	}

	return $objs2
    }

    proc do_grouper { GroupName Boolean ListLimit } {
	variable objs
	variable GroupNameGlobal
	variable center_orig
	variable size_orig
	variable ae_orig

	set GroupNameGlobal $GroupName
	set center_orig [center]
	set size_orig [size]
	set ae_orig [ae]

	# Temporarily erase group from display while getting objects in rectangle"
	after 1 {
	    if { [search -name $::Grouper::GroupNameGlobal] != "" } {
		set ::Grouper::erase_status [erase $::Grouper::GroupNameGlobal]
	    } else {
		set ::Grouper::erase_status 1
	    }
	}

	# Wait for the group to be erased from the display before we continue
	vwait ::Grouper::erase_status

	after 1 {set ::Grouper::objs [::Grouper::getObjInRectangle]}

	puts stdout "Selection running ..."
	vwait ::Grouper::objs

	if { $objs == "invalid_selection" } {
	    puts stdout "Invalid selection (zero area selection box)."
	    # highlight in yellow current group
	    if { [search -name $GroupName] != "" } {
		e -C255/255/0 $GroupName
		center $center_orig
		size $size_orig
		ae $ae_orig
	    }
	    return
	}

	if { $objs == "no_result" } {
	    puts stdout "Nothing selected."
	    # highlight in yellow current group
	    if { [search -name $GroupName] != "" } {
		e -C255/255/0 $GroupName
		center $center_orig
		size $size_orig
		ae $ae_orig
	    }
	    return
	}

	set objs [lsort -unique $objs]
	set tot_obj_in_rect [llength $objs]

	if { [search -name $GroupName] == "" } {
	    set grp_obj_list_before ""
	    set grp_obj_list_len_before 0
	} else {
	    set grp_obj_list_before [search $GroupName -maxdepth 1]
	    set grp_obj_list_len_before [expr [llength $grp_obj_list_before] - 1]
	}

	if { $Boolean == "+" } {
	    foreach obj $objs {
		g $GroupName $obj
	    }
	} else {
	    foreach obj $objs {
		catch {rm $GroupName $obj} tmp_msg
	    }
	}

	if { ([search -name $GroupName] != "") && ($Boolean == "+") } {
	    remdup $GroupName
	}

	if { [search -name $GroupName] == "" } {
	    set grp_obj_list_after ""
	    set grp_obj_list_len_after 0
	} else {
	    set grp_obj_list_after [search $GroupName -maxdepth 1]
	    set grp_obj_list_len_after [expr [llength $grp_obj_list_after] - 1]
	}

	if { $grp_obj_list_len_after > $grp_obj_list_len_before } {
	    set objcnt_chng [expr $grp_obj_list_len_after - $grp_obj_list_len_before]
	    puts stdout "$objcnt_chng of $tot_obj_in_rect selected objects added to group '$GroupName'."
	} elseif { $grp_obj_list_len_after < $grp_obj_list_len_before } {
	    set objcnt_chng [expr $grp_obj_list_len_before - $grp_obj_list_len_after]
	    puts stdout "$objcnt_chng of $tot_obj_in_rect selected objects removed from group '$GroupName'."
	} else {
	    puts stdout "No change to group '$GroupName'."
	}

	set objcnt 0

	# hack to prevent script lockup on windows
	set max_objs_per_line 200

	if { ($ListLimit > 0 && $ListLimit <= $max_objs_per_line) ||
	     ($ListLimit > 0 && $tot_obj_in_rect <= $max_objs_per_line) } {
	    puts stdout "Selected object list\:"
	    foreach obj $objs {
		incr objcnt
		if { $objcnt > $ListLimit } {
		    puts -nonewline stdout "\nListed $ListLimit of $tot_obj_in_rect selected objects."
		    break;
		}
		puts -nonewline stdout "$obj "
	    }
	    puts -nonewline stdout "\n"
	} elseif { $ListLimit > 0 } {
	    puts stdout "Selected object list\:"
	    foreach obj $objs {
		incr objcnt
		if { $objcnt > $ListLimit } {
		    puts -nonewline stdout "\nListed $ListLimit of $tot_obj_in_rect selected objects."
		    break;
		}
		puts stdout "$objcnt\t$obj"
	    }
	}

	# highlight in yellow current group
	if { [search -name $GroupName] != "" } {
	    e -C255/255/0 $GroupName
	    center $center_orig
	    size $size_orig
	    ae $ae_orig
	}

	puts stdout "Selection complete."
    }
}; # end namespace

proc gr { args } {
    grouper $args
}

proc grouper { args } {
    global mged_gui mged_default mged_players

    set GroupName ""
    set Boolean ""
    set ListLimit 0
    set ::Grouper::parent_only 0
    set usage_msg "Usage: {GroupName} {+|-} \[ListLimit\] \[-p\]"

    if {$::Grouper::GrouperRunning == 1} {
	return "Grouper already running, use the 'dg' command to exit grouper before restarting."
    }

    set args [join $args]
    set args_cnt [llength $args]

    if { $args_cnt < 2 || $args_cnt > 4 } {
	puts stdout "$usage_msg"
	return
    }

    set listlimit_flag_cnt 0
    set boolean_flag_cnt 0
    set groupname_flag_cnt 0
    set invalid_flag_cnt 0

    foreach arg $args {
	if { $arg == "-p" } {
	    set ::Grouper::parent_only 1
	} elseif { [string is integer $arg] && $arg > 0 } {
	    incr listlimit_flag_cnt
	    set ListLimit $arg
	} elseif { ($arg == "+" || $arg == "-") && ($arg != $Boolean) } {
	    incr boolean_flag_cnt
	    set Boolean $arg
	} elseif { ([string index $arg 0] != "-") && ![string is integer $arg] } {
	    incr groupname_flag_cnt
	    set GroupName $arg
	} else {
	    puts stdout "Invalid parameter: \"$arg\""
	    incr invalid_flag_cnt
	}
    }

    if { $groupname_flag_cnt != 1 || $boolean_flag_cnt != 1 ||
	 $listlimit_flag_cnt > 1 || $invalid_flag_cnt != 0} {
	puts stdout "$usage_msg"
	return
    }

    set ::Grouper::GrouperRunning 1

    for {set i 0} {1} {incr i} {
	set id [subst $mged_default(id)]_$i
	if { [lsearch -exact $mged_players $id] != -1 } {
	    break;
	}
    }

    set ::Grouper::draw_orig [rset r draw]
    set ::Grouper::pos_orig [rset r pos]
    set ::Grouper::dim_orig [rset r dim]
    set ::Grouper::fb_orig [rset var fb]
    set ::Grouper::fb_all_orig [rset var fb_all]
    set ::Grouper::listen_orig [rset var listen]
    set ::Grouper::center_orig [center]
    set ::Grouper::size_orig [size]
    set ::Grouper::ae_orig [ae]

    set mged_gui($id,mouse_behavior) p
    set_mouse_behavior $id

    rset vars fb 0
    rset r draw 1

    bind $mged_gui($id,active_dm) <ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; ::Grouper::do_grouper $GroupName $Boolean $ListLimit"
    bind $mged_gui($id,active_dm) <Control-ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; done_grouper"

    # highlight in yellow current group
    if { [search -name $GroupName] != "" } {
	e -C255/255/0 $GroupName
	center $::Grouper::center_orig
	size $::Grouper::size_orig
	ae $::Grouper::ae_orig
    }
}

proc dg {} {
    done_grouper
}

proc done_grouper {} {
    global mged_gui mged_default mged_players

    if {$::Grouper::GrouperRunning != 1} {
	return "Grouper not running."
    }

    for {set i 0} {1} {incr i} {
	set id [subst $mged_default(id)]_$i
	if { [lsearch -exact $mged_players $id] != -1 } {
	    break;
	}
    }

    set mged_gui($id,mouse_behavior) d
    set_mouse_behavior $id

    bind $mged_gui($id,active_dm) <Control-ButtonRelease-2> ""
    bind $mged_gui($id,active_dm) <ButtonRelease-2> ""

    # remove yellow highlights from the display
    if { [search -name $::Grouper::GroupNameGlobal] != "" } {
	erase $::Grouper::GroupNameGlobal
    }
    set ::Grouper::GrouperRunning 0

    # return display to state before grouper changed it
    rset r draw $::Grouper::draw_orig
    rset r pos $::Grouper::pos_orig
    rset r dim $::Grouper::dim_orig
    rset var fb $::Grouper::fb_orig
    rset var fb_all $::Grouper::fb_all_orig
    rset var listen $::Grouper::listen_orig

    puts stdout "Grouper done."
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
