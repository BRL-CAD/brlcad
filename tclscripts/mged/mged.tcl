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
#
#  $Revision$
#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of this
#             user interface.

if {[info exists env(DISPLAY)] == 0} {
    puts "The DISPLAY environment variable was not set."
    puts "Setting the DISPLAY environment variable to :0\n"
    set env(DISPLAY) ":0"
}


#==============================================================================
# Ensure that tk.tcl has been loaded already via mged command 'loadtk'.
#==============================================================================
if { [info exists tk_strictMotif] == 0 } {
    loadtk
}

#==============================================================================
# Support routines: MGED dialog boxes
#------------------------------------------------------------------------------
# "mged_dialog" and "mged_input_dialog" are based off of the "tk_dialog" that
# comes with Tk 4.0.
#==============================================================================

# mged_dialog
# Much like tk_dialog, but doesn't perform a grab.
# Makes a dialog window with the given title, text, bitmap, and buttons.

proc mged_dialog { w screen title text bitmap default args } {
    global button$w

    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    message $w.top.msg -text $text -width 12i
    pack $w.top.msg -side right -expand yes -fill both -padx 2m -pady 2m
    if { $bitmap != "" } {
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

    if { $default >= 0 } {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    set pxy [winfo pointerxy $w]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    set width [winfo reqwidth $w]
    set dx [expr $width / 2]
    set x [expr $x - $dx]
    set y [expr $y - 70]
    if {$x < 0} {
	set x 0
    }
    if {$y < 0} {
	set y 0
    }
    wm geometry $w +$x+$y

    tkwait variable button$w
    catch { destroy $w }
    return [set button$w]
}

## mged_input_dialog
##   Creates a dialog with the given title, text, and buttons, along with an
##   entry box (with possible default value) whose contents are to be returned
##   in the variable name contained in entryvar.

proc mged_input_dialog { w screen title text entryvar defaultentry default args } {
    global button$w entry$w
    upvar $entryvar entrylocal

    set entry$w $defaultentry
    
    toplevel $w -screen $screen
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

    if { $default >= 0 } {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    set pxy [winfo pointerxy $w]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    set width [winfo reqwidth $w]
    set dx [expr $width / 2]
    set x [expr $x - $dx]
    set y [expr $y - 70]
    if {$x < 0} {
	set x 0
    }
    if {$y < 0} {
	set y 0
    }
    wm geometry $w +$x+$y

    tkwait variable button$w
    set entrylocal [set entry$w]
    catch { destroy $w }
    return [set button$w]
}

#==============================================================================
# Loading MGED support routines from other files:
#------------------------------------------------------------------------------
#        vmath.tcl  :  The vector math library
#         menu.tcl  :  The Tk menu replacement
#      sliders.tcl  :  The Tk sliders replacement
# html_library.tcl  :  Stephen Uhler's routines for processing HTML
#==============================================================================
if [info exists env(MGED_HTML_DIR)] {
    set mged_html_dir $env(MGED_HTML_DIR)
} else {
    set mged_html_dir [lindex $auto_path 0]/../../html/mged
}

#catch { source [lindex $auto_path 0]/sliders.tcl }

proc ia_help { parent screen cmds } {
    set w $parent.help

    catch { destroy $w }
    toplevel $w -screen $screen
    wm title $w "MGED Help"

    button $w.cancel -command "destroy $w" -text "Cancel"
    pack $w.cancel -side bottom -fill x

    scrollbar $w.s -command "$w.l yview"
    listbox $w.l -bd 2 -yscroll "$w.s set" -exportselection false
    pack $w.s -side right -fill y
    pack $w.l -side top -fill both -expand yes

    foreach cmd $cmds {
	$w.l insert end $cmd
    }

#    bind $w.l <Button-1> "mged_help %W $w.u $screen"
    bind $w.l <Button-1> "handle_select %W %y; mged_help %W $w.u $screen"
    bind $w.l <Button-2> "handle_select %W %y; mged_help %W $w.u $screen"
    bind $w.l <Return> "mged_help %W $w.u $screen"
}

proc handle_select { w y } {
    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
	$w selection clear $curr_sel
    }

    $w selection set [$w nearest $y]
}

proc mged_help { w1 w2 screen } {
    catch { help [$w1 get [$w1 curselection]]} msg
    catch { destroy $w2 }
    mged_dialog $w2 $screen Usage $msg info 0 OK
}

proc ia_apropos { parent screen } {
    set w $parent.apropos

    catch { destroy $w }
    if { [mged_input_dialog $w $screen Apropos \
	   "Enter keyword to search for:" keyword "" 0 OK Cancel] == 1 } {
	return
    }

    ia_help $parent $screen [apropos $keyword]
}

proc ia_changestate { args } {
    global mged_display ia_illum_label
    global mged_active_dm
    global transform
    global transform_what

    set id [lindex $args 0]

    if {$mged_display($mged_active_dm($id),adc) != ""} {
	set ia_illum_label($id) $mged_display($mged_active_dm($id),adc)
    } elseif {[string length $mged_display(keypoint)]>0} {
	set ia_illum_label($id) $mged_display(keypoint)
    } elseif {$mged_display(state) == "VIEWING"} {
	set ia_illum_label($id) $mged_display($mged_active_dm($id),fps)
    } else {
	if {$mged_display(state) == "OBJ PATH"} {
	    set ia_illum_label($id) [format "Illuminated path: %s/__MATRIX__%s" \
		    $mged_display(path_lhs) $mged_display(path_rhs)]
	} else {
	    set ia_illum_label($id) [format "Illuminated path: %s    %s" \
		    $mged_display(path_lhs) $mged_display(path_rhs)]
	}
    }
}

proc tkTextInsert {w s} {
    if {$s == ""} {
	return
    }
    catch {
	if {[$w compare sel.first <= insert] && \
		[$w compare sel.last >= insert]} {
	    $w tag remove sel sel.first promptEnd
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    $w see insert
}

if ![info exists mged_players] {
    set mged_players ""
}

proc get_player_id_t { w } {
    global mged_players
    
    foreach id $mged_players {
	set _w .$id.t
	if { $w == $_w } {
	    return $id
	}
    }

    return ":"
}

proc do_Open { id } {
    global player_screen

    set file_types {{{MGED Database} {.g}}}
    set ia_filename [tk_getOpenFile -parent .$id -filetypes $file_types\
	    -initialdir . -title "Open MGED Database" -defaultextension ".g"]
    if {$ia_filename != ""} {
	opendb $ia_filename
	mged_dialog .$id.cool $player_screen($id) "File loaded" \
		"Database $ia_filename successfully loaded." info 0 OK
    }
}

proc do_New { id } {
    global player_screen

    set ret [mged_input_dialog .$id.new $player_screen($id) "New MGED Database" \
	    "Enter new database filename:" ia_filename "" 0 OK CANCEL]

    if {$ia_filename != "" && $ret == 0} {
	if [file exists $ia_filename] {
	    mged_dialog .$id.uncool $player_screen($id) "Existing Database" \
		    "$ia_filename already exists" info 0 OK
	} else {
	    opendb $ia_filename y
	    mged_dialog .$id.cool $player_screen($id) "File loaded" \
		    "Database $ia_filename successfully loaded." info 0 OK
	}
    }
}

proc do_Concat { id } {
    global player_screen

    set file_types {{{MGED Database} {.g}}}
    set ia_filename [tk_getOpenFile -parent .$id -filetypes $file_types\
	    -initialdir . -title "Insert MGED Database" -defaultextension ".g"]
    if {$ia_filename != ""} {
	mged_input_dialog .$id.prefix $player_screen($id) "Prefix" \
		"Enter prefix:" prefix "" 0 OK

	if {$prefix != ""} {
	    dbconcat $ia_filename $prefix
	} else {
	    dbconcat $ia_filename /
	}
    }
}

proc do_Units { id } {
    global mged_display

    _mged_units $mged_display(units)
}

proc do_rt_script { id } {
    global player_screen

    set ia_filename [fs_dialog .$id.rtscript .$id "./*"]
    if {[string length $ia_filename] > 0} {
	saveview $ia_filename
    } else {
	mged_dialog .$id.uncool $player_screen($id) "Error" \
		"No such file exists." warning 0 OK
    }
}

proc do_About_MGED { id } {
    global player_screen

    mged_dialog .$id.about $player_screen($id) "About MGED..." \
	    "MGED: Multi-device Geometry EDitor\n\
\n\
MGED is a part of the BRL-CAD(TM) package.\n\n\
Developed by The U. S. Army Research Laboratory\n\
Aberdeen Proving Ground, Maryland  21005-5068  USA\n\
" \
	    {} 0 OK
}

proc command_help { id } {
    global player_screen

    ia_help .$id $player_screen($id) [concat [?]]
}

proc on_context_help { id } {
}

proc ia_invoke { w } {
    global ia_cmd_prefix
    global ia_more_default
    global dm_insert_char_flag
    global mged_apply_to

    set id [get_player_id_t $w]

    if {([string length [$w get promptEnd insert]] == 1) &&\
	    ([string length $ia_more_default($id)] > 0)} {
	#If no input and a default is supplied then use it
	set hcmd [concat $ia_cmd_prefix($id) $ia_more_default($id)]
    } else {
	set hcmd [concat $ia_cmd_prefix($id) [$w get promptEnd insert]]
    }

    if {$mged_apply_to($id) == 1} {
	set cmd "mged_apply_local $id \"$hcmd\""
    } elseif {$mged_apply_to($id) == 2} {
	set cmd "mged_apply_using_list $id \"$hcmd\""
    } elseif {$mged_apply_to($id) == 3} {
	set cmd "mged_apply_all \"$hcmd\""
    } else {
	set cmd $hcmd
    }

    set ia_more_default($id) ""

    if [info complete $cmd] {
	if {!$dm_insert_char_flag} {
	    cmd_set $id
	}
	catch [list db_glob $cmd] globbed_cmd
	set result [catch [list uplevel #0 $globbed_cmd] ia_msg]

	if { ![winfo exists $w] } {
	    distribute_text $w $hcmd $ia_msg
	    stuff_str "\nmged:$id> $hcmd\n$ia_msg"
	    hist_add $hcmd
	    return 
	}

	if {$result != 0} {
            set i [string first "more arguments needed::" $ia_msg]
            if { $i > -1 } {
		if { $i != 0 } {
		    mged_print $w [string range $ia_msg 0 [expr $i - 1]]
		}
		
		set ia_prompt [string range $ia_msg [expr $i + 23] end]
		mged_print_prompt $w $ia_prompt
		set ia_cmd_prefix($id) $hcmd
		$w see insert
		set ia_more_default($id) [get_more_default]
		return
	    }
	    mged_print_tag $w "Error: $ia_msg\n" result
	} else {
	    if {$ia_msg != ""} {
		.$id.t tag add oldcmd promptEnd insert
		mged_print_tag $w $ia_msg\n result
	    }

	    distribute_text $w $hcmd $ia_msg
	    stuff_str "\nmged:$id> $hcmd\n$ia_msg"
	}

	hist_add $hcmd
	set ia_cmd_prefix($id) ""
	mged_print_prompt $w "mged> "
    }
    $w see insert
}

proc echo args {
    return $args
}

#==============================================================================
# HTML support
#==============================================================================
proc man_goto { w screen } {
    global ia_url

    mged_input_dialog $w.goto $screen "Go To" "Enter filename to read:" \
	    filename $ia_url(current) 0 OK
    if { [file exists $filename]!=0 } {
	if { [string match /* $filename] } {
	    set new_url $filename
	} else {
	    set new_url [pwd]/$filename
	}

	HMlink_callback $w.text $new_url
    } else {
	mged_dialog $w.gotoerror $screen "Error reading file" \
		"Cannot read file $filename." error 0 OK
    }
}

proc ia_man { parent screen } {
    global mged_html_dir ia_url message
    
    set w $parent.man
    catch { destroy $w }
    toplevel $w -screen $screen
    wm title $w "MGED HTML browser"

    frame $w.f -relief sunken -bd 1
    pack $w.f -side top -fill x

    button $w.f.close -text Close -command "destroy $w"
    button $w.f.goto -text "Go To" -command "man_goto $w $screen"
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

#    if { [info exists ia_url]==0 } {
#	while { [file exists $mged_html_dir/index.html]==0 } {
#	    mged_input_dialog .mgedhtmldir $screen "Need path to MGED .html files"
#		    "Please enter the full path to the MGED .html files:" \
#		    mged_html_dir $mged_html_dir 0 OK
#	}
#    }

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
	if { [file isdirectory $ia_url(current)] } {
	    set new_url $ia_url(current)$href
	} else {
	    set new_url [file dirname $ia_url(current)]/$href
	}
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

    if {[llength $ia_url(backtrack)]<1} {
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

    if {[string match http://* $src]} {
	return
    }
    
    if {[string match /* $src]} {
	set image $src
    } else {
	set image [file dirname $ia_url(current)]/$src
    }

    set message "Fetching image $image."
    update
    if {[string first " $image " " [image names] "] >= 0} {
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

    if {[string match *.gif $file]} {
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

#==============================================================================
# Other Support Routines
#==============================================================================
