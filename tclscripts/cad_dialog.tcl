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
#	"cad_dialog" and "cad_input_dialog" are based off of the "tk_dialog" that
#	comes with Tk 4.0.
#
# Modifications -
#        (Bob Parker):
#		*- mods to popup the dialog box near the pointer
#
#==============================================================================

# cad_dialog --
#
# Much like tk_dialog, but doesn't perform a grab.
# Makes a dialog window with the given title, text, bitmap, and buttons.
#
proc cad_dialog { w screen title text bitmap default args } {
    global button$w

    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m
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

    set pxy [winfo pointerxy $w]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    set width [winfo reqwidth $w]
    set dx [expr $width / 2]
    set x [expr $x - $dx]
    set y [expr $y - 70]
    if {$x < 0} {
	set x 0
    }
    if {$y < 0} {
	set y 0
    }
    wm geometry $w +$x+$y

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
proc cad_input_dialog { w screen title text entryvar defaultentry default args } {
    global button$w entry$w
    upvar $entryvar entrylocal

    set entry$w $defaultentry
    
    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both
    frame $w.mid -relief raised -bd 1
    pack $w.mid -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m

    entry $w.mid.ent -relief sunken -width 16 -textvariable entry$w
    pack $w.mid.ent -side top -expand yes -fill both -padx 1m -pady 1m

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

    set pxy [winfo pointerxy $w]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    set width [winfo reqwidth $w]
    set dx [expr $width / 2]
    set x [expr $x - $dx]
    set y [expr $y - 70]
    if {$x < 0} {
	set x 0
    }
    if {$y < 0} {
	set y 0
    }
    wm geometry $w +$x+$y

    tkwait variable button$w
    set entrylocal [set entry$w]
    catch { destroy $w }
    return [set button$w]
}
