#                    B I N D I N G S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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

proc mged_bind_dm { w } {
    global hot_key
    global forwarding_key
    global tcl_platform

    set hot_key 65478

#make this the current display manager
    if { $::tcl_platform(platform) != "windows" && $::tcl_platform(os) != "Darwin" } {
	bind $w <Enter> "winset $w; focus $w;"
    } else {
	# some platforms should not be forced window activiation (winset)
	bind $w <Enter> "winset $w;"
    }

#default mouse bindings
    default_mouse_bindings $w

#default key bindings
    set forwarding_key($w) 0
    default_key_bindings $w
}

proc print_return_val str {
    if {$str != ""} {
	distribute_text "" "" $str
	stuff_str $str
    }
}

if ![info exists mged_default(dm_key_bindings)] {
    set mged_default(dm_key_bindings) "\tKey Sequence\t\tBehavior
\ta\t\t\ttoggle angle distance cursor (ADC)
\te\t\t\ttoggle edit axes
\tm\t\t\ttoggle model axes
\tv\t\t\ttoggle view axes
\ti\t\t\tadvance illumation pointer forward
\tI\t\t\tadvance illumation pointer backward
\tp\t\t\tsimulate mouse press (i.e. to pick a solid)
\t0\t\t\tzero knobs
\tx\t\t\trate rotate about x axis
\ty\t\t\trate rotate about y axis
\tz\t\t\trate rotate about z axis
\tX\t\t\trate translate in X direction
\tY\t\t\trate translate in Y direction
\tZ\t\t\trate translate in Z direction
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
    bind $w a "winset $w; adc; break"
    bind $w e "winset $w; rset ax edit_draw !;\
	    update_gui $w edit_draw \[rset ax edit_draw\]; break"
    bind $w m "winset $w; rset ax model_draw !;\
	    update_gui $w model_draw \[rset ax model_draw\]; break"
    bind $w v "winset $w; rset ax view_draw !;\
	    update_gui $w view_draw \[rset ax view_draw\]; break"
    bind $w i "winset $w; aip f; break"
    bind $w I "winset $w; aip b; break"
    bind $w p "winset $w; M 1 0 0; break"
    bind $w 0 "winset $w; knob zero; break"
    bind $w x "winset $w; knob -i x 0.3; break"
    bind $w y "winset $w; knob -i y 0.3; break"
    bind $w z "winset $w; knob -i z 0.3; break"
    bind $w X "winset $w; knob -i x -0.3; break"
    bind $w Y "winset $w; knob -i y -0.3; break"
    bind $w Z "winset $w; knob -i z -0.3; break"
    bind $w 3 "winset $w; press 35,25; break"
    bind $w 4 "winset $w; press 45,45; break"
    bind $w f "winset $w; press front; break"
    bind $w t "winset $w; press top; break"
    bind $w b "winset $w; press bottom; break"
    bind $w l "winset $w; press left; break"
    bind $w r "winset $w; press right; break"
    bind $w R "winset $w; press rear; break"
    bind $w s "winset $w; press sill; break"
    bind $w o "winset $w; press oill; break"
    bind $w q "winset $w; press reject; break"
    bind $w A "winset $w; press accept; break"
    bind $w P "winset $w; sed_apply; break"
    bind $w S "winset $w; sed_reset; break"
    bind $w u "winset $w; svb; break"
    bind $w <F1> "winset $w; dm set depthcue !; update_gui $w depthcue \[dm set depthcue\]; break"
    bind $w <F2> "winset $w; dm set zclip !; update_gui $w zclip \[dm set zclip\]; break"
    bind $w <F3> "winset $w; set perspective_mode !; update_gui $w perspective_mode \$perspective_mode; break"
    bind $w <F4> "winset $w; dm set zbuffer !; update_gui $w zbuffer \[dm set zbuffer\]; break"
    bind $w <F5> "winset $w; dm set lighting !; update_gui $w lighting \[dm set lighting\]; break"
    bind $w <F6> "winset $w; set toggle_perspective !; break"
    bind $w <F7> "winset $w; set faceplate !; update_gui $w faceplate \$faceplate; break"
    bind $w <F8> "winset $w; set orig_gui !; update_gui $w orig_gui \$orig_gui; break"
# KeySym for <F9> --> 0xffc6 --> 65478
    bind $w <F9> "toggle_forward_key_bindings $w; update_gui $w forward_keys \$forwarding_key($w); break"
    bind $w <F12> "winset $w; knob zero; break"

    bind $w <Left> "winset $w; knob -i ay -\$mged_default(rot_factor); break"
    bind $w <Right> "winset $w; knob -i ay \$mged_default(rot_factor); break"
    bind $w <Down> "winset $w; knob -i ax \$mged_default(rot_factor); break"
    bind $w <Up> "winset $w; knob -i ax -\$mged_default(rot_factor); break"
    bind $w <Shift-Left> "winset $w; knob -i aX \$mged_default(tran_factor); break"
    bind $w <Shift-Right> "winset $w; knob -i aX -\$mged_default(tran_factor); break"
    bind $w <Shift-Down> "winset $w; knob -i aZ -\$mged_default(tran_factor); break"
    bind $w <Shift-Up> "winset $w; knob -i aZ \$mged_default(tran_factor); break"
    bind $w <Control-Shift-Left> "winset $w; knob -i az \$mged_default(rot_factor); break"
    bind $w <Control-Shift-Right> "winset $w; knob -i az -\$mged_default(rot_factor); break"
    bind $w <Control-Shift-Down> "winset $w; knob -i aY \$mged_default(tran_factor); break"
    bind $w <Control-Shift-Up> "winset $w; knob -i aY -\$mged_default(tran_factor); break"

    bind $w <Control-n> "winset $w; _mged_view_ring next; break"
    bind $w <Control-p> "winset $w; _mged_view_ring prev; break"
    bind $w <Control-t> "winset $w; _mged_view_ring toggle; break"

    bind $w <Escape> "winset $w; press reject ; break"

    # Throw away other key events
    bind $w <KeyPress> {
	break
    }
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
    bind $w 0 {}
    bind $w x {}
    bind $w y {}
    bind $w z {}
    bind $w X {}
    bind $w Y {}
    bind $w Z {}
    bind $w 3 {}
    bind $w 4 {}
    bind $w f {}
    bind $w t {}
    bind $w b {}
    bind $w l {}
    bind $w r {}
    bind $w R {}
    bind $w s {}
    bind $w o {}
    bind $w q {}
    bind $w u {}
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
    global transform

    # default button bindings
    bind $w <1> "winset $w; focus $w; zoom 0.5; break"
    bind $w <2> "winset $w; focus $w; set tmpstr \[dm m %x %y\]; print_return_val \$tmpstr; break"
    bind $w <3> "winset $w; focus $w; zoom 2.0; break"

    bind $w <ButtonRelease> "winset $w; dm idle; break"
    bind $w <KeyRelease-Control_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Control_R> "winset $w; dm idle; break"
    bind $w <KeyRelease-Shift_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Shift_R> "winset $w; dm idle; break"
    bind $w <KeyRelease-Alt_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Alt_R> "winset $w; dm idle; break"

    if ![catch {adc draw} result] {
	set adcflag $result
    } else {
	set adcflag 0
    }

    if {$adcflag == "1" && $transform == "a"} {
	bind $w <Shift-ButtonPress-1> "winset $w; dm adc t %x %y; \
		shift_grip_hints $w \"Translate ADC\"; break"
	bind $w <Shift-ButtonPress-2> "winset $w; dm adc t %x %y; \
		shift_grip_hints $w \"Translate ADC\"; break"
	bind $w <Shift-ButtonPress-3> "winset $w; dm adc d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"

	bind $w <Control-ButtonPress-1> "winset $w; dm adc 1 %x %y; \
		shift_grip_hints $w \"Rotate Angle 1\"; break"
	bind $w <Control-ButtonPress-2> "winset $w; dm adc 2 %x %y; \
		shift_grip_hints $w \"Rotate Angle 2\"; break"
	bind $w <Control-ButtonPress-3> "winset $w; dm adc d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm adc d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm adc d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm adc d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"

#constrained adc defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con a x %x %y; \
		shift_grip_hints $w \"X Translate ADC\"; break"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con a y %x %y; \
		shift_grip_hints $w \"Y Translate ADC\"; break"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con a d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con a 1 %x %y; \
		shift_grip_hints $w \"Rotate Angle 1\"; break"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con a 2 %x %y; \
		shift_grip_hints $w \"Rotate Angle 2\"; break"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con a d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con a d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con a d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con a d %x %y; \
		shift_grip_hints $w \"Translate Tick Distance\"; break"
    } else {
	bind $w <Shift-ButtonPress-1> "winset $w; dm am t %x %y; \
		shift_grip_hints $w Translate; break"
	bind $w <Shift-ButtonPress-2> "winset $w; dm am t %x %y; \
		shift_grip_hints $w Translate; break"
	bind $w <Shift-ButtonPress-3> "winset $w; dm am t %x %y; \
		shift_grip_hints $w Translate; break"

	bind $w <Control-ButtonPress-1> "winset $w; dm am r %x %y; \
		shift_grip_hints $w Rotate; break"
	bind $w <Control-ButtonPress-2> "winset $w; dm am r %x %y; \
		shift_grip_hints $w Rotate; break"
	bind $w <Control-ButtonPress-3> "winset $w; dm am r %x %y; \
		shift_grip_hints $w Rotate; break"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm am s %x %y; \
		shift_grip_hints $w Scale/Zoom; break"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm am s %x %y; \
		shift_grip_hints $w Scale/Zoom; break"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm am s %x %y; \
		shift_grip_hints $w Scale/Zoom; break"

#constrained defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con t x %x %y; \
		shift_grip_hints $w \"X Translation\"; break"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con t y %x %y; \
		shift_grip_hints $w \"Y Translation\"; break"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con t z %x %y; \
		shift_grip_hints $w \"Z Translation\"; break"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con r x %x %y; \
		shift_grip_hints $w \"X Rotation\"; break"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con r y %x %y; \
		shift_grip_hints $w \"Y Rotation\"; break"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con r z %x %y; \
		shift_grip_hints $w \"Z Rotation\"; break"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con s x %x %y; \
		scale_shift_grip_hints $w X; break"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con s y %x %y; \
		scale_shift_grip_hints $w Y; break"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con s z %x %y; \
		scale_shift_grip_hints $w Z; break"
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
    if {[status state] == "OBJ EDIT"} {
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
