# listbox.tcl --
#
# This file defines the default bindings for Tk listbox widgets
# and provides procedures that help in implementing those bindings.
#
# RCS: @(#) $Id$
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
# Copyright (c) 1998 by Scriptics Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#--------------------------------------------------------------------------
# tkPriv elements used in this file:
#
# afterId -		Token returned by "after" for autoscanning.
# listboxPrev -		The last element to be selected or deselected
#			during a selection operation.
# listboxSelection -	All of the items that were selected before the
#			current selection operation (such as a mouse
#			drag) started;  used to cancel an operation.
#--------------------------------------------------------------------------

#-------------------------------------------------------------------------
# The code below creates the default class bindings for listboxes.
#-------------------------------------------------------------------------

# Note: the check for existence of %W below is because this binding
# is sometimes invoked after a window has been deleted (e.g. because
# there is a double-click binding on the widget that deletes it).  Users
# can put "break"s in their bindings to avoid the error, but this check
# makes that unnecessary.

bind Listbox <1> {
    if {[winfo exists %W]} {
	tkListboxBeginSelect %W [%W index @%x,%y]
    }
}

# Ignore double clicks so that users can define their own behaviors.
# Among other things, this prevents errors if the user deletes the
# listbox on a double click.

bind Listbox <Double-1> {
    # Empty script
}

bind Listbox <B1-Motion> {
    set tkPriv(x) %x
    set tkPriv(y) %y
    tkListboxMotion %W [%W index @%x,%y]
}
bind Listbox <ButtonRelease-1> {
    tkCancelRepeat
    %W activate @%x,%y
}
bind Listbox <Shift-1> {
    tkListboxBeginExtend %W [%W index @%x,%y]
}
bind Listbox <Control-1> {
    tkListboxBeginToggle %W [%W index @%x,%y]
}
bind Listbox <B1-Leave> {
    set tkPriv(x) %x
    set tkPriv(y) %y
    tkListboxAutoScan %W
}
bind Listbox <B1-Enter> {
    tkCancelRepeat
}

bind Listbox <Up> {
    tkListboxUpDown %W -1
}
bind Listbox <Shift-Up> {
    tkListboxExtendUpDown %W -1
}
bind Listbox <Down> {
    tkListboxUpDown %W 1
}
bind Listbox <Shift-Down> {
    tkListboxExtendUpDown %W 1
}
bind Listbox <Left> {
    %W xview scroll -1 units
}
bind Listbox <Control-Left> {
    %W xview scroll -1 pages
}
bind Listbox <Right> {
    %W xview scroll 1 units
}
bind Listbox <Control-Right> {
    %W xview scroll 1 pages
}
bind Listbox <Prior> {
    %W yview scroll -1 pages
    %W activate @0,0
}
bind Listbox <Next> {
    %W yview scroll 1 pages
    %W activate @0,0
}
bind Listbox <Control-Prior> {
    %W xview scroll -1 pages
}
bind Listbox <Control-Next> {
    %W xview scroll 1 pages
}
bind Listbox <Home> {
    %W xview moveto 0
}
bind Listbox <End> {
    %W xview moveto 1
}
bind Listbox <Control-Home> {
    %W activate 0
    %W see 0
    %W selection clear 0 end
    %W selection set 0
    event generate %W <<ListboxSelect>>
}
bind Listbox <Shift-Control-Home> {
    tkListboxDataExtend %W 0
}
bind Listbox <Control-End> {
    %W activate end
    %W see end
    %W selection clear 0 end
    %W selection set end
    event generate %W <<ListboxSelect>>
}
bind Listbox <Shift-Control-End> {
    tkListboxDataExtend %W [%W index end]
}
bind Listbox <<Copy>> {
    if {[string equal [selection own -displayof %W] "%W"]} {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get -displayof %W]
    }
}
bind Listbox <space> {
    tkListboxBeginSelect %W [%W index active]
}
bind Listbox <Select> {
    tkListboxBeginSelect %W [%W index active]
}
bind Listbox <Control-Shift-space> {
    tkListboxBeginExtend %W [%W index active]
}
bind Listbox <Shift-Select> {
    tkListboxBeginExtend %W [%W index active]
}
bind Listbox <Escape> {
    tkListboxCancel %W
}
bind Listbox <Control-slash> {
    tkListboxSelectAll %W
}
bind Listbox <Control-backslash> {
    if {[string compare [%W cget -selectmode] "browse"]} {
	%W selection clear 0 end
	event generate %W <<ListboxSelect>>
    }
}

