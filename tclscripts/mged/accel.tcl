#			      A C C E L . T C L
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
    set usage "Usage: rcc-cap rccname newname \[height\] \[b|t\] \n \
	       \t(create a cap at an end of an rcc)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 4} {
	error "$usage"
    }
    set rccname [lindex $args 0]
    set newname [lindex $args 1]
    if {$argc == 2} {
	foreach {ax ay az} [lindex [db get $rccname A] 0] {}
	set amag [expr sqrt($ax*$ax + $ay*$ay + $az*$az)]
	set height $amag
	set end b
    } elseif {$argc == 3} {
	if { [regexp {^[0-9]+.*[0-9]*$} [lindex $args 2]]} {
	    set height [lindex $args 2]
            set end b
	} else {
	    foreach {ax ay az} [lindex [db get $rccname A] 0] {}
	    set amag [expr sqrt($ax*$ax + $ay*$ay + $az*$az)]
	    set height $amag
	    set end [lindex $args 2]
	}
    } else {
       set height [lindex $args 2]
       set end [lindex $args 3]
    }
  #Choices for $end are either "b" (base) or "t" (top)
    if {$end != "b" && $end != "t"} {
	error "bad end '$end' : must be b or t"
    }
  #Set Variables
    set hv [eval db get $rccname H]
    set uhv [eval vunitize $hv]
    set ellheight [eval vscale {$uhv} $height]
    set vertex [lindex [db get $rccname V] 0]
    if {$end == "t"} {
	set vertex [eval vadd2 {$vertex} $hv]
        set vec1 C
        set vec2 D  
    } else {
	set ellheight [eval vreverse {$ellheight}]
        set vec1 A
        set vec2 B
    }
    set Alist [lindex [db get $rccname $vec1] 0]
    set Blist [lindex [db get $rccname $vec2] 0]
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
	set vertex [vadd2 [lindex [db get $rccname V] 0] [lindex [db get $rccname H] 0]]
    } else {
	set vec1 A
        set vec2 B
        set vertex [lindex [db get $rccname V] 0]
    }
    set vector1 [lindex [db get $rccname $vec1] 0]
    set vector2 [lindex [db get $rccname $vec2] 0]
    set scalar .000001
    set apex [list $ptx $pty $ptz]
    set height [eval vsub2 {$apex} {$vertex}]
  #Create TGC
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
    set usage "Usage: tor-rcc torname newname \n \
	       \t(create an rcc from a tor)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 2} {
	error "$usage"
    }
    set torname [lindex $args 0]
    set newname [lindex $args 1]
  #Get TOR coordinates
    foreach {tvx tvy tvz} [lindex [db get $torname V] 0] {}   
    foreach {thx thy thz} [lindex [db get $torname H] 0] {}   
    set radius1 [eval db get $torname r_a]
    set radius2 [eval db get $torname r_h]
  #Set RCC coordinates
    foreach coord {rvx rvy rvz} vval {$tvx $tvy $tvz} hval {$thx $thy $thz} {
	set $coord [expr $vval + $hval * $radius2]
    }
    set scale [expr -2*$radius2]
    foreach coord {rhx rhy rhz} hval {$thx $thy $thz} {
	set $coord [expr $scale * $hval]
    }
  #Create RCC
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
    set usage "Usage: rcc-tor rccname newname \n \
	       \t(create a tor from an rcc)"
    set argc [llength $args]
    if {$argc < 2 || $argc > 2} {
	error "$usage"
    }
    set rccname [lindex $args 0]
    set newname [lindex $args 1]
  #Get RCC coordinates    
    foreach {rvx rvy rvz} [lindex [db get $rccname V] 0] {}
    foreach {rax ray raz} [lindex [db get $rccname A] 0] {}
    foreach {rhx rhy rhz} [lindex [db get $rccname H] 0] {}
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
    set usage "Usage: rcc-blend rccname newname thickness \[b|t\] \n \
	       \t(create a blend at an end of an rcc)"
    set argc [llength $args]
    if {$argc < 3 || $argc > 4} {
	error "$usage"
    }
    set rccname1 [lindex $args 0]
    set newname [lindex $args 1]
    set thickness [lindex $args 2]
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
    foreach {rvx rvy rvz} [lindex [db get $rccname1 V] 0] {}
    foreach {rhx rhy rhz} [lindex [db get $rccname1 H] 0] {}
    foreach {rax ray raz} [lindex [db get $rccname1 A] 0] {}        
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
    set torname [eval make_name $newname]
    in $torname tor $tvx $tvy $tvz $rhx $rhy $rhz $amag $thickness
  #Create RCC2
    set rccname2 [eval make_name $newname]
    set hscale [expr $thickness/$hmag]
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
    set usage "Usage: rpp-cap rppname newname face height \[0|1\] \n \
	       \t(create a cap (arb6) at a face of an rpp)"
    set argc [llength $args]
    if {$argc < 4 || $argc > 5} {
	error "$usage"
    }
    set rppname [lindex $args 0]
    set newname [lindex $args 1]
    set face [lindex $args 2]
    set height [lindex $args 3]
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
    set u1 [eval vunitize {[eval vsub $V2 $V4]}]
    set u2 [eval vunitize {[eval vsub $V1 $V3]}]
    set normal [eval vcross {$u1} {$u2}]
    set hvector [eval vscale {$normal} $height]
  #Determine Midpoint
    if {$orient == 0} {
	set rise1 [eval vadd2 $V1 $V2]
	set rise2 [eval vadd2 $V3 $V4]
        set P1 [lindex $V1 0]
        set P2 [lindex $V2 0]
        set P3 [lindex $V3 0]
        set P4 [lindex $V4 0]
    } else {
	set rise2 [eval vadd2 $V1 $V4]
        set rise1 [eval vadd2 $V2 $V3]
        set P1 [lindex $V2 0]
        set P2 [lindex $V3 0]
        set P3 [lindex $V4 0]
        set P4 [lindex $V1 0]
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
    set direct [eval vunitize {[eval vsub $V1 [db get $rppname V$pt]]}]
    set dp [vdot $normal $direct]
    if {$dp < 0} {
	set hvector [eval vreverse {$hvector}]
    }
  #Set ARB6 coordinates
    foreach coord {r1x r1y r1z} value {$r1x $r1y $r1z} index {0 1 2} {
      	set $coord [expr $value + [lindex $hvector $index]]
    }
    foreach coord {r2x r2y r2z} value {$r2x $r2y $r2z} index {0 1 2} {
	set $coord [expr $value + [lindex $hvector $index]]
    }
  #Create ARB6
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
    set height  [eval vsub2 $V2 $V1]
    set vertex [eval vscale {[eval vadd2 $V4 $V1]} .5]
    set radius [eval vmagnitude {[eval vsub2 {$vertex} $V4]}]
  #Create RCC 
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
    foreach {vx1 vy1 vz1} [lindex [db get $sphname1 V] 0] {}
    foreach {vx2 vy2 vz2} [lindex [db get $sphname2 V] 0] {}
    foreach {ax1 ay1 az1} [lindex [db get $sphname1 A] 0] {}
    foreach {ax2 ay2 az2} [lindex [db get $sphname2 A] 0] {}
  #Set Variables
    set radius1 [expr sqrt($ax1*$ax1 + $ay1*$ay1 + $az1*$az1)]
    set radius2 [expr sqrt($ax2*$ax2 + $ay2*$ay2 + $az2*$az2)] 
    set hx [expr $vx2-$vx1]
    set hy [expr $vy2-$vy1]
    set hz [expr $vz2-$vz1]
  #Create PART
    in $newname part $vx1 $vy1 $vz1 $hx $hy $hz $radius1 $radius2
    return $newname
}

