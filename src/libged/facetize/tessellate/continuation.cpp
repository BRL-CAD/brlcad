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

static void
_pnts_bbox(point_t rpp_min, point_t rpp_max, int pnt_cnt, point_t *pnts)
{
    int i = 0;
    if (!rpp_min || !rpp_max || !pnts || !pnts || !pnt_cnt) return;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    for (i = 0; i < pnt_cnt; i++) {
	VMINMAX(rpp_min, rpp_max, pnts[i]);
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

void free_bot_internal(struct rt_bot_internal *bot)
{
    if (!bot)
	return;

    if (bot->faces)
	bu_free(bot->faces, "faces");

    if (bot->vertices)
	bu_free(bot->vertices, "vertices");

    BU_PUT(bot, struct rt_bot_internal);
}

int
bot_gen(struct rt_bot_internal **obot, fastf_t feature_size, point_t seed, const char *objname, struct db_i *dbip, struct analyze_polygonize_params *params)
{
    /* Build the BoT */
    struct rt_bot_internal *bot;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->faces = NULL;
    bot->vertices = NULL;
    int ret = analyze_polygonize(&(bot->faces), (int *)&(bot->num_faces),
	    (point_t **)&(bot->vertices),
	    (int *)&(bot->num_vertices),
	    feature_size, seed, objname, dbip, params);

    if (ret == 3)
	bu_log("CM: Too little available memory to continue, aborting\n");

    if (ret == 2)
	bu_log("CM: timed out after %d seconds with size %g\n", params->max_time, feature_size);

    if (!ret) {
	*obot = bot;
    } else {
	free_bot_internal(bot);
    }
    return ret;
}

int
continuation_mesh(struct rt_bot_internal **obot, struct db_i *dbip, const char *objname, struct tess_opts *s, point_t seed)
{

    int first_run = 1;
    int fatal_error_cnt = 0;
    double successful_feature_size = 0.0;
    int decimation_succeeded = 0;
    struct rt_bot_internal *bot = NULL;
    struct rt_bot_internal *dbot = NULL;
    int pret = -1;
    struct analyze_polygonize_params params = ANALYZE_POLYGONIZE_PARAMS_DEFAULT;
    double feature_size = s->feature_size;

    /* Run the polygonize routine.  Because it is quite simple to accidentally
     * specify inputs that will take huge amounts of time to run, we will
     * attempt a series of progressively courser polygonize runs until we
     * either succeed, or reach a feature size that is greater than 2x the
     * average thickness according to the raytracer without succeeding. If
     * max_time has been explicitly set to 0 by the caller this will run
     * unbounded, but the algorithm is n**2 and we're trying the finest level
     * first so may run a *very* long time... */
    params.max_time = s->max_time;
    params.verbosity = 1;
    params.minimum_free_mem = FACETIZE_MEMORY_THRESHOLD;

    double timestamp = bu_gettime();
    while (feature_size > 0.9*s->target_feature_size) {
	struct rt_bot_internal *candidate = NULL;
	pret = bot_gen(&candidate, feature_size, seed, objname, dbip, &params);
	fastf_t delta = (int)((bu_gettime() - timestamp)/1e6);
	if (s->max_time > 0 && delta > s->max_time)
	    break;

	if (!pret && candidate->num_faces) {
	    // Success - check output
	    if (!bot) {
		bot = candidate;
		successful_feature_size = feature_size;
	    } else {
		if (bot->num_faces == candidate->num_faces) {
		    // Stable answer - breaking out of loop
		    free_bot_internal(candidate);
		    break;
		}
		if (bot->num_faces < candidate->num_faces) { 
		    free_bot_internal(bot);
		    bot = candidate;
		    successful_feature_size = feature_size;
		} else {
		    // Lost faces compared to prior output - rejecting
		    free_bot_internal(candidate);
		}
	    }
	}

	if (pret) {
	    fatal_error_cnt++;
	    if (first_run) {
		pret = 0;
		feature_size = feature_size * 5;
	    }
	}

	first_run = 0;
	if (pret == 2 || pret == 3) 
	    break;

	/* Nudge the feature size and try again */
	pret = 0;
	feature_size = feature_size * ((!first_run) ? 0.95 : ((fatal_error_cnt) ? 0.5 : 0.1));
	bu_log("CM: retrying with size %g\n", feature_size);
    }

    if (bot && bot->num_faces) {
	bu_log("CM: successfully polygonized BoT with %d faces at feature size %g\n", (int)bot->num_faces, successful_feature_size);
    }

    if (!bot->faces) {
	bu_log("CM: surface reconstruction failed: %s\n", objname);
	if (bot->vertices) bu_free(bot->vertices, "verts");
	if (bot->faces) bu_free(bot->faces, "verts");
	BU_PUT(bot, struct rt_bot_internal *);
	return BRLCAD_ERROR;
    }

    /* do decimation */
    {
	double d_feature_size = (s->d_feature_size > 0) ? s->d_feature_size : 1.5 * feature_size;
	*obot = bot;

	bu_log("CM: decimating with feature size %g\n", d_feature_size);

	dbot = _tess_facetize_decimate(bot, d_feature_size);

	if (bot == dbot || !dbot->num_vertices || !dbot->num_faces) {
	    bu_log("decimation failed\n");
	    return BRLCAD_ERROR;
	}

	bot = dbot;
	decimation_succeeded = 1;
    }

    /* Get the volume of the bounding box of the decimated BoT */
    point_t b_min, b_max;
    VSETALL(b_min, INFINITY);
    VSETALL(b_max, -INFINITY);
    _pnts_bbox(b_min, b_max, bot->num_vertices, (point_t *)bot->vertices);
    double bot_bbox_vol = _bbox_vol(b_min, b_max);

    /* Check the BoT bbox vol against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  For the moment, use >50% difference. */
    if (fabs(s->pnts_bbox_vol - bot_bbox_vol) > s->pnts_bbox_vol * 0.5) {
	if (bot->vertices) bu_free(bot->vertices, "verts");
	if (bot->faces) bu_free(bot->faces, "verts");
	BU_PUT(bot, struct rt_bot_internal);
	return BRLCAD_ERROR;
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    BU_PUT(bot, struct rt_bot_internal);
	    bu_log("CM: facetization failed, final BoT was not solid\n");
	    return BRLCAD_ERROR;
	}
    }

    if (decimation_succeeded) {
	bu_log("CM: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

    *obot = bot;

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

