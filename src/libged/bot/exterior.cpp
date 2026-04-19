/*                      E X T E R I O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/bot/exterior.cpp
 *
 * BoT "exterior" subcommand — identify and retain only exterior faces.
 *
 * Two methods are supported, selectable with --flood-fill:
 *
 *  Ray sampling (default)
 *    For each face, probe rays are fired from the centroid in multiple
 *    directions.  A face is classified exterior when at least one
 *    direction makes it the first or last surface hit along that ray.
 *
 *  Flood fill (--flood-fill, requires OpenVDB)
 *    The model is voxelised via rt_rtip_to_occupancy_grid, then a
 *    BFS flood-fill from the boundary finds all "water-reachable" voxels.
 *    A face is exterior when any of its six face-adjacent voxels is
 *    reachable.
 *
 * Also provides the legacy ged_bot_exterior() entry point, which
 * accepts  [-f] old_bot new_bot  and forwards to _bot_cmd_exterior.
 */

#include "common.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/application.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/ray_partition.h"
#include "rt/seg.h"
#include "raytrace.h"
#include "ged.h"

#include "./ged_bot.h"


/* Flood-fill classifier lives in bot_flood_exterior.cpp and is only
 * compiled when OpenVDB is available. */
#ifdef HAVE_OPENVDB_BOT_EXTERIOR
extern "C" int bot_flood_exterior_classify(struct rt_i *rtip,
					   struct rt_bot_internal *bot,
					   int *face_exterior,
					   double voxel_size);
#endif


/* ======================================================================
 * Ray-sampling exterior classifier
 * ====================================================================== */

/*
 * Offset the probe-ray origin outside the entry point by 10x BN_TOL_DIST.
 * This steps past near-surface numeric jitter while remaining negligible
 * relative to model scale.
 */
#define EXTERIOR_RAY_OFFSET_FACTOR 10.0


static int
exterior_miss(struct application *UNUSED(app))
{
    return 0;
}


/* Per-ray state tracked across all hit segments. */
struct exterior_probe {
    int face;
    int first_surf;
    int last_surf;
    int have_event;
    fastf_t last_dist;
};

/* Application user-data: bot pointer + probe state. */
struct exterior_ctx {
    struct rt_bot_internal *bot;
    struct exterior_probe probe;
};


static void
exterior_probe_hit(struct exterior_probe *probe, int surfno, fastf_t dist)
{
    /* Discard behind-origin noise from tolerance jitter. */
    if (dist < -BN_TOL_DIST)
	return;

    /* Collapse near-duplicate hits (grazing rays). */
    if (probe->have_event && NEAR_EQUAL(dist, probe->last_dist, BN_TOL_DIST))
	return;

    if (!probe->have_event)
	probe->first_surf = surfno;
    probe->last_surf  = surfno;
    probe->have_event = 1;
    probe->last_dist  = dist;
}


static int
exterior_hit(struct application *app, struct partition *PartHeadp,
	     struct seg *UNUSED(seg))
{
    struct exterior_ctx *ectx = (struct exterior_ctx *)app->a_uptr;
    for (struct partition *pp = PartHeadp->pt_forw;
	 pp != PartHeadp;
	 pp = pp->pt_forw) {
	exterior_probe_hit(&ectx->probe,
			   pp->pt_inhit->hit_surfno, pp->pt_inhit->hit_dist);
	exterior_probe_hit(&ectx->probe,
			   pp->pt_outhit->hit_surfno, pp->pt_outhit->hit_dist);
    }
    return 1;
}


/*
 * Fire one probe ray from face centroid fc in direction dir.
 *
 * Returns:
 *   1  — face is exterior (first or last hit along this ray)
 *  -1  — face is interior
 *   0  — inconclusive (ray misses model bounds or no hit)
 */
