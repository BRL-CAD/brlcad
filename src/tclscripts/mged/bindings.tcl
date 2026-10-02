#                    B I N D I N G S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
if ![info exists mged_players] {
    set mged_players {}
}

namespace eval ::brlcad::mged::bindings {}

proc ::brlcad::mged::bindings::run {w args} {
    ::brlcad::mged winset $w
    ::brlcad::mged {*}$args
    return -code break
}

proc ::brlcad::mged::bindings::run_catching {w args} {
    ::brlcad::mged winset $w
    catch {::brlcad::mged {*}$args}
    return -code break
}

proc ::brlcad::mged::bindings::run_focused {w args} {
    ::brlcad::mged winset $w
    focus $w
    ::brlcad::mged {*}$args
    return -code break
}

proc ::brlcad::mged::bindings::run_and_print {w focus_window args} {
    ::brlcad::mged winset $w
    if {$focus_window} {
	focus $w
    }
    ::print_return_val [::brlcad::mged {*}$args]
    return -code break
}

proc ::brlcad::mged::bindings::run_and_puts {w args} {
    ::brlcad::mged winset $w
    puts [::brlcad::mged {*}$args]
    return -code break
}

proc ::brlcad::mged::bindings::toggle_rset {w resource setting} {
    ::brlcad::mged winset $w
    ::brlcad::mged rset $resource $setting !
    ::update_gui $w $setting [::brlcad::mged rset $resource $setting]
    return -code break
}

proc ::brlcad::mged::bindings::toggle_dm {w setting} {
    ::brlcad::mged winset $w
    ::brlcad::mged dm set $setting !
    ::update_gui $w $setting [::brlcad::mged dm set $setting]
    return -code break
}

proc ::brlcad::mged::bindings::toggle_variable {w variable update_name} {
    upvar #0 $variable value

    ::brlcad::mged winset $w
    set value !
    if {$update_name ne ""} {
	::update_gui $w $update_name $value
    }
    return -code break
}

proc ::brlcad::mged::bindings::toggle_forwarding {w} {
    global forwarding_key

    ::toggle_forward_key_bindings $w
    ::update_gui $w forward_keys $forwarding_key($w)
    return -code break
}

proc ::brlcad::mged::bindings::reset {w} {
    ::brlcad::mged winset $w
    ::reset_everything $w
    return -code break
}

proc ::brlcad::mged::bindings::release {w} {
    ::mged_release_dm $w
    return -code break
}

proc ::brlcad::mged::bindings::mouse_wheel {w focus_window stop_event delta} {
    ::brlcad::mged winset $w
    if {$focus_window} {
	focus $w
    }
    ::brlcad::mged zoom [expr {$delta < 0 ? 0.9 : 1.1}]
    if {$stop_event} {
	return -code break
    }
}

proc ::brlcad::mged::bindings::mouse_motion {w x y} {
    ::brlcad::mged winset $w
    if {[::brlcad::mged dm type] eq "tkswrast"} {
	::brlcad::mged dm motion $x $y
    }
    return -code break
}

proc ::brlcad::mged::bindings::run_with_hint {w hint args} {
    ::brlcad::mged winset $w
    ::brlcad::mged {*}$args
    ::shift_grip_hints $w $hint
    return -code break
}

proc ::brlcad::mged::bindings::run_with_scale_hint {w axis args} {
    ::brlcad::mged winset $w
    ::brlcad::mged {*}$args
    ::scale_shift_grip_hints $w $axis
    return -code break
}

proc ::brlcad::mged::bindings::run_factor {w token variable sign} {
    upvar #0 $variable value

    ::brlcad::mged winset $w
    ::brlcad::mged knob -i $token [expr {$sign * $value}]
    return -code break
}

proc mged_command_busy {options} {
    expr {[dict exists $options -errorcode] &&
	[dict get $options -errorcode] eq {BRLCAD MGED COMMAND_BUSY}}
}

proc mged_activate_dm {w} {
    global tcl_platform

    if {[catch {::brlcad::mged winset $w} message options]} {
	if {[mged_command_busy $options]} {
	    return
	}
	return -options $options $message
    }

    # Some platforms should not be forced to move keyboard focus on entry.
    if {$tcl_platform(platform) != "windows" &&
	$tcl_platform(os) != "Darwin"} {
	focus $w
    }
}

proc mged_release_dm {w} {
    if {![winfo exists $w]} {
	return
    }

    if {[catch {
	::brlcad::mged winset $w
	::brlcad::mged dm idle
    } message options]} {
	if {[mged_command_busy $options]} {
	    # A mouse pick may pump events while an edit command runs.  Defer
	    # the release until its shared GED state is available again.
	    set retry_interval_ms 25
	    after $retry_interval_ms [list ::mged_release_dm $w]
	    return
	}
	return -options $options $message
    }
}

