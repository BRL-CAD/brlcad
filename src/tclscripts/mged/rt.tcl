#                          R T . T C L
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
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
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
	    -label "One" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "one" "Objects - one"\
	    { { summary "Raytrace only the selected object. Note - this will
change the mouse behavior of the source window to
\"o\" (i.e. raytrace object)." } }
    $top.menubar.obj add radiobutton -value several -variable rt_control($id,omode)\
	    -label "Several" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "several" "Objects - several"\
	    { { summary "Add the selected object to the list of objects to be
raytraced. Note - this will change the mouse behavior
of the source window to \"o\" (i.e. pick raytrace objects)." } }
    $top.menubar.obj add radiobutton -value all -variable rt_control($id,omode)\
	    -label "All" -underline 0\
	    -command "rt_set_mouse_behavior $id"
    hoc_register_menu_data "Objects" "all" "Objects - all"\
	    { { summary "Raytrace all displayed objects. Note - this will
change the mouse behavior of the source window to
\"d\" (i.e. the default mouse behavior)." } }
    $top.menubar.obj add separator
    $top.menubar.obj add command -label "Edit List"\
	    -command "rt_olist_edit $id"
    hoc_register_menu_data "Objects" "edit list" "Edit List"\
	    { { summary "Pop up a tool to edit the list
of objects to be raytraced." } }
    $top.menubar.obj add command -label "Clear List"\
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
    label $top.srcL -text "Source" -anchor e -padx 0 -pady 0
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
    label $top.destL -text "Destination" -anchor e -padx 0 -pady 0
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

    label $top.sizeL -text "Size" -anchor e -padx 0 -pady 0
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

    label $top.colorL -text "Background Color" -anchor e -padx 0 -pady 0
    hoc_register_data $top.colorL "Background Color"\
	    { { summary "This refers to the background color
used for raytracing. This is also the color
that is used when clearing the framebuffer." } }

    # $top.colorF is the name of the container created by color_entry_build
    # that contains the entry and menubutton for specifying a color
    color_entry_build $top color rt_control($id,color)\
	    "color_entry_chooser $id $top color \"Background Color\"\
	    rt_control $id,color"\
	    12 $rt_control($id,color) rt

    button $top.advancedB -relief raised -text "Advanced Settings..."\
	    -command "do_Advanced_Settings $id" \
	    -padx 0.5m -pady 0.5m
    hoc_register_data $top.advancedB "Advanced Settings"\
	    { { summary "Pop up another GUI for advanced settings." } }
    button $top.raytraceB -relief raised -text "Raytrace" \
	    -command "do_Raytrace $id" \
	    -padx 0.5m -pady 0.5m
    hoc_register_data $top.raytraceB "Raytrace"\
	    { { summary "Begin raytracing the view of the source pane.
The results of the raytrace will go to the place
specified by the destination." } }
    button $top.clearB -relief raised -text "fbclear" \
	    -command "do_fbclear $id" \
	    -padx 0.5m -pady 0.5m
    hoc_register_data $top.clearB "Clear the Framebuffer"\
	    { { summary "Clear the framebuffer specified by the
destination to the background color." } }
    button $top.abortB -relief raised -text "Abort" \
	    -command "rtabort" \
	    -padx 0.5m -pady 0.5m
    hoc_register_data $top.abortB "Abort Raytrace"\
	    { { summary "Abort all raytraces initiated by MGED." } }
    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "rt_dismiss $id" \
	    -padx 0.5m -pady 0.5m
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

    grid $top.srcL $top.srcF - - - -pady 1 -sticky nsew -in $top.gridF1
    grid $top.destL $top.destF - - - -pady 1 -sticky nsew -in $top.gridF1
    grid $top.sizeL $top.sizeF - - - -pady 1 -sticky nsew -in $top.gridF1
    grid $top.colorL $top.colorF - - - -pady 1 -sticky nsew -in $top.gridF1
    grid $top.raytraceB $top.abortB $top.clearB x $top.advancedB -sticky "ew" -in $top.gridF1
    grid columnconfigure $top.gridF1 3 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 1 -weight 1
    grid rowconfigure $top.gridF1 2 -weight 1
    grid rowconfigure $top.gridF1 3 -weight 1

    grid $top.gridF1 -padx 4 -pady 4 -sticky nsew -in $top.gridF2
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    grid $top.dismissB -sticky "ns" -in $top.gridF3

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
    global ::tk::Priv

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
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Improper size specification!"\
		    "Improper size specification: $rt_control($id,size)"\
		    "" 0 OK
	    return
	}
    } else {
	set aspect 1
    }

    if {[winfo exists $rt_control($id,top).colorMB] && $rt_control($id,color) != ""} {
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
	if {$rt_control($id,lmodel) == 7} {
		append rt_cmd ",$rt_control($id,pmGlobalPhotonsEntry),$rt_control($id,pmCausticsPercentScale),$rt_control($id,pmIrradianceRaysScale),$rt_control($id,pmAngularTolerance),$rt_control($id,pmRandomSeedEntry),$rt_control($id,pmImportanceMapping),$rt_control($id,pmIrradianceHypersamplingCache),$rt_control($id,pmVisualizeIrradiance),$rt_control($id,pmScaleIndirectEntry),$rt_control($id,pmCacheFileEntry) -A0"
	}
    }
	#puts $rt_cmd

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
    global ::tk::Priv

    if {$rt_control($id,cooked_dest) == ""} {
	return
    }

    if {$rt_control($id,color) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_control($id,color) cmatch red green blue]
	if {!$result} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
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
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
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
	    set rt_control($id,lmodelTitle) \"Full\";\
            PMMenu $id $top 0"
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
	    set rt_control($id,lmodelTitle) \"Diffuse\";\
            PMMenu $id $top 0"
    hoc_register_menu_data "Light Model" "Diffuse"\
	    "Lighting Model - Diffuse"\
	    { { summary "This is a diffuse lighting model only and\
is intended for debugging." } }
    $top.lmodelMB.lmodelM add command -label "Surface Normals"\
	    -command "set rt_control($id,lmodel) 2;\
	    set rt_control($id,lmodelTitle) \"Surface Normals\";\
            PMMenu $id $top 0"
    hoc_register_menu_data "Light Model" "Surface Normals"\
	    "Lighting Model - Surface Normals"\
	    { { summary "This lighting model displays the surface normals
as colors which makes it useful for examining
curvature and surface orientation." } }
    $top.lmodelMB.lmodelM add command -label "Diffuse - 3 light"\
	    -command "set rt_control($id,lmodel) 3;\
	    set rt_control($id,lmodelTitle) \"Diffuse - 3 light\";\
            PMMenu $id $top 0"
    hoc_register_menu_data "Light Model" "Diffuse - 3 light"\
	    "Lighting Model - Diffuse 3 Light"\
	    { { summary "This is a three-light diffuse-lighting model\
and is intended for debugging." } }
    $top.lmodelMB.lmodelM add command -label "Curvature - inverse radius"\
	    -command "set rt_control($id,lmodel) 4;\
	    set rt_control($id,lmodelTitle) \"Curvature - inverse radius\";\
            PMMenu $id $top 0"
    hoc_register_menu_data "Light Model" "Curvature - inverse radius"\
	    "Lighting Model - Curvature, Inverse Radius"\
	    { { summary "This is a curvature debugging display,
showing the inverse radius of curvature." } }
    $top.lmodelMB.lmodelM add command -label "Curvature - direction vector"\
	    -command "set rt_control($id,lmodel) 5;\
	    set rt_control($id,lmodelTitle) \"Curvature - direction vector\";\
            PMMenu $id $top 0"
    hoc_register_menu_data "Light Model" "Curvature - direction vector"\
	    "Lighting Model - Curvature, Direction Vector"\
	    { { summary "This is a curvature debugging display,
showing the principal direction vector." } }
    $top.lmodelMB.lmodelM add command -label "Photon Mapping"\
	    -command "set rt_control($id,lmodel) 7;\
	    set rt_control($id,lmodelTitle) \"Photon Mapping\";\
	    PMMenu $id $top 1"
    hoc_register_menu_data "Light Model" "Photon Mapping"\
	    "Lighting Model - Photon Mapping"\
	    { { summary "This is a replacement for the Full Phong lighting model." } }

    set hoc_data { { summary "A place to specify other rt options." } }
    label $top.otherL -text "Other Options" -anchor e
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

    PMMenu $id $top 2
    if {$rt_control($id,lmodelTitle) == "Photon Mapping"} {
      PMMenu $id $top 1
    }
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
	set mged_gui($id,listen) $listen
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
	set rt_control($id,nproc) 16
	set rt_control($id,hsample) 0
	set rt_control($id,jitter) 0
	set rt_control($id,jitterTitle) "None"
	set rt_control($id,lmodel) 0
	set rt_control($id,lmodelTitle) "Full"
	set rt_control($id,other) {}

        ## Photon Mapping Init Stuff
	set rt_control($id,pmGlobalPhotonsEntry) 16384
	set rt_control($id,pmGlobalPhotonsScale) 14
	set rt_control($id,pmCausticsPercentScale) 0
	set rt_control($id,pmIrradianceRaysEntry) 100
	set rt_control($id,pmIrradianceRaysScale) 10
	set rt_control($id,pmAngularTolerance) 60.0
	set rt_control($id,pmRandomSeedEntry) 0
	set rt_control($id,pmImportanceMapping) 0
	set rt_control($id,pmIrradianceHypersamplingCache) 0
	set rt_control($id,pmVisualizeIrradiance) 0
	set rt_control($id,pmScaleIndirectScale) 1.0
	set rt_control($id,pmCacheFileEntry) ""


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


##
##  Photon Mapping Interface Procedures (JLS_2003)
##


## Update for each inividual Entry/Scale pair
proc PMNonLinearEvent {EntryWidget ScaleValue} {
  ## Purge old characters
  $EntryWidget delete 0 [string length [$EntryWidget get]]
  ## Insert new characters
  $EntryWidget insert 0 [expr int(pow(2,$ScaleValue))]
}


## Update for each inividual Entry/Scale pair
proc PMLinearEvent {EntryWidget ScaleValue} {
  ## Purge old characters
  $EntryWidget delete 0 [string length [$EntryWidget get]]
  ## Insert new characters
  $EntryWidget insert 0 $ScaleValue
}


## Update for each inividual Entry/Scale pair
proc PMRaysEvent {EntryWidget ScaleValue} {
  ## Purge old characters
  $EntryWidget delete 0 [string length [$EntryWidget get]]
  ## Insert new characters
  $EntryWidget insert 0 [expr int(pow($ScaleValue,2))]
}


proc PMMenu {id top enable} {
  if {$enable == 2} {
    ## Setup
    frame $top.gridF4 -relief groove -bd 2

    set hoc_data { { summary "Photon Mapping is used to simulate indirect illumination and caustics.\nThis is a substitute to using ambient light to simulate indirect illumination." } }

    label $top.gridF4.pmOptionsLabel -text "Photon Mapping Controls" -foreground #666666
    hoc_register_data $top.gridF4.pmOptionsLabel "Photon Mapping Synopsis" $hoc_data
    grid $top.gridF4.pmOptionsLabel -row 0 -column 0 -columnspan 3


    ## Number of Photons in the Global Map
    set hoc_data { { summary "The total number of photons that will be used in the scene." } }

    label $top.gridF4.pmGlobalPhotonsLabel -text "Global Photons"
    grid $top.gridF4.pmGlobalPhotonsLabel -row 1 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmGlobalPhotonsLabel "Global Photons" $hoc_data

    entry $top.gridF4.pmGlobalPhotonsEntry -width 8 -textvar rt_control($id,pmGlobalPhotonsEntry)
    grid $top.gridF4.pmGlobalPhotonsEntry -row 1 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmGlobalPhotonsEntry "Global Photons" $hoc_data

    scale $top.gridF4.pmGlobalPhotonsScale -orient horizontal -showvalue 0 -from 10 -to 24 -command "PMNonLinearEvent $top.gridF4.pmGlobalPhotonsEntry" -variable rt_control($id,pmGlobalPhotonsScale)
    grid $top.gridF4.pmGlobalPhotonsScale -row 1 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmGlobalPhotonsScale "Global Photons" $hoc_data


    ## Percent of Global Photons that Caustics may consume
    set hoc_data { { summary "The percent of global photons that should be used as caustic photons.\nIf there are no caustics in the scene then the percentage of caustic\nphotons chosen will be wasted." } }

    label $top.gridF4.pmCausticsPercentLabel -text "Caustics Percent"
    grid $top.gridF4.pmCausticsPercentLabel -row 2 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmCausticsPercentLabel "Caustics Percent" $hoc_data

    entry $top.gridF4.pmCausticsPercentEntry -width 8 -textvar rt_control($id,pmCausticsPercentEntry)
    grid $top.gridF4.pmCausticsPercentEntry -row 2 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmCausticsPercentEntry "Caustics Percent" $hoc_data

    scale $top.gridF4.pmCausticsPercentScale -orient horizontal -showvalue 0 -from 0 -to 100 -command "PMLinearEvent $top.gridF4.pmCausticsPercentEntry" -variable rt_control($id,pmCausticsPercentScale)
    grid $top.gridF4.pmCausticsPercentScale -row 2 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmCausticsPercentScale "Caustics Percent" $hoc_data


    ## Number of Sample Rays for Irradidance Hemisphere Sample
    set hoc_data { { summary "The number of irradiance rays used to approximate irradiance at each irradiance cache point.\nIrradiance is the incoming light at a given point.  Increasing the number of rays will decrease\nthe amount of noise in the scene." } }

    label $top.gridF4.pmIrradianceRaysLabel -text "Irradiance Rays"
    grid $top.gridF4.pmIrradianceRaysLabel -row 3 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmIrradianceRaysLabel "Irradiance Rays" $hoc_data

    entry $top.gridF4.pmIrradianceRaysEntry -width 8 -textvar rt_control($id,pmIrradianceRaysEntry)
    grid $top.gridF4.pmIrradianceRaysEntry -row 3 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmIrradianceRaysEntry "Irradiance Rays" $hoc_data

    scale $top.gridF4.pmIrradianceRaysScale -orient horizontal -showvalue 0 -from 4 -to 32 -command "PMRaysEvent $top.gridF4.pmIrradianceRaysEntry" -variable rt_control($id,pmIrradianceRaysScale)
    grid $top.gridF4.pmIrradianceRaysScale -row 3 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmIrradianceRaysScale "Irradiance Rays" $hoc_data


    ## Angular Tolerance Entry and Scale
    set hoc_data { { summary "The angular tolerance used when looking for candidate photons.\nSetting this high will remove normal banding, however photons\nmay appear in unwanted areas around edges." } }

    label $top.gridF4.pmAngularToleranceLabel -text "Angular Tol"
    grid $top.gridF4.pmAngularToleranceLabel -row 4 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmAngularToleranceLabel "Angular Tolerance" $hoc_data

    entry $top.gridF4.pmAngularToleranceEntry -width 8 -textvar rt_control($id,pmAngularTolerance)
    grid $top.gridF4.pmAngularToleranceEntry -row 4 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmAngularToleranceEntry "Angular Tolerance" $hoc_data

    scale $top.gridF4.pmAngularToleranceScale -orient horizontal -showvalue 0 -from 0 -to 180 -command "PMLinearEvent $top.gridF4.pmAngularToleranceEntry" -variable rt_control($id,pmAngularTolerance)
    grid $top.gridF4.pmAngularToleranceScale -row 4 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmAngularToleranceScale "Angular Tolerance" $hoc_data


    ## Random Seed for Emitting Photons
    set hoc_data { { summary "Sets the random seed used for every part of photon mapping.\nFor large numbers of photons this setting will have no affect\non the scene.  This setting is useful for reproducing the same\nmap for a given scene." } }

    label $top.gridF4.pmRandomSeedLabel -text "Random Seed"
    grid $top.gridF4.pmRandomSeedLabel -row 5 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmRandomSeedLabel "Random Seed" $hoc_data

    entry $top.gridF4.pmRandomSeedEntry -width 8 -textvar rt_control($id,pmRandomSeedEntry)
    grid $top.gridF4.pmRandomSeedEntry -row 5 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmRandomSeedEntry "Random Seed" $hoc_data

    scale $top.gridF4.pmRandomSeedScale -orient horizontal -showvalue 0 -from 0 -to 9 -command "PMLinearEvent $top.gridF4.pmRandomSeedEntry" -variable rt_control($id,pmRandomSeedScale)
    grid $top.gridF4.pmRandomSeedScale -row 5 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmRandomSeedScale "Random Seed" $hoc_data


    ## Scale Indirect
    set hoc_data { { summary "This setting scales indirect illumination in the scene by the specified amount.\nThis is useful for tweaking the amount of global illumination the scene\nshould be receiving." } }

    label $top.gridF4.pmScaleIndirectLabel -text "Scale Indirect"
    grid $top.gridF4.pmScaleIndirectLabel -row 6 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmScaleIndirectLabel "Scale Indirect" $hoc_data

    entry $top.gridF4.pmScaleIndirectEntry -width 8 -textvar rt_control($id,pmScaleIndirectEntry)
    grid $top.gridF4.pmScaleIndirectEntry -row 6 -column 1 -sticky news
    hoc_register_data $top.gridF4.pmScaleIndirectEntry "Scale Indirect" $hoc_data

    scale $top.gridF4.pmScaleIndirectScale -orient horizontal -showvalue 0 -from 0.1 -to 10.0 -resolution 0.01 -command "PMLinearEvent $top.gridF4.pmScaleIndirectEntry" -variable rt_control($id,pmScaleIndirectScale)
    grid $top.gridF4.pmScaleIndirectScale -row 6 -column 2 -sticky news
    hoc_register_data $top.gridF4.pmScaleIndirectScale "Scale Indirect" $hoc_data


    ## Cache File
    set hoc_data { { summary "Entering a valid file name is this box permits saving and loading of the photon mapping\ndata.  This is useful for rendering a scene with static geometry without having to\ngenerate the data each time." } }

    label $top.gridF4.pmCacheFileLabel -text "Load/Save File"
    grid $top.gridF4.pmCacheFileLabel -row 7 -column 0 -sticky e
    hoc_register_data $top.gridF4.pmCacheFileLabel "Load/Save Photon Map Data" $hoc_data

    entry $top.gridF4.pmCacheFileEntry -width 8 -textvar rt_control($id,pmCacheFileEntry)
    grid $top.gridF4.pmCacheFileEntry -row 7 -column 1 -columnspan 2 -sticky news
    hoc_register_data $top.gridF4.pmCacheFileEntry "Load/Save Photon Map Data" $hoc_data


    ## Generate Importons
    set hoc_data { { summary "Use this setting to distribute the photons in a view dependent fashion.  This is useful when\nscenes are dominated by complex geometry in which only small portions are being viewed.\nImportance Mapping is view dependent and therefore should not be used in conjunction\nwith the file saving option unless the view remains static." } }

    checkbutton $top.gridF4.pmImportanceMapping -text "Use Importance Mapping" -variable rt_control($id,pmImportanceMapping)
    grid $top.gridF4.pmImportanceMapping -row 8 -column 1 -columnspan 2 -sticky w
    hoc_register_data $top.gridF4.pmImportanceMapping "Importance Mapping" $hoc_data
#    $top.gridF4.pmImportanceMapping select

    ## Irradiance Hypersampling Cache
    set hoc_data { { summary "With this option enabled only one irradiance and caustic lookup will be performed per pixel." } }

    checkbutton $top.gridF4.pmIrradianceHypersamplingCache -text "Use Irradiance Hypersampling Cache" -variable rt_control($id,pmIrradianceHypersamplingCache)
    grid $top.gridF4.pmIrradianceHypersamplingCache -row 9 -column 1 -columnspan 2 -sticky w
    hoc_register_data $top.gridF4.pmIrradianceHypersamplingCache "Irradiance Hypersampling Cache" $hoc_data
#    $top.gridF4.pmIrradianceHypersamplingCache select

    ## Irradiance Visualization
    set hoc_data { { summary "With this option enabled only the indirect illumination and caustics will be rendered." } }

    checkbutton $top.gridF4.pmVisualizeIrradiance -text "Visualize Irradiance Cache" -variable rt_control($id,pmVisualizeIrradiance)
    grid $top.gridF4.pmVisualizeIrradiance -row 10 -column 1 -columnspan 2 -sticky w
    hoc_register_data $top.gridF4.pmVisualizeIrradiance "Visualize Irradiance" $hoc_data
#    $top.gridF4.pmVisualizeIrradiance select

    ## Edge Compensator
#    checkbutton $top.gridF4.pmEdgeCompensation -text "Edge Compensation"
#    grid $top.gridF4.pmEdgeCompensation -row 7 -column 0 -columnspan 2 -sticky w
#    $top.gridF4.pmEdgeCompensation select

    ## Use Direct Photons for Direct Illumination
#    checkbutton $top.gridF4.pmDirectIllumination -text "Photon Map for Direct Illum."
#    $top.gridF4.pmDirectIllumination select
#    grid $top.gridF4.pmDirectIllumination -row 7 -column 2 -sticky w

  } elseif {$enable == 1} {
    if {[grid size $top.gridF1] != "2 6"} {
      grid $top.gridF4 -sticky news -columnspan 2 -in $top.gridF1 -ipadx 8 -ipady 8
    }
  } else {
    if {[grid size $top.gridF1] == "2 6"} {
      grid remove $top.gridF4
    }
  }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
