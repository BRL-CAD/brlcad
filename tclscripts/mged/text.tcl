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

proc insert_char { c } {
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
	do_keypress $w $c
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

proc do_keypress { w c } {
    if {$c == "\r" || $c == "\n"} {
	do_return $w
    } elseif {$c == "\x01"} {
	do_ctrl_a $w
    } elseif {$c == "\x02"} {
	do_ctrl_b $w
    } elseif {$c == "\x03"} {
	do_ctrl_c $w
    } elseif {$c == "\x04"} {
	do_ctrl_d $w
    } elseif {$c == "\x05"} {
	do_ctrl_e $w
    } elseif {$c == "\x06"} {
	do_ctrl_f $w
    } elseif {$c == "\x08"} {
	do_backspace $w
    } elseif {$c == "\x0b"} {
	do_ctrl_k $w
    } elseif {$c == "\x0e"} {
	do_ctrl_n $w
    } elseif {$c == "\x10"} {
	do_ctrl_p $w
    } elseif {$c == "\x14"} {
	do_ctrl_t $w
    } elseif {$c == "\x15"} {
	do_ctrl_u $w
    } elseif {$c == "\x17"} {
	do_ctrl_w $w
    } elseif {$c == "\x7f"} {
	do_delete $w
    } else {
	$w insert insert $c
    }
}

proc do_return { w } {
    $w mark set insert {end - 2c}
    $w insert insert \n
    ia_invoke $w
    do_text_highlight $w
}

proc do_ctrl_a { w } {
    $w mark set insert promptEnd
    do_text_highlight $w
}

proc do_ctrl_b { w } {
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	do_text_highlight $w
    }
}

proc do_ctrl_c { w } {
    global ia_cmd_prefix
    global ia_more_default

    set id [get_player_id_t $w]
    set ia_cmd_prefix($id) ""
    set ia_more_default(id) ""
    $w insert insert \n
    ia_print_prompt $w "mged> "
    $w see insert
}

proc do_ctrl_d { w } {
    catch {$w tag remove sel sel.first promptEnd}
    if {[$w compare insert >= promptEnd] && [$w compare insert < {end - 2c}]} {
	$w delete insert
	do_text_highlight $w
    }
}

proc do_ctrl_e { w } {
    $w mark set insert {end - 2c}
    do_text_highlight $w
}

proc do_ctrl_f { w } {
    if [$w compare insert < {end - 2c}] {
	$w mark set insert {insert + 1c}
	do_text_highlight $w
    }
}

proc do_ctrl_k { w } {
    if [$w compare insert >= promptEnd] {
	$w delete insert {end - 2c}
	do_text_highlight $w
    }
}

proc do_ctrl_n { w } {
    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd
    set id [get_player_id_t $w]
    cmd_set $id
    set result [catch hist_next msg]
    if {$result==0} {
	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    do_text_highlight $w
}

proc do_ctrl_p { w } {
    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd
    set id [get_player_id_t $w]
    cmd_set $id
    set result [catch hist_prev msg]
    if {$result==0} {
	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    do_text_highlight $w
}

proc do_ctrl_t { w } {
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
    do_text_highlight $w
}

proc do_ctrl_u { w } {
    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd
    do_text_highlight $w
}

proc do_ctrl_w { w } {
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]+\[ \t\]*" insert promptEnd]
    if [string length $ti] {
	$w delete "$ti + 1c" insert
    } else {
	$w delete promptEnd insert
    }
    do_text_highlight $w
}

proc do_backspace { w } {
    catch {$w tag remove sel sel.first promptEnd}
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	$w delete insert
	do_text_highlight $w
    }
}

proc do_delete { w } {
    catch {$w tag remove sel sel.first promptEnd}
    if {[$w compare insert >= promptEnd] && [$w compare insert < {end - 2c}]} {
	$w delete insert
    }
    do_text_highlight $w
}

proc do_text_highlight { w } {
    $w tag delete hlt
    $w tag add hlt insert
    $w tag configure hlt -background yellow
}

proc reset_input_strings {} {
    set id [lindex [cmd_get] 2]
    do_ctrl_c .$id.t
}
