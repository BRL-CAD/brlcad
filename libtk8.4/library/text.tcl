# text.tcl --
#
# This file defines the default bindings for Tk text widgets and provides
# procedures that help in implementing the bindings.
#
# RCS: @(#) $Id$
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
# Copyright (c) 1998 by Scriptics Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# Elements of ::tk::Priv that are used in this file:
#
# afterId -		If non-null, it means that auto-scanning is underway
#			and it gives the "after" id for the next auto-scan
#			command to be executed.
# char -		Character position on the line;  kept in order
#			to allow moving up or down past short lines while
#			still remembering the desired position.
# mouseMoved -		Non-zero means the mouse has moved a significant
#			amount since the button went down (so, for example,
#			start dragging out a selection).
# prevPos -		Used when moving up or down lines via the keyboard.
#			Keeps track of the previous insert position, so
#			we can distinguish a series of ups and downs, all
#			in a row, from a new up or down.
# selectMode -		The style of selection currently underway:
#			char, word, or line.
# x, y -		Last known mouse coordinates for scanning
#			and auto-scanning.
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# The code below creates the default class bindings for text widgets.
#-------------------------------------------------------------------------

# Standard Motif bindings:

bind Text <1> {
    tk::TextButton1 %W %x %y
    %W tag remove sel 0.0 end
}
bind Text <B1-Motion> {
    set tk::Priv(x) %x
    set tk::Priv(y) %y
    tk::TextSelectTo %W %x %y
}
bind Text <Double-1> {
    set tk::Priv(selectMode) word
    tk::TextSelectTo %W %x %y
    catch {%W mark set insert sel.last}
}
bind Text <Triple-1> {
    set tk::Priv(selectMode) line
    tk::TextSelectTo %W %x %y
    catch {%W mark set insert sel.last}
}
bind Text <Shift-1> {
    tk::TextResetAnchor %W @%x,%y
    set tk::Priv(selectMode) char
    tk::TextSelectTo %W %x %y
}
bind Text <Double-Shift-1>	{
    set tk::Priv(selectMode) word
    tk::TextSelectTo %W %x %y 1
}
bind Text <Triple-Shift-1>	{
    set tk::Priv(selectMode) line
    tk::TextSelectTo %W %x %y
}
bind Text <B1-Leave> {
    set tk::Priv(x) %x
    set tk::Priv(y) %y
    tk::TextAutoScan %W
}
bind Text <B1-Enter> {
    tk::CancelRepeat
}
bind Text <ButtonRelease-1> {
    tk::CancelRepeat
}
bind Text <Control-1> {
    %W mark set insert @%x,%y
}
bind Text <Left> {
    tk::TextSetCursor %W insert-1c
}
bind Text <Right> {
    tk::TextSetCursor %W insert+1c
}
bind Text <Up> {
    tk::TextSetCursor %W [tk::TextUpDownLine %W -1]
}
bind Text <Down> {
    tk::TextSetCursor %W [tk::TextUpDownLine %W 1]
}
bind Text <Shift-Left> {
    tk::TextKeySelect %W [%W index {insert - 1c}]
}
bind Text <Shift-Right> {
    tk::TextKeySelect %W [%W index {insert + 1c}]
}
bind Text <Shift-Up> {
    tk::TextKeySelect %W [tk::TextUpDownLine %W -1]
}
bind Text <Shift-Down> {
    tk::TextKeySelect %W [tk::TextUpDownLine %W 1]
}
bind Text <Control-Left> {
    tk::TextSetCursor %W [tk::TextPrevPos %W insert tcl_startOfPreviousWord]
}
bind Text <Control-Right> {
    tk::TextSetCursor %W [tk::TextNextWord %W insert]
}
bind Text <Control-Up> {
    tk::TextSetCursor %W [tk::TextPrevPara %W insert]
}
bind Text <Control-Down> {
    tk::TextSetCursor %W [tk::TextNextPara %W insert]
}
bind Text <Shift-Control-Left> {
    tk::TextKeySelect %W [tk::TextPrevPos %W insert tcl_startOfPreviousWord]
}
bind Text <Shift-Control-Right> {
    tk::TextKeySelect %W [tk::TextNextWord %W insert]
}
bind Text <Shift-Control-Up> {
    tk::TextKeySelect %W [tk::TextPrevPara %W insert]
}
bind Text <Shift-Control-Down> {
    tk::TextKeySelect %W [tk::TextNextPara %W insert]
}
bind Text <Prior> {
    tk::TextSetCursor %W [tk::TextScrollPages %W -1]
}
bind Text <Shift-Prior> {
    tk::TextKeySelect %W [tk::TextScrollPages %W -1]
}
bind Text <Next> {
    tk::TextSetCursor %W [tk::TextScrollPages %W 1]
}
bind Text <Shift-Next> {
    tk::TextKeySelect %W [tk::TextScrollPages %W 1]
}
bind Text <Control-Prior> {
    %W xview scroll -1 page
}
bind Text <Control-Next> {
    %W xview scroll 1 page
}

