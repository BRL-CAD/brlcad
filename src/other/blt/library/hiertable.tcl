# hiertable.tcl
# ----------------------------------------------------------------------
# Bindings for the BLT hiertable widget
# ----------------------------------------------------------------------
#   AUTHOR:  George Howlett
#            Bell Labs Innovations for Lucent Technologies
#            gah@lucent.com
#            http://www.tcltk.com/blt
#
#      RCS:  $Id$
#
# ----------------------------------------------------------------------
# Copyright (c) 1998  Lucent Technologies, Inc.
# ======================================================================
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that the copyright notice and warranty disclaimer appear in
# supporting documentation, and that the names of Lucent Technologies
# any of their entities not be used in advertising or publicity
# pertaining to distribution of the software without specific, written
# prior permission.
#
# Lucent Technologies disclaims all warranties with regard to this
# software, including all implied warranties of merchantability and
# fitness.  In no event shall Lucent be liable for any special, indirect
# or consequential damages or any damages whatsoever resulting from loss
# of use, data or profits, whether in an action of contract, negligence
# or other tortuous action, arising out of or in connection with the use
# or performance of this software.
#
# ======================================================================

namespace eval blt::Hiertable {
    set afterId ""
    set scroll 0
    set column ""
    set space   off
    set x 0
    set y 0
}

# 
# ButtonPress assignments
#
#	B1-Enter	start auto-scrolling
#	B1-Leave	stop auto-scrolling
#	ButtonPress-2	start scan
#	B2-Motion	adjust scan
#	ButtonRelease-2 stop scan
#

bind Hiertable <ButtonPress-2> {
    set blt::Hiertable::cursor [%W cget -cursor]
    %W configure -cursor hand1
    %W scan mark %x %y
}

bind Hiertable <B2-Motion> {
    %W scan dragto %x %y
}

bind Hiertable <ButtonRelease-2> {
    %W configure -cursor $blt::Hiertable::cursor
}

bind Hiertable <B1-Leave> {
    if { $blt::Hiertable::scroll } {
	blt::Hiertable::AutoScroll %W 
    }
}

bind Hiertable <B1-Enter> {
    after cancel $blt::Hiertable::afterId
}

# 
# KeyPress assignments
#
#	Up			
#	Down
#	Shift-Up
#	Shift-Down
#	Prior (PageUp)
#	Next  (PageDn)
#	Left
#	Right
#	space		Start selection toggle of entry currently with focus.
#	Return		Start selection toggle of entry currently with focus.
#	Home
#	End
#	F1
#	F2
#	ASCII char	Go to next open entry starting with character.
#
# KeyRelease
#
#	space		Stop selection toggle of entry currently with focus.
#	Return		Stop selection toggle of entry currently with focus.


bind Hiertable <KeyPress-Up> {
    blt::Hiertable::MoveFocus %W up
    if { $blt::Hiertable::space } {
	%W selection toggle focus
    }
}

bind Hiertable <KeyPress-Down> {
    blt::Hiertable::MoveFocus %W down
    if { $blt::Hiertable::space } {
	%W selection toggle focus
    }
}

bind Hiertable <Shift-KeyPress-Up> {
    blt::Hiertable::MoveFocus %W prevsibling
}

bind Hiertable <Shift-KeyPress-Down> {
    blt::Hiertable::MoveFocus %W nextsibling
}

bind Hiertable <KeyPress-Prior> {
    blt::Hiertable::MovePage %W top
}

bind Hiertable <KeyPress-Next> {
    blt::Hiertable::MovePage %W bottom
}

bind Hiertable <KeyPress-Left> {
    %W close focus
}
bind Hiertable <KeyPress-Right> {
    %W open focus
    %W see focus -anchor w
}

bind Hiertable <KeyPress-space> {
    if { [%W cget -selectmode] == "single" } {
	if { [%W selection includes focus] } {
	    %W selection clearall
	} else {
	    %W selection clearall
	    %W selection set focus
	}
    } else {
	%W selection toggle focus
    }
    set blt::Hiertable::space on
}

bind Hiertable <KeyRelease-space> { 
    set blt::Hiertable::space off
}

bind Hiertable <KeyPress-Return> {
    blt::Hiertable::MoveFocus %W focus
    set blt::Hiertable::space on
}