# Additional Tk bindings that aren't part of the Motif look and feel:

bind Listbox <2> {
    %W scan mark %x %y
}
bind Listbox <B2-Motion> {
    %W scan dragto %x %y
}

# The MouseWheel will typically only fire on Windows.  However,
# someone could use the "event generate" command to produce one
# on other platforms.

bind Listbox <MouseWheel> {
    %W yview scroll [expr {- (%D / 120) * 4}] units
}

if {[string equal "unix" $tcl_platform(platform)]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind Listbox <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -5 units
	}
    }
    bind Listbox <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 5 units
	}
    }
}

# tkListboxBeginSelect --
#
# This procedure is typically invoked on button-1 presses.  It begins
# the process of making a selection in the listbox.  Its exact behavior
# depends on the selection mode currently in effect for the listbox;
# see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc tkListboxBeginSelect {w el} {
    global tkPriv
    if {[string equal [$w cget -selectmode] "multiple"]} {
	if {[$w selection includes $el]} {
	    $w selection clear $el
	} else {
	    $w selection set $el
	}
    } else {
	$w selection clear 0 end
	$w selection set $el
	$w selection anchor $el
	set tkPriv(listboxSelection) {}
	set tkPriv(listboxPrev) $el
    }
    event generate $w <<ListboxSelect>>
}

# tkListboxMotion --
#
# This procedure is called to process mouse motion events while
# button 1 is down.  It may move or extend the selection, depending
# on the listbox's selection mode.
#
# Arguments:
# w -		The listbox widget.
# el -		The element under the pointer (must be a number).

proc tkListboxMotion {w el} {
    global tkPriv
    if {$el == $tkPriv(listboxPrev)} {
	return
    }
    set anchor [$w index anchor]
    switch [$w cget -selectmode] {
	browse {
	    $w selection clear 0 end
	    $w selection set $el
	    set tkPriv(listboxPrev) $el
	    event generate $w <<ListboxSelect>>
	}
	extended {
	    set i $tkPriv(listboxPrev)
	    if {[string equal {} $i]} {
		set i $el
		$w selection set $el
	    }
	    if {[$w selection includes anchor]} {
		$w selection clear $i $el
		$w selection set anchor $el
	    } else {
		$w selection clear $i $el
		$w selection clear anchor $el
	    }
	    if {![info exists tkPriv(listboxSelection)]} {
		set tkPriv(listboxSelection) [$w curselection]
	    }
	    while {($i < $el) && ($i < $anchor)} {
		if {[lsearch $tkPriv(listboxSelection) $i] >= 0} {
		    $w selection set $i
		}
		incr i
	    }
	    while {($i > $el) && ($i > $anchor)} {
		if {[lsearch $tkPriv(listboxSelection) $i] >= 0} {
		    $w selection set $i
		}
		incr i -1
	    }
	    set tkPriv(listboxPrev) $el
	    event generate $w <<ListboxSelect>>
	}
    }
}

# tkListboxBeginExtend --
#
# This procedure is typically invoked on shift-button-1 presses.  It
# begins the process of extending a selection in the listbox.  Its
# exact behavior depends on the selection mode currently in effect
# for the listbox;  see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc tkListboxBeginExtend {w el} {
    if {[string equal [$w cget -selectmode] "extended"]} {
	if {[$w selection includes anchor]} {
	    tkListboxMotion $w $el
	} else {
	    # No selection yet; simulate the begin-select operation.
	    tkListboxBeginSelect $w $el
	}
    }
}

# tkListboxBeginToggle --
#
# This procedure is typically invoked on control-button-1 presses.  It
# begins the process of toggling a selection in the listbox.  Its
# exact behavior depends on the selection mode currently in effect
# for the listbox;  see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc tkListboxBeginToggle {w el} {
    global tkPriv
    if {[string equal [$w cget -selectmode] "extended"]} {
	set tkPriv(listboxSelection) [$w curselection]
	set tkPriv(listboxPrev) $el
	$w selection anchor $el
	if {[$w selection includes $el]} {
	    $w selection clear $el
	} else {
	    $w selection set $el
	}
	event generate $w <<ListboxSelect>>
    }
}

