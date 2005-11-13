#                       V M A T H . T C L
# BRL-CAD
#
# Copyright (C) 1995-2005 United States Government as represented by
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
#
# Authors -
#	Glenn Durfee
#	Carl Nuzman
#	Lee A. Butler
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
# 	Simple interpreted functions for doing vmath stuff (see ../h/vmath.h
#	for the C preprocessor equivalents and comments!  Much of this was
#	automatically generated.

# math constants from math.h
set M_E		2.7182818284590452354
set M_LOG2E	1.4426950408889634074
set M_LOG10E	0.43429448190325182765
set M_LN2	0.69314718055994530942
set M_LN10	2.30258509299404568402
set M_PI	3.14159265358979323846
set M_PI_2	1.57079632679489661923
set M_PI_4	0.78539816339744830962
set M_1_PI	0.31830988618379067154
set M_2_PI	0.63661977236758134308
set M_2_SQRTPI	1.12837916709551257390
set M_SQRT2	1.41421356237309504880
set M_SQRT1_2	0.70710678118654752440

proc init_vmath {} {
        # this routine does nothing except ensure that the abpve global variables get set
}

proc near_zero { val epsilon } {
    return [expr abs([lindex $val 0])<$epsilon && \
	         abs([lindex $val 1])<$epsilon && \
		 abs([lindex $val 2])<$epsilon]
}

proc dist_pt_plane { pt plane } {
    return [expr [vdot $pt $plane]-[lindex $plane 3]]
}

proc dist_pt_pt { a b } {
    return [magnitude [vsub2 $a $b]]
}

proc mat_deltas_get { m } {
    return [list [lindex $m 3] [lindex $m 7] [lindex $m 11]]
}

proc mat_zero { } {
    return { 0 0 0 0  0 0 0 0  0 0 0 0  0 0 0 0 }
}

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

proc vadd2 { u v } {
    return [list [expr [lindex $u 0]+[lindex $v 0]] \
	         [expr [lindex $u 1]+[lindex $v 1]] \
	         [expr [lindex $u 2]+[lindex $v 2]]]
}

proc vsub2 { u v } {
    return [list [expr [lindex $u 0]-[lindex $v 0]] \
	         [expr [lindex $u 1]-[lindex $v 1]] \
	         [expr [lindex $u 2]-[lindex $v 2]]]
}

proc hadd2 { g h } {
    return [list [expr [lindex $g 0]+[lindex $h 0]] \
	         [expr [lindex $g 1]+[lindex $h 1]] \
	         [expr [lindex $g 2]+[lindex $h 2]] \
	         [expr [lindex $g 3]+[lindex $h 3]]]
}

proc hsub2 { g h } {
    return [list [expr [lindex $g 0]-[lindex $h 0]] \
	         [expr [lindex $g 1]-[lindex $h 1]] \
	         [expr [lindex $g 2]-[lindex $h 2]] \
	         [expr [lindex $g 3]-[lindex $h 3]]]
}

proc vadd3 { u v w } {
    return [vadd2 $u [vadd2 $v $w]]
}

proc vsub3 { u v w } {
    return [vsub2 $u [vadd2 $v $w]]
}

proc vadd4 { u v w x } {
    return [vadd2 $u [vadd3 $v $w $x]]
}

proc vsub4 { u v w x } {
    return [vsub2 $u [vadd3 $v $w $x]]
}

proc vadd args {
    if { [llength $args]==0 } then {
	return "0 0 0"
    } else {
	return [vadd2 [lindex $args 0] \
		      [eval vadd [lrange $args 1 end]]]
    }
}

proc hadd args {
    if { [llength $args]==0 } then {
	return "0 0 0 0"
    } else {
	return [hadd2 [lindex $args 0] \
		      [eval hadd [lrange $args 1 end]]]
    }
}

proc vsub args {
    if { [llength $args]==0 } then {
	return "0 0 0"
    } else {
	return [vsub2 [lindex $args 0] \
		      [eval vadd [lrange $args 1 end]]]
    }
}

proc hsub args {
    if { [llength $args]==0 } then {
	return "0 0 0 0"
    } else {
	return [hsub2 [lindex $args 0] \
		      [eval hadd [lrange $args 1 end]]]
    }
}

proc v2add2 { x y } {
    return [list [expr [lindex $x 0]+[lindex $y 0]] \
	         [expr [lindex $x 1]+[lindex $y 1]]]
}

