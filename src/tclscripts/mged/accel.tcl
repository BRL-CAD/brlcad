#                       A C C E L . T C L
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
#
# Author -
#      Bob Hartley, SEAP
#
# Date-
#      15 August 2000
#
# Description-
#      Various accelerators for BRL-CAD.
#
# Modifications-       
#      TraNese Christy
#        *- Changed proc names
#        *- Added ability to accept variable number of args
#        *- Added usage message
#        *- Made the code more readable and efficient
#



#
#                               R C C - C A P
#
#     Generate a cap on a specified RCC 
#     Default end is "b"; Default height creates spherical cap
#     Assume (A=B=C=D) || (A=B && C=D)
#
proc rcc-cap {args} {
    global mged_display
    set usage "Usage: rcc-cap rccname newname \[height\] \[b|t\] \n \
	       \t(create a cap (ell) at an end of an rcc)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 4} {
	error "$usage"
    }
    set rccname [lindex $args 0]
    set newname [lindex $args 1]
    if {$argc == 2} {
	foreach {ax ay az} [db get $rccname A] {}
	set amag [expr sqrt($ax*$ax + $ay*$ay + $az*$az)]
	set height $amag
	set end b
    } elseif {$argc == 3} {
	if { [regexp {^[0-9]+.*[0-9]*$} [lindex $args 2]]} {
	    set height [expr [bu_units_conversion $mged_display(units)] *[lindex $args 2]]
            set end b
	} else {
	    foreach {ax ay az} [db get $rccname A] {}
	    set amag [expr sqrt($ax*$ax + $ay*$ay + $az*$az)]
	    set height $amag
	    set end [lindex $args 2]
	}
    } else {
       set height [expr [bu_units_conversion $mged_display(units)] *[lindex $args 2]]
       set end [lindex $args 3]
    }
  #Choices for $end are either "b" (base) or "t" (top)
    if {$end != "b" && $end != "t"} {
	error "bad end '$end' : must be b or t"
    }
  #Set Variables
    set hv [db get $rccname H]
    set uhv [vunitize $hv]
    set ellheight [vscale $uhv $height]
    set vertex [db get $rccname V]
    if {$end == "t"} {
	set vertex [vadd2 {$vertex} $hv]
        set vec1 C
        set vec2 D  
    } else {
	set ellheight [vreverse $ellheight]
        set vec1 A
        set vec2 B
    }
    set Alist [db get $rccname $vec1]
    set Blist [db get $rccname $vec2]
  #Create ELL 
    db put $newname ell V "$vertex" A "$Alist" B "$Blist" C "$ellheight"        
    e $newname
    return $newname
}

#
#                               R C C - T G C
#
#   Generate a TGC with a specified apex (ptx pty ptz) at a specified end of
#   a specified RCC; Default end is "b"
#
#
proc rcc-tgc {args} {
    global mged_display
    set usage "Usage: rcc-tgc rccname newname x y z \[b|t\] \n   \
               \t(create a tgc with the specified apex at an end of an rcc)"
    set argc [llength $args]
    if {$argc < 5 || $argc > 6} {
	error "$usage"
    }
    set rccname [lindex $args 0]
    set newname [lindex $args 1]
    set ptx [lindex $args 2]
    set pty [lindex $args 3]
    set ptz [lindex $args 4]
    if {$argc == 5} {
	set end b
    } else {
	set end [lindex $args 5]
    }
  #Choices for $end are either "b" (base) or "t" (top)  
    if {$end != "b" && $end != "t"} {
	error "bad end '$end' : must be b or t" 
    }
  #Set Variables
    if {$end == "t"} {
	set vec1 C
	set vec2 D
	set vertex [vadd2 [db get $rccname V] [db get $rccname H]]
    } else {
	set vec1 A
        set vec2 B
        set vertex [db get $rccname V]
    }
    set vector1 [db get $rccname $vec1]
    set vector2 [db get $rccname $vec2]
    set scalar .000001
#    set apex [list $ptx $pty $ptz]
    set apex ""
    foreach pt {$ptx $pty $ptz} {
	set result [expr $pt*[bu_units_conversion $mged_display(units)]]
        lappend apex $result
    }
    set height [vsub2 $apex $vertex]
  #Create TGC
  #in order to create properly, convert to display units
    foreach param {vertex height vector1 vector2} val "{$vertex} {$height} {$vector1} {$vector2}" {
	set result ""
	foreach coord $val {
	    set temp [expr $coord/[bu_units_conversion $mged_display(units)]]
            lappend result $temp
	}
	set $param $result
    }
    set scalar [expr $scalar/[bu_units_conversion $mged_display(units)]]
    eval in $newname tgc $vertex $height $vector1 $vector2 $scalar $scalar
    return $newname
}

