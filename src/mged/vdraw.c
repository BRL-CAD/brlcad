/*                         V D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file vdraw.c
 *
 */

/*******************************************************************

CMD_VDRAW - edit vector lists and display them as pseudosolids

OPEN COMMAND
vdraw	open			- with no argument, asks if there is
				  an open vlist (1 yes, 0 no)

		name		- opens the specified vlist
				  returns 1 if creating new vlist
					  0 if opening an existing vlist

EDITING COMMANDS - no return value

vdraw	write	i	c x y z	- write params into i-th vector
		next	c x y z	- write params to end of vector list
		rpp	x y z x y z - write RPP outline at end of vector list

vdraw	insert	i	c x y z	- insert params in front of i-th vector

vdraw	delete 	i		- delete i-th vector
		last		- delete last vector on list
		all		- delete all vectors on list

PARAMETER SETTING COMMAND - no return value
vdraw	params	color		- set the current color with 6 hex digits
				  representing rrggbb
		name		- change the name of the current vlist

QUERY COMMAND
vdraw	read	i		- returns contents of i-th vector "c x y z"
		color		- return the current color in hex
		length		- return number of vectors in list
		name		- return name of current vlist

DISPLAY COMMAND -
vdraw	send			- send the current vlist to the display
				  returns 0 on success, -1 if the name
				  conflicts with an existing true solid

CURVE COMMANDS
vdraw	vlist	list		- return list of all existing vlists
vdraw	vlist	delete	name	- delete the named vlist

All textual arguments can be replaced by their first letter.
(e.g. "vdraw d a" instead of "vdraw delete all"

In the above listing:

"i" refers to an integer
"c" is an integer representing one of the following bn_vlist commands:
	 BN_VLIST_LINE_MOVE	0	/ begin new line /
	 BN_VLIST_LINE_DRAW	1	/ draw line /
	 BN_VLIST_POLY_START	2	/ pt[] has surface normal /
	 BN_VLIST_POLY_MOVE	3	/ move to first poly vertex /
	 BN_VLIST_POLY_DRAW	4	/ subsequent poly vertex /
	 BN_VLIST_POLY_END	5	/ last vert (repeats 1st), draw poly /
	 BN_VLIST_POLY_VERTNORM	6	/ per-vertex normal, for interpoloation /

"x y z" refer to floating point values which represent a point or normal
	vector. For commands 0, 1, 3, 4, and 5, they represent a point, while
	for commands 2 and 6 they represent normal vectors

Example Use -
	vdraw open rays
	vdraw delete all
	foreach partition $ray {
		...stuff...
		vdraw write next 0 $inpt
		vdraw write next 1 $outpt
	}
	vdraw send


********************************************************************/

#include "common.h"

#include <string.h>
#include <math.h>
#include <signal.h>
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "nmg.h"
#include "raytrace.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
