/*                      S P S R . C P P
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
/** @file libged/facetize/tessellate/spsr.cpp
 *
 * Tessellation interface to the Screened Poisson surface reconstruction
 * algorithm.
 *
 */

#include "common.h"

#include "bg/spsr.h"
#include "../../ged_private.h"
#include "./tessellate.h"

static int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    if (db_lookup(dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

int
spsr_mesh(struct rt_bot_internal **obot, struct db_i *dbip, struct rt_pnts_internal *pnts, struct tess_opts *s)
{
    int ret = BRLCAD_OK;
    struct directory *dp;
    int decimation_succeeded = 0;
    int max_time = 0; // TODO - pass in
    struct rt_db_internal in_intern;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    double avg_thickness = 0.0;
    int flags = 0;
    int i = 0;
    struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    point_t *input_points_3d = NULL;
    vect_t *input_normals_3d = NULL;
    point_t rpp_min, rpp_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);

    input_points_3d = (point_t *)bu_calloc(pnts->count, sizeof(point_t), "points");
    input_normals_3d = (vect_t *)bu_calloc(pnts->count, sizeof(vect_t), "normals");

    struct pnt_normal *pn;
    struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
    i = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMOVE(input_points_3d[i], pn->v);
	VMOVE(input_normals_3d[i], pn->n);
	i++;
    }

    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    if (bg_3d_spsr(&(bot->faces), (int *)&(bot->num_faces),
		   (point_t **)&(bot->vertices),
		   (int *)&(bot->num_vertices),
		   (const point_t *)input_points_3d,
		   (const vect_t *)input_normals_3d,
		   pnts->count, &s_opts) ) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_spsr_memfree;
    }

    /* do decimation */
    {
	*obot = bot;
	fastf_t feature_size = 0.0; // TODO - pass in
	fastf_t xlen = fabs(rpp_max[X] - rpp_min[X]);
	fastf_t ylen = fabs(rpp_max[Y] - rpp_min[Y]);
	fastf_t zlen = fabs(rpp_max[Z] - rpp_min[Z]);
	feature_size = (xlen < ylen) ? xlen : ylen;
	feature_size = (feature_size < zlen) ? feature_size : zlen;
	feature_size = feature_size * 0.15;

	bu_log("SPSR: decimating with feature size: %g\n", feature_size);

	bot = _tess_facetize_decimate(bot, feature_size);

	if (bot == *obot) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_spsr_memfree;
	}
	if (bot != *obot) {
	    decimation_succeeded = 1;
	}
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = BRLCAD_ERROR;
	    bu_log("SPSR: facetization failed, final BoT was not solid\n");
	    goto ged_facetize_spsr_memfree;
	}
    }

    /* Because SPSR has some observed failure modes that produce a "valid" BoT totally different
     * from the original shape, if we know the avg. thickness of the original object raytrace
     * the candidate BoT and compare the avg. thicknesses */
    if (avg_thickness > 0) {
	int max_pnts = s->max_pnts; // old value 200000;
	double navg_thickness = 0.0;
	struct bu_vls tmpname = BU_VLS_INIT_ZERO;
	struct rt_bot_internal *tbot = NULL;
	BU_ALLOC(tbot, struct rt_bot_internal);
	tbot->magic = RT_BOT_INTERNAL_MAGIC;
	tbot->mode = RT_BOT_SOLID;
	tbot->orientation = RT_BOT_UNORIENTED;
	tbot->thickness = (fastf_t *)NULL;
	tbot->face_mode = (struct bu_bitv *)NULL;

	tbot->num_vertices = bot->num_vertices;
	tbot->num_faces = bot->num_faces;
	tbot->vertices = (fastf_t *)bu_malloc(sizeof(fastf_t) * tbot->num_vertices * 3, "vert array");
	memcpy(tbot->vertices, bot->vertices, sizeof(fastf_t) * tbot->num_vertices * 3);
	tbot->faces = (int *)bu_malloc(sizeof(int) * tbot->num_faces * 3, "faces array");
	memcpy(tbot->faces, bot->faces, sizeof(int) * tbot->num_faces *3);

	flags = ANALYZE_OBJ_TO_PNTS_RAND;
	bu_vls_sprintf(&tmpname, "spsr_bot.tmp");
	if (db_lookup(dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(&tmpname, "-0");
	    bu_vls_incr(&tmpname, NULL, NULL, &_db_uniq_test, (void *)dbip);
	}
	if (_tess_facetize_write_bot(dbip, tbot, bu_vls_addr(&tmpname)) & BRLCAD_ERROR) {
	    bu_log("SPSR: could not write BoT to temporary name %s\n", bu_vls_addr(&tmpname));
	    bu_vls_free(&tmpname);
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_spsr_memfree;
	}

	if (analyze_obj_to_pnts(NULL, &navg_thickness, dbip, bu_vls_cstr(&tmpname), &btol, flags, max_pnts, max_time, 1)) {
	    bu_log("SPSR: could not raytrace temporary BoT %s\n", bu_vls_cstr(&tmpname));
	    ret = BRLCAD_ERROR;
	}

	/* Remove the temporary BoT object, succeed or fail. */
	dp = db_lookup(dbip, bu_vls_cstr(&tmpname), LOOKUP_QUIET);
	if (!dp || db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
	    bu_vls_free(&tmpname);
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_spsr_memfree;
	}

	if (fabs(avg_thickness - navg_thickness) > avg_thickness * 0.5) {
	    bu_log("SPSR: BoT average sampled thickness %f is widely different from original sampled thickness %f\n", navg_thickness, avg_thickness);
	    ret = BRLCAD_ERROR;
	    bu_vls_free(&tmpname);
	    goto ged_facetize_spsr_memfree;
	}

	/* Passed test, continue */
	bu_vls_free(&tmpname);
    }

    if (decimation_succeeded) {
	bu_log("SPSR: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

ged_facetize_spsr_memfree:
    if (input_points_3d) bu_free(input_points_3d, "3d pnts");
    if (input_normals_3d) bu_free(input_normals_3d, "3d pnts");
    rt_db_free_internal(&in_intern);

    *obot = bot;

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

