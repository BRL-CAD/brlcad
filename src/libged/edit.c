/*                         E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/edit.c
 *
 * Command to edit objects by translating, rotating, and scaling.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "ged.h"
#include "./ged_private.h"

/* edit: Proposed manual page
 *
 * NAME
 *	edit
 *
 * SYNOPSIS
 *	edit COMMAND_NAME [ARGS OBJECT ...]
 *	edit help | rotate | scale | translate [ARGS OBJECT ...]
 *
 *	ARGS:
 *	    see manual for given COMMAND_NAME
 *
 * DESCRIPTION
 *	Used to change objects through the use of subcommands.
 *
 */

/*
 * rotate: Proposed operations, and manual page
 *
 * NAME
 *	rotate (alias for "edit rotate")
 *
 * SYNOPSIS
 *	rotate [-R] [AXIS] [CENTER] ANGLE OBJECT ...
 *	rotate [-R] [[AXIS_FROM] AXIS_TO] [CENTER] [ANGLE_ORIGIN]
 *	    [ANGLE_FROM] ANGLE_TO OBJECT ...
 * 	rotate [-R] [[[-n] -k {AXIS_FROM_OBJECT | AXIS_FROM_POS}]
 * 	    [[-n] [-a | -r] {AXIS_TO_OBJECT | AXIS_TO_POS}]]
 * 	    [[-n] -c {CENTER_OBJECT | CENTER_POS}]
 * 	    [[-n] -O {ANGLE_ORIGIN_OBJECT| ANGLE_ORIGIN_POS}]
 * 	    {[[-n] -k {ANGLE_FROM_OBJECT | ANGLE_FROM_POS}]
 * 	    [-n | -o] [-a | -r | -d] {ANGLE_TO_OBJECT | ANGLE_TO_POS}}
 * 	    OBJECT ...
 *
 *	*OBJECT:
 *	    [PATH/]OBJECT [OFFSET_DIST]
 *	    [PATH/]OBJECT [x [y [z]]]
 *
 *	*POS:
 *	    {x [y [z]]} | {[-x {x | X_OBJECT}] [-y {y | Y_OBJECT}]
 *	        [-z {z | Z_OBJECT}]}
 *
 * DESCRIPTION
 *	Used to rotate one or more instances of primitive or
 *	combination objects. OBJECTs rotate around CENTER at ANGLE,
 *	which is optionally constrained to rotating around AXIS. *POS
 *	represents either position, distance, degrees, or radians,
 *	depending on the context and supplied arguments.
 *
 *	AXIS_FROM_POS is always interpreted as an absolute position.
 *	AXIS_TO_POS may be either an absolute position (-a), or a
 *	relative distance (-r) from AXIS_FROM. OFFSET_DIST is always a
 *	relative distance. A special case is made for ANGLE_TO_POS,
 *	which defaults to relative degrees from ANGLE_FROM_POS (-d),
 *	but may also be interpreted a as relative distance from
 *	ANGLE_FROM_POS (-r) or an absolute position (-a). See
 *	documentation of the -R option to see how it can be used to
 *	allow specification of radians. All *_TO_OBJECT arguments are
 *	always interpreted as absolute positions (-a).
 *
 *	By default, AXIS is interpreted as the axis to rotate upon
 *	(but does not specify where), with the rotation angle
 *	ANGLE_TO_POS in relative degrees. The default AXIS is the
 *	x-axis, with AXIS_FROM at the origin and AXIS_TO at 1,0,0.
 *
 *	Any *_OBJECT argument may be set to ".", which causes each
 *	individual OBJECT to be used in its place (a batch operation
 *	if there is more than one OBJECT).
 *
 * 	(see DESCRIPTION of translate command for other information)
 *
 * OPTIONS
 *
 * 	-R
 * 	    Interpret AXIS_TO as the center of a circle to rotate
 * 	    with, and the distance between AXIS_FROM and AXIS_TO as
 * 	    its radius. ANGLE_TO_POS is then interpreted as the amount
 * 	    to rotate OBJECT, in radians. If AXIS_TO is not provided,
 * 	    the center of the circle defaults to OBJECT. To use the
 * 	    natural origin of OBJECT as the center of the circle, use
 * 	    OBJECT as AXIS_TO_OBJECT, and enable option "-n".
 *
 *	    If -R is not given, the default is to interpret AXIS_FROM
 *	    as the "zero" point of a new axis of rotation, and AXIS_TO
 *	    as its endpoint, with ANGLE_TO_POS interpreted as relative
 *	    number of degrees to rotate from ANGLE_FROM.
 *
 *	-c CENTER_OBJECT | CENTER_POS
 *	    Set CENTER of the rotation. If omitted, CENTER defaults to
 *	    the bounding box center of the first OBJECT. To use the
 *	    natural origin of the first OBJECT as CENTER, you must set
 *	    CENTER_OBJECT to OBJECT and enable the matching -n option.
 *	    Set CENTER_OBJECT to "." to to force each OBJECT to rotate
 *	    around its own center.
 *
 *	-O ANGLE_ORIGIN_OBJECT | ANGLE_ORIGIN_POS
 *	    Sets ANGLE_ORIGIN, the origin of ANGLE. ANGLE_ORIGIN
 *	    defaults to CENTER, the center of the rotation.
 *
 * 	-n *_FROM_OBJECT | *_TO_OBJECT
 *	    Use the natural origin of *_FROM_OBJECT or *_TO_OBJECT,
 *	    rather than the default of the bounding box center.
 *
 * 	-k *_FROM_POS | *_FROM_OBJECT
 *	    Sets the keypoint for the angle or axis of rotation.
 *
 *	    If the AXIS_FROM keypoint is omitted, it defaults to the
 *	    origin (0,0,0).
 *
 *	    The line between the points ANGLE_ORIGIN and ANGLE_FROM
 *	    defines the y-axis of a custom AXIS. Therefore, if AXIS is
 *	    omitted, ANGLE_FROM defaults to a point y-offset +1 from
 *	    ANGLE_ORIGIN; the y-axis of the drawing. In essence,
 *	    ANGLE_FROM helps define the y-axis, and AXIS defines the
 *	    x-axis.
 *	   
 *	    If AXIS is provided, then ANGLE_FROM is aligned to it in
 *	    such a way that the 90 degree angle created by the
 *	    following points is maintained (as illustrated below):
 *
 *	    1) ANGLE_FROM ->
 *	    2) AXIS_TO superimposed onto ANGLE_ORIGIN ->
 *	    3) AXIS_FROM the same relative distance from the
 *	    superimposed AXIS_TO that it was from the actual AXIS_TO
 *
 *          Default:                        | Default 90 degree angle:
 *                                          |
 *                   +z    ANGLE_ORIGIN     |   AXIS_TO->ANGLE_ORIGIN
 *                    |   / (from OBJECT)   |    \  90
 *          AXIS_FROM |  o                  |     o/  ANGLE_FROM
 *	             \|                     |     ^  /        
 *	    AXIS_TO   o     o               |   o   o          _______
 *	           \ / \     \              |    \            |compass
 *	            o   \     ANGLE_FROM    |     AXIS_FROM   |  +z
 *	         +x/   +y\    (ANGLE_ORIGIN |                 |   |
 *	                       -y 1)        |                 |+x/ \+y
 *
 *	    This is achieved in a consistent manner by starting
 *	    ANGLE_FROM at the x/y of the superimposed AXIS_FROM, and
 *	    the z coordinate of ANGLE_ORIGIN, and rotating it
 *	    counterclockwise around the z-axis of ANGLE_ORIGIN. The
 *	    first encountered 90 degree angle is used. The result is
 *	    that the line between ANGLE_ORIGIN and ANGLE_FROM defines
 *	    the y-axis of the rotation, from the origin to positive-y.
 *	    The rotation y-axis (ANGLE_FROM) is always perpendicular
 *	    to the z-axis of the coordinate system.
 *
 *	    If ANGLE_FROM is set, the default AXIS is ignored, and
 *	    the rotation to ANGLE_TO is unconstrained. This means that
 *	    OBJECT may rotate at CENTER using any ANGLE. There is one
 *	    exception: unconstrained rotations of exactly 180 degrees
 *	    are ambiguous, and will therefore fail. To bypass this
 *	    limitation, perform a relative rotation of 180 degrees in
 *	    the direction required.
 *
 *	    If both ANGLE_FROM and AXIS are set, the rotation from
 *	    ANGLE_FROM to ANGLE_TO is constrained around AXIS.
 *	    This allows for the use of reference objects that are not
 *	    perfectly lined up on AXIS.
 *
 *	-o  ANGLE_TO_POS
 *	    Overrides constraint to AXIS. Only allowed when AXIS is
 *	    supplied, and ANGLE_TO_POS is given in degrees or radians.
 *	    Use -O when the axis you would like to rotate on is
 *	    difficult to reference, but you are able to set AXIS to an
 *	    axis that is perpendicular to it. ANGLE_TO_POS options -y
 *	    or -z would then enable rotation upon the desired axis.
 *
 * 	-a *_TO_POS | *_TO_OBJECT
 *	    Interpret *_TO_POS or *_TO_OBJECT as an absolute position.
 *	    This option is required if *_TO_OBJECT is supplied.
 *
 *	    If any arguments precede AXIS_TO or ANGLE_TO, they are
 *	    required to have a matching "-a", "-r", or, in the case of
 *	    ANGLE_TO, "-d".
 *
 * 	-r *_TO_POS
 *	    Interpret *_TO_POS as a point a relative distance from
 *	    its *_FROM_POS keypoint. This is the default if *_TO_POS
 *	    is set but *_FROM_POS is omitted. Must be omitted if
 *	    *_TO_OBJECT is specified.
 *
 *	    If any arguments precede AXIS_TO or ANGLE_TO, they are
 *	    required to have a matching "-a", "-r", or, in the case of
 *	    ANGLE_TO, "-d".
 *
 * 	-d ANGLE_TO_POS
 *	    Interpret ANGLE_TO_POS as relative degrees (or radians if
 *	    -R is also set) to rotate its ANGLE_FROM_POS keypoint.
 *	    This option implies -o (override AXIS), since constraining
 *	    to AXIS is not helpful in this case. This option (-d) is
 *	    the default, but in some cases it is required:
 *
 *	    If any arguments precede ANGLE_TO, it is required to have
 *	    matching "-a", "-r", or "-d".
 *
 * EXAMPLES
 *
 *	The following examples assume we are facing the top of a
 *	cube, which is centered at the origin, and intersects the x
 *	and y axes perpendicularly. Two spheres will serve to
 *	illustrate how reference objects may be used to aid in complex
 *	rotations. The angle between the spheres is intended to
 *	exactly represent the angle between the points at the corners
 *	of the cube that are facing us in the 2nd and 4th quadrants.
 *
 *			  +y     cube  sphere1
 *			 ___|___ /    /
 *			|q2 | q1|    o
 *		   -x __|___|___|___________+x             
 *			|q3 | q4|        o        
 *			|___|___|       /
 *			    |    sphere2
 *			   -y     
 *
 *	# Rotate the cube 45 degrees counterclockwise around its
 *	# bounding box center, on the z-axis. The corner in q1 will
 *	# end up on the positive y-axis, intersecting the y/z plane.
 *	rotate -z 45 cube
 *
 * 		# all of these have the same result as above
 * 		rotate 0 0 45 cube
 * 		rotate -c 0 0 0 -d 0 0 45 cube
 * 		rotate -c cube 0 0 45 cube
 * 		rotate -c . 0 0 45 cube
 * 		rotate -c -x . -y . -z . cube -z 45 cube
 * 		rotate -c . -z 17 cube -z 45 cube -z -17
 *
 *		# return it to the last position (clockwise)
 *		rotate -z -45 cube
 *
 *		# return it to its last position (counterclockwise)
 *		rotate -z 315 cube
 *
 *	# Rotate the cube (in 3d) so that the edge facing us in the
 *	# positive y quadrants is rolled directly down and towards us,
 *	# stopping where the edge in the negative y quadrants is now.
 *	rotate 90 cube.s
 *
 *	# Rotate the cube in such a way that the corner facing us in
 *	# the q3 is pointing directly at us (we are on a positive z)
 *	rotate -45 45 cube
 *
 *	# Rotate the cube on its center, from sphere1 to sphere2
 *	# (this just happens to be on the z-axis)
 *	rotate -k sphere1 -a sphere2 cube
 *
 *	    # all of these have the same result as above
 *	    rotate -c cube -k sphere1 -a sphere2 cube
 *	    rotate -c . -k sphere1 -a sphere2 cube
 *	    rotate -k -z 0 -r -z 1 -k sphere1 -a sphere2 cube
 *	    rotate -k -z 0 -a -z 1 -k sphere1 -a sphere2 cube
 *	    rotate -k -z 0 -r -z 1 -c . -k sphere1 -a sphere2 cube
 *
 *	    # all of these rotate it back
 *	    rotate -k -z 1 -r -z -1 -k sphere1 -a sphere2 cube
 *	    rotate -k -z 1 -a -z 0 -k sphere1 -a sphere2 cube
 *	    rotate -k -z 5000 -a -z 23 -k sphere1 -a sphere2 cube
 *
 *          # Note that in the examples where AXIS is specified, even
 *          # if sphere1 and sphere2 had different z-coordinates for
 *          # their centers, the rotate behavior would be the same as
 *          # above. In the examples where AXIS is not specified,
 *          # however, the cube would rotate unconstrained in the
 *          # z-axis as well.
 *
 *      # Rotate the cube around a point 25 units in front of itself
 *      # (the front is the right face in the diagram, sharing edge q1
 *      # and q4 with our top view) so that it faces the opposite
 *      # direction
 *      rotate -c . -x 25 -d 180 cube
 *
 *          # same effect (preferred method)
 *          rotate -a 180 cube -x 25
 *
 *      # Rotate the cube so that face that is facing us, is instead
 *      # facing sphere1 (a 3d rotation). Note that CENTER is using
 *      # its default of OBJECT's bounding box center, and
 *      # ANGLE_CENTER is using its default of CENTER.
 *      rotate -k . -z 1 -a sphere1 cube
 *
 *          # all of these have the same result as above
 *          rotate -k . -z 5981 -a sphere1 cube
 *          rotate -k . -z .01 -a sphere1 cube
 *
 *          # move it back
 *          rotate -k sphere1 -a . -z 1 cube
 *
 *      # Rotate cube around an AXIS from sphere2 to sphere1
 *      rotate -k sphere2 -a sphere1 -c sphere2 -d 180 cube
 *
 *          # same effect as above
 *          rotate -k sphere2 -a sphere1 -c sphere2 -d 180 cube
 *          rotate -k sphere1 -a sphere1 -c sphere1 -d 180 cube
 *
 *          # rotate cube on the same axis, so that it is in front of
 *          # the spheres
 *          rotate -k sphere2 -a wphere1 -c sphere1 -d 90 cube
 *
 *      # Rotate both spheres individually around separate axis,
 *      # so that the same points on each sphere are still facing
 *      # cube, but they are upside down.
 *      rotate -k . -a cube -c . -d 180 sphere1 sphere2
 *
 *      # Rotate both spheres around an axis in the middle of them,
 *      # so that they switch places. Any point on AXIS points could
 *      # have been used as CENTER. If the spheres were at different
 *      # z-coordinates, this would still work.
 *      rotate -k sphere1 -y sphere2 -a sphere2 -y sphere1 -c sphere2
 *          -y sphere1 -d 180 sphere1 sphere2
 *
 *
 */

