#                        G R O U P E R . T C L
# BRL-CAD
#
# Copyright (c) 2005-2012 United States Government as represented by
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
# displayed in yellow.
#


proc gr_childcount { args } {
    if { [llength $args] != 1 } {
	return "Usage: gr_childcount collection"
    }
    set collection [lindex $args 0]
    if { [lindex [get $collection] 0 ] != "comb" } {
	return "This is not a collection!"
    }
    set collData [lt $collection]
    return "[llength $collData]"	
}


proc gr_remdup { args } {
    if { [llength $args] != 1 } {
	return "Usage: gr_remdup collection"
    }
    set collection [lindex $args 0]

    if { [lindex [get $collection] 0 ] != "comb" } {
	return "This is not a collection!"
    }
    set collData [lt $collection]
    set beforeCnt [gr_childcount $collection]

    foreach row $collData {
	set rowBool [lindex $row 0]
	set rowName [lindex $row 1]
	if { $rowBool == "u" } {
	    rm $collection $rowName
	    g $collection $rowName
	}
    }
    set collData [lt $collection]
    set afterCnt [gr_childcount $collection]
    return "$beforeCnt \-\> $afterCnt "
}


proc gr_getObjInRectangle {} {
    set size [dm size]
    set sizeX [lindex $size 0].0
    set sizeY [lindex $size 1].0
    set adjX [expr {($sizeX / 2.0) - 1.0}]
    set adjY [expr {($sizeY / 2.0) - 1.0}]
    set dim [rset r dim]     
    set dimX [lindex $dim 0].0     
    set dimY [lindex $dim 1].0    

    if { $dimX == 0.0 || $dimY == 0.0 } {
	return "no_result"
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

    foreach obj $objs {
	set obj2 [file tail $obj]
	lappend objs2 $obj2
    }

    return $objs2
}


proc gr { GroupName Boolean } {
    grouper $GroupName $Boolean
}


proc grouper { GroupName Boolean } {
    global mged_gui mged_default mged_players

    for {set i 0} {1} {incr i} {
	set id [subst $mged_default(id)]_$i
	if { [lsearch -exact $mged_players $id] != -1 } {
	    break;
	}
    }

    if {$Boolean != "+" && $Boolean != "-" } {
	return "Please provide a Boolean of either + or -"
    }
    set mged_gui($id,mouse_behavior) p     
    set_mouse_behavior $id     
    rset vars fb 0     
    rset r draw 1     

    bind $mged_gui($id,active_dm) <ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; do_grouper $GroupName $Boolean" 
    bind $mged_gui($id,active_dm) <Control-ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; done_grouper" 

    # highlight in yellow current group
    if { [search -name $GroupName] != "" } {
	e -C255/255/0 $GroupName
    }
}


proc do_grouper { GroupName Boolean } {
    set objs [gr_getObjInRectangle]

    if { $objs == "no_result" } {
	puts stdout "Nothing selected"
	return
    }

    set objs [lsort -unique $objs]
    set tot_obj_in_rect [llength $objs]

    if { [search -name $GroupName] == "" } {
	set grp_obj_list_before ""
	set grp_obj_list_len_before 0
    } else {
	set grp_obj_list_before [search $GroupName]
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
	gr_remdup $GroupName
    }

    if { [search -name $GroupName] == "" } {
	set grp_obj_list_after ""
	set grp_obj_list_len_after 0
    } else {
	set grp_obj_list_after [search $GroupName]
	set grp_obj_list_len_after [expr [llength $grp_obj_list_after] - 1]
    }

    if { $grp_obj_list_len_after > $grp_obj_list_len_before } {
	set objcnt_chng [expr $grp_obj_list_len_after - $grp_obj_list_len_before]
	puts stdout "$objcnt_chng of $tot_obj_in_rect objects added to '$GroupName'"
    } elseif { $grp_obj_list_len_after < $grp_obj_list_len_before } {
	set objcnt_chng [expr $grp_obj_list_len_before - $grp_obj_list_len_after]
	puts stdout "$objcnt_chng of $tot_obj_in_rect objects removed from '$GroupName'"
    } else {
	puts stdout "No change to '$GroupName'"
    }

    if { $tot_obj_in_rect > 0 } {
	puts stdout "Selected objects\:\n$objs"
    } else {
	puts stdout "Nothing selected"
    }

    # highlight in yellow current group
    if { [search -name $GroupName] != "" } {
	e -C255/255/0 $GroupName
    }
}


proc dg {} {
    done_grouper
}


proc done_grouper {} {
    global mged_gui mged_default mged_players

    for {set i 0} {1} {incr i} {
	set id [subst $mged_default(id)]_$i
	if { [lsearch -exact $mged_players $id] != -1 } {
	    break;
	}
    }
    set mged_gui($id,mouse_behavior) d
    set_mouse_behavior $id
    rset r draw 0

    bind $mged_gui($id,active_dm) <ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle"
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
