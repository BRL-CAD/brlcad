#
# hierbox.tcl
# ----------------------------------------------------------------------
# Bindings for the BLT hierbox widget
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

array set bltHierbox {
    afterId ""
    scroll  0
    space   off
    x       0
    y       0
}

catch { 
    namespace eval blt::Hierbox {} 
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

bind Hierbox <ButtonPress-2> {
    set bltHierbox(cursor) [%W cget -cursor]
    %W configure -cursor hand1
    %W scan mark %x %y
}

bind Hierbox <B2-Motion> {
    %W scan dragto %x %y
}

bind Hierbox <ButtonRelease-2> {
    %W configure -cursor $bltHierbox(cursor)
}

bind Hierbox <B1-Leave> {
    if { $bltHierbox(scroll) } {
	blt::Hierbox::AutoScroll %W 
    }
}

bind Hierbox <B1-Enter> {
    after cancel $bltHierbox(afterId)
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


bind Hierbox <KeyPress-Up> {
    blt::Hierbox::MoveFocus %W up
    if { $bltHierbox(space) } {
	%W selection toggle focus
    }
}

bind Hierbox <KeyPress-Down> {
    blt::Hierbox::MoveFocus %W down
    if { $bltHierbox(space) } {
	%W selection toggle focus
    }
}

bind Hierbox <Shift-KeyPress-Up> {
    blt::Hierbox::MoveFocus %W prevsibling
}

bind Hierbox <Shift-KeyPress-Down> {
    blt::Hierbox::MoveFocus %W nextsibling
}

bind Hierbox <KeyPress-Prior> {
    blt::Hierbox::MovePage %W top
}

bind Hierbox <KeyPress-Next> {
    blt::Hierbox::MovePage %W bottom
}

bind Hierbox <KeyPress-Left> {
    %W close focus
}
bind Hierbox <KeyPress-Right> {
    %W open focus
    %W see focus -anchor w
}

bind Hierbox <KeyPress-space> {
    blt::HierboxToggle %W focus
    set bltHierbox(space) on
}

bind Hierbox <KeyRelease-space> { 
    set bltHierbox(space) off
}

bind Hierbox <KeyPress-Return> {
    blt::HierboxToggle %W focus
    set bltHierbox(space) on
}

bind Hierbox <KeyRelease-Return> { 
    set bltHierbox(space) off
}

bind Hierbox <KeyPress> {
    blt::Hierbox::NextMatchingEntry %W %A
}

bind Hierbox <KeyPress-Home> {
    blt::Hierbox::MoveFocus %W root
}

bind Hierbox <KeyPress-End> {
    blt::Hierbox::MoveFocus %W end
}

bind Hierbox <KeyPress-F1> {
    %W open -r root
}

bind Hierbox <KeyPress-F2> {
    eval %W close -r [%W entry children root 0 end] 
}

# ----------------------------------------------------------------------
# USAGE: blt::HierboxToggle <hierbox> <index>
# Arguments:	hierbox		hierarchy widget
#
# Invoked when the user presses the space bar.  Toggles the selection
# for the entry at <index>.
# ----------------------------------------------------------------------
proc blt::HierboxToggle { widget index } {
    switch -- [$widget cget -selectmode] {
        single {
            if { [$widget selection includes $index] } {
                $widget selection clearall
            } else {
		$widget selection set $index
	    }
        }
        multiple {
            $widget selection toggle $index
        }
    }
}


# ----------------------------------------------------------------------
# USAGE: blt::Hierbox::MovePage <hierbox> <where>
# Arguments:	hierbox		hierarchy widget
#
# Invoked by KeyPress bindings.  Pages the current view up or down.
# The <where> argument should be either "top" or "bottom".
# ----------------------------------------------------------------------

proc blt::Hierbox::MovePage { widget where } {
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

bind Hierbox <ButtonPress-3> {
    set node [%W nearest %x %y]
    %W entry insert $node @%x,%y ""
#    %W entry insert $node 2 ""
}


proc blt::Hierbox::Init { widget } {
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
	blt::Hierbox::SetSelectionAnchor %W current
	set bltHierbox(scroll) 1
    }

    $widget bin Entry <Double-ButtonPress-1> {
	%W toggle current
    }

    #
    # B1-Motion
    #
    #	For "multiple" mode only.  Saves the current location of the
    #	pointer for auto-scrolling.
    #
    $widget bind Entry <B1-Motion> { 
	set bltHierbox(x) %x
	set bltHierbox(y) %y
	set index [%W nearest %x %y]
	if { [%W cget -selectmode] == "multiple" } {
	    %W selection mark $index
	} else {
	    blt::Hierbox::SetSelectionAnchor %W $index
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
	after cancel $bltHierbox(afterId)
	set bltHierbox(scroll) 0
    }

    #
    # Shift-ButtonPress-1
    #
    #	For "multiple" mode only.
    #
    $widget bind Entry <Shift-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" && [%W selection present] }  {
	    if { [%W index anchor] == "" } {
		%W selection anchor current
	    }
	    set index [%W index anchor]
	    %W selection clearall
	    %W selection set $index current
	} else {
	    blt::Hierbox::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Shift-B1-Motion> { 
	# do nothing
    }
    $widget bind Entry <Shift-ButtonRelease-1> { 
	after cancel $bltHierbox(afterId)
	set bltHierbox(scroll) 0
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
	    blt::Hierbox::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Control-B1-Motion> { 
	# do nothing
    }
    $widget bind Entry <Control-ButtonRelease-1> { 
	after cancel $bltHierbox(afterId)
	set bltHierbox(scroll) 0
    }
    #
    # Control-Shift-ButtonPress-1
    #
    #	For "multiple" mode only.  
    #
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
	    blt::Hierbox::SetSelectionAnchor %W current
	}
    }
    $widget bind Entry <Control-Shift-B1-Motion> { 
	# do nothing
    }
}


# ----------------------------------------------------------------------
# USAGE: blt::Hierbox::AutoScroll <hierbox>
#
# Invoked when the user is selecting elements in a hierbox widget
# and drags the mouse pointer outside of the widget.  Scrolls the
# view in the direction of the pointer.
#
# Arguments:	hierbox		hierarchy widget
#
# ----------------------------------------------------------------------
proc blt::Hierbox::AutoScroll { widget } {
    global bltHierbox
    if { ![winfo exists $widget] } {
	return
    }
    set x $bltHierbox(x)
    set y $bltHierbox(y)
    set index [$widget nearest $x $y]
    if { $y >= [winfo height $widget] } {
	$widget yview scroll 1 units
	set neighbor down
    } elseif { $y < 0 } {
	$widget yview scroll -1 units
	set neighbor up
    } else {
	set neighbor $index
    }
    if { [$widget cget -selectmode] == "single" } {
	blt::Hierbox::SetSelectionAnchor $widget $neighbor
    } else {
	$widget selection mark $index
    }
    set bltHierbox(afterId) [after 10 blt::Hierbox::AutoScroll $widget]
}

proc blt::Hierbox::SetSelectionAnchor { widget index } {
    set index [$widget index $index]
    $widget selection clearall
    $widget see $index
    $widget focus $index
    $widget selection set $index
    $widget selection anchor $index
}


# ----------------------------------------------------------------------
# USAGE: blt::Hierbox::NextMatchingEntry <hierbox> <char>
# Arguments:	hierbox		hierarchy widget
#
# Invoked by KeyPress bindings.  Searches for an entry that starts
# with the letter <char> and makes that entry active.
# ----------------------------------------------------------------------

proc blt::Hierbox::NextMatchingEntry { widget key } {
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

# ----------------------------------------------------------------------
# USAGE: blt::Hierbox::MoveFocus <hierbox> <where>
#
# Invoked by KeyPress bindings.  Moves the active selection to the
# entry <where>, which is an index such as "up", "down", "prevsibling",
# "nextsibling", etc.
# ----------------------------------------------------------------------
proc blt::Hierbox::MoveFocus { widget where } {
    catch {$widget focus $where}
    if { [$widget cget -selectmode] == "single" } {
        $widget selection clearall
        $widget selection set focus
    }
    $widget see focus
}
