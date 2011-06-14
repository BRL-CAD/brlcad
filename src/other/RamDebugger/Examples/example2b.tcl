
proc pp1 { string } {
    puts pp1
    for { set i 0 } { $i < [string length $string] } { incr i } {
	set bb [string index $string $i]
    }
}

################################################################################
# In Windows, it is necessary to load the package comm (not the standard, but the
# one modified in RamDebugger), in order to debug the program remotely
################################################################################

package require commR
comm::register example2 1

################################################################################
#
# Program gets stopped here waiting for the debugger to connect
# NOTE: this command must be at the end of the file, after the definition of all proc
#
################################################################################

commR::wait_for_debugger

################################################################################
#  normal program begins here
#  breakpoints need to be added inside a proc in order to work. Try to put a breakpoint in proc 'pp1'
################################################################################

button .b -text Go -width 10 -command "pp1 aaaaaa"
button .exit -text Exit -width 10 -command exit

pack .b .exit -side left -padx 5

