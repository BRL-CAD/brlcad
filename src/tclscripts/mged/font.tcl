#                        F O N T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
##
#                       F O N T . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	A tool for creating/configuring named fonts.
#

if ![info exists mged_default(text_font)] {
    set mged_default(text_font) {
	-family courier -size 12 -weight normal -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(menu_font)] {
    set mged_default(menu_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(button_font)] {
    set mged_default(button_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(menubutton_font)] {
    set mged_default(menubutton_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(list_font)] {
    set mged_default(list_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(label_font)] {
    set mged_default(label_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(entry_font)] {
    set mged_default(entry_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(bold_font)] {
    set mged_default(bold_font) {
	-family "new century schoolbook" -size 16 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(italic_font)] {
    set mged_default(italic_font) {
	-family "new century schoolbook" -size 16 -weight normal -slant italic -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(underline_font)] {
    set mged_default(underline_font) {
	-family "new century schoolbook" -size 16 -weight normal -slant roman -underline 1 -overstrike 0
    }
}

if ![info exists mged_default(overstrike_font)] {
    set mged_default(overstrike_font) {
	-family "new century schoolbook" -size 16 -weight normal -slant roman -underline 0 -overstrike 1
    }
}

if ![info exists mged_default(all_font)] {
    set mged_default(all_font) {
	-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists font_scheme_data] {
    set font_scheme_data { {fs_button_font button_font Buttons}
                           {fs_entry_font entry_font Entries}
			   {fs_label_font label_font Labels}
			   {fs_list_font list_font Lists}
			   {fs_menu_font menu_font Menus}
			   {fs_menubutton_font menubutton_font Menubuttons}
			   {fs_text_font text_font Text}
			   {fs_all_font {} All}
		         }
}

## - font_init
#
# Create fonts used by GUI components.
#
proc font_init {} {
    global mged_default

    # create named fonts used by GUI components
    eval font create text_font $mged_default(text_font)
    eval font create menu_font $mged_default(menu_font)
    eval font create button_font $mged_default(button_font)
    eval font create menubutton_font $mged_default(menubutton_font)
    eval font create list_font $mged_default(list_font)
    eval font create label_font $mged_default(label_font)
    eval font create entry_font $mged_default(entry_font)

    # create named fonts used by the font gui
    eval font create bold_font $mged_default(bold_font)
    eval font create italic_font $mged_default(italic_font)
    eval font create underline_font $mged_default(underline_font)
    eval font create overstrike_font $mged_default(overstrike_font)

    option add *Text.font text_font
    option add *Menu.font menu_font
    option add *Button.font button_font
    option add *Menubutton.font menubutton_font
    option add *List.font list_font
    option add *Label.font label_font
    option add *Entry.font entry_font
}

## - font_scheme_init
#
# A control panel for viewing/modifying MGED's font scheme.
#
proc font_scheme_init { id } {
    global mged_gui
    global mged_default
    global font_gui
    global font_scheme_data

    set top .$id.font_scheme
    set gui_top .$id.font_gui

    if [winfo exists $top] {
	raise $top
	return
    }

    set font_gui($id,padx) 4
    set font_gui($id,pady) 4

    toplevel $top -screen $mged_gui($id,screen)
    frame $top.gridF1
    frame $top.gridF2

    ################## display gui fonts ##################
    label $top.family -text "Family"
    label $top.size -text "Size"
    grid x $top.family $top.size -sticky nsew -in $top.gridF1
    foreach datum $font_scheme_data {
	set fsname [lindex $datum 0]($id)
	set fname [lindex $datum 1]
	set fstitle [lindex $datum 2]

	if {$fname == {}} {
	    set cb font_scheme_callback_all

	    #  Create the named font fs_all_font.
	    catch {eval font create $fsname $mged_default(all_font)}

	    # make l2 and l3 placeholders
	    set l2 x
	    set l3 x
	} else {
	    set cb font_scheme_callback

	    # Create the named font used by the font scheme control panel
	    #     and initialize with the font used by the GUI component.
	    set fconfig [font configure $fname]
	    catch {eval font create $fsname $fconfig}

	    set l2 $top._$fstitle\L2
	    set l3 $top._$fstitle\L3
	}


	set l1 $top._$fstitle\L1
	set mb $top._$fstitle\MB
	set mbm $top._$fstitle\MB.menu
	font_scheme_build_display $id $top $gui_top $fsname $fstitle $fconfig \
		$cb $l1 $l2 $l3 $mb $mbm

	grid $l1 $l2 $l3 $mb -sticky nsew -in $top.gridF1
    }

    ################## buttons along bottom ##################
    button $top.okB -relief raised -text "OK"\
	    -command "font_scheme_ok $id $top $gui_top"
    button $top.applyB -relief raised -text "Apply"\
	    -command "font_scheme_apply $id"
    button $top.resetB -relief raised -text "Reset"\
	    -command "font_scheme_reset $id $top"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "font_scheme_dismiss $id $top $gui_top"
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky nsew -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1
    grid columnconfigure $top.gridF2 4 -weight 1

    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 1 -weight 1

    grid $top.gridF1 -sticky nsew -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid $top.gridF2 -sticky nsew -padx $font_gui($id,padx) -pady $font_gui($id,pady)

    # pop up near mouse
    place_near_mouse $top

    # disallow the user to resize
    wm resizable $top 0 0
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Fonts"
}

## - font_scheme_build_display
#
# Builds a font display line.
#
proc font_scheme_build_display { id top gui_top fsname fstitle fconfig cb l1 l2 l3 mb mbm } {
    global mged_default

    label $l1 -text $fstitle -anchor e
    if {$l2 != "x"} {
	label $l2 -text [lindex $fconfig 1] -font $fsname
	label $l3 -text [lindex $fconfig 3] -font $fsname
    }

    menubutton $mb -menu $mbm -indicatoron 1
#	bind $mb <Enter> "%W configure -relief raised; break"
#	bind $mb <Leave> "%W configure -relief flat; break"
    menu $mbm -title "" -tearoff $mged_default(tearoff_menus)

    $mbm add command -label "courier 12" \
	    -command " $cb $id $top $fsname $fstitle \"-family courier -size 12 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "courier 18" \
	    -command "$cb $id $top $fsname $fstitle \"-family courier -size 18 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "fixed 12" \
	    -command "$cb $id $top $fsname $fstitle \"-family fixed -size 12 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "fixed 18" \
	    -command "$cb $id $top $fsname $fstitle \"-family fixed -size 18 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "helvetica 12" \
	    -command "$cb $id $top $fsname $fstitle \"-family helvetica -size 12 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "helvetica 18" \
	    -command "$cb $id $top $fsname $fstitle \"-family helvetica -size 18 -weight normal -slant roman -underline 0 -overstrike 0\""
    $mbm add command -label "Font Tool..." \
	    -command "font_gui_init $id $gui_top $fsname $fstitle \"$cb $id $top $fsname $fstitle\""
}

## - font_gui_init
#
# A control panel for modifying MGED's named fonts.
#
proc font_gui_init { id top fname title callback } {
    global mged_gui
    global font_gui

    if [winfo exists $top] {
	set font_gui($id,callback) $callback
	font_set_name $id $fname
	font_bindCB $id $top.weightCB font_gui($id,weight) bold 0
	font_bindCB $id $top.slantCB font_gui($id,slant) italic 0
	font_bindCB $id $top.underlineCB font_gui($id,underline) 1 0
	font_bindCB $id $top.overstrikeCB font_gui($id,overstrike) 1 0
	wm title $top "$title Font"

	raise $top
	return
    }

    # create the font_gui_font($id)
    catch {font create font_gui_font($id)}

    set font_gui($id,padx) 4
    set font_gui($id,pady) 4
    set font_gui($id,callback) $callback

    toplevel $top -screen $mged_gui($id,screen)

    ################## describe the font ##################
    frame $top.gridF1

    # name
    font_set_name $id $fname

    # family
    menubutton $top.familyMB -relief raised -bd 2 -indicatoron 1 \
	    -textvariable font_gui($id,family) \
	    -menu $top.familyMB.menu
    font_build_familyM $id $top.familyMB.menu

    # size
    frame $top.sizeF -relief sunken -bd 2
    entry $top.sizeE -relief flat -textvariable font_gui($id,size) -width 3
    menubutton $top.sizeMB -relief raised -bd 2 -indicatoron 1 -menu $top.sizeMB.menu
    font_build_sizeM $id $top.sizeMB.menu
    grid $top.sizeE $top.sizeMB -sticky nsew -in $top.sizeF
    grid rowconfigure $top.sizeF 0 -weight 1
    grid columnconfigure $top.sizeF 0 -weight 1
    bind $top.sizeE <Return> "font_update $id"

    # weight
    checkbutton $top.weightCB -offvalue normal -onvalue bold \
	    -variable font_gui($id,weight) \
	    -indicatoron 0 \
	    -width 2 \
	    -text B \
	    -font bold_font \
	    -selectcolor #ececec \
	    -command "font_bindCB $id $top.weightCB font_gui($id,weight) bold 1; font_update $id"
    font_bindCB $id $top.weightCB font_gui($id,weight) bold 0

    # slant
    checkbutton $top.slantCB -offvalue roman -onvalue italic \
	    -variable font_gui($id,slant) \
	    -indicatoron 0 \
	    -width 2 \
	    -text I \
	    -font italic_font \
	    -selectcolor #ececec \
	    -command "font_bindCB $id $top.slantCB font_gui($id,slant) italic 1; font_update $id"
    font_bindCB $id $top.slantCB font_gui($id,slant) italic 0

    # underline
    checkbutton $top.underlineCB -offvalue 0 -onvalue 1 \
	    -variable font_gui($id,underline) \
	    -indicatoron 0 \
	    -width 2 \
	    -text U \
	    -font underline_font \
	    -selectcolor #ececec \
	    -command "font_bindCB $id $top.underlineCB font_gui($id,underline) 1 1; font_update $id"
    font_bindCB $id $top.underlineCB font_gui($id,underline) 1 0

    # overstrike
    checkbutton $top.overstrikeCB -offvalue 0 -onvalue 1 \
	    -variable font_gui($id,overstrike)\
	    -indicatoron 0 \
	    -width 2 \
	    -text O \
	    -font overstrike_font \
	    -selectcolor #ececec \
	    -command "font_bindCB $id $top.overstrikeCB font_gui($id,overstrike) 1 1; font_update $id"
    font_bindCB $id $top.overstrikeCB font_gui($id,overstrike) 1 0

    grid x $top.familyMB $top.sizeF $top.weightCB $top.slantCB \
	    $top.underlineCB $top.overstrikeCB x \
	    -padx $font_gui($id,padx) \
	    -sticky nsew -in $top.gridF1
    grid columnconfigure $top.gridF1 0 -weight 1
    grid columnconfigure $top.gridF1 7 -weight 1

    ################# demonstrate the font ################
    frame $top.gridF2
    label $top.demo -font font_gui_font($id) \
	    -text "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789"
    grid $top.demo -sticky nsew -in $top.gridF2
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    ################# buttons along bottom ################
    frame $top.gridF3
    button $top.okB -relief raised -text "OK"\
	    -command "font_ok $id $top"
    button $top.applyB -relief raised -text "Apply"\
	    -command "font_apply $id"
    button $top.resetB -relief raised -text "Reset"\
	    -command "font_reset $id $top"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "font_dismiss $id $top"
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky nsew -in $top.gridF3
    grid columnconfigure $top.gridF3 2 -weight 1
    grid columnconfigure $top.gridF3 4 -weight 1

    grid $top.gridF1 -sticky nsew -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid $top.gridF2 -sticky nsew -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid $top.gridF3 -sticky nsew -padx $font_gui($id,padx) -pady $font_gui($id,pady)

    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 1 -weight 1

    # pop up near mouse
    place_near_mouse $top

    # disallow the user to resize
    wm resizable $top 0 0
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "$title Font"
}

## - font_build_nameM
#
# Build menu of named fonts.
#
proc font_build_nameM { id m } {
    global mged_default
    global font_gui

    set named_fonts [font_get_names]

    font_build_menu $id $m $named_fonts font_set_name "Named Fonts"
    $m configure -postcommand "font_name_post $id $m font_set_name"

    # for now initialize using first font name in list
    set fname [lindex $named_fonts 0]
    font_set_name $id $fname
}

## - font_build_familyM
#
# Build menu of font families.
#
proc font_build_familyM { id m } {
    global mged_default

    set families [lsort [font families]]

    menu $m -title "Font Family" -tearoff $mged_default(tearoff_menus)

    set nmenus 3
    set len [llength $families]
    set size [expr $len / $nmenus]

    for {set n 1} {$n <= $nmenus} {incr n} {
	set i [expr ($n - 1) * $size]
	if {$n == $nmenus} {
	    set j end
	} else {
	    set j [expr $i + $size - 1]
	}

	set fam [lrange $families $i $j]
	$m add cascade -label "Family $n" -menu $m.cmenu$n
	font_build_menu $id $m.cmenu$n $fam font_set_family "Font Family $n"
    }
}

## - font_build_sizeM
#
# Build menu of font sizes.
#
proc font_build_sizeM { id m } {
    set sizes { 4 6 8 10 12 14 16 18 20 22 24 26 28 30 }
    font_build_menu $id $m $sizes font_set_size "Font Size"
}

## - font_build_weightM
#
# Build menu of font weights.
#
proc font_build_weightM { id m } {
    set weights { bold normal }
    font_build_menu $id $m $weights font_set_weight "Font Weight"
}

## - font_build_slantM
#
# Build menu of font slant values.
#
proc font_build_slantM { id m } {
    set slants { italic roman }
    font_build_menu $id $m $slants font_set_slant "Font Slant"
}

## - font_build_menu
#
# Build menu.
#
proc font_build_menu { id menu items cmd title } {
    global mged_default

    if [winfo exists $menu] {
	destroy $menu
    }

    menu $menu -title $title -tearoff $mged_default(tearoff_menus)

    foreach item $items {
	$menu add command -label $item \
		-command "$cmd $id \"$item\""
    }
}

## - font_update_menu
#
# Update menu items.
#
proc font_update_menu { id menu items cmd } {
    $menu delete 0 end

    foreach item $items {
	$menu add command -label $item \
		-command "$cmd $id \"$item\""
    }
}

## - font_set_name
#
# Update gui to reflect the font name.
#
proc font_set_name { id name } {
    global font_gui

    set font_gui($id,name) $name

    set ret [catch {font configure $font_gui($id,name)} font_params]
    if {$ret == 0} {
	# configure font_gui_font
	catch {eval font configure font_gui_font($id) $font_params}

	# update variables
	set font_gui($id,family) [lindex $font_params 1]
	set font_gui($id,size) [lindex $font_params 3]
	set font_gui($id,weight) [lindex $font_params 5]
	set font_gui($id,slant) [lindex $font_params 7]
	set font_gui($id,underline) [lindex $font_params 9]
	set font_gui($id,overstrike) [lindex $font_params 11]
    }
}

proc font_set_family { id family } {
    global font_gui

    set font_gui($id,family) $family
    font_update $id
}

proc font_set_size { id size } {
    global font_gui

    set font_gui($id,size) $size
    font_update $id
}

proc font_set_weight { id weight } {
    global font_gui

    set font_gui($id,weight) $weight
    font_update $id
}

proc font_set_slant { id slant } {
    global font_gui

    set font_gui($id,slant) $slant
    font_update $id
}

## - font_bindCB
#
# If the button is checked, set the bindings to raise
# and flatten the button as the mouse passes over.
# Otherwise, disable the bindings and leave the button
# in a sunken state.
#
proc font_bindCB { id w var val callback } {
    global font_gui

    if {[subst $[subst $var]] == $val} {
	# In this case (i.e. the button is checked),
	# so the border is set to a nonzero value
	# to make the button appear sunken.
	$w configure -bd 2 -padx 1 -pady 1

	bind $w <Enter> ""
	bind $w <Leave> ""
    } else {
	if {$callback} {
	    # assume the mouse is over the checkbutton
	    $w configure -bd 2 -padx 1 -pady 1
	} else {
	    $w configure -bd 0 -padx 3 -pady 3
	}

	bind $w <Enter> "%W configure -bd 2 -padx 1 -pady 1"
	bind $w <Leave> "%W configure -bd 0 -padx 3 -pady 3"
    }
}

## - font_update
#
# update label to reflect the state of the font gui
#
proc font_update { id } {
    global font_gui

    font configure font_gui_font($id) \
	    -family $font_gui($id,family) \
	    -size $font_gui($id,size) \
	    -weight $font_gui($id,weight) \
	    -slant $font_gui($id,slant) \
	    -underline $font_gui($id,underline) \
	    -overstrike $font_gui($id,overstrike)
}

## - font_name_post
#
# Populate "Named Fonts" menu before posting.
#
proc font_name_post { id menu cmd } {
    set named_fonts [font_get_names]

    font_update_menu $id $menu $named_fonts $cmd
}

## - font_get_names
#
# Returns a list of named fonts with all
# occurences of font_gui_font* removed.
#
proc font_get_names {} {
    set named_fonts [lsort [font names]]

    # check to see if font_gui_font* is in list of named fonts
    set index [lsearch -glob $named_fonts font_gui_font*]

    while {$index >= 0} {
	# remove font_gui_font* from sorted list
	set named_fonts [lreplace $named_fonts $index $index]

	# check to see if font_gui_font* is in list of named fonts
	set index [lsearch -glob $named_fonts font_gui_font*]
    }

    return $named_fonts
}

## - font_ok
#
# apply and dismiss
#
proc font_ok { id top } {
    font_apply $id
    font_dismiss $id $top
}

## - font_apply
#
# apply gui settings to the font name font_gui($id,name)
#
proc font_apply { id } {
    global font_gui

    if {$font_gui($id,name) == ""} {
	return
    }

    set font_params [font configure font_gui_font($id)]

    # try to create new font with parameters from font_gui_font($id)
    set ret [catch {eval font create $font_gui($id,name) $font_params}]

    # if failed, assume it already exists, so configure using font_gui_font($id) parameters
    if {$ret} {
	catch {eval font configure $font_gui($id,name) $font_params}
    }

    if {$font_gui($id,callback) != {}} {
	eval $font_gui($id,callback) {$font_params}
    }
}

## - font_reset
#
# if the named font $font_gui($id,name) exists,
# use its' parameters to configure font_gui_font($id)
#
proc font_reset { id top } {
    global font_gui

    font_set_name $id $font_gui($id,name)
}

proc font_dismiss { id top } {
    destroy $top
    font delete font_gui_font($id)
}

proc font_scheme_callback { id top font key fconfig } {
    global font_gui

#    set fconfig [font configure $font]
    eval font configure $font $fconfig
    $top._$key\L2 configure -text [lindex $fconfig 1]
    $top._$key\L3 configure -text [lindex $fconfig 3]
}

proc font_scheme_callback_all { id top font key fconfig } {
    global font_scheme_data

#    set fconfig [font configure $font]

    foreach datum $font_scheme_data {
	set fsname [lindex $datum 0]($id)
	set fname [lindex $datum 1]
	set key [lindex $datum 2]

	if {$fname != {}} {
	    font_scheme_callback $id $top $fsname $key $fconfig
	} else {
	    # configure fs_all_font($id)
	    eval font configure $fsname $fconfig
	}
    }
}

proc font_scheme_set { id top font fconfig } {
    eval font configure $font $fconfig
}

proc font_scheme_set_all { id top font fconfig } {
    global font_scheme_data

    foreach datum $font_scheme_data {
	set font [lindex $datum 0]($id)
	set fname [lindex $datum 1]

	if {$fname != {}} {
	    font_scheme_set $id $top $font $fconfig
	}
    }
}

proc font_scheme_ok { id top gui_top } {
    font_scheme_apply $id
    font_scheme_dismiss $id $top $gui_top
}

proc font_scheme_apply { id } {
    global font_scheme_data

    # apply the font_scheme font configurations to the gui component fonts
    foreach datum $font_scheme_data {
	set fsname [lindex $datum 0]($id)
	set fname [lindex $datum 1]

	if {$fname != {}} {
	    set fsconfig [font configure $fsname]
	    eval font configure $fname $fsconfig
	}
    }
}

proc font_scheme_reset { id top } {
    global font_scheme_data

    # apply the gui component font configurations to the font_scheme fonts
    foreach datum $font_scheme_data {
	set fsname [lindex $datum 0]($id)
	set fname [lindex $datum 1]

	if {$fname != {}} {
	    set fstitle [lindex $datum 2]
	    set fconfig [font configure $fname]
	    eval font configure $fsname $fconfig
	    font_scheme_callback $id $top $fsname $fstitle $fconfig
	}
    }
}

proc font_scheme_dismiss { id top gui_top } {
    global font_scheme_data

    destroy $top
    catch {destroy $gui_top}

    # destroy the named fonts used by font_schemes
    foreach datum $font_scheme_data {
	set fsname [lindex $datum 0]($id)
	font delete $fsname
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
