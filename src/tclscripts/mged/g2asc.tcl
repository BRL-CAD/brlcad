#                       G 2 A S C . T C L
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
#			G 2 A S C . T C L
#
#	Tool for saving the current database to an ascii file.
#
#	Author - Robert G. Parker
#

proc init_g2asc { id } {
    global mged_gui
    global ::tk::Priv
    global tcl_platform

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
	    "No database has been opened!" info 0 OK
	return
    }

    set top .$id.ascii
    catch { destroy $top }

    # get the name of the ascii database to save
    set db_name [_mged_opendb]
    set default_name [file tail [file rootname $db_name]].asc
    set ftypes {{{Ascii Database} {.asc}} {{All Files} {*}}}
    set filename [tk_getSaveFile -parent .$id -filetypes $ftypes \
                                 -initialdir $mged_gui(databaseDir) \
                                 -initialfile $default_name \
                                 -title "Extract Ascii Database"]

    if { $filename != "" } {  
        # save the current directory for subsequent file saves
        set mged_gui(databaseDir) [ file dirname $filename ]

        # convert binary database to ascii
        set g2asc [bu_brlcad_root "bin/g2asc"]
        catch {exec $g2asc $db_name $filename} msg
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
