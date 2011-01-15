#                     C O M M A N D . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
#	The Command class implements a command window with command line
#       editing and command history.
#
::itcl::class Command {
    inherit ::iwidgets::Scrolledtext

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

    public method clear {}
    public method history {}
    public method edit_style {args}
    private method do_get_more_args {}
    public method get_more_args {}
    public method putstring {str}
    public method reinitialize {} 

    private method invoke {}
    private method invokeMaster {hcmd}
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
    private method get_longest_common_string {matches}
    private method tab_completion {}
    private method tab_expansion {line}
    private method print {str}
    public method print_more_args_prompt {_prompt}
    private method print_prompt {}
    private method print_prompt2 {}
    private method print_tag {str tag}
    private method cursor_highlight {}

    private method doBindings {}
    private method doButtonBindings {}
    private method doCopy {}
    private method doKeyBindings {}
    private method doControl_a {}
    private method doControl_c {}
    private method doMeta_d {}
    private method doMeta_BackSpace {}
    private method doReturn {}
    private method doLeft {}
    private method doRight {}

    private variable slaveInterp
    private variable hist ""
    private variable cmdlist ""
    private variable scratchline ""
    private variable moveView 0
    private variable freshline 1
    private variable do_history 0

    private variable overwrite_flag 0
    private variable change_flag 0
    private variable delete_flag 0
    private variable search_flag 0
    private variable search_char ""
    private variable search_dir ""

    private variable more_args_interrupted 0
    private variable more_args_list {}
    private variable more_args_var ""
    private variable more_args_begin_index ""
    private variable more_args_end_index ""
}

::itcl::configbody Command::edit_style {
    edit_style $itk_option(-edit_style)
}

::itcl::configbody Command::cmd_prefix {
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

    # Create new slave interp
    interp delete $slaveInterp
    if { [catch {set slaveInterp [interp create]} err] } {
	error "Unable to reinitalize a slave interpreter"
    }

    # Create slave interp's aliases
    foreach cmd $cmdlist {
	eval $slaveInterp alias $cmd $itk_option(-cmd_prefix) $cmd
    }
}

::itcl::configbody Command::selection_color {
    $itk_component(text) tag configure sel -foreground $itk_option(-selection_color)
}

::itcl::configbody Command::prompt_color {
    $itk_component(text) tag configure prompt -foreground $itk_option(-prompt_color)
}

::itcl::configbody Command::cmd_color {
    $itk_component(text) tag configure cmd -foreground $itk_option(-cmd_color)
}

::itcl::configbody Command::oldcmd_color {
    $itk_component(text) tag configure oldcmd -foreground $itk_option(-oldcmd_color)
}

::itcl::configbody Command::result_color {
    $itk_component(text) tag configure result -foreground $itk_option(-result_color)
}

::itcl::configbody Command::maxlines {
    if {$itk_option(-maxlines) < 1} {
	error "-maxlines must be greater than zero"
    }
}

::itcl::body Command::constructor {args} {
    eval itk_initialize $args

    # thar be dragons here
    if { [catch {set slaveInterp [interp create]} err] } {
	error "Unable to initalize a slave interpreter"
    }

    doBindings

    # create command history object
    if {[info command ch_open] != ""} {
	set do_history 1
	set hist [string map {:: "" . _} $this]
	set hist [ch_open [subst $hist]_hist]
    }

    # initialize text widget
    print_prompt
    $itk_component(text) insert insert " "
    beginning_of_line

    $itk_component(text) tag configure sel -background $itk_option(-selection_color)
    $itk_component(text) tag configure prompt -foreground $itk_option(-prompt_color)
    $itk_component(text) tag configure cmd -foreground $itk_option(-cmd_color)
    $itk_component(text) tag configure oldcmd -foreground $itk_option(-oldcmd_color)
    $itk_component(text) tag configure result -foreground $itk_option(-result_color)

#    if {[catch {info body ::gets}] == 1} {
#        rename ::gets ::tcl_gets
#    }
}

::itcl::body Command::destructor {} {
    interp delete $slaveInterp

    # destroy command history object
    rename $hist ""
}


