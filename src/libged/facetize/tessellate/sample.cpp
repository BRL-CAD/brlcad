/*                     S A M P L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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

std::string
sample_opts::print_options_help()
{
    std::string h;
    h.append("feature_scale    -  Percentage of the average thickness observed by\n");
    h.append("                    the raytracer to use for a targeted feature size\n");
    h.append("                    with sampling based methods.\n");
    h.append("feature_size     -  Explicit feature length to try for sampling\n");
    h.append("                    based methods - overrides feature_scale.\n");
    h.append("d_feature_size   -  Initial feature length to try for decimation\n");
    h.append("                    in sampling based methods.  By default, this\n");
    h.append("                    value is set to 1.5x the feature size.\n");
    h.append("max_sample_time  -  Maximum time to allow point sampling to continue\n");
    h.append("max_pnts         -  Maximum number of points to sample\n");
    return h;
}

int
sample_opts::set_var(const std::string &key, const std::string &val)
{
    if (key.length() == 0)
	return BRLCAD_ERROR;

    const char *cstr[2];
    cstr[0] = val.c_str();
    cstr[1] = NULL;

    if (key == std::string("feature_scale")) {
	if (!val.length()) {
	    feature_scale = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&feature_scale) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("feature_size")) {
	if (!val.length()) {
	    feature_size = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&feature_size) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("d_feature_size")) {
	if (!val.length()) {
	    d_feature_size = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&d_feature_size) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("max_sample_time")) {
	if (!val.length()) {
	    max_sample_time = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_sample_time) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("max_pnts")) {
	if (!val.length()) {
	    max_pnts = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_pnts) < 0)
	    return BRLCAD_ERROR;
    }

    return BRLCAD_ERROR;
}

void
sample_opts::sync(sample_opts &o)
{
    feature_scale = o.feature_scale;
    feature_size = o.feature_size;
    d_feature_size = o.d_feature_size;
    max_sample_time = o.max_sample_time;
    max_pnts = o.max_pnts;
    obj_bbox_vol = o.obj_bbox_vol;
    pnts_bbox_vol = o.pnts_bbox_vol;
    target_feature_size = o.target_feature_size;
    avg_thickness = o.avg_thickness;
}

bool
sample_opts::equals(sample_opts &o)
{
    if (!NEAR_EQUAL(feature_scale, o.feature_scale, VUNITIZE_TOL))
	return false;
    if (!NEAR_EQUAL(feature_size, o.feature_size, VUNITIZE_TOL))
	return false;
    if (!NEAR_EQUAL(d_feature_size, o.d_feature_size, VUNITIZE_TOL))
	return false;
    if (max_sample_time != o.max_sample_time)
	return false;
    if (max_pnts != o.max_pnts)
	return false;

    return true;
}

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
    struct bn_tol btol = BN_TOL_INIT_TOL;
    if (analyze_obj_to_pnts(pnts, &s->pnt_options.avg_thickness, dbip, oname, &btol, flags, s->pnt_options.max_pnts, s->pnt_options.max_sample_time, 1) || pnts->count <= 0) {
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
	s->pnt_options.pnts_bbox_vol = _bbox_vol(p_min, p_max);
	s->pnt_options.obj_bbox_vol = _bbox_vol(rpp_min, rpp_max);
	if (fabs(s->pnt_options.obj_bbox_vol - s->pnt_options.pnts_bbox_vol)/s->pnt_options.obj_bbox_vol > 1) {
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
    min_len = (min_len < s->pnt_options.avg_thickness) ? min_len : s->pnt_options.avg_thickness;

    if (s->pnt_options.feature_size > 0) {
	s->pnt_options.target_feature_size = 0.5*s->pnt_options.feature_size;
    } else {
	s->pnt_options.target_feature_size = min_len * s->pnt_options.feature_scale;
    }

    bu_log("feature_size: %f\n", s->pnt_options.feature_size);
    bu_log("feature_scale: %f\n", s->pnt_options.feature_scale);
    bu_log("target_feature_size: %f\n", s->pnt_options.target_feature_size);

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

