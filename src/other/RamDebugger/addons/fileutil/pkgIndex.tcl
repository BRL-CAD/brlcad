if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded fileutil 1.14.4 [list source [file join $dir fileutil.tcl]]

if {![package vsatisfies [package provide Tcl] 8.3]} {return}
package ifneeded fileutil::traverse 0.4.1 [list source [file join $dir traverse.tcl]]

if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded fileutil::multi     0.1   [list source [file join $dir multi.tcl]]
package ifneeded fileutil::multi::op 0.5.3 [list source [file join $dir multiop.tcl]]
