# these routines implement the gui for the BRL-CAD shaders
# shader_params is a global array containing all the values for this shader gui
# the 'id' is passed to these routines to use for uniqueness
# the top-level interface is 'do_shader'

# To implement a new shader gui:
#	1. add the new shader to the 'switch' in 'do_shader_apply'
#	2. add the new shader to the 'switch' in 'do_shader'
#	3. add the new shader to the 'switch' in 'set_shader_params'
#	4. add the new shader to the switch command in 'stack_add', 'stack_insert', and 'env_select', if this shader is
#		appropriate for being in a stack or environment map
#	5. add the new shader to the menus in 'do_stack' and 'do_envmap'
#	6. add a menu item for the new shader in 'init_comb' (comb.tcl)
#	7. add the new shader to the list of shader names in the 'is_good_shader' proc
#	8. add the following routines:

# proc do_newshader { shade_var id } - Creates the frame to hold the shader widgets and
#	creates the labels, entries, buttons... Also registers 'help-in-context' data
#	calls 'set_newshader_values' to set initial settings of widgets
#	returns the created frame name.

# proc set_newshader_values { shader_str id }
#	sets the shader widget values according to the passed in shader string

# proc do_newshader_apply { shade_var id }
#	builds a shader string from the widgets and places the finished string
#	in a level 0 variable, whose name gets passed in

# proc set_newshader_defaults { id }
#	This routine sets the default values for this shader





proc vec_compare { v1 v2 n } {
	for { set i 0 } { $i < $n } { incr i } {
		if { [expr [lindex $v1 $i] != [lindex $v2 $i]] } then {
			return 1
		}
	}
	return 0
}

# camouflage routines

proc color_trigger { shade_var id name1 name2 op } {
	do_shader_apply $shade_var $id
}

proc do_camo {  shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	trace vdelete shader_params($id,c1) w "color_trigger $shade_var $id"
	trace vdelete shader_params($id,c2) w "color_trigger $shade_var $id"
	trace vdelete shader_params($id,c3) w "color_trigger $shade_var $id"

	set shader_params($id,c1) ""
	set shader_params($id,c2) ""
	set shader_params($id,c3) ""

	label $shader_params($id,window).fr.lacun -text Lacunarity
	entry $shader_params($id,window).fr.lacun_e -width 5 -textvariable shader_params($id,lacun)
	bind $shader_params($id,window).fr.lacun_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.lacun "Lacunarity" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The grid on which the noise function is built is scaled by this value\n\
			for each successive octave of noise which will be combined to produce the\n\
			final result. (default is 2.1753974)"}
		{range "Must be greater than zero"}
	}
	hoc_register_data $shader_params($id,window).fr.lacun_e "Lacunarity" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The grid on which the noise function is built is scaled by this value\n\
			for each successive octave of noise which will be combined to produce the\n\
			final result. (default is 2.1753974)"}
		{range "Must be greater than zero"}
	}

	label $shader_params($id,window).fr.h -text "H value"
	entry $shader_params($id,window).fr.h_e -width 5 -textvariable shader_params($id,hval)
	bind $shader_params($id,window).fr.h_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.h "H Value" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The value for H (default is 1.0)"}
		{range "??"}
	}
	hoc_register_data $shader_params($id,window).fr.h_e "H Value" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The value for H (default is 1.0)"}
		{range "??"}
	}

	label $shader_params($id,window).fr.octaves -text "Octaves"
	entry $shader_params($id,window).fr.octaves_e -width 5 -textvariable shader_params($id,octaves)
	bind $shader_params($id,window).fr.octaves_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.octaves "Octaves" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The number of times the noise grid will be scaled\n\
			and recombined to produce the final noise function."}
		{range "Must be greater than 0 (default is  4.0)"}
	}
	hoc_register_data $shader_params($id,window).fr.octaves_e "Octaves" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "The number of times the noise grid will be scaled\n\
			and recombined to produce the final noise function."}
		{range "Must be greater than 0 (default is  4.0)"}
	}

	label $shader_params($id,window).fr.size -text "Noise Size"
	entry $shader_params($id,window).fr.size_e -width 5 -textvariable shader_params($id,size)
	bind $shader_params($id,window).fr.size_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.size "Noise Size" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "A larger number here produces larger color patches (default is 1.0)"}
		{range "must be greater than zero"}
	}
	hoc_register_data $shader_params($id,window).fr.size_e "Noise Size" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "A larger number here produces larger color patches (default is 1.0)"}
		{range "must be greater than zero"}
	}

	label $shader_params($id,window).fr.scale -text "Noise Scale (X, Y, Z)"
	entry $shader_params($id,window).fr.scale_e -width 20 -textvariable shader_params($id,scale)
	bind $shader_params($id,window).fr.scale_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.scale "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Three scale values are required (X, Y, and Z)."}
		{range "Must be greater than 0 Default values are (1.0 1.0 1.0)"}
	}
	hoc_register_data $shader_params($id,window).fr.scale_e "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Three scale values are required (X, Y, and Z)."}
		{range "Must be greater than 0 Default values are (1.0 1.0 1.0)"}
	}

	label $shader_params($id,window).fr.c1 -text "Color #1"
	frame $shader_params($id,window).fr.c1_e
	color_entry_build $shader_params($id,window).fr.c1_e color shader_params($id,c1)\
		"color_entry_chooser $id $shader_params($id,window).fr.c1_e color \"Color #1\"\
		 shader_params $id,c1"\
		12 $shader_params($id,c1)
	hoc_register_data $shader_params($id,window).fr.c1 "Color #1" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "This is one of three colors used in the camouflage pattern\n\
			(default color is 97/74/41)"}
	}

	label $shader_params($id,window).fr.c2 -text "Background Color"
	frame $shader_params($id,window).fr.c2_e
	color_entry_build $shader_params($id,window).fr.c2_e color shader_params($id,c2)\
		"color_entry_chooser $id $shader_params($id,window).fr.c2_e color \"Background Color\"\
		 shader_params $id,c2"\
		12 $shader_params($id,c2)
	hoc_register_data $shader_params($id,window).fr.c2 "Background Color" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "This is the background color used in the camouflage pattern\n\
			(default color is 26/77/10)"}
	}

	label $shader_params($id,window).fr.c3 -text "Color #2"
	frame $shader_params($id,window).fr.c3_e
	color_entry_build $shader_params($id,window).fr.c3_e color shader_params($id,c3)\
		"color_entry_chooser $id $shader_params($id,window).fr.c3_e color \"Color #2\"\
		 shader_params $id,c3"\
		12 $shader_params($id,c3)
	hoc_register_data $shader_params($id,window).fr.c3 "Color #2" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "This is one of three colors used in the camouflage pattern\n\
			(default color is 38/38/38)"}
	}

	label $shader_params($id,window).fr.t1 -text "Threshold #1"
	entry $shader_params($id,window).fr.t1_e -width 5 -textvariable  shader_params($id,t1)
	bind $shader_params($id,window).fr.t1_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.t1 "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Areas where the noise function returns a value less than this threshold\n\
			will be colored using color #1"}
		{range " -1.0 to +1.0 (default is -0.25)"}
	}
	hoc_register_data $shader_params($id,window).fr.t1_e "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Areas where the noise function returns a value less than this threshold\n\
			will be colored using color #1"}
		{range " -1.0 to +1.0 (default is -0.25)"}
	}

	label $shader_params($id,window).fr.t2 -text "Threshold #2"
	entry $shader_params($id,window).fr.t2_e -width 5 -textvariable  shader_params($id,t2)
	bind $shader_params($id,window).fr.t2_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.t2 "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Areas where the noise function returns a value greater than this threshold\n\
			will be colored using color #2 (default is 0.25)"}
		{range " -1.0 to +1.0"}
	}
	hoc_register_data $shader_params($id,window).fr.t2_e "Noise Scale" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "Areas where the noise function returns a value greater than this threshold\n\
			will be colored using color #2 (default is 0.25)"}
		{range " -1.0 to +1.0"}
	}

	label $shader_params($id,window).fr.delta -text "Noise Delta (X, Y, Z)"
	entry $shader_params($id,window).fr.delta_e -width 20  -textvariable shader_params($id,delta)
	bind $shader_params($id,window).fr.delta_e <KeyRelease> "do_shader_apply $shade_var $id"
	hoc_register_data $shader_params($id,window).fr.delta "Noise Delta" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "These values provide a delta into the noise space.\n\
			Default values are (1000.0 1000.0 1000.0)" }
	}
	hoc_register_data $shader_params($id,window).fr.delta_e "Noise Delta" {
		{summary "The 'camo' shader creates a pseudo-random tri-color camoflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
		{description "These values provide a delta into the noise space.\n\
			Default values are (1000.0 1000.0 1000.0)" }
	}

	set_camo_values $shader_str $id

	trace variable shader_params($id,c1) w "color_trigger $shade_var $id"
	trace variable shader_params($id,c2) w "color_trigger $shade_var $id"
	trace variable shader_params($id,c3) w "color_trigger $shade_var $id"

	grid $shader_params($id,window).fr.c2 -row 0 -column 0 -columnspan 2 -sticky e
	grid $shader_params($id,window).fr.c2_e.colorF
	grid $shader_params($id,window).fr.c2_e -row 0 -column 2 -columnspan 2 -sticky w
	grid $shader_params($id,window).fr.c1 -row 1 -column 0 -columnspan 2 -sticky e
	grid $shader_params($id,window).fr.c1_e.colorF
	grid $shader_params($id,window).fr.c1_e -row 1 -column 2 -columnspan 2 -sticky w
	grid $shader_params($id,window).fr.c3 -row 2 -column 0 -columnspan 2 -sticky e
	grid $shader_params($id,window).fr.c3_e.colorF
	grid $shader_params($id,window).fr.c3_e -row 2 -column 2 -columnspan 2 -sticky w
	grid $shader_params($id,window).fr.t1 -row 3 -column 0 -sticky e
	grid $shader_params($id,window).fr.t1_e -row 3 -column 1 -sticky w
	grid $shader_params($id,window).fr.t2 -row 3 -column 2 -sticky e
	grid $shader_params($id,window).fr.t2_e -row 3 -column 3 -sticky w
	grid $shader_params($id,window).fr.lacun -row 4 -column 0 -sticky e
	grid $shader_params($id,window).fr.lacun_e -row 4 -column 1 -sticky w
	grid $shader_params($id,window).fr.h -row 4 -column 2 -sticky e
	grid $shader_params($id,window).fr.h_e -row 4 -column 3 -sticky w
	grid $shader_params($id,window).fr.size -row 5 -column 0 -sticky e
	grid $shader_params($id,window).fr.size_e -row 5 -column 1 -sticky w
	grid $shader_params($id,window).fr.octaves -row 5 -column 2 -sticky e
	grid $shader_params($id,window).fr.octaves_e -row 5 -column 3 -sticky w
	grid $shader_params($id,window).fr.scale -row 6 -column 0 -columnspan 2 -sticky e
	grid $shader_params($id,window).fr.scale_e -row 6 -column 2 -columnspan 2 -sticky w
	grid $shader_params($id,window).fr.delta -row 7 -column 0  -columnspan 2 -sticky e
	grid $shader_params($id,window).fr.delta_e -row 7 -column 2  -columnspan 2 -sticky w

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

	return $shader_params($id,window).fr
}

