#!/bin/sh
#                   M O R P H E D I T . T C L
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Glenn Durfee
#
# Source -
#	SECAD/VLD Computing Consortium, Bldg 394
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005-5066
#
# Description -
#       Utility for creating lines files for the pixmorph utility.
#
# Note: this utility needs to be run under BRL-CAD's "bwish" environment, as
# it needs the PIX image type and the fb_common_file_size functions.

# Use the bwish that's in the same bin directory as morphedit.tcl
# this is a comment \
CADPATH=`dirname $0`
# this is a comment \
BWISH=$CADPATH/bwish
# the next line restarts using bwish \
exec $BWISH "$0" "$@"

set brlcad_path [bu_brlcad_root bin]
set PIXFB [file join $brlcad_path pix-fb]
set PIXMORPH [file join $brlcad_path pixmorph]

set colors(normal) red
set colors(highlighted) orange
set colors(held) yellow
set colors(other) blue
set colors(endpt0) black
set colors(endpt1) white
set POINTRAD 4
set LINEWID 3
set building ""

proc usage {} {
    global argv0

    puts \
 "Usage: [file tail $argv0] \[-w width\] \[-n height\] \[-a align\] picA.pix picB.pix linesfile"
    puts "   where align is one of the following:"
    puts "     horizontal   : images are arranged horizontally in a single window"
    puts "     vertical     : images are arranged vertically in a single window"
    puts "     separate     : images are placed in separate windows"
    exit
}

if {$argc < 3} {
    usage
}

set width 0
set height 0
set align ""

while { $argc > 3 } {
    switch -glob -- [lindex $argv 0] {
	-w { set width [lindex $argv 1];
   	     incr argc -1; set argv [lrange $argv 1 end] }
        -n { set height [lindex $argv 1];
	     incr argc -1; set argv [lrange $argv 1 end] }
        -a { set align [lindex $argv 1];
	     incr argc -1; set argv [lrange $argv 1 end] }
        -w* { scan [lindex $argv 0] -w%d width }
  	-n* { scan [lindex $argv 0] -n%d height }
	-a* { scan [lindex $argv 0] -a%s align }
	-* { puts "Bad option [lindex $argv 0]"; usage }
	default usage
    }
    set argc [expr $argc-1]
    set argv [lrange $argv 1 end]
}

set file0name [lindex $argv 0]
set file1name [lindex $argv 1]
set lfname [lindex $argv 2]

if { $width>0 && $height<=0 } then {
    set height [expr [file size $file0name]/(3*$width)]
}

if { $width<=0 && $height>0 } then {
    set width [expr [file size $file0name]/(3*$height)]
}

if { $width<=0 && $height<=0 } then {
    set result [fb_common_file_size $file0name]
    if { $result=="0 0" } then {
	set result [fb_common_file_size $file1name]
	if { $result=="0 0" } then {
	    puts "Cannot determine dimensions of images.  Use -w or -n."
	    exit
	}
    }
    set width [lindex $result 0]
    set height [lindex $result 1]
}

if { $align=="" } then {
    set maxsize [wm maxsize .]
    set maxwidth [lindex $maxsize 0]
    set maxheight [lindex $maxsize 1]

    if { $width<=$height } then {
	if { [expr 2*$width] > $maxwidth } then {
	    set align separate
	} else {
	    set align horizontal
	}
    } else {
	if { [expr 2*$height] > $maxheight } then {
	    set align separate
	} else {
	    set align vertical
	}
    }
} else {
    switch -- $align {
	h -
	horiz -
	horizontal { set align horizontal }
	v -
	vert -
	vertical { set align vertical }
	s -
	sep -
	separate { set align separate }
	default usage
    }
}

set linesegs {}

if [file exists $lfname]>0 then {
    set lf [open $lfname "r"]

    gets $lf line
    set a [lindex $line 0]
    set b [lindex $line 1]
    set p [lindex $line 2]
    set n [lindex $line 3]

    for {set i 0} {$i<$n} {incr i} {
	gets $lf line
	lappend linesegs $line
    }

    if { [llength $a] < 1 } {
	set a 0.1
    }
    if { [llength $b] < 1 } {
	set b 2.0
    }
    if { [llength $p] < 1 } {
	set p 0.2
    }
    if { [llength $n] < 1 } {
	set n 0
    }


    close $lf
} else {
    set lf [open $lfname "w"]
    close $lf

    set a 0.1
    set b 2.0
    set p 0.2
    set n 0
}

