#                    S O L C L I C K . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#

#
#    Preliminary...
#    Ensure that all commands used here but not defined herein
#    are provided by the application
#

set extern_commands "M _mged_M"
foreach cmd $extern_commands {
    catch {auto_load $cmd} val
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "[info script]: Application fails to provide command '$cmd'"
	return
    }
}


proc solclick { } {
    echo "\nShoot a ray by clicking the middle mouse button."
    # Replace mouse event handler
    proc M { up x y } {
	if {$up == 0} {
	    return
	}

	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}

	#	set w .solclick$id
	set w .metasolclick.solclick

	catch { destroy $w }

	set solids [solids_on_ray $x $y]

	#	toplevel $w -screen $mged_gui($id,screen)
	toplevel $w
	wm title $w "Primitive edit"
	set i 0
	foreach solid $solids {
	    button $w.s$i -text [lindex [split $solid /] end] \
		-command "destroy $w; sed $solid"
	    pack $w.s$i -side top -fill x -expand yes
	    incr i
	}
	button $w.s$i -text "Cancel" -command "destroy $w"
	pack $w.s$i -side top -fill x -expand yes
    }
}

proc init_solclick { id } {
    global mged_gui
    global solclick_user

    set w .metasolclick
    if [winfo exists $w] {
	mged_dialog .solclick_dialog $mged_gui($id,screen) "In Use" "The primitive click tool is in use by $solclick_user" info 0 OK
	return
    }

    #    catch { destroy $w }
    set solclick_user $id
    toplevel $w -screen $mged_gui($id,screen)
    wm title $w "My 1st menu"
    button $w.s0 -text "PRIMITIVE CLICK" -command "solclick"
    pack $w.s0 -side top -fill x -fill y -expand yes
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
