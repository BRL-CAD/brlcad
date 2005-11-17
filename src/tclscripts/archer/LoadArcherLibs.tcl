##
#
# Author(s):
#    Bob Parker (SURVICE Engineering Company)
#
# Description:
#    Code to load Archer's shared libraries.
#

proc LoadArcherLibs {dir} {
    global tcl_platform

    if {$tcl_platform(os) == "Windows NT"} {
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
	    load [file join $dir bin libbu]
	    load [file join $dir bin libbn]
	    load [file join $dir bin libsysv]
	    load [file join $dir bin librt]
	    load [file join $dir bin libdm]
	    load [file join $dir bin tkimg]
	}
    } else {
	if {[info exists $Archer::debug] && $Archer::debug} {
	} else {
	    load [file join $dir lib libblt2.4.so]

	    # Load Brlcad libraries
	    #load [file join $dir lib libpng.so]
	    load [file join $dir lib tkimg.so]
	}
    }

    # Try to load Sdb
    if {[catch {package require Sdb 1.1}]} {
	set Archer::haveSdb 0
    } else {
	set Archer::haveSdb 1
    }
}
