#                   E D I T _ M E N U . T C L
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
#	Routines for implementing Tcl/Tk edit menus.
#

## - init_solid_edit_menus
#
# Routine that build solid edit menus.
#
proc init_solid_edit_menus { stype menu } {
    global mged_players
    global mged_gui
    global edit_type

    if ![info exists mged_players] {
	return
    }

    set stype [cook_solid_type $stype $menu]
    init_solid_edit_menu_hoc $stype

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.menubar.settings.transform entryconfigure 2 -state normal
	set mged_gui($id,transform) "e"
	set_transform $id

	.$id.menubar.settings.coord entryconfigure 2 -state normal
	set mged_gui($id,coords) "o"
	mged_apply $id "set coords $mged_gui($id,coords)"

	.$id.menubar.settings.origin entryconfigure 3 -state normal
	set mged_gui($id,rotate_about) "k"
	mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"

	.$id.menubar.edit entryconfigure 0 -state disabled
	.$id.menubar.edit entryconfigure 1 -state disabled

	set cmds "set mged_gui($id,transform) e; \
		  set_transform $id
		  press \"Edit Menu\"; "
	set i [build_solid_edit_menus .$id.menubar.edit $id 0 $cmds $menu]

	.$id.menubar.edit insert $i separator

	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Rotate" -underline 0 -command "press srot; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Translate" -underline 0 -command "press sxy; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Scale" -underline 0 -command "press sscale; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "None Of Above" -underline 0 -command "set edit_solid_flag 0; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i

	.$id.menubar.edit insert $i separator

	incr i
	.$id.menubar.edit insert $i command -label "Reject" -underline 2 \
	    -command "press reject"
	incr i
	.$id.menubar.edit insert $i command -label "Accept" -underline 0 \
	    -command "press accept"
	incr i
	.$id.menubar.edit insert $i command -label "Apply" -underline 1 \
	    -command "sed_apply"
	incr i
	.$id.menubar.edit insert $i command -label "Reset" -underline 1 \
	    -command "sed_reset"
	incr i
	.$id.menubar.edit insert $i separator
    }
}

## - build_solid_edit_menus
#
# Routine that recursively builds solid edit menus.
#
proc build_solid_edit_menus { w id pos cmds menu } {
    global mged_gui
    global mged_default
    global edit_type

    # skip menu title
    foreach item_list [lrange $menu 1 end] {
	set item [lindex $item_list 0]
	set submenu [lindex $item_list 1]
	if {$item != "RETURN"} {
	    if {$submenu == {}} {
		$w insert $pos radiobutton \
		    -label $item \
		    -variable edit_type \
		    -command "$cmds; press \"$item\""
	    } else {
		set title [lindex [lindex $submenu 0] 0]
		$w insert $pos cascade \
		    -label $item \
		    -menu $w.menu$pos
		menu $w.menu$pos -title $title \
		    -tearoff $mged_default(tearoff_menus)

		build_solid_edit_menus $w.menu$pos $id 0 \
		    "$cmds; press \"$item\"" $submenu
	    }

	    incr pos
	}
    }

    return $pos
}

## - init_object_edit_menus
#
# Routine that build object edit menus.
#
proc init_object_edit_menus {} {
    global mged_players
    global mged_gui
    global edit_type

    if ![info exists mged_players] {
	return
    }

    init_object_edit_menu_hoc

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.menubar.settings.transform entryconfigure 2 -state normal
	set mged_gui($id,transform) "e"
	set_transform $id

	.$id.menubar.settings.coord entryconfigure 2 -state normal
	set mged_gui($id,coords) "o"
	mged_apply $id "set coords $mged_gui($id,coords)"

	.$id.menubar.settings.origin entryconfigure 3 -state normal
	set mged_gui($id,rotate_about) "k"
	mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"

	.$id.menubar.edit entryconfigure 0 -state disabled
	.$id.menubar.edit entryconfigure 1 -state disabled

	set reset_cmd "oed_reset"

	set i 0
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Scale" -command "press \"Scale\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "X move" -command "press \"X Move\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Y move" -command "press \"Y Move\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "XY move" -command "press \"XY Move\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Rotate" -command "press \"Rotate\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Scale X" -command "press \"Scale X\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Scale Y" -command "press \"Scale Y\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "Scale Z" -command "press \"Scale Z\"; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i radiobutton -variable edit_type \
	    -label "none of above" -command "set edit_object_flag 0; \
		set mged_gui($id,transform) e; set_transform $id"
	incr i
	.$id.menubar.edit insert $i separator
	incr i

	.$id.menubar.edit insert $i command -label "Reject" -underline 2 \
	    -command "press reject"
	incr i
	.$id.menubar.edit insert $i command -label "Accept" -underline 0 \
	    -command "press accept"
	incr i
	.$id.menubar.edit insert $i command -label "Apply" -underline 1 \
	    -command "oed_apply"
	incr i
	.$id.menubar.edit insert $i command -label "Reset" -underline 1 \
	    -command $reset_cmd

	incr i
	.$id.menubar.edit insert $i separator
    }
}

