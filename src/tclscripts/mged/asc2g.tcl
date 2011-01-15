#                       A S C 2 G . T C L
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
#			A S C 2 G . T C L
#
#	Tool for importing an ascii database to the current database
#
#	Author - Christopher Sean Morrison
#

proc init_asc2g { id } {
    global mged_gui
    global ::tk::Priv
    global tcl_platform

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." "No database has been opened!" info 0 OK
	return
    }

    set top .$id.ascii
    catch { destroy $top }

    # get the name of the ascii database to open
    set ftypes {{{Ascii Database} {.asc}} {{All Files} {*}}}
    set filename [tk_getOpenFile -parent .$id -filetypes $ftypes \
		      -initialdir $mged_gui(databaseDir) \
		      -title "Insert Ascii Database"]

    if { $filename != "" } {
	# save the current directory for subsequent file loads
	set mged_gui(databaseDir) [ file dirname $filename ]

	set ret [cad_input_dialog .$id.prefix $mged_gui($id,screen) "Prefix" "Enter prefix (optional):" prefix "" 0 {{ summary "
Database objects imported from the ascii
database may optionally include an assigned
prefix.  The prefix is prepended to each
object before being inserted into the
current database." } {} } OK Cancel]

	if { $ret == 0 } {
	    if {$prefix == ""} {
		set prefix /
	    }

	    # XXX horrible hack to open a temp file for conversion.
	    # imitating other code in anim.tcl for temp file generation
	    set db_name "./_asc_2_g_temp_"
	    if [ file exists $db_name ] {
		error "Temporary file ($db_name) is in the way (rename, delete, or move it)"
		return
	    }

	    # convert ascii database to binary
	    set asc2g [file join [bu_brlcad_root "bin"] asc2g]
	    catch {exec $asc2g $filename $db_name} msg

	    # concat the binary
	    dbconcat $db_name $prefix
	}

	file delete $db_name
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