/*
 * scale: Proposed operations, and manual page
 *
 * NAME
 *	scale (alias for "edit scale")
 *
 * SYNOPSIS
 *	scale [SCALE] [CENTER] FACTOR OBJECT ...
 *	scale [[SCALE_FROM] SCALE_TO] [CENTER] [FACTOR_FROM]
 *	    FACTOR_TO OBJECT ...
 * 	scale [[[-n] -k {SCALE_FROM_OBJECT | SCALE_FROM_POS}]
 * 	    [-n] [-a | -r] {SCALE_TO_OBJECT | SCALE_TO_POS}]
 * 	    [[-n] -c {CENTER_OBJECT | CENTER_POS}]
 * 	    [[-n] -k {FACTOR_FROM_OBJECT | FACTOR_FROM_POS}]
 * 	    [-n] [-a | -r] {FACTOR_TO_OBJECT | FACTOR_TO_POS} OBJECT
 * 	    ...
 *
 *	*OBJECT:
 *	    [PATH/]OBJECT [OFFSET_DIST]
 *	    [PATH/]OBJECT [x [y [z]]]
 *
 *	FACTOR_TO_POS:
 *	    {xyz_factor | {x y [z]}} | {[-x {x | X_OBJ}]
 *	        [-y {y | Y_OBJ}] [-z {z | Z_OBJ}]}
 *
 *	*POS (except FACTOR_TO_POS):
 *	    {x [y [z]]} | {[-x {x | X_OBJ}] [-y {y | Y_OBJ}]
 *	        [-z {z | Z_OBJ}]}
 *
 * DESCRIPTION
 *	Used to enlarge or reduce the size of one or more instances of
 *	primitive or combination objects, by applying a scaling
 *	factor. OBJECTs are scaled by a FACTOR of SCALE, at CENTER.
 *
 *	It is important to note that FACTOR is interpreted as a factor
 *	of SCALE. Although it might appear so, the distance between
 *	SCALE and FACTOR points is irrelevant.
 *
 *	By default, the reference scale SCALE, is from a
 *	SCALE_FROM_POS of (0,0,0) to a SCALE_TO_POS of (1,1,1). Given
 *	these default SCALE values, specifying a FACTOR_TO_POS of
 *	(2,2,2) would double the size of OBJECT, while (0.5,0.5,0.5)
 *	would halve its size.
 *
 *	If SCALE were from a SCALE_FROM_POS of (0,0,0) to a
 *	SCALE_TO_POS of (5,10,15), doubling the size of OBJECT would
 *	require a FACTOR_TO_POS of (10,20,30), or of (2.5,5,7.5) in
 *	order to halve it. Specifying a FACTOR_TO_POS of (30,30,30)
 *	would result in the x-axes of all OBJECT's being stretched to
 *	quintuple their original lengths, y-axes tripled in length,
 *	and z-axes double in length.
 *
 * 	(see DESCRIPTION of translate command for other information)
 *
 * OPTIONS
 * 	-n *_FROM_OBJECT | *_TO_OBJECT
 *	    Use the natural origin of *_FROM_OBJECT and/or
 *	    *_TO_OBJECT, rather than the default of its bounding box
 *	    center.
 *
 * 	-k *_FROM_OBJECT | *_FROM_POS
 *	    Sets the keypoint to *_FROM_OBJECT's bounding box center
 *	    (or natural origin if -n is used). If this option is
 *	    omitted, the keypoint defaults to the first OBJECT's
 *	    bounding box center.
 *
 *	-c CENTER_OBJECT | CENTER_POS
 *	    Set CENTER of where the scale will occur. If omitted,
 *	    CENTER defaults to the bounding box center of the first
 *	    OBJECT. To use the natural origin of the first OBJECT as
 *	    CENTER, you must set CENTER_OBJECT to OBJECT and enable
 *	    the matching -n option. Set CENTER_OBJECT to "." to to
 *	    force each OBJECT to scale from its own center.
 *
 * 	-r *_TO_POS
 *	    Interpret *_TO_POS as the relative distance to scale
 *	    OBJECTs from FROM keypoint. This is the default if TO_POS
 *	    is set.  Must be omitted to specify *_TO_OBJECT is
 *	    specified.
 *
 * 	-a *_TO_POS | *_TO_OBJECT
 *	    Interpret TO_POS/TO_OBJECT as an absolute position. The
 *	    vector implied by FROM and TO is used to scale OBJECT.
 *	    This option is required if TO_OBJECT is specified.
 *
 * EXAMPLES
 *	# All of the following double the size of cube from its center
 *	scale 2 cube
 *	scale 2 2 2 cube
 *	scale -r 2 2 2 cube
 *	scale -r 2 cube
 *	scale -k 0 0 0 -a 2 2 2 cube
 *	scale -k 0 0 0 -a 1 1 1 -c cube -r 2 cube
 *	scale -k 0 0 0 -a 1 1 1 -c . -r 2 cube
 *	scale -k 0 0 0 -a 1 1 1 -c . -a 2 2 2 cube
 *	scale -k 5 10 15 -a 7 11 -2 -r 4 2 34 cube
 *	scale -k 5 10 15 -a 7 11 -2 -k 0 0 0 -a 4 2 34 cube
 *	scale -k 5 10 15 -a 7 11 -2 -k 0 0 0 -r 4 2 34 cube
 *	scale -k 5 10 15 -a 7 11 -2 -k 3 6 9 -r 7 8 43 cube
 *	scale -k 5 10 15 -a 7 11 -2 -c cube -k 3 6 9 -r 7 8 43 cube
 *                                                         
 */