proc v2sub2 { x y } {
    return [list [expr [lindex $x 0]-[lindex $y 0]] \
	         [expr [lindex $x 1]-[lindex $y 1]]]
}

proc v2add args {
    if { [llength $args]==0 } then {
	return "0 0"
    } else {
	return [v2add2 [lindex $args 0] \
		       [eval vadd [lrange $args 1 end]]]
    }
}

proc v2sub args {
    if { [llength $args]==0 } then {
	return "0 0"
    } else {
	return [v2sub2 [lindex $args 0] \
		       [eval v2add [lrange $args 1 end]]]
    }
}

proc vscale { v c } {
    if { [llength $v] > 2 } then {
	return [list [expr [lindex $v 0]*$c] \
		     [expr [lindex $v 1]*$c] \
		     [expr [lindex $v 2]*$c]]
    } else {
	return [list [expr [lindex $c 0]*$v] \
		     [expr [lindex $c 0]*$v] \
		     [expr [lindex $c 0]*$v]]
    }
}

proc hscale { h c } {
    if { [llength $h] > 3 } then {
	return [list [expr [lindex $h 0]*$c] \
		     [expr [lindex $h 1]*$c] \
		     [expr [lindex $h 2]*$c] \
		     [expr [lindex $h 3]*$c]]
    } else {
	return [list [expr [lindex $c 0]*$h] \
		     [expr [lindex $c 1]*$h] \
		     [expr [lindex $c 2]*$h] \
		     [expr [lindex $c 3]*$h]]
    }
}

proc v2scale { x c } {
    if { [llength $x] > 1 } then {
	return [list [expr [lindex $x 0]*$c] \
		     [expr [lindex $x 1]*$c]]
    } else {
	return [list [expr [lindex $c 0]*$x] \
		     [expr [lindex $c 0]*$x]]
    }
}

proc vadd2scale { a b s } {
    return [vscale [vadd2 $a $b] $s]
}

proc vsub2scale { a b s } {
    return [vscale [vsub2 $a $b] $s]
}

proc vcomb2 { a b  c d } {
    return [vadd2 [vscale $b $a] [vscale $d $c]]
}

proc vcomb3 { a b  c d  e f } {
    return [vadd3 [vscale $b $a] [vscale $d $c] [vscale $f $e]]
}

proc vjoin1 { b  c d } {
    return [vcomb2 1 $b $c $d]
}

proc vjoin2 { b  c d  e f } {
    return [vcomb3 1 $b $c $d $e $f]
}

proc vjoin3 { b  c d  e f  g h } {
	return [vadd2 $b [vcomb3 $c $d $e $f $g $h]]
}

proc vjoin4 { b  c d  e f  g h  i j } {
	return [vadd3 $b [vcomb3 $c $d $e $f $g $h] [vscale $j $i]]
}


proc hjoin1 { b  c d } {
    return [hadd2 $b [hscale $d $c]]
}

proc v2join1 { b  c d } {
    return [v2add2 $b [v2scale $d $c]]
}

proc vblend2 { b c  d e } {
    return [vcomb2 $c $b $e $d]
}

proc vunitize { v } {
    return [vscale $v [expr 1.0/[magnitude $v]]]
}

proc magsq { v } {
    return [vdot $v $v]
}

proc vmagsq { v } {
    return [magsq $v]
}

proc magnitude { v } {
    return [expr sqrt([vmagsq $v])]
}

