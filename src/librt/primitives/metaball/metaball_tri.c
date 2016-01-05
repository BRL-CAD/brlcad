/*                    M E T A B A L L _ T R I . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2016 United States Government as represented by
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
 * Tessellation/facetization routines for the metaball primitive.
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
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/nurb.h"
#include "wdb.h"

#include "metaball.h"

/**
 * Tessellate a metaball.
 */
int
rt_metaball_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_metaball_internal *mb;
    fastf_t mtol, radius;
    point_t center, min, max;
    fastf_t i, j, k, finalstep = +INFINITY;
    struct bu_vls times = BU_VLS_INIT_ZERO;
    struct wdb_metaballpt *mbpt;
    struct shell *s;
    int numtri = 0;

    if (r == NULL || m == NULL)
	return -1;
    *r = NULL;

    NMG_CK_MODEL(m);

    RT_CK_DB_INTERNAL(ip);
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);

    rt_prep_timer();

    /* since this geometry isn't necessarily prepped, we have to figure out the
     * finalstep and bounding box manually. */
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
	V_MIN(finalstep, mbpt->fldstr);
    finalstep /= (fastf_t)1e5;

    radius = rt_metaball_get_bounding_sphere(&center, mb->threshold, mb);
    if (radius < 0) {	/* no control points */
	bu_log("Attempting to tessellate metaball with no control points");
	return -1;
    }
    rt_metaball_bbox(ip, &min, &max, tol);

    /* TODO: get better sampling tolerance, unless this is "good enough" */
    mtol = ttol->abs;
    V_MAX(mtol, ttol->rel * radius * 10);
    V_MAX(mtol, tol->dist);

    *r = nmg_mrsv(m);	/* new empty nmg */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

