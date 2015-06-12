/*                     N U R B _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file primitives/bspline/nurb_eval.c
 *
 * Evaluate a Non Uniform Rational B-spline curve or at the given (u,
 * v) values.
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/nurb.h"


/**
 * Algorithm -
 *
 * The algorithm uses the traditional COX-deBoor approach found in the
 * book "Practical Guide to Splines" Carl de Boor, pg 147 to evaluate a
 * parametric value on a curve. This is expanded to the surface.
 */
void
rt_nurb_s_eval(const struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t *final_value)
{
    fastf_t * mesh_ptr = srf->ctl_points;
    fastf_t * curves;
    int i, j, k;
    int row_size = srf->s_size[RT_NURB_SPLIT_ROW];
    int col_size = srf->s_size[RT_NURB_SPLIT_COL];
    fastf_t * c_ptr;
    fastf_t * diff_curve, *ev_pt;
    int k_index;
    int coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

    NMG_CK_SNURB(srf);

    /* Because the algorithm is destructive in nature, the rows of the
     * control mesh are copied. The evaluation is then done on each
     * row curve and then evaluation is then performed on the
     * resulting curve.
     */

    diff_curve = (fastf_t *)
	bu_malloc(row_size * sizeof(fastf_t) * coords,
		  "rt_nurb_s__eval: diff_curve");

    c_ptr = diff_curve;

    k_index = rt_nurb_knot_index(&srf->u, u, srf->order[RT_NURB_SPLIT_ROW]);
    if (k_index < 0) {
	bu_log("rt_nurb_s_eval: u value outside parameter range\n");
	bu_log("\tUV = (%g %g)\n", u, v);
	rt_nurb_s_print("", srf);
	bu_bomb("rt_nurb_s_eval: u value outside parameter range\n");
    }

    curves = (fastf_t *) bu_malloc(col_size * sizeof(fastf_t) * coords,
				   "rt_nurb_s_eval:crv_ptr");

    for (i = 0; i < row_size; i++) {
	fastf_t * rtr_pt;
	fastf_t * crv_ptr;

	crv_ptr = curves;

	for (j = 0; j < (col_size * coords); j++) {
	    *crv_ptr++ = *mesh_ptr++;
	}

	rtr_pt =  (fastf_t *) rt_nurb_eval_crv(curves, srf->order[RT_NURB_SPLIT_ROW], u,
					       &srf->u, k_index, coords);

	for (k = 0; k < coords; k++)
	    c_ptr[k] = rtr_pt[k];
	c_ptr += coords;
    }

    bu_free((char *)curves, "rt_nurb_s_eval: curves");

    k_index = rt_nurb_knot_index(&srf->v, v, srf->order[RT_NURB_SPLIT_COL]);
    if (k_index < 0) {
	bu_log("rt_nurb_s_eval: v value outside parameter range\n");
	bu_log("\tUV = (%g %g)\n", u, v);
	rt_nurb_s_print("", srf);
	bu_bomb("rt_nurb_s_eval: v value outside parameter range\n");
    }

    ev_pt = (fastf_t *) rt_nurb_eval_crv(diff_curve, srf->order[RT_NURB_SPLIT_COL],
					 v, &srf->v, k_index, coords);

    for (k = 0; k < coords; k++)
	final_value[k] = ev_pt[k];

    bu_free ((char *)diff_curve, "rt_nurb_s_eval: diff curve");
}


void
rt_nurb_c_eval(const struct edge_g_cnurb *crv, fastf_t param, fastf_t *final_value)
{
    fastf_t * pnts;
    fastf_t * ev_pt;
    int coords;
    int i, k_index;

    NMG_CK_CNURB(crv);

    coords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

    k_index = rt_nurb_knot_index(&crv->k, param, crv->order);
    if (k_index < 0) {
	bu_log("rt_nurb_c_eval: param value outside parameter range\n");
	bu_log("\tparam = (%g)\n", param);
	rt_nurb_c_print(crv);
	bu_bomb("rt_nurb_c_eval: param value outside parameter range\n");
    }

    pnts = (fastf_t *) bu_malloc(coords * sizeof(fastf_t) *
				 crv->c_size, "diff: rt_nurb_c_eval");

    for (i = 0; i < coords * crv->c_size; i++)
	pnts[i] = crv->ctl_points[i];

    ev_pt = (fastf_t *) rt_nurb_eval_crv(
	pnts, crv->order, param, &crv->k, k_index, coords);

    for (i = 0; i < coords; i++)
	final_value[i] = ev_pt[i];

    bu_free((char *) pnts, "rt_nurb_c_eval");
}


fastf_t *
rt_nurb_eval_crv(register fastf_t *crv, int order, fastf_t param, const struct knot_vector *k_vec, int k_index, int coords)
{
    int i, j;

    if (order <= 1)
	return
	    (crv + ((k_index) * coords));

    j = k_index;

    while (j > (k_index - order + 1)) {
	register fastf_t k1, k2;

	k1 =  k_vec->knots[ (j + order - 1)];

	k2 =  k_vec->knots[ (j) ];

	if (!ZERO(k1 - k2)) {
	    for (i= 0; i < coords; i++) {
		*((crv + ((j) * coords)) + i) =
		    ((k1 - param) *
		     *((crv + ((j - 1) * coords)) + i)
		     + (param - k2) * *((crv + ((j) *
						coords)) + i)) / (k1 - k2);
	    }
	}
	j--;
    }
    return rt_nurb_eval_crv(crv, order - 1, param, k_vec,
			    k_index, coords);
}


void
rt_nurb_pr_crv(fastf_t *crv, int c_size, int coords)
{
    int i;

    fprintf(stderr, "\n");
    if (coords == 3)
	for (i = 0; i < c_size; i++)
	    fprintf(stderr, "p%d   %f   %f   %f\n", i, *((crv + ((i) * coords))),
		    *((crv + ((i) * coords)) + 1),
		    *((crv + ((i) * coords)) + 2));

    else if (coords == 4)
	for (i = 0; i < c_size; i++)
	    fprintf(stderr, "p%d   %f   %f   %f   %f\n", i,
		    *((crv + ((i) * coords))),
		    *((crv + ((i) * coords)) + 1),
		    *((crv + ((i) * coords)) + 2),
		    *((crv + ((i) * coords)) + 3));
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
