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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
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

## - font_init
#
# Create fonts used by GUI components.
#
proc font_init {} {
    global mged_default

    eval font create text_font $mged_default(text_font)
    eval font create menu_font $mged_default(menu_font)
    eval font create button_font $mged_default(button_font)
    eval font create menubutton_font $mged_default(menubutton_font)
    eval font create list_font $mged_default(list_font)
    eval font create label_font $mged_default(label_font)

    option add *Text.font text_font
    option add *Menu.font menu_font
    option add *Button.font button_font
    option add *Menubutton.font menubutton_font
    option add *List.font list_font
    option add *Label.font label_font
}

## - font_gui_init
#
# A control panel for modifying MGED's named fonts.
#
proc font_gui_init { id } {
    global mged_gui
    global font_gui

    set top .$id.font_gui

    if [winfo exists $top] {
	raise $top
	return
    }

    # create the font_gui_font($id)
    catch {font create font_gui_font($id)}

    set font_gui($id,padx) 4
    set font_gui($id,pady) 4

    toplevel $top -screen $mged_gui($id,screen)

    ################## describe the font ##################
    frame $top.gridF1
    frame $top.gridF1A
    frame $top.gridF1B
    frame $top.gridF1C

    # name
    frame $top.nameF -relief sunken -bd 2
    label $top.nameL -text "Name:" -anchor e
    entry $top.nameE -relief flat -textvariable font_gui($id,name)
    menubutton $top.nameMB -relief raised -bd 2 -indicatoron 1 -menu $top.nameMB.menu
    font_build_nameM $id $top.nameMB.menu
    grid $top.nameE $top.nameMB -sticky nsew -in $top.nameF
    grid columnconfigure $top.nameF 0 -weight 1
    bind $top.nameE <Return> "font_set_name $id \$font_gui($id,name)"

    # family
    frame $top.familyF
    label $top.familyL -text "Family:" -anchor e
    menubutton $top.familyMB -relief raised -bd 2 -indicatoron 1 \
	    -textvariable font_gui($id,family) \
	    -menu $top.familyMB.menu
    font_build_familyM $id $top.familyMB.menu
    grid $top.familyL $top.familyMB -sticky nsew -in $top.familyF

    # size
    frame $top.sizeF -relief sunken -bd 2
    label $top.sizeL -text "Size:" -anchor e
    entry $top.sizeE -relief flat -textvariable font_gui($id,size)
    menubutton $top.sizeMB -relief raised -bd 2 -indicatoron 1 -menu $top.sizeMB.menu
    font_build_sizeM $id $top.sizeMB.menu
    grid $top.sizeE $top.sizeMB -sticky nsew -in $top.sizeF
    grid columnconfigure $top.sizeF 0 -weight 1
    bind $top.sizeE <Return> "font_update $id"

    # weight
    frame $top.weightF
    label $top.weightL -text "Weight:" -anchor e
    menubutton $top.weightMB -relief raised -bd 2 -indicatoron 1 \
	    -textvariable font_gui($id,weight) \
	    -menu $top.weightMB.menu
    font_build_weightM $id $top.weightMB.menu
    grid $top.weightL $top.weightMB -sticky nsew -in $top.weightF

    # slant
    frame $top.slantF
    label $top.slantL -text "Slant:" -anchor e
    menubutton $top.slantMB -relief raised -bd 2 -indicatoron 1 \
	    -textvariable font_gui($id,slant) \
	    -menu $top.slantMB.menu
    font_build_slantM $id $top.slantMB.menu
    grid $top.slantL $top.slantMB -sticky nsew -in $top.slantF

    # underline
    checkbutton $top.underlineCB -offvalue 0 -onvalue 1 -variable font_gui($id,underline) \
	    -text "Underline" \
	    -command "font_update $id"

    # overstrike
    checkbutton $top.overstrikeCB -offvalue 0 -onvalue 1 -variable font_gui($id,overstrike)\
	    -text "Overstrike" \
	    -command "font_update $id"

    grid $top.nameL $top.nameF $top.sizeL $top.sizeF \
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady) \
	    -sticky nsew -in $top.gridF1A
    grid columnconfigure $top.gridF1A 1 -weight 1
    grid columnconfigure $top.gridF1A 3 -weight 1

    grid x $top.familyF $top.weightF $top.slantF x\
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady) \
	    -sticky nsew -in $top.gridF1B
    grid columnconfigure $top.gridF1B 0 -weight 1
    grid columnconfigure $top.gridF1B 4 -weight 1

    grid x $top.underlineCB $top.overstrikeCB x \
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady) \
	    -sticky nsew -in $top.gridF1C
    grid columnconfigure $top.gridF1C 0 -weight 1
    grid columnconfigure $top.gridF1C 3 -weight 1

    grid $top.gridF1A -sticky nsew -in $top.gridF1 \
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid $top.gridF1B -sticky nsew -in $top.gridF1 \
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid $top.gridF1C -sticky nsew -in $top.gridF1 \
	    -padx $font_gui($id,padx) -pady $font_gui($id,pady)
    grid columnconfigure $top.gridF1 0 -weight 1

    ################# demonstrate the font ################
    frame $top.gridF2
    label $top.demo -font font_gui_font($id) \
	    -text "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789"
    grid $top.demo -sticky nsew -in $top.gridF2
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    ################# buttons along bottom ################
    frame $top.gridF3
    button $top.okB -relief raised -text "Ok"\
	    -command "font_ok $id $top"
    button $top.applyB -relief raised -text "Apply"\
	    -command "font_apply $id"
    button $top.resetB -relief raised -text "Reset"\
	    -command "font_reset $id $top"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "font_dismiss $id $top"
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky ew -in $top.gridF3
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
    wm title $top "Font Editor ($id)"
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
