# 
#                        R T . T C L
#
#	Widget for raytracing MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_rt"

proc init_Raytrace { id } {
    global player_screen
    global rt_fb_or_file
    global rt_fb
    global rt_file
    global rt_size
    global rt_color
    global rt_nproc
    global rt_hsample
    global rt_jitter
    global rt_lmodel

    set top .$id.do_rt

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists rt_fb_or_file($id)] {
	set rt_fb_or_file($id) framebuffer
    }

    if ![info exists rt_fb($id)] {
	set rt_fb($id) ""
    }

    if ![info exists rt_file($id)] {
	regsub \.g$ [_mged_opendb] .pix default_file
	set rt_file($id) $default_file
    }

    if ![info exists rt_size($id)] {
	set rt_size($id) 512
    }

    if ![info exists rt_color($id)] {
	set rt_color($id) "0 0 0"
    }

    if ![info exists rt_nproc($id)] {
	set rt_nproc($id) 1
    }

    if ![info exists rt_hsample($id)] {
	set rt_hsample($id) 0
    }

    if ![info exists rt_jitter($id)] {
	set rt_jitter($id) 0
    }

    if ![info exists rt_lmodel($id)] {
	set rt_lmodel($id) 0
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF -relief groove -bd 2
    frame $top.gridF2
    frame $top.gridF3
    frame $top.framebufferF -relief sunken -bd 2
    frame $top.filenameF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2
    frame $top.colorF -relief sunken -bd 2

    if {$rt_fb_or_file($id) == "framebuffer"} {
	set fb_state normal
	set file_state disabled
    } else {
	set fb_state disabled
	set file_state normal
    }

    entry $top.framebufferE -relief flat -width 12 -textvar rt_fb($id)\
	    -state $fb_state
    radiobutton $top.framebufferRB -text "Frame Buffer" -anchor w\
	    -value framebuffer -variable rt_fb_or_file($id)\
	    -command "rt_set_fb_state $id"

    entry $top.filenameE -relief flat -width 12 -textvar rt_file($id)\
	    -state $file_state
    radiobutton $top.filenameRB -text "File Name" -anchor w\
	    -value filename -variable rt_fb_or_file($id)\
	    -command "rt_set_file_state $id"

    label $top.sizeL -text "Size" -anchor w
    entry $top.sizeE -relief flat -width 12 -textvar rt_size($id)
    menubutton $top.sizeMB -relief raised -bd 2\
	    -menu $top.sizeMB.sizeM -indicatoron 1
    menu $top.sizeMB.sizeM -tearoff 0
    $top.sizeMB.sizeM add command -label 128\
	    -command "set rt_size($id) 128"
    $top.sizeMB.sizeM add command -label 256\
	    -command "set rt_size($id) 256"
    $top.sizeMB.sizeM add command -label 512\
	    -command "set rt_size($id) 512"
    $top.sizeMB.sizeM add command -label 640x480\
	    -command "set rt_size($id) 640x480"
    $top.sizeMB.sizeM add command -label 720x486\
	    -command "set rt_size($id) 720x486"
    $top.sizeMB.sizeM add command -label 1024\
	    -command "set rt_size($id) 1024"

    label $top.colorL -text "Background Color" -anchor w
    entry $top.colorE -relief flat -width 12 -textvar rt_color($id)
    menubutton $top.colorMB -relief raised -bd 2\
	    -menu $top.colorMB.colorM -indicatoron 1
    menu $top.colorMB.colorM -tearoff 0
    $top.colorMB.colorM add command -label "Choose Color"\
	    -command "rt_choose_color $id"
    $top.colorMB.colorM add separator
    $top.colorMB.colorM add command -label black\
	     -command "set rt_color($id) \"0 0 0\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label white\
	     -command "set rt_color($id) \"220 220 220\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label red\
	     -command "set rt_color($id) \"220 0 0\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label green\
	     -command "set rt_color($id) \"0 220 0\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label blue\
	     -command "set rt_color($id) \"0 0 220\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label yellow\
	     -command "set rt_color($id) \"220 220 0\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label cyan\
	    -command "set rt_color($id) \"0 220 220\"; rt_set_colorMB $id"
    $top.colorMB.colorM add command -label magenta\
	    -command "set rt_color($id) \"220 0 220\"; rt_set_colorMB $id"

    button $top.advancedB -relief raised -text "Advanced Settings..."\
	    -command "do_Advanced_Settings $id"
    button $top.raytraceB -relief raised -text "Raytrace" \
	    -command "do_Raytrace $id"
    button $top.clearB -relief raised -text "fbclear" \
	    -command "do_fbclear $id" -state $fb_state
    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "catch { destroy $top }"

    grid $top.framebufferE -sticky "ew" -in $top.framebufferF
    grid $top.framebufferF $top.framebufferRB -sticky "ew"\
	    -in $top.gridF -padx 8 -pady 8
    grid $top.filenameE -sticky "ew" -in $top.filenameF
    grid $top.filenameF $top.filenameRB -sticky "ew"\
	    -in $top.gridF -padx 8 -pady 8
    grid columnconfigure $top.framebufferF 0 -weight 1
    grid columnconfigure $top.filenameF 0 -weight 1
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.sizeE $top.sizeMB -sticky "ew" -in $top.sizeF
    grid $top.sizeF $top.sizeL -sticky "ew" -in $top.gridF2 -pady 4
    grid $top.colorE $top.colorMB -sticky "ew" -in $top.colorF
    grid $top.colorF $top.colorL -sticky "ew" -in $top.gridF2 -pady 4

    grid columnconfigure $top.sizeF 0 -weight 1
    grid columnconfigure $top.colorF 0 -weight 1
    grid columnconfigure $top.gridF2 0 -weight 1

    grid $top.advancedB -sticky "nsew" -in $top.gridF3
    grid configure $top.advancedB -columnspan 5
    grid $top.raytraceB x $top.clearB x $top.dismissB -sticky "nsew" -in $top.gridF3
    grid columnconfigure $top.gridF3 1 -weight 1 -minsize 8
    grid columnconfigure $top.gridF3 3 -weight 1 -minsize 8

    pack $top.gridF $top.gridF2 $top.gridF3 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    bind $top.colorE <Return> "rt_set_colorMB $id"
    rt_set_colorMB $id

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Raytrace..."
}

