if {[info exists env(DISPLAY)] == 0} {
    puts "The DISPLAY environment variable was not set."
    puts "Setting the DISPLAY environment variable to :0\n"
    set env(DISPLAY) ":0"
}

if { [info exists tk_strictMotif] == 0 } {
    loadtk
}

if [info exists env(MGED_HTML_DIR)] {
        set mged_html_dir $env(MGED_HTML_DIR)
} else {
        set mged_html_dir [lindex $auto_path 0]/../html/mged
}

if ![info exists mged_players] {
    set mged_players ""
}

if ![info exists mged_collaborators] {
    set mged_collaborators ""
}

set extern_commands "attach aim"
foreach cmd $extern_commands {
    if {[string compare [info command $cmd] $cmd] != 0} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}

proc openw args {
    global ia_cmd_prefix
    global ia_more_default
    global ia_font
    global ia_illum_label
    global mged_html_dir
    global mged_players
    global mged_collaborators
    global mged_display
    global player_screen
    global openw_usage
    global env
    global faceplate
    global edit_info
    global multi_view
    global sliders_on
    global buttons_on
    global show_edit_info
    global mged_top
    global mged_dmc
    global win_size

set save_id [lindex [cmd_get] 1]
set comb 0
set join_c 0
set i 0

#==============================================================================
# PHASE 0: Process options
#==============================================================================
if { [llength $args] < 3 } {
    return [help openw]
}

foreach arg $args {
    if [string match "-c" $arg] {
	incr i
	set comb 1
    } elseif [string match "-j" $arg] { 
	incr i
	set join_c 1
    } else {
	break
    }
}

set id [lindex $args $i]
set screen [lindex $args [expr $i + 1]]
set dtype [lindex $args [expr $i + 2]]
set dscreen [lindex $args [expr $i + 3]]

if ![string length $dscreen] {
    set dscreen $screen
}

#==============================================================================
# PHASE 1: Creation of main window
#==============================================================================
if [ winfo exists .ia$id ] {
    return "A session with the id \"$id\" already exists!"
}
	
toplevel .ia$id -screen $screen
wm title .ia$id "MGED Interaction Window"

lappend mged_players $id
set player_screen($id) $screen

#==============================================================================
# Create display manager window and menu
#==============================================================================
if {$comb} {
    set mged_dmc($id) .ia$id.dmf
    set mged_top($id) .ia$id

    frame $mged_dmc($id) -relief sunken -borderwidth 2
    set win_size($id) [expr [winfo screenheight $mged_dmc($id)] - 274]
    set mv_size [expr $win_size($id) / 2 - 4]
    openmv $mged_dmc($id) $id $dtype $mv_size

    if [catch {attach -t 0 -n $mged_top($id).$id -S $win_size($id) $dtype} result] {
	releasemv $id
	destroy .ia$id
	return $result
    }
} else {
    set mged_top($id) .top$id
    set mged_dmc($id) $mged_top($id)

    toplevel $mged_dmc($id) -screen $dscreen -relief sunken -borderwidth 2
    set win_size($id) [expr [winfo screenheight $mged_dmc($id)] - 24]
    set mv_size [expr $win_size($id) / 2 - 4]
    openmv $mged_dmc($id) $id $dtype $mv_size

    if [catch {attach -t 0 -n $mged_dmc($id).$id -S $win_size($id) $dtype} result] {
	releasemv $id
	destroy .ia$id
	destroy .top$id
	return $result
    }
}

#==============================================================================
# PHASE 2: Construction of menu bar
#==============================================================================

frame .ia$id.m -relief raised -bd 1

menubutton .ia$id.m.file -text "File" -menu .ia$id.m.file.m -underline 0
menu .ia$id.m.file.m
.ia$id.m.file.m add checkbutton -offvalue 0 -onvalue 1 -variable buttons_on($id)\
	-label "Button Menu" -underline 0 -command "toggle_button_menu $id"
.ia$id.m.file.m add checkbutton -offvalue 0 -onvalue 1 -variable sliders_on($id)\
	-label "Sliders" -underline 0 -command "toggle_sliders $id"
.ia$id.m.file.m add checkbutton -offvalue 0 -onvalue 1 -variable show_edit_info($id)\
	-label "Edit Info" -underline 0 -command "toggle_edit_info $id"
.ia$id.m.file.m add checkbutton -offvalue 0 -onvalue 1 -variable multi_view($id)\
	-label "Multi View" -underline 0 -command "setmv $id"
.ia$id.m.file.m add command -label "Opendb" -underline 0 -command "do_Open $id"
.ia$id.m.file.m add command -label "Close Window" -underline 0 -command "closew $id"
.ia$id.m.file.m add command -label "Quit" -command quit -underline 0

menubutton .ia$id.m.tools -text "Tools" -menu .ia$id.m.tools.m -underline 0
menu .ia$id.m.tools.m
.ia$id.m.tools.m add command -label "Place new solid" -underline 10 -command "solcreate $id"
.ia$id.m.tools.m add command -label "Place new instance" -underline 10 -command "icreate $id"
.ia$id.m.tools.m add command -label "Solid Click" -underline 6 -command "init_solclick $id"

menubutton .ia$id.m.help -text "Help" -menu .ia$id.m.help.m -underline 0
menu .ia$id.m.help.m
.ia$id.m.help.m add command -label "About MGED" -underline 6 -command "do_About_MGED $id"
.ia$id.m.help.m add command -label "On command..." -underline 0 \
	-command "do_On_command $id"
.ia$id.m.help.m add command -label "Apropos" -underline 0 -command "ia_apropos .ia$id $screen"

if [info exists env(WEB_BROWSER)] {
    if [ file exists $env(WEB_BROWSER) ] {
	set web_cmd "exec $env(WEB_BROWSER) -display $screen $mged_html_dir/index.html &"
    }
}

# Minimal attempt to locate a browser
if ![info exists web_cmd] {
    if [ file exists /usr/X11/bin/netscape ] {
	set web_cmd "exec /usr/X11/bin/netscape -display $screen $mged_html_dir/index.html &"
    } elseif [ file exists /usr/X11/bin/Mosaic ] {
	set web_cmd "exec /usr/X11/bin/Mosaic -display $screen $mged_html_dir/index.html &"
    } else {
	set web_cmd "ia_man .ia$id $screen"
    }
}
.ia$id.m.help.m add command -label "MGED Manual" -underline 0 -command $web_cmd

#==============================================================================
# PHASE 3: Bottom-row display
#==============================================================================

frame .ia$id.dpy
frame .ia$id.dpy.f1
frame .ia$id.dpy.f2

if [llength $mged_collaborators] {
    cmd_set [lindex $mged_collaborators 0]
    set cmd_list [cmd_get]
    set dm_id [lindex $cmd_list 1]
} else {
    set dm_id $id
}

label .ia$id.dpy.cent -text "Center: " -anchor w
label .ia$id.dpy.centvar -textvar mged_display($dm_id,center) -anchor w
label .ia$id.dpy.size -text "  Size: " -anchor w
label .ia$id.dpy.sizevar -textvar mged_display($dm_id,size) -anchor w
label .ia$id.dpy.unitsvar -textvar mged_display(units) -anchor w -padx 4
label .ia$id.dpy.aet -textvar mged_display($dm_id,aet) -anchor w
label .ia$id.dpy.ang -textvar mged_display($dm_id,ang) -anchor w -padx 4

frame .ia$id.illum

label .ia$id.illum.label -textvar ia_illum_label($id)

#==============================================================================
# PHASE 4: Text widget for interaction
#==============================================================================

frame .ia$id.tf
if {$comb} {
    text .ia$id.t -width 10 -relief sunken -bd 2 -yscrollcommand ".ia$id.s set" -setgrid true
} else {
    text .ia$id.t -relief sunken -bd 2 -yscrollcommand ".ia$id.s set" -setgrid true
}
scrollbar .ia$id.s -relief flat -command ".ia$id.t yview"

bind .ia$id.t <Enter> "focus .ia$id.t"

bind .ia$id.t <Return> {
    %W mark set insert {end - 1c};
    %W insert insert \n;
    ia_invoke %W;
    break;
}

bind .ia$id.t <Control-u> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
}

