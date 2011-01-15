#                   S O L C R E A T E . T C L
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
#       Program to allow automated generation and interactive placement of new
#       solids with user-selectable default values.
#

#=============================================================================
# PHASE 0: variable defaults
#=============================================================================

set solc(types) {arb8 sph ell ellg tor tgc rec half rpc rhc epa ehy eto part}
set solc(descr_arb8) "Arbitrary 8-vertex polyhedron"
set solc(default_arb8) {V1 {1 -1 -1}  V2 {1 1 -1}  V3 {1 1 1}  V4 {1 -1 1} \
			    V5 {-1 -1 -1} V6 {-1 1 -1} V7 {-1 1 1} V8 {-1 -1 1}}
set solc(descr_sph)  "Sphere"
set solc(default_sph)  {V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}}
set solc(descr_ell)  "Ellipsoid"
set solc(default_ell)  {V {0 0 0} A {2 0 0} B {0 1 0} C {0 0 1}}
set solc(descr_ellg) "General Ellipsoid"
set solc(default_ellg) {V {0 0 0} A {4 0 0} B {0 2 0} C {0 0 1}}
set solc(descr_tor)  "Torus"
set solc(default_tor)  {V {0 0 0} H {1 0 0} r_h 2 r_a 1}
set solc(descr_tgc)  "Truncated General Cone"
set solc(default_tgc)  {V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} \
			    C {.5 0 0} D {0 1 0}}
set solc(descr_rec)  "Right Elliptical Cylinder"
set solc(default_rec)  {V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} \
			    C {1 0 0} D {0 .5 0}}
set solc(descr_half) "Halfspace"
set solc(default_half) {N {0 0 1} d -1}
set solc(descr_rpc)  "Right Parabolic Cylinder"
set solc(default_rpc)  {V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25}
set solc(descr_rhc)  "Right Hyperbolic Cylinder"
set solc(default_rhc)  {V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25 c .1}
set solc(descr_epa)  "Elliptical Paraboloid"
set solc(default_epa)  {V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25}
set solc(descr_ehy)  "Right Hyperbolic Cylinder"
set solc(default_ehy)  {V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25 \
			    c .25}
set solc(descr_eto)  "Elliptical Torus"
set solc(default_eto)  {V {-1 -1 -1} N {0 0 1} C {.1 0 .1} r .5 r_d .05}
set solc(descr_part) "Particle Primitive"
set solc(default_part) {V {-1 -1 -.5} H {0 0 1} r_v 0.5 r_h 0.25}

set solc(label,V) "Vertex"
set solc(label,A) "A vector"
set solc(label,B) "B vector"
set solc(label,C) "C vector"
set solc(label,D) "D vector"
set solc(label,H) "H vector"
set solc(label,N) "Normal vector"
set solc(label,c) "c value"
set solc(label,d) "d value"
set solc(label,r) "Radius"
set solc(label,r_a) "Radius a"
set solc(label,r_v) "Radius v"
set solc(label,r_h) "Radius h"
set solc(label,r_1) "Radius 1"
set solc(label,r_2) "Radius 2"
set solc(label,r_d) "Radius d"

set solc(winnum) 0
set solc(default_type) arb8
set solc(default_indexvar) index
set solc(default_operation) "incr index"

#=============================================================================
# PHASE 1: solcreate procedure
#-----------------------------------------------------------------------------
# Makes a toplevel window with the labels, entry fields, and buttons to allow
# the user to create new solids.
#=============================================================================

