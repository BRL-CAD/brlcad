#                     I C R E A T E . T C L
# BRL-CAD
#
# Copyright (c) 1995-2007 United States Government as represented by
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
#			 I C R E A T E . T C L
#
# Author -
#	Glenn Durfee
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#       Program to allow automated generation and interactive placement of new
#       instances of preexisting combinations.
#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#

#=============================================================================
# PHASE 0: variable defaults
#=============================================================================

set ic(winnum) 0
set ic(default_type) ""
set ic(default_path) ""
set ic(default_indexvar) index
set ic(default_operation) "incr index"

#=============================================================================
# PHASE 1: icreate procedure
#-----------------------------------------------------------------------------
# Makes a toplevel window with the labels, entry fields, and buttons to allow
# the user to create new instances of existing objects.
#=============================================================================

proc icreate { id args } {
    global ic
    global $ic(default_indexvar)
    global mged_gui

    set w .ic$ic(winnum)
    incr ic(winnum)

    # Pull the object name out of the argument list if present

    if { [llength $args]>0 } then {
	set ic(default_type) [lindex $args 0]
    }

    catch { destroy $w }
    toplevel $w -screen $mged_gui($id,screen)
    wm title $w "Instance Creation"

    # Make two frames: top one for entry fields and labels, buttom one for
    # create and quit buttons

    frame $w.t
    frame $w.b

    pack $w.t $w.b -side top -fill x -expand yes

    # Top frame contains two frames: l (left) and r (right)
    # Left one is for labels and right one is for entry boxes

    frame $w.t.l
    frame $w.t.r

    # Right frame (entry fields) should grow if window width increased

    pack $w.t.l -side left -fill y
    pack $w.t.r -side left -fill x -expand yes

    # Create a label for each entry field

    label $w.t.l.format   -text "Format of name"
    label $w.t.l.indexvar -text "Index variable"
    label $w.t.l.index    -text "Index"
    label $w.t.l.oper     -text "Operation"
    label $w.t.l.type     -text "Prototype"
    label $w.t.l.comb     -text "Comb to add to"
    label $w.t.l.ref      -text "Reference path" -relief raised -bd 1
    bind $w.t.l.ref <1> "ic_reflist $w $id; break"

    pack $w.t.l.format $w.t.l.indexvar $w.t.l.index $w.t.l.oper $w.t.l.type \
	    $w.t.l.comb $w.t.l.ref -side top -fill y -expand yes -anchor w

    # Set up some reasonable defaults for the entry fields
    # If the default index variable does not exist, set it equal to 1

    set ic($w,format) "my$ic(default_type).\$$ic(default_indexvar)"
    set ic($w,indexvar) $ic(default_indexvar)
    if { [catch { set ic($w,index) [set $ic(default_indexvar)] }]!=0 } {
	set ic($w,index) 1
	set $ic(default_indexvar) 1
    }
    set ic($w,oper) $ic(default_operation)
    set ic($w,type) $ic(default_type)
    set ic($w,comb) ""
    if { [catch { set ic($w,ref) [lindex [pathlist $ic($w,type)] 0] }] != 0 } {
	set ic($w,ref) ""
    }

    # Create the entry boxes

    entry $w.t.r.format   -relief sunken -width 16 -textvar ic($w,format)
    entry $w.t.r.indexvar -relief sunken -width 16 -textvar ic($w,indexvar)
    entry $w.t.r.index    -relief sunken -width 16 -textvar ic($w,index)
    entry $w.t.r.oper     -relief sunken -width 16 -textvar ic($w,oper)
    entry $w.t.r.type     -relief sunken -width 16 -textvar ic($w,type)
    entry $w.t.r.comb     -relief sunken -width 16 -textvar ic($w,comb)
    entry $w.t.r.ref      -relief sunken -width 12 -textvar ic($w,ref)

    bind $w.t.r.indexvar <Key-Return> "ic_newvar $w; break"
    bind $w.t.r.index    <Key-Return> "set \$ic($w,indexvar) \$ic($w,index); break"

    pack $w.t.r.format $w.t.r.indexvar $w.t.r.index $w.t.r.oper $w.t.r.type \
	    $w.t.r.comb $w.t.r.ref -side top -fill x -expand yes

    # Bottom contains two frames: left and right, which each contain two
    # buttons

    button $w.b.quit   -text "Quit"   -command "ic_quit $w"
    button $w.b.create -text "Create" -command "ic_create $w"
    pack $w.b.quit $w.b.create -side left -fill x -expand yes

    button $w.accept -text "Accept" -state disabled -command "ic_accept $w"
    pack $w.accept -side top -fill x -expand yes
}

#=============================================================================
# PHASE 2: Support routines
#=============================================================================

# ic_newvar

proc ic_newvar { w } {
    global ic
    global $ic($w,indexvar)

    if { [catch { set ic($w,index) [set $ic($w,indexvar)] }] != 0 } then {
	set ic($w,index) 1
	set $ic($w,indexvar) 1
    }
}

# ic_accept

proc ic_accept { w } {
    $w.accept configure -state disabled
    $w.b.create configure -state normal
    press accept
}

# ic_quit

proc ic_quit { w } {
    global ic
    global $ic($w,indexvar)

    set $ic($w,indexvar) $ic($w,index)
    set ic(default_indexvar) $ic($w,indexvar)
    set ic(default_type) $ic($w,type)
    set ic(default_oper) $ic($w,oper)

    destroy $w
}

# ic_create
# Actually carries out the task of creating a new instance

proc ic_create { w } {
    global ic
    global $ic($w,indexvar)

    set $ic($w,indexvar) $ic($w,index)

    # Perform the formatting of the "name format" supplied by the user
    set name [eval list $ic($w,format)]

    # Perform the operation described by the "index operation" supplied by user
    eval $ic($w,oper)

    set ic(default_indexvar) $ic($w,indexvar)
    set ic(default_type) $ic($w,type)
    set ic(default_oper) $ic($w,oper)

    g $name $ic($w,type)
    if { $ic($w,comb)!="" } then {
	i $ic($w,comb) $name
    }
    e $ic($w,comb)/$name
    oed $ic($w,comb) $name$ic($w,ref)
    press oxy

    $w.accept configure -state normal
    $w.b.create configure -state disabled
}

# ic_reflist
# Pops up a window list containing all of the possible reference solids

proc ic_reflist { w id } {
    global ic
    global mged_gui

    catch { destroy $w.ref }
    if {[llength $ic($w,type)]==0} then {
	error "Please enter a prototype first."
	return
    }

    toplevel $w.ref -screen $mged_gui($id,screen)
    wm title $w.ref "Reference solid list"

    listbox $w.ref.lbox

    foreach solid [pathlist $ic($w,type)] {
	$w.ref.lbox insert end $solid
    }

    bind $w.ref.lbox <Double-Button-1> "set ic($w,ref) \[selection get\]; break"

    button $w.ref.dismiss -text "Dismiss" -command "destroy $w.ref"

    pack $w.ref.lbox -side top -fill y -fill x -expand yes
    pack $w.ref.dismiss -side top -fill x
}








# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
