#!../src/bltwish

#package require BLT
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
option add *Scrollbar.relief	flat
set oldLabel "dummy"

proc RunDemo { program } {
    if { ![file executable $program] } {
	return
    }
    set cmd [list $program -name "demo:$program" -geom -4000-4000]
    global programInfo
    if { [info exists programInfo(lastProgram)] } {
	set programInfo($programInfo(lastProgram)) 0
    }
    eval bgexec programInfo($program) $cmd &
    set programInfo(lastProgram) $program
    puts stderr [.top.tab.f1 search -name demo:$program]
    .top.tab.f1 configure -name demo:$program
}

frame .top
hierbox .top.hier -separator "." -xscrollincrement 1 \
    -yscrollcommand { .top.yscroll set } -xscrollcommand { .top.xscroll set } \
    -selectcommand { 
	set index [.top.hier curselection]
	set label [.top.hier entry cget $index -label]
	.top.title configure -text $label
	.top.tab tab configure Example -window .top.tab.f1 
	if { $label != $oldLabel }  {
	    RunDemo $label
	}
    }
	

scrollbar .top.yscroll -command { .top.hier yview }
scrollbar .top.xscroll -command { .top.hier xview } -orient horizontal
label .top.mesg -relief groove -borderwidth 2 
label .top.title -text "Synopsis" -highlightthickness 0
tabset .top.tab -side bottom -relief flat -bd 0 -highlightthickness 0 \
    -pageheight 4i

.top.tab insert end \
    "Example" \
    "See Code" \
    "Manual"
 
set pics /DOS/f/gah/Pics
set pics /home/gah/Pics
image create photo dummy -file $pics/Ex1.gif
image create photo graph.img -width 50 -height 50
winop resample dummy graph.img box box

image create photo dummy -file $pics/Ex11.gif
image create photo barchart.img -width 50 -height 50
winop resample dummy barchart.img box box

.top.hier entry configure root -label "BLT"
.top.hier insert end \
    "Plotting" \
    "Plotting.graph" \
    "Plotting.graph.graph" \
    "Plotting.graph.graph2" \
    "Plotting.graph.graph3" \
    "Plotting.graph.graph4" \
    "Plotting.graph.graph5" \
    "Plotting.graph.graph6" \
    "Plotting.barchart" \
    "Plotting.barchart.barchart1" \
    "Plotting.barchart.barchart2" \
    "Plotting.barchart.barchart3" \
    "Plotting.barchart.barchart4" \
    "Plotting.barchart.barchart5" \
    "Plotting.stripchart" \
    "Plotting.vector" \
    "Composition" \
    "Composition.htext" \
    "Composition.table" \
    "Composition.tabset" \
    "Composition.hierbox" \
    "Miscellaneous" \
    "Miscellaneous.busy" \
    "Miscellaneous.bgexec" \
    "Miscellaneous.watch" \
    "Miscellaneous.bltdebug" 
.top.hier open -r root
.top.hier entry configure root -labelfont *-helvetica*-bold-r-*-18-* \
    -labelcolor red -labelshadow red3
.top.hier entry configure "Plotting" "Composition" "Miscellaneous" \
    -labelfont *-helvetica*-bold-r-*-14-* \
    -labelcolor blue4 -labelshadow blue2

.top.hier entry configure "Plotting.graph" \
    -labelfont *-helvetica*-bold-r-*-14-* -label "X-Y Graph"
.top.hier entry configure "Plotting.barchart" \
    -labelfont *-helvetica*-bold-r-*-14-* -label "Bar Chart"

.top.hier entry configure "Plotting.stripchart" \
    -labelfont *-helvetica*-bold-r-*-14-* -label "X-Y Graph"
.top.hier entry configure "Plotting.stripchart" \
    -labelfont *-helvetica*-bold-r-*-14-* -label "Strip Chart"

.top.hier entry configure "Plotting.graph" -icon graph.img
.top.hier entry configure "Plotting.barchart" -icon barchart.img

table .top \
    0,0 .top.hier -fill both -rspan 2 \
    0,1 .top.yscroll -fill y -rspan 2 \
    0,2 .top.mesg -padx 2 -pady { 8 2 } -fill both \
    0,2 .top.title -anchor nw -padx { 8 8 }  \
    1,2 .top.tab -fill both -rspan 2 \
    2,0 .top.xscroll -fill x 

table configure .top c1 r2 -resize none
table configure .top c0 -width { 3i {} }
table configure .top c2 -width { 4i {} }
table . \
    .top -fill both

proc DoExit { code } {
    global progStatus
    set progStatus 1
    exit $code
}

container .top.tab.f1 -relief raised -bd 2 -takefocus 0
.top.tab tab configure Example -window .top.tab.f1 

if  1 {
    set cmd "xterm -fn fixed -geom +4000+4000"
    eval bgexec programInfo(xterm) $cmd &
    set programInfo(lastProgram) xterm
    .top.tab.f1 configure -command $cmd 
} 
wm protocol . WM_DELETE_WINDOW { destroy . }