image create photo image0 -format pix-w$width-n$height -file $file0name
image create photo image1 -format pix-w$width-n$height -file $file1name

switch $align {
    horizontal {
	toplevel .edit
	wm title .edit "$file0name -> $file1name"
	set canv(0) [canvas .edit.c0 -width $width -height $height]
	set canv(1) [canvas .edit.c1 -width $width -height $height]
	pack $canv(0) $canv(1) -side left
    }
    vertical {
	toplevel .edit
	wm title .edit "$file0name -> $file1name"
	set canv(0) [canvas .edit.c0 -width $width -height $height]
	set canv(1) [canvas .edit.c1 -width $width -height $height]
	pack $canv(0) $canv(1) -side top
    }
    separate {
	toplevel .edit0
	toplevel .edit1
	wm title .edit0 "1 : $file0name"
	wm title .edit1 "2 : $file1name"
	set canv(0) [canvas .edit0.c -width $width -height $height]
	set canv(1) [canvas .edit1.c -width $width -height $height]
	pack $canv(0)
	pack $canv(1)
    }
}

set imageitem(0) [$canv(0) create image 0 0 -image image0 -anchor nw]
set imageitem(1) [$canv(1) create image 0 0 -image image1 -anchor nw]

proc lineseg2screen { lineseg } {
    global width height

    return [list [expr round([lindex $lineseg 0]*$width)] \
	         [expr round((1-[lindex $lineseg 1])*$height)] \
		 [expr round([lindex $lineseg 2]*$width)] \
		 [expr round((1-[lindex $lineseg 3])*$height)] \
		 [expr round([lindex $lineseg 4]*$width)] \
		 [expr round((1-[lindex $lineseg 5])*$height)] \
		 [expr round([lindex $lineseg 6]*$width)] \
		 [expr round((1-[lindex $lineseg 7])*$height)]]
}

proc screen2lineseg { slineseg } {
    global width height

    return [list [expr double([lindex $slineseg 0])/$width] \
	         [expr 1.0-double([lindex $slineseg 1])/$height] \
		 [expr double([lindex $slineseg 2])/$width] \
		 [expr 1.0-double([lindex $slineseg 3])/$height] \
		 [expr double([lindex $slineseg 4])/$width] \
		 [expr 1.0-double([lindex $slineseg 5])/$height] \
		 [expr double([lindex $slineseg 6])/$width] \
		 [expr 1.0-double([lindex $slineseg 7])/$height]]
}

# create items for existing line segments

set citems {}
foreach line $linesegs {
    scan [lineseg2screen $line] "%d %d %d %d %d %d %d %d" x1A y1A x1B y1B \
	                                                  x2A y2A x2B y2B

    set line1 [$canv(0) create line $x1A $y1A $x1B $y1B -fill $colors(normal) \
                                  -width $LINEWID -tags lineseg]
    set line2 [$canv(1) create line $x2A $y2A $x2B $y2B -fill $colors(normal) \
                                  -width $LINEWID -tags lineseg]
    set pt1A [$canv(0) create rectangle [expr $x1A-$POINTRAD] \
	          [expr $y1A-$POINTRAD] [expr $x1A+$POINTRAD] \
		  [expr $y1A+$POINTRAD] -fill $colors(normal) -tags endpt]
    set pt1B [$canv(0) create rectangle [expr $x1B-$POINTRAD] \
	          [expr $y1B-$POINTRAD] [expr $x1B+$POINTRAD] \
		  [expr $y1B+$POINTRAD] -fill $colors(normal) -tags endpt]
    set pt2A [$canv(1) create rectangle [expr $x2A-$POINTRAD] \
	          [expr $y2A-$POINTRAD] [expr $x2A+$POINTRAD] \
	          [expr $y2A+$POINTRAD] -fill $colors(normal) -tags endpt]
    set pt2B [$canv(1) create rectangle [expr $x2B-$POINTRAD] \
	          [expr $y2B-$POINTRAD] [expr $x2B+$POINTRAD] \
		  [expr $y2B+$POINTRAD] -fill $colors(normal) -tags endpt]

    lappend citems [list [list $line1 $line2] \
	    [list $pt1A $pt2A] [list $pt1B $pt2B]]
}