proc mged_bind_dm { w } {
    global hot_key
    global forwarding_key

    # KeySym for <F9> --> 0xffc6 --> 65478
    set hot_key 65478

    bind $w <Enter> [list ::mged_activate_dm $w]

    #default mouse bindings
    default_mouse_bindings $w

    #default key bindings
    set forwarding_key($w) 0
    default_key_bindings $w
}

proc print_return_val str {
    if {$str != ""} {
	distribute_text "" "" $str
	::brlcad::mged stuff_str $str
    }
}

if ![info exists mged_default(dm_key_bindings)] {
    set mged_default(dm_key_bindings) "\tKey Sequence\t\tBehavior
\ta\t\t\ttoggle angle distance cursor (ADC)
\te\t\t\ttoggle edit axes
\tm\t\t\ttoggle model axes
\tv\t\t\ttoggle view axes
\ti\t\t\tadvance illumination pointer forward
\tI\t\t\tadvance illumination pointer backward
\tp\t\t\tsimulate mouse press (i.e. to pick a solid)
\t3\t\t\tview - ae 35 25
\t4\t\t\tview - ae 45 45
\tf\t\t\tfront view
\tt\t\t\ttop view
\tb\t\t\tbottom view
\tl\t\t\tleft view
\tr\t\t\tright view
\tR\t\t\trear view
\ts\t\t\tenter solid illumination state
\to\t\t\tenter object illumination state
\tq\t\t\treject edit
\tu\t\t\tzero knobs and sliders
\t<F1>\t\t\ttoggle depthcue
\t<F2>\t\t\ttoggle zclip
\t<F3>\t\t\ttoggle perspective
\t<F4>\t\t\ttoggle zbuffer
\t<F5>\t\t\ttoggle lighting
\t<F6>\t\t\ttoggle perspective angle
\t<F7>\t\t\ttoggle faceplate
\t<F8>\t\t\ttoggle faceplate GUI
\t<F9>\t\t\ttoggle keystroke forwarding
\t<F12>\t\t\tzero knobs
\t<Left>\t\t\tabsolute rotate about y axis
\t<Right>\t\t\tabsolute rotate about y axis
\t<Down>\t\t\tabsolute rotate about x axis
\t<Up>\t\t\tabsolute rotate about x axis
\t<Shift-Left>\t\tabsolute translate in X direction
\t<Shift-Right>\t\tabsolute translate in X direction
\t<Shift-Down>\t\tabsolute translate in Z direction
\t<Shift-Up>\t\tabsolute translate in Z direction
\t<Control-Shift-Left>\tabsolute rotate about z axis
\t<Control-Shift-Right>\tabsolute rotate about z axis
\t<Control-Shift-Down>\tabsolute translate in Y direction
\t<Control-Shift-Up>\tabsolute translate in Y direction
\t<Control-n>\t\tgoto next view
\t<Control-p>\t\tgoto previous view
\t<Control-t>\t\ttoggle between the current view and the last view"
}

