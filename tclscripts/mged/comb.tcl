#
#			C O M B . T C L
#
#	Widget for editing a combination.
#
#	Author - Robert G. Parker
#

check_externs "get_comb put_comb"

proc init_comb { id } {
    global player_screen
    global mged_active_dm
    global comb_control

    set top .$id.comb

    if [winfo exists $top] {
	raise $top
	return
    }

    # set the padding
    set comb_control($id,padx) 4
    set comb_control($id,pady) 2

    set comb_control($id,name) ""
    set comb_control($id,isRegion) "Yes"
    set comb_control($id,id) ""
    set comb_control($id,air) ""
    set comb_control($id,material) ""
    set comb_control($id,los) ""
    set comb_control($id,color) ""
    set comb_control($id,inherit) ""
    set comb_control($id,comb) ""
    set comb_control($id,shader) ""
    set comb_control($id,shader_gui) ""

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2
    frame $top.gridF3
    frame $top.gridF4
    frame $top.nameF
    frame $top.nameFF -relief sunken -bd 2
    frame $top.idF
    frame $top.idFF -relief sunken -bd 2
    frame $top.airF
    frame $top.airFF -relief sunken -bd 2
    frame $top.materialF
    frame $top.materialFF -relief sunken -bd 2
    frame $top.losF
    frame $top.losFF -relief sunken -bd 2
    frame $top.color_F
    frame $top.shaderF -relief groove -bd 2
    frame $top.shaderFF -relief sunken -bd 2
    frame $top.combF
    frame $top.combFF -relief sunken -bd 2

    set comb_control($id,shader_frame) $top.shaderF

    label $top.nameL -text "Name" -anchor w
    set hoc_data { { summary "The combination name is the name of a
region or a group. The region flag must be set
in order for the combination to be a region.
Note that a region defines a space containing
homogeneous material. In contrast, a group can
contain many different materials." } }
    hoc_register_data $top.nameL "Combination Name" $hoc_data
    entry $top.nameE -relief flat -width 12 -textvar comb_control($id,name)
    hoc_register_data $top.nameE "Combination Name" $hoc_data
    menubutton $top.nameMB -relief raised -bd 2\
	    -menu $top.nameMB.m -indicatoron 1
    hoc_register_data $top.nameMB "Combination Selection"\
	    { { summary "This pops up a menu of methods for selecting
a combination. At the moment there are two methods.
The first brings up a listbox containing the combinations
currently being displayed in the geometry window. The
user can select from among the combinations listed here.
The second selection method allows the user to use the
mouse to fire a ray at the combination of interest. If
only one combination is hit, that combination is selected.
If nothing is hit the user can simply fire another ray,
perhaps taking better aim :-). If more than one combination
is hit, a listbox of the hit combinations is presented
as with the first selection method. Note - When selecting
items from a listbox, a left buttonpress highlights the combination
in question until the button is released. To select a combination,
double click with the left mouse button." } }
    menu $top.nameMB.m -title "Combination Selection Method" -tearoff 0
    $top.nameMB.m add command -label "Select from all displayed"\
	    -command "winset \$mged_active_dm($id); build_comb_menu_all"
    hoc_register_menu_data "Combination Selection Method" "Select from all displayed"\
	    "Select Combination From All Displayed"\
	    { { summary "This pops up a listbox containing the combinations
currently being displayed in the geometry window. The
user can select from among the combinations listed here.
Note - When selecting items from a listbox, a left buttonpress
highlights the combination in question until the button is
released. To select a combination, double click with the left
mouse button." } }
    $top.nameMB.m add command -label "Select along ray"\
	    -command "winset \$mged_active_dm($id); set mouse_behavior c"
    hoc_register_menu_data "Combination Selection Method" "Select along ray"\
	    "Select Combination Along Ray"\
	    { { summary "This method allows the user to use the
mouse to fire a ray at the combination of interest.
If only one combination is hit, that combination is
selected. If nothing is hit the user can simply fire
another ray, perhaps taking better aim :-). If more
than one combination is hit, a listbox of the hit
combinations is presented. Note - When selecting
items from a listbox, a left buttonpress highlights
the combination in question until the button is
released. To select a combination, double click with
the left mouse button." } }

    label $top.idL -text "Region Id" -anchor w
    set hoc_data { { summary "The region id (i.e. item code) is a number
that is typically used for grouping regions
belonging to a particular component. If the
region id is non-zero it is considered to be
a model component. Otherwise, it is considered
to be air. The air code is then used to designate
the kind of air." } }
    hoc_register_data $top.idL "Region Id" $hoc_data
    entry $top.idE -relief flat -width 12 -textvar comb_control($id,id)
    hoc_register_data $top.idE "Region Id" $hoc_data

    label $top.airL -text "Air Code" -anchor w
    set hoc_data { { summary "The air code is a number that is typically
used to designate the kind of air a region
represents. An air code of \"1\" signifies
universal air. An air code that is greater
than \"1\" signifies some other kind of air.
While an air code of \"0\" means that the
region represents a component." } }
    hoc_register_data $top.airL "Air Code" $hoc_data
    entry $top.airE -relief flat -width 12 -textvar comb_control($id,air)
    hoc_register_data $top.airE "Air Code" $hoc_data

    label $top.materialL -text "Material Id" -anchor w
    set hoc_data { { summary "The material id represents a particular
material type as identified by a material
database. In the past, the gift material
database was used to identify the material
type." } }
    hoc_register_data $top.materialL "Material Id" $hoc_data
    entry $top.materialE -relief flat -width 12 -textvar comb_control($id,material)
    hoc_register_data $top.materialE "Material Id" $hoc_data

    label $top.losL -text "LOS" -anchor w
    set hoc_data { { summary "LOS is a number that represents the
percentage of material a component region
is composed of. For example, if some component
region is defined to be made of \"mild steel\", as
designated by its material id, with an LOS of 20
then the region is considered to be composed of
20% \"mild steel\"."  } }
    hoc_register_data $top.losL "LOS" $hoc_data
    entry $top.losE -relief flat -width 12 -textvar comb_control($id,los)
    hoc_register_data $top.losE "LOS" $hoc_data

    label $top.colorL -text "Color" -anchor w
    hoc_register_data $top.colorL "Color"\
	    { { summary "Material color is used by the ray tracer
when rendering. It is also used by MGED
when determining what color to use when
drawing an object" } }
    # $top.colorF is the name of the container created by color_entry_build
    # that contains the entry and menubutton for specifying a color
    color_entry_build $top color comb_control($id,color)\
	    "color_entry_chooser $id $top color \"Combination Color\"\
	    comb_control $id,color"\
	    12 $comb_control($id,color)

    label $top.shaderL -text "Shader" -anchor w
    set hoc_data { { summary "Use this to manually enter the shader
parameters. Note - when entering the
shader parameters directly, the shader
GUI will automatically be updated." } }
    hoc_register_data $top.shaderL "Shader" $hoc_data
    entry $top.shaderE -relief flat -width 12 -textvar comb_control($id,shader)
    hoc_register_data $top.shaderE "Shader" $hoc_data

    bind $top.shaderE <KeyRelease> "set_shader_params comb_control($id,shader) $id"

    menubutton $top.shaderMB -relief raised -bd 2\
	    -menu $top.shaderMB.m -indicatoron 1
    hoc_register_data $top.shaderMB "Menu of Shaders"\
	    { { summary "This pops up a menu of shader types.
Selecting a shader from the menu will
reconfigure the shader's GUI to take on
the form of the selected shader type." } }
    menu $top.shaderMB.m -title "Shader" -tearoff 0
    $top.shaderMB.m add command -label plastic\
	    -command "comb_shader_gui $id plastic $top.shaderF"
    hoc_register_menu_data "Shader" plastic "Shader - Plastic"\
	    { { summary "Set shader parameters to make this object appear as plastic." } }
    $top.shaderMB.m add command -label mirror\
	    -command "comb_shader_gui $id mirror $top.shaderF"
    hoc_register_menu_data "Shader" mirror "Shader - Mirror"\
	    { { summary "Set shader parameters to make this object appear as a mirror." } }
    $top.shaderMB.m add command -label glass\
	    -command "comb_shader_gui $id glass $top.shaderF"
    hoc_register_menu_data "Shader" glass "Shader - Glass"\
	    { { summary "Set shader parameters to make this object appear as glass." } }
    $top.shaderMB.m add command -label "texture (color)"\
	    -command "comb_shader_gui $id texture $top.shaderF"
    hoc_register_menu_data "Shader" "texture (color)" "Shader - Texture (color)"\
	    { { summary "Map a color texture on this object." } }
    $top.shaderMB.m add command -label "texture (b/w)"\
	    -command "comb_shader_gui $id bwtexture $top.shaderF"
    hoc_register_menu_data "Shader" "texture (b/w)" "Shader - Texture (b/w)"\
	    { { summary "Map a black and white texture on this object." } }
    $top.shaderMB.m add command -label "bump map"\
	    -command "comb_shader_gui $id bump $top.shaderF"
    hoc_register_menu_data "Shader" "bump map" "Shader - Bump"\
		{ { summary "Apply a bump map to perturb the surface normals of this object." } }
    $top.shaderMB.m add command -label "checker"\
	    -command "comb_shader_gui $id checker $top.shaderF"
    hoc_register_menu_data "Shader" "checker" "Shader - Checker"\
		{ { summary "texture map a checkerboard pattern on this object." } }
    $top.shaderMB.m add command -label "test map"\
	    -command "comb_shader_gui $id testmap $top.shaderF"
    hoc_register_menu_data "Shader" "test map" "Shader - testmap"\
		{ { summary "Map a red and blue gradient on this object proportional to 'uv'\n\
			texture map coordinates." } }
    $top.shaderMB.m add command -label "fake star pattern"\
	    -command "comb_shader_gui $id fakestar $top.shaderF"
    hoc_register_menu_data "Shader" "fake star pattern" "Shader - fakestar"\
		{ { summary "Map a fake star field on this object." } }
    $top.shaderMB.m add command -label "cloud"\
	-command "comb_shader_gui $id cloud $top.shaderF"
    hoc_register_menu_data "Shader" "cloud" "Shader - cloud"\
	{ { summary "Map a cloud texture on this object." } }
    $top.shaderMB.m add command -label "stack"\
	-command "comb_shader_gui $id stack $top.shaderF"
    hoc_register_menu_data "Shader" "stack" "Shader - stack"\
	{ { summary "Consecutively apply multiple shaders to this object." } }
    $top.shaderMB.m add command -label "envmap"\
	-command "comb_shader_gui $id envmap $top.shaderF"
    hoc_register_menu_data "Shader" "envmap" "Shader - envmap"\
	{ { summary "Apply an environment map using this region." } }

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
    text $top.combT -relief sunken -bd 2 -width 40 -height 10\
	    -yscrollcommand "$top.gridF3.s set" -setgrid true
    hoc_register_data $top.combT "Edit Boolean Expression" $hoc_data
    scrollbar $top.gridF3.s -relief flat -command "$top.combT yview"

    checkbutton $top.isRegionCB -relief raised -text "Is Region"\
	    -offvalue No -onvalue Yes -variable comb_control($id,isRegion)\
	    -command "comb_toggle_isRegion $id"
    hoc_register_data $top.isRegionCB "Is Region"\
	    { { summary "Toggle the region flag on/off. If the
region flag is toggled on (i.e. checkbutton
is highligted) the GUI reconfigures itself to
handle regions. If the region flag is toggled
off the GUI reconfigures itself to handle
groups. Note - both regions and groups are
combinations. However, a region is special
in that it has its region flag set. It is
also required to be homogeneous (i.e. consisting
of the same material)." } }
    checkbutton $top.inheritCB -relief raised -text "Inherit"\
	    -offvalue No -onvalue Yes -variable comb_control($id,inherit)
    hoc_register_data $top.inheritCB "Inherit"\
	    { { summary "Toggle inheritance on/off. If inheritance
is on, everything in the group takes on
the characteristics of the group. Note -
setting the inheritance for a region has
no effect (i.e. the members of a region
always take on the characteristics of the
region)." } }

    button $top.okB -relief raised -text "Ok"\
	    -command "comb_ok $id $top"
    hoc_register_data $top.okB "Ok"\
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
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "comb_dismiss $id $top"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss (close) the \"Combination Editor\"." } }

    grid $top.nameL -sticky "ew" -in $top.nameF
    grid $top.nameE $top.nameMB -sticky "ew" -in $top.nameFF
    grid $top.nameFF -sticky "ew" -in $top.nameF
    grid $top.idL -sticky "ew" -in $top.idF
    grid $top.idE -sticky "ew" -in $top.idFF
    grid $top.idFF -sticky "ew" -in $top.idF
    grid $top.nameF x $top.idF -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
    grid columnconfigure $top.nameF 0 -weight 1
    grid columnconfigure $top.nameFF 0 -weight 1
    grid columnconfigure $top.idF 0 -weight 1
    grid columnconfigure $top.idFF 0 -weight 1

    grid $top.colorL -sticky "ew" -in $top.color_F
    grid $top.colorF -sticky "ew" -in $top.color_F
    grid $top.airL -sticky "ew" -in $top.airF
    grid $top.airE -sticky "ew" -in $top.airFF
    grid $top.airFF -sticky "ew" -in $top.airF
    grid $top.color_F x $top.airF -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
    grid columnconfigure $top.color_F 0 -weight 1
    grid columnconfigure $top.airF 0 -weight 1
    grid columnconfigure $top.airFF 0 -weight 1

    grid $top.materialL -sticky "ew" -in $top.materialF
    grid $top.materialE -sticky "ew" -in $top.materialFF
    grid $top.materialFF -sticky "ew" -in $top.materialF
    grid $top.losL -sticky "ew" -in $top.losF
    grid $top.losE -sticky "ew" -in $top.losFF
    grid $top.losFF -sticky "ew" -in $top.losF
    grid $top.materialF x $top.losF -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
    grid columnconfigure $top.materialF 0 -weight 1
    grid columnconfigure $top.materialFF 0 -weight 1
    grid columnconfigure $top.losF 0 -weight 1
    grid columnconfigure $top.losFF 0 -weight 1

    grid $top.shaderL -sticky "ew" -in $top.shaderF\
	    -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid $top.shaderE $top.shaderMB -sticky "ew" -in $top.shaderFF
    grid $top.shaderFF -sticky "ew" -in $top.shaderF\
	    -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid $top.shaderF - - -sticky "ew" -in $top.gridF -pady [expr $comb_control($id,pady) + 4]
#    grid $top.selectMaterialB -row 3 -column 2 -sticky "sw" -in $top.gridF -pady $comb_control($id,pady)
    grid columnconfigure $top.shaderF 0 -weight 1
    grid columnconfigure $top.shaderFF 0 -weight 1

    grid $top.isRegionCB $top.inheritCB x -sticky "ew" -in $top.gridF2\
	    -ipadx 4 -ipady 4

    grid columnconfigure $top.gridF 0 -weight 1
    grid columnconfigure $top.gridF 1 -minsize 20
    grid columnconfigure $top.gridF 2 -weight 1
    grid columnconfigure $top.gridF2 2 -weight 1

    grid $top.combL x -sticky "w" -in $top.gridF3
    grid $top.combT $top.gridF3.s -sticky "nsew" -in $top.gridF3
    grid rowconfigure $top.gridF3 1 -weight 1
    grid columnconfigure $top.gridF3 0 -weight 1

    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew"\
	    -in $top.gridF4 -pady $comb_control($id,pady)
    grid columnconfigure $top.gridF4 2 -weight 1
    grid columnconfigure $top.gridF4 4 -weight 1

    grid $top.gridF -sticky "ew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid $top.gridF2 -sticky "ew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid $top.gridF3 -sticky "nsew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid $top.gridF4 -sticky "ew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
    grid rowconfigure $top 2 -weight 1
    grid columnconfigure $top 0 -weight 1

    bind $top.nameE <Return> "comb_reset $id; break"

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Combination Editor ($id)"
}

proc comb_ok { id top } {
    comb_apply $id
    comb_dismiss $id $top
}

proc comb_apply { id } {
    global player_screen
    global comb_control

    set top .$id.comb
    set comb_control($id,comb) [$top.combT get 0.0 end]

# apply the parameters in the shader frame to the shader string
    if { $comb_control($id,shader) != "" } then {
	do_shader_apply comb_control($id,shader) $id
    }

    if {$comb_control($id,isRegion)} {
	if {$comb_control($id,id) < 0} {
	    cad_dialog .$id.combDialog $player_screen($id)\
		    "Bad region id!"\
		    "Region id must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$comb_control($id,air) < 0} {
	    cad_dialog .$id.combDialog $player_screen($id)\
		    "Bad air code!"\
		    "Air code must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$comb_control($id,id) == 0 && $comb_control($id,air) == 0} {
	    cad_dialog .$id.combDialog $player_screen($id)\
		    "Warning: both region id and air code are 0"\
		    "Warning: both region id and air code are 0"\
		    "" 0 OK
	}

	set result [catch {put_comb $comb_control($id,name) $comb_control($id,isRegion)\
		$comb_control($id,id) $comb_control($id,air) $comb_control($id,material) $comb_control($id,los)\
		$comb_control($id,color) $comb_control($id,shader) $comb_control($id,inherit)\
		$comb_control($id,comb)} comb_error]

	if {$result} {
	    return $comb_error
	}

	return
    }

    set result [catch {put_comb $comb_control($id,name) $comb_control($id,isRegion)\
	    $comb_control($id,color) $comb_control($id,shader) $comb_control($id,inherit)\
	    $comb_control($id,comb)} comb_error]

    if {$result} {
	return $comb_error
    }
}

proc comb_reset { id } {
    global player_screen
    global comb_control

    set top .$id.comb

    if {$comb_control($id,name) == ""} {
	cad_dialog .$id.combDialog $player_screen($id)\
		"You must specify a region/combination name!"\
		"You must specify a region/combination name!"\
		"" 0 OK
	return
    }

    set save_isRegion $comb_control($id,isRegion)
    set result [catch {get_comb $comb_control($id,name)} comb_defs]
    if {$result == 1} {
	cad_dialog .$id.combDialog $player_screen($id)\
		"comb_reset: Error"\
		$comb_defs\
		"" 0 OK
	return
    }

    set comb_control($id,isRegion) [lindex $comb_defs 1]

    if {$comb_control($id,isRegion) == "Yes"} {
	set comb_control($id,id) [lindex $comb_defs 2]
	set comb_control($id,air) [lindex $comb_defs 3]
	set comb_control($id,material) [lindex $comb_defs 4]
	set comb_control($id,los) [lindex $comb_defs 5]
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

    color_entry_update $top color $comb_control($id,color)
    $top.combT delete 0.0 end
    $top.combT insert end $comb_control($id,comb)

    if {$save_isRegion != $comb_control($id,isRegion)} {
	comb_toggle_isRegion $id
    }

    if { [llength $comb_control($id,shader)] > 0 } {
	set comb_control($id,shader_gui) [do_shader comb_control($id,shader) $id $comb_control($id,shader_frame)]
    } else {
	catch { destroy $comb_control($id,shader_gui) }
    }
}

proc comb_dismiss { id top } {
    global comb_control

    catch { destroy $top }
    catch { destroy $comb_control($id,shader_gui) }
}

proc comb_toggle_isRegion { id } {
    global comb_control

    set top .$id.comb
    grid remove $top.gridF

    if {$comb_control($id,isRegion) == "Yes"} {
	grid forget $top.nameF $top.color_F $top.shaderF

	frame $top.idF
	frame $top.idFF -relief sunken -bd 2
	frame $top.airF
	frame $top.airFF -relief sunken -bd 2
	frame $top.materialF
	frame $top.materialFF -relief sunken -bd 2
	frame $top.losF
	frame $top.losFF -relief sunken -bd 2

	label $top.idL -text "Region Id" -anchor w
	entry $top.idE -relief flat -width 12 -textvar comb_control($id,id)

	label $top.airL -text "Air Code" -anchor w
	entry $top.airE -relief flat -width 12 -textvar comb_control($id,air)

	label $top.materialL -text "Material" -anchor w
	entry $top.materialE -relief flat -width 12 -textvar comb_control($id,material)

	label $top.losL -text "LOS" -anchor w
	entry $top.losE -relief flat -width 12 -textvar comb_control($id,los)

	grid $top.idL -sticky "ew" -in $top.idF
	grid $top.idE -sticky "ew" -in $top.idFF
	grid $top.idFF -sticky "ew" -in $top.idF
	grid $top.nameF x $top.idF -sticky "ew" -row 0 -in $top.gridF -pady $comb_control($id,pady)
	grid columnconfigure $top.idF 0 -weight 1
	grid columnconfigure $top.idFF 0 -weight 1

	grid $top.airL -sticky "ew" -in $top.airF
	grid $top.airE -sticky "ew" -in $top.airFF
	grid $top.airFF -sticky "ew" -in $top.airF
	grid $top.color_F x $top.airF -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
	grid columnconfigure $top.airF 0 -weight 1
	grid columnconfigure $top.airFF 0 -weight 1

	grid $top.materialL -sticky "ew" -in $top.materialF
	grid $top.materialE -sticky "ew" -in $top.materialFF
	grid $top.materialFF -sticky "ew" -in $top.materialF
	grid $top.losL -sticky "ew" -in $top.losF
	grid $top.losE -sticky "ew" -in $top.losFF
	grid $top.losFF -sticky "ew" -in $top.losF
	grid $top.materialF x $top.losF -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)

	grid $top.shaderF - - -sticky "ew" -in $top.gridF -pady [expr $comb_control($id,pady) + 4]
	grid columnconfigure $top.losF 0 -weight 1
	grid columnconfigure $top.losFF 0 -weight 1

	grid columnconfigure $top.materialF 0 -weight 1
	grid columnconfigure $top.materialFF 0 -weight 1
    } else {
	grid forget $top.nameF $top.idF $top.airF $top.materialF $top.losF\
		$top.color_F $top.shaderF

	destroy $top.idL $top.idE
	destroy $top.airL $top.airE
	destroy $top.materialL $top.materialE
	destroy $top.losL $top.losE
	destroy $top.idF $top.idFF
	destroy $top.airF $top.airFF
	destroy $top.materialF $top.materialFF
	destroy $top.losF $top.losFF

	grid $top.nameF - - -sticky "ew" -row 0 -in $top.gridF -pady $comb_control($id,pady)
	grid $top.color_F - - -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
	grid $top.shaderF - - -sticky "ew" -in $top.gridF -pady $comb_control($id,pady)
    }

    grid $top.gridF
}

proc comb_shader_gui { id shader_type shader_frame } {
    global comb_control

    set current_shader_type [lindex $comb_control($id,shader) 0]

    if { $current_shader_type != $shader_type } {
	set comb_control($id,shader) $shader_type
    }

    set comb_control($id,shader_gui) [do_shader comb_control($id,shader) $id $shader_frame]
}

#proc comb_select_material { id } {
#    global player_screen
#    global comb_control
#
#    set top .$id.sel_material
#
#    if [winfo exists $top] {
#	raise $top
#	return
#    }
#
#    toplevel $top -screen $player_screen($id)
#
#    frame $top.gridF
#    frame $top.gridF2
#    frame $top.gridF3 -relief groove -bd 2
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
#    grid rowconfigure $top.gridF 0 -weight 1
#    grid columnconfigure $top.gridF 0 -weight 1
#
#    grid $top.materialL x -sticky "ew" -in $top.gridF2
#    grid $top.materialE $top.resetB -sticky "nsew" -in $top.gridF2
#    grid columnconfigure $top.gridF2 0 -weight 1
#
#    grid $top.dismissB -in $top.gridF3 -pady $comb_control($id,pady)
#
#    grid $top.gridF -sticky "nsew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
#    grid $top.gridF2 -sticky "ew" -padx $comb_control($id,padx) -pady $comb_control($id,pady)
#    grid $top.gridF3 -sticky "ew"
#    grid rowconfigure $top 0 -weight 1
#    grid columnconfigure $top 0 -weight 1
#
#    wm title $top "Select Material"
#}
#
#proc load_material { id } {
#    global comb_control
#}