bind Text <Home> {
    tk::TextSetCursor %W {insert linestart}
}
bind Text <Shift-Home> {
    tk::TextKeySelect %W {insert linestart}
}
bind Text <End> {
    tk::TextSetCursor %W {insert lineend}
}
bind Text <Shift-End> {
    tk::TextKeySelect %W {insert lineend}
}
bind Text <Control-Home> {
    tk::TextSetCursor %W 1.0
}
bind Text <Control-Shift-Home> {
    tk::TextKeySelect %W 1.0
}
bind Text <Control-End> {
    tk::TextSetCursor %W {end - 1 char}
}
bind Text <Control-Shift-End> {
    tk::TextKeySelect %W {end - 1 char}
}

bind Text <Tab> {
    if { [string equal [%W cget -state] "normal"] } {
	tk::TextInsert %W \t
	focus %W
	break
    }
}
bind Text <Shift-Tab> {
    # Needed only to keep <Tab> binding from triggering;  doesn't
    # have to actually do anything.
    break
}
bind Text <Control-Tab> {
    focus [tk_focusNext %W]
}
bind Text <Control-Shift-Tab> {
    focus [tk_focusPrev %W]
}
bind Text <Control-i> {
    tk::TextInsert %W \t
}
bind Text <Return> {
    tk::TextInsert %W \n
    if {[%W cget -autoseparators]} {%W edit separator}
}
bind Text <Delete> {
    if {[string compare [%W tag nextrange sel 1.0 end] ""]} {
	%W delete sel.first sel.last
    } else {
	%W delete insert
	%W see insert
    }
}
bind Text <BackSpace> {
    if {[string compare [%W tag nextrange sel 1.0 end] ""]} {
	%W delete sel.first sel.last
    } elseif {[%W compare insert != 1.0]} {
	%W delete insert-1c
	%W see insert
    }
}

