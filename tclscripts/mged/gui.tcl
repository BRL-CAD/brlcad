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
#	Commands to create and destroy an MGED Tcl/Tk interface.
#
# $Revision
#
if ![info exists mged_gui_create] {
    set mged_gui_create gui_create_default
}

if ![info exists mged_gui_destroy] {
    set mged_gui_destroy gui_destroy_default
}

proc gui {args} {
    eval gui_create $args
}

proc gui_create {args} {
    global mged_gui_create

    eval $mged_gui_create $args
}

proc gui_destroy {args} {
    global mged_gui_destroy

    eval $mged_gui_destroy $args
}