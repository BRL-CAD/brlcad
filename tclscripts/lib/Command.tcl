#                 C O M M A N D . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The Command class implements a command window with command line
#       editing and command history.
#
class Command {
    inherit iwidgets::Scrolledtext

    constructor {args} {}
    destructor {}

    itk_option define -edit_style edit_style Edit_style emacs
    itk_option define -prompt prompt Prompt "> "
    itk_option define -prompt2 prompt2 Prompt ""
    itk_option define -cmd_prefix cmd_prefix Cmd_prefix ""
    itk_option define -selection_color selection_color TextColor #fefe8e
    itk_option define -prompt_color prompt_color TextColor red1
    itk_option define -cmd_color cmd_color TextColor black
    itk_option define -oldcmd_color oldcmd_color TextColor red3
    itk_option define -result_color result_color TextColor blue3
    itk_option define -maxlines maxlines MaxLines 1000

    public method history {}
    public method edit_style {args}
    public method putstring {str}

    private method invoke {}
    private method first_char_in_line {}
    private method beginning_of_line {}
    private method end_of_line {}
    private method backward_char {}
    private method forward_char {}
    private method backward_word {}
    private method forward_word {}
    private method end_word {}
    private method backward_delete_char {}
    private method delete_char {}
    private method backward_delete_word {}
    private method delete_word {}
    private method delete_end_word {}
    private method delete_line {}
    private method delete_end_of_line {}
    private method delete_beginning_of_line {}
    private method next {}
    private method prev {}
    private method transpose {}
    private method execute {}
    private method interrupt {}
    private method vi_edit_mode {}
    private method vi_overwrite_mode {}
    private method vi_insert_mode {}
    private method vi_process_edit {c state}
    private method vi_process_overwrite {c state}
    private method text_op_begin {x y}
    private method text_paste {}
    private method text_scroll {x y}
    private method selection_begin {x y}
    private method selection_add {x y}
    private method select_word {x y}
    private method select_line {x y}
    private method selection_modify {x y}
    private method print {str}
    private method print_prompt {}
    private method print_prompt2 {}
    private method print_tag {str tag}
    private method cursor_highlight {}

    private method doBindings {}
    private method doButtonBindings {}
    private method doKeyBindings {}
    private method doControl_a {}
    private method doControl_c {}
    private method doMeta_d {}
    private method doMeta_BackSpace {}
    private method doReturn {}
    private method doLeft {}
    private method doRight {}

    private variable hist ""
    private variable cmdlist ""
    private variable scratchline ""
    private variable moveView 0
    private variable freshline 1

    private variable overwrite_flag 0
    private variable change_flag 0
    private variable delete_flag 0
    private variable search_flag 0
    private variable search_char ""
    private variable search_dir ""
}

configbody Command::edit_style {
    edit_style $itk_option(-edit_style)
}

configbody Command::cmd_prefix {
    if {$itk_option(-cmd_prefix) == ""} {
	set cmdlist ""
	return
    }

    # if getUserCmds doesn't exist, use all recognized functions
    if {[catch {eval $itk_option(-cmd_prefix) info function getUserCmds}]} {
	if {[catch {eval $itk_option(-cmd_prefix) info function} _cmdlist]} {
	    error "Bad command prefix: no related functions"
	}

	# strip off namespace
	set cmdlist ""
	foreach cmd $_cmdlist {
	    lappend cmdlist [namespace tail $cmd]
	}
    } else {
	set cmdlist [eval $itk_option(-cmd_prefix) getUserCmds]
    }

}

configbody Command::selection_color {
	$itk_component(text) tag configure sel -foreground $itk_option(-selection_color)
}

configbody Command::prompt_color {
	$itk_component(text) tag configure prompt -foreground $itk_option(-prompt_color)
}

configbody Command::cmd_color {
	$itk_component(text) tag configure cmd -foreground $itk_option(-cmd_color)
}

configbody Command::oldcmd_color {
	$itk_component(text) tag configure oldcmd -foreground $itk_option(-oldcmd_color)
}

