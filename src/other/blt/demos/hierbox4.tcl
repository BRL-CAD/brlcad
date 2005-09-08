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

proc AddDirEntries { w dir } {
    if { [file isdirectory $dir] } {
	set files [glob -nocomplain $dir/*] 
	eval $w insert end [lsort $files]
	set subdirs [glob -nocomplain $dir/*/]
	eval $w entry configure [lsort $subdirs] -gadget yes
    }
}

#blt::bltdebug 100

image create photo openFolder -file images/open.gif
image create photo closeFolder -file images/close.gif

option add *Hierbox.icons "closeFolder openFolder"

option add *Hierbox.cursor crosshair
option add *Hierbox.Button.Relief solid
option add *Hierbox.Button.ActiveBackground white
option add *Hierbox.Button.Background white

hierbox .h  \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set } \
    -activebackground lightskyblue1 \
    -selectbackground lightskyblue2

scrollbar .vs -orient vertical -command { .h yview }
scrollbar .hs -orient horizontal -command { .h xview }
button .test -text Test -command {
    set index [.h curselection]
    set names [eval .h get -full $index]
    puts "selected names are $names"
}

table . \
    0,0 .h  -fill both \
    0,1 .vs -fill y \
    1,0 .hs -fill x \
    2,0 .test 

table configure . c1 r1 -resize none

.h configure -autocreate yes  -font { Helvetica 34 }
focus .h
.h insert end { The Quick Brown Fox Jumped Over the }
.h entry configure root -label {[Root]}
.h insert end { The\nQuick\nBrown\nFox\nJumped\nOver\nthe }

.h configure -focusedit yes
