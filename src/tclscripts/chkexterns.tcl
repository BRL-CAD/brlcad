#                  C H K E X T E R N S . T C L
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
#			C H E C K _ E X T E R N S
#
#	Ensure that all commands that the caller uses but doesn't define
#	are provided by the application
#
proc check_externs {extern_list} {
    set unsat 0
    set s [info script]
    upvar #0 argv0 app
    set msg ""
    foreach cmd $extern_list {
	if {[info command $cmd] == ""} {
	    # append this string once
	    if {$msg == ""} {
		if {[info exists app]} {
		    append msg "Application '$app' unsuited to use Tcl script '$s':\n"
		} else {
		    append msg "Application unsuited to use Tcl script '$s':\n"
		}

		append msg " Fails to define the following commands: $cmd"
		continue
	    }
	    append msg ", $cmd"
	}
    }
    if {$msg != ""} {
	error $msg
    }
}

#
#			D E L E T E _ P R O C S
#
#		    Silently delete Tcl procedures
#
#	Delete each of the specified procedures that exist.
#	This guard is necessary since Tcl's rename command
#	squawks whenever its argument is not the name of
#	an existing procedure.
#
proc delete_procs {proc_list} {
    foreach proc_name $proc_list {
	if {[string length [info command $proc_name]] != 0} {
	    rename $proc_name {}
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