configbody Command::result_color {
	$itk_component(text) tag configure result -foreground $itk_option(-result_color)
}

configbody Command::maxlines {
    if {$itk_option(-maxlines) < 1} {
	error "-maxlines must be greater than zero"
    }
}

body Command::constructor {args} {
	eval itk_initialize $args

	doBindings

	# create command history object
	set hist [string map {:: "" . _} $this]
	set hist [ch_open [subst $hist]_hist]

	# initialize text widget
	print_prompt
	$itk_component(text) insert insert " "
	beginning_of_line

	$itk_component(text) tag configure sel -background $itk_option(-selection_color)
	$itk_component(text) tag configure prompt -foreground $itk_option(-prompt_color)
	$itk_component(text) tag configure cmd -foreground $itk_option(-cmd_color)
	$itk_component(text) tag configure oldcmd -foreground $itk_option(-oldcmd_color)
	$itk_component(text) tag configure result -foreground $itk_option(-result_color)
}

body Command::destructor {} {
    # destroy command history object
    rename $hist ""
}


############################## Public Methods ##############################
body Command::history {} {
    eval $hist history
}

body Command::edit_style {args} {
    if {$args == ""} {
	return $itk_option(-edit_style)
    }

    switch $args {
	emacs -
	vi {
	    set itk_option(-edit_style) $args
	    doKeyBindings
	}
	default {
	    error "Bad edit_style - $args"
	}
    }
}

body Command::putstring {str} {
    set w $itk_component(text)
    set promptBegin [$w index {end - 1 l}]
    $w mark set curr insert
    $w mark set insert $promptBegin

    if {$str != ""} {
	if {[string index $str end] == "\n"} {
	    print_tag $str result
	} else {
	    print_tag $str\n result
	}
    }

    $w mark set insert curr
    $w see insert

    # get rid of oldest output
    set nlines [expr int([$w index end])]
    if {$nlines > $itk_option(-maxlines)} {
	$w delete 1.0 [expr $nlines - $itk_option(-maxlines)].end
    }
}

############################## Protected/Private Methods  ##############################

body Command::invoke {} {
    set w $itk_component(text)

    set cmd [$w get promptEnd insert]

    # remove any instances of prompt2 from the beginning of each secondary line
    regsub -all "\n$itk_option(-prompt2)" $cmd "" cmd
    
    set hcmd $cmd

    if {$itk_option(-cmd_prefix) != ""} {
	# get command name
	if {[regexp {^[ \t]*([^\]\[ \t\n\r{}]+)} $cmd match cname]} {
	    set cindex [lsearch -exact $cmdlist $cname]
	    if {$cindex != -1} {
		# found command name in cmdlist, so prepend cmd_prefix to cmd
		set cmd [concat $itk_option(-cmd_prefix) $cmd]
	    }
	}
    }

    if [info complete $cmd] {
	set result [catch {uplevel #0 $cmd} msg]

	if {$result != 0} {
	    $w tag add oldcmd promptEnd insert
	    print_tag "Error: $msg\n" result
	} else {
	    $w tag add oldcmd promptEnd insert

	    if {$msg != ""} {
		print_tag $msg\n result
	    }
	}

	$hist add $hcmd
	print_prompt

	# get rid of oldest output
	set nlines [expr int([$w index end])]
	if {$nlines > $itk_option(-maxlines)} {
	    $w delete 1.0 [expr $nlines - $itk_option(-maxlines)].end
	}
    } else {
	print_prompt2
    }
    $w see insert
}

body Command::first_char_in_line {} {
    set w $itk_component(text)
    $w mark set insert promptEnd
    set c [$w get insert]
    if {$c == " "} {
	forward_word
    }
    cursor_highlight
}

body Command::beginning_of_line {} {
    set w $itk_component(text)
    $w mark set insert promptEnd
    cursor_highlight
}

body Command::end_of_line {} {
    set w $itk_component(text)
    $w mark set insert {end - 2c}
    cursor_highlight
    $w see insert
}

body Command::backward_char {} {
    set w $itk_component(text)
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	cursor_highlight
    }
}

body Command::forward_char {} {
    set w $itk_component(text)
    if [$w compare insert < {end - 2c}] {
	$w mark set insert {insert + 1c}
	cursor_highlight
    }
}

body Command::backward_word {} {
    set w $itk_component(text)
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
    } else {
	$w mark set insert promptEnd
    }

    cursor_highlight
}

