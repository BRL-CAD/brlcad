if {![package vsatisfies [package provide Tcl] 8]} {return}
if {[info exists ::tcl_platform(debug)]} {
    package ifneeded registry 1.1.1 \
            [list load [file join $dir tclreg11g.dll] registry]
} else {
    package ifneeded registry 1.1.1 \
            [list load [file join $dir tclreg11.dll] registry]
}
