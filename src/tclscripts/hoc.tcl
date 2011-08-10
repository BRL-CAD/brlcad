#                         H O C . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#                        H O C . T C L
#
#	Procs for implementing "Help On Context".
#
#	Authors -
#		Robert G. Parker
#		Paul Tanenbaum
#

if {[namespace exists ::tk]} {
    if {![info exists ::tk::Priv(cad_dialog)]} {
	set ::tk::Priv(cad_dialog) .cad_dialog
    }
}

# hoc_build_string --
#
# Generic procedure for building uniform "Help On Context" strings.
#
proc hoc_build_string { sname subject ksl } {
    upvar $sname hoc_string

    # Initialize string variables
    set hoc_string ""
    set summary ""
    set synopsis ""
    set description ""
    set examples ""
    set accelerator ""
    set range ""
    set see_also ""

    # Set string variables according to { keyword string } list
    foreach ks $ksl {
	switch [lindex $ks 0] {
	    summary {
		set summary [lindex $ks 1]
	    }
	    synopsis {
		set synopsis [lindex $ks 1]
	    }
	    description {
		set description [lindex $ks 1]
	    }
	    examples {
		set examples [lindex $ks 1]
	    }
	    accelerator {
		set accelerator [lindex $ks 1]
	    }
	    range {
		set range [lindex $ks 1]
	    }
	    see_also {
		set see_also [lindex $ks 1]
	    }
	}
    }

    # Build hoc_string
    if { $summary != "" } {
	set hoc_string $summary
    }

    if { $synopsis != "" } {
	set hoc_string "$hoc_string\nSYNOPSIS\n\t$synopsis"
    }

    if { $description != "" } {
	set  hoc_string "$hoc_string\n\nDESCRIPTION\n\t$description"
    }

    if { $examples != "" } {
	set hoc_string "$hoc_string\n\nEXAMPLES\n\t$examples"
    }

    if { $accelerator != "" } {
	set hoc_string "$hoc_string\n\nACCELERATOR\n\t$accelerator"
    }

    if { $range != "" } {
	set hoc_string "$hoc_string\n\nRANGE\n\t$range"
    }

    if { $see_also != "" } {
	set hoc_string "$hoc_string\n\nSEE ALSO\n\t$see_also"
    }

    if { $hoc_string == "" } {
	set hoc_string "No information was found for \\\"$subject\\\""
    }
}

# create_hoc_binding --
#
# Create bindings to call help on context dialog.
#
proc hoc_create_binding { w subject ksl } {
    global hoc_data

    if ![winfo exists $w] {
	return
    }

    hoc_build_string hoc_string $subject $ksl
    lappend hoc_data($w) $subject $hoc_string

    # cause right mouse button click to bring up dialog widget
    bind $w <ButtonPress-3><ButtonRelease-3> "hoc_callback $w %X %Y"
}

# hoc_create_label_binding --
#
# Calls hoc_create_binding, then create <Enter> and <Leave> bindings
# to highlight the label.
#
proc hoc_create_label_binding { w subject ksl } {
    hoc_create_binding $w "$subject" "$ksl"

    # Create bindings to highlight the label
    bind $w <Enter> "$w configure -background #ececec"
    bind $w <Leave> "$w configure -background #d9d9d9"
}

# hoc_register_data --
#
# Register help on context data.
#
proc hoc_register_data { index subject ksl } {
    global hoc_data

    if [info exists hoc_data($index)] {
	set hoc_data($index) ""
    }

    hoc_build_string hoc_string $subject $ksl
    lappend hoc_data($index) $subject $hoc_string
}

# hoc_register_menu_data --
#
# Call hoc_register_data with an appropriate index.
#
proc hoc_register_menu_data { title label subject ksl } {
    hoc_register_data $title,$label $subject $ksl
}

# hoc_dialog --
#
# Call cad_dialog with hoc_data.
#
proc hoc_dialog { w index } {
    global hoc_data
    global ::tk::Priv

    set screen [winfo screen $w]

    if {[info exists hoc_data($index)]} {
	set subject [lindex $hoc_data($index) 0]
	set description [lindex $hoc_data($index) 1]
	cad_dialog $::tk::Priv(cad_dialog) $screen $subject $description info 0 Dismiss
    }
}

# hoc_menu_callback --
#
# Call hoc_dialog with an appropriate index.
#
proc hoc_menu_callback { w } {
    set title [lindex [$w configure -title] 4]
    set label [$w entrycget active -label]

    hoc_dialog $w $title,$label
}

# hoc_callback --
#
# Call hoc_dialog using $w as an index into hoc_data.
#
proc hoc_callback { w x y } {
    # Check to see if the triggering event actually occurred within $w.
    set cwin [winfo containing $x $y]
    if { $cwin != $w } {
	return
    }

    hoc_dialog $w $w
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
