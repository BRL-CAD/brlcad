# bgerror.tcl --
#
#	Implementation of the bgerror procedure.  It posts a dialog box with
#	the error message and gives the user a chance to see a more detailed
#	stack trace, and possible do something more interesting with that
#	trace (like save it to a log).  This is adapted from work done by
#	Donal K. Fellows.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# All rights reserved.
# 
# RCS: @(#) $Id$
# $Id$

namespace eval ::tk {
    namespace eval dialog {
	namespace eval error {
	    namespace import ::tk::msgcat::*
	    namespace export bgerror
	    option add *ErrorDialog.function.text [mc "Save To Log"] \
		    widgetDefault
	    option add *ErrorDialog.function.command [namespace code SaveToLog]
	}
    }
}

proc ::tk::dialog::error::Return {} {
    variable button
    
    .bgerrorDialog.ok configure -state active -relief sunken
    update idletasks
    after 100
    set button 0
}

proc ::tk::dialog::error::Details {} {
    set w .bgerrorDialog
    set caption [option get $w.function text {}]
    set command [option get $w.function command {}]
    if { ($caption eq "") || ($command eq "") } {
	grid forget $w.function
    }
    $w.function configure -text $caption -command \
	"$command [list [.bgerrorDialog.top.info.text get 1.0 end]]"
    grid $w.top.info - -sticky nsew -padx 3m -pady 3m
}

proc ::tk::dialog::error::SaveToLog {text} {
    if { $::tcl_platform(platform) eq "windows" } {
	set allFiles *.*
    } else {
	set allFiles *
    }
    set types [list	\
	    [list [mc "Log Files"] .log]	\
	    [list [mc "Text Files"] .txt]	\
	    [list [mc "All Files"] $allFiles] \
	    ]
    set filename [tk_getSaveFile -title [mc "Select Log File"] \
	    -filetypes $types -defaultextension .log -parent .bgerrorDialog]
    if {![string length $filename]} {
	return
    }
    set f [open $filename w]
    puts -nonewline $f $text
    close $f
}

proc ::tk::dialog::error::Destroy {w} {
    if {$w eq ".bgerrorDialog"} {
	variable button
	set button -1
    }
}

# ::tk::dialog::error::bgerror --
# This is the default version of bgerror. 
# It tries to execute tkerror, if that fails it posts a dialog box containing
# the error message and gives the user a chance to ask to see a stack
# trace.
# Arguments:
# err -			The error message.