bind Hiertable <KeyRelease-Return> { 
    set blt::Hiertable::space off
}

bind Hiertable <KeyPress> {
    blt::Hiertable::NextMatchingEntry %W %A
}

bind Hiertable <KeyPress-Home> {
    blt::Hiertable::MoveFocus %W top
}

bind Hiertable <KeyPress-End> {
    blt::Hiertable::MoveFocus %W bottom
}

bind Hiertable <KeyPress-F1> {
    %W open -r root
}

bind Hiertable <KeyPress-F2> {
    eval %W close -r [%W entry children root] 
}

#
# Differences between "current" and nearest.
#
#	set index [$widget index current]
#	set index [$widget nearest $x $y]
#
#	o Nearest gives you the closest entry.
#	o current is "" if
#	   1) the pointer isn't over an entry.
#	   2) the pointer is over a open/close button.
#	   3) 
#
#
# ----------------------------------------------------------------------
#
# USAGE: blt::Hiertable::Init <hiertable> 
#
# Invoked by internally by Hiertable_Init routine.  Initializes the 
# default bindings for the hiertable widget entries.  These are local
# to the widget, so they can't be set through the widget's class
# bind tags.
#
# Arguments:	hiertable		hierarchy widget
#
# ----------------------------------------------------------------------
proc blt::Hiertable::Init { widget } {
    #
    # Active entry bindings
    #
    $widget bind Entry <Enter> { 
	%W entry highlight current 
    }
    $widget bind Entry <Leave> { 
	%W entry highlight "" 
    }

    #
    # Button bindings
    #
    $widget button bind all <ButtonRelease-1> {
	%W see -anchor nw current
	%W toggle current
    }
    $widget button bind all <Enter> {
	%W button highlight current
    }
    $widget button bind all <Leave> {
	%W button highlight ""
    }

    #
    # ButtonPress-1
    #
    #	Performs the following operations:
    #
    #	1. Clears the previous selection.
    #	2. Selects the current entry.
    #	3. Sets the focus to this entry.
    #	4. Scrolls the entry into view.
    #	5. Sets the selection anchor to this entry, just in case
    #	   this is "multiple" mode.
    #
    
    $widget bind Entry <ButtonPress-1> { 	
	blt::Hiertable::SetSelectionAnchor %W current
	set blt::Hiertable::scroll 1
    }

    $widget bind Entry <Double-ButtonPress-1> {
	%W toggle current
    }

    #
    # B1-Motion
    #
    #	For "multiple" mode only.  Saves the current location of the
    #	pointer for auto-scrolling.  Resets the selection mark.  
    #
    $widget bind Entry <B1-Motion> { 
	set blt::Hiertable::x %x
	set blt::Hiertable::y %y
	set index [%W nearest %x %y]
	if { [%W cget -selectmode] == "multiple" } {
	    %W selection mark $index
	} else {
	    blt::Hiertable::SetSelectionAnchor %W $index
	}
    }

    #
    # ButtonRelease-1
    #
    #	For "multiple" mode only.  
    #
    $widget bind Entry <ButtonRelease-1> { 
	if { [%W cget -selectmode] == "multiple" } {
	    %W selection anchor current
	}
	after cancel $blt::Hiertable::afterId
	set blt::Hiertable::scroll 0
    }

    #
    # Shift-ButtonPress-1
    #
    #	For "multiple" mode only.
    #

    $widget bind Entry <Shift-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" && [%W selection present] } {
	    if { [%W index anchor] == "" } {
		%W selection anchor current
	    }
	    set index [%W index anchor]
	    %W selection clearall
	    %W selection set $index current
	} else {
	    blt::Hiertable::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Shift-Double-ButtonPress-1> {
	puts <Shift-Double-ButtonPress-1> 
	# do nothing
    }
    $widget bind Entry <Shift-B1-Motion> { 
	# do nothing
    }
    $widget bind Entry <Shift-ButtonRelease-1> { 
	after cancel $blt::Hiertable::afterId
	set blt::Hiertable::scroll 0
    }

    #
    # Control-ButtonPress-1
    #
    #	For "multiple" mode only.  
    #
    $widget bind Entry <Control-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" } {
	    set index [%W index current]
	    %W selection toggle $index
	    %W selection anchor $index
	} else {
	    blt::Hiertable::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Control-Double-ButtonPress-1> {
	puts <Control-Double-ButtonPress-1> 
	# do nothing
    }
    $widget bind Entry <Control-B1-Motion> { 
	# do nothing
    }
    $widget bind Entry <Control-ButtonRelease-1> { 
	after cancel $blt::Hiertable::afterId
	set blt::Hiertable::scroll 0
    }

    $widget bind Entry <Control-Shift-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" && [%W selection present] } {
	    if { [%W index anchor] == "" } {
		%W selection anchor current
	    }
	    if { [%W selection includes anchor] } {
		%W selection set anchor current
	    } else {
		%W selection clear anchor current
		%W selection set current
	    }
	} else {
	    blt::Hiertable::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Control-Shift-Double-ButtonPress-1> {
	puts <Control-Shift-Double-ButtonPress-1> 
	# do nothing
    }
    $widget bind Entry <Control-Shift-B1-Motion> { 
	# do nothing
    }
    $widget column bind all <Enter> {
	%W column highlight [%W column current]
    }
    $widget column bind all <Leave> {
	%W column highlight ""
    }
    $widget column bind Rule <Enter> {
	%W column highlight [%W column current]
	%W column resize activate [%W column current]
    }
    $widget column bind Rule <Leave> {
	%W column highlight ""
	%W column resize activate ""
    }
    $widget column bind Rule <ButtonPress-1> {
	%W column resize anchor %x
    }
    $widget column bind Rule <B1-Motion> {
	%W column resize mark %x
    }
    $widget column bind Rule <ButtonRelease-1> {
	%W column configure [%W column current] -width [%W column resize set]
    }
    $widget column bind all <ButtonRelease-1> {
	set column [%W column nearest %x %y]
	if { $column != "" } {
	    %W column invoke $column
	}
    }
}

