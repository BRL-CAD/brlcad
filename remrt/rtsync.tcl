# rtsync.tcl
# A prototype GUI for rtsync.
# This file is executed by the rtsync program, not directly from the shell.
# Depends on rtnode defining [get_dbip] and [get_rtip].
# Depends on rtnode & mged having executed {wdb_open .inmem inmem [get_dbip]}
# Uses various RTSYNC built-in commands, as well as LIBRT's Tcl commands.
#  -Mike Muuss, ARL, March 97.

# Set the application name by which other applications will be able to
# SEND commands to us with "send rtsync _stuff_".
tk appname rtsync
wm title . rtsync

puts "Remember, run ./dg.tcl for Dynamic Geometry control."

puts "running rtsync.tcl"
##option add *background #ffffff
##. configure -background #ffffff

# Create main interaction widget
frame .mbar -relief raised -bd 2 ; pack .mbar -side top
frame .title_fr ; pack .title_fr -side top
frame .status ; pack .status -side top

# Menu bar, acros very top
menubutton .mbar.file -text "File" -menu .mbar.file.menu
menubutton .mbar.debug -text "Debug" -menu .mbar.debug.menu
menubutton .mbar.spacepart -text "SpacePart" -menu .mbar.spacepart.menu
menubutton .mbar.help -text "Help" -menu .mbar.help.menu
pack .mbar.file -side left -in .mbar -expand 1 -fill x
pack .mbar.debug -side left -in .mbar -expand 1 -fill x
pack .mbar.spacepart -side left -in .mbar -expand 1 -fill x
pack .mbar.help -side right -in .mbar -expand 1 -fill x
menu .mbar.file.menu
.mbar.file.menu add command -label "Exit" -command "exit"
menu .mbar.help.menu
.mbar.help.menu add command -label "Exit" -command "exit"
menu .mbar.debug.menu
.mbar.debug.menu add command -label "Net Speed Test ON" -command "net_speed_test 1"
.mbar.debug.menu add command -label "Net Speed Test OFF" -command "net_speed_test 0"
.mbar.debug.menu add command -label "Remote bu_log ON" -command "node_send set print_on 1"
.mbar.debug.menu add command -label "Remote bu_log OFF" -command "node_send set print_on 0"
menu .mbar.spacepart.menu
.mbar.spacepart.menu add command -label "Muuss NUBSP" -command "space_partitioning 1"
.mbar.spacepart.menu add command -label "Gigante NUgrid" -command "space_partitioning 0"

# Title, across the top
frame .words_fr
frame .title3
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title1 -text "BRL-CAD's SWISS"
label .title2 -text "Real-Time Ray-Tracer"
label .title3a -textvariable cpu_count
label .title3b -text "CPUs active"
label .title4 -textvariable database
pack .title3a .title3b -side left -in .title3
pack .title1 .title2 .title3 .title4 -side top -in .words_fr
pack .logo .words_fr -side left -in .title_fr

# The Update CPU Status button
## No longer in it's own window.
##toplevel .status
frame .status.incr_fr
button .status.button -text "Update CPU Status" -command update_cpu_status
button .status.incr -text "NCPU++" -command \
	{cur_node_send {global npsw; incr npsw 1; set npsw}}
button .status.decr -text "NCPU--" -command \
	{cur_node_send {global npsw; incr npsw -1; set npsw}}
button .status.drop -text "DROP" -command \
	{set node_num [lindex [selection get] 0]; \
	drop_rtnode $node_num; update_cpu_status}
listbox .status.list -height 1 -width 60
pack .status.incr .status.decr .status.drop -side left -in .status.incr_fr
pack .status.button .status.incr_fr .status.list -side top -in .status


proc update_cpu_status {} {
	set nodes [list_rtnodes]

	.status.list delete 0 end
	.status.list configure -height [expr [llength $nodes] + 1]
	.status.list insert end [get_rtnode -1];# generate title
	foreach node $nodes {
		.status.list insert end [get_rtnode $node]
	}
}

proc cur_node_send {remote_cmd} {
	set node_num [lindex [selection get] 0]
	one_node_send $node_num $remote_cmd
}

proc net_speed_test {val} {
	node_send \
		set test_fb_speed $val ";" \
		set curframe 0
}

proc space_partitioning {val} {
	node_send \
		sh_opt -,$val ";" \
		set curframe 0
	reprep
}

# Aids for memory debugging
proc memdebug {}  {
	node_send \
		set bu_debug 2 ";" \
		bu_printb bu_debug {$bu_debug} {$BU_DEBUG_FORMAT}
}
# Follow this up by issuing a:
# one_node_send 01 bu_prmem

puts "done rtsync.tcl"

# node_send sh_opt -P1 -x1 -X1
# node_send .inmem get LIGHT
# node_send {.inmem adjust LIGHT V {20 -13 5}}
# node_send {.inmem adjust LIGHT V {50 -13 5}} ; reprep; refresh
