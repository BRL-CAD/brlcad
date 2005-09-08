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

# ----------------------------------------------------------------------
#  EXAMPLE: simple driver for stripchart widget
# ----------------------------------------------------------------------
#  Michael J. McLennan
#  mmclennan@lucent.com
#  Bell Labs Innovations for Lucent Technologies
# ======================================================================
#               Copyright (c) 1996  Lucent Technologies
# ======================================================================

option add *x.range 20.0
option add *x.shiftBy 15.0
option add *bufferElements no
option add *bufferGraph yes
option add *symbol triangle
option add *Axis.lineWidth 1
option add *Axis*Rotate 90
option add *pixels 1.25m
#option add *PlotPad 25
option add *Stripchart.width 6i
#option add *Smooth quadratic
#option add *Stripchart.invertXY yes
#option add *x.descending yes

# ----------------------------------------------------------------------
#  USAGE:  random ?<max>? ?<min>?
#
#  Returns a random number in the range <min> to <max>.
#  If <min> is not specified, the default is 0; if max is not
#  specified, the default is 1.
# ----------------------------------------------------------------------

proc random {{max 1.0} {min 0.0}} {
    global randomSeed

    set randomSeed [expr (7141*$randomSeed+54773) % 259200]
    set num  [expr $randomSeed/259200.0*($max-$min)+$min]
    return $num
}
set randomSeed 14823

# ----------------------------------------------------------------------

toplevel .addSource
wm title .addSource "Add Source"
wm group .addSource .
wm withdraw .addSource
wm protocol .addSource WM_DELETE_WINDOW {.addSource.controls.cancel invoke}

frame .addSource.info
pack .addSource.info -expand yes -fill both -padx 4 -pady 4
label .addSource.info.namel -text "Name:"
entry .addSource.info.name
label .addSource.info.maxl -text "Maximum:"
entry .addSource.info.max
label .addSource.info.minl -text "Minimum:"
entry .addSource.info.min
table .addSource.info \
    .addSource.info.namel 0,0 -anchor e \
    .addSource.info.name 0,1 -fill x \
    .addSource.info.maxl 1,0 -anchor e \
    .addSource.info.max 1,1 -fill x \
    .addSource.info.minl 2,0 -anchor e \
    .addSource.info.min 2,1 -fill x

frame .addSource.color
pack .addSource.color -padx 8 -pady 4
frame .addSource.color.sample -width 30 -height 30 -borderwidth 2 -relief raised
pack .addSource.color.sample -side top -fill both
scale .addSource.color.r -label "Red" -orient vertical \
    -from 100 -to 0 -command source_color
pack .addSource.color.r -side left -fill y
scale .addSource.color.g -label "Green" -orient vertical \
    -from 100 -to 0 -command source_color
pack .addSource.color.g -side left -fill y
scale .addSource.color.b -label "Blue" -orient vertical \
    -from 100 -to 0 -command source_color
pack .addSource.color.b -side left -fill y

proc source_color {args} {
    set r [expr round(2.55*[.addSource.color.r get])]
    set g [expr round(2.55*[.addSource.color.g get])]
    set b [expr round(2.55*[.addSource.color.b get])]
    set color [format "#%2.2x%2.2x%2.2x" $r $g $b]
    .addSource.color.sample configure -background $color
}
source_color

frame .addSource.sep -borderwidth 1 -height 2 -relief sunken
pack .addSource.sep -fill x -pady 4

frame .addSource.controls
pack .addSource.controls -fill x -padx 4 -pady 4
button .addSource.controls.ok -text "OK" -command {
    wm withdraw .addSource
    set name [.addSource.info.name get]
    set color [.addSource.color.sample cget -background]
    set max [.addSource.info.max get]
    set min [.addSource.info.min get]
    if {[catch {source_create $name $color $min $max} err] != 0} {
        puts "error: $err"
    }
}
pack .addSource.controls.ok -side left -expand yes -padx 4
button .addSource.controls.cancel -text "Cancel" -command {
    wm withdraw .addSource
}
pack .addSource.controls.cancel -side left -expand yes -padx 4

set useAxes y

