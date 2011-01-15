#                     S H A D E R S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#  routines implement the gui for the BRL-CAD shaders
# shader_params is a global array containing all the values for this shader gui
# make your shader param names unique
# the 'id' is passed to these routines to use for uniqueness
# the top-level interface is 'do_shader'
# See "comb.tcl" for an explanation of the widget hierarchy

# To implement a new shader gui:
#	1. add the new shader to the switch command in 'stack_add',
#		'stack_insert', and 'env_select', if this shader is
#		appropriate for being in a stack or environment map
#	2. add the new shader to the menus in 'do_stack' and 'do_envmap'
#		(again, if appropriate)
#	3. add a menu item for the new shader in 'init_comb' (comb.tcl)
#		(required)
#	4. add the following routines: (all are required, replace 'newshader'
#		 with your shader name

# proc do_newshader { shade_var id } - Creates the frame to hold the shader widgets and
#	creates the labels, entries, buttons... Also registers 'help-on-context' data.
#	all entry widgets should bind <KeyRelease> to "do_shader_apply".
#	calls 'set_newshader_values' to set initial settings of widgets.
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

# extern routines (the "extern" shader)
proc do_extern { shade_var id } {
    global shader_params
    upvar #0 $shade_var shader_str

    catch { destroy $shader_params($id,window).fr }
    frame $shader_params($id,window).fr

    label $shader_params($id,window).fr.file -text File
    entry $shader_params($id,window).fr.file_e -width 20 -textvariable shader_params($id,extern_file)
    bind $shader_params($id,window).fr.file_e <KeyRelease> "do_shader_apply $shade_var $id"

    hoc_register_data $shader_params($id,window).fr.file "File" {
	{summary "The 'extern' shader is merely another way of assigning shaders to combinations.\n\
			In this shader, all the parameters are stored in an outboard file. The format of the file\n\
			is:\n\
				shader_name key_word1=value keyword2=value...\n\
			\n\
			This shader is actually a variant of the stack shader, so you can supply more than one\n\
			shader in the file by using the ';' as a separator. For example:\n\
			\n\
				camo s=200 t1=-.3 t2=.125;plastic di=.8\n\
			\n\
			will apply the 'camo' shader, then the 'plastic' shader.\n\
			\n\
			Enter the name of the outboard file here."}
    }
    hoc_register_data $shader_params($id,window).fr.file_e "File" {
	{summary "The 'extern' shader is merely another way of assigning shaders to combinations.\n\
			In this shader, all the parameters are stored in an outboard file. The format of the file\n\
			is:\n\
				shader_name key_word1=value keyword2=value...\n\
			\n\
			This shader is actually a variant of the stack shader, so you can supply more than one\n\
			shader in the file by using the ';' as a separator. For example:\n\
			\n\
				camo s=200 t1=-.3 t2=.125;plastic di=.8\n\
			\n\
			will apply the 'camo' shader, then the 'plastic' shader.\n\
			\n\
			Enter the name of the outboard file here."}
    }

    set_extern_values $shader_str $id

    grid $shader_params($id,window).fr.file -row 0 -column 0 -sticky e
    grid $shader_params($id,window).fr.file_e -row 0 -column 1 -sticky w
    grid $shader_params($id,window).fr -ipadx 3 -ipady 3 -sticky ew
    return $shader_params($id,window).fr
}

proc set_extern_values { shader_str id } {
    global shader_params

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "extern" } {
	    return
	}
    } else {
	set params ""
    }
    set err [catch {set list_len [llength $params]}]
    if { $err } {set list_len 0}
    if { $list_len > 0 } then {
	set shader_params($id,extern_file) [lindex $params 0]
    }

}

proc do_extern_apply { shade_var id } {
    global shader_params
    upvar #0 $shade_var shade_str

    set params ""
    if { [string length $shader_params($id,extern_file) ] > 0 } then {
	lappend params $shader_params($id,extern_file)
    }

    set shade_str [list extern $params]
}

