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

set saved [pwd]

#blt::bltdebug 100

image create photo bgTexture -file ./images/rain.gif

set imageList {}
foreach f [glob ./images/mini-*.gif] {
    lappend imageList [image create photo -file $f]
}

#option add *Hiertable.Tile	bgTexture
#option add *Hiertable.Column.background grey90
option add *Hiertable.ScrollTile  yes
option add *Hiertable.titleShadow { grey80 }
option add *Hiertable.titleFont {*-helvetica-bold-r-*-*-11-*-*-*-*-*-*-*}

option add *xHiertable.openCommand	{
    set path /home/gah/src/blt/%P
    if { [file isdirectory $path] } {
	cd $path
	set files [glob -nocomplain * */. ]
	if { $files != "" } {
	    eval %W insert -at %n end $files
	}
    }
}

option add *xHiertable.closeCommand {
    eval %W delete %n 0 end
}

hiertable .h  -hideroot no -width 0 \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set }  \
    -selectmode single -hideleaves false
    

.h column configure treeView -text View
.h column insert 0 mtime atime gid 
.h column insert end nlink mode type ctime uid ino size dev
.h column configure uid -background \#eaeaff -relief raised -bd 1
.h column configure mtime -hide no -bg \#ffeaea -relief raised -bd 1
.h column configure size gid nlink uid ino dev -justify right -edit yes
.h column configure treeView -hide no -edit no
scrollbar .vs -orient vertical -command { .h yview }
scrollbar .hs -orient horizontal -command { .h xview }
table . \
    0,0 .h  -fill both \
    0,1 .vs -fill y \
    1,0 .hs -fill x

proc FormatSize { size } {
   set string ""
   while { $size > 0 } {
       set rem [expr $size % 1000]
       set size [expr $size / 1000]
       if { $size > 0 } {
           set rem [format "%03d" $rem]
       } 
       if { $string != "" } {
           set string "$rem,$string"
       } else {
           set string "$rem"
       }
   } 
   return $string
}

array set modes {
   0	---
   1    --x
   2    -w-
   3    -wx
   4    r-- 
   5    r-x
   6    rw-
   7    rwx
}

proc FormatMode { mode } {
   global modes

   set mode [format %o [expr $mode & 07777]]
   set owner $modes([string index $mode 0])
   set group $modes([string index $mode 1])
   set world $modes([string index $mode 2])

   return "${owner}${group}${world}"
}

table configure . c1 r1 -resize none
image create photo fileImage -file images/stopsign.gif
proc DoFind { dir path } {
    global fileList count 
    set saved [pwd]

    cd $dir
    foreach f [lsort [glob -nocomplain *]] {
	set entry [file join $path $f]
	if { [catch { file stat $entry info }] != 0 } {
	    lappend fileList $entry 
	} else {
	    if 0 {
	    if { $info(type) == "file" } {
		set info(type) @fileImage
	    } else {
		set info(type) ""
	    }
	    }
	    set info(mtime) [clock format $info(mtime) -format "%b %d, %Y"]
	    set info(atime) [clock format $info(atime) -format "%b %d, %Y"]
	    set info(ctime) [clock format $info(ctime) -format "%b %d, %Y"]
            set info(size)  [FormatSize $info(size)]
	    set info(mode)  [FormatMode $info(mode)]
	    lappend fileList $entry -data [array get info]
	}
	incr count
	if { [file type $f] == "directory" } {
	    DoFind $f $entry
	}
    }
    cd $saved
}

proc Find { dir } {
    global fileList count
    set fileList {}
    catch { file stat $dir info }
    incr count
    lappend fileList $dir -data [array get info]
    DoFind $dir $dir
    return $fileList
}

proc GetAbsolutePath { dir } {
    set saved [pwd]
    cd $dir
    set path [pwd] 
    cd $saved
    return $path
}

set top [GetAbsolutePath ..]
set trim "$top"

.h configure -separator "/" -trim $trim

set count 0
.h entry configure root -label [file tail [GetAbsolutePath $top]] 
.h configure -bg grey90
regsub -all {\.\./*} [Find $top] {} fileList
puts "$count entries"
eval .h insert end $fileList
.h configure -bg white

focus .h

set nodes [.h find -glob -name *.c]
eval .h entry configure $nodes -foreground green4
set nodes [.h find -glob -name *.h]
eval .h entry configure $nodes -foreground cyan4
set nodes [.h find -glob -name *.o]
eval .h entry configure $nodes -foreground red4

cd $saved
#bltdebug 100

toplevel .top
hiertable .top.h2 -tree .h -yscrollcommand { .top.sbar set }
scrollbar .top.sbar -command { .top.h2 yview }
pack .top.h2 -side left -expand yes -fill both
pack .top.sbar -side right -fill y

.h column bind all <ButtonRelease-3> {
    %W configure -flat no
}

proc SortColumn { column } {
    set old [.h sort cget -column] 
    set decreasing 0
    if { "$old" == "$column" } {
	set decreasing [.h sort cget -decreasing]
	set decreasing [expr !$decreasing]
    }
    .h sort configure -decreasing $decreasing -column $column -mode integer
    .h configure -flat yes
    .h sort auto yes

    blt::busy hold .h
    update
    blt::busy release .h
}

foreach column [.h column names] {
    .h column configure $column -command [list SortColumn $column]
}

scale .s -from 0 -to 300 -orient horizontal -length 300
table . \
    3,0 .s
update
.s set 20
if 1 {
    .s configure -command { .h entry configure 0 -height }
}

