# choosedir.tcl --
#
#	Choose directory dialog implementation for Unix/Mac.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id$

# Make sure the tk::dialog namespace, in which all dialogs should live, exists
namespace eval ::tk::dialog {}
namespace eval ::tk::dialog::file {}

# Make the chooseDir namespace inside the dialog namespace
namespace eval ::tk::dialog::file::chooseDir {
}

# ::tk::dialog::file::tkChooseDirectory --
#
#	Implements the TK directory selection dialog.
#
# Arguments:
#	args		Options parsed by the procedure.
#
proc ::tk::dialog::file::chooseDir::tkChooseDirectory {args} {
    global tkPriv
    set dataName __tk_choosedir
    upvar ::tk::dialog::file::$dataName data
    ::tk::dialog::file::chooseDir::Config $dataName $args

    if {[string equal $data(-parent) .]} {
        set w .$dataName
    } else {
        set w $data(-parent).$dataName
    }

    # (re)create the dialog box if necessary
    #
    if {![winfo exists $w]} {
	::tk::dialog::file::Create $w TkChooseDir
    } elseif {[string compare [winfo class $w] TkChooseDir]} {
	destroy $w
	::tk::dialog::file::Create $w TkChooseDir
    } else {
	set data(dirMenuBtn) $w.f1.menu
	set data(dirMenu) $w.f1.menu.menu
	set data(upBtn) $w.f1.up
	set data(icons) $w.icons
	set data(ent) $w.f2.ent
	set data(okBtn) $w.f2.ok
	set data(cancelBtn) $w.f3.cancel
    }
    wm transient $w $data(-parent)

    trace variable data(selectPath) w [list ::tk::dialog::file::SetPath $w]
    $data(dirMenuBtn) configure \
	    -textvariable ::tk::dialog::file::${dataName}(selectPath)

    set data(filter) "*"
    set data(previousEntryText) ""
    ::tk::dialog::file::UpdateWhenIdle $w

    # Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    ::tk::PlaceWindow $w widget $data(-parent)
    wm title $w $data(-title)

    # Set a grab and claim the focus too.

    ::tk::SetFocusGrab $w $data(ent)
    $data(ent) delete 0 end
    $data(ent) insert 0 $data(selectPath)
    $data(ent) selection range 0 end
    $data(ent) icursor end

    # Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable tkPriv(selectFilePath)

    ::tk::RestoreFocusGrab $w $data(ent) withdraw

    # Cleanup traces on selectPath variable
    #

    foreach trace [trace vinfo data(selectPath)] {
	trace vdelete data(selectPath) [lindex $trace 0] [lindex $trace 1]
    }
    $data(dirMenuBtn) configure -textvariable {}

    # Return value to user
    #
    
    return $tkPriv(selectFilePath)
}

# ::tk::dialog::file::chooseDir::Config --
#
#	Configures the Tk choosedir dialog according to the argument list
#
proc ::tk::dialog::file::chooseDir::Config {dataName argList} {
    upvar ::tk::dialog::file::$dataName data

    # 0: Delete all variable that were set on data(selectPath) the
    # last time the file dialog is used. The traces may cause troubles
    # if the dialog is now used with a different -parent option.
    #
    foreach trace [trace vinfo data(selectPath)] {
	trace vdelete data(selectPath) [lindex $trace 0] [lindex $trace 1]
    }

    # 1: the configuration specs
    #
    set specs {
	{-mustexist "" "" 0}
	{-initialdir "" "" ""}
	{-parent "" "" "."}
	{-title "" "" ""}
    }

    # 2: default values depending on the type of the dialog
    #
    if {![info exists data(selectPath)]} {
	# first time the dialog has been popped up
	set data(selectPath) [pwd]
    }

    # 3: parse the arguments
    #
    tclParseConfigSpec ::tk::dialog::file::$dataName $specs "" $argList

    if {$data(-title) == ""} {
	set data(-title) "Choose Directory"
    }

    # 4: set the default directory and selection according to the -initial
    #    settings
    #
    if {$data(-initialdir) != ""} {
	# Ensure that initialdir is an absolute path name.
	if {[file isdirectory $data(-initialdir)]} {
	    set old [pwd]
	    cd $data(-initialdir)
	    set data(selectPath) [pwd]
	    cd $old
	} else {
	    set data(selectPath) [pwd]
	}
    }

    if {![winfo exists $data(-parent)]} {
	error "bad window path name \"$data(-parent)\""
    }
}