proc set_extern_defaults { id } {
    global shader_params

    set shader_params($id,extern_file) ""
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
    entry $shader_params($id,window).fr.lacun_e -width 7 -textvariable shader_params($id,lacun)
    bind $shader_params($id,window).fr.lacun_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.lacun "Lacunarity" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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
    entry $shader_params($id,window).fr.h_e -width 7 -textvariable shader_params($id,hval)
    bind $shader_params($id,window).fr.h_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.h "H Value" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Larger values mean additional octaves have less effect"}
	{range "At least 1.0 (default is 1.0)"}
    }
    hoc_register_data $shader_params($id,window).fr.h_e "H Value" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Larger values mean additional octaves have less effect"}
	{range "At least 1.0 (default is 1.0)"}
    }

    label $shader_params($id,window).fr.octaves -text "Octaves"
    entry $shader_params($id,window).fr.octaves_e -width 7 -textvariable shader_params($id,octaves)
    bind $shader_params($id,window).fr.octaves_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.octaves "Octaves" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "The number of times the noise grid will be scaled\n\
			and recombined to produce the final noise function."}
	{range "Must be greater than 0 (default is  4.0)"}
    }
    hoc_register_data $shader_params($id,window).fr.octaves_e "Octaves" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "The number of times the noise grid will be scaled\n\
			and recombined to produce the final noise function."}
	{range "Must be greater than 0 (default is  4.0)"}
    }

    label $shader_params($id,window).fr.size -text "Noise Size"
    entry $shader_params($id,window).fr.size_e -width 7 -textvariable shader_params($id,size)
    bind $shader_params($id,window).fr.size_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.size "Noise Size" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "A larger number here produces larger color patches (default is 1.0)"}
	{range "must be greater than zero"}
    }
    hoc_register_data $shader_params($id,window).fr.size_e "Noise Size" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Three scale values are required (X, Y, and Z). These values\n\
			allow scaling the noise pattern non-uniformly"}
	{range "Must be greater than 0 Default values are (1.0 1.0 1.0)"}
    }
    hoc_register_data $shader_params($id,window).fr.scale_e "Noise Scale" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Three scale values are required (X, Y, and Z). These values\n\
			allow scaling the noise pattern non-uniformly"}
	{range "Must be greater than 0 Default values are (1.0 1.0 1.0)"}
    }

    label $shader_params($id,window).fr.c1 -text "Color #1"
    frame $shader_params($id,window).fr.c1_e
    color_entry_build $shader_params($id,window).fr.c1_e color shader_params($id,c1)\
	"color_entry_chooser $id $shader_params($id,window).fr.c1_e color \"Color #1\"\
		 shader_params $id,c1"\
	12 $shader_params($id,c1) not_rt
    hoc_register_data $shader_params($id,window).fr.c1 "Color #1" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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
	12 $shader_params($id,c2) not_rt
    hoc_register_data $shader_params($id,window).fr.c2 "Background Color" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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
	12 $shader_params($id,c3) not_rt
    hoc_register_data $shader_params($id,window).fr.c3 "Color #2" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "This is one of three colors used in the camouflage pattern\n\
			(default color is 38/38/38)"}
    }

    label $shader_params($id,window).fr.t1 -text "Threshold #1"
    entry $shader_params($id,window).fr.t1_e -width 7 -textvariable  shader_params($id,t1)
    bind $shader_params($id,window).fr.t1_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.t1 "Noise Scale" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Areas where the noise function returns a value less than this threshold\n\
			will be colored using color #1"}
	{range " -1.0 to +1.0 (default is -0.25)"}
    }
    hoc_register_data $shader_params($id,window).fr.t1_e "Threshold #1" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Areas where the noise function returns a value less than this threshold\n\
			will be colored using color #1"}
	{range " -1.0 to +1.0 (default is -0.25)"}
    }

    label $shader_params($id,window).fr.t2 -text "Threshold #2"
    entry $shader_params($id,window).fr.t2_e -width 7 -textvariable  shader_params($id,t2)
    bind $shader_params($id,window).fr.t2_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.t2 "Noise Scale" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Areas where the noise function returns a value greater than this threshold\n\
			will be colored using color #2"}
	{range " -1.0 to +1.0 (default is 0.25)"}
    }
    hoc_register_data $shader_params($id,window).fr.t2_e "Threshold #2" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "Areas where the noise function returns a value greater than this threshold\n\
			will be colored using color #2"}
	{range " -1.0 to +1.0 (default is 0.25)"}
    }

    label $shader_params($id,window).fr.delta -text "Noise Delta (X, Y, Z)"
    entry $shader_params($id,window).fr.delta_e -width 20  -textvariable shader_params($id,delta)
    bind $shader_params($id,window).fr.delta_e <KeyRelease> "do_shader_apply $shade_var $id"
    hoc_register_data $shader_params($id,window).fr.delta "Noise Delta" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
			on the object using a fractal noise pattern. This is a procedural shader\n\
			based upon a fractal noise function known as fractional brownian motion or fBm.\n\
			The fractal noise function produces a pseudo-random number in the range\n\
			[-1.0 ... 1.0] from the 3-space coordinates of a point in the bounding volume of the region."}
	{description "These values provide a delta into the noise space.\n\
			Default values are (1000.0 1000.0 1000.0)" }
    }
    hoc_register_data $shader_params($id,window).fr.delta_e "Noise Delta" {
	{summary "The 'camo' shader creates a pseudo-random tri-color camouflage pattern\n\
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

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "camo" } {
	    return
	}
    } else {
	set params ""
    }

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

    set shader_params($id,c1) $shader_params($id,def_c1)
    set shader_params($id,c2) $shader_params($id,def_c2)
    set shader_params($id,c3) $shader_params($id,def_c3)
    set shader_params($id,lacun) $shader_params($id,def_lacun)
    set shader_params($id,hval) $shader_params($id,def_hval)
    set shader_params($id,octaves) $shader_params($id,def_octaves)
    set shader_params($id,size) $shader_params($id,def_size)
    set shader_params($id,scale) $shader_params($id,def_scale)
    set shader_params($id,t1) $shader_params($id,def_t1)
    set shader_params($id,t2) $shader_params($id,def_t2)
    set shader_params($id,delta) $shader_params($id,def_delta)

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
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "prj" } {
	    return
	}
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

    label $shader_params($id,window).fr.fakestar_m \
	-text "There are no parameters to set \n\
		   for the fakestar texture map"
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

    grid $shader_params($id,window).fr -sticky new -ipadx 3 -ipady 3
    grid columnconfigure $shader_params($id,window).fr 0 -weight 1
    grid columnconfigure $shader_params($id,window).fr 1 -weight 1

    return $shader_params($id,window).fr
}

proc set_checker_values { shader_str id } {
    global shader_params

    set shader_params($id,ckr_a) $shader_params($id,def_ckr_a)
    set shader_params($id,ckr_b) $shader_params($id,def_ckr_b)
    set shader_params($id,ckr_scale) $shader_params($id,def_ckr_scale)

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "checker" } {
	    return
	}
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

proc do_plastic { shade_var id } {
    return [do_phong $shade_var $id]
}

proc do_mirror { shade_var id } {
    return [do_phong $shade_var $id]
}

