##
#			C O L O R . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#       The U. S. Army Research Laboratory
#       Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#       Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	Color utilities.
#

## color_entry_build --
#
# Build a color entry widget. The color entry widget consists of
# an entry and a menubutton widget. The menu associated with the 
# menubutton contains some canned color entries as well as an
# entry that invokes a user specified command for setting the color.
#
proc color_entry_build { top key var_name user_color_cmd width icolor } {
    frame $top.$key\F -relief sunken -bd 2
    entry $top.$key\E -relief flat -width $width -textvar $var_name
    menubutton $top.$key\MB -relief raised -bd 2\
	    -menu $top.$key\MB.m -indicatoron 1
    menu $top.$key\MB.m -tearoff 0
    $top.$key\MB.m add command -label black\
	    -command "set $var_name \"0 0 0\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label white\
	    -command "set $var_name \"255 255 255\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label red\
	    -command "set $var_name \"255 0 0\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label green\
	    -command "set $var_name \"0 255 0\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label blue\
	    -command "set $var_name \"0 0 255\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label yellow\
	    -command "set $var_name \"255 255 0\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label cyan\
	    -command "set $var_name \"0 255 255\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add command -label magenta\
	    -command "set $var_name \"255 0 255\"; set_WidgetRGBColor $top.$key\MB \$$var_name"
    $top.$key\MB.m add separator
    $top.$key\MB.m add command -label "Color Tool..." -command $user_color_cmd

    # initialize color
    catch [list uplevel #0 [list set $var_name $icolor]]
    set_WidgetRGBColor $top.$key\MB $icolor

    grid $top.$key\E $top.$key\MB -sticky "nsew" -in $top.$key\F
    grid columnconfigure $top.$key\F 0 -weight 1
    grid rowconfigure $top.$key\F 0 -weight 1

    bind $top.$key\E <Return> "set_WidgetRGBColor $top.$key\MB \$$var_name"

    return $top.$key\F
}

proc color_entry_destroy { top key } {
    grid forget $top.$key\F
    destroy $top.$key\F $top.$key\E $top.$key\MB
}

proc color_entry_update { top key icolor } {
    set_WidgetRGBColor $top.$key\MB $icolor
}

proc color_entry_chooser { id top key title vn vin } {
    set child color

    cadColorWidget dialog $top $child\
	    -title $title\
	    -initialcolor [$top.$key\MB cget -background]\
	    -ok "color_entry_ok $id $top $top.$child $key $vn $vin"\
	    -cancel "cadColorWidget_destroy $top.$child"
}

proc color_entry_ok { id top w key vn vin } {
    upvar #0 $w data $vn varname

    $top.$key\MB configure -bg $data(finalColor)

    if {$vin == ""} {
	set varname "$data(red) $data(green) $data(blue)"
    } else {
	set varname($vin) "$data(red) $data(green) $data(blue)"
    }

    destroy $w
    unset data
}

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
