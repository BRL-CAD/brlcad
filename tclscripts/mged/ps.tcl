#
#			P S . T C L
#
#	Widget for producing PostScript files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_ps"

proc init_psTool { id } {
    global player_screen
    global ps_file
    global ps_title
    global ps_creator
    global ps_font
    global ps_size
    global ps_linewidth
    global ps_zclip

    set top .$id.do_ps

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists ps_file($id)] {
	regsub \.g$ [_mged_opendb] .ps default_file
	set ps_file($id) $default_file
    }

    if ![info exists ps_title($id)] {
	set ps_title($id) "No Title"
    }

    if ![info exists ps_creator($id)] {
	set ps_creator($id) "$id"
    }

    if ![info exists ps_font($id)] {
	set ps_font($id) "Courier"
    }

    if ![info exists ps_size($id)] {
	set ps_size($id) 4.5
    }

    if ![info exists ps_linewidth($id)] {
	set ps_linewidth($id) 1
    }

    if ![info exists ps_zclip($id)] {
	set ps_zclip($id) 1
    }

    toplevel $top -screen $player_screen($id)

    frame $top.elF
    frame $top.fileF -relief sunken -bd 2
    frame $top.titleF -relief sunken -bd 2
    frame $top.creatorF -relief sunken -bd 2
    frame $top.fontF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2
    frame $top.linewidthF -relief sunken -bd 2
    frame $top.buttonF
    frame $top.buttonF2

    label $top.fileL -text "File Name" -anchor w
    entry $top.fileE -relief flat -width 10 -textvar ps_file($id)

    label $top.titleL -text "Title" -anchor w
    entry $top.titleE -relief flat -width 10 -textvar ps_title($id)

    label $top.creatorL -text "Creator" -anchor w
    entry $top.creatorE -relief flat -width 10 -textvar ps_creator($id)

    label $top.fontL -text "Font" -anchor w
    entry $top.fontE -relief flat -width 17 -textvar ps_font($id)
    menubutton $top.fontMB -relief raised -bd 2\
	    -menu $top.fontMB.fontM -indicatoron 1
    menu $top.fontMB.fontM -tearoff 0
    $top.fontMB.fontM add cascade -label "Courier"\
	    -menu $top.fontMB.fontM.courierM
    $top.fontMB.fontM add cascade -label "Helvetica"\
	    -menu $top.fontMB.fontM.helveticaM
    $top.fontMB.fontM add cascade -label "Times"\
	    -menu $top.fontMB.fontM.timesM

    menu $top.fontMB.fontM.courierM -tearoff 0
    $top.fontMB.fontM.courierM add command -label "Normal"\
	    -command "set ps_font($id) Courier"
    $top.fontMB.fontM.courierM add command -label "Oblique"\
	    -command "set ps_font($id) Courier-Oblique"
    $top.fontMB.fontM.courierM add command -label "Bold"\
	    -command "set ps_font($id) Courier-Bold"
    $top.fontMB.fontM.courierM add command -label "BoldOblique"\
	    -command "set ps_font($id) Courier-BoldOblique"

    menu $top.fontMB.fontM.helveticaM -tearoff 0
    $top.fontMB.fontM.helveticaM add command -label "Normal"\
	    -command "set ps_font($id) Helvetica"
    $top.fontMB.fontM.helveticaM add command -label "Oblique"\
	    -command "set ps_font($id) Helvetica-Oblique"
    $top.fontMB.fontM.helveticaM add command -label "Bold"\
	    -command "set ps_font($id) Helvetica-Bold"
    $top.fontMB.fontM.helveticaM add command -label "BoldOblique"\
	    -command "set ps_font($id) Helvetica-BoldOblique"

    menu $top.fontMB.fontM.timesM -tearoff 0
    $top.fontMB.fontM.timesM add command -label "Roman"\
	    -command "set ps_font($id) Times-Roman"
    $top.fontMB.fontM.timesM add command -label "Italic"\
	    -command "set ps_font($id) Times-Italic"
    $top.fontMB.fontM.timesM add command -label "Bold"\
	    -command "set ps_font($id) Times-Bold"
    $top.fontMB.fontM.timesM add command -label "BoldItalic"\
	    -command "set ps_font($id) Times-BoldItalic"

    label $top.sizeL -text "Size" -anchor w
    entry $top.sizeE -relief flat -width 10 -textvar ps_size($id)

    label $top.linewidthL -text "Line Width" -anchor w
    entry $top.linewidthE -relief flat -width 10 -textvar ps_linewidth($id)

    checkbutton $top.zclipCB -relief raised -text "Z Clipping"\
	    -variable ps_zclip($id)

    button $top.createB -relief raised -text "Create"\
	    -command "do_ps $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.fileE -sticky "ew" -in $top.fileF
    grid $top.fileF  $top.fileL -sticky "ew" -in $top.elF -pady 4
    grid $top.titleE -sticky "ew" -in $top.titleF
    grid $top.titleF $top.titleL -sticky "ew" -in $top.elF -pady 4
    grid $top.creatorE -sticky "ew" -in $top.creatorF
    grid $top.creatorF $top.creatorL -sticky "ew" -in $top.elF -pady 4
    grid $top.fontE $top.fontMB -sticky "ew" -in $top.fontF
    grid $top.fontF $top.fontL -sticky "ew" -in $top.elF -pady 4
    grid $top.sizeE -sticky "ew" -in $top.sizeF
    grid $top.sizeF $top.sizeL -sticky "ew" -in $top.elF -pady 4
    grid $top.linewidthE -sticky "ew" -in $top.linewidthF
    grid $top.linewidthF $top.linewidthL -sticky "ew" -in $top.elF -pady 4
    grid columnconfigure $top.fileF 0 -weight 1
    grid columnconfigure $top.titleF 0 -weight 1
    grid columnconfigure $top.creatorF 0 -weight 1
    grid columnconfigure $top.fontF 0 -weight 1
    grid columnconfigure $top.sizeF 0 -weight 1
    grid columnconfigure $top.linewidthF 0 -weight 1
    grid columnconfigure $top.elF 0 -weight 1

    grid $top.zclipCB x -sticky "ew" -in $top.buttonF -ipadx 4 -ipady 4
    grid columnconfigure $top.buttonF 1 -weight 1

    grid $top.createB x $top.dismissB -sticky "ew" -in $top.buttonF2
    grid columnconfigure $top.buttonF2 1 -weight 1 -minsize 40

    pack $top.elF $top.buttonF $top.buttonF2 -expand 1 -fill both -padx 8 -pady 8

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "PostScript Tool..."
}

