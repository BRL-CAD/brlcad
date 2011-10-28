#                        T E X T . T C L
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
# Utility routines called by MGED's Tcl/Tk command window(s).
#

catch {bind Text <Control-Key-slash> {}} err
catch {bind Text <<Cut>> {}} err

proc tk_textPaste {w} {
    global tcl_platform
    if {![catch {::tk::GetSelection $w CLIPBOARD} sel]} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	#if {[tk windowingsystem] ne "x11"} {
	#    catch { $w delete sel.first sel.last }
	#}
	$w insert insert $sel
	if {$oldSeparator} {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
}

proc distribute_text { w cmd str } {
    global mged_players
    global mged_default

    set src_id [get_player_id_t $w]
    foreach id $mged_players {
	set _w .$id.t
	if [winfo exists $_w] {
	    if {$w != $_w} {
		set _promptBegin [$_w index {end - 1 l}]
		$_w mark set curr insert
		$_w mark set insert $_promptBegin

		if {$cmd != ""} {
		    mged_print_tag $_w "mged:$src_id> " prompt
		    mged_print_tag $_w $cmd\n oldcmd
		}

		if {$str != ""} {
		    if {[string index $str end] == "\n"} {
			mged_print_tag $_w $str result
		    } else {
			mged_print_tag $_w $str\n result
		    }
		}

		$_w mark set insert curr
		$_w see insert
	    }

	    # get rid of oldest output
	    set nlines [expr int([$_w index end])]
	    if {$nlines > $mged_default(max_text_lines)} {
		$_w delete 1.0 [expr $nlines - $mged_default(max_text_lines)].end
	    }
	}
    }
}

proc get_player_id_dm { win } {
    global win_to_id

    if [info exists win_to_id($win)] {
	return $win_to_id($win)
    }

    return "mged"
}

proc first_char_in_line { w } {
    $w mark set insert promptEnd
    set c [$w get insert]
    if {$c == " "} {
	forward_word $w
    }
    cursor_highlight $w
}

proc beginning_of_line { w } {
    $w mark set insert promptEnd
    cursor_highlight $w
}

proc end_of_line { w } {
    $w mark set insert {end - 2c}
    cursor_highlight $w
    $w see insert
}

proc backward_char { w } {
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	cursor_highlight $w
    }
}

proc forward_char { w } {
    if [$w compare insert < {end - 2c}] {
	$w mark set insert {insert + 1c}
	cursor_highlight $w
    }
}

proc backward_word { w } {
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
    } else {
	$w mark set insert promptEnd
    }

    cursor_highlight $w
}

proc forward_word { w } {
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" insert {end - 2c}]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
	cursor_highlight $w
    }
}

proc end_word { w } {
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w mark set insert $ti
    } else {
	$w mark set insert {end - 2c}
    }

    cursor_highlight $w
}

proc backward_delete_char { w } {
    #    catch {$w tag remove sel sel.first promptEnd}
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	$w delete insert
	cursor_highlight $w
    }
}

proc delete_char { w } {
    #    catch {$w tag remove sel sel.first promptEnd}
    if {[$w compare insert >= promptEnd] && [$w compare insert < {end - 2c}]} {
	$w delete insert
	cursor_highlight $w
    }
}

proc backward_delete_word { w } {
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w delete "$ti + 1c" insert
    } else {
	$w delete promptEnd insert
    }
    cursor_highlight $w
}

proc delete_word { w } {
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight $w
}

proc delete_end_word { w } {
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight $w
}


proc delete_line { w } {
    $w delete promptEnd end-2c
    cursor_highlight $w
}

proc delete_end_of_line { w } {
    $w delete insert end-2c
    cursor_highlight $w
}

proc delete_beginning_of_line { w } {
    $w delete promptEnd insert
    cursor_highlight $w
}

proc next_command { w } {
    global mged_gui

    set id [get_player_id_t $w]
    cmd_win set $id
    set result [catch {hist next} msg]

    if {$result==0} {
	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd
	$w insert insert [string range $msg 0 \
			      [expr [string length $msg]-2]]

	cursor_highlight $w
	$w see insert
    } else {
	if {!$mged_gui($w,freshline)} {
	    $w delete promptEnd {end - 2c}
	    $w mark set insert promptEnd
	    $w insert insert [string range $mged_gui($w,scratchline) 0\
				  [expr [string length $mged_gui($w,scratchline)] - 1]]
	    set mged_gui($w,freshline) 1
	    cursor_highlight $w
	}
    }
}

proc prev_command { w } {
    global mged_gui

    set id [get_player_id_t $w]
    cmd_win set $id
    set result [catch {hist prev} msg]

    if {$result==0} {
	if {$mged_gui($w,freshline)} {
	    set mged_gui($w,scratchline) [$w get promptEnd {end -2c}]
	    set mged_gui($w,freshline) 0
	}

	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd

	$w insert insert [string range $msg 0 \
			      [expr [string length $msg]-2]]

	cursor_highlight $w
	$w see insert
    }
    return $result
}

proc transpose { w } {
    if {[$w compare insert > promptEnd] && [$w compare {end - 2c} > {promptEnd + 1c}]} {
	if [$w compare insert >= {end - 2c}] {
	    set before [$w get {insert - 2c}]
	    $w delete {insert - 2c}
	    $w insert insert $before
	} else {
	    set before [$w get {insert - 1c}]
	    $w delete {insert - 1c}
	    $w insert {insert + 1c} $before
	    $w mark set insert {insert + 2c}
	}
    }
    cursor_highlight $w
}


# hack to hide the standard tcl gets implementation so we can wrap it.
if {[catch {info body gets}] == 1 && [catch {info body tcl_gets}] == 1} {
    rename gets tcl_gets
}

proc gets {channelId args} {
    global getsVal

    set len [llength $args]
    if {$len != 0 && $len != 1} {
	error "Usage: gets channelId ?varName?"
    }
   
    upvar $args [lindex $args 0]

    if {$channelId != "stdin"} {
	return [eval tcl_gets $channelId $args]
    }

    # reading from stdin so we hack the text input widget to capture
    # the entire input, then we strip off the gets line leaving just
    # the line that was input.  total hack.  total fugly hack.

    proc execute_cmd_save {} {}
    rename execute_cmd_save ""
    rename execute_cmd execute_cmd_save
    rename gets_execute_cmd execute_cmd

    vwait getsVal
    upvar $args vname

    set lines [split $getsVal "\n"]
    if {[lindex $lines end] == {}} {
	set lines [lreplace $lines end end]
    }

    set vname [lindex $lines end]

    # if a var is provided, return the length.  otherwise return the
    # string itself.  this matches gets behavior.
    if {$len == 1} {
	return [string length $vname]
    }
    return $vname
}

proc gets_execute_cmd {w} {
    global getsVal

    rename execute_cmd gets_execute_cmd
    rename execute_cmd_save execute_cmd

    $w mark set insert {end - 2c}
    $w insert insert \n

    $w see insert
    update

    set getsVal [$w get promptEnd insert]
}