proc nop_mode {} {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> {}
	bind $canv($i) <B1-Motion> {}
	bind $canv($i) <ButtonRelease-1> {}
	$canv($i) bind endpt <Enter> {}
	$canv($i) bind endpt <Leave> {}
	$canv($i) bind lineseg <Enter> {}
	$canv($i) bind lineseg <Leave> {}
	$canv($i) bind lineseg <Leave> {}
    }
}

proc normal_mode {} {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> "press_normal $i %x %y"
	bind $canv($i) <B1-Motion> {}
	bind $canv($i) <ButtonRelease-1> {}
	$canv($i) bind endpt <Enter> "enter_endpt $i"
	$canv($i) bind endpt <Leave> {}
	$canv($i) bind lineseg <Enter> "enter_lineseg $i"
	$canv($i) bind lineseg <Leave> {}
	$canv($i) bind lineseg <Leave> {}
    }
}

proc selected_endpt_mode { index pindex item itempair itemrecord } {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> \
  	 [list press_endpt $i $index $pindex $item $itempair $itemrecord %x %y]
	bind $canv($i) <B1-Motion> {}
	bind $canv($i) <ButtonRelease-1> {}
	$canv($i) bind endpt <Enter> {}
	$canv($i) bind endpt <Leave> \
	       [list leave_endpt $i $pindex $index $item $itempair $itemrecord]
	$canv($i) bind lineseg <Enter> {}
	$canv($i) bind lineseg <Leave> {}
    }
}

proc held_endpt_mode { index pindex item itempair itemrecord deltax deltay } {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> {}
	bind $canv($i) <B1-Motion> \
	       [list drag_endpt $i $index $pindex $item $itempair $itemrecord \
	                        $deltax $deltay %x %y]
	bind $canv($i) <ButtonRelease-1> \
	       [list release_endpt $i $index $pindex $item $itempair \
	                           $itemrecord $deltax $deltay %x %y]
	$canv($i) bind endpt <Enter> {}
	$canv($i) bind endpt <Leave> {}
	$canv($i) bind lineseg <Enter> {}
	$canv($i) bind lineseg <Leave> {}
    }
}

proc selected_lineseg_mode { index item itempair itemrecord } {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> \
	       [list press_lineseg $i $index $item $itempair $itemrecord %x %y]
	bind $canv($i) <B1-Motion> {}
	bind $canv($i) <ButtonRelease-1> {}
	$canv($i) bind endpt <Enter> {}
	$canv($i) bind endpt <Leave> {}
	$canv($i) bind lineseg <Enter> {}
	$canv($i) bind lineseg <Leave> \
	       [list leave_lineseg $i $index $item $itempair $itemrecord]
    }
}

proc held_lineseg_mode { index item itempair itemrecord dx1 dy1 dx2 dy2 } {
    global canv

    foreach i { 0 1 } {
	bind $canv($i) <ButtonPress-1> {}
	bind $canv($i) <B1-Motion> \
		[list drag_lineseg $i $index $item $itempair $itemrecord \
		                   $dx1 $dy1 $dx2 $dy2 %x %y]
	bind $canv($i) <ButtonRelease-1> \
		[list release_lineseg $i $index $item $itempair $itemrecord \
		                   $dx1 $dy1 $dx2 $dy2 %x %y]
	$canv($i) bind endpt <Enter> {}
	$canv($i) bind endpt <Leave> {}
	$canv($i) bind lineseg <Enter> {}
	$canv($i) bind lineseg <Leave> {}
    }
}

proc find_item { ci tcl_id indexp pindexp itempairp itemrecordp } {
    global citems building
    upvar $indexp index $pindexp pindex $itempairp itempair \
	    $itemrecordp itemrecord

    set index -1
    set pindex -1
    set itempair ""
    set itemrecord ""

    set i 0
    foreach record $citems {
	set j 0
	foreach pair $record {
	    foreach item $pair {
		if { $item==$tcl_id } then {
		    set index $i
		    set pindex $j
		    set itempair $pair
		    set itemrecord $record
		}
	    }
	    incr j
	}
	incr i
    }

    if { $index==-1 } then {
	foreach item $building {
	    if { $item==$tcl_id } then {
		set index building
		set pindex building
		set itempair $building
		set itemrecord ""
	    }
	}
    }
}

