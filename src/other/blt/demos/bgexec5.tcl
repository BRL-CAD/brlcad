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

set shell bltwish
if { [info exists tcl_platform] && $tcl_platform(platform) == "windows" } {
    set shell "$shell.exe"
}
if { [file executable "../src/$shell"] } {
    set shell "../src/$shell"
}

set count 0
foreach demo [glob barchart?.tcl] {
    bgexec var $shell $demo &
}

button .kill -text "Kill All" -command { set var 0 }
table . .kill -fill both 


