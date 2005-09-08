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

set file images/qv100.t.gif

if { [file exists $file] } {
    set src [image create photo -file $file]  
} else {
    puts stderr "no image file"
    exit 0
}

set width [image width $src]
set height [image height $src]

option add *Label.font *helvetica*10*
option add *Label.background white

set i 0
foreach r { 0 90 180 270 360 45 } {
    set dest [image create photo]
    winop image rotate $src $dest $r
    label .footer$i -text "$r degrees"
    label .l$i -image $dest
    table . \
	0,$i .l$i \
	1,$i .footer$i
    update
    incr i
}


