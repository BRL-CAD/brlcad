set solc(default,arb8) { V1 {1 -1 -1}  V2 {1 1 -1}  V3 {1 1 1}  V4 {1 -1 1} \
                         V5 {-1 -1 -1} V6 {-1 1 -1} V7 {-1 1 1} V8 {-1 -1 1} }
set solc(default,sph)  { V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1} }
set solc(default,ell)  { V {0 0 0} A {2 0 0} B {0 1 0} C {0 0 1} }
set solc(default,ellg) { V {0 0 0} A {4 0 0} B {0 2 0} C {0 0 1} }
set solc(default,tor)  { V {0 0 0} H {1 0 0} r_h 2 r_a 1 }
set solc(default,tgc)  { V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} \
	                 C {.5 0 0} D {0 1 0} }
set solc(default,rec)  { V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} \
	                 C {1 0 0} D {0 .5 0} }
set solc(default,half) { N {0 0 1} d -1 }
set solc(default,rpc)  { V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25 }
set solc(default,rhc)  { V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25 c .1 }
set solc(default,epa)  { V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25 }
set solc(default,ehy)  { V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25 \
	                 c .25 }
set solc(default,eto)  { V {-1 -1 -1} N {0 0 1} C {.1 0 .1} r .5 r_d .05 }
set solc(default,part) { V {-1 -1 -.5} H {0 0 1} r_v 0.5 r_h 0.25 }

set solc(winnum) 0
set solc(default,type) arb8
set solc(default,indexvar) index

set solc(label,V) "Vertex"
set solc(label,A) "A vector"
set solc(label,B) "B vector"
set solc(label,C) "C vector"
set solc(label,D) "D vector"
set solc(label,H) "H vector"
set solc(label,N) "Normal vector"
set solc(label,c) "c value"
set solc(label,d) "d value"
set solc(label,r) "Radius"
set solc(label,r_a) "Radius a"
set solc(label,r_v) "Radius v"
set solc(label,r_h) "Radius h"
set solc(label,r_1) "Radius 1"
set solc(label,r_2) "Radius 2"
set solc(label,r_d) "Radius d"

proc solcreate { } {
    global solc
    global $solc(default,indexvar)

    set w $solc(winnum)
    incr solc(winnum)

    catch { destroy .solc$w }
    toplevel .solc$w
    wm title .solc$w "Solid Creation"

    frame .solc$w.top
    frame .solc$w.top.left
    frame .solc$w.top.right

    pack .solc$w.top -side top -fill x -expand yes
    pack .solc$w.top.left -side left -fill y
    pack .solc$w.top.right -side left -fill x -expand yes

    label .solc$w.top.left.index -text "Index" -anchor w
    label .solc$w.top.left.indexvar -text "Index var" -anchor w
    label .solc$w.top.left.format -text "Format" -anchor w
    label .solc$w.top.left.type -text "Solid type" -anchor w

    pack .solc$w.top.left.index .solc$w.top.left.indexvar \
	    .solc$w.top.left.format .solc$w.top.left.type \
	    -side top -fill y -expand yes -anchor w

    if { [catch { set solc($w,index) [set $solc(default,indexvar)] }]!=0 } {
	set solc($w,index) 1
    }
    entry .solc$w.top.right.index -relief sunken -width 12 \
	    -textvariable solc($w,index)

    set solc($w,indexvar) $solc(default,indexvar)
    entry .solc$w.top.right.indexvar -relief sunken -width 12 \
	    -textvariable solc($w,indexvar)

    set solc($w,format) "my$solc(default,type).\$$solc(default,indexvar)"
    entry .solc$w.top.right.format -relief sunken -width 12 \
	    -textvariable solc($w,format)

    set solc($w,type) $solc(default,type)
    entry .solc$w.top.right.type -relief sunken -width 12 \
	    -textvariable solc($w,type)
    bind .solc$w.top.right.type <Key-Return> "solnewtype $w"

    pack .solc$w.top.right.index .solc$w.top.right.indexvar \
	    .solc$w.top.right.format .solc$w.top.right.index \
	    .solc$w.top.right.type  -side top -fill x -expand yes

    soldefaults .solc$w.def $solc(default,type)
    pack .solc$w.def -side top -fill x -expand yes

    frame .solc$w.bot
    pack .solc$w.bot -side top -fill x -expand yes

    button .solc$w.bot.quit -text "Quit" -command "solquit $w"
    button .solc$w.bot.create -text "Create" -command "soldoit $w"
    pack .solc$w.bot.quit .solc$w.bot.create -side left -fill x -expand yes

    button .solc$w.accept -text "Accept" -command "press accept" \
	    -state disabled -command "solaccept $w"
    pack .solc$w.accept -side top -fill x -expand yes
    
}

