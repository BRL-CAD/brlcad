#
# $Id$
#
# Copyright (c) 2005, Joe English.  Freely redistributable.
#
# Ttk widget set: dialog boxes.
#
# TODO: option to keep dialog onscreen ("persistent" / "transient")
# TODO: accelerator keys.
# TODO: use message catalogs for button labels
# TODO: routines to selectively enable/disable individual command buttons
# TODO: use megawidgetoid API [$dlg dismiss] vs. [ttk::dialog::dismiss $dlg]
# TODO: MAYBE: option for app-modal dialogs
# TODO: MAYBE: [wm withdraw] dialog on dismiss instead of self-destructing 
#

namespace eval ttk::dialog {

    variable Config
    #
    # Spacing parameters:
    # (taken from GNOME HIG 2.0, may need adjustment for other platforms)
    # (textwidth just a guess)
    #
    set Config(margin)		12	;# space between icon and text
    set Config(interspace)	6	;# horizontal space between buttons
    set Config(sepspace) 	24	;# vertical space above buttons
    set Config(textwidth) 	400	;# width of dialog box text (pixels)

    variable DialogTypes	;# map -type => list of dialog options
    variable ButtonOptions	;# map button name => list of button options

    # stockButton -- define new built-in button
    #
    proc stockButton {button args} {
	variable ButtonOptions
	set ButtonOptions($button) $args
    }

    # Built-in button types:
    #
    stockButton ok 	-text OK
    stockButton cancel	-text Cancel
    stockButton yes	-text Yes
    stockButton no	-text No
    stockButton retry 	-text Retry

    # stockDialog -- define new dialog type.
    #
    proc stockDialog {type args} {
	variable DialogTypes
	set DialogTypes($type) $args
    }

    # Built-in dialog types:
    #
    stockDialog ok \
	-icon info -buttons {ok} -default ok
    stockDialog okcancel \
	-icon info -buttons {ok cancel} -default ok -cancel cancel
    stockDialog retrycancel \
	-icon question -buttons {retry cancel} -cancel cancel
    stockDialog yesno \
	-icon question -buttons {yes no}
    stockDialog yesnocancel \
	-icon question -buttons {yes no cancel} -cancel cancel
}

## ttk::dialog::nop --
#	Do nothing (used as a default callback command). 
#
proc ttk::dialog::nop {args} { }

## ttk::dialog -- dialog box constructor.
#
interp alias {} ttk::dialog {} ttk::dialog::Constructor