############################## Public Methods ##############################
::itcl::body Command::clear {} {
    set w $itk_component(text)
    $w delete 1.0 end
    $w insert insert " "
    beginning_of_line
    $w edit reset
}

::itcl::body Command::history {} {
    eval $hist history
}

::itcl::body Command::edit_style {args} {
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

::itcl::body Command::do_get_more_args {} {
    set w $itk_component(text)
    set more_args_end_index [$itk_component(text) index insert]
    $w mark set insert {end - 2c}
    $w insert insert \n

    $w see insert
    update

    set more_args_var [$w get $more_args_begin_index $more_args_end_index]
}

::itcl::body Command::get_more_args {} {
    set more_args_interrupted 0
    set more_args_var ""
    set more_args_begin_index [$itk_component(text) index insert]
    set w $itk_component(text)
    bind $w <Return> "[::itcl::code $this do_get_more_args]; break"
    bind $w <KP_Enter> "[::itcl::code $this do_get_more_args]; break"
    vwait [::itcl::scope more_args_var]
    bind $w <Return> "[::itcl::code $this doReturn]; break"
    bind $w <KP_Enter> "[::itcl::code $this doReturn]; break"

    if {$more_args_interrupted} {
	error ""
    } else {
	eval lappend more_args_list $more_args_var
	return $more_args_var
    }
}

::itcl::body Command::putstring {str} {
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

::itcl::body Command::reinitialize {} {
    $itk_component(text) delete 1.0 end
    print_prompt
    $itk_component(text) insert insert " "
    beginning_of_line
}

############################## Protected/Private Methods  ##############################

::itcl::body Command::invoke {} {
    set w $itk_component(text)

    set cmd [$w get promptEnd insert]
    set more_args_list {}

    # remove any instances of prompt2 from the beginning of each secondary line
    regsub -all "\n$itk_option(-prompt2)" $cmd "" cmd

    set cname [lindex $cmd 0]
    if {$cname == "master"} {
	invokeMaster $cmd

	return
    }

    if {[$slaveInterp eval info complete [list $cmd]]} {
	set result [catch {$slaveInterp eval uplevel \#0 [list $cmd]} msg]

	if {$result != 0} {
	    $w tag add oldcmd promptEnd insert
	    print_tag "Error: $msg\n" result
	} else {
	    $w tag add oldcmd promptEnd insert

	    if {$msg != ""} {
		print_tag $msg\n result
	    }
	}

	if {$do_history} {
	    eval lappend cmd $more_args_list
	    $hist add $cmd
	}

	if {$more_args_interrupted} {
	    set more_args_interrupted 0
	} else {
	    print_prompt
	}

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

::itcl::body Command::invokeMaster {hcmd} {
    set cmd [lrange $hcmd 1 end]

    if {$cmd == ""} {
	return ""
    }

    if {[info complete $cmd]} {
	set result [catch {uplevel \#0 $cmd} msg]

	if {$result != 0} {
	    $itk_component(text) tag add oldcmd promptEnd insert
	    print_tag "Error: $msg\n" result
	} else {
	    $itk_component(text) tag add oldcmd promptEnd insert

	    if {$msg != ""} {
		print_tag $msg\n result
	    }
	}

	if {$do_history} {
	    $hist add $hcmd
	}
	print_prompt

	# get rid of oldest output
	set nlines [expr int([$itk_component(text) index end])]
	if {$nlines > $itk_option(-maxlines)} {
	    $itk_component(text) delete 1.0 [expr $nlines - $itk_option(-maxlines)].end
	}
    } else {
	print_promt2
    }
    $itk_component(text) see insert
}

::itcl::body Command::first_char_in_line {} {
    set w $itk_component(text)
    $w mark set insert promptEnd
    set c [$w get insert]
    if {$c == " "} {
	forward_word
    }
    cursor_highlight
}

::itcl::body Command::beginning_of_line {} {
    set w $itk_component(text)
    $w mark set insert promptEnd
    cursor_highlight
}

::itcl::body Command::end_of_line {} {
    set w $itk_component(text)
    $w mark set insert {end - 2c}
    cursor_highlight
    $w see insert
}

::itcl::body Command::backward_char {} {
    set w $itk_component(text)
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	cursor_highlight
    }
}

::itcl::body Command::forward_char {} {
    set w $itk_component(text)
    if [$w compare insert < {end - 2c}] {
	$w mark set insert {insert + 1c}
	cursor_highlight
    }
}

::itcl::body Command::backward_word {} {
    set w $itk_component(text)
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
    } else {
	$w mark set insert promptEnd
    }

    cursor_highlight
}

::itcl::body Command::forward_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" insert {end - 2c}]
    if [string length $ti] {
	$w mark set insert "$ti + 1c"
	cursor_highlight
    }
}

::itcl::body Command::end_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w mark set insert $ti
    } else {
	$w mark set insert {end - 2c}
    }

    cursor_highlight
}

::itcl::body Command::backward_delete_char {} {
    set w $itk_component(text)
    #    catch {$w tag remove sel sel.first promptEnd}
    if [$w compare insert > promptEnd] {
	$w mark set insert {insert - 1c}
	$w delete insert
	cursor_highlight
    }
}

::itcl::body Command::delete_char {} {
    set w $itk_component(text)
    #    catch {$w tag remove sel sel.first promptEnd}
    if {[$w compare insert >= promptEnd] && [$w compare insert < {end - 2c}]} {
	$w delete insert
	cursor_highlight
    }
}

::itcl::body Command::backward_delete_word {} {
    set w $itk_component(text)
    set ti [$w search -backwards -regexp "\[ \t\]\[^ \t\]" {insert - 1c} promptEnd]
    if [string length $ti] {
	$w delete "$ti + 1c" insert
    } else {
	$w delete promptEnd insert
    }
    cursor_highlight
}

::itcl::body Command::delete_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[ \t\]\[^ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight
}

::itcl::body Command::delete_end_word {} {
    set w $itk_component(text)
    set ti [$w search -forward -regexp "\[^ \t\]\[ \t\]" {insert + 1c} {end - 2c}]
    if [string length $ti] {
	$w delete insert "$ti + 1c"
    } else {
	$w delete insert "end - 2c"
    }

    cursor_highlight
}

::itcl::body Command::delete_line {} {
    set w $itk_component(text)
    $w delete promptEnd end-2c
    cursor_highlight
}

::itcl::body Command::delete_end_of_line {} {
    set w $itk_component(text)
    $w delete insert end-2c
    cursor_highlight
}

::itcl::body Command::delete_beginning_of_line {} {
    set w $itk_component(text)
    $w delete promptEnd insert
    cursor_highlight
}

::itcl::body Command::next {} {
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

::itcl::body Command::prev {} {
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

::itcl::body Command::transpose {} {
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

::itcl::body Command::execute {} {
    set w $itk_component(text)
    $w mark set insert {end - 2c}
    $w insert insert \n

    $w see insert
    update

    invoke
    set freshline 1
    cursor_highlight
}

::itcl::body Command::interrupt {} {
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
::itcl::body Command::vi_edit_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[::itcl::code $this backward_char]; break"
    bind $w <space> "[::itcl::code $this forward_char]; break"
    bind $w <KeyPress> "[::itcl::code $this vi_process_edit %A %s]; break"
}

::itcl::body Command::vi_overwrite_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[::itcl::code $this backward_delete_char]; break"
    bind $w <space> "[::itcl::code $this delete_char]; %W insert insert %A; break"
    bind $w <KeyPress> "[::itcl::code $this vi_process_overwrite %A %s]; break"
}

::itcl::body Command::vi_insert_mode {} {
    set w $itk_component(text)
    bind $w <BackSpace> "[::itcl::code $this backward_delete_char]; break"
    bind $w <space> {}
    bind $w <KeyPress> {}
}

::itcl::body Command::vi_process_edit {c state} {
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

::itcl::body Command::vi_process_overwrite {c state} {
    set w $itk_component(text)
    # Throw away all non-visible characters
    if {![string match \[!-~\] $c] || $state > 1} {
	return
    }

    delete_char
    $w insert insert $c
}
# End - VI Specific Callbacks


::itcl::body Command::text_op_begin {x y} {
    set w $itk_component(text)
    global mged_gui

    set moveView 0
    $w scan mark $x $y
}

::itcl::body Command::text_paste {} {
    set w $itk_component(text)

    if {!$moveView} {
	catch {$w insert insert [selection get -displayof $w]}
	$w see insert
    }

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

::itcl::body Command::text_scroll {x y} {
    set w $itk_component(text)

    set moveView 1
    $w scan dragto $x $y
}

::itcl::body Command::selection_begin {x y} {
    set w $itk_component(text)
    $w mark set anchor [::tk::TextClosestGap $w $x $y]
    $w tag remove sel 0.0 end

    if {[$w cget -state] == "normal"} {
	focus $w
    }
}

::itcl::body Command::selection_add {x y} {
    set w $itk_component(text)
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

::itcl::body Command::select_word {x y} {
    set w $itk_component(text)
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

::itcl::body Command::select_line {x y} {
    set w $itk_component(text)
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

::itcl::body Command::selection_modify {x y} {
    set w $itk_component(text)
    ::tk::TextResetAnchor $w @$x,$y
    selection_add $x $y
}

# find the longest common initial string from a list of strings
::itcl::body Command::get_longest_common_string {matches} {
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

::itcl::body Command::tab_completion {} {
    set w $itk_component(text)

    set line [$w get -- promptEnd {promptEnd lineend -1c}]
    set results [tab_expansion $line]

    set expansions [lindex $results 1]
    if { [llength $expansions] > 1 } {
	# show the possible matches
	$w delete {insert linestart} {end-2c}
	$w insert insert "\n${expansions}\n"
	print_prompt
    }

    # display the expanded line
    $w delete promptEnd {end - 2c}
    $w mark set insert promptEnd
    $w insert insert [lindex $results 0]
    $w see insert
}

::itcl::body Command::tab_expansion {line} {
    set matches {}

    set len [llength $line]

    if { $len > 1 } {
	# already have complete command, so do object expansion

	# check if we have an open db
	if {$itk_option(-cmd_prefix) == ""} {
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
		# "$itk_option(-cmd_prefix) get_type" does not blather on error
		if [catch {eval $itk_option(-cmd_prefix) get_type $element} type] {
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
	    set matches [eval $itk_option(-cmd_prefix) expand ${grp}*]
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
	    set matches [lsearch -all -inline $cmdlist "${cmd}*"]
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

::itcl::body Command::print {str} {
    set w $itk_component(text)
    $w insert insert $str
}

::itcl::body Command::print_more_args_prompt {_prompt} {
    $itk_component(text) insert insert $_prompt
}

::itcl::body Command::print_prompt {} {
    set w $itk_component(text)
    print_tag $itk_option(-prompt) prompt
    $w mark set promptEnd insert
    $w mark gravity promptEnd left
}

::itcl::body Command::print_prompt2 {} {
    set w $itk_component(text)
    $w insert insert $itk_option(-prompt2)
}

::itcl::body Command::print_tag {str tag} {
    set w $itk_component(text)
    set first [$w index insert]
    $w insert insert $str
    set last [$w index insert]
    $w tag add $tag $first $last
}

::itcl::body Command::cursor_highlight {} {
    set w $itk_component(text)
    $w tag delete hlt
    $w tag add hlt insert
    $w tag configure hlt -background yellow
}

::itcl::body Command::doBindings {} {
    set w $itk_component(text)
    bind $w <Enter> "focus $w; break"

    doKeyBindings
    doButtonBindings
}

::itcl::body Command::doCopy {} {
    set w $itk_component(text)
    catch {
	clipboard clear -displayof $w;
	clipboard append -displayof $w [selection get -displayof $w]
    }
}

::itcl::body Command::doKeyBindings {} {
    global tcl_platform

    set w $itk_component(text)
    switch $itk_option(-edit_style) {
	vi {
	    vi_insert_mode

	    bind $w <Escape> "[::itcl::code $this vi_edit_mode]; break"
	    bind $w <Control-d> "break"
	    bind $w <Control-u> "[::itcl::code $this delete_beginning_of_line]; break"
	}
	default
	-
	emacs {
	    bind $w <Escape> "break"
	    bind $w <Control-d> "[::itcl::code $this delete_char]; break"
	    bind $w <Control-u> "[::itcl::code $this delete_line]; break"
	    bind $w <BackSpace> "[::itcl::code $this backward_delete_char]; break"
	    bind $w <space> {}
	    bind $w <KeyPress> {}
	}
    }

    # Common Key Bindings
    bind $w <Return> "[::itcl::code $this doReturn]; break"
    bind $w <KP_Enter> "[::itcl::code $this doReturn]; break"
    bind $w <Delete> "[::itcl::code $this backward_delete_char]; break"
    bind $w <Left> "[::itcl::code $this doLeft]; break"
    bind $w <Right> "[::itcl::code $this doRight]; break"
    bind $w <Control-a> "[::itcl::code $this doControl_a]; break"
    bind $w <Control-b> "[::itcl::code $this backward_char]; break"
    if {$tcl_platform(platform) == "windows"} {
	bind $w <Control-c> "[::itcl::code $this doCopy]; break"
    } else {
	bind $w <Control-c> "[::itcl::code $this doControl_c]; break"
    }
    bind $w <Control-e> "[::itcl::code $this end_of_line]; break"
    bind $w <Control-f> "[::itcl::code $this forward_char]; break"
    bind $w <Control-k> "[::itcl::code $this delete_end_of_line]; break"
    bind $w <Control-n> "[::itcl::code $this next]; break"
    bind $w <Control-o> "break"
    bind $w <Control-p> "[::itcl::code $this prev]; break"
    bind $w <Control-t> "[::itcl::code $this transpose]; break"
    bind $w <Control-w> "[::itcl::code $this backward_delete_word]; break"
    bind $w <Up> "[::itcl::code $this prev]; break"
    bind $w <Down> "[::itcl::code $this next]; break"
    bind $w <Home> "[::itcl::code $this beginning_of_line]; break"
    bind $w <End> "[::itcl::code $this end_of_line]; break"
    bind $w <Meta-d> "[::itcl::code $this doMeta_d]; break"
    bind $w <Meta-BackSpace> "[::itcl::code $this doMeta_BackSpace]; break"
    bind $w <Tab>  "[::itcl::code $this tab_completion]; break"

    bind $w <Alt-Key> {
	::tk::TraverseToMenu %W %A
	break
    }
}

::itcl::body Command::doButtonBindings {} {
    set w $itk_component(text)
    bind $w <1> "[::itcl::code $this selection_begin %x %y]; break"
    bind $w <B1-Motion> "[::itcl::code $this selection_add %x %y]; break"
    bind $w <Double-1> "[::itcl::code $this select_word %x %y]; break"
    bind $w <Triple-1> "[::itcl::code $this select_line %x %y]; break"
    bind $w <Shift-1> "[::itcl::code $this selection_modify %x %y]; break"

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

    bind $w <ButtonRelease-2> "[::itcl::code $this text_paste]; break"
    bind $w <2> "[::itcl::code $this text_op_begin %x %y]; break"
    bind $w <B2-Motion> "[::itcl::code $this text_scroll %x %y]; break"
}

::itcl::body Command::doControl_a {} {
    if {$itk_option(-edit_style) == "vi"} {
	first_char_in_line
    } else {
	beginning_of_line
    }
}

::itcl::body Command::doControl_c {} {
    interrupt
    if {$itk_option(-edit_style) == "vi"} {
	vi_insert_mode
    }

    set more_args_interrupted 1
    set more_args_var ""
}

::itcl::body Command::doMeta_d {} {
    if [%W compare insert < promptEnd] {
	break
    }
    cursor_highlight
}

::itcl::body Command::doMeta_BackSpace {} {
    if [%W compare insert <= promptEnd] {
	break
    }
    cursor_highlight
}

::itcl::body Command::doReturn {} {
    execute
    if {$itk_option(-edit_style) == "vi"} {
	vi_insert_mode
    }
}

::itcl::body Command::doLeft {} {
    backward_char
    if {$itk_option(-edit_style) == "vi"} {
	vi_edit_mode
    }
}

::itcl::body Command::doRight {} {
    forward_char
    if {$itk_option(-edit_style) == "vi"} {
	vi_edit_mode
    }
}

bind Text <Control-Key-slash> {}
bind Text <<Cut>> {}

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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
