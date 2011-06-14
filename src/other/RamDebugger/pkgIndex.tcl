
set Version 7.8

proc LoadRamDebugger { dir version } {
    if { [info exists ::GIDDEFAULT] } {
	GidUtils::WaitState .gid
    }
    set argv ""
    if { [interp exists ramdebugger] } {
	if { [ramdebugger eval info exists argv] } {
	    set argv [ramdebugger eval set argv]
	}
	interp delete ramdebugger
    }
#     interp create ramdebugger
    if { ![interp exists ramdebugger] } {
	interp create ramdebugger
    }
    interp alias ramdebugger master "" eval
    if { [info exists ::GIDDEFAULT] } {
	#loaded inside GiD
	foreach i { _  } {
	    interp alias ramdebugger $i "" $i
	}
    }

    ramdebugger eval [list load {} Tk]
    ramdebugger eval package require Tk
    if { ![ramdebugger eval info exists argv] } {
	ramdebugger eval [list set argc 0]
	ramdebugger eval [list set argv $argv]
    }
    ramdebugger eval [list set is_package 1]
    ramdebugger eval [list set auto_path $::auto_path]
    ramdebugger eval [list set argv0 [file join $dir RamDebugger.tcl]]
    catch { ramdebugger hide exit }
    ramdebugger alias exit EndLoadRamDebugger
    ramdebugger eval [list source [file join $dir RamDebugger.tcl]]
    package provide RamDebugger $version
    update idletasks
    if { [info exists ::GIDDEFAULT] } {
	GidUtils::EndWaitState .gid
    }
}

proc EndLoadRamDebugger {} {
    interp delete ramdebugger
    package forget RamDebugger
}

if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded RamDebugger $Version [list LoadRamDebugger $dir $Version]
