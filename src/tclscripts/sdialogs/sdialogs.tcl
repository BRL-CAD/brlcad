 
package require Tcl 8.0
package require Tk 8.0
package require Itcl 3.2
package require Itk 3.2
package require Iwidgets
 
namespace eval ::sdialogs {
    namespace export *

    variable library [file dirname [info script]]
    variable version 1.0
}

lappend auto_path [file join $sdialogs::library scripts]
package provide Sdialogs $sdialogs::version