proc set_camo_values { shader_str id } {
	global shader_params

	# temporarily remove traces on the colors
	set tr1_info [trace vinfo shader_params($id,c1)]
	set tr1_len [llength $tr1_info]
	for {set index 0} {$index < $tr1_len } {incr index } {
		set atrace [lindex $tr1_info $index]
		trace vdelete shader_params($id,c1) [lindex $atrace 0] [lindex $atrace 1]
	}
	set tr2_info [trace vinfo shader_params($id,c2)]
	set tr2_len [llength $tr2_info]
	for {set index 0} {$index < $tr2_len } {incr index } {
		set atrace [lindex $tr2_info $index]
		trace vdelete shader_params($id,c2) [lindex $atrace 0] [lindex $atrace 1]
	}
	set tr3_info [trace vinfo shader_params($id,c3)]
	set tr3_len [llength $tr3_info]
	for {set index 0} {$index < $tr3_len } {incr index } {
		set atrace [lindex $tr3_info $index]
		trace vdelete shader_params($id,c3) [lindex $atrace 0] [lindex $atrace 1]
	}

	set shader_params($id,c1) ""
	set shader_params($id,c2) ""
	set shader_params($id,c3) ""
	set shader_params($id,lacun) ""
	set shader_params($id,hval) ""
	set shader_params($id,octaves) ""
	set shader_params($id,size) ""
	set shader_params($id,scale) ""
	set shader_params($id,t1) ""
	set shader_params($id,t2) ""
	set shader_params($id,delta) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set list_len 0}
	if { $list_len > 0 } then {
		 for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				lacunarity -
				l { catch {
					if { $value != $shader_params($id,def_lacun) } then {
						set shader_params($id,lacun) $value
					}
				   }  }
				H { catch {
					if { $value != $shader_params($id,def_hval) } then {
						set shader_params($id,hval) $value
					}
				  }  }
				octaves -
				o { catch {
					if { $value != $shader_params($id,def_octaves) } then  {
						set shader_params($id,octaves) $value
					}
				  }  }
				t1 { catch {
					if { $value != $shader_params($id,def_t1) } then  {
						set shader_params($id,t1) $value
					}
				  }  }
				t2 { catch {
					if { $value != $shader_params($id,def_t2) } then  {
						set shader_params($id,t2) $value
					}
				  }  }
				size -
				s { catch {
					if { $value != $shader_params($id,def_size) } then  {
						set shader_params($id,size) $value
					}
				  }  }
				vscale -
				vs -
				v { catch {
					if { [vec_compare $value $shader_params($id,def_scale) 3] } then  {
						set shader_params($id,scale) $value
					}
				  }  }
				c1 { catch {
					if { [vec_compare $value $shader_params($id,def_c1) 3] } then  {
						set shader_params($id,c1) $value
					}
				  }  }
				c2 { catch {
					if { [vec_compare $value $shader_params($id,def_c2) 3] } then  {
						set shader_params($id,c2) $value
					}
				  }  }
				c3 { catch {
					if { $value != $shader_params($id,def_c3) } then  {
						set shader_params($id,c3) $value
					}
				  }  }
				delta -
				d { catch {
					if { [vec_compare $value $shader_params($id,def_delta) 3] } then {
						set shader_params($id,delta) $value
					}
				  }   }
			}
		}
	}

	# reinstate the traces on the colors
	for {set index 0} {$index < $tr1_len } {incr index } {
		set atrace [lindex $tr1_info $index]
		trace variable shader_params($id,c1) [lindex $atrace 0] [lindex $atrace 1]
	}
	for {set index 0} {$index < $tr2_len } {incr index } {
		set atrace [lindex $tr2_info $index]
		trace variable shader_params($id,c2) [lindex $atrace 0] [lindex $atrace 1]
	}
	for {set index 0} {$index < $tr3_len } {incr index } {
		set atrace [lindex $tr3_info $index]
		trace variable shader_params($id,c3) [lindex $atrace 0] [lindex $atrace 1]
	}
}

proc do_camo_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

	set params ""

	if { [string length $shader_params($id,lacun) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,lacun) != $shader_params($id,def_lacun)] } then {
				lappend params l $shader_params($id,lacun)
			}
		}
	}

	if { [string length $shader_params($id,hval) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,hval) != $shader_params($id,def_hval)] } then {
				lappend params H $shader_params($id,hval)
			}
		}
	}

	if { [string length $shader_params($id,octaves) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,octaves) != $shader_params($id,def_octaves)] } then {
				lappend params o $shader_params($id,octaves)
			}
		}
	}

	if { [string length $shader_params($id,t1) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,t1) != $shader_params($id,def_t1)] } then {
				lappend params t1 $shader_params($id,t1)
			}
		}
	}

	if { [string length $shader_params($id,t2) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,t2) != $shader_params($id,def_t2)] } then {
				lappend params t2 $shader_params($id,t2)
			}
		}
	}

	if { [string length $shader_params($id,size) ] > 0 } then {
		catch {
			if { [expr $shader_params($id,size) != $shader_params($id,def_size)] } then {
				lappend params s $shader_params($id,size)
			}
		}
	}

	if { [string length $shader_params($id,scale) ] > 0 } then {
		catch {
			if { [vec_compare $shader_params($id,scale) $shader_params($id,def_scale) 3] } then {
				lappend params v $shader_params($id,scale)
			}
		}
	}

	if { [string length $shader_params($id,c1) ] > 0 } then {
		catch {
			if { [vec_compare $shader_params($id,c1) $shader_params($id,def_c1) 3] } then {
				lappend params c1 $shader_params($id,c1)
			}
		}
	}

	if { [string length $shader_params($id,c2) ] > 0 } then {
		catch {
			if { [vec_compare $shader_params($id,c2) $shader_params($id,def_c2) 3] } then {
				lappend params c2 $shader_params($id,c2)
			}
		}
	}

	if { [string length $shader_params($id,c3) ] > 0 } then {
		catch {
			if { [vec_compare $shader_params($id,c3) $shader_params($id,def_c3) 3] } then {
				lappend params c3 $shader_params($id,c3)
			}
		}
	}

	if { [string length $shader_params($id,delta) ] > 0 } then {
		catch {
			if { [vec_compare $shader_params($id,delta) $shader_params($id,def_delta) 3] } then {
				lappend params d $shader_params($id,delta)
			}
		}
	}

	set shade_str [list camo $params]
}

