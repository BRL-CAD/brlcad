/*                    C O N V E R S I O N . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file conversion.c
 *
 */

/*      CONVERSION.C    */
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "common.h"



#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "./nirt.h"
#include "./usrfmt.h"

extern outval	ValTab[];

void dir2ae(void)
{
	azimuth() = ((direct(Y) == 0) && (direct(X) == 0)) ? 0.0 :
			atan2 ( -(direct(Y)), -(direct(X)) ) / deg2rad;
	elevation() = atan2 ( -(direct(Z)), 
		sqrt(direct(X) * direct(X) + direct(Y) * direct(Y))) / deg2rad;
}

void grid2targ(void)
{
    double	ar = azimuth() * deg2rad;
    double	er = elevation() * deg2rad;

    target(X) = - grid(HORZ) * sin(ar)
		      - grid(VERT) * cos(ar) * sin(er)
		      + grid(DIST) * cos(ar) * cos(er);
    target(Y) =   grid(HORZ) * cos(ar)
		      - grid(VERT) * sin(ar) * sin(er)
		      + grid(DIST) * sin(ar) * cos(er);
    target(Z) =   grid(VERT) * cos(er)
		      + grid(DIST) * sin(er);
}

void targ2grid(void)
{
    double	ar = azimuth() * deg2rad;
    double	er = elevation() * deg2rad;

    grid(HORZ) = - target(X) * sin(ar)
		       + target(Y) * cos(ar);
    grid(VERT) = - target(X) * cos(ar) * sin(er)
		       - target(Y) * sin(er) * sin(ar)
		       + target(Z) * cos(er);
    grid(DIST) =   target(X) * cos(er) * cos(ar)
		       + target(Y) * cos(er) * sin(ar)
		       + target(Z) * sin(er);
}

void ae2dir(void)
{
    double	ar = azimuth() * deg2rad;
    double	er = elevation() * deg2rad;

    int		i;
    vect_t	dir;

    dir[X] = -cos(ar) * cos(er);
    dir[Y] = -sin(ar) * cos(er);
    dir[Z] = -sin(er);
    VUNITIZE( dir );
    for (i = 0; i < 3; ++i)
	direct(i) = dir[i];
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