/*
 * translate: Proposed operations, and manual page
 *
 * NAME
 *	translate (alias for "edit translate")
 *
 * SYNOPSIS
 *	translate [FROM] TO OBJECT ...
 * 	translate [[-n] -k {FROM_OBJECT | FROM_POS}]
 * 	    [-n] [-a | -r] {TO_OBJECT | TO_POS} OBJECT ...
 *
 *	*OBJECT:
 *	    [PATH/]OBJECT [OFFSET_DIST]
 *	    [PATH/]OBJECT [x [y [z]]]
 *
 *	*POS:
 *	    {x [y [z]]} | {[-x {x | X_OBJ}] [-y {y | Y_OBJ}]
 *	        [-z {z | Z_OBJ}]}
 *
 * DESCRIPTION
 *	Used to move one or more instances of primitive or combination
 *	objects. The positions of OBJECTs are translated from FROM to
 *	TO.
 *
 *	If FROM is omitted, the bounding box center of the first
 *	[PATH/]OBJECT is used instead. To use the natural origin of
 *	the first [PATH/]OBJECT as FROM, FROM_OBJECT must be manually
 *	set to [PATH/]OBJECT.
 *
 *	If FROM is "-k .", then each individual [PATH/]OBJECT argument
 *	uses its own bounding box center (or natural origin if
 *	"-n -k ." is used). Likewise, if TO is "-a ." or "-n -a ."
 *
 *	FROM_POS and TO_POS represent 3d points in "x y z"
 *	coordinates. To specify one or more specific axis while
 *	ignoring the others, the options "-x x", "-y y", "-z z" may be
 *	used as FROM_POS or TO_POS.
 *
 * OPTIONS
 * 	-n FROM_OBJECT | TO_OBJECT
 *	    Use the natural origin of FROM_OBJECT and/or TO_OBJECT,
 *	    rather than the default of its bounding box center.
 *
 * 	-k FROM_OBJECT | FROM_POS
 *	    Sets the keypoint to FROM_OBJECT's bounding box
 *	    center (or natural origin if -n is used). If this option
 *	    is omitted, the keypoint defaults to OBJECT's bounding
 *	    box center.
 *
 * 	-a TO_POS | TO_OBJECT
 *	    Interpret TO_POS/TO_OBJECT as an absolute position. The
 *	    vector implied by FROM and TO is used to move OBJECT. This
 *	    option is required if TO_OBJECT is specified.
 *
 * 	-r TO_POS
 *	    Interpret TO_POS as the relative distance to move OBJECT
 *          from FROM keypoint. This is the default if TO_POS is set.
 *	    Must be omitted if TO_OBJECT is specified.
 *
 *
 * VISUAL EXAMPLE:
 *	translate -n -k rcc.s -a sph.s table.c/box.c
 *     
 *      Move the instance of box.c in table.c from the natural origin
 *	of rcc.s to the bounding box center of sph.s:
 *
 *	   |.| <=keypoint: natural origin of rcc.s         
 *	                                                   
 *	       o <= center of sph.2                        
 *                                   [] <=box.c start      
 *	                                                   
 *	                                [] <=box.c moved   
 *                                                         
 * EXAMPLES
 *
 * FIXME: These are outdated, and some are just plain wrong.
 *
 *	# move all instances of sph.s to x=1, y=2, z=3
 *	translate -a 1 2 3 /sph.s
 *
 *	    # these all have the same effect as above
 *	    translate -a 1 2 3 sph.s
 *	    translate -k sph.s -a 1 2 3 sph.s
 *	    translate -k . -a 1 2 3 sph.s
 *
 *	# move all instances of sph.s from a point 5 units above
 *	# sph.s's center to x=1, y=2, z=3, by using OFFSET_DIST of
 *	# "-z 5".
 *	translate -a 1 2 3 sph.s -z 5
 *
 *	    # these all have the same effect as above
 *	    translate -k . -z 5 -a 1 2 3 sph.s
 *	    translate -k sph.s -z 5 -a 1 2 3 sph.s
 *
 *	# OFFSET_DIST of "-z 5" is meaningless here, since OBJECT is
 *	# not implicitly used to calculate the translation. This
 *	# produces an error.
 *	#translate -k sph.s -a 1 2 3 sph.s -z 5
 *
 *      # Place sph1.s and sph2.s at the same height as cube.s using
 *      # OFFSET_DIST of Z_OBJ, and leaving x/y positions alone.
 *      translate -a . -z sph.s sph2.s sph3.s
 *
 *      # Move sph1.s and sph2.s to a height offset 20 units above
 *      # cube.s, while leaving x/y positions alone
 *      translate -k . -z -20 -a -z cube.s sph1.s sph2.s
 *
 *          # these all have the same effect as above
 *          translate -k . -z -20 -a . -z cube.s sph1.s sph2.s
 *          translate -k . -a -z cube.s sph1.s -z -20 sph2.s -z -20
 *          translate -k -x . -y . -z -20 -a -z cube.s sph1.s sph2.s
 *          translate -k 6 7 -20 -a -x 6 -y 7 -z cube.s sph1.s sph2.s
 *
 *	# move sph2.s twice as far away from sph.s as it is now
 *	translate -k sph.s -a . sph2.s
 *
 *	# move all instances of sph.s x+1,y+2,z+3
 *	translate 1 2 3 sph.s
 *
 *	    # these all have the same effect as above
 *	    translate -r 1 2 3 sph.s
 *	    translate -k sph.s -r 1 2 3 sph.s
 *	    translate -k . -r 1 2 3 sph.s
 *
 *	# move instance of sph.s in bowl.c x+1,y+2
 *	translate 1 2 bowl.c/sph.s
 *
 *	# move instance of sph.s in bowl.c z+7
 *	translate -z 7 bowl.c/sph.s
 *
 *	    # exactly the same as above (moving x+0 y+0 z+7)
 *	    translate 0 0 7 bowl.c/sph.s
 *
 *      # move all sph.s from the bounding box center of sph.s to
 *	# the natural origin of sph.s
 *	translate -k sph.s -n -a sph.s sph.s
 *
 *	    # these all have the same effect as above
 *	    translate -n -a . sph.s
 *	    translate -k . -n -a . sph.s
 *
 *	# move all instances of bowl.c, from sph.s's bounding
 *	# box center to y=5, without changing the x and z coordinates.
 *	translate -k sph.s -a -y 5 bowl.c
 *
 *      # move instance of two.c, from instance of sph.s's
 *      # matrix-modified natural origin to x=5
 *      translate -n -k bowl.c/sph.s -a 5 one.c/two.c
 *
 *	# move all bowl.c instances and one instance of two.c from
 *	# x=-23,y=4,z=17 to x=9,y=2,z=1
 *	translate -k -23 4 17 -a 9 2 1 bowl.c one.c/two.c
 *
 *	    # exactly the same as above, using relative positioning
 *	    translate -k . -r 32 -2 16 bowl.c one.c/two.c
 *
 *	# these all do nothing
 *	translate -a . sph.s
 *	translate 0 sph.s
 *	translate -k 1 2 3 -a 1 2 3 sph.s
 *	translate -k 1 2 3 -r 0 sph.s
 *	translate -k . -a . sph.s
 *	translate -k sph.s -a sph.s sph.s
 *	translate -n -k . -n -a . sph.s
 *	translate -n -k sph.s -n -a sph.s sph.s
 *    
 *	# center sph1.s and sph2.s on natural origin of rcc.s
 *	translate -k . -n -a rcc.s sph1.s sph2.s
 *
 *      # center sph1.s on natural origin of rcc.s, and move sph.2
 *	# from center of sph1.s to natural origin of rcc.s (keypoint
 *	# is sph1.s's center, so sph2.s comes along for the ride)
 *      translate -n -a rcc.s sph1.s sph2.s
 *
 *	# move each of sph.s, sph2.s and sph3.s a relative x+5
 *	translate -k . -r 5 sph.s sph2.s sph3.s
 *	   
 *	    # same as above
 *	    translate -n -k . -r 5 sph.s sph2.s sph3.s
 *
 *	# move sph.s to a point z+10 above bounding box center of
 *	# /sph2.s
 *	translate -k sph2.s -r -z 10 sph.s
 *	   
 *	# move the bounding box center of all instances of sph.s
 *	# to the natural origin of rcc.s, and move sph2.s from
 *	# the bounding box center of sph.s to the natural origin of
 *	# rcc.s (both sph.s and sph2.s stay the same relative distance
 *	# from each other; they have shifted together)
 *	translate -n -a rcc.s sph.s sph2.s
 *
 *	# move the natural origins of all instances of sph.s
 *	# and sph2.s to the natural origin of rcc.s
 *	translate -n -k . -n -a rcc.s sph.s sph2.s
 *
 *	# move instance of two.c from x=93.2 to x=-41.7
 *	translate -k 93.2 -a -41.7 one.c/two.c
 *
 *	    # all of these have the same end result as above
 *	    translate -k -x 93.2 -a -41.7 one.c/two.c
 *	    translate -k -x 93.2 -a -x -41.7 one.c/two.c
 *	    translate -k 93.2 0 0 -a -41.7 0 0 one.c/two.c
 *	    translate -k 93.2 21 32 -a -41.7 21 32 one.c/two.c
 *
 *	    # same result as above, using a relative distance
 *	    translate -134.9 one.c/two.c
 *	    translate -r -134.9 one.c/two.c
 *	    translate -k . -r -134.9 one.c/two.c
 *
 */

/* Max # of global options + max number of options for a single arg */
#define EDIT_MAX_ARG_OPTIONS 3

/*
 * Use one of these nodes for each argument of the edit subcommands
 * (see manuals)
 */
struct edit_arg {
    struct edit_arg *next; /* nodes rel to arg in cmd args grouping */
    char cl_options[EDIT_MAX_ARG_OPTIONS]; /* unique cmd line opts */
    unsigned int coords_used : 6; /* flag which coords will be used */
    unsigned int type : 7; /* flag the arg type and type modifiers */
    struct db_full_path *object; /* path and obj */
    vect_t *vector; /* abs pos, or offset dist from an obj */
};

/*
 * edit_arg coordinate flags (careful: used in bitshift)
 */

/* edit_arg flags of coordinates that will be used */
#define EDIT_COORD_X 	0x01
#define EDIT_COORD_Y 	0x02
#define EDIT_COORD_Z 	0x04
#define EDIT_COORDS_ALL (EDIT_COORD_X | EDIT_COORD_Y | EDIT_COORD_Z)

/* edit_arg flags of coordinates that are already set; only used by 
 * edit_str[s]_to_arg(). Ignored by edit() */
/* FIXME: these shouldn't be edit_arg flags... edit_strs_to_arg()
 * should just pass edit_str_to_arg() references to automatic vars */
#define EDIT_COORD_IS_SET_X 	0x08
#define EDIT_COORD_IS_SET_Y 	0x10
#define EDIT_COORD_IS_SET_Z 	0x20

/*
 * edit_arg argument type flags
 */

/* argument types */
#define EDIT_FROM			0x01 /* aka keypoint */
#define EDIT_TO				0x02
#define EDIT_TARGET_OBJ			0x04 /* obj to operate on */

/* "TO" argument type modifiers */
#define EDIT_REL_DIST			0x08
#define EDIT_ABS_POS 			0x10

/* object argument type modifiers */
#define EDIT_NATURAL_ORIGIN		0x20 /* use n.o. of object */
#define EDIT_USE_TARGETS		0x40 /* for batch ops */

/* all type modifiers that indicate the arg contains an obj */
#define EDIT_OBJ_TYPE_MODS (EDIT_NATURAL_ORIGIN | EDIT_USE_TARGETS)

/* in batch ops, these flags are not discarded from the target obj */
#define EDIT_TARGET_OBJ_BATCH_TYPES (EDIT_NATURAL_ORIGIN)


/*
 * Arg groupings for each command.
 */
union edit_cmd{
    const struct edit_cmd_tab *cmd;

    struct {
	const struct edit_cmd_tab *padding_for_cmd;
	struct edit_arg *objects;
    } common;

    struct {
	const struct edit_cmd_tab *padding_for_cmd;
	struct edit_arg *args;
    } cmd_line; /* similar to common; used when parsing cl args */

    struct {
	const struct edit_cmd_tab *padding_for_cmd;
	struct edit_arg *objects;
	struct {
	    struct edit_arg *from;
	    struct edit_arg *to;
	} ref_axis;
	struct edit_arg *center;
	struct {
	    struct edit_arg *origin;
	    struct edit_arg *from;
	    struct edit_arg *to;
	} ref_angle;
    } rotate;

    struct {
	const struct edit_cmd_tab *padding_for_cmd;
	struct edit_arg *objects;
	struct {
	    struct edit_arg *from;
	    struct edit_arg *to;
	} ref_scale;
	struct edit_arg *center;
	struct {
	    struct edit_arg *from;
	    struct edit_arg *to;
	} ref_factor;
    } scale;

    struct {
	const struct edit_cmd_tab *padding_for_cmd;
	struct edit_arg *objects;
	struct {
	    struct edit_arg *from;
	    struct edit_arg *to;
	} ref_vector;
    } translate;
};

/**
 * Command specific information, for a table of available commands.
 */
typedef int (*exec_handler)(struct ged *gedp, const union edit_cmd *const cmd);
typedef int (*add_cl_args_handler)(struct ged *gedp, union edit_cmd *const cmd,
				   const int flags);
typedef struct edit_arg ** (*get_arg_head_handler)(
	const union edit_cmd *const cmd, int idx);

struct edit_cmd_tab {
    char *name;
    char *opt_global;
    char *usage;
    char *help;
    exec_handler exec;
    add_cl_args_handler add_cl_args;
    get_arg_head_handler get_arg_head;
};


/* 
 * Argument builder/helper functions
 */


/**
 * Initialize an argument node.
 */
HIDDEN void
edit_arg_init(struct edit_arg *arg)
{
    arg->next = (struct edit_arg *)NULL;
    (void)memset((void *)&arg->cl_options[0], 0, EDIT_MAX_ARG_OPTIONS);
    arg->coords_used = EDIT_COORDS_ALL;
    arg->type = 0;
    arg->object = (struct db_full_path *)NULL;
    arg->vector = (vect_t *)NULL;
}

/**
 * Attach an argument node to the end of the list.
 */
HIDDEN void
edit_arg_postfix(struct edit_arg *head,
		       struct edit_arg *node)
{
    struct edit_arg *pos = head;

    while (pos->next)
	pos = pos->next;
    pos->next = node;
}

/**
 * Allocate space and attach a new node to the end of the list.
 * Returns a pointer to the new node. Caller is responsible for
 * freeing.
 */
HIDDEN struct edit_arg *
edit_arg_postfix_new(struct edit_arg *head)
{
    struct edit_arg *arg;

    BU_GETSTRUCT(arg, edit_arg);
    edit_arg_postfix(head, arg);
    edit_arg_init(arg);
    return arg;
}

/**
 * Duplicate an argument node on top of an existing node. Prior to
 * calling, caller is responsible for freeing objects contained by
 * destination argument node, if necessary, via edit_arg_free_inner().
 */
HIDDEN void
edit_arg_duplicate_in_place(struct edit_arg *const dest,
			    const struct edit_arg *src)
{
    int i;

    edit_arg_init(dest);
    dest->next = (struct edit_arg *)NULL;
    for (i = 0; i < EDIT_MAX_ARG_OPTIONS; ++i)
	dest->cl_options[i] = src->cl_options[i];
    dest->coords_used = src->coords_used;
    dest->type = src->type;
    if (src->object) {
	BU_GETSTRUCT(dest->object, db_full_path);
	db_full_path_init(dest->object);
	db_dup_full_path(dest->object, src->object);
    }
    if (src->vector) {
	dest->vector = (vect_t *)bu_malloc(sizeof(vect_t),
			  "vect_t block for edit_arg_duplicate_in_place()");
	(*dest->vector)[0] = (*src->vector)[0];
	(*dest->vector)[1] = (*src->vector)[1];
	(*dest->vector)[2] = (*src->vector)[2];
    }
}

/**
 * Duplicate an argument node into a new argument. Caller is
 * responsible for freeing destination argument, using the
 * appropriate edit_arg_free*() function.
 */
HIDDEN void
edit_arg_duplicate(struct edit_arg **dest, const struct edit_arg *src)
{
    BU_GETSTRUCT(*dest, edit_arg);
    edit_arg_duplicate_in_place(*dest, src);
}

