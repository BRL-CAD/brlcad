# ANIM.TCL
# Tcl/Tk Gui Interface for Creating and Displaying Animation Scripts within
# MGED.
# Author: Carl Nuzman
#Sections:
#	Create main window
#	Curve Editor
#	View Editor
#	Table Editor
#	Create Script
#	Create Track Script
#	Combine Scripts
#	Show Scripts
#	Quit Animator
#	General Procedures

#-----------------------------------------------------------------
# Create main window
#-----------------------------------------------------------------
proc sketch_main_window {} {
	sketch_init_globals
	catch {destroy .sketch}

	toplevel .sketch
	wm title .sketch "MGED Animator"
	button .sketch.b0 -text "CURVE EDITOR" -command "sketch_popup_draw"
	button .sketch.b1 -text "VIEW EDITOR" -command "sketch_popup_view"
	button .sketch.b2 -text "TABLE EDITOR" -command "sketch_popup_text"
	menubutton .sketch.b3 -text "CREATE SCRIPT" -menu .sketch.b3.m0
	menu .sketch.b3.m0 -tearoff 0
	.sketch.b3.m0 add command -label "Object" -command "sketch_popup_objanim object"
	.sketch.b3.m0 add command -label "View" -command "sketch_popup_objanim view"
	.sketch.b3.m0 add command -label "Articulated Track" -command "sketch_popup_track_anim"
	button .sketch.b4 -text "COMBINE SCRIPTS" -command "sketch_popup_sort"
	button .sketch.b5 -text "SHOW SCRIPT" -command "sketch_popup_preview"
	button .sketch.b6 -text "QUIT ANIMATOR" -command "sketch_quit"

	pack .sketch.b0 .sketch.b1 .sketch.b2 .sketch.b3 .sketch.b4 \
		.sketch.b5 .sketch.b6 \
		-side left -fill x -expand yes
}

proc sketch_init_globals {} {
	# global variable initialisations
	uplevel #0 set mged_sketch_anim_dir "/m/cad/anim/"
	uplevel #0 set mged_sketch_tab_dir "/m/cad/tab/"
	uplevel #0 set mged_sketch_temp1 "./_mged_sketch_temp1_"
	uplevel #0 set mged_sketch_temp2 "./_mged_sketch_temp2_"
	uplevel #0 set mged_sketch_sort_temp "./_mged_sketch_sort_"
	#curve
	uplevel #0 set mged_sketch_node 0		
	uplevel #0 set mged_sketch_count 0		
	uplevel #0 set mged_sketch_time 0.0	
	uplevel #0 set mged_sketch_tinc 1.0	
	uplevel #0 {set mged_sketch_name ""}
	uplevel #0 {set mged_sketch_splname ""}
	uplevel #0 {set mged_sketch_color "255 255 0"}
	uplevel #0 set mged_sketch_fps "10"
	uplevel #0 set mged_sketch_defname "vdraw"
	#view curve
	uplevel #0 set mged_sketch_vnode 0		
	uplevel #0 set mged_sketch_vcount 0		
	uplevel #0 set mged_sketch_vtime 0.0	
	uplevel #0 set mged_sketch_vtinc 1.0	
	uplevel #0 {set mged_sketch_vname ""}
	uplevel #0 {set mged_sketch_vparams {size eye quat}}
	uplevel #0 set mged_sketch_cmdlen(quat) 4
	uplevel #0 set mged_sketch_cmdlen(eye) 3
	uplevel #0 set mged_sketch_cmdlen(center) 3
	uplevel #0 set mged_sketch_cmdlen(ypr) 3
	uplevel #0 set mged_sketch_cmdlen(aet) 3
	uplevel #0 set mged_sketch_cmdlen(size) 1
	# object animation
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
	# preview script
	uplevel #0 {set mged_sketch_prevs ""}
	uplevel #0 {set mged_sketch_preve ""}
	uplevel #0 {set mged_sketch_prevp ""}
	#table editor
	uplevel #0 set mged_sketch_text_lmode "replace"
	uplevel #0 set mged_sketch_text_index 0
	#track animation
	uplevel #0 {set mged_sketch_whlsource ""}
	uplevel #0 {set mged_sketch_lnkname ""}
	uplevel #0 set mged_sketch_numlinks 1
	uplevel #0 set mged_sketch_radii "1"
}

#-----------------------------------------------------------------
# Curve Editor
#-----------------------------------------------------------------
proc sketch_popup_draw {} {
	global mged_sketch_fps mged_sketch_color mged_sketch_time \
		 mged_sketch_name mged_sketch_count mged_sketch_node \
		mged_sketch_splname mged_sketch_defname

	if { [info commands .sketch.draw] != ""} {
		raise .sketch.draw
		return
	}
	toplevel .sketch.draw
	wm title .sketch.draw "MGED curve editor"
	button .sketch.draw.b0 -text "APPEND" -command "sketch_append"
	button .sketch.draw.b1 -text "INSERT" -command {sketch_insert $mged_sketch_node}
	button .sketch.draw.b2 -text "MOVE" -command {sketch_move $mged_sketch_node}
	button .sketch.draw.b3 -text "DELETE" -command {sketch_delete $mged_sketch_node}
	frame  .sketch.draw.f1
	label  .sketch.draw.f1.l0 -text "Node "
	label  .sketch.draw.f1.l1 -textvariable mged_sketch_node
	label  .sketch.draw.f1.l2 -text " of "
	label  .sketch.draw.f1.l3 -textvariable mged_sketch_count
	frame  .sketch.draw.f0 
	button .sketch.draw.f0.b4 -text "-->" -command {sketch_incr 10}
	button .sketch.draw.f0.b40 -text "->" -command {sketch_incr 1}
	button .sketch.draw.f0.b50 -text "<-" -command {sketch_incr -1}
	button .sketch.draw.f0.b5 -text "<--" -command {sketch_incr -10}
	frame  .sketch.draw.f4
	#label  .sketch.draw.f4.l0 -text "Current curve:"
	menubutton .sketch.draw.f4.mb0 -text "Current curve:" -menu .sketch.draw.f4.mb0.m
	menu .sketch.draw.f4.mb0.m -tearoff 0
	.sketch.draw.f4.mb0.m add command -label "Select Curve" -command {sketch_popup_name select}
	.sketch.draw.f4.mb0.m add command -label "Rename Curve" -command {sketch_popup_name rename}
	.sketch.draw.f4.mb0.m add command -label "Copy Curve" -command {sketch_popup_name copy}

	label  .sketch.draw.f4.l1 -textvariable mged_sketch_name

	frame  .sketch.draw.f5
	label  .sketch.draw.f5.l0 -text "Time:"
	entry  .sketch.draw.f5.e0 -width 8 -textvariable mged_sketch_time
	bind   .sketch.draw.f5.e0 <Key-Return> { sketch_time_set [.sketch.draw.f5.e0 get]}

	frame  .sketch.draw.f2
	#label .sketch.draw.f2.l0 -text "Color:"
	menubutton .sketch.draw.f2.mb0 -text "Color:" -menu .sketch.draw.f2.mb0.m
	menu .sketch.draw.f2.mb0.m -tearoff 0
	.sketch.draw.f2.mb0.m add cascade -label "Current curve" \
		-menu .sketch.draw.f2.mb0.m.m0
	.sketch.draw.f2.mb0.m add cascade -label "Current spline" \
		-menu .sketch.draw.f2.mb0.m.m1
	.sketch.draw.f2.mb0.m add cascade -label "Other" \
		-menu .sketch.draw.f2.mb0.m.m2
	menu .sketch.draw.f2.mb0.m.m0 -tearoff 0
	menu .sketch.draw.f2.mb0.m.m1 -tearoff 0
	menu .sketch.draw.f2.mb0.m.m2 -tearoff 0
	sketch_add_color_menu .sketch.draw.f2.mb0.m.m0 current
	sketch_add_color_menu .sketch.draw.f2.mb0.m.m1 spline
	sketch_add_color_menu .sketch.draw.f2.mb0.m.m2 other
	entry .sketch.draw.f2.e0 -width 12 -textvariable mged_sketch_color
	bind  .sketch.draw.f2.e0 <Key-Return> { sketch_color [.sketch.draw.f2.e0 get]}
	frame .sketch.draw.f6 -relief groove -bd 3
	button .sketch.draw.f6.b0 -text "Spline Interpolate" -command {sketch_do_spline spline}
	button .sketch.draw.f6.b1 -text "Cspline Interpolate" -command {sketch_do_spline cspline}
	frame .sketch.draw.f6.f0
	label .sketch.draw.f6.f0.l0 -text "into curve:"
	entry .sketch.draw.f6.f0.e0 -width 15 -textvariable mged_sketch_splname
	frame  .sketch.draw.f6.f1
	label .sketch.draw.f6.f1.l0 -text "Frames per second:"
	entry .sketch.draw.f6.f1.e0 -width 4 -textvariable mged_sketch_fps
	bind  .sketch.draw.f6.f1.e0 <Key-Return> { focus .sketch.draw }
	frame .sketch.draw.f8
	button .sketch.draw.f8.b0 -text "Main Window" -command "raise .sketch"
	button .sketch.draw.f8.b1 -text "Cancel" -command "destroy .sketch.draw"

	menubutton .sketch.draw.mb0 -text "Read/Write" -menu .sketch.draw.mb0.m0
	menu .sketch.draw.mb0.m0
	.sketch.draw.mb0.m0 add command -label "Read Curve From File" -command {sketch_popup_load}
	.sketch.draw.mb0.m0 add command -label "Write Curve To File" -command {sketch_popup_save curve}
	.sketch.draw.mb0.m0 add command -label "Write Spline To File" -command {sketch_popup_save spline}
	.sketch.draw.mb0.m0 add command -label "Delete Curve" -command {sketch_popup_delete_curve}
	
	pack \
		.sketch.draw.f4 .sketch.draw.f5 .sketch.draw.f1 .sketch.draw.f0 \
		.sketch.draw.b0 .sketch.draw.b1 .sketch.draw.b2 .sketch.draw.b3 \
		.sketch.draw.f2 \
		.sketch.draw.f6 \
		.sketch.draw.mb0 \
		.sketch.draw.f8	\
		-side top -fill x -expand yes
	pack \
		.sketch.draw.f6.b0 .sketch.draw.f6.b1 \
		.sketch.draw.f6.f0 .sketch.draw.f6.f1 \
		-side top -fill x -expand yes
	pack .sketch.draw.f6.f0.l0 .sketch.draw.f6.f0.e0 \
		.sketch.draw.f6.f1.l0 .sketch.draw.f6.f1.e0 \
		.sketch.draw.f8.b0 .sketch.draw.f8.b1 \
		-side left -expand yes
	pack .sketch.draw.f0.b4 .sketch.draw.f0.b40 .sketch.draw.f0.b50 .sketch.draw.f0.b5 \
		-side right -expand yes
	pack .sketch.draw.f1.l0 .sketch.draw.f1.l1 .sketch.draw.f1.l2 .sketch.draw.f1.l3 \
		.sketch.draw.f2.mb0 .sketch.draw.f2.e0 \
		.sketch.draw.f4.mb0 .sketch.draw.f4.l1 \
		.sketch.draw.f5.l0 .sketch.draw.f5.e0 \
		-side left -expand yes
	
	#initialize name
	if { [vdraw o] } {
		sketch_open_curve [vdraw r n]
	} else {
		sketch_open_curve $mged_sketch_defname
	} 
	set mged_sketch_splname "spl_[vdraw r n]"
	sketch_update
}