static int
exterior_face_probe(struct application *app, int face,
		    point_t fc, const fastf_t *dir)
{
    struct exterior_ctx *ectx = (struct exterior_ctx *)app->a_uptr;
    if (!app || !ectx)
	return 0;

    vect_t inv_dir;
    VMOVE(app->a_ray.r_pt,  fc);
    VMOVE(app->a_ray.r_dir, dir);
    VUNITIZE(app->a_ray.r_dir);
    VINVDIR(inv_dir, app->a_ray.r_dir);

    if (!rt_in_rpp(&app->a_ray, inv_dir,
		   app->a_rt_i->mdl_min, app->a_rt_i->mdl_max)) {
	static size_t suppressed = 0;
	if (suppressed < 100) {
	    bu_log("bot exterior: probe ray does not intersect model bounds\n");
	    if (++suppressed == 100)
		bu_log("bot exterior: further bound-miss messages suppressed\n");
	}
	return 0;
    }

    /* Step the origin slightly outside the entry point. */
    fastf_t off = app->a_ray.r_min - (EXTERIOR_RAY_OFFSET_FACTOR * BN_TOL_DIST);
    VJOIN1(app->a_ray.r_pt, app->a_ray.r_pt, off, app->a_ray.r_dir);

    ectx->probe.face       = face;
    ectx->probe.first_surf = -1;
    ectx->probe.last_surf  = -1;
    ectx->probe.have_event = 0;
    ectx->probe.last_dist  = 0.0;

    rt_shootray(app);

    if (!ectx->probe.have_event)
	return 0;

    return (ectx->probe.first_surf == face ||
	    ectx->probe.last_surf  == face) ? 1 : -1;
}


/*
 * Determine whether a single face is exterior via multi-direction voting.
 *
 * Six primary probe directions (rotated, non-axis-aligned) give broad
 * octant coverage.  When primary votes tie, eight extra directions break
 * the tie.  Simple majority wins.
 */
static int
exterior_face(struct application *app, struct rt_bot_internal *bot, int face)
{
    if (!app || !bot || face < 0)
	return 0;

    int v0 = bot->faces[face * ELEMENTS_PER_POINT + X];
    int v1 = bot->faces[face * ELEMENTS_PER_POINT + Y];
    int v2 = bot->faces[face * ELEMENTS_PER_POINT + Z];

    if (v0 < 0 || v1 < 0 || v2 < 0 ||
	(size_t)v0 >= bot->num_vertices ||
	(size_t)v1 >= bot->num_vertices ||
	(size_t)v2 >= bot->num_vertices)
	return 0;

    vect_t p1, p2, p3;
    VMOVE(p1, &bot->vertices[v0 * ELEMENTS_PER_POINT]);
    VMOVE(p2, &bot->vertices[v1 * ELEMENTS_PER_POINT]);
    VMOVE(p3, &bot->vertices[v2 * ELEMENTS_PER_POINT]);

    point_t fc;
    VADD3(fc, p1, p2, p3);
    VSCALE(fc, fc, 1.0 / 3.0);

    static const vect_t dirs[] = {
	{  1.0,  2.0,  3.0 }, { -1.0, -2.0, -3.0 },
	{  2.0, -3.0,  1.0 }, { -2.0,  3.0, -1.0 },
	{ -3.0, -1.0,  2.0 }, {  3.0,  1.0, -2.0 }
    };
    static const vect_t extra_dirs[] = {
	{  3.0,  1.0,  2.0 }, { -3.0, -1.0, -2.0 },
	{ -1.0,  3.0,  2.0 }, {  1.0, -3.0, -2.0 },
	{  2.0,  3.0, -1.0 }, { -2.0, -3.0,  1.0 },
	{ -2.0,  1.0,  3.0 }, {  2.0, -1.0, -3.0 }
    };

    int ext_votes = 0, int_votes = 0;

    for (size_t dir_idx = 0; dir_idx < sizeof(dirs) / sizeof(dirs[0]); dir_idx++) {
	int r = exterior_face_probe(app, face, fc, dirs[dir_idx]);
	if (r < 0) int_votes++;
	else if (r > 0) ext_votes++;
    }

    if (ext_votes == int_votes) {
	for (size_t dir_idx = 0; dir_idx < sizeof(extra_dirs) / sizeof(extra_dirs[0]); dir_idx++) {
	    int r = exterior_face_probe(app, face, fc, extra_dirs[dir_idx]);
	    if (r < 0) int_votes++;
	    else if (r > 0) ext_votes++;
	}
    }

    if (ext_votes == 0) return 0;
    if (int_votes == 0) return 1;
    return (ext_votes > int_votes) ? 1 : 0;
}


/*
 * Classify all faces via ray sampling.
 *
 * On success allocates *face_exterior (caller must bu_free) and returns
 * the number of exterior faces (>= 0).  Returns -1 on allocation failure.
 */
static int
bot_exterior_classify_ray(struct application *app,
			  struct rt_bot_internal *bot,
			  int **face_exterior_out)
{
    if (!app || !bot || !face_exterior_out)
	return -1;

    int *mask = (int *)bu_calloc(bot->num_faces, sizeof(int), "face_exterior");
    int n_ext = 0;

    for (size_t i = 0; i < bot->num_faces; i++) {
	if (exterior_face(app, bot, (int)i)) {
	    mask[i] = 1;
	    n_ext++;
	}
    }

    *face_exterior_out = mask;
    return n_ext;
}