/**
 * Returns GED_OK if arg is empty, otherwise GED_ERROR is returned
 */
HIDDEN int
edit_arg_is_empty(struct edit_arg *arg)
{
    if (!arg->next &&
	(arg->cl_options[0] == '\0') &&
	(arg->coords_used & EDIT_COORDS_ALL) &&
	(!arg->type) && 
	(!arg->object) &&
	(!arg->vector))
	return GED_OK;
    return GED_ERROR;
}

/**
 * Free all objects contained by an argument node.
 */
HIDDEN void
edit_arg_free_inner(struct edit_arg *arg)
{
    if (arg->object) {
	db_free_full_path(arg->object);
	bu_free((genptr_t)arg->object, "db_string_to_path");
	arg->object = (struct db_full_path*)NULL;
    }
    if (arg->vector) {
	bu_free(arg->vector, "vect_t");
	arg->vector = (vect_t *)NULL;
    }
}

/**
 * Free an argument node, including what it contains.
 */
HIDDEN void
edit_arg_free(struct edit_arg *arg)
{
    edit_arg_free_inner(arg);
    bu_free(arg, "edit_arg");
}

/**
 * Free the last argument node in the list.
 */
HIDDEN void
edit_arg_free_last(struct edit_arg *arg)
{
    struct edit_arg *last_arg = arg;

    do {
	if (!(arg->next))
	    break;
	last_arg = arg;
    } while ((arg = arg->next));
    last_arg->next = (struct edit_arg *)NULL;
    edit_arg_free(arg);
}

/**
 * Free an argument node and all nodes down its list.
 */
HIDDEN void
edit_arg_free_all(struct edit_arg *arg)
{
    if (arg->next)
	edit_arg_free_all(arg->next);
    edit_arg_free(arg);
}

/**
 * Gets the apparent coordinates of an object.
 *
 * Combines the effects of all the transformation matrices in the
 * combinations in the given path that affect the the position of the
 * combination or shape at the end of the path. The result is the
 * apparent coordinates of the object at the end of the path, if the
 * first combination in the path were drawn. The only flags respected
 * are object argument type modifier flags.
 *
 * If the path only contains a primitive, the coordinates of the
 * primitive will be the result of the conversion.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 */
int
edit_arg_to_apparent_coord(struct ged *gedp, const struct edit_arg *const arg,
			   vect_t *const coord)
{
    const struct db_full_path *const path = arg->object;
    struct rt_db_internal intern;
    struct directory *d;
    struct directory *d_next;
    struct rt_comb_internal *comb_i;
    union tree *leaf;
    vect_t leaf_deltas = VINIT_ZERO;
    struct _ged_trace_data gtd;
    point_t rpp_min;
    point_t rpp_max;
    size_t i;

    if (ged_path_validate(gedp, path) == GED_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "path \"%s\" does not exist in"
		      "the database", db_path_to_string(path));
	return GED_ERROR;
    }

    /* sum the transformation matrices of each object in path */
    d = DB_FULL_PATH_ROOT_DIR(path);
    for (i = (size_t)0; i < path->fp_len - (size_t)1; ++i) {
	d_next = DB_FULL_PATH_GET(path, i + (size_t)1);

	/* The path was validated, and this loop doesn't process the
	 * last item, so it must be a combination.
	 */
	BU_ASSERT(d->d_flags & (RT_DIR_COMB | RT_DIR_REGION));

	/* sum transformation matrices */
	GED_DB_GET_INTERNAL(gedp, &intern, d, (fastf_t *)NULL,
			    &rt_uniresource, GED_ERROR);
	comb_i = (struct rt_comb_internal *)intern.idb_ptr;
	leaf = db_find_named_leaf(comb_i->tree, d_next->d_namep);
	BU_ASSERT_PTR(leaf, !=, TREE_NULL); /* path is validated */
	if (leaf->tr_l.tl_mat) {
	    MAT_DELTAS_GET(leaf_deltas, leaf->tr_l.tl_mat);
	    VADD2(*coord, *coord, leaf_deltas);
	}

	rt_db_free_internal(&intern);
	d = d_next; /* prime for next iteration */
    }
    d_next = RT_DIR_NULL; /* none left */

    /* add final combination/primitive natural origin to sum */
    if (d->d_flags & RT_DIR_SOLID) {
	if (_ged_get_obj_bounds2(gedp, 1, (const char **)&d->d_namep, &gtd,
	    rpp_min, rpp_max) == GED_ERROR)
	    return GED_ERROR;
    } else {
	BU_ASSERT(d->d_flags & (RT_DIR_REGION | RT_DIR_COMB));
	if (_ged_get_obj_bounds(gedp, 1, (const char **)&d->d_namep, 1,
	    rpp_min, rpp_max) == GED_ERROR)
	    return GED_ERROR;
    }

    if (arg->type & EDIT_NATURAL_ORIGIN) {
	if (d->d_flags & (RT_DIR_COMB | RT_DIR_REGION)) {
	    bu_vls_printf(gedp->ged_result_str, "combinations do not have a"
		      " natural origin (%s)", d->d_namep );
	    return GED_ERROR;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, d, (fastf_t *)NULL,
			    &rt_uniresource, GED_ERROR);
	if (_ged_get_solid_keypoint(gedp, leaf_deltas, &intern,
	                            (const fastf_t *const)gtd.gtd_xform) ==
				    GED_ERROR) {
	    bu_vls_printf(gedp->ged_result_str, "\nunable to get natural origin"
		      " of \"%s\"", d->d_namep );
	    return GED_ERROR;
	}
	rt_db_free_internal(&intern);
    } else {
	/* bounding box center is the default */
	VADD2SCALE(leaf_deltas, rpp_min, rpp_max, 0.5);
    }

    VADD2(*coord, *coord, leaf_deltas);
    return GED_OK;
}

/**
 * Converts an edit_arg object+offset to coordinates. If *coord is
 * NULL, *arg->vector is overwritten with the coordinates and
 * *arg->object is freed, otherwise coordinates are written to *coord
 * and *arg is in no way modified.
 *
 * The only flags respected are object argument type modifier flags.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 */
int
edit_arg_to_coord(struct ged *gedp, struct edit_arg *const arg, vect_t *coord)
{
    vect_t obj_coord = VINIT_ZERO;
    vect_t **dest;

    if (coord)
	dest = &coord;
    else
	dest = &arg->vector;

    if (edit_arg_to_apparent_coord(gedp, arg, &obj_coord) == GED_ERROR)
	return GED_ERROR;

    if (arg->vector) {
	VADD2(**dest, *arg->vector, obj_coord);
    } else {
	*dest = (vect_t *)bu_malloc(sizeof(vect_t),
				    "vect_t block for edit_arg_to_coord()");
	VMOVE(**dest, obj_coord);
    }

    if (!coord) {
	db_free_full_path(arg->object);
	bu_free((genptr_t)arg->object, "db_full_path");
	arg->object = (struct db_full_path *)NULL;
    }

    return GED_OK;
}

/**
 * "Expands" object arguments for a batch operation.
 *
 * meta_arg is replaced with a list of copies of src_objects, with
 * certain meta_arg flags applied and/or consolidated with those of
 * the source objects. Objects + offsets are converted to coordinates.
 *
 * Set GED_QUIET or GED_ERROR bits in 'flags' to suppress or enable
 * output to ged_result_str, respectively.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 */
HIDDEN int
edit_arg_expand_meta(struct ged *gedp, struct edit_arg *meta_arg,
		const struct edit_arg *src_objs, const int flags)
{
    struct edit_arg *prototype;
    struct edit_arg **dest; 
    const struct edit_arg *src;
    const int noisy = (flags & GED_ERROR); /* side with verbosity */
    int firstrun = 1;

    BU_ASSERT(!meta_arg->next); /* should be at end of list */

    /* repurpose meta_arg, so ptr-to-ptr isn't required */
    edit_arg_duplicate(&prototype, meta_arg);
    edit_arg_free_inner(meta_arg);
    edit_arg_init(meta_arg);
    dest = &meta_arg;

    for (src = src_objs; src; src = src->next, dest = &(*dest)->next) {
	if (firstrun) {
	    firstrun = 0;
	    edit_arg_duplicate_in_place(*dest, src);
	}
	else
	    edit_arg_duplicate(dest, src);

	/* never use coords that target obj didn't include */
	(*dest)->coords_used &= prototype->coords_used;
	if (!((*dest)->coords_used & EDIT_COORDS_ALL)) {
	    if (noisy)
		bu_vls_printf(gedp->ged_result_str,
			      "coordinate filters for batch operator and target"
			      "objects result in no coordinates being used");
	    return GED_ERROR;
	}

	/* respect certain type flags from the prototype/target obj */
	(*dest)->type |= EDIT_TARGET_OBJ_BATCH_TYPES & prototype->type &
			 src->type;
	(*dest)->type |= (EDIT_FROM | EDIT_TO) & prototype->type;

	if (edit_arg_to_coord(gedp, *dest, (vect_t *)NULL) == GED_ERROR)
	    return GED_ERROR;
    }
    return GED_OK;
}


/* 
 * Command helper functions
 */


/**
 * Initialize all command argument-pointer members to NULL.
 */
HIDDEN void
edit_cmd_init(union edit_cmd *const subcmd)
{
    struct edit_arg **arg_head;
    int i = 1;

    arg_head = &subcmd->common.objects;
    do
	*arg_head = (struct edit_arg *)NULL;
    while ((arg_head = subcmd->cmd->get_arg_head(subcmd, i++)) !=
	   &subcmd->common.objects);
}

/**
 * Free any dynamically allocated argument nodes that may exist.
 */
HIDDEN void
edit_cmd_free(union edit_cmd *const cmd)
{
    struct edit_arg **arg_head;
    int i = 0;

    arg_head = cmd->cmd->get_arg_head(cmd, i);
    do {
	if (*arg_head) {
	    edit_arg_free_all(*arg_head);
	    *arg_head = NULL;
	}
    } while ((arg_head = cmd->cmd->get_arg_head(cmd, ++i)) !=
	     &cmd->common.objects);
}

/**
 * Perform a shallow copy of a subcommand's argument grouping.
 */
HIDDEN void
edit_cmd_sduplicate(union edit_cmd *const dest,
		    const union edit_cmd *const src)
{
    struct edit_arg **src_head = NULL;
    struct edit_arg **dest_head;
    int i = 0;

    /* never try to duplicate dissimilar command types */
    BU_ASSERT_PTR(dest->cmd, ==, src->cmd);

    src_head = src->cmd->get_arg_head(src, i);
    do {
	dest_head = dest->cmd->get_arg_head(dest, i);
	*dest_head = *src_head;
    } while ((src_head = src->cmd->get_arg_head(src, ++i)) !=
	     &src->common.objects);
}

/**
 * Sets any skipped vector elements to a reasonable default, sets
 * EDIT_COORDS_ALL on vector arguments, and converts relative offsets
 * to absolute positions. Expects all arguments except target objects
 * to have vector set. Only looks at the first set of args.
 *
 * XXX: This intentionally only looks at the first set of args. At
 * some point, it may be desirable to have the subcommand functions
 * (i.e. edit_<subcmd>_wrapper()) call this prior to execution, so
 * that some commands can expand their own vectors in an unusual ways.
 */