proc do_glass { shade_var id } {
    return [do_phong $shade_var $id]
}

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
    label $shader_params($id,window).fr.emiss -text Emission
    entry $shader_params($id,window).fr.emiss_e -width 5 -textvariable shader_params($id,emiss)
    bind $shader_params($id,window).fr.emiss_e <KeyRelease> "do_shader_apply $shade_var $id"

    hoc_register_data $shader_params($id,window).fr.emiss Emissivity {
	{summary "In addition to reflecting and transmitting light,\n\
			an object may also emit or absorb light. This paramter\n\
			describes that property."}
	{description "Emissivity/absorption"}
	{range "-1.0 through 1.0"}
    }

    hoc_register_data $shader_params($id,window).fr.emiss_e Emissivity {
	{summary "In addition to reflecting and transmitting light,\n\
			an object may also emit or absorb light. This paramter\n\
			describes that property."}
	{description "Emissivity/absorption"}
	{range "-1.0 through 1.0"}
    }

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
    grid $shader_params($id,window).fr.ri -row 2 -column 0 -sticky e
    grid $shader_params($id,window).fr.ri_e -row 2 -column 1 -sticky w
    grid $shader_params($id,window).fr.shine -row 3 -column 0 -sticky e
    grid $shader_params($id,window).fr.shine_e -row 3 -column 1 -sticky w
    grid $shader_params($id,window).fr.ext -row 2 -column 2 -sticky e
    grid $shader_params($id,window).fr.ext_e -row 2 -column 3 -sticky w
    grid $shader_params($id,window).fr.emiss -row 3 -column 2 -sticky e
    grid $shader_params($id,window).fr.emiss_e -row 3 -column 3 -sticky w

    grid $shader_params($id,window).fr -sticky new -ipadx 3 -ipady 3
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

    set shader_params($id,trans) $shader_params($id,def_trans)
    set shader_params($id,refl) $shader_params($id,def_refl)
    set shader_params($id,spec) $shader_params($id,def_spec)
    set shader_params($id,diff) $shader_params($id,def_diff)
    set shader_params($id,ri) $shader_params($id,def_ri)
    set shader_params($id,shine) $shader_params($id,def_shine)
    set shader_params($id,ext) $shader_params($id,def_ext)
    set shader_params($id,emiss) $shader_params($id,def_emiss)

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "plastic" && $shader_name != "glass" && $shader_name != "mirror" } {
	    return
	}
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
		emission -
		em { catch {
		    if { $value != $shader_params($id,def_emiss) } then {
			set shader_params($id,emiss) $value }
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
    if { [string length $shader_params($id,emiss)] > 0 } then {
	catch {
	    if { [expr $shader_params($id,emiss) != $shader_params($id,def_emiss)] } then {
		lappend params em $shader_params($id,emiss) } }
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
    set shader_params($id,def_emiss) 0
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
    set shader_params($id,def_emiss) 0
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
    set shader_params($id,def_emiss) 0
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
    set shader_params($id,width) $shader_params($id,def_width)
    set shader_params($id,height) $shader_params($id,def_height)
    set shader_params($id,mirror) 0
    set shader_params($id,tx_scale_u) 1
    set shader_params($id,tx_scale_v) 1

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "bump" && $shader_name != "bwtexture" && $shader_name != "texture" } {
	    return
	}
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

		m {
		    set shader_params($id,mirror) $value
		}

		uv {
		    set uv_list [string map {","  " " } $value]
		    set shader_params($id,tx_scale_u) [lindex $uv_list 0]
		    set shader_params($id,tx_scale_v) [lindex $uv_list 1]
		}
	    }
	}
    }
}

set light_data {
    e	fraction	f	1.0	"fraction of total light contributed"		"0..1" 		0 0 \
    e	angle		a	180	"angle of light cone"				"0..180" 	1 0 \
    e	target		d	{0 0 0}	"Point to which light is directed\n   (angle must be less than 180)"   "any X,Y,Z"	2 0 \
    e	lumens		b	1.0	"Lumens for Photon mapping" 		        "And Real #"	3 0 \
    c	infinite 	i	0	"Boolean: light is infinite distance away"	"0,1" 		3 2 \
    c	visible 	v	1	"Boolean: light souce object can be seen"	"0,1" 		3 3 \
    i	icon		icon	""	"Shows effect of values for:\n  Shadow Rays\n  infinite\n  visible" "" 0 4 \
}


proc set_light_defaults { id } {
    global shader_params
    global light_data

    foreach {type name abbrev def_val desc range row col } $light_data {
	set shader_params(def_light_$abbrev) $def_val
    }

    set shader_params(def_light_s) 1
}

proc assign_light_defaults { id } {
    global shader_params
    global light_data

    foreach {type name abbrev def_val desc range row col } $light_data {
	set shader_params($id,light_$abbrev) $def_val
    }

    set shader_params($id,light_s) $shader_params(def_light_s)
}