proc ::tk::dialog::error::bgerror err {
    global errorInfo tcl_platform
    variable button

    set info $errorInfo

    set ret [catch {::tkerror $err} msg];
    if {$ret != 1} {return -code $ret $msg}

    # Ok the application's tkerror either failed or was not found
    # we use the default dialog then :
    if {($tcl_platform(platform) eq "macintosh")
             || ([tk windowingsystem] eq "aqua")} {
	set ok		[mc Ok]
	set messageFont	system
	set textRelief	flat
	set textHilight	0
    } else {
	set ok		[mc OK]
	set messageFont	{Times -18}
	set textRelief	sunken
	set textHilight	1
    }


    # Truncate the message if it is too wide (longer than 30 characacters) or
    # too tall (more than 4 newlines).  Truncation occurs at the first point at
    # which one of those conditions is met.
    set displayedErr ""
    set lines 0
    foreach line [split $err \n] {
	if { [string length $line] > 30 } {
	    append displayedErr "[string range $line 0 29]..."
	    break
	}
	if { $lines > 4 } {
	    append displayedErr "..."
	    break
	} else {
	    append displayedErr "${line}\n"
	}
	incr lines
    }

    set w .bgerrorDialog
    set title [mc "Application Error"]
    set text [mc {Error: %1$s} $err]
    set buttons [list ok $ok dismiss [mc "Skip Messages"] \
	    function [mc "Details >>"]]

    # 1. Create the top-level window and divide it into top
    # and bottom parts.

    catch {destroy .bgerrorDialog}
    toplevel .bgerrorDialog -class ErrorDialog
    wm title .bgerrorDialog $title
    wm iconname .bgerrorDialog ErrorDialog
    wm protocol .bgerrorDialog WM_DELETE_WINDOW { }

    if {$tcl_platform(platform) eq "windows"} {
	wm attributes .bgerrorDialog -topmost 1
    }
    if {($tcl_platform(platform) eq "macintosh")
            || ([tk windowingsystem] eq "aqua")} {
	::tk::unsupported::MacWindowStyle style .bgerrorDialog dBoxProc
    }

    frame .bgerrorDialog.bot
    frame .bgerrorDialog.top
    if {[tk windowingsystem] eq "x11"} {
	.bgerrorDialog.bot configure -relief raised -bd 1
	.bgerrorDialog.top configure -relief raised -bd 1
    }
    pack .bgerrorDialog.bot -side bottom -fill both
    pack .bgerrorDialog.top -side top -fill both -expand 1

    set W [frame $w.top.info]
    text $W.text				\
	    -bd 2				\
	    -yscrollcommand [list $W.scroll set]\
	    -setgrid true			\
	    -width 40				\
	    -height 10				\
	    -state normal			\
	    -relief $textRelief			\
	    -highlightthickness $textHilight	\
	    -wrap char

    scrollbar $W.scroll -relief sunken -command [list $W.text yview]
    pack $W.scroll -side right -fill y
    pack $W.text -side left -expand yes -fill both
    $W.text insert 0.0 "$err\n$info"
    $W.text mark set insert 0.0
    bind $W.text <ButtonPress-1> { focus %W }
    $W.text configure -state disabled

    # 2. Fill the top part with bitmap and message

    # Max-width of message is the width of the screen...
    set wrapwidth [winfo screenwidth .bgerrorDialog]
    # ...minus the width of the icon, padding and a fudge factor for
    # the window manager decorations and aesthetics.
    set wrapwidth [expr {$wrapwidth-60-[winfo pixels .bgerrorDialog 9m]}]
    label .bgerrorDialog.msg -justify left -text $text -font $messageFont \
	    -wraplength $wrapwidth
    if {($tcl_platform(platform) eq "macintosh")
            || ([tk windowingsystem] eq "aqua")} {
	# On the Macintosh, use the stop bitmap
	label .bgerrorDialog.bitmap -bitmap stop
    } else {
	# On other platforms, make the error icon
	canvas .bgerrorDialog.bitmap -width 32 -height 32 -highlightthickness 0
	.bgerrorDialog.bitmap create oval 0 0 31 31 -fill red -outline black
	.bgerrorDialog.bitmap create line 9 9 23 23 -fill white -width 4
	.bgerrorDialog.bitmap create line 9 23 23 9 -fill white -width 4
    }
    grid .bgerrorDialog.bitmap .bgerrorDialog.msg \
	    -in .bgerrorDialog.top	\
	    -row 0			\
	    -padx 3m			\
	    -pady 3m
    grid configure	 .bgerrorDialog.msg -sticky nsw -padx {0 3m}
    grid rowconfigure	 .bgerrorDialog.top 1 -weight 1
    grid columnconfigure .bgerrorDialog.top 1 -weight 1

    # 3. Create a row of buttons at the bottom of the dialog.

    set i 0
    foreach {name caption} $buttons {
	button .bgerrorDialog.$name	\
		-text $caption		\
		-default normal		\
		-command [namespace code "set button $i"]
	grid .bgerrorDialog.$name	\
		-in .bgerrorDialog.bot	\
		-column $i		\
		-row 0			\
		-sticky ew		\
		-padx 10
	grid columnconfigure .bgerrorDialog.bot $i -weight 1
	# We boost the size of some Mac buttons for l&f
	if {($tcl_platform(platform) eq "macintosh")
	    || ([tk windowingsystem] eq "aqua")} {
	    if {($name eq "ok") || ($name eq "dismiss")} {
		grid columnconfigure .bgerrorDialog.bot $i -minsize 79
	    }
	}
	incr i
    }
    # The "OK" button is the default for this dialog.
    .bgerrorDialog.ok configure -default active

    bind .bgerrorDialog <Return>	[namespace code Return]
    bind .bgerrorDialog <Destroy>	[namespace code [list Destroy %W]]
    .bgerrorDialog.function configure -command [namespace code Details]

    # 6. Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    wm withdraw .bgerrorDialog
    update idletasks
    set parent [winfo parent	.bgerrorDialog]
    set width  [winfo reqwidth	.bgerrorDialog]
    set height [winfo reqheight	.bgerrorDialog]
    set x [expr {([winfo screenwidth .bgerrorDialog]  - $width )/2 - \
	    [winfo vrootx $parent]}]
    set y [expr {([winfo screenheight .bgerrorDialog] - $height)/2 - \
	    [winfo vrooty $parent]}]
    .bgerrorDialog configure -width $width
    wm geometry .bgerrorDialog +$x+$y
    wm deiconify .bgerrorDialog

    # 7. Set a grab and claim the focus too.

    set oldFocus [focus]
    set oldGrab [grab current .bgerrorDialog]
    if {$oldGrab != ""} {
	set grabStatus [grab status $oldGrab]
    }
    grab .bgerrorDialog
    focus .bgerrorDialog.ok

    # 8. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    vwait [namespace which -variable button]
    set copy $button; # Save a copy...
    catch {focus $oldFocus}
    catch {destroy .bgerrorDialog}
    if {$oldGrab ne ""} {
	if {$grabStatus eq "global"} {
	    grab -global $oldGrab
	} else {
	    grab $oldGrab
	}
    }

    if {$copy == 1} {
	return -code break
    }
}

namespace eval :: {
    # Fool the indexer
    proc bgerror err {}
    rename bgerror {}
    namespace import ::tk::dialog::error::bgerror
}
