#
#                       F O N T . T C L
#
# Author -
#	Robert G. Parker
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
# Description -
#	A tool for configuring the font of various GUI components.
#

if ![info exists mged_default(text_font)] {
    set mged_default(text_font) {
	text_font -family courier -size 12 -weight normal -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(menu_font)] {
    set mged_default(menu_font) {
	menu_font -family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(button_font)] {
    set mged_default(button_font) {
	button_font -family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(list_font)] {
    set mged_default(list_font) {
	list_font -family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

if ![info exists mged_default(label_font)] {
    set mged_default(label_font) {
	label_font -family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0
    }
}

## - mged_font_init
#
# Create fonts used by GUI components.
#
proc mged_font_init {} {
    global mged_default

    eval font create $mged_default(text_font)
    eval font create $mged_default(menu_font)
    eval font create $mged_default(button_font)
    eval font create $mged_default(list_font)
    eval font create $mged_default(label_font)

    option add *Text.font text_font
    option add *Menu.font menu_font
    option add *Button.font button_font
    option add *List.font list_font
    option add *Label.font label_font
}
