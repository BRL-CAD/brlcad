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
#bltdebug 100

source scripts/stipples.tcl

tabset .t \
    -samewidth yes \
    -side left \
    -textside bottom \
    -textside top \
    -bg red \
    -tiers 1 \
    -scrollincrement 10 \
    -scrollcommand { .s set } \
    -rotate 0 \
    -selectcommand {  MakePhoto %W %n } 


scrollbar .s -command { .t view } -orient horizontal
 
option clear
option add *Tabset.Tab.font -*-helvetica-bold-r-*-*-10-*-*-*-*-*-*-*

set files [glob ./images/*.gif]
set files [lsort $files]
#set vertFilter sinc
#set horzFilter sinc
set vertFilter none
set horzFilter none


proc ResizePhoto { src dest maxSize } {
    set maxSize [winfo fpixels . $maxSize]
    set w [image width $src]
    set h [image height $src]
    set sw [expr double($maxSize) / $w]
    set sh [expr double($maxSize) / $h]
    set s [expr min($sw, $sh)]
    set w [expr round($s * $w)]
    set h [expr round($s * $h)]
    $dest configure -width $w -height $h
    
    global horzFilter vertFilter
    winop resample $src $dest $horzFilter $vertFilter
}

image create photo src
image create photo dest

label .t.label -image dest -bg purple

proc MakePhoto { w name } {
    set file ./images/$name.gif
    src configure -file $file

    set width [$w tab pagewidth]
    set height [$w tab pageheight]
    if { $width < $height } {
	ResizePhoto src dest $width
    } else {
	ResizePhoto src dest $height
    }
    .t tab dockall
    .t tab configure $name -window .t.label -padx 4m -pady 4m -fill both
}

table . \
    .t 0,0 -fill both \
    .s 1,0 -fill x 

table configure . r1 -resize none
focus .t

foreach f $files {
    src configure -file $f
    set f [file tail [file root $f]]
    set thumb [image create photo]
    ResizePhoto src $thumb 0.5i
    .t insert end $f -image $thumb -fill both
}

.t focus 0
.t invoke 0