proc vmagnitude { v } {
    return [magnitude $v]
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

proc hdot { g h } {
    return [expr [lindex $g 0]*[lindex $h 0] + \
                 [lindex $g 1]*[lindex $h 1] + \
                 [lindex $g 2]*[lindex $h 2] + \
                 [lindex $g 3]*[lindex $h 3]]
}

proc v2dot { x y } {
    return [expr [lindex $x 0]*[lindex $y 0] + [lindex $x 1]*[lindex $y 1]]
}

proc vsub2dot { p2 p1 v } {
    return [vdot [vsub2 $p2 $p1] $v]
}

proc velmul { u v } {
    return [list [expr [lindex $u 0]*[lindex $v 0]] \
	         [expr [lindex $u 1]*[lindex $v 1]] \
		 [expr [lindex $u 2]*[lindex $v 2]]]
}

proc veldiv { u v } {
    return [list [expr [lindex $u 0]*1.0/[lindex $v 0]] \
	         [expr [lindex $u 1]*1.0/[lindex $v 1]] \
		 [expr [lindex $u 2]*1.0/[lindex $v 2]]]
}

proc vinvdir { v } {
    return [veldiv { 1 1 1 } $v]
}

proc mat3x3vec { m v } {
    return [list [vdot [lrange $m 0 2] $v] \
	         [vdot [lrange $m 4 6] $v] \
		 [vdot [lrange $m 8 10] $v]]
}

proc vec3x3mat { v m } {
    return [list [vdot [list [lindex $m 0] [lindex $m 4] [lindex $m 8]] $v] \
	         [vdot [list [lindex $m 1] [lindex $m 5] [lindex $m 9]] $v] \
		 [vdot [list [lindex $m 2] [lindex $m 6] [lindex $m 10]] $v]]
}

proc mat3x2vec { m v } {
    return [list [v2dot [lrange $m 0 1] $v] \
                 [v2dot [lrange $m 4 5] $v] \
                 [v2dot [lrange $m 8 9] $v]]
}

proc vec2x3mat { v m } {
    return [list [v2dot [list [lindex $m 0] [lindex $m 4]] $v] \
                 [v2dot [list [lindex $m 1] [lindex $m 5]] $v] \
                 [v2dot [list [lindex $m 2] [lindex $m 6]] $v]]
}

proc mat4x3pnt { m p } {
    set f [expr 1.0/([vdot [lrange $m 12 14] $p]+[lindex $m 15])]
    return [list [expr ([lindex $m 0]*[lindex $p 0] + \
                        [lindex $m 1]*[lindex $p 1] + \
                        [lindex $m 2]*[lindex $p 2] + [lindex $m 3])*$f] \
                 [expr ([lindex $m 4]*[lindex $p 0] + \
                        [lindex $m 5]*[lindex $p 1] + \
                        [lindex $m 6]*[lindex $p 2] + [lindex $m 7])*$f] \
                 [expr ([lindex $m 8]*[lindex $p 0] + \
                        [lindex $m 9]*[lindex $p 1] + \
                        [lindex $m 10]*[lindex $p 2] + [lindex $m 11])*$f]]
}

proc pnt3x4mat { p m } {
    set f [vdot [list [lindex $m 3] [lindex $m 7] [lindex $m 14]] $p]
    set f [expr 1.0/($f+[lindex $m 15])]
    return [list [expr ([lindex $m 0]*[lindex $p 0] + \
                        [lindex $m 4]*[lindex $p 1] + \
                        [lindex $m 8]*[lindex $p 2] + [lindex $m 12])*$f] \
                 [expr ([lindex $m 1]*[lindex $p 0] + \
                        [lindex $m 5]*[lindex $p 1] + \
                        [lindex $m 9]*[lindex $p 2] + [lindex $m 13])*$f] \
                 [expr ([lindex $m 2]*[lindex $p 0] + \
                        [lindex $m 6]*[lindex $p 1] + \
                        [lindex $m 10]*[lindex $p 2] + [lindex $m 14])*$f]]
}

proc mat4x4pnt { m h } {
    return [list [hdot [lrange $m 0 3] $h] \
                 [hdot [lrange $m 4 7] $h] \
                 [hdot [lrange $m 8 11] $h] \
                 [hdot [lrange $m 12 15] $h]]
}

proc mat4x3vec { m v } {
    set f [expr 1.0/[lindex $m 15]]
    return [list [expr [vdot [lrange $m 0 2] $v]*$f] \
                 [expr [vdot [lrange $m 4 6] $v]*$f] \
                 [expr [vdot [lrange $m 8 10] $v]*$f]]
}

proc vec3x4mat { v m } {
    set f [expr 1.0/[lindex $m 15]]
    return [list [expr [vdot [list [lindex $m 0] [lindex $m 4] [lindex $m 8]] \
                             $v]*$f] \
                 [expr [vdot [list [lindex $m 1] [lindex $m 5] [lindex $m 9]] \
                             $v]*$f] \
                 [expr [vdot [list [lindex $m 2] [lindex $m 6] [lindex $m 10]]\
                             $v]*$f]] \
}

proc vec2x4mat { v m } {
  set f [expr 1.0/[lindex $m 15]]
  return [list [expr [lindex $m 0]*[lindex $v 0]+[lindex $m 4]*[lindex $v 1]] \
               [expr [lindex $m 1]*[lindex $v 0]+[lindex $m 5]*[lindex $v 1]] \
               [expr [lindex $m 2]*[lindex $v 0]+[lindex $m 6]*[lindex $v 1]]]
}

proc vequal { u v } {
    return [expr [lindex $u 0]==[lindex $v 0] && \
	         [lindex $u 1]==[lindex $v 1] && \
		 [lindex $u 2]==[lindex $v 2]]
}

proc vapproxequal { u v tol } {
    return [near_zero [vsub $u $v] $tol]
}

proc vnear_zero { v tol } {
    return [vapproxequal { 0 0 0 } $v $tol]
}

proc vmin { u v } {
    set result ""
    if { [lindex $u 0]<[lindex $v 0] } then {
	lappend result [lindex $u 0]
    } else {
	lappend result [lindex $v 0]
    }
    if { [lindex $u 1]<[lindex $v 1] } then {
	lappend result [lindex $u 1]
    } else {
	lappend result [lindex $v 1]
    }
    if { [lindex $u 2]<[lindex $v 2] } then {
	lappend result [lindex $u 2]
    } else {
	lappend result [lindex $v 2]
    }

    return $result
}

proc vmax { u v } {
    set result ""
    if { [lindex $u 0]>[lindex $v 0] } then {
	lappend result [lindex $u 0]
    } else {
	lappend result [lindex $v 0]
    }
    if { [lindex $u 1]>[lindex $v 1] } then {
	lappend result [lindex $u 1]
    } else {
	lappend result [lindex $v 1]
    }
    if { [lindex $u 2]>[lindex $v 2] } then {
	lappend result [lindex $u 2]
    } else {
	lappend result [lindex $v 2]
    }

    return $result
}

proc hdivide { h } {
    return [vscale [lrange $h 0 2] [expr 1.0/[lindex $h 3]]]
}

proc quat_from_vrot {r v} {
	set rot [expr $r * 0.5]
	set q [vscale [vunitize $v] [expr sin($rot)]]
	lappend q [expr cos($rot)]
	return $q
}

proc quat_from_rot {r x y z} {
	return [quat_from_vrot $r [list $x $y $z]]
}

proc quat_from_rot_deg {r x y z} {
	global M_PI
	return [quat_from_vrot [expr $r * ( $M_PI / 180.0 )] [list $x $y $z]]
}

proc quat_from_vrot_deg {r v} {
	global M_PI
	return [quat_from_vrot [expr $r * ($M_PI / 180.0) ] $v]
}


proc qadd2 { q r } {
    return [hadd2 $q $r]
}

proc qsub2 { q r } {
    return [hsub2 $q $r]
}

proc qadd args {
    return [eval hadd $args]
}

proc qsub args {
    return [eval hsub $args]
}

proc qscale { q c } {
    return [hscale $q $c]
}

proc qdot { q r } {
    return [hdot $q $r]
}

proc qmagsq { q } {
    return [qdot $q $q]
}

proc qmagnitude { q } {
    return [expr sqrt([qmagsq $q])]
}

proc qunitize { q } {
    return [qscale $q [expr 1.0/[qmagnitude $q]]]
}

proc qmul { q r } {
    return [list [expr +[lindex $q 0]*[lindex $r 3] \
                       +[lindex $q 1]*[lindex $r 2] \
                       -[lindex $q 2]*[lindex $r 1] \
                       +[lindex $q 3]*[lindex $r 0]] \
                 [expr -[lindex $q 0]*[lindex $r 2] \
                       +[lindex $q 1]*[lindex $r 3] \
                       +[lindex $q 2]*[lindex $r 0] \
                       +[lindex $q 3]*[lindex $r 1]] \
                 [expr +[lindex $q 0]*[lindex $r 1] \
                       -[lindex $q 1]*[lindex $r 0] \
                       +[lindex $q 2]*[lindex $r 3] \
                       +[lindex $q 3]*[lindex $r 2]] \
                 [expr -[lindex $q 0]*[lindex $r 0] \
                       -[lindex $q 1]*[lindex $r 1] \
                       -[lindex $q 2]*[lindex $r 2] \
                       +[lindex $q 3]*[lindex $r 3]]]
}

proc qconjugate { q } {
    return [concat [vreverse [lrange $q 0 2]] [lindex $q 3]]
}

proc qinverse { q } {
    return [qscale [qconjugate $q] [expr 1.0/[qmagsq $q]]]
}

proc qblend2 { b c  d e } {
    return [qadd2 [qscale $c $b] [qscale $e $d]]
}

#
# qtom -- Quaternion to Matrix
#
#
proc qtom {q} {
    set qx 	[lindex $q 0]
    set qy 	[lindex $q 1]
    set qz 	[lindex $q 2]
    set qw 	[lindex $q 3]

    set Nq [expr $qx * $qx + $qy * $qy + $qz * $qz + $qw * $qw]
    if {$Nq > 0.0} {
    	set s [expr 2.0 / $Nq]
    } else {
    	set s 0.0
    }

    set xs [expr $qx * $s]
    set ys [expr $qy * $s]
    set zs [expr $qz * $s]
    set wx [expr $qw * $xs]
    set wy [expr $qw * $ys]
    set wz [expr $qw * $zs]
    set xx [expr $qx * $xs]
    set xy [expr $qx * $ys]
    set xz [expr $qx * $zs]
    set yy [expr $qy * $ys]
    set yz [expr $qy * $zs]
    set zz [expr $qz * $zs]

    return [list \
	[ expr 1.0 - ( $yy + $zz ) ] 	[expr $xy - $wz] \
		[expr $xz + $wy] 	0.0 \
	[expr $xy + $wz] 		[expr 1.0 - ( $xx + $zz ) ] \
		[expr $yz - $wx] 	0.0 \
	[expr $xz - $wy] 		[expr $yz + $wx] \
		[expr 1.0 - ( $xx + $yy ) ] 	0.0 \
	0.0 0.0 0.0 1.0 ]
}

proc v3rpp_overlap { l1 h1 l2 h2 } {
    return [expr !([lindex $l1 0]>[lindex $h2 0] || \
                   [lindex $l1 1]>[lindex $h2 1] || \
                   [lindex $l1 2]>[lindex $h2 2] || \
                   [lindex $l2 0]>[lindex $h1 0] || \
                   [lindex $l2 1]>[lindex $h1 1] || \
                   [lindex $l2 2]>[lindex $h1 2])]
}

proc v3rpp_overlap_tol { l1 h1 l2 h2 toldist } {
    return [expr !([lindex $l1 0]>([lindex $h2 0]+$toldist) || \
                   [lindex $l1 1]>([lindex $h2 1]+$toldist) || \
                   [lindex $l1 2]>([lindex $h2 2]+$toldist) || \
                   [lindex $l2 0]>([lindex $h1 0]+$toldist) || \
                   [lindex $l2 1]>([lindex $h1 1]+$toldist) || \
                   [lindex $l2 2]>([lindex $h1 2]+$toldist))]
}

proc v3pt_in_rpp { pt lo hi } {
    return [expr [lindex $pt 0]>=[lindex $lo 0] && \
                 [lindex $pt 0]<=[lindex $hi 0] && \
                 [lindex $pt 1]>=[lindex $lo 1] && \
                 [lindex $pt 1]<=[lindex $hi 1] && \
                 [lindex $pt 2]>=[lindex $lo 2] && \
                 [lindex $pt 2]<=[lindex $hi 2]]
}

proc v3pt_in_rpp_tol { pt lo hi toldist } {
    return [expr [lindex $pt 0]>=([lindex $lo 0]-$toldist) && \
                 [lindex $pt 0]<=([lindex $hi 0]+$toldist) && \
                 [lindex $pt 1]>=([lindex $lo 1]-$toldist) && \
                 [lindex $pt 1]<=([lindex $hi 1]+$toldist) && \
                 [lindex $pt 2]>=([lindex $lo 2]-$toldist) && \
                 [lindex $pt 2]<=([lindex $hi 2]+$toldist)]
}

proc v3rpp1_in_rpp2 { lo1 hi1 lo2 hi2 } {
    return [expr [lindex $lo1 0]>=[lindex $lo2 0] && \
                 [lindex $hi1 0]<=[lindex $hi2 0] && \
                 [lindex $lo1 1]>=[lindex $lo2 1] && \
                 [lindex $hi1 1]<=[lindex $hi2 1] && \
                 [lindex $lo1 2]>=[lindex $lo2 2] && \
                 [lindex $hi1 2]<=[lindex $hi2 2]]
}



proc mat_deltas { m x y z } {
	return [ list [lindex $m 0]\
		      [lindex $m 1]\
		      [lindex $m 2]\
			$x \
		      [lindex $m 4]\
		      [lindex $m 5]\
		      [lindex $m 6]\
			$y \
		      [lindex $m 8]\
		      [lindex $m 9]\
		      [lindex $m 10]\
			$z \
		      [lindex $m 12]\
		      [lindex $m 13]\
		      [lindex $m 14]\
		      [lindex $m 15]]
}

proc mat_deltas_vec { m v } {
	return [ mat_deltas $m 	[lindex $v 0]\
				[lindex $v 1]\
				[lindex $v 2]]
}

proc mag2sq { v } {
	return [expr [lindex $v 0]*[lindex $v 0] + \
		     [lindex $v 1]*[lindex $v 1]]
}

proc vminmax { min max pt } {
	return [list [vmin $min $pt] [vmax $max $pt]]
}


proc vadd2n { u v n } {
	set i 0
	while { $i < $n} {
		lappend retlist [expr [lindex $u $i]+[lindex $v $i]]
		incr i
	}
	return $retlist
}

proc vsub2n { u v n } {
	set i 0
	while { $i < $n} {
		lappend retlist [expr [lindex $u $i]-[lindex $v $i]]
		incr i
	}
	return $retlist
}

proc vadd3n { u v w n } {
	return [vadd2n $u [vadd2n $v $w $n] $n]
}

proc vsub3n { u v w n } {
	return [vsub2n $u [vadd3n $v $w $n] $n]
}
proc vadd4n { u v w x n } {
	return [vadd2n $u [vadd3n $v $w $x $n] $n]
}

proc vsub4n { u v w x n } {
	return [vsub2n $u [vadd3n $v $w $x $n] $n]
}

#make a zero vector of arbitrary length (used in vaddn and vsubn)
proc vzeron { n } {
	set i 0
	while {$i < $n} {
		lappend retlist 0
		incr i
	}
	return $retlist
}

#extension of vadd
#add arbitrary number of vectors of length n
# usage: vaddn $v1 $v2 $v3 ... $vm $n
proc vaddn args {
	set cur_length [llength $args]
	if { $cur_length==1 } then {
		return [vzeron [lindex $args 0]]
	} else {
		return [vadd2n [lindex $args 0] \
			[eval vaddn [lrange $args 1 end]] \
			[lindex $args [expr $cur_length-1]]]
	}
}

proc vsubn args {
	set cur_length [llength $args]
	if { $cur_length==1 } then {
		return [vzeron [lindex $args 0]]
	} else {
		return [vsub2n [lindex $args 0] \
			[eval vaddn [lrange $args 1 end]] \
			[lindex $args [expr $cur_length-1]]]
	}
}


proc vscalen { v c n } {
	set i 0
	while { $i < $n } {
		lappend retlist [expr [lindex $v $i]*$c]
		incr i
	}
	return $retlist
}

proc vadd2scalen { a b s n } {
	return [vscalen [vadd2n $a $b $n] $s $n]
}

proc vsub2scalen { a b s n } {
	return [vscalen [vsub2n $a $b $n] $s $n]
}

proc vcomb2n { a b  c d  n } {
	return [vadd2n [vscalen $b $a $n] [vscale $d $c $n] $n]
}

proc vcomb3n { a b  c d  e f n } {
	return [vadd3n [vscalen $b $a $n] [vscale $d $c $n] [vscale $f $e $n] $n]
}

proc vjoin1n { b  c d  n } {
	return [vcomb2n 1 $b  $c $d $n]
}

proc vjoin2n { b  c d  e f  n } {
	return [vcomb3n 1 $b  $c $d  $e $f $n]
}

proc vjoin3n { b  c d  e f  g h  n } {
	return [vadd2n $b [vcomb3n $c $d $e $f $g $h $n] $n]
}

proc vblend2n { b c  d e  n } {
	return [vcomb2n $c $b $e $d $n]
}

#  mat_fmt matrix
#
#  Formats a matrix sort of like mat_print,
#  only it doesn't explicitly do any I/O
#
proc mat_fmt {m} {
	set str [format "%8.3f %8.3f %8.3f %8.3f\n"\
		[lindex $m 0] [lindex $m 1] [lindex $m 2] [lindex $m 3]]
	append str [format "%8.3f %8.3f %8.3f %8.3f\n"\
		[lindex $m 4] [lindex $m 5] [lindex $m 6] [lindex $m 7]]
	append str [format "%8.3f %8.3f %8.3f %8.3f\n"\
		[lindex $m 8] [lindex $m 9] [lindex $m 10] [lindex $m 11]]
	append str [format "%8.3f %8.3f %8.3f %8.3f\n"\
		[lindex $m 12] [lindex $m 13] [lindex $m 14] [lindex $m 15]]
	return $str
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
