#
# Author -
#	 Robert G. Parker
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
#	Routines for handling database upgrades.
#

## - dbupgrade
#
# -f
# --help
#
proc dbupgrade {args} {
    global mged_gui
    global tkPriv

    # get the name of the current database
    set dbname [opendb]

    if {[dbversion] != 4} {
	return "$dbname is already current!"
    }

    set id [get_player_id_dm [winset]]

    if {[llength $args] == 0} {
	if {[info exists tkPriv]} {
	    # inform and prompt the user to upgrade
	    set result [cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen)\
		    "Would you like to upgrade $dbname?"\
		    "The benefits of this are blah blah blah.
	    Furthermore, blah blah blah.

	    Would you like to upgrade $dbname?"\
		    "" 0 Upgrade Cancel]

	    if {$result == 1} {
		return
	    }
	} else {
	    set_more_default upgrade
	    error "more arguments needed::upgrade or cancel?  \[upgrade\]:"
	}
    } else {
	switch -- $args {
	    "upgrade" -
	    "-f" {
		# nothing to do here, fall through and do upgrade below
	    }
	    "cancel" {
		return
	    }
	    default -
	    "-help" {
		return [help dbupgrade]
	    }
	}
    }

    # find unused file name for tmp database
    for {set i 1} {$i} {incr i} {
	set tmp_dbname dbtmp$i
	if {![file exists $tmp_dbname]} {
	    break
	}
    }

    # open this tmp file in order to close the current database
    opendb $tmp_dbname y

    # find unused file name to hold the original database.
    if {![file exists $dbname\R4]} {
	# use name without numeric suffix
	set db_orig $dbname\R4
    } else {
	for {set i 1} {$i} {incr i} {
	    set db_orig $dbname\R4.$i
	    if {![file exists $db_orig]} {
		break
	    }
	}
    }

    # rename the original database
    file rename $dbname $db_orig

    # get file permissions from original
    set perms [file attributes $db_orig -permissions]

    # dbupgrade converts the original database to the current db format
    catch {exec dbupgrade $db_orig $dbname} ret

    if {[file exists $dbname]} {
	# set file permissions
	file attributes $dbname -permissions $perms
	opendb $dbname y

	# remove tmp file
	file delete $tmp_dbname

	return
    } else {
	# open original database
	opendb $db_orig y

	# remove tmp file
	file delete $tmp_dbname

	return "dbupgrade: $ret\n\treopening $db_orig"
    }
}