## - undo_edit_menus
#
# Routine that reconfigures the edit menu to its non-edit form
#
proc undo_edit_menus {} {
    global mged_players
    global mged_gui

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	destroy_edit_info $id

	while {1} {
	    if {[.$id.menubar.edit type 0] == "separator"} {
		.$id.menubar.edit delete 0
		continue
	    }

	    if {[.$id.menubar.edit entrycget 0 -label] != \
		    "Primitive Selection..."} {
		.$id.menubar.edit delete 0
	    } else {
		break
	    }
	}

	set submenus [winfo children .$id.menubar.edit]
	foreach submenu $submenus {
	    destroy $submenu
	}

	.$id.menubar.edit entryconfigure 0 -state normal
	.$id.menubar.edit entryconfigure 1 -state normal

	.$id.menubar.settings.transform entryconfigure 2 -state disabled
	if {$mged_gui($id,transform) == "e"} {
	    set mged_gui($id,transform) "v"
	    set_transform $id
	}

	.$id.menubar.settings.coord entryconfigure 2 -state disabled
	if {$mged_gui($id,coords) == "o"} {
	    set mged_gui($id,coords) "v"
	    mged_apply $id "set coords $mged_gui($id,coords)"
	}

	.$id.menubar.settings.origin entryconfigure 3 -state disabled
	if {$mged_gui($id,rotate_about) == "k"} {
	    set mged_gui($id,rotate_about) "v"
	    mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"
	}
    }
}

## - init_object_edit_menu_hoc
#
# Routine that initializes help on context
# for object edit menu entries.
#
proc init_object_edit_menu_hoc {} {
    hoc_register_menu_data "Edit" "Scale" \
	"Object Edit - Scale"\
	{{summary "Scale"}}
    hoc_register_menu_data "Edit" "X move" \
	"Object Edit - X move"\
	{{summary "X move"}}
    hoc_register_menu_data "Edit" "Y move" \
	"Object Edit - Y move"\
	{{summary "Y move"}}
    hoc_register_menu_data "Edit" "XY move" \
	"Object Edit - XY move"\
	{{summary "XY move"}}
    hoc_register_menu_data "Edit" "Rotate" \
	"Object Edit - Rotate"\
	{{summary "Rotate"}}
    hoc_register_menu_data "Edit" "Scale X" \
	"Object Edit - Scale X"\
	{{summary "Scale X"}}
    hoc_register_menu_data "Edit" "Scale Y" \
	"Object Edit - Scale Y"\
	{{summary "Scale Y"}}
    hoc_register_menu_data "Edit" "Scale Z" \
	"Object Edit - Scale Z"\
	{{summary "Scale Z"}}
    hoc_register_menu_data "Edit" "none of above" \
	"Object Edit - none of above"\
	{{summary "Reject"}}

    hoc_register_menu_data "Edit" "Reject" \
	"Object Edit - Reject"\
	{{summary "Reject"}}
    hoc_register_menu_data "Edit" "Accept" \
	"Object Edit - Accept"\
	{{summary "Accept"}}
    hoc_register_menu_data "Edit" "Reset" \
	"Object Edit - Reset"\
	{{summary "Reset"}}
}

