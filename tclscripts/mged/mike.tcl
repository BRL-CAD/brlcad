proc mike_dedication {} {

	set top .mike

	if { [winfo exists $top] } {
		wm deiconify $top
		raise $top
		return
	}

	toplevel $top
	set row 0

	set mike_file [bu_brlcad_path "tclscripts/mged"]/mike-tux.ppm
	if { [file exists $mike_file] } {
		set mike [image create photo -file $mike_file]
		label $top.mike_im -image $mike -relief sunken
		grid $top.mike_im -row $row -column 0 -columnspan 2 -pady 3
		incr row
	}
	
	label $top.dates -text "Michael John Muuss\nOctober 16, 1958 - November 20, 2000"
	grid $top.dates -row $row -column 0 -columnspan 2
	grid rowconfigure $top $row -weight 1
	incr row

	set dedi_file [bu_brlcad_path "tclscripts/mged"]/mike-dedication.txt
	if { [file exists $dedi_file] } {
		if { [catch {open $dedi_file "r"} fp] == 0 } {
			set dedi_text [read -nonewline $fp]
			scrollbar $top.sb -command "$top.dedi yview" -orient vertical
			text $top.dedi -wrap word -yscrollcommand "$top.sb set"
			set text_font [$top.dedi cget -font]
			set tab_len [font measure  $text_font -displayof $top "Oooo"]
			$top.dedi configure -tabs $tab_len
			$top.dedi insert end $dedi_text
			grid $top.dedi -row $row -column 0 -sticky e -pady 3
			grid $top.sb -row $row -column 1 -sticky nsw -pady 3
			grid rowconfigure $top $row -weight 1
			incr row
		}
	}

	button $top.dismiss -text "dismiss" -command "destroy $top"
	grid $top.dismiss -row $row -column 0 -columnspan 2 -pady 3

	grid columnconfigure $top 0 -weight 1

	wm title $top "Dedication"
}