#
#                               T O R - R C C
#
#  Generate an RCC from a specified TOR
#
#
proc tor-rcc {args} {
    global mged_display
    set usage "Usage: tor-rcc torname newname \n \
	       \t(create an rcc from a tor)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 2} {
	error "$usage"
    }
    set torname [lindex $args 0]
    set newname [lindex $args 1]
  #Get TOR coordinates
    foreach {tvx tvy tvz} [db get $torname V] {}   
    foreach {thx thy thz} [db get $torname H] {}   
    set radius1 [db get $torname r_a]
    set radius2 [db get $torname r_h]
  #Set RCC coordinates
    foreach coord {rvx rvy rvz} vval {$tvx $tvy $tvz} hval {$thx $thy $thz} {
	set $coord [expr $vval + $hval * $radius2]
    }
    set scale [expr -2*$radius2]
    foreach coord {rhx rhy rhz} hval {$thx $thy $thz} {
	set $coord [expr $scale * $hval]
    }
  #Create RCC
  #in order to create properly, have to use display units
    foreach param {rvx rvy rvz rhx rhy rhz radius1} val "$rvx $rvy $rvz $rhx $rhy $rhz $radius1" {
	set $param [expr $val/[bu_units_conversion $mged_display(units)]]
    }
    in $newname rcc $rvx $rvy $rvz $rhx $rhy $rhz $radius1
    return $newname
}

#
#                               R C C - T O R
#
#  Generate a TOR from a specified RCC
#
#   
proc rcc-tor {args} {
    global mged_display
    set usage "Usage: rcc-tor rccname newname \n \
	       \t(create a tor from an rcc)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 2} {
	error "$usage"
    }
    set rccname [lindex $args 0]
    set newname [lindex $args 1]
  #Get RCC coordinates    
    foreach {rvx rvy rvz} [db get $rccname V] {}
    foreach {rax ray raz} [db get $rccname A] {}
    foreach {rhx rhy rhz} [db get $rccname H] {}
  #Set TOR coordinates
    set radius1 [expr sqrt($rax*$rax + $ray*$ray + $raz*$raz)]
    set radius2 [expr sqrt($rhx*$rhx + $rhy*$rhy + $rhz*$rhz)*.5] 
  #TOR not made if RCC is at least twice as high as it is wide
    if {$radius2 >= $radius1} {
	error "RCC is at least twice as high as it is wide -- \n \
		\t radius2 > radius1 : TOR not made!"
    }
    foreach coord {tvx tvy tvz} vval {$rvx $rvy $rvz} hval {$rhx $rhy $rhz} {
	set $coord [expr $vval + $hval*.5]
    }
  #Create TOR
  #in order to create properly, have to use display units
    foreach param {tvx tvy tvz rhx rhy rhz radius1 radius2} val "$tvx $tvy $tvz $rhx $rhy $rhz $radius1 $radius2" {
	set $param [expr $val/[bu_units_conversion $mged_display(units)]]
    }
    in $newname tor $tvx $tvy $tvz $rhx $rhy $rhz $radius1 $radius2
    return $newname
}

