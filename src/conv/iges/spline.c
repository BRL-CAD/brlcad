/*                        S P L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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

#include "./iges_struct.h"
#include "./iges_extern.h"

#define PATCH_COUNT 1

int
spline(int entityno, struct face_g_snurb **b_patch)
{
    int k1;	/* upper index of first sum */
    int k2;	/* upper index of second sum */
    int m1;	/* degree of 1st set of basis functions */
    int m2;	/* degree of 2nd set of basis functions */
    int prop1;	/* !0 if closed in first direction */
    int prop2;	/* !0 if closed in second direction */
    int prop3;	/* !0 if polynomial (else rational) */
    int prop4;	/* !0 if periodic in first direction */
    int prop5;	/* !0 if periodic in second direction */
    int sol_num; /* IGES solid type number */
    int n1, n2;
    int i, j, k;
    int count = 0;
    int point_size;
    fastf_t min_knot;
    double max_wt;
    double scan;

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&k1, "");
    Readint(&k2, "");
    Readint(&m1, "");
    Readint(&m2, "");
    Readint(&prop1, "");
    Readint(&prop2, "");
    Readint(&prop3, "");
    Readint(&prop4, "");
    Readint(&prop5, "");

    n1 = k1 - m1 + 1;
    n2 = k2 - m2 + 1;

    /* spl_new: Creates a spline surface data structure
     * u_order (e.g. cubic = order 4)
     * v_order
     * num_u (e.g. num control points + order)
     * num_v
     * num_rows num control points in V direction
     * num_cols num control points in U direction
     * point_size number of values in a point (e.g. 3 or 4)
     */

    if (prop3 == 0) {
	point_size = 4;
    } else {
	point_size = 3;
    }

    (*b_patch) = rt_nurb_new_snurb(
	m1+1, m2+1,
	n1+2*m1+1, n2+2*m2+1,
	k2+1, k1+1,
	RT_NURB_MAKE_PT_TYPE(point_size, 2,
			     (prop3 == 0 ? RT_NURB_PT_RATIONAL : RT_NURB_PT_NONRAT)),
	(struct resource *)NULL);

    /* U knot vector */
    min_knot = 0.0;
    for (i = 0; i <= n1+2*m1; i++) {
	Readdbl(&scan, "");
	(*b_patch)->u.knots[i] = scan; /* double to fastf_t */
	V_MIN(min_knot, (*b_patch)->u.knots[i]);
    }

    if (min_knot < 0.0) {
	for (i = 0; i <= n1+2*m1; i++) {
	    (*b_patch)->u.knots[i] -= min_knot;
	}
    }

    min_knot = 0.0;
    /* V knot vector */
    for (i = 0; i <= n2+2*m2; i++) {
	Readdbl(&scan, "");
	(*b_patch)->v.knots[i] = scan; /* double to fastf_t */
	V_MIN(min_knot, (*b_patch)->v.knots[i]);
    }
    if (min_knot < 0.0) {
	for (i = 0; i <= n2+2*m2; i++) {
	    (*b_patch)->v.knots[i] -= min_knot;
	}
    }


    /* weights */
    max_wt = 0.0;
    count = 0;
    for (i = 0; i <= k2; i++) {
	for (j = 0; j <= k1; j++) {
	    if (point_size == 4) {
		Readdbl(&scan, "");
		(*b_patch)->ctl_points[count*4 + 3] = scan; /* double to fastf_t */
		V_MAX(max_wt, (*b_patch)->ctl_points[count*4 + 3]);
	    } else {
		Readdbl(&max_wt, "");
	    }
	    count++;
	}
    }

    /* control points */
    count = 0;
    for (i = 0; i <= k2; i++) {
	for (j = 0; j <= k1; j++) {
	    Readcnv(&(*b_patch)->ctl_points[count*point_size], "");
	    Readcnv(&(*b_patch)->ctl_points[count*point_size + 1], "");
	    Readcnv(&(*b_patch)->ctl_points[count*point_size + 2], "");
	    count++;
	}
    }

    if (point_size == 4) {
	/* apply weights */
	count = 0;
	for (i = 0; i <= k2; i++) {
	    for (j = 0; j <= k1; j++) {
		for (k = 0; k < 3; k++)
		    (*b_patch)->ctl_points[count*4 + k] *= (*b_patch)->ctl_points[count*4 + 3];
		count++;
	    }
	}
    }

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
