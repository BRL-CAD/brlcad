
################################################################################
#  normal program begins here
################################################################################

label .l -text "Press <F10> to open the debugger window"
button .b -text Go -width 10 -command "pp1 aaaaaa"
button .exit -text Exit -width 10 -command exit

grid .l - -
grid .b .exit -padx 5

proc pp1 { string } {
    puts pp1
    for { set i 0 } { $i < [string length $string] } { incr i } {
	set bb [string index $string $i]
    }
}

proc pp2 { string } {
    grid [button .nn -text "hello"] -column 2 -row 1
}

bind . <F10> {
    lappend auto_path ..
    package require RamDebugger
}

pp2 eeeee