# ----------------------------------------------------------------------
# USAGE: blt::Hiertable::AutoScroll <hiertable>
#
# Invoked when the user is selecting elements in a hiertable widget
# and drags the mouse pointer outside of the widget.  Scrolls the
# view in the direction of the pointer.
#
# Arguments:	hiertable		hierarchy widget
#
# ----------------------------------------------------------------------
proc blt::Hiertable::AutoScroll { widget } {
    if { ![winfo exists $widget] } {
	return
    }
    set x $blt::Hiertable::x
    set y $blt::Hiertable::y

    set index [$widget nearest $x $y]

    if {$y >= [winfo height $widget]} {
	$widget yview scroll 1 units
	set neighbor down
    } elseif {$y < 0} {
	$widget yview scroll -1 units
	set neighbor up
    } else {
	set neighbor $index
    }
    if { [$widget cget -selectmode] == "single" } {
	blt::Hiertable::SetSelectionAnchor $widget $neighbor
    } else {
	$widget selection mark $index
    }
    set ::blt::Hiertable::afterId [after 10 blt::Hiertable::AutoScroll $widget]
}

proc blt::Hiertable::SetSelectionAnchor { widget index } {
    set index [$widget index $index]
    # If the anchor hasn't changed, don't do anything
    if { $index != [$widget index anchor] } {
	$widget selection clearall
	$widget see $index
	$widget focus $index
	$widget selection set $index
	$widget selection anchor $index
    }
}

# ----------------------------------------------------------------------
# USAGE: blt::Hiertable::MoveFocus <hiertable> <where>
#
# Invoked by KeyPress bindings.  Moves the active selection to the
# entry <where>, which is an index such as "up", "down", "prevsibling",
# "nextsibling", etc.
# ----------------------------------------------------------------------
proc blt::Hiertable::MoveFocus { widget where } {
    catch {$widget focus $where}
    if { [$widget cget -selectmode] == "single" } {
        $widget selection clearall
        $widget selection set focus
    }
    $widget see focus
}

# ----------------------------------------------------------------------
# USAGE: blt::Hiertable::MovePage <hiertable> <where>
# Arguments:	hiertable		hierarchy widget
#
# Invoked by KeyPress bindings.  Pages the current view up or down.
# The <where> argument should be either "top" or "bottom".
# ----------------------------------------------------------------------

