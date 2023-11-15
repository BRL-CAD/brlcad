/*                C O N T I N U A T I O N . C P P
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

void
_ged_facetize_pnts_bbox(point_t rpp_min, point_t rpp_max, int pnt_cnt, point_t *pnts)
{
    int i = 0;
    if (!rpp_min || !rpp_max || !pnts || !pnts || !pnt_cnt) return;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    for (i = 0; i < pnt_cnt; i++) {
	VMINMAX(rpp_min, rpp_max, pnts[i]);
    }
}

void
_ged_facetize_rt_pnts_bbox(point_t rpp_min, point_t rpp_max, struct rt_pnts_internal *pnts)
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

double
_ged_facetize_bbox_vol(point_t b_min, point_t b_max)
{
    double bbox_vol = 0.0;
    fastf_t b_xlen, b_ylen, b_zlen;
    b_xlen = fabs(b_max[X] - b_min[X]);
    b_ylen = fabs(b_max[Y] - b_min[Y]);
    b_zlen = fabs(b_max[Z] - b_min[Z]);
    bbox_vol = b_xlen * b_ylen * b_zlen;
    return bbox_vol;
}


struct cm_info {
    double feature_size;
    double avg_thickness;
    double obj_bbox_vol;
    double pnts_bbox_vol;
    double bot_bbox_vol;
};
#define CM_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0}

int
_ged_continuation_obj(struct ged *gedp, const char *objname, const char *newname)
{
    struct cm_info cmi = CM_INFO_INIT;

    int max_time = 0; // TODO - pass in
    int max_pnts = 50000;  // TODO - pass in
    fastf_t feature_size = 0.0; // TODO - pass in
    fastf_t feature_scale = 1.0; // TODO - pass in
    fastf_t d_feature_size = 0.0; // TODO - pass in
    fastf_t target_feature_size = 0.0;
    int first_run = 1;
    int fatal_error_cnt = 0;
    int ret = BRLCAD_OK;
    double avg_thickness = 0.0;
    double min_len = 0.0;
    int face_cnt = 0;
    double successful_feature_size = 0.0;
    unsigned int successful_bot_count = 0;
    int decimation_succeeded = 0;
    double xlen, ylen, zlen;
    struct directory *dp;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal in_intern;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
    int polygonize_failure = 0;
    struct analyze_polygonize_params params = ANALYZE_POLYGONIZE_PARAMS_DEFAULT;
    int flags = 0;
    int free_pnts = 0;
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);

    dp = db_lookup(dbip, objname, LOOKUP_QUIET);
    if (!dp)
       	return BRLCAD_ERROR;

    // TODO - sanity check.  CM + plate mode == no-go

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("Error: could not determine type of object %s, skipping\n", objname);
	return BRLCAD_ERROR;
    }

    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS || in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_HALF) {
	/* If we have a point cloud or half, this won't work */
	return BRLCAD_ERROR;
    }


    BU_ALLOC(pnts, struct rt_pnts_internal);
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->count = 0;
    pnts->type = RT_PNT_TYPE_NRM;
    pnts->point = NULL;
    free_pnts = 1;

    /* Key some settings off the bbox size */
    rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&objname, 0, obj_min, obj_max);
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    xlen = fabs(rpp_max[X] - rpp_min[X]);
    ylen = fabs(rpp_max[Y] - rpp_min[Y]);
    zlen = fabs(rpp_max[Z] - rpp_min[Z]);

    /* Pick our mode(s) */
    flags |= ANALYZE_OBJ_TO_PNTS_RAND;
    flags |= ANALYZE_OBJ_TO_PNTS_SOBOL;

    /* Shoot - we need both the avg thickness of the hit partitions and seed points */
    if (analyze_obj_to_pnts(pnts, &avg_thickness, gedp->dbip, objname, &btol, flags, max_pnts, max_time, 1) || pnts->count <= 0) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_continuation_memfree;
    }

    /* Check the volume of the bounding box of input object against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  This one we are a bit more generous with, since the parent bbox
     * may not be terribly tight. */
    {
	point_t p_min, p_max;
	VSETALL(p_min, INFINITY);
	VSETALL(p_max, -INFINITY);
	_ged_facetize_rt_pnts_bbox(p_min, p_max, pnts);
	cmi.pnts_bbox_vol = _ged_facetize_bbox_vol(p_min, p_max);
	cmi.obj_bbox_vol = _ged_facetize_bbox_vol(rpp_min, rpp_max);
	if (fabs(cmi.obj_bbox_vol - cmi.pnts_bbox_vol)/cmi.obj_bbox_vol > 1) {
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_continuation_memfree;
	}
    }

    bu_log("CM: average raytrace thickness: %g\n", avg_thickness);

    /* Find the smallest value from either the bounding box lengths or the avg
     * thickness observed by the rays.  Some fraction of this value is our box
     * size for the polygonizer and the decimation routine */
    min_len = (xlen < ylen) ? xlen : ylen;
    min_len = (min_len < zlen) ? min_len : zlen;
    min_len = (min_len < avg_thickness) ? min_len : avg_thickness;

    if (feature_size > 0) {
	target_feature_size = 0.5*feature_size;
    } else {
	target_feature_size = min_len * feature_scale;
    }

    bu_log("CM: targeting feature size %g\n", target_feature_size);

    /* Build the BoT */
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->faces = NULL;
    bot->vertices = NULL;

    /* Run the polygonize routine.  Because it is quite simple to accidentally
     * specify inputs that will take huge amounts of time to run, we will
     * attempt a series of progressively courser polygonize runs until we
     * either succeed, or reach a feature size that is greater than 2x the
     * average thickness according to the raytracer without succeeding. If
     * max_time has been explicitly set to 0 by the caller this will run
     * unbounded, but the algorithm is n**2 and we're trying the finest level
     * first so may run a *very* long time... */
    pl = (struct pnt_normal *)pnts->point;
    pn = BU_LIST_PNEXT(pnt_normal, pl);
    if (!(feature_size > 0)) {
	feature_size = 2*avg_thickness;
    }

    params.max_time = max_time;
    params.verbosity = 1;
    params.minimum_free_mem = FACETIZE_MEMORY_THRESHOLD;

    while (!polygonize_failure && (feature_size > 0.9*target_feature_size || face_cnt < 1000) && fatal_error_cnt < 8) {
	double timestamp = bu_gettime();
	int delta;
	fastf_t *verts = bot->vertices;
	int *faces = bot->faces;
	int num_faces = bot->num_faces;
	int num_verts = bot->num_vertices;
	bot->vertices = NULL;
	bot->faces = NULL;
	polygonize_failure = analyze_polygonize(&(bot->faces), (int *)&(bot->num_faces),
						(point_t **)&(bot->vertices),
						(int *)&(bot->num_vertices),
						feature_size, pn->v, objname, gedp->dbip, &params);
	delta = (int)((bu_gettime() - timestamp)/1e6);
	if (polygonize_failure || bot->num_faces < successful_bot_count || delta < 2) {
	    if (polygonize_failure == 3) {
		bu_log("CM: Too little available memory to continue, aborting\n");
		ret = BRLCAD_ERROR;
		goto ged_facetize_continuation_memfree;
	    }
	    if (polygonize_failure == 2) {
		bu_log("CM: timed out after %d seconds with size %g\n", max_time, feature_size);
		/* If we still haven't had a successful run, back the feature size out and try again */
		if (first_run) {
		    polygonize_failure = 0;
		    feature_size = feature_size * 5;
		    fatal_error_cnt++;
		    continue;
		}
	    }
	    bot->faces = faces;
	    bot->vertices = verts;
	    bot->num_vertices = num_verts;
	    bot->num_faces = num_faces;
	    if (polygonize_failure != 2 && feature_size <= 0) {
		/* Something about the previous size didn't work - nudge the feature size and try again
		 * unless we've had multiple fatal errors. */
		polygonize_failure = 0;
		bu_log("CM: error at size %g\n", feature_size);
		/* If we've had a successful first run, just nudge the feature
		 * size down and retry.  If we haven't succeeded yet, and we've
		 * got just this one error, try dropping the feature size down by an order
		 * of magnitude.  If we haven't succeed yet *and* we've got
		 * multiple fatal errors, try dropping it by half. */
		feature_size = feature_size * ((!first_run) ? 0.95 : ((fatal_error_cnt) ? 0.5 : 0.1));
		bu_log("CM: retrying with size %g\n", feature_size);
		fatal_error_cnt++;
		continue;
	    }
	    feature_size = successful_feature_size;
	    if (bot->faces) {
		if (feature_size <= 0) {
		    bu_log("CM: unable to polygonize at target size (%g), using last successful BoT with %d faces, feature size %g\n", target_feature_size, (int)bot->num_faces, successful_feature_size);
		} else {
		    bu_log("CM: successfully created %d faces, feature size %g\n", (int)bot->num_faces, successful_feature_size);
		}
	    }
	} else {
	    if (verts) bu_free(verts, "old verts");
	    if (faces) bu_free(faces, "old faces");
	    /* if we have had a fatal error in the past, reset on subsequent success */
	    fatal_error_cnt = 0;
	    successful_feature_size = feature_size;
	    bu_log("CM: completed in %d seconds with size %g\n", delta, feature_size);
	    feature_size = feature_size * ((delta < 5) ? 0.7 : 0.9);
	    face_cnt = bot->num_faces;
	    successful_bot_count = bot->num_faces;
	}
	first_run = 0;
    }

    if (bot->num_faces && feature_size < target_feature_size) {
	bu_log("CM: successfully polygonized BoT with %d faces at feature size %g\n", (int)bot->num_faces, feature_size);
    }

    if (!bot->faces) {
	bu_log("CM: surface reconstruction failed: %s\n", objname);
	ret = BRLCAD_ERROR;
	goto ged_facetize_continuation_memfree;
    }

    /* do decimation */
    {
	d_feature_size = (d_feature_size > 0) ? d_feature_size : 1.5 * feature_size;
	struct rt_bot_internal *obot = bot;

	bu_log("CM: decimating with feature size %g\n", d_feature_size);

	bot = _tess_facetize_decimate(bot, d_feature_size);

	if (bot == obot) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_continuation_memfree;
	}
	if (bot != obot) {
	    decimation_succeeded = 1;
	}
    }

    /* Check the volume of the bounding box of the BoT against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  For the moment, use >50% difference. */
    {
	point_t b_min, b_max;
	VSETALL(b_min, INFINITY);
	VSETALL(b_max, -INFINITY);
	_ged_facetize_pnts_bbox(b_min, b_max, bot->num_vertices, (point_t *)bot->vertices);
	cmi.bot_bbox_vol = _ged_facetize_bbox_vol(b_min, b_max);
	if (fabs(cmi.pnts_bbox_vol - cmi.bot_bbox_vol) > cmi.pnts_bbox_vol * 0.5) {
	    ret = BRLCAD_ERROR;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    goto ged_facetize_continuation_memfree;
	}
    }

    /* Check the volume of the bounding box of the BoT against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  For the moment, use >50% difference. */
    {
	point_t b_min, b_max;
	VSETALL(b_min, INFINITY);
	VSETALL(b_max, -INFINITY);
	_ged_facetize_pnts_bbox(b_min, b_max, bot->num_vertices, (point_t *)bot->vertices);
	cmi.bot_bbox_vol = _ged_facetize_bbox_vol(b_min, b_max);
	if (fabs(cmi.pnts_bbox_vol - cmi.bot_bbox_vol) > cmi.pnts_bbox_vol * 0.5) {
	    ret = BRLCAD_ERROR;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    goto ged_facetize_continuation_memfree;
	}
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = BRLCAD_ERROR;
	    bu_log("CM: facetization failed, final BoT was not solid\n");
	    goto ged_facetize_continuation_memfree;
	}
    }

    if (decimation_succeeded) {
	bu_log("CM: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

    ret = _tess_facetize_write_bot(gedp, bot, newname);

ged_facetize_continuation_memfree:
    cmi.feature_size = feature_size;
    cmi.avg_thickness = avg_thickness;

    if (free_pnts && pnts) {
	struct pnt_normal *rpnt = (struct pnt_normal *)pnts->point;
	if (rpnt) {
	    struct pnt_normal *entry;
	    while (BU_LIST_WHILE(entry, pnt_normal, &(rpnt->l))) {
		BU_LIST_DEQUEUE(&(entry->l));
		BU_PUT(entry, struct pnt_normal);
	    }
	    BU_PUT(rpnt, struct pnt_normal);
	}
	bu_free(pnts, "free pnts");
    }
    rt_db_free_internal(&in_intern);

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

