#                A N I M A T I O N P R E V I E W . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.

namespace eval ::RtWizard::AnimationPreview {
    variable window .rtwizard_animation_preview
    variable state
    variable poll_ms 75

    proc open {type settings} {
	variable window
	variable state

	if {[winfo exists $window]} {
	    if {$state(closing)} {return}
	    if {$state(running)} {
		raise $window
		return
	    }
	    close
	}

	set fps [dict get $settings _animation_fps]
	if {$fps <= 0} {error "Animation frame rate must be positive."}
	set directory [rtwizard_render $type $settings 1]
	array set state [list directory $directory photo [image create photo] \
		frame 0 ready 0 running 1 playing 1 cancelled 0 closing 0 \
		tick {} interval [expr {max(1, round(1000.0 / $fps))}] \
		status "Planning animation..."]
	toplevel $window
	wm title $window "RtWizard Animation Preview"
	wm protocol $window WM_DELETE_WINDOW [namespace code close]

	ttk::label $window.image -image $state(photo) -anchor center
	ttk::label $window.status -textvariable [namespace current]::state(status) -wraplength 360
	scale $window.frame -from 1 -to 1 -orient horizontal -length 240 -showvalue 0 \
		-variable [namespace current]::state(frame) -command [namespace code show_frame] -state disabled
	bind $window.frame <ButtonPress-1> [namespace code {play 0}]
	ttk::frame $window.controls
	ttk::button $window.controls.play -text Pause -state disabled -command [namespace code toggle_play]
	ttk::button $window.controls.cancel -text Cancel -command [namespace code cancel]
	ttk::button $window.controls.close -text Close -command [namespace code close]
	pack $window.controls.play $window.controls.cancel $window.controls.close -side left -padx 4
	pack $window.image -fill both -expand true -padx 8 -pady 8
	pack $window.frame $window.status $window.controls -fill x -padx 8 -pady 4
	poll
    }

    proc show_frame {number} {
	variable state
	set number [expr {int($number)}]
	if {$number < 1 || $number > $state(ready)} {return}
	$state(photo) configure -file [file join $state(directory) [format "frame-%06d.png" $number]]
	set state(frame) $number
    }

    proc tick {} {
	variable state
	set state(tick) {}
	if {!$state(playing)} {return}
	if {$state(frame) < $state(ready)} {
	    show_frame [expr {$state(frame) + 1}]
	} elseif {!$state(running) && $state(ready) > 1} {
	    show_frame 1
	}
	set state(tick) [after $state(interval) [namespace code tick]]
    }

    proc play {playing} {
	variable window
	variable state
	set state(playing) $playing
	after cancel $state(tick)
	set state(tick) {}
	$window.controls.play configure -text [expr {$playing ? "Pause" : "Play"}]
	if {$playing} {tick}
    }

    proc toggle_play {} {
	variable state
	play [expr {!$state(playing)}]
    }

    proc cancel {} {
	variable window
	variable state
	if {!$state(running) || $state(cancelled)} {return}
	rtwizard_preview cancel
	set state(cancelled) 1
	set state(status) "Cancelling..."
	play 0
	$window.controls.cancel configure -state disabled
    }

    proc close {} {
	variable window
	variable state
	if {$state(running)} {
	    cancel
	    set state(closing) 1
	    wm withdraw $window
	    return
	}
	play 0
	rtwizard_preview release
	image delete $state(photo)
	destroy $window
    }

    proc poll {} {
	variable window
	variable state
	variable poll_ms
	if {[catch {rtwizard_preview poll} result]} {
	    set state(running) 0
	    set state(status) [string trim $result]
	} else {
	    lassign $result state(running) ready total
	    if {$ready > $state(ready)} {
		set state(ready) $ready
		$window.frame configure -to $ready
		if {$state(frame) < 1 || [image width $state(photo)] == 0} {show_frame 1}
		if {$ready > 1} {
		    $window.frame configure -state normal
		    $window.controls.play configure -state normal
		    if {$state(playing) && $state(tick) eq ""} {play 1}
		}
	    }
	    if {$state(cancelled)} {
		if {!$state(running)} {set state(status) "Preview cancelled"}
	    } elseif {$state(running)} {
		if {$total > 0} {set state(status) "Rendering frames ($ready of $total)"}
	    } else {
		set state(status) "Preview ready ($total frames)"
	    }
	}
	if {$state(running)} {
	    after $poll_ms [namespace code poll]
	} elseif {$state(closing)} {
	    close
	} else {
	    $window.controls.cancel configure -state disabled
	}
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