proc set_camo_defaults { id } {
	global shader_params

	set shader_params($id,def_lacun) 2.1753974
	set shader_params($id,def_hval) 1.0
	set shader_params($id,def_octaves) 4.0
	set shader_params($id,def_size) 1.0
	set shader_params($id,def_scale) { 1.0 1.0 1.0 }
	set shader_params($id,def_c1) { 97  74 41 }
	set shader_params($id,def_c2) { 26 77 10 }
	set shader_params($id,def_c3) { 38 38 38 }
	set shader_params($id,def_t1) -0.25
	set shader_params($id,def_t2) 0.25
	set shader_params($id,def_delta) { 1000.0 1000.0 1000.0 }
}

# Projection routines
proc do_prj {  shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.fname -text "Parameter File"
	entry $shader_params($id,window).fr.fname_e -width 20 -textvariable shader_params($id,fname)
	bind $shader_params($id,window).fr.fname_e <KeyRelease> "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.fname "File Name" {
		{summary "The projection shader projects one or more images on the object.\n\
			All the parameters required by the projection shader are\n\
			expected to be supplied in a single file. The MGED command\n\
			'prj_add' may be used to construct this file."}

		{description "The name of the file containing the parameters for\n\
			the projection shader"}
	}
	hoc_register_data $shader_params($id,window).fr.fname_e "File Name" {
		{summary "The projection shader projects one or more images on the object.\n\
			All the parameters required by the projection shader are\n\
			expected to be supplied in a single file. The MGED command\n\
			'prj_add' may be used to construct this file."}
		{description "The name of the file containing the parameters for\n\
			the projection shader"}
	}

	set_prj_values $shader_str $id

	grid $shader_params($id,window).fr.fname -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.fname_e -row 0 -column 1 -sticky w

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

	return $shader_params($id,window).fr
}

proc set_prj_values { shader_str id } {
	global shader_params

	set shader_params($id,fname) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set list_len 0}
	if { $list_len > 0 } then {
		set shader_params($id,fname) [lindex $params 0]
	}
}

proc do_prj_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

	set params ""

	if { [string length $shader_params($id,fname) ] > 0 } then {
		lappend params $shader_params($id,fname)
	}

	set shade_str [list prj $params]
}

proc set_prj_defaults { id } {
	global shader_params

	set shader_params($id,fname) ""
}

# FAKESTAR routines
proc do_fakestar { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.fakestar_m -text "There are no parameters to set for the fakestar texture map"
	hoc_register_data $shader_params($id,window).fr.fakestar_m "Fake Star" {
		{summary "The Fake Star texture maps an imaginary star field onto the object."}
	}

	grid $shader_params($id,window).fr.fakestar_m -sticky ew
	grid $shader_params($id,window).fr -ipadx 3 -pady 3

	grid columnconfigure $shader_params($id,window).fr.fakestar_m 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr 0 -weight 1

	return $shader_params($id,window).fr
}

proc set_fakestar_values { shader_str id } {
#	there are none
	return
}

proc do_fakestar_apply { shade_var id } {
	return
}

proc set_fakestar_defaults { id } {
	return
}

# TESTMAP routines
proc do_testmap { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.tst_m -text "There are no parameters to set for the testmap"
	hoc_register_data $shader_params($id,window).fr.tst_m "Testmap" {
		{summary "The 'testmap' shader maps red and blue colors onto the object\n\
			based on the 'uv' coordinates. The colors vary from (0 0 0) at\n\
			'uv' == (0,0) to (255 0 255) at 'uv' == (1,1)."}
	}

	grid $shader_params($id,window).fr.tst_m -sticky ew
	grid $shader_params($id,window).fr -ipadx 3 -ipady 3

	grid columnconfigure $shader_params($id,window).fr.tst_m 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr 0 -weight 1

	return $shader_params($id,window).fr
}

proc set_testmap_values { shader_str id } {
#	there are none
	return
}

proc do_testmap_apply { shade_var id } {
	return
}

proc set_testmap_defaults { id } {
	return
}

# CHECKER routines
proc do_checker { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.color1 -text "First Color"
	entry $shader_params($id,window).fr.color1_e -width 15 -textvariable shader_params($id,ckr_a)
	bind $shader_params($id,window).fr.color1_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.color2 -text "Second Color"
	entry $shader_params($id,window).fr.color2_e -width 15 -textvariable shader_params($id,ckr_b)
	bind $shader_params($id,window).fr.color2_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.scale -text "Scale"
	entry $shader_params($id,window).fr.scale_e -width 15 -textvariable shader_params($id,ckr_scale)
	bind $shader_params($id,window).fr.scale_e <KeyRelease> "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.color1_e "First Color" {
		{summary "Enter one of the colors to use in the checkerboard pattern\n\
			default here is '255 255 255'"}
		{range "An RGB triple, each value from 0 to 255"}
	}
	hoc_register_data $shader_params($id,window).fr.color2_e "Second Color" {
		{summary "Enter another color to use in the checkerboard pattern\n\
			default here is '0 0 0'"}
		{range "An RGB triple, each value from 0 to 255"}
	}
	hoc_register_data $shader_params($id,window).fr.color1 "First Color" {
		{summary "Enter one of the colors to use in the checkerboard pattern"}
		{range "An RGB triple, each value from 0 to 255"}
	}
	hoc_register_data $shader_params($id,window).fr.color2 "Second Color" {
		{summary "Enter another color to use in the checkerboard pattern"}
		{range "An RGB triple, each value from 0 to 255"}
	}
	hoc_register_data $shader_params($id,window).fr.scale "Scale" {
		{summary "Enter a scale factor to use for the checkerboard pattern\n\
			(Allows different numbers of checks)"}
		{range "Non-zero positive numbers"}
	}
	hoc_register_data $shader_params($id,window).fr.scale_e "Scale" {
		{summary "Enter a scale factor to use for the checkerboard pattern\n\
			(Allows different numbers of checks)"}
		{range "Non-zero positive numbers"}
	}

	set_checker_values $shader_str $id

	grid $shader_params($id,window).fr.color1 -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.color1_e -row 0 -column 1 -sticky w
	grid $shader_params($id,window).fr.color2 -row 1 -column 0 -sticky e
	grid $shader_params($id,window).fr.color2_e -row 1 -column 1 -sticky w
	grid $shader_params($id,window).fr.scale -row 2 -column 0 -sticky e
	grid $shader_params($id,window).fr.scale_e -row 2 -column 1 -sticky w

	grid columnconfigure $shader_params($id,window).fr.color1_e 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr.color2_e 0 -weight 1

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3
	grid columnconfigure $shader_params($id,window).fr 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr 1 -weight 1

	return $shader_params($id,window).fr
}

proc set_checker_values { shader_str id } {
	global shader_params

	set shader_params($id,ckr_a) ""
	set shader_params($id,ckr_b) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set  list_len 0}
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				a { catch {
					if { $value != $shader_params($id,def_ckr_a) } then {
						set shader_params($id,ckr_a) $value }
				    }
				}

				b { catch {
					if { $value != $shader_params($id,def_ckr_b) } then {
						set shader_params($id,ckr_b) $value }
				    }
				}
				s { catch {
					if { $value != $shader_params($id,def_ckr_scale) } then {
						set shader_params($id,ckr_scale) $value }
				    }
				}
			}
		}
	}
}

proc do_checker_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	catch {
		if { [string length $shader_params($id,ckr_a) ] > 0 } then {
			if { $shader_params($id,ckr_a) != $shader_params($id,def_ckr_a) } then {
				lappend params a $shader_params($id,ckr_a)
			}
		}
	}
	catch {
		if { [string length $shader_params($id,ckr_b) ] > 0 } then {
			if { $shader_params($id,ckr_b) != $shader_params($id,def_ckr_b) } then {
				lappend params b $shader_params($id,ckr_b)
			}
		}
	}
	catch {
		if { [string length $shader_params($id,ckr_scale) ] > 0 } then {
			if { $shader_params($id,ckr_scale) != $shader_params($id,def_ckr_scale) } then {
				lappend params s $shader_params($id,ckr_scale)
			}
		}
	}

	set shader [list checker $params ]
}

proc set_checker_defaults { id } {
	global shader_params

	set shader_params($id,def_ckr_a) [list 255 255 255]
	set shader_params($id,def_ckr_b) [list 0 0 0]
	set shader_params($id,def_ckr_scale) 2.0
}

# PHONG routines

