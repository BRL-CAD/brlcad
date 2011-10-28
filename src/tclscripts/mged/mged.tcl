#                        M G E D . T C L
# BRL-CAD
#
# Copyright (c) 1995-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#       Sample user interface for MGED
#

if ![info exists mged_players] {
    set mged_players {}
}

if ![info exists env(DISPLAY)] {
    set env(DISPLAY) ":0"
}

#==============================================================================
# Ensure that tk.tcl has been loaded already via mged command 'loadtk'.
#==============================================================================
if { [info exists tk_strictMotif] == 0 } {
    loadtk
}

#==============================================================================
# Loading MGED support routines from other files:
#------------------------------------------------------------------------------
#        vmath.tcl  :  The vector math library
#         menu.tcl  :  The Tk menu replacement
# html_library.tcl  :  Stephen Uhler's routines for processing HTML
#==============================================================================

# MGED html manual directory search order precedence should be:
#   MGED_HTML_DIR
#   bu_brlcad_data/html/manuals/mged

if ![info exists mged_default(html_dir)] {
    set mged_default(html_dir) [file normalize [file join [bu_brlcad_data "html"] manuals mged]]
    if {![file exists $mged_default(html_dir)]} {
	set mged_default(html_dir) [file normalize [file join [bu_brlcad_data "doc"] html manuals mged]]
    }
}


if [info exists env(MGED_HTML_DIR)] {
    set mged_html_dir $env(MGED_HTML_DIR)
} else {
    set mged_html_dir $mged_default(html_dir)
}

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

    #    bind $w.l <Button-1> "mged_help %W $screen"
    bind $w.l <Button-1> "handle_select %W %y; mged_help %W $screen; break"
    bind $w.l <Button-2> "handle_select %W %y; mged_help %W $screen; break"
    bind $w.l <Return> "mged_help %W $screen; break"

    place_near_mouse $w
}


proc handle_select { w y } {
    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
	$w selection clear $curr_sel
    }

    $w selection set [$w nearest $y]
}

proc mged_help { w1 screen } {
    global ::tk::Priv

    catch { help [$w1 get [$w1 curselection]]} msg
    cad_dialog $::tk::Priv(cad_dialog) $screen Usage $msg info 0 OK
}



proc ia_apropos { parent screen } {
    set w $parent.apropos

    catch { destroy $w }
    if { [cad_input_dialog $w $screen Apropos \
	      "Enter keyword to search for:" keyword ""\
	      0 {{ summary "This is where the keyword, on which to search,
is entered. The keyword is used to search
for commands whose descriptions contains the
given keyword."} { see_also apropos }} OK Cancel] == 1 } {
	return
    }

    ia_help $parent $screen [apropos $keyword]
}

proc ia_changestate { args } {
    global mged_display
    global mged_gui
    global transform
    global transform_what

    set id [lindex $args 0]

    if {$mged_display($mged_gui($id,active_dm),adc) != ""} {
	set mged_gui($id,illum_label) $mged_display($mged_gui($id,active_dm),adc)
    } elseif {[string length $mged_display(keypoint)]>0} {
	set mged_gui($id,illum_label) $mged_display(keypoint)
    } elseif {$mged_display(state) == "VIEWING"} {
	set mged_gui($id,illum_label) $mged_display($mged_gui($id,active_dm),fps)
    } else {
	if {$mged_display(state) == "OBJ PATH"} {
	    set mged_gui($id,illum_label) [format "Illuminated path: %s/__MATRIX__%s" \
					       $mged_display(path_lhs) $mged_display(path_rhs)]
	} else {
	    set mged_gui($id,illum_label) [format "Illuminated path: %s    %s" \
					       $mged_display(path_lhs) $mged_display(path_rhs)]
	}
    }
}

proc get_player_id_t { w } {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	set _w .$id.t
	if { $w == $_w } {
	    return $id
	}
    }

    return ":"
}

proc do_New { id } {
    global mged_gui
    global ::tk::Priv

    set ftypes {{{MGED Database} {.g}} {{All Files} *}}
    set filename [tk_getSaveFile -parent .$id \
                                 -filetypes $ftypes \
                                 -initialdir $mged_gui(databaseDir) \
                                 -title "Create a New Database"]

    if {$filename != ""} {
        # save the current directory for subsequent file saves
        set mged_gui(databaseDir) [file dirname $filename]

        file delete $filename
        if [catch {opendb $filename y} msg] {
            cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
            "Error" $msg info 0 OK
        } else {
            cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) \
            "File created" $msg info 0 OK
        }
    }
}

proc do_Open { id } {
    global mged_gui
    global ::tk::Priv

    set ftypes {{{MGED Database} {.g}} {{All Files} {*}}}
    set filename [tk_getOpenFile -parent .$id -filetypes $ftypes \
		      -initialdir $mged_gui(databaseDir) -title "Open MGED Database"]

    if {$filename != ""} {
	# save the directory
	set mged_gui(databaseDir) [file dirname $filename]

	set ret [catch {opendb $filename} msg]
	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" \
		$msg info 0 OK
	} else {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "File loaded" \
		$msg info 0 OK
	}
    }
}

