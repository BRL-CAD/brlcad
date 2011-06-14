if {[catch {package require Tcl 8.5b1}]} return
package ifneeded TclOO 0.6.2  [list load [file join $dir libTclOO0.6.2.so] TclOO]