proc solcreate { id args } {
    global solc
    global $solc(default_indexvar)
    global mged_gui

    set w .solc$solc(winnum)
    incr solc(winnum)

    # Pull the solid type out of the argument list if present

    if { [llength $args]>0 } then {
	set ic(default_type) [lindex $args 0]
    }

    catch { destroy $w }
    toplevel $w -screen $mged_gui($id,screen)
    wm title $w "Primitive Creation"

    # Make three frames: top one for entry fields and labels, middle one for
    # solid creation defaults, and bottom one for create and quit buttons

    frame $w.t
    frame $w.m
    frame $w.b

    pack $w.t $w.m $w.b -side top -fill x -expand yes

    # Top frame contains two frames: l (left) and r (right)
    # Left one is for labels and right one is for entry boxes

    frame $w.t.l
    frame $w.t.r

    # Right frame (entry fields) should grow if window width increased

    pack $w.t.l -side left -fill y
    pack $w.t.r -side left -fill x -expand yes

    # Create a label for each entry field

    label $w.t.l.format   -text "Format of name" -anchor w
    label $w.t.l.indexvar -text "Index variable" -anchor w
    label $w.t.l.index    -text "Index" -anchor w
    label $w.t.l.oper     -text "Operation" -anchor w
    label $w.t.l.type     -text "Primitive type" -relief raised -bd 1 -anchor w

    pack $w.t.l.format $w.t.l.indexvar $w.t.l.index $w.t.l.oper $w.t.l.type \
	-side top -fill both -expand yes -anchor w

    # For the "Primitive type" label, allow left-clicking to get a list of solids

    bind $w.t.l.type <1> "solc_list $w $id"

    # Set up some reasonable defaults for the entry fields
    # If the default index variable does not exist, set it equal to 1

    set solc($w,format) "my$solc(default_type).\$$solc(default_indexvar)"
    if { [catch { set $solc(default_indexvar) }]!=0 } {
	set $solc(default_indexvar) 1
    }
    set solc($w,indexvar) $solc(default_indexvar)
    set solc($w,oper) $solc(default_operation)
    set solc($w,type) $solc(default_type)

    # Create the entry boxes

    entry $w.t.r.format   -relief sunken -width 16 -textvar solc($w,format)
    entry $w.t.r.indexvar -relief sunken -width 16 -textvar solc($w,indexvar)
    entry $w.t.r.index    -relief sunken -width 16 -textvar $solc($w,indexvar)
    entry $w.t.r.oper     -relief sunken -width 16 -textvar solc($w,oper)
    entry $w.t.r.type     -relief sunken -width 16 -textvar solc($w,type)

    bind $w.t.r.indexvar <Return> "solc_newvar $w"
    bind $w.t.r.type     <Return> "solc_defaults $w $w.m"

    pack $w.t.r.format $w.t.r.indexvar $w.t.r.index $w.t.r.oper $w.t.r.type \
	-side top -fill x -expand yes

    solc_defaults $w $w.m

    button $w.b.quit   -text "Quit"   -command "solc_quit $w"
    button $w.b.create -text "Create" -command "solc_create $w"
    pack $w.b.quit $w.b.create -side left -fill x -expand yes

    button $w.accept -text "Accept" -state disabled -command "solc_accept $w"
    pack $w.accept -side top -fill x -expand yes
}

#==============================================================================
# PHASE 2: Support routines
#==============================================================================

# solc_list
# Pops up a list of supported solid types.

proc solc_list { w id } {
    global solc
    global $solc($w,indexvar)
    global mged_gui

    catch { destroy $w.slist }
    toplevel $w.slist -screen $mged_gui($id,screen)
    wm title $w.slist "Primitive type list"

    set solc($w,descr) ""

    set doit "set solc($w,type) \[selection get\] ; solc_defaults $w $w.m ; \
	      destroy $w.slist"

    label $w.slist.descr -textvar solc($w,descr)
    listbox $w.slist.l -height [llength $solc(types)] -width 20
    button $w.slist.accept -text "Accept" -command $doit
    foreach soltype [lsort $solc(types)] {
	$w.slist.l insert end $soltype
    }
    pack $w.slist.descr $w.slist.l $w.slist.accept -side top
    set new_descr "catch \{ set solc($w,descr) \
	    \$solc(descr_\[selection get\]) \}"

    bind $w.slist.l <ButtonRelease> $new_descr
    bind $w.slist.descr <1> $doit
    bind $w.slist.l <2> "::tk::ListboxBeginSelect \%W \[\%W index \@\%x,\%y\] ; \
	    $new_descr"
    bind $w.slist <Return> $doit
}

