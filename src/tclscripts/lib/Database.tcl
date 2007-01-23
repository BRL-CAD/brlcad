#                    D A T A B A S E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#	The Database class inherits from Db and Drawable.
#
::itcl::class Database {
    inherit Db Drawable

    constructor {dbOrFile} {
	Db::constructor $dbOrFile
	Drawable::constructor [Db::get_dbname]
    } {}

    destructor {}

    public {
	method ? {}
	method apropos {key}
	method help {args}
	method getUserCmds {}
	method shareDb {_db}
    }
}

::itcl::body Database::? {} {
    return "[Db::?]\n[Drawable::?]"
}

::itcl::body Database::apropos {key} {
    return "[Db::apropos $key] [Drawable::apropos $key]"
}

::itcl::body Database::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval Db::help $args} result]} {
	    set result [eval Drawable::help $args]
	}

	return $result
    }

    # list all help messages for Db and Drawable
    return "[Db::help][Drawable::help]"
}

::itcl::body Database::getUserCmds {} {
    return "[Db::getUserCmds] [Drawable::getUserCmds]"
}

::itcl::body Database::shareDb {_db} {
    Db::shareDb $_db
    Drawable::assoc $_db
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