proc source_create {name color min max} {
    global sources

    if {[info exists sources($name-controls)]} {
        error "source \"$name\" already exists"
    }
    if {$max <= $min} {
        error "bad range: $min - $max"
    }

    set unique 0
    set win ".sources.nb.s[incr unique]"
    while {[winfo exists $win]} {
        set win ".sources.nb.s[incr unique]"
    }

    set xvname "xvector$unique"
    set yvname "yvector$unique"
    set wvname "wvector$unique"
    global $xvname $yvname $wvname
#    catch { $xvname delete }
#    catch { $yvname delete }
#    catch { $wvname delete }
    vector $xvname $yvname $wvname

    if {$xvname == "xvector1"} {
        $xvname append 0
    } else {
	xvector1 variable thisVec
        $xvname append $thisVec(end)
    }
    $yvname append [random $max $min]
    $wvname append 0
    
    catch {.sc element delete $name}
    .sc element create $name -x $xvname -y $yvname -color $color 
    if { $name != "default" } {
	.sc axis create $name -title $name \
	    -limitscolor $color -limitsformat "%4.4g" -titlecolor ${color}
	.sc element configure $name -mapy $name
	global useAxes
	lappend useAxes $name
	set count 0
if 1 {
	set yUse {}
	set y2Use {}
	foreach axis $useAxes {
	    if { $count & 1 } {
		lappend yUse $axis
		.sc axis configure $axis -rotate 90
	    } else {
		lappend y2Use $axis
		.sc axis configure $axis -rotate -90
	    }
	    incr count
	}
	.sc y2axis use $y2Use
	.sc yaxis use $yUse
} else {
        .sc yaxis use $useAxes
}
    }
    set cwin .sources.choices.rb$unique
    radiobutton $cwin -text $name \
        -variable choices -value $win -command "
            foreach w \[pack slaves .sources.nb\] {
                pack forget \$w
            }
            pack $win -fill both
        "
    pack $cwin -anchor w

    frame $win
    pack $win -fill x
    label $win.limsl -text "Limits:"
    entry $win.lims
    bind $win.lims <KeyPress-Return> "
        .sc yaxis configure -limits {%%g}
    "
    label $win.smoothl -text "Smooth:"
    frame $win.smooth
    radiobutton $win.smooth.linear -text "Linear" \
        -variable smooth -value linear -command "
            .sc element configure $name -smooth linear
        "
    pack $win.smooth.linear -side left
    radiobutton $win.smooth.step -text "Step" \
        -variable smooth -value step -command "
            .sc element configure $name -smooth step
        "
    pack $win.smooth.step -side left
    radiobutton $win.smooth.natural -text "Natural" \
        -variable smooth -value natural -command "
            .sc element configure $name -smooth natural
        "
    pack $win.smooth.natural -side left
    label $win.ratel -text "Sampling Rate:"
    scale $win.rate -orient horizontal -from 10 -to 1000

    table $win \
        $win.smoothl 0,0 -anchor e \
        $win.smooth 0,1 -fill x -padx 4 \
        $win.limsl 1,0 -anchor e \
        $win.lims 1,1 -fill x -padx 4 \
        $win.ratel 2,0 -anchor e \
        $win.rate 2,1 -fill x -padx 2

    if {$unique != 1} {
        button $win.del -text "Delete" -command [list source_delete $name]
        pack $win.del -anchor w
        table $win $win.del 3,1 -anchor e -padx 4 -pady 4
    }

    $win.rate set 100
    catch {$win.smooth.[.sc element cget $name -smooth] invoke} mesg

    set sources($name-choice) $cwin
    set sources($name-controls) $win
    set sources($name-stream) [after 100 [list source_event $name 100]]
    set sources($name-x) $xvname
    set sources($name-y) $yvname
    set sources($name-w) $wvname
    set sources($name-max) $max
    set sources($name-min) $min
    set sources($name-steady) [random $max $min]

    $cwin invoke
}

proc source_delete {name} {
    global sources

    after cancel $sources($name-stream)
    destroy $sources($name-choice)
    destroy $sources($name-controls)
    unset sources($name-controls)

    set first [lindex [pack slaves .sources.choices] 0]
    $first invoke
}

