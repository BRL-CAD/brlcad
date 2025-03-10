#                   D B U P G R A D E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2025 United States Government as represented by
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
# Description -
#	Routines for handling database upgrades.
#
# Acknowledgements -
#	The dbupgrade(message) string was written by Lee Butler.
#

namespace eval ::tk {}
if {![info exists ::tk::Priv(cad_dialog)]} {
    set ::tk::Priv(cad_dialog) .cad_dialog
}

set dbupgrade_priv(message) "
Release 6.0 of BRL-CAD introduced a new geometry file format with
several new features.

To convert an older geometry file to the new format click the
\"upgrade\" button below.  The existing file will be saved with an
extension of \".R4.g\" added to the filename.  The upgraded file will
be given the original filename.

The primary benefits of the new file format to the user include:

    Machine independent format:
	The new file format is machine independent.  A database can be used
	on a machine of any architecture without any conversion.  There is
	no need to convert files with g2asc/asc2g when moving files between
	machines.

	In previous releases when a geometry database was moved from one
	machine to another of a different architecture (CPU), the database had
	to be converted from binary to ASCII format, moved, and converted from
	ASCII to binary on the new machine.

    Double precision floating point values for all parameters:
	The new format stores all floating point values in IEEE 754 double
	precision form.  This increases the range and precision of numbers
	which can be represented.  The result is that more accurate geometry
	can be created, and larger objects can be modeled.

	In previous releases, many parameters were stored in single precision
	floating point values.  While this reduced storage requirements, it
	has become a severe limitation on modeling.

    Unlimited length object names:
	The new file format uses unlimited length strings for all
	database objects.  This permits much more descriptive names to be
	used by modelers.

	In previous releases, object names were limited to 15
	characters in length.

    Object attributes:
	The new file format allows arbitrary text information to be stored
	with each database object.  These text attributes take the form of
	<name> <value> pairs.  Examples of possible use include assigning
	a stock number to a \"StockNumber\" attribute, or
	\"left roadwheel #1\" to a \"MUVES_Component\" attribute.  See the \"attr\"
	command for more information.

    Binary objects:
	Arbitrary data can now be stored into and extracted
	from the database.  This data might be used for geometric
	purposes (such as elevation data for DSP primitives, or
	textures for the texture shader).  It might simply be data
	that the user wants to keep with the geometry (such as the
	original commercial CAD representation from which the model
	was constructed).  Alternatively, it might be data that an
	outboard processing module needs (such as the MUVES region-map
	file contents).  See the \"binary\" command for more information.

	Previously, such information had to be stored in ancillary files.

    Hidden objects:
	Objects can now be \"hidden\" from the default listing of file
	contents.  This allows old geometry to be kept for historical
	reference without cluttering the current listings.  See the \"hide\"
	command for more information.


BRL-CAD retains the ability to edit and use geometry files created
with previous releases (4.0+) of the package.  The features mentioned
above, however, are not be available when using geometry files
retained in the old format.

Support for the previous file format may be removed in a future release.
"