HIDDEN int
edit_cmd_expand_vectors(struct ged *gedp, union edit_cmd *const subcmd)
{
    struct edit_arg **arg_head;
    vect_t src_v = VINIT_ZERO; /* where omitted points draw from */
    vect_t *kp_v = (vect_t *)NULL; /* 'from' point, aka keypoint */
    vect_t *to_v = (vect_t *)NULL; /* 'to' point */
    int i = 0;

    /* draw source vector from target object */
    arg_head = subcmd->cmd->get_arg_head(subcmd, i++);
    if (edit_arg_to_apparent_coord(gedp, *arg_head, &src_v) == GED_ERROR)
	return GED_ERROR;

    while ((arg_head = subcmd->cmd->get_arg_head(subcmd, i++)) != 
	   &subcmd->common.objects) {
	if (!(*arg_head))
	    continue;

	to_v = (*arg_head)->vector;
	if ((*arg_head)->type & EDIT_FROM) {
	    kp_v = (*arg_head)->vector;
	    if (!((*arg_head)->coords_used & EDIT_COORD_X))
		(*to_v)[0] = src_v[0];
	    if (!((*arg_head)->coords_used & EDIT_COORD_Y))
		(*to_v)[1] = src_v[1];
	    if (!((*arg_head)->coords_used & EDIT_COORD_Z))
		(*to_v)[2] = src_v[2];
	} else if ((*arg_head)->type & EDIT_REL_DIST) {
	    /* convert to absolute position */
	    BU_ASSERT(kp_v); /* edit_*_add_cl_args should set this */
	    (*arg_head)->type &= ~EDIT_REL_DIST;
	    if ((*arg_head)->coords_used & EDIT_COORD_X) {
		(*to_v)[0] += (*kp_v)[0];
		(*kp_v)[0] = src_v[0];
	    } else /* no movement */
		(*to_v)[0] = (*kp_v)[0];
	    if ((*arg_head)->coords_used & EDIT_COORD_Y) {
		(*to_v)[1] += (*kp_v)[1];
		(*kp_v)[1] = src_v[1];
	    } else
		(*to_v)[1] = (*kp_v)[1];
	    if ((*arg_head)->coords_used & EDIT_COORD_Z) {
		(*to_v)[2] += (*kp_v)[2];
		(*kp_v)[2] = src_v[2];
	    } else
		(*to_v)[2] = (*kp_v)[2];
	    kp_v = (vect_t *)NULL;
	} else {
	    BU_ASSERT((*arg_head)->type &= ~EDIT_ABS_POS);
	    if (!((*arg_head)->coords_used & EDIT_COORD_X))
		(*to_v)[0] = (*kp_v)[0];
	    if (!((*arg_head)->coords_used & EDIT_COORD_Y))
		(*to_v)[1] = (*kp_v)[1];
	    if (!((*arg_head)->coords_used & EDIT_COORD_Z))
		(*to_v)[2] = (*kp_v)[2];
	}
	(*arg_head)->coords_used |= EDIT_COORDS_ALL;
    }
    return GED_OK;
}

/**
 * Consolidates compatible arguments. If any of the arguments that are
 * being consolidated link to objects, they are converted to
 * coordinates. Only consolidatable arguments flagged with the same
 * argument type (or if the second arg has no type) that are under
 * the same argument head are consolidated.
 *
 * Common objects are left alone if skip_common_objects != 0.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 */
HIDDEN int
edit_cmd_consolidate (struct ged *gedp, union edit_cmd *const subcmd, 
		      int skip_common_objects)
{
    struct edit_arg **arg_head;
    struct edit_arg *prev_arg;
    struct edit_arg *cur_arg;
    struct edit_arg *next_arg;
    int i = 0;

    if (skip_common_objects)
	i = 1;

    while (((arg_head = subcmd->cmd->get_arg_head(subcmd, i++)) != 
	   &subcmd->common.objects) || !skip_common_objects) {
	skip_common_objects = 1;
	prev_arg = *arg_head;
	if (!prev_arg)
	    continue; /* only one element in list */
	for (cur_arg = prev_arg->next; cur_arg; cur_arg = cur_arg->next) {
	    if (((prev_arg->coords_used & EDIT_COORDS_ALL) ^
		(cur_arg->coords_used & EDIT_COORDS_ALL)) &&
		(cur_arg->type == 0 || prev_arg->type == cur_arg->type) &&
		!(cur_arg->type & EDIT_TARGET_OBJ) )  {

		/* It should be impossible to have no coords set. If
		 * one arg has all coords set, it implies that the
		 * other has none set.
		 */
		BU_ASSERT((cur_arg->coords_used & EDIT_COORDS_ALL) !=
			  EDIT_COORDS_ALL);
		BU_ASSERT((prev_arg->coords_used & EDIT_COORDS_ALL) !=
			  EDIT_COORDS_ALL);

		/* convert objects to coords */
		if (cur_arg->object && edit_arg_to_coord(gedp, cur_arg,
		    (vect_t *)NULL) == GED_ERROR)
		    return GED_ERROR;
		if (prev_arg->object && edit_arg_to_coord(gedp, prev_arg,
		    (vect_t *)NULL) == GED_ERROR)
		    return GED_ERROR;

		/* consolidate */
		if (cur_arg->coords_used & EDIT_COORD_X) {
		    prev_arg->coords_used |= EDIT_COORD_X;
		    (*prev_arg->vector)[0] = (*cur_arg->vector)[0];
		}
		if (cur_arg->coords_used & EDIT_COORD_Y) {
		    prev_arg->coords_used |= EDIT_COORD_Y;
		    (*prev_arg->vector)[1] = (*cur_arg->vector)[1];
		}
		if (cur_arg->coords_used & EDIT_COORD_Z) {
		    prev_arg->coords_used |= EDIT_COORD_Z;
		    (*prev_arg->vector)[2] = (*cur_arg->vector)[2];
		}

		/* remove consolidated argument */
		next_arg = cur_arg->next;
		edit_arg_free(cur_arg);
		cur_arg = prev_arg;
		prev_arg->next = next_arg;
	    } else {
		prev_arg = cur_arg; /* the args are incompatible */
	    }
	}
    }
    return GED_OK;
}


/* 
 * Command-specific functions.
 *
 * The translate command will be documented well to introduce the
 * concepts. Documentation of functions for other commands will be
 * minimal, since they are quite similar.
 *
 * To add a new command, you need to:
 *	1) add a struct to the union edit_cmd
 *	2) create 4 functions that
 *		a) add args to build the command
 *		b) get the next arg head in union edit_cmd (trivial)
 *		c) perform the command
 *		d) wrap the command, to alternatively accept a union
 *		   edit_cmd; perform any unusual pre-execution
 *		   changes (trivial)
 *	3) add command data/function pointers to the command table
 */


/**
 * Rotate an object by specifying points.
 */
int
edit_rotate(struct ged *gedp, const vect_t *const axis_from,
	    const vect_t *const axis_to, const vect_t *const center,
	    const vect_t *const angle_origin, const vect_t *const angle_from,
	    const vect_t *const angle_to, const struct db_full_path *const path)
{
    (void)gedp;
    (void)axis_from;
    (void)axis_to;
    (void)center;
    (void)angle_origin;
    (void)angle_from;
    (void)angle_to;
    (void)path;
    return GED_OK;
}

/**
 * Maps edit_arg fields to the subcommand function's arguments and
 * calls it.
 */
int
edit_rotate_wrapper(struct ged *gedp, const union edit_cmd *const cmd)
{
    return edit_rotate(gedp,
	(const vect_t *const)cmd->rotate.ref_axis.from->vector,
	(const vect_t *const)cmd->rotate.ref_axis.to->vector,
	(const vect_t *const)cmd->rotate.center->vector,
	(const vect_t *const)cmd->rotate.ref_angle.origin->vector,
	(const vect_t *const)cmd->rotate.ref_angle.from->vector,
	(const vect_t *const)cmd->rotate.ref_angle.to->vector,
	(const struct db_full_path *const)cmd->rotate.objects->object);
}

/*
 * Add arguments to the command that were built from the cmd line.
 */
int
edit_rotate_add_cl_args(struct ged *gedp, union edit_cmd *const cmd,
			const int flags)
{
    (void)gedp;
    (void)cmd;
    (void)flags;
    return GED_OK;
}

/**
 * Given an pointer to an argument head in the edit_cmd union, this
 * function will return the next argument head in the union.
 */
struct edit_arg **
edit_rotate_get_arg_head(const union edit_cmd *const cmd, int idx)
{
#define EDIT_ROTATE_ARG_HEADS_LEN 7
    const struct edit_arg **arg_heads[EDIT_ROTATE_ARG_HEADS_LEN];

    idx %= EDIT_ROTATE_ARG_HEADS_LEN;

    arg_heads[0] = (const struct edit_arg **)&cmd->rotate.objects;
    arg_heads[1] = (const struct edit_arg **)&cmd->rotate.ref_axis.from;
    arg_heads[2] = (const struct edit_arg **)&cmd->rotate.ref_axis.to;
    arg_heads[3] = (const struct edit_arg **)&cmd->rotate.center;
    arg_heads[4] = (const struct edit_arg **)&cmd->rotate.ref_angle.origin;
    arg_heads[5] = (const struct edit_arg **)&cmd->rotate.ref_angle.from;
    arg_heads[6] = (const struct edit_arg **)&cmd->rotate.ref_angle.to;

    return (struct edit_arg **)arg_heads[idx];
}

/**
 * Scale an object by specifying points.
 */
int
edit_scale(struct ged *gedp, const vect_t *const scale_from,
	   const vect_t *const scale_to, const vect_t *const center,
	   const vect_t *const factor_from, const vect_t *const factor_to,
	   const struct db_full_path *const path)
{
    (void)gedp;
    (void)scale_from;
    (void)scale_to;
    (void)center;
    (void)factor_from;
    (void)factor_to;
    (void)path;
    return GED_OK;
}

/**
 * Maps edit_arg fields to the subcommand function's arguments and
 * calls it.
 */
edit_scale_wrapper(struct ged *gedp, const union edit_cmd *const cmd)
{
    return edit_scale(gedp,
	(const vect_t *const)cmd->scale.ref_scale.from->vector,
	(const vect_t *const)cmd->scale.ref_scale.to->vector,
	(const vect_t *const)cmd->scale.center->vector,
	(const vect_t *const)cmd->scale.ref_factor.from->vector,
	(const vect_t *const)cmd->scale.ref_factor.to->vector,
	(const struct db_full_path *const)cmd->scale.objects->object);
}

/*
 * Add arguments to the command that were built from the cmd line.
 */
int
edit_scale_add_cl_args(struct ged *gedp, union edit_cmd *const cmd,
		       const int flags)
{
    (void)gedp;
    (void)cmd;
    (void)flags;
    return GED_OK;
}

/**
 * Given an pointer to an argument head in the edit_cmd union, this
 * function will return the next argument head in the union.
 */
struct edit_arg **
edit_scale_get_arg_head(const union edit_cmd *const cmd, int idx)
{
#define EDIT_SCALE_ARG_HEADS_LEN 6
    const struct edit_arg **arg_heads[EDIT_SCALE_ARG_HEADS_LEN];

    idx %= EDIT_SCALE_ARG_HEADS_LEN;

    arg_heads[0] = (const struct edit_arg **)&cmd->scale.objects;
    arg_heads[1] = (const struct edit_arg **)&cmd->scale.ref_scale.from;
    arg_heads[2] = (const struct edit_arg **)&cmd->scale.ref_scale.to;
    arg_heads[3] = (const struct edit_arg **)&cmd->scale.center;
    arg_heads[4] = (const struct edit_arg **)&cmd->scale.ref_factor.from;
    arg_heads[5] = (const struct edit_arg **)&cmd->scale.ref_factor.to;

    return (struct edit_arg **)arg_heads[idx];
}

/**
 * Perform a translation on an object by specifying points.
 */
int
edit_translate(struct ged *gedp, const vect_t *const from,
	       const vect_t *const to,
	       const struct db_full_path *const path)
{
    struct directory *d_to_modify = NULL;
    struct directory *d_obj = NULL;
    vect_t delta;
    struct rt_db_internal intern;

    VSUB2(delta, *to, *from);
    VSCALE(delta, delta, gedp->ged_wdbp->dbip->dbi_local2base);
    d_obj = DB_FULL_PATH_CUR_DIR(path);

    if (ged_path_validate(gedp, path) == GED_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "path \"%s\" does not exist in"
		      "the database", db_path_to_string(path));
	return GED_ERROR;
    }

    if (path->fp_len > 1) {
	/* A path was supplied; move obj instance only (obj's CWD
	 * modified).
	 */
	struct rt_comb_internal *comb;
	union tree *leaf_to_modify;

	d_to_modify = DB_FULL_PATH_GET(path, path->fp_len - (size_t)2);
	GED_DB_GET_INTERNAL(gedp, &intern, d_to_modify, (fastf_t *)NULL,
			    &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	leaf_to_modify = db_find_named_leaf(comb->tree, d_obj->d_namep);

	/* path is already validated */
	BU_ASSERT_PTR(leaf_to_modify, !=, TREE_NULL); 
	if (!leaf_to_modify->tr_l.tl_mat) {
	    leaf_to_modify->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t),
	    				  "mat_t block for edit_translate()");
	    MAT_IDN(leaf_to_modify->tr_l.tl_mat);
	}
	MAT_DELTAS_ADD_VEC(leaf_to_modify->tr_l.tl_mat, delta);
    } else {
	/* No path given; move all obj instances (obj's entire tree
	 * modified).
	 */
	struct _ged_trace_data gtd;
	mat_t dmat;
	mat_t emat;
	mat_t tmpMat;
	mat_t invXform;
	point_t rpp_min;
	point_t rpp_max;

	d_to_modify = d_obj;
	if (_ged_get_obj_bounds2(gedp, 1, (const char **)&d_to_modify->d_namep,
				 &gtd, rpp_min, rpp_max) == GED_ERROR)
	    return GED_ERROR;
	if (!(d_to_modify->d_flags & RT_DIR_SOLID) &&
	    (_ged_get_obj_bounds(gedp, 1, (const char **)&d_to_modify->d_namep,
				 1, rpp_min, rpp_max) == GED_ERROR))
	    return GED_ERROR;

	MAT_IDN(dmat);
	MAT_DELTAS_VEC(dmat, delta);

	bn_mat_inv(invXform, gtd.gtd_xform);
	bn_mat_mul(tmpMat, invXform, dmat);
	bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

	GED_DB_GET_INTERNAL(gedp, &intern, d_to_modify, emat,
			    &rt_uniresource, GED_ERROR);
    }

    RT_CK_DB_INTERNAL(&intern);
    GED_DB_PUT_INTERNAL(gedp, d_to_modify, &intern, &rt_uniresource,
			GED_ERROR);
    rt_db_free_internal(&intern);
    return GED_OK;
}

