#                        C O M B . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#			C O M B . T C L
#
#	Widget for editing a combination.
#
#	Author - Robert G. Parker
#
# This is the widget hierarchy from the top level down to the shader frames or Boolean
# expression frames
#
# widget names include $id to make them unique. Typically, $id == "id_0"
#
# Top level combination editor widget: $id.comb
#
# slaves:
# 	.id_0.comb.control_buttons_frame
# 		frame for control buttons (OK, Apply, Reset, Dismiss)
# 	.id_0.comb.my_tabbed
# 		frame for holding the Boolean or Shader stuff
# 	.id_0.comb.region_inherit_frame
# 		frame for "Is Region" and "Inherit" as well as button to
# 		switch between Boolean and shader displays
# 	.id_0.comb.name_stuff
# 		frame for combination label, name and menubutton
#
# -------------------------------------------------
# .id_0.comb.my_tabbed
#
# 	This frame is just a holder for either the Boolean stuff or the shader stuff
#
# slaves:
# 	.id_0.comb.my_tabbed.bool_labeled
# 			or
# 	.id_0.comb.my_tabbed.shader_labeled
#
# -------------------------------------------------
# .id_0.comb.my_tabbed.bool_labeled.childsite
#
# 	This frame is a childsite of the ".id_0.comb.my_tabbed.bool_labeled" mentioned above
# 	This frame is only mapped when the Boolean expression is displayed.
#
# slaves:
# 	.id_0.comb.id_los_air_mater
# 		frame containing the region id, air code, material id, and LOS
# 		This frame is toggled on and off using the "Is Region" checkbutton
#
# 	.id_0.comb.bool_expr_frame
# 		Frame for the Boolean expression. Contains a label, scrollbar,
# 		and text widget.
#
# -------------------------------------------------
# .id_0.comb.my_tabbed.shader_labeled.childsite
# 	This frame is the childsite of the ".id_0.comb.my_tabbed.shader_labeled"
# 	mentioned above. This frame is only mapped when the shader is displayed.
# slaves:
#
# 	.id_0.comb.color_frame
# 		frame for the color label, entry, and menubutton
# 	.id_0.comb.shader_top_frame
# 		frame for the shader stuff
#
# -------------------------------------------------
# .id_0.comb.shader_top_frame
# slaves:
# 	.id_0.comb.shader_label_entry_frame
# 		frame for the shader label, entry, and menubutton
# 	.id_0.comb.shader_frame
# 		frame for the individual shaders to fill
# 		this frame is passed by "do_shade" to the shaders
#
# -------------------------------------------------
# .id_0.comb.shader_frame
# slaves:
# 	.id_0.comb.shader_frame.fr
# 		frame created by an individual shader to hold all its stuff
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr
# slaves:
# 	For the typical shader, the slaves of this frame are the individual
# 	labels, buttons, checkbuttons, entry widgets, menubuttons, etc, for
# 	that particular shader. This is the end of the hierarchy for simple
# 	shaders. From here down, the discussion only relates to the "stack"
# 	shader
#
# 	For the "stack" shader, the slaves of this frame are:
#
# 	.id_0.comb.shader_frame.fr.add
# 		frame for the "Add shader" button
# 	.id_0.comb.shader_frame.fr.leesf
# 		scrolledframe to hold all the stacked shaders
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr.leesf
# slaves:
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite
# 		frame internal to the scrolledframe
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr.leesf.lwchildsite
# slaves:
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.vertsb
# 		vertical scrollbar for scrolledframe
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.horizsb
# 		horizontal scrollbar (dynamic)
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper
# 		frame internal to scrolledframe
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper
# slaves:
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas
# 		another object internal to the scrolledframe widget
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite
# 	This frame is the actual childsite provided by the scrolledframe widget "leesf"
# 	It will contain a "stack" of frames, one for each shader currently
# 	in the stack
#
# slaves:
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_1
# 		frame for first stacked shader
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_2
# 		frame for second stacked shader
# 	.
# 	.
# 	.
#
# -------------------------------------------------
# .id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_1
# 	This is a frame for an individual shader in the stack
#
# slaves:
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_1.del
# 		The delete button for this shader
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_1.lab
# 		The label identifying which type of shader this one is
# 	.id_0.comb.shader_frame.fr.leesf.lwchildsite.clipper.canvas.sfchildsite.stk_1.fr
# 		The frame where the actual buttons, label, entries, etc for this
# 		particular shader located.
#


# We use the Labeledframe from Iwidgets
package require Iwidgets

check_externs "get_comb put_comb"

# this routine displays "which_frame" ("Bool" or "Shade") and forgets the other
proc toggle_bool_shade_frame { id which_frame } {

    set top .$id.comb

    if { $which_frame == "Bool" } {
	grid forget $top.my_tabbed.shader_labeled
	grid $top.my_tabbed.bool_labeled -sticky nsew
	$top.region_inherit_frame.bool_shade_b configure \
	    -text "Show Shader" \
	    -command "toggle_bool_shade_frame $id Shade"
    } else {
	grid forget $top.my_tabbed.bool_labeled
	grid $top.my_tabbed.shader_labeled -sticky nsew
	$top.region_inherit_frame.bool_shade_b configure \
	    -text "Show Boolean" \
	    -command "toggle_bool_shade_frame $id Bool"
    }
}