#
#                             R C C - B L E N D
#
#  Generate a flange at a specified base of a specified RCC
#  The flange created is a region made up of a TOR and an RCC
#  thickness is the thickness of the TOR; Default end is "b"
#
#
proc rcc-blend {args} {
    global mged_display
    set usage "Usage: rcc-blend rccname newname thickness \[b|t\] \n \
	       \t(create a blend at an end of an rcc)"
    set argc [llength $args]
    if {$argc < 3 || $argc > 4} {
	error "$usage"
    }
    set rccname1 [lindex $args 0]
    set newname [lindex $args 1]
    set thickness [expr [bu_units_conversion $mged_display(units)] * [lindex $args 2]]
    if {$argc == 3} {
	set end b
    } else {
	set end [lindex $args 3]
    }
  #Choices for $end are either "b" (base) or "t" (top)
    if {$end != "b" && $end != "t"} {
	error "bad end '$end': must be b or t"
    }
  #Get RCC coordinates
    foreach {rvx rvy rvz} [db get $rccname1 V] {}
    foreach {rhx rhy rhz} [db get $rccname1 H] {}
    foreach {rax ray raz} [db get $rccname1 A] {}    
  #Set Variables  
    set amag [expr $thickness + (sqrt($rax*$rax + $ray*$ray + $raz*$raz))]
    set hmag [expr sqrt($rhx*$rhx + $rhy*$rhy + $rhz*$rhz)]
    set num [expr $hmag-$thickness] 
    if {$end == "t"} {
	foreach coord {vx vy vz} vval {$rvx $rvy $rvz} hval {$rhx $rhy $rhz} {
	    set t$coord [expr $vval + $hval * $num/$hmag]
            set r$coord [expr $vval + $num/$hmag * $hval]
        }
    } else {
	foreach coord {tvx tvy tvz} vval {$rvx $rvy $rvz} hval {$rhx $rhy $rhz} {
	    set $coord [expr $vval + $hval * $thickness/$hmag]
        }
    }
  #Create TOR
    set torname [make_name $newname]
    set hscale [expr $thickness/$hmag]
  #In order to make TOR properly, have to convert parameters to display units  
    foreach param {tvx tvy tvz rhx rhy rhz amag thickness} val "$tvx $tvy $tvz $rhx $rhy $rhz $amag $thickness" {
	set $param [expr $val/[bu_units_conversion $mged_display(units)]]
    } 
    in $torname tor $tvx $tvy $tvz $rhx $rhy $rhz $amag $thickness
  #Create RCC2
    set rccname2 [make_name $newname]
  #In order to make TOR properly, have to convert parameters to display units
    foreach param {rvx rvy rvz } val "$rvx $rvy $rvz" {
	set $param [expr $val/[bu_units_conversion $mged_display(units)]]
    }
    in $rccname2 rcc $rvx $rvy $rvz [expr $hscale*$rhx] [expr $hscale*$rhy] [expr $hscale*$rhz] $amag
  #Create Flange
    r $newname u $rccname2 - $torname - $rccname1
    d $torname $rccname2
    e $newname
    return $newname
}

#
#                               R P P - C A P
#
#  Generate an ARB6 with a specified height at the specified face of
#  a specified RPP; Default orient is "0"
#
#
proc rpp-cap {args} {
#If orient = 0, the peaks of the ARB6 are drawn at the midpoint between the
# first and second points and the midpoint between the third and fourth points
# of the specified face
#If orient = 1, the peaks of the ARB6 are drawn at the midpoint between the 
# first and fourth points and the midpoint between the second and third points
# of the specified face
    global mged_display
    set usage "Usage: rpp-cap rppname newname face height \[0|1\] \n \
	       \t(create a cap (arb6) at a face of an rpp)"
    set argc [llength $args]
    if {$argc < 4 || $argc > 5} {
	error "$usage"
    }
    set rppname [lindex $args 0]
    set newname [lindex $args 1]
    set face [lindex $args 2]
    set height [expr [lindex $args 3]*[bu_units_conversion $mged_display(units)]]
    if {$argc == 4} {
	set orient 0
    } else {
	set orient [lindex $args 4]
    }   
  #Choices for orient are either "0" or "1"
    if {$orient != 0 && $orient != 1} {
	error "bad orient '$orient': must be 0 or 1"
    }
  #Set Constants
    foreach pt {a b c d} index {0 1 2 3} {
	set $pt [string index $face $index]
    }
    foreach vert {V1 V2 V3 V4} pt {$a $b $c $d} {
	set $vert [eval db get $rppname V$pt]
    }

  #Determine Normal
    set u1 [vunitize [vsub $V4 $V3]]
    set u2 [vunitize [vsub $V2 $V3]]
    set normal [vunitize [vcross $u1 $u2]]
    set hvector [vscale $normal $height]
  #Determine Midpoint
    if {$orient == 0} {
	set rise1 [vadd2 $V1 $V2]
	set rise2 [vadd2 $V3 $V4]
        set P1 $V1
        set P2 $V2
        set P3 $V3
        set P4 $V4
    } else {
	set rise2 [vadd2 $V1 $V4]
        set rise1 [vadd2 $V2 $V3]
        set P1 $V2
        set P2 $V3
        set P3 $V4
        set P4 $V1
    }

    foreach coord {r1x r1y r1z} index {0 1 2} {
	set $coord [expr [lindex $rise1 $index] * .5]
    }
    foreach coord {r2x r2y r2z} index {0 1 2} {
	set $coord [expr [lindex $rise2 $index] * .5]
    }
  #Choose the first point (1-8) not on the specified face
    set pt 1
    set index 0
    while {$index <= 3 && $pt <= 8} {
	if {$pt == [string index $face $index]} {
	    incr pt
            set index 0 
        } else {
            incr index 
        }
    }
  #Determine direction of hvector 
  #Form vector from a point on the specified face and a point not on the face
    set direct [vunitize [vsub $V1 [db get $rppname V$pt]]]
    set dp [vdot $normal $direct]
    if {$dp < 0} {
	set hvector [vreverse $hvector]
    }
  #Set ARB6 coordinates
    foreach coord {r1x r1y r1z} value {$r1x $r1y $r1z} index {0 1 2} {
      	set $coord [expr $value + [lindex $hvector $index]]
    }
    foreach coord {r2x r2y r2z} value {$r2x $r2y $r2z} index {0 1 2} {
	set $coord [expr $value + [lindex $hvector $index]]
    }
  #Create ARB6
  #in order to create properly, convert to display units
    foreach param {P1 P2 P3 P4} val "{$P1} {$P2} {$P3} {$P4}" {
	set result ""
	foreach coord $val {
	    set temp [expr $coord/[bu_units_conversion $mged_display(units)]]
            lappend result $temp
	}
	set $param $result
    }
    foreach coord {r1x r1y r1z r2x r2y r2z } val "$r1x $r1y $r1z $r2x $r2y $r2z" {
	set $coord [expr $val/[bu_units_conversion $mged_display(units)]]
    }

    eval in $newname arb6 $P1 $P2 $P3 $P4 $r1x $r1y $r1z $r2x $r2y $r2z
    return $newname
}

