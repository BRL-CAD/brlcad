/*                          P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
 *
 */
/** @file burst/plot.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "bn/plot3.h"

#include "./burst.h"
#include "./extern.h"

void
plotInit()
{
    int x_1, y_1, z_1, x_2, y_2, z_2;
    if (plotfp == NULL)
	return;
    x_1 = (int) rtip->mdl_min[X] - 1;
    y_1 = (int) rtip->mdl_min[Y] - 1;
    z_1 = (int) rtip->mdl_min[Z] - 1;
    x_2 = (int) rtip->mdl_max[X] + 1;
    y_2 = (int) rtip->mdl_max[Y] + 1;
    z_2 = (int) rtip->mdl_max[Z] + 1;
    pl_3space(plotfp, x_1, y_1, z_1, x_2, y_2, z_2);
    return;
}


void
plotGrid(fastf_t *r_pt)
{
    if (plotfp == NULL)
	return;
    pl_color(plotfp, R_GRID, G_GRID, B_GRID);
    pl_3point(plotfp, (int) r_pt[X], (int) r_pt[Y], (int) r_pt[Z]);
    return;
}


void
plotRayLine(struct xray *rayp)
{
    int endpoint[3];
    if (plotfp == NULL)
	return;
    VJOIN1(endpoint, rayp->r_pt, cellsz, rayp->r_dir);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_color(plotfp, R_BURST, G_BURST, B_BURST);

    /* draw line */
    pl_3line(plotfp,
	     (int) rayp->r_pt[X],
	     (int) rayp->r_pt[Y],
	     (int) rayp->r_pt[Z],
	     endpoint[X],
	     endpoint[Y],
	     endpoint[Z]
	);

    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}


void
plotRayPoint(struct xray *rayp)
{
    int endpoint[3];
    if (plotfp == NULL)
	return;
    VJOIN1(endpoint, rayp->r_pt, cellsz, rayp->r_dir);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_color(plotfp, R_BURST, G_BURST, B_BURST);

    /* draw point */
    pl_3point(plotfp, (int) endpoint[X], (int) endpoint[Y], (int) endpoint[Z]);

    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}


void
plotPartition(struct hit *ihitp, struct hit *ohitp)
{
    if (plotfp == NULL)
	return;
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_3line(plotfp,
	     (int) ihitp->hit_point[X],
	     (int) ihitp->hit_point[Y],
	     (int) ihitp->hit_point[Z],
	     (int) ohitp->hit_point[X],
	     (int) ohitp->hit_point[Y],
	     (int) ohitp->hit_point[Z]
	);
    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
