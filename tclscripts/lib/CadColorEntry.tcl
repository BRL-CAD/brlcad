##                 C A D C O L O R E N T R Y . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	CadColorEntry instances are used to specify colors.
#

class CadColorEntry {
    inherit CadComboBox

    constructor {args} {}
    destructor {}

    private method chooser {}
    private method setcolor {rgb}
    private method setMBcolor {}
    private method colorok {}
}

body CadColorEntry::constructor {args} {
    $itk_component(menu) add command -label black \
	    -command [code $this setcolor {0 0 0}]
    $itk_component(menu) add command -label white \
	    -command [code $this setcolor {255 255 255}]
    $itk_component(menu) add command -label red \
	    -command [code $this setcolor {255 0 0}]
    $itk_component(menu) add command -label green \
	    -command [code $this setcolor {0 255 0}]
    $itk_component(menu) add command -label blue\
	    -command [code $this setcolor {0 0 255}]
    $itk_component(menu) add command -label yellow \
	    -command [code $this setcolor {255 255 0}]
    $itk_component(menu) add command -label cyan \
	    -command [code $this setcolor {0 255 255}]
    $itk_component(menu) add command -label magenta \
	    -command [code $this setcolor {255 0 255}]
    $itk_component(menu) add separator
    $itk_component(menu) add command -label "Color Tool..." \
	    -command [code $this chooser]

    eval itk_initialize $args
    bind $itk_component(entry) <Return> [code $this setMBcolor]
}

body CadColorEntry::chooser {} {
    cadColorWidget dialog $itk_interior color \
	    -title "Color Chooser" \
	    -initialcolor [$itk_component(menubutton) cget -background] \
	    -ok [code $this colorok] \
	    -cancel "cadColorWidget_destroy $itk_interior.color"
}

body CadColorEntry::setcolor {rgb} {
    if {$rgb != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]*$" \
		$rgb cmatch red green blue]
	if {!$result} {
	    error "Improper color specification - $rgb"
	}
    } else {
	return
    }

    entryset $rgb
    $itk_component(menubutton) configure -bg [format "#%02x%02x%02x" $red $green $blue]
}

body CadColorEntry::setMBcolor {} {
    setcolor [entryget]
}

body CadColorEntry::colorok {} {
    upvar #0 $itk_interior.color data
    setcolor "$data(red) $data(green) $data(blue)"
    cadColorWidget_destroy $itk_interior.color
}
