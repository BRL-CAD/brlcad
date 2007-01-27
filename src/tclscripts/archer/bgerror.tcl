#                     B G E R R O R . T C L
# BRL-CAD
#
# Copyright (c) 2000-2007 United States Government as represented by
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
#    Filename: bgerror.tcl
#      Author: Rick Garriques, Jr.
#        Date: 30-May-2000
# Description: This file contains the procedures used to override the
#              standard Tcl/Tk error code. To use it all you need to do
#              is source it into your main Tcl/Tk file. The following
#              global variables are used in this code (they could interfer
#              with other code):
#
#                 bgerrorMessage - holds error message displayed in error dialog
#                 bgerrorLog    - holds stack track error
#                 bgerrorInfo   - holds user entered information
#                 bgerrorButton - allows for notification of a button press
#
#   Revisions:
#              -----
#
#              Miscellaneous changes, standardized globals, etc.
#
#              12 Jun 00
#              MTB
#              -----
#
#              Changed error text height to from 30 to 20.
#
#              25 May 01
#              MTB
#              -----

# globals
set bgerrorMessage ""
set bgerrorLog ""
set bgerrorInfo ""
set bgerrorButton ""

proc initBgerror {} {
    # Do nothing
}

#--------------------------------------------------------------------
#   Procedure: bgerrorExit
# Description: Procedure used to terminate program
#       Input: base    - the parent window
#     Returns: nothing
#--------------------------------------------------------------------
proc bgerrorExit { base } {
    global App

    set answer [tk_messageBox \
	    -message "Are you sure you want to terminate the program (without saving)?" \
	    -icon question \
	    -parent $base \
	    -type yesno -title "Exit Confirmation"]

    # cleanup stuff used by the gui
    if {[string equal $answer "yes"]} {
	if [info exists App::gui] {
	    $App::gui CloseServer
	    $App::gui MemoryCleanup
	}
	exit
    }

    # set bgerrorButton Exit
}