bind .ia$id.t <Control-p> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
    set id [get_player_id_t %W]
    cmd_set $id
    set result [catch hist_prev msg]
    if {$result==0} {
	%W insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .ia$id.t <Control-n> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
    set id [get_player_id_t %W]
    cmd_set $id
    set result [catch hist_next msg]
    if {$result==0} {
	%W insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .ia$id.t <Control-c> {
    %W mark set insert {end - 1c};
    %W insert insert \n;
    ia_print_prompt %W "mged> "
    %W see insert
    set id [get_player_id_t %W]
    set ia_cmd_prefix($id) ""
    set ia_more_default($id) ""
}

bind .ia$id.t <Control-w> {
    set ti [%W search -backwards -regexp "\[ \t\]\[^ \t\]+\[ \t\]*" insert promptEnd]
    if [string length $ti] {
	%W delete "$ti + 1c" insert
    } else {
	%W delete promptEnd insert
    }
}

bind .ia$id.t <Control-a> {
    %W mark set insert promptEnd
    break
}

#bind .ia$id.t <Delete> {
#    catch {%W tag remove sel sel.first promptEnd}
#    if {[%W tag nextrange sel 1.0 end] == ""} {
#	if [%W compare insert < promptEnd] {
#	    break
#	}
#    }
#}
#
#bind .ia$id.t <BackSpace> {
#    catch {%W tag remove sel sel.first promptEnd}
#    if {[%W tag nextrange sel 1.0 end] == ""} {
#	if [%W compare insert <= promptEnd] {
#	    break
#	}
#    }
#}
#
#bind .ia$id.t <Control-d> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .ia$id.t <Control-k> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .ia$id.t <Control-t> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .ia$id.t <Meta-d> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .ia$id.t <Meta-BackSpace> {
#    if [%W compare insert <= promptEnd] {
#	break
#    }
#}
#
#bind .ia$id.t <Control-h> {
#    if [%W compare insert <= promptEnd] {
#	break
#    }
#}

set ia_cmd_prefix($id) ""
set ia_more_default($id) ""
ia_print_prompt .ia$id.t "mged> "

.ia$id.t tag configure bold -font -*-Courier-Bold-R-Normal-*-120-*-*-*-*-*-*
set ia_font -*-Courier-Medium-R-Normal-*-120-*-*-*-*-*-*
.ia$id.t configure -font $ia_font

#==============================================================================
# Pack windows
#==============================================================================

pack .ia$id.m.file .ia$id.m.tools -side left
pack .ia$id.m.help -side right
pack .ia$id.m -side top -expand 1 -fill x

#pack .ia$id.$id -in .ia$id.dmf -expand 1 -fill both
#pack .ia$id.dmf -side top -expand 1 -fill both -padx 2 -pady 2
#Prevent window from being resized

pack $mged_top($id).$id -in $mged_dmc($id)
if {$comb} {
    pack $mged_dmc($id) -side top -padx 2 -pady 2
}

pack .ia$id.dpy.cent .ia$id.dpy.centvar .ia$id.dpy.size .ia$id.dpy.sizevar \
	.ia$id.dpy.unitsvar -in .ia$id.dpy.f1 -side left -anchor w
pack .ia$id.dpy.aet .ia$id.dpy.ang -in .ia$id.dpy.f2 -side left -anchor w
pack .ia$id.dpy.f1 .ia$id.dpy.f2 -expand 1 -fill x
pack .ia$id.dpy -side bottom -anchor w -expand 1 -fill x

pack .ia$id.illum.label -side left -anchor w
pack .ia$id.illum -side bottom -before .ia$id.dpy -anchor w -fill x -expand 1

pack .ia$id.s -in .ia$id.tf -side right -fill y
pack .ia$id.t -in .ia$id.tf -side top -fill both -expand yes
pack .ia$id.tf -side top -fill both -expand yes

#==============================================================================
# PHASE 5: Creation of other auxilary windows
#==============================================================================

#mmenu_init $id
#cmd_init $id .ia$id.t
cmd_init $id

#==============================================================================
# PHASE 6: Further setup
#==============================================================================

setupmv $id
aim $id $mged_top($id).$id

if { $join_c } {
    jcs $id
}

trace variable mged_display($id,fps) w "ia_changestate $id"

# reset current_cmd_list so that its cur_hist gets updated
cmd_set $save_id

wm protocol $mged_top($id) WM_DELETE_WINDOW "closew $id"
wm title $mged_top($id) "$id\'s MGED Interaction Window"
}


proc closew args {
    global mged_players
    global mged_collaborators
    global multi_view
    global sliders_on
    global buttons_on
    global show_edit_info
    global mged_top
    global mged_dmc

    if { [llength $args] != 1 } {
	return [help closew]
    }

    set id [lindex $args 0]

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 } {
	qcs $id
    }

    set i [lsearch -exact $mged_players $id]
    if { $i == -1 } {
	return "closecw: bad id - $id"
    }
    set mged_players [lreplace $mged_players $i $i]

    set multi_view($id) 0
    set sliders_on($id) 0
    set buttons_on($id) 0
    set show_edit_info($id) 0

    releasemv $id
    catch { release $mged_top($id).$id }
    catch { cmd_close $id }
    catch { destroy .mmenu$id }
    catch { destroy .sliders$id }
    catch { destroy $mged_top($id) }
    catch { destroy .ia$id }

    reconfig_openw_all
}


