#
#			C O L O R . T C L
#
#	Color utilities.
#
#	Author - Robert G. Parker
#

#
#		C H O O S E C O L O R
#
# Returns a list containing the chosen color expressed two ways:
#	1) #XXXXXX, where the X's are hex(i.e. 2 for each of RGB)
#	2) r g b
#
proc chooseColor { parent } {
    set hex_color [tk_chooseColor -parent $parent] 
    if {$hex_color == ""} {
	return {}
    }

    set result [regexp "^#(\[0-9AaBbCcDdEeFf\]\[0-9AaBbCcDdEeFf\])(\[0-9AaBbCcDdEeFf\]\[0-9AaBbCcDdEeFf\])(\[0-9AaBbCcDdEeFf\]\[0-9AaBbCcDdEeFf\])$" $hex_color cmatch hred hgreen hblue]

    if {$result} {
	set rgb [format "%d %d %d" 0X$hred 0X$hgreen 0X$hblue]
	return [list $hex_color $rgb]
    } else {
	mged_dialog $w.colorDialog [winfo screen $parent]\
		"chooseColor: tk_chooseColor returned bad value"\
		"chooseColor: tk_chooseColor returned bad value - $hex_color"\
		"" 0 OK
	return {}
    }
}

proc set_WidgetRGBColor { w rgb_str } {
    if ![winfo exists $w] {
	return "set_WidgetRGBColor: bad Tk window name --> $w"
    }

    if {$rgb_str != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rgb_str cmatch red green blue]
	if {!$result} {
	    mged_dialog $w.colorDialog [winfo screen $w]\
		    "Improper color specification!"\
		    "Improper color specification: $rgb_str"\
		    "" 0 OK
	    return
	}
    } else {
	return
    }

    $w configure -bg [format "#%02x%02x%02x" $red $green $blue]
}
