# 
#                        R T . T C L
#
#	Widget for raytracing MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_rt"

proc init_Raytrace { id } {
    global mged_gui
    global fb
    global rt_control
    global env

    set top .$id.rt

    if [winfo exists $top] {
	raise $top
	return
    }

    winset $mged_gui($id,active_dm)
    rt_init_vars $id $top $top\AS

    toplevel $top -screen $mged_gui($id,screen) -menu $top.menubar

    frame $top.gridF1
    frame $top.gridF2 -relief groove -bd 2
    frame $top.gridF3
    frame $top.gridF4
    frame $top.srcF -relief sunken -bd 2
    frame $top.destF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2

    menu $top.menubar -tearoff 0
    $top.menubar add cascade -label "Framebuffer" -underline 0 -menu $top.menubar.fb
    $top.menubar add cascade -label "Objects" -underline 0 -menu $top.menubar.obj

    menu $top.menubar.fb -title "Framebuffer" -tearoff 0
    $top.menubar.fb add checkbutton -offvalue 0 -onvalue 1 -variable rt_control($id,fb)\
	    -label "Active" -underline 0 \
	    -command "rt_set_fb $id"
    hoc_register_menu_data "Framebuffer" "Active" "Framebuffer Active"\
	    { { summary "This activates the framebuffer." } }
    $top.menubar.fb add separator
    $top.menubar.fb add radiobutton -value 1 -variable rt_control($id,fb_all)\
	    -label "All" -underline 0\
	    -command "rt_set_fb_all $id"
    hoc_register_menu_data "Framebuffer" "All" "Framebuffer - All"\
	    { { summary "Use the entire pane as a framebuffer." } }
    $top.menubar.fb add radiobutton -value 0 -variable rt_control($id,fb_all)\
	    -label "Rectangle Area" -underline 0\
	    -command "rt_set_fb_all $id"
    hoc_register_menu_data "Framebuffer" "Rectangle Area" "Framebuffer - Rectangle Area"\
	    { { summary "Use only the rectangular area for the framebuffer." } }
    $top.menubar.fb add separator
    $top.menubar.fb add radiobutton -value 1 -variable rt_control($id,fb_overlay)\
	    -label "Overlay" -underline 0\
	    -command "rt_set_fb_overlay $id"
    hoc_register_menu_data "Framebuffer" "Overlay" "Framebuffer - Overlay"\
	    { { summary "Draw the framebuffer on top of the geometry." } }
    $top.menubar.fb add radiobutton -value 0 -variable rt_control($id,fb_overlay)\
	    -label "Underlay" -underline 0\
	    -command "rt_set_fb_overlay $id"
    hoc_register_menu_data "Framebuffer" "Underlay" "Framebuffer - Underlay"\
	    { { summary "Draw the framebuffer under the geometry." } }

    menu $top.menubar.obj -title "Objects" -tearoff 0
    $top.menubar.obj add radiobutton -value one -variable rt_control($id,omode)\
	    -label "one" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    $top.menubar.obj add radiobutton -value several -variable rt_control($id,omode)\
	    -label "several" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    $top.menubar.obj add radiobutton -value all -variable rt_control($id,omode)\
	    -label "all" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    $top.menubar.obj add separator
    $top.menubar.obj add command -label "edit list"\
	    -command "rt_edit_olist $id"
    $top.menubar.obj add command -label "clear list"\
	    -command "set rt_control($id,olist) {}"

    label $top.srcL -text "Source" -anchor e
    entry $top.srcE -relief flat -width 12 -textvar rt_control($id,raw_src)
    bind $top.srcE <KeyRelease> "rt_cook_src $id \$rt_control($id,raw_src)"
    menubutton $top.srcMB -relief raised -bd 2\
	    -menu $top.srcMB.menu -indicatoron 1
    menu $top.srcMB.menu -title "Source" -tearoff 0
    $top.srcMB.menu add command -label "Active Pane"\
	    -command "rt_cook_src $id \$mged_gui($id,active_dm)"
    $top.srcMB.menu add separator
    $top.srcMB.menu add command -label "Upper Left"\
	    -command "rt_cook_src $id $mged_gui($id,top).ul"
    hoc_register_menu_data "Source" "Upper Left" "Source - Upper Left"\
	    { { summary "Set the source to \"Upper Left\"." } }
    $top.srcMB.menu add command -label "Upper Right"\
	    -command "rt_cook_src $id $mged_gui($id,top).ur"
    hoc_register_menu_data "Source" "Upper Right" "Source - Upper Right"\
	    { { summary "Set the source to \"Upper Right\"." } }
    $top.srcMB.menu add command -label "Lower Left"\
	    -command "rt_cook_src $id $mged_gui($id,top).ll"
    hoc_register_menu_data "Source" "Lower Left" "Source - Lower Left"\
	    { { summary "Set the source to \"Lower Left\"." } }
    $top.srcMB.menu add command -label "Lower Right"\
	    -command "rt_cook_src $id $mged_gui($id,top).lr"
    hoc_register_menu_data "Source" "Lower Right" "Source - Lower Right"\
	    { { summary "Set the source to \"Lower Right\"." } }

    label $top.destL -text "Destination" -anchor e
    entry $top.destE -relief flat -width 12 -textvar rt_control($id,raw_dest)
    bind $top.destE <KeyRelease> "rt_cook_dest $id \$rt_control($id,raw_dest)"
    menubutton $top.destMB -relief raised -bd 2\
	    -menu $top.destMB.menu -indicatoron 1
    menu $top.destMB.menu -title "Destination" -tearoff 0
    $top.destMB.menu add command -label "Active Pane"\
	    -command "rt_cook_dest $id \$mged_gui($id,active_dm)"
    $top.destMB.menu add separator
    $top.destMB.menu add command -label "Upper Left"\
	    -command "rt_cook_dest $id $mged_gui($id,top).ul"
    $top.destMB.menu add command -label "Upper Right"\
	    -command "rt_cook_dest $id $mged_gui($id,top).ur"
    $top.destMB.menu add command -label "Lower Left"\
	    -command "rt_cook_dest $id $mged_gui($id,top).ll"
    $top.destMB.menu add command -label "Lower Right"\
	    -command "rt_cook_dest $id $mged_gui($id,top).lr"
    $top.destMB.menu add separator
    if {[info exists env(FB_FILE)] && $env(FB_FILE) != ""} {
	$top.destMB.menu add command -label "$env(FB_FILE)"\
		-command "rt_cook_dest $id $env(FB_FILE)"
    }

    set dbname [rt_db_to_pix]
    if {$dbname != ""} {
	$top.destMB.menu add command -label $dbname\
		-command "rt_cook_dest $id $dbname"
    }

    label $top.sizeL -text "Size" -anchor e
    hoc_register_data $top.sizeL "Size"\
	    { { summary "Indicates the size of the framebuffer.
This defaults to the size of the active pane." } }
    entry $top.sizeE -relief flat -width 12 -textvar rt_control($id,size)
    hoc_register_data $top.sizeE "Size"\
	    { { summary "Enter the framebuffer size." } }
    menubutton $top.sizeMB -relief raised -bd 2\
	    -menu $top.sizeMB.sizeM -indicatoron 1
    hoc_register_data $top.sizeMB "Size"\
	    { { summary "Pops up a menu of popular framebuffer sizes." } }
    menu $top.sizeMB.sizeM -title "Size" -tearoff 0
    $top.sizeMB.sizeM add command -label "Size of Pane"\
	    -command "rt_set_fb_size $id"
    hoc_register_menu_data "Size" "Size of Pane" "Size of Pane"\
	    { { summary "Set the framebuffer size to be the
same size as the active pane." } }
    $top.sizeMB.sizeM add command -label 128\
	    -command "set rt_control($id,size) 128"
    hoc_register_menu_data "Size" 128 "Size - 128x128"\
	    { { summary "Set the framebuffer size to 128x128." } }
    $top.sizeMB.sizeM add command -label 256\
	    -command "set rt_control($id,size) 256"
    hoc_register_menu_data "Size" 256 "Size - 256x256"\
	    { { summary "Set the framebuffer size to 256x256." } }
    $top.sizeMB.sizeM add command -label 512\
	    -command "set rt_control($id,size) 512"
    hoc_register_menu_data "Size" 512 "Size - 512x512"\
	    { { summary "Set the framebuffer size to 512x512." } }
    $top.sizeMB.sizeM add command -label 640x480\
	    -command "set rt_control($id,size) 640x480"
    hoc_register_menu_data "Size" 640x480 "Size - 640x480"\
	    { { summary "Set the framebuffer size to 640x480." } }
    $top.sizeMB.sizeM add command -label 720x486\
	    -command "set rt_control($id,size) 720x486"
    hoc_register_menu_data "Size" 720x486 "Size - 720x486"\
	    { { summary "Set the framebuffer size to 720x486." } }
    $top.sizeMB.sizeM add command -label 1024\
	    -command "set rt_control($id,size) 1024"
    hoc_register_menu_data "Size" 1024 "Size - 1024x1024"\
	    { { summary "Set the framebuffer size to 1024x1024." } }

    label $top.colorL -text "Background Color" -anchor e
    hoc_register_data $top.colorL "Background Color"\
	    { { summary "This refers to the background color
used for raytracing. This is also the color
that is used when clearing the framebuffer." } }

    # $top.colorF is the name of the container created by color_entry_build
    # that contains the entry and menubutton for specifying a color
    color_entry_build $top color rt_control($id,color)\
	    "color_entry_chooser $id $top color \"Background Color\"\
	    rt_control $id,color"\
	    12 $rt_control($id,color)

    button $top.advancedB -relief raised -text "Advanced Settings..."\
	    -command "do_Advanced_Settings $id"
    hoc_register_data $top.advancedB "Advanced Settings"\
	    { { summary "Pops up another GUI for advanced settings." } }
    button $top.okB -relief raised -text "Ok"\
	    -command "rt_ok $id $top"
    button $top.raytraceB -relief raised -text "Raytrace" \
	    -command "do_Raytrace $id"
    hoc_register_data $top.raytraceB "Raytrace"\
	    { { summary "Begin raytracing the current view of
the active pane. The results of the raytrace
will go either to the framebuffer that lives
in the active pane or to the file specified
in the filename entry." } }
    button $top.clearB -relief raised -text "fbclear" \
	    -command "do_fbclear $id"
    hoc_register_data $top.clearB "Clear the Framebuffer"\
	    { { summary "If the framebuffer of the active pane
is enabled, it will be cleared." } }
    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "rt_dismiss $id"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the raytrace control panel." } }

    grid $top.srcE $top.srcMB -sticky nsew -in $top.srcF
    grid columnconfigure $top.srcF 0 -weight 1
    grid rowconfigure $top.srcF 0 -weight 1

    grid $top.destE $top.destMB -sticky nsew -in $top.destF
    grid columnconfigure $top.destF 0 -weight 1
    grid rowconfigure $top.destF 0 -weight 1

    grid $top.sizeE $top.sizeMB -sticky nsew -in $top.sizeF
    grid columnconfigure $top.sizeF 0 -weight 1
    grid rowconfigure $top.sizeF 0 -weight 1

    grid $top.srcL $top.srcF -pady 1 -sticky nsew -in $top.gridF1
    grid $top.destL $top.destF -pady 1 -sticky nsew -in $top.gridF1
    grid $top.sizeL $top.sizeF -pady 1 -sticky nsew -in $top.gridF1
    grid $top.colorL $top.colorF -pady 1 -sticky nsew -in $top.gridF1
    grid $top.advancedB - -pady 1 -sticky ns -in $top.gridF1
    grid columnconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 2 -weight 1
    grid rowconfigure $top.gridF1 3 -weight 1

    grid $top.gridF1 -padx 4 -pady 4 -sticky nsew -in $top.gridF2
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    grid $top.okB $top.raytraceB x $top.clearB x $top.dismissB -sticky "nsew" -in $top.gridF3
    grid columnconfigure $top.gridF3 2 -weight 1
    grid columnconfigure $top.gridF3 4 -weight 1

    grid $top.gridF2 -padx 4 -pady 4 -sticky nsew
    grid $top.gridF3 -padx 4 -pady 4 -sticky nsew
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    color_entry_update $top color $rt_control($id,color)

    place_near_mouse $top
    wm title $top "Raytrace Control Panel ($id)"
}

proc rt_ok { id top } {
    do_Raytrace $id
    rt_dismiss $id
}

proc do_Raytrace { id } {
    global mged_gui
    global port
    global fb_all
    global rt_control

    if {$rt_control($id,cooked_src) == ""} {
	return
    }

    if ![winfo exists $rt_control($id,cooked_src)] {
	return
    }

    winset $rt_control($id,cooked_src)
    set rt_cmd "_mged_rt" 

if { 1 } {
    if {$rt_control($id,cooked_dest) != ""} {
	append rt_cmd " -F$rt_control($id,cooked_dest)"
    }
} else {
    if {$rt_control($id,fb_or_file) == "filename"} {
	if {$rt_control($id,file) != ""} {
	    if {[file exists $rt_control($id,file)]} {
		set result [cad_dialog .$id.rtDialog $mged_gui($id,screen)\
			"Overwrite $rt_control($id,file)?"\
			"Overwrite $rt_control($id,file)?"\
			"" 0 OK CANCEL]

		if {$result} {
		    return
		} else {
		    file rename -force $rt_control($id,file) $rt_control($id,file).bak
		}
	    }

	    append rt_cmd " -o $rt_control($id,file)"
	} else {
	    cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		    "No file name specified!"\
		    "No file name specified!"\
		    "" 0 OK
	    return
	}
    } else {
	append rt_cmd " -F $port"
    }
}

    if {$rt_control($id,size) != ""} {
	set result [regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$"\
		$rt_control($id,size) smatch width junkA junkB junkC height]
	if {$result} {
	    if {$height != ""} {
		append rt_cmd " -w $width -n $height"
		set width "$width.0"
		set height "$height.0"
		set aspect [expr $width / $height]
		append rt_cmd " -V $aspect"
	    } else {
		set aspect 1
		append rt_cmd " -s $width"
	    }
	} else {
	    cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		    "Improper size specification!"\
		    "Improper size specification: $rt_control($id,size)"\
		    "" 0 OK
	    return
	}
    } else {
	set aspect 1
    }

    if {$rt_control($id,color) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_control($id,color) cmatch red green blue]
	if {$result} {
	    append rt_cmd " -C$red/$green/$blue"
	} else {
	    cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		    "Improper color specification!"\
		    "Improper color specification: $rt_control($id,color)"\
		    "" 0 OK
	    return
	}
    }

    if {$rt_control($id,nproc) != ""} {
	append rt_cmd " -P$rt_control($id,nproc)"
    }

    if {$rt_control($id,hsample) != ""} {
	append rt_cmd " -H$rt_control($id,hsample)"
    }

    if {$rt_control($id,jitter) != ""} {
	append rt_cmd " -J$rt_control($id,jitter)"
    }

    if {$rt_control($id,lmodel) != ""} {
	append rt_cmd " -l$rt_control($id,lmodel)"
    }

    if {!$fb_all} {
	set pos [rset rb pos]
	set xmin [lindex $pos 0]
	set ymin [lindex $pos 1]
	set dim [rset rb dim]
	set width [lindex $dim 0]
	set height [lindex $dim 1]
	regexp "^\[-\]?\[0-9\]+" $height height

	if {$width != 0 && $height != 0} {
	    if {$width > 0} {
		set xmax [expr $xmin + $width]
	    } else {
		set xmax $xmin
		set xmin [expr $xmax + $width]
	    }

	    if {$height > 0} {
		set ymax [expr $ymin + $height]
	    } else {
		set ymax $ymin
		set ymin [expr $ymax + $height]
	    }

	    append rt_cmd " -j $xmin,$ymin,$xmax,$ymax"
	}
    }

    switch $rt_control($id,omode) {
	one 
	    -
	several {
	    catch {eval $rt_cmd -- $rt_control($id,olist)}
	}
	all {
	    catch {eval $rt_cmd}
	}
    }
}

proc do_fbclear { id } {
    global mged_gui
    global rt_control

    if {$rt_control($id,cooked_dest) == ""} {
	return
    }

    if {$rt_control($id,color) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_control($id,color) cmatch red green blue]
	if {!$result} {
	    cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		    "Improper color specification!"\
		    "Improper color specification: $rt_control($id,color)"\
		    "" 0 OK
	    return
	}
    } else {
	set red 0
	set green 0
	set blue 0
    }

    set result [catch { exec fbclear -F $rt_control($id,cooked_dest)\
	    $red $green $blue & } rt_error]

    if {$result != 0} {
	cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		"RT Error!" "Rt Error: $rt_error" "" 0 OK
    }
}

proc rt_set_fb_size { id } {
    global mged_gui
    global rt_control

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"
}

proc rt_dismiss { id } {
    global rt_control

    set top $rt_control($id,topAS)
    if [winfo exists $top] {
	catch { destroy $top }
    }

    set top $rt_control($id,top)
    if [winfo exists $top] {
	catch { destroy $top }
    }
}

proc do_Advanced_Settings { id } {
    global mged_gui
    global rt_control

    set top $rt_control($id,topAS)
    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1
    frame $top.gridF2 -relief groove -bd 2

    set hoc_data { { summary "Indicates the number of processors
to use for raytracing." } }
    label $top.nprocL -text "# of Processors" -anchor e
    hoc_register_data $top.nprocL "# of Processors" $hoc_data
    entry $top.nprocE -relief sunken -bd 2 -width 2 -textvar rt_control($id,nproc)
    hoc_register_data $top.nprocE "# of Processors" $hoc_data

    set hoc_data { { summary "Hypersampling is the number of extra rays
to fire for each pixel. The results are then
averaged to determine the pixel value." } }
    label $top.hsampleL -text "Hypersample" -anchor e
    hoc_register_data $top.hsampleL "Hypersample" $hoc_data
    entry $top.hsampleE -relief sunken -bd 2 -width 2 -textvar rt_control($id,hsample)
    hoc_register_data $top.hsampleE "Hypersample" $hoc_data

    label $top.jitterL -text "Jitter" -anchor e
    hoc_register_data $top.jitterL "Jitter"\
	    { { summary "Jitter is used to randomly vary the point
from which a ray is fired." } }
    menubutton $top.jitterMB -relief sunken -bd 2 -textvar rt_control($id,jitterTitle)\
	    -menu $top.jitterMB.jitterM -indicatoron 1
    hoc_register_data $top.jitterMB "Jitter"\
	    { { summary "Pops up a menu of jitter values." } }
    menu $top.jitterMB.jitterM -title "Jitter" -tearoff 0
    $top.jitterMB.jitterM add command -label "None"\
	 -command "set rt_control($id,jitter) 0; set rt_control($id,jitterTitle) None"
    hoc_register_menu_data "Jitter" "None" "Jitter - None"\
	    { { summary "Turns off jittering. The rays will be
fired from the center of each cell." } }
    $top.jitterMB.jitterM add command -label "Cell"\
	 -command "set rt_control($id,jitter) 1; set rt_control($id,jitterTitle) Cell"
    hoc_register_menu_data "Jitter" "Cell" "Jitter - Cell"\
	    { { summary "Randomly jitter each cell by +/- one
half of the pixel size." } }
    $top.jitterMB.jitterM add command -label "Frame"\
	 -command "set rt_control($id,jitter) 2; set rt_control($id,jitterTitle) Frame"
    hoc_register_menu_data "Jitter" "Frame" "Jitter - Frame"\
	    { { summary "Randomly jitter the frame by +/- one
half of the pixel size. This variance will
be applied uniformly to each cell." } }
    $top.jitterMB.jitterM add command -label "Both"\
	 -command "set rt_control($id,jitter) 3; set rt_control($id,jitterTitle) Both"
    hoc_register_menu_data "Jitter" "Both" "Jitter - Both"\
	    { { summary "Randomly jitter the frame as well
as each cell." } }
    
    label $top.lmodelL -text "Light Model" -anchor e
    hoc_register_data $top.lmodelL "Light Model"\
	    { { summary "The light model determines how the
ray tracer will handle light." } }
    menubutton $top.lmodelMB -relief sunken -bd 2\
	    -width 24 -textvar rt_control($id,lmodelTitle)\
	    -menu $top.lmodelMB.lmodelM -indicatoron 1
    hoc_register_data $top.lmodelMB "Light Model"\
	    { { summary "Pops up a menu of light models." } }
    menu $top.lmodelMB.lmodelM -title "Light Model" -tearoff 0
    $top.lmodelMB.lmodelM add command -label "Full"\
	    -command "set rt_control($id,lmodel) 0;\
	    set rt_control($id,lmodelTitle) Full"
    hoc_register_menu_data "Light Model" "Full"\
	    "Lighting Model - Full"\
	    { { summary "This is the default. The full lighting model has the
ability to implement Phong shading, transparant and
reflective objects, shadow penumbras, texture
maps, etc.  In addition to ambient light, a
small amount of light is supplied from the eye
position. All objects in the active model space
with a material property string of ``light''
represent additional light sources (up to 16
are presently permitted), and shadow computations
will be initiated automatically." } }
    $top.lmodelMB.lmodelM add command -label "Diffuse"\
	    -command "set rt_control($id,lmodel) 1;\
	    set rt_control($id,lmodelTitle) Diffuse"
    hoc_register_menu_data "Light Model" "Diffuse"\
	    "Lighting Model - Diffuse"\
	    { { summary "This is a diffuse lighting model only and\
is intended for debugging." } }
    $top.lmodelMB.lmodelM add command -label "Surface Normals"\
	    -command "set rt_control($id,lmodel) 2;\
	    set rt_control($id,lmodelTitle) \"Surface Normals\""
    hoc_register_menu_data "Light Model" "Surface Normals"\
	    "Lighting Model - Surface Normals"\
	    { { summary "This lighting model displays the surface normals
as colors which makes it useful for examining
curvature and surface orientation." } }
    $top.lmodelMB.lmodelM add command -label "Diffuse - 3 light"\
	    -command "set rt_control($id,lmodel) 3;\
	    set rt_control($id,lmodelTitle) \"Diffuse - 3 light\""
    hoc_register_menu_data "Light Model" "Diffuse - 3 light"\
	    "Lighting Model - Diffuse 3 Light"\
	    { { summary "This is a three-light diffuse-lighting model\
and is intended for debugging." } }
    $top.lmodelMB.lmodelM add command -label "Curvature - inverse radius"\
	    -command "set rt_control($id,lmodel) 4;\
	    set rt_control($id,lmodelTitle) \"Curvature - inverse radius\""
    hoc_register_menu_data "Light Model" "Curvature - inverse radius"\
	    "Lighting Model - Curvature, Inverse Radius"\
	    { { summary "This is a curvature debugging display,
showing the inverse radius of curvature." } }
    $top.lmodelMB.lmodelM add command -label "Curvature - direction vector"\
	    -command "set rt_control($id,lmodel) 5;\
	    set rt_control($id,lmodelTitle) \"Curvature - direction vector\""
    hoc_register_menu_data "Light Model" "Curvature - direction vector"\
	    "Lighting Model - Curvature, Direction Vector"\
	    { { summary "This is a curvature debugging display,
showing the principal direction vector." } }

    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the advanced raytrace panel." } }

    grid $top.nprocL $top.nprocE -sticky nsew -pady 1 -in $top.gridF1
    grid $top.hsampleL $top.hsampleE -sticky nsew -pady 1 -in $top.gridF1
    grid $top.jitterL $top.jitterMB -sticky nsew -pady 1 -in $top.gridF1
    grid $top.lmodelL $top.lmodelMB -sticky nsew -pady 1 -in $top.gridF1
    grid columnconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 2 -weight 1
    grid rowconfigure $top.gridF1 3 -weight 1

    grid $top.gridF1 -sticky nsew -padx 8 -pady 8 -in $top.gridF2
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    grid $top.gridF2 -sticky nsew -padx 2 -pady 2
    grid $top.dismissB -sticky s -padx 2 -pady 2
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    place_near_mouse $top
    wm title $top "Advanced Settings ($id)"
}

## - update_Raytrace
#
# Called by main GUI to update the Raytrace Control Panel
#
proc update_Raytrace { id } {
    global mged_gui
    global rt_control
    global listen
    global port
    global fb
    global fb_all
    global fb_overlay

    set top $rt_control($id,top)
    if ![winfo exists $top] {
	return
    }

    if {$rt_control($id,half_baked_dest) != $mged_gui($id,active_dm)} {
	return
    }

    set rt_control($id,cooked_dest) $port
    set rt_control($id,fb) $fb
    set rt_control($id,fb_all) $fb_all
    set rt_control($id,fb_overlay) $fb_overlay
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"

    set tmplist [list summary "The active pane is $rt_control($id,cooked_src)."]
    hoc_register_data $top.framebufferL "Active Pane"\
	    [list $tmplist]
}

proc rt_edit_olist { id } {
    global mged_gui
    global rt_control

    return
}

proc rt_set_mouse_behavior { id } {
    global mged_gui
    global rt_control
    global mouse_behavior

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    switch $rt_control($id,omode) {
	one
	    -
	several {
	    set mouse_behavior o

	    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
		set mged_gui($id,mouse_behavior) o
	    }
	}
	all {
	    set mouse_behavior d

	    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
		set mged_gui($id,mouse_behavior) d
	    }
	}
    }
}

#
# raw	 ->	pathname, filename,
#		ul, upper left, ur, upper right,
#		ll, lower left, lr, lower right
# cooked ->	port spec
proc rt_cook_dest { id raw_dest } {
    global mged_gui
    global rt_control
    global listen
    global port
    global fb
    global fb_all
    global fb_overlay
    global mouse_behavior

    if {$raw_dest == ""} {
	return
    }

    set rt_control($id,raw_dest) $raw_dest
    set rt_control($id,half_baked_dest) [rt_half_bake $id $raw_dest]

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	set rt_control($id,cooked_dest) $raw_dest


	# disable framebuffer menu
	if [winfo exists $rt_control($id,top)] {
	    $rt_control($id,top).menubar entryconfigure 0 -state disabled
	}

	return
    }

    # re-enable framebuffer menu
    if [winfo exists $rt_control($id,top)] {
	$rt_control($id,top).menubar entryconfigure 0 -state normal
    }

    winset $rt_control($id,half_baked_dest)
    set fb 1
    set fb_all 1
    set listen 1
    set rt_control($id,cooked_dest) $port
    set rt_control($id,fb) 1
    set rt_control($id,fb_all) $fb_all
    set rt_control($id,fb_overlay) $fb_overlay
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"

    if {$mouse_behavior == "o"} {
	set rt_control($id,omode) one
    } else {
	set rt_control($id,omode) all
    }

    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
	set mged_gui($id,fb) $fb
	set mged_gui($id,listen) $listen
	.$id.menubar.settings.fb entryconfigure 7 -state normal
	.$id.menubar.modes entryconfigure 4 -state normal
    }
}

#
# raw	 ->	pathname,
#		ul, upper left, ur, upper right,
#		ll, lower left, lr, lower right
# cooked ->	pathname
#
proc rt_cook_src { id raw_src } {
    global rt_control

    if {$raw_src == ""} {
	return
    }

    set rt_control($id,raw_src) $raw_src
    set rt_control($id,cooked_src) [rt_half_bake $id $raw_src]
}

proc rt_set_fb { id } {
    global mged_gui
    global rt_control
    global fb

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    set fb $rt_control($id,fb)

    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
	set mged_gui($id,fb) $rt_control($id,fb)

	if {$mged_gui($id,fb)} {
	    set mged_gui($id,listen) 1
	    .$id.menubar.settings.fb entryconfigure 7 -state normal
	    .$id.menubar.modes entryconfigure 4 -state normal
	} else {
	    .$id.menubar.settings.fb entryconfigure 7 -state disabled
	    .$id.menubar.modes entryconfigure 4 -state disabled
	    set mged_gui($id,listen) 0
	}
    }
}

proc rt_set_fb_all { id } {
    global rt_control
    global mged_gui
    global fb_all

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    set fb_all $rt_control($id,fb_all)

    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
	set mged_gui($id,fb_all) $rt_control($id,fb_all)
    }

}

proc rt_set_fb_overlay { id } {
    global mged_gui
    global rt_control
    global fb_overlay

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    set fb_overlay $rt_control($id,fb_overlay)

    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
	set mged_gui($id,fb_overlay) $rt_control($id,fb_overlay)
    }
}

## - rt_half_bake
#
# If possible, returns a valid display manager window.
# Otherwise, returns $raw.
#
proc rt_half_bake { id raw } {
    global mged_gui

    switch $raw {
	"active" 
	    -
	"Active" {
	    return $mged_gui($id,active_dm)
	}
	"ul" 
	    -
	"upper left" {
	    return $mged_gui($id,top).ul
	}
	"ur" 
	    -
	"upper right" {
	    return $mged_gui($id,top).ur
	}
	"ll" 
	    -
	"lower left" {
	    return $mged_gui($id,top).ll
	}
	"lr" 
	    -
	"lower right" {
	    return $mged_gui($id,top).lr
	}
	default {
	    if [winfo exists .$raw] {
		return .$raw
	    }

	    return $raw
	}
    }
}

proc rt_db_to_pix {} {
    global rt_control

    regsub \.g$ [_mged_opendb] .pix default_file
    return $default_file
}

## - rt_init_vars
#
# Called by init_Raytrace to initialize rt_control
#
proc rt_init_vars { id top topAS } {
    global mged_gui
    global rt_control
    
    # initialize once
    if ![info exists rt_control($id,top)] {
	set rt_control($id,top) $top
	set rt_control($id,topAS) $topAS
	set rt_control($id,nproc) 1
	set rt_control($id,hsample) 0
	set rt_control($id,jitter) 0
	set rt_control($id,jitterTitle) "None"
	set rt_control($id,lmodel) 0
	set rt_control($id,lmodelTitle) "Full"

	# set widget padding
	set rt_control($id,padx) 4
	set rt_control($id,pady) 2
    }

    # initialize everytime
    set rt_control($id,olist) {}
    set rt_control($id,color) [rset cs bg]
    rt_cook_src $id $mged_gui($id,active_dm)
    rt_cook_dest $id $mged_gui($id,active_dm)
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"
}