proc do_phong { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.trans -text Transparency
	entry $shader_params($id,window).fr.trans_e -width 5 -textvariable shader_params($id,trans)
	bind $shader_params($id,window).fr.trans_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.refl -text "mirror reflectance"
	entry $shader_params($id,window).fr.refl_e -width 5 -textvariable shader_params($id,refl)
	bind $shader_params($id,window).fr.refl_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.spec -text "Specular reflectivity"
	entry $shader_params($id,window).fr.spec_e -width 5 -textvariable shader_params($id,spec)
	bind $shader_params($id,window).fr.spec_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.diff -text "Diffuse reflectivity"
	entry $shader_params($id,window).fr.diff_e -width 5 -textvariable shader_params($id,diff)
	bind $shader_params($id,window).fr.diff_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.ri -text "Refractive index"
	entry $shader_params($id,window).fr.ri_e -width 5 -textvariable shader_params($id,ri)
	bind $shader_params($id,window).fr.ri_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.shine -text Shininess
	entry $shader_params($id,window).fr.shine_e -width 5 -textvariable shader_params($id,shine)
	bind $shader_params($id,window).fr.shine_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.ext -text Extinction
	entry $shader_params($id,window).fr.ext_e -width 5 -textvariable shader_params($id,ext)
	bind $shader_params($id,window).fr.ext_e <KeyRelease> "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.trans Transparency {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "The fraction of light that will be\n\
			transmitted through this object" }
		{range "0.0 through 1.0"}
	}

	hoc_register_data $shader_params($id,window).fr.trans_e Transparency {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that\n\
			will be transmitted through this object"}
		{range "0.0 through 1.0"}
	}

	hoc_register_data $shader_params($id,window).fr.refl "Mirror Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "The fraction of light that will be reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.refl_e "Mirror Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that will be reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.spec "Specular Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that will be specularly reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.spec_e "Specular Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that will be specularly reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.diff "Diffuse Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that will be diffusely reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.diff_e "Diffuse Reflectivity" {
		{summary "The observer can see diffuse and specular reflections from\n\
			an object as well as transmitted and/or reflected images.\n\
			The transparency is the fraction of light that will be\n\
			transmitted through the object. The mirror reflectance is\n\
			the fraction of light that will be reflected as a mirror image.\n\
			The fraction of light remaining after transmission and mirror\n\
			reflection is divided between diffuse and specular reflections\n\
			The diffuse and specular reflections are what the observer uses to\n\
			perceive the color and shape of an object. Transparency plus mirror\n\
			reflectance is typically less than 1.0, and diffuse plus specular\n\
			is typically equal to 1.0."}
		{description "Enter the fraction of light that will be diffusely reflected\n\
			by the surface of this object"}
		{range "0.0 through 1.0" }
	}

	hoc_register_data $shader_params($id,window).fr.ri_e "Index of Refraction" {
		{summary "This is the index of refraction for this material. The index for\n\
			air is 1.0. This parameter is only useful for materials that have\n\
			a non-zero transparency."}
		{range "not less than 1.0"}
	}

	hoc_register_data $shader_params($id,window).fr.shine_e "Shininess" {
		{summary "An indication of the 'shininess' of this material. A higher number\n\
			will make the object look more shiney"}
		{range "integer values from 1 to 10"}
	}

	hoc_register_data $shader_params($id,window).fr.ext_e "Extinction" {
		{summary "Fraction of light absorbed per linear meter of this material.\n\
			The default value here is 0.0."}
		{range "non-negative"}
	}

#	set variables from current 'params' list

	set_phong_values $shader_str $id

	grid $shader_params($id,window).fr.trans -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.trans_e -row 0 -column 1 -sticky w
	grid $shader_params($id,window).fr.refl -row 0 -column 2 -sticky e
	grid $shader_params($id,window).fr.refl_e -row 0 -column 3 -sticky w
	grid $shader_params($id,window).fr.spec -row 1 -column 0 -sticky e
	grid $shader_params($id,window).fr.spec_e -row 1 -column 1 -sticky w
	grid $shader_params($id,window).fr.diff -row 1 -column 2 -sticky e
	grid $shader_params($id,window).fr.diff_e -row 1 -column 3 -sticky w
	grid $shader_params($id,window).fr.ri -row 2 -column 1 -sticky e
	grid $shader_params($id,window).fr.ri_e -row 2 -column 2 -sticky w
	grid $shader_params($id,window).fr.shine -row 3 -column 1 -sticky e
	grid $shader_params($id,window).fr.shine_e -row 3 -column 2 -sticky w
	grid $shader_params($id,window).fr.ext -row 4 -column 1 -sticky e
	grid $shader_params($id,window).fr.ext_e -row 4 -column 2 -sticky w
	
	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

	return $shader_params($id,window).fr
}

proc set_plastic_values { shader_str id } {
	set_phong_values $shader_str $id
}

proc set_glass_values { shader_str id } {
	set_phong_values $shader_str $id
}

proc set_mirror_values { shader_str id } {
	set_phong_values $shader_str $id
}

proc set_phong_values { shader_str id } {
	global shader_params

#	make sure all the entry variables start empty

	set shader_params($id,trans) ""
	set shader_params($id,refl) ""
	set shader_params($id,spec) ""
	set shader_params($id,diff) ""
	set shader_params($id,ri) ""
	set shader_params($id,shine) ""
	set shader_params($id,ext) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set list_len 0}
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				ri { catch {
					if { $value != $shader_params($id,def_ri) } then {
						set shader_params($id,ri) $value }
				     }
				}

				shine -
				sh { catch {
					if { $value != $shader_params($id,def_shine) } then {
						set shader_params($id,shine) $value }
				     }
				}

				specular -
				sp { catch {
					if { $value != $shader_params($id,def_spec) } then {
						set shader_params($id,spec) $value }
				     }
				}

				diffuse -
				di { catch {
					if { $value != $shader_params($id,def_diff) } then {
						set shader_params($id,diff) $value }
				     }
				}

				transmit -
				tr { catch {
					if { $value != $shader_params($id,def_trans) } then {
						set shader_params($id,trans) $value }
				     }
				}

				reflect -
				re { catch {
					if { $value != $shader_params($id,def_refl) } then {
						set shader_params($id,refl) $value }
				     }
				}

				extinction_per_meter -
				extinction -
				ex { catch {
					if { $value != $shader_params($id,def_ext) } then {
						set shader_params($id,ext) $value }
				     }
				}
			}
		}
	}
}

proc do_plastic_apply { shade_var id } {
	upvar #0 $shade_var shader

	set params [do_phong_apply $id]

	set shader [list plastic $params]
}

proc do_glass_apply { shade_var id } {
	upvar #0 $shade_var shader

	set params [do_phong_apply $id]

	set shader [list glass $params]
}

proc do_mirror_apply { shade_var id } {
	upvar #0 $shade_var shader

	set params [do_phong_apply $id]

	set shader [list mirror $params]
}


proc do_phong_apply { id } {
	global shader_params

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params($id,trans) ] > 0 } then {
	    catch {
		if { [expr $shader_params($id,trans) != $shader_params($id,def_trans)] } then {
			lappend params tr $shader_params($id,trans) } }
	    }
	if { [string length $shader_params($id,refl) ] > 0 } then {
	    catch {
		if { [expr $shader_params($id,refl) != $shader_params($id,def_refl)] } then {
			lappend params re $shader_params($id,refl) } }
	    }
	if { [string length $shader_params($id,spec) ] > 0 } then {
	    catch {
		if { [expr $shader_params($id,spec) != $shader_params($id,def_spec)] } then {
			lappend params sp $shader_params($id,spec) } }
	    }
	if { [string length $shader_params($id,diff) ] > 0 } then {
	    catch {
		if { [expr $shader_params($id,diff) != $shader_params($id,def_diff)] } then {
			lappend params di $shader_params($id,diff) } }
	    }
	if { [string length $shader_params($id,ri) ] > 0 } then {
	    catch {
		if { [expr $shader_params($id,ri) != $shader_params($id,def_ri)] } then {
			lappend params ri $shader_params($id,ri) } }
	    }
	if { [string length $shader_params($id,shine)] > 0 } then {
	    catch {
		if { [expr $shader_params($id,shine) != $shader_params($id,def_shine)] } then {
			lappend params sh $shader_params($id,shine) } }
	    }
	if { [string length $shader_params($id,ext)] > 0 } then {
	    catch {
		if { [expr $shader_params($id,ext) != $shader_params($id,def_ext)] } then {
			lappend params ex $shader_params($id,ext) } }
	    }

	return "$params"
}

proc set_plastic_defaults { id } {
	global shader_params

	set shader_params($id,def_shine) 10
	set shader_params($id,def_spec) 0.7
	set shader_params($id,def_diff) 0.3
	set shader_params($id,def_trans) 0
	set shader_params($id,def_refl) 0
	set shader_params($id,def_ri) 1.0
	set shader_params($id,def_ext) 0
}

proc set_mirror_defaults { id } {
	global shader_params

	set shader_params($id,def_shine) 4
	set shader_params($id,def_spec) 0.6
	set shader_params($id,def_diff) 0.4
	set shader_params($id,def_trans) 0
	set shader_params($id,def_refl) 0.75
	set shader_params($id,def_ri) 1.65
	set shader_params($id,def_ext) 0
}

proc set_glass_defaults { id } {
	global shader_params

	set shader_params($id,def_shine) 4
	set shader_params($id,def_spec) 0.7
	set shader_params($id,def_diff) 0.3
	set shader_params($id,def_trans) 0.8
	set shader_params($id,def_refl) 0.1
	set shader_params($id,def_ri) 1.65
	set shader_params($id,def_ext) 0
}