proc blt::Hiertable::MovePage { widget where } {

    # If the focus is already at the top/bottom of the window, we want
    # to scroll a page. It's really one page minus an entry because we
    # want to see the last entry on the next/last page.
    if { [$widget index focus] == [$widget index view.$where] } {
        if {$where == "top"} {
	    $widget yview scroll -1 pages
	    $widget yview scroll 1 units
        } else {
	    $widget yview scroll 1 pages
	    $widget yview scroll -1 units
        }
    }
    update

    # Adjust the entry focus and the view.  Also activate the entry.
    # just in case the mouse point is not in the widget.
    $widget entry highlight view.$where
    $widget focus view.$where
    $widget see view.$where
    if { [$widget cget -selectmode] == "single" } {
        $widget selection clearall
        $widget selection set focus
    }
}

# ----------------------------------------------------------------------
# USAGE: blt::Hiertable::NextMatchingEntry <hiertable> <char>
# Arguments:	hiertable		hierarchy widget
#
# Invoked by KeyPress bindings.  Searches for an entry that starts
# with the letter <char> and makes that entry active.
# ----------------------------------------------------------------------

proc blt::Hiertable::NextMatchingEntry { widget key } {
    if {[string match {[ -~]} $key]} {
	set last [$widget index focus]
	set next [$widget index next]
	while { $next != $last } {
	    set label [$widget entry cget $next -label]
	    if { [string index $label 0] == $key } {
		break
	    }
	    set next [$widget index -at $next next]
	}
	$widget focus $next
        if {[$widget cget -selectmode] == "single"} {
            $widget selection clearall
            $widget selection set focus
        }
	$widget see focus
    }
}

#
#  Edit mode assignments
#
#	ButtonPress-3   Enables/disables edit mode on entry.  Sets focus to 
#			entry.
#
#  KeyPress
#
#	Left		Move insertion position to previous.
#	Right		Move insertion position to next.
#	Up		Move insertion position up one line.
#	Down		Move insertion position down one line.
#	Return		End edit mode.
#	Shift-Return	Line feed.
#	Home		Move to first position.
#	End		Move to last position.
#	ASCII char	Insert character left of insertion point.
#	Del		Delete character right of insertion point.
#	Delete		Delete character left of insertion point.
#	Ctrl-X		Cut
#	Ctrl-V		Copy
#	Ctrl-P		Paste
#	
#  KeyRelease
#
#  ButtonPress-1	Start selection if in entry, otherwise clear selection.
#  B1-Motion		Extend/reduce selection.
#  ButtonRelease-1      End selection if in entry, otherwise use last selection.
#  B1-Enter		Disabled.
#  B1-Leave		Disabled.
#  ButtonPress-2	Same as above.
#  B2-Motion		Same as above.
#  ButtonRelease-2	Same as above.
#	
# All bindings in editting mode will "break" to override other bindings.
#
#

bind xEditor <ButtonPress-3> {
    set node [%W nearest %x %y]
    %W entry insert $node @%x,%y ""
#    %W entry insert $node 2 ""
}

image create photo blt::Hiertable::CloseNormalFolder -format gif -data {
R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
ZTKAsiCtWq0JADs=
}
image create photo blt::Hiertable::OpenNormalFolder -format gif -data {
R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
AAM1WBrM+rAEMigJ8c3Kb3OSII6kGABhp1JnaK1VGwjwKwtvHqNzzd263M3H4n2OH1QBwGw6
nQkAOw==
}
image create photo blt::Hiertable::CloseActiveFolder -format gif -data {
R0lGODlhEAANAMIAAAAAAH9/f/////+/AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
ZTKAsiCtWq0JADs=
}
image create photo blt::Hiertable::OpenActiveFolder -format gif -data {
R0lGODlhEAANAMIAAAAAAH9/f/////+/AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
AAM1WBrM+rAEMigJ8c3Kb3OSII6kGABhp1JnaK1VGwjwKwtvHqNzzd263M3H4n2OH1QBwGw6
nQkAOw==
}


if { $tcl_platform(platform) == "windows" } {
    if { $tk_version >= 8.3 } {
	set cursor "@[file join $blt_library treeview.cur]"
    } else {
	set cursor "size_we"
    }
    option add *Hiertable.ResizeCursor [list $cursor]
} else {
    option add *Hiertable.ResizeCursor \
	"@$blt_library/treeview.xbm $blt_library/treeview_m.xbm black white"
}