proc do_Raytrace { id } {
    global player_screen
    global rt_fb_or_file
    global rt_fb
    global rt_file
    global rt_size
    global rt_color
    global rt_nproc
    global rt_hsample
    global rt_jitter
    global rt_lmodel

    cmd_set $id
    set rt_cmd "_mged_rt"

    if {$rt_fb_or_file($id) == "filename"} {
	if {$rt_file($id) != ""} {
	    if {[file exists $rt_file($id)]} {
		set result [mged_dialog .$id.rtDialog $player_screen($id)\
			"Overwrite $rt_file($id)?"\
			"Overwrite $rt_file($id)?"\
			"" 0 OK CANCEL]

		if {$result} {
		    return
		}
	    }

	    append rt_cmd " -o $rt_file($id)"
	} else {
	    mged_dialog .$id.rtDialog $player_screen($id)\
		    "No file name specified!"\
		    "No file name specified!"\
		    "" 0 OK
	    return
	}
    } else {
	if {$rt_fb($id) != ""} {
	    append rt_cmd " -F $rt_fb($id)"
	}
    }

    if {$rt_size($id) != ""} {
	set result [regexp "^(\[0-9\]+)\[xX\]?(\[0-9\]*)$"\
		$rt_size($id) smatch width height]
	if {$result} {
	    if {$height != ""} {
		append rt_cmd " -w $width -n $height"
	    } else {
		append rt_cmd " -s $width"
	    }
	} else {
	    mged_dialog .$id.rtDialog $player_screen($id)\
		    "Improper size specification!"\
		    "Improper size specification: $rt_size($id)"\
		    "" 0 OK
	    return
	}
    }

    if {$rt_color($id) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_color($id) cmatch red green blue]
	if {$result} {
	    append rt_cmd " -C$red/$green/$blue"
	} else {
	    mged_dialog .$id.rtDialog $player_screen($id)\
		    "Improper color specification!"\
		    "Improper color specification: $rt_color($id)"\
		    "" 0 OK
	    return
	}
    }

    if {$rt_nproc($id) != ""} {
	append rt_cmd " -P$rt_nproc($id)"
    }

    if {$rt_hsample($id) != ""} {
	append rt_cmd " -H$rt_hsample($id)"
    }

    if {$rt_jitter($id) != ""} {
	append rt_cmd " -J$rt_jitter($id)"
    }

    if {$rt_lmodel($id) != ""} {
	append rt_cmd " -l$rt_lmodel($id)"
    }


    catch {eval $rt_cmd}
}

proc do_fbclear { id } {
    global player_screen
    global rt_fb_or_file
    global rt_fb
    global rt_color

    if {$rt_color($id) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_color($id) cmatch red green blue]
	if {!$result} {
	    mged_dialog .$id.rtDialog $player_screen($id)\
		    "Improper color specification!"\
		    "Improper color specification: $rt_color($id)"\
		    "" 0 OK
	    return
	}
    } else {
	set red 0
	set green 0
	set blue 0
    }

    if {$rt_fb($id) != ""} {
	set result [catch { exec fbclear -F $rt_fb($id) $red $green $blue } rt_error]
    } else {
	set result [catch { exec fbclear $red $green $blue } rt_error]
    }

    if {$result != 0} {
	mged_dialog .$id.rtDialog $player_screen($id)\
		"RT Error!" "Rt Error: $rt_error" "" 0 OK
    }
}

proc rt_set_fb_state { id } {
    set top .$id.do_rt

    $top.clearB configure -state normal
    $top.framebufferE configure -state normal
    $top.filenameE configure -state disabled

    focus $top.framebufferE
}