proc default_key_bindings { w } {
    # common view shortcuts
    bind $w 2 [list ::brlcad::mged::bindings::run $w ae 35 -25]
    bind $w 3 [list ::brlcad::mged::bindings::run $w press 35,25]
    bind $w 4 [list ::brlcad::mged::bindings::run $w press 45,45]
    bind $w 5 [list ::brlcad::mged::bindings::run $w ae 145 25]
    bind $w 6 [list ::brlcad::mged::bindings::run $w ae 215 25]
    bind $w 7 [list ::brlcad::mged::bindings::run $w ae 325 25]

    # default key shortcuts
    bind $w a [list ::brlcad::mged::bindings::run $w adc]
    bind $w b [list ::brlcad::mged::bindings::run $w press bottom]
    bind $w c [list ::open_cmd_win id_0]
    bind $w e [list ::brlcad::mged::bindings::toggle_rset $w ax edit_draw]
    bind $w f [list ::brlcad::mged::bindings::run $w press front]
    bind $w i [list ::brlcad::mged::bindings::run $w aip f]
    bind $w l [list ::brlcad::mged::bindings::run $w press left]
    bind $w m [list ::brlcad::mged::bindings::toggle_rset $w ax model_draw]
    bind $w o [list ::brlcad::mged::bindings::run $w press oill]
    bind $w p [list ::brlcad::mged::bindings::run $w M 1 0 0]
    bind $w q [list ::brlcad::mged::bindings::run $w press reject]
    bind $w r [list ::brlcad::mged::bindings::run $w press right]
    bind $w s [list ::brlcad::mged::bindings::run $w press sill]
    bind $w t [list ::brlcad::mged::bindings::run $w press top]
    bind $w u [list ::brlcad::mged::bindings::run $w svb]
    bind $w v [list ::brlcad::mged::bindings::toggle_rset $w ax view_draw]

    # default shift+key shortcuts
    bind $w A [list ::brlcad::mged::bindings::run $w press accept]
    bind $w I [list ::brlcad::mged::bindings::run $w aip b]
    bind $w N [list ::brlcad::mged::bindings::run_and_puts $w nirt -b]
    bind $w P [list ::brlcad::mged::bindings::run_catching $w sed_apply]
    bind $w R [list ::brlcad::mged::bindings::run $w press rear]
    bind $w S [list ::brlcad::mged::bindings::run_catching $w sed_reset]

    # default control+key shortcuts
    bind $w <Control-n> [list ::brlcad::mged::bindings::run $w view_ring next]
    bind $w <Control-p> [list ::brlcad::mged::bindings::run $w view_ring prev]
    bind $w <Control-t> [list ::brlcad::mged::bindings::run $w view_ring toggle]

    # shift grips navigation
    bind $w <Control-Shift-Down> [list ::brlcad::mged::bindings::run_factor $w aY mged_default(tran_factor) 1]
    bind $w <Control-Shift-Left> [list ::brlcad::mged::bindings::run_factor $w az mged_default(rot_factor) 1]
    bind $w <Control-Shift-Right> [list ::brlcad::mged::bindings::run_factor $w az mged_default(rot_factor) -1]
    bind $w <Control-Shift-Up> [list ::brlcad::mged::bindings::run_factor $w aY mged_default(tran_factor) -1]
    bind $w <Down> [list ::brlcad::mged::bindings::run_factor $w ax mged_default(rot_factor) 1]
    bind $w <Left> [list ::brlcad::mged::bindings::run_factor $w ay mged_default(rot_factor) -1]
    bind $w <Right> [list ::brlcad::mged::bindings::run_factor $w ay mged_default(rot_factor) 1]
    bind $w <Up> [list ::brlcad::mged::bindings::run_factor $w ax mged_default(rot_factor) -1]
    bind $w <Shift-Down> [list ::brlcad::mged::bindings::run_factor $w aZ mged_default(tran_factor) -1]
    bind $w <Shift-Left> [list ::brlcad::mged::bindings::run_factor $w aX mged_default(tran_factor) 1]
    bind $w <Shift-Right> [list ::brlcad::mged::bindings::run_factor $w aX mged_default(tran_factor) -1]
    bind $w <Shift-Up> [list ::brlcad::mged::bindings::run_factor $w aZ mged_default(tran_factor) 1]

    # function keys (settings)
    bind $w <F1> [list ::brlcad::mged::bindings::toggle_dm $w depthcue]
    bind $w <F2> [list ::brlcad::mged::bindings::toggle_dm $w zclip]
    bind $w <F3> [list ::brlcad::mged::bindings::toggle_variable $w perspective_mode perspective_mode]
    bind $w <F4> [list ::brlcad::mged::bindings::toggle_dm $w zbuffer]
    bind $w <F5> [list ::brlcad::mged::bindings::toggle_dm $w lighting]
    bind $w <F6> [list ::brlcad::mged::bindings::toggle_variable $w toggle_perspective {}]
    bind $w <F7> [list ::brlcad::mged::bindings::toggle_variable $w faceplate faceplate]
    bind $w <F8> [list ::brlcad::mged::bindings::toggle_variable $w orig_gui orig_gui]
    bind $w <F9> [list ::brlcad::mged::bindings::toggle_forwarding $w]
    bind $w <F12> [list ::brlcad::mged::bindings::run $w knob zero]

    # interrupt
    bind $w <Escape> [list ::brlcad::mged::bindings::reset $w]

    # Throw away other key events
    bind $w <KeyPress> {
	break
    }
}

proc reset_everything { w } {
    global mged_gui

    # stop all the spinning
    ::brlcad::mged knob zero

    # stop all the editing
    ::brlcad::mged press reject
    
    # restore default mouse behavior
    set id [get_player_id_dm $w]
    set mged_gui($id,mouse_behavior) d
    set_mouse_behavior $id
}

proc set_forward_keys { w val } {
    global forwarding_key

    set forwarding_key($w) $val
    if {$forwarding_key($w)} {
	forward_key_bindings $w
    } else {
	default_key_bindings $w
    }
}

