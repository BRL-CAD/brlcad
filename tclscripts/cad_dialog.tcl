#			C A D . T C L
#
# Author -
#	Glenn Durfee
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#  
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#	your "Statement of Terms and Conditions for the Release of
#	The BRL-CAD Package" agreement.
#
# Copyright Notice -
#	This software is Copyright (C) 1995 by the United States Army
#	in all countries except the USA.  All rights reserved.
#
# Description -
#	"cad_dialog" and "cad_input_dialog" are based off of the
#	"tk_dialog" that comes with Tk 4.0.
#
# Modifications -
#        (Bob Parker):
#		*- mods to pop up the dialog box near the pointer.
#		*- mods to cad_dialog (i.e. use text widget with
#		   scrollbar if string length becomes too large).
#	 (John Anderson):
#		*- added cad_radio proc
#
#==============================================================================

# cad_dialog --
#
# Much like tk_dialog, but doesn't perform a grab.
# Makes a dialog window with the given title, text, bitmap, and buttons.
#
proc cad_dialog { w screen title text bitmap default args } {
    global button$w

    if [winfo exists $w] {
	raise $w
	return
    }

    # The screen parameter can be the pathname of some
    # widget where the screen value can be obtained.
    # Otherwise, it is assumed to be a genuine X DISPLAY
    # string.
    if [winfo exists $screen] {
	set screen [winfo screen $screen]
    }

    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -expand yes -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    # Use a text widget with scrollbar if string is too large
    if {[string length $text] > 1000} {
	frame $w.top.msgF
	text $w.top.msgT -yscrollcommand "$w.top.msgS set"
	$w.top.msgT insert 1.0 $text
	$w.top.msgT configure -state disabled
	scrollbar $w.top.msgS -command "$w.top.msgT yview"
	grid $w.top.msgT $w.top.msgS -sticky nsew -in $w.top.msgF
	grid columnconfigure $w.top.msgF 0 -weight 1
	grid rowconfigure $w.top.msgF 0 -weight 1

	# since pack is being used elsewhere,
	# we'll begin using it now.
	pack $w.top.msgF -side right -expand yes -fill both -padx 2m -pady 2m
    } else {
	message $w.top.msg -text $text -width 12i
	pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m
    }

    if { $bitmap != "" } {
	label $w.top.bitmap -bitmap $bitmap
	pack $w.top.bitmap -side left -padx 2m -pady 2m
    }

    set i 0
    foreach but $args {
	button $w.bot.button$i -text $but -command "set button$w $i"
	if { $i == $default } {
	    frame $w.bot.default -relief sunken -bd 1
	    raise $w.bot.button$i
	    pack $w.bot.default -side left -expand yes -padx 2m -pady 1m
	    pack $w.bot.button$i -in $w.bot.default -side left -padx 1m \
		    -pady 1m -ipadx 1m -ipady 1
	} else {
	    pack $w.bot.button$i -side left -expand yes \
		    -padx 2m -pady 2m -ipadx 1m -ipady 1
	}
	incr i
    }

    if { $default >= 0 } {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    place_near_mouse $w

    tkwait variable button$w
    catch { destroy $w }
    return [set button$w]
}

# cad_input_dialog --
#
#   Creates a dialog with the given title, text, and buttons, along with an
#   entry box (with possible default value) whose contents are to be returned
#   in the variable name contained in entryvar.
#
proc cad_input_dialog { w screen title text entryvar defaultentry default entry_hoc_data args } {
    global hoc_data
    global button$w entry$w
    upvar $entryvar entrylocal

    set entry$w $defaultentry
    
    # The screen parameter can be the pathname of some
    # widget where the screen value can be obtained.
    # Otherwise, it is assumed to be a genuine X DISPLAY
    # string.
    if [winfo exists $screen] {
	set screen [winfo screen $screen]
    }

    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -expand yes -fill both
    frame $w.mid -relief raised -bd 1
    pack $w.mid -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m

    entry $w.mid.ent -relief sunken -width 16 -textvariable entry$w
    hoc_register_data $w.mid.ent "Entry Widget" $entry_hoc_data
    pack $w.mid.ent -side top -expand yes -fill both -padx 1m -pady 1m

    set i 0
    foreach but $args {
	button $w.bot.button$i -text $but -command "set button$w $i"
	hoc_register_data $w.bot.button$i "Button Action"\
		{{summary "Dismiss the dialog box, taking other
actions as indicated by the button label."}}
	if { $i == $default } {
	    frame $w.bot.default -relief sunken -bd 1
	    raise $w.bot.button$i
	    pack $w.bot.default -side left -expand yes -padx 2m -pady 1m
	    pack $w.bot.button$i -in $w.bot.default -side left -padx 1m \
		    -pady 1m -ipadx 1m -ipady 1
	} else {
	    pack $w.bot.button$i -side left -expand yes \
		    -padx 2m -pady 2m -ipadx 1m -ipady 1
	}
	incr i
    }

    if { $default >= 0 } {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    place_near_mouse $w

    tkwait variable button$w
    set entrylocal [set entry$w]
    catch { destroy $w }
    return [set button$w]
}

# This proc pops up a dialog box with some radio buttons, an "apply" button, and a "dismiss" button
#
#	my_widget_name is the name to be used for this toplevel window
#	screen can be the pathname of some widget where the screen value can be obtained. Otherwise,
#		it is assumed to be a genuine X DISPLAY string.
#	radio_result is a string containing the name of the variable to hold the result (must be global)
#	title is the title to be displayed on the popup
#	text_message is a message to be displayed in the window (typically instructions for the user)
#	choice_labels is a list of labels for the radio buttons
#	default is the index of the default choice
#	help_strings is a list of help strings for the corresponding labels in choice_labels
proc cad_radio { my_widget_name screen radio_result title text_message default choice_labels help_strings } {
	global $radio_result
	# The screen parameter can be the pathname of some
	# widget where the screen value can be obtained.
	# Otherwise, it is assumed to be a genuine X DISPLAY
	# string.
	if [winfo exists $screen] {
		set screen [winfo screen $screen]
	}

	set done 0
	set w $my_widget_name

	if [winfo exists $w] { catch "destroy $w" }
	toplevel $w -screen $screen
	wm title $w $title
	wm iconname $w Dialog
	message $w.mess -text $text_message -justify center -width 500
	hoc_register_data $w.mess "Radio Button Selection Dialog" {
		{ summary "Use this window to select any one of the possibilities listed" }
	}
	grid $w.mess -row 0 -column 0 -columnspan 2 -sticky ew
	set counter 0
	foreach choice $choice_labels {
		radiobutton $w.but_$counter -value $counter -variable $radio_result
		label $w.lab_$counter -text $choice
		grid $w.lab_$counter -row [expr $counter + 1] -column 1 -sticky ew
		grid $w.but_$counter -row [expr $counter + 1] -column 0 -sticky ew
		set hoc_data [subst {{ summary \"[lindex $help_strings $counter]\" }}]
		hoc_register_data $w.lab_$counter [lindex $choice_labels $counter] $hoc_data
		hoc_register_data $w.but_$counter [lindex $choice_labels $counter] $hoc_data
		incr counter
	}
	$w.but_$default invoke

	button $w.apply -text Apply -command {set done 1}
	hoc_register_data $w.apply "Apply" {
		{ summary "Click on this button to indicate you have finished making your selection" }
	}

	button $w.dismiss -text Dismiss -command "set $radio_result $default; set done 2"
	hoc_register_data $w.dismiss "DIsmiss" {
		{ summary "Click on this button to indicate you do not want to change\nthis selection from its value when the window appeard" }
	}

	grid $w.apply -row [expr $counter + 1] -column 0
	grid $w.dismiss -row [ expr $counter + 1] -column 1
	update

	tkwait variable done
	catch " destroy $w "
}