## - getFile
#
# Call Tk's filebrowser to get a file. Initialize the browser to
# look in $dir at files of type $ftype. The dir parameter is
# updated before returning. This routine returns the absolute
# pathname of the file. Note - the filename length is currently
# limited to a max of 127 characters.
#
proc getFile { parent dir ftypes title } {
    global mged_gui
    global ::tk::Priv

    upvar #0 $dir path

    if {![info exists path] || $path == ""} {
	set path [pwd]
    }

    set filename [tk_getOpenFile -parent $parent -filetypes $ftypes \
		      -initialdir $path -title $title]

    # warn about filenames whose lengths are larger than 127 characters
    if {[string length $filename] > 127} {
	if {$parent == "."} {
	    set parent ""
	}
	cad_dialog $::tk::Priv(cad_dialog) $parent "Error" \
	    "Length of path is greater than 127 bytes." info 0 OK
	return ""
    }

    if {$filename != ""} {
	# save the path
	set path [file dirname $filename]
    }

    return $filename
}

proc do_Concat { id } {
    global mged_gui
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
	    "No database has been opened!" info 0 OK
	return
    }

    set ftypes {{{MGED Database} {.g}} {{All Files} {*}}}
    set filename [tk_getOpenFile -parent .$id -filetypes $ftypes \
		      -initialdir $mged_gui(databaseDir) \
		      -title "Insert MGED Database"]

    if {$filename != ""} {
	# save the directory
	set mged_gui(databaseDir) [file dirname $filename]

	set ret [cad_input_dialog .$id.prefix $mged_gui($id,screen) "Prefix" \
		     "Enter prefix (optional):" prefix ""\
		     0 {{ summary "
This is where to enter a prefix. The prefix,
if entered, is prepended to each object of
the database being inserted."} { see_also dbconcat } } OK Cancel]

	if {$prefix == ""} {
	    set prefix /
	}

	if {$ret == 0} {
	    dbconcat $filename $prefix
	}
    }
}

proc do_LoadScript { id } {
    global mged_gui
    global ::tk::Priv

    set ftypes {{{All Readable Scripts} {.tcl .sh .rt}} {{Raytrace Scripts} {.rt .sh}} {{Tcl Scripts} {.tcl}} {{All Files} {*}}}
    set ia_filename [tk_getOpenFile -parent .$id -filetypes $ftypes -initialdir $mged_gui(loadScriptDir) -title "Load Script"]

    if {$ia_filename != ""} {
	# save the directory
	set mged_gui(loadScriptDir) [file dirname $ia_filename]

	# read in the first two lines of the script file
	# XXX this is not the best thing to do if we have a big script file..
	set ret [ catch { open $ia_filename } scriptfd ]
	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" $scriptfd info 0 OK
	}
	set ret [ catch { read $scriptfd } scriptsource ]
	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" $scriptsource info 0 OK
	}
	set ret [ catch { close $scriptfd } msg ]
	if {$ret} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" $msg info 0 OK
	}

	# the script should now be stored in scriptsource. attempt to determine some basic types
	if [ regexp {^#!/bin/sh.*?\nrt} $scriptsource ] {
	    # handle "saveview" raytrace script

	    # attempt to loadview
	    set ret [catch { loadview $ia_filename } output ]

	} elseif [ regexp {^#!/.*\n} $scriptsource ] {
	    # handle other shell scripts by simply loading them

	    set ret [ catch { exec $ia_filename } output ]

	} else {
	    # assume we have a tcl script

	    set ret [catch {source $ia_filename} output ]

	}
		   # end iteration over script detection

		   # output a result dialog
		   if {$ret} {
		       cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" $output info 0 OK
		   } else {
		       cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Script loaded" "Script successfully loaded!" info 0 OK
		   }
	       }
	     # end check if filename was given
	 }

	proc do_Units { id } {
	    global mged_gui
	    global mged_display
	    global ::tk::Priv

	    if {[opendb] == ""} {
		set mged_display(units) ""
		cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		    "No database has been opened!" info 0 OK
		return
	    }

	    _mged_units $mged_display(units)
	}

	proc do_rt_script { id } {
	    global mged_gui
	    global ::tk::Priv

	    set ia_filename [fs_dialog .$id.rtscript .$id "./*"]
	    if {[string length $ia_filename] > 0} {
		saveview $ia_filename
	    } else {
		cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "Error" \
		    "No such file exists." warning 0 OK
	    }
	}

	proc do_About_MGED { id } {
	    global mged_gui
	    global mged_default
	    global version
	    global ::tk::Priv

	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "About MGED..." \
		"$version
MGED (Multi-device Geometry EDitor) is part
of the BRL-CAD Open Source package.

Note - html documentation can be found in
$mged_default(html_dir)" \
	    {} 0 OK
}

proc on_context_help { id } {
}

proc ia_invoke { w } {
    global mged_gui
    global glob_compat_mode

    set id [get_player_id_t $w]

    if {([string length [$w get promptEnd insert]] == 1) &&\
	    ([string length $mged_gui($id,more_default)] > 0)} {
	#If no input and a default is supplied then use it
	set hcmd [concat $mged_gui($id,cmd_prefix) $mged_gui($id,more_default)]
    } else {
	set hcmd [concat $mged_gui($id,cmd_prefix) [$w get promptEnd insert]]
    }

    if {$mged_gui($id,apply_to) == 1} {
	set cmd "mged_apply_local $id \"$hcmd\""
    } elseif {$mged_gui($id,apply_to) == 2} {
	set cmd "mged_apply_using_list $id \"$hcmd\""
    } elseif {$mged_gui($id,apply_to) == 3} {
	set cmd "mged_apply_all $mged_gui($id,active_dm) \"$hcmd\""
    } else {
	set cmd $hcmd
    }

    set mged_gui($id,more_default) ""

    if [info complete $cmd] {
	if {!$mged_gui($w,insert_char_flag)} {
	    cmd_win set $id
	}

	# NOTE: Ideally, control of globbing behavior should
	# be handled on a command-by-command basis, not an
	# "all or nothing" approach.  Perhaps if there are
	# situations where a complete top level glob expansion
	# is desired, a non-default option could be added to
	# do it here - as it is however, commands like search
	# that want to do their own handling of such strings
	# are out of luck unless the input strings are heavily
	# (and manually) quoted to pass through db_glob and come
	# out as valid strings for the next round.
	if {$glob_compat_mode == 0} {
	    set result [catch { uplevel \#0 $cmd } ia_msg]
	} else {
	    catch { db_glob $cmd } globbed_cmd
	    set result [catch { uplevel \#0 $globbed_cmd } ia_msg]
	}

	if { ![winfo exists $w] } {
	    distribute_text $w $hcmd $ia_msg
	    stuff_str "\nmged:$id> $hcmd\n$ia_msg"
	    hist add $hcmd
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
		set mged_gui($id,cmd_prefix) $hcmd
		$w see insert
		set mged_gui($id,more_default) [get_more_default]
		return
	    }
	    .$id.t tag add oldcmd promptEnd insert
	    mged_print_tag $w "Error: $ia_msg\n" result
	} else {
	    .$id.t tag add oldcmd promptEnd insert

	    if {$ia_msg != ""} {
		mged_print_tag $w $ia_msg\n result
	    }

	    distribute_text $w $hcmd $ia_msg
	    stuff_str "\nmged:$id> $hcmd\n$ia_msg"
	}

	hist add $hcmd
	set mged_gui($id,cmd_prefix) ""
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
    global ::tk::Priv

    cad_input_dialog $::tk::Priv(cad_dialog) $screen "Go To" "Enter filename to read:" \
	filename $ia_url(current) \
	0 {{ summary "Enter a filename or URL."}} OK

    if { [file exists $filename]!=0 } {
	if { [string match /* $filename] } {
	    set new_url $filename
	} else {
	    set new_url [pwd]/$filename
	}

	HMlink_callback $w.text $new_url
    } else {
	cad_dialog $::tk::Priv(cad_dialog) $screen "Error reading file" \
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

    set w $w.text

    HMinit_win $w
    set ia_url(current)   $mged_html_dir/
    set ia_url(last)      ""
    set ia_url(backtrack) ""
    HMlink_callback $w contents.html
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
    regsub {"#[0-9a-zA-Z]*"} $new_url {} ia_url(current)
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
