#                       G 2 A S C . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
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

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.ascii
    catch { destroy $top }

    set db_name [_mged_opendb]
    set default_name [file rootname $db_name].asc
    set ret [cad_input_dialog $top $mged_gui($id,screen) "Save as Ascii"\
	    "Enter ascii filename:" ascii_filename\
	    $default_name 0 {{ summary "Enter a filename to indicate where
to put the acsii converted database."}} OK Cancel]


    if { $ascii_filename != "" } {
	if { $ret == 0 } {
	    if [file exists $ascii_filename] {
		set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
			"Overwrite $ascii_filename?"\
			"Overwrite $ascii_filename?"\
			"" 0 OK Cancel]

		if { $result } {
		    return
		}
	    }

	    catch {exec g2asc $db_name $ascii_filename} msg
	}
    } else {
	if { $ret == 0 } {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "No file name specified!"\
		    "No file name specified!"\
		    "" 0 OK
	}
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