# solc_newvar
# Called when a new variable is requested.

proc solc_newvar { w } {
    global solc
    global $solc($w,indexvar)

    if { [catch { set $solc($w,indexvar) }] != 0 } then {
	set $solc($w,indexvar) 1
    }
}

# solc_accept

proc solc_accept { w } {
    global solc
    global $solc($w,indexvar)

    $w.accept configure -state disabled
    $w.b.create configure -state normal
    press accept
}

proc solc_quit { w } {
    global solc
    global $solc($w,indexvar)

    set solc(default_indexvar) $solc($w,indexvar)
    set solc(default_type) $solc($w,type)
    set solc(default_oper) $solc($w,oper)

    destroy $w
}

proc solc_create { w } {
    global solc
    global $solc($w,indexvar)

    # Perform the formatting of the "name format" supplied by the user
    set name [eval list $solc($w,format)]

    # Perform the operation described by the "index operation" supplied by user
    eval $solc($w,oper)

    set solc(default_indexvar) $solc($w,indexvar)
    set solc(default_type) $solc($w,type)
    set solc(default_oper) $solc($w,oper)

    set solc(default_$solc($w,type)) [eval list [set solc($w,do)]]
    press top
    eval db put $name $solc($w,type) [set solc($w,do)]
    e $name
    sed $name
    press sxy

    $w.accept configure -state normal
    $w.b.create configure -state disabled
}

proc solc_defs { type } {
    global solc

    set retval {}
    catch { set retval $solc(default_[string tolower $type]) }

    return $retval
}

proc solc_defaults { w wfr } {
    global solc
    global $solc($w,indexvar)

    catch { eval destroy [winfo children $wfr] }

    if { [catch { db form $solc($w,type) }]!=0 } then {
	return
    }

    label $wfr.header -text "Default values"
    pack $wfr.header -side top -fill x -expand yes -anchor c

    set form [db form $solc($w,type)]
    set len [llength $form]

    set defs [solc_defs $solc($w,type)]

    if { [llength $defs]<1 } then {
	return
    }

    frame $wfr.l
    frame $wfr.r
    pack $wfr.l -side left -fill y
    pack $wfr.r -side left -fill x -expand yes

    set solc($w,do) ""

    for { set i 0 } { $i<$len } { incr i } {
	set attr [lindex $form $i]
	incr i

	frame $wfr.r.f$attr
	pack $wfr.r.f$attr -side top -fill x -expand yes
	set solc($w,do) [eval concat \[set solc($w,do)\] $attr \\\[eval list]

	set field [lindex $form $i]
	set fieldlen [llength $field]
	set defvals [lindex $defs [expr [lsearch $defs $attr]+1]]

	if { [string first "%f" $field]>-1 || [string first "%d" $field]>-1 } {
	    for { set num 0 } { $num<$fieldlen } { incr num } {
		entry $wfr.r.f$attr.e$num -width 6 -relief sunken
		pack $wfr.r.f$attr.e$num -side left -fill x -expand yes
		$wfr.r.f$attr.e$num insert insert [lindex $defvals $num]
		set solc($w,do) [eval concat \[set solc($w,do)\] \
				     \\\[$wfr.r.f$attr.e$num get\\\]]
	    }
	}

	set solc($w,do) [eval concat \[set solc($w,do)\] \\\]]

	if { [catch { label $wfr.l.l$attr -text "$solc(label,$attr)" \
			  -anchor w}]!=0 } then {
	    label $wfr.l.l$attr -text "$attr" -anchor w
	}

	pack $wfr.l.l$attr -side top -anchor w -fill y -expand yes
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