proc enter_endpt { ci } {
    global building citems canv colors

    set tcl_id [$canv($ci) find withtag current]
    find_item $ci $tcl_id index pindex itempair itemrecord

    if { $index==-1 } then return

# To prevent infinite loops, disable entry/exit events while raising.

    nop_mode
    foreach pair $itemrecord {
	$canv(0) raise [lindex $pair 0]
	$canv(1) raise [lindex $pair 1]
    }

# Highlight both the point on this canvas and the point on the other canvas.

    $canv(0) itemconfigure [lindex $itempair 0] -fill $colors(highlighted)
    $canv(1) itemconfigure [lindex $itempair 1] -fill $colors(highlighted)

    selected_endpt_mode $index $pindex $tcl_id $itempair $itemrecord
}

proc leave_endpt { ci index pindex item itempair itemrecord } {
    global building citems canv colors

    $canv(0) itemconfigure [lindex $itempair 0] -fill $colors(normal)
    $canv(1) itemconfigure [lindex $itempair 1] -fill $colors(normal)

    normal_mode
}

proc press_endpt { ci index pindex item itempair itemrecord x y } {
    global building citems canv colors POINTRAD

    set coords [$canv($ci) coords $item]
    set xbase [expr [lindex $coords 0]+$POINTRAD]
    set ybase [expr [lindex $coords 1]+$POINTRAD]
    set deltax [expr $x-$xbase]
    set deltay [expr $y-$ybase]

    $canv($ci) itemconfigure $item -fill $colors(held)
    if { $pindex==1 } then { set qindex 2 } else { set qindex 1 }
    $canv($ci) itemconfigure [lindex [lindex $itemrecord $qindex] $ci] \
	    -fill $colors(other)
#    $canv($ci) coords [lindex [lindex $itemrecord 0] $ci] \
#	    [expr $xbase-$POINTRAD] [expr $ybase-$POINTRAD] \
#	    [expr $xbase+$POINTRAD] [expr $ybase+$POINTRAD]

    held_endpt_mode $index $pindex $item $itempair $itemrecord $deltax $deltay
}

proc drag_endpt { ci index pindex item itempair itemrecord dx dy x y } {
    global building citems canv colors POINTRAD

    set nx [expr $x-$dx]
    set ny [expr $y-$dy]

    if { $pindex!="building" } then {
	set coords [$canv($ci) coords [lindex [lindex $itemrecord 0] $ci]]
	set coords [lreplace $coords [expr 2*$pindex-2] [expr 2*$pindex-1] \
		             $nx $ny]
	eval $canv($ci) coords [lindex [lindex $itemrecord 0] $ci] $coords
    }
    $canv($ci) coords $item [expr $nx-$POINTRAD] [expr $ny-$POINTRAD] \
	                    [expr $nx+$POINTRAD] [expr $ny+$POINTRAD]
    $canv($ci) raise $item
}

proc release_endpt { ci index pindex item itempair itemrecord dx dy x y } {
    global building citems canv colors POINTRAD linesegs

    set nx [expr $x-$dx]
    set ny [expr $y-$dy]

    if { $index=="building" } then {
	set building [lreplace $building [expr 2+2*$ci] [expr 3+2*$ci] $nx $ny]
	$canv(0) itemconfigure [lindex $itempair 0] -fill $colors(highlighted)
	$canv(1) itemconfigure [lindex $itempair 1] -fill $colors(highlighted)
	selected_endpt_mode $index $pindex $item $itempair $itemrecord
	return
    }

    set line [lreplace [lineseg2screen [lindex $linesegs $index]] \
	    [expr 4*$ci+2*$pindex-2] [expr 4*$ci+2*$pindex-1] $nx $ny]
    set linesegs [lreplace $linesegs $index $index [screen2lineseg $line]]

    $canv(0) itemconfigure [lindex $itempair 0] -fill $colors(highlighted)
    $canv(1) itemconfigure [lindex $itempair 1] -fill $colors(highlighted)
    if { $pindex==1 } then { set qindex 2 } else { set qindex 1 }
    $canv($ci) itemconfigure [lindex [lindex $itemrecord $qindex] $ci] \
	    -fill $colors(normal)

    eval $canv($ci) coords [lindex [lindex $itemrecord 0] $ci] \
	    [lrange $line [expr 4*$ci] [expr 4*$ci+3]]
    $canv($ci) coords $item [expr $nx-$POINTRAD] [expr $ny-$POINTRAD] \
	                    [expr $nx+$POINTRAD] [expr $ny+$POINTRAD]

    selected_endpt_mode $index $pindex $item $itempair $itemrecord
}

