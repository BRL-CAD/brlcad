if {![package vsatisfies [package provide Tcl] 8.5]} {return}
package ifneeded msgcat 1.5.2 [list source [file join $dir msgcat.tcl]]