/**
 * Maps edit_arg fields to the subcommand function's arguments and
 * calls it.  Provides an common interface, so that all subcommands
 * can use the same function pointer type. Ignores all edit_arg fields
 * other than vector, and the first object to operate on in the
 * objects edit_arg. Ignores all edit_arg->next arguments.
 */
int
edit_translate_wrapper(struct ged *gedp, const union edit_cmd *const cmd)
{
    return edit_translate(gedp,
	(const vect_t *const)cmd->translate.ref_vector.from->vector,
	(const vect_t *const)cmd->translate.ref_vector.to->vector,
	(const struct db_full_path *const)cmd->translate.objects->object);
}

/**
 * Add arguments to the command that were built from the cmd line.
 * All unique argument pointers in the command should be initialized
 * to NULL before using.
 *
 * Note: This command happens to only accept the standard command line
 * options, so others are ignored.
 */
int
edit_translate_add_cl_args(struct ged *gedp, union edit_cmd *const cmd,
			   const int flags)
{
    const int noisy = (flags & GED_ERROR); /* side with verbosity */
    struct edit_arg *cur_arg = cmd->cmd_line.args;

    BU_ASSERT_PTR(cur_arg, !=, (struct edit_arg *)NULL);

    if (cur_arg->type & EDIT_FROM) {
	/* if there isn't an EDIT_TO, this func shouldn't be called */
	BU_ASSERT_PTR(cur_arg->next, !=, (struct edit_arg *)NULL); 

	/* A 'from' position is set; only flags that were possible
	 * when this function was last updated should be handled.
	 */
	BU_ASSERT(cur_arg->type ^ ~(EDIT_FROM |
				    EDIT_NATURAL_ORIGIN |
				    EDIT_USE_TARGETS));

	/* disallow non-standard opts */
	if (cur_arg->cl_options[0] != '\0') 
	    goto err_option_unknown; 

	cmd->translate.ref_vector.from = cur_arg;
	cur_arg = cmd->cmd_line.args = cmd->cmd_line.args->next;
	cmd->translate.ref_vector.from->next = NULL;
    }

    if ((cur_arg->type & EDIT_TO) || (cur_arg->type == 0)) {
	/* If there isn't an EDIT_TARGET_OBJECT, this func shouldn't
	 * be called.
	 */
	BU_ASSERT_PTR(cur_arg->next, !=, (struct edit_arg *)NULL);

	/* A 'TO' position is set; only flags that were possible when
	 * this function was last updated should be handled.
	 */
	BU_ASSERT(cur_arg->type ^ ~(EDIT_TO |
				    EDIT_NATURAL_ORIGIN |
				    EDIT_REL_DIST |
				    EDIT_ABS_POS |
				    EDIT_USE_TARGETS));

	/* disallow non-standard opts */
	if (cur_arg->cl_options[0] != '\0') 
	    goto err_option_unknown; 

	if (!(cur_arg->type & EDIT_ABS_POS)) {
	    /* interpret 'TO' arg as a relative distance by default */
	    cur_arg->type |= EDIT_REL_DIST;

	    if (cur_arg->object) {
		if (noisy)
		    bu_vls_printf(gedp->ged_result_str,
				  "cannot use a reference object's coordinates"
				  " as an offset distance");
		return GED_ERROR;
	    }
	}

	cmd->translate.ref_vector.to = cur_arg;
	cur_arg = cmd->cmd_line.args = cmd->cmd_line.args->next;
	cmd->translate.ref_vector.to->next= NULL;
    } else {
	if (noisy) {
	    if (cur_arg->type & EDIT_FROM)
		bu_vls_printf(gedp->ged_result_str,
			      "too many \"FROM\" arguments");
	    else
		bu_vls_printf(gedp->ged_result_str, "missing \"TO\" argument");
	}
	return GED_ERROR;
    }

    /* all that should be left is target objects; validate them */
    do {
	if (!(cur_arg->type & EDIT_TARGET_OBJ)) {
	    if (noisy)
		bu_vls_printf(gedp->ged_result_str, "invalid syntax\n"
			      "Usage: %s [help] | %s",
			      cmd->cmd->name, cmd->cmd->usage);
	    return GED_ERROR;
	} else {
	    /* a target obj is set; only flags that were possible when
	     * this function was last updated should be handled
	     */
	    BU_ASSERT(cur_arg->type ^ ~(EDIT_TARGET_OBJ |
					EDIT_NATURAL_ORIGIN));
	}

	/* disallow non-standard opts */
	if (cur_arg->cl_options[0] != '\0')
	    goto err_option_unknown;
    } while ((cur_arg = cur_arg->next));

    /* the default 'FROM' is the first target object */
    if (!cmd->translate.ref_vector.from) {
	edit_arg_duplicate(&cmd->translate.ref_vector.from,
			   cmd->translate.objects);
	cmd->translate.ref_vector.from->type &= ~EDIT_TARGET_OBJ;
	cmd->translate.ref_vector.from->type |= EDIT_FROM;
    }

    return GED_OK;

err_option_unknown:
    if (noisy)
	bu_vls_printf(gedp->ged_result_str, "unknown option \"-%c\"",
		      cur_arg->cl_options[0]);
    return GED_ERROR;
}

/**
 * Given an pointer to an argument head in the edit_cmd union, this
 * function will return the next argument head in the union.
 *
 * This function is used to traverse a commands arguments, without
 * needing to know their structure. edit_cmd.common.objects should be
 * used as the first argument head, to traverse the entire struct.
 *
 * XXX: Kind of dirty; haven't found a better way yet, though.
 */
struct edit_arg **
edit_translate_get_arg_head(const union edit_cmd *const cmd, int idx)
{
#define EDIT_TRANSLATE_ARG_HEADS_LEN 3
    const struct edit_arg **arg_heads[EDIT_TRANSLATE_ARG_HEADS_LEN];

    idx %= EDIT_TRANSLATE_ARG_HEADS_LEN;
    
    arg_heads[0] = (const struct edit_arg **)&cmd->translate.objects;
    arg_heads[1] = (const struct edit_arg **)&cmd->translate.ref_vector.from;
    arg_heads[2] = (const struct edit_arg **)&cmd->translate.ref_vector.to;

    return (struct edit_arg **)arg_heads[idx];
}

/* 
 * Table of edit command data/functions
 */
