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

    if {[opendb] == ""} {
	cad_dialog .$id.uncool $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.rt

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists mged_gui($id,active_dm)] {
	return
    }

    if ![winfo exists $mged_gui($id,active_dm)] {
	return
    }

    winset $mged_gui($id,active_dm)
    rt_init_vars $id $mged_gui($id,active_dm)

    toplevel $top -screen $mged_gui($id,screen) -menu $top.menubar

    frame $top.gridF1
    frame $top.gridF2 -relief groove -bd 2
    frame $top.gridF3
    frame $top.srcF -relief sunken -bd 2
    frame $top.destF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2

    menu $top.menubar -tearoff 0
    $top.menubar add cascade -label "Framebuffer" -underline 0 -menu $top.menubar.fb
    $top.menubar add cascade -label "Objects" -underline 0 -menu $top.menubar.obj

    menu $top.menubar.fb -title "Framebuffer" -tearoff 0
    $top.menubar.fb add checkbutton -offvalue 0 -onvalue 1\
	    -variable rt_control($id,fb)\
	    -label "Active" -underline 0 \
	    -command "rt_set_fb $id"
    hoc_register_menu_data "Framebuffer" "Active" "Destination Framebuffer Active"\
	    { { summary "This activates/deactivates the destination framebuffer.
Note - this pertains only to MGED's framebuffers." } }
    $top.menubar.fb add separator
    $top.menubar.fb add radiobutton -value 1 -variable rt_control($id,fb_all)\
	    -label "All" -underline 0\
	    -command "rt_set_fb_all $id"
    hoc_register_menu_data "Framebuffer" "All" "Destination Framebuffer - All"\
	    { { summary "Use the entire pane as a framebuffer.
Note - this pertains only to MGED's framebuffers." } }
    $top.menubar.fb add radiobutton -value 0 -variable rt_control($id,fb_all)\
	    -label "Rectangle Area" -underline 0\
	    -command "rt_set_fb_all $id"
    hoc_register_menu_data "Framebuffer" "Rectangle Area" "Destination Framebuffer - Rectangle Area"\
	    { { summary "Use only the rectangular area, as defined by the
sweep rectangle, for the framebuffer. Note - this
pertains only to MGED's framebuffers." } }
    $top.menubar.fb add separator
    $top.menubar.fb add radiobutton -value 2 -variable rt_control($id,fb_overlay)\
	    -label "Overlay" -underline 0\
	    -command "rt_set_fb_overlay $id"
    hoc_register_menu_data "Framebuffer" "Overlay" "Destination Framebuffer - Overlay"\
	    { { summary "Draw the framebuffer above everything (i.e. above the
geometry and faceplate). Note - this pertains only to
MGED's framebuffers." } }
    $top.menubar.fb add radiobutton -value 1 -variable rt_control($id,fb_overlay)\
	    -label "Interlay" -underline 0\
	    -command "rt_set_fb_overlay $id"
    hoc_register_menu_data "Framebuffer" "Interlay" "Destination Framebuffer - Interlay"\
	    { { summary "Draw the framebuffer above the geometry and below
the faceplate. Note - this pertains only to MGED's
framebuffers." } }
    $top.menubar.fb add radiobutton -value 0 -variable rt_control($id,fb_overlay)\
	    -label "Underlay" -underline 0\
	    -command "rt_set_fb_overlay $id"
    hoc_register_menu_data "Framebuffer" "Underlay" "Destination Framebuffer - Underlay"\
	    { { summary "Draw the framebuffer below everything (i.e. below the
geometry and faceplate). Note - this pertains only to
MGED's framebuffers." } }

    menu $top.menubar.obj -title "Objects" -tearoff 0
    $top.menubar.obj add radiobutton -value one -variable rt_control($id,omode)\
	    -label "one" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "one" "Objects - one"\
	    { { summary "Raytrace only the selected object. Note - this will
change the mouse behavior of the source window to
\"o\" (i.e. raytrace object)." } }
    $top.menubar.obj add radiobutton -value several -variable rt_control($id,omode)\
	    -label "several" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "several" "Objects - several"\
	    { { summary "Add the selected object to the list of objects to be
raytraced. Note - this will change the mouse behavior
of the source window to \"o\" (i.e. pick raytrace objects)." } }
    $top.menubar.obj add radiobutton -value all -variable rt_control($id,omode)\
	    -label "all" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "all" "Objects - all"\
	    { { summary "Raytrace all displayed objects. Note - this will
change the mouse behavior of the source window to
\"d\" (i.e. the default mouse behavior)." } }
    $top.menubar.obj add separator
    $top.menubar.obj add command -label "edit list"\
	    -command "rt_olist_edit $id"
    hoc_register_menu_data "Objects" "edit list" "Edit List"\
	    { { summary "Pop up a tool to edit the list
of objects to be raytraced." } }
    $top.menubar.obj add command -label "clear list"\
	    -command "rt_olist_clear $id"
    hoc_register_menu_data "Objects" "clear list" "Clear List"\
	    { { summary "Clear the object list and the contents
of the object list editor if it exists." } }

    set hoc_data { { summary "
Enter the desired source. The source is used to obtain
the view information (i.e. size, position and orientation)
that is passed to the raytracer and must be one of the internal
panes (display manager windows). The source can be specified
using the pathname of any pane. The panes associated with this
instance of the GUI may also be specified with keywords. For
example, ul, \"upper left\" and \"Upper Left\" all specify the
upper left pane." } }
    label $top.srcL -text "Source" -anchor e
    hoc_register_data $top.srcL "Source" $hoc_data
    entry $top.srcE -relief flat -width 12 -textvar rt_control($id,raw_src)
    hoc_register_data $top.srcE "Source" $hoc_data
    bind $top.srcE <KeyRelease> "rt_force_cook_src $id \$rt_control($id,raw_src)"
    menubutton $top.srcMB -relief raised -bd 2\
	    -menu $top.srcMB.menu -indicatoron 1
    hoc_register_data $top.srcMB "Source"\
	    { { summary "Pop up a menu of likely sources." } }
    menu $top.srcMB.menu -title "Source" -tearoff 0
    $top.srcMB.menu add command -label "Active Pane"\
	    -command "rt_force_cook_src $id \$mged_gui($id,active_dm)"
    hoc_register_menu_data "Source" "Active Pane" "Source - Active Pane"\
	    { { summary "Set the source to the active pane. The
active pane is the pane currently tied
to the GUI." } }
    $top.srcMB.menu add separator
    $top.srcMB.menu add command -label "Upper Left"\
	    -command "rt_force_cook_src $id $mged_gui($id,top).ul"
    hoc_register_menu_data "Source" "Upper Left" "Source - Upper Left"\
	    { { summary "Set the source to the \"Upper Left\" pane." } }
    $top.srcMB.menu add command -label "Upper Right"\
	    -command "rt_force_cook_src $id $mged_gui($id,top).ur"
    hoc_register_menu_data "Source" "Upper Right" "Source - Upper Right"\
	    { { summary "Set the source to the \"Upper Right\" pane." } }
    $top.srcMB.menu add command -label "Lower Left"\
	    -command "rt_force_cook_src $id $mged_gui($id,top).ll"
    hoc_register_menu_data "Source" "Lower Left" "Source - Lower Left"\
	    { { summary "Set the source to the \"Lower Left\" pane." } }
    $top.srcMB.menu add command -label "Lower Right"\
	    -command "rt_force_cook_src $id $mged_gui($id,top).lr"
    hoc_register_menu_data "Source" "Lower Right" "Source - Lower Right"\
	    { { summary "Set the source to the \"Lower Right\" pane." } }
    $top.srcMB.menu add separator
    $top.srcMB.menu add checkbutton -offvalue 0 -onvalue 1\
	    -variable rt_control($id,fixedSrc)\
	    -label "Fixed"
    hoc_register_menu_data "Source" "Fixed" "Source - Fixed"\
	    { { summary "By default, when the mouse_behavior is in the mode
to pick raytrace objects, the source will change
to the window wherein the object is selected. To
prevent the source from changing while selecting
objects, check the fixed button." } }

    set hoc_data { { summary "
Enter the desired destination. This is the place where
the pixels will be sent and can be the pathname of any internal
pane (display manager window). The panes associated with this
instance of the GUI can also be specified with keywords. For
example, ul, \"upper left\" and \"Upper Left\" all specify the
upper left pane. The destination can also be a file or an external
framebuffer. To specify an external framebuffer the user might
enter fbhost:0 to send the output to the framebuffer running on
the machine fbhost and listening on port 0." } }
    label $top.destL -text "Destination" -anchor e
    hoc_register_data $top.destL "Destination" $hoc_data
    entry $top.destE -relief flat -width 12 -textvar rt_control($id,raw_dest)
    hoc_register_data $top.destE "Destination" $hoc_data
    bind $top.destE <KeyRelease> "rt_force_cook_dest $id \$rt_control($id,raw_dest)"
    menubutton $top.destMB -relief raised -bd 2\
	    -menu $top.destMB.menu -indicatoron 1
    hoc_register_data $top.destMB "Destination"\
	    { { summary "Pop up a menu of likely destinations." } }
    menu $top.destMB.menu -title "Destination" -tearoff 0
    $top.destMB.menu add command -label "Active Pane"\
	    -command "rt_force_cook_dest $id \$mged_gui($id,active_dm)"
    hoc_register_menu_data "Destination" "Active Pane" "Destination - Active Pane"\
	    { { summary "Set the destination to the active pane.
The active pane is the pane currently
tied to the GUI." } }
    $top.destMB.menu add separator
    $top.destMB.menu add command -label "Upper Left"\
	    -command "rt_force_cook_dest $id $mged_gui($id,top).ul"
    hoc_register_menu_data "Destination" "Upper Left" "Destination - Upper Left"\
	    { { summary "Set the destination to \"Upper Left\" pane." } }
    $top.destMB.menu add command -label "Upper Right"\
	    -command "rt_force_cook_dest $id $mged_gui($id,top).ur"
    hoc_register_menu_data "Destination" "Upper Right" "Destination - Upper Right"\
	    { { summary "Set the destination to \"Upper Right\" pane." } }
    $top.destMB.menu add command -label "Lower Left"\
	    -command "rt_force_cook_dest $id $mged_gui($id,top).ll"
    hoc_register_menu_data "Destination" "Lower Left" "Destination - Lower Left"\
	    { { summary "Set the destination to \"Lower Left\" pane." } }
    $top.destMB.menu add command -label "Lower Right"\
	    -command "rt_force_cook_dest $id $mged_gui($id,top).lr"
    hoc_register_menu_data "Destination" "Lower Right" "Destination - Lower Right"\
	    { { summary "Set the destination to \"Lower Right\" pane." } }
    $top.destMB.menu add separator
    set dbname [rt_db_to_pix]
    if {$dbname == ""} {
	set dbname foo.pix
    }
    $top.destMB.menu add command -label $dbname\
	    -command "rt_force_cook_dest $id $dbname"
    hoc_register_menu_data "Destination" "$dbname" "Destination - $dbname"\
	    { { summary "Set the destination to the specified file." } }
    if {[info exists env(FB_FILE)] && $env(FB_FILE) != ""} {
	$top.destMB.menu add command -label "$env(FB_FILE)"\
		-command "rt_force_cook_dest $id $env(FB_FILE)"
	hoc_register_menu_data "Destination" "$env(FB_FILE)" "Destination - $env(FB_FILE)"\
		{ { summary "Set the destination to the specified framebuffer." } }
    }
    $top.destMB.menu add separator
    $top.destMB.menu add checkbutton -offvalue 0 -onvalue 1\
	    -variable rt_control($id,fixedDest)\
	    -label "Fixed"
    hoc_register_menu_data "Destination" "Fixed" "Destination - Fixed"\
	    { { summary "By default, when the mouse_behavior is in the mode
to pick raytrace objects, the destination will change
to the window wherein the object is selected. To
prevent the destination from changing while selecting
objects, check the fixed button." } }

    label $top.sizeL -text "Size" -anchor e
    hoc_register_data $top.sizeL "Size"\
	    { { summary "Indicates the size of the image.
This defaults to the size of the active pane." } }
    entry $top.sizeE -relief flat -width 12 -textvar rt_control($id,size)
    hoc_register_data $top.sizeE "Size"\
	    { { summary "Enter the desired image size." } }
    menubutton $top.sizeMB -relief raised -bd 2\
	    -menu $top.sizeMB.sizeM -indicatoron 1
    hoc_register_data $top.sizeMB "Size"\
	    { { summary "Pop up a menu of popular image sizes." } }
    menu $top.sizeMB.sizeM -title "Size" -tearoff 0
    $top.sizeMB.sizeM add command -label "Size of Pane"\
	    -command "rt_set_fb_size $id"
    hoc_register_menu_data "Size" "Size of Pane" "Size of Pane"\
	    { { summary "Set the image size to be the
same size as the active pane." } }
    $top.sizeMB.sizeM add command -label 128\
	    -command "set rt_control($id,size) 128"
    hoc_register_menu_data "Size" 128 "Size - 128x128"\
	    { { summary "Set the image size to 128x128." } }
    $top.sizeMB.sizeM add command -label 256\
	    -command "set rt_control($id,size) 256"
    hoc_register_menu_data "Size" 256 "Size - 256x256"\
	    { { summary "Set the image size to 256x256." } }
    $top.sizeMB.sizeM add command -label 512\
	    -command "set rt_control($id,size) 512"
    hoc_register_menu_data "Size" 512 "Size - 512x512"\
	    { { summary "Set the image size to 512x512." } }
    $top.sizeMB.sizeM add command -label 640x480\
	    -command "set rt_control($id,size) 640x480"
    hoc_register_menu_data "Size" 640x480 "Size - 640x480"\
	    { { summary "Set the image size to 640x480." } }
    $top.sizeMB.sizeM add command -label 720x486\
	    -command "set rt_control($id,size) 720x486"
    hoc_register_menu_data "Size" 720x486 "Size - 720x486"\
	    { { summary "Set the image size to 720x486." } }
    $top.sizeMB.sizeM add command -label 1024\
	    -command "set rt_control($id,size) 1024"
    hoc_register_menu_data "Size" 1024 "Size - 1024x1024"\
	    { { summary "Set the image size to 1024x1024." } }

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
	    { { summary "Pop up another GUI for advanced settings." } }
    button $top.okB -relief raised -text "Ok"\
	    -command "rt_ok $id $top"
    hoc_register_data $top.okB "Raytrace"\
	    { { summary "Begin raytracing the view of the source pane.
The results of the raytrace will go to the place
specified by the destination. Afterwards, dismiss
the raytrace control panel." } }
    button $top.raytraceB -relief raised -text "Raytrace" \
	    -command "do_Raytrace $id"
    hoc_register_data $top.raytraceB "Raytrace"\
	    { { summary "Begin raytracing the view of the source pane.
The results of the raytrace will go to the place
specified by the destination." } }
    button $top.clearB -relief raised -text "fbclear" \
	    -command "do_fbclear $id"
    hoc_register_data $top.clearB "Clear the Framebuffer"\
	    { { summary "Clear the framebuffer specified by the
destination to the background color." } }
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

    color_entry_update $top color rt_control($id,color) $rt_control($id,color)
    rt_solid_list_callback $id
    rt_olist_edit_configure $id
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

    if {$rt_control($id,cooked_dest) != ""} {
	append rt_cmd " -F$rt_control($id,cooked_dest)"
    }

    if {$rt_control($id,size) != ""} {
	set result [regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$"\
		$rt_control($id,size) smatch width junkA junkB junkC height]
	if {$result} {
	    if {$height != ""} {
		append rt_cmd " -w $width -n $height"
		set width "$width.0"
		set height "$height.0"
		set aspect [expr {$width / $height}]
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
	set rgb [getRGBorReset $rt_control($id,top).colorMB rt_control($id,color) $rt_control($id,color)]
	append rt_cmd " -C[lindex $rgb 0]/[lindex $rgb 1]/[lindex $rgb 2]"
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

    if {$rt_control($id,other) != ""} {
	append rt_cmd " $rt_control($id,other)"
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
	    # update rt_control($id,olist) with what's in the text widget
	    rt_olist_apply $id

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

    if [winfo exists $rt_control($id,topAS)] {
	catch { destroy $rt_control($id,topAS) }
    }

    if [winfo exists $rt_control($id,topOLE)] {
	catch { destroy $rt_control($id,topOLE) }
    }

    if [winfo exists $rt_control($id,top)] {
	catch { destroy $rt_control($id,top) }
    }

    set rt_control($id,fixedSrc) 0
    set rt_control($id,fixedDest) 0
    set rt_control($id,omode) one
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
	    { { summary "Pop up a menu of jitter values." } }
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
	    { { summary "Pop up a menu of light models." } }
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

    set hoc_data { { summary "A place to specify other rt options." } }
    label $top.otherL -text "Other options" -anchor e
    hoc_register_data $top.otherL "Other" $hoc_data
    entry $top.otherE -relief sunken -bd 2 -width 2 -textvar rt_control($id,other)
    hoc_register_data $top.otherE "Other" $hoc_data

    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the advanced raytrace panel." } }

    grid $top.nprocL $top.nprocE -sticky nsew -pady 1 -in $top.gridF1
    grid $top.hsampleL $top.hsampleE -sticky nsew -pady 1 -in $top.gridF1
    grid $top.jitterL $top.jitterMB -sticky nsew -pady 1 -in $top.gridF1
    grid $top.lmodelL $top.lmodelMB -sticky nsew -pady 1 -in $top.gridF1
    grid $top.otherL $top.otherE -sticky nsew -pady 1 -in $top.gridF1
    grid columnconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 2 -weight 1
    grid rowconfigure $top.gridF1 3 -weight 1
    grid rowconfigure $top.gridF1 4 -weight 1

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

## - rt_update_dest
#
# Called by main GUI to update the Raytrace Control Panel
#
proc rt_update_dest { id } {
    global mged_gui
    global rt_control
    global port
    global fb
    global fb_all
    global fb_overlay

    if ![info exists rt_control($id,top)] {
	return
    }

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

## - rt_update_src
#
# Called by main GUI to update the Raytrace Control Panel
#
proc rt_update_src { id } {
    global mged_gui
    global rt_control
    global mouse_behavior

    if ![info exists rt_control($id,top)] {
	return
    }

    if ![winfo exists $rt_control($id,top)] {
	return
    }

    if {$rt_control($id,cooked_src) != $mged_gui($id,active_dm)} {
	return
    }

    if {$mouse_behavior == "o"} {
	if {$rt_control($id,omode) == "all"} {
	    set rt_control($id,omode) one
	}
    } else {
	set rt_control($id,omode) all
    }
}

proc rt_olist_edit { id } {
    global mged_gui
    global rt_control

    set top $rt_control($id,topOLE)

    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top

    frame $top.olistF
    text $top.olistT -relief sunken -bd 2 -width 40 -height 10\
	    -yscrollcommand "$top.olistS set" -setgrid true
    hoc_register_data $top.olistT "Object List"\
	    { { summary "This shows the objects that will be raytraced when the
raytrace button is pressed. The contents herein may
be edited directly by the user when in \"several\"
mode (look in the \"Objects\" menu). If there are no
objects herein, then all objects being displayed will
be raytraced." } }
    scrollbar $top.olistS -relief flat -command "$top.olistT yview"
    grid $top.olistT $top.olistS -sticky nsew -in $top.olistF
    grid columnconfigure $top.olistF 0 -weight 1
    grid rowconfigure $top.olistF 0 -weight 1

    frame $top.buttonF
    button $top.clearB -relief raised -text "Clear"\
	    -command "rt_olist_clear $id"
    hoc_register_data $top.clearB "Clear Object List."\
	    { { summary "Clear the object list and the contents
of the object list editor." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "rt_olist_dismiss $id"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss the object list editor. Note - this does
not destroy the object list." } }
    grid x $top.clearB x $top.dismissB x\
	    -sticky nsew -in $top.buttonF
    grid columnconfigure $top.buttonF 0 -weight 1
    grid columnconfigure $top.buttonF 2 -weight 1
    grid columnconfigure $top.buttonF 4 -weight 1

    grid $top.olistF -sticky nsew -padx 8 -pady 4
    grid $top.buttonF -sticky nsew -padx 8 -pady 4
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    rt_olist_reset $id
    rt_olist_edit_configure $id
    place_near_mouse $top
    wm title $top "Object List Editor ($id)"
}

proc rt_olist_edit_configure { id } {
    global rt_control

    switch $rt_control($id,omode) {
	several {
	    rt_olist_edit_enable $id
	}
	one
	   -
	all {
	    rt_olist_edit_disable $id
	}
    }
}

proc rt_olist_edit_disable { id } {
    global rt_control

    $rt_control($id,top).menubar.obj entryconfigure 5 -state disabled

    if ![winfo exists $rt_control($id,topOLE)] {
	return
    }
    $rt_control($id,topOLE).clearB configure -state disabled
    $rt_control($id,topOLE).olistT configure -state disabled
}

proc rt_olist_edit_enable { id } {
    global rt_control

    $rt_control($id,top).menubar.obj entryconfigure 5 -state normal

    if ![winfo exists $rt_control($id,topOLE)] {
	return
    }
    $rt_control($id,topOLE).clearB configure -state normal
    $rt_control($id,topOLE).olistT configure -state normal
}

## - rt_olist_apply
#
# update rt_control($id,olist) with what's in the text widget
#
proc rt_olist_apply { id } {
    global rt_control

    if ![winfo exists $rt_control($id,topOLE)] {
	return
    }

    set rt_control($id,olist) {}
    foreach obj  [$rt_control($id,topOLE).olistT get 0.0 end] {
	lappend rt_control($id,olist) $obj
    }
}

proc rt_olist_dismiss { id } {
    global rt_control

    rt_olist_apply $id
    destroy $rt_control($id,topOLE)
}

## - rt_olist_reset
#
# Reset the text widget with elements from rt_control($id,olist).
#
proc rt_olist_reset { id } {
    global rt_control

    rt_olist_set $id $rt_control($id,olist)
}

## - rt_olist_set
#
# Set the text widget and rt_control($id,olist) with elements from olist.
#
proc rt_olist_set { id olist } {
    global rt_control

    set rt_control($id,olist) $olist

    if ![winfo exists $rt_control($id,topOLE)] {
	return
    }

    # save state of text widget
    set save_state [lindex [$rt_control($id,topOLE).olistT\
	    configure -state] 4]

    # enable the text widget (it may already be enabled, so what)
    # so we can write to it.
    $rt_control($id,topOLE).olistT configure -state normal

    # clean out text widget
    $rt_control($id,topOLE).olistT delete 0.0 end

    # fill it back up with olist elements
    foreach obj $olist {
	$rt_control($id,topOLE).olistT insert end $obj\n
    }

    # put back state
    $rt_control($id,topOLE).olistT configure -state $save_state
}

proc rt_olist_add { id obj } {
    global rt_control

    if {[lsearch $rt_control($id,olist) $obj] != -1} {
	# already in list
	return
    }

    lappend rt_control($id,olist) $obj

    if ![winfo exists $rt_control($id,topOLE)] {
	return
    }

    # save state of text widget
    set save_state [lindex [$rt_control($id,topOLE).olistT\
	    configure -state] 4]

    # enable the text widget (it may already be enabled, so what)
    # so we can write to it.
    $rt_control($id,topOLE).olistT configure -state normal

    $rt_control($id,topOLE).olistT insert end $obj\n

    # put back state
    $rt_control($id,topOLE).olistT configure -state $save_state
}

proc rt_olist_clear { id } {
    global rt_control

    set rt_control($id,olist) {}

    if [winfo exists $rt_control($id,topOLE)] {
	$rt_control($id,topOLE).olistT delete 0.0 end
    }
}

proc rt_set_mouse_behavior { id } {
    global mged_gui
    global rt_control
    global mouse_behavior

    set bad [catch {winset $rt_control($id,cooked_src)} msg]
    if {$bad} {
	return
    }

    switch $rt_control($id,omode) {
	one
	    -
	several {
	    # apply to source window
	    set mouse_behavior o

	    # update the GUI specified by $id
	    if {$rt_control($id,cooked_src) == $mged_gui($id,active_dm)} {
		set mged_gui($id,mouse_behavior) o
	    }

	    # apply to all windows in the GUI specified by $id
	    #mged_apply_local $id "set mouse_behavior o"

	    rt_olist_apply $id
	}
	all {
	    # apply to source window
	    set mouse_behavior d

	    # update the GUI specified by $id
	    if {$rt_control($id,cooked_src) == $mged_gui($id,active_dm)} {
		set mged_gui($id,mouse_behavior) d
	    }

	    # apply to all windows in the GUI specified by $id
	    #mged_apply_local $id "set mouse_behavior d"

	    rt_solid_list_callback $id
	}
    }

    rt_olist_edit_configure $id
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

    if {$raw_dest == ""} {
	set rt_control($id,cooked_dest) ""
	return
    }

    if {$rt_control($id,fixedDest)} {
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

    set bad [catch {winset $rt_control($id,half_baked_dest)} msg]
    if {$bad} {
	return
    }

    # re-enable framebuffer menu
    if [winfo exists $rt_control($id,top)] {
	$rt_control($id,top).menubar entryconfigure 0 -state normal
    }

    set fb 1
#    set fb_all 1

    if {!$listen} {
	set listen 1
    }
    set rt_control($id,cooked_dest) $port
    set rt_control($id,fb) 1
    set rt_control($id,fb_all) $fb_all
    set rt_control($id,fb_overlay) $fb_overlay
    set size [dm size]
    set rt_control($id,size) "[lindex $size 0]x[lindex $size 1]"
    set rt_control($id,color) [rset cs bg]
    color_entry_update $rt_control($id,top) color rt_control($id,color) $rt_control($id,color)

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
    global mouse_behavior

    if {$raw_src == ""} {
	return
    }

    if {$rt_control($id,fixedSrc)} {
	return
    }

    set rt_control($id,raw_src) $raw_src
    set rt_control($id,cooked_src) [rt_half_bake $id $raw_src]

    set bad [catch {winset $rt_control($id,cooked_src)} msg]
    if {$bad} {
	return
    }

    if {$mouse_behavior == "o"} {
	if {![info exists rt_control($id,omode)] ||\
		$rt_control($id,omode) == "all"} {
	    set rt_control($id,omode) one
	}
    } else {
	set rt_control($id,omode) all
    }
}

proc rt_set_fb { id } {
    global mged_gui
    global rt_control
    global fb
    global listen

    if ![winfo exists $rt_control($id,half_baked_dest)] {
	return
    }

    winset $rt_control($id,half_baked_dest)
    set fb $rt_control($id,fb)
    if {$fb && !$listen} {
	set listen 1
    }

    if {$rt_control($id,half_baked_dest) == $mged_gui($id,active_dm)} {
	set mged_gui($id,fb) $rt_control($id,fb)

	if {$mged_gui($id,fb)} {
	    set mged_gui($id,listen) 1
	    .$id.menubar.settings.fb entryconfigure 8 -state normal
	    .$id.menubar.modes entryconfigure 4 -state normal
	} else {
	    .$id.menubar.settings.fb entryconfigure 8 -state disabled
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
# Called to initialize rt_control
# Called by: init_Raytrace, mouse_rt_obj_select
#
proc rt_init_vars { id win } {
    global mged_gui
    global rt_control

    if ![winfo exists $win] {
	return
    }
    
    # initialize once
    if ![info exists rt_control($id,top)] {
	set rt_control($id,top) .$id.rt
	set rt_control($id,topAS) .$id.rtAS
	set rt_control($id,topOLE) .$id.rtOLE
	set rt_control($id,olist) {}
	set rt_control($id,nproc) 1
	set rt_control($id,hsample) 0
	set rt_control($id,jitter) 0
	set rt_control($id,jitterTitle) "None"
	set rt_control($id,lmodel) 0
	set rt_control($id,lmodelTitle) "Full"
	set rt_control($id,other) {}

	# set widget padding
	set rt_control($id,padx) 4
	set rt_control($id,pady) 2

	set rt_control($id,fixedSrc) 0
	set rt_control($id,fixedDest) 0
    }

    # initialize everytime
    rt_cook_src $id $win
    rt_cook_dest $id $win
}

proc rt_force_cook_src { id win } {
    global rt_control

    # save fixed source
    set save_fsrc $rt_control($id,fixedSrc)

    set rt_control($id,fixedSrc) 0
    rt_cook_src $id $win

    # restore fixed source
    set rt_control($id,fixedSrc) $save_fsrc
}

proc rt_force_cook_dest { id win } {
    global rt_control

    # save fixed destination
    set save_fdest $rt_control($id,fixedDest)

    set rt_control($id,fixedDest) 0
    rt_cook_dest $id $win

    # restore fixed destination
    set rt_control($id,fixedDest) $save_fdest
}

#################### CALLBACKS ####################

proc rt_opendb_callback { id } {
    global rt_control

    if ![info exists rt_control($id,top)] {
	return
    }

    if [winfo exists $rt_control($id,top)] {
	set dbname [rt_db_to_pix]
	if {$dbname != ""} {
	    $rt_control($id,top).destMB.menu entryconfigure 7\
		    -label $dbname -command "rt_cook_dest $id $dbname"
	}
    }

    rt_olist_clear $id
}

proc rt_solid_list_callback { id } {
    global rt_control

    if ![info exists rt_control($id,top)] {
	return
    }

    if ![winfo exists $rt_control($id,top)] {
	return
    }

    if {$rt_control($id,omode) == "all"} {
	set rt_control($id,olist) [_mged_who]
	rt_olist_reset $id
	return
    }
}

proc rt_handle_configure { id } {
    after cancel rt_set_fb_size $id
    after idle rt_set_fb_size $id
}