#!/usr/brlcad/bin/btclsh


set sketch_number 0
set pipe_number 0
set arb_number 0
set cyl_number 0
set sph_number 0
set point_number 0



#	v R o t U p
#
#	Given a vector and a point, form a rotation matrix
#	that rotates the vector the th Z axis with the origin at the point
#
proc vRotUp {plane pt} {
    set up {0 0 1}

    set dot [expr abs( [vdot $plane $pt] ) ]
    if {$dot < 1e-8} {
	return [mat_idn]
    }


    set qaxis [vunitize [vcross [lrange $plane 0 2] $up]]

    # find the angle of rotation, halfangle, and cosine/sine
    set cosangle [vdot $plane $up]
    set angle [expr acos($cosangle)]
    set halfangle [expr $angle * 0.5]
    set cosangle [expr cos($halfangle)]
    set sinangle [expr sin($halfangle)]

    # construct the quaternion for the rotation
    set quat [concat [vscale $qaxis $sinangle] $cosangle]

    # generate the matrix from the quaternion

    set m_r [quat_quat2mat $quat]
    set m_tx [mat_deltas_vec [mat_idn] [vreverse $pt]]
    set m [mat_mul $m_r $m_tx]

    return $m
}

#
#	circleFrom3Pts
#
#	Given 3 points in 3d space, compute the center, radius, and the plane 
#	defined by the points.  These are all returned as a single list:
#	{{x y z} radius {x y z w}}
#
#	If the points are co-linear, then an empty list is returned
#
proc circleFrom3Pts {a b c} {

    puts "circleFrom3Pts\n\ta $a\n\tb $b\n\tc $c"

    # form vectors
    set AB [vsub $b $a]
    set BC [vsub $c $b]

    # find midpoints
    set ABmid [vjoin1 $a 0.5 $AB]
    set BCmid [vjoin1 $b 0.5 $BC]

    set up [vcross $AB $BC]
    if {[magsq $up] < 1e-5} {
	return {}
    }

    set up [vunitize $up]
    set ABperp [vcross $AB $up]
    set BCperp [vcross $BC $up]

    if {[catch {bn_isect_line3_line3 $ABmid $ABperp $BCmid $BCperp} center]} {
	# failed
	return {}
    }
    # success, find radius, plane

    set radius [magnitude [vsub $center $a]]
    set plane $up
    lappend plane [vdot $up $a]

    return [list $center $radius $plane]
}



#
#	C Y L S
#
#	Given points on one side of a sequence of cylinders and the last point
#	on the other side of the last cylinder, make the set of cylinders
#
proc cyls  {pts} {
    global cyl_number

    # get an index for this primitive so we have a unique name
    while {[catch {db get cyls$cyl_number} v] == 0} {
	incr cyl_number
    }


}


#
#       S P H
#
#
proc sph {pts} {
    global sph_number

    # get an index for this primitive so we have a unique name
    while {[catch {db get sph$sph_number} v] == 0} {
        incr sph_number
    }



}


#
#	P O I N T S
#
#
proc points {pts} {
    global point_number

    # get an index for this primitive so we have a unique name
    while {[catch {db get sph$point_number} v] == 0} {
	incr point_number
    }

    set last [expr [llength $pts] - 1]
    for {set i 0} {$i < $last} {incr i 3} {
	eval "put point$point_number sph V { [lindex $pts $i] [lindex $pts [expr $i + 1]] [lindex $pts [expr $i + 2]] } A {1.0 0.0 0.0} B {0.0 1.0 0.0} C {0.0 0.0 1.0}"
	incr point_number
    }

}


#
#	C Y L I N D E R
#
#	Given three points on the base of a cylinder, and one point somewhere
#	on the top of the cylinder, construct the appropriate cylinder
#
proc cylinder {pts} {
    global cyl_number

    # get an index for this primitive so we have a unique name
    while {[catch {db get cylinder$cyl_number} v] == 0} {
	incr cyl_number
    }

    set name "cylinder$cyl_number"


    set stuff [circleFrom3Pts [lindex $pts 0] [lindex $pts 1] [lindex $pts 2]]

    if {[llength $stuff] != 3} {
	puts "cannot make cylinder from:\n\t[lindex $pts 0]\n\t[lindex $pts 1]\n\t[lindex $pts 2]"
	return
    }

    set center [lindex $stuff 0]
    set radius [lindex $stuff 1]
    set plane [lindex $stuff 2]

    # Find height vector of cylinder
    set dist [vdot $plane [lindex $pts end]]
    set dist [expr $dist - [lindex $plane end]]
    set Hvec [vscale $plane $dist]

    puts "in $name rcc $center $Hvec $radius"
}



