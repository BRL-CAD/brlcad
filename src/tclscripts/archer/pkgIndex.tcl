# Tcl package index file, version 1.0

proc LoadArcherLibs {dir} {
    global archerDebug
    global env

    if {[info exists $archerDebug] && $archerDebug} {
	load [file join $env(ARCHER_HOME) bin itcl33_d.dll]
	load [file join $env(ARCHER_HOME) bin itk33_d.dll]
	load [file join $env(ARCHER_HOME) bin BLT24_d.dll]

	# Load Brlcad libraries
	load [file join $env(ARCHER_HOME) bin libbu_d]
	load [file join $env(ARCHER_HOME) bin libbn_d]
	load [file join $env(ARCHER_HOME) bin libsysv_d]
	load [file join $env(ARCHER_HOME) bin librt_d]
	load [file join $env(ARCHER_HOME) bin libdm_d]
	load [file join $env(ARCHER_HOME) bin tkimg_d]
	load [file join $env(ARCHER_HOME) bin pngtcl_d]
	load [file join $env(ARCHER_HOME) bin tkimgpng_d]
    } else {
	load [file join $env(ARCHER_HOME) bin itcl33.dll]
	load [file join $env(ARCHER_HOME) bin itk33.dll]
	load [file join $env(ARCHER_HOME) bin BLT24.dll]

	# Load Brlcad libraries
	load [file join $env(ARCHER_HOME) bin libbu]
	load [file join $env(ARCHER_HOME) bin libbn]
	load [file join $env(ARCHER_HOME) bin libsysv]
	load [file join $env(ARCHER_HOME) bin librt]
	load [file join $env(ARCHER_HOME) bin libdm]
	load [file join $env(ARCHER_HOME) bin tkimg]
	load [file join $env(ARCHER_HOME) bin pngtcl]
	load [file join $env(ARCHER_HOME) bin tkimgpng]
    }

    source [file join $dir Archer.tcl]
}

package ifneeded Archer 1.0 [list LoadArcherLibs $dir]
