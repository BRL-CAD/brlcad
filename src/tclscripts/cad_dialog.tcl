#                  C A D _ D I A L O G . T C L
# BRL-CAD
#
# Copyright (C) 1995-2005 United States Government as represented by
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
#			C A D . T C L
#
# Author -
#	Glenn Durfee
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#  
#
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
#		*- add ::tk::Priv(wait_cmd) and use in all dialogs
#	 (John Anderson):
#		*- added cad_radio proc
#
#==============================================================================

if {![info exists ::tk::Priv(wait_cmd)]} {
    set ::tk::Priv(wait_cmd) tkwait
}

# cad_dialog --
#
# Much like tk_dialog, but doesn't perform a grab.
# Makes a dialog window with the given title, text, bitmap, and buttons.
#
proc cad_dialog { w screen title text bitmap default args } {
    global button$w
    global ::tk::Priv

    if [winfo exists $w] {
	catch {destroy $w}
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

    $::tk::Priv(wait_cmd) variable button$w
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
    global ::tk::Priv
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

    $::tk::Priv(wait_cmd) variable button$w
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
        global ::tk::Priv
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
	hoc_register_data $w.dismiss "Dismiss" {
		{ summary "Click on this button to indicate you do not want to change\nthis selection from its value when the window first appeared" }
	}

	grid $w.apply -row [expr $counter + 1] -column 0
	grid $w.dismiss -row [ expr $counter + 1] -column 1
	update

	$::tk::Priv(wait_cmd) variable done
	catch " destroy $w "
}

proc cad_list_buts { my_widget_name screen list_of_results cur_settings title text_message choice_labels help_strings } {
    global $list_of_results
    global list_but_result
    global list_buts_done
    global ::tk::Priv

    # The screen parameter can be the pathname of some
    # widget where the screen value can be obtained.
    # Otherwise, it is assumed to be a genuine X DISPLAY
    # string.
    if [winfo exists $screen] {
	set screen [winfo screen $screen]
    }

    set list_buts_done 0
    set w $my_widget_name

    if [winfo exists $w] { catch "destroy $w" }
    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    message $w.mess -text $text_message -justify center -width 500
    hoc_register_data $w.mess "List selection Dialog" {
	{ summary "Use this window select any number (or none) of the possibilities listed" }
    }

    grid $w.mess -row 0 -column 0 -columnspan 2 -sticky ew
    if { [llength $cur_settings] != [llength $choice_labels]} {
	puts "settings do not match choices"
	set $list_of_results $cur_settings
	catch " destroy $w "
	return
    }
    set counter 0
    foreach setting $cur_settings choice $choice_labels {
	set list_but_result($counter) $setting
	checkbutton $w.but_$counter -text [lindex $choice_labels $counter] -variable list_but_result($counter)
	grid $w.but_$counter -row [expr $counter + 1] -column 0 -sticky ew -columnspan 2
	set hoc_data [subst {{ summary \"[lindex $help_strings $counter]\" }}]
	hoc_register_data $w.but_$counter [lindex $choice_labels $counter] $hoc_data
	incr counter
    }

    button $w.apply -text Apply -command "set list_buts_done 1"
    hoc_register_data $w.apply "Apply" {
	{ summary "Click on this button to indicate you have finished making your selections" }
    }

    button $w.dismiss -text Dismiss -command "set list_buts_done 2"
    hoc_register_data $w.dismiss "Dismiss" {
	{ summary "Click on this button to indicate you do not want to change\nthe selections from their values when the window first appeared" }
    }

    grid $w.apply -row [expr $counter + 1] -column 0
    grid $w.dismiss -row [ expr $counter + 1] -column 1
    update

    $::tk::Priv(wait_cmd) variable list_buts_done

    set num_choices [llength $choice_labels]
    if { $list_buts_done == 2 } {
	set $list_of_results $cur_settings
    } else {
	set $list_of_results {}
	for { set counter 0 } { $counter < $num_choices } { incr counter } {
	    if { $list_but_result($counter) == 1 } {
		lappend $list_of_results "1"
	    } else {
		lappend $list_of_results "0"
	    }
	}
    }
    catch " destroy $w "
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
