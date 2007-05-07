#
# $Id$
#
# Demonstration of custom classes.
#
# The Ttk button doesn't have built-in support for autorepeat.
# Instead of adding -repeatdelay and -repeatinterval options,
# and all the extra binding scripts required to deal with them,
# we create a custom widget class for autorepeating buttons.
#
# Usage:
#	ttk::button .b -class Repeater [... other options ...]
#
# TODO:
#	Use system settings for repeat interval and initial delay.
#
# Notes:
#	Repeater buttons work more like scrollbar arrows than
#	Tk repeating buttons: they fire once immediately when
#	first pressed, and $State(delay) specifies the initial
#	interval before the button starts autorepeating.
#

namespace eval ttk::Repeater {
    variable State
    set State(timer) 	{}	;# [after] id of repeat script
    set State(interval)	100	;# interval between repetitions
    set State(delay)	300	;# delay after initial invocation
}

### Class bindings.
#

bind Repeater <Enter>		{ %W state active }
bind Repeater <Leave>		{ %W state !active }

bind Repeater <Key-space> 	{ ttk::Repeater::Activate %W }
bind Repeater <<Invoke>> 	{ ttk::Repeater::Activate %W }

bind Repeater <ButtonPress-1> 	{ ttk::Repeater::Press %W }
bind Repeater <ButtonRelease-1> { ttk::Repeater::Release %W }
bind Repeater <B1-Leave> 	{ ttk::Repeater::Pause %W }
bind Repeater <B1-Enter> 	{ ttk::Repeater::Resume %W } ;# @@@ see below

# @@@ Workaround for metacity-induced bug:
bind Repeater <B1-Enter> \
    { if {"%d" ne "NotifyUngrab"} { ttk::Repeater::Resume %W } }

### Binding procedures.
#

## Activate -- Keyboard activation binding.
#	Simulate clicking the button, and invoke the command once.
#
proc ttk::Repeater::Activate {w} {
    $w instate disabled { return }
    set oldState [$w state pressed]
    update idletasks; after 100
    $w state $oldState
    after idle [list $w invoke]
}

## Press -- ButtonPress-1 binding.
#	Invoke the command once and start autorepeating after
#	$State(delay) milliseconds.
#
proc ttk::Repeater::Press {w} {
    variable State
    $w instate disabled { return }
    $w state pressed
    $w invoke
    after cancel $State(timer)
    set State(timer) [after $State(delay) [list ttk::Repeater::Repeat $w]]
}

## Release -- ButtonRelease binding.
#	Stop repeating.
#
proc ttk::Repeater::Release {w} {
    variable State
    $w state !pressed
    after cancel $State(timer)
}

## Pause -- B1-Leave binding
#	Temporarily suspend autorepeat.
#
proc ttk::Repeater::Pause {w} {
    variable State
    $w state !pressed
    after cancel $State(timer)
}

## Resume -- B1-Enter binding
#	Resume autorepeat.
#
proc ttk::Repeater::Resume {w} {
    variable State
    $w instate disabled { return }
    $w state pressed
    $w invoke
    after cancel $State(timer)
    set State(timer) [after $State(interval) [list ttk::Repeater::Repeat $w]]
}

## Repeat -- Timer script
#	Invoke the command and reschedule another repetition
#	after $State(interval) milliseconds.
#
proc ttk::Repeater::Repeat {w} {
    variable State
    $w instate disabled { return }
    $w invoke
    set State(timer) [after $State(interval) [list ttk::Repeater::Repeat $w]]
}

#*EOF*
