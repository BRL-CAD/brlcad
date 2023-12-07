/*                     S A M P L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/continuation.cpp
 *
 * The facetize command's Continuation Method implementation.
 */

#include "common.h"

#include "vmath.h"
#include "bu/time.h"
#include "bg/trimesh.h"
#include "raytrace.h"
#include "analyze/polygonize.h"
#include "../../ged_private.h"
#include "./tessellate.h"

#define FACETIZE_MEMORY_THRESHOLD 150000000

static void
_rt_pnts_bbox(point_t rpp_min, point_t rpp_max, struct rt_pnts_internal *pnts)
{
    struct pnt_normal *pn = NULL;
    struct pnt_normal *pl = NULL;
    if (!rpp_min || !rpp_max || !pnts || !pnts->point) return;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMINMAX(rpp_min, rpp_max, pn->v);
    }
}

static double
_bbox_vol(point_t b_min, point_t b_max)
{
    double bbox_vol = 0.0;
    fastf_t b_xlen, b_ylen, b_zlen;
    b_xlen = fabs(b_max[X] - b_min[X]);
    b_ylen = fabs(b_max[Y] - b_min[Y]);
    b_zlen = fabs(b_max[Z] - b_min[Z]);
    bbox_vol = b_xlen * b_ylen * b_zlen;
    return bbox_vol;
}

struct rt_pnts_internal *
_tess_pnts_sample(const char *oname, struct db_i *dbip, struct tess_opts *s)
{
    if (!oname || !dbip || !s)
	return NULL;

    struct directory *dp = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (!dp)
	return NULL;

    /* If we have a point cloud or half, this won't work */
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_PNTS || dp->d_minor_type == DB5_MINORTYPE_BRLCAD_HALF)
	return NULL;

    /* Key some settings off the bbox size */
    point_t obj_min, obj_max;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    rt_obj_bounds(&msg, dbip, 1, (const char **)&oname, 0, obj_min, obj_max);
    if (bu_vls_strlen(&msg))
	bu_log("Bounds calculation: %s\n", bu_vls_cstr(&msg));
    bu_vls_free(&msg);

    point_t rpp_min, rpp_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    double xlen = fabs(rpp_max[X] - rpp_min[X]);
    double ylen = fabs(rpp_max[Y] - rpp_min[Y]);
    double zlen = fabs(rpp_max[Z] - rpp_min[Z]);

    /* Pick our mode(s) */
    int flags = 0;
    flags |= ANALYZE_OBJ_TO_PNTS_RAND;
    //flags |= ANALYZE_OBJ_TO_PNTS_SOBOL;

    struct rt_pnts_internal *pnts = NULL;
    BU_GET(pnts, struct rt_pnts_internal);
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->count = 0;
    pnts->type = RT_PNT_TYPE_NRM;
    pnts->point = NULL;

    /* Shoot - we need both the avg thickness of the hit partitions and seed points */
    double avg_thickness = 0.0;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    if (analyze_obj_to_pnts(pnts, &avg_thickness, dbip, oname, &btol, flags, s->max_pnts, s->max_time, 1) || pnts->count <= 0) {
	struct pnt_normal *rpnt = (struct pnt_normal *)pnts->point;
	if (rpnt) {
	    struct pnt_normal *entry;
	    while (BU_LIST_WHILE(entry, pnt_normal, &(rpnt->l))) {
		BU_LIST_DEQUEUE(&(entry->l));
		BU_PUT(entry, struct pnt_normal);
	    }
	    BU_PUT(rpnt, struct pnt_normal);
	}
	BU_PUT(pnts, struct rt_pnts_internal);
	return NULL;
    }

    /* Check the volume of the bounding box of input object against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  This one we are a bit more generous with, since the parent bbox
     * may not be terribly tight. */
    {
	point_t p_min, p_max;
	VSETALL(p_min, INFINITY);
	VSETALL(p_max, -INFINITY);
	_rt_pnts_bbox(p_min, p_max, pnts);
	s->pnts_bbox_vol = _bbox_vol(p_min, p_max);
	s->obj_bbox_vol = _bbox_vol(rpp_min, rpp_max);
	if (fabs(s->obj_bbox_vol - s->pnts_bbox_vol)/s->obj_bbox_vol > 1) {
	    struct pnt_normal *rpnt = (struct pnt_normal *)pnts->point;
	    if (rpnt) {
		struct pnt_normal *entry;
		while (BU_LIST_WHILE(entry, pnt_normal, &(rpnt->l))) {
		    BU_LIST_DEQUEUE(&(entry->l));
		    BU_PUT(entry, struct pnt_normal);
		}
		BU_PUT(rpnt, struct pnt_normal);
	    }
	    BU_PUT(pnts, struct rt_pnts_internal);
	    return NULL;
	}
    }

    /* Find the smallest value from either the bounding box lengths or the avg
     * thickness observed by the rays.  Some fraction of this value is our box
     * size for the polygonizer and the decimation routine */
    double min_len = (xlen < ylen) ? xlen : ylen;
    min_len = (min_len < zlen) ? min_len : zlen;
    min_len = (min_len < avg_thickness) ? min_len : avg_thickness;

    if (s->feature_size > 0) {
	s->target_feature_size = 0.5*s->feature_size;
    } else {
	s->target_feature_size = min_len * s->feature_scale;
    }

    if (!(s->feature_size > 0)) {
	s->feature_size = 2*avg_thickness;
    }
    if (!(s->target_feature_size > 0)) {
	s->target_feature_size = 2*avg_thickness;
    }

    return pnts;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

