#			M G E D . T C L
#
# Author -
#	Glenn Durfee
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#  
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#	your "Statement of Terms and Conditions for the Release of
#	The BRL-CAD Package" agreement.
#
# Copyright Notice -
#	This software is Copyright (C) 1995 by the United States Army
#	in all countries except the USA.  All rights reserved.
#
# Description -
#       Sample user interface for MGED


#==============================================================================
# PHASE 0: Support routines: MGED dialog boxes
#------------------------------------------------------------------------------
# "mged_dialog" and "mged_input_dialog" are based off of the "tk_dialog" that
# comes with Tk 4.0.
#==============================================================================

# mged_dialog
# Much like tk_dialog, but doesn't perform a grab.
# Makes a dialog window with the given title, text, bitmap, and buttons.

proc mged_dialog { w title text bitmap default args } {
    global button$w

    toplevel $w
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m
    if { $bitmap != "" } then {
	label $w.top.bitmap -bitmap $bitmap
	pack $w.top.bitmap -side left -padx 2m -pady 2m
    }

    set i 0
    foreach but $args {
	button $w.bot.button$i -text $but -command "set button$w $i"
	if { $i == $default } {
	    frame $w.bot.default -relief sunken -bd 1
	    raise $w.bot.button$i
	    pack $w.bot.default -side left -expand yes -padx 2m -pady 1m
	    pack $w.bot.button$i -in $w.bot.default -side left -padx 1m \
		    -pady 1m -ipadx 1m -ipady 1
	} else {
	    pack $w.bot.button$i -side left -expand yes \
		    -padx 2m -pady 2m -ipadx 1m -ipady 1
	}
	incr i
    }

    if { $default >= 0 } then {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    tkwait variable button$w
    catch { destroy $w }
    return [set button$w]
}

## mged_input_dialog
##   Creates a dialog with the given title, text, and buttons, along with an
##   entry box (with possible default value) whose contents are to be returned
##   in the variable name contained in entryvar.

proc mged_input_dialog { w title text entryvar defaultentry default args } {
    global button$w entry$w
    upvar $entryvar entrylocal

    set entry $defaultentry
    
    toplevel $w
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both
    frame $w.mid -relief raised -bd 1
    pack $w.mid -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m

    entry $w.mid.ent -relief sunken -width 16 -textvariable entry$w
    pack $w.mid.ent -side top -expand yes -fill both -padx 1m -pady 1m

    set i 0
    foreach but $args {
	button $w.bot.button$i -text $but -command "set button$w $i"
	if { $i == $default } {
	    frame $w.bot.default -relief sunken -bd 1
	    raise $w.bot.button$i
	    pack $w.bot.default -side left -expand yes -padx 2m -pady 1m
	    pack $w.bot.button$i -in $w.bot.default -side left -padx 1m \
		    -pady 1m -ipadx 1m -ipady 1
	} else {
	    pack $w.bot.button$i -side left -expand yes \
		    -padx 2m -pady 2m -ipadx 1m -ipady 1
	}
	incr i
    }

    if { $default >= 0 } then {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    tkwait variable button$w
    set entrylocal [set entry$w]
    catch { destroy $w }
    return [set button$w]
}

#==============================================================================
# PHASE 0.5: Loading MGED support routines from other files:
#------------------------------------------------------------------------------
#        vmath.tcl  :  The vector math library
#         menu.tcl  :  The Tk menu replacement
#      sliders.tcl  :  The Tk sliders replacement
# html_library.tcl  :  Stephen Uhler's routines for processing HTML
#==============================================================================

if [info exists env(MGED_LIBRARY)] then {
    set mged_library $env(MGED_LIBRARY)
} else {
    set mged_library ../mged
}

while { [file exists $mged_library/vmath.tcl]==0 } {
    mged_input_dialog .mgeddir "Need path to MGED .tcl files" \
	    "Please enter the full path to the MGED .tcl files:" \
	    mged_library $mged_library 0 OK
}

catch { source $mged_library/vmath.tcl }
catch { source $mged_library/menu.tcl }
catch { source $mged_library/editobj.tcl }
update
catch { source $mged_library/sliders.tcl }
update
catch { source $mged_library/html_library.tcl }

#==============================================================================
# PHASE 1: Creation of main window
#==============================================================================

catch { destroy .ia }
toplevel .ia
wm title .ia "MGED Interaction Window"

#==============================================================================
# PHASE 2: Construction of menu bar
#==============================================================================

frame .ia.m -relief raised -bd 1
pack .ia.m -side top -fill x

menubutton .ia.m.file -text "File" -menu .ia.m.file.m -underline 0
menu .ia.m.file.m
.ia.m.file.m add command -label "Open" -underline 0 -command {
    if { [mged_input_dialog .ia.open "Open New File" \
	    "Enter filename of database you wish to open:" \
	    ia_filename "" 0 Open Cancel] == 0 } then {
	if [file exists $ia_filename] then {
	    opendb $ia_filename
	    mged_dialog .ia.cool "File loaded" \
		    "Database $ia_filename successfully loaded." info 0 OK
	} else {
	    mged_dialog .ia.toobad "Error" \
		    "No such file exists." warning 0 OK
	}
    }
}
    
.ia.m.file.m add command -label "Quit" -command quit -underline 0

menubutton .ia.m.tools -text "Tools" -menu .ia.m.tools.m -underline 0
menu .ia.m.tools.m
.ia.m.tools.m add command -label "Place new solid" -underline 10 -command {
    if { [info exists solc]==0 } then {
	source $mged_library/solcreate.tcl
    }
    solcreate
}
.ia.m.tools.m add command -label "Place new instance" -underline 10 -command {
    if { [info exists ic]==0 } then {
	source $mged_library/icreate.tcl
    }
    icreate
}
.ia.m.tools.m add command -label "Solid Click" -underline 6 -command {
    if { [winfo exists .metasolclick]==0 } then {
	source $mged_library/solclick.tcl
    }
}

menubutton .ia.m.help -text "Help" -menu .ia.m.help.m -underline 0
menu .ia.m.help.m
.ia.m.help.m add command -label "About MGED" -underline 6 -command {
    mged_dialog .ia_about "About MGED..." \
	    "MGED: Multidisplay \[combinatorial solid\] Geometry EDitor\n\
\n\
MGED is a part of the BRL-CAD package developed at the Army\n\
Research Laboratory at Aberdeen Proving Ground, Maryland, U.S.A." \
	    {} 0 OK

    
}
.ia.m.help.m add command -label "On command..." -underline 0 \
	-command [list ia_help [lrange [?] 5 end]]
.ia.m.help.m add command -label "Apropos" -underline 0 -command ia_apropos
.ia.m.help.m add command -label "MGED Manual" -underline 0 -command ia_man

pack .ia.m.file .ia.m.tools -side left
pack .ia.m.help -side right

proc ia_help { cmds } {
    set w .ia.help

    catch { destroy $w }
    toplevel $w
    wm title $w "MGED Help"

    button $w.cancel -command "destroy $w" -text "Cancel"
    pack $w.cancel -side bottom -fill x

    scrollbar $w.s -command "$w.l yview"
    listbox $w.l -bd 2 -yscroll "$w.s set"
    pack $w.s -side right -fill y
    pack $w.l -side top -fill both -expand yes

    foreach cmd $cmds {
	$w.l insert end $cmd
    }

    set doit "catch { destroy $w.u } ; \
	    catch { help \[selection get\] } msg ; \
	    mged_dialog $w.u Usage \$msg info 0 OK"
    bind $w.l <Double-Button-1> $doit
    bind $w.l <2> "tkListboxBeginSelect \%W \[\%W index \@\%x,\%y\] ; $doit"
    bind $w.l <Return> $doit
}

proc ia_apropos { } {
    set w .ia.apropos

    catch { destroy $w }
    if { [mged_input_dialog $w Apropos \
	   "Enter keyword to search for:" keyword "" 0 OK Cancel] == 1 } then {
	return
    }

    ia_help [apropos $keyword]
}

#==============================================================================
# PHASE 3: Bottom-row display
#==============================================================================

frame .ia.dpy
pack .ia.dpy -side bottom -anchor w -fill x

label .ia.dpy.cent -text "Center: " -anchor w
label .ia.dpy.centvar -textvar mged_display(center) -anchor w
label .ia.dpy.size -text "Size: " -anchor w
label .ia.dpy.sizevar -textvar mged_display(size) -anchor w
label .ia.dpy.unitsvar -textvar mged_display(units) -anchor w
label .ia.dpy.azim -text "Azim: " -anchor w
label .ia.dpy.azimvar -textvar mged_display(azimuth) -anchor w
label .ia.dpy.elev -text "Elev: " -anchor w
label .ia.dpy.elevvar -textvar mged_display(elevation) -anchor w
label .ia.dpy.twist -text "Twist: " -anchor w
label .ia.dpy.twistvar -textvar mged_display(twist) -anchor w

pack .ia.dpy.cent .ia.dpy.cent .ia.dpy.centvar .ia.dpy.size .ia.dpy.sizevar \
	.ia.dpy.unitsvar -side left -anchor w

pack .ia.dpy.twistvar .ia.dpy.twist .ia.dpy.elevvar .ia.dpy.elev \
	.ia.dpy.azimvar .ia.dpy.azim -side right -anchor w

frame .ia.illum
pack .ia.illum -side bottom -before .ia.dpy -anchor w -fill x

proc ia_changestate args {
    global mged_display ia_illum_label

    if { [string compare $mged_display(state) VIEWING]==0 } then {
	set ia_illum_label "No objects illuminated"
    } else {
	set ia_illum_label [format "Illuminated path:    %s    %s" \
		$mged_display(path_lhs) $mged_display(path_rhs)]
    }
    
#    if { [string length $mged_display(adc)]>0 } then {
#	set ia_illum_label $mged_display(adc)
#    } elseif { [string length $mged_display(keypoint)]>0 } then {
#	set ia_illum_label $mged_display(keypoint)
#    } elseif { [string compare $mged_display(state) VIEWING]==0 } then {
#	set ia_illum_label "No objects illuminated"
#    } else {
#	set ia_illum_label [format "Illuminated path:    %s    %s" \
#		$mged_display(path_lhs) $mged_display(path_rhs)]
#    }
}

ia_changestate

label .ia.illum.label -textvar ia_illum_label
pack .ia.illum.label -side left -anchor w

trace variable mged_display(state)    w ia_changestate
trace variable mged_display(path_lhs) w ia_changestate
trace variable mged_display(path_rhs) w ia_changestate
trace variable mged_display(keypoint) w ia_changestate
trace variable mged_display(adc)      w ia_changestate

#==============================================================================
# PHASE 4: Text widget for interaction
#==============================================================================

text .ia.t -relief sunken -bd 2 -yscrollcommand ".ia.s set" -setgrid true
scrollbar .ia.s -relief flat -command ".ia.t yview"
pack .ia.s -side right -fill y
pack .ia.t -side top -fill both -expand yes

bind .ia.t <Return> {
    .ia.t mark set insert {end - 1c}
    .ia.t insert insert \n
    ia_invoke
    break
}

bind .ia.t <Delete> {
    catch {.ia.t tag remove sel sel.first promptEnd}
    if {[.ia.t tag nextrange sel 1.0 end] == ""} {
	if [.ia.t compare insert < promptEnd] {
	    break
	}
    }
}

bind .ia.t <BackSpace> {
    catch {.ia.t tag remove sel sel.first promptEnd}
    if {[.ia.t tag nextrange sel 1.0 end] == ""} then {
	if [.ia.t compare insert <= promptEnd] then {
	    break
	}
    }
}

bind .ia.t <Control-a> {
    .ia.t mark set insert promptEnd
    break
}

bind .ia.t <Control-u> {
    .ia.t delete promptEnd {promptEnd lineend}
    .ia.t mark set insert promptEnd
}

bind .ia.t <Control-p> {
    .ia.t delete promptEnd {promptEnd lineend}
    .ia.t mark set insert promptEnd
    set result [catch hist_prev msg]
    if {$result==0} then {
	.ia.t insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .ia.t <Control-n> {
    .ia.t delete promptEnd {promptEnd lineend}
    .ia.t mark set insert promptEnd
    set result [catch hist_next msg]
    if {$result==0} then {
	.ia.t insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .ia.t <Control-d> {
    if [.ia.t compare insert < promptEnd] {
	break
    }
}

bind .ia.t <Control-k> {
    if [.ia.t compare insert < promptEnd] {
	break
    }
}

bind .ia.t <Control-t> {
    if [.ia.t compare insert < promptEnd] {
	break
    }
}

bind .ia.t <Meta-d> {
    if [.ia.t compare insert < promptEnd] {
	break
    }
}

bind .ia.t <Meta-BackSpace> {
    if [.ia.t compare insert <= promptEnd] {
	break
    }
}

bind .ia.t <Control-h> {
    if [.ia.t compare insert <= promptEnd] {
	break
    }
}

auto_load tkTextInsert
proc tkTextInsert {w s} {
    if {$s == ""} {
	return
    }
    catch {
	if {[$w compare sel.first <= insert] && \
		[$w compare sel.last >= insert]} then {
	    $w tag remove sel sel.first promptEnd
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    $w see insert
}

proc ia_rtlog { str } {
    set logStart [.ia.t index insert]
    .ia.t insert insert $str
    .ia.t mark set promptEnd {insert}
    .ia.t mark gravity promptEnd left
    .ia.t tag add bold $logStart promptEnd
    .ia.t yview -pickplace insert
}

proc ia_invoke {} {
    global ia_cmd_prefix
    
    set cmd [concat $ia_cmd_prefix [.ia.t get promptEnd insert]]
    if [info complete $cmd] {
	set result [catch [list uplevel #0 $cmd] ia_msg]
	if {$result != 0} then {
	    if { [regexp "more arguments needed::" $ia_msg] } then {
		set ia_prompt [string range $ia_msg 23 end]
		ia_rtlog $ia_prompt
		set ia_cmd_prefix $cmd
		return
	    }
	    ia_rtlog "Error: $ia_msg\n"
	} else {
	    if {$ia_msg != ""} {
		ia_rtlog $ia_msg\n
	    }
	}
	hist_add $cmd
	set ia_cmd_prefix ""
	ia_rtlog "mged> "
    }
    .ia.t yview -pickplace insert
}

set ia_cmd_prefix ""
ia_rtlog "mged> "
set output_as_return 1
set faceplate 0
# output_hook ia_rtlog

proc echo args {
    return $args
}

.ia.t tag configure bold -font -*-Courier-Bold-R-Normal-*-120-*-*-*-*-*-*
set ia_font -*-Courier-Medium-R-Normal-*-120-*-*-*-*-*-*
.ia.t configure -font $ia_font

#==============================================================================
# PHASE 5: HTML support
#==============================================================================

if [info exists env(MGED_HTML_DIR)] then {
    set mged_html_dir $env(MGED_HTML_DIR)
} else {
    set mged_html_dir ../html/mged
}

proc ia_man { } {
    global mged_library mged_html_dir ia_url message
    
    set w .ia.man
    catch { destroy $w }
    toplevel $w
    wm title $w "MGED HTML browser"

    frame $w.f -relief sunken -bd 1
    pack $w.f -side top -fill x

    button $w.f.close -text Close -command "destroy $w"
    button $w.f.goto -text "Go To" -command {
	mged_input_dialog .ia.man.goto "Go To" "Enter filename to read:" \
		filename $ia_url(current) 0 OK
	if { [file exists filename]!=0 } then {
	    if { [string match /* $filename] } then {
		set new_url $filename
	    } else {
		set new_url [pwd]/$filename
	    }

    	    HMlink_callback .ia.man.text $new_url
	} else {
	    mged_dialog .ia.man.gotoerror "Error reading file" \
		    "Cannot read file $filename." error 0 OK
	}
    }
    button $w.f.back -text "Back" -command "ia_man_back $w.text"

    pack $w.f.close $w.f.goto $w.f.back -side left -fill x -expand yes

    label $w.message -textvar message -anchor w
    pack $w.message -side top -anchor w -fill x

    scrollbar $w.scrolly -command "$w.text yview"
    scrollbar $w.scrollx -command "$w.text xview" -orient horizontal
    text $w.text -relief ridge -yscroll "$w.scrolly set" \
	    -xscroll "$w.scrollx set"
    pack $w.scrolly -side right -fill y
    pack $w.text -side top -fill both -expand yes
    pack $w.scrollx -side bottom -fill x

    if { [info exists ia_url]==0 } then {
	while { [file exists $mged_html_dir/index.html]==0 } {
	    mged_input_dialog .mgedhtmldir "Need path to MGED .html files" \
		    "Please enter the full path to the MGED .html files:" \
		    mged_html_dir $mged_html_dir 0 OK
	}
    }

    set w $w.text

    HMinit_win $w
    set ia_url(current)   $mged_html_dir/
    set ia_url(last)      ""
    set ia_url(backtrack) ""
    HMlink_callback $w index.html
}

proc HMlink_callback { w href } {
    global ia_url message

    if {[string match /* $href]} {
	set new_url $href
    } else {
	set new_url [file dirname $ia_url(current)]/$href
    }

    # Remove tags
    regsub {#[0-9a-zA-Z]*} $new_url {} ia_url(current)
    
    lappend ia_url(last) $ia_url(current)
    set ia_url(backtrack) [lrange $ia_url(last) 0 \
	    [expr [llength $ia_url(last)]-2]]

    HMreset_win $w
    HMparse_html [ia_get_html $ia_url(current)] "HMrender $w"
    update
}

proc ia_man_back { w } {
    global ia_url message

    if {[llength $ia_url(backtrack)]<1} then {
	return
    }
    
    set new_url [lindex $ia_url(backtrack) end]
    set ia_url(backtrack) [lrange $ia_url(backtrack) 0 \
	    [expr [llength $ia_url(backtrack)]-2]]

    HMreset_win $w
    HMparse_html [ia_get_html $new_url] "HMrender $w"
    update
}

proc HMset_image { handle src } {
    global ia_url message

    if {[string match http://* $src]} then {
	return
    }
    
    if {[string match /* $src]} then {
	set image $src
    } else {
	set image [file dirname $ia_url(current)]/$src
    }

    set message "Fetching image $image."
    update
    if {[string first " $image " " [image names] "] >= 0} then {
	HMgot_image $handle $image
    } else {
	catch {image create photo $image -file $image} image
	HMgot_image $handle $image
    }
}

proc ia_get_html {file} {
    global message ia_url

    set message "Reading file $file"
    update

    if {[string match *.gif $file]} then {
	return "<img src=\"[file tail $file]\">"
    }
    
    if {[catch {set fd [open $file]} msg]} {
	return "
	<title>Bad file $file</title>
	<h1>Error reading $file</h1><p>
	$msg<hr>
	"
    }
    set result [read $fd]
    close $fd
    return $result
}