proc reconfig_openw { id } {
    global mged_display

    cmd_set $id
    set cmd_list [cmd_get]
    set shared_id [lindex $cmd_list 1]

    .ia$id.dpy.centvar configure -textvar mged_display($shared_id,center)
    .ia$id.dpy.sizevar configure -textvar mged_display($shared_id,size)
    .ia$id.dpy.unitsvar configure -textvar mged_display(units)
    .ia$id.dpy.aet configure -textvar mged_display($shared_id,aet)
    .ia$id.dpy.ang configure -textvar mged_display($shared_id,ang)

    reconfig_mmenu $id
}

proc reconfig_openw_all {} {
    global mged_collaborators

    foreach id $mged_collaborators {
	reconfig_openw $id
    }
}


proc toggle_button_menu { id } {
    global buttons_on

    if [ winfo exists .mmenu$id ] {
	destroy .mmenu$id
	set buttons_on($id) 0
	return
    }

    mmenu_init $id
}


proc toggle_sliders { id } {
    global sliders_on
    global scroll_enabled

    if [ winfo exists .sliders$id ] {
	cmd_set $id
	sliders off
	return
    }

    cmd_set $id
    sliders on
}


proc toggle_edit_info { id } {
    global player_screen
    global show_edit_info

    if [ winfo exists .sei$id] {
	destroy .sei$id
	set show_edit_info($id) 0
	return
    }

    toplevel .sei$id -screen $player_screen($id)
    label .sei$id.l -bg black -fg yellow -textvar edit_info -font fixed
    pack .sei$id.l -expand 1 -fill both

    wm title .sei$id "$id\'s Edit Info"
    wm protocol .sei$id WM_DELETE_WINDOW "toggle_edit_info $id"
    wm resizable .sei$id 0 0
}


