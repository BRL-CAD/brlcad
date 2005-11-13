#                        A N I M . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
# ANIM.TCL - AnimMate
# Tcl/Tk Gui Interface for Creating and Displaying Animation Scripts within
#  MGED.
# Author: Carl Nuzman
#Sections:
#	Create main window
#	Curve Editor
#	Table Editor
#	View Editor
#	Create Script
#	Create Track Script
#	Combine Scripts
#	Show Scripts
#	Quit AnimMate
#	General Procedures

if ![info exists tk_version] {
    loadtk
}

#Conventions:
# 1.> for each main widget *foo*, the calling routine should call
#  sketch_init_*foo* once before making any calls to sketch_popup_*foo*
#  Currently the choices for *foo* are from the following list:
#  {draw view table objanim track sort preview}
# 2.> a "p" argument indicates a parent widget.
#  eg. when calling sketch_popup_draw the calling function provides a widget
#  to be the new widget's parent. Whenever tk is running, there is a toplevel
#  widget called "." which can be used.
#-----------------------------------------------------------------
# Create main window
#-----------------------------------------------------------------
proc sketch_init_main {} {
	# global variable initialisations
	uplevel #0 set mged_sketch_init_main 1
	uplevel #0 set mged_sketch_temp1 "./_mged_sketch_temp1_"
	uplevel #0 set mged_sketch_temp2 "./_mged_sketch_temp2_"

	#note - change this variable in production version
#	set version "developement"
        set version ""
	if { $version == "developement" } {
		uplevel #0 {set mged_sketch_anim_path "/m/cad/.anim.6d/"}
		uplevel #0 {set mged_sketch_tab_path "/m/cad/.tab.6d/"}
	} else {
		uplevel #0 {set mged_sketch_anim_path ""}
		uplevel #0 {set mged_sketch_tab_path ""}
	}

	#variable shared between draw and table
	uplevel #0 set mged_sketch_fps "30"

	#allow button 2 to activate buttons
	uplevel #0 { set mged_sketch_bindclasses {Button Radiobutton Checkbutton Menubutton}}
	upvar #0 mged_sketch_bindclasses wlist
	foreach wclass $wlist {
		#save previous bindings
		uplevel #0 [list set mged_sketch_bindB($wclass) [bind $wclass <Button-2>] ]
		uplevel #0 [list set mged_sketch_bindBR($wclass) [bind $wclass <ButtonRelease-2>] ]
		uplevel #0 [list set mged_sketch_bindBM($wclass) [bind $wclass <B2-Motion>] ]
		#add new bindings
		bind $wclass <Button-2> +[bind $wclass <Button-1>]
		bind $wclass <ButtonRelease-2> +[bind $wclass <ButtonRelease-1>]
		bind $wclass <B2-Motion> +[bind $wclass <B1-Motion>]
	}
}

proc sketch_popup_main { {p .} } {
	sketch_init_main
	sketch_init_draw
	sketch_init_view
	sketch_init_table
	sketch_init_objanim
	sketch_init_track
	sketch_init_sort
	sketch_init_preview

	if { $p == "." } {
		set root ".sketch"
	} else {
		set root "$p.sketch"
	}
	catch {destroy $root}

	toplevel $root
	place_near_mouse $root
	wm title $root "MGED AnimMate"
	button $root.b0 -text "Curve Editor" -command "sketch_popup_draw $root"
	button $root.b1 -text "View Editor" -command "sketch_popup_view $root"
	menubutton $root.b2 -text "Table Editor" -menu $root.b2.m0
	menu $root.b2.m0 -tearoff 0 -postcommand "sketch_post_table_menu $root.b2.m0"
	$root.b2.m0 add command -label "New Editor" -command "incr mged_sketch_table_index; sketch_popup_table $root \$mged_sketch_table_index"
	menubutton $root.b3 -text "Create Script" -menu $root.b3.m0
	menu $root.b3.m0 -tearoff 0
	$root.b3.m0 add command -label "Object" -command "sketch_popup_objanim $root obj"
	$root.b3.m0 add command -label "View" -command "sketch_popup_objanim $root view"
	$root.b3.m0 add command -label "Articulated Track" -command "sketch_popup_track_anim $root"
	button $root.b4 -text "Combine Scripts" -command "sketch_popup_sort $root"
	button $root.b5 -text "Show Script" -command "sketch_popup_preview $root"
	button $root.b6 -text "Quit" -command "sketch_quit $root"

	pack $root.b0 $root.b2 $root.b1 $root.b3 $root.b4 \
		$root.b5 $root.b6 \
		-side top -fill x -expand yes

}

proc sketch_post_table_menu {menu} {
	if { [$menu index end] > 0 } {
		$menu delete 1 end
	}
	foreach ted [sketch_table_list] {
		$menu add command -label "Editor [sketch_table_get_label $ted]" -command "raise $ted"
	}
}

