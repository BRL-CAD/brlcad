#                        M I K E . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
proc mike_dedication {id} {
	global mged_gui

	set top .$id\_mike

	if { [winfo exists $top] } {
		wm deiconify $top
		raise $top
		return
	}

	toplevel $top -screen $mged_gui($id,screen)
	set row 0

	set mike_file [bu_brlcad_data "tclscripts/mged"]/mike-tux.ppm
	if { [file exists $mike_file] } {
		set mike [image create photo -file $mike_file]
		label $top.mike_im -image $mike -relief sunken
		grid $top.mike_im -row $row -column 0 -columnspan 2 -pady 3
		incr row
	}

	label $top.dates -text "Michael John Muuss\nOctober 16, 1958 - November 20, 2000"
	grid $top.dates -row $row -column 0 -columnspan 2
#	grid rowconfigure $top $row -weight 1
	incr row

	set dedi_file [bu_brlcad_data "tclscripts/mged"]/mike-dedication.txt
	if { [file exists $dedi_file] } {
		if { [catch {open $dedi_file "r"} fp] == 0 } {
			set dedi_text [read -nonewline $fp]
			scrollbar $top.sb -command "$top.dedi yview" -orient vertical
			text $top.dedi -wrap word -yscrollcommand "$top.sb set"
			set text_font [$top.dedi cget -font]
			set tab_len [font measure  $text_font -displayof $top "Oooo"]
			$top.dedi configure -tabs $tab_len
			$top.dedi insert end $dedi_text
			grid $top.dedi -row $row -column 0 -sticky nsew -pady 3
			grid $top.sb -row $row -column 1 -sticky nsw -pady 3
			grid rowconfigure $top $row -weight 1
			incr row
		}
	}

	button $top.dismiss -text "Ttfn" -command "destroy $top"
	grid $top.dismiss -row $row -column 0 -columnspan 2 -pady 3
	hoc_register_data $top.dismiss "Ttfn" {
		{summary "Ta ta, for now\n              -Tigger" }
	}

	grid columnconfigure $top 0 -weight 1

	wm title $top "Dedication"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