static const struct edit_cmd_tab edit_cmds[] = {
    {"help", (char *)NULL, "[subcommand]", "[subcommand]", NULL, NULL, NULL},
#define EDIT_CMD_HELP 0 /* idx of "help" in edit_cmds */
    {"rotate",		"R",
	"[-R] [AXIS] [CENTER] ANGLE OBJECT ...",
  	"[-R] [[[-n] -k {AXIS_FROM_OBJECT | AXIS_FROM_POS}]\n"
	"[[-n] [-a | -r] {AXIS_TO_OBJECT | AXIS_TO_POS}]]\n"
	    "[[-n] -c {CENTER_OBJECT | CENTER_POS}]\n"
	    "[[-n] -O {ANGLE_ORIGIN_OBJECT| ANGLE_ORIGIN_POS}]\n"
	    "[[-n] -k {ANGLE_FROM_OBJECT | ANGLE_FROM_POS}]\n"
	    "[-n | -o] [-a | -r | -d]"
		"{ANGLE_TO_OBJECT | ANGLE_TO_POS}} OBJECT ...",
	&edit_rotate_wrapper,
	&edit_rotate_add_cl_args,
	&edit_rotate_get_arg_head
    },
    {"scale",		(char *)NULL,
	"[SCALE] [CENTER] FACTOR OBJECT ...",
	"[[[-n] -k {SCALE_FROM_OBJECT | SCALE_FROM_POS}]\n"
	    "[-n] [-a | -r] {SCALE_TO_OBJECT | SCALE_TO_POS}]\n"
	    "[[-n] -c {CENTER_OBJECT | CENTER_POS}]\n"
	    "[[-n] -k {FACTOR_FROM_OBJECT | FACTOR_FROM_POS}]\n"
	    "[-n] [-a | -r] {FACTOR_TO_OBJECT | FACTOR_TO_POS}"
		" OBJECT ...",
	&edit_scale_wrapper,
	&edit_scale_add_cl_args,
	&edit_scale_get_arg_head
    },
    {"translate",	(char *)NULL,
	"[FROM] TO OBJECT ...",
	"[[-n] -k {FROM_OBJECT | FROM_POS}]\n"
	    "[-n] [-a | -r] {TO_OBJECT | TO_POS} OBJECT ...",
	&edit_translate_wrapper,
	&edit_translate_add_cl_args,
	&edit_translate_get_arg_head
    },
    {(char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, NULL, NULL,
	NULL}
};


/* 
 * Command agnostic functions
 */


/**
 * A wrapper for the edit commands. It adds the capability to
 * perform batch operations, and accepts objects and distances in
 * addition to coordinates. As a side effect, arguments flagged
 * with EDIT_USE_TARGETS will be replaced (expanded) in performing
 * batch commands.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 *
 * Note that this function ignores most argument type flags, since
 * it's expected that all args will be in the proper locations in the
 * given command struct. An exception is made for
 * EDIT_TARGET_OBJ_BATCH_TYPES, which is respected since certain flags
 * may propagate in batch operations. Coordinate flags are always
 * respected.
 *
 * If batch arguments are to be used, they should be either:
 *     a) the first and only argument in the given argument head
 *       or
 *     b) expanded prior to being passed to this function
 * This is because an expansion of a batch character leads to an
 * argument head having any prior nodes, *plus* the expanded nodes.
 * If there were any prior nodes, the expansion would cause a
 * lopsided set of arguments, which should result in a BU_ASSERT
 * failure.
 */
int
edit(struct ged *gedp, union edit_cmd *const subcmd)
{
    struct edit_arg **arg_head;
    struct edit_arg *prev_arg; 
    struct edit_arg *cur_arg;
    union edit_cmd subcmd_iter; /* to iterate through subcmd args */
    int i = 0;
    int num_target_objs = 0;
    int num_args_set = 0;

    /* Check all arg nodes in subcmd->common.objects first. They may
     * be copied later, if there are any batch modifiers to expand.
     */
    arg_head = subcmd->cmd->get_arg_head(subcmd, i++);
    for (cur_arg = *arg_head; cur_arg; cur_arg = cur_arg->next) {
	/* target objects must be... objects */
	BU_ASSERT(cur_arg->object);

	/* cmd line opts should have been handled/removed */
	BU_ASSERT(cur_arg->cl_options[0] == '\0');

	++num_target_objs;
    }

    /* process all other arg nodes */
    while ((arg_head = subcmd->cmd->get_arg_head(subcmd, i++)) != 
	   &subcmd->common.objects) {
	num_args_set = 0;
	prev_arg = NULL;
	for (cur_arg = *arg_head; cur_arg; cur_arg = cur_arg->next) {

	    /* cmd line opts should have been handled/removed */
	    BU_ASSERT(cur_arg->cl_options[0] == '\0');
	    prev_arg = cur_arg;

	    if (cur_arg->type & EDIT_USE_TARGETS) {
		if (edit_arg_expand_meta(gedp, cur_arg, subcmd->common.objects,
					 GED_ERROR) == GED_ERROR)
		    return GED_ERROR;
		num_args_set += num_target_objs;
		break; /* batch opertor should be last arg */
	    }
	    if (cur_arg->object) {
		if (edit_arg_to_coord(gedp, cur_arg, (vect_t *)NULL) ==
		    GED_ERROR)
		    return GED_ERROR;
	    }
	    ++num_args_set;
	}

	/* All argument lists should be the same length as the common
	 * objects list. Duplicate the last argument until this is so.
	 */
	if (prev_arg) {
	    while (num_args_set < num_target_objs) {
		edit_arg_duplicate(&prev_arg->next, prev_arg);
		prev_arg = prev_arg->next;
		++num_args_set;
	    }
	    BU_ASSERT(num_args_set == num_target_objs);
	}
    }

    /* Create a copy to iterate over groups of batch args; note that
     * the copy is shallow and *must not be freed*.
     */
    subcmd_iter.cmd = subcmd->cmd;
    edit_cmd_init(&subcmd_iter);
    edit_cmd_sduplicate(&subcmd_iter, subcmd);

    /* Iterate over each set of batch args, executing the subcmd once
     * for each level of edit_arg nodes. The number of common objects
     * determines how many times the command is run.
     */
    do {
	if (edit_cmd_expand_vectors(gedp, &subcmd_iter) == GED_ERROR)
	    return GED_ERROR;
	if (subcmd_iter.cmd->exec(gedp, &subcmd_iter) == GED_ERROR)
	    return GED_ERROR;

	/* set all heads to the next arguments in their lists */
	num_args_set = 0;
	i = 0; /* reinit for get_arg_head() */
	while (((arg_head =
		subcmd_iter.cmd->get_arg_head(&subcmd_iter, i)) !=
		&subcmd_iter.common.objects) || (i == 0)) {
	    ++i;
	    if (*arg_head && (*arg_head)->next) {
		    *arg_head = (*arg_head)->next;
		    ++num_args_set;
	    }
	}
    } while (num_args_set != 0);

    return GED_OK;
}

/**
 * Converts a string to an existing edit_arg. See subcommand manuals
 * for examples of acceptable argument strings.
 * 
 * Set GED_QUIET or GED_ERROR bits in 'flags' to suppress or enable
 * output to ged_result_str, respectively.
 *
 * Returns GED_ERROR on failure, and GED_OK on success.
 */
HIDDEN int
edit_str_to_arg(struct ged *gedp, const char *str, struct edit_arg *arg,
		const int flags)
{
    const int noisy = (flags & GED_ERROR); /* side with verbosity */
    char const *first_slash = NULL;
    char *endchr = NULL; /* for strtod's */
    vect_t coord;

    /*
     * Here is how numbers that are also object names are interpreted:
     * if an object is not yet set in *arg, try to treat the number as
     * an object first. If the user has an object named, say '5', they
     * can avoid using the object named 5 by specifying 5.0 or
     * something. If an arg->object has already been set, then the
     * number was most likely intended to be an offset, so interpret
     * it as as such.
     */

    if (!arg->object && !(arg->type & EDIT_USE_TARGETS) && !arg->vector) {

	/* batch operators; objects with same name will be masked */
	if (BU_STR_EQUAL(str, ".")) {
	    arg->type |= EDIT_USE_TARGETS;
	    return GED_OK;
	}

	/* an arg with a slash is always interpreted as a path */
	first_slash = strchr(str, '/');
	if (first_slash) {
	    char const *path_start;
	    char const *path_end;

	    /* position start after leading slashes */
	    path_start = str;
	    while (path_start[0] == '/')
		++path_start;

	    /* position end before trailing slashes */
	    path_end = path_start + strlen(path_start) - (size_t)1;
	    while (path_end[0] == '/')
		--path_end;

	    if (path_end < path_start) {
		/* path contained nothing but '/' char(s) */
		if (noisy)
		    bu_vls_printf(gedp->ged_result_str, "cannot use root path "
				  "alone");
		return GED_ERROR;
	    }

	    /* detect >1 inner slashes */
	    first_slash = (char *)memchr((void *)path_start, '/',
					 (size_t)(path_end - path_start + 1));
	    goto convert_obj;
	}

	/* it may still be an obj, so quietly check db for obj name */
	if (db_lookup(gedp->ged_wdbp->dbip, str, LOOKUP_QUIET) != RT_DIR_NULL)
	    goto convert_obj;
    }

    /* if string is a number, fall back to interpreting it as one */
    coord[0] = strtod(str, &endchr);
    if (*endchr != '\0') {
	if (noisy)
	    bu_vls_printf(gedp->ged_result_str, "unrecognized argument, \"%s\"",
			  str);
	return GED_ERROR;
    }
    if (!arg->vector)
	arg->vector = (vect_t *)bu_malloc(sizeof(vect_t),
					  "vect_t block for edit_str_to_arg");

    /* Attempt to interpret/record the number as the next unset X, Y,
     * or Z coordinate/position.
     */
    if (((arg->coords_used & EDIT_COORDS_ALL) == EDIT_COORDS_ALL) ||
	arg->object || (arg->type & EDIT_USE_TARGETS)) {

	/* set the first coordinate that isn't set */
	if (!(arg->coords_used & EDIT_COORD_IS_SET_X)) {
	    (*arg->vector)[0] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_X;
	} else if (!(arg->coords_used & EDIT_COORD_IS_SET_Y)) {
	    (*arg->vector)[1] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_Y;
	} else if (!(arg->coords_used & EDIT_COORD_IS_SET_Z)) {
	    (*arg->vector)[2] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_Z;
	} else {
	    if (noisy)
		bu_vls_printf(gedp->ged_result_str, "too many consecutive"
			      " coordinates: %f %f %f %f ...",
			      (*arg->vector)[0], (*arg->vector)[1],
			      (*arg->vector)[2], coord[0]);
	    return GED_ERROR;
	}
    } else {
	/* only set specified coord */
	BU_ASSERT((arg->coords_used & EDIT_COORDS_ALL) != 0);
	if (arg->coords_used & EDIT_COORD_X) {
	    (*arg->vector)[0] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_X;
	} else if (arg->coords_used & EDIT_COORD_Y) {
	    (*arg->vector)[1] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_Y;
	} else if (arg->coords_used & EDIT_COORD_Z) {
	    (*arg->vector)[2] = coord[0];
	    arg->coords_used |= EDIT_COORD_IS_SET_Z;
	}
    }
    return GED_OK;

convert_obj:
    /* convert string to path/object */
    BU_GETSTRUCT(arg->object, db_full_path);
    if (db_string_to_path(arg->object, gedp->ged_wdbp->dbip,
			   str)) {
	db_free_full_path(arg->object);
	bu_free((genptr_t)arg->object, "db_string_to_path");
	arg->object = (struct db_full_path *)NULL; 
	if (noisy)
	    bu_vls_printf(gedp->ged_result_str, "one of the objects in"
			  " the path \"%s\" does not exist", str);
	return GED_ERROR;
    }
    if (ged_path_validate(gedp, arg->object) == GED_ERROR) {
	db_free_full_path(arg->object);
	bu_free((genptr_t)arg->object, "db_string_to_path");
	arg->object = (struct db_full_path *)NULL; 
	if (noisy)
	    bu_vls_printf(gedp->ged_result_str, "path \"%s\" does not exist in"
						"the database", str);
	return GED_ERROR;
    }
    return GED_OK;
}

/**
 * Converts as much of an array of strings as possible to a single
 * edit_arg. Both argc and argv are modified to be past the matching
 * arguments. See subcommand manuals for examples of acceptable
 * argument strings.
 * 
 * Set GED_QUIET or GED_ERROR bits in 'flags' to suppress or enable
 * output to ged_result_str, respectively. Note that output is always
 * suppressed after the first string is successfully converted.
 *
 * Returns GED_OK if at least one string is converted, otherwise
 * GED_ERROR is returned.
 *
 */
HIDDEN int
edit_strs_to_arg(struct ged *gedp, int *argc, const char **argv[],
		 struct edit_arg *arg, int flags)
{
    int ret = GED_ERROR;
    int len;
    int idx;

    while (*argc >= 1) {

	/* skip options */
	idx = 0;
	len = strlen((*argv)[0]);
	if ((**argv)[0] == '-') {
	    if (len == 2 && !isdigit((**argv)[1]))
		break;
	    if (len > 2 && !isdigit((**argv)[1]))
		/* option/arg pair with no space, i.e. "-ksph.s" */
		idx = 2;
	}

	if (edit_str_to_arg(gedp, &(**argv)[idx], arg, flags) == GED_ERROR)
	    break;

	ret = GED_OK;
	flags = GED_QUIET; /* only first conv attempt can be noisy */
	--(*argc);
	++(*argv);
    }

    /* If all EDIT_COORDS_ALL are set, and at least one other flag, it
     * implies that the coords were in the '[x [y [z]]]" format. Set
     * coords used to whatever coords are actually set.
     */
    if (((arg->coords_used & EDIT_COORDS_ALL) == EDIT_COORDS_ALL) &&
	(arg->coords_used & ~EDIT_COORDS_ALL))
	arg->coords_used >>= 3;

    /* these flags are only for internal use */
    /* FIXME: exactly why they should be internalized, and not
     * edit_arg flags */
    arg->coords_used &= ~(EDIT_COORD_IS_SET_X | EDIT_COORD_IS_SET_Y |
			  EDIT_COORD_IS_SET_Z);
    return ret;
}

/**
 * A command line interface to the edit commands. Will handle any new
 * commands without modification. Validates as much as possible in a
 * single pass, without going back over the arguments list. Further,
 * more specific, validation is performed by edit().
 */
int
ged_edit(struct ged *gedp, int argc, const char *argv[])
{
    const char *const cmd_name = argv[0];
    union edit_cmd subcmd;
    const char *subcmd_name = NULL;
    struct edit_arg *cur_arg;
    struct edit_arg *keypoint;
    static const char *const usage = "[subcommand] [args]";
    int idx_cur_opt = 0; /* pos in options array for current arg */
    int conv_flags = 0; /* for edit_strs_to_arg */
    int i; /* iterator */
    int c; /* for bu_getopt */
    int allow_subopts = 0; /* false(=0) when a subopt is bad syntax */
    int ret;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* initialize the subcommand */
    subcmd.cmd = (const struct edit_cmd_tab *)NULL;

    /*
     * Validate command name
     */
    
    /* FIXME: usage/help messages for subcommands should contain the
     * name of the 'parent' command when necessary: i.e.: 'edit
     * translate' rather than just 'translate'. Also, the help option
     * of subcommands is not displayed properly; it should display
     * when command is called directly, i.e. 'translate', but not
     * 'edit translate'
     */

    for (i = 0; edit_cmds[i].name; ++i) {
	/* search for command name in the table */
	if (BU_STR_EQUAL(edit_cmds[i].name, cmd_name)) {
	    subcmd_name = cmd_name; /* saves a strcmp later */
	    subcmd.cmd = &edit_cmds[i];
	    /* match of cmd name takes precedence over match of subcmd
	     * name
	     */
	    break; 
	}
	/* interpret first arg as a cmd, and search for it in table */
	if (!subcmd_name && argc > 1 &&
	    BU_STR_EQUAL(edit_cmds[i].name, argv[1])) {
	    subcmd_name = argv[1];
	    subcmd.cmd = &edit_cmds[i];
	}
    }

    if (subcmd_name == cmd_name) { /* ptr cmp */
	/* command name is serving as the subcommand */
	--argc;
	++argv;
    } else if (subcmd.cmd) {
	/* first arg is the subcommand */
	argc -= 2;
	argv += 2;
    } else {
	/* no subcommand was found */
	ret = GED_HELP;

	if (argc > 1) {
	    /* no arguments accepted without a subcommand */
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand \"%s\"\n",
	    		  argv[1]);
	    ret = GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s"
		      "\nAvailable subcommands: ", cmd_name, usage);
	for (i = 0; edit_cmds[i].name; ++i)
	    bu_vls_printf(gedp->ged_result_str, "%s ", edit_cmds[i].name);
	return ret;
    }

    /*
     * Subcommand help system
     */

    /* switch on idx of subcmd.cmd in edit_cmds[] */
    switch (subcmd.cmd - edit_cmds) { 
	case EDIT_CMD_HELP:
	    if (argc == 0) {
		/* get generic help */
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n",
			      subcmd.cmd->name, subcmd.cmd->usage);
		bu_vls_printf(gedp->ged_result_str, "Available subcommands: ");
		for (i = 0; edit_cmds[i].name; ++i)
		    bu_vls_printf(gedp->ged_result_str, "%s ",
				  edit_cmds[i].name);
		return GED_HELP;
	    } else {
		/* get long usage string for a specific command */
		for (i = 0; edit_cmds[i].name; ++i)
		    /* search for command name in the table */
		    if (BU_STR_EQUAL(edit_cmds[i].name, argv[0]))
			break; /* for loop */

		if (!edit_cmds[i].name) {
		    bu_vls_printf(gedp->ged_result_str,
		   		  "unknown subcommand \"%s\"\n",
				  argv[0]);
		    bu_vls_printf(gedp->ged_result_str,
				  "Available subcommands: ");
		    for (i = 0; edit_cmds[i].name; ++i)
			bu_vls_printf(gedp->ged_result_str, "%s ",
				      edit_cmds[i].name);
		    return GED_ERROR;
		} else /* point to the cmd we want help for */
		    subcmd.cmd = &edit_cmds[i];
		goto get_full_help;
	    }
	default:
	    if (argc == 0) { 
		/* no args to subcommand; must want usage */
		bu_vls_printf(gedp->ged_result_str, "Usage: %s [help] | %s",
			      subcmd.cmd->name, subcmd.cmd->usage);
		return GED_HELP;
	    }

	    /* Handle "subcmd help" (identical to "help subcmd"),
	     * but only if there are no more args; would rather mask
	     * the help system with an object that happened to be
	     * named "help" than the other way around.  This syntax is
	     * needed to access the help system when a subcmd is the
	     * calling command.
	     */
	    if (argc == 1 &&
		BU_STR_EQUAL(edit_cmds[EDIT_CMD_HELP].name,
		argv[0])) {
		goto get_full_help;
	    }
	    break;

get_full_help:
    bu_vls_printf(gedp->ged_result_str,
		  "Usage: %s [help] | %s\n\n%s [help] | %s",
		  subcmd.cmd->name, subcmd.cmd->usage,
		  subcmd.cmd->name, subcmd.cmd->help);
    return GED_HELP;
    }

    /* Now that the cmd type is known (and wasn't "help"), we can
     * initialize the subcmd's arguments. From here on out,
     * edit_cmd_free() must be called before exiting.
     */
    edit_cmd_init(&subcmd);

    BU_GETSTRUCT(subcmd.cmd_line.args, edit_arg);
    edit_arg_init(subcmd.cmd_line.args);
    cur_arg = subcmd.cmd_line.args;

    /*
     * Parse all subcmd args and pass to edit()
     */

    /*
     * The object of the game is to remain agnostic of unique aspects
     * of argument structures of subcommands (i.e. inside the edit_cmd
     * union). Therefore, we will simply link all of the arguments
     * together with edit_arg structs for fuller parsing elsewhere.
     *
     * There are, however, a several things that are standardized for
     * each command (and for any newly added command).
     *     1) interpretation of nonstandard options:
     *        a) nonstandard options (not -k/-a/-r/-x/-y/-z)
     *            that *precede* either an argument specifier
     *            (-k/-a/-r) or an option (-n), modify the argument
     *            specifier's interpretation
     *        b) all other nonstandard options are considered argument
     *           specifiers themselves
     *        c) unique global options are not recognized at this
     *           point; therefore, they will be recorded in the same
     *           way as nonstandard options that modify argument
     *           specifiers (a, above), and parsed by the subcommand's
     *           routines.
     *     2) keypoints ('FROM' arg options) are always preceded by
     *        '-k'
     *     3) keypoints are all considered optional as far as this
     *        function is concerned
     *     4) if a keypoint is specified, the argument is the first in
     *        a pair; therefore a matching 'TO' argument is required
     *     5) any object specification string may:
     *        a) contain a full path:[PATH/]OBJECT
     *        b) use an offset: [x [y [z]]] ('-x' style not supported
     *           in this context)
     *        c) be used for a specific coordinate: -x/-y/-z OBJECT
     *        d) be used for a specific coordinate and use an offset
     *           simultaneously; ex: -z OBJECT 5 0 2 (note that 5 and
     *           0 are merely placeholders in this context)
     *     6) the batch object substitution character may be used as
     *        an argument to -k/-a/-r ("." at the time of writing)
     *     7) at least one object must be operated on
     *     8) the last argument is a list of objects to be operated on
     */

    /* no options are required by default, so quietly look for args */
    while (edit_strs_to_arg(gedp, &argc, &argv, cur_arg, GED_QUIET) !=
	   GED_ERROR) {
	if (argc == 0) {
	    if (edit_arg_is_empty(subcmd.cmd_line.args) == GED_OK) {
		edit_arg_free(subcmd.cmd_line.args);
		subcmd.cmd_line.args = NULL;
	    }

	    cur_arg = subcmd.cmd_line.args;
	    if (cur_arg && cur_arg->next) {
		cur_arg->type |= EDIT_TO;
		cur_arg = cur_arg->next;
	    }

	    /* parsing is done and there were no options, so all args
	     * after the first arg should be a target obj
	     */
	    for (; cur_arg; cur_arg = cur_arg->next) {
		if (!(cur_arg->object)) {
		    bu_vls_printf(gedp->ged_result_str,
				  "expected only objects after first arg");
		    edit_cmd_free(&subcmd);
		    return GED_ERROR;
		}
		cur_arg->type |= EDIT_TARGET_OBJ;
	    }

	    /* let cmd specific func validate/move args to proper
	     * locations
	     */
	    if (subcmd.cmd->add_cl_args(gedp, &subcmd, GED_ERROR) ==
		GED_ERROR)
		return GED_ERROR;
	    ret = edit(gedp, &subcmd);
	    edit_cmd_free(&subcmd);
	    return ret;
	}
	cur_arg = edit_arg_postfix_new(subcmd.cmd_line.args);
    }

    /* All leading args are parsed. If we're not not at an option,
     * then there is a bad arg. Let the conversion function cry about
     * what it choked on.
     */
    if (strlen(argv[0]) > 1 && ((*argv)[0] != '-') && isdigit((*argv)[1])) {
	ret = edit_strs_to_arg(gedp, &argc, &argv, cur_arg, GED_ERROR);
	edit_cmd_free(&subcmd);
	BU_ASSERT(ret == GED_ERROR);
	return GED_ERROR;
    }

    bu_optind = 1; /* re-init bu_getopt() */
    bu_opterr = 0; /* suppress errors; accept unknown options */
    ++argc; /* bu_getopt doesn't expect first element to be an arg */
    --argv;
    while ((c = bu_getopt(argc, (char * const *)argv, ":n:k:a:r:x:y:z:"))
	   != -1) {
	if (bu_optind >= argc) {
	    /* last element is an option */
	    bu_vls_printf(gedp->ged_result_str,
			  "No OBJECT provided; nothing to operate on");
	    edit_cmd_free(&subcmd);
	    return GED_ERROR;
	}
	if (idx_cur_opt >= EDIT_MAX_ARG_OPTIONS) {
	    bu_vls_printf(gedp->ged_result_str, "too many options given, \"");
	    for (i = 0; i < EDIT_MAX_ARG_OPTIONS; ++i)
		bu_vls_printf(gedp->ged_result_str, "-%c ",
			      cur_arg->cl_options[i]);
	    bu_vls_printf(gedp->ged_result_str, "-%c\"", c);
	    edit_cmd_free(&subcmd);
	    return GED_ERROR;
	}

	conv_flags = GED_ERROR;
	switch (c) {
	    case 'n': /* use natural coordinates of object */
	    	conv_flags = GED_QUIET;
		allow_subopts = 0;
		break;
	    case 'x': /* singular coord specif. sub-opts */
	    case 'y':
	    case 'z':
		idx_cur_opt = 0;
	    	if (!bu_optarg)
		    goto err_missing_arg;
		if ((strlen(bu_optarg) > 1) && (bu_optarg[0] == '-') &&
		    (!isdigit(bu_optarg[1])))
		    goto err_missing_arg;
		if (!allow_subopts) {
		    bu_vls_printf(gedp->ged_result_str, "-%c must follow an"
				  " argument specification option", c);
		    edit_cmd_free(&subcmd);
		    return GED_ERROR;
		}
		break;
	    case 'k': /* standard arg specification options */
	    case 'a':
	    case 'r':
		idx_cur_opt = 0;
		allow_subopts = 1;
	    	if (!bu_optarg)
		    goto err_missing_arg;
		if ((strlen(bu_optarg) > 1) && (bu_optarg[0] == '-')) {
		    switch (bu_optarg[1]) {
			case 'x': 
			case 'y':
			case 'z':
			    /* the only acceptable sub-options here */
			    conv_flags = GED_QUIET;
			    break;
			default:
			    if (!isdigit(bu_optarg[1]))
				goto err_missing_arg;
		    }
		}
		break;
	    case '?': /* nonstandard or unknown option */
		allow_subopts = 1;
	        c = bu_optopt;
	    	if (!bu_optarg)
		if (!isprint(c)) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Unknown option character '\\x%x'", c);
		    edit_cmd_free(&subcmd);
		    return GED_ERROR;
		}

		/* next element may be an arg */
	    	conv_flags = GED_QUIET;

		/* record opt for validation/processing by subcmd */
		cur_arg->cl_options[idx_cur_opt] = c;
		++idx_cur_opt;
		break;
	    case ':':
	        goto err_missing_arg;
	}

	/* set flags for standard options */
	switch (c) {
	    case 'x':
		cur_arg->coords_used &= EDIT_COORD_X;
		break;
	    case 'y':
		cur_arg->coords_used &= EDIT_COORD_Y;
		break;
	    case 'z':
		cur_arg->coords_used &= EDIT_COORD_Z;
		break;
	    case 'k':
	        cur_arg->type |= EDIT_FROM;
	        keypoint = cur_arg;
		break;
	    case 'a':
	        cur_arg->type |= EDIT_TO | EDIT_ABS_POS;
		keypoint = NULL;
		break;
	    case 'r':
	        cur_arg->type |= EDIT_TO | EDIT_REL_DIST;
		keypoint = NULL;
		break;
	    case 'n':
	        cur_arg->type |= EDIT_NATURAL_ORIGIN;
		break;
	    default:
		break;
	}

	/* move to current arg */
	argc -= 2;
	argv += 2;
	BU_ASSERT(argc > 0);

	/* convert next element to an arg */
	if (edit_strs_to_arg(gedp, &argc, &argv, cur_arg, conv_flags) ==
	    GED_ERROR) {
	    if (conv_flags & GED_ERROR) {
		edit_cmd_free(&subcmd);
		return GED_ERROR;
	    }
	} else {
	    /* init for next arg */
	    if (argc == 0)
		break; /* no more args */
	    cur_arg = edit_arg_postfix_new(subcmd.cmd_line.args);
	}

	/* FIXME: It might take some time to make this work. */
	if ((cur_arg->type & EDIT_TARGET_OBJ) &&
	    ((cur_arg->coords_used & EDIT_COORDS_ALL) != EDIT_COORDS_ALL)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "using the batch operator to specify individual"
			  " coordinates is not yet supported");
	    edit_cmd_free(&subcmd);
	    return GED_ERROR;
	}

	/* conversion moves argc/argv, so re-init bu_getopt() */
	bu_optind = 1;

	/* move to before next supposed option */
	++argc;
	--argv;
    }

    if (keypoint) {
	bu_vls_printf(gedp->ged_result_str,
		      "a keypoint is missing its matching 'TO' argument");
	edit_cmd_free(&subcmd);
	return GED_ERROR;
    }

    /* get final/trailing args */
    ++argv;
    --argc;
    while (argc > 0) {
	if (edit_strs_to_arg(gedp, &argc, &argv, cur_arg, GED_ERROR) ==
	    GED_ERROR) {
	    edit_cmd_free(&subcmd);
	    return GED_ERROR;
	}
	cur_arg->type |= EDIT_TARGET_OBJ;
	cur_arg = edit_arg_postfix_new(subcmd.cmd_line.args);
    }

    /* free unused arg block */
    edit_arg_free_last(subcmd.cmd_line.args);

    if (edit_cmd_consolidate(gedp, &subcmd, 0) == GED_ERROR)
	return GED_ERROR;
    if (subcmd.cmd->add_cl_args(gedp, &subcmd, GED_ERROR) == GED_ERROR)
	return GED_ERROR;
    ret = edit(gedp, &subcmd);
    edit_cmd_free(&subcmd);
    return ret;

err_missing_arg:
    bu_vls_printf(gedp->ged_result_str, "Missing argument for option -%c",
	bu_optopt);
    edit_cmd_free(&subcmd);
    return GED_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
