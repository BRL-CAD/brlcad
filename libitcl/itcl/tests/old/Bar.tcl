#
# Old test suite for [incr Tcl] v1.5
# ----------------------------------------------------------------------
#   AUTHOR:  Michael J. McLennan
#            Bell Labs Innovations for Lucent Technologies
#            mmclennan@lucent.com
#            http://www.tcltk.com/itcl
#
#      RCS:  $Id$
# ----------------------------------------------------------------------
#            Copyright (c) 1993-1998  Lucent Technologies, Inc.
# ======================================================================
# See the file "license.terms" for information on usage and
# redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.

itcl_class Bar {
	#
	#  Constructor/destructor add their name to a global var for
	#  tracking implicit constructors/destructors
	#
	constructor {config} {
		global WATCH
		lappend WATCH [namespace current]
	}
	destructor {
		global WATCH
		lappend WATCH [namespace current]
	}

	method config {config} {
		return $config
	}

	#
	#  Define variables that will be shadowed by another class.
	#
	public blit
	protected _blit
}
