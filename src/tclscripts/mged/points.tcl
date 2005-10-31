#!/usr/brlcad/bin/btclsh
set sketch_number 0
set pipe_number 0
set arb_number 0
set sphere_number 0

proc mat_print {str m} {
    puts "\n$str"
    for {set j 0} { $j < 4} {incr j} {
	for {set i 0} { $i < 4} {incr i} {
	    puts -nonewline [format "%8.4f" [lindex $m [expr $j * 4 + $i]]]
	}
	puts ""
    }
}

proc sphere {pts} {
  global sphere_number
  puts "sphere"

  while {[catch {db get point$sphere_number} v] == 0} {
    incr sphere_number
  }


  set last [expr [llength $pts] - 1]
  for {set i 0} {$i < $last} {incr i 3} {
    eval "put point$sphere_number sph V { [lindex $pts $i] [lindex $pts [expr $i + 1]] [lindex $pts [expr $i + 2]] } A {.10 0.0 0.0} B {0.0 .10 0.0} C {0.0 0.0 .10}"
    incr sphere_number
  }

  return
}  


#
#	P I P E
#
#       Takes a list of points on one side of a pipe, and 1 point on the 
#       opposite side of the pipe.  From this it constructs the pipe.  Note that
#       this means the pipe has to lie in a single plane
#
proc pipe {pts} {
    global pipe_number

    puts "pipe"

    # check to make sure we have a bunch of 3-tuples
    for {set i 0} {$i < [llength $pts]} {incr i} {
	set p [lindex $pts $i]
	if {[llength $p] != 3} {
	    puts "pt $i {$p} does not have 3 elements"
	    return
	}
    }

    # find diameter of the pipe
    set last [expr [llength $pts] - 1]
    
    set depth_pt [lindex $pts end]
    set last_pt [lindex $pts end-1]

    set vec [vsub $last_pt $depth_pt]
    set dia [magnitude $vec]

    set radius [expr $dia * .5]
    set center [vscale $vec 0.5]

    while {[catch {db get pipe$pipe_number} v] == 0} {
	incr pipe_number
    }

    set cmd "put pipe$pipe_number pipe"

    if {$dia < 1e-16} {
	puts "Warning: diameter of pipe$pipe_number is zero"
    }


    for {set i 0} {$i < $last} {incr i} {
	lappend cmd "V$i" [vadd [lindex $pts $i] $center]
	lappend cmd "O$i" $dia "I$i" 0 "R$i" $radius
    }

    eval $cmd

    incr pipe_number

    return
}

#
#    A R B
#
#    Takes a set of 8 points and makes an arb out of them.
#    If they do not happen to be in the right order, the arb will be
#    corrupted.  Unlike the arb8 command the order of points for this proc
#    Connects all the points in a single line, such as:
#
#       ___
#      |   |
#      |   |
#     /   
#  __/_
# | /  |
# |/   |
#
#
proc arb {pts} {
    global arb_number

    puts "arb"
    # check to make sure we have a bunch of 3-tuples
    for {set i 0} {$i < [llength $pts]} {incr i} {
	set p [lindex $pts $i]
	if {[llength $p] != 3} {
	    puts "pt $i {$p} does not have 3 elements"
	    return
	}
    }


    if {[llength $pts] < 8} {
	puts stderr "arb:  Must provide at 8 points"
	return
    }

    while {[catch {db get arb$arb_number} v] == 0} {
	incr arb_number
    }

    set cmd "put arb$arb_number arb8"
    for {set i 0} {$i < 4} {incr i} {
	lappend cmd "V[expr $i + 1]" [lindex $pts $i]
    }

    for {set i 4} {$i < 8} {incr i} {
	lappend cmd "V[expr 8 - ($i - 4) ]" [lindex $pts $i]
    }

    if {[llength $cmd] != 19 } {
	puts "mal-formed arb8 not made"
	puts "$cmd"
	return
    }

    eval $cmd
    incr arb_number

    return
}

