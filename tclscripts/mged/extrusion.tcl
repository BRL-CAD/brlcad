namespace eval ::extrusion {
	variable curr_sketch_name ""
	variable saved_sketch_name ""
	variable done 0
}

proc get_sketch { screen initial_sketch_name  } {
	global final_sketch_name

	if [winfo exists $screen] {
		 set screen [winfo screen $screen]
	}

	set ::extrusion::done 0
	set ::extrusion::curr_sketch_name $initial_sketch_name
	set ::extrusion::saved_sketch_name $initial_sketch_name

	if [winfo exists .skt_name] {
		# don't rebuild the widget, just fill in the details
		if { [winfo ismapped .skt_name] == 0 } {
			wm deiconify .skt_name
		}
		raise .skt_name
		fill_curve_listbox
	}

	toplevel .skt_name -screen $screen
	wm title .skt_name "Sketch Name for Extrusion"
	wm iconname .skt_name Dialog

	hoc_register_data .skt_name "Sketch Name for Extrusion" {
		{summary "Use this widget to change the sketch used by an extrusion solid"}
	}

	label .skt_name.skt_name_l -text "Sketch Name:"
	entry .skt_name.name -width 20 -textvariable ::extrusion::curr_sketch_name

	hoc_register_data .skt_name.skt_name_l "Sketch_name" {
		{summary "An extrusion solid uses a sketch and extrudes it through an extrusion\n\
			vector to form a solid object. The sketch used by an extrusion must be a closed curve"}
		{description "Enter the name of a sketch object here"}
	}
	hoc_register_data .skt_name.skt_name_e "Sketch_name" {
		{summary "An extrusion solid uses a sketch and extrudes it through an extrusion\n\
			vector to form a solid object. The sketch used by an extrusion must be a closed curve"}
		{description "Enter the name of a sketch object here"}
	}

	button .skt_name.apply -text Apply -command {
		set final_sketch_name $::extrusion::curr_sketch_name
		set ::extrusion::done 1
	}

	hoc_register_data .skt_name.apply "Apply" {
		{summary "An extrusion solid uses a sketch and extrudes it through an extrusion\n\
			vector to form a solid object. The sketch used by an extrusion must be a closed curve"}
		{description "Click your left mouse button here to apply the above sketch\n\
			to the extrusion being edited"}
	}

	button .skt_name.dismiss -text Dismiss -command {
		set final_sketch_name $::extrusion::saved_sketch_name
		set ::extrusion::done 1
	}

	hoc_register_data .skt_name.dismiss "Dismis" {
		{summary "An extrusion solid uses a sketch and extrudes it through an extrusion\n\
			vector to form a solid object. The sketch used by an extrusion must be a closed curve"}
		{description "Click your left mouse button here to dismiss this widget without\n\
			applying any changes to the extrusion being edited"}
	}

	grid .skt_name.skt_name_l -row 0 -column 0
	grid .skt_name.name -row 0 -column 1
	grid .skt_name.apply -row 1 -column 0
	grid .skt_name.dismiss -row 1 -column 1

	tkwait variable ::extrusion::done
	catch " destroy .skt_name "
}