bind Text <Control-space> {
    %W mark set anchor insert
}
bind Text <Select> {
    %W mark set anchor insert
}
bind Text <Control-Shift-space> {
    set tk::Priv(selectMode) char
    tk::TextKeyExtend %W insert
}
bind Text <Shift-Select> {
    set tk::Priv(selectMode) char
    tk::TextKeyExtend %W insert
}
bind Text <Control-slash> {
    %W tag add sel 1.0 end
}
bind Text <Control-backslash> {
    %W tag remove sel 1.0 end
}
bind Text <<Cut>> {
    tk_textCut %W
}
bind Text <<Copy>> {
    tk_textCopy %W
}
bind Text <<Paste>> {
    tk_textPaste %W
}
bind Text <<Clear>> {
    catch {%W delete sel.first sel.last}
}
bind Text <<PasteSelection>> {
    if {$tk_strictMotif || ![info exists tk::Priv(mouseMoved)]
	|| !$tk::Priv(mouseMoved)} {
	tk::TextPasteSelection %W %x %y
    }
}
bind Text <Insert> {
    catch {tk::TextInsert %W [::tk::GetSelection %W PRIMARY]}
}
bind Text <KeyPress> {
    tk::TextInsert %W %A
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for <Escape>.

bind Text <Alt-KeyPress> {# nothing }
bind Text <Meta-KeyPress> {# nothing}
bind Text <Control-KeyPress> {# nothing}
bind Text <Escape> {# nothing}
bind Text <KP_Enter> {# nothing}
if {[string equal [tk windowingsystem] "classic"]
	|| [string equal [tk windowingsystem] "aqua"]} {
    bind Text <Command-KeyPress> {# nothing}
}

# Additional emacs-like bindings:

bind Text <Control-a> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W {insert linestart}
    }
}
bind Text <Control-b> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W insert-1c
    }
}
bind Text <Control-d> {
    if {!$tk_strictMotif} {
	%W delete insert
    }
}
bind Text <Control-e> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W {insert lineend}
    }
}
bind Text <Control-f> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W insert+1c
    }
}
bind Text <Control-k> {
    if {!$tk_strictMotif} {
	if {[%W compare insert == {insert lineend}]} {
	    %W delete insert
	} else {
	    %W delete insert {insert lineend}
	}
    }
}
bind Text <Control-n> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextUpDownLine %W 1]
    }
}
bind Text <Control-o> {
    if {!$tk_strictMotif} {
	%W insert insert \n
	%W mark set insert insert-1c
    }
}
bind Text <Control-p> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextUpDownLine %W -1]
    }
}
bind Text <Control-t> {
    if {!$tk_strictMotif} {
	tk::TextTranspose %W
    }
}

bind Text <<Undo>> {
    catch { %W edit undo }
}

bind Text <<Redo>> {
    catch { %W edit redo }
}

if {[string compare $tcl_platform(platform) "windows"]} {
bind Text <Control-v> {
    if {!$tk_strictMotif} {
	tk::TextScrollPages %W 1
    }
}
}

bind Text <Meta-b> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextPrevPos %W insert tcl_startOfPreviousWord]
    }
}
bind Text <Meta-d> {
    if {!$tk_strictMotif} {
	%W delete insert [tk::TextNextWord %W insert]
    }
}
bind Text <Meta-f> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextNextWord %W insert]
    }
}
bind Text <Meta-less> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W 1.0
    }
}
bind Text <Meta-greater> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W end-1c
    }
}
bind Text <Meta-BackSpace> {
    if {!$tk_strictMotif} {
	%W delete [tk::TextPrevPos %W insert tcl_startOfPreviousWord] insert
    }
}
bind Text <Meta-Delete> {
    if {!$tk_strictMotif} {
	%W delete [tk::TextPrevPos %W insert tcl_startOfPreviousWord] insert
    }
}

# Macintosh only bindings:

# if text black & highlight black -> text white, other text the same
if {[string equal [tk windowingsystem] "classic"]
	|| [string equal [tk windowingsystem] "aqua"]} {
bind Text <FocusIn> {
    %W tag configure sel -borderwidth 0
    %W configure -selectbackground systemHighlight -selectforeground systemHighlightText
}
bind Text <FocusOut> {
    %W tag configure sel -borderwidth 1
    %W configure -selectbackground white -selectforeground black
}
bind Text <Option-Left> {
    tk::TextSetCursor %W [tk::TextPrevPos %W insert tcl_startOfPreviousWord]
}
bind Text <Option-Right> {
    tk::TextSetCursor %W [tk::TextNextWord %W insert]
}
bind Text <Option-Up> {
    tk::TextSetCursor %W [tk::TextPrevPara %W insert]
}
bind Text <Option-Down> {
    tk::TextSetCursor %W [tk::TextNextPara %W insert]
}
bind Text <Shift-Option-Left> {
    tk::TextKeySelect %W [tk::TextPrevPos %W insert tcl_startOfPreviousWord]
}
bind Text <Shift-Option-Right> {
    tk::TextKeySelect %W [tk::TextNextWord %W insert]
}
bind Text <Shift-Option-Up> {
    tk::TextKeySelect %W [tk::TextPrevPara %W insert]
}
bind Text <Shift-Option-Down> {
    tk::TextKeySelect %W [tk::TextNextPara %W insert]
}

# End of Mac only bindings
}