proc solaccept { w } {
    .solc$w.accept configure -state disabled
    .solc$w.bot.create configure -state normal
    press accept
}

proc solnewtype { w } {
    global solc
    
    soldefaults .solc$w.def $solc($w,type)
    pack .solc$w.def -after .solc$w.top -side top -fill x -expand yes
}

proc solquit { w } {
    global solc

    set solc(default,indexvar) $solc($w,indexvar)
    set solc(default,type) $solc($w,type)

    destroy .solc$w
}

proc soldoit { w } {
    global solc
    global $solc($w,indexvar)
    
    set $solc($w,indexvar) $solc($w,index)

    set objname [eval list $solc($w,format)]

    incr solc($w,index)
    set $solc($w,indexvar) $solc($w,index)
    set solc(default,indexvar) $solc($w,indexvar)
    set solc(default,type) $solc($w,type)

    set solc(default,$solc($w,type)) [eval list [set solc(do,.solc$w.def)]]

    eval db put $objname $solc($w,type) [set solc(do,.solc$w.def)]
    sed $objname
    press sxy
    .solc$w.accept configure -state normal
    .solc$w.bot.create configure -state disabled

}

proc soldefs { type } {
    global solc

    set retval {}
    catch { set retval $solc(default,[string tolower $type]) }

    return $retval
}

proc soldefaults { fr type } {
    global solc

    catch { destroy $fr }
    frame $fr
    
    if { [catch { db form $type }]!=0 } then {
	return
    }

    label $fr.header -text "Default values"
    pack $fr.header -side top -fill x -expand yes -anchor c
    
    set form [db form $type]
    set len [llength $form]

    set defs [soldefs $type]

    if { [llength $defs]<1 } then {
	return
    }

    frame $fr.l
    frame $fr.r
    pack $fr.l -side left -fill y
    pack $fr.r -side left -fill x -expand yes

    set solc(do,$fr) ""

    for { set i 0 } { $i<$len } { incr i } {
	set attr [lindex $form $i]
	incr i
	
	frame $fr.r.f$attr
	pack $fr.r.f$attr -side top -fill x -expand yes
	set solc(do,$fr) [eval concat \[set solc(do,$fr)\] $attr \\\"]

	set field [lindex $form $i]
	set fieldlen [llength $field]
	set defvals [lindex $defs [expr [lsearch $defs $attr]+1]]

	if { [string first "%f" $field]>-1 || [string first "%d" $field]>-1 } {
	    for { set num 0 } { $num<$fieldlen } { incr num } {
		entry $fr.r.f$attr.e$num -width 6 -relief sunken
		pack $fr.r.f$attr.e$num -side left -fill x -expand yes
		$fr.r.f$attr.e$num insert insert [lindex $defvals $num]
		set solc(do,$fr) [eval concat \[set solc(do,$fr)\] \
			\\\[$fr.r.f$attr.e$num get\\\]]
	    }
	}

	set solc(do,$fr) [eval concat \[set solc(do,$fr)\] \\\"]

	if { [catch { label $fr.l.l$attr -text "$solc(label,$attr)" \
		-anchor w}]!=0 } then {
	    label $fr.l.l$attr -text "$attr" -anchor w
	}

	pack $fr.l.l$attr -side top -anchor w -fill y -expand yes
    }
}












