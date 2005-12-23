#!../src/bltwish

package require BLT
# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------

if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}

source scripts/demo.tcl

proc ShowResult { name1 name2 how } {
    global var
    .l$name2 configure -text "$var($name2)"
    after 2000 "table forget .l$name2"
}
    
for { set i 1 } { $i <= 20 } { incr i } {
    label .l$i 
    table . .l$i $i,0
    set pid [bgexec var($i) du /usr/include &]
    .l$i configure -text "Starting #$i pid=$pid"
    trace variable var($i) w ShowResult
    update
    after 500
}

