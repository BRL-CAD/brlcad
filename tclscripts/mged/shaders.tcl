#!/vld/jra/brlcad/.libtk.m4i64/wish

# these routines implement the gui for the BRL-CAD shaders
# shader_params is an array containing all the values for this shader
# the top-level interface is 'do_shader'

# common routine to dismiss the gui
proc do_dismiss { id } {
	global shader_params
	destroy $shader_params($id,window)
}


# PHONG routines

proc do_phong { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str
	set saved_str $shader_str

	set shader_params($id,window) .top_$id
	catch {destroy $shader_params($id,window)}
	toplevel $shader_params($id,window)
	wm title $shader_params($id,window) "$shader_params($id,default_choice) Shader Parameters"
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.trans -text Transparency
	entry $shader_params($id,window).fr.trans_e -width 5 -textvariable shader_params($id,trans)
	label $shader_params($id,window).fr.refl -text Reflectivity
	entry $shader_params($id,window).fr.refl_e -width 5 -textvariable shader_params($id,refl)
	label $shader_params($id,window).fr.spec -text Specular
	entry $shader_params($id,window).fr.spec_e -width 5 -textvariable shader_params($id,spec)
	label $shader_params($id,window).fr.diff -text Diffuse
	entry $shader_params($id,window).fr.diff_e -width 5 -textvariable shader_params($id,diff)
	label $shader_params($id,window).fr.ri -text "Refractive index"
	entry $shader_params($id,window).fr.ri_e -width 5 -textvariable shader_params($id,ri)
	label $shader_params($id,window).fr.shine -text Shininess
	entry $shader_params($id,window).fr.shine_e -width 5 -textvariable shader_params($id,shine)
	label $shader_params($id,window).fr.ext -text Extinction
	entry $shader_params($id,window).fr.ext_e -width 5 -textvariable shader_params($id,ext)

	button $shader_params($id,window).fr.apply -text apply -command "do_phong_apply $shade_var $id"
	button $shader_params($id,window).fr.reset -text reset -command "do_phong_reset $shade_var $id {$saved_str}"
	button $shader_params($id,window).fr.dismiss -text dismiss -command "do_dismiss $id"

#	set variables from current 'params' list

	set_phong_values $shader_str $id

	grid $shader_params($id,window).fr.trans -row 0 -column 0
	grid $shader_params($id,window).fr.trans_e -row 0 -column 1
	grid $shader_params($id,window).fr.refl -row 0 -column 2
	grid $shader_params($id,window).fr.refl_e -row 0 -column 3
	grid $shader_params($id,window).fr.spec -row 1 -column 0
	grid $shader_params($id,window).fr.spec_e -row 1 -column 1
	grid $shader_params($id,window).fr.diff -row 1 -column 2
	grid $shader_params($id,window).fr.diff_e -row 1 -column 3
	grid $shader_params($id,window).fr.ri -row 2 -column 1
	grid $shader_params($id,window).fr.ri_e -row 2 -column 2
	grid $shader_params($id,window).fr.shine -row 3 -column 1
	grid $shader_params($id,window).fr.shine_e -row 3 -column 2
	grid $shader_params($id,window).fr.ext -row 4 -column 1
	grid $shader_params($id,window).fr.ext_e -row 4 -column 2
	grid $shader_params($id,window).fr.apply -row 6 -column 0
	grid $shader_params($id,window).fr.reset -row 6 -column 1 -columnspan 2
	grid $shader_params($id,window).fr.dismiss -row 6 -column 3
	
	pack $shader_params($id,window).fr

	return $shader_params($id,window)
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
	set list_len [llength $params]
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				ri {
					if { $value != $shader_params($id,def_ri) } then {
						set shader_params($id,ri) $value }
				}

				shine -
				sh {
					if { $value != $shader_params($id,def_shine) } then {
						set shader_params($id,shine) $value }
				}

				specular -
				sp {
					if { $value != $shader_params($id,def_spec) } then {
						set shader_params($id,spec) $value }
				}

				diffuse -
				di {
					if { $value != $shader_params($id,def_diff) } then {
						set shader_params($id,diff) $value }
				}

				transmit -
				tr {
					if { $value != $shader_params($id,def_trans) } then {
						set shader_params($id,trans) $value }
				}

				reflect -
				re {
					if { $value != $shader_params($id,def_refl) } then {
						set shader_params($id,refl) $value }
				}

				extinction_per_meter -
				extinction -
				ex {
					if { $value != $shader_params($id,def_ext) } then {
						set shader_params($id,ext) $value }
				}
			}
		}
	}
}

