#                            T E X T . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1995 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	Utility routines called by MGED's Tcl/Tk command window(s).
#
# $Revision
#

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
		    mged_print_tag $_w "mged:$src_id> " prompt
		    mged_print_tag $_w $cmd\n oldcmd
		}

		if {$str != ""} {
		    mged_print_tag $_w $str\n result
		}

		$_w mark set insert curr
		$_w see insert
	    }
	}
    }
}

proc insert_char { c } {
    global mged_edit_style
    global dm_insert_char_flag

    set win [winset]
    set id [get_player_id_dm $win]

    if {$id == "mged"} {
	return "mged"
    } else {
	set dm_insert_char_flag 1
	set w .$id.t
	cmd_set $id
	winset $win

	switch $mged_edit_style($id) {
	    vi {
		vi_style $w $c
	    }
	    emacs {
		emacs_style $w $c
	    }
	}

	set dm_insert_char_flag 0
    }
}

proc get_player_id_dm { win } {
    global win_to_id

    if [info exists win_to_id($win)] {
	return $win_to_id($win)
    }

    return "mged"
}

proc vi_style { w c } {
    global vi_mode
    global vi_debug_char

    set vi_debug_char $c

    switch $vi_mode($w) {
	insert {
	    switch $c {
		"\r"
		    -
		"\n" {
		    execute_cmd $w
		}
		"\x01" {
		    break
		}
		"\x02" {
		    break
		}
		"\x03" {
		    break
		}
		"\x04" {
		    break
		}
		"\x05" {
		    break
		}
		"\x06" {
		    break
		}
		"\x08" {
		    break
		}
		"\x0b" {
		    break
		}
		"\x0e" {
		    break
		}
		"\x10" {
		    break
		}
		"\x14" {
		    break
		}
		"\x15" {
		    break
		}
		"\x17" {
		    break
		}
		"\x1b" {
		    vi_edit_mode $w
		    break
		}
		"\x7f" {
		    break
		}
		default {
		    $w insert insert $c
		}
	    }
	}
	edit {
	    switch $c {
		"\r"
		    -
		"\n" {
		    execute_cmd $w
		}
		default {
		    vi_process_edit_cmd $w $c 0
		}
	    }
	}
    }
}

proc emacs_style { w c } {
    switch $c {
	"\r"
	     -
	"\n" {
	    execute_cmd $w
	}
	"\x01" {
	    beginning_of_line $w
	}
	"\x02" {
	    backward_char $w
	}
	"\x03" {
	    interrupt_cmd $w
	}
	"\x04" {
	    delete_char $w
	}
	"\x05" {
	    end_of_line $w
	}
	"\x06" {
	    forward_char $w
	}
	"\x08" {
	    backward_delete_char $w
	}
	"\x0b" {
	    delete_end_of_line $w
	}
	"\x0e" {
	    next_command $w
	}
	"\x10" {
	    prev_command $w
	}
	"\x14" {
	    transpose $w
	}
	"\x15" {
	    delete_line $w
	}
	"\x17" {
	    backward_delete_word $w
	}
	"\x7f" {
	    delete_char $w
	}
	default {
	    $w insert insert $c
	}
    }
}

proc beginning_of_line { w } {
    $w mark set insert promptEnd
    cursor_highlight $w
}

proc end_of_line { w } {
    $w mark set insert {end - 2c}
    cursor_highlight $w
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
    catch {$w tag remove sel sel.first promptEnd}
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	$w delete insert
	cursor_highlight $w
    }
}

proc delete_char { w } {
    catch {$w tag remove sel sel.first promptEnd}
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
    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd
    cursor_highlight $w
}

proc delete_end_of_line { w } {
    if [$w compare insert >= promptEnd] {
	$w delete insert {end - 2c}
	cursor_highlight $w
    }
}

proc next_command { w } {
    global freshline
    global scratchline

    set id [get_player_id_t $w]
    cmd_set $id
    set result [catch hist_next msg]

    if {$result==0} {
	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd
	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]

	cursor_highlight $w
    } else {
	if {!$freshline($w)} {
	    $w delete promptEnd {end - 2c}
	    $w mark set insert promptEnd
	    $w insert insert [string range $scratchline($w) 0\
		    [expr [string length $scratchline($w)] - 1]]
	    set freshline($w) 1
	    cursor_highlight $w
	}
    }
}

proc prev_command { w } {
    global freshline
    global scratchline

    set id [get_player_id_t $w]
    cmd_set $id
    set result [catch hist_prev msg]

    if {$result==0} {
	if {$freshline($w)} {
	    set scratchline($w) [$w get promptEnd {end -2c}]
	    set freshline($w) 0
	}

	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd

	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]

	cursor_highlight $w
    }
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

proc execute_cmd { w } {
    global freshline

    $w mark set insert {end - 2c}
    $w insert insert \n
    ia_invoke $w
    set freshline($w) 1
    cursor_highlight $w
}

proc interrupt_cmd { w } {
    global ia_cmd_prefix
    global ia_more_default

    set id [get_player_id_t $w]
    set ia_cmd_prefix($id) ""
    set ia_more_default(id) ""
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
    global vi_mode

    set vi_mode($w) edit
    bind $w <KeyPress> {
	vi_process_edit_cmd %W %K 1
	break
    }
    bind $w <Delete> {}
    bind $w <BackSpace> {}
}

proc vi_insert_mode { w } {
    global vi_mode

    set vi_mode($w) insert
    bind $w <KeyPress> {}
    bind $w <Delete> { break }
    bind $w <BackSpace> { break }
}