proc toggle_forward_key_bindings { w } {
    global forwarding_key

    if {$forwarding_key($w)} {
	default_key_bindings $w
	set forwarding_key($w) 0
    } else {
	forward_key_bindings $w
	set forwarding_key($w) 1
    }
}

proc forward_key_bindings { w } {
    set id [get_player_id_dm $w]

    if {$id == "mged"} {
	return
    }

    # First, unset the default key bindings
    bind $w a {}
    bind $w e {}
    bind $w m {}
    bind $w v {}
    bind $w i {}
    bind $w I {}
    bind $w p {}
    bind $w 2 {}
    bind $w 3 {}
    bind $w 4 {}
    bind $w 5 {}
    bind $w 6 {}
    bind $w 7 {}
    bind $w f {}
    bind $w t {}
    bind $w b {}
    bind $w l {}
    bind $w N {}
    bind $w r {}
    bind $w R {}
    bind $w s {}
    bind $w o {}
    bind $w q {}
    bind $w u {}
    bind $w c {}
    bind $w A {}
    bind $w P {}
    bind $w S {}
    bind $w <F1> {}
    bind $w <F2> {}
    bind $w <F3> {}
    bind $w <F4> {}
    bind $w <F5> {}
    bind $w <F6> {}
    bind $w <F7> {}
    bind $w <F8> {}
    bind $w <F12> {}

    bind $w <Left> {}
    bind $w <Right> {}
    bind $w <Down> {}
    bind $w <Up> {}
    bind $w <Shift-Left> {}
    bind $w <Shift-Right> {}
    bind $w <Shift-Down> {}
    bind $w <Shift-Up> {}
    bind $w <Control-Shift-Left> {}
    bind $w <Control-Shift-Right> {}
    bind $w <Control-Shift-Down> {}
    bind $w <Control-Shift-Up> {}

    bind $w <Control-n> {}
    bind $w <Control-p> {}
    bind $w <Control-t> {}

    bind $w <Escape> {}

    # The focus commands in the binding below are necessary to insure
    # that .$id.t gets the event.
    bind $w <KeyPress> "\
	    focus .$id.t;\
	    set mged_gui(.$id.t,insert_char_flag) 1;\
	    event generate .$id.t <KeyPress> -state %s -keysym %K;\
	    set mged_gui(.$id.t,insert_char_flag) 0;\
	    focus %W;\
	    break"
}

