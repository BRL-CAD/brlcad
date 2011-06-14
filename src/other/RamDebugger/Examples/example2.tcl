



################################################################################
# In Windows, it is necessary to load the package comm (not the standard, but the
# one modified in RamDebugger), in order to debug the program remotely
# In Unix, it is not necessary to do anything, only have the send command working
################################################################################

if { $::tcl_platform(platform) == "windows" } {
    lappend ::auto_path ../addons
    package require commR
    comm::register example2 1
} else {
    tk appname example2
}

################################################################################
#  normal program begins here
################################################################################

button .b -text Go -width 10 -command "pp1 aaaaaa"
button .exit -text Exit -width 10 -command exit

pack .b .exit -side left -padx 5

proc pp1 { string } {
    puts pp1
    for { set i 0 } { $i < [string length $string] } { incr i } {
	set bb [string index $string $i]
    }
}