/* ======================================================================
 * Shared face-rebuild helper
 * ====================================================================== */

/*
 * Replace bot->faces in-place with only the faces whose mask entry is 1.
 * num_exterior must equal the count of set entries in face_exterior[].
 * Returns the number of faces removed.
 */
static size_t
bot_apply_exterior_mask(struct rt_bot_internal *bot,
			const int *face_exterior, size_t num_exterior)
{
    if (!bot || !bot->faces || !face_exterior || !bot->num_faces)
	return 0;

    size_t actual_exterior = 0;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (face_exterior[i])
	    actual_exterior++;
    }

    if (num_exterior != actual_exterior) {
	bu_log("bot exterior: warning - classifier reported %zu exterior faces, "
	       "mask contains %zu; using mask count\n",
	       num_exterior, actual_exterior);
    }

    size_t removed  = bot->num_faces - actual_exterior;
    int   *newfaces = (int *)bu_calloc(actual_exterior, 3 * sizeof(int),
				       "bot exterior new faces");
    size_t j = 0;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (face_exterior[i]) {
	    newfaces[j * 3 + 0] = bot->faces[i * 3 + 0];
	    newfaces[j * 3 + 1] = bot->faces[i * 3 + 1];
	    newfaces[j * 3 + 2] = bot->faces[i * 3 + 2];
	    j++;
	}
    }

    bu_free(bot->faces, "bot exterior old faces");
    bot->faces     = newfaces;
    bot->num_faces = actual_exterior;
    return removed;
}


/* ======================================================================
 * Usage helper
 * ====================================================================== */

static void
exterior_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d)
{
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str,
	"If output_name is omitted, the result is written to input_bot_exterior.\n"
	"Use -i/--in-place to overwrite the input BoT directly.\n");
}


/* ======================================================================
 * _bot_cmd_exterior — new-style subcommand handler
 * ====================================================================== */