# TEXTURE MAP routines

proc set_bump_defaults { id } {
	set_texture_defaults $id
}

proc set_bwtexture_defaults { id } {
	set_texture_defaults $id
}

proc set_texture_defaults { id } {
	global shader_params

	set shader_params($id,def_width) 512
	set shader_params($id,def_height) 512
}

proc set_bump_values { shader_str id } {
	set_texture_values $shader_str $id
}

proc set_bwtexture_values { shader_str id } {
	set_texture_values $shader_str $id
}

proc set_texture_values { shader_str id } {
	global shader_params

#       make sure all the entry variables start empty

	set shader_params($id,transp) ""
	set shader_params($id,trans_valid) 0
	set shader_params($id,file) ""
	set shader_params($id,width) ""
	set shader_params($id,height) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set list_len 0}
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				w { catch {
					if { $value != $shader_params($id,def_width) } then {
						set shader_params($id,width) $value }
				    }
				}

				n -
				l { catch {
					if { $value != $shader_params($id,def_height) } then {
						set shader_params($id,height) $value }
				    }
				}

				transp {
						set shader_params($id,transp) $value
				}

				trans_valid {
						if { $value != 0 } then {
							set shader_params($id,trans_valid) 1
						} else {
							set shader_params($id,trans_valid) 0
						}
				}

				file {
						set shader_params($id,file) $value
				}
			}
		}
	}
}

proc do_bump_apply { shade_var id } {
	do_texture_apply $shade_var $id
}

proc do_bwtexture_apply { shade_var id } {
	do_texture_apply $shade_var $id
}

proc do_texture_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params($id,file) ] > 0 } then {
			lappend params file $shader_params($id,file) }
	if { [string length $shader_params($id,width) ] > 0 } then {
	    catch {
		if { $shader_params($id,width) != $shader_params($id,def_width) } then {
			lappend params w $shader_params($id,width) } }
	    }
	if { [string length $shader_params($id,height) ] > 0 } then {
	    catch {
		if { $shader_params($id,height) != $shader_params($id,def_height) } then {
			lappend params n $shader_params($id,height) } }
	    }
	if { [string length $shader_params($id,transp) ] > 0 } then {
			lappend params transp $shader_params($id,transp) }
	if { $shader_params($id,trans_valid) != 0 } then {
			if { [string length $shader_params($id,transp) ] == 0 } then {
				tk_messageBox -type ok -icon error -title "ERROR: Missing transparency RGB" -message "Cannot set transparency to valid without a transparency color"
				set shader_params($id,trans_valid) 0
			} else {
				lappend params trans_valid 1 }
	}

	set shader [list $shader_params($id,shader_name) $params ]
}

proc do_texture { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.file -text "Texture File Name"
	entry $shader_params($id,window).fr.file_e -width 40 -textvariable shader_params($id,file)
	bind $shader_params($id,window).fr.file_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.width -text "File Width (pixels)"
	entry $shader_params($id,window).fr.width_e -width 5 -textvariable shader_params($id,width)
	bind $shader_params($id,window).fr.width_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.height -text "File height (pixels)"
	entry $shader_params($id,window).fr.height_e -width 5 -textvariable shader_params($id,height)
	bind $shader_params($id,window).fr.height_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.trans -text "Transparency (RGB)"
	entry $shader_params($id,window).fr.trans_e -width 15 -textvariable shader_params($id,transp)
	bind $shader_params($id,window).fr.trans_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.valid -text "Use transparency"
	checkbutton $shader_params($id,window).fr.valid_e \
		-variable shader_params($id,trans_valid) \
		-command  "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.file_e File {
		{ summary "Enter the name of the file containing the texture to be mapped to this\n\
			object. This file should be a 'pix' file for the 'texture' or 'bump' shaders,\n\
			or a 'bw' file for 'bwtexture'. For the 'bump' shader, the red and blue\n\
			channels of the image are used to perturb the true surface normal.\n\
			Red and blue values of 128 produce no perturbation, while values of 0\n\
			produce maximum perturbation in one direction and 255 produces maximum\n\
			perturbation in the opposite"}
	}

	hoc_register_data $shader_params($id,window).fr.width_e Width {
		{ summary "Enter the width of the texture in pixels\n(default is 512)"}
	}

	hoc_register_data $shader_params($id,window).fr.height_e Height {
		{ summary "Enter the height of the texture in pixels\n(default is 512)"}
	}

	hoc_register_data $shader_params($id,window).fr.trans_e Transparency {
		{ summary "Enter the color in the texture that will be treated as\n\
			transparent. All pixels on this object that get assigned this\n\
			color from the texture will be treated as though this object\n\
			does not exist. For the 'texture' shader, this must be an RGB\n\
			triple. For the 'bwtexture' shader, a single value is sufficient\n\
			This is ignored for the 'bump' shader"}
		{ range "RGB values must be integers from 0 to 255"}
	}

	hoc_register_data $shader_params($id,window).fr.valid_e Transparency {
		{ summary "If depressed, transparency is enabled using the transparency\n\
			color. Otherwise, the transparency color is ignored. The 'bump'\n\
			shader does not use this button."}
	}

	hoc_register_data $shader_params($id,window).fr.file File {
		{ summary "Enter the name of the file containing the texture to be mapped to this\n\
			object. This file should be a 'pix' file for the 'texture' or 'bump' shaders,\n\
			or a 'bw' file for 'bwtexture'. For the 'bump' shader, the red and blue\n\
			channels of the image are used to perturb the true surface normal.\n\
			Red and blue values of 128 produce no perturbation, while values of 0\n\
			produce maximum perturbation in one direction and 255 produces maximum\n\
			perturbation in the opposite"}
	}

	hoc_register_data $shader_params($id,window).fr.width Width {
		{ summary "Enter the width of the texture in pixels\n(default is 512)"}
	}

	hoc_register_data $shader_params($id,window).fr.height Height {
		{ summary "Enter the height of the texture in pixels\n(default is 512)"}
	}

	hoc_register_data $shader_params($id,window).fr.trans Transparency {
		{ summary "Enter the color in the texture that will be treated as\n\
			transparent. All pixels on this object that get assigned this\n\
			color from the texture will be treated as though this object\n\
			does not exist. For the 'texture' shader, this must be an RGB\n\
			triple. For the 'bwtexture' shader, a single value is sufficient.\n\
			This is ignored for the 'bump' shader"}
		{ range "RGB values must be integers from 0 to 255"}
	}

	hoc_register_data $shader_params($id,window).fr.valid Transparency {
		{ summary "If depressed, transparency is enabled using the transparency\n\
			color. Otherwise, the transparency color is ignored. The 'bump'\n\
			shader does not use this button."}
	}

#	set variables from current 'params' list

	set_texture_values $shader_str $id

	grid $shader_params($id,window).fr.file -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.file_e -row 0 -column 1 -columnspan 3 -sticky w
	grid $shader_params($id,window).fr.width -row 1 -column 0 -sticky e
	grid $shader_params($id,window).fr.width_e -row 1 -column 1 -sticky w
	grid $shader_params($id,window).fr.height -row 1 -column 2 -sticky e
	grid $shader_params($id,window).fr.height_e -row 1 -column 3 -sticky w
	grid $shader_params($id,window).fr.trans -row 2 -column 0 -sticky e
	grid $shader_params($id,window).fr.trans_e -row 2 -column 1 -sticky w
	grid $shader_params($id,window).fr.valid_e -row 2 -column 2 -sticky e
	grid $shader_params($id,window).fr.valid -row 2 -column 3 -sticky w

	grid $shader_params($id,window).fr -columnspan 4 -sticky ew -ipadx 3 -ipady 3

	return $shader_params($id,window).fr
}

# STACK routines

proc do_stack_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

# this may be called via a binding in some other shader and so might get the 'id' from
# that shader. So it may be of the form 'id_#,stk_#' (where # is some number)

	set index [string first ",stk" $id]
	if { $index == -1 } then {
		set use_id $id
	} else {
		set index2 [expr $index - 1]
		set use_id [string range $id 0 $index2]
	}

	set params ""

	for {set index 0} {$index < $shader_params($use_id,stack_len)} {incr index} {
		if { [string compare $shader_params($use_id,stk_$index,window) "deleted"] == 0 } {continue}
		set shade_str $shader_params($use_id,stk_$index,shader_name)
		if { [string length $shade_str] == 0 } {set shade_str unknown}
		set shader_name [lindex $shade_str 0]

		do_shader_apply $shade_var $use_id,stk_$index
#		if { ![is_good_shader $shader_name] } then {
#			# this is an unrecognized shader
#			if { [is_good_shader [lindex $shade_str 0]] } then {
#				# and the name has changed to a recognized shader
#				stack_delete $index $shade_var $id
#				stack_insert $index [lindex $shade_str 0] $shade_var $id
#				set_${shader}_values $shade_str $id,stk_$i
#				do_shader_apply $shade_var $use_id,stk_$index
#			}
#		}

		if { [string length $shade_str] == 0 } then {
			lappend params {unknown}
		} else {
			lappend params $shade_str
		}
	}

	if {$params == "" } {
		tk_messageBox -type ok -icon error -title "ERROR: Empty stack"\
			-message "The stack shader is meaningless without placing other shaders in the stack"
		set shade_str stack
	} else {
		set shade_str "stack {$params}"
	}
}

