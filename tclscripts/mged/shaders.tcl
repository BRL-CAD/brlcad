#!/vld/jra/brlcad/.libtk.m4i64/wish

# these routines implement the gui for the BRL-CAD shaders
# shader_params is a global array containing all the values for this shader gui
# the 'id' is passed to these routines to use for uniqueness
# the top-level interface is 'do_shader'

# To implement a new shader gui:
#	1. add the new shader to the 'switch' in 'do_shader_apply'
#	2. add the new shader to the 'switch' in 'do_shader'
#	3. add the new shader to the 'switch' in 'set_shader_params'
#	4. add the new shader to the switch command in 'stack_add', if this shader is
#		appropriate for being in a stack
#	5. add a menu item for the new shader in 'init_comb' (comb.tcl)
#	6. add the new shader to the list of shader names in the 'is_good_shader' proc
#	7. add the following routines:

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

	grid $shader_params($id,window).fr.fakestar_m -row 0 -column 1 -columnspan 2
	grid $shader_params($id,window).fr -columnspan 4

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

	grid $shader_params($id,window).fr.tst_m -row 0 -column 1 -columnspan 2
	grid $shader_params($id,window).fr -columnspan 4

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

	hoc_register_data $shader_params($id,window).fr.color1_e "First Color" {
		{summary "Enter one of the colors to use in the checkerboard pattern"}
		{range "An RGB triple, each value from 0 to 255"}
	}
	hoc_register_data $shader_params($id,window).fr.color2_e "Second Color" {
		{summary "Enter another color to use in the checkerboard pattern"}
		{range "An RGB triple, each value from 0 to 255"}
	}

	set_checker_values $shader_str $id

	grid $shader_params($id,window).fr.color1 -row 0 -column 1 -sticky e
	grid $shader_params($id,window).fr.color1_e -row 0 -column 2 -sticky w
	grid $shader_params($id,window).fr.color2 -row 1 -column 1 -sticky e
	grid $shader_params($id,window).fr.color2_e -row 1 -column 2 -sticky w

	grid $shader_params($id,window).fr -columnspan 4

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
	set list_len [llength $params]
	if { $list_len > 1 } then {

		for { set index 0 } { $index < $list_len } { set index [expr $index + 2] } {
			set key [lindex $params $index]
			set value [lindex $params [expr $index + 1]]

			switch $key {
				a {
					if { $value != $shader_params($id,def_ckr_a) } then {
						set shader_params($id,ckr_a) $value }
				}

				b {
					if { $value != $shader_params($id,def_ckr_b) } then {
						set shader_params($id,ckr_b) $value }
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
	if { [string length $shader_params($id,ckr_a) ] > 0 } then {
		if { $shader_params($id,ckr_a) != $shader_params($id,def_ckr_a) } then {
			lappend params a $shader_params($id,ckr_a) } }
	if { [string length $shader_params($id,ckr_b) ] > 0 } then {
		if { $shader_params($id,ckr_b) != $shader_params($id,def_ckr_b) } then {
			lappend params b $shader_params($id,ckr_b) } }

	set shader [list checker $params ]
}

proc set_checker_defaults { id } {
	global shader_params

	set shader_params($id,def_ckr_a) [list 255 255 255]
	set shader_params($id,def_ckr_b) [list 0 0 0]
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
	
	grid $shader_params($id,window).fr -columnspan 4

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
			object. This file should be a 'pix' file for the 'texture' shader,\n\
			or a 'bw' file for 'bwtexture'."}
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
			triple. For the 'bwtexture' shader, a single value is sufficient"}
		{ range "RGB values must be integers from 0 to 255"}
	}

	hoc_register_data $shader_params($id,window).fr.valid_e Transparency {
		{ summary "If depressed, transparency is enabled using the transparency\n\
			color. Otherwise, the transparency color is ignored"}
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

	grid $shader_params($id,window).fr -columnspan 4

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

	set index 0
	for {set index 0} {$index < $shader_params($use_id,stack_len)} {incr index} {
		if {$shader_params($use_id,stk_$index,shader_name) == "" } {continue}
		set shade_str $shader_params($use_id,stk_$index,shader_name)
		do_shader_apply $shade_var $use_id,stk_$index
		lappend params $shade_str
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

proc stack_delete { index id } {
	global shader_params

# destroy the shader subwindow
	catch {destroy $shader_params($id,stk_$index,window) }

# remove the shader from the 'delete' menu
	catch {$shader_params($id,window).fr.del.m delete $index}

# adjust the shader list
	set shader_params($id,stk_$index,window) deleted
	set shader_params($id,stk_$index,shader_name) ""
}

proc stack_add { shader shade_var id } {
	global shader_params

	set index $shader_params($id,stack_len)
	incr shader_params($id,stack_len)
	frame $shader_params($id,window).fr.stk_$index -relief raised -bd 3
	set shader_params($id,stk_$index,window) $shader_params($id,window).fr.stk_$index

	label $shader_params($id,window).fr.stk_$index.lab -text $shader
	grid $shader_params($id,window).fr.stk_$index.lab -columnspan 4
	set shader_params($id,stk_$index,shader_name) $shader

	set index_lab [expr $index + 1]
	$shader_params($id,window).fr.del.m add command -label "#$index_lab $shader"\
		-command "stack_delete $index $id"

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
		fakestar {
			set_fakestar_defaults "$id,stk_$index"
			set tmp_win [do_fakestar $shade_var $id,stk_$index]
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
	}

	grid $shader_params($id,window).fr.stk_$index -columnspan 2 -sticky ew
}

proc set_stack_values { shade_str id } {
	global shader_params

	set err [catch "set shade_length [llength $shade_str]"]
	if { $err != 0 } {return}

	if {$shade_length < 2 } {return}

	set shader_list [lindex $shade_str 1]

	set err [catch "set no_of_shaders [llength $shader_list]"]
	if { $err != 0 } {return}

	for {set index 0} {$index < $no_of_shaders} {incr index } {
		set sub_str [lindex $shader_list $index]
		set shader [lindex $sub_str 0]
		if { $index >= $shader_params($id,stack_len) } {
			if { [is_good_shader $shader]} then {
				stack_add $shader $shader_params($id,shade_var) $id
				set_${shader}_values $sub_str $id,stk_$index
			}
		} else {
			set count -1
			for { set i 0 } { $i < $shader_params($id,stack_len) } { incr i } {
				if { $shader_params($id,stk_$i,shader_name) != "" } {
					incr count
					if { $count == $index } then {
						if { [string compare $shader_params($id,stk_$i,shader_name) $shader] == 0 } then {
							set_${shader}_values $sub_str $id,stk_$i
						} else {
							stack_delete $i $id
							if { [is_good_shader $shader] } then {
								stack_add $shader $shader_params($id,shade_var) $id
								set j [expr $shader_params($id,stack_len) - 1 ]
								set_${shader}_values $sub_str $id,stk_$j
							}
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
		-label testmap -command "stack_add testmap $shade_var $id"

	menubutton $shader_params($id,window).fr.del\
		-menu $shader_params($id,window).fr.del.m\
		-text "Delete shader" -relief raised

	menu $shader_params($id,window).fr.del.m -tearoff 0

	grid $shader_params($id,window).fr.add $shader_params($id,window).fr.del

	grid $shader_params($id,window).fr -ipadx 3 -ipady 3 -sticky ew

	set_stack_values $shade_str $id

	return $shader_params($id,window).fr
}

proc set_shader_params { shade_var id } {
	upvar #0 $shade_var shade_str
	global shader_params

	set err [catch "set shader [lindex $shade_str 0]"]

	if { $err != 0 } {return}

	if { $shader != $shader_params($id,shader_name) } { 
		do_shader $shade_var $id $shader_params($id,window)
		return
	}
	
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
	}
}

# This is the top-level interface
#	'shade_var' contains the name of the variable that the level 0 caller is using
#	to hold the shader string, e.g., "plastic { sh 8 dp .1 }"
#	These routines will update that variable
proc do_shader { shade_var id frame_name } {
	global shader_params
	upvar #0 $shade_var shade_str

	set shader_params($id,parent_window_id) $id
	set shader_params($id,window) $frame_name

	set my_win ""

	if { [llength $shade_str] < 1 } then {
		set shade_str plastic
		set shader_params($id,shader_name) $shade_str
		set my_win [do_plastic $shade_var $id]
	} else {
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
		testmap { return 1 }
		default { return 0 }
	}
}