# Standard Motif bindings:

bind HiertableEditor <ButtonPress-1> {
    %W text icursor @%x,%y
}

bind HiertableEditor <Left> {
    %W text icursor last
}
bind HiertableEditor <Right> {
    %W text icursor next
}
bind HiertableEditor <Shift-Left> {
    set new [expr {[%W text index insert] - 1}]
    if {![%W text selection present]} {
	%W text selection from insert
	%W text selection to $new
    } else {
	%W text selection adjust $new
    }
    %W text icursor $new
}
bind HiertableEditor <Shift-Right> {
    set new [expr {[%W text index insert] + 1}]
    if {![%W text selection present]} {
	%W text selection from insert
	%W text selection to $new
    } else {
	%W text selection adjust $new
    }
    %W text icursor $new
}

bind HiertableEditor <Home> {
    %W text icursor 0
}
bind HiertableEditor <Shift-Home> {
    set new 0
    if {![%W text selection present]} {
	%W text selection from insert
	%W text selection to $new
    } else {
	%W text selection adjust $new
    }
    %W text icursor $new
}
bind HiertableEditor <End> {
    %W text icursor end
}
bind HiertableEditor <Shift-End> {
    set new end
    if {![%W text selection present]} {
	%W text selection from insert
	%W text selection to $new
    } else {
	%W text selection adjust $new
    }
    %W text icursor $new
}

bind HiertableEditor <Delete> {
    if { [%W text selection present]} {
	%W text delete sel.first sel.last
    } else {
	%W text delete insert
    }
}

bind HiertableEditor <BackSpace> {
    if { [%W text selection present] } {
	%W text delete sel.first sel.last
    } else {
	set index [expr [%W text index insert] - 1]
	if { $index >= 0 } {
	    %W text delete $index $index
	}
    }
}

bind HiertableEditor <Control-space> {
    %W text selection from insert
}

bind HiertableEditor <Select> {
    %W text selection from insert
}

bind HiertableEditor <Control-Shift-space> {
    %W text selection adjust insert
}

bind HiertableEditor <Shift-Select> {
    %W text selection adjust insert
}

bind HiertableEditor <Control-slash> {
    %W text selection range 0 end
}

bind HiertableEditor <Control-backslash> {
    %W text selection clear
}