proc default_mouse_bindings { w } {
    global no_focus_hack
    global transform
    global tcl_platform

    # default button bindings
    if {![regexp {^[0-9]+} $::tcl_platform(osVersion) osMajorVersion]} {
	set osMajorVersion 0
    }

    if {[info exists no_focus_hack] && $no_focus_hack} {
	bind $w <1> [list ::brlcad::mged::bindings::run $w zoom 0.5]
	bind $w <2> [list ::brlcad::mged::bindings::run_and_print $w 0 dm m %x %y]
	bind $w <3> [list ::brlcad::mged::bindings::run $w zoom 2.0]

	# Mouse wheel fun with Tcl/Tk - see https://wiki.tcl-lang.org/page/mousewheel
	bind $w <MouseWheel> [list ::brlcad::mged::bindings::mouse_wheel $w 1 0 %D]
	bind $w <Button-4> {event generate [focus] <MouseWheel> -delta  120}
	bind $w <Button-5> {event generate [focus] <MouseWheel> -delta -120}

	if {$::tcl_platform(os) == "Darwin"} {
	    # Mac option-key emulates mouse 2 (doesn't work?)
	    bind $w <Option-ButtonPress-1> [list ::brlcad::mged::bindings::run_and_print $w 0 dm m %x %y]
	    # Mac command-key emulates mouse 3
	    bind $w <Mod2-ButtonPress-1> [list ::brlcad::mged::bindings::run $w zoom 2.0]
	}
    } else {
	bind $w <1> [list ::brlcad::mged::bindings::run_focused $w zoom 0.5]
	bind $w <2> [list ::brlcad::mged::bindings::run_and_print $w 1 dm m %x %y]
	bind $w <3> [list ::brlcad::mged::bindings::run_focused $w zoom 2.0]

	# Mouse wheel fun with Tcl/Tk - see https://wiki.tcl-lang.org/page/mousewheel
	bind $w <MouseWheel> [list ::brlcad::mged::bindings::mouse_wheel $w 1 1 %D]
	bind $w <Button-4> {event generate [focus] <MouseWheel> -delta  120}
	bind $w <Button-5> {event generate [focus] <MouseWheel> -delta -120}

	if {$::tcl_platform(os) == "Darwin"} {
	    # Mac command-key emulates mouse 2 (doesn't work?)
	    bind $w <Option-ButtonPress-1> [list ::brlcad::mged::bindings::run_and_print $w 1 dm m %x %y]
	    # Mac command-key emulates mouse 3
	    bind $w <Mod2-ButtonPress-1> [list ::brlcad::mged::bindings::run_focused $w zoom 2.0]
	}
    }

    bind $w <ButtonRelease> [list ::brlcad::mged::bindings::release $w]
    bind $w <Motion> [list ::brlcad::mged::bindings::mouse_motion $w %x %y]
    foreach modifier {Control_L Control_R Shift_L Shift_R Alt_L Alt_R} {
	bind $w <KeyRelease-$modifier> [list ::brlcad::mged::bindings::run $w dm idle]
    }

    if ![catch {::brlcad::mged adc draw} result] {
	set adcflag $result
    } else {
	set adcflag 0
    }

    if {$adcflag == "1" && $transform == "a"} {
	bind $w <Shift-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "Translate ADC" dm adc t %x %y]
	bind $w <Shift-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Translate ADC" dm adc t %x %y]
	bind $w <Shift-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm adc d %x %y]

	bind $w <Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "Rotate Angle 1" dm adc 1 %x %y]
	bind $w <Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Rotate Angle 2" dm adc 2 %x %y]
	bind $w <Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm adc d %x %y]

	bind $w <Shift-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm adc d %x %y]
	bind $w <Shift-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm adc d %x %y]
	bind $w <Shift-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm adc d %x %y]

	#constrained adc defaults
	bind $w <Alt-Shift-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "X Translate ADC" dm con a x %x %y]
	bind $w <Alt-Shift-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Y Translate ADC" dm con a y %x %y]
	bind $w <Alt-Shift-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm con a d %x %y]

	bind $w <Alt-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "Rotate Angle 1" dm con a 1 %x %y]
	bind $w <Alt-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Rotate Angle 2" dm con a 2 %x %y]
	bind $w <Alt-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm con a d %x %y]

	bind $w <Alt-Shift-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm con a d %x %y]
	bind $w <Alt-Shift-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm con a d %x %y]
	bind $w <Alt-Shift-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Translate Tick Distance" dm con a d %x %y]
    } else {
	bind $w <Shift-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w Translate dm am t %x %y]
	bind $w <Shift-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w Translate dm am t %x %y]
	bind $w <Shift-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w Translate dm am t %x %y]

	bind $w <Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w Rotate dm am r %x %y]
	bind $w <Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w Rotate dm am r %x %y]
	bind $w <Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w Rotate dm am r %x %y]

	bind $w <Shift-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w Scale/Zoom dm am s %x %y]
	bind $w <Shift-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w Scale/Zoom dm am s %x %y]
	bind $w <Shift-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w Scale/Zoom dm am s %x %y]

	#constrained defaults
	bind $w <Alt-Shift-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "X Translation" dm con t x %x %y]
	bind $w <Alt-Shift-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Y Translation" dm con t y %x %y]
	bind $w <Alt-Shift-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Z Translation" dm con t z %x %y]

	bind $w <Alt-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_hint $w "X Rotation" dm con r x %x %y]
	bind $w <Alt-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_hint $w "Y Rotation" dm con r y %x %y]
	bind $w <Alt-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_hint $w "Z Rotation" dm con r z %x %y]

	bind $w <Alt-Shift-Control-ButtonPress-1> [list ::brlcad::mged::bindings::run_with_scale_hint $w X dm con s x %x %y]
	bind $w <Alt-Shift-Control-ButtonPress-2> [list ::brlcad::mged::bindings::run_with_scale_hint $w Y dm con s y %x %y]
	bind $w <Alt-Shift-Control-ButtonPress-3> [list ::brlcad::mged::bindings::run_with_scale_hint $w Z dm con s z %x %y]
    }
}

proc shift_grip_hints { w hint } {
    global mged_display
    global mged_gui
    global win_to_id

    if ![info exists win_to_id($w)] {
	return
    }

    set id $win_to_id($w)
    set mged_gui($id,illum_label) $hint
}

proc scale_shift_grip_hints { w axis } {
    if {[::brlcad::mged status state] == "OBJ EDIT"} {
	shift_grip_hints $w "$axis Scale"
    } else {
	shift_grip_hints $w "Scale/Zoom"
    }
}

proc update_gui { w vname val } {
    global mged_players
    global mged_gui

    foreach id $mged_players {
	if {$mged_gui($id,active_dm) == $w} {
	    set mged_gui($id,$vname) $val
	    return
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
