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

    set top .$id.do_rt

    if [winfo exists $top] {
	raise $top
	return
    }

    winset $mged_gui($id,active_dm)

    if ![info exists rt_control($id,padx)] {

	# set widget padding
	set rt_control($id,padx) 4
	set rt_control($id,pady) 2

	set rt_control($id,fb_or_file) framebuffer

	regsub \.g$ [_mged_opendb] .pix default_file
	set rt_control($id,file) $default_file

	set rt_control($id,size) 512
	set rt_control($id,color) [rset cs bg]
	set rt_control($id,nproc) 1
	set rt_control($id,hsample) 0
	set rt_control($id,jitter) 0
	set rt_control($id,jitterTitle) "None"
	set rt_control($id,lmodel) 0
	set rt_control($id,lmodelTitle) "Full"
    }

    if {$fb} {
	set rt_control($id,fb_or_file) "framebuffer"
	set fb_state normal
	set file_state disabled
    } else {
	set rt_control($id,fb_or_file) "filename"
	set fb_state disabled
	set file_state normal
    }

    toplevel $top -screen $mged_gui($id,screen) -menu $top.menubar

    frame $top.gridF2 -relief groove -bd 2
    frame $top.gridF3 -relief groove -bd 2
    frame $top.gridF4
    frame $top.filenameF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2

    menu $top.menubar -tearoff 0
    $top.menubar add cascade -label "Active Pane" -underline 0 -menu $top.menubar.active
    $top.menubar add cascade -label "Framebuffer" -underline 0 -menu $top.menubar.fb

    menu $top.menubar.active -title "Active Pane" -tearoff 0
    $top.menubar.active add radiobutton -value ul -variable mged_gui($id,dm_loc)\
	    -label "Upper Left" -underline 6\
	    -command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Upper Left" "Active Pane - Upper Left"\
	    { { summary "Set the active pane to \"Upper Left\"." } }
    $top.menubar.active add radiobutton -value ur -variable mged_gui($id,dm_loc)\
	    -label "Upper Right" -underline 6\
	    -command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Upper Right" "Active Pane - Upper Right"\
	    { { summary "Set the active pane to \"Upper Right\"." } }
    $top.menubar.active add radiobutton -value ll -variable mged_gui($id,dm_loc)\
	    -label "Lower Left" -underline 2\
	    -command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Lower Left" "Active Pane - Lower Left"\
	    { { summary "Set the active pane to \"Lower Left\"." } }
    $top.menubar.active add radiobutton -value lr -variable mged_gui($id,dm_loc)\
	    -label "Lower Right" -underline 3\
	    -command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Lower Right" "Active Pane - Lower Right"\
	    { { summary "Set the active pane to \"Lower Right\"." } }

    menu $top.menubar.fb -title "Framebuffer" -tearoff 0
    $top.menubar.fb add radiobutton -value 1 -variable mged_gui($id,fb_all)\
	    -label "All" -underline 0\
	    -command "mged_apply $id \"set fb_all \$mged_gui($id,fb_all)\""
    hoc_register_menu_data "Framebuffer" "All" "Framebuffer - All"\
	    { { summary "Use the entire pane as a framebuffer." } }
    $top.menubar.fb add radiobutton -value 0 -variable mged_gui($id,fb_all)\
	    -label "Rectangle Area" -underline 0\
	    -command "mged_apply $id \"set fb_all \$mged_gui($id,fb_all)\""
    hoc_register_menu_data "Framebuffer" "Rectangle Area" "Framebuffer - Rectangle Area"\
	    { { summary "Use only the rectangular area for the framebuffer." } }
    $top.menubar.fb add separator
    $top.menubar.fb add radiobutton -value 1 -variable mged_gui($id,fb_overlay)\
	    -label "Overlay" -underline 0\
	    -command "mged_apply $id \"set fb_overlay \$mged_gui($id,fb_overlay)\""
    hoc_register_menu_data "Framebuffer" "Overlay" "Framebuffer - Overlay"\
	    { { summary "Draw the framebuffer on top of the geometry." } }
    $top.menubar.fb add radiobutton -value 0 -variable mged_gui($id,fb_overlay)\
	    -label "Underlay" -underline 0\
	    -command "mged_apply $id \"set fb_overlay \$mged_gui($id,fb_overlay)\""
    hoc_register_menu_data "Framebuffer" "Underlay" "Framebuffer - Underlay"\
	    { { summary "Draw the framebuffer under the geometry." } }

    radiobutton $top.framebufferRB -text "Frame Buffer" -anchor w\
	    -value framebuffer -variable rt_control($id,fb_or_file)\
	    -command "rt_set_fb_state $id"
    hoc_register_data $top.framebufferRB "Frame Buffer"\
	    { { summary "This activates the framebuffer and deactivates
the file (i.e. the results of raytracing
will end up in the framebuffer). Note - one
can use fb-pix to capture the contents of the
framebuffer to a file." } }
    label $top.framebufferL -textvariable rt_control($id,fb)

    radiobutton $top.filenameRB -text "File Name" -anchor w\
	    -value filename -variable rt_control($id,fb_or_file)\
	    -command "rt_set_file_state $id"
    hoc_register_data $top.filenameRB "File Name"\
	    { { summary "This activates the file and deactivates
the framebuffer (i.e. the results of raytracing
will end up in the file)." } }
    entry $top.filenameE -relief flat -width 12 -textvar rt_control($id,file)\
	    -state $file_state
    hoc_register_data $top.filenameE "File Name"\
	    { { summary "Enter a file name. If the \"File Name\"
radiobutton is selected, the results of
raytracing will go to the specified file." } }

    label $top.sizeL -text "Size" -anchor w
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

    label $top.colorL -text "Background Color" -anchor w
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
	    -command "do_fbclear $id" -state $fb_state
    hoc_register_data $top.clearB "Clear the Framebuffer"\
	    { { summary "If the framebuffer of the active pane
is enabled, it will be cleared." } }
    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "rt_dismiss $id"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the raytrace control panel." } }

    grid $top.framebufferL $top.framebufferRB -sticky "ew"\
	    -in $top.gridF2 -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.filenameE -sticky "nsew" -in $top.filenameF
    grid columnconfigure $top.filenameF 0 -weight 1
    grid rowconfigure $top.filenameF 0 -weight 1
    grid $top.filenameF $top.filenameRB -sticky "nsew"\
	    -in $top.gridF2 -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 1 -weight 1

    grid $top.sizeE $top.sizeMB -sticky "nsew" -in $top.sizeF
    grid columnconfigure $top.sizeF 0 -weight 1
    grid rowconfigure $top.sizeF 0 -weight 1
    grid $top.sizeF $top.sizeL -sticky "nsew" -in $top.gridF3 \
	    -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.colorF $top.colorL -sticky "nsew" -in $top.gridF3 \
	    -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.advancedB - -in $top.gridF3 -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid columnconfigure $top.gridF3 0 -weight 1
    grid rowconfigure $top.gridF3 0 -weight 1
    grid rowconfigure $top.gridF3 1 -weight 1

    grid $top.okB $top.raytraceB x $top.clearB x $top.dismissB -sticky "nsew" -in $top.gridF4
    grid columnconfigure $top.gridF4 2 -weight 1 -minsize 8
    grid columnconfigure $top.gridF4 4 -weight 1 -minsize 8

    grid $top.gridF2 -sticky "nsew" \
	    -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.gridF3 -sticky "nsew" \
	    -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.gridF4 -sticky "nsew" \
	    -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1
    grid rowconfigure $top 1 -weight 2

    color_entry_update $top color $rt_control($id,color)
    update_Raytrace $id

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

    winset $mged_gui($id,active_dm)
    set rt_cmd "_mged_rt"

    if {$rt_control($id,fb_or_file) == "filename"} {
	if {$rt_control($id,file) != ""} {
	    if {[file exists $rt_control($id,file)]} {
		set result [cad_dialog .$id.rtDialog $mged_gui($id,screen)\
			"Overwrite $rt_control($id,file)?"\
			"Overwrite $rt_control($id,file)?"\
			"" 0 OK CANCEL]

		if {$result} {
		    return
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
#	if {$rt_control($id,fb) != ""} {
#	    append rt_cmd " -F $rt_control($id,fb)"
#	}
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

    catch {eval $rt_cmd}
}

proc do_fbclear { id } {
    global mged_gui
    global port
    global rt_control

    winset $mged_gui($id,active_dm)

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

    if {$rt_control($id,fb) != ""} {
	set result [catch { exec fbclear -F $port $red $green $blue & } rt_error]
    } else {
	set result [catch { exec fbclear $red $green $blue & } rt_error]
    }

    if {$result != 0} {
	cad_dialog .$id.rtDialog $mged_gui($id,screen)\
		"RT Error!" "Rt Error: $rt_error" "" 0 OK
    }
}

proc rt_set_fb_state { id } {
    global mged_gui

    set mged_gui($id,fb) 1
    set_fb $id

    set top .$id.do_rt
    $top.clearB configure -state normal
    $top.menubar entryconfigure 1 -state normal
    $top.filenameE configure -state disabled

    rt_set_fb_size $id
}

proc rt_set_file_state { id } {
    global mged_gui

    set mged_gui($id,fb) 0
    set_fb $id

    set top .$id.do_rt
    $top.clearB configure -state disabled
    $top.menubar entryconfigure 1 -state disabled
    $top.filenameE configure -state normal
    focus $top.filenameE
}

proc rt_set_fb_size { id } {
    global mged_gui
    global rt_control

    winset $mged_gui($id,top).$mged_gui($id,dm_loc)
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"
}

proc rt_dismiss { id } {
    set top .$id.do_rtAS
    if [winfo exists $top] {
	catch { destroy $top }
    }

    set top .$id.do_rt
    if [winfo exists $top] {
	catch { destroy $top }
    }
}

proc do_Advanced_Settings { id } {
    global mged_gui
    global rt_control

    set top .$id.do_rtAS
    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    frame $top.nprocF
    frame $top.nprocFF -relief sunken -bd 2
    frame $top.hsampleF
    frame $top.hsampleFF -relief sunken -bd 2
    frame $top.jitterF
    frame $top.jitterFF -relief sunken -bd 2
    frame $top.lmodelF
    frame $top.lmodelFF -relief sunken -bd 2

    set hoc_data { { summary "Indicates the number of processors
to use for raytracing." } }
    label $top.nprocL -text "# of Processors" -anchor w
    hoc_register_data $top.nprocL "# of Processors" $hoc_data
    entry $top.nprocE -relief flat -width 4 -textvar rt_control($id,nproc)
    hoc_register_data $top.nprocE "# of Processors" $hoc_data

    set hoc_data { { summary "Hypersampling is the number of extra rays
to fire for each pixel. The results are then
averaged to determine the pixel value." } }
    label $top.hsampleL -text "Hypersample" -anchor w
    hoc_register_data $top.hsampleL "Hypersample" $hoc_data
    entry $top.hsampleE -relief flat -width 4 -textvar rt_control($id,hsample)
    hoc_register_data $top.hsampleE "Hypersample" $hoc_data

    label $top.jitterL -text "Jitter" -anchor w
    hoc_register_data $top.jitterL "Jitter"\
	    { { summary "Jitter is used to randomly vary the point
from which a ray is fired." } }
    menubutton $top.jitterMB -relief raised -bd 2 -textvar rt_control($id,jitterTitle)\
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
    
    label $top.lmodelL -text "Light Model" -anchor w
    hoc_register_data $top.lmodelL "Light Model"\
	    { { summary "The light model determines how the
ray tracer will handle light." } }
    menubutton $top.lmodelMB -relief raised -bd 2\
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

    grid $top.nprocL -sticky "ew" -in $top.nprocF
    grid $top.nprocE -sticky "ew" -in $top.nprocFF
    grid $top.nprocFF -sticky "ew" -in $top.nprocF

    grid $top.hsampleL -sticky "ew" -in $top.hsampleF
    grid $top.hsampleE -sticky "ew" -in $top.hsampleFF
    grid $top.hsampleFF -sticky "ew" -in $top.hsampleF

    grid $top.jitterL -sticky "ew" -in $top.jitterF
    grid $top.jitterMB -sticky "ew" -in $top.jitterFF
    grid $top.jitterFF -sticky "ew" -in $top.jitterF

    grid $top.lmodelL -sticky "ew" -in $top.lmodelF
    grid $top.lmodelMB -sticky "ew" -in $top.lmodelFF
    grid $top.lmodelFF -sticky "ew" -in $top.lmodelF

    grid $top.nprocF x $top.hsampleF -sticky "ew" -in $top.gridF1 -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.jitterF x $top.lmodelF -sticky "ew" -in $top.gridF1 -padx $rt_control($id,padx) -pady $rt_control($id,pady)

    grid $top.dismissB -in $top.gridF2

    grid $top.gridF1 -sticky "ew" -padx $rt_control($id,padx) -pady $rt_control($id,pady)
    grid $top.gridF2 -sticky "ew" -padx $rt_control($id,padx) -pady $rt_control($id,pady)

    grid columnconfigure $top.nprocF 0 -weight 1
    grid columnconfigure $top.nprocFF 0 -weight 1
    grid columnconfigure $top.hsampleF 0 -weight 1
    grid columnconfigure $top.hsampleFF 0 -weight 1
    grid columnconfigure $top.jitterF 0 -weight 1
    grid columnconfigure $top.jitterFF 0 -weight 1
    grid columnconfigure $top.lmodelF 0 -weight 1
    grid columnconfigure $top.lmodelFF 0 -weight 1
    grid columnconfigure $top.gridF1 0 -weight 1
    grid columnconfigure $top.gridF1 2 -weight 1
    grid columnconfigure $top.gridF2 0 -weight 1
    grid columnconfigure $top 0 -weight 1

    place_near_mouse $top
    wm title $top "Advanced Settings ($id)"
}

proc update_Raytrace { id } {
    global mged_gui
    global fb
    global listen
    global port
    global rt_control

    set top .$id.do_rt
    if ![winfo exists $top] {
	return
    }

    winset $mged_gui($id,active_dm)
    switch $mged_gui($id,dm_loc) {
	ul {
	    set rt_control($id,fb) "Upper Left"
	}
	ur {
	    set rt_control($id,fb) "Upper Right"
	}
	ll {
	    set rt_control($id,fb) "Lower Left"
	}
	lr {
	    set rt_control($id,fb) "Upper Right"
	}
    }

    if {$mged_gui($id,fb)} {
	set rt_control($id,fb_or_file) "framebuffer"
	set fb_state normal
	set file_state disabled
	rt_set_fb_state $id
    } else {
	set rt_control($id,fb_or_file) "filename"
	set fb_state disabled
	set file_state normal
	rt_set_file_state $id
    }

    set tmplist [list summary "The active pane is $rt_control($id,fb)."]
    hoc_register_data $top.framebufferL "Active Pane"\
	    [list $tmplist]
}
