/*                   N U R B _ I N T E R P . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file primitives/bspline/nurb_interp.c
 *
 * Interpolation routines for fitting NURB curves and and surfaces to
 * existing data.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nurb.h"


void
nurb_nodes(fastf_t *nodes, const struct knot_vector *knots, int order)
{
    int i, j;
    fastf_t sum;

    for (i = 0; i < knots->k_size -order; i++) {

	sum = 0.0;

	for (j = 1; j <= order -1; j++) {
	    sum += knots->knots[i+j];
	}
	nodes[i] = sum/(order -1);
    }
}


void
nurb_interp_mat(fastf_t *imat, struct knot_vector *knots, fastf_t *nodes, int order, int dim)
{
    int i, j;
    int ptr;

    ptr = 0;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++) {
	    imat[ptr] = nurb_basis_eval(knots, j, order, nodes[i]);
	    ptr++;
	}

    imat[ptr-1] = 1.0;
}


/**
 * main routine for interpolation of curves
 */
void
nurb_cinterp(struct edge_g_cnurb *crv, int order, const fastf_t *data, int n)
{
    fastf_t * interp_mat;
    fastf_t * nodes;
    fastf_t *local_data;

    /* Create Data memory and fill in curve structs */

    interp_mat = (fastf_t *) bu_malloc(n * n * sizeof(fastf_t),
				       "nurb_interp: interp_mat");

    nodes = (fastf_t *) bu_malloc(n * sizeof(fastf_t), "nurb_interp:nodes");
    local_data = (fastf_t *)bu_malloc(n * 3 * sizeof(fastf_t), "nurb_interp() local_data[]");

    crv->ctl_points = (fastf_t *) bu_malloc(n * 3 * sizeof(fastf_t), "solution");

    crv->order = order;
    crv->c_size = n;
    crv->pt_type = NURB_MAKE_PT_TYPE(3, NURB_PT_XYZ, 0);

    /* First set up Curve data structs */
    /* For now we will assume that all parameterizations are uniform */

    nurb_kvknot(&crv->k, order, 0.0, 1.0, (n - order), (struct resource *)NULL);

    /* Calculate Nodes at which the data points will be evaluated in
     * the curve
     */

    nurb_nodes(nodes, &crv->k, order);

    /* use the node values to create the interpolation matrix which is
     * a diagonal matrix
     */

    nurb_interp_mat(interp_mat, &crv->k, nodes, order, n);

    /* Solve the system of equations to get the control points Because
     * rt_nurb_solve needs to modify the data as it works, and it
     * wouldn't be polite to trash our caller's data, make a local
     * copy.  This creates the final ctl_points[] array.
     */
    memcpy((char *)local_data, (char *)data, n * 3 * sizeof(fastf_t));
    nurb_solve(interp_mat, local_data, crv->ctl_points, n, 3);

    /* Free up node and interp_mat storage */

    bu_free((char *) interp_mat, "nurb_cinterp: interp_mat");
    bu_free((char *) nodes, "nurb_cinterp: nodes");
    bu_free((char *) local_data, "nurb_cinterp() local_data[]");

    /* All done, The resulting crv now interpolates the data */
}


/**
 * Interpolate the 2-D grid of data values and fit a B-spline surface
 * to it.
 *
 * This is done in two steps:
 *
 * 1) Fit a curve to the data in each row.
 *
 * 2) Fit a curve to the control points from step 1 in each column.
 * The result is a mesh of control points which defines the surface.
 *
 * Input data is assumed to be a 3-tuple of (X, Y, Z) where Z is the
 * independent variable being interpolated to make the surface.
 */
void
nurb_sinterp(struct face_g_snurb *srf, int order, const fastf_t *data, int ymax, int xmax)


    /* data[x, y] */
    /* nrow = max Y */
    /* ncol = max X */
{
    int x;
    int y;
    struct edge_g_cnurb *crv;	/* array of cnurbs */
    fastf_t *tmp;
    fastf_t *cpt;	/* surface control point pointer */

    /* Build the resultant surface structure */
    srf->order[0] = srf->order[1] = order;
    srf->dir = 0;
    srf->s_size[0] = xmax;
    srf->s_size[1] = ymax;
    srf->l.magic = NMG_FACE_G_SNURB_MAGIC;
    srf->pt_type = NURB_MAKE_PT_TYPE(3, NURB_PT_XYZ, NURB_PT_NONRAT);

    /* the U knot vector replates to the points in a row therefore you
     * want to determine how many cols there are similar for the V knot
     * vector
     */

    nurb_kvknot(&srf->u, order, 0.0, 1.0, ymax - order, (struct resource *)NULL);
    nurb_kvknot(&srf->v, order, 0.0, 1.0, xmax - order, (struct resource *)NULL);

    srf->ctl_points = (fastf_t *) bu_malloc(
	sizeof(fastf_t) * xmax * ymax * 3,
	"nurb_sinterp() surface ctl_points[]");
    cpt = &srf->ctl_points[0];

/* _col is X, _row is Y */
#define NVAL(_col, _row) data[((_row)*xmax+(_col))*3]

    crv = (struct edge_g_cnurb *)bu_calloc(sizeof(struct edge_g_cnurb), ymax,
					   "nurb_sinterp() crv[]");

    /* Interpolate the data across the rows, fitting a curve to each. */
    for (y = 0; y < ymax; y++) {
	crv[y].l.magic = NMG_EDGE_G_CNURB_MAGIC;
	/* Build curve from from (0, y) to (xmax-1, y) */
	nurb_cinterp(&crv[y], order, &NVAL(0, y), xmax);
    }
#undef NVAL

    tmp = (fastf_t *)bu_malloc(sizeof(fastf_t)*3 * ymax,
			       "nurb_sinterp() tmp[]");
    for (x = 0; x < xmax; x++) {
	struct edge_g_cnurb ncrv;

	/* copy the curve ctl points into col major format */
	for (y = 0; y < ymax; y++) {
	    VMOVE(&tmp[y*3], &crv[y].ctl_points[x*3]);
	}

	/* Interpolate the curve interpolates, giving rows of a surface */
	ncrv.l.magic = NMG_EDGE_G_CNURB_MAGIC;
	nurb_cinterp(&ncrv, order, tmp, ymax);

	/* Move new curve interpolations into snurb ctl_points[] */
	for (y = 0; y < ymax*3; y++) {
	    *cpt++ = ncrv.ctl_points[y];
	}
	nurb_clean_cnurb(&ncrv);
    }
    for (y = 0; y < ymax; y++) {
	nurb_clean_cnurb(&crv[y]);
    }
    bu_free((char *)crv, "crv[]");
    bu_free((char *)tmp, "tmp[]");
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
