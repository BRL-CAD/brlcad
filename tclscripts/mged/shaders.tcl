#!/vld/jra/brlcad/.libtk.m4i64/wish

# these routines implement the gui for the BRL-CAD shaders
# shader_params is an array containing all the values for this shader
# the top-level interface is 'do_shader'

# common routine to dismiss the gui
proc do_dismiss {} {
	global shader_params
	destroy $shader_params(window)
}


# PHONG routines

proc do_phong { shade_var } {
	global shader_params
	upvar #0 $shade_var shader_str
	set saved_str $shader_str

	set shader_params(window) .top
	catch {destroy $shader_params(window)}
	toplevel $shader_params(window)
	wm title $shader_params(window) "$shader_params(default_choice) Shader Parameters"
	frame $shader_params(window).fr

	label $shader_params(window).fr.trans -text Transparency
	entry $shader_params(window).fr.trans_e -width 5 -textvariable shader_params(trans)
	label $shader_params(window).fr.refl -text Reflectivity
	entry $shader_params(window).fr.refl_e -width 5 -textvariable shader_params(refl)
	label $shader_params(window).fr.spec -text Specular
	entry $shader_params(window).fr.spec_e -width 5 -textvariable shader_params(spec)
	label $shader_params(window).fr.diff -text Diffuse
	entry $shader_params(window).fr.diff_e -width 5 -textvariable shader_params(diff)
	label $shader_params(window).fr.ri -text "Refractive index"
	entry $shader_params(window).fr.ri_e -width 5 -textvariable shader_params(ri)
	label $shader_params(window).fr.shine -text Shininess
	entry $shader_params(window).fr.shine_e -width 5 -textvariable shader_params(shine)
	label $shader_params(window).fr.ext -text Extinction
	entry $shader_params(window).fr.ext_e -width 5 -textvariable shader_params(ext)

	button $shader_params(window).fr.apply -text apply -command "do_phong_apply $shade_var"
	button $shader_params(window).fr.reset -text reset -command "do_phong_reset $shade_var {$saved_str}"
	button $shader_params(window).fr.dismiss -text dismiss -command do_dismiss

#	set variables from current 'params' list

	set_phong_values $shader_str

	grid $shader_params(window).fr.trans -row 0 -column 0
	grid $shader_params(window).fr.trans_e -row 0 -column 1
	grid $shader_params(window).fr.refl -row 0 -column 2
	grid $shader_params(window).fr.refl_e -row 0 -column 3
	grid $shader_params(window).fr.spec -row 1 -column 0
	grid $shader_params(window).fr.spec_e -row 1 -column 1
	grid $shader_params(window).fr.diff -row 1 -column 2
	grid $shader_params(window).fr.diff_e -row 1 -column 3
	grid $shader_params(window).fr.ri -row 2 -column 1
	grid $shader_params(window).fr.ri_e -row 2 -column 2
	grid $shader_params(window).fr.shine -row 3 -column 1
	grid $shader_params(window).fr.shine_e -row 3 -column 2
	grid $shader_params(window).fr.ext -row 4 -column 1
	grid $shader_params(window).fr.ext_e -row 4 -column 2
	grid $shader_params(window).fr.apply -row 6 -column 0
	grid $shader_params(window).fr.reset -row 6 -column 1 -columnspan 2
	grid $shader_params(window).fr.dismiss -row 6 -column 3
	
	pack $shader_params(window).fr

	return $shader_params(window)
}

proc set_phong_values { shader_str } {
	global shader_params

#	make sure all the entry variables start empty

	set shader_params(trans) ""
	set shader_params(refl) ""
	set shader_params(spec) ""
	set shader_params(diff) ""
	set shader_params(ri) ""
	set shader_params(shine) ""
	set shader_params(ext) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set list_len [llength $params]
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				ri {
					if { $value != $shader_params(def_ri) } then {
						set shader_params(ri) $value }
				}

				shine -
				sh {
					if { $value != $shader_params(def_shine) } then {
						set shader_params(shine) $value }
				}

				specular -
				sp {
					if { $value != $shader_params(def_spec) } then {
						set shader_params(spec) $value }
				}

				diffuse -
				di {
					if { $value != $shader_params(def_diff) } then {
						set shader_params(diff) $value }
				}

				transmit -
				tr {
					if { $value != $shader_params(def_trans) } then {
						set shader_params(trans) $value }
				}

				reflect -
				re {
					if { $value != $shader_params(def_refl) } then {
						set shader_params(refl) $value }
				}

				extinction_per_meter -
				extinction -
				ex {
					if { $value != $shader_params(def_ext) } then {
						set shader_params(ext) $value }
				}
			}
		}
	}
}