proc ttk::dialog::Constructor {dlg args} {
    upvar #0 $dlg D
    variable Config
    variable ButtonOptions
    variable DialogTypes

    # 
    # Option processing:
    #
    array set defaults {
	-title 		""
    	-message	""
	-detail		""
	-command	ttk::dialog::nop
	-icon 		""
	-buttons 	{}
	-labels 	{}
	-default 	{}
	-cancel		{}
	-parent		#AUTO
    }

    array set options [array get defaults]

    foreach {option value} $args {
	if {$option eq "-type"} {
	    array set options $DialogTypes($value)
	} elseif {![info exists options($option)]} {
	    set validOptions [join [lsort [array names options]] ", "]
	    return -code error \
	    	"Illegal option $option: must be one of $validOptions"
	}
    }
    array set options $args

    # ...
    #
    array set buttonOptions [array get ::ttk::dialog::ButtonOptions]
    foreach {button label} $options(-labels) {
	lappend buttonOptions($button) -text $label
    }

    #
    # Initialize dialog private data:
    #
    foreach option {-command -message -detail} {
	set D($option) $options($option)
    }

    toplevel $dlg -class Dialog; wm withdraw $dlg

    #
    # Determine default transient parent.
    #
    # NB: menus (including menubars) are considered toplevels,
    # so skip over those. 
    #
    if {$options(-parent) eq "#AUTO"} {
	set parent [winfo toplevel [winfo parent $dlg]]
	while {[winfo class $parent] eq "Menu" && $parent ne "."} {
	    set parent [winfo toplevel [winfo parent $parent]]
	}
	set options(-parent) $parent
    }

    #
    # Build dialog:
    #
    if {$options(-parent) ne ""} {
    	wm transient $dlg $options(-parent)
    }
    wm title $dlg $options(-title)
    wm protocol $dlg WM_DELETE_WINDOW { }

    set f [ttk::frame $dlg.f] 

    ttk::label $f.icon 
    if {$options(-icon) ne ""} {
	$f.icon configure -image [ttk::stockIcon dialog/$options(-icon)]
    }
    ttk::label $f.message -textvariable ${dlg}(-message) \
    	-font TkCaptionFont -wraplength $Config(textwidth)\
	-anchor w -justify left
    ttk::label $f.detail -textvariable ${dlg}(-detail) \
    	-font TkTextFont -wraplength $Config(textwidth) \
	-anchor w -justify left

    #
    # Command buttons:
    #
    set cmd [ttk::frame $f.cmd]
    set column 0
    grid columnconfigure $f.cmd 0 -weight 1

    foreach button $options(-buttons) {
	incr column
	eval [linsert $buttonOptions($button) 0 ttk::button $cmd.$button]
        $cmd.$button configure -command [list ttk::dialog::Done $dlg $button]
    	grid $cmd.$button -row 0 -column $column \
	    -padx [list $Config(interspace) 0] -sticky ew
	grid columnconfigure $cmd $column -uniform buttons
    }

    if {$options(-default) ne ""} {
    	keynav::defaultButton $cmd.$options(-default)
	focus $cmd.$options(-default)
    }
    if {$options(-cancel) ne ""} {
	bind $dlg <KeyPress-Escape> \
	    [list event generate $cmd.$options(-cancel) <<Invoke>>]
	wm protocol $dlg WM_DELETE_WINDOW \
	    [list event generate $cmd.$options(-cancel) <<Invoke>>]
    }

    #
    # Assemble dialog.
    #
    pack $f.cmd -side bottom -expand false -fill x \
    	-pady [list $Config(sepspace) $Config(margin)] -padx $Config(margin)

    if {0} {
	# GNOME and Apple HIGs say not to use separators.
	# But in case we want them anyway:
	#
	pack [ttk::separator $f.sep -orient horizontal] \
	    -side bottom -expand false -fill x \
	    -pady [list $Config(sepspace) 0] \
	    -padx $Config(margin)
    }

    if {$options(-icon) ne ""} {
	pack $f.icon -side left -anchor n -expand false \
	    -pady $Config(margin) -padx $Config(margin)
    }

    pack $f.message -side top -expand false -fill x \
    	-padx $Config(margin) -pady $Config(margin)
    if {$options(-detail) != ""} {
	pack $f.detail -side top -expand false -fill x \
	    -padx $Config(margin)
    }

    # Client area goes here.

    pack $f -expand true -fill both
    keynav::enableMnemonics $dlg
    wm deiconify $dlg
}

## ttk::dialog::clientframe --
#	Returns the widget path of the dialog client frame,
#	creating and managing it if necessary.
#
proc ttk::dialog::clientframe {dlg} { 
    variable Config
    set client $dlg.f.client
    if {![winfo exists $client]} {
	pack [ttk::frame $client] -side top -expand true -fill both \
		-pady $Config(margin) -padx $Config(margin)
	lower $client	;# so it's first in keyboard traversal order
    }
    return $client 
}

## ttk::dialog::Done --
#	-command callback for dialog command buttons (internal)
#
proc ttk::dialog::Done {dlg button} {
    upvar #0 $dlg D
    set rc [catch [linsert $D(-command) end $button] result]
    if {$rc == 1} {
    	return -code $rc -errorinfo $::errorInfo -errorcode $::errorCode $result
    } elseif {$rc == 3 || $rc == 4} {
	# break or continue -- don't dismiss dialog
	return
    }
    dismiss $dlg
}

## ttk::dialog::activate $dlg $button --
#	Simulate a button press.
#
proc ttk::dialog::activate {dlg button} {
    event generate $dlg.f.cmd.$button <<Invoke>>
}

## dismiss --
#	Dismiss the dialog (without invoking any actions).
#
proc ttk::dialog::dismiss {dlg} {
    uplevel #0 [list unset $dlg]
    destroy $dlg
}

#*EOF*