proc rt_set_file_state { id } {
    set top .$id.do_rt

    $top.clearB configure -state disabled
    $top.framebufferE configure -state disabled
    $top.filenameE configure -state normal

    focus $top.filenameE
}

proc rt_choose_color { id } {
    global player_screen
    global rt_color

    set top .$id.do_rt
    set colors [chooseColor $top]

    if {[llength $colors] != 2} {
	mged_dialog .$id.rtDialog $player_screen($id)\
		"Error choosing a color!"\
		"Error choosing a color!"\
		"" 0 OK
	return
    }

    $top.colorMB configure -bg [lindex $colors 0]
    set rt_color($id) [lindex $colors 1]
}

proc rt_set_colorMB { id } {
    global player_screen
    global rt_color

    set top .$id.do_rt

    if {$rt_color($id) != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rt_color($id) cmatch red green blue]
	if {!$result} {
	    mged_dialog .$id.rtDialog $player_screen($id)\
		    "Improper color specification!"\
		    "Improper color specification: $rt_color($id)"\
		    "" 0 OK
	    return
	}
    } else {
	return
    }

    $top.colorMB configure -bg [format "#%02x%02x%02x" $red $green $blue]
}

proc do_Advanced_Settings { id } {
    global player_screen
    global rt_hsample
    global rt_jitter
    global rt_lmodel
    global rt_nproc

    set top .$id.do_rtAS
    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2
    frame $top.nprocF -relief sunken -bd 2
    frame $top.hsampleF -relief sunken -bd 2
    frame $top.jitterF -relief sunken -bd 2
    frame $top.lmodelF -relief sunken -bd 2

    label $top.nprocL -text "# of Processors" -anchor w
    entry $top.nprocE -relief flat -width 4 -textvar rt_nproc($id)

    label $top.hsampleL -text "Hypersample" -anchor w
    entry $top.hsampleE -relief flat -width 4 -textvar rt_hsample($id)

    label $top.jitterL -text "Jitter" -anchor w
    entry $top.jitterE -relief flat -width 4 -textvar rt_jitter($id)
    menubutton $top.jitterMB -relief raised -bd 2\
	    -menu $top.jitterMB.jitterM -indicatoron 1
    menu $top.jitterMB.jitterM -tearoff 0
    $top.jitterMB.jitterM add command -label 0\
	 -command "set rt_jitter($id) 0"
    $top.jitterMB.jitterM add command -label 1\
	 -command "set rt_jitter($id) 1"
    $top.jitterMB.jitterM add command -label 2\
	 -command "set rt_jitter($id) 2"
    $top.jitterMB.jitterM add command -label 3\
	 -command "set rt_jitter($id) 3"
    
    label $top.lmodelL -text "Light Model" -anchor w
    entry $top.lmodelE -relief flat -width 4 -textvar rt_lmodel($id)
    menubutton $top.lmodelMB -relief raised -bd 2\
	    -menu $top.lmodelMB.lmodelM -indicatoron 1
    menu $top.lmodelMB.lmodelM -tearoff 0
    $top.lmodelMB.lmodelM add command -label 0\
	    -command "set rt_lmodel($id) 0"
    $top.lmodelMB.lmodelM add command -label 1\
	    -command "set rt_lmodel($id) 1"
    $top.lmodelMB.lmodelM add command -label 2\
	    -command "set rt_lmodel($id) 2"
    $top.lmodelMB.lmodelM add command -label 3\
	    -command "set rt_lmodel($id) 3"
    $top.lmodelMB.lmodelM add command -label 4\
	    -command "set rt_lmodel($id) 4"
    $top.lmodelMB.lmodelM add command -label 5\
	    -command "set rt_lmodel($id) 5"

    button $top.dismissB -relief raised -text "Dismiss" \
	    -command "catch { destroy $top }"

    grid $top.nprocE -sticky "ew" -in $top.nprocF
    grid $top.hsampleE -sticky "ew" -in $top.hsampleF
    grid $top.jitterE $top.jitterMB -sticky "ew" -in $top.jitterF
    grid $top.lmodelE $top.lmodelMB -sticky "ew" -in $top.lmodelF

    grid $top.nprocF $top.nprocL -sticky "ew" -in $top.gridF -pady 4
    grid $top.hsampleF $top.hsampleL -sticky "ew" -in $top.gridF -pady 4
    grid $top.jitterF $top.jitterL -sticky "ew" -in $top.gridF -pady 4
    grid $top.lmodelF $top.lmodelL -sticky "ew" -in $top.gridF -pady 4
    grid $top.dismissB -in $top.gridF2

    grid columnconfigure $top.nprocF 0 -weight 1
    grid columnconfigure $top.hsampleF 0 -weight 1
    grid columnconfigure $top.jitterF 0 -weight 1
    grid columnconfigure $top.lmodelF 0 -weight 1
    grid columnconfigure $top.gridF 0 -weight 1

    pack $top.gridF $top.gridF2 -side top -expand 1 -fill both -padx 8 -pady 4

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Advanced Settings..."
}
