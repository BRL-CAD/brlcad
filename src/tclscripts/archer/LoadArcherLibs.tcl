#              L O A D A R C H E R L I B S . T C L
# BRL-CAD
#
# Copyright (c) 2006-2007 United States Government as represented by
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
# Author(s):
#    Bob Parker (SURVICE Engineering Company)
#
# Description:
#    Code to load Archer's shared libraries.
###

proc LoadArcherLibs {dir} {
    global tcl_platform

    if {$tcl_platform(platform) == "windows"} {
	if {[info exists Archer::debug] && $Archer::debug} {
	    load [file join $dir bin itcl33_d.dll]
	    load [file join $dir bin itk33_d.dll]
	    load [file join $dir bin BLT24_d.dll]

	    # Load Brlcad libraries
	    load [file join $dir bin libbu_d]
	    load [file join $dir bin libbn_d]
	    load [file join $dir bin libsysv_d]
	    load [file join $dir bin librt_d]
	    load [file join $dir bin libdm_d]
	    load [file join $dir bin tkimg_d]
	} else {
	    load [file join $dir bin itcl33.dll]
	    load [file join $dir bin itk33.dll]
	    load [file join $dir bin BLT24.dll]

	    # Load Brlcad libraries
	    load [file join $dir bin libbu.dll]
	    load [file join $dir bin libbn.dll]
	    load [file join $dir bin libsysv.dll]
	    load [file join $dir bin librt.dll]
	    load [file join $dir bin libdm.dll]
	    load [file join $dir bin tkimg.dll]
	}
    }

    # load tkimg
    set tkimgdir [bu_brlcad_root "lib"]
    if {![file exists $tkimgdir]} {
	set tkimgdir [file join [bu_brlcad_data "src"] other tkimg .libs]
    }

    # can't use sharedlibextension without changing tkimg build
    if {![file exists [file join $tkimgdir tkimg.so]]} {
	puts "ERROR: Unable to initialize Archer imagery"
	exit 1
    }

    load [file join $tkimgdir tkimg.so]

    # load blt
    set bltdir [bu_brlcad_root "lib"]
    if {![file exists $bltdir]} {
	set bltdir [file join [bu_brlcad_data "src"] other blt .libs]
    }

    set bltlibname libBLT[info sharedlibextension]
    if {![file exists [file join $bltdir $bltlibname]]} {
	puts "ERROR: Unable to initialize Archer interface"
	exit 2
    }

    load [file join $bltdir $bltlibname]

    # Try to load Sdb
    if {[catch {package require Sdb 1.1}]} {
	set Archer::haveSdb 0
    } else {
	set Archer::haveSdb 1
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