proc do_light { shade_var id } {
    global shader_params
    global light_data
    upvar #0 $shade_var shader_str

    # Destroy our frame in case it already exists
    catch { destroy $shader_params($id,window).fr }

    # Create a frame for the widgets for this shader
    frame $shader_params($id,window).fr


    # For each variable, create a label and an entry widgets
    # and bind <KeyRelease> so that the shader string wil be updated

    set w $shader_params($id,window).fr

    assign_light_defaults $id


    # Load the images
    if { [info exists shader_params(light_i0_v0_s0)] == 0 } {
	foreach i { 0 1 } {
	    foreach v { 0 1 } {
		foreach s { 0 1 2 3 4 5 6 7 8 9 } {
		    set shader_params(light_i${i}_v${v}_s${s}) \
			[image create photo -file \
			     [file join [bu_brlcad_data "tclscripts"] mged l_i${i}_v${v}_s${s}.gif]]
		}
	    }
	}
    }


    foreach {type name abbrev def_val summary range row col } $light_data {
	switch $type {
	    e {
		# Create the labeled entry widgets
		grid [label $w.${abbrev}_lbl -text $name ] -row $row -column $col
		grid [entry $w.${abbrev}_ent -width 10 -textvariable shader_params($id,light_$abbrev)]\
		    -row $row -column [expr $col + 1]

		bind $w.${abbrev}_ent <KeyRelease> "do_shader_apply $shade_var $id"

		hoc_register_data $w.${abbrev}_lbl $name [list [list summary $summary] [list range "$range (default: $def_val)"]]
		hoc_register_data $w.${abbrev}_ent $name [list [list summary $summary] [list range "$range (default: $def_val)"]]
	    }
	    c {
		# Create checkboxes
		grid [checkbutton $w.${abbrev} -text $name -relief sunken -bd 3 \
			  -variable shader_params($id,light_$abbrev) \
			  -command "do_shader_apply $shade_var $id"] \
		    -row $row -column $col

		hoc_register_data $w.${abbrev} $name [list [list summary $summary] [list range "$range (default: $def_val)"]]
	    }
	    i {
		# Create label to display the selected image

		grid [label $w.icon -relief sunken -bd 3 \
			  -image $shader_params(light_i0_v1_s1) ] \
		    -rowspan 3 -row $row -column $col
		hoc_register_data $w.icon shadows [list [list summary $summary]]

		set shader_params($id,icon) $w.icon
	    }
	}
    }

    # Create the scale for shadow rays
    grid [scale $w.shadows -orient horiz -label "Shadow Rays" \
	      -from 0 -to 64 -bd 3 -relief sunken \
	      -command "light_scale $shade_var $id $w.icon"\
	      -variable shader_params($id,light_s) ] \
	-row 0 -column 2 -rowspan 3 -columnspan 2 -sticky nesw
    hoc_register_data $w.shadows shadows [list [list summary "number of rays to fire at light source in determining shadow\n0 rays means no shadows"] [list range "0..64"]]


    # Set the entry widget values from the current shader string
    set_light_values $shader_str $id

    # Grid the shader widget window into the parent widget
    grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

    # weight the columns of the grid evenly
    grid columnconfigure $shader_params($id,window).fr 0 -weight 1
    grid columnconfigure $shader_params($id,window).fr 1 -weight 1

    return $shader_params($id,window).fr
}

# set the entry widget values from the shader string and
# fill in the gui variables

# called when user modifies shader string directly
proc set_light_values { shader_str id } {
    global shader_params

    # grab OUR shader parameters from the shader string
    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "light" } {
	    return
	}
    } else {
	set params ""
    }

    set err [catch {set list_len [llength $params]}]
    if { $err } {set  list_len 0}
    if { $list_len > 1 } then {

	# For each of my parameters, set
	foreach { key value } $params {

	    switch -- $key {
		default {
		    tk_messageBox -message "key $key value $value"
		}
		infinite -
		i { set shader_params($id,light_i) $value }
		visible -
		v {	set shader_params($id,light_v) $value }
		shadows -
		s {	set shader_params($id,light_s) $value }
		bright -
		b {	set shader_params($id,light_b) $value }
		angle -
		a { set shader_params($id,light_a) $value }
		fraction -
		f { set shader_params($id,light_f) $value }
		direct -
		d { set shader_params($id,light_d) $value }
	    }
	}
    }

    do_light_icon $id

}
# Since the scale appends a value to the end of it's args, we can't
# use do_shader_apply directly for scale widgets  That's why we have this
# wrapper proc
proc light_scale {shade_var id icon val args} {
    global shader_params
    do_shader_apply $shade_var $id
}

# This routine takes the values in the shader_params array and creates
# A shader string from it.
# This is called when the user modifies a value in one of the entry widgets
proc do_light_apply { shade_var id } {
    global shader_params
    upvar #0 $shade_var shader

    set params ""

    set pattern ($id,light_)(\[a-z\]*)
    foreach i [array names shader_params] {
	if { [regexp $pattern $i name head varname] && \
		 $shader_params($id,light_$varname) != \
		 $shader_params(def_light_$varname) } {
	    lappend params $varname $shader_params($id,light_$varname)
	}
    }

    do_light_icon $id

    set shader [list "light" $params ]
}

proc do_light_icon { id } {
    global shader_params

    set name ""

    append name "light_i" $shader_params($id,light_i)
    append name "_v"  $shader_params($id,light_v)

    # only have shadow images for up to 9 shadow rays
    if { $shader_params($id,light_s) > 9 } {
	append name "_s" 9
    } else {
	append name "_s"  $shader_params($id,light_s)
    }

    $shader_params($id,icon) configure -image $shader_params($name)
}


######################################################################


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
    if { [string length $shader_params($id,mirror)] > 0 } then {
	if { $shader_params($id,mirror) != 0 } {
	    lappend params m $shader_params($id,mirror)
	}
    }
    if { [string length $shader_params($id,tx_scale_u)] > 0 || [string length $shader_params($id,tx_scale_v)] > 0 } then {
	if { $shader_params($id,tx_scale_u) != 1 || $shader_params($id,tx_scale_v) != 1 } {
	    lappend params uv $shader_params($id,tx_scale_u),$shader_params($id,tx_scale_v)
	}
    }

    set shader [list $shader_params($id,shader_name) $params ]
}

proc do_bump { shade_var id } {
    return [do_texture $shade_var $id]
}

proc do_bwtexture { shade_var id } {
    return [do_texture $shade_var $id]
}