# Join Mged Collaborative Session
proc jcs { id } {
    global mged_collaborators
    global mged_players

    if { [lsearch -exact $mged_players $id] == -1 } {
	return "join_csession: $id is not listed as an mged_player"
    }

    if { [lsearch -exact $mged_collaborators $id] != -1 } {
	return "join_csession: $id is already in the collaborative session"
    }

    if [winfo exists .ia$id.$id] {
	set nw .ia$id.$id
    } elseif [winfo exists .top$id.$id] {
	set nw .top$id.$id
    } else {
	return "join_csession: unrecognized pathnames - .ia$id.$id and .top$id.$id"
    }

    if [llength $mged_collaborators] {
	set cid [lindex $mged_collaborators 0]
	if [winfo exists .ia$cid.$cid] {
	    set ow .ia$cid.$cid
	} elseif [winfo exists .top$cid.$cid] {
	    set ow .top$cid.$cid
	} else {
	    return "join_csession: me thinks the session is corrupted"
	}

	catch { tie $nw $ow }
	reconfig_openw $id
    }

    lappend mged_collaborators $id
}


# Quit Mged Collaborative Session
proc qcs { id } {
    global mged_collaborators

    set i [lsearch -exact $mged_collaborators $id]
    if { $i == -1 } {
	return "qcs: bad id - $id"
    }

    if [winfo exists .ia$id.$id] {
	set w .ia$id.$id
    } elseif  [winfo exists .top$id.$id] {
	set w .top$id.$id
    } else {
	return "qcs: unrecognized pathnames - .ia$id.$id and .top$id.$id"
    }

    catch {untie $w}
    set mged_collaborators [lreplace $mged_collaborators $i $i]
}


# Print Collaborative Session participants
proc pcs {} {
    global mged_collaborators

    return $mged_collaborators
}


# Print Mged Players
proc pmp {} {
    global mged_players

    return $mged_players
}


proc aim args {
    set result [eval _mged_aim $args]
    
    if { [llength $args] == 2 } {
	reconfig_openw [lindex $args 0]
    }

    return $result
}