#
#                              R P P - A R C H
#
#  Generate a cylinder between the specified edges of an RPP
#  The edges are represented by the first and second and the third and fourth
#  points of the specified face.
#
#
proc rpp-arch {args} {
    global mged_display
    set usage "Usage: rpp-arch rppname newname face \n \
	       \t(create an arch at a face of an rpp)"
    set argc [llength $args]
    if {$argc < 3 || $argc > 3} {
	error "$usage"
    }
    set rppname [lindex $args 0]
    set newname [lindex $args 1]
    set face [lindex $args 2]
  #Set Constants
    foreach pt {a b c d} index {0 1 2 3} {
	set $pt [string index $face $index]	
    }
    foreach vert {V1 V2 V3 V4} pt {$a $b $c $d} {
	set $vert [eval db get $rppname V$pt]
    }
  #Set Variables
    set height  [vsub2 $V2 $V1]
    set vertex [vscale [vadd2 $V4 $V1] .5]
    set radius [vmagnitude [vsub2 $vertex $V4]]
  #Create RCC 
  #in order to create properly, have to use display units
    foreach param {vertex height radius} val "{$vertex} {$height} {$radius}" {
	set result ""
	foreach coord $val {
	    set temp [expr $coord/[bu_units_conversion $mged_display(units)]]
            lappend result $temp
	}
	set $param $result
    }
    eval in $newname rcc $vertex $height $radius
    return $newname
}

#
#                              S P H - P A R T 
#
#  Generates a PART that encompasses two specified SPHs
#
#
proc sph-part {args} {
    global mged_display
    set usage "Usage: sph-part sph1name sph2name newname \n \
	       \t(create a part from two sph's)"
    set argc [llength $args]
    if {$argc < 3 || $argc > 3} {
	error "$usage"
    }
    set sphname1 [lindex $args 0]
    set sphname2 [lindex $args 1]
    set newname [lindex $args 2]
  #Get SPH coordinates
    foreach {vx1 vy1 vz1} [db get $sphname1 V] {}
    foreach {vx2 vy2 vz2} [db get $sphname2 V] {}
    foreach {ax1 ay1 az1} [db get $sphname1 A] {}
    foreach {ax2 ay2 az2} [db get $sphname2 A] {}
  #Set Variables
    set radius1 [expr sqrt($ax1*$ax1 + $ay1*$ay1 + $az1*$az1)]
    set radius2 [expr sqrt($ax2*$ax2 + $ay2*$ay2 + $az2*$az2)] 
    set hx [expr $vx2-$vx1]
    set hy [expr $vy2-$vy1]
    set hz [expr $vz2-$vz1]    
  #in order to create properly, convert to display units
    foreach param {vx1 vy1 vz1 hx hy hz radius1 radius2} val "$vx1 $vy1 $vz1 $hx $hy $hz $radius1 $radius2" {
	set $param [expr $val/[bu_units_conversion $mged_display(units)]]
    }
  #Create PART
    in $newname part $vx1 $vy1 $vz1 $hx $hy $hz $radius1 $radius2
    return $newname
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