proc do_texture { shade_var id } {
    global shader_params
    upvar #0 $shade_var shader_str

    catch { destroy $shader_params($id,window).fr }
    frame $shader_params($id,window).fr
    frame $shader_params($id,window).fr.file_fr -relief flat
    frame $shader_params($id,window).fr.repl -relief groove -bd 2
    frame $shader_params($id,window).fr.transp_fr -relief flat

    label $shader_params($id,window).fr.file_fr.file -text "Texture File Name"
    entry $shader_params($id,window).fr.file_fr.file_e -width 20 \
	-textvariable shader_params($id,file)
    bind $shader_params($id,window).fr.file_fr.file_e <KeyRelease> "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.file_fr.width -text "File Width (pixels)"
    entry $shader_params($id,window).fr.file_fr.width_e -width 5 \
	-textvariable shader_params($id,width)
    bind $shader_params($id,window).fr.file_fr.width_e <KeyRelease> "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.file_fr.height -text "File height (pixels)"
    entry $shader_params($id,window).fr.file_fr.height_e -width 5 \
	-textvariable shader_params($id,height)
    bind $shader_params($id,window).fr.file_fr.height_e \
	<KeyRelease> "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.repl.mirror -text "Mirror Adjacent tiles"
    checkbutton $shader_params($id,window).fr.repl.mirror_e \
	-variable shader_params($id,mirror) \
	-command "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.repl.u_scale -text "in U-direction"
    entry $shader_params($id,window).fr.repl.u_scale_e -width 4 \
	-textvariable shader_params($id,tx_scale_u)
    bind $shader_params($id,window).fr.repl.u_scale_e <KeyRelease> "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.repl.v_scale -text "in V-direction"
    entry $shader_params($id,window).fr.repl.v_scale_e -width 4 \
	-textvariable shader_params($id,tx_scale_v)
    bind $shader_params($id,window).fr.repl.v_scale_e <KeyRelease> "do_shader_apply $shade_var $id"
    label $shader_params($id,window).fr.transp_fr.trans -text "Transparency (RGB)"
    entry $shader_params($id,window).fr.transp_fr.trans_e -width 11 \
	-textvariable shader_params($id,transp)
    bind $shader_params($id,window).fr.transp_fr.trans_e <KeyRelease> \
	"do_shader_apply $shade_var $id"
    checkbutton $shader_params($id,window).fr.transp_fr.valid_e \
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

    hoc_register_data $shader_params($id,window).fr.transp_fr.trans_e Transparency {
	{ summary "Enter the color in the texture that will be treated as\n\
		transparent. All pixels on this object that get assigned this\n\
		color from the texture will be treated as though this object\n\
		does not exist. For the 'texture' shader, this must be an RGB\n\
		triple. For the 'bwtexture' shader, a single value is sufficient\n\
		This is ignored for the 'bump' shader"}
	{ range "RGB values must be integers from 0 to 255"}
    }

    hoc_register_data $shader_params($id,window).fr.repl.mirror Mirror {
	{ summary "Turn this option on to get smooth transitions between adjacent tiles\n\
		of the texture by mirroring. This only has an effect when texture\n\
		replication is greater than 1"}
    }

    hoc_register_data $shader_params($id,window).fr.repl.mirror_e Mirror {
	{ summary "Turn this option on to get smooth transitions between adjacent tiles\n\
		of the texture by mirroring. This only has an effect when texture\n\
		replication is greater than 1"}
    }

    hoc_register_data $shader_params($id,window).fr.repl.u_scale "Texture Replication" {
	{summary "Each object being shaded has UV coordinates from 0 through 1.0 used to\n\
		lookup which part of the texture should be applied where.  Normally,\n\
		one entire copy of the texture is stretched or compressed to fit the object.\n\
		This is changed when a value other than 1 is entered here. The value entered here\n\
		specifies the number of texture patterns to be used across the object in the U\n\
		parameter direction"}
	{ range "Real numbers greater than 0.0" }
    }

    hoc_register_data $shader_params($id,window).fr.repl.u_scale_e "Texture Replication" {
	{summary "Each object being shaded has UV coordinates from 0 through 1.0 used to\n\
		lookup which part of the texture should be applied where.  Normally,\n\
		one entire copy of the texture is stretched or compressed to fit the object.\n\
		This is changed when a value other than 1 is entered here. The value entered here\n\
		specifies the number of texture patterns to be used across the object in the U\n\
		parameter direction"}
	{ range "Real numbers greater than 0.0" }
    }

    hoc_register_data $shader_params($id,window).fr.repl.v_scale "Texture Replication" {
	{summary "Each object being shaded has UV coordinates from 0 through 1.0 used to\n\
		lookup which part of the texture should be applied where.  Normally,\n\
		one entire copy of the texture is stretched or compressed to fit the object.\n\
		This is changed when a value other than 1 is entered here. The value entered here\n\
		specifies the number of texture patterns to be used across the object in the V\n\
		parameter direction"}
	{ range "Real numbers greater than 0.0" }
    }

    hoc_register_data $shader_params($id,window).fr.repl.v_scale_e "Texture Replication" {
	{summary "Each object being shaded has UV coordinates from 0 through 1.0 used to\n\
		lookup which part of the texture should be applied where.  Normally,\n\
		one entire copy of the texture is stretched or compressed to fit the object.\n\
		This is changed when a value other than 1 is entered here. The value entered here\n\
		specifies the number of texture patterns to be used across the object in the V\n\
		parameter direction"}
	{ range "Real numbers greater than 0.0" }
    }

    hoc_register_data $shader_params($id,window).fr.transp_fr.valid_e Transparency {
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

    hoc_register_data $shader_params($id,window).fr.transp_fr.trans Transparency {
	{ summary "Enter the color in the texture that will be treated as\n\
		transparent. All pixels on this object that get assigned this\n\
		color from the texture will be treated as though this object\n\
		does not exist. For the 'texture' shader, this must be an RGB\n\
		triple. For the 'bwtexture' shader, a single value is sufficient.\n\
		The checkbutton to the left must be depressed for transparency\n\
		to be active. This is ignored for the 'bump' shader"}
	{ range "RGB values must be integers from 0 to 255"}
    }

    #	set variables from current 'params' list

    set_texture_values $shader_str $id

    label $shader_params($id,window).fr.repl.repl_label \
	-text "Texture Replication"
    grid $shader_params($id,window).fr.repl.repl_label \
	-row 0 -column 0 -columnspan 4 -sticky ew
    grid $shader_params($id,window).fr.repl.u_scale \
	-row 1 -column 0 -sticky e
    grid $shader_params($id,window).fr.repl.u_scale_e \
	-row 1 -column 1 -sticky w
    grid $shader_params($id,window).fr.repl.v_scale \
	-row 1 -column 2 -sticky e
    grid $shader_params($id,window).fr.repl.v_scale_e \
	-row 1 -column 3 -sticky w
    grid $shader_params($id,window).fr.repl.mirror_e \
	-row 2 -column 1 -sticky e
    grid $shader_params($id,window).fr.repl.mirror \
	-row 2 -column 2 -sticky w

    grid $shader_params($id,window).fr.file_fr.file -row 0 -column 0 -sticky e
    grid $shader_params($id,window).fr.file_fr.file_e -row 0 -column 1 -columnspan 3 -sticky ew
    grid $shader_params($id,window).fr.file_fr.width -row 1 -column 0 -sticky e
    grid $shader_params($id,window).fr.file_fr.width_e -row 1 -column 1 -sticky w
    grid $shader_params($id,window).fr.file_fr.height -row 1 -column 2 -sticky e
    grid $shader_params($id,window).fr.file_fr.height_e -row 1 -column 3 -sticky w
    grid $shader_params($id,window).fr.file_fr -row 0 -column 0 -sticky ew
    grid columnconfigure $shader_params($id,window).fr.file_fr 0 -weight 1
    grid columnconfigure $shader_params($id,window).fr.file_fr 1 -weight 1
    grid columnconfigure $shader_params($id,window).fr.file_fr 2 -weight 1
    grid columnconfigure $shader_params($id,window).fr.file_fr 3 -weight 1
    grid $shader_params($id,window).fr.repl -row 1 -column 0 -sticky nsew
    grid $shader_params($id,window).fr.transp_fr.valid_e -row 0 -column 0 -sticky w
    grid $shader_params($id,window).fr.transp_fr.trans -row 0 -column 1 -sticky e
    grid $shader_params($id,window).fr.transp_fr.trans_e -row 0 -column 2 -sticky w
    grid $shader_params($id,window).fr.transp_fr -row 2 -column 0

    grid $shader_params($id,window).fr -sticky new -ipadx 3 -ipady 3
    grid columnconfigure $shader_params($id,window).fr 0 -weight 1

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
}

proc stack_add { shader shade_var id childsite} {
    global shader_params

    set index $shader_params($id,stack_len)
    incr shader_params($id,stack_len)
    frame $childsite.stk_$index -bd 2 -relief groove
    set shader_params($id,stk_$index,window) $childsite.stk_$index

    if { [is_good_shader $shader] } then {
	label $childsite.stk_$index.lab -text $shader -bg CadetBlue -fg white
    } else {
	label $childsite.stk_$index.lab -text "Unrecognized Shader" -bg CadetBlue -fg white
    }
    grid $childsite.stk_$index.lab -columnspan 4 -sticky ew
    set shader_params($id,stk_$index,shader_name) $shader

    button $childsite.stk_$index.del -text delete -width 8 \
	-command "stack_delete $index $shade_var $id;\
			do_shader_apply $shade_var $id"
    hoc_register_data $childsite.stk_$index.del "Delete" {
	{summary "The 'stack' shader applies a series of shaders to the\n\
			object being edited. This button will delete one shader\n\
			from the stack"}
    }

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
	light {
	    set_light_defaults "$id,light_$index"
	    set tmp_win [do_light $shade_var $id,stk_$index]
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
	air {
	    set_air_defaults "$id,stk_$index"
	    set tmp_win [do_air $shade_var $id,stk_$index]
	}
	default {
	    set_unknown_defaults "$id,stk_$index"
	    set tmp_win [do_unknown $shade_var $id,stk_$index]
	}
    }

    grid $childsite.stk_$index.del -columnspan 4
    grid $childsite.stk_$index -columnspan 2 -sticky ew
    grid columnconfigure $childsite.stk_$index 0 -minsize 400
}

# do not call this routine without first deleting the index_th window
proc stack_insert { index shader shade_var id } {
    global shader_params

    set childsite [$shader_params($id,window).fr.leesf childsite]
    frame $childsite.stk_$index -relief raised -bd 3
    set shader_params($id,stk_$index,window) $childsite.stk_$index

    if { [is_good_shader $shader] } then {
	label $childsite.stk_$index.lab -text $shader -bg CadetBlue -fg white
    } else {
	label $childsite.stk_$index.lab -text "Unrecognized Shader" -bg CadetBlue -fg white
    }
    button $childsite.stk_$index.del -text delete -width 8 \
	-command "stack_delete $index $shade_var $id;\
			do_shader_apply $shade_var $id"
    hoc_register_data $childsite.stk_$index.del "Delete" {
	{summary "The 'stack' shader applies a series of shaders to the\n\
			object being edited. This button will delete one shader\n\
			from the stack"}
    }

    grid $childsite.stk_$index.lab -columnspan 4 -sticky ew
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
	light {
	    set_light_defaults "$id,light_$index"
	    set tmp_win [do_light $shade_var $id,light_$index]
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
	air {
	    set_air_defaults "$id,stk_$index"
	    set tmp_win [do_air $shade_var $id,stk_$index]
	}
	default {
	    set_unknown_defaults "$id,stk_$index"
	    set tmp_win [do_unknown $shade_var $id,stk_$index]
	}
    }
    grid $childsite.stk_$index.del -columnspan 4 -sticky ew

    set index 0
    for { set i 0 } { $i < $shader_params($id,stack_len) } { incr i } {
	if { [string compare $shader_params($id,stk_$i,window) "deleted"] == 0 } continue
	grid $childsite.stk_$i -columnspan 2 -sticky ew -row [expr $index + 2]
	grid columnconfigure $childsite.stk_$i 0 -minsize 400 -weight 1
	incr index
    }
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
	    stack_add $shader $shader_params($id,shade_var) $id [$shader_params($id,window).fr.leesf childsite]
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
    frame $shader_params($id,window).fr -padx 3 -pady 3

    set childsite [[iwidgets::scrolledframe $shader_params($id,window).fr.leesf -width 432 -height 250 -hscrollmode dynamic ] childsite]

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
	-label plastic -command "stack_add plastic $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label glass -command "stack_add glass $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label mirror -command "stack_add mirror $shade_var $id $childsite; do_shader_apply $shade_var $id"

    $shader_params($id,window).fr.add.m add command \
	-label light -command "stack_add light $shade_var $id $childsite; do_shader_apply $shade_var $id"

    $shader_params($id,window).fr.add.m add command \
	-label "bump map" -command "stack_add bump $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label texture -command "stack_add texture $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label bwtexture -command "stack_add bwtexture $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label fakestar -command "stack_add fakestar $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label cloud -command "stack_add cloud $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label checker -command "stack_add checker $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label camouflage -command "stack_add camo $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label projection -command "stack_add prj $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label air -command "stack_add air $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label testmap -command "stack_add testmap $shade_var $id $childsite; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.add.m add command \
	-label Unknown -command "stack_add unknown $shade_var $id $childsite; do_shader_apply $shade_var $id"

    grid $shader_params($id,window).fr.add -columnspan 2 -row 0
    grid $shader_params($id,window).fr.leesf -sticky nsew -row 1
    grid columnconfigure $shader_params($id,window).fr 0 -weight 1
    grid rowconfigure $shader_params($id,window).fr 1 -weight 1

    grid rowconfigure $shader_params($id,window) 0 -weight 1
    grid rowconfigure $shader_params($id,window) 1 -weight 0
    grid columnconfigure $shader_params($id,window) 0 -weight 1
    grid $shader_params($id,window).fr -sticky ewns
    grid rowconfigure $shader_params($id,window).fr 1 -weight 1

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
	light {
	    set_light_defaults "$id,light_$index"
	    set tmp_win [do_light $shade_var $id,light_$index]
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
	air {
	    set_air_defaults "$id,env"
	    set tmp_win [do_air $shade_var $id,env]
	}
	default {
	    set_unknown_defaults "$id,env"
	    set tmp_win [do_unknown $shade_var $id,env]
	}
    }
    #	do_envmap_apply $shade_var $id
}

proc set_envmap_defaults { id } {
    set shader_params($id,env,shader_name) ""
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
    set err [catch {set env_params [lindex $shade_str 1]}]
    if { $err } {return}
    set err [catch "set sub_len [llength $env_params]"]
    if { $err != 0 } {return}

    set shader [lindex $env_params 0]

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

    if { [string compare $old_shader $shader] != 0 }  then {
	if { [is_good_shader $old_shader] || [is_good_shader $shader]} then {
	    env_select $shader $shader_params($id,shade_var) $id
	}
    }
    if { [is_good_shader $shader] } then {
	set_${shader}_values $env_params $id,env
    } else {
	set_unknown_values $env_params $id,env
    }
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
	-label plastic -command "env_select plastic $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label glass -command "env_select glass $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label mirror -command "env_select mirror $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label "bump map" -command "env_select bump $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label texture -command "env_select texture $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label bwtexture -command "env_select bwtexture $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label fakestar -command "env_select fakestar $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label cloud -command "env_select cloud $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label checker -command "env_select checker $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label camouflage -command "env_select camo $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label projection -command "env_select prj $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label testmap -command "env_select testmap $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label Unrecognized -command "env_select unknown $shade_var $id; do_shader_apply $shade_var $id"
    $shader_params($id,window).fr.sel_env.m add command \
	-label stack -command "env_select stack $shade_var $id; do_shader_apply $shade_var $id"

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

    set shader_params($id,cl_thresh) $shader_params($id,def_cl_thresh)
    set shader_params($id,cl_range) $shader_params($id,def_cl_range)

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "cloud" } {
	    return
	}
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
    entry $shader_params($id,window).fr.param_e -width 20 -textvariable shader_params($id,unk_param)
    bind $shader_params($id,window).fr.param_e <KeyRelease> "do_shader_apply $shade_var $id"

    hoc_register_data $shader_params($id,window).fr.name_e "Shader Name" {
	{summary "Enter the name of a BRL-CAD shader"}
    }
    hoc_register_data $shader_params($id,window).fr.param_e "Shader Parameters" {
	{summary "Enter the parameters for this BRL-CAD shader"}
    }
    hoc_register_data $shader_params($id,window).fr.name "Shader Name" {
	{summary "Enter the name of a BRL-CAD shader"}
    }
    hoc_register_data $shader_params($id,window).fr.param "Shader Parameters" {
	{summary "Enter the parameters for this BRL-CAD shader"}
    }

    set_unknown_values $shader_str $id

    grid $shader_params($id,window).fr.name -row 0 -column 0 -sticky e
    grid $shader_params($id,window).fr.name_e -row 0 -column 1 -sticky w
    grid $shader_params($id,window).fr.param -row 1 -column 0 -sticky e
    grid $shader_params($id,window).fr.param_e -row 1 -column 1 -sticky w

    grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3
    grid columnconfigure $shader_params($id,window).fr 0 -weight 0
    grid columnconfigure $shader_params($id,window).fr 1 -weight 1

    return $shader_params($id,window).fr
}