#-----------------------------------------------------------------
# Curve Editor
#-----------------------------------------------------------------
#Comments:
#A curve is a list of nodes. Each node contains a time parameter and
#a 3-D point. The list of points is stored in a vlist using "vdraw".
#The list of time parameters is stored in a global variable
#mged_sketch_time_*name*.
proc sketch_init_draw {} {
	#curve
	uplevel #0 set mged_sketch_init_draw 1
	uplevel #0 set mged_sketch_node 0
	uplevel #0 set mged_sketch_count 0
	uplevel #0 set mged_sketch_time 0.0
	uplevel #0 set mged_sketch_tinc 1.0
	uplevel #0 set mged_sketch_tinit 0.0
	uplevel #0 {set mged_sketch_name ""}
	uplevel #0 {set mged_sketch_splname ""}
	uplevel #0 {set mged_sketch_splprefix "spl_"}
	uplevel #0 {set mged_sketch_color "255 255 0"}
	uplevel #0 set mged_sketch_defname "vdraw"
	#dependencies
	foreach dep {main} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

proc sketch_popup_draw { p } {
	global mged_sketch_fps mged_sketch_color mged_sketch_time \
		 mged_sketch_name mged_sketch_count mged_sketch_node \
		mged_sketch_splname mged_sketch_splprefix mged_sketch_defname

	if { $p == "." } {
		set root ".draw"
	} else {
		set root "$p.draw"
	}
   	if { [info commands $root] != ""} {
		raise $root
		return
	}
	toplevel $root
	place_near_mouse $root
	wm title $root "MGED AnimMate curve editor"
	button $root.b0 -text "Add" -command {sketch_add [view center] $mged_sketch_node}
	button $root.b1 -text "Insert" -command {sketch_insert [view center] $mged_sketch_node}
	button $root.b2 -text "Move" -command {sketch_move [view center] $mged_sketch_node}
	button $root.b3 -text "Delete" -command {sketch_delete $mged_sketch_node}
	frame  $root.f1
	label  $root.f1.l0 -text "Node "
	label  $root.f1.l1 -textvariable mged_sketch_node
	label  $root.f1.l2 -text " of "
	label  $root.f1.l3 -textvariable mged_sketch_count
	frame  $root.f0
	button $root.f0.b4 -text "-->" -command {sketch_incr 10}
	button $root.f0.b40 -text "->" -command {sketch_incr 1}
	button $root.f0.b50 -text "<-" -command {sketch_incr -1}
	button $root.f0.b5 -text "<--" -command {sketch_incr -10}
	frame  $root.f4
	#label  $root.f4.l0 -text "Current Curve:"
	menubutton $root.f4.mb0 -text "Current Curve:" -menu $root.f4.mb0.m
	menu $root.f4.mb0.m -tearoff 0
	$root.f4.mb0.m add command -label "New Curve" -command {sketch_popup_name new}
	$root.f4.mb0.m add cascade -label "Open Curve" \
		-menu $root.f4.mb0.m.m0
	$root.f4.mb0.m add command -label "Rename Curve" -command {sketch_popup_name rename}
	$root.f4.mb0.m add command -label "Copy Curve" -command {sketch_popup_name copy}
	$root.f4.mb0.m add cascade -label "Delete Curve" \
		-menu $root.f4.mb0.m.m1

	menu $root.f4.mb0.m.m0 -tearoff 0 \
		-postcommand "sketch_post_curve_list $root.f4.mb0.m.m0 open"
	$root.f4.mb0.m.m0 add command -label "dummy"
	menu $root.f4.mb0.m.m1 -tearoff 0 \
		-postcommand "sketch_post_curve_list $root.f4.mb0.m.m1 delete"
	$root.f4.mb0.m.m1 add command -label "dummy"

	label  $root.f4.l1 -textvariable mged_sketch_name

	frame  $root.f5
	label  $root.f5.l0 -text "Time:"
	entry  $root.f5.e0 -width 8 -textvariable mged_sketch_time
	bind   $root.f5.e0 <Key-Return> " sketch_time_set \[$root.f5.e0 get\]"

	frame  $root.f2
	#label $root.f2.l0 -text "Color:"
	menubutton $root.f2.mb0 -text "Color:" -menu $root.f2.mb0.m
	menu $root.f2.mb0.m -tearoff 0
	$root.f2.mb0.m add cascade -label "Current Curve" \
		-menu $root.f2.mb0.m.m0
	$root.f2.mb0.m add cascade -label "Current Spline" \
		-menu $root.f2.mb0.m.m1
	$root.f2.mb0.m add cascade -label "Other" \
		-menu $root.f2.mb0.m.m2
	menu $root.f2.mb0.m.m0 -tearoff 0
	menu $root.f2.mb0.m.m1 -tearoff 0
	menu $root.f2.mb0.m.m2 -tearoff 0
	sketch_add_color_menu $root.f2.mb0.m.m0 current
	sketch_add_color_menu $root.f2.mb0.m.m1 spline
	sketch_add_color_menu $root.f2.mb0.m.m2 other
	entry $root.f2.e0 -width 12 -textvariable mged_sketch_color
	bind  $root.f2.e0 <Key-Return> "sketch_color \[$root.f2.e0 get\]"
	frame $root.f6 -relief groove -bd 3
	button $root.f6.b0 -text "Spline Interpolate" -command {sketch_do_spline spline}
	button $root.f6.b1 -text "Cspline Interpolate" -command {sketch_do_spline cspline}
	frame $root.f6.f0
	label $root.f6.f0.l0 -text "Into Curve:"
	entry $root.f6.f0.e0 -width 15 -textvariable mged_sketch_splname
	frame  $root.f6.f1
	label $root.f6.f1.l0 -text "Frames Per Second:"
	entry $root.f6.f1.e0 -width 4 -textvariable mged_sketch_fps
	bind  $root.f6.f1.e0 <Key-Return> "focus $root "
	frame $root.f8
	button $root.f8.b0 -text "Up" -command "raise $p"
	button $root.f8.b1 -text "Cancel" -command "destroy $root"

	menubutton $root.mb0 -text "Read/Write" -menu $root.mb0.m0
	menu $root.mb0.m0
	$root.mb0.m0 add command -label "Read Curve from File" -command {sketch_popup_load}
	$root.mb0.m0 add command -label "Write Curve to File" -command {sketch_popup_save curve}
	$root.mb0.m0 add command -label "Write Spline to File" -command {sketch_popup_save spline}

	pack \
		$root.f4 $root.f5 $root.f1 $root.f0 \
		$root.b0 $root.b1 $root.b2 $root.b3 \
		$root.f2 \
		$root.f6 \
		$root.mb0 \
		$root.f8	\
		-side top -fill x -expand yes
	pack \
		$root.f6.b0 $root.f6.b1 \
		$root.f6.f0 $root.f6.f1 \
		-side top -fill x -expand yes
	pack $root.f6.f0.l0 $root.f6.f0.e0 \
		$root.f6.f1.l0 $root.f6.f1.e0 \
		$root.f8.b0 $root.f8.b1 \
		-side left -expand yes
	pack $root.f0.b4 $root.f0.b40 $root.f0.b50 $root.f0.b5 \
		-side right -expand yes
	pack $root.f1.l0 $root.f1.l1 $root.f1.l2 $root.f1.l3 \
		$root.f2.mb0 $root.f2.e0 \
		$root.f4.mb0 $root.f4.l1 \
		$root.f5.l0 $root.f5.e0 \
		-side left -expand yes

	#initialize name
	if { [vdraw open] } {
		sketch_open_curve [vdraw read n]
	} else {
		sketch_open_curve $mged_sketch_defname
	}
	set mged_sketch_splname "$mged_sketch_splprefix[vdraw read n]"
	sketch_update
}

proc sketch_post_curve_list { menu function } {
	switch $function {
		open {set command sketch_name}
		delete {set command sketch_delete_curve}
	}
	$menu delete 0 end
	foreach curve [vdraw vlist l] {
		if { $curve != "_sketch_hl_" } {
		  $menu add command -label $curve -command "$command $curve"
		}
	}
}

proc sketch_open_curve {name} {
	global mged_sketch_tinc
	set res [vdraw open $name]
	if {$res < 0} {
		tk_dialog ._sketch_msg {Couldn't open curve} \
			"Curve $name cannot be opened - it conflicts\
			 with existing geometry." {} 0 {OK}
	} else {
	       #create associated time variable if non-existent
		if { [vdraw read n] != "$name" } {
			#debugging - should never happen
			puts "Warning: wanted $name got [vdraw read n]"
		}
		set tname "mged_sketch_time_$name"
		uplevel #0 "append $tname {}"
		upvar #0 $tname time
		set len [vdraw read l]
		set lenn [expr $len - 1]
		set tlen [llength $time]
		if { $tlen > $len } {
			set time [lrange $time 0 $lenn]
		} elseif { $tlen < $len } {
			set last [lindex $time [expr $tlen - 1]]
			set time [lrange $time 0 [expr $tlen - 2]]
			set val 0.0
			for {set i 0} { $i <= [expr $len - $tlen]} {incr i} {
				lappend time [expr $last + $val]
				set val [expr $val + $mged_sketch_tinc]
			}
		}
	}
	#puts "Opening curve $name with result $res"
	return $res
}

#set the time stamp for current node
proc sketch_time_set { value } {
	upvar #0 mged_sketch_node node
	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	if {$node != ""} {
	set tlist [lreplace $tlist $node $node $value]
	}
	focus .
}

#update graphical representation of current curve
proc sketch_update {} {
	global mged_sketch_count mged_sketch_time mged_sketch_node
	global mged_sketch_name mged_sketch_splname mged_sketch_color

	set mged_sketch_count [vdraw read l]

	sketch_clip

	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist

	if {$mged_sketch_node == ""} {
		set mged_sketch_time ""
	} else {
		set mged_sketch_time [lindex $tlist $mged_sketch_node]
	}

	set mged_sketch_color [sketch_hex_to_rgb [vdraw read c]]
	set mged_sketch_name  [vdraw read n]

	if { [vdraw send] < 0 } {
		tk_dialog ._sketch_msg {Can't display curve} \
		  "Can't create pseudo-solid _VDRW$mged_sketch_name because true solid \
		   with that name exists. Kill the true solid or choose a \
		    a different name for this curve." {} 0 {OK}
		return -1
	}


	if { $mged_sketch_count > 0 } {
		sketch_highlight
	} else {
		kill -f "_VDRW_sketch_hl_"
	}
	return 0

}


#keep current node in bounds
# 0 <= node < count, or "" if count == 0
proc sketch_clip {} {
	global mged_sketch_node mged_sketch_count

	if {$mged_sketch_count <= 0} {
		set mged_sketch_node ""
		return
	}
	#else
	if {$mged_sketch_node == ""} {
		set mged_sketch_node 0
	}
	if { $mged_sketch_node >= $mged_sketch_count } {
		set mged_sketch_node [expr $mged_sketch_count - 1]
	}
	if { $mged_sketch_node < 0 } {
		set mged_sketch_node 0
	}
}

#show current node with 3-d cursor
proc sketch_highlight { } {
	global mged_sketch_node

	if {$mged_sketch_node == ""} return

	set offset [expr [view size] * 0.01]
	set oldname [vdraw read n]
	set vertex [eval [concat vdraw read $mged_sketch_node]]

	set v_x [lindex $vertex 1]
	set v_y [lindex $vertex 2]
	set v_z [lindex $vertex 3]
	vdraw send
	sketch_open_curve _sketch_hl_
	sketch_draw_highlight $v_x $v_y $v_z $offset
	sketch_open_curve $oldname

}

proc sketch_draw_highlight {v_x v_y v_z offset} {
	vdraw delete a
	vdraw write n 0 [expr $v_x - $offset] $v_y $v_z
	vdraw write n 1 [expr $v_x + $offset] $v_y $v_z
	vdraw write n 0 $v_x [expr $v_y - $offset] $v_z
	vdraw write n 1 $v_x [expr $v_y + $offset] $v_z
	vdraw write n 0 $v_x $v_y [expr $v_z - $offset]
	vdraw write n 1 $v_x $v_y [expr $v_z + $offset]
	vdraw params c 0x00ffff
	vdraw send
}

#increment current node by specified amount
proc sketch_incr { i } {
	global mged_sketch_node

	if { $mged_sketch_node != "" } {
		incr mged_sketch_node $i
	}
	sketch_update
}

#add node behind node n, where n can range from -1 to l-1
proc sketch_add { point n } {
	global mged_sketch_tinc mged_sketch_tinit mged_sketch_node

	set length [vdraw read l]
	set last [expr $length - 1]
	upvar #0 "mged_sketch_time_[vdraw read n]" tlist
	if { $length == 0 } {
		eval vdraw write 0 0 $point
		set tlist [list $mged_sketch_tinit]
		sketch_update
		return
	}
	if { ($n == "") || ($n < -1) || ($n > $last) } {
		sketch_update
		return
	}
	set newn [expr $n + 1]
	if { $n == -1 } {
		eval vdraw insert $newn 0 $point
		set vertex [vdraw read 1]
		eval vdraw write 1 1 [lrange $vertex 1 3]
		set tn [expr [lindex $tlist 0] - $mged_sketch_tinc]
	} elseif { $n == $last} {
		eval vdraw insert $newn 1 $point
		set tn [expr [lindex $tlist $last] + $mged_sketch_tinc]
	} else {
		eval vdraw insert $newn 1 $point
		set tn [expr ([lindex $tlist $n]+[lindex $tlist $newn])*0.5]
	}
	set tlist [linsert $tlist $newn $tn]
	set mged_sketch_node $newn
	sketch_update
}

#insert current view center before specified node
proc sketch_insert { point n } {
	if { $n != "" } {
		set n [expr $n - 1]
	}
	sketch_add $point $n
}

#move specified node to current view center
proc sketch_move { point n } {
	if { $n == "" } {
		sketch_update
		return
	}
	if { $n == 0 } {
		eval vdraw write $n 0 $point
	} else {
		eval vdraw write $n 1 $point
	}
	sketch_update
}

#delete specified node
proc sketch_delete { n } {

	if { $n == "" } {
		sketch_update
		return
	}
	vdraw delete $n
	if { ($n == 0) && ([vdraw read l] > 0) } {
		set vertex [vdraw read 0]
		eval [concat vdraw write 0 0 [lrange $vertex 1 3]]
	}
	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	set tlist [lreplace $tlist $n $n]

	sketch_update
}

proc sketch_add_color_menu {m {type current}} {
	set colors {
		{red		{255 0 0}}
		{green		{0 255 0}}
		{blue		{0 0 255}}
		{yellow		{255 255 0}}
		{cyan		{0 255 255}}
		{magenta	{255 0 255}}
		{white		{255 255 255}}
		{gray		{150 150 150}}
		{black		{1 1 1}}
		{other		""}
	}
	foreach item $colors {
		$m add command -label [lindex $item 0] \
		   -command "sketch_popup_color $type \{[lindex $item 1]\}" \
		   -background [sketch_rgb_to_hex [lindex $item 1] pound] \
		   -foreground [sketch_rgb_to_hex [sketch_rgb_inv [lindex $item 1]] pound]
	}
}


proc sketch_popup_color {type color} {
	global mged_sketch_color mged_sketch_splname
	set flag 0
	if {($color == "other")||($color == "")} {
		set color ""
		incr flag
	}
	if { $type == "current" } {
		set mged_sketch_color $color
		set name [vdraw read n]
	} elseif { $type == "spline" } {
		set name $mged_sketch_splname
	} else {
		#other
		set name ""
		incr flag
	}

	if { $flag == 0 } {
		set oldname [vdraw read n]
		vdraw open $name
		sketch_color $color
		vdraw open $oldname
		sketch_update
		return
	}
	#else
	set entries [list \
		[list "Name of curve:" $name] \
		[list "New color:" $color] \
		]
	set buttons [list \
		[list "OK" "set oldname \[vdraw read n\]; \
			vdraw open \[._sketch_input.f0.e get\]; \
			sketch_color \[._sketch_input.f1.e get \]; \
			vdraw open \$oldname; \
			sketch_update; \
			destroy ._sketch_input"] \
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Color Curve" $entries $buttons
	return
}


#set current curve color
proc sketch_color { color } {
	global mged_sketch_color
	vdraw params c [sketch_rgb_to_hex $color]
	vdraw send
	set mged_sketch_color [sketch_hex_to_rgb [vdraw read c]]
	catch {focus .}
}


proc sketch_do_spline { mode } {
	global mged_sketch_fps mged_sketch_splname \
		mged_sketch_temp1 mged_sketch_temp2 \
		mged_sketch_tab_path

	#write vertices to temp2, result to temp1
	set fo [open $mged_sketch_temp2 w]
	set length [vdraw read l]
	if { $length < 2 } {
		puts {Need at least two vertices}
		close $fo
		return -1
	} elseif { $length == 2 } {
		set cmdstr "linear"
	} else {
		set cmdstr $mode
	}

	sketch_write_to_fd $fo $length
	close $fo

	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	set start [lindex $tlist 0]
	set end [lindex $tlist [expr $length - 1]]
	set fo [open "| ${mged_sketch_tab_path}tabinterp -q > $mged_sketch_temp1" w]
	puts $fo "file $mged_sketch_temp2 0 1 2;"
	puts $fo [concat times $start $end $mged_sketch_fps {;}]
	puts $fo "interp $cmdstr 0 1 2;"
	# catch can be removed when tabinterp -q option is installed
	catch {close $fo}
	file delete $mged_sketch_temp2

	#read results into curve
	set fi [open $mged_sketch_temp1 r]
	set oldname [vdraw read n]
	set oldcolor [sketch_hex_to_rgb [vdraw read c]]
	vdraw send
	sketch_open_curve $mged_sketch_splname
	vdraw delete a
	set num_read [sketch_read_from_fd $fi]
	close $fi
	#vdraw params c $oldcolor
	vdraw send
	sketch_open_curve $oldname
	file delete $mged_sketch_temp1
	return $num_read
}



proc sketch_popup_load {} {
	set entries [list \
		{"File to Load"} \
		[list "Name of Curve" [vdraw read n]] \
		]
	set buttons [list \
		{"OK" {sketch_load [._sketch_input.f0.e get] \
				[._sketch_input.f1.e get]} } \
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Load Curve" $entries $buttons
}

#load from file to specified curve
proc sketch_load { filename  curve } {
	global mged_sketch_splname mged_sketch_splprefix
	if { [sketch_open_curve $curve] < 0 } {
		echo "Couldn't open" $curve
		return
	}
	set fd [open $filename r]
	vdraw delete a
	sketch_read_from_fd $fd
	close $fd
	catch {destroy ._sketch_input}
	sketch_update
	set mged_sketch_splname "$mged_sketch_splprefix[vdraw read n]"
}


proc sketch_popup_save { type  } {
	global mged_sketch_splprefix

	set entries [list \
		[list "Name of Curve" [vdraw read n]] \
		{"Save to File:"} \
		]
	set buttons [list \
		{"OK" {sketch_save [._sketch_input.f0.e get] \
				[._sketch_input.f1.e get]} }\
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Save Curve" $entries $buttons
	if {$type == "spline"} {
		._sketch_input.f0.e insert 0 $mged_sketch_splprefix
	}
}

#save specified curve to file
proc sketch_save { curve filename } {
	if {[file exists $filename] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set oldcurve [vdraw read n]
	set fd [open $filename w]
	sketch_open_curve $curve
	sketch_write_to_fd $fd [vdraw read l]
	close $fd
	sketch_open_curve $oldcurve
	catch {destroy ._sketch_input}
}



proc sketch_popup_name {{mode new}} {
	if { $mode == "new"} {
		sketch_popup_input "Select New Curve" {
			{"Name for new curve:" ""}
		} {
			{"OK" {sketch_name [._sketch_input.f0.e get]}}
			{"Cancel" "destroy ._sketch_input"}
		}
	} elseif { $mode == "rename" } {
		sketch_popup_input "Rename Curve" {
			{"New name for curve:" ""}
		} [list \
			[list "OK" "sketch_rename \[._sketch_input.f0.e get\]" ] \
			{"Cancel" "destroy ._sketch_input"} \
		]
	} elseif { $mode == "copy" } {
		sketch_popup_input "Copy Curve" {
			{"Name for copy:" ""}
		} [list \
			[list "OK" "sketch_copy \[._sketch_input.f0.e get\]" ] \
			{"Cancel" "destroy ._sketch_input"} \
		]
	}
}

proc sketch_name { name } {
	global mged_sketch_splname mged_sketch_splprefix
	if {[sketch_open_curve $name] < 0} {
		return
	}
	catch {destroy ._sketch_input}
	sketch_update
	set mged_sketch_splname "$mged_sketch_splprefix[vdraw read n]"
}

proc sketch_rename { name } {
	global mged_sketch_splname mged_sketch_splprefix
	set oldname [vdraw read n]
	if { [catch {vdraw params n $name }] == 1 } {
		#error occurred - name already exists
		set ans [tk_dialog ._sketch_msg {Curve exists} \
		  "A curve with name $name already exists." {} \
		  1 {Rename anyway} {Cancel} ]
		if { $ans == 1 } {
			return -1
		} else {
			vdraw vlist d $name
			vdraw params n $name
		}
	}
	#for some reason, this update is needed to prevent dialog from
	#crashing
	update
	upvar #0 "mged_sketch_time_$oldname" oldtime
	uplevel #0 "append mged_sketch_time_$name {}"
	upvar #0 "mged_sketch_time_$name" newtime
	set newtime $oldtime
	#sketch_update will fail if name conflicts with true solid
	if { [sketch_update] == 0 } {
		catch {destroy ._sketch_input}
		kill -f "_VDRW$oldname"
		unset oldtime
		set mged_sketch_name [vdraw read n]
		if { "$mged_sketch_name" != "$name" } {
			puts "sketch_rename error. This should never happen."
		}
		set mged_sketch_splname "$mged_sketch_splprefix$mged_sketch_name"
	} else {
		#put things back
		unset newtime
		vdraw params name $oldname
		sketch_update
	}
}

proc sketch_copy { name } {
	set basename [vdraw read n]
	if { [vdraw open $name ] == 0 } {
		set ans [tk_dialog ._sketch_msg {Curve exists} \
		  "A curve with name $name already exists." {} \
		  1 {Copy anyway} {Cancel} ]
		if { $ans == 1 } {
			vdraw open $basename
			return -1
		} else {
			vdraw delete a
		}
	}
	vdraw open $basename
	set buffer ._sketch_scratch_
	text $buffer
	sketch_text_echoc $buffer
	sketch_open_curve $name
	sketch_text_apply $buffer replace
	destroy $buffer
	if {[sketch_update] == 0} {
		catch {destroy ._sketch_input}
	} else {
		sketch_open_curve $basename
		vdraw vlist d $name
		sketch_update
	}
}

proc sketch_popup_delete_curve {} {
	set entries [list \
		[list "Delete Curve:" [vdraw read n]] \
		]
	set buttons [list \
		{ "OK" {sketch_delete_curve [._sketch_input.f0.e get]; \
				destroy ._sketch_input} } \
		{ "Cancel" "destroy ._sketch_input" } \
		]
	sketch_popup_input "Delete Curve" $entries $buttons
}

proc sketch_delete_curve { name } {
	global mged_sketch_defname

	vdraw vlist d $name
	catch {vdraw vlist d _sketch_hl_}
	if { [vdraw open] } {
		sketch_open_curve [vdraw read n]
	} else {
		sketch_open_curve $mged_sketch_defname
	}
	uplevel #0 "set mged_sketch_time_$name {}"
	kill -f "_VDRW$name"
	sketch_update
}

#-----------------------------------------------------------------
# View Curve Editor
#-----------------------------------------------------------------
#Comments:
# A view curve consists of a series of nodes. Each node contains all
# the information necessary to reproduce a view state. Different
#combinations of parameters are possible - the implemented combinations
#can be found in the first switch statement of sketch_set_vparams{}.
#Each view curve is realized as a read-only table widget whose parent is
#$mged_sketch_vwidget and whose name is $mged_sketch_vprefix*name*.
proc sketch_init_view {} {
	#view curve
	uplevel #0 set mged_sketch_init_view 1
	uplevel #0 set mged_sketch_vapply 0
	uplevel #0 set mged_sketch_vwidget ".view"
	uplevel #0 set mged_sketch_vprefix "_v_"
	uplevel #0 set mged_sketch_vnode 0
	uplevel #0 set mged_sketch_vcount 0
	uplevel #0 set mged_sketch_vtime 0.0
	uplevel #0 set mged_sketch_vtinc 1.0
	uplevel #0 {set mged_sketch_vname ""}
	uplevel #0 {set mged_sketch_vparams {size eye quat}}
	uplevel #0 {set mged_sketch_vchoices {
		{size eye quat}
		{size eye ypr}
		{size center quat}
		{size center ypr}
		{eye center}
		}}
	uplevel #0 set mged_sketch_cmdlen(quat) 4
	uplevel #0 set mged_sketch_cmdlen(eye) 3
	uplevel #0 set mged_sketch_cmdlen(center) 3
	uplevel #0 set mged_sketch_cmdlen(ypr) 3
	uplevel #0 set mged_sketch_cmdlen(aet) 3
	uplevel #0 set mged_sketch_cmdlen(size) 1
	#dependencies
	foreach dep {main table} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

proc sketch_popup_view { p } {
	global mged_sketch_vtime \
		 mged_sketch_vname mged_sketch_vcount mged_sketch_vnode \
		mged_sketch_vparams mged_sketch_vwidget mged_sketch_vprefix \
		mged_sketch_vchoices

	if { $p == "." } {
		set root ".view"
	} else {
		set root "$p.view"
	}
	set mged_sketch_vwidget "$root"
	#set mged_sketch_vprefix "_v_"
	set prefix $mged_sketch_vwidget.$mged_sketch_vprefix
	if { [info commands $root] != ""} {
		wm deiconify $root
		raise $root
		return
	}
	toplevel $root
	place_near_mouse $root
	wm title $root "MGED AnimMate view curve editor"
	button $root.b0 -text "Add" -command {sketch_vadd $mged_sketch_vnode}
	button $root.b1 -text "Insert" -command {sketch_vinsert $mged_sketch_vnode}
	button $root.b2 -text "Move" -command {sketch_vmove $mged_sketch_vnode}
	button $root.b3 -text "Delete" -command {sketch_vdelete $mged_sketch_vnode}
	frame  $root.f1
	label  $root.f1.l0 -text "Node "
	label  $root.f1.l1 -textvariable mged_sketch_vnode
	label  $root.f1.l2 -text " of "
	label  $root.f1.l3 -textvariable mged_sketch_vcount
	checkbutton $root.cb0 -text "Apply Current Node to View" \
		-variable mged_sketch_vapply -command "sketch_vupdate"
	$root.cb0 deselect
	frame  $root.f0
	button $root.f0.b4 -text "-->" -command {sketch_vincr 10}
	button $root.f0.b40 -text "->" -command {sketch_vincr 1}
	button $root.f0.b50 -text "<-" -command {sketch_vincr -1}
	button $root.f0.b5 -text "<--" -command {sketch_vincr -10}
	frame  $root.f4
	#label  $root.f4.l0 -text "Current V-Curve:"
	menubutton $root.f4.mb0 -text "Current V-Curve:" -menu $root.f4.mb0.m
	menu $root.f4.mb0.m -tearoff 0
	$root.f4.mb0.m add command -label "New V-Curve" \
		-command {sketch_popup_vname select}
	$root.f4.mb0.m add cascade -label "Open V-Curve" \
		-menu $root.f4.mb0.m.m0
	$root.f4.mb0.m add command -label "Rename V-Curve" \
		-command {sketch_popup_vname rename}
	$root.f4.mb0.m add command -label "Copy V-Curve" \
		-command {sketch_popup_vname copy}
	$root.f4.mb0.m add cascade -label "Delete V-Curve" \
		-menu $root.f4.mb0.m.m1

	menu $root.f4.mb0.m.m0 -tearoff 0 \
		-postcommand "sketch_post_vcurve_list $root.f4.mb0.m.m0 open"
	$root.f4.mb0.m.m0 add command -label "dummy"
	menu $root.f4.mb0.m.m1 -tearoff 0 \
		-postcommand "sketch_post_vcurve_list $root.f4.mb0.m.m1 delete"
	$root.f4.mb0.m.m1 add command -label "dummy"

	button  $root.f4.l1 -textvariable mged_sketch_vname \
		-command "wm deiconify $prefix\$mged_sketch_vname; \
			raise $prefix\$mged_sketch_vname"
	frame $root.f3
	menubutton $root.f3.mb0 -text "Parameters:" -menu $root.f3.mb0.m
	menu $root.f3.mb0.m -tearoff 0
	foreach choice $mged_sketch_vchoices {
		$root.f3.mb0.m add command -label $choice \
			-command "sketch_set_vparams \{$choice\}"
	}
	label $root.f3.l0 -textvariable mged_sketch_vparams

	frame  $root.f5
	label  $root.f5.l0 -text "Time:"
	entry  $root.f5.e0 -width 8 -textvariable mged_sketch_vtime
	bind   $root.f5.e0 <Key-Return> "sketch_vtime_set \[$root.f5.e0 get\]"

	frame $root.f8
	button $root.f8.b0 -text "Up" -command "raise $p"
	button $root.f8.b1 -text "Cancel" -command "sketch_view_cancel"

	menubutton $root.mb0 -text "Read/Write" -menu $root.mb0.m0
	menu $root.mb0.m0
	$root.mb0.m0 add command -label "Read V-Curve from File" -command {sketch_popup_vload}
	$root.mb0.m0 add command -label "Write V-Curve to File" -command {sketch_popup_vsave curve}

	pack \
		$root.f4 $root.f3 $root.f5 $root.f1 $root.cb0 $root.f0 \
		$root.b0 $root.b1 $root.b2 $root.b3 \
		$root.mb0 \
		$root.f8	\
		-side top -fill x -expand yes
	pack \
		$root.f8.b0 $root.f8.b1 \
		-side left -expand yes
	pack $root.f0.b4 $root.f0.b40 $root.f0.b50 $root.f0.b5 \
		-side right -expand yes
	pack $root.f1.l0 $root.f1.l1 $root.f1.l2 $root.f1.l3 \
		$root.f4.mb0 $root.f4.l1 \
		$root.f3.mb0 $root.f3.l0 \
		$root.f5.l0 $root.f5.e0 \
		-side left -expand yes

	#initialize name
	sketch_open_vcurve $mged_sketch_vname
	sketch_vupdate
}

proc sketch_open_vcurve {name} {
	global mged_sketch_vname mged_sketch_vparams mged_sketch_vwidget \
		mged_sketch_vprefix mged_sketch_vapply

	set prefix $mged_sketch_vwidget.$mged_sketch_vprefix
	#get non-empty name
	if { $name == "" } {
		#pick from existing
		set any [sketch_vcurve_get_label \
			[lindex [sketch_vcurve_list] 0] ]
		if { $any == "" } {
			set name "view"
		} else {
			set name $any
		}
	}

	#create if doesn't exist
	if { [info commands $prefix$name.t] == "" } {
		sketch_popup_table_create $mged_sketch_vwidget \
			$mged_sketch_vprefix$name "View curve: $name" vcurve
		$prefix$name.t tag configure current -background white \
			-relief raised -borderwidth 2
	}
	set mged_sketch_vname $name
	#create parameter list if need be
	set vpname "mged_sketch_vparams_$name"
	uplevel #0 "append $vpname {}"
	upvar #0 $vpname vpn
	sketch_set_vparams $vpn

	#wm deiconify $prefix$name
	#raise $prefix$name
	raise $mged_sketch_vwidget
	set mged_sketch_vapply 0
}

proc sketch_post_vcurve_list { menu function } {
	switch $function {
		open {set command sketch_vname}
		delete {set command sketch_delete_vcurve}
	}

	$menu delete 0 end
	foreach ved [sketch_vcurve_list] {
		set vcurve [sketch_vcurve_get_label $ved]
		$menu add command -label $vcurve -command "$command $vcurve"
	}

}



#set the viewparameters for the current view curve and convert if necessary
proc sketch_set_vparams { newlist } {
	global mged_sketch_vname mged_sketch_vparams \
		mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_path\
		mged_sketch_vwidget mged_sketch_vprefix mged_sketch_vchoices

	#make it one of the allowable values
	set flag 0
	foreach choice $mged_sketch_vchoices {
		if { $newlist == $choice } {
			set flag 1
			break
		}
	}
	if { !$flag} {
			set newlist {size eye quat}
	}

	set mged_sketch_vparams $newlist
	uplevel #0 "append mged_sketch_vparams_$mged_sketch_vname {}"
	upvar #0 mged_sketch_vparams_$mged_sketch_vname oldlist
	if { $oldlist == $newlist } return
	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	if { ([info commands $text] == "") || ([sketch_text_rows $text] < 1)} {
		set oldlist $newlist
		return
	}
	#otherwise, we must convert the text
	#convert to {size eye ypr}
	set buffer $text._params_scratch_
	if { [info commands $buffer] != "" } {
		destroy $buffer
	}
	text $buffer
	$text configure -state normal
	switch $oldlist {
		{size eye quat} {
			set fd [open "| ${mged_sketch_anim_path}anim_orient qv y \
					> $mged_sketch_temp1 " w]
			sketch_text_to_fd $text $fd "5,6,7,8"
			catch {close $fd}
			set fd [open $mged_sketch_temp1 r]
			sketch_text_col_arith $text all {@0 @1 @2 @3 @4}
			sketch_text_from_fd $text $fd all right
			close $fd
			file delete $mged_sketch_temp1
		}
		{eye center} {

			set fd [open "| ${mged_sketch_anim_path}anim_lookat -y -v \
					> $mged_sketch_temp1" w]
			sketch_text_to_fd $text $fd all
			close $fd
			set fd [open $mged_sketch_temp1 r]
			sketch_text_from_fd $text $fd all replace
			close $fd
			file delete $mged_sketch_temp1
		}
		{size center ypr} {
			set fd [open "| ${mged_sketch_anim_path}anim_cascade -ry 0 0 0 > $mged_sketch_temp1" w]
			sketch_text_do_script $buffer $text all {@0 @2 @3 @4 @5 @6 @7 {-@1/2.0} 0.0 0.0}
			sketch_text_to_fd $buffer $fd all
			close $fd
			set fd [open $mged_sketch_temp1 r]
			sketch_text_from_fd $buffer $fd "1,2,3" right
			close $fd
			$text delete 1.0 end
			sketch_text_do_script $text $buffer all {@0 {-2.0*@7} @10 @11 @12 @4 @5 @6}
			file delete $mged_sketch_temp1
		}
		{size center quat} {
			set fd [open "| ${mged_sketch_anim_path}anim_orient qv y > $mged_sketch_temp1" w]
			sketch_text_to_fd $text $fd "5,6,7,8"
			catch {close $fd}
			sketch_text_do_script $buffer $text all {@0 @2 @3 @4 {-@1/2.0} 0.0 0.0}
			set fd [open "| ${mged_sketch_anim_path}chan_permute -i stdin 0 1 2 3 4 5 6 -i $mged_sketch_temp1 8 9 10 -o stdout 0 1 2 3 8 9 10 4 5 6 | ${mged_sketch_anim_path}anim_cascade -ry 0 0 0 > $mged_sketch_temp2" w]
			sketch_text_to_fd $buffer $fd all
			close $fd
			set fd [open $mged_sketch_temp2 r]
			$text delete 1.0 end
			sketch_text_do_script $text $buffer all {@0 {-2.0*@4} }
			sketch_text_from_fd $text $fd "1,2,3,4,5,6" right
			close $fd
			file delete $mged_sketch_temp1 $mged_sketch_temp2
		}
		{size eye ypr} -
		default {}
	}

	$buffer delete 1.0 end

	#convert from {size eye ypr}
	switch $newlist {
		{size eye quat} {
			set fd [open "| ${mged_sketch_anim_path}anim_orient y qv \
					> $mged_sketch_temp1 " w]
			sketch_text_to_fd $text $fd "5,6,7"
			catch {close $fd}
			set fd [open $mged_sketch_temp1 r]
			sketch_text_col_arith $text all {@0 @1 @2 @3 @4}
			sketch_text_from_fd $text $fd all right
			close $fd
			file delete $mged_sketch_temp1
		}
		{eye center} {
			sketch_text_do_script $buffer $text all \
			   {@0 @2 @3 @4 @5 @6 @7 {@1*0.5} 0.0 0.0 0.0 0.0 0.0}
			set fd [open "| ${mged_sketch_anim_path}anim_cascade \
					> $mged_sketch_temp2" w]
			sketch_text_to_fd $buffer $fd all
			close $fd
			sketch_text_col_arith $text all {@0 @2 @3 @4}
			set fd [open $mged_sketch_temp2 r]
			sketch_text_from_fd $text $fd "1,2,3" right
			close $fd
			file delete $mged_sketch_temp2
		}
		{size center ypr} {
			set fd [open "| ${mged_sketch_anim_path}anim_cascade -ry 0 0 0 > $mged_sketch_temp1" w]
			sketch_text_do_script $buffer $text all {@0 @2 @3 @4 @5 @6 @7 {@1/2.0} 0.0 0.0}
			sketch_text_to_fd $buffer $fd all
			close $fd
			set fd [open $mged_sketch_temp1 r]
			sketch_text_from_fd $buffer $fd "1,2,3" right
			close $fd
			$text delete 1.0 end
			sketch_text_do_script $text $buffer all {@0 {2.0*@7} @10 @11 @12 @4 @5 @6}
			file delete $mged_sketch_temp1
		}
		{size center quat} {
			set fd [open "| ${mged_sketch_anim_path}anim_cascade -ry 0 0 0 > $mged_sketch_temp1" w]
			sketch_text_do_script $buffer $text all {@0 @2 @3 @4 @5 @6 @7 {@1/2.0} 0.0 0.0}
			sketch_text_to_fd $buffer $fd all
			close $fd
			set fd [open "| ${mged_sketch_anim_path}chan_permute -i $mged_sketch_temp1 0 1 2 3 4 5 6 -o stdout 4 5 6 | ${mged_sketch_anim_path}anim_orient y qv | ${mged_sketch_anim_path}chan_permute -i stdin 7 8 9 10 -i $mged_sketch_temp1 0 1 2 3 4 5 6 -o stdout 1 2 3 7 8 9 10" r]
			$text delete 1.0 end
			sketch_text_do_script $text $buffer all {@0 {2.0*@7}}
			sketch_text_from_fd $text $fd all right
			close $fd
			file delete $mged_sketch_temp1
		}
		{size eye ypr} -
		default {}
	}

	$text configure -state disabled
	destroy $buffer
	set oldlist $newlist
	return
}



#append current view parameters to view curve
proc sketch_vadd { n } {
	global mged_sketch_vnode mged_sketch_vcount mged_sketch_vtinc
	global mged_sketch_vtime mged_sketch_vname mged_sketch_vparams mged_sketch_vwidget mged_sketch_vprefix

	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	set length [sketch_text_rows $text]
	set last [expr $length - 1]
	if { $length == 0 } {
		set mged_sketch_vtime 0.0
		set mged_sketch_vnode 0
		set n $mged_sketch_vnode
		$text configure -state normal
		$text insert "1.0" [sketch_get_view_line $mged_sketch_vtime nl]
		$text configure -state disabled
		sketch_vupdate
		return
	}
	if { ($n == "") || ($n < -1) || ($n > $last) } {
		sketch_vupdate
		return
	}
	set line [expr $n + 2]
	set preline [expr $n + 1]
	if { $n == -1 } {
		set time1 [lindex [$text get "$line.0" "$line.0 lineend"] 0]
		set mged_sketch_vtime [expr $time1 - $mged_sketch_vtinc]
	} elseif { $n == $last } {
		set time0 [lindex [$text get "$preline.0" "$preline.0 lineend"] 0]
		set mged_sketch_vtime [expr $time0 + $mged_sketch_vtinc]
	} else {
		set time0 [lindex [$text get "$preline.0" "$preline.0 lineend"] 0]
		set time1 [lindex [$text get "$line.0" "$line.0 lineend"] 0]
		set mged_sketch_vtime [expr ($time0 + $time1)*0.5]
	}
	$text configure -state normal
	$text insert "$line.0" [sketch_get_view_line $mged_sketch_vtime nl]
	$text configure -state disabled
	set mged_sketch_vnode $preline
	sketch_vupdate
}

proc sketch_vinsert { n } {
	if { $n != "" } {
		set n [expr $n - 1]
	}
	sketch_vadd $n
}

proc sketch_get_view_line { time {mode 0}} {
	global mged_sketch_vparams

	set line "\t$time"
	foreach cmd $mged_sketch_vparams {
		set new [join [view $cmd] "\t"]
		append line "\t$new"
	}
	if { $mode == "nl" } {
		return "$line\n"
	}
	return $line
}

#delete specified node
proc sketch_vdelete { n } {
	global mged_sketch_vname mged_sketch_vwidget mged_sketch_vprefix

	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	if { $n == "" } {
		sketch_vupdate
		return
	}
	incr n 1
	$text configure -state normal
	$text delete "$n.0" "$n.0 lineend + 1 c"
	$text configure -state disabled

	sketch_vupdate
}

#move specified node to current view
proc sketch_vmove { n } {
	global mged_sketch_vtime mged_sketch_vname mged_sketch_vwidget mged_sketch_vprefix

	if { $n == "" } {
		sketch_vupdate
		return
	}
	incr n 1
	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	set mged_sketch_vtime [lindex [$text get "$n.0" "$n.0 lineend"] 0]
	$text configure -state normal
	$text delete "$n.0" "$n.0 lineend"
	$text insert "$n.0" [sketch_get_view_line $mged_sketch_vtime]
	$text configure -state disabled

	sketch_vupdate
}

#insert current view center at specified node
proc sketch_vinsert2 { n } {
	global mged_sketch_vtinc global mged_sketch_vtime mged_sketch_vname \
		mged_sketch_vwidget mged_sketch_vprefix

	if { $n == "" } {
		sketch_vupdate
		return
	}
	incr n 1
	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	set t2 [lindex [$text get "$n.0" "$n.0 lineend"] 0]
	if { $n == 1 } {
		set mged_sketch_vtime [expr $t2 - $mged_sketch_vtinc]
	} else {
		set i [expr $n - 1]
		set t1 [lindex [$text get "$i.0" "$i.0 lineend"] 0]
		set mged_sketch_vtime  [expr 0.5*($t1+$t2)]
	}
	$text configure -state normal
	$text insert "$n.0" [sketch_get_view_line $mged_sketch_vtime nl]
	$text configure -state disabled

	sketch_vupdate
}

#update description of view curve
proc sketch_vupdate {} {
	global mged_sketch_vcount mged_sketch_vtime mged_sketch_vnode
	global mged_sketch_vname mged_sketch_vparams mged_sketch_cmdlen \
		mged_sketch_vwidget mged_sketch_vprefix mged_sketch_vapply

	if { $mged_sketch_vname == "" } {
		puts "sketch_vupdate: no view curve"
		return
	}
	set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
	set mged_sketch_vcount [sketch_text_rows $text]
	sketch_vclip

	if {$mged_sketch_vnode == ""} {
		set mged_sketch_vtime ""
		return
	}

	set node [expr $mged_sketch_vnode + 1]
	set line [$text get "$node.0" "$node.0 lineend"]

	set len [llength $line]
	if { $len < 1 } {
		puts "sketch_vupdate: Empty line"
		return
	}

	set mged_sketch_vtime [lindex $line 0]

	if { $mged_sketch_vapply } {
		set i 1
		set str ""
		foreach cmd $mged_sketch_vparams {
			set cargs [lrange $line $i \
			   [expr $i + $mged_sketch_cmdlen($cmd) - 1] ]
			set str [concat $str $cmd $cargs]
			incr i $mged_sketch_cmdlen($cmd)
		}
		if { $i != $len } {
			puts "sketch_vupdate: expected $i columns, got $len"
			return
		}
		eval view $str
	}

	#highlight the current line
	$text tag remove current 1.0 end
	set line [expr $mged_sketch_vnode + 1]
	set nline [expr $mged_sketch_vnode + 2]
	$text tag add current "$line.0" "$nline.0"
}

#increment current node by specified amount
proc sketch_vincr { i } {
	global mged_sketch_vnode

	if { $mged_sketch_vnode != "" } {
		incr mged_sketch_vnode $i
	}
	sketch_vupdate
}

#keep current node in bounds
# 0 <= node < count, or "" if count == 0
proc sketch_vclip {} {
	global mged_sketch_vnode mged_sketch_vcount

	if {$mged_sketch_vcount <= 0} {
		set mged_sketch_vnode ""
		return
	}
	#else
	if {$mged_sketch_vnode == ""} {
		set mged_sketch_vnode 0
	}
	if { $mged_sketch_vnode >= $mged_sketch_vcount } {
		set mged_sketch_vnode [expr $mged_sketch_vcount - 1]
	}
	if { $mged_sketch_vnode < 0 } {
		set mged_sketch_vnode 0
	}
}


#set the time stamp for current node
proc sketch_vtime_set { value } {
	global mged_sketch_vname mged_sketch_vwidget mged_sketch_vprefix

	upvar #0 mged_sketch_vnode node
	if {$node != ""} {
		set n [expr $node + 1]
		set text $mged_sketch_vwidget.$mged_sketch_vprefix$mged_sketch_vname.t
		set line [$text get "$n.0" "$n.0 lineend"]
		set lline [split $line "\t"]
		set lline [lreplace $lline 1 1 $value]
		set line [join $lline "\t"]
		$text configure -state normal
		$text delete "$n.0" "$n.0 lineend"
		$text insert "$n.0" $line
		$text configure -state disabled
	}
	focus .
}


proc sketch_popup_vsave { type  } {
	global mged_sketch_vname

	set entries [list \
		[list "Name of View curve" $mged_sketch_vname] \
		{"Save to File:"} \
		{"Which columns:" "all"} \
		]
	set buttons [list \
		{"OK" {sketch_vsave [._sketch_input.f0.e get] \
			[._sketch_input.f1.e get] \
			[._sketch_input.f2.e get] } }\
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Save View curve" $entries $buttons
}

proc sketch_vsave { vcurve filename cols } {
	global mged_sketch_vwidget mged_sketch_vprefix

	set text $mged_sketch_vwidget.$mged_sketch_vprefix$vcurve.t
	if {[info commands $text] == ""} {
		tk_dialog ._sketch_msg {Can't find View Curve} \
		   "Can't find view curve $vcurve." {} 0 {OK}
		return
	}
	if {[file exists $filename] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}
	set fd [open $filename w]
	sketch_text_to_fd $text $fd $cols
	close $fd
	catch {destroy ._sketch_input}
}


proc sketch_popup_vload {} {
	global mged_sketch_vname
	set entries [list \
		{"File to Load"} \
		[list "Name of View Curve" $mged_sketch_vname] \
		{"Load which columns:" "all"} \
		]
	set buttons [list \
		[list "OK" "sketch_vload \[._sketch_input.f0.e get\] \
			\[._sketch_input.f1.e get\] \
			\[._sketch_input.f2.e get\]" ] \
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Load View Curve" $entries $buttons
}

proc sketch_vload { filename vcurve cols} {
	global mged_sketch_vname mged_sketch_vparams mged_sketch_vwidget mged_sketch_vprefix

	set oldname $mged_sketch_vname
	sketch_open_vcurve $vcurve
	#check for correct number of columns
	set fd [open $filename r]
	set numcol [sketch_line_cols [gets $fd]]
	close $fd
	if {$cols == "all"} {
		set num $numcol
	} else {
		set num [sketch_parse_col $col $numcol output]
	}
	if { [sketch_vcurve_check_col $mged_sketch_vparams $num] == -1} {
		sketch_open_vcurve $oldname
		return -1
	}
	set text $mged_sketch_vwidget.$mged_sketch_vprefix$vcurve.t
	set fd [open $filename r]
	$text configure -state normal
	sketch_text_from_fd $text $fd $cols replace
	$text configure -state disabled
	close $fd
	catch {destroy ._sketch_input}
	sketch_vupdate
}



proc sketch_popup_vname {{mode select}} {
	if { $mode == "select"} {
		sketch_popup_input "Select View Curve" {
			{"Name for new v-curve:" ""}
		} {
			{"OK" {sketch_vname [._sketch_input.f0.e get]}}
			{"Cancel" "destroy ._sketch_input"}
		}
	} elseif { $mode == "rename" } {
		sketch_popup_input "Rename View Curve" {
			{"New name for v-curve:" ""}
		} [list \
			[list "OK" "sketch_vrename \[._sketch_input.f0.e get\] \
				$mode" ] \
			{"Cancel" "destroy ._sketch_input"} \
		]
	} elseif { $mode == "copy" } {
		sketch_popup_input "Copy View Curve" {
			{"Name for copy:" ""}
		} [list \
			[list "OK" "sketch_vrename \[._sketch_input.f0.e get\] \
				$mode" ] \
			{"Cancel" "destroy ._sketch_input"} \
		]
	}
}

proc sketch_vname { name } {
	sketch_open_vcurve $name
	catch {destroy ._sketch_input}
	sketch_vupdate
}

proc sketch_vrename { name mode } {
	global mged_sketch_vname mged_sketch_vwidget mged_sketch_vprefix \
		mged_sketch_vparams

	set oldname $mged_sketch_vname
	set oldparams $mged_sketch_vparams
	if { $oldname == $name } {
		catch {destroy ._sketch_input}
		return
	}
	set ntext $mged_sketch_vwidget.$mged_sketch_vprefix$name.t
	set otext $mged_sketch_vwidget.$mged_sketch_vprefix$oldname.t

	if {[info commands $ntext] != ""} {
		set ans [tk_dialog ._sketch_msg {View Curve Exists} \
		   "View curve $name already exists." {} 1 {Overwrite} \
		   {Cancel}]
		if {$ans == 1} return
	}
	sketch_open_vcurve $name
	$ntext configure -state normal
	$ntext delete 1.0 end
	sketch_set_vparams $oldparams
	sketch_text_from_text $ntext $otext all replace
	$ntext configure -state disabled
	if { $mode == "rename"} {
		destroy $mged_sketch_vwidget.$mged_sketch_vprefix$oldname
	}
	#else copy
	sketch_vupdate
	catch {destroy ._sketch_input}
}

proc sketch_popup_delete_vcurve {} {
	global mged_sketch_vname
	set entries [list \
		[list "Delete View Curve:" $mged_sketch_vname] \
		]
	set buttons [list \
		{ "OK" {sketch_delete_vcurve [._sketch_input.f0.e get]; \
				destroy ._sketch_input} } \
		{ "Cancel" "destroy ._sketch_input" } \
		]
	sketch_popup_input "Delete View Curve" $entries $buttons
}

proc sketch_delete_vcurve { name } {
	global mged_sketch_vwidget mged_sketch_vprefix

	catch {destroy $mged_sketch_vwidget.$mged_sketch_vprefix$name}
	catch {unset mged_sketch_vparams_$name}
	sketch_open_vcurve ""
	sketch_vupdate
}


proc sketch_view_cancel {} {
	global mged_sketch_vwidget mged_sketch_vprefix

	wm withdraw $mged_sketch_vwidget
	set prefix $mged_sketch_vwidget.$mged_sketch_vprefix
	foreach ved [sketch_vcurve_list] {
		wm withdraw $ved
	}
}



#display message and return -1 if wrong number of columns
proc sketch_vcurve_check_col { vparams incol } {
	global mged_sketch_cmdlen
	set k 1
	set descr "time(1)"
	foreach cmd $vparams {
		set i $mged_sketch_cmdlen($cmd)
		append descr ", $cmd\($i\)"
		incr k $i
	}
	if { $incol != $k } {
		tk_dialog ._sketch_msg {Wrong number of columns} \
		   "You provided $incol columns of data. However, this view \
		   curve requires $k columns.  \
		   The columns should have the following format: $descr" \
		   {} 0 {OK}
		return -1
	}
	return 0
}

proc sketch_vcurve_list {} {
	global mged_sketch_vwidget mged_sketch_vprefix
	set prefix $mged_sketch_vwidget.$mged_sketch_vprefix
	set list ""
	foreach text [ info commands $prefix*.t] {
		set last [expr [string length $text] - 3]
	  	lappend list [string range $text 0 $last]
	}
	return $list
}

proc sketch_vcurve_get_label { vcurve} {
	global mged_sketch_vwidget mged_sketch_vprefix
	set prefix $mged_sketch_vwidget.$mged_sketch_vprefix
	set j [string length $prefix]
	return [string range $vcurve $j end]
}




#-----------------------------------------------------------------
# Table Editor
#-----------------------------------------------------------------
proc sketch_init_table {} {
	#table editor
	uplevel #0 set mged_sketch_init_table 1
	uplevel #0 set mged_sketch_table_lmode "replace"
	uplevel #0 set mged_sketch_table_index -1
	uplevel #0 set mged_sketch_table_prefix "_a_txt_"
	uplevel #0 set mged_sketch_table_interp "quat"
	uplevel #0 set mged_sketch_table_v0 "100%"
	uplevel #0 set mged_sketch_table_v1 "100%"
	uplevel #0 set mged_sketch_table_pcols "1,2,3"
	#dependencies
	foreach dep {main } {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

#table editor for curves
proc sketch_popup_table { p name args } {
	global mged_sketch_table_prefix

	if { $p == "." } {
		set root ".$mged_sketch_table_prefix$name"
	} else {
		set root "$p.$mged_sketch_table_prefix$name"
	}

	if { [info commands $root ] != "" } {
		raise $root
		return
	}

	sketch_popup_table_create $p $mged_sketch_table_prefix$name \
		"Table editor $name" table

	#fill with appropriate text
	switch [lindex $args 0] {
		empty {$root.t delete 1.0 end}
		curve {
		  set oldname [vdraw read n]
		  sketch_open_curve [lindex $args 1]
		  $root.t delete 1.0 end
		  sketch_text_echoc $root.t
		  sketch_open_curve $oldname }
		clone {
		  sketch_text_copy [lindex $args 1] \
		     $root.t replace }
		default {
		  $root.t delete 1.0 end
		  sketch_text_echoc $root.t
		}
	}

	#finish colbar initialization
	#$root.colbar insert 1.0 "\ttime(0)\tx(1)\ty(2)\tz(3)"
	sketch_table_bar_set $root.t $root.colbar 0.0
}

#p 	- parent widget
#suffix - name for this widget
#label  - text for label
#mode	- table (read/write) or vcurve (read only)
proc sketch_popup_table_create { p suffix label {mode table}} {

	if { $p == "." } {
		set name ".$suffix"
	} else {
		set name "$p.$suffix"
	}
	toplevel $name
	place_near_mouse $name
	wm title $name "MGED AnimMate $label"
	if { $mode == "vcurve" } {
		wm withdraw $name
	}
	text $name.t -width 80 -height 20 -wrap none \
		-tabs {20 numeric 220 numeric 420 numeric 620 numeric} \
		-xscrollcommand \
		"sketch_scroll_both $name" \
		-yscrollcommand "$name.s1 set"
	text $name.colbar -width 80 -height 1 -wrap none \
		-tabs {20 center 230 center 430 center 630 center}
	scrollbar $name.s0 -command \
		"$name.t xview" \
		-orient horizontal
	scrollbar $name.s1 -command "$name.t yview"
	frame $name.f1
	label $name.f1.l0 -text $label
	frame  $name.f0
	if { $mode == "table" } {
		button $name.f0.b3 -text "Clear" -command "$name.t delete 1.0 end"
		button $name.f0.b4 -text "Interpolate" -command "sketch_popup_table_interp $name.t $name.colbar"
		button $name.f0.b5 -text "Edit Columns" -command "sketch_popup_table_col $name.t $name.colbar"
		button $name.f0.b7 -text "Estimate Time" -command "sketch_popup_table_time $name.t"

		menubutton $name.f0.mb0 -text "Read" -menu $name.f0.mb0.m
		menu $name.f0.mb0.m -tearoff 0 -postcommand "sketch_post_read_menu $name.f0.mb0.m $name.t"
		$name.f0.mb0.m add command -label "dummy"
		button $name.f0.b6 -text "Cancel" -command "destroy $name"
	} else {
		button $name.f0.b6 -text "Hide" -command "wm withdraw $name"
	}
	button $name.f0.b8 -text "Clone" -command "incr mged_sketch_table_index; sketch_popup_table $p \$mged_sketch_table_index clone $name.t"
	button $name.f0.b9 -text "Up" -command "raise $p"
	menubutton $name.f0.mb1 -text "Write" -menu $name.f0.mb1.m
	menu $name.f0.mb1.m -tearoff 0 \
		-postcommand "sketch_post_write_menu $name.f0.mb1.m $name.t"
	$name.f0.mb1.m add command -label "dummy"
	pack $name.f0 $name.s0 -side bottom -fill x
	pack $name.s1 -side right -fill y
	pack $name.f0.mb1 -side left -fill x -expand yes
	if { $mode == "table" } {
		pack $name.f0.mb0 \
			$name.f0.b3 $name.f0.b4 $name.f0.b5 $name.f0.b7 \
			-side left -fill x -expand yes
	}
	pack $name.f0.b8 $name.f0.b9 $name.f0.b6 \
		-side left -fill x -expand yes

	pack $name.f1 $name.colbar $name.t\
		-side top -expand yes -fill x -anchor w
	pack $name.f1.l0

	if { $mode == "vcurve" } {
		$name.t configure -state disabled
	}

}

proc sketch_popup_table_time { w } {
	global mged_sketch_table_v0 mged_sketch_table_v1 \
		mged_sketch_table_pcols

	set entries [list \
		[list "Start Speed:" $mged_sketch_table_v0] \
		[list "End Speed:" $mged_sketch_table_v1] \
		[list "Path Columns:" $mged_sketch_table_pcols] \
		]
	set buttons [list \
		[list  "OK" "sketch_table_time $w \
		    \[._sketch_input.f0.e get\] \[._sketch_input.f1.e get\] \
			\[._sketch_input.f2.e get\]"] \
		{"Cancel" "destroy ._sketch_input"} \
		]
	sketch_popup_input "Estimate Time" $entries $buttons
}

proc sketch_table_time {w v0 v1 cols } {
	global mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_path \
		mged_sketch_table_v0 mged_sketch_table_v1 mged_sketch_table_pcols

	set mged_sketch_table_v0 $v0
	set mged_sketch_table_v1 $v1
	set mged_sketch_table_pcols $cols
	#global mged_sketch_table_lmode

	if { ($v0 == "100%") || ($v0 == "") } {
		set arg0 ""
	} else {
		set temp [split $v0 %]
		if { [llength $temp] > 1 } {
			set arg0 "-i [expr [lindex $temp 0]/100.0]"
		} else {
			set arg0 "-s $v0"
		}
	}
	if { ($v1 == "100%") || ($v1 == "") } {
		set arg1 ""
	} else {
		set temp [split $v1 %]
		if { [llength $temp] > 1 } {
			set arg1 "-f [expr [lindex $temp 0]/100.0]"
		} else {
			set arg1 "-e $v1"
		}
	}
	if { $cols == "" } {
		set $cols "1,2,3"
	}
	#count number of lines, doesn't matter if a couple extra
	scan [$w index end] %d maxlen
	set arg2 "-m $maxlen"
	set cmd "| ${mged_sketch_anim_path}anim_time $arg0 $arg1 $arg2 > $mged_sketch_temp1"
	#puts $cmd
	set f1 [open $cmd w]
	set mycols "0,$cols"
	sketch_text_to_fd $w $f1 $mycols
	close $f1
	#set temp $mged_sketch_table_lmode
	#set mged_sketch_table_lmode left
	set f1 [open $mged_sketch_temp1 r]
	sketch_text_from_fd $w $f1 0 left
	close $f1
	sketch_text_from_text $w $w "0,2-" replace
	file delete $mged_sketch_temp1
	#set mged_sketch_table_lmode $temp
	catch {destroy ._sketch_input}
}

proc sketch_scroll_both { w args} {
	eval $w.s0 set $args
	eval sketch_table_bar_set $w.t $w.colbar $args
}

#match number of columns in time bar with number of columns in text
#first line. Adjust time bar scroll
proc sketch_table_bar_set { w wbar args } {
	set i [sketch_text_cols $w]
	set j [sketch_text_cols $wbar]
	if { $i != $j } {
		$wbar delete 1.0 end
		set j 0
		while { $j < $i } {
			append addstr "\t$j"
			incr j
		}
		append addstr "         "
		$wbar insert "1.0 lineend" $addstr
	}
	$wbar xview moveto [lindex $args 0]
}


proc sketch_table_bar_reset { w } {
	if {[regsub {(^\..+)(\.[^\.]+$)} $w {\1} parent] == 0} {
		#no parent, do nothing
		return
	}
	if { [info commands $parent.colbar] == "" } {
		return
	}
	sketch_table_bar_set $w $parent.colbar [lindex [$w xview] 0]
}

proc sketch_post_write_menu { menu text } {
	$menu delete 0 end
	$menu add command -label "To File" \
		-command "sketch_popup_write $text file"
	if { [info globals mged_sketch_init_draw] != "" } {
		$menu add command -label "To Curve" \
			-command "sketch_popup_write $text curve"
	}
	if { [info globals mged_sketch_init_view] != "" } {
		$menu add command -label "To V-Curve" \
			-command "sketch_popup_write $text vcurve"
	}
}

proc sketch_table_list {} {
	global mged_sketch_table_prefix
	set list ""
	foreach text [info commands *.$mged_sketch_table_prefix*.t] {
		set last [expr [string length $text] - 3]
		lappend list [string range $text 0 $last]
	}
	return $list
}

proc sketch_table_get_label { ted } {
	global mged_sketch_table_prefix
	if { [regsub "(.+\\.$mged_sketch_table_prefix)(.+\$)" $ted {\2} label] } {
		return $label
	} else {
		return ""
	}
}

proc sketch_post_read_menu { menu text } {
	$menu delete 0 end
	$menu add command -label "From File" \
		-command "sketch_popup_read $text file file"
	if { [info globals mged_sketch_init_table] != "" } {
		foreach ted [sketch_table_list] {
			$menu add command \
			 -label "From Editor [sketch_table_get_label $ted]" \
			 -command "sketch_popup_read $text text $ted.t"
		}
	}
	if { [info globals mged_sketch_init_draw] != "" } {
		foreach curve [vdraw vlist l] {
			if { [info globals "mged_sketch_time_$curve"] != ""} {
			  $menu add command -label "From Curve $curve" \
			    -command "sketch_popup_read $text curve $curve"
			}
		}
	}
	if { [info globals mged_sketch_init_view] != "" } {
		foreach ved [sketch_vcurve_list] {
			$menu add command -label \
			 "From V-Curve [sketch_vcurve_get_label $ved]" \
			 -command "sketch_popup_read $text text $ved.t"
		}
	}
}

proc sketch_popup_table_col {w wbar} {
	#make sure bar is up to date
	sketch_table_bar_reset $w
	#sketch_table_bar_set $w $wbar [lindex [$w xview] 0]

	catch { destroy ._sketch_col }
	toplevel ._sketch_col
	place_near_mouse ._sketch_col
	wm title ._sketch_col "Edit Columns"
	frame ._sketch_col.fa
	frame ._sketch_col.fb
	pack ._sketch_col.fb ._sketch_col.fa -side bottom -anchor e
	set collist [lrange [split [$wbar get 1.0 "1.0 lineend"] "\t"] \
			1 end]
	set i 0
	set cmd "sketch_text_do_col $w \[._sketch_col.fb.e0 get\]"
	foreach col $collist {
		set cmd [sketch_table_col_add $i $col $cmd old]
		incr i
	}
	#append cmd "; sketch_table_bar_reset $w; destroy ._sketch_col"

	if {$i > 0} {
		bind  ._sketch_col.fr[expr $i-1].e0 <Key-Return> \
			{._sketch_col.fa.b0 invoke}
	}
	button ._sketch_col.fa.b2 -text "Add Column" -command {sketch_table_col_add_one}
	button ._sketch_col.fa.b0 -text "OK" -command $cmd
	button ._sketch_col.fa.b1 -text "Cancel" -command {destroy ._sketch_col}
	label ._sketch_col.fb.l0 -text "Number of Rows:"
	entry ._sketch_col.fb.e0 -width 5
	._sketch_col.fb.e0 insert end "all"
	bind ._sketch_col.fb.e0 <Key-Return> {._sketch_col.fa.b0 invoke}
	pack ._sketch_col.fa.b2 ._sketch_col.fa.b0 ._sketch_col.fa.b1 -side left
	pack ._sketch_col.fb.l0 ._sketch_col.fb.e0 -side left -fill x

	if { $i > 0 } {
		focus ._sketch_col.fr0.e0
	}
}

proc sketch_table_col_add_one {} {
	set num [llength [info commands ._sketch_col.fr*.e0]]
	set cmd [lindex [split [._sketch_col.fa.b0 cget -command] \;] 0]
	set cmd [sketch_table_col_add $num $num $cmd new]
	bind  ._sketch_col.fr$num.e0 <Key-Return> {._sketch_col.fa.b0 invoke}
	._sketch_col.fa.b0 configure -command $cmd
}

proc sketch_table_col_add { i col cmd flag } {
		frame ._sketch_col.fr$i
		set col [string trim $col]
		label ._sketch_col.fr$i.l0 -text "$col:"
		entry ._sketch_col.fr$i.e0 -width 20
		if { $flag == "old" } {
			._sketch_col.fr$i.e0 insert end @$i
		}
		append cmd " \[._sketch_col.fr$i.e0 get\]"
		if {$i > 0} {
			set j [expr $i-1]
			bind ._sketch_col.fr$j.e0 <Key-Return> \
			  "focus ._sketch_col.fr$i.e0"
		}
		pack ._sketch_col.fr$i -side top -fill x -expand yes
		pack ._sketch_col.fr$i.l0 ._sketch_col.fr$i.e0 -side left -fill x -expand yes
		return $cmd
}

proc sketch_text_do_col {w rows args} {
	sketch_text_col_arith $w $rows $args
	sketch_table_bar_reset $w
	destroy ._sketch_col
}


proc sketch_text_col_arith {w rows arglist} {
	set buffer $w._col_scratch_
	#destroy if exists already
	if {[info commands $buffer] != ""} {
		destroy $buffer
	}
	text $buffer
	sketch_text_do_script $buffer $w $rows $arglist
	$w delete 1.0 end
	$w insert end [$buffer get 1.0 end]
	destroy $buffer
}

#win - take text from
#wout - write text to
#rows - number of rows to write (copies source length if rows not pos. int.)
#args - series of column arithmetic descriptions
# @1 refers to column 1, @pi refers to pi, @i refers to row index
# @n refers to number of rows, @e refers to e
proc sketch_text_do_script {wout win rows slist} {
	#parse scripts
	set colout 0

	foreach script $slist {
		if {$script != ""} {
			regsub -all {@pi} $script 3.14159265358979323846 temp2
			regsub -all {@e} $temp2 2.7182818284590452354 temp
			regsub -all {(@)([in])} $temp {$column(\2)} \
				script
			regsub -all {(@)([0-9]+)}  $script {$column(\2)} \
				outscript($colout)
			incr colout
		}
	}
	if { [regexp {^[0-9]+$} $rows] == "0" } {
		set column(n) [sketch_text_rows $win]
	} else {
		set column(n) $rows
	}
	for {set i 1} { $i <= $column(n) } {incr i} {
		set cols [lrange \
			[split [$win get "$i.0" "$i.0 lineend"] "\t"] \
			1 end]
		set collen [llength $cols]
		set column(i) [expr $i - 1.0]
		for {set j 0} {$j < $collen} {incr j} {
			set column($j) "[lindex $cols $j]"
		}
		for {set j 0} {$j < $colout} {incr j} {
			$wout insert end \
	 		  [format "\t%.12g" [expr $outscript($j)]]
		}
		$wout insert end "\n"
	}
}

proc sketch_popup_table_interp {w wbar}	{
	global mged_sketch_table_interp
	#make sure bar is up to date
	sketch_table_bar_reset $w

	catch { destroy ._sketch_col }
	toplevel ._sketch_col
	place_near_mouse ._sketch_col
	wm title ._sketch_col "Column Interpolator"
	frame ._sketch_col.fz
	label ._sketch_col.fz.l0 -text "0:"
	label ._sketch_col.fz.l1 -text "Time" -width 20
	pack ._sketch_col.fz -side top -fill x -expand yes
	pack ._sketch_col.fz.l0 ._sketch_col.fz.l1 -side left -fill x -expand yes
	frame ._sketch_col.fa
	frame ._sketch_col.fb
	frame ._sketch_col.fc
	frame ._sketch_col.fd

	frame ._sketch_col.fe
	menubutton ._sketch_col.fe.mb0 -text "Active Command:" \
		-menu ._sketch_col.fe.mb0.m0
	menu ._sketch_col.fe.mb0.m0
	label ._sketch_col.fe.l0 -textvariable mged_sketch_table_interp

	pack ._sketch_col.fe ._sketch_col.fa -side bottom -fill x -expand yes
	pack ._sketch_col.fd ._sketch_col.fc \
		._sketch_col.fb -side bottom -anchor e

	set collist [lrange [split [$wbar get 1.0 "1.0 lineend"] "\t"] \
			2 end]
	set i 1
	set cmd "sketch_text_do_interp $w \[._sketch_col.fb.e0 get\] \
		\[._sketch_col.fc.e0 get\] \[._sketch_col.fd.e0 get\]"
	foreach col $collist {
		set cmd [sketch_table_interp_add $i $col $cmd old]
		incr i
	}
	#append cmd "; sketch_table_bar_reset $w; destroy ._sketch_col"

	if {$i > 1} {
		bind  ._sketch_col.fr[expr $i-1].e0 <Key-Return> \
			{focus ._sketch_col.fb.e0}
	}
	button ._sketch_col.fa.b2 -text "Add Column" -command {sketch_table_interp_add_one}
	button ._sketch_col.fa.b0 -text "OK" -command $cmd
	button ._sketch_col.fa.b1 -text "Cancel" -command {destroy ._sketch_col}
	label ._sketch_col.fb.l0 -text "Start Time:"
	entry ._sketch_col.fb.e0 -width 10
	bind ._sketch_col.fb.e0 <Key-Return> {focus ._sketch_col.fc.e0}
	label ._sketch_col.fc.l0 -text "End Time:"
	entry ._sketch_col.fc.e0 -width 10
	bind ._sketch_col.fc.e0 <Key-Return> {focus ._sketch_col.fd.e0}
	label ._sketch_col.fd.l0 -text "Frames Per Second:"
	entry ._sketch_col.fd.e0 -width 10 -textvariable mged_sketch_fps
	bind ._sketch_col.fd.e0 <Key-Return> {._sketch_col.fa.b0 invoke}
	pack ._sketch_col.fa.b2 ._sketch_col.fa.b0 ._sketch_col.fa.b1 \
		._sketch_col.fe.mb0 ._sketch_col.fe.l0 \
		-side left -expand yes -fill x
	pack ._sketch_col.fb.l0 ._sketch_col.fb.e0 \
		._sketch_col.fc.l0 ._sketch_col.fc.e0 \
		._sketch_col.fd.l0 ._sketch_col.fd.e0 \
		-side left -fill x

	._sketch_col.fe.mb0.m0 add command -label "Step (src)" -command {set mged_sketch_table_interp step }
	._sketch_col.fe.mb0.m0 add command -label "Linear (src)" -command {set mged_sketch_table_interp linear }
	._sketch_col.fe.mb0.m0 add command -label "Spline (src)" -command {set mged_sketch_table_interp spline }
	._sketch_col.fe.mb0.m0 add command -label "Periodic Spline (src)" -command {set mged_sketch_table_interp cspline }
	._sketch_col.fe.mb0.m0 add command -label "Quaternion (src)" -command {set mged_sketch_table_interp quat }
	._sketch_col.fe.mb0.m0 add command -label "Rate (init) (incr/s)" -command {set mged_sketch_table_interp rate }
	._sketch_col.fe.mb0.m0 add command -label "Accel (init) (incr/s)" -command {set mged_sketch_table_interp accel}
	._sketch_col.fe.mb0.m0 add command -label "Next (src) (offset)" -command {set mged_sketch_table_interp next}
	._sketch_col.fe.mb0.m0 add command -label "Delete Column" -command {set mged_sketch_table_interp delete}

	if { $i > 1 } {
		focus ._sketch_col.fr1.e0
	}

	#guess start and end times
	set n [sketch_text_rows $w]
	if { $n < 2} {return}
	._sketch_col.fb.e0 insert end \
		[lindex [split [$w get 1.0 "1.0 lineend"] \t] 1]
	._sketch_col.fc.e0 insert end \
		[lindex [split [$w get $n.0 "$n.0 lineend"] \t] 1]

}

proc sketch_text_interpolate { w start stop fps slist } {
	global mged_sketch_temp1 mged_sketch_temp2 mged_sketch_tab_path
	#global mged_sketch_table_lmode
	#check all instructions in args
	set i 0
	set indlist 0
	set quatcount 0
	foreach script $slist {
		set type [lindex $script 0]
		switch $type {
			step -
			spline -
			linear -
			cspline {
				lappend filelist $i
				lappend indlist [lindex $script 1]
				set cmd($i) "interp $type $i;"
				incr i
			}
			quat {
				lappend filelist $i
				lappend indlist [lindex $script 1]
				if {$quatcount == 0} {
					set cmd($i) "interp quat $i;"
					set quatend [expr $i + 4]
					set quatcount 3
				} else {
					if { $i != [expr $quatend - $quatcount] } {
						tk_dialog ._sketch_msg {Invalid entry} \
							{The interpolator requires four adjacent "quat" columns.} {} \
							 0 {OK}
						return -1
					}
					set cmd($i) ""
					incr quatcount -1
				}
				incr i
			}
			rate -
			accel {
				set cmd($i) "$type $i [lindex $script 1] \
					[lindex $script 2];"
				incr i
			}
			next {
				set cmd($i) "$type $i \
					[expr [lindex $script 1] - 1] \
					[lindex $script 2];"
				incr i
			}
			default { puts "unknown command" }
		}
	}
	if { $quatcount != 0} {
		tk_dialog ._sketch_msg {Invalid entry} \
			{The interpolator requires four adjacent "quat" columns.} {} \
			 0 {OK}
		return -1
	}
	set fd [open $mged_sketch_temp1 w]
	sketch_text_to_fd $w $fd [join $indlist ,]
	close $fd

	set fd [open $mged_sketch_temp2 w]
	if { [info exists filelist] == 1} {
		puts $fd "file $mged_sketch_temp1 $filelist;"
	}
	puts $fd "times $start $stop $fps;"
	for {set j 0} { $j < $i } {incr j} {
		puts $fd $cmd($j)
	}
	close $fd

	set fd [open "| ${mged_sketch_tab_path}tabinterp -q < $mged_sketch_temp2 " r]
	sketch_text_from_fd $w $fd all replace
	#catch can be removed when -q option added to tabinterp
	catch {close $fd}
	catch {file delete $mged_sketch_temp1 $mged_sketch_temp2}
	return 0
}

proc sketch_interp_fill { str args} {
	if { $args == "" } {
		foreach ent [info commands {._sketch_col.fr[0-9]*.e0}] {
			$ent delete 0 end
			$ent insert end $str
		}
	} else {
		set ent [info commands ._sketch_col.fr1.e0]
		for {set i 1} { $ent != ""} {
			set ent [info commands ._sketch_col.fr$i.e0]} {
			$ent delete 0 end
			$ent insert end "$str $i"
			incr i
		}
	}
}


proc sketch_table_interp_add { i col cmd flag } {
		frame ._sketch_col.fr$i
		set col [string trim $col]
		button ._sketch_col.fr$i.l0 -text "$col:" \
			-command "sketch_table_interp_entry ._sketch_col.fr$i.e0 $i"
		entry ._sketch_col.fr$i.e0 -width 20
		if { $flag == "old" } {
			._sketch_col.fr$i.e0 insert end "spline $i"
		}
		append cmd " \[._sketch_col.fr$i.e0 get\]"
		if {$i > 1} {
			set j [expr $i-1]
			bind ._sketch_col.fr$j.e0 <Key-Return> \
			  "focus ._sketch_col.fr$i.e0"
		}
		pack ._sketch_col.fr$i -side top -fill x -expand yes
		pack ._sketch_col.fr$i.l0 ._sketch_col.fr$i.e0 -side left -fill x -expand yes
		return $cmd
}

proc sketch_table_interp_entry { entry index } {
	upvar #0 mged_sketch_table_interp type

	switch $type {
		step -
		linear -
		spline -
		cspline -
		quat {
			$entry delete 0 end
			$entry insert 0 "$type $index"
		}
		rate -
		accel -
		next {
			$entry delete 0 end
			$entry insert 0 $type
		}
		delete {
			$entry delete 0 end
		}
	}
}


proc sketch_table_interp_add_one {} {
	set num [llength [info commands ._sketch_col.fr*.e0]]
	incr num
	set cmd [._sketch_col.fa.b0 cget -command]
	#set cmd [lindex [split [._sketch_col.fa.b0 cget -command] \;] 0]
	set cmd [sketch_table_interp_add $num $num $cmd new]
	bind  ._sketch_col.fr$num.e0 <Key-Return> {focus ._sketch_col.fb.e0}
	._sketch_col.fa.b0 configure -command $cmd
}

proc sketch_text_do_interp { w start stop fps args } {
	if {[sketch_text_interpolate $w $start $stop $fps $args] != 0} {
		return
	}
	sketch_table_bar_reset $w
	destroy ._sketch_col
}

proc sketch_popup_read {w type src} {
	global mged_sketch_table_lmode
	switch $type {
		file {
			set entries [list [list "File to read:" ""]]
			set okcmd "sketch_text_readf $w \
				\[._sketch_input.f0.e get\] \
				\[._sketch_input.f1.e get\] \
				\$mged_sketch_table_lmode"
		}
		curve {
			set entries {}
			set okcmd "sketch_text_readc $w $src \
				\[._sketch_input.f0.e get\] \
				\$mged_sketch_table_lmode"
		}
		default {
			set entries {}
			set okcmd "sketch_text_from_text $w $src \
				\[._sketch_input.f0.e get\] \
				\$mged_sketch_table_lmode; \
				destroy ._sketch_input"
		}
	}
	lappend entries {"Which columns:" "all"}
	set buttons [list \
		[list "OK" $okcmd] \
		[list "Cancel" "destroy ._sketch_input"] \
		]
	sketch_popup_input "Read from $src" $entries $buttons
	frame ._sketch_input.f3
	pack ._sketch_input.f3 -side bottom
	radiobutton ._sketch_input.f3.r0 -text "Replace" \
		-variable mged_sketch_table_lmode -value "replace"
	radiobutton ._sketch_input.f3.r1 -text "Append" \
		-variable mged_sketch_table_lmode -value "end"
	radiobutton ._sketch_input.f3.r2 -text "Add New Columns" \
		-variable mged_sketch_table_lmode -value "right"

	pack ._sketch_input.f3.r0 ._sketch_input.f3.r1 ._sketch_input.f3.r2 \
		-side left -fill x

}

proc sketch_text_readc {w curve col mode} {
	set oldcurve [vdraw read n]
	sketch_open_curve $curve
	set buffer $w._readc_scratch_
	text $buffer
	sketch_text_echoc $buffer
	sketch_text_from_text $w $buffer $col $mode
	destroy $buffer
	sketch_open_curve $oldcurve
	sketch_table_bar_reset $w
	catch {destroy ._sketch_input}

}

proc sketch_popup_write {w dst} {
	global mged_sketch_vname
	switch $dst {
		file {
			set entries [list [list "Write to file:" ""]]
			set okcmd "sketch_text_writef $w \
				\[._sketch_input.f0.e get\] \
				\[._sketch_input.f1.e get\]"
		}
		curve {
			set entries [list [list "Write to curve:" [vdraw read n]]]
			set okcmd "sketch_text_writec $w \
				\[._sketch_input.f0.e get\] \
				\[._sketch_input.f1.e get\]"
		}
		vcurve {
			set entries [list [list "Write to v-curve:" $mged_sketch_vname]]
			set okcmd "sketch_text_writevc $w \
				\[._sketch_input.f0.e get\] \
				\[._sketch_input.f1.e get\]"
		}

	}
	lappend entries {"Which columns:" "all"}
	set buttons [list \
		[list "OK" $okcmd] \
		[list "Cancel" "destroy ._sketch_input"] \
		]
	sketch_popup_input "Write to $dst" $entries $buttons
}

proc sketch_text_writec {w curve col} {
	set buffer $w._writec_scratch_
	text $buffer
	sketch_text_from_text $buffer $w $col append
	set i [sketch_text_cols $buffer]
	if { $i < 3 } {
		destroy $buffer
		puts "Need at least three columns"
		return -1
	}
	if { $i == 3 } {
		#assume time is missing
		sketch_text_col_arith $buffer all {@i @0 @1 @2}
		puts "Filling in missing time column"
	}
	set oldcurve [vdraw read n]
	sketch_open_curve $curve
	sketch_text_apply $buffer replace
	sketch_open_curve $oldcurve
	destroy $buffer
	catch {destroy ._sketch_input}
	sketch_update

}

proc sketch_text_writevc {w vcurve col} {
	global mged_sketch_vparams mged_sketch_vname mged_sketch_vwidget mged_sketch_vprefix

	set oldname $mged_sketch_vname
	sketch_open_vcurve $vcurve
	#check for correct number of columns
	set numcol [sketch_text_cols $w]
	if { $col == "all" } {
		set num $numcol
	} else {
		set num [sketch_parse_col $col $numcol output]
	}
	if { [sketch_vcurve_check_col $mged_sketch_vparams $num] == -1} {
		sketch_open_vcurve $oldname
		return -1
	}

	set text $mged_sketch_vwidget.$mged_sketch_vprefix$vcurve.t
	$text configure -state normal
	sketch_text_from_text $text $w $col replace
	$text configure -state disabled
	sketch_vupdate
	catch {destroy ._sketch_input}
}



#-----------------------------------------------------------------
# Create Scripts
#-----------------------------------------------------------------
proc sketch_init_objanim {} {
	# object animation
	uplevel #0 set mged_sketch_init_objanim 1
	uplevel #0 set mged_sketch_objorv "object"
	uplevel #0 set mged_sketch_objname "/foo.r"
	uplevel #0 set mged_sketch_objvsize "500"
	uplevel #0 {set mged_sketch_objcen "0 0 0"}
	uplevel #0 {set mged_sketch_objori "0 0 0"}
	uplevel #0 {set mged_sketch_eyecen "0 0 0"}
	uplevel #0 {set mged_sketch_eyeori "0 0 0"}
	uplevel #0 {set mged_sketch_objsteer ""}
	uplevel #0 set mged_sketch_objopt "none"
	uplevel #0 set mged_sketch_objmang "60"
	uplevel #0 {set mged_sketch_objlaf ""}
	uplevel #0 {set mged_sketch_objdisp ""}
	uplevel #0 {set mged_sketch_objrot ""}
	uplevel #0 set mged_sketch_objframe "0"
	uplevel #0 set mged_sketch_objscript "foo.script"
	uplevel #0 set mged_sketch_objsrctype "curve:"
	uplevel #0 set mged_sketch_objsource "foo"
	uplevel #0 set mged_sketch_objcname "foo"
	uplevel #0 set mged_sketch_objfname "foo.table"
	uplevel #0 set mged_sketch_objrv 0
	uplevel #0 set mged_sketch_objrotonly 0
	uplevel #0 set mged_sketch_objncols 4
	uplevel #0 {set mged_sketch_objcols "t x y z"}
	#dependencies
	foreach dep {main} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

#control creation of animation scripts
#mode can be obj or view
proc sketch_popup_objanim { p {mode obj} } {
	if { $p == "." } {
		set root ".oanim"
	} else {
		set root "$p.oanim"
	}
	if { [info commands $root ] != ""} {
		if { [info commands $root.l$mode] != ""} {
			raise $root
			return
		} else {
			destroy $root
		}
	}

	#create widget
	toplevel $root
	frame $root.f0
	label $root.f0.l0 -text "Output File: "
	entry $root.f0.e0 -width 20 -textvariable mged_sketch_objscript
	frame $root.f1
	label $root.f1.l0 -text Source
	tk_optionMenu $root.f1.om0 mged_sketch_objsrctype \
		"curve:" "view curve:" "table editor:" "file:"
	entry $root.f1.e0 -width 20 -textvariable mged_sketch_objsource
	frame $root.f2
	place_near_mouse $root
	if {$mode == "view"} {
		wm title $root "MGED AnimMate View Animation"
		label $root.l$mode -text "Create View Animation"
		button $root.f2.l0 -text "View Size:" -command \
			{set mged_sketch_objvsize [view size]}
		entry $root.f2.e0 -width 20 -textvariable mged_sketch_objvsize
		frame $root.f9
		button $root.f9.b0 -text "Eye Point:" \
			-command { set mged_sketch_eyecen [view eye] }
		entry $root.f9.e0 -width 20 -textvariable mged_sketch_eyecen
		frame $root.f10
		button $root.f10.b0 -text "Eye Yaw,Pitch,Roll: " \
			-command { set mged_sketch_eyeori [view ypr] }
		entry $root.f10.e0 -width 20 -textvariable mged_sketch_eyeori
		set if_view "$root.f9 $root.f10"
		checkbutton $root.cb0 -text "Read View Size from Source" \
			-variable mged_sketch_objrv -command "sketch_script_update $mode"
		$root.cb0 deselect
		uplevel #0 set mged_sketch_objdisp "-d"
		uplevel #0 set mged_sketch_objrot "-b"
		set lookat_txt "Eye path and look-at path"
	} else {
		wm title $root "MGED AnimMate Object Animation"
		set if_view ""
		label $root.l$mode -text "Create Object Animation"
		label $root.f2.l0 -text "Object Name:"
		entry $root.f2.e0 -width 20 -textvariable mged_sketch_objname
		checkbutton $root.cb1 -text "Relative Displacement" \
			-variable mged_sketch_objdisp -offvalue "-d" -onvalue "-c"
		checkbutton $root.cb2 -text "Relative Orientation" \
			-variable mged_sketch_objrot -offvalue "-b" -onvalue "-a"
		uplevel #0 set mged_sketch_objrv 0
		$root.cb1 deselect
		$root.cb2 deselect
		set lookat_txt "Object path and look-at path"
	}
	frame $root.f3
	button $root.f3.b0 -text "Object Center:" \
		-command { set mged_sketch_objcen [view center] }
	entry $root.f3.e0 -width 20 -textvariable mged_sketch_objcen
	frame $root.f4
	button $root.f4.b0 -text "Object Yaw,Pitch,Roll: " \
		-command { set mged_sketch_objori [view ypr] }
	entry $root.f4.e0 -width 20 -textvariable mged_sketch_objori
	checkbutton $root.cb3 -text "No Translation" \
		-variable mged_sketch_objrotonly -command "sketch_script_update $mode"
	$root.cb3 deselect
	frame $root.f5
	label $root.f5.l0 -text "First Frame:"
	entry $root.f5.e0 -width 20 -textvariable mged_sketch_objframe
	frame $root.f6
	button $root.f6.b0 -text "OK" -command "sketch_objanim $mode"
	button $root.f6.b1 -text "Show Script" -command "sketch_popup_preview $p \$mged_sketch_objscript"
	button $root.f6.b2 -text "Up" -command "raise $p"
	button $root.f6.b3 -text "Cancel" -command "destroy $root"

	label $root.l1 -text "Orientation Control: "
	radiobutton $root.rb0 -text "No Rotation" \
		-variable mged_sketch_objopt -value "none" -command "sketch_script_update $mode"
	radiobutton $root.rb1 -text "Automatic Steering" \
		-variable mged_sketch_objopt -value "steer" -command "sketch_script_update $mode"
	radiobutton $root.rb2 -text "Automatic Steering and Banking" \
		-variable mged_sketch_objopt -value "bank" -command "sketch_script_update $mode"
	frame $root.f7
	label $root.f7.l0 -text "    Maximum Bank Angle ="
	entry $root.f7.e0 -textvariable mged_sketch_objmang -width 4
	radiobutton $root.rb3 -text "Rotation Specified as YPR" \
		-variable mged_sketch_objopt -value "ypr" -command "sketch_script_update $mode"
	radiobutton $root.rb4 -text "Rotation Specified as Quat" \
		-variable mged_sketch_objopt -value "quat" -command "sketch_script_update $mode"
	radiobutton $root.rb5 -text $lookat_txt \
		-variable mged_sketch_objopt -value "lookat" -command "sketch_script_update $mode"
	frame $root.f8
	label $root.f8.l0 -textvariable mged_sketch_objncols
	label $root.f8.l1 -text "Input Columns Needed:"
	label $root.f8.l2 -textvariable mged_sketch_objcols


	pack	$root.l$mode $root.f0 $root.f1 \
		-side top -fill x -expand yes
	pack 	$root.f8 -side top
	eval pack	$root.f2 $root.f3 \
		$root.f4 ${if_view} $root.f5 \
		-side top -fill x -expand yes

	if {$mode == "view"} {
		pack $root.cb0 $root.cb3 -side top -anchor w

		pack $root.f9.b0 $root.f10.b0 \
			-side left -anchor w
		pack $root.f9.e0 $root.f10.e0 \
			-side right -anchor e
	} else {
		pack $root.cb1 $root.cb2 $root.cb3 \
			-side top -anchor w
	}

	pack 	$root.l1 -side top -anchor w

	pack 	$root.rb0 $root.rb1 $root.rb2 \
		-side top -anchor w
	pack	$root.f7 -side top -anchor e
	pack	$root.rb3 $root.rb4 $root.rb5 \
		-side top -anchor w
	pack 	$root.f6 -side top -fill x -expand yes


	pack \
		$root.f0.l0 $root.f1.l0 $root.f1.om0\
		$root.f2.l0 $root.f3.b0 \
		$root.f4.b0 $root.f5.l0 \
		$root.f8.l0 $root.f8.l1 $root.f8.l2\
		-side left -anchor w

	pack \
		$root.f0.e0 $root.f1.e0 $root.f2.e0\
		$root.f3.e0 $root.f4.e0 $root.f5.e0\
		-side right -anchor e

	pack \
		$root.f6.b0 $root.f6.b1 $root.f6.b2 $root.f6.b3 \
		-side left -fill x -expand yes

	pack \
		$root.f7.e0 $root.f7.l0 \
		-side right

	focus $root.f0.e0
	bind $root.f0.e0 <Key-Return> "focus $root.f1.e0"
	bind $root.f1.e0 <Key-Return> "focus $root.f2.e0"
	bind $root.f2.e0 <Key-Return> "focus $root.f3.e0"
	bind $root.f3.e0 <Key-Return> "focus $root.f4.e0"
	if { $mode == "view" } {
		bind $root.f4.e0 <Key-Return> "focus $root.f9.e0"
		bind $root.f9.e0 <Key-Return> "focus $root.f10.e0"
		bind $root.f10.e0 <Key-Return> "focus $root.f5.e0"
	} else {
		bind $root.f4.e0 <Key-Return> "focus $root.f5.e0"
	}
	bind $root.f5.e0 <Key-Return> "$root.f6.b0 invoke"

	sketch_script_update $mode
}

#create animation script
proc sketch_objanim { objorview } {
	global mged_sketch_objname \
		mged_sketch_objcen mged_sketch_objori mged_sketch_objsteer \
		mged_sketch_eyecen mged_sketch_eyeori \
		mged_sketch_objopt mged_sketch_objdisp mged_sketch_objrot \
		mged_sketch_objframe mged_sketch_objscript \
		mged_sketch_objmang \
		mged_sketch_objorv mged_sketch_objvsize \
		mged_sketch_objlaf mged_sketch_objrv mged_sketch_objrotonly \
		mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_path \
		mged_sketch_table_prefix \
		mged_sketch_vwidget mged_sketch_vprefix

	upvar #0 mged_sketch_objsrctype ltype
	upvar #0 mged_sketch_objsource src
	upvar #0 mged_sketch_objncols ncols

	# make sure animated object exists (this will create an error if it doesn't)
	set tmp 0
	set tmp [db get $mged_sketch_objname]
	if { $tmp == 0 } return

	#find the source
	switch $ltype {
		"curve:" {
			set type curve
			set oldcurve [vdraw read n]
			vdraw send
			set ret [sketch_open_curve $src]
			if {$ret != 0} {
				tk_dialog ._sketch_msg {Couldn't find curve} \
				  "Couldn't find curve $src." \
				  {} 0 {OK}
				sketch_open_curve $oldcurve
				return
			}
		}
		"view curve:" {
			set type text
			set w ""
			foreach ved [sketch_vcurve_list] {
				if { [sketch_vcurve_get_label $ved] == $src} {
					set w $ved.t
					break
				}
			}
			if { $w == ""} {
				tk_dialog ._sketch_msg {Couldn't find view curve} \
				"Couldn't find view curve $src." \
				{} 0 {OK}
				return
			}
		}
		"table editor:" {
			set type text
			set w ""
			foreach ted [sketch_table_list] {
				if { [sketch_table_get_label $ted] == $src } {
					set w $ted.t
					break
				}
			}
			if { $w  == ""} {
				tk_dialog ._sketch_msg {Couldn't find editor} \
				  "Couldn't find table editor $src. \
				   (Text editor identifier must be an integer)." \
				  {} 0 {OK}
				return
			}
		}
		"file:" {
			#non-existent file errors handled by Tcl
			set type file
		}
		default {
			puts "sketch_objanim: Unknown ltype $ltype"
			return -1
		}
	}


	#test for valid number of columns
	switch $type {
	"curve" {
		if {$ncols != 4} {
			tk_dialog ._sketch_msg {Wrong number of columns} \
			"The animation you requested requires $ncols \
			 input columns. A curve provides 4." {} 0 "OK"
			sketch_open_curve $oldcurve
			return
		}
	}
	"text" {
		set nsrc [sketch_text_cols $w]
		if { $nsrc > $ncols } {
			set ans [tk_dialog ._sketch_msg {Excess columns} \
			"The animation you requested only uses $ncols \
			input columns. Text editor $src has $nsrc columns. \
			Only the first $ncols columns will be used." \
			{} 0 {OK} {Cancel}]
			if { $ans } return
			set colsp "0-[expr $ncols-1]"
		} elseif { $nsrc < $ncols } {
			tk_dialog ._sketch_msg {Insufficient columns} \
			"The animation you requested requires $ncols \
			input columns. Text editor $src has only $nsrc." \
			{} 0 "OK"
			return
		} else {
			set colsp all
		}
	}
	"file" {
		set fd [open $src r]
		gets $fd line
		close $fd
		set nsrc [sketch_line_cols $line]
		if { $nsrc > $ncols } {
			set ans [tk_dialog ._sketch_msg {Excess columns} \
			"The animation you requested only uses $ncols \
			input columns. File $src has $nsrc columns. \
			Only the first $ncols columns will be used." \
			{} 0 {OK} {Cancel}]
			if { $ans } return
			for {set i 0} { $i < $nsrc} {incr i} {
				append incol " $i"
				if { $i < $ncols } {
					append outcol " $i"
				}
			}
			set filecmd "${mged_sketch_anim_path}chan_permute -i $src $incol -o stdout $outcol"
		} elseif { $nsrc < $ncols } {
			tk_dialog ._sketch_msg {Insufficient columns} \
			"The animation you requested requires $ncols \
			input columns. File $src has only $nsrc." \
			{} 0 "OK"
			return
		} else {
			set filecmd ""
		}
	}
	}

	#check for overwriting script file
	if {[file exists $mged_sketch_objscript] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			"File $mged_sketch_objscript already exists." \
			{} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}


	# build argument string
	if {$mged_sketch_objframe == ""} { set mged_sketch_objframe 0}
	set opts "-f $mged_sketch_objframe"
	if { $objorview == "view" } {
		set rcen $mged_sketch_objcen
		set rypr $mged_sketch_objori
		set wcen $mged_sketch_eyecen
		set wypr $mged_sketch_eyeori
		set i 0
		if {$rcen == ""} { set rcen "0 0 0"; incr i }
		if {$rypr == ""} { set rypr "0 0 0"; incr i }
		if {$wcen == ""} { set wcen "0 0 0"; incr i }
		if {$wypr == ""} { set wypr "0 0 0"; incr i }
		if { $i < 4 } {
			set fd [open "| ${mged_sketch_anim_path}anim_cascade \
			   -or -fc $wcen -fy $wypr -ac $rcen -ay $rypr" r]
			gets $fd line
			close $fd
			set veye [lrange $line 1 3]
			set vypr [lrange $line 4 6]
			#puts "ypri $ypri eyei $eyei"
			append opts " -d $veye -b $vypr"
		}
		if { $mged_sketch_objrv == 1} {
			set ovname " -v -1"
			set lookat_v "-v"
		} else {
			set ovname " -v $mged_sketch_objvsize"
			set lookat_v ""
		}
	} else {
		if { $mged_sketch_objcen != "" } {
			append opts " $mged_sketch_objdisp $mged_sketch_objcen"
		}
		if { $mged_sketch_objori != "" } {
			append opts " $mged_sketch_objrot $mged_sketch_objori"
		}
		set ovname " $mged_sketch_objname"
		set lookat_v ""
	}
	#puts "anim_script options: $opts"

	if { $mged_sketch_objopt == "lookat" } {
		set anim_lookat ${mged_sketch_anim_path}anim_lookat
		if { $type == "curve" } {
			#This shouldn't happen
			puts "sketch_objanim: Can't do lookat orientation \
				from curve."
			return
		} elseif { $type == "text" } {
			set fd [open "| $anim_lookat -y $lookat_v | \
			  ${mged_sketch_anim_path}anim_script $opts $ovname > \
			  $mged_sketch_objscript" w]
			sketch_text_to_fd $w $fd $colsp
			catch {close $fd}
			return
		} elseif { $type == "file" } {
			if { $filecmd == "" } {
				catch {eval exec $anim_lookat -y $lookat_v < $src | \
				  ${mged_sketch_anim_path}anim_script $opts $ovname > \
				  $mged_sketch_objscript}
			} else {
				catch {eval exec $filecmd | $anim_lookat -y $lookat_v | \
				  ${mged_sketch_anim_path}anim_script $opts $ovname > \
				  $mged_sketch_objscript}
			}

			return
		}
		return

	}

	if { $mged_sketch_objopt == "bank" } {
		if { $mged_sketch_objmang > 89 } {
			set mged_sketch_objmang 89
		} elseif { $mged_sketch_objmang < -89 } {
			set mged_sketch_objmang -89
		}
		set do_bank ${mged_sketch_anim_path}anim_fly
		if { $type == "curve" } {
			set sfile $mged_sketch_temp1
			set fd [open $sfile w]
			sketch_write_to_fd $fd [vdraw read l]
			close $fd
		} elseif { $type == "text" } {
			set sfile $mged_sketch_temp1
			set fd [open $sfile w]
			sketch_text_to_fd $w $fd "0,1,2,3"
			close $fd
		} elseif { $type == "file"} {
			if { $filecmd == ""} {
				set sfile $src
			} else {
				set sfile $mged_sketch_temp1
				exec $filecmd > $sfile
			}
		}

		set factor [exec $do_bank -b $mged_sketch_objmang < $sfile]
		eval exec $do_bank -f $factor < $sfile \
			| ${mged_sketch_anim_path}anim_script $opts $ovname > $mged_sketch_objscript

		if { $type == "curve" } {
			sketch_open_curve $oldcurve
			file delete $sfile
		} elseif { $type == "text" } {
			file delete $sfile
		} elseif { $type == "file"} {
			catch {rm $mged_sketch_temp1}
		}
		return
	}


	#else just use anim_script
	switch $mged_sketch_objopt {
	none	{ append opts " -t" }
	steer	{ append opts " -s" }
	ypr	{ }
	quat	{ append opts " -q -p" }
	}

	if { $mged_sketch_objrotonly } {
		append opts " -r"
	}

	#puts "anim_script options: $opts"
	#puts "anim_script name: $ovname"
	if { $type == "file"} {
		#puts "filecmd = $filecmd src = $src"
		if { $filecmd == "" } {
			eval exec ${mged_sketch_anim_path}anim_script $opts $ovname < $src > \
			  $mged_sketch_objscript
		} else {
			eval exec $filecmd | ${mged_sketch_anim_path}anim_script $opts $ovname | \
			  $mged_sketch_objscript
		}
	} elseif { $type == "curve" } {
		set fd [open \
		     [concat | ${mged_sketch_anim_path}anim_script $opts $ovname > \
		     $mged_sketch_objscript] w ]
		sketch_write_to_fd $fd [vdraw read l]
		close $fd
		sketch_open_curve $oldcurve
	} elseif { $type == "text" } {
		set fd [open \
		     [concat | ${mged_sketch_anim_path}anim_script $opts $ovname > \
		     $mged_sketch_objscript] w ]
		sketch_text_to_fd $w $fd $colsp
		close $fd
	}

	return
}

proc sketch_script_update { objorview } {
	global mged_sketch_objopt mged_sketch_objcols mged_sketch_objncols \
		mged_sketch_objrv mged_sketch_objrotonly

	switch $mged_sketch_objopt {
	none	-
	steer	{
		set base "t x y z"
		set mged_sketch_objrotonly 0
	}
	bank	{
		set base "t x y z"
		set mged_sketch_objrv 0
		set mged_sketch_objrotonly 0
	}
	ypr	{set base "t x y z y p r"}
	quat	{set base "t x y z qx qy qz qw"}
	lookat  {
		set base "t x y z lx ly lz"
		set mged_sketch_objrotonly 0
	}
	}

	if { $mged_sketch_objrotonly } {
		set base [lreplace $base 1 3]
	}

	if { ($mged_sketch_objrv) && ($mged_sketch_objopt != "lookat") } {
		set base [linsert $base 1 v]
	}

	set mged_sketch_objncols [llength $base]
	set mged_sketch_objcols $base
}



#-----------------------------------------------------------------
# Create Track Animation Scripts
#-----------------------------------------------------------------
proc sketch_init_track {} {
	#track animation
	uplevel #0 set mged_sketch_init_track 1
	uplevel #0 {set mged_sketch_track_vsrc ""}
	uplevel #0 {set mged_sketch_track_wname ""}
	uplevel #0 {set mged_sketch_track_wsrc ""}
	uplevel #0 {set mged_sketch_track_pname ""}
	uplevel #0 set mged_sketch_track_npads 1
	uplevel #0 {set mged_sketch_track_dist "-s"  }
	uplevel #0 {set mged_sketch_track_type Minimize  }
	uplevel #0 {set mged_sketch_track_len ""  }
	uplevel #0 {set mged_sketch_track_geom 0  }
	uplevel #0 {set mged_sketch_track_arced "0"}
	uplevel #0 {set mged_sketch_track_antistr 0}
	#dependencies
	foreach dep {main objanim} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

#control creation of animation scripts
proc sketch_popup_track_anim { p } {
	if { $p == "." } {
		set root ".tanim"
	} else {
		set root "$p.tanim"
	}
	if { [info commands $root] != ""} {
		raise $root
		return
	}

	toplevel $root

	place_near_mouse $root
	wm title $root "MGED AnimMate Track Animation"
	label $root.l0 -text "Create Track Animation"
	frame $root.f0
	label $root.f0.l0 -text "Output File: "
	entry $root.f0.e0 -width 20 -textvariable mged_sketch_objscript
	frame $root.f1
	label $root.f1.l0 -text "Vehicle Path from Table: "
	entry $root.f1.e0 -width 20 -textvariable mged_sketch_track_vsrc
	frame $root.f1a
	radiobutton $root.f1a.r0 -text "Distance" -variable mged_sketch_track_dist -value "-u"
	radiobutton $root.f1a.r1 -text "Position" -variable mged_sketch_track_dist -value "-s"
	radiobutton $root.f1a.r2 -text "Position and YPR" -variable mged_sketch_track_dist -value "-y"
	frame $root.f2
	label $root.f2.l0 -text "Wheel Specs from Table: "
	entry $root.f2.e0 -width 20 -textvariable mged_sketch_track_wsrc
	frame $root.fw
	label $root.fw.l0 -text "Wheel Base Name:"
	entry $root.fw.e0 -width 20 -textvariable mged_sketch_track_wname
	frame $root.f4
	label $root.f4.l0 -text "Pad Base Name:"
	entry $root.f4.e0 -width 20 -textvariable mged_sketch_track_pname
	frame $root.f5
	label $root.f5.l0 -text "Number of Pads: "
	entry $root.f5.e0 -textvariable mged_sketch_track_npads
	frame $root.f3
	tk_optionMenu $root.f3.om mged_sketch_track_type \
		"Minimize" "Elastic" "Rigid"
	label $root.f3.l0 -text "Track Length:"
	entry $root.f3.e0 -width 20 -textvariable mged_sketch_track_len
	frame $root.f3a
	button $root.f3a.b0 -text "Get Track Length from Wheel Specs" \
		-command "sketch_track_get_length \$mged_sketch_track_wsrc"
	frame $root.f6
	button $root.f6.b0 -text "Vehicle Center:" \
			-command { set mged_sketch_objcen [view center] }
	entry $root.f6.e0 -width 20 -textvariable mged_sketch_objcen
	frame $root.f7
	button $root.f7.b0 -text "Vehicle Yaw,Pitch,Roll: " \
			-command { set mged_sketch_objori [view ypr] }
	entry $root.f7.e0 -width 20 -textvariable mged_sketch_objori
	frame $root.f8
	label $root.f8.l0 -text "First Frame:"
	entry $root.f8.e0 -width 20 -textvariable mged_sketch_objframe
	frame $root.fa
	checkbutton $root.fa.cb -text "Create Geometry File from Frame:" -variable mged_sketch_track_geom
	entry $root.fa.e0 -width 3 -textvariable mged_sketch_track_arced
	checkbutton $root.cb0 -text "Enable Anti-Strobing" -variable mged_sketch_track_antistr
	frame $root.f9
	button $root.f9.b0 -text "OK" -command {sketch_do_track }
	button $root.f9.b1 -text "Show Script" -command "sketch_popup_preview $p \$mged_sketch_objscript"
	button $root.f9.b2 -text "Up" -command "raise $p"
	button $root.f9.b3 -text "Cancel" -command "destroy $root"


	pack	$root.l0 $root.f0 $root.f1 $root.f1a $root.f2 \
		$root.fw \
		$root.f4 $root.f5 $root.f3 $root.f3a\
		$root.f6 $root.f7 \
		$root.f8 $root.fa $root.cb0 $root.f9\
		-side top -fill x -expand yes

	pack \
		$root.f0.l0 $root.f1.l0 $root.f2.l0 \
		$root.fw.l0 \
		$root.f4.l0 $root.f5.l0 \
		$root.f6.b0 $root.f7.b0 \
		$root.f8.l0 \
		$root.f3.om $root.f3.l0 \
		-side left -anchor w

	pack \
		$root.f0.e0 $root.f1.e0 $root.f2.e0 $root.f3.e0\
		$root.fw.e0 $root.f4.e0 $root.f5.e0\
		$root.f6.e0 $root.f7.e0 $root.f8.e0 \
		$root.fa.e0 $root.fa.cb \
		$root.f1a.r2 $root.f1a.r1 $root.f1a.r0 \
		$root.f3a.b0 \
		-side right -anchor e

	pack \
		$root.f9.b0 $root.f9.b1 $root.f9.b2 $root.f9.b3 \
		-side left -fill x -expand yes


	focus $root.f0.e0
	bind $root.f0.e0 <Key-Return> "focus $root.f1.e0"
	bind $root.f1.e0 <Key-Return> "focus $root.f2.e0"
	bind $root.f2.e0 <Key-Return> "focus $root.fw.e0"
	bind $root.fw.e0 <Key-Return> "focus $root.f4.e0"
	bind $root.f4.e0 <Key-Return> "focus $root.f5.e0"
	bind $root.f5.e0 <Key-Return> "focus $root.f3.e0"
	bind $root.f3.e0 <Key-Return> "focus $root.f6.e0"
	bind $root.f6.e0 <Key-Return> "focus $root.f7.e0"
	bind $root.f7.e0 <Key-Return> "focus $root.f8.e0"
	bind $root.f8.e0 <Key-Return> "focus $root.fa.e0"
	bind $root.fa.e0 <Key-Return> "$root.f9.b0 invoke; focus $root"


}

proc sketch_track_get_length { tid } {
	global mged_sketch_temp1 mged_sketch_anim_path mged_sketch_track_len

	set text [sketch_text_from_table $tid 4]
	if { $text == "" } {return 0}
	set fd [open $mged_sketch_temp1 w]
	sketch_text_to_fd $text $fd all
	close $fd
	set fd [open "| ${mged_sketch_anim_path}anim_track \
		-c $mged_sketch_temp1" r]
	catch {flush $fd}
	gets $fd length
	catch {close $fd}
	set mged_sketch_track_len $length
	file delete $mged_sketch_temp1
}


proc sketch_do_track { } {

	global mged_sketch_temp1 mged_sketch_anim_path \
		mged_sketch_track_vsrc \
		mged_sketch_track_wsrc \
		mged_sketch_track_dist \
		mged_sketch_track_type \
		mged_sketch_track_len \
		mged_sketch_track_geom \
		mged_sketch_track_arced mged_sketch_track_antistr\
		mged_sketch_objcen mged_sketch_objori mged_sketch_objframe

	upvar #0 mged_sketch_track_wname wname
	upvar #0 mged_sketch_track_pname pname
	upvar #0 mged_sketch_track_npads numpads
	upvar #0 mged_sketch_objscript outfile
	set ypr $mged_sketch_objori
	set center $mged_sketch_objcen

	#check for overwriting script file
	if {[file exists $outfile] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			"File $outfile already exists." \
			{} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set wtext [sketch_text_from_table $mged_sketch_track_wsrc 4]
	if { $wtext == "" } { return -1 }

	switch -- $mged_sketch_track_dist {
		"-u" {set needcol 2}
		"-s" {set needcol 4}
		"-y" {set needcol 7}
	}


	if { ($mged_sketch_track_vsrc == "") && $mged_sketch_track_geom && ($mged_sketch_track_arced == "0") } {
		set g_except 1
		set vtext ._sketch_track_vtext
		catch {destroy ._sketch_track_vtext}
		text $vtext
		$vtext insert 1.0 "0 0 0 0 0 0 0"
	} else {
		set g_except 0
		set vtext \
		  [sketch_text_from_table $mged_sketch_track_vsrc $needcol]
		if { $vtext == "" } { return -1 }
	}


	set numwheels [sketch_text_rows $wtext]
	if { $numwheels < 2 } {
		tk_dialog ._sketch_msg "Not enough wheels" "The wheel file \
		has only $numwheels wheel(s). You must specify the position \
		and radius of at least 2 wheels." {} 0 "OK"
		return -1
	}

	if { $wname == "" } {
		set wcmd ""
	} else {
		set wcmd "-w $wname"
		# make sure wheel names exist
		set tmp 0
		for { set count 0 } { $count < $numwheels } { incr count } {
			set tmp [db get $wname$count]
			if { $tmp == 0 } return
		}
	}
	if { ($pname == "") || ($numpads == "") } {
		set pcmd ""
	} else {
		set pcmd "-p $numpads $pname"
		# make sure the pad names exist
		set tmp 0
		for { set count 0 } { $count < $numpads } { incr count } {
			set tmp [db get $pname$count]
			if { $tmp == 0 } return
		}
	}
	if { ($pcmd == "") && ($wcmd == "") } {
		tk_dialog ._sketch_msg "AnimMate Track animation error" \
		   "You must specify a pad name and number and/or a wheel name." {} 0 "OK"
		return -1;
	}
	set fd [open $mged_sketch_temp1 w]
	sketch_text_to_fd $wtext $fd all
	close $fd

	while { [llength $ypr] < 3 } { lappend ypr 0}
	while { [llength $center] < 3 } { lappend center 0}
	if { $mged_sketch_track_geom == 1 } {
		if { $mged_sketch_track_arced == "" } {
			set mged_sketch_track_arced 0
		}
		set arccmd "-g $mged_sketch_track_arced"
	} else {
		set arccmd ""
	}

	set tlen [expr $mged_sketch_track_len]
	if { $tlen == "" } {
		set tlen 0
	}
	switch $mged_sketch_track_type {
		Minimize { set lencmd "-lm" }
		Elastic { set lencmd "-le $tlen" }
		Rigid { set lencmd "-lf $tlen" }
	}

	if { $mged_sketch_objframe != ""} {
		set fcmd "-f $mged_sketch_objframe"
	} else {
		set fcmd ""
	}

	if { $mged_sketch_track_antistr == 1 } {
		set acmd "-a"
	} else {
		set acmd " "
	}

	set myargs "$acmd $lencmd $arccmd $mged_sketch_track_dist -b $ypr \
		-d $center $fcmd $wcmd $pcmd"
	#puts $myargs

	set fd [ open "| ${mged_sketch_anim_path}anim_track \
		$myargs	$mged_sketch_temp1 > $outfile" w]
	sketch_text_to_fd $vtext $fd all
	close $fd
	file delete $mged_sketch_temp1
	if { $g_except } {
		destroy $vtext
	}
}


#-----------------------------------------------------------------
# Combine Scripts
#-----------------------------------------------------------------
proc sketch_init_sort {} {
	uplevel #0 set mged_sketch_init_sort 1
	uplevel #0 set mged_sketch_sort_temp "./_mged_sketch_sort_"
	#dependencies
	foreach dep {main} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

proc sketch_popup_sort { p } {
	if { $p == "." } {
		set root ".sort"
	} else {
		set root "$p.sort"
	}
	if { [info commands $root] != ""} {
		raise $root
		return
	}
	toplevel $root
	place_near_mouse $root
	wm title $root "MGED AnimMate Combine Scripts"
	label $root.l0 -text "Combine Scripts"
	frame $root.f0
	label $root.f0.l0 -text "Combine Scripts:"
	entry $root.f0.e0 -width 20
	frame $root.f0.f0
	listbox $root.f0.f0.lb0 -height 6 -width 20 \
		-yscrollcommand "$root.f0.f0.s0 set"
	scrollbar $root.f0.f0.s0 -command "$root.f0.f0.lb0 yview"
	frame $root.f1
	button $root.f1.b0 -text "Filter:" -command " \
	  sketch_list_filter $root.f1.f1.lb1 \[$root.f1.e1 get \] "
	entry $root.f1.e1 -width 20
	frame $root.f1.f1
	listbox $root.f1.f1.lb1 -height 6 -width 20 \
		-yscrollcommand "$root.f1.f1.s0 set"
	scrollbar $root.f1.f1.s0 -command "$root.f1.f1.lb1 yview"
	frame $root.f2
	label $root.f2.l0 -text "Create Script: "
	entry $root.f2.e0 -width 20
	frame $root.f3
	button $root.f3.b0 -text "OK" -command "sketch_sort $root \
		\[$root.f2.e0 get\] $root.f0.f0.lb0; \
		$root.f1.b0 invoke"
	button $root.f3.b1 -text "Show Script" -command "sketch_popup_preview $p \[$root.f2.e0 get\]"
	button $root.f3.b2 -text "Up" -command "raise $p"
	button $root.f3.b3 -text "Cancel" -command "destroy $root"

	bind $root.f0.e0 <Key-Return> " sketch_sort_entry1 $root.f0.e0 $root.f0.f0.lb0 $root.f2.e0 "
	bind $root.f1.e1 <Key-Return> " $root.f1.b0 invoke "
	bind $root.f0.f0.lb0 <Button-1> "sketch_list_remove_y $root.f0.f0.lb0 %y "
	bind $root.f1.f1.lb1 <Button-1> "sketch_list_add_y $root.f1.f1.lb1 $root.f0.f0.lb0 %y "
	bind $root.f2.e0 <Key-Return> "$root.f3.b0 invoke"

	$root.f2.e0 insert end ".script"
	$root.f2.e0 selection range 0 end
	$root.f2.e0 icursor 0

	$root.f1.e1 insert end "./*.script"
	sketch_list_filter $root.f1.f1.lb1 "./*.script"

	pack $root.f3 $root.f2 -side bottom -fill x -expand yes
	pack $root.l0 -side top
	pack $root.f0 -side left
	pack $root.f1 -side right

	pack $root.f0.l0 $root.f0.e0 $root.f0.f0 \
		$root.f1.b0 $root.f1.e1 $root.f1.f1 \
		-side top -anchor w
	pack $root.f2.l0 $root.f2.e0 \
		-side left
	pack $root.f3.b0 $root.f3.b1 $root.f3.b2 $root.f3.b3 \
		-side left -expand yes -fill x
	pack $root.f0.f0.lb0 $root.f0.f0.s0 \
		$root.f1.f1.lb1 $root.f1.f1.s0 \
		-side left -fill y -expand yes

	focus $root.f0.e0

}

proc sketch_sort_entry1 { entry list nentry } {
	if { [set line [$entry get]] == "" } {
		focus $nentry
	}
	$list insert end $line
	$entry delete 0 end
}

proc sketch_sort { sortp outfile list } {
	global mged_sketch_sort_temp mged_sketch_tab_path

	if { [info commands $sortp.fa] != "" } {
		tk_dialog ._sketch_msg {Script already sorting} \
		  "The previous script is still being sorted" {} 0 "OK"
		return
	}
	#check for overwriting script file
	if {[file exists $outfile] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			"File $outfile already exists." \
			{} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set files ""
	foreach file [$list get 0 end] {
		append files "$file "
	}
	set pid [eval exec cat $files | ${mged_sketch_tab_path}scriptsort \
		-q -b 1 > $outfile &]

	frame $sortp.fa
	label $sortp.fa.l0 -text "Sorting $outfile ..."
	button $sortp.fa.b0 -text "Halt" -command "exec kill $pid"
	pack $sortp.fa -side bottom -before $sortp.f3
	pack $sortp.fa.l0 $sortp.fa.b0 -side left -fill x

	set done "destroy $sortp.fa"
	sketch_sort_monitor $outfile -1 $done
}

proc sketch_sort_monitor { file oldtime script} {
	set newtime [file mtime $file]
	if { $newtime > $oldtime } {
		after 1000 [list sketch_sort_monitor $file $newtime $script]
	} else {
		eval $script
	}
}

proc sketch_list_remove_y { list y } {
	$list delete [$list nearest $y]
}

proc sketch_list_add_y { in out y } {
	$out insert end [$in get [$in nearest $y]]
}

proc sketch_list_filter { list filter } {
	$list delete 0 end
	if { $filter == "" } return
	foreach file [glob $filter] {
		$list insert end [file tail $file]
	}
}

#-----------------------------------------------------------------
# Show Script
#-----------------------------------------------------------------
proc sketch_init_preview {} {
	# preview script
	uplevel #0 set mged_sketch_init_preview 1
	uplevel #0 {set mged_sketch_prevs ""}
	uplevel #0 {set mged_sketch_preve ""}
	uplevel #0 {set mged_sketch_prevp ""}
	uplevel #0 {set mged_sketch_prev_size [view size]}
	uplevel #0 {set mged_sketch_prev_center [view center]}
	uplevel #0 {set mged_sketch_prev_quat [view quat]}
	uplevel #0 {set mged_sketch_prev_fps "30"}
	#dependencies
	foreach dep {main} {
		if { [info globals mged_sketch_init_$dep] == "" } {
			sketch_init_$dep
		}
	}
}

#control animation previews
proc sketch_popup_preview { p {filename ""} } {
	if { $p == "." } {
		set root ".prev"
	} else {
		set root "$p.prev"
	}
	if { [info commands $root] != ""} {
		raise $root
		if { $filename != "" } {
			$root.f0.e0 delete 0 end
			$root.f0.e0 insert end $filename
		}
		$root.f0.e0 selection range 0 end
		return
	}
	toplevel $root
	place_near_mouse $root
	wm title $root "MGED AnimMate Show Script"
	label $root.l0 -text "Show Script"
	frame $root.f0
	label $root.f0.l0 -text "Script File: "
	entry $root.f0.e0 -width 20
	frame $root.f1
	label $root.f1.l0 -text "Max Frames Per Second:"
	entry $root.f1.e0 -width 5 \
		-textvariable mged_sketch_prev_fps
	frame $root.f2
	label $root.f2.l0 -text "Start Frame: "
	entry $root.f2.e0 -width 5 -textvariable mged_sketch_prevs
	frame $root.f3
	label $root.f3.l0 -text "End Frame: "
	entry $root.f3.e0 -width 5 -textvariable mged_sketch_preve
	checkbutton $root.cb0 -text "Polygon Rendering" \
		-variable mged_sketch_prevp -onvalue "-v" -offvalue ""
	frame $root.f4
	button $root.f4.b0 -text "Show" \
		-command "sketch_preview \[$root.f0.e0 get\]"
	button $root.f4.b1 -text "Up" -command "raise $p"
	button $root.f4.b2 -text "Cancel" -command "destroy $root"
	button $root.f4.b3 -text "Restore" -command "sketch_prev_restore"

	$root.f0.e0 delete 0 end
	$root.f0.e0 insert 0 $filename
	$root.f0.e0 selection range 0 end

	pack $root.l0 \
		$root.f0 $root.f1 $root.f2 \
		$root.f3 $root.cb0 $root.f4 \
		-side top -expand yes -fill x -anchor w

	pack $root.f0.l0 $root.f1.l0 $root.f2.l0 $root.f3.l0 \
		-side left
	pack $root.f0.e0 $root.f1.e0 $root.f2.e0 $root.f3.e0 \
		-side right
	pack $root.f4.b0 $root.f4.b1 $root.f4.b2 $root.f4.b3 \
		-side left -expand yes -fill x

	focus $root.f0.e0
	bind $root.f0.e0 <Key-Return> "focus $root.f1.e0"
	bind $root.f1.e0 <Key-Return> "focus $root.f2.e0"
	bind $root.f2.e0 <Key-Return> "focus $root.f3.e0"
	bind $root.f3.e0 <Key-Return> "$root.f4.b0 invoke"
}

#preview an animation script
proc sketch_preview { filename } {
	upvar #0 mged_sketch_prev_fps fps
	upvar #0 mged_sketch_prevp arg0
	global mged_sketch_prevs mged_sketch_preve mged_sketch_prev_size \
		mged_sketch_prev_center mged_sketch_prev_quat

	#save list of curves currently displayed
	set clist ""
	set vlist [db_glob "dummy_cmd _VDRW*"]
	foreach name [lrange $vlist 1 end] {
		lappend clist [string range $name 5 end]
	}

	set mged_sketch_prev_size [view size]
	set mged_sketch_prev_center [view center]
	set mged_sketch_prev_quat [view quat]

	if {($mged_sketch_prevs == "first")||($mged_sketch_prevs == "")} {
		set arg1 ""
	} else {
		set arg1 [format "-D%s" $mged_sketch_prevs]
	}
	if {($mged_sketch_preve == "last")||($mged_sketch_preve == "")} {
		set arg2 ""
	} else {
		set arg2 [format "-K%s" $mged_sketch_preve]
	}
	if {($fps <= 0) || ($fps == "") } {
		set arg3 ""
	} else {
		set arg3 [format "-d%s" [expr 1.0 / $fps]]
	}


	eval [concat preview $arg0 $arg1 $arg2 $arg3 $filename]
	# restore current curves to display
	if [vdraw open] {
		set oldname [vdraw read n]
	} else {
		set oldname ""
	}
	foreach curve $clist {
		vdraw open $curve
		vdraw send
	}
	if {$oldname != "" } {
		sketch_open_curve $oldname
	}
}

proc sketch_prev_restore {} {
	global mged_sketch_prev_size mged_sketch_prev_center \
		mged_sketch_prev_quat

	eval viewset size $mged_sketch_prev_size \
		center $mged_sketch_prev_center quat $mged_sketch_prev_quat
	eval e [who]
}

#-----------------------------------------------------------------
# Quit
#-----------------------------------------------------------------
proc sketch_quit { p } {
	destroy $p
	foreach var [info globals mged_sketch_init*] {
		uplevel #0 "unset $var"
	}
	kill -f _VDRW_sketch_hl_

	#reset button 2 bindings
	upvar #0 mged_sketch_bindclasses wlist
	global mged_sketch_bindB mged_sketch_bindBR mged_sketch_bindBM
	foreach wclass $wlist {
		bind $wclass <Button-2> $mged_sketch_bindB($wclass)
		bind $wclass <ButtonRelease-2> $mged_sketch_bindBR($wclass)
		bind $wclass <B2-Motion> $mged_sketch_bindBM($wclass)
	}
	# anything else?
}

#-----------------------------------------------------------------
# Other Procedures
#-----------------------------------------------------------------
# write "length" curve nodes to given file descriptor
proc sketch_write_to_fd { fd length} {
	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	if { ![info exists tlist] } { return }
	for { set i 0} { $i < $length} {incr i} {
		puts $fd [concat \
			[lindex $tlist $i] \
			[lrange [vdraw read $i] 1 3] ]
	}
}

# read curve nodes from file, return number appended
proc sketch_read_from_fd { fd } {
	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	set line {}
	set tlist {}
	if { [gets $fd line] >= 0 } {
		lappend tlist [lindex $line 0]
		vdraw write n 0 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		set num_added 1
	} else {
		tk_dialog ._sketch_msg {Empty file} \
			{The file you loaded was empty.} {} \
			 0 {OK}
		return 0
	}
	while { [gets $fd line] >= 0 } {
		lappend tlist [lindex $line 0]
		vdraw write n 1 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		incr num_added
	}
	return $num_added
}

#read current curve into end of table editor
proc sketch_text_echoc { w } {
	if { ! [vdraw open] } { return }
	set length [vdraw read l]
	upvar #0 "mged_sketch_time_[vdraw read n]" tlist
	if { ![info exists tlist] } { return }
	for {set i 0} {$i < $length} {incr i} {
		set temp [vdraw read $i]
		$w insert end [format "\t%s\t%s\t%s\t%s\n" \
			[lindex $tlist $i] [lindex $temp 1] \
			[lindex $temp 2] [lindex $temp 3] ]

	}
}

#copy one text widget to another, replacing if specified
proc sketch_text_copy { win wout args } {
	if { [lindex $args 0] == "replace" } {
		$wout delete 1.0 end
	}
	#otherwise just append to existing
	$wout insert end [$win get 1.0 "end -1 c"]
}

#apply first four columns from table editor to current curve
proc sketch_text_apply { w mode} {
	upvar #0 [format "mged_sketch_time_%s" [vdraw read n]] tlist
	if {$mode == "replace"} {
		vdraw delete a
		set tlist {}
	}
	#otherwise append to existing
	if { [set line [$w get 1.0 "1.0 lineend"]] != ""} {
		lappend tlist [lindex $line 0]
		vdraw write n 0 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		set num_added 2
	} else {
		tk_dialog ._sketch_msg {Empty text} \
			{The text you loaded was empty.} {} \
			 0 {OK}
		return 0
	}
	while { [set line [$w get $num_added.0 "$num_added.0 lineend"]] != ""} {
		lappend tlist [lindex $line 0]
		vdraw write n 1 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		incr num_added
	}
	sketch_update
	return [expr $num_added - 1]
}

#save the specified columns of the text to a file
proc sketch_text_writef { w filename col } {
	if {[file exists $filename] } {
		set ans [tk_dialog ._sketch_msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set fd [open $filename w]
	sketch_text_to_fd $w $fd $col
	close $fd
	catch {	destroy ._sketch_input}
}

proc sketch_text_readf { w filename col mode } {
	set fd [open $filename r]
	sketch_text_from_fd $w $fd $col $mode
	close $fd
	sketch_table_bar_reset $w
	catch {destroy ._sketch_input}
}

#write the specified columns of the text to fd
proc sketch_text_to_fd { w fd col} {

	if {$col == "all"} {
		set i 1
		while { [set line [$w get $i.0 "$i.0 lineend"]] != ""} {
			puts $fd $line
			incr i
		}
		return [expr $i - 1]
	} else {
		sketch_parse_col $col [sketch_text_cols $w] \
			colarray
		set numcols [array size colarray]
		set i 1
		while { [set line [$w get $i.0 "$i.0 lineend"]] != ""} {
			set outline ""
			for {set j 0} { $j < $numcols} {incr j} {
				lappend outline [lindex $line $colarray($j)]
			}
			puts $fd [join $outline \t]
			incr i
		}
		return [expr $i - 1]
	}
}

proc sketch_pos_int {str} {
	return [regexp {^[0-9]+$} $str]
}

#str is a list of columns, something like "0-2,5,7,9-"
#num is the number of columns that exist, eg "11"
#output is an array holding the columns, eg 0,1,2,5,7,9,10
#returns the number of columns requested, or -1 on error
proc sketch_parse_col {str num output} {
	upvar $output out
	set temp [split $str ,]
	set len [llength $temp]
	set k 0
	for {set i 0} {$i < $len} {incr i} {
		set temp1 [lindex $temp $i]
		set temp1 [split $temp1 -]
		set len1 [llength $temp1]
		set start [lindex $temp1 0]
		if {$len1 > 1} {
			set end [lindex $temp1 1]
		} else {
			set end $start
		}
		if {$start == "" } {set start 0}
		if {$end == "" } {set end [expr $num - 1]}
		if {!(([sketch_pos_int $start])&&([sketch_pos_int $end]))} {
			return -1
		}
		set sign 1
		if {$start > $end} {
			set test {$j >= $end}
			set change {incr j -1}
		} else {
			set test {$j <= $end}
			set change {incr j 1}
		}
		for {set j $start} $test $change {
			if {($j>=0)&&($j<$num)} {
				set out($k) $j
				incr k
			}
		}
	}
	return $k
}




proc sketch_print {} {
	set length [vdraw read l]
	puts "Name is [vdraw read n]"
	for {set i 0} { $i < $length} { incr i} {
		puts [vdraw read $i]
	}

}

proc sketch_popup_input {title entries buttons} {
	catch {destroy ._sketch_input}
	toplevel ._sketch_input
	place_near_mouse ._sketch_input
	if {$title != ""} { wm title ._sketch_input $title }
	set max 0
	foreach pair $entries {
		set len [string length [lindex $pair 0]]
		if { $len > $max} {
			set max $len
		}
	}
	set i 0
	foreach pair $entries {
		frame ._sketch_input.f$i
		pack ._sketch_input.f$i -side top -expand yes -anchor w -fill x
		set mylabel [lindex $pair 0]
		set k [string length $mylabel]
		label ._sketch_input.f$i.l -text $mylabel -width $max -anchor e
		entry ._sketch_input.f$i.e -width 20
		if { $i > 0 } {
			bind  ._sketch_input.f[expr $i-1].e <Key-Return> \
			  "focus ._sketch_input.f$i.e; \
			   ._sketch_input.f$i.e selection range 0 end"
		}
		._sketch_input.f$i.e insert end [lindex $pair 1]
		pack ._sketch_input.f$i.l ._sketch_input.f$i.e \
			-side left -anchor w
		incr i
	}
	set max 0
	foreach pair $buttons {
		set len [string length [lindex $pair 0]]
		if { $len > $max} {
			set max $len
		}
	}
	set j 0
	foreach pair $buttons {
		if { $j >= $i } {
			frame ._sketch_input.f$j
			pack ._sketch_input.f$j -side top -anchor e
		}
		button ._sketch_input.f$j.b -text [lindex $pair 0] \
			-command [lindex $pair 1] -width $max
		pack ._sketch_input.f$j.b -side right
		incr j
	}
	if { $j > $i } {
		set max $j; set min $i } else {
		set max $i; set min $j }
	if { $max < 1 } {
		destroy ._sketch_input
		return -1
	}
	if { $min < 1} {
		return 0
	}
	._sketch_input.f0.e selection range 0 end
	focus ._sketch_input.f0.e
	bind ._sketch_input.f[expr $i-1].e <Key-Return> \
			{._sketch_input.f0.b invoke}
	return 0
}

#transfer columns from one text widget to another
#columns is any string accepted by sketch_parse_col
#mode is one of replace, append, left, right
#rows optionally specifies maximum number of rows
proc sketch_text_from_text { wout win col mode {rows all}} {
	set srclines [sketch_text_rows $win]
	if { [regexp {^[0-9]+$} $rows] == "0" } {
		set numlines $srclines
	} else {
		set numlines $rows
		if { $numlines > $srclines } {
			set numlines $srclines
		}
	}

	if { $mode == "replace" } {
		$wout mark set prev_end "end - 2 c"
		if { [$wout index prev_end] == "1.0"} {set mode append}
	}
	if {$col == "all"} {
		if {($mode == "right") || ($mode == "left")} {
			if  {$mode == "right"} {
				set place lineend
			} else {
				set place ""
			}
			for {set i 1} { $i <= $numlines} {incr i} {
				if {[$wout get "$i.0 lineend"] == ""} {
					$wout insert "$i.0 lineend" "\n"
				}
				set line [join [$win get $i.0 "$i.0 lineend"]\
					"\t" ]
				$wout insert "$i.0 $place" "\t$line"
			}
			return
		}
		#else
		if {[lindex [split [$wout index "end - 1 c"] .] 1] != 0} {
			$wout insert "end - 1 c" "\n"
		}
		for {set i 1} { $i <= $numlines} {incr i} {
			set line [join [$win get $i.0 "$i.0 lineend"] "\t" ]
			$wout insert end "\t$line\n"
		}
		if { $mode == "replace"} {
			$wout delete 1.0 "prev_end + 1 c"
		}
		return
	}
	#else
	#note: depends on first row for number of columns
	sketch_parse_col $col [sketch_text_cols $win] colarray
	set numcols [array size colarray]

	if {($mode == "right") || ($mode == "left")} {
		if  {$mode == "right"} {
			set place lineend
		} else {
			set place ""
		}
		for {set i 1} { $i <= $numlines} {incr i} {
			if {[$wout get "$i.0 lineend"] == ""} {
				$wout insert "$i.0 lineend" "\n"
			}
			set line [$win get $i.0 "$i.0 lineend"]
			set outline ""
			for {set j 0} { $j < $numcols} {incr j} {
				lappend outline [lindex $line $colarray($j)]
			}
			set outline [join $outline "\t"]
			$wout insert "$i.0 $place" "\t$outline"
		}
		return
	}
	if {[lindex [split [$wout index "end - 1 c"] .] 1] != 0} {
		$wout insert "end - 1 c" "\n"
	}
	for {set i 1} { $i <= $numlines} {incr i} {
		set line [$win get $i.0 "$i.0 lineend"]
		set outline ""
		for {set j 0} { $j < $numcols} {incr j} {
			lappend outline [lindex $line $colarray($j)]
		}
		set outline [join $outline "\t"]
		$wout insert end "\t$outline\n"
	}
	if { $mode == "replace"} {
		$wout delete 1.0 "prev_end + 1 c"
	}
	return
}

proc sketch_text_from_fd { w fd col mode } {
	if { ($col != "all") || ($mode == "left") || ($mode == "right")} {
		set mymode two
		set myw $w._ffd_scratch
		text $myw
	} else {
		#simple case, don't need intermediate buffer
		set mymode one
		set myw $w
		if {$mode == "replace"} {
			$w delete 1.0 end
		}
	}

	if {[lindex [split [$myw index "end - 1 c"] .] 1] != 0} {
		$myw insert "end - 1 c" "\n"
	}
	while { [gets $fd line] >= 0} {
		set line [join $line "\t"]
		$myw insert end "\t$line\n"
	}
	if { $mymode == "one" } {
		return 1
	}

	sketch_text_from_text $w $myw $col $mode
	destroy $myw
	return 2
}

#get number of rows in text widget, excluding vestigial rows on the end
proc sketch_text_rows {w} {
	set i [lindex [split [$w index end] .] 0]
	while { $i > 0 } {
		if { [$w index "$i.0 lineend"] != "$i.0" } break
		incr i -1
	}
	return $i
}

proc sketch_line_cols {line} {
	regsub -all \[\t\ \n\]+ $line \t res
	return [llength [split [string trim $res " \t\n"] "\t"]]
}

proc sketch_text_cols {w} {
	return [sketch_line_cols [$w get 1.0 "1.0 lineend"]]
}

proc sketch_rgb_clip { rgb } {
	while { [llength $rgb] < 3 } { lappend rgb "0" }
	for {set i 0} { $i < 3} { incr i} {
		set color [expr int([lindex $rgb $i])]
		if { $color < 0 } {
			set color 0
		} elseif { $color > 255} {
			set color 255
		}
		lappend result $color
	}
	return $result
}

proc sketch_rgb_to_hex { rgb {type no_pound}} {
	set rgb [sketch_rgb_clip $rgb]
	if {$type == "no_pound"} {
		return [format "%.2x%.2x%.2x" [lindex $rgb 0] \
				[lindex $rgb 1] [lindex $rgb 2]]
	} else {
		return [format "#%.2x%.2x%.2x" [lindex $rgb 0] \
				[lindex $rgb 1] [lindex $rgb 2]]
	}
}

proc sketch_rgb_inv { rgb } {
	set rgb [sketch_rgb_clip $rgb]
	foreach color $rgb {
		lappend result [expr 255 - $color]
	}
	return $result
}

proc sketch_hex_to_rgb { hex } {
	set hex [string trimleft $hex #]
	if { [string index $hex 1] != "x" } {
		set hex "0x$hex"
	}
	set blue [expr $hex%0x100]
	set green [expr ($hex-$blue)%0x10000]
	set red [expr ($hex-$green-$blue)%0x1000000]
	return [ list [expr $red/0x10000] [expr $green/0x100] [expr $blue]]
}

#Given table editor id, return corresponding text widget.
#If needcol is specified, check that widget has needcol columns.
proc sketch_text_from_table {tid {needcol -1}} {
	set text ""
	foreach ted [sketch_table_list] {
		if { [sketch_table_get_label $ted] == $tid } {
			set text $ted.t
			break
		}
	}
	if { $text  == ""} {
		tk_dialog ._sketch_msg {Couldn't find editor} \
		  "Couldn't find table editor $tid. \
		   (Text editor identifier must be an integer)." \
		  {} 0 {OK}
		return
	}
	#check number of columns
	set numcol [sketch_text_cols $text]
	if { ($needcol != -1) && ( $numcol != $needcol) } {
		tk_dialog ._sketch_msg {Wrong number of columns} \
		 "Table editor $tid has $numcol \
		  columns, but $needcol are required." {} 0 {OK}
		return
	}
	return $text
}



#-------------------------------------------------------------------
# Go
#-------------------------------------------------------------------
# Uncomment the following command in order to run the animator
# automatically when anim.tcl is sourced.

proc animmate { id {p .} } {
    global mged_gui
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    sketch_popup_main $p
}

#animmate

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
