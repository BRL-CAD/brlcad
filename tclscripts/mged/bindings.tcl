set mged_rotate_factor 1
set mged_tran_factor .01

proc mged_bind_dm { w } {
    global hot_key
    global mged_rotate_factor
    global mged_tran_factor
    global slidersflag

    set hot_key 65478

#make this the current display manager
    bind $w <Enter> "winset $w; focus $w;"

#default mouse bindings
    do_mouse_bindings $w

#default key bindings
    bind $w a "winset $w; adc"
    bind $w e "winset $w; set e_axes !"
    bind $w v "winset $w; set v_axes !"
    bind $w w "winset $w; set m_axes !"
    bind $w i "winset $w; aip f"
    bind $w I "winset $w; aip b"
    bind $w p "winset $w; M 1 0 0"
    bind $w 0 "winset $w; knob zero"
    bind $w x "winset $w; knob -i x 0.3"
    bind $w y "winset $w; knob -i y 0.3"
    bind $w z "winset $w; knob -i z 0.3"
    bind $w X "winset $w; knob -i x -0.3"
    bind $w Y "winset $w; knob -i y -0.3"
    bind $w Z "winset $w; knob -i z -0.3"
    bind $w 3 "winset $w; press 35,25"
    bind $w 4 "winset $w; press 45,45"
    bind $w f "winset $w; press front"
    bind $w t "winset $w; press top"
    bind $w b "winset $w; press bottom"
    bind $w l "winset $w; press left"
    bind $w r "winset $w; press right"
    bind $w R "winset $w; press rear"
    bind $w s "winset $w; press sill"
    bind $w o "winset $w; press oill"
    bind $w q "winset $w; press reject"
    bind $w u "winset $w; svb"
    bind $w <F1> "winset $w; dm set depthcue !"
    bind $w <F2> "winset $w; dm set zclip !"
    bind $w <F3> "winset $w; dm set perspective !"
    bind $w <F4> "winset $w; dm set zbuffer !"
    bind $w <F5> "winset $w; dm set lighting !"
    bind $w <F6> "winset $w; dm set set_perspective !"
    bind $w <F7> "winset $w; set faceplate !"
    bind $w <F8> "winset $w; set orig_gui !"
# KeySym for <F9> --> 0xffc6 --> 65478
    bind $w <F9> "winset $w; set send_key !"
    bind $w <F12> "winset $w; knob zero"

    bind $w <Left> "winset $w; knob -i ay -\$mged_rotate_factor"
    bind $w <Right> "winset $w; knob -i ay \$mged_rotate_factor"
    bind $w <Down> "winset $w; knob -i ax \$mged_rotate_factor"
    bind $w <Up> "winset $w; knob -i ax -\$mged_rotate_factor"
    bind $w <Shift-Left> "winset $w; knob -i aX \$mged_tran_factor"
    bind $w <Shift-Right> "winset $w; knob -i aX -\$mged_tran_factor"
    bind $w <Shift-Down> "winset $w; knob -i aZ -\$mged_tran_factor"
    bind $w <Shift-Up> "winset $w; knob -i aZ \$mged_tran_factor"
    bind $w <Control-Shift-Left> "winset $w; knob -i az \$mged_rotate_factor"
    bind $w <Control-Shift-Right> "winset $w; knob -i az -\$mged_rotate_factor"
    bind $w <Control-Shift-Down> "winset $w; knob -i aY \$mged_tran_factor"
    bind $w <Control-Shift-Up> "winset $w; knob -i aY -\$mged_tran_factor"

    bind $w <Control-n> "winset $w; next_view"
    bind $w <Control-p> "winset $w; prev_view"
    bind $w <Control-t> "winset $w; toggle_view"
}

proc print_return_val str {
    if {$str != ""} {
	distribute_text "" "" $str
	stuff_str $str
    }
}


proc do_mouse_bindings { w } {
    global adcflag
    global transform

# default button bindings
    bind $w <1> "winset $w; zoom 0.5"
    bind $w <2> "winset $w; set tmpstr \[dm m %b %x %y\]; print_return_val \$tmpstr"
    bind $w <3> "winset $w; zoom 2.0"

    bind $w <ButtonRelease> "winset $w; dm idle"
    bind $w <KeyRelease-Control_L> "winset $w; dm idle"
    bind $w <KeyRelease-Control_R> "winset $w; dm idle"
    bind $w <KeyRelease-Shift_L> "winset $w; dm idle"
    bind $w <KeyRelease-Shift_R> "winset $w; dm idle"
    bind $w <KeyRelease-Alt_L> "winset $w; dm idle"
    bind $w <KeyRelease-Alt_R> "winset $w; dm idle"

    if {$adcflag && $transform == "a"} {
	bind $w <Shift-ButtonPress-1> "winset $w; dm adc t %x %y"
	bind $w <Shift-ButtonPress-2> "winset $w; dm adc t %x %y"
	bind $w <Shift-ButtonPress-3> "winset $w; dm adc d %x %y"

	bind $w <Control-ButtonPress-1> "winset $w; dm adc 1 %x %y"
	bind $w <Control-ButtonPress-2> "winset $w; dm adc 2 %x %y"
	bind $w <Control-ButtonPress-3> "winset $w; dm adc d %x %y"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm adc d %x %y"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm adc d %x %y"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm adc d %x %y"

#constrained adc defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con a x %x %y"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con a y %x %y"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con a d %x %y"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con a 1 %x %y"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con a 2 %x %y"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con a d %x %y"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con a d %x %y"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con a d %x %y"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con a d %x %y"
    } else {
	bind $w <Shift-ButtonPress-1> "winset $w; dm am t %x %y"
	bind $w <Shift-ButtonPress-2> "winset $w; dm am t %x %y"
	bind $w <Shift-ButtonPress-3> "winset $w; dm am t %x %y"

	bind $w <Control-ButtonPress-1> "winset $w; dm am r %x %y"
	bind $w <Control-ButtonPress-2> "winset $w; dm am r %x %y"
	bind $w <Control-ButtonPress-3> "winset $w; dm am r %x %y"

	bind $w <Shift-Control-ButtonPress-1> "winset $w; dm am s %x %y"
	bind $w <Shift-Control-ButtonPress-2> "winset $w; dm am s %x %y"
	bind $w <Shift-Control-ButtonPress-3> "winset $w; dm am s %x %y"

#constrained defaults
	bind $w <Alt-Shift-ButtonPress-1> "winset $w; dm con t x %x %y"
	bind $w <Alt-Shift-ButtonPress-2> "winset $w; dm con t y %x %y"
	bind $w <Alt-Shift-ButtonPress-3> "winset $w; dm con t z %x %y"

	bind $w <Alt-Control-ButtonPress-1> "winset $w; dm con r x %x %y"
	bind $w <Alt-Control-ButtonPress-2> "winset $w; dm con r y %x %y"
	bind $w <Alt-Control-ButtonPress-3> "winset $w; dm con r z %x %y"

	bind $w <Alt-Shift-Control-ButtonPress-1> "winset $w; dm con s x %x %y"
	bind $w <Alt-Shift-Control-ButtonPress-2> "winset $w; dm con s y %x %y"
	bind $w <Alt-Shift-Control-ButtonPress-3> "winset $w; dm con s z %x %y"
    }   
}