proc set_unknown_values { shader_str id } {
    global shader_params

    set shader_params($id,unk_param) ""

    set err [catch {set shader [lindex $shader_str 0]}]
    if { $err } return
    if { [string compare "stack" $shader] == 0 } then {
	set shader_params($id,unk_name) unknown
    } elseif { [string compare "envmap" $shader] == 0 } then {
	set shader1 ""
	set err [catch {set shader1 [lindex $shader_str 1]}]
	if { $err } return
	set err [catch {set envshader [lindex $shader1 0]}]
	if { $err } return
	set shader_params($id,unk_name) $envshader
	set err [catch {set envparams [lindex $shader1 1]}]
	if { $err } return
	set shader_params($id,unk_param) $envparams
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

proc do_air { shade_var id } {
    global shader_params
    upvar #0 $shade_var shader_str

    catch { destroy $shader_params($id,window).fr }
    frame $shader_params($id,window).fr

    label $shader_params($id,window).fr.density -text "Density"
    entry $shader_params($id,window).fr.density_e -width 5 -textvariable shader_params($id,density)
    bind $shader_params($id,window).fr.density_e <KeyRelease> "do_shader_apply $shade_var $id"

    label $shader_params($id,window).fr.delta -text "Delta"
    entry $shader_params($id,window).fr.delta_e -width 5 -textvariable shader_params($id,delta)
    bind $shader_params($id,window).fr.delta_e <KeyRelease> "do_shader_apply $shade_var $id"

    label $shader_params($id,window).fr.scale -text "Scale"
    entry $shader_params($id,window).fr.scale_e -width 5 -textvariable shader_params($id,air_scale)
    bind $shader_params($id,window).fr.scale_e <KeyRelease> "do_shader_apply $shade_var $id"

    hoc_register_data $shader_params($id,window).fr.density "Density" {
	{summary "The 'air' shader implements Beer's law to produce realistic\n\
			fog or haze"}
	{description "The 'density' (optical density) is the fraction of energy\n\
			 scattered or absorbed per meter"}
	{range "non-negative (default is 0.1)"}
    }
    hoc_register_data $shader_params($id,window).fr.density_e "Density" {
	{summary "The 'air' shader implements Beer's law to produce realistic\n\
			fog or haze"}
	{description "The 'density' (optical density) is the fraction of energy\n\
			 scattered or absorbed per meter"}
	{range "non-negative (default is 0.1)"}
    }

    set_air_values $shader_str $id

    grid $shader_params($id,window).fr.density -row 0 -column 0 -sticky e
    grid $shader_params($id,window).fr.density_e -row 0 -column 1 -sticky w
    grid $shader_params($id,window).fr.delta -row 0 -column 2 -sticky e
    grid $shader_params($id,window).fr.delta_e -row 0 -column 3 -sticky w
    grid $shader_params($id,window).fr.scale -row 1 -column 1 -sticky e
    grid $shader_params($id,window).fr.scale_e -row 1 -column 2 -sticky w

    grid $shader_params($id,window).fr -sticky ew -ipadx 3 -ipady 3

    return $shader_params($id,window).fr
}

proc set_air_values { shader_str id } {
    global shader_params

    set shader_params($id,density) $shader_params($id,def_density)
    set shader_params($id,delta) $shader_params($id,def_delta)
    set shader_params($id,air_scale) $shader_params($id,def_air_scale)

    if { [llength $shader_str] > 1 } then {
	set params [lindex $shader_str 1]
	set shader_name [lindex $shader_str 0]
	if { $shader_name != "air" } {
	    return
	}
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
		dpm { catch {
		    if { $value != $shader_params($id,def_density) } then {
			set shader_params($id,density) $value
		    }
		}
		}
		delta -
		d { catch {
		    if { $value != $shader_params($id,def_delta) } then {
			set shader_params($id,delta) $value
		    }
		}
		}
		scale -
		s { catch {
		    if { $value != $shader_params($id,def_air_scale) } then {
			set shader_params($id,air_scale) $value
		    }
		}
		}
	    }
	}
    }
}

