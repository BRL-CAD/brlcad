namespace eval hv3 {set {version($Id$)} 1}

package require starkit
starkit::startup
set ::HV3_STARKIT 1
source [file join [file dirname [info script]] hv3_main.tcl] 

