char *editobj_gui_str = "\
set eoname(V) \"Vertex\";\
set eoname(A) \"A vector\";\
set eoname(B) \"B vector\";\
set eoname(C) \"C vector\";\
set eoname(D) \"D vector\";\
set eoname(H) \"H vector\";\
set eoname(r) \"Radius\";\
set eoname(r_a) \"Radius a\";\
set eoname(r_h) \"Radius h\";\
\
proc editobj { id oname } {\
    global eofin$oname;\
    global eoname;\
    global player_screen;\
\
    set vals [db get $oname];\
    set form [db form [lindex $vals 0]];\
    set len [llength $form];\
\
    if { [winfo exists .eo$oname] } {\
	return \"Someone is already editing $oname\";\
    };\
\
    toplevel .eo$oname -screen $player_screen($id);\
    wm title .eo$oname \"Object editor: $oname\";\
\
    frame .eo$oname.t -borderwidth 2;\
    pack .eo$oname.t -side top -fill x -expand yes;\
\
    frame .eo$oname.t.l;\
    frame .eo$oname.t.r;\
    pack .eo$oname.t.l -side left -fill y -expand yes;\
    pack .eo$oname.t.r -side left -fill x -expand yes;\
\
    set eofin$oname \"db adjust $oname\";\
\
    for { set i 0 } { $i<$len } { incr i } {\
	set attr [lindex $form $i];\
	incr i;\
\
	frame .eo$oname.t.r.f$attr;\
	pack .eo$oname.t.r.f$attr -side top -fill x -expand yes;\
	set eofin$oname [eval concat \\[set eofin$oname\\] $attr \\\\\\\"];\
\
	set field [lindex $form $i];\
	set fieldlen [llength $field];\
	for { set num 0 } { $num<$fieldlen } { incr num } {\
	    if { [string first \"%f\" $field]>-1 } {\
		button .eo$oname.t.r.f$attr.dec$num -text \- -command \
			\"eodec $oname .eo$oname.t.r.f$attr.e$num\";\
		button .eo$oname.t.r.f$attr.inc$num -text \+ -command \
			\"eoinc $oname .eo$oname.t.r.f$attr.e$num\";\
		entry .eo$oname.t.r.f$attr.e$num -width 6 -relief sunken;\
		pack .eo$oname.t.r.f$attr.dec$num -side left;\
		pack .eo$oname.t.r.f$attr.e$num -side left -fill x -expand yes;\
		pack .eo$oname.t.r.f$attr.inc$num -side left;\
		.eo$oname.t.r.f$attr.e$num insert insert \
			[lindex [lindex $vals [expr $i+1]] $num];\
		set eofin$oname [eval concat \\[set eofin$oname\\] \
			\\\\\\[.eo$oname.t.r.f$attr.e$num get\\\\\\]];\
		bind .eo$oname.t.r.f$attr.e$num <Key-Return> \"eoapply $oname\";\
	    };\
	};\
\
	set eofin$oname [eval concat \\[set eofin$oname\\] \\\\\\\"];\
\
	if { [catch { label .eo$oname.t.l.l$attr -text \"$eoname($attr)\" \
		-anchor w }]!=0 }  {\
	    label .eo$oname.t.l.l$attr -text \"$attr\" -anchor w;\
	};\
\
	pack .eo$oname.t.l.l$attr -side top -anchor w -fill y -expand yes;\
    };\
\
    frame .eo$oname.b -borderwidth 2;\
    pack .eo$oname.b -side top -fill x -expand yes;\
    button .eo$oname.b.apply -text \"Apply\" -command \"eoapply $oname\";\
    button .eo$oname.b.cancel -text \"Close\" -command \"destroy .eo$oname\";\
    pack .eo$oname.b.apply .eo$oname.b.cancel -side left -fill x -expand yes;\
};\
\
proc eoapply { oname } {\
    global eofin$oname;\
    global eoname;\
\
    eval [set eofin$oname];\
\
    set vals [db get $oname];\
    set form [db form [lindex $vals 0]];\
    set len [llength $form];\
\
    for { set i 0 } { $i<$len } { incr i } {\
	set attr [lindex $form $i];\
	incr i;\
\
	set field [lindex $form $i];\
	set fieldlen [llength $field];\
	for { set num 0 } { $num<$fieldlen } { incr num } {\
	    if { [string first \"%f\" $field]>-1 } {\
		.eo$oname.t.r.f$attr.e$num delete 0 end;\
		.eo$oname.t.r.f$attr.e$num insert insert \
			[lindex [lindex $vals [expr $i+1]] $num];\
	    };\
	};\
    };\
};\
\
proc eoinc { oname entryfield } {\
    set oldval [$entryfield get];\
    set increase [expr $oldval*0.1];\
    if { $increase<0.1 }  { set increase 0.1 };\
    $entryfield delete 0 end;\
    $entryfield insert insert [expr $oldval+$increase];\
    eoapply $oname;\
};\
\
proc eodec { oname entryfield } {\
    set oldval [$entryfield get];\
    set decrease [expr $oldval*0.1];\
    if { $decrease<0.1 } { set decrease 0.1 };\
    $entryfield delete 0 end;\
    $entryfield insert insert [expr $oldval-$decrease];\
    eoapply $oname;\
};\
";