# A few additional bindings of my own.

bind Text <Control-h> {
    if {!$tk_strictMotif} {
	if {[%W compare insert != 1.0]} {
	    %W delete insert-1c
	    %W see insert
	}
    }
}
bind Text <2> {
    if {!$tk_strictMotif} {
	tk::TextScanMark %W %x %y
    }
}
bind Text <B2-Motion> {
    if {!$tk_strictMotif} {
	tk::TextScanDrag %W %x %y
    }
}
set ::tk::Priv(prevPos) {}

# The MouseWheel will typically only fire on Windows.  However,
# someone could use the "event generate" command to produce one
# on other platforms.

bind Text <MouseWheel> {
    %W yview scroll [expr {- (%D / 120) * 4}] units
}

if {[string equal "x11" [tk windowingsystem]]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind Text <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -5 units
	}
    }
    bind Text <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 5 units
	}
    }
}

# ::tk::TextClosestGap --
# Given x and y coordinates, this procedure finds the closest boundary
# between characters to the given coordinates and returns the index
# of the character just after the boundary.
#
# Arguments:
# w -		The text window.
# x -		X-coordinate within the window.
# y -		Y-coordinate within the window.

proc ::tk::TextClosestGap {w x y} {
    set pos [$w index @$x,$y]
    set bbox [$w bbox $pos]
    if {[string equal $bbox ""]} {
	return $pos
    }
    if {($x - [lindex $bbox 0]) < ([lindex $bbox 2]/2)} {
	return $pos
    }
    $w index "$pos + 1 char"
}

# ::tk::TextButton1 --
# This procedure is invoked to handle button-1 presses in text
# widgets.  It moves the insertion cursor, sets the selection anchor,
# and claims the input focus.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		The x-coordinate of the button press.
# y -		The x-coordinate of the button press.

proc ::tk::TextButton1 {w x y} {
    variable ::tk::Priv

    set Priv(selectMode) char
    set Priv(mouseMoved) 0
    set Priv(pressX) $x
    $w mark set insert [TextClosestGap $w $x $y]
    $w mark set anchor insert
    # Allow focus in any case on Windows, because that will let the
    # selection be displayed even for state disabled text widgets.
    if {[string equal $::tcl_platform(platform) "windows"] \
	    || [string equal [$w cget -state] "normal"]} {focus $w}
    if {[$w cget -autoseparators]} {$w edit separator}
}

# ::tk::TextSelectTo --
# This procedure is invoked to extend the selection, typically when
# dragging it with the mouse.  Depending on the selection mode (character,
# word, line) it selects in different-sized units.  This procedure
# ignores mouse motions initially until the mouse has moved from
# one character to another or until there have been multiple clicks.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		Mouse x position.
# y - 		Mouse y position.

proc ::tk::TextSelectTo {w x y {extend 0}} {
    global tcl_platform
    variable ::tk::Priv

    set cur [TextClosestGap $w $x $y]
    if {[catch {$w index anchor}]} {
	$w mark set anchor $cur
    }
    set anchor [$w index anchor]
    if {[$w compare $cur != $anchor] || (abs($Priv(pressX) - $x) >= 3)} {
	set Priv(mouseMoved) 1
    }
    switch $Priv(selectMode) {
	char {
	    if {[$w compare $cur < anchor]} {
		set first $cur
		set last anchor
	    } else {
		set first anchor
		set last $cur
	    }
	}
	word {
	    if {[$w compare $cur < anchor]} {
		set first [TextPrevPos $w "$cur + 1c" tcl_wordBreakBefore]
		if { !$extend } {
		    set last [TextNextPos $w "anchor" tcl_wordBreakAfter]
		} else {
		    set last anchor
		}
	    } else {
		set last [TextNextPos $w "$cur - 1c" tcl_wordBreakAfter]
		if { !$extend } {
		    set first [TextPrevPos $w anchor tcl_wordBreakBefore]
		} else {
		    set first anchor
		}
	    }
	}
	line {
	    if {[$w compare $cur < anchor]} {
		set first [$w index "$cur linestart"]
		set last [$w index "anchor - 1c lineend + 1c"]
	    } else {
		set first [$w index "anchor linestart"]
		set last [$w index "$cur lineend + 1c"]
	    }
	}
    }
    if {$Priv(mouseMoved) || [string compare $Priv(selectMode) "char"]} {
	$w tag remove sel 0.0 end
	$w mark set insert $cur
	$w tag add sel $first $last
	$w tag remove sel $last end
	update idletasks
    }
}