body Command::forward_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" insert {end - 2c}]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
	cursor_highlight
    }
}

body Command::end_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w mark set insert $ti
    } else {
	$w mark set insert {end - 2c}
    }

    cursor_highlight
}

body Command::backward_delete_char {} {
    set w $itk_component(text)
#    catch {$w tag remove sel sel.first promptEnd}
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	$w delete insert
	cursor_highlight
    }
}

body Command::delete_char {} {
    set w $itk_component(text)
#    catch {$w tag remove sel sel.first promptEnd}
    if {[$w compare insert >= promptEnd] && [$w compare insert < {end - 2c}]} {
	$w delete insert
	cursor_highlight
    }
}

body Command::backward_delete_word {} {
    set w $itk_component(text)
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w delete "$ti + 1c" insert
    } else {
	$w delete promptEnd insert
    }
    cursor_highlight
}

body Command::delete_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight
}

body Command::delete_end_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight
}

body Command::delete_line {} {
    set w $itk_component(text)
    $w delete promptEnd end-2c
    cursor_highlight
}

body Command::delete_end_of_line {} {
    set w $itk_component(text)
    $w delete insert end-2c
    cursor_highlight
}

body Command::delete_beginning_of_line {} {
    set w $itk_component(text)
    $w delete promptEnd insert
    cursor_highlight
}

body Command::next {} {
    set w $itk_component(text)
    set result [catch {$hist next} msg]

    if {$result==0} {
	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd
	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]

	cursor_highlight
	$w see insert
    } else {
	if {!$freshline} {
	    $w delete promptEnd {end - 2c}
	    $w mark set insert promptEnd
	    $w insert insert [string range $scratchline 0\
		    [expr [string length $scratchline] - 1]]
	    set freshline 1
	    cursor_highlight
	}
    }
}

body Command::prev {} {
    set w $itk_component(text)
    set result [catch {$hist prev} msg]

    if {$result==0} {
	if {$freshline} {
	    set scratchline [$w get promptEnd {end -2c}]
	    set freshline 0
	}

	$w delete promptEnd {end - 2c}
	$w mark set insert promptEnd

	$w insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]

	cursor_highlight
	$w see insert
    }
}

body Command::transpose {} {
    set w $itk_component(text)
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
    cursor_highlight
}

body Command::execute {} {
    set w $itk_component(text)
    $w mark set insert {end - 2c}
    $w insert insert \n

    $w see insert
    update

    invoke
    set freshline 1
    cursor_highlight
}

body Command::interrupt {} {
    set w $itk_component(text)

    $w insert insert \n
    print_prompt
    $w see insert
}

##################################################################################
#                                                                                #
#                        VI Specific Callbacks                                   #
#                                                                                #
##################################################################################
body Command::vi_edit_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[code $this backward_char]; break"
    bind $w <space> "[code $this forward_char]; break"
    bind $w <KeyPress> "[code $this vi_process_edit %A %s]; break"
}

body Command::vi_overwrite_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[code $this backward_delete_char]; break"
    bind $w <space> "[code $this delete_char]; %W insert insert %A; break"
    bind $w <KeyPress> "[code $this vi_process_overwrite %A %s]; break"
}

body Command::vi_insert_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[code $this backward_delete_char]; break"
    bind $w <space> {}
    bind $w <KeyPress> {}
}

