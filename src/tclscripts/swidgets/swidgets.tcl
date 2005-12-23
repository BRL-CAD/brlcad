 
package require Tcl 8.0
package require Tk 8.0
package require Itcl 3.2
package require Itk 3.2
package require Iwidgets
 
namespace eval ::swidgets {
    namespace export *

    variable library [file dirname [info script]]
    variable version 1.0
}

lappend auto_path [file join $swidgets::library scripts]
package provide Swidgets $swidgets::version