proc do_phong_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params($id,trans) ] > 0 } then {
		if { [expr $shader_params($id,trans) != $shader_params($id,def_trans)] } then {
			lappend params tr $shader_params($id,trans) } }
	if { [string length $shader_params($id,refl) ] > 0 } then {
		if { [expr $shader_params($id,refl) != $shader_params($id,def_refl)] } then {
			lappend params re $shader_params($id,refl) } }
	if { [string length $shader_params($id,spec) ] > 0 } then {
		if { [expr $shader_params($id,spec) != $shader_params($id,def_spec)] } then {
			lappend params sp $shader_params($id,spec) } }
	if { [string length $shader_params($id,diff) ] > 0 } then {
		if { [expr $shader_params($id,diff) != $shader_params($id,def_diff)] } then {
			lappend params di $shader_params($id,diff) } }
	if { [string length $shader_params($id,ri) ] > 0 } then {
		if { [expr $shader_params($id,ri) != $shader_params($id,def_ri)] } then {
			lappend params ri $shader_params($id,ri) } }
	if { [string length $shader_params($id,shine)] > 0 } then {
		if { [expr $shader_params($id,shine) != $shader_params($id,def_shine)] } then {
			lappend params sh $shader_params($id,shine) } }
	if { [string length $shader_params($id,ext)] > 0 } then {
		if { [expr $shader_params($id,ext) != $shader_params($id,def_ext)] } then {
			lappend params ex $shader_params($id,ext) } }

	set shader [list $shader_params($id,default_choice) $params ]
}

proc do_phong_reset { shade_var id saved_str } {
	global shader_params
	upvar #0 $shade_var shader_str

	set shader_str $saved_str
	set_phong_values $saved_str $id
}

proc set_phong_defaults { id } {
	global shader_params

	switch $shader_params($id,default_choice) {

		plastic {
			set shader_params($id,def_shine) 10
			set shader_params($id,def_spec) 0.7
			set shader_params($id,def_diff) 0.3
			set shader_params($id,def_trans) 0
			set shader_params($id,def_refl) 0
			set shader_params($id,def_ri) 1.0
			set shader_params($id,def_ext) 0
		}

		mirror {
			set shader_params($id,def_shine) 4
			set shader_params($id,def_spec) 0.6
			set shader_params($id,def_diff) 0.4
			set shader_params($id,def_trans) 0
			set shader_params($id,def_refl) 0.75
			set shader_params($id,def_ri) 1.65
			set shader_params($id,def_ext) 0
		}

		glass {
			set shader_params($id,def_shine) 4
			set shader_params($id,def_spec) 0.7
			set shader_params($id,def_diff) 0.3
			set shader_params($id,def_trans) 0.8
			set shader_params($id,def_refl) 0.1
			set shader_params($id,def_ri) 1.65
			set shader_params($id,def_ext) 0
		}
	}

}


proc do_plastic { shade_var id } {
	global shader_params

	set shader_params($id,default_choice) plastic
	set_phong_defaults $id
	set my_win [do_phong $shade_var $id]
	return $my_win
}

proc do_mirror { shade_var id } {
	global shader_params

	set shader_params($id,default_choice) mirror
	set_phong_defaults $id
	set my_win [do_phong $shade_var $id]
	return $my_win
}

proc do_glass { shade_var id } {
	global shader_params

	set shader_params($id,default_choice) glass
	set_phong_defaults $id
	set my_win [do_phong $shade_var $id]
	return $my_win
}

# TEXTURE MAP routines

proc do_texture_reset { shade_var id saved_str } {
	global shader_params
	upvar #0 $shade_var shader_str

	set shader_str $saved_str
	set_texture_values $saved_str $id
}

proc set_texture_defaults { id } {
	global shader_params

	switch $shader_params($id,default_choice) {
		texture {
			set shader_params($id,def_width) 512
			set shader_params($id,def_height) 512
		}
	}
}

