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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	This is a solid editor for MGED that can be used to both edit and
#	create solids.
#
# Acknowledgements -
#	This editor is based on Glen Durfee's editobj.tcl (to edit solids)
#	and solcreate.tcl (to create solids).
#

# Call this routine to initialize the "solid_data" array
solid_data_init

## - init_edit_solid
#
proc init_edit_solid { id args } {
    global player_screen
    global solid_data
    global esol_db_cmd
    global esol_inc_operation
    global esol_dec_operation
    global esol_format_string
    global esol_name
    global esol_type
    global esol_row
    global esol_draw

    set w .$id.edit_solid

    if { [winfo exists $w] } {
	raise $w
	return
    }

    set esol_db_cmd($id) ""

    if ![info exists esol_inc_operation($id)] {
	set esol_inc_operation($id) $solid_data(entry,incr_op)
    }

    if ![info exists esol_dec_operation($id)] {
	set esol_dec_operation($id) $solid_data(entry,decr_op)
    }

    if ![info exists esol_format_string($id)] {
	set esol_format_string($id) $solid_data(entry,fmt)
    }

    if {[llength $args] > 0} {
	# if a name is provided, use it
	set esol_name($id) [lindex $args 0]
    } elseif {![info exists esol_name($id)] || $esol_name($id) == "" } {
	set esol_name($id) $solid_data(name,default)
    }

    if ![info exists esol_type($id)] {
	set esol_type($id) $solid_data(type,default)
    }

    if ![info exists esol_draw($id)] {
	set esol_draw($id) 1
    }

    set row -1
    toplevel $w -screen $player_screen($id)

    incr row
    frame $w._F$row -relief flat -bd 2
    frame $w.nameF
    label $w.nameL -text "Name:" -anchor e
    set hoc_data { { summary "The name of a solid must be unique
within a database." } }
    hoc_register_data $w.nameL "Solid Name" $hoc_data
    entry $w.nameE -relief sunken -bd 2 -textvar esol_name($id)
    hoc_register_data $w.nameE "Solid Name" $hoc_data
    grid $w.nameL $w.nameE -in $w.nameF
    bind $w.nameE <Return> "esol_reset $id $w 0"
    #
    frame $w.typeF
    label $w.typeL -text "Type:" -anchor e
    hoc_register_data $w.typeL "Solid Type"\
	    { { summary "The supported solid types are: arb8, sph,
ell, tor, rec, half, rpc, rhc, epa, ehy, eto
and part." } }
    menubutton $w.typeMB -relief raised -bd 2 -textvariable esol_type($id)\
	    -menu $w.typeMB.m -indicatoron 1
    hoc_register_data $w.typeMB "Solid Type"\
	    { { summary "Indicates the current solid type. Note that
the left mouse button will pop up a menu
of the supported solid types." } }
    menu $w.typeMB.m -title "Solid Type" -tearoff 0
    foreach type $solid_data(types) {
	$w.typeMB.m add command -label $type\
		-command "esol_process_type $id $w.sformF $type"
	hoc_register_menu_data "Solid Type" $type "Solid Type - $type"\
		"\"synopsis \\\"Change solid type to $type.\\\"\"
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
    set esol_row(form) $row
    esol_build_default_form $id $w.sformF
    grid $w.sformF -row $row -column 0 -sticky nsew -padx 8
    grid rowconfigure $w $row -weight 1

    incr row
    frame $w._F$row -relief groove -bd 2
    #
    frame $w.decF
    set hoc_data { { summary "The decrement equation is used to modify
the solid parameter values. Note that \"$val\"
represents the current value in a solid
parameter entry widget and must be present
somewhere in the equation." } }
    label $w.decL -text "-operator:" -width 9 -anchor e
    hoc_register_data $w.decL "Decrement Operator Equation" $hoc_data
    entry $w.decE -relief sunken -bd 2 -textvar esol_dec_operation($id)
    hoc_register_data $w.decE "Decrement Operator Equation" $hoc_data
    grid $w.decL $w.decE -in $w.decF -sticky nsew
    grid columnconfigure $w.decF 0 -weight 0
    grid columnconfigure $w.decF 1 -weight 1
    #
    frame $w.incF
    set hoc_data { { summary "The increment equation is used to modify
the solid parameter values. Note that \"$val\"
represents the current value in a solid
parameter entry widget and must be present
somewhere in the equation." } }
    label $w.incL -text "+operator:" -width 9 -anchor e
    hoc_register_data $w.incL "Increment Operator Equation" $hoc_data
    entry $w.incE -relief sunken -bd 2 -textvar esol_inc_operation($id)
    hoc_register_data $w.incE "Increment Operator Equation" $hoc_data
    grid $w.incL $w.incE -in $w.incF -sticky nsew
    grid columnconfigure $w.incF 0 -weight 0
    grid columnconfigure $w.incF 1 -weight 1
    #
    frame $w.fmtF
    set hoc_data { { summary "This string is used with the Tcl
format command to format the values
of the solid parameter entries." }
              { see_also format } }
    label $w.fmtL -text "format:" -width 9 -anchor e
    hoc_register_data $w.fmtL "Format" $hoc_data
    entry $w.fmtE -relief sunken -bd 2 -textvar esol_format_string($id)
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
	    -offvalue 0 -onvalue 1 -variable esol_draw($id)
    hoc_register_data $w.drawCB "Draw"\
	    { { summary "Toggle drawing/updating the solid
in the panes (geometry windows)." } }
    grid $w.drawCB -row $row -column 0 -sticky nsew -padx 8
    grid rowconfigure $w $row -weight 0

    incr row
    set esol_row(buttons) $row
    frame $w._F$row -borderwidth 2
    button $w.okB -text "Ok" \
	    -command "esol_ok $id $w $w.sformF"
    hoc_register_data $w.okB "Ok"\
	    { { summary "Apply the control panel settings to the
database solid, then dismiss/close the
control panel." } }
    button $w.applyB -text "Apply" -command "esol_apply $id $w $w.sformF"
    hoc_register_data $w.applyB "Apply"\
	    { { summary "Apply the control panel settings
to the database solid." } }
    button $w.resetB -text "Reset" -command "esol_reset $id $w 1"
    hoc_register_data $w.resetB "Reset"\
	    { { summary "Reset the solid parameter entries
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

    set pxy [winfo pointerxy $w]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $w WM_DELETE_WINDOW "catch { destroy $w }"
    wm geometry $w +$x+$y
    wm title $w "Solid Editor ($id)"
}

## - esol_build_default_form
#
# Using $esol_name($id), determine if a solid exists by this name.
# If so, then use its values to initialize the form. Otherwise, use
# the default values for the current solid type to initialize the form.
#
proc esol_build_default_form { id w } {
    global solid_data
    global esol_db_cmd
    global esol_name
    global esol_type
    global esol_edit

    if [catch {set vals [db get $esol_name($id)]} msg] {
	# This solid doesn't exist yet, so use the last solid type
	#   to get attributes and their values.
	set vals $solid_data(attr,$esol_type($id))

	# create mode
	set esol_edit($id) 0
	set esol_db_cmd($id) "db put $esol_name($id) $esol_type($id)"
    } else {
	set esol_edit($id) 1
	set esol_type($id) [lindex $vals 0]
	set esol_db_cmd($id) "db adjust $esol_name($id)"

	# skip solid type
	set vals [lrange $vals 1 end]
    }

    esol_build_form $id $w $esol_type($id) $vals 1 1 1
}

## - esol_build_form
#
# This proc builds a solid form for creating/editing solids.
#
proc esol_build_form { id w type vals do_gui do_cmd do_entries } {
    global base2local
    global local2base
    global solid_data
    global esol_db_cmd
    global esol_format_string
    global esol_name

    set sform $w._F

    if $do_gui {
	frame $w -relief groove -bd 2
	frame $sform
    }

    set form [db form $type]
    set len [llength $form]

    for { set i 0; set row 0 } { $i<$len } { incr i; incr row } {
	set attr [lindex $form $i]
	incr i

	if $do_cmd {
	    set esol_db_cmd($id) [eval concat \[set esol_db_cmd($id)\] $attr \\\"]
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
	for { set num 0 } { $num<$fieldlen } { incr num } {
	    if { [string first "%f" $field]>-1 } {
		if $do_gui {
		    button $sform._$attr\decB$num -text \- -command \
			    "esol_dec $id $sform._$attr\E$num"
		    button $sform._$attr\incB$num -text \+ -command \
			    "esol_inc $id $sform._$attr\E$num"
		    entry $sform._$attr\E$num -width 6 -relief sunken

		    grid $sform._$attr\decB$num -row $row -column [expr $num * 3 + 1] -sticky nsew
		    grid $sform._$attr\E$num -row $row -column [expr $num * 3 + 2] -sticky nsew
		    grid $sform._$attr\incB$num -row $row -column [expr $num * 3 + 3] -sticky nsew
		    grid columnconfigure $sform [expr $num * 3 + 1] -weight 0
		    grid columnconfigure $sform [expr $num * 3 + 2] -weight 1
		    grid columnconfigure $sform [expr $num * 3 + 3] -weight 0
		}


		if $do_entries {
		    $sform._$attr\E$num delete 0 end
		    $sform._$attr\E$num insert insert \
			    [format $esol_format_string($id) [expr [lindex \
			    [lindex $vals $i] $num] * $base2local]]
		}

		if $do_cmd {
		    set esol_db_cmd($id) [eval concat \[set esol_db_cmd($id)\] \
			    \\\[expr \\\[$sform._$attr\E$num get\\\] * $local2base\\\]]
		}
	    } else {
		# XXXX Temporary debugging
		puts "esol_build_form: skipping field"
	    }
	}

	if $do_gui {
	    grid rowconfigure $sform $row -weight 1
	}

	if $do_cmd {
	    set esol_db_cmd($id) [eval concat \[set esol_db_cmd($id)\] \\\"]
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
    global esol_db_cmd
    global esol_name
    global esol_type
    global esol_edit
    global esol_draw

    set solid_exists [expr ![catch {db get $esol_name($id)} vals]]
    if {$esol_name($id) != [lindex $esol_db_cmd($id) 2] ||\
	    ($esol_edit($id) && !$solid_exists)} {
	# Here we assume (yikes!!!) that the user is aware
	# of what he/she is doing and oblige by making
	# a last minute change to the command list.

	switch $esol_edit($id) {
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
	    if {$esol_type($id) == [lindex $vals 0]} {
		set tmp_db_cmd [lreplace $esol_db_cmd($id)\
			0 $last db adjust $esol_name($id)]
	    } else {
		set tmp_db_cmd "kill $esol_name($id);\
			[lreplace $esol_db_cmd($id)\
			0 $last db put $esol_name($id) $esol_type($id)]"
	    }
	} else {
	    # This solid doesn't exist.
	    set tmp_db_cmd [lreplace $esol_db_cmd($id)\
		    0 $last db put $esol_name($id) $esol_type($id)]
	}

	# fix the broken command list
	regsub -all "\[{}\]" $tmp_db_cmd \" esol_db_cmd($id)
    }

    eval [set esol_db_cmd($id)]

    set esol_edit($id) 1
    esol_rename_applyB $id $w

    # rebuild esol_db_cmd
    set esol_db_cmd($id) "db adjust $esol_name($id)"
    esol_build_form $id $sform $esol_type($id) {} 0 1 0

    if $esol_draw($id) {
	if [esol_isdrawn $esol_name($id)] {
	    eval _mged_draw [_mged_who]
	} else {
	    _mged_draw $esol_name($id)
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

## - esol_inc
#
proc esol_inc { id entryfield } {
    global esol_inc_operation
    global esol_format_string

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $esol_format_string($id) [expr $esol_inc_operation($id)]]
}

## - esol_dec
#
proc esol_dec { id entryfield } {
    global esol_dec_operation
    global esol_format_string

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $esol_format_string($id) [expr $esol_dec_operation($id)]]
}

## - esol_process_type
#
proc esol_process_type { id w newType } {
    global solid_data
    global esol_db_cmd
    global esol_name
    global esol_type
    global esol_row
    global esol_edit

    # nothing to do
    if {$newType == $esol_type($id)} {
	return
    }

    if [catch {db get $esol_name($id)} vals] {
	# This solid doesn't exist.

	# create mode
	set esol_edit($id) 0
	set esol_db_cmd($id) "db put $esol_name($id) $newType"
	set vals $solid_data(attr,$newType)
    } else {
	# This solid exists

	if { $newType == [lindex $vals 0] } {
	    # going back to original type, so use values from solid
	    # skip type
	    set vals [lrange $vals 1 end]

	    set esol_edit($id) 1
	    set esol_db_cmd($id) "db adjust $esol_name($id)"
	} else {
	    # changing solid type, so use default values
	    set vals $solid_data(attr,$newType)

	    set esol_edit($id) 2
	    set esol_db_cmd($id) "kill $esol_name($id); db put $esol_name($id) $newType"
	}
    }

    set esol_type($id) $newType

    # remove and destroy existing form
    grid forget $w
    destroy $w

    # create and install a new form
    esol_build_form $id $w $newType $vals 1 1 1
    grid $w -row $esol_row(form) -column 0 -sticky nsew -padx 8 -pady 8
}

## - esol_reset
#
proc esol_reset { id w reset_entry_values } {
    global solid_data
    global esol_db_cmd
    global esol_name
    global esol_type
    global esol_row
    global esol_edit

    if [catch {db get $esol_name($id)} vals] {
	# This solid doesn't exist.

	# create mode
	set esol_edit($id) 0
	set esol_db_cmd($id) "db put $esol_name($id) $esol_type($id)"

	if $reset_entry_values {
	    set vals $solid_data(attr,$esol_type($id))

	    # reset command and entry values
	    esol_build_form $id $w.sformF $esol_type($id) $vals 0 1 1
	} else {
	    # reset command
	    esol_build_form $id $w.sformF $esol_type($id) {} 0 1 0
	}
    } else {
	# This solid exists

	# edit mode
	set esol_edit($id) 1
	set esol_db_cmd($id) "db adjust $esol_name($id)"

	set type [lindex $vals 0]
	set vals [lrange $vals 1 end]
	if { $esol_type($id) == $type } {
	    # reset command and entry values
	    esol_build_form $id $w.sformF $esol_type($id) $vals 0 1 1
	} else {
	    set esol_type($id) $type

	    # remove and destroy existing form
	    grid forget $w.sformF
	    destroy $w.sformF

	    # create and install a new form
	    esol_build_form $id $w.sformF $esol_type($id) $vals 1 1 1
	    grid $w.sformF -row $esol_row(form) -column 0 -sticky nsew -padx 8 -pady 8
	}
    }

    esol_rename_applyB $id $w
}

proc esol_rename_applyB { id w } {
    global esol_edit

    #XXX Disable for now.
    if { 0 } {
	if { $esol_edit($id) } {
	    $w.applyB configure -text "Apply"
	} else {
	    $w.applyB configure -text "Create"
	}
    }
}

proc esol_format_entries { id w } {
    global esol_format_string

    foreach child [winfo children $w] {
	if { [winfo class $child] == "Entry" } {
	    set val [$child get]
	    $child delete 0 end
	    $child insert insert [format $esol_format_string($id) $val]
	}
    }
}
