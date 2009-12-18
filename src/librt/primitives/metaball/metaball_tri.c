/*			M B _ T R I . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2009 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file metaball.c
 *
 * Tesselation/facetization routines for the metaball primitive.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "wdb.h"

#include "metaball.h"

static int bitcount(unsigned char w) { return (w==0) ? 0 : bitcount(w>>1) + (w|1); }

/**
 * R T _ M E T A B A L L _ T E S S
 *
 * Tessellate a metaball.
 */
int
rt_metaball_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_metaball_internal *mb;
    fastf_t mtol, radius;
    point_t center, min, max;
    fastf_t i, j, k, finalstep = +INFINITY;
    struct bu_vls times;
    struct wdb_metaballpt *mbpt;

    RT_CK_DB_INTERNAL(ip);
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);

    bu_vls_init(&times);
    rt_prep_timer();

    /* since this geometry isn't necessarily prepped, we have to figure out the
     * finalstep and bounding box manually. */
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
	V_MIN(finalstep, mbpt->fldstr);
    finalstep /= (fastf_t)1e5;

    radius = rt_metaball_get_bounding_sphere(&center, mb->threshold, mb);
    rt_metaball_set_bbox(center, radius, &min, &max);

    /* TODO: get better sampling tolerance, unless this is "good enough" */
    mtol = ttol->abs;
    V_MAX(mtol, ttol->rel * radius);
    V_MAX(mtol, tol->dist);

    /* the incredibly naïve approach. Time could be cut in half by simply
     * caching 4 point values, more by actually marching or doing active
     * refinement. This is the simplest pattern for now.
     */
    for (i = min[X]; i<max[X]; i+=mtol)
	for (j = min[Y]; j<max[Y]; j+=mtol)
	    for (k = min[Z]; k<max[Z]; k+=mtol) {
		point_t p;
		int pv = 0;
		int pvbc;	/* bit count */
		VSET(p, i,      j,      k);		pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 0;
		VSET(p, i,      j,      k+mtol);	pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 1;
		VSET(p, i,      j+mtol, k);		pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 2;
		VSET(p, i,      j+mtol, k+mtol);	pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 3;
		VSET(p, i+mtol, j,      k);		pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 4;
		VSET(p, i+mtol, j,      k+mtol);	pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 5;
		VSET(p, i+mtol, j+mtol, k);		pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 6;
		VSET(p, i+mtol, j+mtol, k+mtol);	pv |= rt_metaball_point_inside((const point_t *)&p, mb) << 7;
		pvbc = bitcount(pv);
		if (pvbc==1) {
		    point_t a, b, mid;
		    VSET(a, i, j, k);
		    VSET(b, i, j, k+mtol);
		    rt_metaball_find_intersection(&mid, mb, (const point_t *)&a, (const point_t *)&*b, mtol, finalstep);
		    bu_log("Intersect between %f, %f, %f and %f, %f, %f is at %f, %f, %f\n", V3ARGS(a), V3ARGS(b), V3ARGS(mid));
		}
		/* should the actual surface intersection be searched for, or
		 * just say the mid point is good enough? */
		/* needs to be stitched into a triangle style NMG. Then
		 * decimated, perhaps? */
	    }

    rt_get_timer(&times, NULL);
    bu_log("metaball tesselate: %s\n", bu_vls_addr(&times));

    bu_log("ERROR: rt_metaball_tess called() is not implemented\n");
    return -1;
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
