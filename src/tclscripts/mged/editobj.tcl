#                     E D I T O B J . T C L
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
#
# Author -
#	Glen Durfee
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	Generic object editor for MGED
#
# Modifications -
#	(Bob Parker):
#		- Generalized the code to accommodate multiple instances
#		  of the user interface.
#		- added eoformat_string to control format of entry strings
#		- added eoinc_operation and eodec_operation to control increment
#		  and decrement of entry values
#		- when checking for the existence of the editor, instead of complaining,
#		  raise the editor and return
#		- when creating for the first time, make the editor appear to the
#		  lower right of mouse pointer
#		- modify eoinc and eodec to NOT apply changes, that's what the "Apply"
#		  button is for.
#		- display info in local units and assume the same when applying to database
#

set eoname(V) "Vertex"
set eoname(A) "A vector"
set eoname(B) "B vector"
set eoname(C) "C vector"
set eoname(D) "D vector"
set eoname(H) "H vector"
set eoname(r) "Radius"
set eoname(r_a) "Radius a"
set eoname(r_h) "Radius h"

if ![info exists eoinc_operation] {
    set eoinc_operation "\$val * 1.1"
}

if ![info exists eodec_operation] {
    set eodec_operation "\$val / 1.1"
}

if ![info exists eoformat_string] {
    set eoformat_string "%.4f"
}

proc editobj { id oname } {
    global mged_gui
    global eofin$oname
    global eoname
    global base2local
    global local2base
    global eoformat_string

    set vals [db get $oname]
    set form [db form [lindex $vals 0]]
    set len [llength $form]

    if { [winfo exists .eo$oname] } {
	raise .eo$oname
	return
    }

    toplevel .eo$oname -screen $mged_gui($id,screen)

    frame .eo$oname.t -borderwidth 2
    pack .eo$oname.t -side top -fill x -expand yes

    frame .eo$oname.t.l
    frame .eo$oname.t.r
    pack .eo$oname.t.l -side left -fill y -expand yes
    pack .eo$oname.t.r -side left -fill x -expand yes

    set eofin$oname "db adjust $oname"

    for { set i 0 } { $i<$len } { incr i } {
	set attr [lindex $form $i]
	incr i

	frame .eo$oname.t.r.f$attr
	pack .eo$oname.t.r.f$attr -side top -fill x -expand yes
	set eofin$oname [eval concat \[set eofin$oname\] $attr \\\"]

	set field [lindex $form $i]
	set fieldlen [llength $field]
	for { set num 0 } { $num<$fieldlen } { incr num } {
	    if { [string first "%f" $field]>-1 } then {
		button .eo$oname.t.r.f$attr.dec$num -text \- -command \
			"eodec $oname .eo$oname.t.r.f$attr.e$num"
		button .eo$oname.t.r.f$attr.inc$num -text \+ -command \
			"eoinc $oname .eo$oname.t.r.f$attr.e$num"
		entry .eo$oname.t.r.f$attr.e$num -width 6 -relief sunken
		pack .eo$oname.t.r.f$attr.dec$num -side left
		pack .eo$oname.t.r.f$attr.e$num -side left -fill x -expand yes
		pack .eo$oname.t.r.f$attr.inc$num -side left
		.eo$oname.t.r.f$attr.e$num insert insert \
			[format $eoformat_string [expr [lindex \
			[lindex $vals [expr $i+1]] $num] * $base2local]]
		set eofin$oname [eval concat \[set eofin$oname\] \
			\\\[expr \\\[.eo$oname.t.r.f$attr.e$num get\\\] * $local2base\\\]]
		bind .eo$oname.t.r.f$attr.e$num <Key-Return> "eoapply $oname; break"
	    }
	}

	set eofin$oname [eval concat \[set eofin$oname\] \\\"]

	if { [catch { label .eo$oname.t.l.l$attr -text "$eoname($attr)" \
		-anchor w }]!=0 } then {
	    label .eo$oname.t.l.l$attr -text "$attr" -anchor w
	}

	pack .eo$oname.t.l.l$attr -side top -anchor w -fill y -expand yes
    }

    frame .eo$oname.b -borderwidth 2
    pack .eo$oname.b -side top -fill x -expand yes
    button .eo$oname.b.apply -text "Apply" -command "eoapply $oname"
    button .eo$oname.b.cancel -text "Close" -command "destroy .eo$oname"
    pack .eo$oname.b.apply .eo$oname.b.cancel -side left -fill x -expand yes

    set pxy [winfo pointerxy .eo$oname]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm protocol .eo$oname WM_DELETE_WINDOW "catch { destroy .eo$oname }"
    wm geometry .eo$oname +$x+$y
    wm title .eo$oname "Object editor: $oname"
}

proc eoapply { oname } {
    global eofin$oname
    global eoname
    global base2local
    global eoformat_string

    eval [set eofin$oname]

    set vals [db get $oname]
    set form [db form [lindex $vals 0]]
    set len [llength $form]

    for { set i 0 } { $i<$len } { incr i } {
	set attr [lindex $form $i]
	incr i

	set field [lindex $form $i]
	set fieldlen [llength $field]
	for { set num 0 } { $num<$fieldlen } { incr num } {
	    if { [string first "%f" $field]>-1 } {
		.eo$oname.t.r.f$attr.e$num delete 0 end
		.eo$oname.t.r.f$attr.e$num insert insert \
			[format $eoformat_string [expr [lindex \
			[lindex $vals [expr $i+1]] $num] * $base2local]]
	    }
	}
    }
}

proc eoinc { oname entryfield } {
    global eoinc_operation
    global eoformat_string

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $eoformat_string [expr $eoinc_operation]]
}

proc eodec { oname entryfield } {
    global eodec_operation
    global eoformat_string

    set val [$entryfield get]

    $entryfield delete 0 end
    $entryfield insert insert [format $eoformat_string [expr $eodec_operation]]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