proc vi_process_edit_cmd { w c iskeysym } {
    global vi_delete_flag
    global vi_debug

    set vi_debug($w) $c

    switch $c {
	0 {
	    beginning_of_line $w
	    set vi_delete_flag($w) 0
	}
	a {
	    forward_char $w
	    vi_insert_mode $w
	    set vi_delete_flag($w) 0
	}
	b {
	    if {$vi_delete_flag($w)} {
		backward_delete_word $w
	    } else {
		backward_word $w
	    }
	    set vi_delete_flag($w) 0
	}
	d {
	    if { $vi_delete_flag($w)} {
		delete_line $w
		set vi_delete_flag($w) 0
	    } else {
		set vi_delete_flag($w) 1
	    }
	}
	e {
	    if {$vi_delete_flag($w)} {
		delete_end_word $w
	    } else {
		end_word $w
	    }
	    set vi_delete_flag($w) 0
	}
	h {
	    backward_char $w
	    set vi_delete_flag($w) 0
	}
	i {
	    vi_insert_mode $w
	    set vi_delete_flag($w) 0
	}
	j {
	    next_command $w
	    set vi_delete_flag($w) 0
	}
	k {
	    prev_command $w
	    set vi_delete_flag($w) 0
	}
	l {
	    forward_char $w
	    set vi_delete_flag($w) 0
	}
	w {
	    if {$vi_delete_flag($w)} {
		delete_word $w
	    } else {
		forward_word $w
	    }
	    set vi_delete_flag($w) 0
	}
	x {
	    delete_char $w
	    set vi_delete_flag($w) 0
	}
	A {
	    end_of_line $w
	    vi_insert_mode $w
	    set vi_delete_flag($w) 0
	}
	D {
	    delete_end_of_line $w
	    set vi_delete_flag($w) 0
	}
	I {
	    beginning_of_line $w
	    vi_insert_mode $w
	    set vi_delete_flag($w) 0
	}
	X {
	    backward_delete_char $w
	    set vi_delete_flag($w) 0
	}
	default {
	    if { $iskeysym } {
		switch $c {
		    BackSpace {
			backward_delete_char $w
		    }
		    Delete {
			delete_char $w
		    }
		    dollar {
			end_of_line $w
		    }
		}
	    } else {
		switch $c {
		    "\b" {
			backward_delete_char $w
		    }
		    "\x7f" {
			delete_char $w
		    }
		    $ {
			end_of_line $w
		    }
		}
	    }

	    set vi_delete_flag($w) 0
	}
    }
}
# End - VI Specific Callbacks


proc text_op_begin { w x y } {
    global moveView

    set moveView($w) 0
    $w scan mark $x $y
}

proc text_paste { w } {
    global moveView

    if {!$moveView($w)} {
	catch {$w insert insert [selection get -displayof $w]}
    }

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

proc text_scroll { w x y } {
    global moveView

    set moveView($w) 1
    $w scan dragto $x $y
}

proc selection_begin { w x y } {
    $w mark set anchor [tkTextClosestGap $w $x $y]
    $w tag remove sel 0.0 end

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

proc selection_add { w x y } {
    set cur [tkTextClosestGap $w $x $y]

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
    set cur [tkTextClosestGap $w $x $y]

    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }

    if [$w compare $cur < anchor] {
	set first [tkTextPrevPos $w "$cur + 1c" tcl_wordBreakBefore]
	set last [tkTextNextPos $w "anchor" tcl_wordBreakAfter]
    } else {
	set first [tkTextPrevPos $w anchor tcl_wordBreakBefore]
	set last [tkTextNextPos $w "$cur - 1c" tcl_wordBreakAfter]
    }

    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

proc select_line { w x y } {
    set cur [tkTextClosestGap $w $x $y]

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
    tkTextResetAnchor $w @$x,$y
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

proc set_text_key_bindings { id } {
    global mged_edit_style
    global vi_mode

    set w .$id.t
    switch $mged_edit_style($id) {
	vi {
	    vi_insert_mode $w

	    bind $w <Escape> {
		vi_edit_mode %W
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

	    bind $w <Up> {
		break
	    }

	    bind $w <Down> {
		break
	    }

	    bind $w <Control-a> {
		break
	    }

	    bind $w <Control-b> {
		break
	    }

	    bind $w <Control-c> {
		break
	    }

	    bind $w <Control-d> {
		break
	    }

	    bind $w <Control-e> {
		break
	    }

	    bind $w <Control-f> {
		break
	    }

	    bind $w <Control-k> {
		break
	    }

	    bind $w <Control-n> {
		break
	    }

	    bind $w <Control-p> {
		break
	    }

	    bind $w <Control-t> {
		break
	    }

	    bind $w <Control-u> {
		break
	    }

	    bind $w <Control-w> {
		break
	    }

	    bind $w <Meta-d> {
		break
	    }

	    bind $w <Meta-BackSpace> {
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

	    bind $w <Up> {
		break
	    }

	    bind $w <Down> {
		break
	    }

	    bind $w <Control-a> {
		beginning_of_line %W
		break
	    }

	    bind $w <Control-b> {
		backward_char %W
		break
	    }

	    bind $w <Control-c> {
		interrupt_cmd %W
		break
	    }

	    bind $w <Control-d> {
		delete_char %W
		break
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

	    bind $w <Control-p> {
		prev_command %W
		break
	    }

	    bind $w <Control-t> {
		transpose %W
		break
	    }

	    bind $w <Control-u> {
		delete_line %W
		break
	    }

	    bind $w <Control-w> {
		backward_delete_word %W
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

	    bind $w <Delete> {
		delete_char %W
		break
	    }

	    bind $w <BackSpace> {
		backward_delete_char %W
		break
	    }
	}
    }

# Common Key Bindings
    bind $w <Return> {
	execute_cmd %W
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
