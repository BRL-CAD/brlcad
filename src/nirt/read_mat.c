/*                      R E A D _ M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file read_mat.c
 *
 */

/*      READ_MAT.C      */
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#if HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./nirt.h"
#include "./usrfmt.h"


#define			RMAT_SAW_EYE	0x01
#define			RMAT_SAW_ORI	0x02
#define			RMAT_SAW_VR	0x02

extern outval			ValTab[];
extern int			nirt_debug;

/*	               R E A D _ M A T ( )
 *
 */

void read_mat (void)
{
    char	*buf;
    int		status = 0x0;
    mat_t	m;
    point_t	p;
    quat_t	q;

    while ((buf = rt_read_cmd(stdin)) != (char *) 0)
	if (strncmp(buf, "eye_pt", 6) == 0)
	{
	    if (sscanf(buf + 6, "%lf%lf%lf", &p[X], &p[Y], &p[Z]) != 3)
	    {
		(void) fputs("nirt: read_mat(): Failed to read eye_pt\n",
		    stderr);
		exit (1);
	    }
	    target(X) = p[X];
	    target(Y) = p[Y];
	    target(Z) = p[Z];
	    status |= RMAT_SAW_EYE;
	}
	else if (strncmp(buf, "orientation", 11) == 0)
	{
	    if (sscanf(buf + 11,
		"%lf%lf%lf%lf",
		&q[0], &q[1], &q[2], &q[3]) != 4)
	    {
		(void) fputs("nirt: read_mat(): Failed to read orientation\n",
		    stderr);
		exit (1);
	    }
	    quat_quat2mat(m,q);
	    if (nirt_debug & DEBUG_MAT)
		bn_mat_print("view matrix", m);
	    azimuth() = atan2(-m[0],m[1]) / deg2rad;
	    elevation() = atan2(m[10],m[6]) / deg2rad;
	    status |= RMAT_SAW_ORI;
	}
	else if (strncmp(buf, "viewrot", 7) == 0)
	{
	    if (sscanf(buf + 7,
		"%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
		&m[0], &m[1], &m[2], &m[3],
		&m[4], &m[5], &m[6], &m[7],
		&m[8], &m[9], &m[10], &m[11],
		&m[12], &m[13], &m[14], &m[15]) != 16)
	    {
		(void) fputs("nirt: read_mat(): Failed to read viewrot\n",
		    stderr);
		exit (1);
	    }
	    if (nirt_debug & DEBUG_MAT)
		bn_mat_print("view matrix", m);
	    azimuth() = atan2(-m[0],m[1]) / deg2rad;
	    elevation() = atan2(m[10],m[6]) / deg2rad;
	    status |= RMAT_SAW_VR;
	}

    if ((status & RMAT_SAW_EYE) == 0)
    {
	(void) fputs("nirt: read_mat(): Was given no eye_pt\n", stderr);
	exit (1);
    }
    if ((status & (RMAT_SAW_ORI | RMAT_SAW_VR)) == 0)
    {
	(void) fputs("nirt: read_mat(): Was given no orientation or viewrot\n",
		stderr);
	exit (1);
    }
    direct(X) = -m[8];
    direct(Y) = -m[9];
    direct(Z) = -m[10];
    dir2ae();
#if 0
    ae2dir();
#endif
    targ2grid();
    shoot("", 0);
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
