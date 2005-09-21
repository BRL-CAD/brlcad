
proc LoadArcherLibs {dir} {
    global archerDebug
    global tcl_platform

    if {$tcl_platform(os) == "Windows NT"} {
	if {[info exists $archerDebug] && $archerDebug} {
	    load [file join $dir bin itcl33_d.dll]
	    load [file join $dir bin itk33_d.dll]
	    load [file join $dir bin BLT24_d.dll]

	    # Load Brlcad libraries
	    load [file join $dir bin libbu_d]
	    load [file join $dir bin libbn_d]
	    load [file join $dir bin libsysv_d]
	    load [file join $dir bin librt_d]
	    load [file join $dir bin libdm_d]
	    load [file join $dir bin libpng_d]
	    load [file join $dir bin tkimg_d]
	    load [file join $dir bin tkimgpng_d]
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
	    load [file join $dir bin libpng]
	    load [file join $dir bin tkimg]
	    load [file join $dir bin tkimgpng]
	}
    } else {
	if {[info exists $archerDebug] && $archerDebug} {
	} else {
	    load [file join $dir lib libblt2.4.so]

	    # Load Brlcad libraries
	    #load [file join $dir lib libpng.so]
	    load [file join $dir lib tkimg.so]
	}
    }
}
