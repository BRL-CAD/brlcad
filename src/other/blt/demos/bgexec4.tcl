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
source scripts/globe.tcl

option add *HighlightThickness 0

set program ../src/bltwish
if { [info exists tcl_platform ] } {
    puts stderr $tcl_platform(platform)
    if { $tcl_platform(platform) == "windows" } {
        set shells [glob C:/Program\ Files/Tcl/bin/tclsh8*.exe ] 
        set program [lindex $shells 0]
    }
}
if { ![file executable $program] } {
    error "Can't execute $program"
}

set command [list $program scripts/bgtest.tcl]

array set animate {
    index	-1
    interval	200
    colors	"#ff8813 #ffaa13 #ffcc13 #ffff13 #ffcc13 #ffaa13 #ff8813"
    numBitmaps	30
    prefix	globe
}

proc Animate {} {
    global animate
    if { [info commands .logo] != ".logo" } {
	set animate(index) 0
	return
    }
    if { $animate(index) >= 0 } {
	.logo configure -bitmap $animate(prefix).$animate(index) 
	incr animate(index)
	if { $animate(index) >= $animate(numBitmaps) } {
	    set animate(index) 0
	}
	after $animate(interval) Animate
    }
}

proc InsertText { string tag } {
    .text insert end "$tag: " "" $string $tag
    set textlen [expr int([.text index end])]
    scan [.vscroll get] "%s %s %s %s" total window first last
    if { $textlen > $total } {
	.text yview [expr $textlen-$window]
    }
    update idletasks
}

proc DisplayOutput { name1 name2 how } {
    upvar #0 $name1 arr

    InsertText "$arr($name2)\n" stdout
    set arr($name2) {}
}

proc DisplayErrors { name1 name2 how } {
    upvar #0 $name1 arr

    InsertText "$arr($name2)\n" stderr
    set arr($name2) {}
}


option add *text.yScrollCommand { .vscroll set }
option add *text.relief sunken
option add *text.width 20
option add *text.height 10
option add *text.height 10
option add *text.borderWidth 2
option add *vscroll.command { .text yview }
option add *vscroll.minSlider 4p
option add *stop.command { set results {} }
option add *stop.text { stop }
option add *logo.padX 4
option add *title.text "Catching stdout and stderr"
option add *title.font -*-Helvetica-Bold-R-*-*-14-*-*-*-*-*-*-*

set visual [winfo screenvisual .] 
if { [string match *color $visual] } {
    option add *text.background white
    option add *text.foreground blue
    option add *stop.background yellow
    option add *stop.activeBackground yellow2
    option add *stop.foreground navyblue
    option add *start.activeBackground green2
    option add *start.background green
    option add *start.foreground navyblue
    option add *logo.background beige
    option add *logo.foreground brown 
    option add *logo.foreground green4
    option add *title.background lightblue
    option add *logo.background lightblue
}
. configure -bg lightblue

trace variable results(stdout) w DisplayOutput
trace variable results(stderr) w DisplayErrors

proc Start { command } {
    global results animate
    .text delete 1.0 end
    if { $animate(index) < 0 } {
        set results(status) {}
	eval "bgexec results(status) -lasterror results(stderr) \
		-lastoutput results(stdout) $command &"
        set animate(index) 0
        Animate
    }
}

proc Stop { } {
    global results animate
    set results(status) {}
    set animate(index) -1
}

# Create widgets
text .text 
.text tag configure stdout -font -*-Helvetica-Bold-R-*-*-18-*-*-*-*-*-*-* \
    -foreground green2
.text tag configure stderr -font -*-Helvetica-Medium-O-*-*-18-*-*-*-*-*-*-* \
    -foreground red2

scrollbar .vscroll 
button .start -text "Start" -command [list Start $command]
button .stop -text "Stop" -command Stop
label .logo  -bitmap globe.0
label .title

# Layout widgets in table
table . \
    .title      0,0 -columnspan 4 \
    .text 	1,0 -columnspan 3 \
    .vscroll 	1,3 -fill y \
    .logo 	2,0 -anchor w -padx 10 -reqheight .6i -pady 4 \
    .start 	2,1 \
    .stop 	2,2 

set buttonWidth 1i
table configure . c1 c2 -width 1i
table configure . c3 r0 r2 -resize none
table configure . .start .stop -reqwidth $buttonWidth -anchor e
table configure . .title .text -fill both

wm min . 0 0


