namespace eval ::extrusion {
	variable curr_sketch_name ""
	variable curr_curve_name ""
	variable saved_sketch_name ""
	variable saved_curve_name ""
	variable done 0
}

proc get_curve_list {} {
	if { [catch [list db get $::extrusion::curr_sketch_name CL] curve_list] } {
		puts "sketch does not exist"
		puts $curve_list
		return ""
	}
	set curves [lindex $curve_list 0]
	set length [llength $curves]

	set curve_names ""
	for { set i 0 } { $i < $length } { incr i 2 } {
		lappend curve_names [lindex $curves $i]
	}

	return $curve_names
}

proc fill_curve_listbox {} {
	set curve_list [get_curve_list]
	set length [llength $curve_list]
	for { set i  0 } { $i < $length } { incr i } {
		set aname [lindex $curve_list $i]
		.skt_name.name_lb insert end $aname
		if { [string compare $aname $::extrusion::curr_curve_name] == 0 } {
			.skt_name.name_lb selection set end
		}
	}
}

proc get_sketch_and_curve { screen initial_sketch_name initial_curve_name } {
	global final_sketch_name final_curve_name

	if [winfo exists $screen] {
		 set screen [winfo screen $screen]
	}

	set ::extrusion::done 0
	set ::extrusion::curr_sketch_name $initial_sketch_name
	set ::extrusion::curr_curve_name $initial_curve_name
	set ::extrusion::saved_sketch_name $initial_sketch_name
	set ::extrusion::saved_curve_name $initial_curve_name

	if [winfo exists .skt_name] {
		# don't rebuild the widget, just fill in the details
		if { [winfo ismapped .skt_name] == 0 } {
			wm deiconify .skt_name
		}
		raise .skt_name
		fill_curve_listbox
	}

	toplevel .skt_name -screen $screen
	wm title .skt_name "Sketch and Curve Names for Extrusion"
	wm iconname .skt_name Dialog

	hoc_register_data .skt_name "Sketch and Curve Names for Extrusion" {
		{summary "Use this widget to change the sketch and/or curve used by an extrusion solid"}
	}

	label .skt_name.skt_name_l -text "Sketch Name:"
	entry .skt_name.name -width 20 -textvariable ::extrusion::curr_sketch_name
	bind .skt_name.name <Return> {
		set curve_list [get_curve_list]
		set length [llength $curve_list]
		.skt_name.name_lb delete 0 end
		for { set i  0 } { $i < $length } { incr i } {
			set aname [lindex $curve_list $i]
			.skt_name.name_lb insert end $aname
			if { [string compare $aname $::extrusion::curr_curve_name] == 0 } {
				.skt_name.name_lb selection set end
			}
		}

	}

	hoc_register_data .skt_name.skt_name_l "Sketch_name" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Enter the name of a sketch object here and hit return to see the\n\
			list of curves in that sketch"}
	}
	hoc_register_data .skt_name.skt_name_e "Sketch_name" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Enter the name of a sketch object here and hit return to see the\n\
			list of curves in that sketch"}
	}
	label .skt_name.crv_name -text "Curve Name:"
	listbox .skt_name.name_lb -width 20 -selectmode single

	fill_curve_listbox

	hoc_register_data .skt_name.crv_name "Curve Name" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Select one of the curves listed here by clicking on it with\n\
			your left mouse button"}
	}
	hoc_register_data .skt_name.name_lb "Curve Name" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Select one of the curves listed here by clicking on it with\n\
			your left mouse button"}
	}

	button .skt_name.apply -text Apply -command {
		set final_sketch_name $::extrusion::curr_sketch_name
		set final_curve_name [.skt_name.name_lb get [.skt_name.name_lb curselection]]
		set ::extrusion::done 1
	}

	hoc_register_data .skt_name.apply "Apply" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Click your left mouse button here to apply the above sketch and selected\n\
			curve to the extrusion being edited"}
	}

	button .skt_name.dismiss -text Dismiss -command {
		set final_sketch_name $::extrusion::saved_sketch_name
		set final_curve_name $::extrusion::saved_curve_name
		set ::extrusion::done 1
	}

	hoc_register_data .skt_name.dismiss "Dismis" {
		{summary "An extrusion solid uses a curve and extrudes it through an extrusion\n\
			vector to form a solid object. One or more curves may be kept in a 'sketch'\n\
			object. The curve used by an extrusion must be a closed curve"}
		{description "Click your left mouse button here to dismiss this widget without\n\
			applying any changes to the extrusion being edited"}
	}

	grid .skt_name.skt_name_l -row 0 -column 0
	grid .skt_name.name -row 0 -column 1
	grid .skt_name.crv_name -row 1 -column 0 -columnspan 2
	grid .skt_name.name_lb -row 2 -column 0 -columnspan 2
	grid .skt_name.apply -row 3 -column 0
	grid .skt_name.dismiss -row 3 -column 1

	tkwait variable ::extrusion::done
	catch " destroy .skt_name "
}
