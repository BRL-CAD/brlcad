# cscroll.tcl --
#
# This demonstration script creates a simple canvas that can be
# scrolled in two dimensions.
#
# RCS: @(#) $Id$

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

set w .cscroll
catch {destroy $w}
toplevel $w
wm title $w "Scrollable Canvas Demonstration"
wm iconname $w "cscroll"
positionWindow $w
set c $w.c

label $w.msg -font $font -wraplength 4i -justify left -text "This window displays a canvas widget that can be scrolled either using the scrollbars or by dragging with button 2 in the canvas.  If you click button 1 on one of the rectangles, its indices will be printed on stdout."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.grid
scrollbar $w.hscroll -orient horiz -command "$c xview"
scrollbar $w.vscroll -command "$c yview"
canvas $c -relief sunken -borderwidth 2 -scrollregion {-11c -11c 50c 20c} \
	-xscrollcommand "$w.hscroll set" \
	-yscrollcommand "$w.vscroll set"
pack $w.grid -expand yes -fill both -padx 1 -pady 1
grid rowconfig    $w.grid 0 -weight 1 -minsize 0
grid columnconfig $w.grid 0 -weight 1 -minsize 0

grid $c -padx 1 -in $w.grid -pady 1 \
    -row 0 -column 0 -rowspan 1 -columnspan 1 -sticky news
grid $w.vscroll -in $w.grid -padx 1 -pady 1 \
    -row 0 -column 1 -rowspan 1 -columnspan 1 -sticky news
grid $w.hscroll -in $w.grid -padx 1 -pady 1 \
    -row 1 -column 0 -rowspan 1 -columnspan 1 -sticky news


set bg [lindex [$c config -bg] 4]
for {set i 0} {$i < 20} {incr i} {
    set x [expr {-10 + 3*$i}]
    for {set j 0; set y -10} {$j < 10} {incr j; incr y 3} {
	$c create rect ${x}c ${y}c [expr $x+2]c [expr $y+2]c \
		-outline black -fill $bg -tags rect
	$c create text [expr $x+1]c [expr $y+1]c -text "$i,$j" \
	    -anchor center -tags text
    }
}

$c bind all <Any-Enter> "scrollEnter $c"
$c bind all <Any-Leave> "scrollLeave $c"
$c bind all <1> "scrollButton $c"
bind $c <2> "$c scan mark %x %y"
bind $c <B2-Motion> "$c scan dragto %x %y"

proc scrollEnter canvas {
    global oldFill
    set id [$canvas find withtag current]
    if {[lsearch [$canvas gettags current] text] >= 0} {
	set id [expr $id-1]
    }
    set oldFill [lindex [$canvas itemconfig $id -fill] 4]
    if {[winfo depth $canvas] > 1} {
	$canvas itemconfigure $id -fill SeaGreen1
    } else {
	$canvas itemconfigure $id -fill black
	$canvas itemconfigure [expr $id+1] -fill white
    }
}

proc scrollLeave canvas {
    global oldFill
    set id [$canvas find withtag current]
    if {[lsearch [$canvas gettags current] text] >= 0} {
	set id [expr $id-1]
    }
    $canvas itemconfigure $id -fill $oldFill
    $canvas itemconfigure [expr $id+1] -fill black
}

proc scrollButton canvas {
    global oldFill
    set id [$canvas find withtag current]
    if {[lsearch [$canvas gettags current] text] < 0} {
	set id [expr $id+1]
    }
    puts stdout "You buttoned at [lindex [$canvas itemconf $id -text] 4]"
}
