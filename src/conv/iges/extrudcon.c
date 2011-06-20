/*                     E X T R U D C O N . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file iges/extrudcon.c
 *
 * Create a TGC from a ellipse extrusion.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
Extrudcon(int entityno, int curve, vect_t evect)
    /* extrusion entity number */
    /* elliptical arc entity number */
    /* extrusion vector */
{
    fastf_t a, b, c, d, e, f;	/* Coefficients of conic equation
				   a*X*X + b*X*Y + c*Y*Y + d*X + e*Y + f = 0 */
    fastf_t conv_sq;	/* conv-factor squared */
    point_t start, stop;	/* starting and stopping points on arc */
    int sol_num;	/* Solid number */
    fastf_t q1, q2, q3;	/* terms for determining type of conic */
    int ellipse;	/* flag to indicate eillipse */
    fastf_t tmp;		/* scratch */
    point_t center;		/* center of ellipse */
    fastf_t theta;		/* angle that elipse is rotated */
    fastf_t a1, c1, f1;	/* coefficients of translated and rotated ellipse */
    vect_t r1, r2;		/* radii vectors for ellipse and TGC */

    /* Acquiring Data */

    if (dir[curve]->form > 1) {
	bu_log("Conic arc for extrusion is not closed:\n");
	bu_log("\textrusion entity D%07d (%s)\n", dir[entityno]->direct ,
	       dir[entityno]->name);
	bu_log("\tarc entity D%07d (%s)\n", dir[curve]->direct, dir[curve]->name);
	return 0;
    }

    if (dir[curve]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[curve]->direct, dir[curve]->name);
	return 0;
    }
    Readrec(dir[curve]->param);
    Readint(&sol_num, "");
    Readflt(&a, "");
    Readflt(&b, "");
    Readflt(&c, "");
    Readflt(&d, "");
    Readflt(&e, "");
    Readflt(&f, "");
    Readcnv(&start[Z], "");
    Readcnv(&start[X], "");
    Readcnv(&start[Y], "");
    Readcnv(&stop[X], "");
    Readcnv(&stop[Y], "");
    stop[Z] = start[Z];

    /* Convert coefficients to "mm" units */

    conv_sq = conv_factor*conv_factor;
    a = a/conv_sq;
    b = b/conv_sq;
    c = c/conv_sq;
    d = d/conv_factor;
    e = e/conv_factor;

    /* set "a" to 1.0 */

    tmp = fabs(a);
    if (fabs(b) < tmp && !ZERO(b))
	tmp = fabs(b);
    if (fabs(c) < tmp)
	tmp = fabs(c);
    a = a/tmp;
    b = b/tmp;
    c = c/tmp;
    d = d/tmp;
    e = e/tmp;
    f = f/tmp;

    /* Check for closure */

    if (!ZERO(start[X] - stop[X]) || !ZERO(start[Y] - stop[Y])) {
	bu_log("Conic arc for extrusion is not closed:\n");
	bu_log("\textrusion entity D%07d (%s)\n", dir[entityno]->direct ,
	       dir[entityno]->name);
	bu_log("\tarc entity D%07d (%s)\n", dir[curve]->direct, dir[curve]->name);
	return 0;
    }

    /* Check type of conic */

    q2 = a*c - b*b/4.0;
    ellipse = 1;
    if (q2 <= 0.0)
	ellipse = 0;
    else {
	q3 = a + c;
	q1 = a*(c*f - e*e/4.0) - 0.5*b*(b*f/2.0 + e*d/4.0) + 0.5*d*(b*e/4.0 - d*c/2.0);
	if (q1*q3 >= 0.0)
	    ellipse = 0;
    }

    if (!ellipse) {
	bu_log("Conic arc for extrusion is not an elipse:\n");
	bu_log("\textrusion entity D%07d (%s)\n", dir[entityno]->direct ,
	       dir[entityno]->name);
	bu_log("\tarc entity D%07d (%s)\n", dir[curve]->direct, dir[curve]->name);
	return 0;
    }

    /* Calculate center of ellipse */

    center[X] = (b*e/4.0 - d*c/2.0)/q2;
    center[Y] = (b*d/4.0 - a*e/2.0)/q2;
    center[Z] = start[Z];

    /* calculate rotation about z-axis */
    if (ZERO(b))
	theta = 0.0;
    else
	theta = 0.5*atan2(b, a-c);

    /* calculate coefficients for same ellipse, but translated to
       origin, and rotated to align with axes */

    a1 = a + 0.5*b*tan(theta);
    c1 = c - 0.5*b*tan(theta);
    f1 = f - a*center[X]*center[X] - b*center[X]*center[Y] - c*center[Y]*center[Y];

    /* Calculate radii vectors */

    tmp = sqrt(-f1/a1);
    r1[X] = tmp*cos(theta);
    r1[Y] = tmp*sin(theta);
    r1[Z] = 0.0;

    tmp = sqrt(-f1/c1);
    r2[X] = tmp*(-sin(theta));
    r2[Y] = tmp*cos(theta);
    r2[Z] = 0.0;

    /* Construct solid */
    mk_tgc(fdout, dir[entityno]->name, center, evect, r1, r2, r1, r2);

    return 1;
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
