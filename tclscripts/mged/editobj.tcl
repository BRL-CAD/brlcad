# editobj
#
# Command for editing ellipsoids.

set eoname(V) "Vertex"
set eoname(A) "A vector"
set eoname(B) "B vector"
set eoname(C) "C vector"
set eoname(H) "H vector"
set eoname(r) "Radius"
set eoname(r_a) "Radius a"
set eoname(r_h) "Radius h"

proc editobj { oname } {
    set form [db form $oname]
    set vals [db get $oname]
    set objtype [lindex $vals 0]
    set len [llength $vals]
    global eofin$oname
    global eoname

    catch { destroy .eo$oname }
    toplevel .eo$oname
    wm title .eo$oname "Object editor: $oname"

    frame .eo$oname.l
    frame .eo$oname.r
    pack .eo$oname.l -side left
    pack .eo$oname.r -side left -fill x -expand yes

    set eofin$oname "db adjust $oname"
    set i 1
    while { $i<$len } {
	set attr [lindex $form $i]
	incr i
	frame .eo$oname.r.f$attr
	pack .eo$oname.r.f$attr -side top -fill x -expand yes
	set eofin$oname [eval concat \$eofin$oname $attr]

	set num 0
	while { 1 } {
	    set field [lindex $form $i]
	    if { [string compare $field "%f"]==0 } then {
		entry .eo$oname.r.f$attr.e$num -width 5 -relief sunken
		pack .eo$oname.r.f$attr.e$num -side left -fill x -expand yes
		.eo$oname.r.f$attr.e$num insert insert [lindex $vals $i]
		set eofin$oname [eval concat \$eofin$oname \
			\\\[.eo$oname.r.f$attr.e$num get\\\]]
		incr i
		incr num
	    } else {
		break
	    }
	}

	label .eo$oname.l.l$attr -text "$eoname($attr)" -anchor w
	pack .eo$oname.l.l$attr -side top -fill y -expand yes
    }

    frame .eo$oname.b
    pack .eo$oname.b -after .eo$oname.r -side bottom -fill x -expand yes
    button .eo$oname.b.ok -text "Ok" -command "eval eval \$eofin$oname"
    button .eo$oname.b.apply -text "Apply" -command "eval eval \$eofin$oname"
    button .eo$oname.b.cancel -text "Cancel" -command "destroy .eo$oname"
    pack .eo$oname.b.ok .eo$oname.b.apply .eo$oname.b.cancel -side left \
	    -fill x -expand yes

    echo [eval format \"Final eofin$oname: %s\" \$eofin$oname]
}
		
	