proc do_ps { id } {
    global player_screen
    global ps_file
    global ps_title
    global ps_creator
    global ps_font
    global ps_size
    global ps_linewidth
    global ps_zclip

    cmd_set $id
    set ps_cmd "_mged_ps"

    if {$ps_file($id) != ""} {
	if {[file exists $ps_file($id)]} {
	    set result [mged_dialog .$id.psDialog $player_screen($id)\
		    "Overwrite $ps_file($id)?"\
		    "Overwrite $ps_file($id)?"\
		    "" 0 OK CANCEL]

	    if {$result} {
		return
	    }
	}
    } else {
	mged_dialog .$id.psDialog $player_screen($id)\
		"No file name specified!"\
		"No file name specified!"\
		"" 0 OK
	return
    }

    if {$ps_title($id) != ""} {
	append ps_cmd " -t \"$ps_title($id)\""
    }

    if {$ps_creator($id) != ""} {
	 append ps_cmd " -c \"$ps_creator($id)\""
    }

    if {$ps_font($id) != ""} {
	 append ps_cmd " -f $ps_font($id)"
    }

    if {$ps_size($id) != ""} {
	 append ps_cmd " -s $ps_size($id)"
    }

    if {$ps_linewidth($id) != ""} {
	 append ps_cmd " -l $ps_linewidth($id)"
    }

    if {$ps_zclip($id) != 0} {
	append ps_cmd " -z"
    }

    append ps_cmd " $ps_file($id)"
    catch {eval $ps_cmd}
}