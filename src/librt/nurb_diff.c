/*                     N U R B _ D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file nurb_diff.c
 *
 *	Differentiate a Non Uniform Rational B-Spline (NURB) Surface.
 *  Author -
 *	Paul Randal Stay
 *
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 */
/** @} */

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/* Given a NURB surface and a direction, differentiate the surface
 * and return a new surface which is the derivative of the original
 * surface.
 *
 * The algorithm is found in the following reference:
 *	Carl De Boor, "A Practical Guide To Splines", page 139
 *
 * The algorithm deals mainly with the new control mesh, but the new knot
 * vector is a subset of the original. (subtract a knot from each of
 * the ends).
 *
 * Arguments to rt_nurb_s_diff() --
 *	srf - NURB surface
 *	dir - parametric direction of the split.
 */

struct face_g_snurb *
rt_nurb_s_diff(const struct face_g_snurb *srf, int dir)
{
	struct face_g_snurb *nsrf;
	int	i;

	NMG_CK_SNURB(srf);

	if (dir == RT_NURB_SPLIT_ROW) {
		nsrf = (struct face_g_snurb *)
		rt_nurb_new_snurb( srf->order[0] - 1, srf->order[1],
		    srf->u.k_size - 2, srf->v.k_size,
		    srf->s_size[0], srf->s_size[1] - 1,
		    srf->pt_type, (struct resource *)NULL );

		for ( i = 0; i < srf->s_size[0]; i++) {
			fastf_t * old_points, *new_points;

			old_points = srf->ctl_points +
			    i * RT_NURB_EXTRACT_COORDS(srf->pt_type)
			*srf->s_size[1];

			new_points = nsrf->ctl_points +
			    i * RT_NURB_EXTRACT_COORDS(nsrf->pt_type)
			*nsrf->s_size[1];

			rt_nurb_mesh_diff( srf->order[0],
			    old_points, new_points, srf->u.knots,
			    RT_NURB_EXTRACT_COORDS(srf->pt_type),
			    RT_NURB_EXTRACT_COORDS(nsrf->pt_type),
			    srf->s_size[1], srf->pt_type);
		}

		for (i = 1; i < srf->u.k_size - 1; i++)
			nsrf->u.knots[i - 1] = srf->u.knots[i];

		for (i = 0; i < srf->v.k_size; i++)
			nsrf->v.knots[i] = srf->v.knots[i];
	} else	 {
		nsrf = (struct face_g_snurb *) rt_nurb_new_snurb(
		    srf->order[0], srf->order[1] - 1,
		    srf->u.k_size, srf->v.k_size - 2,
		    srf->s_size[0] - 1, srf->s_size[1],
		    srf->pt_type, (struct resource *)NULL );

		for ( i = 0; i < srf->s_size[1]; i++) {
			fastf_t * old_points, *new_points;

			old_points = srf->ctl_points +
			    i * RT_NURB_EXTRACT_COORDS(srf->pt_type);

			new_points = nsrf->ctl_points +
			    i * RT_NURB_EXTRACT_COORDS(nsrf->pt_type);

			rt_nurb_mesh_diff( srf->order[1],
			    old_points, new_points, srf->v.knots,
			    RT_NURB_EXTRACT_COORDS(srf->pt_type) *
			    srf->s_size[1],
			    RT_NURB_EXTRACT_COORDS(nsrf->pt_type) *
			    nsrf->s_size[1],
			    srf->s_size[0], srf->pt_type);
		}

		for (i = 0; i < srf->u.k_size; i++)
			nsrf->u.knots[i] = srf->u.knots[i];

		for (i = 1; i < srf->v.k_size - 1; i++)
			nsrf->v.knots[i-1] = srf->v.knots[i];
	}
	return nsrf;
}

/* Do the same thing for a curve. */

struct edge_g_cnurb *
rt_nurb_c_diff(const struct edge_g_cnurb *crv)
{

	struct edge_g_cnurb *ncrv;
	fastf_t * opts, *npts;
	int	i;

	NMG_CK_CNURB(crv);

	ncrv = (struct edge_g_cnurb *) rt_nurb_new_cnurb( crv->order - 1,
	    crv->k.k_size - 2, crv->c_size - 1,
	    crv->pt_type);

	opts = (fastf_t * ) crv->ctl_points;
	npts = (fastf_t * ) ncrv->ctl_points;

	rt_nurb_mesh_diff( crv->order, opts, npts, crv->k.knots,
	    RT_NURB_EXTRACT_COORDS( crv->pt_type),
	    RT_NURB_EXTRACT_COORDS( ncrv->pt_type),
	    crv->c_size, crv->pt_type );

	for ( i = 1; i < crv->k.k_size - 1; i++)
		ncrv->k.knots[ i - 1] = crv->k.knots[i];

	return ncrv;

}

void
rt_nurb_mesh_diff(int order, const fastf_t *o_pts, fastf_t *n_pts, const fastf_t *knots, int o_stride, int n_stride, int o_size, int pt_type)
{
	int	i, k;
	int	coords;
	fastf_t denom;

	coords = RT_NURB_EXTRACT_COORDS(pt_type);

	for ( i = 1; i < o_size; i++) {
		denom = knots[ i + order - 1] - knots[i];
		for (k = 0; k < coords; k++) {
			if (denom == 0.0)
				n_pts[k] = 0.0;
			else
				n_pts[k] = (order - 1) *
				    (o_pts[k+o_stride] - o_pts[k]) /
				    denom;
		}
		n_pts += n_stride;
		o_pts += o_stride;
	}
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
