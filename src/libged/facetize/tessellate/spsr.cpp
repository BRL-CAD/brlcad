/*                     F A C E T I Z E . C P P
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
/** @file libged/facetize.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include "bg/spsr.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

int
_ged_spsr_obj(struct _ged_facetize_state *s, const char *objname, const char *newname)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_OK;
    struct directory *dp;
    int decimation_succeeded = 0;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal in_intern;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
    double avg_thickness = 0.0;
    int flags = 0;
    int i = 0;
    int free_pnts = 0;
    point_t *input_points_3d = NULL;
    vect_t *input_normals_3d = NULL;
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);

    dp = db_lookup(dbip, objname, LOOKUP_QUIET);
    if (!dp) {
	if (s->verbosity) {
	    bu_log("Error: could not determine type of object %s, skipping\n", objname);
	}
	return FACETIZE_FAILURE;
    }

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	if (s->verbosity) {
	    bu_log("Error: could not determine type of object %s, skipping\n", objname);
	}
	return FACETIZE_FAILURE;
    }

    /* If we have a half, this won't work */
    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_HALF) {
	return FACETIZE_FAILURE;
    }

    /* Key some settings off the bbox size */
    rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&objname, 0, obj_min, obj_max);
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);


    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS) {
	/* If we have a point cloud, there's no need to raytrace it */
	pnts = (struct rt_pnts_internal *)in_intern.idb_ptr;

    } else {
	int max_pnts = (s->max_pnts) ? s->max_pnts : 200000;
	BU_ALLOC(pnts, struct rt_pnts_internal);
	pnts->magic = RT_PNTS_INTERNAL_MAGIC;
	pnts->scale = 0.0;
	pnts->type = RT_PNT_TYPE_NRM;
	flags = ANALYZE_OBJ_TO_PNTS_RAND;
	free_pnts = 1;

	if (s->verbosity) {
	    bu_log("SPSR: generating %d points from %s\n", max_pnts, objname);
	}

	if (analyze_obj_to_pnts(pnts, &avg_thickness, gedp->dbip, objname, &btol, flags, max_pnts, s->max_time, s->verbosity)) {
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	}
    }

    input_points_3d = (point_t *)bu_calloc(pnts->count, sizeof(point_t), "points");
    input_normals_3d = (vect_t *)bu_calloc(pnts->count, sizeof(vect_t), "normals");
    i = 0;
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMOVE(input_points_3d[i], pn->v);
	VMOVE(input_normals_3d[i], pn->n);
	i++;
    }

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
		   pnts->count, &(s->s_opts)) ) {
	ret = FACETIZE_FAILURE;
	goto ged_facetize_spsr_memfree;
    }

    /* do decimation */
    {
	struct rt_bot_internal *obot = bot;
	fastf_t feature_size = 0.0;
	if (s->feature_size > 0) {
	    feature_size = s->feature_size;
	} else {
	    fastf_t xlen = fabs(rpp_max[X] - rpp_min[X]);
	    fastf_t ylen = fabs(rpp_max[Y] - rpp_min[Y]);
	    fastf_t zlen = fabs(rpp_max[Z] - rpp_min[Z]);
	    feature_size = (xlen < ylen) ? xlen : ylen;
	    feature_size = (feature_size < zlen) ? feature_size : zlen;
	    feature_size = feature_size * 0.15;
	}

	if (s->verbosity) {
	    bu_log("SPSR: decimating with feature size: %g\n", feature_size);
	}

	bot = _ged_facetize_decimate(s, bot, feature_size);

	if (bot == obot) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	}
	if (bot != obot) {
	    decimation_succeeded = 1;
	}
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    if (!s->quiet) {
		bu_log("SPSR: facetization failed, final BoT was not solid\n");
	    }
	    goto ged_facetize_spsr_memfree;
	}
    }

    /* Because SPSR has some observed failure modes that produce a "valid" BoT totally different
     * from the original shape, if we know the avg. thickness of the original object raytrace
     * the candidate BoT and compare the avg. thicknesses */
    if (avg_thickness > 0) {
	const char *av[3];
	int max_pnts = (s->max_pnts) ? s->max_pnts : 200000;
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
	bu_vls_sprintf(&tmpname, "%s.tmp", newname);
	if (db_lookup(gedp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(&tmpname, "-0");
	    bu_vls_incr(&tmpname, NULL, NULL, &_db_uniq_test, (void *)gedp);
	}
	if (_ged_facetize_write_bot(s, tbot, bu_vls_addr(&tmpname)) & BRLCAD_ERROR) {
	    bu_log("SPSR: could not write BoT to temporary name %s\n", bu_vls_addr(&tmpname));
	    bu_vls_free(&tmpname);
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	}

	if (analyze_obj_to_pnts(NULL, &navg_thickness, gedp->dbip, bu_vls_addr(&tmpname), &btol, flags, max_pnts, s->max_time, s->verbosity)) {
	    bu_log("SPSR: could not raytrace temporary BoT %s\n", bu_vls_addr(&tmpname));
	    ret = FACETIZE_FAILURE;
	}

	/* Remove the temporary BoT object, succeed or fail. */
	av[0] = "kill";
	av[1] = bu_vls_addr(&tmpname);
	av[2] = NULL;
	(void)ged_exec(gedp, 2, (const char **)av);

	if (ret == FACETIZE_FAILURE) {
	    bu_vls_free(&tmpname);
	    goto ged_facetize_spsr_memfree;
	}


	if (fabs(avg_thickness - navg_thickness) > avg_thickness * 0.5) {
	    bu_log("SPSR: BoT average sampled thickness %f is widely different from original sampled thickness %f\n", navg_thickness, avg_thickness);
	    ret = FACETIZE_FAILURE;
	    bu_vls_free(&tmpname);
	    goto ged_facetize_spsr_memfree;
	}

	/* Passed test, continue */
	bu_vls_free(&tmpname);
    }

    if (decimation_succeeded && !s->quiet) {
	bu_log("SPSR: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

    if (!s->make_nmg) {

	ret = _ged_facetize_write_bot(s, bot, newname);

    } else {
	/* Convert BoT to NMG */
	struct model *m = nmg_mm();
	struct nmgregion *nr;
	struct rt_db_internal intern;

	/* Use intern to fake out rt_bot_tess, since it expects an
	 * rt_db_internal wrapper */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_ptr = (void *)bot;
	if (rt_bot_tess(&nr, m, &intern, NULL, &btol) < 0) {
	    if (!s->quiet) {
		bu_log("SPSR: failed to convert BoT to NMG: %s\n", objname);
	    }
	    rt_db_free_internal(&intern);
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	} else {
	    /* OK,have NMG now - write it out */
	    ret = _ged_facetize_write_nmg(s, m, newname);
	    rt_db_free_internal(&intern);
	}
    }

ged_facetize_spsr_memfree:
    if (free_pnts) bu_free(pnts, "free pnts");
    if (input_points_3d) bu_free(input_points_3d, "3d pnts");
    if (input_normals_3d) bu_free(input_normals_3d, "3d pnts");
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

