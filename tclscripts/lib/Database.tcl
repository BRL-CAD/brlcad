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

    constructor {file args} {
	Db::constructor $file
	Drawable::constructor [Db::get_name]
    } {
	# process options
	eval configure $args
    }

    destructor {
    }
}