proc source_event {name delay} {
    global sources

    set xv $sources($name-x)
    set yv $sources($name-y)
    set wv $sources($name-w)
    global $xv $yv $wv

    $xv variable x
    set x(++end) [expr $x(end) + 0.001 * $delay]

    $yv variable y
    if {[random] > 0.97} {
        set y(++end) [random $sources($name-max) $sources($name-min)]
    } else {
        set y(++end) [expr $y(end)+0.1*($sources($name-steady)-$y(end))]
    }
    set val [random]
    if {$val > 0.95} {
        $wv append 2
    } elseif {$val > 0.8} {
        $wv append 1
    } else {
        $wv append 0
    }
    #$wv notify now
    if { [$xv length] > 100 } {
	$xv delete 0
	$yv delete 0
	$wv delete 0
    }
    update
    set win $sources($name-controls)
    set delay [$win.rate get]
    set sources($name-stream) [after $delay [list source_event $name $delay]]
}

# ----------------------------------------------------------------------
frame .mbar -borderwidth 2 -relief raised
pack .mbar -fill x

menubutton .mbar.main -text "Main" -menu .mbar.main.m
pack .mbar.main -side left -padx 4
menu .mbar.main.m
.mbar.main.m add command -label "Add Source..." -command {
    set x [expr [winfo rootx .]+50]
    set y [expr [winfo rooty .]+50]
    wm geometry .addSource +$x+$y
    wm deiconify .addSource
}
.mbar.main.m add separator
.mbar.main.m add command -label "Quit" -command exit

menubutton .mbar.prefs -text "Preferences" -menu .mbar.prefs.m
pack .mbar.prefs -side left -padx 4

menu .mbar.prefs.m
.mbar.prefs.m add cascade -label "Warning Symbol" -menu .mbar.prefs.m.wm
menu .mbar.prefs.m.wm
.mbar.prefs.m add cascade -label "Error Symbol" -menu .mbar.prefs.m.em
menu .mbar.prefs.m.em

foreach sym {square circle diamond plus cross triangle} {
    .mbar.prefs.m.wm add radiobutton -label $sym \
        -variable warningsym -value $sym \
        -command {.sc pen configure "warning" -symbol $warningsym}

    .mbar.prefs.m.em add radiobutton -label $sym \
        -variable errorsym -value $sym \
        -command {.sc pen configure "error" -symbol $errorsym}
}
catch {.mbar.prefs.m.wm invoke "circle"}
catch {.mbar.prefs.m.em invoke "cross"}

# ----------------------------------------------------------------------
stripchart .sc -title "Stripchart" 
pack .sc -expand yes -fill both

.sc xaxis configure -title "Time (s)" -autorange 2.0 -shiftby 0.5
.sc yaxis configure -title "Samples"

frame .sources
frame .sources.nb -borderwidth 2 -relief sunken
label .sources.title -text "Sources:"
frame .sources.choices -borderwidth 2 -relief groove

if 0 {
pack .sources -fill x -padx 10 -pady 4
pack .sources.nb -side right -expand yes -fill both -padx 4 -pady 4
pack .sources.title -side top -anchor w -padx 4
pack .sources.choices -expand yes -fill both -padx 4 -pady 4
}


source_create default red 0 10
source_create temp blue3 0 10
source_create pressure green3 0 200
source_create volume orange3 0 1020
source_create power yellow3 0 0.01999
source_create work magenta3 0 10

Blt_ZoomStack .sc

.sc axis bind Y <Enter> {
    set axis [%W axis get current]
    set detail [%W axis get detail]
    if { $detail == "line" } {
	%W axis configure $axis -background grey 
    }
}
.sc axis bind Y <Leave> {
    set axis [%W axis get current]
    %W axis configure $axis -background ""
}

.sc axis bind Y <ButtonPress-1> {
   set axis [%W axis get current] 
#   scan [%W axis limits $axis] "%%g %%g" min max
#   set min [expr $min + (($max - $min) * 0.1)]
#   set max [expr $max - (($max - $min) * 0.1)]
#   %W axis configure $axis -min $min -max $max
   %W axis configure $axis -logscale yes
}

.sc axis bind Y <ButtonPress-3> {
   set axis [%W axis get current] 
#   %W axis configure $axis -min {} -max {}
   %W axis configure $axis -logscale no
}