# Gets called when user presses Return in the "Selection" entry or presses OK.
#
proc ::tk::dialog::file::chooseDir::OkCmd {w} {
    upvar ::tk::dialog::file::[winfo name $w] data

    # This is the brains behind selecting non-existant directories.  Here's
    # the flowchart:
    # 1.  If the icon list has a selection, join it with the current dir,
    #     and return that value.
    # 1a.  If the icon list does not have a selection ...
    # 2.  If the entry is empty, do nothing.
    # 3.  If the entry contains an invalid directory, then...
    # 3a.   If the value is the same as last time through here, end dialog.
    # 3b.   If the value is different than last time, save it and return.
    # 4.  If entry contains a valid directory, then...
    # 4a.   If the value is the same as the current directory, end dialog.
    # 4b.   If the value is different from the current directory, change to
    #       that directory.

    set iconText [tkIconList_Get $data(icons)]
    if { ![string equal $iconText ""] } {
	set iconText [file join $data(selectPath) $iconText]
	::tk::dialog::file::chooseDir::Done $w $iconText
    } else {
	set text [$data(ent) get]
	if { [string equal $text ""] } {
	    return
	}
	set text [eval file join [file split [string trim $text]]]
	if { ![file exists $text] || ![file isdirectory $text] } {
	    # Entry contains an invalid directory.  If it's the same as the
	    # last time they came through here, reset the saved value and end
	    # the dialog.  Otherwise, save the value (so we can do this test
	    # next time).
	    if { [string equal $text $data(previousEntryText)] } {
		set data(previousEntryText) ""
		::tk::dialog::file::chooseDir::Done $w $text
	    } else {
		set data(previousEntryText) $text
	    }
	} else {
	    # Entry contains a valid directory.  If it is the same as the
	    # current directory, end the dialog.  Otherwise, change to that
	    # directory.
	    if { [string equal $text $data(selectPath)] } {
		::tk::dialog::file::chooseDir::Done $w $text
	    } else {
		set data(selectPath) $text
	    }
	}
    }
    return
}

proc ::tk::dialog::file::chooseDir::DblClick {w} {
    upvar ::tk::dialog::file::[winfo name $w] data
    set text [tkIconList_Get $data(icons)]
    if {[string compare $text ""]} {
	set file $data(selectPath)
	if {[file isdirectory $file]} {
	    ::tk::dialog::file::ListInvoke $w $text
	    return
	}
    }
}    

# Gets called when user browses the IconList widget (dragging mouse, arrow
# keys, etc)
#
proc ::tk::dialog::file::chooseDir::ListBrowse {w text} {
    upvar ::tk::dialog::file::[winfo name $w] data

    if {[string equal $text ""]} {
	return
    }

    set file [::tk::dialog::file::JoinFile $data(selectPath) $text]
    $data(ent) delete 0 end
    $data(ent) insert 0 $file
}

# ::tk::dialog::file::chooseDir::Done --
#
#	Gets called when user has input a valid filename.  Pops up a
#	dialog box to confirm selection when necessary. Sets the
#	tkPriv(selectFilePath) variable, which will break the "tkwait"
#	loop in tk_chooseDirectory and return the selected filename to the
#	script that calls tk_getOpenFile or tk_getSaveFile
#
proc ::tk::dialog::file::chooseDir::Done {w {selectFilePath ""}} {
    upvar ::tk::dialog::file::[winfo name $w] data
    global tkPriv

    if {[string equal $selectFilePath ""]} {
	set selectFilePath $data(selectPath)
    }
    if { $data(-mustexist) } {
	if { ![file exists $selectFilePath] || \
		![file isdir $selectFilePath] } {
	    return
	}
    }
    set tkPriv(selectFilePath) $selectFilePath
}