## - dbupgrade
#
# -f
# -help
#
proc dbupgrade {args} {

    global mged_gui
    global dbupgrade_priv
    global tcl_platform

    set ws [winset]
    set id [get_player_id_dm $ws]

    # first time through only
    if {![info exists dbupgrade_priv(dbname)]} {
	if {[opendb] == ""} {
	    if {[info exists .cad_dialog]} {
		cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		    "No database has been opened!" info 0 OK
		return
	    } else {
		error "No database has been opened!"
	    }
	}

	if {[dbversion] > 4} {
	    error "[opendb] is already current!"
	}

	set dbupgrade_priv(dbname) [opendb]

	# find unused file name for tmp database
	for {set i 1} {$i} {incr i} {
	    set dbupgrade_priv(tmp_dbname) dbtmp$i
	    if {![file exists $dbupgrade_priv(tmp_dbname)]} {
		break
	    }
	}
    }

    set dbname "$dbupgrade_priv(dbname)"
    set tmp_dbname "$dbupgrade_priv(tmp_dbname)"
    set overwrite 0

    if {[llength $args] == 0} {
	# inform and prompt the user to upgrade

	if {[info exists .cad_dialog]} {
	    set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
			    "Would you like to upgrade $dbname?" \
			    $dbupgrade_priv(message) \
			    "" 0 Upgrade Cancel]

	    if {$result == 1} {
		unset dbupgrade_priv(dbname)
		return
	    }
	} else {
	    set_more_default help
	    error "more arguments needed::upgrade, cancel, or help? \[default: help\] "
	}
    } else {
	# process user's response to prompting

	# second and third time through
	switch -- [lindex $args 0] {
	    "upgrade" -
	    "-f" {
		# nothing to do here, fall through and do upgrade below
	    }
	    "cancel" {
		unset dbupgrade_priv(dbname)
		return "dbupgrade cancelled"
	    }
	    default -
	    "help" -
	    "-help" {
		puts "$dbupgrade_priv(message)"
		unset dbupgrade_priv(dbname)
		return [help dbupgrade]
	    }
	}

	# third time through only
	if {[llength $args] == 2} {
	    switch -- [lindex $args 1] {
		"y" -
		"Y" -
		"yes" -
		"yeS" -
		"yEs" -
		"yES" -
		"Yes" -
		"YeS" -
		"YEs" -
		"YES" {
		    set overwrite 1
		}
		default {
		    # restore original database
		    puts "dbupgrade cancelled.  Reopening database \[$dbname\]"

		    opendb "$dbname" y

		    # remove tmp file
		    file delete "$tmp_dbname"

		    unset dbupgrade_priv(dbname)
		    return
		}
	    }
	}
    }

    # open this tmp database file in order to close the current database
    opendb "$tmp_dbname" y

    set suffix ".R4.g"

    # find unused file name to hold the original database.
    if {![file exists "$dbname$suffix"]} {
	# use name without numeric suffix
	set db_orig "$dbname$suffix"
    } else {
	# make sure $dname$suffix is not a directory
	if {[file type "$dbname$suffix"] == "directory"} {
	    # restore original database
	    opendb "$dbname" y

	    unset dbupgrade_priv(dbname)
	    error "dbupgrade: $dbname$suffix already exists and is a directory!"
	}

	# when overwrite is 0, the user hasn't been prompted to overwrite yet
	if {!$overwrite} {
	    # not a directory, so prompt the user about overwriting
	    if {[info exists .cad_dialog]} {
		set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
				"About to overwrite $dbname$suffix"\
				"Would you like to overwrite $dbname$suffix"\
				"" 0 Overwrite Cancel]

		if {$result == 1} {
		    # restore original database
		    opendb "$dbname" y

		    unset dbupgrade_priv(dbname)
		    return
		}

	    } else {
		set_more_default n
		error "more arguments needed::overwrite $dbname$suffix \[y|n\]? \[default: n\] "
	    }
	}

	set db_orig "$dbname$suffix"
    }

    # rename the original database
    file rename -force "$dbname" "$db_orig"

    # get file permissions from original
    if { [catch {set perms [file attributes "$db_orig" -permissions]}] } {
	# windows doesn't understand -permissions
	if { [catch {set perms [file attributes "$db_orig" -readonly]}] } {
	    # wtf
	    set perms 0
	}
    }

    # make original read-only
    if { [catch {file attributes "$db_orig" -permissions 0440}] } {
	# windows doesn't understand -permissions
	catch {file attributes "$db_orig" -readonly 1}
    }

    # dbupgrade converts the original database to the current db format
    set dbupgrade_cmd [file join [bu_dir bin] dbupgrade]
    catch {exec "$dbupgrade_cmd" "$db_orig" "$dbname"} ret

    if {![file exists "$dbname"]} {
	# Something went wrong with the conversion, so
	# put things back the way they were.

	# rename original to previous name
	file rename -force "$db_orig" "$dbname"

	error "dbupgrade: $ret\nreopening $dbname"
    }

    # set file permissions to match original state
    if { [catch {file attributes "$dbname" -permissions $perms}] } {
	# windows doesn't understand -permissions
	catch {file attributes "$dbname" -readonly $perms}
    }

    # reopen original or new db
    opendb "$dbname" y

    # remove tmp file
    catch {file delete "$tmp_dbname"}

    unset dbupgrade_priv(dbname)
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
