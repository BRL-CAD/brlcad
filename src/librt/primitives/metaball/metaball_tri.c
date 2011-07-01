/*			M B _ T R I . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file primitives/metaball/metaball_tri.c
 *
 * Tesselation/facetization routines for the metaball primitive.
 *
 * Here be magic.
 *
 *
 *              4
 *        4-----------5
 *      8/|         9/|
 *      / |   0     / |
 *     0-----------1  |5
 *     |  |7       |  |
 *     |  |   6   1|  |
 *    3|  7--------|--6
 *     | /         | /
 *     |/11        |/10
 *     3-----------2
 *          2
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
    struct shell *s;
    int numtri = 0;

    if (r) *r = NULL;
    if (m) NMG_CK_MODEL(m);

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
    if(radius < 0) {	/* no control points */
	bu_log("Attempting to tesselate metaball with no control points");
	return -1;
    }
    rt_metaball_set_bbox(center, radius, &min, &max);

    /* TODO: get better sampling tolerance, unless this is "good enough" */
    mtol = ttol->abs;
    V_MAX(mtol, ttol->rel * radius);
    V_MAX(mtol, tol->dist);

    *r = nmg_mrsv(m);	/* new empty nmg */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* the incredibly naïve approach. Time could be cut in half by simply
     * caching 4 point values, more by actually marching or doing active
     * refinement. This is the simplest pattern for now.
     */
    for (i = min[X]; i < max[X]; i += mtol)
	for (j = min[Y]; j < max[Y]; j += mtol)
	    for (k = min[Z]; k < max[Z]; k += mtol) {
		point_t p[8];
		int pv = 0;

		/* generate the vertex values */
#define MEH(c,di,dj,dk) VSET(p[c], i+di, j+dj, k+dk); pv |= rt_metaball_point_inside((const point_t *)&p[c], mb) << c;
		MEH(0, 0, 0, mtol);
		MEH(1, mtol, 0, mtol);
		MEH(2, mtol, 0, 0);
		MEH(3, 0, 0, 0);
		MEH(4, 0, mtol, mtol);
		MEH(5, mtol, mtol, mtol);
		MEH(6, mtol, mtol, 0);
		MEH(7, 0, mtol, 0);
#undef MEH

		if ( pv != 0 && pv != 255 ) {	/* entire cube is either inside or outside */
		    point_t edges[12];
		    int rval;

		    /* compute the edge values (if needed) */
#define MEH(a,b,c) if(!(pv&(1<<b)&&pv&(1<<c))) { \
    rt_metaball_find_intersection(edges+a, mb, (const point_t *)(p+b), (const point_t *)(p+c), mtol, finalstep); \
}

		    /* magic numbers! an edge, then the two attached vertices.
		     * For edge/vertex mapping, refer to the awesome ascii art
		     * at the beginning of this file. */
		    MEH(0 ,0,1);
		    MEH(1 ,1,2);
		    MEH(2 ,2,3);
		    MEH(3 ,0,3);
		    MEH(4 ,4,5);
		    MEH(5 ,5,6);
		    MEH(6 ,6,7);
		    MEH(7 ,4,7);
		    MEH(8 ,0,4);
		    MEH(9 ,1,5);
		    MEH(10,2,6);
		    MEH(11,3,7);
#undef MEH

		    rval = nmg_mc_realize_cube(s, pv, (point_t *)edges, tol);
		    numtri += rval;
		    if(rval < 0) {
			bu_log("Error attempting to realize a cube O.o\n");
			return rval;
		    }
		}
	    }

    nmg_mark_edges_real(&s->l.magic);
    nmg_region_a(*r, tol);

    nmg_model_fuse(m, tol);

    rt_get_timer(&times, NULL);
    bu_log("metaball tesselate (%d triangles): %s\n", numtri, bu_vls_addr(&times));

    return 0;
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