proc execute_cmd {w} {
    global mged_gui

    $w mark set insert {end - 2c}
    $w insert insert \n

    $w see insert
    update

    ia_invoke $w
    set mged_gui($w,freshline) 1
    cursor_highlight $w
    $w edit reset
}

proc interrupt_cmd { w } {
    global mged_gui

    set id [get_player_id_t $w]
    set mged_gui($id,cmd_prefix) ""
    set mged_gui($id,more_default) ""
    $w insert insert \n
    mged_print_prompt $w "mged> "
    $w see insert
}

##################################################################################
#                                                                                #
#                        VI Specific Callbacks                                   #
#                                                                                #
##################################################################################
proc vi_edit_mode { w } {
    global vi_state

    bind $w <BackSpace> {
	backward_char %W
	break
    }

    bind $w <Return> {
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    bind $w <KP_Enter> {
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    #
    # In edit mode, <Escape> means abort the current multi-key command.
    # If no multi-key command is pending or we are expecting a serach
    # char, beep.
    #
    bind $w <Escape> {
	if {![llength $vi_state(%W,cmd_list)] || [string compare "" $vi_state(%W,search_flag)]} {
	    bell
	}
	vi_reset_cmd %W
	break
    }

    bind $w <KeyPress> {
	vi_process_edit_cmd %W %A %K %s
	break
    }
}

proc vi_overwrite_mode { w } {
    bind $w <BackSpace> {
	backward_delete_char %W
	break
    }

    bind $w <Return> {
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    bind $w <KP_Enter> {
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    bind $w <Escape> {
	%W edit separator
	vi_edit_mode %W
	break
    }

    bind $w <KeyPress> {
	vi_process_overwrite %W %A %s
	break
    }
}

proc vi_hsrch_mode { w } {
    global vi_state

    bind $w <BackSpace> {
	%W mark set insert {insert - 1c}
	%W delete insert
	if [%W compare insert > promptEnd] {
	    set vi_state(%W,hsrch_buf) [string replace $vi_state(%W,hsrch_buf) end end]
	    cursor_highlight %W
	} else {
	    vi_edit_mode %W
	    vi_kill_cmd %W
	}
	break
    }

    bind $w <Return> {
	vi_finish_hsrch %W "n"
	break
    }

    bind $w <KP_Enter> {
	vi_finish_hsrch %W "n"
	break
    }

    bind $w <Escape> {
	vi_append_hsrch %W %A
	break
    }

    bind $w <KeyPress> {
	vi_append_hsrch %W %A
	break
    }
}

proc vi_insert_mode { w } {
    global vi_state

    bind $w <BackSpace> {
	if {$vi_state(%W,dot_flag)} {
	    lappend vi_state(%W,dot_list) %K
	}
	backward_delete_char %W
	break
    }

    bind $w <Return> {
	if {$vi_state(%W,dot_flag)} {
	    lappend vi_state(%W,dot_list) Escape
	}
	set vi_state(%W,dot_flag) 0
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    bind $w <KP_Enter> {
	if {$vi_state(%W,dot_flag)} {
	    lappend vi_state(%W,dot_list) Escape
	}
	set vi_state(%W,dot_flag) 0
	execute_cmd %W
	vi_insert_mode %W
	break
    }

    bind $w <Escape> {
	if {$vi_state(%W,dot_flag)} {
	    lappend vi_state(%W,dot_list) %K
	}
	set vi_state(%W,dot_flag) 0
	%W edit separator
	vi_edit_mode %W
	break
    }

    bind $w <KeyPress> {
	if {$vi_state(%W,dot_flag)} {
	    lappend vi_state(%W,dot_list) %K
	}
    }
}

# vi_reset_cmd is meant to be called whenever the vi_state is to be
# returned to a start-of-command state.
proc vi_reset_cmd { w } {
    global vi_state

    if {$vi_state($w,warn_flag)} {
	set vi_state($w,warn_flag) 0
	bell
    }

    set vi_state($w,yank_flag) 0
    set vi_state($w,delete_flag) 0
    set vi_state($w,change_flag) 0
    set vi_state($w,count_flag) 0
    set vi_state($w,hsrch_flag) 0
    set vi_state($w,cmd_count) 1
    set vi_state($w,pos_count) 1
    set vi_state($w,tmp_count) 1
    set vi_state($w,cmd_list) [list]
}

# vi_finish_cmd is meant to be called whenever an edit-mode command is
# complete.  Insert mode is entered if applicable, the cursor is highlit,
# and vi_process_edit_cmd state is reset.  If the command results in a
# buffer change, it is stored in dot_list.
proc vi_finish_cmd { w } {
    global vi_state

    if {$vi_state($w,change_flag)} {
	vi_insert_mode $w
    }
    cursor_highlight $w
    $w see insert

    # If we are doing a dot command, ignore cmd_list.
    if {$vi_state($w,dot_flag)} {
	set vi_state($w,dot_flag) 0
    } elseif {$vi_state($w,delete_flag)} {
	set vi_state($w,dot_list) $vi_state($w,cmd_list)
	if {$vi_state($w,change_flag)} {
	    set vi_state($w,dot_flag) 1
	}
    }

    vi_reset_cmd $w
}

# vi_kill_cmd is meant to be called whenever an edit-mode command is
# malformed or otherwise invalid.  vi_process_edit_cmd state is reset,
# bell is issued.
proc vi_kill_cmd { w } {
    global vi_state

    set vi_state($w,warn_flag) 1

    vi_reset_cmd $w
}

# vi_finish_hsrch finishes up a history search.
proc vi_finish_hsrch { w dir } {
    global vi_state
    global mged_gui

    set id [get_player_id_t $w]
    cmd_win set $id

    set next next
    set prev prev

    if {$dir == "N"} {
	set next prev
	set prev next
    }

    if {$vi_state($w,hsrch_type)=="/"} {
	set result [catch {hist $prev $vi_state($w,hsrch_buf)} msg]
    } else {
	set result [catch {hist $next $vi_state($w,hsrch_buf)} msg]
    }

    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd

    # If we failed to find a command, retrieve the current command.
    if {$result} {
	set vi_state($w,warn_flag) 1
	if {$mged_gui($w,freshline)} {
	    set msg $mged_gui($w,scratchline)
	    append msg "x"
	    set result 0
	} else {
	    set result [catch {hist cur} msg]
	}
    } else {
	set mged_gui($w,freshline) 0
    }

    if {$result==0} {
	$w insert insert [string range $msg 0 [expr [string length $msg]-2]]
    }
    vi_edit_mode $w
    vi_finish_cmd $w
}

#vi_append_hsrch appends a char onto the history search buffer.
proc vi_append_hsrch { w c } {
    global vi_state

    if {$vi_state($w,hsrch_flag)} {
	set vi_state($w,hsrch_buf) ""
	set vi_state($w,hsrch_flag) 0
    }
    append vi_state($w,hsrch_buf) $c
    $w insert insert $c
}

# vi_word_search accomplishes the mechanics of word boundary searching...
proc vi_word_search { w c } {
    global vi_state

    set bward [expr {"b" == [string tolower $c]}]
    set vi_state($w,tmp_count) $vi_state($w,pos_count)

    # search while pos_count is gte one and search returns an index.
    for { set newindex [$w index insert] } {1} {
	if {1 == $vi_state($w,pos_count) || $newindex == ""} {
	    break
	} else {
	    set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) - 1}]
	}
    } {
	for {
	    if {$bward} { # b/B branch
		set newindex [$w index "$newindex - 1c"]
		set cmpindex [$w index "$newindex - 1c"]
	    } else {
		set newindex [$w index "$newindex + 1c"]
		if { "w" == [string tolower $c]} {
		    set cmpindex [$w index "$newindex - 1c"]
		} else { # e/E branch
		    set cmpindex [$w index "$newindex + 1c"]
		}
	    }
	} {1} {
	    if {$bward} {
		if [$w compare $newindex < promptEnd] {
		    set newindex ""
		    break
		}
	    } else {
		if [$w compare $newindex > {end - 2c}] {
		    set newindex ""
		    break
		}
	    }
	    if [string is lower $c] {
		if {   ([string is space $cc] && [string is punct $nc])
		       || ([string is space $cc] && [string is wordchar $nc])
		       || ([string is punct $cc] && [string is wordchar $nc])
		       || ([string is wordchar $cc] && [string is punct $nc])} {
		    break
		}
	    } else {
		if {[string is space $cc] && ([string is punct $nc] || [string is wordchar $nc])} {
		    break
		}
	    }
	    if {$bward} {
		set newindex [$w index "$newindex - 1c"]
		set cmpindex [$w index "$cmpindex - 1c"]
	    } else {
		set newindex [$w index "$newindex + 1c"]
		set cmpindex [$w index "$cmpindex + 1c"]
	    }
	} {
	    set cc [$w get $cmpindex]
	    set nc [$w get $newindex]
	}
    }

    # Correct for expected "y/cw" behaviour, retrace whitespace.
    if {   $newindex != ""
	   && (   $vi_state($w,change_flag)
		  || ($vi_state($w,yank_flag) && !$vi_state($w,delete_flag)))
	   && "w" == [string tolower $c]} {
	for { set newindex [$w index "$newindex - 1c"] } \
	    { [string is space [$w get $newindex]] } \
	    { if [$w compare $newindex > insert ] {
		set newindex [$w index "$newindex - 1c"]
	    } else {
		break
	    }
	    } { }
	# Yank/change operate across chars, so increment newindex.
	set newindex [$w index "$newindex + 1c"]
    }

    # Fix count overruns.
    if {$vi_state($w,yank_flag) && $newindex == ""} {
	if {$bward} {
	    set newindex [$w index promptEnd]
	} else {
	    set newindex [$w index {end - 2c}]
	}
    }

    if {$newindex != ""} {
	# Save the indicated range in the cut_buf.
	if {$vi_state($w,yank_flag)} {
	    if {$bward} {
		set vi_state($w,cut_buf) [$w get $newindex insert]
	    } else {
		set vi_state($w,cut_buf) [$w get insert $newindex]
	    }
	}
	# Delete the indicated range.
	if {$vi_state($w,delete_flag)} {
	    $w edit separator
	    if {$bward} {
		$w delete $newindex insert
	    } else {
		$w delete insert $newindex
	    }
	}
	# Or just move the insert gnomon.
	if {!$vi_state($w,yank_flag)} {
	    $w mark set insert $newindex
	}
	vi_finish_cmd $w
    } else {
	vi_kill_cmd $w
    }
}

proc vi_process_edit_cmd { w c k state } {
    global mged_gui
    global vi_state

    set vi_state($w,debug) $c

    # Throw away all control characters
    if {[string is control $c] || $state > 1} {
	return
    }

    if {$vi_state($w,overwrite_flag)} {
	set vi_state($w,overwrite_flag) 0
	# Some vi CLI's allow writing past the
	# end of the line; vi proper doesn't and neither do we.
	set newindex [$w index [subst "insert + $vi_state($w,cmd_count)c"]]
	if [$w compare $newindex > {end - 2c}] {
	    vi_kill_cmd $w
	} else {
	    $w edit separator
	    $w delete insert $newindex
	    # Insert chars while cmd_count hasn't fallen to one, this is
	    # do {} while () a la TCL:
	    for {} {1} {
		if {1 == $vi_state($w,cmd_count)} {
		    break
		} else {
		    set vi_state($w,cmd_count) [expr {$vi_state($w,cmd_count) - 1}]
		}
	    } {
		$w insert insert $c
	    }
	    # Append the replacement char to cmd_list and set delete_flag
	    # to cause dot_list storage.
	    lappend vi_state($w,cmd_list) $k
	    set vi_state($w,delete_flag) 1
	    vi_finish_cmd $w
	}
	return
    }

    # If a search char is provided, store it and masquerade as a ";"
    # command.  This saves duplication of dozens of lines of complicated
    # search code.
    if [string compare "" $vi_state($w,search_flag)] {
	set vi_state($w,search_type) $vi_state($w,search_flag)
	set vi_state($w,search_char) $c
	set vi_state($w,search_flag) ""
	set c ";"
	set k semicolon
	# Trim the existing f, F, t, or T off the cmd_list.  A ";" will
	# be added below.
	set vi_state($w,cmd_list) [lreplace $vi_state($w,cmd_list) end end]
    }

    # If we have been scanning a command count and are encountering a
    # command character, make sure the command is consistent with a
    # count.
    if {$vi_state($w,count_flag)} {
	switch -glob -- $c {
	    [0-9] {
		# Ignore counts; handled below.
	    }
	    [+,-;BEFTWbefhjkltw] {
		# These position specifiers happily take counts.
		set vi_state($w,pos_count) $vi_state($w,tmp_count)
		set vi_state($w,count_flag) 0
		set vi_state($w,tmp_count) 1
	    }
	    [Xrsux~] {
		# These commands happily take counts.
		set vi_state($w,cmd_count) $vi_state($w,tmp_count)
		set vi_state($w,count_flag) 0
		set vi_state($w,tmp_count) 1
	    }
	    [cdy] {
		# "c..", "d..", and "y.." can take a cmd_count, but "cc" and "dd"
		# do not. (in CLE; in real VI "cc" and "dd" do take counts.)
		if {!$vi_state($w,yank_flag)} {
		    set vi_state($w,cmd_count) $vi_state($w,tmp_count)
		} else {
		    set vi_state($w,cmd_count) 1
		}
		set vi_state($w,count_flag) 0
		set vi_state($w,tmp_count) 1
	    }
	    [$./?ACDINPRY^_ainp] {
		# These commands ignore counts.
		set vi_state($w,count_flag) 0
		set vi_state($w,tmp_count) 1
	    }
	}
    }

    # If we are in yank, change, or delete mode, make sure the command character
    # is consistent with that mode.
    if {$vi_state($w,yank_flag)} {
	switch -glob -- $c {
	    [0] {
		# if we are counting, ignore; begining-of-line ignores counts.
		if {!$vi_state($w,count_flag)} {
		    set vi_state($w,cmd_count) 1
		}
	    }
	    [1-9] {
		# Ignore numbers; counts are dealt with below.
	    }
	    [cdy] {
		# "c.." and "d.." are legit in change and delete modes
		# respectively, otherwise they are in error.
		if {   ("c" == $c && !$vi_state($w,change_flag))
		       || ("d" == $c && $vi_state($w,change_flag))} {
		    vi_kill_cmd $w
		    return
		}
	    }
	    [+,-;BEFTWbefhltw] {
		# These positional specifiers can occur in y/c/d modes.
		# Multiply pos_count by cmd_count (2d2w means delete FOUR words.)
		set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) * $vi_state($w,cmd_count)}]
		set vi_state($w,cmd_count) 1
	    }
	    [./?ACDXINPRYadijknprsux~] {
		# These commands cannot occur in y/c/d modes.
		vi_kill_cmd $w
		return
	    }
	    [$^_] {
		# End-of-line, start-of-line occurs in y/c/d modes, ignores counts.
		set vi_state($w,cmd_count) 1
	    }
	}
    }

    # At this point it looks like this character is part of a well-formed
    # edit command.  Append it to the cmd_list.
    lappend vi_state($w,cmd_list) $k

    switch -glob -- $c {
	$ {
	    # Motion to the end of the line.
	    if {$vi_state($w,yank_flag)} {
		set vi_state($w,cut_buf) [$w get insert {end - 2c}]
	    }
	    if {$vi_state($w,delete_flag)} {
		$w edit separator
		$w delete insert {end - 2c}
	    }
	    if {!$vi_state($w,yank_flag)} {
		$w mark set insert {end - 2c}
	    }
	    vi_finish_cmd $w
	}
	% {
	    # Motion to matching brace.
	    set newindex [$w index insert]
	    set om [$w get insert]
	    set bward 0
	    switch -glob -- $om {
		[{] {
		    set cm "\}"
		}
		 [}] {
		     set cm $om
		     set om "\{"
		     set bward 1
		 }
		[(] {
		    set cm "\)"
		}
		 [)] {
		     set cm $om
		     set om "\("
		     set bward 1
		 }
		[\[] {
		    set cm "\]"
		}
		\] {
		    set cm $om
		    set om "\["
		    set bward 1
		}
		default {
		    set om ""
		    vi_kill_cmd $w
		}
	    }

	    if [string length $om] {
		for { set dc 0 } {1} \
		    {
		    if {$bward} {
			if [$w compare $newindex < promptEnd] {
			    set newindex ""
			    break
			}
			set newindex [$w index "$newindex - 1c"]
		    } else {
			if [$w compare $newindex > {end - 2c}] {
			    set newindex ""
			    break
			}
			set newindex [$w index "$newindex + 1c"]
		    }
		} {
		    set cc [$w get $newindex]
		    if {$cc == $cm} {
			set dc [expr $dc - 1]
		    } elseif {$cc == $om} {
			incr dc
		    }
		    if {!$dc} {
			break
		    }
		}

		if {$newindex != ""} {
		    # Save the indicated range in the cut_buf, and delete it.
		    if {$vi_state($w,yank_flag)} {
			if {!$bward} {
			    set vi_state($w,cut_buf) [$w get insert "$newindex + 1c"]
			} else {
			    set vi_state($w,cut_buf) [$w get $newindex {insert + 1c}]
			}
		    }
		    if {$vi_state($w,delete_flag)} {
			$w edit separator
			if {!$bward} {
			    $w delete insert "$newindex + 1c"
			} else {
			    $w delete $newindex {insert + 1c}
			}
		    }
		    if {!$vi_state($w,yank_flag)} {
			# Or just move the insert gnomon.
			$w mark set insert $newindex
		    }
		    vi_finish_cmd $w
		} else {
		    vi_kill_cmd $w
		}
	    }
	}
	[,;] {
	    # ";" Previous char search in the direction it was indicated.
	    # "," Previous char search in the direction opposite that indicated.
	    if {$vi_state($w,search_char) == ""} {
		vi_kill_cmd $w
		return
	    }
	    # Is this a "find" or a "to" search?
	    set to_search [expr {"t" == [string tolower $vi_state($w,search_type)]}]

	    # Forward searches...
	    if {   (";" == $c && [string is lower $vi_state($w,search_type)])
		   || ("," == $c && [string is upper $vi_state($w,search_type)])} {

		# do while pos_count is gte one and seach_char is found
		for { set newindex [$w index {insert}] } {1} {
		    if {1 == $vi_state($w,pos_count) || $newindex == ""} {
			break
		    } else {
			set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) - 1}]
		    }
		} {
		    set newindex [$w search $vi_state($w,search_char) "$newindex +1c" {end - 2c}]
		}
		if {$newindex != ""} {
		    # Save the indicated range in the cut_buf.
		    if {$vi_state($w,yank_flag)} {
			if {!$to_search} {
			    set vi_state($w,cut_buf) [$w get insert "$newindex + 1c"]
			} else {
			    set vi_state($w,cut_buf) [$w get insert $newindex]
			}
		    }
		    # Delete the indicated range.
		    if {$vi_state($w,delete_flag)} {
			$w edit separator
			$w delete insert $newindex
			if {!$to_search} {
			    $w delete insert {insert + 1c}
			}
		    }
		    if {!$vi_state($w,yank_flag)} {
			# Just move the insert gnomon.
			$w mark set insert $newindex
			if {$to_search} {
			    $w mark set insert {insert - 1c}
			}
		    }
		}
		# Backward searches...
	    } else {
		# do while pos_count is gte one and seach_char is found
		for { set newindex [$w index {insert}] } {1} {
		    if {1 == $vi_state($w,pos_count) || $newindex == ""} {
			break
		    } else {
			set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) - 1}]
		    }
		} {
		    set newindex [$w search -backwards $vi_state($w,search_char) "$newindex" promptEnd]
		}
		if {$newindex != ""} {
		    # Save the indicated range in the cut_buf, and delete it.
		    if {$vi_state($w,yank_flag)} {
			if {!$to_search} {
			    set vi_state($w,cut_buf) [$w get $newindex insert]
			} else {
			    set vi_state($w,cut_buf) [$w get "$newindex + 1c" insert]
			}
		    }
		    if {$vi_state($w,delete_flag)} {
			$w edit separator
			$w delete "$newindex + 1c" insert
			if {!$to_search} {
			    $w delete $newindex insert
			}
		    }
		    if {!$vi_state($w,yank_flag)} {
			# Just move the insert gnomon.
			$w mark set insert $newindex
			if {$to_search} {
			    $w mark set insert {insert + 1c}
			}
		    }
		}
	    }
	    if {$newindex == ""} {
		vi_kill_cmd $w
	    } else {
		vi_finish_cmd $w
	    }
	}
	. {
	    # Redo last command.
	    if {![llength $vi_state($w,dot_list)]} {
		vi_kill_cmd $w
	    } else {
		set script [subst {
		    set vi_state($w,dot_flag) 1
		    set range \[llength \$vi_state($w,dot_list)\]
		    for {set i 0} {\$i < \$range} {incr i} {
			event generate $w <KeyPress> -state $state -keysym \[lindex \$vi_state($w,dot_list) \$i\]
		    }
		}]
		vi_finish_cmd $w
		after idle $script
	    }
	}
	/ {
	    # Search backwards for command.
	    set vi_state($w,hsrch_type) "/"
	    # hsrch_flag controls deletion of hsrch_buf. An empty search
	    # is the same as "n".
	    set vi_state($w,hsrch_flag) 1
	    if {$mged_gui($w,freshline)} {
		set mged_gui($w,scratchline) [$w get promptEnd {end -2c}]
	    }
	    $w mark set insert promptEnd
	    $w delete insert {end - 2c}
	    $w insert insert "/"
	    vi_hsrch_mode $w
	}
	0 {
	    # Motion to begining of line (or zero in a command or position count.)
	    if {$vi_state($w,count_flag)} {
		set vi_state($w,tmp_count) [expr $vi_state($w,tmp_count) * 10]
	    } else {
		if {$vi_state($w,yank_flag)} {
		    set vi_state($w,cut_buf) [$w get promptEnd insert]
		}
		if {$vi_state($w,delete_flag)} {
		    $w edit separator
		    $w delete promptEnd insert
		}
		if {!$vi_state($w,yank_flag)} {
		    $w mark set insert promptEnd
		}
		vi_finish_cmd $w
	    }
	}
	[1-9] {
	    # Part of a command or position count.
	    if {$vi_state($w,count_flag)} {
		set vi_state($w,tmp_count) [expr $vi_state($w,tmp_count) * 10 + $c]
	    } else {
		set vi_state($w,tmp_count) $c
		set vi_state($w,count_flag) 1
	    }
	}
	[?] {
	    # Search forwards for command.
	    set vi_state($w,hsrch_type) "?"
	    # hsrch_flag controls deletion of hsrch_buf. An empty search
	    # is the same as "n".
	    set vi_state($w,hsrch_flag) 1
	    if {$mged_gui($w,freshline)} {
		set mged_gui($w,scratchline) [$w get promptEnd {end -2c}]
	    }
	    $w mark set insert promptEnd
	    $w delete insert {end - 2c}
	    $w insert insert "?"
	    vi_hsrch_mode $w
	}
	A {
	    # Append to the end of line.
	    $w mark set insert {end - 2c}
	    $w edit separator
	    set vi_state($w,change_flag) 1
	    vi_finish_cmd $w
	}
	B {
	    # Backwards motion to the start of a word/punct string.
	    vi_word_search $w "B"
	}
	C {
	    # Change to the end of the line.
	    set vi_state($w,cut_buf) [$w get insert {end - 2c}]
	    $w edit separator
	    $w delete insert {end - 2c}
	    set vi_state($w,change_flag) 1
	    vi_finish_cmd $w
	}
	D {
	    # Delete to the end of the line.
	    set vi_state($w,cut_buf) [$w get insert {end - 2c}]
	    $w edit separator
	    $w delete insert {end - 2c}
	    set vi_state($w,delete_flag) 1
	    vi_finish_cmd $w
	}
	E {
	    # Forwards motion to the end of a word/punct string.
	    vi_word_search $w "E"
	}
	F {
	    # Find the previous ocurrence of a char.
	    set vi_state($w,search_flag) "F"
	}
	I {
	    # Insert at the beginning of the line.
	    $w mark set insert promptEnd
	    $w edit separator
	    set vi_state($w,change_flag) 1
	    vi_finish_cmd $w
	}
	N {
	    # Next command search in opposite to original direction.
	    vi_finish_hsrch $w $c
	}
	P {
	    # Insert cut but before cursor
	    if [string length $vi_state($w,cut_buf)] {
		$w edit separator
		$w insert insert $vi_state($w,cut_buf)
		# Set delete_flag to indicate the buffer modification.
		set vi_state($w,delete_flag) 1
		vi_finish_cmd $w
	    } else {
		vi_kill_cmd $w
	    }
	}
	R {
	    # Replace chars, overwrite mode.
	    vi_overwrite_mode $w
	    vi_finish_cmd $w
	}
	T {
	    # Motion "back to" an indicated char.
	    set vi_state($w,search_flag) "T"
	}
	U {
	    # Reset the edit buffer.
	    set tmp_reset_buf $vi_state($w,reset_buf)
	    set vi_state($w,reset_buf) [$w get promptEnd {end - 2c}]
	    $w delete promptEnd {end - 2c}
	    $w mark set insert promptEnd
	    $w insert insert $tmp_reset_buf
	    $w edit reset
	    vi_finish_cmd $w
	}
	W {
	    # Backwards motion to the start of a word.
	    vi_word_search $w "W"
	}
	X {
	    # Backwards delete of cmd_num chars.
	    set newindex [$w index [subst "insert - $vi_state($w,cmd_count)c"]]
	    if [$w compare $newindex < promptEnd] {
		set newindex [$w index promptEnd]
	    }
	    if [$w compare $newindex == insert] {
		set vi_state($w,warn_flag) 1
	    } else {
		set vi_state($w,cut_buf) [$w get $newindex insert]
		$w edit separator
		$w delete $newindex insert
		# Set delete_flag to indicate the buffer modification.
		set vi_state($w,delete_flag) 1
	    }
	    vi_finish_cmd $w
	}
	Y {
	    # Yank the entire line.
	    set vi_state($w,cut_buf) [$w get promptEnd {end - 2c}]
	}
	^ -
	_ {
	    # Motion to first non-white-space char on the line.
	    for { set newindex [$w index promptEnd] } \
		{ [string is space [$w get $newindex]] } \
		{
		    set newindex [$w index "$newindex + 1c"]
		} {
		    if [$w compare $newindex >= {end - 2c}] {
			break
		    }
		}
	    $w mark set insert $newindex
	    vi_finish_cmd $w
	}
	a {
	    # Append after the cursor.
	    if [$w compare insert < {end - 2c}] {
		$w mark set insert {insert + 1c}
	    }
	    $w edit separator
	    set vi_state($w,change_flag) 1
	    vi_finish_cmd $w
	}
	b {
	    # Backwards motion to the start of a word.
	    vi_word_search $w "b"
	}
	c {
	    # Change... or change line in the case of "cc".
	    if {$vi_state($w,change_flag)} {
		set vi_state($w,cut_buf) [$w get promptEnd {end - 2c}]
		$w edit separator
		$w delete promptEnd {end - 2c}
		vi_finish_cmd $w
	    } else {
		set vi_state($w,yank_flag) 1
		set vi_state($w,delete_flag) 1
		set vi_state($w,change_flag) 1
	    }
	}
	d {
	    # Delete... or delete lind in the case of "dd".
	    if {$vi_state($w,delete_flag)} {
		set vi_state($w,cut_buf) [$w get promptEnd {end - 2c}]
		$w edit separator
		$w delete promptEnd {end - 2c}
		vi_finish_cmd $w
	    } else {
		set vi_state($w,yank_flag) 1
		set vi_state($w,delete_flag) 1
	    }
	}
	e {
	    # Forwards motion to the end of a word.
	    vi_word_search $w "e"
	}
	f {
	    # Find the next ocurrence of a char.
	    set vi_state($w,search_flag) "f"
	}
	h {
	    # Motion one char left.
	    set newindex [$w index [subst "insert - $vi_state($w,pos_count)c"]]
	    if [$w compare $newindex < promptEnd] {
		set newindex [$w index {promptEnd}]
	    }
	    set traversed_chars  [$w get $newindex insert]
	    if {$vi_state($w,yank_flag)} {
		set vi_state($w,cut_buf) $traversed_chars
	    }
	    if {$vi_state($w,delete_flag)} {
		$w edit separator
		$w delete $newindex insert
	    }
	    if {!$vi_state($w,yank_flag)} {
		# Warn if too few chars to traverse. ("h" only, not "l".)
		if [$w compare $newindex == insert] {
		    set vi_state($w,warn_flag) 1
		} else {
		    $w mark set insert $newindex
		}
	    }
	    vi_finish_cmd $w
	}
	i {
	    # Enter insert mode.
	    $w edit separator
	    set vi_state($w,change_flag) 1
	    # set delete_flag so cmd_list is saved.
	    set vi_state($w,delete_flag) 1
	    vi_finish_cmd $w
	}
	+ -
	j {
	    # Motion down one line.
	    if {$mged_gui($w,freshline)} {
		vi_kill_cmd $w
	    } else {
		for { } {1} {
		    if {1 == $vi_state($w,pos_count)} {
			break
		    } else {
			set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) - 1}]
		    }
		} {
		    next_command $w
		}
		set vi_state($w,reset_buf) [$w get promptEnd {end - 2c}]
		$w edit reset
		vi_finish_cmd $w
	    }
	}
	- -
	k {
	    # Motion up one line.
	    for { } {1} {
		if {1 == $vi_state($w,pos_count)} {
		    break
		} else {
		    set vi_state($w,pos_count) [expr {$vi_state($w,pos_count) - 1}]
		}
	    } {
		if [set prev_result [prev_command $w]] {
		    break
		}
	    }
	    set vi_state($w,reset_buf) [$w get promptEnd {end - 2c}]
	    $w edit reset
	    if {$prev_result} {
		vi_kill_cmd $w
	    } else {
		vi_finish_cmd $w
	    }
	}
	l {
	    # Motion one char right.
	    set newindex [$w index [subst "insert + $vi_state($w,pos_count)c"]]
	    if [$w compare $newindex > {end - 2c}] {
		set newindex [$w index {end - 2c}]
	    }
	    if {$vi_state($w,yank_flag)} {
		set vi_state($w,cut_buf) [$w get insert $newindex]
	    }
	    if {$vi_state($w,delete_flag)} {
		set vi_state($w,cut_buf) [$w get insert $newindex]
		$w edit separator
		$w delete insert $newindex
	    }
	    if {!$vi_state($w,yank_flag)} {
		if [$w compare $newindex == insert] {
		    set vi_state($w,warn_flag) 1
		} else {
		    $w mark set insert $newindex
		}
	    }
	    vi_finish_cmd $w
	}
	n {
	    # Next command search in original direction.
	    vi_finish_hsrch $w $c
	}
	p {
	    # Insert cut but after cursor
	    if [string length $vi_state($w,cut_buf)] {
		$w edit separator
		if [$w compare insert < {end - 2c}] {
		    $w mark set insert {insert + 1c}
		}
		$w insert insert $vi_state($w,cut_buf)
		$w mark set insert {insert - 1c}
		# Set delete_flag to indicate the buffer modification.
		set vi_state($w,delete_flag) 1
		vi_finish_cmd $w
	    } else {
		vi_kill_cmd $w
	    }
	}
	r {
	    # Overwrite cmd_num chars.
	    set vi_state($w,overwrite_flag) 1
	}
	s {
	    # Substitute cmd_num chars.
	    set newindex [$w index [subst "insert + $vi_state($w,cmd_count)c"]]
	    if [$w compare $newindex > {end - 2c}] {
		set newindex [$w index {end - 2c}]
	    }
	    set vi_state($w,cut_buf) [$w get insert $newindex]
	    $w edit separator
	    $w delete insert $newindex
	    set vi_state($w,delete_flag) 1
	    set vi_state($w,change_flag) 1
	    vi_finish_cmd $w
	}
	t {
	    # Motion "to" an indicated char.
	    set vi_state($w,search_flag) "t"
	}
	u {
	    # Undo cmd_num edit actions.
	    for {} {1} {
		if {1 == $vi_state($w,cmd_count) || $caught} {
		    break
		} else {
		    set vi_state($w,cmd_count) [expr {$vi_state($w,cmd_count) - 1}]
		}
	    } {
		set caught [catch {$w edit undo}]
	    }

	    if {$caught} {
		set vi_state($w,warn_flag) 1
	    }
	    vi_finish_cmd $w
	}
	w {
	    # Forwards motion to the beginning of the next word.
	    vi_word_search $w "w"
	}
	x {
	    # Delete cmd_count chars.
	    set newindex [$w index [subst "insert + $vi_state($w,cmd_count)c"]]
	    if [$w compare $newindex > {end - 2c}] {
		set newindex [$w index {end - 2c}]
	    }
	    if [$w compare $newindex == insert] {
		set vi_state($w,warn_flag) 1
	    } else {
		set vi_state($w,cut_buf) [$w get insert $newindex]
		$w edit separator
		$w delete insert $newindex
	    }
	    # Set delete_flag to indicate the buffer modification.
	    set vi_state($w,delete_flag) 1
	    vi_finish_cmd $w
	}
	y {
	    # Yank...
	    if { $vi_state($w,yank_flag) } {
		# "yy" means yank the entire line.
		set vi_state($w,cut_buf) [$w get promptEnd {end - 2c}]
		vi_finish_cmd $w
	    } else {
		set vi_state($w,yank_flag) 1
	    }
	}
	~ {
	    # Switch character case.
	    set newindex [$w index [subst "insert + $vi_state($w,cmd_count)c"]]
	    if [$w compare $newindex > {end - 2c}] {
		set newindex [$w index {end - 2c}]
	    }
	    if [$w compare insert != $newindex] {
		# Set delete_flag to indicate the buffer modification.
		set vi_state($w,delete_flag) 1
	    }
	    for {} {[$w compare insert < $newindex]} {} {
		set ch [$w get insert]
		if [string is lower $ch] {
		    $w edit separator
		    $w delete insert
		    $w insert insert [string toupper $ch]
		} elseif [string is upper $ch] {
		    $w edit separator
		    $w delete insert
		    $w insert insert [string tolower $ch]
		} else {
		    $w mark set insert {insert + 1c}
		}
	    }
	    vi_finish_cmd $w
	}
	default {
	    vi_kill_cmd $w
	}
    }
}