bind HiertableEditor <KeyPress> {
    blt::Hiertable::Insert %W %A
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for Escape, Return, and Tab.

bind HiertableEditor <Alt-KeyPress> {
    # nothing
}

bind HiertableEditor <Meta-KeyPress> {
    # nothing
}

bind HiertableEditor <Control-KeyPress> {
    # nothing
}

bind HiertableEditor <Escape> { 
    %W text cancel 
    grab release %W
}

bind HiertableEditor <Return> { 
    %W text apply 
    grab release %W
}

bind HiertableEditor <Shift-Return> {
    blt::Hiertable::Insert %W "\n"
}

bind HiertableEditor <KP_Enter> {
    # nothing
}

bind HiertableEditor <Tab> {
    # nothing
}

if {![string compare $tcl_platform(platform) "macintosh"]} {
    bind HiertableEditor <Command-KeyPress> {
	# nothing
    }
}

# On Windows, paste is done using Shift-Insert.  Shift-Insert already
# generates the <<Paste>> event, so we don't need to do anything here.
if { [string compare $tcl_platform(platform) "windows"] != 0 } {
    bind HiertableEditor <Insert> {
	catch {blt::Hiertable::Insert %W [selection get -displayof %W]}
    }
}

# Additional emacs-like bindings:
bind HiertableEditor <ButtonPress-3> {
    if { [winfo viewable %W] } {
	grab release %W
	%W text cancel
    } 
}

bind HiertableEditor <Control-a> {
    %W text icursor 0
    %W text selection clear
}

bind HiertableEditor <Control-b> {
    %W text icursor [expr {[%W index insert] - 1}]
    %W text selection clear
}

bind HiertableEditor <Control-d> {
    %W text delete insert
}

bind HiertableEditor <Control-e> {
    %W text icursor end
    %W text selection clear
}

bind HiertableEditor <Control-f> {
    %W text icursor [expr {[%W index insert] + 1}]
    %W text selection clear
}

bind HiertableEditor <Control-h> {
    if {[%W text selection present]} {
	%W text delete sel.first sel.last
    } else {
	set index [expr [%W text index insert] - 1]
	if { $index >= 0 } {
	    %W text delete $index $index
	}
    }
}

bind HiertableEditor <Control-k> {
    %W text delete insert end
}



# blt::Hiertable::Insert --
# Insert a string into an entry at the point of the insertion cursor.
# If there is a selection in the entry, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The entry window in which to insert the string
# s -		The string to insert (usually just a single character)

proc blt::Hiertable::Insert {w s} {
    if {![string compare $s ""]} {
	return
    }
    catch {
	set insert [$w text index insert]
	if {([$w text index sel.first] <= $insert)
		&& ([$w text index sel.last] >= $insert)} {
	    $w delete sel.first sel.last
	}
    }
    $w text insert insert $s
}

# tkEntryTranspose -
# This procedure implements the "transpose" function for entry widgets.
# It tranposes the characters on either side of the insertion cursor,
# unless the cursor is at the end of the line.  In this case it
# transposes the two characters to the left of the cursor.  In either
# case, the cursor ends up to the right of the transposed characters.
#
# Arguments:
# w -		The entry window.

proc HiertableTranspose w {
    set i [$w text index insert]
    if {$i < [$w text index end]} {
	incr i
    }
    set first [expr {$i-2}]
    if {$first < 0} {
	return
    }
    set new [string index [$w get] [expr {$i-1}]][string index [$w get] $first]
    $w delete $first $i
    $w insert insert $new
}

# Hiertable::GetSelection --
#
# Returns the selected text of the entry with respect to the -show option.
#
# Arguments:
# w -         The entry window from which the text to get

proc blt::Hiertable::GetSelection {w} {
    set entryString [string range [$w get] [$w index sel.first] \
                       [expr [$w index sel.last] - 1]]
    if {[$w cget -show] != ""} {
      regsub -all . $entryString [string index [$w cget -show] 0] entryString
    }
    return $entryString
}

if 0 {
    bind HiertableEditor <Control-t> {
	tkEntryTranspose %W
    }
    bind HiertableEditor <Meta-b> {
	%W text icursor [blt::Hiertable::PreviousWord %W insert]
	%W text selection clear
    }
    bind HiertableEditor <Meta-d> {
	%W delete insert [blt::Hiertable::NextWord %W insert]
    }
    bind HiertableEditor <Meta-f> {
	%W text icursor [blt::Hiertable::NextWord %W insert]
	%W text selection clear
    }
    bind HiertableEditor <Meta-BackSpace> {
	%W delete [blt::Hiertable::PreviousWord %W insert] insert
    }
    bind HiertableEditor <Meta-Delete> {
	%W delete [blt::Hiertable::PreviousWord %W insert] insert
    }
    # tkEntryNextWord -- Returns the index of the next word position
    # after a given position in the entry.  The next word is platform
    # dependent and may be either the next end-of-word position or the
    # next start-of-word position after the next end-of-word position.
    #
    # Arguments:
    # w -		The entry window in which the cursor is to move.
    # start -	Position at which to start search.
    
    if {![string compare $tcl_platform(platform) "windows"]}  {
	proc blt::Hiertable::NextWord {w start} {
	    set pos [tcl_endOfWord [$w get] [$w index $start]]
	    if {$pos >= 0} {
		set pos [tcl_startOfNextWord [$w get] $pos]
	    }
	    if {$pos < 0} {
		return end
	    }
	    return $pos
	}
    } else {
	proc blt::Hiertable::NextWord {w start} {
	    set pos [tcl_endOfWord [$w get] [$w index $start]]
	    if {$pos < 0} {
		return end
	    }
	    return $pos
	}
    }
    
    # blt::Hiertable::PreviousWord --
    #
    # Returns the index of the previous word position before a given
    # position in the entry.
    #
    # Arguments:
    # w -		The entry window in which the cursor is to move.
    # start -	Position at which to start search.
    
    proc blt::Hiertable::PreviousWord {w start} {
	set pos [tcl_startOfPreviousWord [$w get] [$w index $start]]
	if {$pos < 0} {
	    return 0
	}
	return $pos
    }
}
