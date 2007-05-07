########################################################################
# keynav package - Enhanced keyboard navigation
# Copyright (C) 2003 Joe English
# Freely redistributable; see the file license.terms for details.
#
# $Id$
#
########################################################################
#
# Usage:
#
# package require keynav
#
# keynav::enableMnemonics $toplevel --
#	Enable mnemonic accelerators for toplevel widget.  Pressing Alt-K,
#	where K is any alphanumeric key, will send an <<Invoke>> event to the
#	widget with mnemonic K (as determined by the -underline and -text
#	options).
#
#	Side effects: adds a binding for <Alt-KeyPress> to $toplevel
#
# keynav::defaultButton $button --
#	Enables default activation for the toplevel window in which $button
#	appears.  Pressing <Key-Return> invokes the default widget.  The
#	default widget is set to the widget with keyboard focus if it is
#	defaultable, otherwise $button.  A widget is _defaultable_ if it has
#	a -default option which is not set to "disabled".
#
#	Side effects: adds <FocusIn> and <KeyPress-Return> bindings
#	to the toplevel containing $button, and a <Destroy> binding
#	to $button.
#
#	$button must be a defaultable widget.
#

namespace eval keynav {}

package require Tcl 8.4
package require Tk 8.4
package provide keynav 1.0

event add <<Help>> <KeyPress-F1>

#
# Bindings for stock Tk widgets:
# (NB: for 8.3 use tkButtonInvoke, tkMbPost instead)
#
bind Button <<Invoke>>		{ tk::ButtonInvoke %W }
bind Checkbutton <<Invoke>>	{ tk::ButtonInvoke %W }
bind Radiobutton <<Invoke>>	{ tk::ButtonInvoke %W }
bind Menubutton <<Invoke>>	{ tk::MbPost %W }

proc keynav::enableMnemonics {w} {
    bind [winfo toplevel $w] <Alt-KeyPress> {+ keynav::Alt-KeyPress %W %K }
}

# mnemonic $w --
#	Return the mnemonic character for widget $w,
#	as determined by the -text and -underline resources.
#
proc keynav::mnemonic {w} {
    if {[catch {
	set label [$w cget -text]
	set underline [$w cget -underline]
    }]} { return "" }
    return [string index $label $underline]
}

# FindMnemonic $w $key --
#	Locate the descendant of $w with mnemonic $key.
#
proc keynav::FindMnemonic {w key} {
    if {[string length $key] != 1} { return }
    set Q [list [set top [winfo toplevel $w]]]
    while {[llength $Q]} {
	set QN [list]
	foreach w $Q {
	    if {[string equal -nocase $key [mnemonic $w]]} {
		return $w
	    }
	    foreach c [winfo children $w] {
		if {[winfo ismapped $c] && [winfo toplevel $c] eq $top} {
		    lappend QN $c
		}
	    }
	}
	set Q $QN
    }
    return {}
}

# Alt-KeyPress --
#	Alt-KeyPress binding for toplevels with mnemonic accelerators enabled.
#
proc keynav::Alt-KeyPress {w k} {
    set w [FindMnemonic $w $k]
    if {$w ne ""} {
    	event generate $w <<Invoke>>
	return -code break
    }
}

# defaultButton $w --
#	Enable default activation for the toplevel containing $w,
#	and make $w the default default widget.
#
proc keynav::defaultButton {w} {
    variable DefaultButton

    $w configure -default active
    set top [winfo toplevel $w]
    set DefaultButton(current.$top) $w
    set DefaultButton(default.$top) $w

    bind $w <Destroy> [list keynav::CleanupDefault $top]
    bind $top <FocusIn> [list keynav::ClaimDefault $top %W]
    bind $top <KeyPress-Return> [list keynav::ActivateDefault $top]
}

proc keynav::CleanupDefault {top} {
    variable DefaultButton
    unset DefaultButton(current.$top)
    unset DefaultButton(default.$top)
}

# ClaimDefault $top $w --
#	<FocusIn> binding for default activation.
#	Sets the default widget to $w if it is defaultable,
#	otherwise set it to the default default.
#
proc keynav::ClaimDefault {top w} {
    variable DefaultButton
    if {![info exists DefaultButton(current.$top)]} {
	# Someone destroyed the default default, but not
	# the rest of the toplevel.
	return;
    }

    set default $DefaultButton(default.$top)
    if {![catch {$w cget -default} dstate] && $dstate ne "disabled"} {
	set default $w
    }

    if {$default ne $DefaultButton(current.$top)} {
	# Ignore errors -- someone may have destroyed the current default
    	catch { $DefaultButton(current.$top) configure -default normal }
	$default configure -default active
	set DefaultButton(current.$top) $default
    }
}

# ActivateDefault --
#	Invoke the default widget for toplevel window, if any.
#
proc keynav::ActivateDefault {top} {
    variable DefaultButton
    if {[info exists DefaultButton(current.$top)]
	&& [winfo exists $DefaultButton(current.$top)]} {
	event generate $DefaultButton(current.$top) <<Invoke>>
    }
}

#*EOF*