#
#	P I P E
#
#       Takes a list of points on one side of a pipe, and 1 point on the 
#       opposite side of the pipe.  From this it constructs the pipe.  
#       Note that this means the pipe has to lie in a single plane
#
#  Takes a set of 3 points for each span of the pipe, and generates the 
#  points for
#  the pipe primitive.  Note that the pipe needs to be vaguely planar, as we
#  translate from the path taken in one direction to find the centerline
#

proc pipe {pts} {
    global pipe_number

    # get an index for this pipe number so we have a unique name
    while {[catch {db get pipe$pipe_number} v] == 0} {
	incr pipe_number
    }

    # find diameter and center of the pipe
    set idx [expr [llength $pts] - 1]
    
    set depth_pt [lindex $pts $idx] ; incr idx -1
    set last_pt [lindex $pts $idx] ;  incr idx -1

    set vec [vsub $depth_pt $last_pt]
    set dia [magnitude $vec]
    set radius [expr $dia * 0.5]
    set center_vec [vscale $vec 0.5]
    puts "center_vec: $center_vec"

    set radius [expr $dia * 0.5]
    set end_center [vadd $last_pt $center_vec]


    # now we walk the list of points (backwards) given and compute the 
    # pipe points from them.

    incr idx -1
    set verts [list [list $end_center $dia]]

    for {} {$idx >= 0} {incr idx -2} {
	set a [lindex $pts $idx]
	set b [lindex $pts [expr $idx + 1]]
	set c [lindex $pts [expr $idx + 2]]


	set l [circleFrom3Pts $a $b $c]	

	set i [expr $idx / 2]
	if {[llength $l] == 0} {
	    # straight line segment... do nothing?
	    puts "cont"
	    continue
	}



	# curved line segment
	set bend_center [lindex $l 0]
	set bend_radius [lindex $l 1]
	set bend_plane [lindex $l 2]

	puts "bend_center $bend_center"
	puts "bend_radius $bend_radius"
	puts "bend_plane $bend_plane"


	# see if we have to rotate the vector to the centerline for
	# this segment.  This happens when the rotation normal is not
	# aligned with the plane normal



	# need to adjust the offset vector to the centerline
	# because we are bending out of the plane

	# find the angle between the two points
	# by forming vectors from the bend center and taking the
	# dot product to get the cosine of the angle
	# then take the arcosine to get the real angle (in radians)
	set ca [vunitize [vsub $a $bend_center]]
	set cb [vunitize [vsub $b $bend_center]]
	set cc [vunitize [vsub $c $bend_center]] 
	set c [vadd $c $center_vec]
	
	puts "ca $ca\ncb $cb\ncc $cc"

	# find angle of rotation
	set cosangle [vdot $cc $cb]
	set angle [expr acos($cosangle)]

	# form quaternion for rotation
	set quat [quat_from_vrot $angle $bend_plane]

	# rotate the vector
	set m [quat_quat2mat $quat]
	set newvec [mat4x3pnt $m $center_vec]

	# find the new point
	set b [vadd $b $newvec]



	# find angle of rotation
	set cosangle [vdot $cc $ca]
	set angle [expr acos($cosangle)]

	# form quaternion for rotation
	set quat [quat_from_vrot $angle $bend_plane]

	# rotate the vector
	set m [quat_quat2mat $quat]
	set center_vec [mat4x3pnt $m $center_vec]

	# find the new point
	set a [vadd $a $center_vec]

	

	set l [circleFrom3Pts $a $b $c]
	set bend_center [lindex $l 0]
	set bend_radius [lindex $l 1]
	set bend_plane [lindex $l 2]

	puts "final bend_center $bend_center"
	puts "final bend_radius $bend_radius"
	puts "final bend_plane $bend_plane"






	# Find the point for the pipe bend by projecting from A and C
	# along the tangent line and finding the intersection


	# form vector from bend center to a and c points
	set a_vec [vsub $a $bend_center]
	set c_vec [vsub $c $bend_center]

	# form tangent/perpendicular lines at points a and c
	set a_tangent [vcross $bend_plane $a_vec]
	set c_tangent [vcross $bend_plane $c_vec]

	# find intersection of tangent lines to get real point for pipe
	if {[catch {bn_isect_line3_line3 $a $a_tangent $c $c_tangent} pt]} {
	    #oops
	    puts "ooops"
	} else {
	    lappend verts [list $pt [expr $bend_radius ]]
	}
    }


    # add in the last point for completeness

    

    lappend verts [list [vadd [lindex $pts 0] $center_vec] $dia]

    set cmd "put pipe$pipe_number pipe"

    set v [llength $verts]
    for {set i [expr $v - 1]} { $i >= 0 } {incr i -1} {
	set val [lindex $verts $i]

	set n [expr $v - $i - 1]

	append cmd " V$n {[lindex $val 0]} O$n $dia I$n 0 R$n [lindex $val 1]"
    }

    eval "$cmd"

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
    if {[llength $pts] < 8} {
	puts stderr "arb:  Must provide at 8 points"
	return
    }

    # get an index for this primitive so we have a unique name
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

    set lim [expr [llength $pts] - 4]
    set normals {}
    set min_len 1e-4

    for {set i 0} {$i < $lim} {incr i} {
	
	# compute plane from first 3 points
	set V [lindex $pts $i]

	set A [lindex $pts [expr $i + 1]]
	set VA [vsub $A $V]

	set B [lindex $pts [expr $i + 2]]
	set VB  [vsub $B $V]

	set cross [vcross $VA $VB]
	if {[magsq $cross] > $min_len} {

	    lappend normals [vunitize $cross]
	}
    }

    if {[llength $normals] <= 0} {
	puts "Plate: No normal found"
	return
    }

    # compute the averaged normal
    set N {0 0 0}
    foreach i $normals {
	set N [vadd $N $i]
    }
    set mul [expr 1.0 / [llength $normals]]
    set plane [vscale $N $mul]

    # find the averate distance to the plane
    set avg_dist 0

    for {set i 0} {$i < [expr [llength $pts] - 1]} {incr i} {
	set avg_dist [expr $avg_dist + [vdot [lindex $pts $i] $plane]]
    }

    # compute the distance of the plane from the origin
    set avg_dist [expr $avg_dist / [expr [llength $pts] - 1]]
    if {$avg_dist < 0} {
	set plane [vreverse $plane]
	set avg_dist [expr abs($avg_dist)]
    }
    lappend plane $avg_dist


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

    set m [vRotUp $plane $V]

    set m_inv [mat_inv $m]
    # mat_print  "inverse matrix" $m_inv

    set twoDpts {}
    foreach p $newpts {
	# puts "$p    [mat4x3pnt $m $p]"
	lappend twoDpts [lrange [mat4x3pnt $m $p] 0 1]
    }


    # get an index for this primitive so we have a unique name
    while {[catch {db get sketch$sketch_number} v] == 0} {
	incr sketch_number
    }

    set sketch_name "sketch$sketch_number"

    # create the command for building the sketch primitive

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

set test_pts {
    {10.0 0.0 0.0}
    {10.0 1.0 0.0}
    {10.0 1.0 1.0}
    {10.0 0.01 1.0}
    { 9.5 0.0 1.0}
}

set new_pts {
    {15.0 0.0 0.0}
    {15.0 1.0 0.0}
    {15.0 1.0 1.0}
    {15.0 .5 .75}
    {15.0 0.01 1.0}
    {14.750 0.0 1.0}
}


set arb1_pts {
    {0.0 0.0 0.0}
    {1.0 0.0 0.0}
    {1.0 1.0 0.0}
    {0.0 1.0 0.0}

    {0.0 1.0 1.0}
    {1.0 1.0 1.0}
    {1.0 0.0 1.0}
    {0.0 0.0 1.0}
}

set arb2_pts {
    {0.0  0.0  10.0}
    {1.0  0.0  10.0}
    {1.0  1.0  10.0}
    {0.0  1.0  10.0}
    {0.0  1.0  11.0}
    {1.0  1.0  11.0}
    {1.0  0.0  11.0}
    {0.0  0.0  11.0}
}


proc mat_print {str m} {
    puts "\n$str"
    for {set j 0} { $j < 4} {incr j} {
	for {set i 0} { $i < 4} {incr i} {
	    puts -nonewline [format "%8.4f" [lindex $m [expr $j * 4 + $i]]]
	}
	puts ""
    }
}


if { 1 == 0 } {
    plate $test_pts
    plate $new_pts


    arb $arb1_pts
    arb $arb2_pts

    if { 1 == 0 } {
	pipe $test_pts
	pipe $new_pts
    }


    set pipe_pts {
	{19 10 0.5}
	{19.3536 10 0.646447}
	{19.5 10 1}
	{19.5 10 4}
	{19.5 10 5}
	{20.5 10 5}
    }
    set pipe_pts {
	{0 0 0.5}
	{5  0 0.5}
	{9  0 0.5}
	{9.707106781187 0.292893218813 0.5} 
	{10 1 0.5}
	{10 5 0.5}
	{10 8 0.5}
	{10.585786437627 9.414213562373 0.5}
	{12 10 0.5}
	{15 10 0.5}
	{19 10 0.5}
	{19.3536 10 0.646447}
	{19.5 10 1}
	{19.5 10 4}
	{19.5 10 5}
	{20.5 10 5}
    }

    set c "O"
    set fd [open pipe.asc w]
    foreach p $pipe_pts {
	puts $fd "$c $p"

	set c "Q"
    }
    close $fd
    exec asc-pl < pipe.asc > pipe.pl
    overlay pipe.pl
    file delete pipe.asc

    pipe $pipe_pts

}