proc do_phong_apply { shade_var } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params(trans) ] > 0 } then {
		if { [expr $shader_params(trans) != $shader_params(def_trans)] } then {
			lappend params tr $shader_params(trans) } }
	if { [string length $shader_params(refl) ] > 0 } then {
		if { [expr $shader_params(refl) != $shader_params(def_refl)] } then {
			lappend params re $shader_params(refl) } }
	if { [string length $shader_params(spec) ] > 0 } then {
		if { [expr $shader_params(spec) != $shader_params(def_spec)] } then {
			lappend params sp $shader_params(spec) } }
	if { [string length $shader_params(diff) ] > 0 } then {
		if { [expr $shader_params(diff) != $shader_params(def_diff)] } then {
			lappend params di $shader_params(diff) } }
	if { [string length $shader_params(ri) ] > 0 } then {
		if { [expr $shader_params(ri) != $shader_params(def_ri)] } then {
			lappend params ri $shader_params(ri) } }
	if { [string length $shader_params(shine)] > 0 } then {
		if { [expr $shader_params(shine) != $shader_params(def_shine)] } then {
			lappend params sh $shader_params(shine) } }
	if { [string length $shader_params(ext)] > 0 } then {
		if { [expr $shader_params(ext) != $shader_params(def_ext)] } then {
			lappend params ex $shader_params(ext) } }

	set shader [list $shader_params(default_choice) $params ]
}

proc do_phong_reset { shade_var saved_str } {
	global shader_params
	upvar #0 $shade_var shader_str

	set shader_str $saved_str
	set_phong_values $saved_str
}

proc set_phong_defaults {} {
	global shader_params

	switch $shader_params(default_choice) {

		plastic {
			set shader_params(def_shine) 10
			set shader_params(def_spec) 0.7
			set shader_params(def_diff) 0.3
			set shader_params(def_trans) 0
			set shader_params(def_refl) 0
			set shader_params(def_ri) 1.0
			set shader_params(def_ext) 0
		}

		mirror {
			set shader_params(def_shine) 4
			set shader_params(def_spec) 0.6
			set shader_params(def_diff) 0.4
			set shader_params(def_trans) 0
			set shader_params(def_refl) 0.75
			set shader_params(def_ri) 1.65
			set shader_params(def_ext) 0
		}

		glass {
			set shader_params(def_shine) 4
			set shader_params(def_spec) 0.7
			set shader_params(def_diff) 0.3
			set shader_params(def_trans) 0.8
			set shader_params(def_refl) 0.1
			set shader_params(def_ri) 1.65
			set shader_params(def_ext) 0
		}
	}

}


proc do_plastic { shade_var } {
	global shader_params

	set shader_params(default_choice) plastic
	set_phong_defaults
	set my_win [do_phong $shade_var]
	return $my_win
}

proc do_mirror { shade_var } {
	global shader_params

	set shader_params(default_choice) mirror
	set_phong_defaults
	set my_win [do_phong $shade_var]
	return $my_win
}

proc do_glass { shade_var } {
	global shader_params

	set shader_params(default_choice) glass
	set_phong_defaults
	set my_win [do_phong $shade_var]
	return $my_win
}

# TEXTURE MAP routines

proc do_texture_reset { shade_var saved_str } {
	global shader_params
	upvar #0 $shade_var shader_str

	set shader_str $saved_str
	set_texture_values $saved_str
}

proc set_texture_defaults {} {
	global shader_params

	switch $shader_params(default_choice) {
		texture {
			set shader_params(def_width) 512
			set shader_params(def_height) 512
		}
	}
}

proc set_texture_values { shader_str } {
	global shader_params

#       make sure all the entry variables start empty

	set shader_params(transp) ""
	set shader_params(trans_valid) ""
	set shader_params(file) ""
	set shader_params(width) ""
	set shader_params(height) ""

	if { [llength $shader_str] > 1 } then {
		set params [lindex $shader_str 1]
	} else {
		set params ""
	}
	set list_len [llength $params]
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				w {
					if { $value != $shader_params(def_width) } then {
						set shader_params(width) $value }
				}

				n -
				l {
					if { $value != $shader_params(def_height) } then {
						set shader_params(height) $value }
				}

				transp {
						set shader_params(transp) $value
				}

				trans_valid {
						if { $value != 0 } then {
							set shader_params(trans_valid) 1
						} else {
							set shader_params(trans_valid) 0
						}
				}

				file {
						set shader_params(file) $value
				}
			}
		}
	}
}