proc sketch_open_curve {name} {
	global mged_sketch_tinc
	set res [vdraw o $name]
	if {$res < 0} {
		tk_dialog .sketch.msg {Couldn't open curve} \
			"Curve $name cannot be opened - it conflicts\
			 with existing geometry." {} 0 {OK}
	} else {
	       #create associated time variable if non-existent
		if { [vdraw r n] != "$name" } {
			#debugging - should never happen
			puts "Warning: wanted $name got [vdraw r n]"
		}
		set tname "mged_time_$name"
		uplevel #0 "append $tname {}"
		upvar #0 $tname time
		set len [vdraw r l]
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
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	if {$node != ""} {
	set tlist [lreplace $tlist $node $node $value]
	}
	focus .sketch
}

#update graphical representation of current curve
proc sketch_update {} {
	global mged_sketch_count mged_sketch_time mged_sketch_node
	global mged_sketch_name mged_sketch_splname mged_sketch_color

	set mged_sketch_count [vdraw r l]

	sketch_clip

	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist 

	if {$mged_sketch_node == ""} {
		set mged_sketch_time ""
	} else {
		set mged_sketch_time [lindex $tlist $mged_sketch_node]
	}

	set mged_sketch_color [sketch_hex_to_rgb [vdraw r c]]
	set mged_sketch_name  [vdraw r n]
	
	if { [vdraw s] < 0 } {
		tk_dialog .sketch.msg {Can't display curve} \
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

	set offset [expr [vget size] * 0.01]
	set oldname [vdraw r n]
	set vertex [eval [concat vdraw r $mged_sketch_node]]

	set v_x [lindex $vertex 1]
	set v_y [lindex $vertex 2]
	set v_z [lindex $vertex 3]
	vdraw s
	sketch_open_curve _sketch_hl_
	sketch_draw_highlight $v_x $v_y $v_z $offset
	sketch_open_curve $oldname
	
}

proc sketch_draw_highlight {v_x v_y v_z offset} {
	vdraw d a
	vdraw w n 0 [expr $v_x - $offset] $v_y $v_z
	vdraw w n 1 [expr $v_x + $offset] $v_y $v_z
	vdraw w n 0 $v_x [expr $v_y - $offset] $v_z
	vdraw w n 1 $v_x [expr $v_y + $offset] $v_z
	vdraw w n 0 $v_x $v_y [expr $v_z - $offset]
	vdraw w n 1 $v_x $v_y [expr $v_z + $offset]
	vdraw p c 0x00ffff
	vdraw s 
}

#increment current node by specified amount
proc sketch_incr { i } {
	global mged_sketch_node

	if { $mged_sketch_node != "" } {
		incr mged_sketch_node $i
	}
	sketch_update
}

#append current view center to current curve
proc sketch_append {} {
	global mged_sketch_node mged_sketch_count mged_sketch_tinc
	global mged_sketch_time

	set length [vdraw read length]
	if { $length == 0 } {
		set cmd 0
		upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
		set mged_sketch_time 0.0
		set tlist $mged_sketch_time
	} else {
		set cmd 1
		upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
		set mged_sketch_time [expr \
			[lindex $tlist [expr $mged_sketch_count - 1]] \
			+ $mged_sketch_tinc]
		lappend tlist $mged_sketch_time
		#puts $mged_sketch_time
	}
	set center [vget center]
	#set center [vget eye]
	set mged_sketch_node [vdraw r l]
	eval [concat vdraw w n $cmd $center]
	sketch_update
}

#insert current view center at specified node
proc sketch_insert { n } {
	global mged_sketch_tinc

	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	if { $n == "" } {
		sketch_update
		return
	}
	if { $n == 0 } {
		eval [concat vdraw i $n 0 [vget center]]
		set vertex [vdraw r 1]
		eval [concat vdraw w 1 1 [lrange $vertex 1 3]]
		set tn [expr [lindex $tlist 0] - $mged_sketch_tinc]
	} else {
		eval [concat vdraw i $n 1 [vget center]]
		set tn [expr [lindex $tlist $n]+[lindex $tlist [expr $n - 1]]]
		set tn [expr $tn * 0.5]
	}
	set tlist [linsert $tlist $n $tn]
	sketch_update
}

#move specified node to current view center
proc sketch_move { n } {
	if { $n == "" } {
		sketch_update
		return
	}
	if { $n == 0 } {
		eval [concat vdraw w $n 0 [vget center]]
	} else {
		eval [concat vdraw w $n 1 [vget center]]
	}
	sketch_update
}

#delete specified node
proc sketch_delete { n } {

	if { $n == "" } {
		sketch_update
		return
	}
	vdraw d $n
	if { ($n == 0) && ([vdraw r l] > 0) } {
		set vertex [vdraw r 0]
		eval [concat vdraw w 0 0 [lrange $vertex 1 3]]
	}
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
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
	if {$color == "other"} { set color ""}
	set name ""
	if { $type == "current" } {
		set mged_sketch_color $color
		set name [vdraw r n]
	} 
	if { $type == "spline" } {
		set name $mged_sketch_splname
	}
	#else
	set entries [list \
		[list "Name of curve:" $name] \
		[list "New color:" $color] \
		]
	set buttons [list \
		[list "OK" "set oldname \[vdraw r n\]; \
			vdraw o \[.sketch.input.f0.e get\]; \
			sketch_color \[.sketch.input.f1.e get \]; \
			vdraw o \$oldname; \
			sketch_update; \
			destroy .sketch.input"] \
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Color Curve" $entries $buttons
	return
}
		

#set current curve color
proc sketch_color { color } {
	global mged_sketch_color
	vdraw p c [sketch_rgb_to_hex $color]
	vdraw s
	set mged_sketch_color [sketch_hex_to_rgb [vdraw r c]]
	catch {focus .sketch.draw}
}


proc sketch_do_spline { mode } {
	global mged_sketch_fps mged_sketch_splname \
		mged_sketch_temp1 mged_sketch_temp2 \
		mged_sketch_tab_dir

	#write vertices to temp2, result to temp1
	set fo [open $mged_sketch_temp2 w]
	set length [vdraw r l]
	if { $length < 3 } {
		puts {Need at least three vertices}
		close $fo
		return -1
	}

	sketch_write_to_fd $fo $length
	close $fo

	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	set start [lindex $tlist 0]
	set end [lindex $tlist [expr $length - 1]]
	set fo [open "| ${mged_sketch_tab_dir}tabinterp -q > $mged_sketch_temp1" w]
	puts $fo "file $mged_sketch_temp2 0 1 2;"
	puts $fo [concat times $start $end $mged_sketch_fps {;}]
	puts $fo "interp $mode 0 1 2;"
	# catch can be removed when tabinterp -q option is installed
	catch {close $fo}
	exec rm $mged_sketch_temp2

	#read results into curve
	set fi [open $mged_sketch_temp1 r]
	set oldname [vdraw r n]
	set oldcolor [sketch_hex_to_rgb [vdraw r c]]
	vdraw s
	sketch_open_curve $mged_sketch_splname
	vdraw d a
	set num_read [sketch_append_from_fd $fi]
	close $fi
	#vdraw p c $oldcolor
	vdraw s
	sketch_open_curve $oldname
	exec rm $mged_sketch_temp1
	return $num_read
}



proc sketch_popup_load {} {
	set entries [list \
		{"File to Load"} \
		[list "Name of Curve" [vdraw r n]] \
		]
	set buttons [list \
		{"OK" {sketch_load [.sketch.input.f0.e get] \
				[.sketch.input.f1.e get]} } \
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Load Curve" $entries $buttons
}

#load from file to specified curve
proc sketch_load { filename  curve } {
	global mged_sketch_splname
	if { [sketch_open_curve $curve] < 0 } {
		echo "Couldn't open" $curve
		return	
	}
	set fd [open $filename r]
	vdraw d a
	sketch_append_from_fd $fd
	close $fd
	catch {destroy .sketch.input}
	sketch_update				
	set mged_sketch_splname "spl_[vdraw r n]"
}


proc sketch_popup_save { type  } {
	set entries [list \
		[list "Name of Curve" [vdraw r n]] \
		{"Save to File:"} \
		]
	set buttons [list \
		{"OK" {sketch_save [.sketch.input.f0.e get] \
				[.sketch.input.f1.e get]} }\
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Save Curve" $entries $buttons
	if {$type == "spline"} {
		.sketch.input.f0.e insert 0 spl_
	}
}

#save specified curve to file
proc sketch_save { curve filename } {
	if {[file exists $filename] } {
		set ans [tk_dialog .sketch.msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set oldcurve [vdraw r n]
	set fd [open $filename w]
	sketch_open_curve $curve
	sketch_write_to_fd $fd [vdraw r l]
	close $fd
	sketch_open_curve $oldcurve
	catch {destroy .sketch.input}
}



proc sketch_popup_name {{mode select}} {
	if { $mode == "select"} {
		sketch_popup_input "Curve Select" {
			{"Curve to select:" ""}
		} {
			{"OK" {sketch_name [.sketch.input.f0.e get]}}
			{"Cancel" "destroy .sketch.input"}
		}
	} elseif { $mode == "rename" } {
		sketch_popup_input "Curve Rename" {
			{"New name for curve:" ""}
		} [list \
			[list "OK" "sketch_rename \[.sketch.input.f0.e get\]" ] \
			{"Cancel" "destroy .sketch.input"} \
		]
	} elseif { $mode == "copy" } {
		sketch_popup_input "Curve Copy" {
			{"Name for copy:" ""}
		} [list \
			[list "OK" "sketch_copy \[.sketch.input.f0.e get\]" ] \
			{"Cancel" "destroy .sketch.input"} \
		]
	}
}

proc sketch_name { name } {
	global mged_sketch_splname
	if {[sketch_open_curve $name] < 0} {
		return
	}
	catch {destroy .sketch.input}
	sketch_update
	set mged_sketch_splname "spl_[vdraw r n]"
}

proc sketch_rename { name } {
	global mged_sketch_splname
	set oldname [vdraw r n]
	set res [vdraw p n $name]
	if { $res < 0 } {
		set ans [tk_dialog .sketch.msg {Curve exists} \
		  "A curve with name $name already exists." {} \
		  1 {Rename anyway} {Cancel} ]
		if { $ans == 1 } { 
			return -1
		} else {
			vdraw c d $name
			vdraw p n $name
		}
	}
	#for some reason, this update is needed to prevent dialog from
	#crashing
	update
	upvar #0 "mged_time_$oldname" oldtime
	uplevel #0 "append mged_time_$name {}"
	upvar #0 "mged_time_$name" newtime
	set newtime $oldtime
	#sketch_update will fail if name conflicts
	if { [sketch_update] == 0 } {
		catch {destroy .sketch.input}
		kill -f "_VDRW$oldname"
		unset oldtime
		set mged_sketch_name [vdraw r n]
		if { "$mged_sketch_name" != "$name" } {
			puts "sketch_rename error. This should never happen."
		}
		set mged_sketch_splname "spl_$mged_sketch_name"
	} else {
		#put things back
		unset newtime
		vdraw p n $oldname
		sketch_update
	}
}

proc sketch_copy { name } {
	set basename [vdraw r n]
	set res [vdraw o $name]
	if { $res == 0 } {
		set ans [tk_dialog .sketch.msg {Curve exists} \
		  "A curve with name $name already exists." {} \
		  1 {Copy anyway} {Cancel} ]
		if { $ans == 1 } {
			return -1
		} else {
			vdraw d a
		}
	}
	vdraw o $basename
	text .sketch._scratch_
	sketch_text_echoc .sketch._scratch_
	sketch_open_curve $name
	sketch_text_apply .sketch._scratch_ replace
	destroy .sketch._scratch_
	if {[sketch_update] == 0} {
		catch {destroy .sketch.input}
	} else {
		sketch_open_curve $basename
		vdraw c d $name
		sketch_update
	}
}

proc sketch_popup_delete_curve {} {
	set entries [list \
		[list "Delete Curve:" [vdraw r n]] \
		]
	set buttons [list \
		{ "OK" {sketch_delete_curve [.sketch.input.f0.e get]; \
				destroy .sketch.input} } \
		{ "Cancel" "destroy .sketch.input" } \
		]
	sketch_popup_input "Delete Curve" $entries $buttons
}

proc sketch_delete_curve { name } {
	global mged_sketch_defname

	puts "deleting $name"
	vdraw c d $name
	catch {vdraw c d _sketch_hl_}
	if { [vdraw o] } {
		sketch_open_curve [vdraw r n]
	} else {
		sketch_open_curve $mged_sketch_defname
	}
	uplevel #0 "set mged_time_$name {}"
	kill -f "_VDRW$name"
	sketch_update
}

#-----------------------------------------------------------------
# View Editor
#-----------------------------------------------------------------
proc sketch_popup_view {} {
	global mged_sketch_vtime \
		 mged_sketch_vname mged_sketch_vcount mged_sketch_vnode \
		mged_sketch_vparams

	if { [info commands .sketch.view] != ""} {
		wm deiconify .sketch.view
		raise .sketch.view
		return
	}
	toplevel .sketch.view
	wm title .sketch.view "MGED view curve editor"
	button .sketch.view.b0 -text "APPEND" -command "sketch_vappend"
	button .sketch.view.b1 -text "INSERT" -command {sketch_vinsert $mged_sketch_vnode}
	button .sketch.view.b2 -text "MOVE" -command {sketch_vmove $mged_sketch_vnode}
	button .sketch.view.b3 -text "DELETE" -command {sketch_vdelete $mged_sketch_vnode}
	frame  .sketch.view.f1
	label  .sketch.view.f1.l0 -text "Node "
	label  .sketch.view.f1.l1 -textvariable mged_sketch_vnode
	label  .sketch.view.f1.l2 -text " of "
	label  .sketch.view.f1.l3 -textvariable mged_sketch_vcount
	frame  .sketch.view.f0 
	button .sketch.view.f0.b4 -text "-->" -command {sketch_vincr 10}
	button .sketch.view.f0.b40 -text "->" -command {sketch_vincr 1}
	button .sketch.view.f0.b50 -text "<-" -command {sketch_vincr -1}
	button .sketch.view.f0.b5 -text "<--" -command {sketch_vincr -10}
	frame  .sketch.view.f4
	#label  .sketch.view.f4.l0 -text "Current v-curve:"
	menubutton .sketch.view.f4.mb0 -text "Current v-curve:" -menu .sketch.view.f4.mb0.m
	menu .sketch.view.f4.mb0.m -tearoff 0
	.sketch.view.f4.mb0.m add command -label "Select V-curve" -command {sketch_popup_vname select}
	.sketch.view.f4.mb0.m add command -label "Rename V-curve" -command {sketch_popup_vname rename}
	.sketch.view.f4.mb0.m add command -label "Copy V-Curve" -command {sketch_popup_vname copy}

	button  .sketch.view.f4.l1 -textvariable mged_sketch_vname \
		-command {wm deiconify .sketch.view._v_$mged_sketch_vname; \
				raise .sketch.view._v_$mged_sketch_vname}
	frame .sketch.view.f3
	menubutton .sketch.view.f3.mb0 -text "Parameters:" -menu .sketch.view.f3.mb0.m
	menu .sketch.view.f3.mb0.m -tearoff 0
	.sketch.view.f3.mb0.m add command -label "{size eye quat}" \
		-command "sketch_set_vparams {size eye quat}"
	.sketch.view.f3.mb0.m add command -label "{size eye ypr}" \
		-command "sketch_set_vparams {size eye ypr}"
	.sketch.view.f3.mb0.m add command -label "{eye center}" \
		-command "sketch_set_vparams {eye center}"
	label .sketch.view.f3.l0 -textvariable mged_sketch_vparams

	frame  .sketch.view.f5
	label  .sketch.view.f5.l0 -text "Time:"
	entry  .sketch.view.f5.e0 -width 8 -textvariable mged_sketch_vtime
	bind   .sketch.view.f5.e0 <Key-Return> { sketch_vtime_set [.sketch.view.f5.e0 get]}

	frame .sketch.view.f8
	button .sketch.view.f8.b0 -text "Main Window" -command "raise .sketch"
	button .sketch.view.f8.b1 -text "Cancel" -command "sketch_view_cancel"

	menubutton .sketch.view.mb0 -text "Read/Write" -menu .sketch.view.mb0.m0
	menu .sketch.view.mb0.m0
	.sketch.view.mb0.m0 add command -label "Read V-curve From File" -command {sketch_popup_vload}
	.sketch.view.mb0.m0 add command -label "Write V-curve To File" -command {sketch_popup_vsave curve}
	.sketch.view.mb0.m0 add command -label "Delete V-curve" -command {sketch_popup_delete_vcurve}
	
	pack \
		.sketch.view.f4 .sketch.view.f3 .sketch.view.f5 .sketch.view.f1 .sketch.view.f0 \
		.sketch.view.b0 .sketch.view.b1 .sketch.view.b2 .sketch.view.b3 \
		.sketch.view.mb0 \
		.sketch.view.f8	\
		-side top -fill x -expand yes
	pack \
		.sketch.view.f8.b0 .sketch.view.f8.b1 \
		-side left -expand yes
	pack .sketch.view.f0.b4 .sketch.view.f0.b40 .sketch.view.f0.b50 .sketch.view.f0.b5 \
		-side right -expand yes
	pack .sketch.view.f1.l0 .sketch.view.f1.l1 .sketch.view.f1.l2 .sketch.view.f1.l3 \
		.sketch.view.f4.mb0 .sketch.view.f4.l1 \
		.sketch.view.f3.mb0 .sketch.view.f3.l0 \
		.sketch.view.f5.l0 .sketch.view.f5.e0 \
		-side left -expand yes
	
	#initialize name
	sketch_open_vcurve $mged_sketch_vname
	sketch_vupdate
}

proc sketch_open_vcurve {name} {
	global mged_sketch_vname mged_sketch_vparams

	#get non-empty name
	if { $name == "" } {
		#pick from existing
		set any [lindex [info commands .sketch.view._v_*.t] 0]
		if {[regexp {(\.sketch\.view\._v_)(.+)(\.t$)} $any all a name b] == 0} {
			# default
			set name "view"
		}
	}

	#create if doesn't exist
	if { [info commands .sketch.view._v_$name.t] == "" } {
		sketch_popup_text_create .sketch.view._v_$name $name ro
	}
	set mged_sketch_vname $name	
	#create parameter list if need be
	set vpname "mged_sketch_vparams_$name"
	uplevel #0 "append $vpname {}"
	upvar #0 $vpname vpn
	sketch_set_vparams $vpn

	wm deiconify .sketch.view._v_$name
	raise .sketch.view._v_$name


}

#set the viewparameters for the current view curve and convert if necessary
proc sketch_set_vparams { newlist } {
	global mged_sketch_vname mged_sketch_vparams \
		mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_dir

	#make it one of the allowable values
	switch $newlist {
		{size eye quat} -
		{size eye ypr} -
		{eye center} {}
		default {
			set newlist {size eye quat}
		}
	}
	set mged_sketch_vparams $newlist
	uplevel #0 "append mged_sketch_vparams_$mged_sketch_vname {}"
	upvar #0 mged_sketch_vparams_$mged_sketch_vname oldlist
	if { $oldlist == $newlist } return
	set text .sketch.view._v_$mged_sketch_vname.t
	if { ([info commands $text] == "") || ([sketch_text_rows $text] < 1)} {
		set oldlist $newlist
		return
	}
	#otherwise, we must convert the text
	#convert to {size eye ypr}
	$text configure -state normal
	switch $oldlist {
		{size eye quat} {
			set fd [open "| ${mged_sketch_anim_dir}anim_orient qv y \
					> $mged_sketch_temp1 " w]
			sketch_text_to_fd $text $fd "5,6,7,8"
			catch {close $fd}
			set fd [open $mged_sketch_temp1 r]
			sketch_text_col_arith $text all {@0 @1 @2 @3 @4}
			sketch_text_from_fd $text $fd all right
			close $fd
			exec rm $mged_sketch_temp1
		}
		{eye center} {
			
			set fd [open "| ${mged_sketch_anim_dir}anim_lookat -y -v \
					> $mged_sketch_temp1" w]
			sketch_text_to_fd $text $fd all
			close $fd
			set fd [open $mged_sketch_temp1 r]
			sketch_text_from_fd $text $fd all replace
			close $fd
			exec rm $mged_sketch_temp1
		}
		{size eye ypr} -
		default {}
	}

	#convert from {size eye ypr}
	switch $newlist {
		{size eye quat} {
			set fd [open "| ${mged_sketch_anim_dir}anim_orient y qv \
					> $mged_sketch_temp1 " w]
			sketch_text_to_fd $text $fd "5,6,7"
			catch {close $fd}
			set fd [open $mged_sketch_temp1 r]
			sketch_text_col_arith $text all {@0 @1 @2 @3 @4}
			sketch_text_from_fd $text $fd all right
			close $fd
			exec rm $mged_sketch_temp1
		}
		{eye center} {
			text $text._scratch_
			sketch_text_do_script $text $text._scratch_ all \
			   {@0 @2 @3 @4 @5 @6 @7 {@1*0.5} 0.0 0.0 0.0 0.0 0.0}
			set fd [open "| ${mged_sketch_anim_dir}anim_cascade \
					> $mged_sketch_temp2" w]
			sketch_text_to_fd $text._scratch_ $fd all
			close $fd
			destroy $text._scratch_
			sketch_text_col_arith $text all {@0 @2 @3 @4}
			set fd [open $mged_sketch_temp2 r]
			sketch_text_from_fd $text $fd "1,2,3" right
			close $fd
			exec rm $mged_sketch_temp2
		}
		{size eye ypr} -
		default {}
	}

	$text configure -state disabled
	set oldlist $newlist
	return
}



#append current view parameters to view curve
proc sketch_vappend {} {
	global mged_sketch_vnode mged_sketch_vcount mged_sketch_vtinc
	global mged_sketch_vtime mged_sketch_vname mged_sketch_vparams

	set text .sketch.view._v_$mged_sketch_vname.t
	set length [sketch_text_rows $text]
	if { $length == 0 } {
		set mged_sketch_vtime 0.0
		set mged_sketch_vnode 0
		set node $mged_sketch_vnode
		incr node 1
	} else {
		set node $mged_sketch_vnode
		incr node 1
		set mged_sketch_vtime [lindex [$text get \
		   "$node.0" "$node.0 lineend"] 0]
		set mged_sketch_vtime \
		   [expr $mged_sketch_vtime+$mged_sketch_vtinc]
	}
	incr node 1
	$text configure -state normal
	$text insert "$node.0" [sketch_get_view_line nl]
	$text configure -state disabled
	incr mged_sketch_vnode 1

	sketch_vupdate
}

proc sketch_get_view_line { {mode 0}} {
	global mged_sketch_vtime mged_sketch_vparams

	set line "\t$mged_sketch_vtime"
	foreach cmd $mged_sketch_vparams {
		set new [join [vget $cmd] "\t"]
		append line "\t$new"
	}
	if { $mode == "nl" } {
		return "$line\n"
	}
	return $line
}
	
#delete specified node
proc sketch_vdelete { n } {
	global mged_sketch_vname

	set text .sketch.view._v_$mged_sketch_vname.t
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
	global mged_sketch_vtime mged_sketch_vname

	if { $n == "" } {
		sketch_vupdate
		return
	}
	incr n 1
	set text .sketch.view._v_$mged_sketch_vname.t
	set mged_sketch_vtime [lindex [$text get "$n.0" "$n.0 lineend"] 0]
	$text configure -state normal
	$text delete "$n.0" "$n.0 lineend"
	$text insert "$n.0" [sketch_get_view_line] 			
	$text configure -state disabled

	sketch_vupdate
}

#insert current view center at specified node
proc sketch_vinsert { n } {
	global mged_sketch_vtinc global mged_sketch_vtime mged_sketch_vname

	if { $n == "" } {
		sketch_vupdate
		return
	}
	incr n 1
	set text .sketch.view._v_$mged_sketch_vname.t
	set t2 [lindex [$text get "$n.0" "$n.0 lineend"] 0]
	if { $n == 1 } {
		set mged_sketch_vtime [expr $t2 - $mged_sketch_vtinc]
	} else {
		set i [expr $n - 1]
		set t1 [lindex [$text get "$i.0" "$i.0 lineend"] 0]
		set mged_sketch_vtime  [expr 0.5*($t1+$t2)]
	}
	$text configure -state normal
	$text insert "$n.0" [sketch_get_view_line nl] 			
	$text configure -state disabled
	
	sketch_vupdate
}

#update description of view curve
proc sketch_vupdate {} {
	global mged_sketch_vcount mged_sketch_vtime mged_sketch_vnode
	global mged_sketch_vname mged_sketch_vparams mged_sketch_cmdlen

	if { $mged_sketch_vname == "" } {
		puts "sketch_vupdate: no view curve"
		return
	}
	set text .sketch.view._v_$mged_sketch_vname.t
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
	eval viewset $str
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
	global mged_sketch_vname

	upvar #0 mged_sketch_vnode node
	if {$node != ""} {
		set n [expr $node + 1]
		set text .sketch.view._v_$mged_sketch_vname.t
		set line [$text get "$n.0" "$n.0 lineend"]
		set lline [split $line "\t"]
		set lline [lreplace $lline 1 1 $value]
		set line [join $lline "\t"]
		$text configure -state normal
		$text delete "$n.0" "$n.0 lineend"
		$text insert "$n.0" $line
		$text configure -state disabled
	}
	focus .sketch
}


proc sketch_popup_vsave { type  } {
	global mged_sketch_vname

	set entries [list \
		[list "Name of View curve" $mged_sketch_vname] \
		{"Save to File:"} \
		{"Which columns:" "all"} \
		]
	set buttons [list \
		{"OK" {sketch_vsave [.sketch.input.f0.e get] \
			[.sketch.input.f1.e get] \
			[.sketch.input.f2.e get] } }\
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Save View curve" $entries $buttons
}

proc sketch_vsave { vcurve filename cols } {
	set text .sketch.view._v_$vcurve.t
	if {[info commands $text] == ""} {
		tk_dialog .sketch.msg {Can't find View Curve} \
		   "Can't find view curve $vcurve." {} 0 {OK}
		return
	}
	if {[file exists $filename] } {
		set ans [tk_dialog .sketch.msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}
	set fd [open $filename w]
	sketch_text_to_fd $text $fd $cols
	close $fd
	catch {destroy .sketch.input}
}


proc sketch_popup_vload {} {
	global mged_sketch_vname
	set entries [list \
		{"File to Load"} \
		[list "Name of View Curve" $mged_sketch_vname] \
		{"Load which columns:" "all"} \
		]
	set buttons [list \
		[list "OK" "sketch_vload \[.sketch.input.f0.e get\] \
			\[.sketch.input.f1.e get\] \
			\[.sketch.input.f2.e get\] \$mged_sketch_text_lmode"] \
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Load View Curve" $entries $buttons
	frame .sketch.input.f3
	pack .sketch.input.f3 -side bottom
	radiobutton .sketch.input.f3.r0 -text "Replace" \
		-variable mged_sketch_text_lmode -value "replace"
	radiobutton .sketch.input.f3.r1 -text "Append" \
		-variable mged_sketch_text_lmode -value "end"
	radiobutton .sketch.input.f3.r2 -text "Add New Columns" \
		-variable mged_sketch_text_lmode -value "right"

	pack .sketch.input.f3.r0 .sketch.input.f3.r1 .sketch.input.f3.r2 \
		-side left -fill x
}

proc sketch_vload { filename vcurve cols mode} {
	global mged_sketch_vname mged_sketch_vparams

	set oldname $mged_sketch_vname
	sketch_open_vcurve $vcurve
	#check for correct number of columns
	set fd [open $filename r]
	set numcol [llength [split [gets $fd] \t]]
	incr numcol -1
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
	set text .sketch.view._v_$vcurve.t
	set fd [open $filename r]
	$text configure -state normal
	sketch_text_from_fd $text $fd $cols $mode
	$text configure -state disabled
	close $fd
	catch {destroy .sketch.input}
	sketch_vupdate
}
	
	

proc sketch_popup_vname {{mode select}} {
	if { $mode == "select"} {
		sketch_popup_input "View Curve Select" {
			{"V-curve to select:" ""}
		} {
			{"OK" {sketch_vname [.sketch.input.f0.e get]}}
			{"Cancel" "destroy .sketch.input"}
		}
	} elseif { ($mode == "rename") || ($mode == "copy") } {
		sketch_popup_input "View Curve Rename" {
			{"New name for v-curve:" ""}
		} [list \
			[list "OK" "sketch_vrename \[.sketch.input.f0.e get\] \
				$mode" ] \
			{"Cancel" "destroy .sketch.input"} \
		]
	}
}

proc sketch_vname { name } {
	sketch_open_vcurve $name
	catch {destroy .sketch.input}
	sketch_vupdate
}
 
proc sketch_vrename { name mode } {
	global mged_sketch_vname

	set oldname $mged_sketch_vname
	if { $oldname == $name } {
		catch {destroy .sketch.input}
	}
	set ntext .sketch.view._v_$name.t
	set otext .sketch.view._v_$oldname.t
	
	if {[info commands $ntext] != ""} {
		set ans [tk_dialog .sketch.msg {View Curve Exists} \
		   "View curve $name already exists." {} 1 {Overwrite} \
		   {Cancel}
		if {$ans == 1} return
	}
	sketch_open_vcurve $name
	$ntext configure -state normal
	sketch_text_from_text $otext $ntext all replace
	$ntext configure -state disabled
	if { $mode == "rename"} {
		destroy .sketch.view._v_$oldname
	}
	#else copy
	sketch_vupdate
	catch {destroy .sketch.input}
}
	
proc sketch_popup_delete_vcurve {} {
	global mged_sketch_vname
	set entries [list \
		[list "Delete View Curve:" $mged_sketch_vname] \
		]
	set buttons [list \
		{ "OK" {sketch_delete_vcurve [.sketch.input.f0.e get]; \
				destroy .sketch.input} } \
		{ "Cancel" "destroy .sketch.input" } \
		]
	sketch_popup_input "Delete View Curve" $entries $buttons
}

proc sketch_delete_vcurve { name } {
	catch {destroy .sketch.view._v_$name}
	catch {unset mged_sketch_vparams_$name}
	sketch_open_vcurve ""
	sketch_vupdate
}


proc sketch_view_cancel {} {
	wm withdraw .sketch.view
	foreach ted [info commands .sketch.view._v_*.t] {
		regsub {(^\.sketch\.view\._v_.*)(\.t$)} $ted {\1} name
		wm withdraw $name
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
		tk_dialog .sketch.msg {Wrong number of columns} \
		   "You provided $incol columns of data. However, this view \
		   curve requires $k columns.  \
		   The columns should have the following format: $descr" \
		   {} 0 {OK}
		return -1
	}
	return 0
}
		

#-----------------------------------------------------------------
# Table Editor
#-----------------------------------------------------------------
proc sketch_popup_text_save { w } {
	set entries [list \
		{"Save to File:"} \
		{"Save which columns:" all} \
		]
	set buttons [list \
		[list  "OK" \
		  [concat sketch_text_save $w \
		    {[.sketch.input.f0.e get] [.sketch.input.f1.e get]}] ]\
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Save Columns" $entries $buttons
}

proc sketch_popup_text_load { w } {
	global mged_sketch_text_lmode

	set entries [list \
		{"Load from file:"} \
		{"Load which columns:" all} \
		]
	set buttons [list \
		[list  "OK" "sketch_text_load $w \
		    \[.sketch.input.f0.e get\] \[.sketch.input.f1.e get\] \
			\$mged_sketch_text_lmode" ] \
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Load Columns" $entries $buttons
	frame .sketch.input.f3
	pack .sketch.input.f3 -side bottom
	radiobutton .sketch.input.f3.r0 -text "Replace" \
		-variable mged_sketch_text_lmode -value "replace"
	radiobutton .sketch.input.f3.r1 -text "Append" \
		-variable mged_sketch_text_lmode -value "end"
	radiobutton .sketch.input.f3.r2 -text "Add New Columns" \
		-variable mged_sketch_text_lmode -value "right"

	pack .sketch.input.f3.r0 .sketch.input.f3.r1 .sketch.input.f3.r2 \
		-side left -fill x
}

proc sketch_popup_text_time { w } {
	set entries [list \
		{"Start Speed:" auto} \
		{"End Speed:" auto} \
		]
	set buttons [list \
		[list  "OK" "sketch_text_time $w \
		    \[.sketch.input.f0.e get\] \[.sketch.input.f1.e get\]"] \
		{"Cancel" "destroy .sketch.input"} \
		]
	sketch_popup_input "Load Columns" $entries $buttons
}

proc sketch_text_time {w v0 v1} {
	global mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_dir

	#global mged_sketch_text_lmode

	if { ($v0 == "auto") || ($v0 == "") } {
		set arg0 ""
	} else {
		set arg0 "-s $v0"
	}
	if { ($v1 == "auto") || ($v1 == "") } {
		set arg1 ""
	} else {
		set arg1 "-e $v1"
	}
	#count number of lines, doesn't matter if a couple extra
	scan [$w index end] %d maxlen
	set arg2 "-m $maxlen"
	#until new command is installed
	set cmd "| ${mged_sketch_anim_dir}anim_time $arg0 $arg1 $arg2 > $mged_sketch_temp1"
	#puts $cmd
	set f1 [open $cmd w] 
	sketch_text_to_fd $w $f1 "0,1,2,3"
	close $f1
	#set temp $mged_sketch_text_lmode
	#set mged_sketch_text_lmode left
	set f1 [open $mged_sketch_temp1 r]
	sketch_text_from_fd $w $f1 0 left
	close $f1
	sketch_text_from_text $w $w "0,2-" replace
	exec rm $mged_sketch_temp1
	#set mged_sketch_text_lmode $temp
	catch {destroy .sketch.input}
}

#table editor for curves
proc sketch_popup_text { args } {

	upvar #0  mged_sketch_text_index index
	set z $index
	#puts "creating .sketch.text$z"
	if { [info commands .sketch.text$z] == "" } {
		#create if not yet existing
		#puts "does not yet exist"
		sketch_popup_text_create .sketch.text$z $z
		incr index
	}

	#fill with appropriate text
	switch [lindex $args 0] {
		empty {.sketch.text$z.t delete 1.0 end}
		curve {
		  set oldname [vdraw r n]
		  sketch_open_curve [lindex $args 1]
		  .sketch.text$z.t delete 1.0 end
		  sketch_text_echoc .sketch.text$z.t
		  sketch_open_curve $oldname }
		clone {
		  sketch_text_copy [lindex $args 1] \
		     .sketch.text$z.t replace }
		default {
		  .sketch.text$z.t delete 1.0 end
		  sketch_text_echoc .sketch.text$z.t
		}
	}

	#finish textbar initialization
	.sketch.text$z.textbar insert 1.0 "\ttime(0)\tx(1)\ty(2)\tz(3)"
	sketch_text_bar_set .sketch.text$z.t .sketch.text$z.textbar 0.0
}

proc sketch_popup_text_create { name index {mode rw}} {

	toplevel $name
	wm title $name "MGED Animator table editor $index"
	text $name.t -width 80 -height 20 -wrap none \
		-tabs {20 numeric 220 numeric 420 numeric 620 numeric} \
		-xscrollcommand \
		"sketch_scroll_both $name" \
		-yscrollcommand "$name.s1 set"
	text $name.textbar -width 80 -height 1 -wrap none \
		-tabs {50 center 250 center 450 center 650 center}
	scrollbar $name.s0 -command \
		"$name.t xview" \
		-orient horizontal
	scrollbar $name.s1 -command "$name.t yview"
	frame $name.f1
	label $name.f1.l0 -text "Table editor $index"
	frame  $name.f0
	if { $mode == "rw" } {
		button $name.f0.b3 -text "Clear" -command "$name.t delete 1.0 end"
		button $name.f0.b4 -text "Interpolate" -command "sketch_popup_text_interp $name.t $name.textbar"
		button $name.f0.b5 -text "Estimate Time" -command "sketch_popup_text_time $name.t"
		button $name.f0.b7 -text "Edit Columns" -command "sketch_popup_text_col $name.t $name.textbar"
		menubutton $name.f0.mb0 -text "Read" -menu $name.f0.mb0.m
		menu $name.f0.mb0.m -tearoff 0 -postcommand "sketch_post_read_menu $name.f0.mb0.m $name.t"
		$name.f0.mb0.m add command -label "from File" -command "sketch_popup_read $name.t file"
		$name.f0.mb0.m add command -label "from Curve" -command "sketch_popup_read $name.t curve"
		button $name.f0.b6 -text "Cancel" -command "destroy $name"
	} else {
		button $name.f0.b6 -text "Hide" -command "wm withdraw $name"
	}
	button $name.f0.b8 -text "Clone" -command "sketch_popup_text clone $name.t"
	button $name.f0.b9 -text "Main" -command "raise .sketch"
	menubutton $name.f0.mb1 -text "Write" -menu $name.f0.mb1.m
	menu $name.f0.mb1.m -tearoff 0
	$name.f0.mb1.m add command -label "to File" -command "sketch_popup_write $name.t file"
	$name.f0.mb1.m add command -label "to Curve" -command "sketch_popup_write $name.t curve"
	$name.f0.mb1.m add command -label "to V-curve" -command "sketch_popup_write $name.t vcurve"

	pack $name.f0 $name.s0 -side bottom -fill x
	pack $name.s1 -side right -fill y	
	pack $name.f0.mb1 -side left -fill x -expand yes
	if { $mode == "rw" } {
		pack $name.f0.mb0 \
			$name.f0.b3 $name.f0.b4 $name.f0.b5 $name.f0.b7 \
			-side left -fill x -expand yes
	}
	pack $name.f0.b8 $name.f0.b9 $name.f0.b6 \
		-side left -fill x -expand yes

	pack $name.f1 $name.textbar $name.t\
		-side top -expand yes -fill x -anchor w
	pack $name.f1.l0

	if { $mode == "ro" } {
		$name.t configure -state disabled
	}

}

proc sketch_scroll_both { w args} {
	eval $w.s0 set $args
	eval sketch_text_bar_set $w.t $w.textbar $args
}

#match number of columns in time bar with number of columns in text
#first line. Adjust time bar scroll
proc sketch_text_bar_set { w wbar args } {
	set i [llength [split [$w get 1.0 "1.0 lineend"] "\t"]]
	set j [llength [split [$wbar get 1.0 "1.0 lineend"] "\t"]]
	if {$i > 0} {
		incr i -1
	}
	if {$j > 0} {
		incr j -1
	}
	while { $j < $i } {
		$wbar insert "1.0 lineend" "\t$j        "
		incr j
	}
	if { $j > $i } {
		set line [$wbar get 1.0 "1.0 lineend"]
		set newline [join [lrange [split $line "\t"] 0 $i] "\t"]
		#puts "barset $i $j $line $newline"
		$wbar delete 1.0 "1.0 lineend"
		$wbar insert 1.0 $newline
	}
	
	$wbar xview moveto [lindex $args 0]
}


proc sketch_text_bar_reset { w } {
	if {[regsub {(^\..+)(\.[^\.]+$)} $w {\1} parent] == 0} {
		#no parent, do nothing
		return
	}
	if { [info commands $parent.textbar] == "" } {
		return
	}
	sketch_text_bar_set $w $parent.textbar [lindex [$w xview] 0]
}

proc sketch_post_read_menu { menu text } {
	$menu delete 0 end
	$menu add command -label "from File" -command "sketch_popup_read $text file"
	$menu add command -label "from Curve" -command "sketch_popup_read $text curve"
	foreach ted [info commands *.text*.t] {
		set good [regsub {(.+\.text)([0-9]+)(\.t$)} $ted {\2} index]
		if { $good == 1 } {
			$menu add command \
			  -label "from editor $index" -command \
			  "sketch_popup_read $text $ted"
		}
	}
	foreach ted [info commands .sketch.view._v_*.t] {
		set good [regsub {(\.sketch\.view\._v_)(.+)(\.t)} \
		   $ted {\2} vcurve]
		if { $good == 1 } {
			$menu add command \
			  -label "from v-curve $vcurve" -command \
			  "sketch_popup_read $text $ted"
		}
	}
}



proc sketch_popup_text_col {w wbar} {
	
	#make sure bar is up to date
	sketch_text_bar_reset $w
	#sketch_text_bar_set $w $wbar [lindex [$w xview] 0]

	catch { destroy .sketch.col }
	toplevel .sketch.col
	frame .sketch.col.fa
	frame .sketch.col.fb
	pack .sketch.col.fb .sketch.col.fa -side bottom -anchor e
	set collist [lrange [split [$wbar get 1.0 "1.0 lineend"] "\t"] \
			1 end]
	set i 0
	set cmd "sketch_text_do_col $w \[.sketch.col.fb.e0 get\]"
	foreach col $collist {
		set cmd [sketch_text_col_add $i $col $cmd old]
		incr i
	}
	#append cmd "; sketch_text_bar_reset $w; destroy .sketch.col"
	
	if {$i > 0} {
		bind  .sketch.col.fr[expr $i-1].e0 <Key-Return> \
			{.sketch.col.fa.b0 invoke}
	}
	button .sketch.col.fa.b2 -text "Add Column" -command {sketch_text_col_add_one} 
	button .sketch.col.fa.b0 -text "OK" -command $cmd
	button .sketch.col.fa.b1 -text "Cancel" -command {destroy .sketch.col}
	label .sketch.col.fb.l0 -text "Number of Rows:" 
	entry .sketch.col.fb.e0 -width 5
	.sketch.col.fb.e0 insert end "all"
	pack .sketch.col.fa.b2 .sketch.col.fa.b0 .sketch.col.fa.b1 -side left
	pack .sketch.col.fb.l0 .sketch.col.fb.e0 -side left -fill x

	if { $i > 0 } {
		focus .sketch.col.fr0.e0
	}
}

proc sketch_text_col_add_one {} {
	set num [llength [info commands .sketch.col.fr*.e0]]
	set cmd [lindex [split [.sketch.col.fa.b0 cget -command] \;] 0]
	set cmd [sketch_text_col_add $num $num $cmd new]
	bind  .sketch.col.fr$num.e0 <Key-Return> {.sketch.col.fa.b0 invoke}
	.sketch.col.fa.b0 configure -command $cmd
}

proc sketch_text_col_add { i col cmd flag } {
		frame .sketch.col.fr$i
		label .sketch.col.fr$i.l0 -text "$col:" -width 10
		entry .sketch.col.fr$i.e0 -width 20
		if { $flag == "old" } {
			.sketch.col.fr$i.e0 insert end @$i
		}
		append cmd " \[.sketch.col.fr$i.e0 get\]"
		if {$i > 0} {
			set j [expr $i-1]
			bind .sketch.col.fr$j.e0 <Key-Return> \
			  "focus .sketch.col.fr$i.e0"
		}
		pack .sketch.col.fr$i -side top
		pack .sketch.col.fr$i.l0 .sketch.col.fr$i.e0 -side left
		return $cmd
}

proc sketch_text_do_col {w rows args} {
	sketch_text_col_arith $w $rows $args
	sketch_text_bar_reset $w
	destroy .sketch.col
}


proc sketch_text_col_arith {w rows arglist} {
	set buffer $w._scratch_
	#destroy if exists already
	if {[info commands $buffer] != ""} {
		destroy $buffer
	}
	text $buffer
	sketch_text_do_script $w $buffer $rows $arglist
	$w delete 1.0 end
	$w insert end [$buffer get 1.0 end]
	destroy $buffer
}

#win - take text from
#wout - write text to
#rows - number of rows to write (copies source length if rows not int >= 0)
#args - series of column arithmetic descriptions
# @1 refers to column 1, @pi refers to pi, @i refers to row index
# @n refers to number of rows
proc sketch_text_do_script {win wout rows slist} {
	#parse scripts
	set colout 0

	foreach script $slist {
		if {$script != ""} {
			regsub -all {@pi} $script 3.141592654 temp
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

proc sketch_popup_text_interp {w wbar}	{
	#make sure bar is up to date
	sketch_text_bar_reset $w

	catch { destroy .sketch.col }
	toplevel .sketch.col
	frame .sketch.col.fa
	frame .sketch.col.fb
	frame .sketch.col.fc
	frame .sketch.col.fd
	menubutton .sketch.col.mb0 -text "Choose Interpolator" \
		-menu .sketch.col.mb0.m0
	menu .sketch.col.mb0.m0
	pack .sketch.col.mb0 .sketch.col.fa -side bottom -fill x -expand yes
	pack .sketch.col.fd .sketch.col.fc \
		.sketch.col.fb -side bottom -anchor e
	set collist [lrange [split [$wbar get 1.0 "1.0 lineend"] "\t"] \
			2 end]
	set i 1
	set cmd "sketch_text_do_interp $w \[.sketch.col.fb.e0 get\] \
		\[.sketch.col.fc.e0 get\] \[.sketch.col.fd.e0 get\]"
	foreach col $collist {
		set cmd [sketch_text_interp_add $i $col $cmd old]
		incr i
	}
	#append cmd "; sketch_text_bar_reset $w; destroy .sketch.col"
	
	if {$i > 1} {
		bind  .sketch.col.fr[expr $i-1].e0 <Key-Return> \
			{focus .sketch.col.fb.e0}
	}
	button .sketch.col.fa.b2 -text "Add Column" -command {sketch_text_interp_add_one} 
	button .sketch.col.fa.b0 -text "OK" -command $cmd
	button .sketch.col.fa.b1 -text "Cancel" -command {destroy .sketch.col}
	label .sketch.col.fb.l0 -text "Start Time:" 
	entry .sketch.col.fb.e0 -width 10
	bind .sketch.col.fb.e0 <Key-Return> {focus .sketch.col.fc.e0}
	label .sketch.col.fc.l0 -text "End Time:" 
	entry .sketch.col.fc.e0 -width 10
	bind .sketch.col.fc.e0 <Key-Return> {focus .sketch.col.fd.e0}
	label .sketch.col.fd.l0 -text "Frames Per Second:" 
	entry .sketch.col.fd.e0 -width 10
	bind .sketch.col.fd.e0 <Key-Return> {.sketch.col.fa.b0 invoke}
	.sketch.col.fd.e0 insert end "30"
	pack .sketch.col.fa.b2 .sketch.col.fa.b0 .sketch.col.fa.b1 -side left
	pack .sketch.col.fb.l0 .sketch.col.fb.e0 \
		.sketch.col.fc.l0 .sketch.col.fc.e0 \
		.sketch.col.fd.l0 .sketch.col.fd.e0 \
		-side left -fill x

	.sketch.col.mb0.m0 add command -label "Step (src)" -command {sketch_interp_fill step i}
	.sketch.col.mb0.m0 add command -label "Linear (src)" -command {sketch_interp_fill linear i}
	.sketch.col.mb0.m0 add command -label "Spline (src)" -command {sketch_interp_fill spline i}
	.sketch.col.mb0.m0 add command -label "Periodic spline (src)" -command {sketch_interp_fill cspline i}
	.sketch.col.mb0.m0 add command -label "Quaternion (src)" -command {sketch_interp_fill quat i}
	.sketch.col.mb0.m0 add command -label "Rate (init) (incr/s)" -command {sketch_interp_fill rate }
	.sketch.col.mb0.m0 add command -label "Accel (init) (incr/s)" -command {sketch_interp_fill accel}
	.sketch.col.mb0.m0 add command -label "Next (src) (offset)" -command {sketch_interp_fill next}

	if { $i > 1 } {
		focus .sketch.col.fr1.e0
	}

	#guess start and end times
	set n [sketch_text_rows $w]
	if { $n < 2} {return}
	.sketch.col.fb.e0 insert end \
		[lindex [split [$w get 1.0 "1.0 lineend"] \t] 1]
	.sketch.col.fc.e0 insert end \
		[lindex [split [$w get $n.0 "$n.0 lineend"] \t] 1]
	
}

proc sketch_text_interpolate { w start stop fps slist } {
	global mged_sketch_temp1 mged_sketch_temp2 mged_sketch_tab_dir
	#global mged_sketch_text_lmode
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
						tk_dialog .sketch.msg {Invalid entry} \
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
		tk_dialog .sketch.msg {Invalid entry} \
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

	set fd [open "| ${mged_sketch_tab_dir}tabinterp -q < $mged_sketch_temp2 " r]
	#set mged_sketch_text_lmode replace	
	sketch_text_from_fd $w $fd all replace
	#catch can be removed when -q option added to tabinterp
	catch {close $fd}
	return 0	
}
	
proc sketch_interp_fill { str args} {
	if { $args == "" } {
		foreach ent [info commands {.sketch.col.fr[0-9]*.e0}] {
			$ent delete 0 end
			$ent insert end $str
		}
	} else {
		set ent [info commands .sketch.col.fr1.e0]
		for {set i 1} { $ent != ""} { 
			set ent [info commands .sketch.col.fr$i.e0]} {
			$ent delete 0 end
			$ent insert end "$str $i"
			incr i
		}
	}
}
			

proc sketch_text_interp_add { i col cmd flag } {
		frame .sketch.col.fr$i
		label .sketch.col.fr$i.l0 -text "$col:" -width 10
		entry .sketch.col.fr$i.e0 -width 20
		if { $flag == "old" } {
			.sketch.col.fr$i.e0 insert end "spline $i"
		}
		append cmd " \[.sketch.col.fr$i.e0 get\]"
		if {$i > 1} {
			set j [expr $i-1]
			bind .sketch.col.fr$j.e0 <Key-Return> \
			  "focus .sketch.col.fr$i.e0"
		}
		pack .sketch.col.fr$i -side top
		pack .sketch.col.fr$i.l0 .sketch.col.fr$i.e0 -side left
		return $cmd
}

proc sketch_text_interp_add_one {} {
	set num [llength [info commands .sketch.col.fr*.e0]]
	incr num
	set cmd [.sketch.col.fa.b0 cget -command]
	#set cmd [lindex [split [.sketch.col.fa.b0 cget -command] \;] 0]
	set cmd [sketch_text_interp_add $num $num $cmd new]
	bind  .sketch.col.fr$num.e0 <Key-Return> {focus .sketch.col.fb.e0}
	.sketch.col.fa.b0 configure -command $cmd
}

proc sketch_text_do_interp { w start stop fps args } {
	if {[sketch_text_interpolate $w $start $stop $fps $args] != 0} {
		return
	}
	sketch_text_bar_reset $w
	destroy .sketch.col
}

proc sketch_popup_read {w src} {
	global mged_sketch_text_lmode
	switch $src {
		file {
			set entries [list [list "File to read:" ""]]
			set okcmd "sketch_text_readf $w \
				\[.sketch.input.f0.e get\] \
				\[.sketch.input.f1.e get\] \
				\$mged_sketch_text_lmode"
		}
		curve {
			set entries [list [list "Curve to read:" [vdraw r n]]]
			set okcmd "sketch_text_readc $w \
				\[.sketch.input.f0.e get\] \
				\[.sketch.input.f1.e get\] \
				\$mged_sketch_text_lmode"
		
}
		#else text-text
		default {
			set entries {}
			set okcmd "sketch_text_from_text $src $w \
				\[.sketch.input.f0.e get\] \
				\$mged_sketch_text_lmode; \
				destroy .sketch.input"
		}
	}
	lappend entries {"Which columns:" "all"}
	set buttons [list \
		[list "OK" $okcmd] \
		[list "Cancel" "destroy .sketch.input"] \
		]
	sketch_popup_input "Read from $src" $entries $buttons
	frame .sketch.input.f3
	pack .sketch.input.f3 -side bottom
	radiobutton .sketch.input.f3.r0 -text "Replace" \
		-variable mged_sketch_text_lmode -value "replace"
	radiobutton .sketch.input.f3.r1 -text "Append" \
		-variable mged_sketch_text_lmode -value "end"
	radiobutton .sketch.input.f3.r2 -text "Add New Columns" \
		-variable mged_sketch_text_lmode -value "right"

	pack .sketch.input.f3.r0 .sketch.input.f3.r1 .sketch.input.f3.r2 \
		-side left -fill x

}

proc sketch_text_readc {w curve col mode} {
	set oldcurve [vdraw r n]
	sketch_open_curve $curve
	text $w._scratch_
	sketch_text_echoc $w._scratch_ 
	sketch_text_from_text $w._scratch_ $w $col $mode
	destroy $w._scratch_
	sketch_open_curve $oldcurve
	sketch_text_bar_reset $w
	catch {destroy .sketch.input}

}

proc sketch_popup_write {w dst} {
	global mged_sketch_vname
	switch $dst {
		file {
			set entries [list [list "Write to file:" ""]]
			set okcmd "sketch_text_writef $w \
				\[.sketch.input.f0.e get\] \
				\[.sketch.input.f1.e get\]"
		}
		curve {
			set entries [list [list "Write to curve:" [vdraw r n]]]
			set okcmd "sketch_text_writec $w \
				\[.sketch.input.f0.e get\] \
				\[.sketch.input.f1.e get\]"
		}
		vcurve {
			set entries [list [list "Write to v-curve:" $mged_sketch_vname]]
			set okcmd "sketch_text_writevc $w \
				\[.sketch.input.f0.e get\] \
				\[.sketch.input.f1.e get\]"
		}

	}
	lappend entries {"Which columns:" "all"}
	set buttons [list \
		[list "OK" $okcmd] \
		[list "Cancel" "destroy .sketch.input"] \
		]
	sketch_popup_input "Write to $dst" $entries $buttons
}

proc sketch_text_writec {w curve col} {
	
	text $w._scratch_
	sketch_text_from_text $w $w._scratch_ $col append
	set i [llength [split [$w._scratch_ get 1.0 "1.0 lineend"] "\t"]]
	incr i -1
	if { $i < 3 } {
		destroy $w._scratch_
		puts "Need at least three columns"
		return -1
	}
	if { $i == 3 } {
		#assume time is missing
		sketch_text_col_arith $w._scratch_ all {@i @0 @1 @2}
		puts "Filling in missing time column"
	}
	set oldcurve [vdraw r n]
	sketch_open_curve $curve
	sketch_text_apply $w._scratch_ replace
	sketch_open_curve $oldcurve
	destroy $w._scratch_
	catch {destroy .sketch.input}
	sketch_update

}

proc sketch_text_writevc {w vcurve col} {
	global mged_sketch_vparams mged_sketch_vname

	set oldname $mged_sketch_vname
	sketch_open_vcurve $vcurve
	#check for correct number of columns	
	set numcol [llength [split [$w get 1.0 "1.0 lineend"] \t]]
	incr numcol -1
	if { $col == "all" } {
		set num $numcol
	} else {
		set num [sketch_parse_col $col $numcol output]
	}
	if { [sketch_vcurve_check_col $mged_sketch_vparams $num] == -1} {
		sketch_open_vcurve $oldname
		return -1
	}

	set text .sketch.view._v_$vcurve.t
	$text configure -state normal
	sketch_text_from_text $w $text $col replace
	$text configure -state disabled
	sketch_vupdate
	catch {destroy .sketch.input}
}



#-----------------------------------------------------------------
# Create Scripts
#-----------------------------------------------------------------
#control creation of animation scripts
proc sketch_popup_objanim { {mode object} } {

	if { [info commands .sketch.oanim] != ""} {
		catch {destroy .sketch.oanim}
	}
	toplevel .sketch.oanim

	frame .sketch.oanim.f0
	label .sketch.oanim.f0.l0 -text "Output file: "
	entry .sketch.oanim.f0.e0 -width 20 -textvariable mged_sketch_objscript
	frame .sketch.oanim.f1
	label .sketch.oanim.f1.l0 -text Source
	tk_optionMenu .sketch.oanim.f1.om0 mged_sketch_objsrctype \
		"curve:" "editor:" "file:" 
	entry .sketch.oanim.f1.e0 -width 20 -textvariable mged_sketch_objsource
	frame .sketch.oanim.f2
	if {$mode == "view"} {
		wm title .sketch.oanim "MGED View Animation"
		label .sketch.oanim.l0 -text "CREATE VIEW ANIMATION"
		button .sketch.oanim.f2.l0 -text "View size:" -command \
			{set mged_sketch_objvsize [vget size]}
		entry .sketch.oanim.f2.e0 -width 20 -textvariable mged_sketch_objvsize
		frame .sketch.oanim.f9
		button .sketch.oanim.f9.b0 -text "Eye point:" \
			-command { set mged_sketch_eyecen [vget eye] }
		entry .sketch.oanim.f9.e0 -width 20 -textvariable mged_sketch_eyecen
		frame .sketch.oanim.f10
		button .sketch.oanim.f10.b0 -text "Eye yaw,pitch,roll: " \
			-command { set mged_sketch_eyeori [vget ypr] }
		entry .sketch.oanim.f10.e0 -width 20 -textvariable mged_sketch_eyeori
		set if_view ".sketch.oanim.f9 .sketch.oanim.f10"
		checkbutton .sketch.oanim.cb0 -text "Read viewsize from source" \
			-variable mged_sketch_objrv -command "sketch_script_update $mode"
		.sketch.oanim.cb0 deselect
		uplevel #0 set mged_sketch_objdisp "-d"
		uplevel #0 set mged_sketch_objrot "-b"
	} else {
		wm title .sketch.oanim "MGED Object Animation"
		set if_view ""
		label .sketch.oanim.l0 -text "CREATE OBJECT ANIMATION"
		label .sketch.oanim.f2.l0 -text "Object name:"
		entry .sketch.oanim.f2.e0 -width 20 -textvariable mged_sketch_objname
		checkbutton .sketch.oanim.cb1 -text "Relative Displacement" \
			-variable mged_sketch_objdisp -offvalue "-d" -onvalue "-c"
		checkbutton .sketch.oanim.cb2 -text "Relative Rotation" \
			-variable mged_sketch_objrot -offvalue "-b" -onvalue "-a"
		uplevel #0 set mged_sketch_objrv 0
		.sketch.oanim.cb1 deselect
		.sketch.oanim.cb2 deselect
	}
	frame .sketch.oanim.f3
	button .sketch.oanim.f3.b0 -text "Object center:" \
		-command { set mged_sketch_objcen [vget center] }
	entry .sketch.oanim.f3.e0 -width 20 -textvariable mged_sketch_objcen
	frame .sketch.oanim.f4
	button .sketch.oanim.f4.b0 -text "Object yaw,pitch,roll: " \
		-command { set mged_sketch_objori [vget ypr] }
	entry .sketch.oanim.f4.e0 -width 20 -textvariable mged_sketch_objori
	checkbutton .sketch.oanim.cb3 -text "No Translation" \
		-variable mged_sketch_objrotonly -command "sketch_script_update $mode"
	.sketch.oanim.cb3 deselect
	frame .sketch.oanim.f5
	label .sketch.oanim.f5.l0 -text "First frame:"
	entry .sketch.oanim.f5.e0 -width 20 -textvariable mged_sketch_objframe
	frame .sketch.oanim.f6
	button .sketch.oanim.f6.b0 -text "Create Script" -command "sketch_objanim $mode"
	button .sketch.oanim.f6.b1 -text "Show Script" -command {sketch_popup_preview $mged_sketch_objscript}
	button .sketch.oanim.f6.b2 -text "Cancel" -command {destroy .sketch.oanim}

	label .sketch.oanim.l1 -text "Orientation Control: "
	radiobutton .sketch.oanim.rb0 -text "No Rotation" \
		-variable mged_sketch_objopt -value "none" -command "sketch_script_update $mode"
	radiobutton .sketch.oanim.rb1 -text "Automatic steering" \
		-variable mged_sketch_objopt -value "steer" -command "sketch_script_update $mode"
	radiobutton .sketch.oanim.rb2 -text "Automatic steering and banking" \
		-variable mged_sketch_objopt -value "bank" -command "sketch_script_update $mode"
	frame .sketch.oanim.f7
	label .sketch.oanim.f7.l0 -text "    maximum bank angle ="
	entry .sketch.oanim.f7.e0 -textvariable mged_sketch_objmang -width 4
	radiobutton .sketch.oanim.rb3 -text "Rotation specified as ypr" \
		-variable mged_sketch_objopt -value "ypr" -command "sketch_script_update $mode"
	radiobutton .sketch.oanim.rb4 -text "Rotation specified as quat" \
		-variable mged_sketch_objopt -value "quat" -command "sketch_script_update $mode"
	radiobutton .sketch.oanim.rb5 -text "Eye-path and look-at path " \
		-variable mged_sketch_objopt -value "lookat" -command "sketch_script_update $mode"
	frame .sketch.oanim.f8
	label .sketch.oanim.f8.l0 -textvariable mged_sketch_objncols
	label .sketch.oanim.f8.l1 -text "input columns needed:"
	label .sketch.oanim.f8.l2 -textvariable mged_sketch_objcols


	pack	.sketch.oanim.l0 .sketch.oanim.f0 .sketch.oanim.f1 \
		-side top -fill x -expand yes
	pack 	.sketch.oanim.f8 -side top
	eval pack	.sketch.oanim.f2 .sketch.oanim.f3 \
		.sketch.oanim.f4 ${if_view} .sketch.oanim.f5 \
		-side top -fill x -expand yes




	if {$mode == "view"} {
		pack .sketch.oanim.cb0 .sketch.oanim.cb3 -side top -anchor w

		pack .sketch.oanim.f9.b0 .sketch.oanim.f10.b0 \
			-side left -anchor w
		pack .sketch.oanim.f9.e0 .sketch.oanim.f10.e0 \
			-side right -anchor e
	} else {
		pack .sketch.oanim.cb1 .sketch.oanim.cb2 .sketch.oanim.cb3 \
			-side top -anchor w
	}

	pack 	.sketch.oanim.l1 -side top -anchor w

	pack 	.sketch.oanim.rb0 .sketch.oanim.rb1 .sketch.oanim.rb2 \
		-side top -anchor w
	pack	.sketch.oanim.f7 -side top -anchor e
	pack	.sketch.oanim.rb3 .sketch.oanim.rb4 .sketch.oanim.rb5 \
		-side top -anchor w
	pack 	.sketch.oanim.f6 -side top -fill x -expand yes


	pack \
		.sketch.oanim.f0.l0 .sketch.oanim.f1.l0 .sketch.oanim.f1.om0\
		.sketch.oanim.f2.l0 .sketch.oanim.f3.b0 \
		.sketch.oanim.f4.b0 .sketch.oanim.f5.l0 \
		.sketch.oanim.f8.l0 .sketch.oanim.f8.l1 .sketch.oanim.f8.l2\
		-side left -anchor w

	pack \
		.sketch.oanim.f0.e0 .sketch.oanim.f1.e0 .sketch.oanim.f2.e0\
		.sketch.oanim.f3.e0 .sketch.oanim.f4.e0 .sketch.oanim.f5.e0\
		-side right -anchor e

	pack \
		.sketch.oanim.f6.b0 .sketch.oanim.f6.b1 .sketch.oanim.f6.b2 \
		-side left -fill x -expand yes

	pack \
		.sketch.oanim.f7.e0 .sketch.oanim.f7.l0 \
		-side right

	focus .sketch.oanim.f0.e0
	bind .sketch.oanim.f0.e0 <Key-Return> {focus .sketch.oanim.f1.e0}
	bind .sketch.oanim.f1.e0 <Key-Return> {focus .sketch.oanim.f2.e0}
	bind .sketch.oanim.f2.e0 <Key-Return> {focus .sketch.oanim.f3.e0}
	bind .sketch.oanim.f3.e0 <Key-Return> {focus .sketch.oanim.f4.e0}
	if { $mode == "view" } {
		bind .sketch.oanim.f4.e0 <Key-Return> {focus .sketch.oanim.f9.e0}
		bind .sketch.oanim.f9.e0 <Key-Return> {focus .sketch.oanim.f10.e0}
		bind .sketch.oanim.f10.e0 <Key-Return> {focus .sketch.oanim.f5.e0}
	} else {
		bind .sketch.oanim.f4.e0 <Key-Return> {focus .sketch.oanim.f5.e0}
	}
	bind .sketch.oanim.f5.e0 <Key-Return> {.sketch.oanim.f6.b0 invoke}

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
		mged_sketch_temp1 mged_sketch_temp2 mged_sketch_anim_dir


	upvar #0 mged_sketch_objsrctype type
	upvar #0 mged_sketch_objsource src
	upvar #0 mged_sketch_objncols ncols

	#test for valid source
	switch $type {
	"spline:" {
		set type "curve:"
	}
	"curve:" {
		if {$ncols != 4} {
			tk_dialog .sketch.msg {Wrong number of columns} \
			"The animation you requested requires $ncols \
			 input columns. A curve provides 4." {} 0 "OK"
			return
		}
		set oldcurve [vdraw r n]
		vdraw s
		set ret [sketch_open_curve $src]
		if {$ret != 0} {
			tk_dialog .sketch.msg {Couldn't find curve} \
			  "Couldn't find curve $src." \
			  {} 0 {OK}
			sketch_open_curve $oldcurve
			return
		}
	} 
	"editor:" {
		set w ""
		foreach ed [info commands .sketch.text*.t] {
			if { $ed == ".sketch.text$src.t"} {
				set w ".sketch.text$src.t"
				break
			}
		}
		if { $w == "" } {
			tk_dialog .sketch.msg {Couldn't find editor} \
			  "Couldn't find table editor $src. \
			   (Text editor identifier must be an integer)." \
			  {} 0 {OK}
			return
		}
		set nsrc [llength [split [$w get 1.0 "1.0 lineend"] \t]]
		incr nsrc -1
		if { $nsrc > $ncols } {
			set ans [tk_dialog .sketch.msg {Excess columns} \
			"The animation you requested only uses $ncols \
			input columns. Text editor $src has $nsrc columns. \
			Only the first $ncols columns will be used." \
			{} 0 {OK} {Cancel}]
			if { $ans } return
			set colsp "0-[expr $ncols-1]"
		} elseif { $nsrc < $ncols } {
			tk_dialog .sketch.msg {Insufficient columns} \
			"The animation you requested requires $ncols \
			input columns. Text editor $src has only $nsrc." \
			{} 0 "OK"
			return
		} else {
			set colsp all
		}
	}
	"file:" {
		set fd [open $src r]
		gets $fd line
		close $fd
		set line "\t$line\t"
		set tab "\t"
		regsub -all "\[$tab \]+" $line "\t" res
		set nsrc [llength [split $res \t]] 
		incr nsrc -2
		if { $nsrc > $ncols } {
			set ans [tk_dialog .sketch.msg {Excess columns} \
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
			set filecmd "${mged_sketch_anim_dir}chan_permute -i $src $incol -o stdout $outcol"
		} elseif { $nsrc < $ncols } {
			tk_dialog .sketch.msg {Insufficient columns} \
			"The animation you requested requires $ncols \
			input columns. File $src has only $nsrc." \
			{} 0 "OK"
			return
		} else {
			set filecmd ""
		}
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
			set fd [open "| ${mged_sketch_anim_dir}anim_cascade \
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
		set anim_lookat ${mged_sketch_anim_dir}anim_lookat
		if { $type == "curve:" } {
			#This shouldn't happen
			puts "sketch_objanim: Can't do lookat orientation \
				from curve."
			return
		} elseif { $type == "editor:" } {
			set fd [open "| $anim_lookat -y $lookat_v | \
			  ${mged_sketch_anim_dir}anim_script $opts $ovname > \
			  $mged_sketch_objscript" w]
			sketch_text_to_fd $w $fd $colsp
			catch {close $fd}
			return
		} elseif { $type == "file:" } {
			if { $filecmd == "" } {
				catch {eval exec $anim_lookat -y $lookat_v < $src | \
				  ${mged_sketch_anim_dir}anim_script $opts $ovname > \
				  $mged_sketch_objscript}
			} else {
				catch {eval exec $filecmd | $anim_lookat -y $lookat_v | \
				  ${mged_sketch_anim_dir}anim_script $opts $ovname > \
				  $mged_sketch_objscript}
			}

			return
		}
		return

	}

	if { $mged_sketch_objopt == "bank" } {
		if { $mged_sketch_objmang > 89 } {
			set mged_sketch_objmang 89
		}
		set do_bank ${mged_sketch_anim_dir}anim_fly
		if { $type == "curve:" } {
			set sfile $mged_sketch_temp1
			set fd [open $sfile w]
			sketch_write_to_fd $fd [vdraw r l]
			close $fd
		} elseif { $type == "editor:" } {
			set sfile $mged_sketch_temp1
			set fd [open $sfile w]
			sketch_text_to_fd $w $fd "0,1,2,3"
			close $fd
		} elseif { $type == "file:"} {
			if { $filecmd == ""} {
				set sfile $src
			} else {
				set sfile $mged_sketch_temp1
				exec $filecmd > $sfile
			}
		}

		set factor [exec $do_bank -b $mged_sketch_objmang < $sfile]
		eval exec $do_bank -f $factor < $sfile \
			| ${mged_sketch_anim_dir}anim_script $opts $ovname > $mged_sketch_objscript
			
		if { $type == "curve:" } {
			sketch_open_curve $oldcurve
			exec rm $sfile
		} elseif { $type == "editor:" } {
			exec rm $sfile
		} elseif { $type == "file:"} {
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
	if { $type == "file:"} {
		puts "filecmd = $filecmd src = $src"
		if { $filecmd == "" } {
			eval exec ${mged_sketch_anim_dir}anim_script $opts $ovname < $src > \
			  $mged_sketch_objscript
		} else {
			eval exec $filecmd | ${mged_sketch_anim_dir}anim_script $opts $ovname | \
			  $mged_sketch_objscript
		}
	} elseif { $type == "curve:" } {
		set fd [open \
		     [concat | ${mged_sketch_anim_dir}anim_script $opts $ovname > \
		     $mged_sketch_objscript] w ]
		sketch_write_to_fd $fd [vdraw r l]
		close $fd
		sketch_open_curve $oldcurve
	} elseif { $type == "editor:" } {
		set fd [open \
		     [concat | ${mged_sketch_anim_dir}anim_script $opts $ovname > \
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
		set base "t ex ey ez lx ly lz"
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
#control creation of animation scripts
proc sketch_popup_track_anim { } {

	if { [info commands .sketch.tanim] != ""} {
		raise .sketch.tanim
		return
	}
	toplevel .sketch.tanim

	wm title .sketch.tanim "MGED Track Animation"
	label .sketch.tanim.l0 -text "CREATE TRACK ANIMATION"
	frame .sketch.tanim.f0
	label .sketch.tanim.f0.l0 -text "Output file: "
	entry .sketch.tanim.f0.e0 -width 20 -textvariable mged_sketch_objscript
	frame .sketch.tanim.f1
	label .sketch.tanim.f1.l0 -text "Vehicle path curve: "
	entry .sketch.tanim.f1.e0 -width 20 -textvariable mged_sketch_objsource
	frame .sketch.tanim.f2
	label .sketch.tanim.f2.l0 -text "Wheel curve: "
	entry .sketch.tanim.f2.e0 -width 20 -textvariable mged_sketch_whlsource
	frame .sketch.tanim.f3
	label .sketch.tanim.f3.l0 -text "Radii of wheels (or common radius):"
	entry .sketch.tanim.f3.e0 -textvariable mged_sketch_radii
	frame .sketch.tanim.f4
	label .sketch.tanim.f4.l0 -text "Link path name:"
	entry .sketch.tanim.f4.e0 -width 20 -textvariable mged_sketch_lnkname
	frame .sketch.tanim.f5
	label .sketch.tanim.f5.l0 -text "Number of links: "
	entry .sketch.tanim.f5.e0 -textvariable mged_sketch_numlinks
	frame .sketch.tanim.f6
	button .sketch.tanim.f6.b0 -text "Vehicle center:" \
			-command { set mged_sketch_objcen [vget center] }
	entry .sketch.tanim.f6.e0 -width 20 -textvariable mged_sketch_objcen
	frame .sketch.tanim.f7
	button .sketch.tanim.f7.b0 -text "Vehicle yaw,pitch,roll: " \
			-command { set mged_sketch_objori [vget ypr] }
	entry .sketch.tanim.f7.e0 -width 20 -textvariable mged_sketch_objori
	frame .sketch.tanim.f8
	label .sketch.tanim.f8.l0 -text "First frame:"
	entry .sketch.tanim.f8.e0 -width 20 -textvariable mged_sketch_objframe
	frame .sketch.tanim.f9
	button .sketch.tanim.f9.b0 -text "Create Script" -command {sketch_do_track $mged_sketch_objscript $mged_sketch_whlsource $mged_sketch_objsource $mged_sketch_objori $mged_sketch_objcen $mged_sketch_radii $mged_sketch_numlinks $mged_sketch_lnkname}
	button .sketch.tanim.f9.b1 -text "Show Script" -command {sketch_popup_preview $mged_sketch_objscript}
	button .sketch.tanim.f9.b2 -text "Cancel" -command {destroy .sketch.tanim}
	

	pack	.sketch.tanim.l0 .sketch.tanim.f0 .sketch.tanim.f1 \
		.sketch.tanim.f2 .sketch.tanim.f3 \
		.sketch.tanim.f4 .sketch.tanim.f5 \
		.sketch.tanim.f6 .sketch.tanim.f7 \
		.sketch.tanim.f8 .sketch.tanim.f9\
		-side top -fill x -expand yes


	pack \
		.sketch.tanim.f0.l0 .sketch.tanim.f1.l0 \
		.sketch.tanim.f2.l0 .sketch.tanim.f3.l0 \
		.sketch.tanim.f4.l0 .sketch.tanim.f5.l0 \
		.sketch.tanim.f6.b0 .sketch.tanim.f7.b0 \
		.sketch.tanim.f8.l0 \
		-side left -anchor w

	pack \
		.sketch.tanim.f0.e0 .sketch.tanim.f1.e0 .sketch.tanim.f2.e0\
		.sketch.tanim.f3.e0 .sketch.tanim.f4.e0 .sketch.tanim.f5.e0\
		.sketch.tanim.f6.e0 .sketch.tanim.f7.e0 .sketch.tanim.f8.e0 \
		-side right -anchor e

	pack \
		.sketch.tanim.f9.b0 .sketch.tanim.f9.b1 .sketch.tanim.f9.b2 \
		-side left -fill x -expand yes


	focus .sketch.tanim.f0.e0
	bind .sketch.tanim.f0.e0 <Key-Return> {focus .sketch.tanim.f1.e0}
	bind .sketch.tanim.f1.e0 <Key-Return> {focus .sketch.tanim.f2.e0}
	bind .sketch.tanim.f2.e0 <Key-Return> {focus .sketch.tanim.f3.e0}
	bind .sketch.tanim.f3.e0 <Key-Return> {focus .sketch.tanim.f4.e0}
	bind .sketch.tanim.f4.e0 <Key-Return> {focus .sketch.tanim.f5.e0}
	bind .sketch.tanim.f5.e0 <Key-Return> {focus .sketch.tanim.f6.e0}
	bind .sketch.tanim.f6.e0 <Key-Return> {focus .sketch.tanim.f7.e0}
	bind .sketch.tanim.f7.e0 <Key-Return> {focus .sketch.tanim.f8.e0}
	bind .sketch.tanim.f8.e0 <Key-Return> {.sketch.tanim.f9.b0 invoke}


}


proc sketch_do_track { outfile wcurve tcurve ypr center radius numlinks \
								linkname} {
	global mged_sketch_temp1 mged_sketch_anim_dir


	set oldname [vdraw r n]
	if {[sketch_open_curve $wcurve] < 0} {
		sketch_open_curve $oldname
		return -1
	}
	set numwheels [vdraw r l]
	if { $numwheels < 2 } {
		tk_dialog .sketch.msg "Not enough wheels" "The curve $wcurve \
		has only $numwheels point(s). You must specify the position \
		of at least 2 wheels." {} 0 "OK"
		return -1
	}
	set fd [open $mged_sketch_temp1 w]
	puts $fd "$numwheels $numlinks"
	set rad 1
	for {set i 0} { $i < $numwheels} { incr i} {
		if {[llength $radius] > $i} {
			set rad [lindex $radius $i]
		}
		set node [vdraw r $i]
		puts $fd [list [lindex $node 1] [lindex $node 2] \
			[lindex $node 3] $rad]
	}
	close $fd
	sketch_open_curve $tcurve
	set len [vdraw r l]
	if { $len < 3 } {
		tk_dialog .sketch.msg "Curve too short"	"The curve $tcurve \
		has only $len point(s). At least 3 are required." {} 0 "OK"
		return -1
	}
	while { [llength $ypr] < 3 } { lappend ypr 0}
	while { [llength $center] < 3 } { lappend ypr 0}
	set fd [ open "| ${mged_sketch_anim_dir}anim_hardtrack -s -b $ypr \
		-d $center $linkname $mged_sketch_temp1 > $outfile" w]
	sketch_write_to_fd $fd $len
	close $fd
	sketch_open_curve $oldname
	exec rm $mged_sketch_temp1
}


#-----------------------------------------------------------------
# Combine Scripts
#-----------------------------------------------------------------
proc sketch_popup_sort {} {
	
	if { [info commands .sketch.sort] != ""} {
		raise .sketch.sort
		return
	}
	toplevel .sketch.sort
	label .sketch.sort.l0 -text "COMBINE SCRIPTS"
	frame .sketch.sort.f0
	label .sketch.sort.f0.l0 -text "Combine Scripts:"
	entry .sketch.sort.f0.e0 -width 20 
	frame .sketch.sort.f0.f0
	listbox .sketch.sort.f0.f0.lb0 -height 6 -width 20 \
		-yscrollcommand ".sketch.sort.f0.f0.s0 set"
	scrollbar .sketch.sort.f0.f0.s0 -command ".sketch.sort.f0.f0.lb0 yview"
	frame .sketch.sort.f1
	button .sketch.sort.f1.b0 -text "Filter:" -command { \
	  sketch_list_filter .sketch.sort.f1.f1.lb1 [.sketch.sort.f1.e1 get ] }
	entry .sketch.sort.f1.e1 -width 20
	frame .sketch.sort.f1.f1
	listbox .sketch.sort.f1.f1.lb1 -height 6 -width 20 \
		-yscrollcommand ".sketch.sort.f1.f1.s0 set"
	scrollbar .sketch.sort.f1.f1.s0 -command ".sketch.sort.f1.f1.lb1 yview"
	frame .sketch.sort.f2
	label .sketch.sort.f2.l0 -text "Create script: "
	entry .sketch.sort.f2.e0 -width 20
	button .sketch.sort.f2.b0 -text "OK" -command {sketch_sort [.sketch.sort.f2.e0 get] .sketch.sort.f0.f0.lb0; .sketch.sort.f1.b0 invoke}
	button .sketch.sort.f2.b1 -text "Cancel" -command "destroy .sketch.sort"

	bind .sketch.sort.f0.e0 <Key-Return> { sketch_sort_entry1 .sketch.sort.f0.e0 .sketch.sort.f0.f0.lb0 .sketch.sort.f2.e0 }
	bind .sketch.sort.f1.e1 <Key-Return> { .sketch.sort.f1.b0 invoke }
	bind .sketch.sort.f0.f0.lb0 <Button-1> { sketch_list_remove_y .sketch.sort.f0.f0.lb0 %y }
	bind .sketch.sort.f1.f1.lb1 <Button-1> { sketch_list_add_y .sketch.sort.f1.f1.lb1 .sketch.sort.f0.f0.lb0 %y }
	bind .sketch.sort.f2.e0 <Key-Return> { .sketch.sort.f2.b0 invoke }

	.sketch.sort.f2.e0 insert end ".script"
	.sketch.sort.f2.e0 selection range 0 end
	.sketch.sort.f2.e0 icursor 0

	.sketch.sort.f1.e1 insert end "./*.script"
	sketch_list_filter .sketch.sort.f1.f1.lb1 "./*.script"

	pack .sketch.sort.f2 -side bottom
	pack .sketch.sort.l0 -side top
	pack .sketch.sort.f0 -side left
	pack .sketch.sort.f1 -side right

	pack .sketch.sort.f0.l0 .sketch.sort.f0.e0 .sketch.sort.f0.f0 \
		.sketch.sort.f1.b0 .sketch.sort.f1.e1 .sketch.sort.f1.f1 \
		-side top -anchor w
	pack .sketch.sort.f2.l0 .sketch.sort.f2.e0 .sketch.sort.f2.b0 \
	 	.sketch.sort.f2.b1 -side left
	pack .sketch.sort.f0.f0.lb0 .sketch.sort.f0.f0.s0 \
		.sketch.sort.f1.f1.lb1 .sketch.sort.f1.f1.s0 \
		-side left -fill y -expand yes

	focus .sketch.sort.f0.e0

}

proc sketch_sort_entry1 { entry list nentry } {
	if { [set line [$entry get]] == "" } {
		focus $nentry
	}
	$list insert end $line
	$entry delete 0 end
}

proc sketch_sort { outfile list } {
	global mged_sketch_anim_dir mged_sketch_sort_temp

	if { [info commands .sketch.sort.fa] != "" } {
		tk_dialog .sketch.sort.msg {Script already sorting} \
		  "The previous script is still being sorted" {} 0 "OK"
		return
	}
	append cmd "cat "
	foreach file [$list get 0 end] {
		append cmd " $file"
	}
	append cmd " > $mged_sketch_sort_temp"
	eval exec $cmd
	set pid [exec ${mged_sketch_anim_dir}anim_sort \
		< $mged_sketch_sort_temp > $outfile &]

	frame .sketch.sort.fa
	label .sketch.sort.fa.l0 -text "Sorting $outfile ..."
	button .sketch.sort.fa.b0 -text "Halt" -command "exec kill $pid"
	pack .sketch.sort.fa -side bottom -before .sketch.sort.f2
	pack .sketch.sort.fa.l0 .sketch.sort.fa.b0 -side left -fill x

	set done "destroy .sketch.sort.fa; exec rm $mged_sketch_sort_temp"
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
#control animation previews
proc sketch_popup_preview { {filename ""} } {

	if { [info commands .sketch.prev] != ""} {
		raise .sketch.prev
		.sketch.prev.f0.e0 delete 0 end
		.sketch.prev.f0.e0 insert end $filename
		.sketch.prev.f0.e0 selection range 0 end
		return
	}
	toplevel .sketch.prev
	label .sketch.prev.l0 -text "SHOW SCRIPT"
	frame .sketch.prev.f0
	label .sketch.prev.f0.l0 -text "Script file: "
	entry .sketch.prev.f0.e0 -width 20
	frame .sketch.prev.f1
	label .sketch.prev.f1.l0 -text "Max frames per second:"
	entry .sketch.prev.f1.e0 -width 5 \
		-textvariable mged_sketch_fps
	frame .sketch.prev.f2
	label .sketch.prev.f2.l0 -text "Start frame: "
	entry .sketch.prev.f2.e0 -width 5 -textvariable mged_sketch_prevs
	frame .sketch.prev.f3
	label .sketch.prev.f3.l0 -text "End frame: "
	entry .sketch.prev.f3.e0 -width 5 -textvariable mged_sketch_preve
	checkbutton .sketch.prev.cb0 -text "Polygon rendering" \
		-variable mged_sketch_prevp -onvalue "-v" -offvalue ""
	frame .sketch.prev.f4
	button .sketch.prev.f4.b0 -text "Show" \
		-command {sketch_preview [.sketch.prev.f0.e0 get]}
	button .sketch.prev.f4.b1 -text "Cancel" \
		-command { destroy .sketch.prev }

	.sketch.prev.f0.e0 delete 0 end
	.sketch.prev.f0.e0 insert 0 $filename
	.sketch.prev.f0.e0 selection range 0 end

	pack .sketch.prev.l0 \
		.sketch.prev.f0 .sketch.prev.f1 .sketch.prev.f2 \
		.sketch.prev.f3 .sketch.prev.cb0 .sketch.prev.f4 \
		-side top -expand yes -fill x -anchor w
	pack .sketch.prev.f0.l0 .sketch.prev.f0.e0 \
		.sketch.prev.f1.l0 .sketch.prev.f1.e0 \
		.sketch.prev.f2.l0 .sketch.prev.f2.e0 \
		.sketch.prev.f3.l0 .sketch.prev.f3.e0 \
		.sketch.prev.f4.b0 .sketch.prev.f4.b1 \
		-side left -expand yes

	focus .sketch.prev.f0.e0
	bind .sketch.prev.f0.e0 <Key-Return> {focus .sketch.prev.f1.e0}
	bind .sketch.prev.f1.e0 <Key-Return> {focus .sketch.prev.f2.e0}
	bind .sketch.prev.f2.e0 <Key-Return> {focus .sketch.prev.f3.e0}
	bind .sketch.prev.f3.e0 <Key-Return> {.sketch.prev.f4.b0 invoke}
}

#preview an animation script
proc sketch_preview { filename } {
	upvar #0 mged_sketch_fps fps
	upvar #0 mged_sketch_prevp arg0
	global mged_sketch_prevs mged_sketch_preve

	#save list of curves currently displayed
	set clist ""
	set vlist [mged_glob "dummy_cmd _VDRW*"]
	foreach name [lrange $vlist 1 end] {
		lappend clist [string range $name 5 end]
	}

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
	if [vdraw o] {
		set oldname [vdraw r n]
	} else {
		set oldname ""
	}
	foreach curve $clist {
		vdraw o $curve
		vdraw s
	}
	if {$oldname != "" } {
		sketch_open_curve $oldname
	}
}


	
#-----------------------------------------------------------------
# Quit
#-----------------------------------------------------------------
proc sketch_quit {} {
	destroy .sketch
	foreach var [info globals mged_sketch*] {
		#uplevel #0 "unset $var"
	}
	kill -f _VDRW_sketch_hl_
	# anything else?
}

#-----------------------------------------------------------------
# Other Procedures
#-----------------------------------------------------------------
# write "length" curve nodes to given file descriptor
proc sketch_write_to_fd { fd length} {
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	for { set i 0} { $i < $length} {incr i} {
		puts $fd [concat \
			[lindex $tlist $i] \
			[lrange [vdraw r $i] 1 3] ]
	}
}	

# read curve nodes from file, return number appended
proc sketch_append_from_fd { fd } {
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	set line {}
	set tlist {}
	if { [gets $fd line] >= 0 } {
		lappend tlist [lindex $line 0]
		vdraw w n 0 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		set num_added 1
	} else {
		tk_dialog .sketch.msg {Empty file} \
			{The file you loaded was empty.} {} \
			 0 {OK}
		return 0
	}
	while { [gets $fd line] >= 0 } {
		lappend tlist [lindex $line 0]
		vdraw w n 1 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		incr num_added
	}
	return $num_added
}

#read current curve into end of table editor
proc sketch_text_echoc { w } {
	set length [vdraw r l]
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	for {set i 0} {$i < $length} {incr i} {
		set temp [vdraw r $i]
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
	upvar #0 [format "mged_time_%s" [vdraw r n]] tlist
	if {$mode == "replace"} {
		vdraw d a
		set tlist {}
	}
	#otherwise append to existing
	if { [set line [$w get 1.0 "1.0 lineend"]] != ""} {
		lappend tlist [lindex $line 0]
		vdraw w n 0 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		set num_added 2
	} else {
		tk_dialog .sketch.msg {Empty text} \
			{The text you loaded was empty.} {} \
			 0 {OK}
		return 0
	}
	while { [set line [$w get $num_added.0 "$num_added.0 lineend"]] != ""} {
		lappend tlist [lindex $line 0]
		vdraw w n 1 [lindex $line 1] [lindex $line 2] \
				[lindex $line 3]
		incr num_added
	}
	sketch_update
	return [expr $num_added - 1]
}

#save the specified columns of the text to a file
proc sketch_text_writef { w filename col } {
	if {[file exists $filename] } {
		set ans [tk_dialog .sketch.msg {File Exists} \
			{File already exists.} {} 1 {Overwrite} {Cancel} ]
		if { $ans == 1} {
			return
		}
	}

	set fd [open $filename w]
	sketch_text_to_fd $w $fd $col
	close $fd
	catch {	destroy .sketch.input}
}

proc sketch_text_readf { w filename col mode } {
	set fd [open $filename r]
	sketch_text_from_fd $w $fd $col $mode
	close $fd
	sketch_text_bar_reset $w
	catch {destroy .sketch.input}
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
		sketch_parse_col $col [llength [$w get 1.0 "1.0 lineend"]] \
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
	set length [vdraw r l]
	puts "Solid name is [vdraw r n]"
	for {set i 0} { $i < $length} { incr i} {
		puts [vdraw r $i]
	}
	
}

proc sketch_popup_input {title entries buttons} {
	catch {destroy .sketch.input}
	toplevel .sketch.input
	if {$title != ""} { wm title .sketch.input $title }
	set max 0
	foreach pair $entries {
		set len [string length [lindex $pair 0]]
		if { $len > $max} {
			set max $len
		}
	}
	set i 0
	foreach pair $entries {
		frame .sketch.input.f$i
		pack .sketch.input.f$i -side top -expand yes -anchor w -fill x
		set mylabel [lindex $pair 0]
		set k [string length $mylabel] 
		label .sketch.input.f$i.l -text $mylabel -width $max -anchor e
		entry .sketch.input.f$i.e -width 20
		if { $i > 0 } {
			bind  .sketch.input.f[expr $i-1].e <Key-Return> \
			  "focus .sketch.input.f$i.e; \
			   .sketch.input.f$i.e selection range 0 end"
		}
		.sketch.input.f$i.e insert end [lindex $pair 1]
		pack .sketch.input.f$i.l .sketch.input.f$i.e \
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
			frame .sketch.input.f$j 
			pack .sketch.input.f$j -side top -anchor e
		}
		button .sketch.input.f$j.b -text [lindex $pair 0] \
			-command [lindex $pair 1] -width $max
		pack .sketch.input.f$j.b -side right
		incr j
	}
	if { $j > $i } { 
		set max $j; set min $i } else { 
		set max $i; set min $j }
	if { $max < 1 } { 
		destroy .sketch.input 
		return -1
	} 
	if { $min < 1} {
		return 0
	}
	.sketch.input.f0.e selection range 0 end
	focus .sketch.input.f0.e
	bind .sketch.input.f[expr $i-1].e <Key-Return> \
			{.sketch.input.f0.b invoke}
	return 0
}

#transfer columns from one text widget to another
#columns is any string accepted by sketch_parse_col
#mode is one of replace, append, left, right
#rows optionally specifies maximum number of rows
proc sketch_text_from_text { win wout col mode {rows all}} {
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
	set i [llength [split [$win get 1.0 "1.0 lineend"] "\t"]]
	sketch_parse_col $col [expr $i - 1] colarray
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
		set myw $w._scratch_
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

	sketch_text_from_text $myw $w $col $mode
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


	
#-------------------------------------------------------------------
# Go
#-------------------------------------------------------------------
# Uncomment the following command in order to run the animator 
# automatically when anim.tcl is sourced.

#sketch_main_window