body Command::vi_process_edit {c state} {
    set w $itk_component(text)

    # Throw away all non-visible characters
    if {![string match \[!-~\] $c] || $state > 1} {
	return
    }

    if {$overwrite_flag} {
	delete_char
	$w insert insert $c
	set overwrite_flag 0

	return
    }

    switch $search_flag {
	f {
	    set search_dir forward
	    set search_char $c
	    set newindex [$w search $c {insert + 1c} {end - 2c}]
	    if {$newindex != ""} {
		if {$delete_flag} {
		    $w delete insert $newindex+1c
		} elseif {$change_flag} {
		    $w delete insert $newindex+1c
		    vi_insert_mode
		} else {
		    $w mark set insert $newindex
		}

		cursor_highlight
	    }

	    set delete_flag 0
	    set change_flag 0
	    set search_flag 0

	    return
	}
	F {
	    set search_dir backward
	    set search_char $c
	    set newindex [$w search -backwards $c {insert - 1c} promptEnd]
	    if {$newindex != ""} {
		if {$delete_flag} {
		    $w delete $newindex insert
		} elseif {$change_flag} {
		    $w delete $newindex insert
		    vi_insert_mode
		} else {
		    $w mark set insert $newindex
		}

		cursor_highlight
	    }

	    set delete_flag 0
	    set change_flag 0
	    set search_flag 0

	    return
	}
    }

    switch $c {
	; {
	    set delete_flag 0
	    if {$search_char == ""} {
		return
	    }

	    switch $search_dir {
		forward {
		    set newindex [$w search $search_char {insert + 1c} {end - 2c}]
		    if {$newindex != ""} {
			if {$delete_flag} {
			    $w delete insert $newindex+1c
			    set delete_flag 0
			} elseif {$change_flag} {
			    $w delete insert $newindex+1c
			    vi_insert_mode
			    set change_flag 0
			} else {
			    $w mark set insert $newindex
			}
			cursor_highlight
		    }
		}
		backward {
		    set newindex [$w search -backwards $search_char {insert - 1c} promptEnd]
		    if {$newindex != ""} {
			if {$delete_flag} {
			    $w delete $newindex insert
			    set delete_flag 0
			} elseif {$change_flag} {
			    $w delete $newindex insert
			    vi_insert_mode
			    set change_flag 0
			} else {
			    $w mark set insert $newindex
			}
			cursor_highlight
		    }
		}
	    }
	}
	, {
	    set delete_flag 0
	    if {$search_char == ""} {
		return
	    }

	    switch $search_dir {
		backward {
		    set newindex [$w search $search_char {insert + 1c} {end - 2c}]
		    if {$newindex != ""} {
			if {$delete_flag} {
			    $w delete insert $newindex+1c
			    set delete_flag 0
			} elseif {$change_flag} {
			    $w delete insert $newindex+1c
			    vi_insert_mode
			    set change_flag 0
			} else {
			    $w mark set insert $newindex
			}
			cursor_highlight
		    }
		}
		forward {
		    set newindex [$w search -backwards $search_char {insert - 1c} promptEnd]
		    if {$newindex != ""} {
			if {$delete_flag} {
			    $w delete $newindex insert
			    set delete_flag 0
			} elseif {$change_flag} {
			    $w delete $newindex insert
			    vi_insert_mode
			    set change_flag 0
			} else {
			    $w mark set insert $newindex
			}
			cursor_highlight
		    }
		}
	    }
	}
	0 {
	    if {$delete_flag} {
		delete_beginning_of_line
		set delete_flag 0
	    } elseif {$change_flag} {
		delete_beginning_of_line
		vi_insert_mode
		set change_flag 0
	    } else {
		beginning_of_line
	    }
	}
	a {
	    forward_char
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	b {
	    if {$delete_flag} {
		backward_delete_word
		set delete_flag 0
	    } elseif {$change_flag} {
		backward_delete_word
		vi_insert_mode
		set change_flag 0
	    } else {
		backward_word
	    }
	}
	c {
	    if {$change_flag} {
		delete_line
		vi_insert_mode
		set change_flag 0
	    } else {
		set change_flag 1
	    }
	    set delete_flag 0
	}
	d {
	    if {$delete_flag} {
		delete_line
		set delete_flag 0
	    } else {
		set delete_flag 1
	    }
	    set change_flag 0
	}
	e {
	    if {$delete_flag} {
		delete_end_word
	    } elseif {$change_flag} {
		delete_end_word
		vi_insert_mode
		set change_flag 0
	    } else {
		end_word
	    }
	    set delete_flag 0
	}
	f {
	    set search_flag f
	}
	h {
	    if {$delete_flag} {
		backward_delete_char
		set delete_flag 0
	    } elseif {$change_flag} {
		backward_delete_char
		vi_insert_mode
		set change_flag 0
	    } else {
		backward_char
	    }
	}
	i {
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	j {
	    next
	    set delete_flag 0
	    set change_flag 0
	}
	k {
	    prev
	    set delete_flag 0
	    set change_flag 0
	}
	l {
	    if {$delete_flag} {
		delete_char
		set delete_flag 0
	    } elseif {$change_flag} {
		delete_char
		vi_insert_mode
		set change_flag 0
	    } else {
		forward_char
	    }
	}
	r {
	    set overwrite_flag 1
	    set delete_flag 0
	    set change_flag 0
	}
	s {
	    delete_char
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	w {
	    if {$delete_flag} {
		delete_word
		set delete_flag 0
	    } elseif {$change_flag} {
		delete_word
		vi_insert_mode
		set change_flag 0
	    } else {
		forward_word
	    }
	}
	x {
	    delete_char
	    set delete_flag 0
	    set change_flag 0
	}
	A {
	    end_of_line
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	C {
	    delete_end_of_line
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	D {
	    delete_end_of_line
	    set delete_flag 0
	    set change_flag 0
	}
	F {
	    set search_flag F
	}
	I {
	    beginning_of_line
	    vi_insert_mode
	    set delete_flag 0
	    set change_flag 0
	}
	R {
	    vi_overwrite_mode
	    set delete_flag 0
	    set change_flag 0
	}
	X {
	    backward_delete_char
	    set delete_flag 0
	    set change_flag 0
	}
	$ {
	    if {$delete_flag} {
		delete_end_of_line
		set delete_flag 0
	    } elseif {$change_flag} {
		delete_end_of_line
		vi_insert_mode
		set change_flag 0
	    } else {
		end_of_line
	    }
	}
	default {
	    set delete_flag 0
	    set change_flag 0
	}
    }
}

body Command::vi_process_overwrite {c state} {
    set w $itk_component(text)
    # Throw away all non-visible characters
    if {![string match \[!-~\] $c] || $state > 1} {
	return
    }

    delete_char
    $w insert insert $c
}
# End - VI Specific Callbacks


body Command::text_op_begin {x y} {
    set w $itk_component(text)
    global mged_gui

    set moveView 0
    $w scan mark $x $y
}

body Command::text_paste {} {
    set w $itk_component(text)

    if {!$moveView} {
	catch {$w insert insert [selection get -displayof $w]}
	$w see insert
    }

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

body Command::text_scroll {x y} {
    set w $itk_component(text)

    set moveView 1
    $w scan dragto $x $y
}

body Command::selection_begin {x y} {
    set w $itk_component(text)
    $w mark set anchor [tkTextClosestGap $w $x $y]
    $w tag remove sel 0.0 end

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

body Command::selection_add {x y} {
    set w $itk_component(text)
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

body Command::select_word {x y} {
    set w $itk_component(text)
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

body Command::select_line {x y} {
    set w $itk_component(text)
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

body Command::selection_modify {x y} {
    set w $itk_component(text)
    tkTextResetAnchor $w @$x,$y
    selection_add $x $y
}

body Command::print {str} {
    set w $itk_component(text)
    $w insert insert $str
}

body Command::print_prompt {} {
    set w $itk_component(text)
    print_tag $itk_option(-prompt) prompt
    $w mark set promptEnd insert
    $w mark gravity promptEnd left
}

body Command::print_prompt2 {} {
    set w $itk_component(text)
    $w insert insert $itk_option(-prompt2)
}

body Command::print_tag {str tag} {
    set w $itk_component(text)
    set first [$w index insert]
    $w insert insert $str
    set last [$w index insert]
    $w tag add $tag $first $last
}

body Command::cursor_highlight {} {
    set w $itk_component(text)
    $w tag delete hlt
    $w tag add hlt insert
    $w tag configure hlt -background yellow
}

body Command::doBindings {} {
    set w $itk_component(text)
    bind $w <Enter> "focus $w; break"

    doKeyBindings
    doButtonBindings
}

body Command::doKeyBindings {} {
    set w $itk_component(text)
    switch $itk_option(-edit_style) {
	vi {
	    vi_insert_mode

	    bind $w <Escape> "[code $this vi_edit_mode]; break"
	    bind $w <Control-d> "break"
	    bind $w <Control-u> "[code $this delete_beginning_of_line]; break"
	}
	default
	    -
	emacs {
	    bind $w <Escape> "break"
	    bind $w <Control-d> "[code $this delete_char]; break"
	    bind $w <Control-u> "[code $this delete_line]; break"
	    bind $w <BackSpace> "[code $this backward_delete_char]; break"
	    bind $w <space> {}
	    bind $w <KeyPress> {}
	}
    }

# Common Key Bindings
    bind $w <Return> "[code $this doReturn]; break"
    bind $w <KP_Enter> "[code $this doReturn]; break"
    bind $w <Delete> "[code $this backward_delete_char]; break"
    bind $w <Left> "[code $this doLeft]; break"
    bind $w <Right> "[code $this doRight]; break"
    bind $w <Control-a> "[code $this doControl_a]; break"
    bind $w <Control-b> "[code $this backward_char]; break"
    bind $w <Control-c> "[code $this doControl_c]; break"
    bind $w <Control-e> "[code $this end_of_line]; break"
    bind $w <Control-f> "[code $this forward_char]; break"
    bind $w <Control-k> "[code $this delete_end_of_line]; break"
    bind $w <Control-n> "[code $this next]; break"
    bind $w <Control-o> "break"
    bind $w <Control-p> "[code $this prev]; break"
    bind $w <Control-t> "[code $this transpose]; break"
    bind $w <Control-w> "[code $this backward_delete_word]; break"
    bind $w <Up> "[code $this prev]; break"
    bind $w <Down> "[code $this next]; break"
    bind $w <Home> "[code $this beginning_of_line]; break"
    bind $w <End> "[code $this end_of_line]; break"
    bind $w <Meta-d> "[code $this doMeta_d]; break"
    bind $w <Meta-BackSpace> "[code $this doMeta_BackSpace]; break"

    bind $w <Alt-Key> {
	tkTraverseToMenu %W %A
	break
    }
}

body Command::doButtonBindings {} {
    set w $itk_component(text)
    bind $w <1> "[code $this selection_begin %x %y]; break"
    bind $w <B1-Motion> "[code $this selection_add %x %y]; break"
    bind $w <Double-1> "[code $this select_word %x %y]; break"
    bind $w <Triple-1> "[code $this select_line %x %y]; break"
    bind $w <Shift-1> "[code $this selection_modify %x %y]; break"

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

    bind $w <ButtonRelease-2> "[code $this text_paste]; break"
    bind $w <2> "[code $this text_op_begin %x %y]; break"
    bind $w <B2-Motion> "[code $this text_scroll %x %y]; break"
}

body Command::doControl_a {} {
    if {$itk_option(-edit_style) == "vi"} {
	first_char_in_line
    } else {
	beginning_of_line
    }
}

body Command::doControl_c {} {
    interrupt
    if {$itk_option(-edit_style) == "vi"} {
	vi_insert_mode
    }
}

body Command::doMeta_d {} {
    if [%W compare insert < promptEnd] {
	break
    }
    cursor_highlight
}

body Command::doMeta_BackSpace {} {
    if [%W compare insert <= promptEnd] {
	break
    }
    cursor_highlight
}

body Command::doReturn {} {
    execute
    if {$itk_option(-edit_style) == "vi"} {
	vi_insert_mode
    }
}

body Command::doLeft {} {
    backward_char
    if {$itk_option(-edit_style) == "vi"} {
	vi_edit_mode
    }
}

body Command::doRight {} {
    forward_char
    if {$itk_option(-edit_style) == "vi"} {
	vi_edit_mode
    }
}
