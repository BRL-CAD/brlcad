##                 D A T A B A S E . T C L
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
#	The Database class inherits from Db and Drawable.
#
class Database {
    inherit Db Drawable

    constructor {file} {
	Db::constructor $file
	Drawable::constructor [Db::get_dbname]
    } {}

    destructor {}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}
}

body Database::? {} {
    return "[Db::?]\n[Drawable::?]"
}

body Database::apropos {key} {
    return "[Db::apropos $key] [Drawable::apropos $key]"
}

body Database::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval Db::help $args} result]} {
	    set result [eval Drawable::help $args]
	}

	return $result
    }

    # list all help messages for Db and Drawable
    return "[Db::help][Drawable::help]"
}

body Database::getUserCmds {} {
    return "[Db::getUserCmds] [Drawable::getUserCmds]"
}
