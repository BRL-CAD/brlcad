# vmath.tcl
# Simple interpreted functions for doing vmath stuff (see ../h/vmath.h for the
# C preprocessor equivalents)

proc mat_idn { } {
    return { 1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1 }
}

proc vreverse { v } {
    return [list [expr -1.0*[lindex $v 0]] \
	         [expr -1.0*[lindex $v 1]] \
		 [expr -1.0*[lindex $v 2]]]
}

proc hreverse { h } {
    return [list [expr -1.0*[lindex $h 0]] \
	         [expr -1.0*[lindex $h 1]] \
		 [expr -1.0*[lindex $h 2]] \
	         [expr -1.0*[lindex $h 3]]]
}

proc vadd { u v } {
    return [list [expr [lindex $u 0]+[lindex $v 0]] \
	         [expr [lindex $u 1]+[lindex $v 1]] \
	         [expr [lindex $u 2]+[lindex $v 2]]]
}

proc vadd2 { u v } { return vadd $u $v }

proc vsub { u v } {
    return [list [expr [lindex $u 0]-[lindex $v 0]] \
	         [expr [lindex $u 1]-[lindex $v 1]] \
	         [expr [lindex $u 2]-[lindex $v 2]]]
}

proc vsub2 { u v } { return vsub $u $v }

proc vscale { v c } {
    return [list [expr [lindex $v 0]*$c] \
	         [expr [lindex $v 1]*$c] \
	         [expr [lindex $v 2]*$c]]
}
    
proc hscale { h c } {
    return [list [expr [lindex $h 0]*$c] \
	         [expr [lindex $h 1]*$c] \
	         [expr [lindex $h 2]*$c] \
	         [expr [lindex $h 3]*$c]]
}

proc vunitize { v } {
    set _f [expr 1.0/[magnitude $v]]
    return [vscale $v $_f]
}

proc magsq { v } {
    return [expr [lindex $v 0]*[lindex $v 0] + \
	         [lindex $v 1]*[lindex $v 1] + \
		 [lindex $v 2]*[lindex $v 2]]
}

proc magnitude { v } {
    return [expr sqrt([magsq $v])]
}

proc vcross { u v } {
  return [list [expr [lindex $u 1]*[lindex $v 2]-[lindex $u 2]*[lindex $v 1]] \
               [expr [lindex $u 2]*[lindex $v 0]-[lindex $u 0]*[lindex $v 2]] \
	       [expr [lindex $u 0]*[lindex $v 1]-[lindex $u 1]*[lindex $v 0]]]
}

proc vdot { u v } {
    return [expr [lindex $u 0]*[lindex $v 0] + \
	         [lindex $u 1]*[lindex $v 1] + \
		 [lindex $u 2]*[lindex $v 2]]
}

proc mat3x3vec { m v } {
    return { }
}

proc mat4x3pnt { m p } {
    return { }
}

proc mat4x4pnt { m p } {
    return { }
}

proc vequal { u v } {
    return [expr [lindex $u 0]==[lindex $v 0] && \
	         [lindex $u 1]==[lindex $v 1] && \
		 [lindex $u 2]==[lindex $v 2]]
}

proc vapproxequal { u v tol } {
    return [near_zero [vsub $u $v] $tol]
}

proc near_zero { v tol } {
    return [expr abs([lindex $v 0])<$tol && \
	         abs([lindex $v 1])<$tol && \
		 abs([lindex $v 2])<$tol]
}