proc enter_lineseg { ci } {
    global building citems canv colors

    set tcl_id [$canv($ci) find withtag current]
    find_item $ci $tcl_id index pindex itempair itemrecord

    if { $index==-1 } then return

# Highlight all parts current item

    nop_mode

    foreach pair $itemrecord {
	$canv(0) raise [lindex $pair 0]
	$canv(1) raise [lindex $pair 1]
    }

    set line [lindex $itemrecord 0]
    set pt0 [lindex $itemrecord 1]
    set pt1 [lindex $itemrecord 2]

    foreach i { 0 1 } {
	$canv($i) itemconfigure [lindex $line $i] -fill $colors(highlighted)
	$canv($i) itemconfigure [lindex $pt0 $i] -fill $colors(endpt0)
	$canv($i) itemconfigure [lindex $pt1 $i] -fill $colors(endpt1)
    }

    selected_lineseg_mode $index $tcl_id $itempair $itemrecord
}

proc leave_lineseg { ci index item itempair itemrecord } {
    global building citems canv colors

    set line [lindex $itemrecord 0]
    set pt0 [lindex $itemrecord 1]
    set pt1 [lindex $itemrecord 2]

    foreach i { 0 1 } {
	$canv($i) itemconfigure [lindex $line $i] -fill $colors(normal)
	$canv($i) itemconfigure [lindex $pt0 $i] -fill $colors(normal)
	$canv($i) itemconfigure [lindex $pt1 $i] -fill $colors(normal)
    }

    normal_mode
}

proc press_lineseg { ci index item itempair itemrecord x y } {
    global building citems canv colors POINTRAD LINEWID n linesegs

    set coords [$canv($ci) coords $item]
    set dx1 [expr $x-[lindex $coords 0]]
    set dy1 [expr $y-[lindex $coords 1]]
    set dx2 [expr $x-[lindex $coords 2]]
    set dy2 [expr $y-[lindex $coords 3]]

    set pt1 [lindex [lindex $itemrecord 1] $ci]
    set pt2 [lindex [lindex $itemrecord 2] $ci]

    $canv($ci) itemconfigure $item -fill $colors(held)
    $canv($ci) itemconfigure $pt1 -fill $colors(held)
    $canv($ci) itemconfigure $pt2 -fill $colors(held)

    held_lineseg_mode $index $item $itempair $itemrecord $dx1 $dy1 $dx2 $dy2
}

proc drag_lineseg { ci index item itempair itemrecord dx1 dy1 dx2 dy2 x y } {
    global canv POINTRAD

    set nx1 [expr $x-$dx1]
    set ny1 [expr $y-$dy1]
    set nx2 [expr $x-$dx2]
    set ny2 [expr $y-$dy2]

    set pt1 [lindex [lindex $itemrecord 1] $ci]
    set pt2 [lindex [lindex $itemrecord 2] $ci]

    $canv($ci) coords $item $nx1 $ny1 $nx2 $ny2
    $canv($ci) coords $pt1 [expr $nx1-$POINTRAD] [expr $ny1-$POINTRAD] \
 	                   [expr $nx1+$POINTRAD] [expr $ny1+$POINTRAD]
    $canv($ci) coords $pt2 [expr $nx2-$POINTRAD] [expr $ny2-$POINTRAD] \
 	                   [expr $nx2+$POINTRAD] [expr $ny2+$POINTRAD]
    $canv($ci) raise $pt1
    $canv($ci) raise $pt2
}

