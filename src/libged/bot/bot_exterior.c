/*                    B O T _ E X T E R I O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/bot_exterior.c
 *
 * The bot exterior subcommand.
 *
 */

#include "common.h"

#include <string.h>

#include "rt/application.h"
#include "rt/ray_partition.h"
#include "rt/seg.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "vmath.h"

#include "../ged_private.h"



static int
exterior_miss(struct application *UNUSED(app))
{
    return 0;
}


static int
exterior_hit(struct application *app, struct partition *PartHeadp, struct seg *UNUSED(seg))
{
    struct partition *pp;

    /* FIXME: could be multiple coincident partitions at front and back */
    struct partition *first_pp = PartHeadp->pt_forw;
    struct partition *last_pp = NULL;
    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	last_pp = pp;
    }

    /* if our triangle is first or last hit, it's definitely outside */
    if (first_pp->pt_inhit->hit_surfno == app->a_user) {
	return 1;
    }
    if (last_pp->pt_outhit->hit_surfno == app->a_user) {
	return 1;
    }

    return 0;
}


/* determine whether a given face is exterior or not.  general idea is
 * we shoot a ray (TODO: should shoot a hemisphere) at the triangle
 * from outside the bounding volume.  if it's the first or last thing
 * hit, it's an exterior triangle.
 */
static int
exterior_face(struct application *app, struct rt_bot_internal *bot, int face) {
    if (!app || !bot || face < 0)
	return 0;

    int i, j, k;
    i = bot->faces[face * ELEMENTS_PER_POINT + X];
    j = bot->faces[face * ELEMENTS_PER_POINT + Y];
    k = bot->faces[face * ELEMENTS_PER_POINT + Z];

    /* sanity */
    if (i < 0 || j < 0 || k < 0 ||
	(size_t)i >= bot->num_vertices || (size_t)j >= bot->num_vertices || (size_t)k >= bot->num_vertices) {
	return 0;
    }

    vect_t p1, p2, p3;
    VMOVE(p1, &bot->vertices[i * ELEMENTS_PER_POINT]);
    VMOVE(p2, &bot->vertices[j * ELEMENTS_PER_POINT]);
    VMOVE(p3, &bot->vertices[k * ELEMENTS_PER_POINT]);

    /* face plane */
    plane_t norm;
    struct bn_tol tol = BN_TOL_INIT_ZERO;
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;
    bg_make_plane_3pnts(norm, p1, p2, p3, &tol);

    /* ray direction */
    VINVDIR(app->a_ray.r_dir, norm);
    VUNITIZE(app->a_ray.r_dir);

    /* face & ray center point */
    VADD3(app->a_ray.r_pt, p1, p2, p3);
    VSCALE(app->a_ray.r_pt, app->a_ray.r_pt, 1.0/3.0);
    /* calculate min/max to exit bounding volume */
    if (!rt_in_rpp(&app->a_ray, app->a_ray.r_dir, app->a_rt_i->mdl_min, app->a_ray.r_dir)) {
	static size_t msgs = 0;
	if (msgs < 100) {
	    bu_log("INTERNAL ERROR: Missed the model??\n");
	    msgs++;
	    if (msgs == 100)
		bu_log("Additional missed model reporting will be suppressed\n");
	}
	return 0;
    }
    VJOIN1(app->a_ray.r_pt, app->a_ray.r_pt, app->a_ray.r_max, app->a_ray.r_dir);

    /* determine visibility */
    app->a_user = face;
    rt_shootray(app);

    return 0;
}


static size_t
bot_exterior(struct application *app, struct rt_bot_internal *bot)
{
    if (!app || !bot)
	return 0;
    RT_BOT_CK_MAGIC(bot);

    size_t i;
    size_t num_exterior = 0;
    int *faces;
    faces = (int *)bu_calloc(bot->num_faces, sizeof(int), "rt_bot_exterior: faces");

    /* walk the list of faces, and mark each one if it is exterior */
    for (i = 0; i < bot->num_faces; i++) {
	if (exterior_face(app, bot, i)) {
	    faces[i] = 1;
	    num_exterior++;
	}
    }

    /* sanity */
    if (num_exterior == 0) {
	bu_log("No interior faces??  Aborting.\n");
	return 0;
    }

    /* rebuild list of faces, copying over ones marked exterior */
    int j = 0;
    int *newfaces = (int *)bu_calloc(num_exterior, 3*sizeof(int), "new bot faces");
    for (i = 0; i < bot->num_faces; i++) {
	if (faces[i]) {
	    VMOVE(&newfaces[j*3], &bot->faces[i*3]);
	    j++;
	}
    }

    bu_free((char *)faces, "rt_bot_exterior: faces");

    if (num_exterior == bot->num_faces)
	return 0;

    int removed = bot->num_faces - num_exterior;
    bot->num_faces = num_exterior;
    bu_free(bot->faces, "release bot faces");
    bot->faces = newfaces;

    return removed;
}


int
ged_bot_exterior(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    size_t fcount = 0;
    size_t vcount = 0;
    static const char *usage = "old_bot new_bot";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_DB_LOOKUP(gedp, old_dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR & BRLCAD_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern,  old_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	bu_log("%s: %s is a PLATE MODE BoT\n"
	       "Calculating exterior faces currently unsupported for PLATE MODE\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    /* prep geometry */
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rt_new_rti(gedp->ged_wdbp->dbip);
    ap.a_hit = exterior_hit;
    ap.a_miss = exterior_miss;
    ap.a_uptr = (void *)bot;
    bu_log("Initializing object context\n");
    if (rt_gettree(ap.a_rt_i, argv[1])) {
	bu_log("%s: unable to load %s\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    rt_prep(ap.a_rt_i);

    /* figure out exteriors via ray tracing */
    bu_log("Calculating exterior faces\n");
    fcount = bot_exterior(&ap, bot);
    vcount = rt_bot_condense(bot);

    /* release our raytrace context */
    rt_free_rti(ap.a_rt_i);

    bu_vls_printf(gedp->ged_result_str, "%s: %zu interior vertices eliminated\n", argv[0], vcount);
    bu_vls_printf(gedp->ged_result_str, "%s: %zu interior faces eliminated\n", argv[0], fcount);

    /* FIXME: if the BoT is not SOLID, create as PLATE instead */

    GED_DB_DIRADD(gedp, new_dp, argv[2], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, new_dp, &intern, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
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
