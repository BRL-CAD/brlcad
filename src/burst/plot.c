/*                          P L O T . C
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
 *
 */
/** @file plot.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "plot3.h"
#include "./vecmath.h"
#include "./burst.h"
#include "./extern.h"

void
plotInit()
	{	int	x1, y1, z1, x2, y2, z2;
	if( plotfp == NULL )
		return;
	x1 = (int) rtip->mdl_min[X] - 1;
	y1 = (int) rtip->mdl_min[Y] - 1;
	z1 = (int) rtip->mdl_min[Z] - 1;
	x2 = (int) rtip->mdl_max[X] + 1;
	y2 = (int) rtip->mdl_max[Y] + 1;
	z2 = (int) rtip->mdl_max[Z] + 1;
	pl_3space( plotfp, x1, y1, z1, x2, y2, z2 );
	return;
	}

void
plotGrid( r_pt )
register fastf_t	*r_pt;
	{
	if( plotfp == NULL )
		return;
	pl_color( plotfp, R_GRID, G_GRID, B_GRID );
	pl_3point( plotfp, (int) r_pt[X], (int) r_pt[Y], (int) r_pt[Z] );
	return;
	}

void
plotRay( rayp )
register struct xray	*rayp;
	{	int	endpoint[3];
	if( plotfp == NULL )
		return;
	VJOIN1( endpoint, rayp->r_pt, cellsz, rayp->r_dir );
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	pl_color( plotfp, R_BURST, G_BURST, B_BURST );
#if 0
	pl_3line(	plotfp,
			(int) rayp->r_pt[X],
			(int) rayp->r_pt[Y],
			(int) rayp->r_pt[Z],
			endpoint[X],
			endpoint[Y],
			endpoint[Z]
			);
#else
	pl_3point( plotfp, (int) endpoint[X], (int) endpoint[Y], (int) endpoint[Z] );
#endif
	bu_semaphore_release( BU_SEM_SYSCALL );
	return;
	}

void
plotPartition( ihitp, ohitp, rayp, regp )
struct hit		*ihitp;
register struct hit	*ohitp ;
register struct xray	*rayp;
struct region		*regp;
	{
	if( plotfp == NULL )
		return;
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	pl_3line(	plotfp,
			(int) ihitp->hit_point[X],
			(int) ihitp->hit_point[Y],
			(int) ihitp->hit_point[Z],
			(int) ohitp->hit_point[X],
			(int) ohitp->hit_point[Y],
			(int) ohitp->hit_point[Z]
			);
	bu_semaphore_release( BU_SEM_SYSCALL );
	return;
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