#
#    P L A T E
#
#    Takes a series of points on a surface, and 1 point off the surface that
#    indicates the depth/thickness of the plate.  It creates the mged commands
#    necessary to create the sketch and extrude primitives needed to make the
#    shape specified
#
#
#
proc plate {pts} {
    global sketch_number

    # check to make sure we have a bunch of 3-tuples
    for {set i 0} {$i < [llength $pts]} {incr i} {
	set p [lindex $pts $i]
	if {[llength $p] != 3} {
	    puts "plate: pt $i {$p} does not have 3 elements"
	    return
	}
    }

    # compute plane from first 3 points
    set V [lindex $pts 0]

    set A [lindex $pts 1]
    set VA [vsub $A $V]

    # make sure the points are distinct
    if {[magsq $VA] < 1.0e-16} {
	puts "first and second points are within tolerance"
	return
    }

    set magsq_val 0
    set len [expr [llength $pts] - 2]
    for {set idx 2} { $magsq_val <= 0 && $idx < $len } {incr idx} {
	puts "plate1a"
	set B [lindex $pts $idx]
	set VB  [vsub $B $V]
	set magsq_val [magsq $VB]
	if {$magsq_val < 1.0e-16} {
	    puts "first and $idx point are within tolerance"
	    continue
	}

	set cross [vcross $VA $VB]
	puts "plate1a.a $cross"
	set magsq_val [magsq $cross]

	if {$magsq_val <= 0.0} {
	    puts "pts co-linear:\nV $V\nA $A\nB $B"
	    puts "moving forward."
	}
    }

    set plane [vunitize $cross]
    puts "plate1b"

    set dist [vdot $V $plane]
    if {$dist < 0.0} {
	set dist [expr 0 - $dist]
	set plane [vreverse $plane]
    }
    lappend plane $dist
    puts "plate2"
    # puts "plane $plane"

    # project points (except last) onto plane

    set limit [expr [llength $pts] - 1]
    set newpts {}
    for {set i 0} {$i < $limit} {incr i} {
	set pt [lindex $pts $i]
	set dist [expr [vdot $pt $plane] - [lindex $plane 3]]
	if {$dist != 0.0} {
	    # project point onto plane
	    set pt [vadd [vscale $plane $dist] $pt]
	}
	lappend newpts $pt
    }

    # construct transformation matrix for the sketch primitive

    # Get the axis of rotation
    set up {0.0 0.0 1.0}
    set qaxis [vunitize [vcross [lrange $plane 0 2] $up]]

    # puts "axis $qaxis"

    puts "... angles"

    # find the angle of rotation, halfangle, and cosine/sine
    set cosangle [vdot $plane $up]
    set angle [expr acos($cosangle)]
    set halfangle [expr $angle * 0.5]
    set cosangle [expr cos($halfangle)]
    set sinangle [expr sin($halfangle)]

    # construct the quaternion for the rotation
    set quat [concat [vscale $qaxis $sinangle] $cosangle]
    
    # puts "quat $quat"

    # generate the matrix from the quaternion
    set m_r [quat_quat2mat $quat]

    set m_tx [mat_deltas_vec [mat_idn] [vreverse $V]]

    set m [mat_mul $m_r $m_tx]
    # mat_print "m" $m

    set m_inv [mat_inv $m]
    # mat_print  "inverse matrix" $m_inv

    set twoDpts {}
    foreach p $newpts {
	# puts "$p    [mat4x3pnt $m $p]"
	lappend twoDpts [lrange [mat4x3pnt $m $p] 0 1]
    }


    # create the command for building the sketch primitive
    while {[catch {db get sketch$sketch_number} v] == 0} {
	incr sketch_number
    }

    set sketch_name "sketch$sketch_number"
    puts "... $sketch_name"

    set cmd "put $sketch_name sketch V"
    lappend cmd [lindex $pts 0]
    set U_vector [mat4x3vec $m_inv [list 1.0 0.0 0.0 ]]
    set V_vector [mat4x3vec $m_inv [list 0.0 1.0 0.0 ]]
    lappend cmd "A" $U_vector
    lappend cmd "B" $V_vector
    lappend cmd "VL" $twoDpts 

    # add the drawing commands to the sketch
    set linecmd {}
    set limit [llength $twoDpts]

    for {set i 0} {$i < $limit} {incr i} {
	lappend linecmd [list line S $i E [expr ( $i + 1 ) % $limit ]]
    }

    lappend cmd "SL" $linecmd

    eval $cmd


    # get depth of plate
    set pt [lindex $pts $limit]
    set dist [expr [vdot $pt $plane] - [lindex $plane 3]]
    # puts "depth $dist"
    set delta [vscale $plane $dist]

    # create the extrude primitive
    set cmd "put extrude$sketch_number extrude V" 
    lappend cmd [lindex $pts 0] 
    # height vector
    lappend cmd "H" [vscale [vreverse [lrange $plane 0 2]] $dist]

    # U vector
    lappend cmd "A" $U_vector

    # V vector
    lappend cmd "B" $V_vector

    # sketch name
    lappend cmd "S" $sketch_name

    # keypoint index
    lappend cmd "K" 0
    
    eval $cmd

    incr sketch_number
}