proc do_texture_apply { shade_var } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params(file) ] > 0 } then {
			lappend params file $shader_params(file) }
	if { [string length $shader_params(width) ] > 0 } then {
		if { [expr $shader_params(width) != $shader_params(def_width)] } then {
			lappend params w $shader_params(width) } }
	if { [string length $shader_params(height) ] > 0 } then {
		if { [expr $shader_params(height) != $shader_params(def_height)] } then {
			lappend params n $shader_params(height) } }
	if { [string length $shader_params(transp) ] > 0 } then {
			lappend params transp $shader_params(transp) }
	if { $shader_params(trans_valid) != 0 } then {
			if { [string length $shader_params(transp) ] == 0 } then {
				tk_messageBox -type ok -icon error -title "ERROR: Missing transparency RGB" -message "Cannot set transparency to valid without a transparency color"
				set shader_params(trans_valid) 0
			} else {
				lappend params trans_valid 1 }
	}

	set shader [list $shader_params(default_choice) $params ]
}

proc do_texture_main { shade_var } {
	global shader_params
	upvar #0 $shade_var shader_str
	set saved_str $shader_str

	set shader_params(window) .top
	catch {destroy $shader_params(window)}
	toplevel $shader_params(window)
	wm title $shader_params(window) "$shader_params(default_choice) Shader Parameters"
	frame $shader_params(window).fr

	label $shader_params(window).fr.file -text "Texture File Name"
	entry $shader_params(window).fr.file_e -width 40 -textvariable shader_params(file)
	label $shader_params(window).fr.width -text "File Width (pixels)"
	entry $shader_params(window).fr.width_e -width 5 -textvariable shader_params(width)
	label $shader_params(window).fr.height -text "File height (pixels)"
	entry $shader_params(window).fr.height_e -width 5 -textvariable shader_params(height)
	label $shader_params(window).fr.trans -text "Transparency (RGB)"
	entry $shader_params(window).fr.trans_e -width 15 -textvariable shader_params(transp)
	label $shader_params(window).fr.valid -text "Use transparency"
	checkbutton $shader_params(window).fr.valid_e -variable shader_params(trans_valid)

	button $shader_params(window).fr.apply -text apply -command "do_texture_apply $shade_var"
	button $shader_params(window).fr.reset -text reset -command "do_texture_reset $shade_var {$saved_str}"
	button $shader_params(window).fr.dismiss -text dismiss -command do_dismiss

#	set variables from current 'params' list

	set_texture_values $shader_str

	grid $shader_params(window).fr.file -row 0 -column 0 -sticky e
	grid $shader_params(window).fr.file_e -row 0 -column 1 -columnspan 3 -sticky w
	grid $shader_params(window).fr.width -row 1 -column 0 -sticky e
	grid $shader_params(window).fr.width_e -row 1 -column 1 -sticky w
	grid $shader_params(window).fr.height -row 1 -column 2 -sticky e
	grid $shader_params(window).fr.height_e -row 1 -column 3 -sticky w
	grid $shader_params(window).fr.trans -row 2 -column 0 -sticky e
	grid $shader_params(window).fr.trans_e -row 2 -column 1 -sticky w
	grid $shader_params(window).fr.valid_e -row 2 -column 2 -sticky e
	grid $shader_params(window).fr.valid -row 2 -column 3 -sticky w
	grid $shader_params(window).fr.apply -row 3 -column 0
	grid $shader_params(window).fr.reset -row 3 -column 1 -columnspan 2
	grid $shader_params(window).fr.dismiss -row 3 -column 3

	pack $shader_params(window).fr

	return $shader_params(window)
}

proc do_texture { shade_var } {
	global shader_params

	set shader_params(default_choice) texture
	set_texture_defaults
	set my_win [do_texture_main $shade_var]
	return $my_win
}

# This is the top-level interface
#	'shade_var' contains the name of the variable that the level 0 caller is using
#	to hold the shader string, e.g., "plastic { sh 8 dp .1 }"
#	These routines will update that variable
proc do_shader { shade_var } {
	upvar #0 $shade_var shade_str

	if { [llength $shade_str] < 1 } then {
		set shade_str plastic
		set my_win [do_plastic $shade_var]
	} else {
		set material [lindex $shade_str 0]
		switch $material {
			glass { set my_win [do_glass $shade_var] }
			mirror { set my_win [do_mirror $shade_var] }
			plastic { set my_win [do_plastic $shade_var] }
			texture { set my_win [do_texture $shade_var] }

			default {
				tk_messageBox -title "Shader Name Error" -type ok -icon error -message "Invalid shader name ($material)"
				set my_win ""
			}
		}
	}

	return $my_win
}
