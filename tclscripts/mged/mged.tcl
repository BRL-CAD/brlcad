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

while { [file exists $mged_html_dir/index.html]==0 } {
    mged_input_dialog .mgeddir $env(DISPLAY) "MGED_HTML_DIR environment variable not set" \
	    "Please enter the full path to the MGED HTML files:" \
	    mged_html_dir $mged_html_dir 0 OK
}

catch { source [lindex $auto_path 0]/sliders.tcl }

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

#    bind $w.l <Button-1> "doit_help %W $w.u $screen"
    bind $w.l <Button-1> "handle_select %W %y; doit_help %W $w.u $screen"
    bind $w.l <Button-2> "handle_select %W %y; doit_help %W $w.u $screen"
    bind $w.l <Return> "doit_help %W $w.u $screen"
}

proc handle_select { w y } {
    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
	$w selection clear $curr_sel
    }

    $w selection set [$w nearest $y]
}

proc doit_help { w1 w2 screen } {
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

    set id [lindex $args 0]

    if { [string length $mged_display(keypoint)]>0 } {
	set ia_illum_label($id) $mged_display(keypoint)
    } elseif { [string compare $mged_display(state) VIEWING]==0 } {
	set ia_illum_label($id) $mged_display($mged_active_dm($id),fps)
    } else {
	set ia_illum_label($id) [format "Illuminated path:    %s    %s" \
		$mged_display(path_lhs) $mged_display(path_rhs)]
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

proc insert_text { id str } {
    ia_rtlog .$id.t $str
}

proc ia_rtlog { w str } {
    $w insert insert $str
#    $w see insert
}

proc ia_rtlog_bold { w str } {
    set boldStart [$w index insert]
    $w insert insert $str
    set boldEnd [$w index insert]
    $w tag add bold $boldStart $boldEnd
#    $w see insert
}

proc ia_print_prompt { w str } {
    ia_rtlog_bold $w $str
    $w mark set promptEnd {insert}
    $w mark gravity promptEnd left
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

proc distribute_text { w cmd str} {
    global mged_players

    set src_id [get_player_id_t $w]
    foreach id $mged_players {
	set _w .$id.t
	if [winfo exists $_w] {
	    if {$w != $_w} {
		set _promptBegin [$_w index {end - 1 l}]
		$_w mark set curr insert
		$_w mark set insert $_promptBegin

		if {$cmd != ""} {
		    ia_rtlog_bold $_w "mged:$src_id> "
		    ia_rtlog_bold $_w $cmd\n
		}

		if {$str != ""} {
		    ia_rtlog_bold $_w $str\n
		}

		$_w mark set insert curr
		$_w see insert
	    }
	}
    }
}

#    if { [mged_input_dialog .$id.open $player_screen($id) "Open New File" \
#	    "Enter filename of database you wish to open:" \
#	    ia_filename "" 0 Open Cancel] == 0 } {
proc do_Open { id } {
    global player_screen

    set ia_filename [fs_dialog .$id.open .$id "./*.g"]
    if {[string length $ia_filename] > 0} {
	if [file exists $ia_filename] {
	    opendb $ia_filename
	    mged_dialog .$id.cool $player_screen($id) "File loaded" \
		    "Database $ia_filename successfully loaded." info 0 OK
	} else {
	    mged_dialog .$id.toobad $player_screen($id) "Error" \
		    "No such file exists." warning 0 OK
	}
    }
}

proc do_About_MGED { id } {
    global player_screen

    mged_dialog .$id.about $player_screen($id) "About MGED..." \
	    "MGED: Multi-device Geometry EDitor\n\
\n\
MGED is a part of The BRL-CAD Package.\n\n\
Developed by The U. S. Army Research Laboratory\n\
Aberdeen Proving Ground, Maryland  21005-5068  USA\n\
" \
	    {} 0 OK
}

proc do_On_command { id } {
    global player_screen

    ia_help .$id $player_screen($id) [concat [?]]
}

proc ia_invoke { w } {
    global ia_cmd_prefix
    global ia_more_default

    set id [get_player_id_t $w]

    if {([string length [$w get promptEnd insert]] == 1) &&\
	    ([string length $ia_more_default($id)] > 0)} {
	#If no input and a default is supplied then use it
	set cmd [concat $ia_cmd_prefix($id) $ia_more_default($id)]
    } else {
	set cmd [concat $ia_cmd_prefix($id) [$w get promptEnd insert]]
    }

    set ia_more_default($id) ""

    if [info complete $cmd] {
	cmd_set $id
	catch [list mged_glob $cmd] globbed_cmd
	set result [catch [list uplevel #0 $globbed_cmd] ia_msg]

	if { ![winfo exists $w] } {
	    distribute_text $w $cmd $ia_msg
	    stuff_str "\nmged:$id> $cmd\n$ia_msg"
	    hist_add $cmd
	    return 
	}

	if {$result != 0} {
            set i [string first "more arguments needed::" $ia_msg]
            if { $i > -1 } {
		if { $i != 0 } {
		    ia_rtlog $w [string range $ia_msg 0 [expr $i - 1]]
		}
		
		set ia_prompt [string range $ia_msg [expr $i + 23] end]
		ia_print_prompt $w $ia_prompt
		set ia_cmd_prefix($id) $cmd
		$w see insert
		set ia_more_default($id) [get_more_default]
		return
	    }
	    ia_rtlog $w "Error: $ia_msg\n"
	} else {
	    if {$ia_msg != ""} {
		ia_rtlog $w $ia_msg\n
	    }

	    distribute_text $w $cmd $ia_msg
	    stuff_str "\nmged:$id> $cmd\n$ia_msg"
	}

	hist_add $cmd
	set ia_cmd_prefix($id) ""
	ia_print_prompt $w "mged> "
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
#
# TCL versions of MGED "help", "?", and "apropos" commands
#
#==============================================================================

set help_data(?)		{{}	{summary of available mged commands}}
set help_data(?lib)		{{}	{summary of available library commands}}
set help_data(%)		{{}	{escape to interactive shell}}
set help_data(3ptarb)		{{}	{makes arb given 3 pts, 2 coord of 4th pt, and thickness}}
set help_data(adc)		{{[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> value(s)]}	{control the angle/distance cursor}}
set help_data(ae)		{{[-i] azim elev [twist]}	{set view using azim, elev and twist angles}}
set help_data(aim)		{{[command_window [pathName of display window]]}	{aims command_window at pathName}}
set help_data(aip)		{{[fb]}		{advance illumination pointer or path position forward or backward}}
set help_data(analyze)		{{[arbname]}	{analyze faces of ARB}}
set help_data(apropos)		{{keyword}	{finds commands whose descriptions contain the given keyword}}
set help_data(arb)		{{name rot fb}	{make arb8, rotation + fallback}}
set help_data(arced)		{{a/b ...anim_command...}	{edit matrix or materials on combination's arc}}
set help_data(area)		{{[endpoint_tolerance]}	{calculate presented area of view}}
set help_data(attach)		{{[-d display_string] [-i init_script] [-n name]
	      [-t is_toplevel] [-W width] [-N height]
	      [-S square_size] dev_type}	{attach to a display manager}}
set help_data(attach4)		{{id screen dtype}	{open a set of 4 display windows}}
set help_data(B)		{{<objects>}	{clear screen, edit objects}}
set help_data(bev)		{{[-t] [-P#] new_obj obj1 op obj2 op obj3 op ...}	{Boolean evaluation of objects via NMG's}}
set help_data(c)		{{[-gr] comb_name [boolean_expr]}	{create or extend a combination using standard notation}}
set help_data(cat)		{{<objects>}	{list attributes (brief)}}
set help_data(center)		{{x y z}	{set view center}}
set help_data(closew)		{{id}	{close display/command window pair}}
set help_data(color)		{{low high r g b str}	{make color entry}}
set help_data(comb)		{{comb_name <operation solid>}	{create or extend combination w/booleans}}
set help_data(comb_color)	{{comb R G B}	{assign a color to a combination (like 'mater')}}
set help_data(copyeval)		{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
set help_data(copymat)		{{a/b c/d}	{copy matrix from one combination's arc to another's}}
set help_data(cp)		{{from to}	{copy [duplicate] object}}
set help_data(cpi)		{{from to}	{copy cylinder and position at end of original cylinder}}
set help_data(d)		{{<objects>}	{remove objects from the screen}}
set help_data(dall)		{{<objects>}	{remove all occurrences of object(s) from the screen}}
set help_data(db)		{{command}	{database manipulation routines}}
set help_data(dbconcat)		{{file [prefix]}	{concatenate 'file' onto end of present database.  Run 'dup file' first.}}
set help_data(debugbu)		{{[hex_code]}	{Show/set debugging bit vector for libbu}}
set help_data(debugdir)		{{}	{Print in-memory directory, for debugging}}
set help_data(debuglib)		{{[hex_code]}	{Show/set debugging bit vector for librt}}
set help_data(debugmem)		{{}	{Print librt memory use map}}
set help_data(debugnmg)		{{[hex code]}	{Show/set debugging bit vector for NMG}}
set help_data(decompose)	{{nmg_solid [prefix]}	{decompose nmg_solid into maximally connected shells}}
set help_data(delay)		{{sec usec}	{Delay for the specified amount of time}}
set help_data(dm)		{{set var [val]}	{Do display-manager specific command}}
set help_data(draw)		{{<objects>}	{draw objects}}
set help_data(dup)		{{file [prefix]}	{check for dup names in 'file'}}
set help_data(E)		{{ [-s] <objects>}	{evaluated edit of objects. Option 's' provides a slower, but better fidelity evaluation}}
set help_data(e)		{{<objects>}	{edit objects}}
set help_data(eac)		{{Air_code(s)}	{display all regions with given air code}}
set help_data(echo)		{{[text]}	{echo arguments back}}
set help_data(edcodes)		{{object(s)}	{edit region ident codes}}
set help_data(edmater)		{{comb(s)}	{edit combination materials}}
set help_data(edcolor)		{{}	{text edit color table}}
set help_data(edcomb)		{{combname Regionflag regionid air los [GIFTmater]}	{edit combination record info}}
set help_data(edgedir)		{{[delta_x delta_y delta_z]|[rot fb]}	{define direction of ARB edge being moved}}
set help_data(erase)		{{<objects>}	{remove objects from the screen}}
set help_data(erase_all)	{{<objects>}	{remove all occurrences of object(s) from the screen}}
set help_data(ev)		{{[-dnqstuvwT] [-P #] <objects>}	{evaluate objects via NMG tessellation}}
set help_data(eqn)		{{A B C}	{planar equation coefficients}}
set help_data(exit)		{{}	{exit}}
set help_data(extrude)		{{#### distance}	{extrude dist from face}}
set help_data(expand)		{{wildcard expression}	{expands wildcard expression}}
set help_data(eye_pt)		{{mx my mz}	{set eye point to given model coordinates (in mm)}}
set help_data(facedef)		{{####}	{define new face for an arb}}
set help_data(facetize)		{{[-tT] [-P#] new_obj old_obj(s)}	{convert objects to faceted NMG objects at current tol}}
set help_data(find)		{{<objects>}	{find all references to objects}}
set help_data(fix)		{{}	{fix display after hardware error}}
set help_data(fracture)		{{NMGsolid [prefix]}	{fracture an NMG solid into many NMG solids, each containing one face\n}}
set help_data(g)		{{groupname <objects>}	{group objects}}
set help_data(getknob)		{{knobname}	{Gets the current setting of the given knob}}
set help_data(output_hook)	{{output_hook_name}	{All output is sent to the Tcl procedure \"output_hook_name\"}}
set help_data(help)		{{[commands]}	{give usage message for given commands}}
set help_data(helplib)		{{[library commands]}	{give usage message for given library commands}}
set help_data(history)		{{[-delays]}	{list command history}}
set help_data(hist_prev)	{{}	{Returns previous command in history}}
set help_data(hist_next)	{{}	{Returns next command in history}}
set help_data(hist_add)		{{[command]}	{Adds command to the history (without executing it)}}
set help_data(i)		{{obj combination [operation]}	{add instance of obj to comb}}
set help_data(idents)		{{file object(s)}	{make ascii summary of region idents}}
set help_data(ill)		{{name}	{illuminate object}}
set help_data(in)		{{[-f] [-s] parameters...}	{keyboard entry of solids.  -f for no drawing, -s to enter solid edit}}
set help_data(inside)		{{}	{finds inside solid per specified thicknesses}}
set help_data(item)		{{region item [air [GIFTmater [los]]]}	{set region ident codes}}
set help_data(jcs)		{{id}	{join collaborative session}}
set help_data(joint)		{{command [options]}	{articulation/animation commands}}
set help_data(journal)		{{[-d] fileName}	{record all commands and timings to journal}}
set help_data(keep)		{{keep_file object(s)}	{save named objects in specified file}}
set help_data(keypoint)		{{[x y z | reset]}	{set/see center of editing transformations}}
set help_data(kill)		{{[-f] <objects>}	{delete object[s] from file}}
set help_data(killall)		{{<objects>}	{kill object[s] and all references}}
set help_data(killtree)		{{<object>}	{kill complete tree[s] - BE CAREFUL}}
set help_data(knob)		{{[-e -i -v] [id [val]]}	{emulate knob twist}}
set help_data(l)		{{<objects>}	{list attributes (verbose)}}
set help_data(L)		{{1|0 xpos ypos}	{handle a left mouse event}}
set help_data(labelvert)	{{object[s]}	{label vertices of wireframes of objects}}
set help_data(listeval)		{{}	{lists 'evaluated' path solids}}
set help_data(load_dv)		{{}	{Initializes the view matrices}}
set help_data(loadtk)		{{[DISPLAY]}	{Initializes Tk window library}}
set help_data(lookat)		{{x y z}	{Adjust view to look at given coordinates}}
set help_data(ls)		{{}	{table of contents}}
set help_data(M)		{{1|0 xpos ypos}	{handle a middle mouse event}}
set help_data(make)		{{name <arb8|sph|ellg|tor|tgc|rpc|rhc|epa|ehy|eto|part|grip|half|nmg|pipe>}	{create a primitive}}
set help_data(make_bb)		{{new_rpp_name obj1_or_path1 [list of objects or paths ...]}	{make a bounding box solid enclosing specified objects/paths}}
set help_data(mater)		{{comb [material]}	{assign/delete material to combination}}
set help_data(matpick)		{{# or a/b}	{select arc which has matrix to be edited, in O_PATH state}}
set help_data(memprint)		{{}	{print memory maps}}
set help_data(mirface)		{{#### axis}	{mirror an ARB face}}
set help_data(mirror)		{{old new axis}	{mirror solid or combination around axis}}
set help_data(model2view)	{{mx my mz}	{convert point in model coords (mm) to view coords}}
set help_data(mv)		{{old new}	{rename object}}
set help_data(mvall)		{{oldname newname}	{rename object everywhere}}
set help_data(nirt)		{{}	{trace a single ray from current view}}
set help_data(nmg_simplify)	{{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
set help_data(oed)		{{path_lhs path_rhs}	{Go from view to object_edit of path_lhs/path_rhs}}
set help_data(opendb)		{{database.g}	{Close current .g file, and open new .g file}}
set help_data(openw)		{{[-c b|c|g] [-d display string] [-gd graphics display string] [-gt graphics type] [-id name] [-h] [-j] [-s]}	{open display/command window pair}}
set help_data(orientation)	{{x y z w}	{Set view direction from quaternion}}
set help_data(orot)		{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set help_data(oscale)		{{factor}	{scale object by factor}}
set help_data(overlay)		{{file.plot [name]}	{Read UNIX-Plot as named overlay}}
set help_data(p)		{{dx [dy dz]}	{set parameters}}
set help_data(paths)		{{pattern}	{lists all paths matching input path}}
set help_data(pathlist)		{{name(s)}	{list all paths from name(s) to leaves}}
set help_data(pcs)		{{}	{print collaborative participants}}
set help_data(pmp)		{{}	{print mged players}}
set help_data(permute)		{{tuple}	{permute vertices of an ARB}}
set help_data(plot)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{make UNIX-plot of view}}
set help_data(pl)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{Experimental - uses dm-plot:make UNIX-plot of view}}
set help_data(polybinout)	{{file}	{store vlist polygons into polygon file (experimental)}}
set help_data(pov)		{{args}	{experimental:  set point-of-view}}
set help_data(prcolor)		{{}	{print color&material table}}
set help_data(prefix)		{{new_prefix object(s)}	{prefix each occurrence of object name(s)}}
set help_data(preview)		{{[-v] [-d sec_delay] [-D start frame] [-K last frame] rt_script_file}	{preview new style RT animation script}}
set help_data(press)		{{button_label}	{emulate button press}}
set help_data(ps)		{{[-f font] [-t title] [-c creator] [-s size in inches] [-l linewidth] file}	{creates a postscript file of the current view}}
set help_data(push)		{{object[s]}	{pushes object's path transformations to solids}}
set help_data(putmat)		{{a/b {I | m0 m1 ... m16}}	{replace matrix on combination's arc}}
set help_data(q)		{{}	{quit}}
set help_data(qcs)		{{id}	{quit collaborative session}}
set help_data(quit)		{{}	{quit}}
set help_data(qorot)		{{x y z dx dy dz theta}	{rotate object being edited about specified vector}}
set help_data(qvrot)		{{dx dy dz theta}	{set view from direction vector and twist angle}}
set help_data(r)		{{region <operation solid>}	{create or extend a Region combination}}
set help_data(R)		{{1|0 xpos ypos}	{handle a right mouse event}}
set help_data(rcodes)		{{filename}	{read region ident codes from filename}}
set help_data(red)		{{object}	{edit a group or region using a text editor}}
set help_data(refresh)		{{}	{send new control list}}
set help_data(regdebug)		{{[number]}	{toggle display manager debugging or set debug level}}
set help_data(regdef)		{{item [air [los [GIFTmaterial]]]}	{change next region default codes}}
set help_data(regions)		{{file object(s)}	{make ascii summary of regions}}
set help_data(release)		{{[name]}	{release display processor}}
set help_data(release4)		{{id}	{release the display manager window opened with attach4}}
set help_data(rfarb)		{{}	{makes arb given point, 2 coord of 3 pts, rot, fb, thickness}}
set help_data(rm)		{{comb <members>}	{remove members from comb}}
set help_data(rmater)		{{filename}	{read combination materials from filename}}
set help_data(rmats)		{{file}	{load view(s) from 'savekey' file}}
set help_data(rotobj)		{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set help_data(rrt)		{{prog [options]}	{invoke prog with view}}
set help_data(rt)		{{[options]}	{do raytrace of view}}
set help_data(rtcheck)		{{[options]}	{check for overlaps in current view}}
set help_data(savekey)		{{file [time]}	{save keyframe in file (experimental)}}
set help_data(saveview)		{{file [args]}	{save view in file for RT}}
set help_data(showmats)		{{path}	{show xform matrices along path}}
set help_data(sed)		{{<path>}	{solid-edit named solid}}
set help_data(setview)		{{x y z}	{set the view given angles x, y, and z in degrees}}
set help_data(shells)		{{nmg_model}	{breaks model into seperate shells}}
set help_data(shader)		{{comb material [arg(s)]}	{assign materials (like 'mater')}}
set help_data(size)		{{size}	{set view size}}
set help_data(sliders)		{{[on|off]}	{turns the sliders on or off, or reads current state}}
set help_data(solids)		{{file object(s)}	{make ascii summary of solid parameters}}
set help_data(solids_on_ray)	{{h v}	{List all displayed solids along a ray}}
set help_data(status)		{{}	{get view status}}
set help_data(summary)		{{[s r g]}	{count/list solid/reg/groups}}
set help_data(sv)		{{x y [z]}	{Move view center to (x, y, z)}}
set help_data(svb)		{{}	{set view reference base}}
set help_data(sync)		{{}	{forces UNIX sync}}
set help_data(t)		{{}	{table of contents}}
set help_data(ted)		{{}	{text edit a solid's parameters}}
set help_data(tie)		{{pathName1 pathName2}	{tie display manager pathName1 to display manager pathName2}}
set help_data(title)		{{[string]}	{print or change the title}}
set help_data(tol)		{{[abs #] [rel #] [norm #] [dist #] [perp #]}	{show/set tessellation and calculation tolerances}}
set help_data(tops)		{{}	{find all top level objects}}
set help_data(track)		{{<parameters>}	{adds tracks to database}}
set help_data(tran)		{{[-i] x y [z]}	{absolute translate using view coordinates}}
set help_data(translate)	{{x y z}	{trans object to x,y, z}}
set help_data(tree)		{{object(s)}	{print out a tree of all members of an object}}
set help_data(units)		{{[mm|cm|m|in|ft|...]}	{change units}}
set help_data(untie)		{{pathName}	{untie display manager pathName}}
set help_data(mged_update)	{{}	{handle outstanding events and refresh}}
set help_data(vars)		{{[var=opt]}	{assign/display mged variables}}
set help_data(vdraw)		{{write|insert|delete|read|length|show [args]}	{Expermental drawing (cnuzman)}}
set help_data(viewget)		{{center|size|eye|ypr|quat|aet}	{Experimental - return high-precision view parameters.}}
set help_data(viewset)		{{center|eye|size|ypr|quat|aet}	{Experimental - set several view parameters at once.}}
set help_data(view2model)	{{mx my mz}	{convert point in view coords to model coords (mm)}}
set help_data(vrmgr)		{{host {master|slave|overview}}	{link with Virtual Reality manager}}
set help_data(vrot)		{{xdeg ydeg zdeg}	{rotate viewpoint}}
set help_data(vrot_center)	{{v|m x y z}	{set center point of viewpoint rotation, in model or view coords}}
set help_data(wcodes)		{{filename object(s)}	{write region ident codes to filename}}
set help_data(whatid)		{{region_name}	{display ident number for region}}
set help_data(whichair)		{{air_codes(s)}	{lists all regions with given air code}}
set help_data(whichid)		{{ident(s)}	{lists all regions with given ident code}}
set help_data(which_shader)	{{Shader(s)}	{lists all combinations using the given shaders}}
set help_data(who)		{{[r(eal)|p(hony)|b(oth)]}	{list the top-level objects currently being displayed}}
set help_data(winset)		{{pathname}	{sets the current display manager to pathname}}
set help_data(wmater)		{{filename comb(s)}	{write combination materials to filename}}
set help_data(x)		{{lvl}	{print solid table & vector list}}
set help_data(xpush)		{{object}	{Experimental Push Command}}
set help_data(Z)		{{}	{zap all objects off screen}}
set help_data(zoom)		{{scale_factor}	{zoom view in or out}}

proc help {args}	{
	global help_data

	if {[llength $args] > 0} {
                return [help_comm help_data $args]
        } else {
                return [help_comm help_data]
        }
}

proc ? {} {
	global help_data

	return [?_comm help_data 15 5]
}

proc apropos key {
	global help_data

	return [apropos_comm help_data $key]
}





#==============================================================================
# Other Support Routines
#==============================================================================