proc set_texture_values { shader_str id } {
	global shader_params

#       make sure all the entry variables start empty

	set shader_params($id,transp) ""
	set shader_params($id,trans_valid) ""
	set shader_params($id,file) ""
	set shader_params($id,width) ""
	set shader_params($id,height) ""

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
					if { $value != $shader_params($id,def_width) } then {
						set shader_params($id,width) $value }
				}

				n -
				l {
					if { $value != $shader_params($id,def_height) } then {
						set shader_params($id,height) $value }
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

proc do_texture_apply { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader

	set params ""

	# check each parameter to see if it's been set
	# if set to the default, ignore it
	if { [string length $shader_params($id,file) ] > 0 } then {
			lappend params file $shader_params($id,file) }
	if { [string length $shader_params($id,width) ] > 0 } then {
		if { [expr $shader_params($id,width) != $shader_params($id,def_width)] } then {
			lappend params w $shader_params($id,width) } }
	if { [string length $shader_params($id,height) ] > 0 } then {
		if { [expr $shader_params($id,height) != $shader_params($id,def_height)] } then {
			lappend params n $shader_params($id,height) } }
	if { [string length $shader_params($id,transp) ] > 0 } then {
			lappend params transp $shader_params($id,transp) }
	if { $shader_params($id,trans_valid) != 0 } then {
			if { [string length $shader_params($id,transp) ] == 0 } then {
				tk_messageBox -type ok -icon error -title "ERROR: Missing transparency RGB" -message "Cannot set transparency to valid without a transparency color"
				set shader_params($id,trans_valid) 0
			} else {
				lappend params trans_valid 1 }
	}

	set shader [list $shader_params($id,default_choice) $params ]
}

proc do_texture_main { shade_var id } {
	global shader_params
	upvar #0 $shade_var shader_str
	set saved_str $shader_str

	set shader_params($id,window) .top_$id
	catch {destroy $shader_params($id,window)}
	toplevel $shader_params($id,window)
	wm title $shader_params($id,window) "$shader_params($id,default_choice) Shader Parameters"
	frame $shader_params($id,window).fr

	label $shader_params($id,window).fr.file -text "Texture File Name"
	entry $shader_params($id,window).fr.file_e -width 40 -textvariable shader_params($id,file)
	label $shader_params($id,window).fr.width -text "File Width (pixels)"
	entry $shader_params($id,window).fr.width_e -width 5 -textvariable shader_params($id,width)
	label $shader_params($id,window).fr.height -text "File height (pixels)"
	entry $shader_params($id,window).fr.height_e -width 5 -textvariable shader_params($id,height)
	label $shader_params($id,window).fr.trans -text "Transparency (RGB)"
	entry $shader_params($id,window).fr.trans_e -width 15 -textvariable shader_params($id,transp)
	label $shader_params($id,window).fr.valid -text "Use transparency"
	checkbutton $shader_params($id,window).fr.valid_e -variable shader_params($id,trans_valid)

	button $shader_params($id,window).fr.apply -text apply -command "do_texture_apply $shade_var $id"
	button $shader_params($id,window).fr.reset -text reset -command "do_texture_reset $shade_var $id {$saved_str}"
	button $shader_params($id,window).fr.dismiss -text dismiss -command "do_dismiss $id"

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
	grid $shader_params($id,window).fr.apply -row 3 -column 0
	grid $shader_params($id,window).fr.reset -row 3 -column 1 -columnspan 2
	grid $shader_params($id,window).fr.dismiss -row 3 -column 3

	pack $shader_params($id,window).fr

	return $shader_params($id,window)
}

proc do_texture { shade_var id } {
	global shader_params

	set shader_params($id,default_choice) texture
	set_texture_defaults $id
	set my_win [do_texture_main $shade_var $id]
	return $my_win
}

# This is the top-level interface
#	'shade_var' contains the name of the variable that the level 0 caller is using
#	to hold the shader string, e.g., "plastic { sh 8 dp .1 }"
#	These routines will update that variable
proc do_shader { shade_var id_name } {
	global shader_params
	upvar #0 $shade_var shade_str

	set shader_params($id_name,parent_window_id) $id_name

	if { [llength $shade_str] < 1 } then {
		set shade_str plastic
		set my_win [do_plastic $shade_var $id_name]
	} else {
		set material [lindex $shade_str 0]
		switch $material {
			glass { set my_win [do_glass $shade_var $id_name] }
			mirror { set my_win [do_mirror $shade_var $id_name] }
			plastic { set my_win [do_plastic $shade_var $id_name] }
			texture { set my_win [do_texture $shade_var $id_name] }

			default {
				tk_messageBox -title "Shader Name Error" -type ok -icon error -message "Invalid shader name ($material)"
				set my_win ""
			}
		}
	}

	return $my_win
}
