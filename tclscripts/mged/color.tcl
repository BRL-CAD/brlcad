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
proc color_entry_build { top key vn user_color_cmd width icolor } {
    frame $top.$key\F -relief sunken -bd 2
    entry $top.$key\E -relief flat -width $width -textvar $vn
    hoc_register_data $top.$key\E "Color"\
	    { { summary "Enter a color specification. The color
can be specified using three integers (i.e. r g b)
in the range 0 to 255. For example:

\tblack\t\t0 0 0
\twhite\t\t255 255 255
\tred\t\t255 0 0
\tgreen\t\t0 255 0
\tblue\t\t0 0 255
\tyellow\t\t255 255 0
\tcyan\t\t0 255 255
\tmagenta\t\t255 0 255

The color can also be specified using a six character
hexadecimal number (i.e. #ff0000 specifies red). And
of course the color can be specified by name. For
example, the name \"blue\" specifies a color with
an rgb value of 0 0 255.

Note - when entering colors directly,
pressing \"Enter\" will update the color
of the menubutton." } }

    menubutton $top.$key\MB -relief raised -bd 2\
	    -menu $top.$key\MB.m -indicatoron 1
    hoc_register_data $top.$key\MB "Color Menu"\
	    { { summary "Pop up a menu of colors. Also included
in the menu is an entry for a color tool." } }
    menu $top.$key\MB.m -title "Color" -tearoff 0
    $top.$key\MB.m add command -label black\
	    -command "set $vn \"0 0 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" black "Color - Black"\
	    { { summary "Black is specified by \"0 0 0\"." } }
    $top.$key\MB.m add command -label white\
	    -command "set $vn \"255 255 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" white "Color - White"\
	    { { summary "White is specified by \"255 255 255\"." } }
    $top.$key\MB.m add command -label red\
	    -command "set $vn \"255 0 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" red "Color - Red"\
	    { { summary "Red is specified by \"255 0 0\"." } }
    $top.$key\MB.m add command -label green\
	    -command "set $vn \"0 255 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" green "Color - Green"\
	    { { summary "Green is specified by \"0 255 0\"." } }
    $top.$key\MB.m add command -label blue\
	    -command "set $vn \"0 0 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" blue "Color - Blue"\
	    { { summary "Blue is specified by \"0 0 255\"." } }
    $top.$key\MB.m add command -label yellow\
	    -command "set $vn \"255 255 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" yellow "Color - Yellow"\
	    { { summary "Yellow is specified by \"255 255 0\"." } }
    $top.$key\MB.m add command -label cyan\
	    -command "set $vn \"0 255 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" cyan "Color - Cyan"\
	    { { summary "Cyan is specified by \"0 255 255\"." } }
    $top.$key\MB.m add command -label magenta\
	    -command "set $vn \"255 0 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    hoc_register_menu_data "Color" magenta "Color - Magenta"\
	    { { summary "Magenta is specified by \"255 0 255\"." } }
    $top.$key\MB.m add separator
    $top.$key\MB.m add command -label "Color Tool..." -command $user_color_cmd
    hoc_register_menu_data "Color" "Color Tool..." "Color Tool"\
	    { { summary "The color tool allows the user to specify
a color using either RGB or HSV. The resulting
color is RGB." } }

    # initialize color
    catch [list uplevel #0 [list set $vn $icolor]]
    setWidgetRGBColor $top.$key\MB $vn $icolor

    grid $top.$key\E $top.$key\MB -sticky "nsew" -in $top.$key\F
    grid columnconfigure $top.$key\F 0 -weight 1
    grid rowconfigure $top.$key\F 0 -weight 1

    bind $top.$key\E <Return> "setWidgetColor $top.$key\MB $vn \$$vn"

    return $top.$key\F
}

proc color_entry_destroy { top key } {
    grid forget $top.$key\F
    destroy $top.$key\F $top.$key\E $top.$key\MB
}

proc color_entry_update { top key vn icolor } {
    if [winfo exists $top.$key\MB] {
	setWidgetColor $top.$key\MB $vn $icolor
    }
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

## -- setWidgetRGBColor
#
# Set the widget color given an rgb string.
#
proc setWidgetRGBColor { w vn rgb } {

    if ![winfo exists $w] {
	return -code error "setWidgetRGBColor: bad Tk window name --> $w"
    }

    if {$rgb != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rgb cmatch red green blue]
	if {!$result} {
	    # reset varname to properly reflect the current color of the widget
	    set varname [lindex [$w configure -bg] 4]
	    return -code error "Improper color specification - $rgb"
	}
    } else {
	return
    }

    $w configure -bg [format "#%02x%02x%02x" $red $green $blue]
}

## -- setWidgetColor
#
# Set the widget color given a color string.
#
proc setWidgetColor { w vn color } {
#    upvar #0 $vn varname

#    if ![winfo exists $w] {
#	return -code error "setWidgetColor: bad Tk window name --> $w"
#    }

    # convert to RGB
#    set result [catch {getRGB $w $color} rgb]
#    if {$result} {
	# reset varname to properly reflect the current color of the widget
#	set varname [lindex [$w configure -bg] 4]
#	return -code error $rgb
#    } else {
#	setWidgetRGBColor $w $vn $rgb
#    }

    set rgb [getRGBorReset $w $vn $color]
    setWidgetRGBColor $w $vn $rgb
}


## -- getRGBorReset
#
# Get the RGB value corresponding to the given color. Failing that
# reset the variable vn to the value provided by the widget w
#
proc getRGBorReset { w vn color } {
    upvar #0 $vn varname

    if ![winfo exists $w] {
	return -code error "getRGBorReset: bad Tk window name --> $w"
    }

    # convert to RGB
    set result [catch {getRGB $w $color} rgb]
    if {$result} {
	# reset varname to properly reflect the current color of the widget
	set varname [lindex [$w configure -bg] 4]
	return -code error $rgb
    }

    return $rgb
}

## -- getRGB
#
# Get the RGB value corresponding to the given color.
#
proc getRGB { w color } {
    # convert to RGB
    set rgb [cadColorWidget_getRGB $w $color]

    # Check to make sure the color is valid
    if {[llength $rgb] == 3} {
	return $rgb
    }

    # not a color recognized by cadColorWidget_getRGB
    set result [regexp "^\[ \]*(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]*$" \
	    $color cmatch red green blue]
    if {!$result ||
        $red < 0 || $red > 255 ||
        $green < 0 || $green > 255 ||
        $blue < 0 || $blue > 255 } {
	return -code error "Improper color specification - $color"
    }

    return $color
}