proc set_stack_defaults { id } {
	global shader_params

	set shader_params($id,stack_len) 0
}

proc stack_delete { index shade_var id } {
	global shader_params
# destroy the shader subwindow
	catch {destroy $shader_params($id,stk_$index,window) }

# adjust the shader list
	set shader_params($id,stk_$index,window) deleted
	set shader_params($id,stk_$index,shader_name) ""
#	do_stack_apply $shade_var $id
}

proc stack_add { shader shade_var id } {
	global shader_params

	set index $shader_params($id,stack_len)
	incr shader_params($id,stack_len)
	frame $shader_params($id,window).fr.stk_$index -relief raised -bd 3
	set shader_params($id,stk_$index,window) $shader_params($id,window).fr.stk_$index

	if { [is_good_shader $shader] } then {
		label $shader_params($id,window).fr.stk_$index.lab -text $shader
	} else {
		label $shader_params($id,window).fr.stk_$index.lab -text "Unrecognized Shader"
	}
	grid $shader_params($id,window).fr.stk_$index.lab -columnspan 4 -sticky ew
	set shader_params($id,stk_$index,shader_name) $shader

	button $shader_params($id,window).fr.stk_$index.del -text delete \
		-command "stack_delete $index $shade_var $id" -width 8

	switch $shader {
		plastic {
			set_plastic_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		glass {
			set_glass_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		mirror {
			set_mirror_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		bump -
		bwtexture -
		texture {
			set_texture_defaults "$id,stk_$index"
			set tmp_win [do_texture $shade_var $id,stk_$index]
		}
		checker {
			set_checker_defaults "$id,stk_$index"
			set tmp_win [do_checker $shade_var $id,stk_$index]
		}
		testmap {
			set_testmap_defaults "$id,stk_$index"
			set tmp_win [do_testmap $shade_var $id,stk_$index]
		}
		fakestar {
			set_fakestar_defaults "$id,stk_$index"
			set tmp_win [do_fakestar $shade_var $id,stk_$index]
		}
		cloud {
			set_cloud_defaults "$id,stk_$index"
			set tmp_win [do_cloud $shade_var $id,stk_$index]
		}
		prj {
			set_prj_defaults "$id,stk_$index"
			set tmp_win [do_prj $shade_var $id,stk_$index]
		}
		camo {
			set_camo_defaults "$id,stk_$index"
			set tmp_win [do_camo $shade_var $id,stk_$index]
		}
		default {
			set_unknown_defaults "$id,stk_$index"
			set tmp_win [do_unknown $shade_var $id,stk_$index]
		}
	}

	grid $shader_params($id,window).fr.stk_$index.del -columnspan 4
	grid $shader_params($id,window).fr.stk_$index -columnspan 2 -sticky ew
	grid columnconfigure $shader_params($id,window).fr.stk_$index 0 -minsize 400
#	do_shader_apply $shade_var $id
}

# do not call this routine without first deleting the index_th window
proc stack_insert { index shader shade_var id } {
	global shader_params

	frame $shader_params($id,window).fr.stk_$index -relief raised -bd 3
	set shader_params($id,stk_$index,window) $shader_params($id,window).fr.stk_$index

	if { [is_good_shader $shader] } then {
		label $shader_params($id,window).fr.stk_$index.lab -text $shader
	} else {
		label $shader_params($id,window).fr.stk_$index.lab -text "Unrecognized Shader"
	}
	button $shader_params($id,window).fr.stk_$index.del -text delete \
		-command "stack_delete $index $shade_var $id"

	grid $shader_params($id,window).fr.stk_$index.lab -columnspan 4 -sticky ew
	set shader_params($id,stk_$index,shader_name) $shader

	switch $shader {
		plastic {
			set_plastic_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		glass {
			set_glass_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		mirror {
			set_mirror_defaults "$id,stk_$index"
			set tmp_win [do_phong $shade_var $id,stk_$index]
		}
		bump -
		bwtexture -
		texture {
			set_texture_defaults "$id,stk_$index"
			set tmp_win [do_texture $shade_var $id,stk_$index]
		}
		checker {
			set_checker_defaults "$id,stk_$index"
			set tmp_win [do_checker $shade_var $id,stk_$index]
		}
		testmap {
			set_testmap_defaults "$id,stk_$index"
			set tmp_win [do_testmap $shade_var $id,stk_$index]
		}
		fakestar {
			set_fakestar_defaults "$id,stk_$index"
			set tmp_win [do_fakestar $shade_var $id,stk_$index]
		}
		cloud {
			set_cloud_defaults "$id,stk_$index"
			set tmp_win [do_cloud $shade_var $id,stk_$index]
		}
		camo {
			set_camo_defaults "$id,stk_$index"
			set tmp_win [do_camo $shade_var $id,stk_$index]
		}
		prj {
			set_prj_defaults "$id,stk_$index"
			set tmp_win [do_prj $shade_var $id,stk_$index]
		}
		default {
			set_unknown_defaults "$id,stk_$index"
			set tmp_win [do_unknown $shade_var $id,stk_$index]
		}
	}
	grid $shader_params($id,window).fr.stk_$index.del -columnspan 4

	set index 0
	for { set i 0 } { $i < $shader_params($id,stack_len) } { incr i } {
		if { [string compare $shader_params($id,stk_$i,window) "deleted"] == 0 } continue
		grid $shader_params($id,window).fr.stk_$i -columnspan 2 -sticky ew -row [expr $index + 2]
		grid columnconfigure $shader_params($id,window).fr.stk_$i 0 -minsize 400
		incr index
	}

#	do_stack_apply $shade_var $id
}

proc set_stack_values { shade_str id } {
	global shader_params

	set err [catch "set shade_length [llength $shade_str]"]
	if { $err != 0 } {return}

	if {$shade_length < 2 } {return}

	# get  shader name (should be stack, or maybe envmap)
	set shader [lindex $shade_str 0]

	# get parameters (this should be the list of shaders in the stack)
	set shader_list [lindex $shade_str 1]

	# if this is an envmap using a stack, get the stack stuff out of the shader_list
	if { [string compare $shader envmap] == 0 } {
		# envmap shader using a stack
		set err [catch "set shade_length [llength $shader_list]"]
		if { $err != 0 } {return}

		if {$shade_length < 2 } {return}

		 set shader [lindex $shader_list 0]
		 set shader_list [lindex $shader_list 1]
	}

	set err [catch "set no_of_shaders [llength $shader_list]"]
	if { $err != 0 } {return}

	# process each shader in the list
	for {set index 0} {$index < $no_of_shaders} {incr index } {
		# get the index_th shader from the shader list
		set sub_str [lindex $shader_list $index]
		set shader [lindex $sub_str 0]

		# if we are beyond the current stack length, then we need to add another shader to the stack
		if { $index >= $shader_params($id,stack_len) } {
			# add another shader frame
			stack_add $shader $shader_params($id,shade_var) $id
			if { [is_good_shader $shader]} then {
				set_${shader}_values $sub_str $id,stk_$index
			} else {
				set_unknown_values $sub_str $id,stk_$index
			}
		} else {
			# make mods to existing frame
			set count -1
			for { set i 0 } { $i < $shader_params($id,stack_len) } { incr i } {
				# ignore deleted windows
				if { [string compare $shader_params($id,stk_$i,window) "deleted"] == 0 } continue
				incr count

				# $shader should match $shader_params($id,stk_$i,shader_name)
				if { $count == $index } then {
					if { [string compare $shader_params($id,stk_$i,shader_name) $shader] == 0 } then {
						# shader name hasn't changed, so just set the new values
						if { [is_good_shader $shader] } then {
							set_${shader}_values $sub_str $id,stk_$i
						} else {
							set_unknown_values $sub_str $id,stk_$i
						}
					} else {
						# shader name has changed
						if { [is_good_shader $shader] } then {
							# new shader is recognized replace the old with the new
							stack_delete $i $shader_params($id,shade_var) $id
							stack_insert $i $shader $shader_params($id,shade_var) $id
							set_${shader}_values $sub_str $id,stk_$i
						} elseif { [is_good_shader $shader_params($id,stk_$i,shader_name)] } then {
							# new shader is unrecognized, but old is recognized, replace
							stack_delete $i $shader_params($id,shade_var) $id
							stack_insert $i $shader $shader_params($id,shade_var) $id
							if { [string length $sub_str] == 0 } then {
								set_unknown_values unknown $id,stk_$i
							}  else {
								set_unknown_values $sub_str $id,stk_$i
							}
						} else {
							# old shader is unrecognized and new is unrecognized, just update values
							set_unknown_values $sub_str $id,stk_$i
						}
					}
				}
			}
		}
	}
}

proc do_stack { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	set shader_params($id,shade_var) $shade_var

	menubutton $shader_params($id,window).fr.add\
		-menu $shader_params($id,window).fr.add.m\
		-text "Add shader" -relief raised
	hoc_register_data $shader_params($id,window).fr.add "Add Shader" {
		{summary "Use this menu to select a shader to add to\n\
			the end of the stack"}
	}

	menu $shader_params($id,window).fr.add.m -tearoff 0
	$shader_params($id,window).fr.add.m add command \
		-label plastic -command "stack_add plastic $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label glass -command "stack_add glass $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label mirror -command "stack_add mirror $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label "bump map" -command "stack_add bump $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label texture -command "stack_add texture $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label bwtexture -command "stack_add bwtexture $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label fakestar -command "stack_add fakestar $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label cloud -command "stack_add cloud $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label checker -command "stack_add checker $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label camouflage -command "stack_add camo $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label projection -command "stack_add prj $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label testmap -command "stack_add testmap $shade_var $id"
	$shader_params($id,window).fr.add.m add command \
		-label Unknown -command "stack_add unknown $shade_var $id"

	grid $shader_params($id,window).fr.add -columnspan 2

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

	set_stack_values $shade_str $id

	return $shader_params($id,window).fr
}

proc env_select { shader shade_var id } {
	global shader_params

	if { [winfo exists $shader_params($id,window).fr.env] } {
		set err [catch "set tmp $shader_params($id,env,shader_name)"]
		if { $err != 0 } {
			set old_shader ""
		} else {
			set old_shader $tmp
		}
	} else {
		set old_shader ""
	}

	if { [string compare $old_shader $shader] == 0 } {return}
	set shader_params($id,env,shader_name) $shader

	catch {destroy $shader_params($id,window).fr.env}
	frame $shader_params($id,window).fr.env -relief raised -bd 3
	set shader_params($id,env,window) $shader_params($id,window).fr.env

	label $shader_params($id,window).fr.env.lab -text $shader
	grid $shader_params($id,window).fr.env.lab -columnspan 4 -sticky ew
	grid $shader_params($id,window).fr.env -sticky ew
	grid columnconfigure $shader_params($id,window).fr.env 0 -minsize 400

	switch $shader {
		plastic {
			set_plastic_defaults "$id,env"
			set tmp_win [do_phong $shade_var $id,env]
		}
		glass {
			set_glass_defaults "$id,env"
			set tmp_win [do_phong $shade_var $id,env]
		}
		mirror {
			set_mirror_defaults "$id,env"
			set tmp_win [do_phong $shade_var $id,env]
		}
		bump -
		bwtexture -
		texture {
			set_texture_defaults "$id,env"
			set tmp_win [do_texture $shade_var $id,env]
		}
		checker {
			set_checker_defaults "$id,env"
			set tmp_win [do_checker $shade_var $id,env]
		}
		testmap {
			set_testmap_defaults "$id,env"
			set tmp_win [do_testmap $shade_var $id,env]
		}
		fakestar {
			set_fakestar_defaults "$id,env"
			set tmp_win [do_fakestar $shade_var $id,env]
		}
		cloud {
			set_cloud_defaults "$id,env"
			set tmp_win [do_cloud $shade_var $id,env]
		}
		stack {
			set_stack_defaults "$id,env"
			set tmp_win [do_stack $shade_var $id,env]
		}
		prj {
			set_prj_defaults "$id,env"
			set tmp_win [do_prj $shade_var $id,env]
		}
		camo {
			set_camo_defaults "$id,env"
			set tmp_win [do_camo $shade_var $id,env]
		}
		default {
			set_unknown_defaults "$id,env"
			set tmp_win [do_unknown $shade_var $id,env]
		}
	}
	do_envmap_apply $shade_var $id
}

proc set_envmap_defaults { id } {
	set shader_params($id,env,shader_name) ""
	return
}

proc do_envmap_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

	set params ""

# this may be called via a binding in some other shader and so might get the 'id' from
# that shader. So it may be of the form 'id_#,env' (where # is some number)

	set index [string first ",env" $id]
	if { $index == -1 } then {
		set use_id $id
	} else {
		set index2 [expr $index - 1]
		set use_id [string range $id 0 $index2]
	}

	set shade_str $shader_params($use_id,env,shader_name)
	do_shader_apply $shade_var $use_id,env
	set params $shade_str

	if {$params == "" } {
		tk_messageBox -type ok -icon error -title "ERROR: Empty envmap"\
			-message "The envmap shader is meaningless without selecting another shader as the environmant"
		set shade_str envmap
	} else {
		set shade_str "envmap {$params}"
	}
}

proc set_envmap_values { shade_str id } {
	global shader_params

	set err [catch "set shade_length [llength $shade_str]"]
	if { $err != 0 } {return}

	if { $shade_length < 2 } { return }
	set err [ctach {set env_params [lindex $shade_str 1]}]
	if { $err } {return}
	set err [catch "set sub_len [llength $env_params]"]
	if { $err != 0 } {return}

	set shader [lindex $env_params 0]

	env_select $shader $shader_params($id,shade_var) $id
	if { [is_good_shader $shader] } then {
		set_${shader}_values $env_params $id,env
	} else {
		set_unknown_values $env_params $id,env
	}
	do_shader_apply $shader_params($id,shade_var) $id
}

proc do_envmap { shade_var id } {
	global shader_params
	upvar #0 $shade_var shade_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	set shader_params($id,shade_var) $shade_var

	menubutton $shader_params($id,window).fr.sel_env\
		-menu $shader_params($id,window).fr.sel_env.m\
		-text "Select shader" -relief raised
	hoc_register_data $shader_params($id,window).fr.sel_env "Select Shader" {
		{summary "Use this menu to select a shader for the environment map"}
	}

	menu $shader_params($id,window).fr.sel_env.m -tearoff 0
	$shader_params($id,window).fr.sel_env.m add command \
		-label plastic -command "env_select plastic $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label glass -command "env_select glass $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label mirror -command "env_select mirror $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label "bump map" -command "env_select bump $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label texture -command "env_select texture $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label bwtexture -command "env_select bwtexture $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label fakestar -command "env_select fakestar $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label cloud -command "env_select cloud $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label checker -command "env_select checker $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label camouflage -command "env_select camo $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label projection -command "env_select prj $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label testmap -command "env_select testmap $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label Unrecognized -command "env_select unknown $shade_var $id"
	$shader_params($id,window).fr.sel_env.m add command \
		-label stack -command "env_select stack $shade_var $id"

	grid $shader_params($id,window).fr.sel_env

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

	set_envmap_values $shade_str $id

	return $shader_params($id,window).fr
}

proc do_cloud { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.cl_thresh -text Threshold
	entry $shader_params($id,window).fr.cl_thresh_e -width 5 -textvariable shader_params($id,cl_thresh)
	bind $shader_params($id,window).fr.cl_thresh_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.cl_range -text Range
	entry $shader_params($id,window).fr.cl_range_e -width 5 -textvariable shader_params($id,cl_range)
	bind $shader_params($id,window).fr.cl_range_e <KeyRelease> "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.cl_thresh Threshold {
		{summary "A value (from 0 to 1) is calculated for each point in the texture\n\
			and compared to the threshold. Values above the threshold are cloud\n\
			otherwise it becomes blue sky. The default value is 0.35"}
		{range "0.0 to 1.0"}
	}
	hoc_register_data $shader_params($id,window).fr.cl_thresh_e Threshold {
		{summary "A value (from 0 to 1) is calculated for each point in the texture\n\
			and compared to the threshold. Values above the threshold are cloud\n\
			otherwise it becomes blue sky. The default value is 0.35"}
		{range "0.0 to 1.0"}
	}
	hoc_register_data $shader_params($id,window).fr.cl_range range {
		{summary "A value (from 0 to 1) is calculated for each point in the texture.\n\
			The range determines what range of values will be used for graduating\n\
			between cloud and blue sky. A range of 0 produces clouds with sharp edges\n\
			The default value is 0.3"}
		{range "0.0 to 1.0"}
	}
	hoc_register_data $shader_params($id,window).fr.cl_range_e range {
		{summary "A value (from 0 to 1) is calculated for each point in the texture.\n\
			The range determines what range of values will be used for graduating\n\
			between cloud and blue sky. A range of 0 produces clouds with sharp edges\n\
			The default value is 0.3"}
		{range "0.0 to 1.0"}
	}

	set_cloud_values $shader_str $id

	grid $shader_params($id,window).fr.cl_thresh -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.cl_thresh_e -row 0 -column 1 -sticky w
	grid $shader_params($id,window).fr.cl_range -row 0 -column 2 -sticky e
	grid $shader_params($id,window).fr.cl_range_e -row 0 -column 3 -sticky w

	grid $shader_params($id,window).fr -ipadx 3 -ipady 3 -sticky ew

	return $shader_params($id,window).fr
}

proc set_cloud_values { shader_str id } {
	global shader_params

	set shader_params($id,cl_thresh) ""
	set shader_params($id,cl_range) ""
	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set err [catch {set list_len [llength $params]}]
	if { $err } {set list_len 0}
	if { $list_len > 1 } then {
		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]
			switch $key {
				thresh { catch {
					if { $value != $shader_params($id,def_cl_thresh) } then {
						set shader_params($id,cl_thresh) $value }
				    }
				}
				range { catch {
					if { $value != $shader_params($id,def_cl_range) } then {
						set shader_params($id,cl_range) $value }
				    }
				}
			}
		}
	}
}

proc do_cloud_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params($id,cl_thresh) ] > 0 } then {
	    catch {
		if { $shader_params($id,cl_thresh) != $shader_params($id,def_cl_thresh) } then {
			lappend params thresh $shader_params($id,cl_thresh) } }
	    }
	if { [string length $shader_params($id,cl_range) ] > 0 } then {
	    catch {
		if { $shader_params($id,cl_range) != $shader_params($id,def_cl_range) } then {
			lappend params range $shader_params($id,cl_range) } }
	    }

	set shader [list cloud $params ]
}

