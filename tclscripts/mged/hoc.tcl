# 
#                        H O C . T C L
#
#	Procs for implementing "Help On Context".
#
#	Author -
#		Robert G. Parker
#		Paul Tanenbaum
#

# create_hoc_binding --
#
# Generic procedure for building uniform "Help On Context" dialog widgets.
#
proc hoc_create_binding { w subject ksl } {
    if ![winfo exists $w] {
	return
    }

    # Initialize string variables
    set summary ""
    set range ""
    set see_also ""
    set hoc_string ""

    # Set string variables according to { keyword string } list
    foreach ks $ksl {
	switch [lindex $ks 0] {
	    summary {
		set summary [lindex $ks 1]
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

    if { $range != "" } {
	set hoc_string "$hoc_string\n\n$range"
    }

    if { $see_also != "" } {
	set hoc_string "$hoc_string\n\nSEE ALSO\n\t$see_also"
    }

    if { $hoc_string == "" } {
	set hoc_string "Sorry, no information is available for \\\"$subject\\\""
    }

    # cause right mouse button click to bring up dialog widget
    set screen [winfo screen $w]
    bind $w <ButtonPress-3><ButtonRelease-3> "hoc_dialog $w \"$subject\" \"$hoc_string\" %X %Y"
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

# hoc_dialog --
#
# Check to see if the triggering event actually occurred within $w.
#
proc hoc_dialog { w subject hoc_string ptr_x ptr_y } {
    set cwin [winfo containing $ptr_x $ptr_y]
    if { $cwin != $w } {
	return
    }

    set screen [winfo screen $w]
    cad_dialog $w.hocDialog $screen\
	    "On context help for \"$subject\""\
	    "$hoc_string" info 0 Dismiss
}