# ::tk::TextKeyExtend --
# This procedure handles extending the selection from the keyboard,
# where the point to extend to is really the boundary between two
# characters rather than a particular character.
#
# Arguments:
# w -		The text window.
# index -	The point to which the selection is to be extended.

proc ::tk::TextKeyExtend {w index} {

    set cur [$w index $index]
    if {[catch {$w index anchor}]} {
	$w mark set anchor $cur
    }
    set anchor [$w index anchor]
    if {[$w compare $cur < anchor]} {
	set first $cur
	set last anchor
    } else {
	set first anchor
	set last $cur
    }
    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

# ::tk::TextPasteSelection --
# This procedure sets the insertion cursor to the mouse position,
# inserts the selection, and sets the focus to the window.
#
# Arguments:
# w -		The text window.
# x, y - 	Position of the mouse.

proc ::tk::TextPasteSelection {w x y} {
    $w mark set insert [TextClosestGap $w $x $y]
    if {![catch {::tk::GetSelection $w PRIMARY} sel]} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	$w insert insert $sel
	if {$oldSeparator} {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
    if {[string equal [$w cget -state] "normal"]} {focus $w}
}

# ::tk::TextAutoScan --
# This procedure is invoked when the mouse leaves a text window
# with button 1 down.  It scrolls the window up, down, left, or right,
# depending on where the mouse is (this information was saved in
# ::tk::Priv(x) and ::tk::Priv(y)), and reschedules itself as an "after"
# command so that the window continues to scroll until the mouse
# moves back into the window or the mouse button is released.
#
# Arguments:
# w -		The text window.

proc ::tk::TextAutoScan {w} {
    variable ::tk::Priv
    if {![winfo exists $w]} return
    if {$Priv(y) >= [winfo height $w]} {
	$w yview scroll 2 units
    } elseif {$Priv(y) < 0} {
	$w yview scroll -2 units
    } elseif {$Priv(x) >= [winfo width $w]} {
	$w xview scroll 2 units
    } elseif {$Priv(x) < 0} {
	$w xview scroll -2 units
    } else {
	return
    }
    TextSelectTo $w $Priv(x) $Priv(y)
    set Priv(afterId) [after 50 [list tk::TextAutoScan $w]]
}

# ::tk::TextSetCursor
# Move the insertion cursor to a given position in a text.  Also
# clears the selection, if there is one in the text, and makes sure
# that the insertion cursor is visible.  Also, don't let the insertion
# cursor appear on the dummy last line of the text.
#
# Arguments:
# w -		The text window.
# pos -		The desired new position for the cursor in the window.

proc ::tk::TextSetCursor {w pos} {

    if {[$w compare $pos == end]} {
	set pos {end - 1 chars}
    }
    $w mark set insert $pos
    $w tag remove sel 1.0 end
    $w see insert
    if {[$w cget -autoseparators]} {$w edit separator}
}

# ::tk::TextKeySelect
# This procedure is invoked when stroking out selections using the
# keyboard.  It moves the cursor to a new position, then extends
# the selection to that position.
#
# Arguments:
# w -		The text window.
# new -		A new position for the insertion cursor (the cursor hasn't
#		actually been moved to this position yet).

proc ::tk::TextKeySelect {w new} {

    if {[string equal [$w tag nextrange sel 1.0 end] ""]} {
	if {[$w compare $new < insert]} {
	    $w tag add sel $new insert
	} else {
	    $w tag add sel insert $new
	}
	$w mark set anchor insert
    } else {
	if {[$w compare $new < anchor]} {
	    set first $new
	    set last anchor
	} else {
	    set first anchor
	    set last $new
	}
	$w tag remove sel 1.0 $first
	$w tag add sel $first $last
	$w tag remove sel $last end
    }
    $w mark set insert $new
    $w see insert
    update idletasks
}

# ::tk::TextResetAnchor --
# Set the selection anchor to whichever end is farthest from the
# index argument.  One special trick: if the selection has two or
# fewer characters, just leave the anchor where it is.  In this
# case it doesn't matter which point gets chosen for the anchor,
# and for the things like Shift-Left and Shift-Right this produces
# better behavior when the cursor moves back and forth across the
# anchor.
#
# Arguments:
# w -		The text widget.
# index -	Position at which mouse button was pressed, which determines
#		which end of selection should be used as anchor point.

proc ::tk::TextResetAnchor {w index} {

    if {[string equal [$w tag ranges sel] ""]} {
	# Don't move the anchor if there is no selection now; this makes
	# the widget behave "correctly" when the user clicks once, then
	# shift-clicks somewhere -- ie, the area between the two clicks will be
	# selected. [Bug: 5929].
	return
    }
    set a [$w index $index]
    set b [$w index sel.first]
    set c [$w index sel.last]
    if {[$w compare $a < $b]} {
	$w mark set anchor sel.last
	return
    }
    if {[$w compare $a > $c]} {
	$w mark set anchor sel.first
	return
    }
    scan $a "%d.%d" lineA chA
    scan $b "%d.%d" lineB chB
    scan $c "%d.%d" lineC chC
    if {$lineB < $lineC+2} {
	set total [string length [$w get $b $c]]
	if {$total <= 2} {
	    return
	}
	if {[string length [$w get $b $a]] < ($total/2)} {
	    $w mark set anchor sel.last
	} else {
	    $w mark set anchor sel.first
	}
	return
    }
    if {($lineA-$lineB) < ($lineC-$lineA)} {
	$w mark set anchor sel.last
    } else {
	$w mark set anchor sel.first
    }
}

# ::tk::TextInsert --
# Insert a string into a text at the point of the insertion cursor.
# If there is a selection in the text, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The text window in which to insert the string
# s -		The string to insert (usually just a single character)

proc ::tk::TextInsert {w s} {
    if {[string equal $s ""] || [string equal [$w cget -state] "disabled"]} {
	return
    }
    set compound 0
    catch {
	if {[$w compare sel.first <= insert] \
		&& [$w compare sel.last >= insert]} {
            set oldSeparator [$w cget -autoseparators]
            if { $oldSeparator } {
                $w configure -autoseparators 0
                $w edit separator
                set compound 1
            }
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    $w see insert
    if { $compound && $oldSeparator } {
        $w edit separator
        $w configure -autoseparators 1
    }
}

# ::tk::TextUpDownLine --
# Returns the index of the character one line above or below the
# insertion cursor.  There are two tricky things here.  First,
# we want to maintain the original column across repeated operations,
# even though some lines that will get passed through don't have
# enough characters to cover the original column.  Second, don't
# try to scroll past the beginning or end of the text.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# n -		The number of lines to move: -1 for up one line,
#		+1 for down one line.

proc ::tk::TextUpDownLine {w n} {
    variable ::tk::Priv

    set i [$w index insert]
    scan $i "%d.%d" line char
    if {[string compare $Priv(prevPos) $i]} {
	set Priv(char) $char
    }
    set new [$w index [expr {$line + $n}].$Priv(char)]
    if {[$w compare $new == end] || [$w compare $new == "insert linestart"]} {
	set new $i
    }
    set Priv(prevPos) $new
    return $new
}

# ::tk::TextPrevPara --
# Returns the index of the beginning of the paragraph just before a given
# position in the text (the beginning of a paragraph is the first non-blank
# character after a blank line).
#
# Arguments:
# w -		The text window in which the cursor is to move.
# pos -		Position at which to start search.

proc ::tk::TextPrevPara {w pos} {
    set pos [$w index "$pos linestart"]
    while {1} {
	if {([string equal [$w get "$pos - 1 line"] "\n"] \
		&& [string compare [$w get $pos] "\n"]) \
		|| [string equal $pos "1.0"]} {
	    if {[regexp -indices {^[ 	]+(.)} [$w get $pos "$pos lineend"] \
		    dummy index]} {
		set pos [$w index "$pos + [lindex $index 0] chars"]
	    }
	    if {[$w compare $pos != insert] || [string equal $pos 1.0]} {
		return $pos
	    }
	}
	set pos [$w index "$pos - 1 line"]
    }
}

# ::tk::TextNextPara --
# Returns the index of the beginning of the paragraph just after a given
# position in the text (the beginning of a paragraph is the first non-blank
# character after a blank line).
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.

proc ::tk::TextNextPara {w start} {
    set pos [$w index "$start linestart + 1 line"]
    while {[string compare [$w get $pos] "\n"]} {
	if {[$w compare $pos == end]} {
	    return [$w index "end - 1c"]
	}
	set pos [$w index "$pos + 1 line"]
    }
    while {[string equal [$w get $pos] "\n"]} {
	set pos [$w index "$pos + 1 line"]
	if {[$w compare $pos == end]} {
	    return [$w index "end - 1c"]
	}
    }
    if {[regexp -indices {^[ 	]+(.)} [$w get $pos "$pos lineend"] \
	    dummy index]} {
	return [$w index "$pos + [lindex $index 0] chars"]
    }
    return $pos
}

# ::tk::TextScrollPages --
# This is a utility procedure used in bindings for moving up and down
# pages and possibly extending the selection along the way.  It scrolls
# the view in the widget by the number of pages, and it returns the
# index of the character that is at the same position in the new view
# as the insertion cursor used to be in the old view.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# count -	Number of pages forward to scroll;  may be negative
#		to scroll backwards.

proc ::tk::TextScrollPages {w count} {
    set bbox [$w bbox insert]
    $w yview scroll $count pages
    if {[string equal $bbox ""]} {
	return [$w index @[expr {[winfo height $w]/2}],0]
    }
    return [$w index @[lindex $bbox 0],[lindex $bbox 1]]
}

# ::tk::TextTranspose --
# This procedure implements the "transpose" function for text widgets.
# It tranposes the characters on either side of the insertion cursor,
# unless the cursor is at the end of the line.  In this case it
# transposes the two characters to the left of the cursor.  In either
# case, the cursor ends up to the right of the transposed characters.
#
# Arguments:
# w -		Text window in which to transpose.

proc ::tk::TextTranspose w {
    set pos insert
    if {[$w compare $pos != "$pos lineend"]} {
	set pos [$w index "$pos + 1 char"]
    }
    set new [$w get "$pos - 1 char"][$w get  "$pos - 2 char"]
    if {[$w compare "$pos - 1 char" == 1.0]} {
	return
    }
    $w delete "$pos - 2 char" $pos
    $w insert insert $new
    $w see insert
}

# ::tk_textCopy --
# This procedure copies the selection from a text widget into the
# clipboard.
#
# Arguments:
# w -		Name of a text widget.

proc ::tk_textCopy w {
    if {![catch {set data [$w get sel.first sel.last]}]} {
	clipboard clear -displayof $w
	clipboard append -displayof $w $data
    }
}

# ::tk_textCut --
# This procedure copies the selection from a text widget into the
# clipboard, then deletes the selection (if it exists in the given
# widget).
#
# Arguments:
# w -		Name of a text widget.

proc ::tk_textCut w {
    if {![catch {set data [$w get sel.first sel.last]}]} {
	clipboard clear -displayof $w
	clipboard append -displayof $w $data
	$w delete sel.first sel.last
    }
}

# ::tk_textPaste --
# This procedure pastes the contents of the clipboard to the insertion
# point in a text widget.
#
# Arguments:
# w -		Name of a text widget.

proc ::tk_textPaste w {
    global tcl_platform
    if {![catch {::tk::GetSelection $w CLIPBOARD} sel]} {
	set oldSeparator [$w cget -autoseparators]
	if { $oldSeparator } {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	if {[string compare [tk windowingsystem] "x11"]} {
	    catch { $w delete sel.first sel.last }
	}
	$w insert insert $sel
	if { $oldSeparator } {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
}

# ::tk::TextNextWord --
# Returns the index of the next word position after a given position in the
# text.  The next word is platform dependent and may be either the next
# end-of-word position or the next start-of-word position after the next
# end-of-word position.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.

if {[string equal $tcl_platform(platform) "windows"]}  {
    proc ::tk::TextNextWord {w start} {
	TextNextPos $w [TextNextPos $w $start tcl_endOfWord] \
	    tcl_startOfNextWord
    }
} else {
    proc ::tk::TextNextWord {w start} {
	TextNextPos $w $start tcl_endOfWord
    }
}

# ::tk::TextNextPos --
# Returns the index of the next position after the given starting
# position in the text as computed by a specified function.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.
# op -		Function to use to find next position.

proc ::tk::TextNextPos {w start op} {
    set text ""
    set cur $start
    while {[$w compare $cur < end]} {
	set text $text[$w get $cur "$cur lineend + 1c"]
	set pos [$op $text 0]
	if {$pos >= 0} {
	    ## Adjust for embedded windows and images
	    ## dump gives us 3 items per window/image
	    set dump [$w dump -image -window $start "$start + $pos c"]
	    if {[llength $dump]} {
		set pos [expr {$pos + ([llength $dump]/3)}]
	    }
	    return [$w index "$start + $pos c"]
	}
	set cur [$w index "$cur lineend +1c"]
    }
    return end
}

# ::tk::TextPrevPos --
# Returns the index of the previous position before the given starting
# position in the text as computed by a specified function.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.
# op -		Function to use to find next position.

proc ::tk::TextPrevPos {w start op} {
    set text ""
    set cur $start
    while {[$w compare $cur > 0.0]} {
	set text [$w get "$cur linestart - 1c" $cur]$text
	set pos [$op $text end]
	if {$pos >= 0} {
	    ## Adjust for embedded windows and images
	    ## dump gives us 3 items per window/image
	    set dump [$w dump -image -window "$cur linestart" "$start - 1c"]
	    if {[llength $dump]} {
		## This is a hokey extra hack for control-arrow movement
		## that should be in a while loop to be correct (hobbs)
		if {[$w compare [lindex $dump 2] > \
			"$cur linestart - 1c + $pos c"]} {
		    incr pos -1
		}
		set pos [expr {$pos + ([llength $dump]/3)}]
	    }
	    return [$w index "$cur linestart - 1c + $pos c"]
	}
	set cur [$w index "$cur linestart - 1c"]
    }
    return 0.0
}

# ::tk::TextScanMark --
#
# Marks the start of a possible scan drag operation
#
# Arguments:
# w -	The text window from which the text to get
# x -	x location on screen
# y -	y location on screen

proc ::tk::TextScanMark {w x y} {
    $w scan mark $x $y
    set ::tk::Priv(x) $x
    set ::tk::Priv(y) $y
    set ::tk::Priv(mouseMoved) 0
}

# ::tk::TextScanDrag --
#
# Marks the start of a possible scan drag operation
#
# Arguments:
# w -	The text window from which the text to get
# x -	x location on screen
# y -	y location on screen

proc ::tk::TextScanDrag {w x y} {
    # Make sure these exist, as some weird situations can trigger the
    # motion binding without the initial press.  [Bug #220269]
    if {![info exists ::tk::Priv(x)]} { set ::tk::Priv(x) $x }
    if {![info exists ::tk::Priv(y)]} { set ::tk::Priv(y) $y }
    if {($x != $::tk::Priv(x)) || ($y != $::tk::Priv(y))} {
	set ::tk::Priv(mouseMoved) 1
    }
    if {[info exists ::tk::Priv(mouseMoved)] && $::tk::Priv(mouseMoved)} {
	$w scan dragto $x $y
    }
}