proc release_lineseg { ci index item itempair itemrecord dx1 dy1 dx2 dy2 x y} {
    global canv colors POINTRAD linesegs

    set nx1 [expr $x-$dx1]
    set ny1 [expr $y-$dy1]
    set nx2 [expr $x-$dx2]
    set ny2 [expr $y-$dy2]

    set pt1 [lindex [lindex $itemrecord 1] $ci]
    set pt2 [lindex [lindex $itemrecord 2] $ci]

    set line [lreplace [lineseg2screen [lindex $linesegs $index]] \
	    [expr 4*$ci] [expr 3+4*$ci] $nx1 $ny1 $nx2 $ny2]
    set linesegs [lreplace $linesegs $index $index [screen2lineseg $line]]

    $canv($ci) itemconfigure $item -fill $colors(highlighted)
    $canv($ci) itemconfigure $pt1 -fill $colors(endpt0)
    $canv($ci) itemconfigure $pt2 -fill $colors(endpt1)

    $canv($ci) coords $item $nx1 $ny1 $nx2 $ny2
    $canv($ci) coords $pt1 [expr $nx1-$POINTRAD] [expr $ny1-$POINTRAD] \
 	                   [expr $nx1+$POINTRAD] [expr $ny1+$POINTRAD]
    $canv($ci) coords $pt2 [expr $nx2-$POINTRAD] [expr $ny2-$POINTRAD] \
 	                   [expr $nx2+$POINTRAD] [expr $ny2+$POINTRAD]
    $canv($ci) raise $pt1
    $canv($ci) raise $pt2

    selected_lineseg_mode $index $item $itempair $itemrecord
}

proc press_normal { ci x y } {
    global building citems canv colors POINTRAD LINEWID n linesegs

    set newpt(0) [$canv(0) create rectangle [expr $x-$POINTRAD] \
	    [expr $y-$POINTRAD] [expr $x+$POINTRAD] [expr $y+$POINTRAD] \
	    -fill $colors(held) -tags endpt]
    set newpt(1) [$canv(1) create rectangle [expr $x-$POINTRAD] \
	    [expr $y-$POINTRAD] [expr $x+$POINTRAD] [expr $y+$POINTRAD] \
	    -fill $colors(held) -tags endpt]

    if { $building=="" } then {
	set building [list $newpt(0) $newpt(1) $x $y $x $y]
	held_endpt_mode "building" "building" $newpt($ci) \
		[list $newpt(0) $newpt(1)] {} 0 0
    } else {
	set x1A [lindex $building 2]
	set y1A [lindex $building 3]
	set x1B $x
	set y1B $y
	set x2A [lindex $building 4]
	set y2A [lindex $building 5]
	set x2B $x
	set y2B $y

	nop_mode

	set line0 [$canv(0) create line $x1A $y1A $x1B $y1B \
		-fill $colors(normal) -width $LINEWID -tags lineseg]
	set line1 [$canv(1) create line $x2A $y2A $x2B $y2B \
		-fill $colors(normal) -width $LINEWID -tags lineseg]
	$canv(0) raise $newpt(0)
	$canv(1) raise $newpt(1)

	set newrecord [list [list $line0 $line1] [lrange $building 0 1] \
		[list $newpt(0) $newpt(1)]]
	set newlineseg [screen2lineseg [list $x1A $y1A $x1B $y1B \
		                             $x2A $y2A $x2B $y2B]]

	lappend citems $newrecord
	lappend linesegs $newlineseg

	held_endpt_mode $n 2 $newpt($ci) [list $newpt(0) $newpt(1)] \
		$newrecord 0 0
	incr n
	set building {}
    }
}

normal_mode

frame .f

label .f.alab -text "a="
entry .f.aent -textvar a -width 4
label .f.blab -text "b="
entry .f.bent -textvar b -width 4
label .f.plab -text "p="
entry .f.pent -textvar p -width 4
label .f.nlab -text "n="
label .f.nent -textvar n
pack .f.alab .f.aent .f.blab .f.bent .f.plab .f.pent .f.nlab .f.nent -side left

frame .h
button .h.preview -text "Preview"  -command { exec $PIXMORPH $file0name $file1name $lfname $frac $frac | $PIXFB -w$width -n$height & }
label .h.frac -text "fraction="
set frac 0.5
entry .h.fracent -textvar frac -width 4
pack .h.preview .h.frac .h.fracent -side left

frame .g
button .g.save -text "Save" -command save
button .g.quit -text "Quit" -command exit
pack .g.save .g.quit -side left -fill both -expand yes

pack .f .h .g -side top -fill both -expand yes

proc save {} {
    global a b p n linesegs lf lfname

    set lf [open $lfname "w"]

    puts $lf "$a $b $p $n"
    foreach line $linesegs {
	puts $lf $line
    }

    close $lf
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