# tkListboxAutoScan --
# This procedure is invoked when the mouse leaves an entry window
# with button 1 down.  It scrolls the window up, down, left, or
# right, depending on where the mouse left the window, and reschedules
# itself as an "after" command so that the window continues to scroll until
# the mouse moves back into the window or the mouse button is released.
#
# Arguments:
# w -		The entry window.

proc tkListboxAutoScan {w} {
    global tkPriv
    if {![winfo exists $w]} return
    set x $tkPriv(x)
    set y $tkPriv(y)
    if {$y >= [winfo height $w]} {
	$w yview scroll 1 units
    } elseif {$y < 0} {
	$w yview scroll -1 units
    } elseif {$x >= [winfo width $w]} {
	$w xview scroll 2 units
    } elseif {$x < 0} {
	$w xview scroll -2 units
    } else {
	return
    }
    tkListboxMotion $w [$w index @$x,$y]
    set tkPriv(afterId) [after 50 [list tkListboxAutoScan $w]]
}

# tkListboxUpDown --
#
# Moves the location cursor (active element) up or down by one element,
# and changes the selection if we're in browse or extended selection
# mode.
#
# Arguments:
# w -		The listbox widget.
# amount -	+1 to move down one item, -1 to move back one item.

proc tkListboxUpDown {w amount} {
    global tkPriv
    $w activate [expr {[$w index active] + $amount}]
    $w see active
    switch [$w cget -selectmode] {
	browse {
	    $w selection clear 0 end
	    $w selection set active
	    event generate $w <<ListboxSelect>>
	}
	extended {
	    $w selection clear 0 end
	    $w selection set active
	    $w selection anchor active
	    set tkPriv(listboxPrev) [$w index active]
	    set tkPriv(listboxSelection) {}
	    event generate $w <<ListboxSelect>>
	}
    }
}

# tkListboxExtendUpDown --
#
# Does nothing unless we're in extended selection mode;  in this
# case it moves the location cursor (active element) up or down by
# one element, and extends the selection to that point.
#
# Arguments:
# w -		The listbox widget.
# amount -	+1 to move down one item, -1 to move back one item.

proc tkListboxExtendUpDown {w amount} {
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set active [$w index active]
    if {![info exists tkPriv(listboxSelection)]} {
	global tkPriv
	$w selection set $active
	set tkPriv(listboxSelection) [$w curselection]
    }
    $w activate [expr {$active + $amount}]
    $w see active
    tkListboxMotion $w [$w index active]
}

# tkListboxDataExtend
#
# This procedure is called for key-presses such as Shift-KEndData.
# If the selection mode isn't multiple or extend then it does nothing.
# Otherwise it moves the active element to el and, if we're in
# extended mode, extends the selection to that point.
#
# Arguments:
# w -		The listbox widget.
# el -		An integer element number.

proc tkListboxDataExtend {w el} {
    set mode [$w cget -selectmode]
    if {[string equal $mode "extended"]} {
	$w activate $el
	$w see $el
        if {[$w selection includes anchor]} {
	    tkListboxMotion $w $el
	}
    } elseif {[string equal $mode "multiple"]} {
	$w activate $el
	$w see $el
    }
}

# tkListboxCancel
#
# This procedure is invoked to cancel an extended selection in
# progress.  If there is an extended selection in progress, it
# restores all of the items between the active one and the anchor
# to their previous selection state.
#
# Arguments:
# w -		The listbox widget.

proc tkListboxCancel w {
    global tkPriv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set first [$w index anchor]
    set last $tkPriv(listboxPrev)
    if { [string equal $last ""] } {
	# Not actually doing any selection right now
	return
    }
    if {$first > $last} {
	set tmp $first
	set first $last
	set last $tmp
    }
    $w selection clear $first $last
    while {$first <= $last} {
	if {[lsearch $tkPriv(listboxSelection) $first] >= 0} {
	    $w selection set $first
	}
	incr first
    }
    event generate $w <<ListboxSelect>>
}

# tkListboxSelectAll
#
# This procedure is invoked to handle the "select all" operation.
# For single and browse mode, it just selects the active element.
# Otherwise it selects everything in the widget.
#
# Arguments:
# w -		The listbox widget.

proc tkListboxSelectAll w {
    set mode [$w cget -selectmode]
    if {[string equal $mode "single"] || [string equal $mode "browse"]} {
	$w selection clear 0 end
	$w selection set active
    } else {
	$w selection set 0 end
    }
    event generate $w <<ListboxSelect>>
}
