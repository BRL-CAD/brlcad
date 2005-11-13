#                  E D I T _ S O L I D . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
##
#				E D I T _ S O L I D . T C L
#
# Authors -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	This is a primitive editor for MGED that can be used to both edit and
#	create primitives.
#
# Acknowledgements -
#	This editor is based on Glen Durfee's editobj.tcl (to edit primitives)
#	and solcreate.tcl (to create primitives).
#

## - init_edit_solid
#
proc init_edit_solid { id args } {
    global mged_gui
    global solid_data
    global esol_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set w .$id.edit_solid

    if { [winfo exists $w] } {
	raise $w
	return
    }

    set esol_control($id,cmd) ""

    if ![info exists esol_control($id,inc_op)] {
	set esol_control($id,inc_op) $solid_data(entry,incr_op)
    }

    if ![info exists esol_control($id,dec_op)] {
	set esol_control($id,dec_op) $solid_data(entry,decr_op)
    }

    if ![info exists esol_control($id,format_string)] {
	set esol_control($id,format_string) $solid_data(entry,fmt)
    }

    if {[llength $args] > 0} {
	# if a name is provided, use it
	set esol_control($id,name) [lindex $args 0]
    } elseif {![info exists esol_control($id,name)] || $esol_control($id,name) == "" } {
	set esol_control($id,name) $solid_data(name,default)
    }

    if ![info exists esol_control($id,type)] {
	set esol_control($id,type) $solid_data(type,default)
    }

    if ![info exists esol_control($id,draw)] {
	set esol_control($id,draw) 1
    }

    set row -1
    toplevel $w -screen $mged_gui($id,screen)

    incr row
    frame $w._F$row -relief flat -bd 2
    frame $w.nameF
    label $w.nameL -text "Name:" -anchor e
    set hoc_data { { summary "The name of a primitive must be unique
within a database." } }
    hoc_register_data $w.nameL "Primitive Name" $hoc_data
    entry $w.nameE -relief sunken -bd 2 -textvar esol_control($id,name)
    hoc_register_data $w.nameE "Primitive Name" $hoc_data
    grid $w.nameL $w.nameE -in $w.nameF
    bind $w.nameE <Return> "esol_reset $id $w 0"
    #
    frame $w.typeF
    label $w.typeL -text "Type:" -anchor e
    hoc_register_data $w.typeL "Primitive Type"\
	    { { summary "The supported primitive types are: arb8, sph,
ell, tor, rec, half, rpc, rhc, epa, ehy, eto
part, dsp, ebm and vol." } }
    menubutton $w.typeMB -relief raised -bd 2 -textvariable esol_control($id,type)\
	    -menu $w.typeMB.m -indicatoron 1
    hoc_register_data $w.typeMB "Primitive Type"\
	    { { summary "Indicates the current primitive type. Note that
the left mouse button will pop up a menu
of the supported primitive types." } }
    menu $w.typeMB.m -title "Primitive Type" -tearoff 0
    foreach type $solid_data(types) {
	$w.typeMB.m add command -label $type\
		-command "esol_process_type $id $w.sformF $type"
	hoc_register_menu_data "Primitive Type" $type "Primitive Type - $type"\
		"\"synopsis \\\"Change primitive type to $type.\\\"\"
	         \"description \\\"$solid_data(hoc,$type)\\\"\""
    }
    grid $w.typeL $w.typeMB -in $w.typeF
    #
    grid $w.nameF x $w.typeF x -sticky nsew -in $w._F$row -padx 8
    grid columnconfigure $w._F$row 0 -weight 0
    grid columnconfigure $w._F$row 1 -weight 0
    grid columnconfigure $w._F$row 2 -weight 0
    grid columnconfigure $w._F$row 3 -weight 1
    grid $w._F$row -row $row -column 0 -sticky nsew -pady 8
    grid rowconfigure $w $row -weight 0

    incr row
    set esol_control(form) $row
    esol_build_default_form $id $w.sformF
    grid $w.sformF -row $row -column 0 -sticky nsew -padx 8
    grid rowconfigure $w $row -weight 1

    incr row
    frame $w._F$row -relief groove -bd 2
    #
    frame $w.decF
    set hoc_data { { summary "The decrement equation is used to modify
the primitive parameter values. Note that \"$val\"
represents the current value in a primitive
parameter entry widget and must be present
somewhere in the equation." } }
    label $w.decL -text "-operator:" -width 9 -anchor e
    hoc_register_data $w.decL "Decrement Operator Equation" $hoc_data
    entry $w.decE -relief sunken -bd 2 -textvar esol_control($id,dec_op)
    hoc_register_data $w.decE "Decrement Operator Equation" $hoc_data
    grid $w.decL $w.decE -in $w.decF -sticky nsew
    grid columnconfigure $w.decF 0 -weight 0
    grid columnconfigure $w.decF 1 -weight 1
    #
    frame $w.incF
    set hoc_data { { summary "The increment equation is used to modify
the primitive parameter values. Note that \"$val\"
represents the current value in a primitive
parameter entry widget and must be present
somewhere in the equation." } }
    label $w.incL -text "+operator:" -width 9 -anchor e
    hoc_register_data $w.incL "Increment Operator Equation" $hoc_data
    entry $w.incE -relief sunken -bd 2 -textvar esol_control($id,inc_op)
    hoc_register_data $w.incE "Increment Operator Equation" $hoc_data
    grid $w.incL $w.incE -in $w.incF -sticky nsew
    grid columnconfigure $w.incF 0 -weight 0
    grid columnconfigure $w.incF 1 -weight 1
    #
    frame $w.fmtF
    set hoc_data { { summary "This string is used with the Tcl
format command to format the values
of the primitive parameter entries." }
              { see_also format } }
    label $w.fmtL -text "format:" -width 9 -anchor e
    hoc_register_data $w.fmtL "Format" $hoc_data
    entry $w.fmtE -relief sunken -bd 2 -textvar esol_control($id,format_string)
    hoc_register_data $w.fmtE "Format" $hoc_data
    grid $w.fmtL $w.fmtE -in $w.fmtF -sticky nsew
    grid columnconfigure $w.fmtF 0 -weight 0
    grid columnconfigure $w.fmtF 1 -weight 1
    bind $w.fmtE <Return> "esol_format_entries $id $w.sformF._F"
    #
    grid $w.decF x $w.incF -sticky nsew -in $w._F$row -padx 8 -pady 8
    grid $w.fmtF x x -sticky nsew -in $w._F$row -padx 8 -pady 8
    grid columnconfigure $w._F$row 0 -weight 1
    grid columnconfigure $w._F$row 1 -weight 0
    grid columnconfigure $w._F$row 2 -weight 1
    grid $w._F$row -row $row -column 0 -sticky nsew -padx 8 -pady 8
    grid rowconfigure $w $row -weight 0

    incr row
    checkbutton $w.drawCB -relief flat -bd 2 -text "Draw"\
	    -offvalue 0 -onvalue 1 -variable esol_control($id,draw)
    hoc_register_data $w.drawCB "Draw"\
	    { { summary "Toggle drawing/updating the primitive
in the panes (geometry windows)." } }
    grid $w.drawCB -row $row -column 0 -sticky nsew -padx 8
    grid rowconfigure $w $row -weight 0

    incr row
    set esol_control(buttons) $row
    frame $w._F$row -borderwidth 2
    button $w.okB -text "OK" \
	    -command "esol_ok $id $w $w.sformF"
    hoc_register_data $w.okB "OK"\
	    { { summary "Apply the control panel settings to the
database primitive, then dismiss/close the
control panel." } }
    button $w.applyB -text "Apply" -command "esol_apply $id $w $w.sformF"
    hoc_register_data $w.applyB "Apply"\
	    { { summary "Apply the control panel settings
to the database primitive." } }
    button $w.resetB -text "Reset" -command "esol_reset $id $w 1"
    hoc_register_data $w.resetB "Reset"\
	    { { summary "Reset the primitive parameter entries
with values from the database." } }
    button $w.dismissB -text "Dismiss" -command "destroy $w"
    hoc_register_data $w.dismissB "Dismiss"\
	    { { summary "Dismiss/close the control panel." } }
    grid $w.okB $w.applyB x $w.resetB x $w.dismissB -sticky nsew -in $w._F$row
    grid columnconfigure $w._F$row 2 -weight 1
    grid columnconfigure $w._F$row 4 -weight 1
    grid $w._F$row -row $row -column 0 -sticky nsew -padx 6 -pady 8
    grid rowconfigure $w $row -weight 0
    esol_rename_applyB $id $w

    # only one column
    grid columnconfigure $w 0 -weight 1

    place_near_mouse $w
    wm protocol $w WM_DELETE_WINDOW "catch { destroy $w }"
    wm title $w "Primitive Editor ($id)"
}

## - esol_build_default_form
#
# Using $esol_control($id,name), determine if a primitive exists by this name.
# If so, then use its values to initialize the form. Otherwise, use
# the default values for the current primitive type to initialize the form.
#
proc esol_build_default_form { id w } {
    global solid_data
    global esol_control

    if [catch {set vals [db get $esol_control($id,name)]} msg] {
	# This primitive doesn't exist yet, so use the last primitive type
	#   to get attributes and their values.
	set vals $solid_data(attr,$esol_control($id,type))

	# create mode
	set esol_control($id,edit) 0
	set esol_control($id,cmd) "db put $esol_control($id,name) $esol_control($id,type)"
    } else {
	set esol_control($id,edit) 1
	set esol_control($id,type) [lindex $vals 0]
	set esol_control($id,cmd) "db adjust $esol_control($id,name)"

	# skip primitive type
	set vals [lrange $vals 1 end]
    }

    esol_build_form $id $w $esol_control($id,type) $vals 1 1 1
}

## - esol_build_form
#
# This proc builds a primitive form for creating/editing primitives.
#
proc esol_build_form { id w type vals do_gui do_cmd do_entries } {
    global base2local
    global local2base
    global solid_data
    global esol_control

    set sform $w._F

    if $do_gui {
	frame $w -relief groove -bd 2
	frame $sform
    }

    set form [db form $type]
    set len [llength $form]

    for { set i 0; set row 0 } { $i < $len } { incr i; incr row } {
	set attr [lindex $form $i]
	incr i

	if $do_cmd {
	    set esol_control($id,cmd) "$esol_control($id,cmd) $attr \""
	}

	if $do_gui {
	    if { [catch { label $sform._$attr\L -text "$solid_data(labels,$attr)" \
		    -anchor w }]!=0 } {
		label $sform._$attr\L -text "$attr" -anchor w
	    }
	    grid $sform._$attr\L -row $row -column 0 -sticky nsew
	    grid columnconfigure $sform 0 -weight 0
	}

	set field [lindex $form $i]
	set fieldlen [llength $field]
	for { set num 0 } { $num < $fieldlen } { incr num } {
	    set fe_type [lindex $field $num]
	    set tnum [expr $num % 4]

	    # increment row if more than 4 field elements
	    if {$num && $tnum == 0} {
		incr row
	    }

	    # configure each row
	    if {$tnum == 0 && $do_gui} {
		grid rowconfigure $sform $row -weight 1
	    }

	    switch -glob $fe_type {
		# bu_vls type arguments are %S, char arrays are %s.
		{%*[sS]} {
		    if $do_gui {
			entry $sform._$attr\E$num -relief sunken
			grid $sform._$attr\E$num - - -row $row -column 1 -sticky nsew
			grid columnconfigure $sform 1 -weight 1
		    }

		    if $do_entries {
			$sform._$attr\E$num delete 0 end
			$sform._$attr\E$num insert insert \
				[lindex [lindex $vals $i] $num]
		    }

		    if $do_cmd {
			set esol_control($id,cmd) "$esol_control($id,cmd)\[$sform._$attr\E$num get\]"
		    }
		}
		%*d {
		    if $do_gui {
			button $sform._$attr\decB$num -text \- -command \
				"esol_dec_int $id $sform._$attr\E$num"
			button $sform._$attr\incB$num -text \+ -command \
				"esol_inc_int $id $sform._$attr\E$num"
			entry $sform._$attr\E$num -width 6 -relief sunken

			grid $sform._$attr\decB$num -row $row -column [expr $tnum * 3 + 1] -sticky nsew
			grid $sform._$attr\E$num -row $row -column [expr $tnum * 3 + 2] -sticky nsew
			grid $sform._$attr\incB$num -row $row -column [expr $tnum * 3 + 3] -sticky nsew
			grid columnconfigure $sform [expr $tnum * 3 + 1] -weight 0
			grid columnconfigure $sform [expr $tnum * 3 + 2] -weight 1
			grid columnconfigure $sform [expr $tnum * 3 + 3] -weight 0
		    }

		    if $do_entries {
			$sform._$attr\E$num delete 0 end
			$sform._$attr\E$num insert insert [lindex [lindex $vals $i] $num]
		    }

		    if $do_cmd {
			set esol_control($id,cmd) "$esol_control($id,cmd) \[$sform._$attr\E$num get\]"
		    }
		}
		%*f {
		    if $do_gui {
			button $sform._$attr\decB$num -text \- -command \
				"esol_dec $id $sform._$attr\E$num"
			button $sform._$attr\incB$num -text \+ -command \
				"esol_inc $id $sform._$attr\E$num"
			entry $sform._$attr\E$num -width 6 -relief sunken

			grid $sform._$attr\decB$num -row $row -column [expr $tnum * 3 + 1] -sticky nsew
			grid $sform._$attr\E$num -row $row -column [expr $tnum * 3 + 2] -sticky nsew
			grid $sform._$attr\incB$num -row $row -column [expr $tnum * 3 + 3] -sticky nsew
			grid columnconfigure $sform [expr $tnum * 3 + 1] -weight 0
			grid columnconfigure $sform [expr $tnum * 3 + 2] -weight 1
			grid columnconfigure $sform [expr $tnum * 3 + 3] -weight 0
		    }

		    if $do_entries {
			$sform._$attr\E$num delete 0 end
			$sform._$attr\E$num insert insert \
				[format $esol_control($id,format_string) [expr [lindex \
				[lindex $vals $i] $num] * $base2local]]
		    }

		    if $do_cmd {
			set esol_control($id,cmd) "$esol_control($id,cmd) \[expr \[$sform._$attr\E$num get\] * $local2base\]"
		    }
		}
		default {
		    puts "esolint_build_form: skipping, attr - $attr, field - $fe_type"
		}
	    }
	}

	if $do_gui {
	    grid rowconfigure $sform $row -weight 1
	}

	if $do_cmd {
	    set esol_control($id,cmd) "$esol_control($id,cmd)\""
	}
    }

    if $do_gui {
	grid $sform -sticky nsew -row 0 -column 0 -padx 8 -pady 8
	grid columnconfigure $w 0 -weight 1
	grid rowconfigure $w 0 -weight 1
    }
}

proc esol_ok { id w sform } {
    esol_apply $id $w $sform
    catch { destroy $w }
}

## - esol_apply
#
proc esol_apply { id w sform } {
    global esol_control

    set solid_exists [expr ![catch {db get $esol_control($id,name)} vals]]
    if {$esol_control($id,name) != [lindex $esol_control($id,cmd) 2] ||\
	    ($esol_control($id,edit) && !$solid_exists)} {
	# Here we assume (yikes!!!) that the user is aware
	# of what he/she is doing and oblige by making
	# a last minute change to the command list.

	switch $esol_control($id,edit) {
	    0 {
		set last 3
	    }
	    1 {
		set last 2
	    }
	    2 {
		set last 5
	    }
	}

	if $solid_exists {
	    if {$esol_control($id,type) == [lindex $vals 0]} {
		set tmp_db_cmd [lreplace $esol_control($id,cmd)\
			0 $last db adjust $esol_control($id,name)]
	    } else {
		set tmp_db_cmd "kill $esol_control($id,name);\
			[lreplace $esol_control($id,cmd)\
			0 $last db put $esol_control($id,name) $esol_control($id,type)]"
	    }
	} else {
	    # This primitive doesn't exist.
	    set tmp_db_cmd [lreplace $esol_control($id,cmd)\
		    0 $last db put $esol_control($id,name) $esol_control($id,type)]
	}

	# fix the broken command list
	regsub -all "\[{}\]" $tmp_db_cmd \" esol_control($id,cmd)
    }

    eval [set esol_control($id,cmd)]

    set esol_control($id,edit) 1
    esol_rename_applyB $id $w

    # rebuild esol_control($id,db_cmd)
    set esol_control($id,cmd) "db adjust $esol_control($id,name)"
    esol_build_form $id $sform $esol_control($id,type) {} 0 1 0

    if $esol_control($id,draw) {
	if [esol_isdrawn $esol_control($id,name)] {
	    eval _mged_draw [_mged_who]
	} else {
	    _mged_draw $esol_control($id,name)
	}
    }
}

proc esol_isdrawn { sol } {
    set sol_list [_mged_x -2]

    if {-1 < [lsearch -exact $sol_list $sol]} {
	return 1
    }

    return 0
}

## - esol_inc_int
#
proc esol_inc_int { id entryfield } {
    global esol_control

    set val [expr int([$entryfield get])]
    incr val

    $entryfield delete 0 end
    $entryfield insert insert $val
}

## - esol_dec_int
#
proc esol_dec_int { id entryfield } {
    global esol_control

    set val [expr int([$entryfield get])]
    incr val -1

    $entryfield delete 0 end
    $entryfield insert insert $val
}

## - esol_inc
#
proc esol_inc { id entryfield } {
    global esol_control

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $esol_control($id,format_string) [expr $esol_control($id,inc_op)]]
}

## - esol_dec
#
proc esol_dec { id entryfield } {
    global esol_control

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $esol_control($id,format_string) [expr $esol_control($id,dec_op)]]
}

## - esol_process_type
#
proc esol_process_type { id w newType } {
    global solid_data
    global esol_control

    # nothing to do
    if {$newType == $esol_control($id,type)} {
	return
    }

    if [catch {db get $esol_control($id,name)} vals] {
	# This primitive doesn't exist.

	# create mode
	set esol_control($id,edit) 0
	set esol_control($id,cmd) "db put $esol_control($id,name) $newType"
	set vals $solid_data(attr,$newType)
    } else {
	# This primitive exists

	if { $newType == [lindex $vals 0] } {
	    # going back to original type, so use values from primitive
	    # skip type
	    set vals [lrange $vals 1 end]

	    set esol_control($id,edit) 1
	    set esol_control($id,cmd) "db adjust $esol_control($id,name)"
	} else {
	    # changing primitive type, so use default values
	    set vals $solid_data(attr,$newType)

	    set esol_control($id,edit) 2
	    set esol_control($id,cmd) "kill $esol_control($id,name); db put $esol_control($id,name) $newType"
	}
    }

    set esol_control($id,type) $newType

    # remove and destroy existing form
    grid forget $w
    destroy $w

    # create and install a new form
    esol_build_form $id $w $newType $vals 1 1 1
    grid $w -row $esol_control(form) -column 0 -sticky nsew -padx 8 -pady 8
}

## - esol_reset
#
proc esol_reset { id w reset_entry_values } {
    global solid_data
    global esol_control

    if [catch {db get $esol_control($id,name)} vals] {
	# This primitive doesn't exist.

	# create mode
	set esol_control($id,edit) 0
	set esol_control($id,cmd) "db put $esol_control($id,name) $esol_control($id,type)"

	if $reset_entry_values {
	    set vals $solid_data(attr,$esol_control($id,type))

	    # reset command and entry values
	    esol_build_form $id $w.sformF $esol_control($id,type) $vals 0 1 1
	} else {
	    # reset command
	    esol_build_form $id $w.sformF $esol_control($id,type) {} 0 1 0
	}
    } else {
	# This primitive exists

	# edit mode
	set esol_control($id,edit) 1
	set esol_control($id,cmd) "db adjust $esol_control($id,name)"

	set type [lindex $vals 0]
	set vals [lrange $vals 1 end]
	if { $esol_control($id,type) == $type } {
	    # reset command and entry values
	    esol_build_form $id $w.sformF $esol_control($id,type) $vals 0 1 1
	} else {
	    set esol_control($id,type) $type

	    # remove and destroy existing form
	    grid forget $w.sformF
	    destroy $w.sformF

	    # create and install a new form
	    esol_build_form $id $w.sformF $esol_control($id,type) $vals 1 1 1
	    grid $w.sformF -row $esol_control(form) -column 0 -sticky nsew -padx 8 -pady 8
	}
    }

    esol_rename_applyB $id $w
}

proc esol_rename_applyB { id w } {
    global esol_control

    #XXX Disable for now.
    if { 0 } {
	if { $esol_control($id,edit) } {
	    $w.applyB configure -text "Apply"
	} else {
	    $w.applyB configure -text "Create"
	}
    }
}

proc esol_format_entries { id w } {
    global esol_control

    foreach child [winfo children $w] {
	if { [winfo class $child] == "Entry" } {
	    set val [$child get]
	    $child delete 0 end
	    $child insert insert [format $esol_control($id,format_string) $val]
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
