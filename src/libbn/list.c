/*                          L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include "vmath.h"
#include "plot3.h"

/* Modes for internal flag */
#define TP_MARK 1 /* Draw marks */
#define TP_LINE 2 /* Draw lines */


void
tp_i2list(register FILE *fp, register int *x, register int *y, register int npoints)

/* array of points */
/* array of points */

{
    if (npoints <= 0)
	return;

    pl_move(fp, *x++, *y++);
    while (--npoints > 0)
	pl_cont(fp, *x++, *y++);
}


void
tp_2list(register FILE *fp, register double *x, register double *y, register int npoints)

/* array of points */
/* array of points */

{
    if (npoints <= 0)
	return;

    pd_move(fp, *x++, *y++);
    while (--npoints > 0)
	pd_cont(fp, *x++, *y++);
}


void
PL_FORTRAN(f2list, F2LIST)(FILE **fpp, float *x, float *y, int *n)
{
    register int npoints = *n-1;	/* FORTRAN uses 1-based subscripts */
    register FILE *fp = *fpp;

    if (npoints <= 0)
	return;

    pd_move(fp, *x++, *y++);
    while (--npoints > 0)
	pd_cont(fp, *x++, *y++);
}


void
tp_3list(FILE *fp, register double *x, register double *y, register double *z, register int npoints)
{
    if (npoints <= 0)
	return;

    pd_3move(fp, *x++, *y++, *z++);
    while (--npoints > 0)
	pd_3cont(fp, *x++, *y++, *z++);
}


void
PL_FORTRAN(f3list, F3LIST)(FILE **fpp, float *x, float *y, float *z, int *n)
{
    register int npoints = *n-1;	/* FORTRAN uses 1-based subscripts */
    register FILE *fp = *fpp;

    if (npoints <= 0)
	return;

    pd_3move(fp, *x++, *y++, *z++);
    while (--npoints > 0)
	pd_3cont(fp, *x++, *y++, *z++);
}


void
tp_2mlist(FILE *fp, register double *x, register double *y, int npoints, int flag, int mark, int interval, double size)


/* arrays of points */

/* TP_MARK|TP_LINE */
/* marker character to use */
/* marker drawn every N points */
/* marker size */
{
    register int i;			/* index variable */
    register int counter;		/* interval counter */

    if (npoints <= 0)
	return;

    if (flag & TP_LINE)
	tp_2list(fp, x, y, npoints);
    if (flag & TP_MARK) {
	tp_2marker(fp, mark, *x++, *y++, size);
	counter = 1;		/* Already plotted one */
	for (i=1; i<npoints; i++) {
	    if (counter >= interval) {
		tp_2marker(fp, mark, *x, *y, size);
		counter = 0;	/* We made a mark */
	    }
	    x++; y++;
	    counter++;		/* One more point done */
	}
    }
}


void
PL_FORTRAN(f2mlst, F2MLST)(FILE **fp, float *x, float *y, int *np, int *flag /* indicates user's mode request */, int *mark, int *interval, float *size)
{
    register int i;			/* index variable */
    register int counter;		/* interval counter */
    register int npoints = *np-1;

    if (npoints <= 0)
	return;

    if (*flag & TP_LINE)
	PL_FORTRAN(f2list, F2LIST)(fp, x, y, np);
    if (*flag & TP_MARK) {
	tp_2marker(*fp, *mark, *x++, *y++, *size);
	counter = 1;			/* We already plotted one */
	for (i=1; i<npoints; i++) {
	    if (counter >= *interval) {
		tp_2marker(*fp, *mark, *x, *y, *size);
		counter = 0;	/* Made a mark */
	    }
	    x++; y++;
	    counter++;		/* One more point done */
	}
    }
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
