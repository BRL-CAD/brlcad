if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded snit 1.0 \
    [list source [file join $dir snit.tcl]]