# top level interface to the combination editor
#
# Creates a top level window containing (among other widgets) two
# labeledframes, one for the Boolean expression and one for shader stuff.
# Only one of these frames is visible at any one time
#
proc init_comb { id } {
    package require Iwidgets
    global mged_gui
    global comb_control
    global shader_params
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    # name of our top level window
    set top .$id.comb

    # if we already have such a window, just pop it up
    if [winfo exists $top] {
	raise $top
	return
    }

    # get default values for ident, air, los, and material
    set defaults [regdef]
    set default_ident [lindex $defaults 1]
    set default_air [lindex $defaults 3]
    set default_los [lindex $defaults 5]
    set default_material [lindex $defaults 7]

    # set the padding
    set comb_control($id,padx) 4
    set comb_control($id,pady) 2

    set comb_control($id,name) ""
    set comb_control($id,isRegion) "Yes"
    set comb_control($id,id) $default_ident
    set comb_control($id,air) $default_air
    set comb_control($id,material) $default_material
    set comb_control($id,los) $default_los
    set comb_control($id,color) ""
    set comb_control($id,inherit) ""
    set comb_control($id,comb) ""
    set comb_control($id,shader) ""
    set comb_control($id,shader_gui) ""
    set comb_control($id,dirty_name) 1

    # invoke a handler whenever the combination name is changed
    trace vdelete comb_control($id,name) w "comb_handle_trace $id"
    trace variable comb_control($id,name) w "comb_handle_trace $id"

    # create our top level window
    toplevel $top -screen $mged_gui($id,screen)

    # create a frame to hold either of the labeledframes
    set my_tabbed_frame [frame $top.my_tabbed -relief flat]

    # create the two labeledframes
    iwidgets::Labeledframe $my_tabbed_frame.shader_labeled -labeltext "Shader"
    iwidgets::Labeledframe $my_tabbed_frame.bool_labeled -labeltext "Boolean"

    # get the childsites (frames) of the labeledframes
    # store them in the "comb_control" array for global access
    set bool_frame [$my_tabbed_frame.bool_labeled childsite]
    set comb_control($id,bool_frame) $bool_frame
    set shade_frame [$my_tabbed_frame.shader_labeled childsite]

    # row 0 is just the color
    # row 1 is the actual frame containing the shader stuff
    grid rowconfigure $shade_frame 0 -weight 0
    grid rowconfigure $shade_frame 1 -weight 1

    # strange, but it appears that the childsite in this labeledframe
    # is in column 1 ?????
    grid columnconfigure $top.my_tabbed.shader_labeled 0 -weight 0
    grid columnconfigure $top.my_tabbed.shader_labeled 1 -weight 1

    # frame to hold the combination name, entry widget, and menunutton
    frame $top.name_stuff

    # frame to hold the region id, los, air code, and GIFT material code
    frame $top.id_los_air_mater

    # frame to hold the color label and the "colorF" frame (created by
    # "color_entry_build")
    frame $top.color_frame

    # frame to contain all the shader stuff
    frame $top.shader_top_frame -relief groove -bd 2

    # frame to hold the display selection menubutton, the "IsRegion",
    # checkbutton, and the "Inherit" checkbutton
    frame $top.region_inherit_frame

    # frame to hold the label, text and scrollbar widgets for the
    # Boolean expression
    frame $top.bool_expr_frame

    # frame to hold the control buttons (Apply, dismiss, ...)
    frame $top.control_buttons_frame

    # frame to hold the region name entry and menubutton widgets
    # this frame sits in the "name_stuff" frame above
    frame $top.name_entry_and_menu_frame -relief sunken -bd 2

    # frame to hold the shader label, entry, and menubutton widgets
    frame $top.shader_label_entry_frame

    # empty frame to pass to the shader routines (in "shaders.tcl")
    frame $top.shader_frame

    # frame to hold the shader entry and menubutton widgets
    # this frame sits in the "shader_label_entry_frame" above
    frame $top.shader_entry_and_menu_frame -relief sunken -bd 2

    # arrange for global access
    set comb_control($id,shader_frame) $top.shader_frame
    set shader_params($id,window) $comb_control($id,shader_frame)

    # initialize name of shader
    set shader_params($id,shader_name) ""

    # create the button that will choose whether the "Boolean" or
    # "Shader" frame is displayed
    set bool_shade_b [button $top.region_inherit_frame.bool_shade_b \
			  -text "Show Shader" \
			  -command "toggle_bool_shade_frame $id Bool"]
    if { 0 } {
    set bool_shade_mb [menubutton $top.region_inherit_frame.bool_shade_menuB \
			   -text "Select View" \
			   -indicatoron true \
			   -relief raised]

    # create menu for above menubutton
    menu $bool_shade_mb.menu -tearoff false
    $bool_shade_mb.menu add command \
	-label "Show Boolean" \
	-command "toggle_bool_shade_frame $id Bool"
    $bool_shade_mb.menu add command \
	-label "Show Shader" \
	-command "toggle_bool_shade_frame $id Shade"
    $bool_shade_mb configure -menu $bool_shade_mb.menu

    # help for above menubutton
    set hoc_data { { summary "Use this button to select whether the Boolean
expression or the Shader parameters is displayed" } }
    hoc_register_data $bool_shade_mb "Select View" $hoc_data
    }

    set hoc_data { { summary "Use this button to select whether the Boolean
expression or the Shader parameters is displayed" } }
    hoc_register_data $bool_shade_b "Select View" $hoc_data

    # create label for combination name
    label $top.nameL -text "Name" -anchor e
    set hoc_data { { summary "The combination name is the name of a
region or a group. The region flag must be set
in order for the combination to be a region.
Note that a region defines a space containing
homogeneous material. In contrast, a group can
contain many different materials." } }
    hoc_register_data $top.nameL "Combination Name" $hoc_data

    # create entry widget for combination name
    entry $top.nameE -relief flat -width 35 -textvar comb_control($id,name)
    hoc_register_data $top.nameE "Combination Name" $hoc_data

    # create menubutton for selecting combination
    menubutton $top.nameMB -relief raised -bd 2\
	    -menu $top.nameMB.m -indicatoron 1
    hoc_register_data $top.nameMB "Combination Selection"\
	    { { summary "This pops up a menu of methods for selecting
a combination name." } }

    # create menu for above menubutton
    menu $top.nameMB.m -title "Combination Selection Method" -tearoff 0

    # add menu items
    $top.nameMB.m add command -label "Select From All Displayed"\
	    -command "winset \$mged_gui($id,active_dm); build_comb_menu_all_displayed"
    hoc_register_menu_data "Combination Selection Method" "Select From All Displayed"\
	    "Select Combination From All Displayed"\
	    { { summary "This pops up a listbox containing the combinations
currently being displayed in the geometry window. The
user can select from among the combinations listed here.
Note - When selecting items from this listbox, a left buttonpress
highlights the combination in question until the button is
released. To select a combination, double click with the left
mouse button." } }
    $top.nameMB.m add command -label "Select Along Ray"\
	    -command "winset \$mged_gui($id,active_dm); set mouse_behavior c"
    hoc_register_menu_data "Combination Selection Method" "Select Along Ray"\
	    "Select Combination Along Ray"\
	    { { summary "This method allows the user to use the
mouse to fire a ray at the combination of interest.
If only one combination is hit, that combination is
selected. If nothing is hit the user can simply fire
another ray, perhaps taking better aim :-). If more
than one combination is hit, a listbox of the hit
combinations is presented. Note - When selecting
items from this listbox, a left buttonpress highlights
the combination in question until the button is
released. To select a combination, double click with
the left mouse button." } }
    $top.nameMB.m add command -label "Select From All"\
	    -command "build_comb_menu_all"
    hoc_register_menu_data "Combination Selection Method" "Select From All"\
	    "Select Combination From All"\
	    { { summary "This pops up a listbox containing all the
combinations in the database. The user can select
from among the combinations listed here. Note - To select
a combination, double click with the left mouse button." } }
    $top.nameMB.m add command -label "Select From All Regions"\
	    -command "build_comb_menu_all_regions"
    hoc_register_menu_data "Combination Selection Method" "Select From All Regions"\
	    "Select Combination From All Regions"\
	    { { summary "This pops up a listbox containing all the
regions in the database. The user can select
from among the regions listed here. Note - To select
a region, double click with the left mouse button." } }
    $top.nameMB.m add command -label "Autoname"\
	    -command "set comb_control($id,name) \[_mged_make_name comb@\]"
    hoc_register_menu_data "Combination Selection Method" "Autoname"\
	    "Automatically generate a combination name."\
	    { { summary "This automatically generates a combination
name of the form 'comb@' where '@' represents
the counter used by make_name." }
              { see_also "make_name" } }

    # create label for region id
    label $top.idL -text "Region Id:" -anchor e
    set hoc_data { { summary "The region id (i.e. item code) is a number
that is typically used for grouping regions
belonging to a particular component. If the
region id is non-zero it is considered to be
a model component. Otherwise, it is considered
to be air. The air code is then used to designate
the kind of air." } }
    hoc_register_data $top.idL "Region Id" $hoc_data

    # create entry widget for region id
    entry $top.idE -relief sunken -width 5 -textvar comb_control($id,id)
    hoc_register_data $top.idE "Region Id" $hoc_data

    # create label for air code
    label $top.airL -text " Air Code:" -anchor e
    set hoc_data { { summary "The air code is a number that is typically
used to designate the kind of air a region
represents. An air code of \"1\" signifies
universal air. An air code that is greater
than \"1\" signifies some other kind of air.
While an air code of \"0\" means that the
region represents a component." } }
    hoc_register_data $top.airL "Air Code" $hoc_data

    # create entry widget for air code
    entry $top.airE -relief sunken -width 2 -textvar comb_control($id,air)
    hoc_register_data $top.airE "Air Code" $hoc_data

    # create label for material code
    label $top.materialL -text " Material Id:" -anchor e
    set hoc_data { { summary "The material id represents a particular
material type as identified by a material
database. In the past, the gift material
database was used to identify the material
type." } }
    hoc_register_data $top.materialL "Material Id" $hoc_data

    # create entry widget for material code
    entry $top.materialE -relief sunken -width 2 -textvar comb_control($id,material)
    hoc_register_data $top.materialE "Material Id" $hoc_data

    # create label for LOS
    label $top.losL -text " LOS:" -anchor e
    set hoc_data { { summary "LOS is a number that represents the
percentage of material a component region
is composed of. For example, if some component
region is defined to be made of \"mild steel\", as
designated by its material id, with an LOS of 20
then the region is considered to be composed of
20% \"mild steel\"."  } }
    hoc_register_data $top.losL "LOS" $hoc_data

    # create entry widget for LOS
    entry $top.losE -relief sunken -width 3 -textvar comb_control($id,los)
    hoc_register_data $top.losE "LOS" $hoc_data

    # create label for color
    label $top.colorL -text "Color" -anchor e
    hoc_register_data $top.colorL "Color"\
	    { { summary "Material color is used by the ray tracer
when rendering. It is also used by MGED
when determining what color to use when
drawing an object" } }

    # create a color entry widget and a color menubutton
    # $top.colorF is the name of the container created by color_entry_build
    # that contains the entry and menubutton for specifying a color
    color_entry_build $top color comb_control($id,color)\
	    "color_entry_chooser $id $top color \"Combination Color\"\
	    comb_control $id,color"\
	    12 $comb_control($id,color) not_rt

    # create "shader" label
    label $top.shaderL -text "Shader" -anchor e
    set hoc_data { { summary "Use this to manually enter the shader
parameters. Note - when entering the
shader parameters directly, the shader
GUI will automatically be updated." } }
    hoc_register_data $top.shaderL "Shader" $hoc_data

    # create entry widget for shader string
    entry $top.shaderE -relief flat -width 12 -textvar comb_control($id,shader)
    hoc_register_data $top.shaderE "Shader" $hoc_data

    # whenever any key is released inside the shader entry widget.
    # call "set_shader_params" to update the shader display
    # (see "shaders.tcl" for this routine)
    bind $top.shaderE <KeyRelease> "set_shader_params comb_control($id,shader) $id"

    # create menubutton to select from all the shaders
    menubutton $top.shaderMB -relief raised -bd 2\
	    -menu $top.shaderMB.m -indicatoron 1
    hoc_register_data $top.shaderMB "Menu of Shaders"\
	    { { summary "This pops up a menu of shader types.
Selecting a shader from the menu will
reconfigure the shader's GUI to take on
the form of the selected shader type." } }

    # create menu for above menubutton
    menu $top.shaderMB.m -title "Shader" -tearoff 0
    $top.shaderMB.m add command -label plastic \
	    -command "comb_shader_gui $id plastic"
    hoc_register_menu_data "Shader" plastic "Shader - Plastic" \
	    { { summary "Set shader parameters to make this object appear as plastic." } }

    # add all the shaders to the above menu
    $top.shaderMB.m add command -label mirror \
	    -command "comb_shader_gui $id mirror"
    hoc_register_menu_data "Shader" mirror "Shader - Mirror" \
	    { { summary "Set shader parameters to make this object appear as a mirror." } }
    $top.shaderMB.m add command -label glass \
	    -command "comb_shader_gui $id glass"
    hoc_register_menu_data "Shader" glass "Shader - Glass" \
	    { { summary "Set shader parameters to make this object appear as glass." } }


    $top.shaderMB.m add command -label light \
	    -command "comb_shader_gui $id light"
    hoc_register_menu_data "Shader" light "Shader - Light" \
	    { { summary "Set shader parameters to make this object appear as a light source." } }



    $top.shaderMB.m add command -label "texture (color)" \
	    -command "comb_shader_gui $id texture"
    hoc_register_menu_data "Shader" "texture (color)" "Shader - Texture (color)"\
	    { { summary "Map a color texture on this object." } }
    $top.shaderMB.m add command -label "texture (b/w)" \
	    -command "comb_shader_gui $id bwtexture"
    hoc_register_menu_data "Shader" "texture (b/w)" "Shader - Texture (b/w)" \
	    { { summary "Map a black and white texture on this object." } }
    $top.shaderMB.m add command -label "bump map" \
	    -command "comb_shader_gui $id bump"
    hoc_register_menu_data "Shader" "bump map" "Shader - Bump" \
		{ { summary "Apply a bump map to perturb the surface normals of this object." } }
    $top.shaderMB.m add command -label "checker" \
	    -command "comb_shader_gui $id checker"
    hoc_register_menu_data "Shader" "checker" "Shader - Checker" \
		{ { summary "texture map a checkerboard pattern on this object." } }
    $top.shaderMB.m add command -label "test map" \
	    -command "comb_shader_gui $id testmap"
    hoc_register_menu_data "Shader" "test map" "Shader - testmap" \
		{ { summary "Map a red and blue gradient on this object proportional to 'uv'\n\
			texture map coordinates." } }
    $top.shaderMB.m add command -label "fake star pattern" \
	    -command "comb_shader_gui $id fakestar"
    hoc_register_menu_data "Shader" "fake star pattern" "Shader - fakestar" \
		{ { summary "Map a fake star field on this object." } }
    $top.shaderMB.m add command -label "cloud" \
	-command "comb_shader_gui $id cloud"
    hoc_register_menu_data "Shader" "cloud" "Shader - cloud" \
	{ { summary "Map a cloud texture on this object." } }
    $top.shaderMB.m add command -label "stack" \
	-command "comb_shader_gui $id stack"
    hoc_register_menu_data "Shader" "stack" "Shader - stack" \
	{ { summary "Consecutively apply multiple shaders to this object." } }
    $top.shaderMB.m add command -label "envmap" \
	-command "comb_shader_gui $id envmap"
    hoc_register_menu_data "Shader" "envmap" "Shader - envmap" \
	{ { summary "Apply an environment map using this region." } }
    $top.shaderMB.m add command -label "projection" \
	-command "comb_shader_gui $id prj"
    hoc_register_menu_data "Shader" "projection" "Shader - prj" \
	{ { summary "Project one or more images on this object." } }
    $top.shaderMB.m add command -label "camouflage" \
	-command "comb_shader_gui $id camo"
    hoc_register_menu_data "Shader" "camouflage" "Shader - camo" \
	{ { summary "Apply camouflage to this object." } }
    $top.shaderMB.m add command -label "air" \
	-command "comb_shader_gui $id air"
    hoc_register_menu_data "Shader" "air" "Shader - air" \
	{ { summary "Apply the air shader to this object." } }
    $top.shaderMB.m add command -label "extern" \
	-command "comb_shader_gui $id extern"
    hoc_register_menu_data "Shader" "extern" "Shader - extern" \
	{ { summary "Use the extern shader (shader parameters in an external file)." } }

    # allow for the possiblity that a shader exists that is not in the above list
    $top.shaderMB.m add command -label "unlisted" \
	-command "comb_shader_gui $id unlisted"
    hoc_register_menu_data "Shader" "unlisted" "Shader - unlisted" \
	{ { summary "Apply a shader that this gui doesn't recognize." } }

    # create label for Boolean expression
    label $top.combL -text "Boolean Expression:" -anchor w
    set hoc_data { { summary "A boolean expression is used to combine
objects to form a region or group. This expression
can consist of three kinds of operators. The 'u'
operator indicates union. The union of two objects
is defined as the volume in both objects. The '-'
operator indicates difference. The difference of two
objects is defined as the volume of the first object
minus the volume of the second object. Lastly, the
'+' operator indicates intersection. The intersection
of two objects is defined as the volume common to
both objects. Note - an object can be a solid, region
or group." } }
    hoc_register_data $top.combL "Boolean Expression" $hoc_data

    # create text widget to hold the Boolean expression
    text $top.combT -relief sunken -bd 2 -width 40 -height 10\
	    -yscrollcommand "$top.combS set" -setgrid true \
	    -tab {3c}
    hoc_register_data $top.combT "Edit Boolean Expression" $hoc_data

    # may need a vertical scrollbar for complex expressions
    scrollbar $top.combS -relief flat -command "$top.combT yview"

    # create "Is Region" checkbutton
    checkbutton $top.isRegionCB -relief raised -text "Is Region"\
	    -offvalue No -onvalue Yes -variable comb_control($id,isRegion)\
	    -command "comb_toggle_isRegion $id"
    hoc_register_data $top.isRegionCB "Is Region" \
	{ { summary "Toggle the region flag on/off. If the
region flag is toggled on \(i.e. checkbutton
is highligted\) the GUI reconfigures itself to
handle regions. If the region flag is toggled
off the GUI reconfigures itself to handle
groups. Note - both regions and groups are
combinations. However, a region is special
in that it has its region flag set. It is
also required to be homogeneous \(i.e. consisting
of the same material\)." } }

    # create "Inherit" checkbutton
    checkbutton $top.inheritCB -relief raised -text "Inherit"\
	    -offvalue No -onvalue Yes -variable comb_control($id,inherit)
    hoc_register_data $top.inheritCB "Inherit"\
	    { { summary "Toggle inheritance on/off. If inheritance
is on, everything in the group takes on
the characteristics of the group. Note -
setting the inheritance for a region has
no effect \(i.e. the members of a region
always take on the characteristics of the
region\)." } }

    # create the control buttons (OK, Apply, ...)
    button $top.okB -relief raised -text "OK"\
	    -command "comb_ok $id $top"
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply the data in the \"Combination Editor\"
to the combination then close the
\"Combination Editor\"." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "comb_apply $id"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the data in the \"Combination Editor\"
to the combination." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "comb_reset $id"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Reset the \"Combination Editor\" with data
from the combination." } }
    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "comb_dismiss $id $top"
    hoc_register_data $top.dismissB "Dismiss" \
	    { { summary "Dismiss (close) the \"Combination Editor\"." } }

    # put name label, entry, and menu button in "name_stuff" frame
    grid $top.nameE $top.nameMB -sticky "ew" -in $top.name_entry_and_menu_frame
    grid columnconfigure $top.name_entry_and_menu_frame 0 -weight 1
    grid $top.nameL -sticky "e" -in $top.name_stuff -pady $comb_control($id,pady) -row 0 -column 0
    grid $top.name_entry_and_menu_frame -sticky "ew" -in $top.name_stuff -pady $comb_control($id,pady) -row 0 -column 1
    grid columnconfigure $top.name_stuff 1 -weight 1

    # put ident, los, air code, and material id into "id_los_air_mater" frame
    grid $top.idL -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 0 -sticky ew
    grid $top.idE -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 1 -sticky ew
    grid $top.airL -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 2 -sticky ew
    grid $top.airE -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 3 -sticky ew
    grid $top.materialL -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 4 -sticky ew
    grid $top.materialE -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 5 -sticky ew
    grid $top.losL -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 6 -sticky ew
    grid $top.losE -in $top.id_los_air_mater -pady $comb_control($id,pady) -row 0 -column 7 -sticky ew
    grid rowconfigure $top.id_los_air_mater 0 -weight 1
    grid columnconfigure $top.id_los_air_mater 1 -weight 1
    grid columnconfigure $top.id_los_air_mater 3 -weight 1
    grid columnconfigure $top.id_los_air_mater 5 -weight 1
    grid columnconfigure $top.id_los_air_mater 7 -weight 1
    grid columnconfigure $top.id_los_air_mater 0 -weight 0
    grid columnconfigure $top.id_los_air_mater 2 -weight 0
    grid columnconfigure $top.id_los_air_mater 4 -weight 0
    grid columnconfigure $top.id_los_air_mater 6 -weight 0


    # put id_los_air_mater into the "Boolean" frame
    grid $top.id_los_air_mater -in $bool_frame \
             -padx $comb_control($id,padx) \
             -pady $comb_control($id,pady) \
             -sticky new -row 0 -column 0
    grid $top.bool_expr_frame -in $bool_frame \
             -padx $comb_control($id,padx) \
             -pady $comb_control($id,pady) \
             -sticky nsew -row 1 -column 0
    grid columnconfigure $bool_frame 0 -weight 1
    grid rowconfigure $bool_frame 0 -weight 0
    grid rowconfigure $bool_frame 1 -weight 1

    # put the color label and the frame built by
    # "color_entry_build" into "color_frame"
    grid $top.colorL \
            -row 0 -column 0 \
	    -sticky "e" \
            -in $top.color_frame \
            -pady $comb_control($id,pady)
    grid $top.colorF \
            -row 0 -column 1 \
	    -sticky "w" \
            -in $top.color_frame \
            -pady $comb_control($id,pady)

    # puts shader entry widget and shader selector menubutton into the
    # "shader_entry_and_menu_frame"
    grid $top.shaderE $top.shaderMB \
	    -sticky "ew" \
	    -in $top.shader_entry_and_menu_frame
    grid columnconfigure $top.shader_entry_and_menu_frame 0 -weight 1

    # combine the above frame and the shader label
    # into  the "shader_label_entry_frame"
    grid $top.shaderL \
	-row 0 -column 0 \
	-sticky "e" \
	-in $top.shader_label_entry_frame
    grid $top.shader_entry_and_menu_frame \
	-row 0 -column 1 \
	-sticky "ew" \
	-in $top.shader_label_entry_frame
    grid columnconfigure $top.shader_label_entry_frame 1 -weight 1

    # puts the above frame in the "shader_top_frame"
    grid $top.shader_label_entry_frame \
	    -sticky "new" \
	    -in $top.shader_top_frame\
	    -padx $comb_control($id,padx) \
	    -pady $comb_control($id,pady)

    grid rowconfigure $top.shader_top_frame 0 -weight 0
    grid rowconfigure $top.shader_top_frame 1 -weight 1
    grid columnconfigure $top.shader_top_frame 0 -weight 1

    # puts color frame into "Shader" labeledframe
    grid $top.color_frame -in $shade_frame \
             -padx $comb_control($id,padx) \
             -pady $comb_control($id,pady) \
             -sticky new -row 0 -column 0

    # add the actual shader frame to the "Shader" labeledframe
    grid $top.shader_top_frame -in $shade_frame \
             -padx $comb_control($id,padx) \
             -pady $comb_control($id,pady) \
             -sticky nsew -row 1 -column 0

    grid rowconfigure $shade_frame 0 -weight 0
    grid rowconfigure $shade_frame 1 -weight 1
    grid columnconfigure $shade_frame 0 -weight 1

    # grid the "Shader labeledframe childsite frame
    grid $shade_frame -sticky nsew

    # put the display selection menubutton, the "Is Region" checkbutton,
    # and the "Inherit" checkbutton in the "region_inherit_frame"
    grid $bool_shade_b \
            -row 0 -column 0 \
            -sticky "w" \
            -in $top.region_inherit_frame
    grid $top.isRegionCB \
            -row 0 -column 2 \
            -sticky "nse" \
            -in $top.region_inherit_frame
    grid $top.inheritCB \
            -row 0 -column 3 \
            -sticky "nse" \
            -in $top.region_inherit_frame
    grid columnconfigure $top.region_inherit_frame 1 -weight 1
    grid rowconfigure $top.region_inherit_frame 0 -weight 0

    # put the Boolean expression label, text and scrollbar into the
    # "bool_expr_frame"
    grid $top.combL -sticky "w" -in $top.bool_expr_frame -row 0 -column 0
    grid $top.combT -sticky "nsew" -in $top.bool_expr_frame -row 1 -column 0
    grid $top.combS -sticky "ns" -in $top.bool_expr_frame -row 1 -column 1
    grid rowconfigure $top.bool_expr_frame 0 -weight 0
    grid rowconfigure $top.bool_expr_frame 1 -weight 1
    grid columnconfigure $top.bool_expr_frame 0 -weight 1
    grid columnconfigure $top.bool_expr_frame 1 -weight 0

    # put the control buttons into the "control_buttons_frame"
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew"\
	    -in $top.control_buttons_frame -pady $comb_control($id,pady)
    grid columnconfigure $top.control_buttons_frame 2 -weight 1
    grid columnconfigure $top.control_buttons_frame 4 -weight 1

    # grid the remaining frames
    grid $top.name_stuff \
         -sticky "ew" \
         -padx $comb_control($id,padx) \
         -pady $comb_control($id,pady) \
         -row 0 -column 0

    grid $top.region_inherit_frame \
         -padx $comb_control($id,padx) \
         -pady $comb_control($id,pady) \
         -sticky new -row 1 -column 0

    grid $my_tabbed_frame \
          -sticky "nsew" \
          -padx $comb_control($id,padx) \
          -pady $comb_control($id,pady) \
          -row 2 -column 0
    grid rowconfigure $my_tabbed_frame 0 -weight 1
    grid columnconfigure $my_tabbed_frame 0 -weight 1

    grid $top.control_buttons_frame \
         -sticky "ew" \
         -padx $comb_control($id,padx) \
         -pady $comb_control($id,pady) \
         -row 3 -column 0

    grid rowconfigure $top 0 -weight 0
    grid rowconfigure $top 2 -weight 1
    grid columnconfigure $top 0 -weight 1

    # start with "Boolean" frame dsplayed
    toggle_bool_shade_frame $id "Bool"

    # handle event when someone presses "Enter"
    # in the combination name entry widget
    bind $top.nameE <Return> "comb_reset $id; break"

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch {comb_dismiss $id $top}"
    wm title $top "Combination Editor ($id)"

}

# called when "OK" is pressed
proc comb_ok {id top} {

    # apply the parameters
    set ret [comb_apply $id]

    if {$ret == 0} {
	# destroy the window
	comb_dismiss $id $top
    }
}

# apply the parameters to the actual combination on disk
proc comb_apply { id } {
    global mged_gui
    global comb_control
    global ::tk::Priv

    set top .$id.comb

    # get the Boolean expression fron the text widget
    set comb_control($id,comb) [$top.combT get 0.0 end]

    # if someone has edited the combination name, take care about
    # overwriting an existing object
    if {$comb_control($id,dirty_name) && [db_exist $comb_control($id,name)]} {
	set ret [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"Warning!"\
		"Warning: about to overwrite $comb_control($id,name)"\
		"" 0 OK Cancel]

	if {$ret} {
	    return 1
	}
    }

    if {$comb_control($id,isRegion)} {

	# this is a region

	if {$comb_control($id,id) < 0} {
	    # region id is less than zero!!!!
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Bad region id!"\
		    "Region id must be >= 0"\
		    "" 0 OK
	    return 1
	}

	if {$comb_control($id,air) < 0} {
	    # air code is less than zero!!!
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Bad air code!"\
		    "Air code must be >= 0"\
		    "" 0 OK
	    return 1
	}

	if {$comb_control($id,id) == 0 && $comb_control($id,air) == 0 ||
            $comb_control($id,id) != 0 && $comb_control($id,air) != 0} {
	    # exactly one of region id and air code should be set
	    set ret [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Warning: both region id and air code are set/unset"\
		    "Warning: both region id and air code are set/unset"\
		    "" 0 OK Cancel]

	    if {$ret} {
		return 1
	    }
	}

	# get color
	if {$comb_control($id,color) == ""} {
	    set color ""
	} else {
	    set color [getRGBorReset $top.colorMB comb_control($id,color) $comb_control($id,color)]
	}

	# actually apply the edist to the combination on disk
	set ret [catch {put_comb $comb_control($id,name) $comb_control($id,isRegion) \
		$comb_control($id,id) $comb_control($id,air) $comb_control($id,material) \
		$comb_control($id,los) $color $comb_control($id,shader) \
		$comb_control($id,inherit) $comb_control($id,comb)} comb_error]

	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
		    "comb_apply: Error"\
		    $comb_error\
		    "" 0 OK
	}

	# set any attributes that we have saved
	set ret [catch {eval attr set $comb_control($id,name) $comb_control($id,attrs) } comb_error ]

	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
		    "comb_apply: Error"\
		    $comb_error\
		    "" 0 OK
	}

	return $ret
    }


    # this is not a region

    # get the color
    if {$comb_control($id,color) == ""} {
	set color ""
    } else {
	set color [getRGBorReset $top.colorMB comb_control($id,color) $comb_control($id,color)]
    }

    # actually apply the edits to the combination on disk
    set ret [catch {put_comb $comb_control($id,name) $comb_control($id,isRegion)\
	    $color $comb_control($id,shader) $comb_control($id,inherit)\
	    $comb_control($id,comb)} comb_error]

    if {$ret} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
		"comb_apply: Error"\
		$comb_error\
		"" 0 OK
    }

    # set any attributes that we have saved
    set ret [catch {eval attr set $comb_control($id,name) $comb_control($id,attrs) } comb_error ]

    if {$ret} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
	    "comb_apply: Error"\
	    $comb_error\
	    "" 0 OK
    }

    return $ret
}

# called when "Reset" button is pressed
# ignores any edits made and restores data from disk
proc comb_reset { id } {
    global mged_gui
    global comb_control
    global ::tk::Priv

    set top .$id.comb

    if {$comb_control($id,name) == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"You must specify a region/combination name!"\
		"You must specify a region/combination name!"\
		"" 0 OK
	return
    }

    # get the combination data from disk
    set save_isRegion $comb_control($id,isRegion)
    set result [catch {get_comb $comb_control($id,name)} comb_defs]
    if {$result == 1} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"comb_reset: Error"\
		$comb_defs\
		"" 0 OK
	return
    }

    # get the attributes
    set result [catch {attr get $comb_control($id,name)} comb_attrs]
    if { $result == 1 } {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"comb_reset: Error"\
		$comb_attrs\
		"" 0 OK
	return
    }

    # eliminate attributes that might conflict with changes we make
    set tmp_comb_attrs {}
    set num_attrs [expr [llength $comb_attrs] / 2]
    for { set i $num_attrs } { $i > 0 } { incr i -1 } {
	set key [lindex $comb_attrs [expr ($i - 1) * 2]]
	set keep 1
	if { [string compare $key "region"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "region_id"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "material_id"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "los"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "aircode"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "rgb"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "oshader"] == 0 } {
	    set keep 0
	} elseif { [string compare $key "inherit"] == 0 } {
	    set keep 0
	}

	if { $keep } {
	    lappend tmp_comb_attrs $key [lindex $comb_attrs [expr {($i - 1) * 2 + 1}]]
	}
    }

    # save the attributes
    set comb_control($id,attrs) $tmp_comb_attrs

    set comb_control($id,isRegion) [lindex $comb_defs 1]

    # set all our data variables for the editor
    if {$comb_control($id,isRegion) == "Yes"} {
	if {$result == 2} {
	    # get default values for ident, air, los, and material
	    set defaults [regdef]
	    set default_ident [lindex $defaults 1]
	    set default_air [lindex $defaults 3]
	    set default_los [lindex $defaults 5]
	    set default_material [lindex $defaults 7]

	    set comb_control($id,id) $default_ident
	    set comb_control($id,air) $default_air
	    set comb_control($id,material) $default_material
	    set comb_control($id,los) $default_los
	} else {
	    set comb_control($id,id) [lindex $comb_defs 2]
	    set comb_control($id,air) [lindex $comb_defs 3]
	    set comb_control($id,material) [lindex $comb_defs 4]
	    set comb_control($id,los) [lindex $comb_defs 5]
	}
	set comb_control($id,color) [lindex $comb_defs 6]
	set comb_control($id,shader) [lindex $comb_defs 7]
	set comb_control($id,inherit) [lindex $comb_defs 8]
	set comb_control($id,comb) [lindex $comb_defs 9]
    } else {
	set comb_control($id,color) [lindex $comb_defs 2]
	set comb_control($id,shader) [lindex $comb_defs 3]
	set comb_control($id,inherit) [lindex $comb_defs 4]
	set comb_control($id,comb) [lindex $comb_defs 5]
    }

    if {$comb_control($id,color) == ""} {
	set comb_control($id,color) [$top cget -bg]
	color_entry_update $top color comb_control($id,color) $comb_control($id,color)
	set comb_control($id,color) ""
    } else {
	color_entry_update $top color comb_control($id,color) $comb_control($id,color)
    }

    $top.combT delete 0.0 end
    $top.combT insert end $comb_control($id,comb)

    if {$save_isRegion != $comb_control($id,isRegion)} {
	comb_toggle_isRegion $id
    }

    if { [llength $comb_control($id,shader)] > 0 } {
	set comb_control($id,shader_gui) [do_shader comb_control($id,shader) $id \
		$comb_control($id,shader_frame)]
	grid $comb_control($id,shader_frame) \
		-row 1 \
		-sticky "nsew" \
		-in $top.shader_top_frame \
		-padx $comb_control($id,padx) \
		-pady $comb_control($id,pady)
	grid rowconfigure $top.shader_top_frame 0 -weight 0
	grid rowconfigure $top.shader_top_frame 1 -weight 1
	grid columnconfigure $top.shader_top_frame 0 -weight 1
	grid columnconfigure $top.shader_frame 0 -weight 1
    } else {
	grid forget $comb_control($id,shader_frame)
	catch { destroy $comb_control($id,shader_gui) }
    }

    set comb_control($id,dirty_name) 0
}

# called when "Dismiss" button is pressed
proc comb_dismiss {id top} {
    global comb_control

    # destroy it all
    catch {destroy $top}
    catch {destroy $comb_control($id,shader_gui)}
    trace vdelete comb_control($id,name) w "comb_handle_trace $id"
}

# called when "Is Region" checkbutton is pressed
proc comb_toggle_isRegion { id } {
    global comb_control

    set top .$id.comb
    grid remove $top.id_los_air_mater

    if {$comb_control($id,isRegion) == "Yes"} {
	grid $top.id_los_air_mater -in $comb_control($id,bool_frame) \
	    -padx $comb_control($id,padx) \
	    -pady $comb_control($id,pady) \
	    -sticky new -row 0 -column 0
    } else {
	grid forget $top.id_los_air_mater
    }
}

proc comb_shader_gui { id shader_type } {
    global comb_control

    set top .$id.comb
    set current_shader_type [lindex $comb_control($id,shader) 0]

    if { $current_shader_type != $shader_type } {
	set comb_control($id,shader) $shader_type
    }

    set comb_control($id,shader_gui) [do_shader comb_control($id,shader) $id \
	    $comb_control($id,shader_frame)]
    grid $comb_control($id,shader_frame) \
	    -row 1 \
	    -sticky "nsew" \
	    -in $top.shader_top_frame \
	    -padx $comb_control($id,padx) \
	    -pady $comb_control($id,pady)

    grid rowconfigure $top.shader_top_frame 0 -weight 0
    grid rowconfigure $top.shader_top_frame 1 -weight 1
    grid columnconfigure $top.shader_top_frame 0 -weight 1
    grid columnconfigure $top.shader_frame 0 -weight 1
}

proc db_exist {obj} {
    set ret [catch {db get $obj}]
    if {$ret} {
	return 0
    } else {
	return 1
    }
}

# mark name as dirty if someone edits it
proc comb_handle_trace {id name1 name2 op} {
    global comb_control

    set comb_control($id,dirty_name) 1
}

#proc comb_select_material { id } {
#    global mged_gui
#    global comb_control
#
#    set top .$id.sel_material
#
#    if [winfo exists $top] {
#	raise $top
#	return
#    }
#
#    toplevel $top -screen $mged_gui($id,screen)
#
#    frame $top.gridF1
#    frame $top.gridF3
#    frame $top.gridF4 -relief groove -bd 2
#
#    listbox $top.materialLB -selectmode single -yscrollcommand "$top.materialS set"
#    scrollbar $top.materialS -relief flat -command "$top.materialLB yview"
#
#    label $top.materialL -text "Material List:" -anchor w
#    entry $top.materialE -width 12 -textvar comb_control($id,material_list)
#
#    button $top.resetB -relief raised -text "Reset"\
#	    -command "load_material $id"
#    button $top.dismissB -relief raised -text "Dismiss"\
#	    -command "catch { destroy $top }"
#
#    grid $top.materialLB $top.materialS -sticky "nsew" -in $top.gridF
#    grid rowconfigure $top.gridF1 0 -weight 1
#    grid columnconfigure $top.gridF1 0 -weight 1
#
#    grid $top.materialL x -sticky "ew" -in $top.gridF3
#    grid $top.materialE $top.resetB -sticky "nsew" -in $top.gridF3
#    grid columnconfigure $top.gridF3 0 -weight 1
#
#    grid $top.dismissB -in $top.gridF4 -pady $comb_control($id,pady)
#
#    grid $top.gridF1 -sticky "nsew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
#    grid $top.gridF3 -sticky "ew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
#    grid $top.gridF4 -sticky "ew"
#    grid rowconfigure $top 0 -weight 1
#    grid columnconfigure $top 0 -weight 1
#
#    wm title $top "Select Material"
#}
#
#proc load_material { id } {
#    global comb_control
#}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