## - init_solid_edit_menu_hoc
#
# Routine that initializes help on context
# for solid edit menu entries.
#
proc init_solid_edit_menu_hoc { stype } {
    # Generic solid edit operations
    hoc_register_menu_data "Edit" "Rotate" \
	"Primitive Edit - Rotate" \
	{{summary "Rotate"}}
    hoc_register_menu_data "Edit" "Translate" \
	"Primitive Edit - Translate" \
	{{summary "Translate"}}
    hoc_register_menu_data "Edit" "Scale" \
	"Primitive Edit - Scale" \
	{{summary "Scale"}}
    hoc_register_menu_data "Edit" "none of above" \
	"Primitive Edit - none of above" \
	{{summary "none of above"}}
    hoc_register_menu_data "Edit" "Reject" \
	"Primitive Edit - Reject" \
	{{summary "Reject"}}
    hoc_register_menu_data "Edit" "Accept" \
	"Primitive Edit - Accept" \
	{{summary "Accept"}}
    hoc_register_menu_data "Edit" "Apply" \
	"Primitive Edit - Apply" \
	{{summary "Apply"}}
    hoc_register_menu_data "Edit" "Reset" \
	"Primitive Edit - Reset" \
	{{summary "Reset"}}

    # Solid specific edit operations
    if { [llength $stype] > 1 } {
	set the_type [lindex $stype 0]
    } else {
	set the_type $stype
    }

    switch $the_type {
	ARB8 {
	    # ARB8 EDGES
	    hoc_register_menu_data "ARB8 EDGES" "move edge 12" \
		"Primitive Edit - move edge 12" \
		{{summary "move edge 12"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 23" \
		"Primitive Edit - move edge 23" \
		{{summary "move edge 23"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 34" \
		"Primitive Edit - move edge 34" \
		{{summary "move edge 34"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 14" \
		"Primitive Edit - move edge 14" \
		{{summary "move edge 14"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 15" \
		"Primitive Edit - move edge 15" \
		{{summary "move edge 15"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 26" \
		"Primitive Edit - move edge 26" \
		{{summary "move edge 26"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 56" \
		"Primitive Edit - move edge 56" \
		{{summary "move edge 56"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 67" \
		"Primitive Edit - move edge 67" \
		{{summary "move edge 67"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 78" \
		"Primitive Edit - move edge 78" \
		{{summary "move edge 78"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 58" \
		"Primitive Edit - move edge 58" \
		{{summary "move edge 58"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 37" \
		"Primitive Edit - move edge 37" \
		{{summary "move edge 37"}}
	    hoc_register_menu_data "ARB8 EDGES" "move edge 48" \
		"Primitive Edit - move edge 48" \
		{{summary "move edge 48"}}

	    # ARB8 FACES - MOVE
	    hoc_register_menu_data "ARB8 FACES" "move face 1234" \
		"Primitive Edit - move face 1234" \
		{{summary "move face 1234"}}
	    hoc_register_menu_data "ARB8 FACES" "move face 5678" \
		"Primitive Edit - move face 5678" \
		{{summary "move face 5678"}}
	    hoc_register_menu_data "ARB8 FACES" "move face 1584" \
		"Primitive Edit - move face 1584" \
		{{summary "move face 1584"}}
	    hoc_register_menu_data "ARB8 FACES" "move face 2376" \
		"Primitive Edit - move face 2376" \
		{{summary "move face 2376"}}
	    hoc_register_menu_data "ARB8 FACES" "move face 1265" \
		"Primitive Edit - move face 1265" \
		{{summary "move face 1265"}}
	    hoc_register_menu_data "ARB8 FACES" "move face 4378" \
		"Primitive Edit - move face 4378" \
		{{summary "move face 4378"}}

	    # ARB8 FACES - ROTATE
	    hoc_register_menu_data "ARB8 FACES" "rotate face 1234" \
		"Primitive Edit - rotate face 1234" \
		{{summary "rotate face 1234"}}
	    hoc_register_menu_data "ARB8 FACES" "rotate face 5678" \
		"Primitive Edit - rotate face 5678" \
		{{summary "rotate face 5678"}}
	    hoc_register_menu_data "ARB8 FACES" "rotate face 1584" \
		"Primitive Edit - rotate face 1584" \
		{{summary "rotate face 1584"}}
	    hoc_register_menu_data "ARB8 FACES" "rotate face 2376" \
		"Primitive Edit - rotate face 2376" \
		{{summary "rotate face 2376"}}
	    hoc_register_menu_data "ARB8 FACES" "rotate face 1265" \
		"Primitive Edit - rotate face 1265" \
		{{summary "rotate face 1265"}}
	    hoc_register_menu_data "ARB8 FACES" "rotate face 4378" \
		"Primitive Edit - rotate face 4378" \
		{{summary "rotate face 4378"}}
	}
	ARB7 {
	    # ARB7 EDGES
	    hoc_register_menu_data "ARB7 EDGES" "move edge 12" \
		"Primitive Edit - move edge 12" \
		{{summary "move edge 12"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 23" \
		"Primitive Edit - move edge 23" \
		{{summary "move edge 23"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 34" \
		"Primitive Edit - move edge 34" \
		{{summary "move edge 34"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 14" \
		"Primitive Edit - move edge 14" \
		{{summary "move edge 14"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 15" \
		"Primitive Edit - move edge 15" \
		{{summary "move edge 15"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 26" \
		"Primitive Edit - move edge 26" \
		{{summary "move edge 26"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 56" \
		"Primitive Edit - move edge 56" \
		{{summary "move edge 56"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 67" \
		"Primitive Edit - move edge 67" \
		{{summary "move edge 67"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 37" \
		"Primitive Edit - move edge 37" \
		{{summary "move edge 37"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 57" \
		"Primitive Edit - move edget 57" \
		{{summary "move edge 57"}}
	    hoc_register_menu_data "ARB7 EDGES" "move edge 45" \
		"Primitive Edit - move edge 45" \
		{{summary "move edge 45"}}
	    hoc_register_menu_data "ARB7 EDGES" "move point 5" \
		"Primitive Edit - move point 5" \
		{{summary "move point 5"}}

	    # ARB7 FACES - MOVE
	    hoc_register_menu_data "ARB7 FACES" "move face 1234" \
		"Primitive Edit - move face 1234" \
		{{summary "move face 1234"}}
	    hoc_register_menu_data "ARB7 FACES" "move face 2376" \
		"Primitive Edit - move face 2376" \
		{{summary "move face 2376"}}

	    # ARB7 FACES - ROTATE
	    hoc_register_menu_data "ARB7 FACES" "rotate face 1234" \
		"Primitive Edit - rotate face 1234" \
		{{summary "rotate face 1234"}}
	    hoc_register_menu_data "ARB7 FACES" "rotate face 2376" \
		"Primitive Edit - rotate face 2376" \
		{{summary "rotate face 2376"}}
	}
	ARB6 {
	    # ARB6 EDGES
	    hoc_register_menu_data "ARB6 EDGES" "move edge 12" \
		"Primitive Edit - move edge 12" \
		{{summary "move edge 12"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 23" \
		"Primitive Edit - move edge 23" \
		{{summary "move edge 23"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 34" \
		"Primitive Edit - move edge 34" \
		{{summary "move edge 34"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 14" \
		"Primitive Edit - move edge 14" \
		{{summary "move edge 14"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 15" \
		"Primitive Edit - move edge 15" \
		{{summary "move edge 15"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 25" \
		"Primitive Edit - move edge 25" \
		{{summary "move edge 25"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 36" \
		"Primitive Edit - move edge 36" \
		{{summary "move edge 36"}}
	    hoc_register_menu_data "ARB6 EDGES" "move edge 46" \
		"Primitive Edit - move edge 46" \
		{{summary "move edge 46"}}
	    hoc_register_menu_data "ARB6 EDGES" "move point 5" \
		"Primitive Edit - move point 5" \
		{{summary "move point 5"}}
	    hoc_register_menu_data "ARB6 EDGES" "move point 6" \
		"Primitive Edit - move point 6" \
		{{summary "move point 6"}}

	    # ARB6 FACES - MOVE
	    hoc_register_menu_data "ARB6 FACES" "move face 1234" \
		"Primitive Edit - move face 1234" \
		{{summary "move face 1234"}}
	    hoc_register_menu_data "ARB6 FACES" "move face 2365" \
		"Primitive Edit - move face 2365" \
		{{summary "move face 2365"}}
	    hoc_register_menu_data "ARB6 FACES" "move face 1564" \
		"Primitive Edit - move face 1564" \
		{{summary "move face 1564"}}
	    hoc_register_menu_data "ARB6 FACES" "move face 125" \
		"Primitive Edit - move face 125" \
		{{summary "move face 125"}}
	    hoc_register_menu_data "ARB6 FACES" "move face 346" \
		"Primitive Edit - move face 346" \
		{{summary "move face 346"}}

	    # ARB6 FACES - ROTATE
	    hoc_register_menu_data "ARB6 FACES" "rotate face 1234" \
		"Primitive Edit - rotate face 1234" \
		{{summary "rotate face 1234"}}
	    hoc_register_menu_data "ARB6 FACES" "rotate face 2365" \
		"Primitive Edit - rotate face 2365" \
		{{summary "rotate face 2365"}}
	    hoc_register_menu_data "ARB6 FACES" "rotate face 1564" \
		"Primitive Edit - rotate face 1564" \
		{{summary "rotate face 1564"}}
	    hoc_register_menu_data "ARB6 FACES" "rotate face 125" \
		"Primitive Edit - rotate face 125" \
		{{summary "rotate face 125"}}
	    hoc_register_menu_data "ARB6 FACES" "rotate face 346" \
		"Primitive Edit - rotate face 346" \
		{{summary "rotate face 346"}}
	}
	ARB5 {
	    # ARB5 EDGES
	    hoc_register_menu_data "ARB5 EDGES" "move edge 12" \
		"Primitive Edit - move edge 12" \
		{{summary "move edge 12"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 23" \
		"Primitive Edit - move edge 23" \
		{{summary "move edge 23"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 34" \
		"Primitive Edit - move edge 34" \
		{{summary "move edge 34"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 14" \
		"Primitive Edit - move edge 14" \
		{{summary "move edge 14"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 15" \
		"Primitive Edit - move edge 15" \
		{{summary "move edge 15"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 25" \
		"Primitive Edit - move edge 25" \
		{{summary "move edge 25"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 35" \
		"Primitive Edit - move edge 35" \
		{{summary "move edge 35"}}
	    hoc_register_menu_data "ARB5 EDGES" "move edge 45" \
		"Primitive Edit - move edge 45" \
		{{summary "move edge 45"}}
	    hoc_register_menu_data "ARB5 EDGES" "move point 5" \
		"Primitive Edit - move point 5" \
		{{summary "move point 5"}}

	    # ARB5 FACES - MOVE
	    hoc_register_menu_data "ARB5 FACES" "move face 1234" \
		"Primitive Edit - move face 1234" \
		{{summary "move face 1234"}}
	    hoc_register_menu_data "ARB5 FACES" "move face 125" \
		"Primitive Edit - move face 125" \
		{{summary "move face 125"}}
	    hoc_register_menu_data "ARB5 FACES" "move face 235" \
		"Primitive Edit - move face 235" \
		{{summary "move face 235"}}
	    hoc_register_menu_data "ARB5 FACES" "move face 345" \
		"Primitive Edit - move face 345" \
		{{summary "move face 345"}}
	    hoc_register_menu_data "ARB5 FACES" "move face 145" \
		"Primitive Edit - move face 145" \
		{{summary "move face 145"}}

	    # ARB5 FACES - ROTATE
	    hoc_register_menu_data "ARB5 FACES" "rotate face 1234" \
		"Primitive Edit - rotate face 1234" \
		{{summary "rotate face 1234"}}
	    hoc_register_menu_data "ARB5 FACES" "rotate face 125" \
		"Primitive Edit - rotate face 125" \
		{{summary "rotate face 125"}}
	    hoc_register_menu_data "ARB5 FACES" "rotate face 235" \
		"Primitive Edit - rotate face 235" \
		{{summary "rotate face 235"}}
	    hoc_register_menu_data "ARB5 FACES" "rotate face 345" \
		"Primitive Edit - rotate face 345" \
		{{summary "rotate face 345"}}
	    hoc_register_menu_data "ARB5 FACES" "rotate face 145" \
		"Primitive Edit - rotate face 145" \
		{{summary "rotate face 145"}}
	}
	ARB4 {
	    # ARB4 POINTS
	    hoc_register_menu_data "ARB4 POINTS" "move point 1" \
		"Primitive Edit - move point 1" \
		{{summary "move point 1"}}
	    hoc_register_menu_data "ARB4 POINTS" "move point 2" \
		"Primitive Edit - move point 2" \
		{{summary "move point 2"}}
	    hoc_register_menu_data "ARB4 POINTS" "move point 3" \
		"Primitive Edit - move point 3" \
		{{summary "move point 3"}}
	    hoc_register_menu_data "ARB4 POINTS" "move point 4" \
		"Primitive Edit - move point 4" \
		{{summary "move point 4"}}

	    # ARB4 FACES - MOVE
	    hoc_register_menu_data "ARB4 FACES" "move face 123" \
		"Primitive Edit - move face 123" \
		{{summary "move face 123"}}
	    hoc_register_menu_data "ARB4 FACES" "move face 124" \
		"Primitive Edit - move face 124" \
		{{summary "move face 124"}}
	    hoc_register_menu_data "ARB4 FACES" "move face 234" \
		"Primitive Edit - move face 234" \
		{{summary "move face 234"}}
	    hoc_register_menu_data "ARB4 FACES" "move face 134" \
		"Primitive Edit - move face 134" \
		{{summary "move face 134"}}

	    # ARB4 FACES - ROTATE
	    hoc_register_menu_data "ARB4 FACES" "rotate face 123" \
		"Primitive Edit - rotate face 123" \
		{{summary "rotate face 123"}}
	    hoc_register_menu_data "ARB4 FACES" "rotate face 124" \
		"Primitive Edit - rotate face 124" \
		{{summary "rotate face 124"}}
	    hoc_register_menu_data "ARB4 FACES" "rotate face 234" \
		"Primitive Edit - rotate face 234" \
		{{summary "rotate face 234"}}
	    hoc_register_menu_data "ARB4 FACES" "rotate face 134" \
		"Primitive Edit - rotate face 134" \
		{{summary "rotate face 134"}}
	}
	ars {
	    # ARS
	    hoc_register_menu_data "Edit" "pick vertex" \
		"Primitive Edit - pick vertex" \
		{{summary "pick vertex"}}
	    hoc_register_menu_data "Edit" "move point" \
		"Primitive Edit - move point" \
		{{summary "move point"}}
	    hoc_register_menu_data "Edit" "delete curve" \
		"Primitive Edit - delete curve" \
		{{summary "delete curve"}}
	    hoc_register_menu_data "Edit" "delete column" \
		"Primitive Edit - delete column" \
		{{summary "delete column"}}
	    hoc_register_menu_data "Edit" "dup curve" \
		"Primitive Edit - dup curve" \
		{{summary "dup curve"}}
	    hoc_register_menu_data "Edit" "dup column" \
		"Primitive Edit - dup column" \
		{{summary "dup column"}}
	    hoc_register_menu_data "Edit" "move curve" \
		"Primitive Edit - move curve" \
		{{summary "move curve"}}
	    hoc_register_menu_data "Edit" "move column" \
		"Primitive Edit - move column" \
		{{summary "move column"}}

	    # ARS PICK
	    hoc_register_menu_data "ARS PICK MENU" "pick vertex" \
		"Primitive Edit - pick vertex" \
		{{summary "pick vertex"}}
	    hoc_register_menu_data "ARS PICK MENU" "next vertex" \
		"Primitive Edit - next vertex" \
		{{summary "next vertex"}}
	    hoc_register_menu_data "ARS PICK MENU" "prev vertex" \
		"Primitive Edit - prev vertex" \
		{{summary "prev vertex"}}
	    hoc_register_menu_data "ARS PICK MENU" "next curve" \
		"Primitive Edit - next curve" \
		{{summary "next curve"}}
	    hoc_register_menu_data "ARS PICK MENU" "prev curve" \
		"Primitive Edit - prev curve" \
		{{summary "prev curve"}}
	}
	tor {
	    # TOR
	    hoc_register_menu_data "Edit" "scale radius 1" \
		"Primitive Edit - scale radius 1" \
		{{summary "scale radius 1"}}
	    hoc_register_menu_data "Edit" "scale radius 2" \
		"Primitive Edit - scale radius 2" \
		{{summary "scale radius 2"}}
	}
	eto {
	    # ETO
	    hoc_register_menu_data "Edit" "scale r" \
		"Primitive Edit - scale r" \
		{{summary "scale r"}}
	    hoc_register_menu_data "Edit" "scale D" \
		"Primitive Edit - scale D" \
		{{summary "scale D"}}
	    hoc_register_menu_data "Edit" "scale C" \
		"Primitive Edit - scale C" \
		{{summary "scale C"}}
	    hoc_register_menu_data "Edit" "rotate C" \
		"Primitive Edit - rotate C" \
		{{summary "rotate C"}}
	}
	ell {
	    # ELL
	    hoc_register_menu_data "Edit" "scale A" \
		"Primitive Edit - scale A" \
		{{summary "scale A"}}
	    hoc_register_menu_data "Edit" "scale B" \
		"Primitive Edit - scale B" \
		{{summary "scale B"}}
	    hoc_register_menu_data "Edit" "scale C" \
		"Primitive Edit - scale C" \
		{{summary "scale C"}}
	    hoc_register_menu_data "Edit" "scale A,B,C" \
		"Primitive Edit - scale A,B,C" \
		{{summary "scale A,B,C"}}
	}
	spl {
	    # SPLINE
	    hoc_register_menu_data "Edit" "pick vertex" \
		"Primitive Edit - pick vertex" \
		{{summary "pick vertex"}}
	    hoc_register_menu_data "Edit" "move vertex" \
		"Primitive Edit - move vertex" \
		{{summary "move vertex"}}
	}
	nmg {
	    # NMG
	    hoc_register_menu_data "Edit" "pick edge" \
		"Primitive Edit - pick edge" \
		{{summary "pick edge"}}
	    hoc_register_menu_data "Edit" "move edge" \
		"Primitive Edit - move edge" \
		{{summary "move edge"}}
	    hoc_register_menu_data "Edit" "split edge" \
		"Primitive Edit - split edge" \
		{{summary "split edge"}}
	    hoc_register_menu_data "Edit" "delete edge" \
		"Primitive Edit - delete edge" \
		{{summary "delete edge"}}
	    hoc_register_menu_data "Edit" "next eu" \
		"Primitive Edit - next eu" \
		{{summary "next eu"}}
	    hoc_register_menu_data "Edit" "prev eu" \
		"Primitive Edit - prev eu" \
		{{summary "prev eu"}}
	    hoc_register_menu_data "Edit" "radial eu" \
		"Primitive Edit - radial eu" \
		{{summary "radial eu"}}
	    hoc_register_menu_data "Edit" "extrude loop" \
		"Primitive Edit - extrude loop" \
		{{summary "extrude loop"}}
	    hoc_register_menu_data "Edit" "debug edge" \
		"Primitive Edit - debug edge" \
		{{summary "debug edge"}}
	}
	part {
	    # PARTICLE
	    hoc_register_menu_data "Edit" "scale H" \
		"Primitive Edit - scale H" \
		{{summary "scale H"}}
	    hoc_register_menu_data "Edit" "scale v" \
		"Primitive Edit - scale v" \
		{{summary "scale v"}}
	    hoc_register_menu_data "Edit" "scale h" \
		"Primitive Edit - scale h" \
		{{summary "scale h"}}
	}
	rpc {
	    # RPC
	    hoc_register_menu_data "Edit" "scale B" \
		"Primitive Edit - scale B" \
		{{summary "scale B"}}
	    hoc_register_menu_data "Edit" "scale H" \
		"Primitive Edit - scale H" \
		{{summary "scale H"}}
	    hoc_register_menu_data "Edit" "scale r" \
		"Primitive Edit - scale r" \
		{{summary "scale r"}}
	}
	rhc {
	    # RHC
	    hoc_register_menu_data "Edit" "scale B" \
		"Primitive Edit - scale B" \
		{{summary "scale B"}}
	    hoc_register_menu_data "Edit" "scale H" \
		"Primitive Edit - scale H" \
		{{summary "scale H"}}
	    hoc_register_menu_data "Edit" "scale r" \
		"Primitive Edit - scale r" \
		{{summary "scale r"}}
	    hoc_register_menu_data "Edit" "scale c" \
		"Primitive Edit - scale c" \
		{{summary "scale c"}}
	}
	epa {
	    # EPA
	    hoc_register_menu_data "Edit" "scale H" \
		"Primitive Edit - scale H" \
		{{summary "scale H"}}
	    hoc_register_menu_data "Edit" "scale A" \
		"Primitive Edit - scale A" \
		{{summary "scale A"}}
	    hoc_register_menu_data "Edit" "scale B" \
		"Primitive Edit - scale B" \
		{{summary "scale B"}}
	}
	ehy {
	    # EHY
	    hoc_register_menu_data "Edit" "scale H" \
		"Primitive Edit - scale H" \
		{{summary "scale H"}}
	    hoc_register_menu_data "Edit" "scale A" \
		"Primitive Edit - scale A" \
		{{summary "scale A"}}
	    hoc_register_menu_data "Edit" "scale B" \
		"Primitive Edit - scale B" \
		{{summary "scale B"}}
	    hoc_register_menu_data "Edit" "scale c" \
		"Primitive Edit - scale c" \
		{{summary "scale c"}}
	}
	pipe {
	    # PIPE
	    hoc_register_menu_data "Edit" "select point" \
		"Primitive Edit - select point" \
		{{summary "select point"}}
	    hoc_register_menu_data "Edit" "next point" \
		"Primitive Edit - next point" \
		{{summary "next point"}}
	    hoc_register_menu_data "Edit" "previous point" \
		"Primitive Edit - previous point" \
		{{summary "previous point"}}
	    hoc_register_menu_data "Edit" "move point" \
		"Primitive Edit - move point" \
		{{summary "move point"}}
	    hoc_register_menu_data "Edit" "delete point" \
		"Primitive Edit - delete point" \
		{{summary "delete point"}}
	    hoc_register_menu_data "Edit" "append point" \
		"Primitive Edit - append point" \
		{{summary "append point"}}
	    hoc_register_menu_data "Edit" "prepend point" \
		"Primitive Edit - prepend point" \
		{{summary "prepend point"}}
	    hoc_register_menu_data "Edit" "scale point OD" \
		"Primitive Edit - scale point OD" \
		{{summary "scale point OD"}}
	    hoc_register_menu_data "Edit" "scale point ID;" \
		"Primitive Edit - scale point ID" \
		{{summary "scale point ID"}}
	    hoc_register_menu_data "Edit" "scale point bend" \
		"Primitive Edit - scale point bend" \
		{{summary "scale point bend"}}
	    hoc_register_menu_data "Edit" "scale pipe OD" \
		"Primitive Edit - scale pipe OD" \
		{{summary "scale pipe OD;"}}
	    hoc_register_menu_data "Edit" "scale pipe ID" \
		"Primitive Edit - scale pipe ID" \
		{{summary "scale pipe ID"}}
	    hoc_register_menu_data "Edit" "scale pipe bend" \
		"Primitive Edit - scale pipe bend" \
		{{summary "scale pipe bend"}}
	}
	metaball {
	    # METABALL
	    hoc_register_menu_data "Edit" "set threshhold" \
		"Primitive Edit - set threshhold" \
		{{summary "set threshhold"}}
	    hoc_register_menu_data "Edit" "set threshhold" \
		"Primitive Edit - select point" \
		{{summary "select point"}}
	    hoc_register_menu_data "Edit" "next point" \
		"Primitive Edit - next point" \
		{{summary "next point"}}
	    hoc_register_menu_data "Edit" "previous point" \
		"Primitive Edit - previous point" \
		{{summary "previous point"}}
	    hoc_register_menu_data "Edit" "move point" \
		"Primitive Edit - move point" \
		{{summary "move point"}}
	    hoc_register_menu_data "Edit" "set point field strength" \
		"Primitive Edit - set point field strength" \
		{{summary "set point field strength"}}
	    hoc_register_menu_data "Edit" "delete point" \
		"Primitive Edit - delete point" \
		{{summary "delete point"}}
	    hoc_register_menu_data "Edit" "add point" \
		"Primitive Edit - add point" \
		{{summary "add point"}}
	}
	vol {
	    # VOL
	    hoc_register_menu_data "Edit" "file name" \
		"Primitive Edit - file name" \
		{{summary "file name"}}
	    hoc_register_menu_data "Edit" "file size (X Y Z)" \
		"Primitive Edit - file size (X Y Z)" \
		{{summary "file size (X Y Z)"}}
	    hoc_register_menu_data "Edit" "voxel size (X Y Z)" \
		"Primitive Edit - voxel size (X Y Z)" \
		{{summary "voxel size (X Y Z)"}}
	    hoc_register_menu_data "Edit" "threshold (low)" \
		"Primitive Edit - threshold (low)" \
		{{summary "threshold (low)"}}
	    hoc_register_menu_data "Edit" "threshold (hi)" \
		"Primitive Edit - threshold (hi)" \
		{{summary "threshold (hi)"}}
	}
	ebm {
	    # EBM
	    hoc_register_menu_data "Edit" "file name" \
		"Primitive Edit - file name" \
		{{summary "file name"}}
	    hoc_register_menu_data "Edit" "file size (W N)" \
		"Primitive Edit - file size (W N)" \
		{{summary "file size (W N)"}}
	    hoc_register_menu_data "Edit" "extrude depth" \
		"Primitive Edit - extrude depth" \
		{{summary "extrude depth"}}
	}
	dsp {
	    # DSP
	    hoc_register_menu_data "Edit" "file name" \
		"Primitive Edit - file name" \
		{{summary "file name"}}
	    hoc_register_menu_data "Edit" "Scale X" \
		"Primitive Edit - Scale X" \
		{{summary "Scale X"}}
	    hoc_register_menu_data "Edit" "Scale Y" \
		"Primitive Edit - Scale Y" \
		{{summary "Scale Y"}}
	    hoc_register_menu_data "Edit" "Scale ALT" \
		"Primitive Edit - Scale ALT" \
		{{summary "Scale ALT"}}
	}
	bot {
	    # BOT
	    hoc_register_menu_data "Edit" "pick vertex" \
		"Primitive Edit - pick vertex" \
		{{summary "Use mouse to select vertex to edit"}}
	    hoc_register_menu_data "Edit" "pick edge" \
		"Primitive Edit - pick edge" \
		{{summary "Use mouse to select edge to edit"}}
	    hoc_register_menu_data "Edit" "pick triangle" \
		"Primitive Edit - pick triangle" \
		{{summary "Use mouse to select triangle to edit"}}
	    hoc_register_menu_data "Edit" "move vertex" \
		"Primitive Edit - move vertex" \
		{{summary "Move selected vertex"}}
	    hoc_register_menu_data "Edit" "move edge" \
		"Primitive Edit - move edge" \
		{{summary "Move selected edge"}}
	    hoc_register_menu_data "Edit" "move triangle" \
		"Primitive Edit - move triangle" \
		{{summary "Move selected triangle"}}
	    hoc_register_menu_data "Edit" "select mode" \
		"Primitive Edit - select mode" \
		{{summary "Select mode for this BOT"}}
	    hoc_register_menu_data "Edit" "select orientation" \
		"Primitive Edit - select orientation" \
		{{summary "Select orientation for BOT faces"}}
	}
    }
}

## - cook_solid_type
#
# Routine that looks for an incoming solid type of
# type "arb8". If found, look in the edit_menus to
# determine the real arb type. The cooked solid type
# for arbs will be one of the following:
#     ARB4 ARB5 ARB6 ARB7 ARB8
#
# All other types will return the raw solid type as
# the cooked type.
#
proc cook_solid_type { raw_stype edit_menus } {
    switch $raw_stype {
	arb8 {
	    return [lindex [lindex [lindex [lindex [lindex $edit_menus 1] 1] 0] 0] 0]
	}
	default {
	    return $raw_stype
	}
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