extern "C" int
_bot_cmd_exterior(void *bs, int argc, const char **argv)
{
    const char *usage_string  = "bot exterior [options] <objname> [output_name]";
    const char *purpose_string =
	"reduce BoT to exterior (outward-facing / water-reachable) faces";

    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    /* Consume the subcommand name token. */
    argc--; argv++;

    int       print_help = 0;
    int       in_place   = 0;
    int       flood_fill = 0;
    fastf_t   voxel_size = 0.0;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h",       "help",       "",      NULL,            &print_help,
	   "Print help");
    BU_OPT(d[1], "i",   "in-place",       "",      NULL,            &in_place,
	   "Overwrite input BoT in-place (no output_name needed)");
    BU_OPT(d[2], "f", "flood-fill",       "",      NULL,            &flood_fill,
	   "Use voxel flood-fill / water-filling method (requires OpenVDB)");
    BU_OPT(d[3], "",  "voxel-size",   "size", &bu_opt_fastf_t,  &voxel_size,
	   "Voxel edge length for flood-fill (default: auto-computed)");
    BU_OPT_NULL(d[4]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	exterior_usage(gb->gedp->ged_result_str, "bot exterior", d);
	return GED_HELP;
    }

    if (in_place && argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "--in-place requires exactly one object name\n%s\n",
		      usage_string);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    const char *input_name = argv[0];

    /* Determine output name. */
    struct bu_vls output_name = BU_VLS_INIT_ZERO;
    if (in_place) {
	bu_vls_sprintf(&output_name, "%s", input_name);
    } else if (argc == 2) {
	bu_vls_sprintf(&output_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&output_name, "%s_exterior", input_name);
    }

    /* For non-in-place writes, verify the output name is free. */
    if (!in_place &&
	db_lookup(gb->gedp->dbip, bu_vls_cstr(&output_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Object %s already exists\n", bu_vls_cstr(&output_name));
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* Load the input BoT. */
    if (_bot_obj_setup(gb, input_name) & BRLCAD_ERROR) {
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    RT_BOT_CK_MAGIC(bot);

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "%s is a PLATE MODE BoT; exterior classification is unsupported\n",
		      input_name);
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* Build the raytrace context (used by both methods). */
    struct application  ap;
    struct exterior_ctx ectx;
    memset(&ectx, 0, sizeof(ectx));
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rt_new_rti(gb->gedp->dbip);
    ap.a_hit  = exterior_hit;
    ap.a_miss = exterior_miss;
    ectx.bot  = bot;
    ap.a_uptr = (void *)&ectx;

    if (rt_gettree(ap.a_rt_i, input_name)) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Unable to load %s for raytracing\n", input_name);
	rt_free_rti(ap.a_rt_i);
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }
    rt_prep(ap.a_rt_i);

    /* ----------------------------------------------------------------
     * Classify faces.
     * ---------------------------------------------------------------- */
    int *face_exterior = NULL;
    int  n_ext;

    if (flood_fill) {
#ifdef HAVE_OPENVDB_BOT_EXTERIOR
	bu_log("bot exterior: using flood-fill (water-filling) method\n");
	face_exterior = (int *)bu_calloc(bot->num_faces, sizeof(int), "face_exterior");
	n_ext = bot_flood_exterior_classify(ap.a_rt_i, bot, face_exterior,
					    (double)voxel_size);
	if (n_ext < 0) {
	    bu_free(face_exterior, "face_exterior");
	    rt_free_rti(ap.a_rt_i);
	    bu_vls_free(&output_name);
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Flood-fill exterior classification failed\n");
	    return BRLCAD_ERROR;
	}
#else
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Note: --flood-fill requires OpenVDB (not compiled in); "
		      "falling back to ray-sampling\n");
	bu_log("bot exterior: falling back to ray-sampling method\n");
	n_ext = bot_exterior_classify_ray(&ap, bot, &face_exterior);
#endif
    } else {
	bu_log("bot exterior: using ray-sampling method\n");
	n_ext = bot_exterior_classify_ray(&ap, bot, &face_exterior);
    }

    rt_free_rti(ap.a_rt_i);

    /* ----------------------------------------------------------------
     * Handle degenerate classification results.
     * ---------------------------------------------------------------- */
    if (!face_exterior || n_ext < 0) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Exterior face classification failed\n");
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    if (n_ext == 0) {
	bu_free(face_exterior, "face_exterior");
	bu_vls_printf(gb->gedp->ged_result_str,
		      "No exterior faces identified; "
		      "verify the mesh is a closed solid\n");
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* ----------------------------------------------------------------
     * Rebuild the face list and condense vertices.
     * ---------------------------------------------------------------- */
    size_t removed = 0;
    if ((size_t)n_ext < bot->num_faces) {
	removed = bot_apply_exterior_mask(bot, face_exterior, (size_t)n_ext);
    } else {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "All %zu faces are exterior; no interior faces to remove\n",
		      bot->num_faces);
    }
    bu_free(face_exterior, "face_exterior");

    size_t vcount = rt_bot_condense(bot);

    bu_vls_printf(gb->gedp->ged_result_str,
		  "Removed %zu interior face(s) and %zu unreferenced vertex/vertices\n",
		  removed, vcount);

    /* ----------------------------------------------------------------
     * Write the result.
     * ---------------------------------------------------------------- */
    int ret = BRLCAD_OK;

    if (in_place) {
	if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Failed to write %s\n", input_name);
	    ret = BRLCAD_ERROR;
	}
    } else {
	struct directory *outdp =
	    db_diradd(gb->gedp->dbip, bu_vls_cstr(&output_name),
		      RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID,
		      (void *)&gb->intern->idb_type);
	if (outdp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Cannot add %s to directory\n", bu_vls_cstr(&output_name));
	    ret = BRLCAD_ERROR;
	} else if (rt_db_put_internal(outdp, gb->gedp->dbip, gb->intern,
				      &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Failed to write %s\n", bu_vls_cstr(&output_name));
	    ret = BRLCAD_ERROR;
	}
    }

    bu_vls_free(&output_name);
    return ret;
}


/* ======================================================================
 * ged_bot_exterior — legacy top-level GED command
 *
 * Accepts:  bot_exterior [-f] old_bot new_bot
 *
 * Forwards to _bot_cmd_exterior via a minimal _ged_bot_info wrapper.
 * argv[0] ("bot_exterior") plays the role of the subcommand token that
 * _bot_cmd_exterior skips on entry.
 * ====================================================================== */

extern "C" int
ged_bot_exterior(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_bot_info gb;
    gb.gedp      = gedp;
    gb.intern    = NULL;
    gb.dp        = NULL;
    gb.vbp       = NULL;
    gb.color     = NULL;
    gb.verbosity = 0;
    gb.visualize = 0;
    gb.vlfree    = &rt_vlfree;
    gb.cmds      = NULL;
    gb.gopts     = NULL;

    return _bot_cmd_exterior(&gb, argc, argv);
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