proc set_cloud_defaults { id } {
	global shader_params

	set shader_params($id,def_cl_thresh) 0.35
	set shader_params($id,def_cl_range) 0.3
}

proc do_unknown { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str

	catch { destroy $shader_params($id,window).fr }
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.name -text "Shader Name"
	entry $shader_params($id,window).fr.name_e -width 15 -textvariable shader_params($id,unk_name)
	bind $shader_params($id,window).fr.name_e <KeyRelease> "do_shader_apply $shade_var $id"
	label $shader_params($id,window).fr.param -text "Shader Parameters"
	entry $shader_params($id,window).fr.param_e -width 40 -textvariable shader_params($id,unk_param)
	bind $shader_params($id,window).fr.param_e <KeyRelease> "do_shader_apply $shade_var $id"

	hoc_register_data $shader_params($id,window).fr.name_e "Shader Name" {
		{summary "Enter the name of a BRL-CAD shader"}
	}
	hoc_register_data $shader_params($id,window).fr.param_e "Shader Parameters" {
		{summary "Enter the parameters for this BRL-CAD shader"}
	}

	set_unknown_values $shader_str $id

	grid $shader_params($id,window).fr.name -row 0 -column 0 -sticky e
	grid $shader_params($id,window).fr.name_e -row 0 -column 1 -sticky w
	grid $shader_params($id,window).fr.param -row 1 -column 0 -sticky e
	grid $shader_params($id,window).fr.param_e -row 1 -column 1 -sticky w

	grid columnconfigure $shader_params($id,window).fr.name_e 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr.param_e 0 -weight 1

	grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3
	grid columnconfigure $shader_params($id,window).fr 0 -weight 1
	grid columnconfigure $shader_params($id,window).fr 1 -weight 1

	return $shader_params($id,window).fr
}