proc vi_process_overwrite { w c state } {
    # Throw away all non-visible characters
    if {[string is control $c] || $state > 1} {
	return
    }

    delete_char $w
    $w insert insert $c
}
# End - VI Specific Callbacks


proc text_op_begin { w x y } {
    global mged_gui

    set mged_gui($w,moveView) 0
    set mged_gui($w,omx) $x
    set mged_gui($w,omy) $y
    $w scan mark $x $y
}

proc text_paste { w } {
    global mged_gui

    if {!$mged_gui($w,moveView)} {
	catch {$w insert insert [selection get -displayof $w]}
	$w see insert
    }

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

proc text_scroll { w x y } {
    global mged_gui

    if [expr {abs($mged_gui($w,omx) - $x) > 4 ||
	      abs($mged_gui($w,omy) - $y) > 4}] {
	set mged_gui($w,moveView) 1
	$w scan dragto $x $y
    }
}

proc selection_begin { w x y } {
    $w mark set anchor [::tk::TextClosestGap $w $x $y]
    $w tag remove sel 0.0 end

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

proc selection_add { w x y } {
    set cur [::tk::TextClosestGap $w $x $y]

    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }

    if [$w compare $cur < anchor] {
	set first $cur
	set last anchor
    } else {
	set first anchor
	set last $cur
    }

    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

proc select_word { w x y } {
    set cur [::tk::TextClosestGap $w $x $y]

    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }

    if [$w compare $cur < anchor] {
	set first [::tk::TextPrevPos $w "$cur + 1c" tcl_wordBreakBefore]
	set last [::tk::TextNextPos $w "anchor" tcl_wordBreakAfter]
    } else {
	set first [::tk::TextPrevPos $w anchor tcl_wordBreakBefore]
	set last [::tk::TextNextPos $w "$cur - 1c" tcl_wordBreakAfter]
    }

    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

proc select_line { w x y } {
    set cur [::tk::TextClosestGap $w $x $y]

    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }

    if [$w compare $cur < anchor] {
	set first [$w index "$cur linestart"]
	set last [$w index "anchor - 1c lineend + 1c"]
    } else {
	set first [$w index "anchor linestart"]
	set last [$w index "$cur lineend + 1c"]
    }

    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

proc selection_modify { w x y } {
    ::tk::TextResetAnchor $w @$x,$y
    selection_add $w $x $y
}

proc mged_print { w str } {
    $w insert insert $str
}

proc mged_print_prompt { w str } {
    mged_print_tag $w $str prompt
    $w mark set promptEnd insert
    $w mark gravity promptEnd left
}

proc mged_print_tag { w str tag } {
    set first [$w index insert]
    $w insert insert $str
    set last [$w index insert]
    $w tag add $tag $first $last
}

proc cursor_highlight { w } {
    $w tag delete hlt
    $w tag add hlt insert
    $w tag configure hlt -background yellow
}

# find the longest common initial string from a list of strings
proc get_longest_common_string { matches } {
    set done 0
    set lastMatchIndex 0
    set lastMatchChar [string index [lindex $matches 0] $lastMatchIndex]
    if { $lastMatchChar == "" } return ""
    while { $done == 0 } {
	foreach m $matches {
	    if { [string index $m $lastMatchIndex] != $lastMatchChar } {
		set done 1
		incr lastMatchIndex -1
		break
	    }
	}
	if { $done == 0 } {
	    incr lastMatchIndex
	    set lastMatchChar [string index [lindex $matches 0] $lastMatchIndex]
	}
    }
    if { $lastMatchIndex > -1 } {
	set name [string range [lindex $matches 0] 0 $lastMatchIndex]
    } else {
	set name ""
    }

    return $name
}

# do tab expansion
proc tab_expansion { line } {
    # list of mged commands
    global mged_cmds

    if { [info exists mged_cmds] == 0 } {
	set mged_cmds [?]
    }
    set matches {}

    set len [llength $line]

    if { $len > 1 } {
	# already have complete command, so do object expansion

	# check if we have an open db
	set dbCommand [info command db]
	if { [string length $dbCommand] == 0 } {
	    # no db command means no db is open, cannot expand
	    return [list $line {}]
	}

	# get last word on command line
	set word [lindex $line [expr $len - 1]]

	# verify that word contains a legit path
	# convert the path to a list of path elements
	set path [string map {"/" " "} $word]
	set pathLength [llength $path]

	# look for the last "/" in the object
	set index2 [string last "/" $word]

	set slashIsLast 0
	if { $index2 == [expr [string length $word]] - 1 } {
	    set slashIsLast 1
	}

	# only check if we have more than one path element
	if { $pathLength > 1 || $slashIsLast == 1 } {
	    if { $slashIsLast != 1 } {
		# do not verify the last element (that is what we expand)
		incr pathLength -1
	    }
	    for { set index 0 } { $index < $pathLength } { incr index } {
		set element [lindex $path $index]
		if { ! [ exists $element ] } {
		    # the current path element is invalid, just return
		    return [list $line {}]
		}
	    }
	}

	# we have a valid path, do expansion
	if { $index2 > 0 } {
	    incr index2 -1
	    set index1 [string last "/" $word $index2]
	    if { $index1 == -1 } {
		set index1 0
	    } else {
		incr index1
	    }

	    # grp contains the object name that appears prior to the last "/"
	    set grp [string range $word $index1 $index2]

	    # use anything after the last "/" to create a search pattern
	    if { $index2 < [expr [string length $word] - 2] } {
		set pattern "* [string range $word [expr $index2 + 2] end]*"
	    } else {
		set pattern "*"
	    }

	    # get the members of the last object on the command line
	    # the "lt" command returns a list of elements like "{ op name }"
	    if [catch {lt $grp} members] {
		set members {}
	    }

	    # use the search pattern to find matches in the list of members
	    set match [lsearch -all -inline $members $pattern]

	    set matchCount [llength $match]
	    if { $matchCount > 1 } {
		# eliminate duplicates
		set match [lsort -index 1 -unique $match]
		set matchCount [llength $match]
	    }

	    if { $matchCount == 0 } {
		# no matches just return
		set newCommand $line
	    } elseif { $matchCount == 1 } {
		# one match, do the substitution
		set name [lindex [lindex $match 0] 1]
		set index [string last "/" $line]
		set newCommand [string replace $line $index end "/$name"]
	    } else {
		# multiple matches, find the longest common match
		# extract the member names from the matches list
		set matches {}
		foreach m $match {
		    lappend matches [lindex $m 1]
		}

		# get the longest common string from the list of member names
		set name [get_longest_common_string $matches]
		if { $name != "" } {
		    # found something useful, add it to the command line
		    set index [string last "/" $line]
		    set newCommand [string replace $line $index end "/$name"]
		} else {
		    set newCommand $line
		}
	    }
	} else {
	    set prependSlash 0
	    if { $index2 == 0 } {
		# first char in word is "/" (only "/" in the word)
		set grp [string range $word 1 end]
		set prependSlash 1
	    } else {
		# no "/" in the object, just expand it with a "*"
		set grp $word
	    }
	    set matches [expand ${grp}*]
	    set len [llength $matches]
	    if { $len == 1 } {
		if [string equal "${grp}*" $matches] {
		    # expand will return the pattern if nothing matches
		    set newCommand $line
		} else {
		    # we have a unique expansion, so add it to the command line
		    if { $prependSlash } {
			set matches "/$matches"
		    }
		    set newCommand [lreplace $line end end $matches]
		}
	    } elseif { $len > 1 } {
		# multiple possible matches, find the longest common string
		set name [get_longest_common_string $matches]

		# add longest common string to the command line
		if { $prependSlash } {
		    set name "/$name"
		}
		set newCommand [lreplace $line end end $name]
	    } else {
		return [list $line {}]
	    }
	}
    } else {
	# command expansion
	set cmd [lindex $line 0]
	if { [string length $cmd] < 1 } {
	    # just a Tab on an empty line, don't show all commands, we have "?" for that
	    set newCommand $line
	} else {
	    set matches [lsearch -all -inline $mged_cmds "${cmd}*"]
	    set numMatches [llength $matches]
	    if { $numMatches == 0  } {
		# no matches
		set newCommand $line
	    } elseif { $numMatches > 1 } {
		# get longest match
		set newCommand [get_longest_common_string $matches]
	    } else {
		# just one match
		set newCommand $matches
	    }
	}
    }

    return [list $newCommand $matches]
}

proc do_windows_copy {_w} {
    catch {
	clipboard clear -displayof $_w;
	clipboard append -displayof $_w [selection get -displayof $_w]
    }
}

proc set_text_key_bindings { id } {
    global mged_gui
    global tcl_platform

    set w .$id.t
    switch $mged_gui($id,edit_style) {
	vi {
	    vi_insert_mode $w

	    bind $w <Left> {
		backward_char %W
		%W edit separator
		vi_edit_mode %W
		break
	    }

	    bind $w <Right> {
		forward_char %W
		%W edit separator
		vi_edit_mode %W
		break
	    }


	    bind $w <Control-d> {
		break
	    }

	    bind $w <Control-u> {
		delete_beginning_of_line %W
		break
	    }

	    bind $w <KP_Enter> {
		execute_cmd %W
		vi_insert_mode %W
		break
	    }

	    bind $w <Delete> {
		backward_delete_char %W
		break
	    }
	}
	default
	-
	emacs {
	    bind $w <Escape> {
		break
	    }

	    bind $w <Left> {
		backward_char %W
		break
	    }

	    bind $w <Right> {
		forward_char %W
		break
	    }

	    bind $w <Control-d> {
		delete_char %W
		break
	    }

	    bind $w <Control-u> {
		delete_line %W
		break
	    }

	    bind $w <BackSpace> {
		backward_delete_char %W
		break
	    }

	    # common misconception, the delete key actually performs a
	    # backwards delete by default in Emacs.
	    bind $w <Delete> {
		backward_delete_char %W
		break
	    }

	    bind $w <Return> {
		execute_cmd %W
		break
	    }

	    bind $w <KP_Enter> {
		execute_cmd %W
		break
	    }

	    bind $w <space> {}

	    bind $w <KeyPress> {}
	}
    }

    # Common Key Bindings
    bind $w <Control-a> "\
	if {\$mged_gui($id,edit_style) == \"vi\"} {\
	    first_char_in_line %W\
	} else {\
	    beginning_of_line %W\
	};\
	break"

    bind $w <Control-b> {
	backward_char %W
	break
    }

    if {$tcl_platform(platform) == "windows"} {
	bind $w <Control-c> "do_windows_copy $w; break"
    } else {
	bind $w <Control-c> "\
	interrupt_cmd %W;\
	if {\$mged_gui($id,edit_style) == \"vi\"} {\
	    vi_insert_mode %W\
	};\
	break"
    }

    bind $w <Control-e> {
	end_of_line %W
	break
    }

    bind $w <Control-f> {
	forward_char %W
	break
    }

    bind $w <Control-k> {
	delete_end_of_line %W
	break
    }

    bind $w <Control-n> {
	next_command %W
	break
    }

    bind $w <Control-o> {
	break
    }

    bind $w <Control-p> {
	prev_command %W
	break
    }

    bind $w <Control-t> {
	transpose %W
	break
    }

    bind $w <Control-w> {
	backward_delete_word %W
	break
    }

    bind $w <Up> {
	prev_command %W
	break
    }

    bind $w <Down> {
	next_command %W
	break
    }

    bind $w <Home> {
	beginning_of_line %W
	break
    }

    bind $w <End> {
	end_of_line %W
	break
    }

    bind $w <Meta-d> {
	if [%W compare insert < promptEnd] {
	    break
	}
	cursor_highlight %W
    }

    bind $w <Meta-BackSpace> {
	if [%W compare insert <= promptEnd] {
	    break
	}
	cursor_highlight %W
    }

    bind $w <Alt-Key> {
	::tk::TraverseToMenu %W %A
	break
    }

    bind $w <Tab> {
	set line [%W get -- promptEnd {promptEnd lineend -1c}]
	set results [tab_expansion $line]

	set expansions [lindex $results 1]
	if { [llength $expansions] > 1 } {
	    # show the possible matches
	    %W delete {insert linestart} {end-2c}
	    %W insert insert "\n${expansions}\n"
	    mged_print_prompt %W "mged> "
	}

	# display the expanded line
	%W delete promptEnd {end - 2c}
	%W mark set insert promptEnd
	%W insert insert [lindex $results 0]
	%W see insert

	break
    }

    # must override the Text bindings that move the cursor via
    # tk::TextSetCursor if we have not already so that we don't move
    # the input cursor off the command prompt.  These include the
    # following: <Left> <Right> <Up> <Down> <Control-Left>
    # <Control-Right> <Control-Up> <Control-Down> <Prior> <Next>
    # <Home> <End> <Control-Home> <Control-End> <Control-a>
    # <Control-b> <Control-e> <Control-f> <Control-n> <Control-p>
    # <Meta-b> <Meta-f> <Meta-less> <Meta-greater>

    bind $w <Prior> {
	tk::TextScrollPages %W -1
 	break
    }

    bind $w <Next> {
	tk::TextScrollPages %W 1
 	break
    }
}

proc set_text_button_bindings { w } {
    bind $w <1> {
	selection_begin %W %x %y
	break
    }

    bind $w <B1-Motion> {
	selection_add %W %x %y
	break
    }

    bind $w <Double-1> {
	select_word %W %x %y
	break
    }

    bind $w <Triple-1> {
	select_line %W %x %y
	break
    }

    bind $w <Shift-1> {
	selection_modify %W %x %y
	break
    }

    bind $w <Double-Shift-1> {
	break
    }

    bind $w <Triple-Shift-1> {
	break
    }

    bind $w <B1-Leave> {
	break
    }

    bind $w <B1-Enter> {
	break
    }

    bind $w <ButtonRelease-1> {
	break
    }

    bind $w <Control-1> {
	break
    }

    bind $w <ButtonRelease-2> {
	text_paste %W
	break
    }

    bind $w <2> {
	text_op_begin %W %x %y
	break
    }

    bind $w <B2-Motion> {
	text_scroll %W %x %y
	break
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
