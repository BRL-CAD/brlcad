set mged_rotate_factor 1
set mged_tran_factor .01

proc mged_bind_dm { w } {
    global hot_key
    global mged_rotate_factor
    global mged_tran_factor
    global slidersflag
    global forwarding_key

    set hot_key 65478

#make this the current display manager
    bind $w <Enter> "winset $w; focus $w;"

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

proc default_key_bindings { w } {
    bind $w a "winset $w; adc; break"
    bind $w e "update_axes_draw $w edit; break"
    bind $w m "update_axes_draw $w model; break"
    bind $w v "update_axes_draw $w view; break"
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
    bind $w u "winset $w; svb; break"
    bind $w <F1> "winset $w; dm set depthcue !; break"
    bind $w <F2> "winset $w; dm set zclip !; break"
    bind $w <F3> "winset $w; set perspective_mode !; break"
    bind $w <F4> "winset $w; dm set zbuffer !; break"
    bind $w <F5> "winset $w; dm set lighting !; break"
    bind $w <F6> "winset $w; set toggle_perspective !; break"
    bind $w <F7> "winset $w; set faceplate !; break"
    bind $w <F8> "winset $w; set orig_gui !; break"
# KeySym for <F9> --> 0xffc6 --> 65478
    bind $w <F9> "toggle_forward_key_bindings $w; break"
    bind $w <F12> "winset $w; knob zero; break"

    bind $w <Left> "winset $w; knob -i ay -\$mged_rotate_factor; break"
    bind $w <Right> "winset $w; knob -i ay \$mged_rotate_factor; break"
    bind $w <Down> "winset $w; knob -i ax \$mged_rotate_factor; break"
    bind $w <Up> "winset $w; knob -i ax -\$mged_rotate_factor; break"
    bind $w <Shift-Left> "winset $w; knob -i aX \$mged_tran_factor; break"
    bind $w <Shift-Right> "winset $w; knob -i aX -\$mged_tran_factor; break"
    bind $w <Shift-Down> "winset $w; knob -i aZ -\$mged_tran_factor; break"
    bind $w <Shift-Up> "winset $w; knob -i aZ \$mged_tran_factor; break"
    bind $w <Control-Shift-Left> "winset $w; knob -i az \$mged_rotate_factor; break"
    bind $w <Control-Shift-Right> "winset $w; knob -i az -\$mged_rotate_factor; break"
    bind $w <Control-Shift-Down> "winset $w; knob -i aY \$mged_tran_factor; break"
    bind $w <Control-Shift-Up> "winset $w; knob -i aY -\$mged_tran_factor; break"

    bind $w <Control-n> "winset $w; _mged_view_ring next; break"
    bind $w <Control-p> "winset $w; _mged_view_ring prev; break"
    bind $w <Control-t> "winset $w; _mged_view_ring toggle; break"

    # Throw away other key events
    bind $w <KeyPress> {
	break
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
	    set dm_insert_char_flag(.$id.t) 1;\
	    event generate .$id.t <KeyPress> -state %s -keysym %K;\
	    set dm_insert_char_flag(.$id.t) 0;\
	    focus %W; break"
}

proc default_mouse_bindings { w } {
    global transform

# default button bindings
    bind $w <1> "winset $w; zoom 0.5; break"
    bind $w <2> "winset $w; set tmpstr \[dm m %x %y\]; print_return_val \$tmpstr; break"
    bind $w <3> "winset $w; zoom 2.0; break"

    bind $w <ButtonRelease> "winset $w; dm idle; break"
    bind $w <KeyRelease-Control_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Control_R> "winset $w; dm idle; break"
    bind $w <KeyRelease-Shift_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Shift_R> "winset $w; dm idle; break"
    bind $w <KeyRelease-Alt_L> "winset $w; dm idle; break"
    bind $w <KeyRelease-Alt_R> "winset $w; dm idle; break"

    set adcflag [adc draw]

    if {$adcflag == "1" && $transform == "a"} {
	bind $w <Shift-ButtonPress-1> "winset $w; dm adc t %x %y; break"
	bind $w <Shift-ButtonPress-2> "winset $w; dm adc t %x %y; break"
	bind $w <Shift-ButtonPress-3> "winset $w; dm adc d %x %y; break"

	bind $w <Control-ButtonPress-1> "winset $w; dm adc 1 %x %y; break"
	bind $w <Control-ButtonPress-2> "winset $w; dm adc 2 %x %y; break"
	bind $w <Control-ButtonPress-3> "winset $w; dm adc d %x %y; break"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm adc d %x %y; break"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm adc d %x %y; break"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm adc d %x %y; break"

#constrained adc defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con a x %x %y; break"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con a y %x %y; break"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con a d %x %y; break"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con a 1 %x %y; break"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con a 2 %x %y; break"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con a d %x %y; break"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con a d %x %y; break"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con a d %x %y; break"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con a d %x %y; break"
    } else {
	bind $w <Shift-ButtonPress-1> "winset $w; dm am t %x %y; break"
	bind $w <Shift-ButtonPress-2> "winset $w; dm am t %x %y; break"
	bind $w <Shift-ButtonPress-3> "winset $w; dm am t %x %y; break"

	bind $w <Control-ButtonPress-1> "winset $w; dm am r %x %y; break"
	bind $w <Control-ButtonPress-2> "winset $w; dm am r %x %y; break"
	bind $w <Control-ButtonPress-3> "winset $w; dm am r %x %y; break"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm am s %x %y; break"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm am s %x %y; break"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm am s %x %y; break"

#constrained defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con t x %x %y; break"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con t y %x %y; break"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con t z %x %y; break"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con r x %x %y; break"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con r y %x %y; break"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con r z %x %y; break"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con s x %x %y; break"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con s y %x %y; break"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con s z %x %y; break"
    }   
}

proc update_axes_draw { w type } {
    global mged_players
    global mged_active_dm
    global mged_axes

    winset $w;
    rset ax $type\_draw !

    foreach id $mged_players {
	if {$mged_active_dm($id) == $w} {
	    set mged_axes($id,$type\_draw) [rset ax $type\_draw]
	    break
	}
    }
}