#--------------------------------------------------------------------
#   Procedure: bgerrorSave
# Description: Procedure used to save error log to fiile
#       Input: base    - the parent window
#     Returns: nothing
#--------------------------------------------------------------------
proc bgerrorSave { base } {
    global bgerrorMessage
    global bgerrorLog
    global bgerrorInfo
    global bgerrorButton

    set bgerrorInfo [$base.frame#4.errorInfo.userText get 1.0 end]

    set itemList {
	{{Text Files} {.txt}}
	{{All Files} * }
    }

    set file [tk_getSaveFile  -defaultextension "" \
	    -initialfile "error.txt" \
	    -title "Save/Append Error Log File" \
	    -filetypes $itemList \
	    -parent $base]

    if {$file == ""} {return}

    set fid [open $file a]
    set currentDateTime [clock format [clock seconds]]

    puts $fid $currentDateTime
    puts $fid ""
    puts $fid "Error Message:"
    puts $fid $bgerrorMessage
    puts $fid ""
    puts $fid "Stack Trace:"
    puts $fid $bgerrorLog
    puts $fid ""
    if {$bgerrorInfo == "\n"} {
	# empty user information box, do nothing
    } else {
	puts $fid "User Comments:"
	puts $fid $bgerrorInfo
    }
    puts $fid "-- End of Error --"
    puts $fid ""
    close $fid

    # set bgerrorButton SAVE
}

#--------------------------------------------------------------------
#   Procedure: bgerrorToggleStack
# Description: Procedure called when pressing the little arrow.
#       Input: arrow   - the arrow
#              stack   - the frame holding all the stack information,
#                           etc.
#     Returns: nothing
#--------------------------------------------------------------------
proc bgerrorToggleStack {arrow stack} {
    switch [$arrow cget -togglestate] {
	"closed" {pack forget $stack}
	"open"   {pack $stack -side bottom -expand yes -fill both}
    }
}

#--------------------------------------------------------------------
#   Procedure: secErrorDialog
# Description: Procedure used to display an error dialog
#       Input: root    - the root window
#              title   - the window title
#              text    - the error message to display
#              default - the default button value
#              args    - the list of buttons values
#     Returns: Either OK or SAVE
#--------------------------------------------------------------------
proc secErrorDialog {root title text default args} {
    global bgerrorLog
    global bgerrorInfo
    global bgerrorButton

    set bgerrorArrowColor "blue"

    if {$root == "."} {
	set base ""
    } else {
	set base $root
    }

    # Create error window dialog
    toplevel $base \
	    -class Dialog
    wm title $base $title
    wm iconname $base Dialog
    wm resizable $base 0 0
    wm withdraw $base
    update idletasks

    # First frame consists of error icon and message
    frame $base.frame#1 \
	    -class Panedwindow
    pack $base.frame#1 -fill x -anchor w

    canvas $base.frame#1.bitmap \
	    -width 32 \
	    -height 32 \
	    -highlightthickness 0
    $base.frame#1.bitmap create oval 0 0 31 31 \
	    -fill red \
	    -outline black
    $base.frame#1.bitmap create line 9 9 23 23 \
	    -fill white \
	    -width 4
    $base.frame#1.bitmap create line 9 23 23 9 \
	    -fill white \
	    -width 4
    pack $base.frame#1.bitmap \
	    -side left \
	    -padx 3m \
	    -pady 3m

    label $base.frame#1.msg \
	    -text $text \
	    -width 80 \
	    -wraplength 525 \
	    -height 3 \
	    -anchor w \
	    -justify left
    pack $base.frame#1.msg \
	    -side left

    # Second frame holds the buttons
    frame $base.frame#2
    pack $base.frame#2 \
	    -fill both

    set i 0
    foreach but $args {
	if { [string equal $but "Exit"] } {
	    button $base.frame#2.button$i \
		    -width 6 \
		    -text $but \
		    -under 0 \
		    -command "bgerrorExit $base"
	} else {
	    button $base.frame#2.button$i \
		    -width 6 \
		    -text $but \
		    -under 0 \
		    -command "set bgerrorButton $but"
	}
	set underIdx 0
	if {$underIdx >= 0} {
	    set key [string index $but $underIdx]
	    bind $base <Alt-[string tolower $key]>  \
		    [list $base.frame#2.button$i invoke]

	    bind $base <Alt-[string toupper $key]>  \
		    [list $base.frame#2.button$i invoke]
	}

	if {[string equal $default $but]} {
	    $base.frame#2.button$i configure \
		    -default active
	}
	pack $base.frame#2.button$i \
		-side right \
		-expand 1 \
		-padx 3m
	incr i
    }

    # Third frame holds the controls to displays the fourth frame or not
    set frm [frame $base.frame#3]
    set arrow [::swidgets::togglearrow $frm.arrow \
	    -color $bgerrorArrowColor \
	    -outline grey]
    $arrow configure -command "bgerrorToggleStack $arrow $base.frame#4"
    pack $arrow -side left
    pack $frm -fill x
    frame $base.frame#3.sep \
	    -borderwidth 1 \
	    -height 2 \
	    -relief sunken
    pack $base.frame#3.sep \
	    -fill x \
	    -padx 1 \
	    -pady 8

    # The fourth frame is hidden at first but displays the error and allows
    # the user to enter comments to save in the log file
    frame $base.frame#4
    frame $base.frame#4.errorLog
    label $base.frame#4.errorLog.msg \
	    -text "The error stack trace:" \
	    -anchor w
    grid $base.frame#4.errorLog.msg \
	    -row 0 \
	    -column 0 \
	    -sticky w
    text $base.frame#4.errorLog.text \
	    -yscrollcommand "$base.frame#4.errorLog.ysbar set" \
	    -xscrollcommand "$base.frame#4.errorLog.xsbar set" \
	    -wrap none \
	    -width 45 \
	    -height 20
    grid $base.frame#4.errorLog.text \
	    -row 1 \
	    -column 0 \
	    -sticky news
    scrollbar $base.frame#4.errorLog.xsbar \
	    -command "$base.frame#4.errorLog.text xview" \
	    -orient horizontal
    grid $base.frame#4.errorLog.xsbar \
	    -row 2 \
	    -column 0 \
	    -sticky ew
    scrollbar $base.frame#4.errorLog.ysbar \
	    -command "$base.frame#4.errorLog.text yview" \
	    -orient vertical
    grid $base.frame#4.errorLog.ysbar \
	    -row 1 \
	    -column 1 \
	    -sticky ns
    grid columnconfigure $base.frame#4.errorLog 0 -weight 1
    grid rowconfigure $base.frame#4.errorLog 1 -weight 1

    $base.frame#4.errorLog.text insert end $bgerrorLog
    $base.frame#4.errorLog.text configure \
	    -state disabled
    grid $base.frame#4.errorLog \
	    -row 0 \
	    -column 0 \
	    -sticky news

    frame $base.frame#4.errorInfo
    label $base.frame#4.errorInfo.msg \
	    -text "User comment:" \
	    -anchor w
    grid $base.frame#4.errorInfo.msg \
	    -row 0 \
	    -column 0 \
	    -sticky w
    text $base.frame#4.errorInfo.userText \
	    -yscrollcommand "$base.frame#4.errorInfo.utsbar set" \
	    -wrap word \
	    -width 45 \
	    -height 5 \
	    -bg white
    grid $base.frame#4.errorInfo.userText \
	    -row 1 \
	    -column 0 \
	    -sticky news
    scrollbar $base.frame#4.errorInfo.utsbar \
	    -command "$base.frame#4.errorInfo.userText yview" \
	    -orient vertical
    grid $base.frame#4.errorInfo.utsbar \
	    -row 1 \
	    -column 1 \
	    -sticky ns
    grid columnconfigure $base.frame#4.errorInfo 0 -weight 1
    grid rowconfigure $base.frame#4.errorInfo 1 -weight 1

    grid $base.frame#4.errorInfo \
	    -row 1 \
	    -column 0 \
	    -sticky news

    button $base.frame#4.button \
	    -text "Save As..." \
	    -under 0 \
	    -command "bgerrorSave $base"
    grid $base.frame#4.button \
	    -row 2 \
	    -column 0 \
	    -pady 3

    grid columnconfigure $base.frame#4 0 -weight 1
    grid rowconfigure $base.frame#4 0 -weight 1

    update idletasks
    set bgerrorX [expr ([winfo screenwidth .]-[winfo reqwidth $base])/2]
    set bgerrorY [expr ([winfo screenheight .])/5]
    wm geometry $base +$bgerrorX+$bgerrorY
    wm deiconify $base

    bind $base <FocusIn> {
	if {[string equal Button [winfo class %W]]} {
	    %W configure -default active
	}
    }
    bind $base <FocusOut> {
	if {[string equal Button [winfo class %W]]} {
	    %W configure -default normal
	}
    }

    bind $base <Return> {
	if {[string equal Button [winfo class %W]]} {
	    tkButtonInvoke %W
	}
    }

    set oldFocus [focus]
    # grab set $base
    focus $base.frame#2.button0

    tkwait variable bgerrorButton

    destroy $base
    focus $oldFocus
    return $bgerrorButton

}

#--------------------------------------------------------------------
#   Procedure: secMessageBox
# Description: Procedure used to display a message box uses tk_messageBox
#              unless the type is error
#       Input: args - variable configuration commands
#     Returns: The value of the selected button
#--------------------------------------------------------------------
proc secMessageBox {args} {

    set answer " "

    set w tkPrivMsgBox
    upvar #0 $w data
    set specs {
	{-default "" "" ""}
	{-icon "" "" "info"}
	{-message "" "" ""}
	{-parent "" "" .}
	{-title "" "" " "}
	{-type "" "" "ok"}
    }
    tclParseConfigSpec $w $specs "" $args

    if {[lsearch -exact {error abortretryignore ok okcancel retrycancel yesno yesnocancel} $data(-type)] == -1} {
	error "bad -type value \"$args(-type)\": must be error abortretryignore, ok, okcancel, retrycancel, yesno, or yesnocancel"
    }

    if {[lsearch -exact {abortretryignore ok okcancel retrycancel yesno yesnocancel} $data(-type)] >= 0} {
	set answer [eval tk_messageBox $args]
    } else {

	if {![winfo exists $data(-parent)]} {
	    error "bad window path name \"$data(-parent)\""
	}
	if {[string compare $data(-parent) .]} {
	    set w $data(-parent).__sec__errordialog
	} else {
	    set w .__sec__errordialog
	}

	set answer [secErrorDialog $data(-parent)_secErrorDialog $data(-title) $data(-message) OK OK Exit]
    }

    return $answer
}

#--------------------------------------------------------------------
#   Procedure: bgerror
# Description: Procedure to replace the standard error dialog and allow
#              for saving the stack trace and user comments
#       Input: error - The error message to display
#     Results:
#--------------------------------------------------------------------
proc bgerror {error} {
    global bgerrorMessage
    global bgerrorLog
    global App

    # tcl error variable
    global errorInfo

    # save text of original error
    set bgerrorLog $errorInfo

    if { $error == "" } {
	if { $errorInfo == "" } {
	    # if no message and no stack trace do nothing
	    return
	}
	set error "Error: An unexpected error has occurred."
    }

    # set global variable for bgerrorSave procedure
    set bgerrorMessage $error

    # display error dialog
    set answer [secMessageBox \
	    -icon error \
	    -type error \
	    -title Error \
	    -message $bgerrorMessage]

    # potentially perform additional tasks upon return of error dialog
    case $answer {
	OK {
	    # Make sure the wait cursor is unlocked
	    if [info exists App::gui] {
		$App::gui SetStatusString
	    }
	    SetNormalCursor
      }
      SAVE {
	  # already handled by bgerrorSave procedure
      }
      Exit {
	  # already handled by bgerrorExit procedure
      }
  }
}

#--------------------------------------------------------------------
# Allow interface to be run "stand-alone" for testing
#--------------------------------------------------------------------
catch {
    if {$argv0 == [info script]} {
	wm title . "Test"
	button .error1 -text "Generate Error" -command {error "This is a test error!"}
	pack .error1 -fill x -expand 1
	button .error2 -text "Long Error" -command {error "This is a test error that is really long and will wrap around the dialog window.  Many Tcl/Tk errors may wrap and we need to take care of these instances.  The dialog window is designed to handle up to 3 relatively long lines of text."}
	pack .error2 -fill x -expand 1
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
