#
#			C O L O R . T C L
#
#	Color utilities.
#
#	Author - Robert G. Parker
#
###############################################################################

# set_WidgetRGBColor --
#
# Set the widget color given an rgb string.
#
proc set_WidgetRGBColor { w rgb_str } {
    if ![winfo exists $w] {
	return "set_WidgetRGBColor: bad Tk window name --> $w"
    }

    if {$rgb_str != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rgb_str cmatch red green blue]
	if {!$result} {
	    cad_dialog $w.colorDialog [winfo screen $w]\
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