proc set_unknown_values { shader_str id } {
	global shader_params

	set shader_params($id,unk_param) ""

	set err [catch {set shader [lindex $shader_str 0]}]
	if { $err } {return}
	if { [string compare "stack" $shader] == 0 || [string compare "envmap" $shader] == 0 } then {
		set shader_params($id,unk_name) unknown
	} else {
		set shader_params($id,unk_name) [lindex $shader_str 0]
		if { [llength $shader_str] > 1 } then {
			set shader_params($id,unk_param) [lindex $shader_str 1]
		}
	}
}

proc do_unknown_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader
	set shader [list $shader_params($id,unk_name) $shader_params($id,unk_param)]

	if { [string length $shader] < 1 } then {
		set shader unknown
	}
}

proc set_unknown_defaults { id } {
	return
}

# This proc is called whenever the user types in the main 'shader' entry widget
proc set_shader_params { shade_var id } {
	upvar #0 $shade_var shade_str
	global shader_params errorInfo

	set err [catch "set shader [lindex $shade_str 0]"]
	if { $err != 0 } {return}

	#  if possible, just update values in the current frame
	if { [string compare $shader $shader_params($id,shader_name)] == 0 ||
		([is_good_shader $shader] == 0 &&
		 [is_good_shader $shader_params($id,shader_name)] == 0) } then {

		switch $shader {
			glass {
				set_glass_values $shade_str $id
			}
			mirror {
				set_mirror_values $shade_str $id
			}
			plastic {
				set_phong_values $shade_str $id
			}
			bump {
				set_bump_values $shade_str $id
			}
			bwtexture {
				set_bwtexture_values $shade_str $id
			}
			texture {
				set_texture_values $shade_str $id
			}
			checker {
				set_checker_values $shade_str $id
			}
			testmap {
				set_testmap_values $shade_str $id
			}
			fakestar {
				set_fakestar_values $shade_str $id
			}
			stack {
				set_stack_values $shade_str $id
			}
			envmap {
				set_envmap_values $shade_str $id
			}
			cloud {
				set_cloud_values $shade_str $id
			}
			prj {
				set_prj_values $shade_str $id
			}
			camo {
				set_camo_values $shade_str $id
			}
			default {
				set_unknown_values $shade_str $id
			}
		}
	} else {
		# we have to replace the current frame with a different shader
		do_shader $shade_var $id $shader_params($id,window)
		return
	}
}

# This is the top-level interface
#	'shade_var' contains the name of the variable that the level 0 caller is using
#	to hold the shader string, e.g., "plastic { sh 8 dp .1 }"
#	These routines will update that variable
proc do_shader { shade_var id frame_name } {
	global shader_params errorInfo
	upvar #0 $shade_var shade_str

	set shader_params($id,parent_window_id) $id
	set shader_params($id,window) $frame_name
	set shader_params($id,shade_var) $shade_var

	set my_win ""

	if { [llength $shade_str] > 0 } then {
		set material [lindex $shade_str 0]
		set shader_params($id,shader_name) $material

		switch $material {
			glass {
				set_glass_defaults $id
				set my_win [do_phong $shade_var $id]
			}
			mirror {
				set_mirror_defaults $id
				set my_win [do_phong $shade_var $id]
			}
			plastic {
				set_plastic_defaults $id
				set my_win [do_phong $shade_var $id]
			}
			bump -
			bwtexture -
			texture {
				set_texture_defaults $id
				set my_win [do_texture $shade_var $id]
			}
			checker {
				set_checker_defaults $id
				set my_win [do_checker $shade_var $id]
			}
			testmap {
				set_testmap_defaults $id
				set my_win [do_testmap $shade_var $id]
			}
			fakestar {
				set_fakestar_defaults $id
				set my_win [do_fakestar $shade_var $id]
			}
			stack {
				set_stack_defaults $id
				set my_win [do_stack $shade_var $id]
			}
			envmap {
				set_envmap_defaults $id
				set my_win [do_envmap $shade_var $id]
			}
			cloud {
				set_cloud_defaults $id
				set my_win [do_cloud $shade_var $id]
			}
			prj {
				set_prj_defaults $id
				set my_win [do_prj $shade_var $id]
			}
			camo {
				set_camo_defaults $id
				set my_win [do_camo $shade_var $id]
			}
			default {
				set_unknown_defaults $id
				set my_win [do_unknown $shade_var $id]
			}
		}
	}

	return $my_win
}


proc do_shader_apply { shade_var id } {
	upvar #0 $shade_var shader_str

	if { [string length $shader_str] == 0 } { return }
	set current_shader_type [lindex $shader_str 0]

	switch $current_shader_type {

		plastic {
			do_plastic_apply $shade_var $id
		}
		mirror {
			do_mirror_apply $shade_var $id
		}
		glass {
			do_glass_apply $shade_var $id
		}

		bump -
		bwtexture -
		texture {
			do_texture_apply $shade_var $id
		}
		checker {
			do_checker_apply $shade_var $id
		}
		testmap {
			do_testmap_apply $shade_var $id
		}
		fakestar {
			do_fakestar_apply $shade_var $id
		}
		stack {
			do_stack_apply $shade_var $id
		}
		envmap {
			do_envmap_apply $shade_var $id
		}
		cloud {
			do_cloud_apply $shade_var $id
		}
		prj {
			do_prj_apply $shade_var $id
		}
		camo {
			do_camo_apply $shade_var $id
		}
		default {
			do_unknown_apply $shade_var $id
		}
	}
}

proc is_good_shader { shader } {
	switch $shader {
		stack -
		plastic -
		glass -
		mirror -
		bump -
		texture -
		bwtexture -
		fakestar -
		checker -
		envmap -
		cloud -
		prj -
		camo -
		testmap { return 1 }
		default { return 0 }
	}
}