proc do_air_apply { shade_var id } {
    global shader_params
    upvar #0 $shade_var shade_str

    set params ""

    if { [string length $shader_params($id,density) ] > 0 } then {
	if { $shader_params($id,density) != $shader_params($id,def_density) } then {
	    lappend params dpm $shader_params($id,density)
	}
    }

    if { [string length $shader_params($id,delta) ] > 0 } then {
	if { $shader_params($id,delta) != $shader_params($id,def_delta) } then {
	    lappend params d $shader_params($id,delta)
	}
    }

    if { [string length $shader_params($id,air_scale) ] > 0 } then {
	if { $shader_params($id,air_scale) != $shader_params($id,def_air_scale) } then {
	    lappend params s $shader_params($id,air_scale)
	}
    }

    set shade_str [list air $params]
}

proc set_air_defaults { id } {
    global shader_params

    set shader_params($id,def_density) 0.1
    set shader_params($id,def_air_scale) 0.01
    set shader_params($id,def_delta) 0.0
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

	if { [llength [info procs set_${shader}_values]] == 1 } {
	    set_${shader}_values $shade_str $id
	} else {
	    set_unknown_values $shade_str $id
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

	if { [llength [info procs do_$material]] == 1 &&
	     [llength [info procs set_${material}_defaults]] == 1} {
	    set_${material}_defaults $id
	    set mywin [do_$material $shade_var $id]
	} else {
	    set_unknown_defaults $id
	    set my_win [do_unknown $shade_var $id]
	}
    }

    return $my_win
}


proc do_shader_apply { shade_var id } {
    upvar #0 $shade_var shader_str

    if { [string length $shader_str] == 0 } { return }

    set current_shader_type [lindex $shader_str 0]

    if { [llength [info procs do_${current_shader_type}_apply]] } {
	do_${current_shader_type}_apply $shade_var $id
    } else {
	do_unknown_apply $shade_var $id
    }
}

proc is_good_shader { shader } {

    return [llength [info procs do_$shader] ]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
