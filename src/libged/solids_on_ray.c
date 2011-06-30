/*                         S O L I D S _ O N _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/solids_on_ray.c
 *
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "solid.h"

#include "./ged_private.h"


/*
 * Null event handler for use by rt_shootray().
 *
 * Does nothing.  Returns 1.
 */
static int
no_op(struct application *ap, struct partition *ph, struct region *r1, struct region *r2, struct partition *hp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (ph) RT_CK_PARTITION(ph);
    if (r1) RT_CK_REGION(r1);
    if (r2) RT_CK_REGION(r2);
    if (hp) RT_CK_PARTITION(hp);

    return 1;
}


/*
 * Each partition represents a segment, i.e. a single solid.
 *
 * Boolean operations have not been performed.
 *
 * The partition list is sorted by ascending inhit distance.  This
 * code does not attempt to eliminate duplicate segs, e.g. from
 * piercing the torus twice.
 */
static int
rpt_hits(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct partition *pp;
    int len;
    char **list;
    int i;

    len = rt_partition_len(PartHeadp) + 2;
    list = (char **)bu_calloc(len, sizeof(char *), "hit list[]");

    i = 0;
    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	RT_CK_PT(pp);
	list[i++] = db_path_to_string(&(pp->pt_inseg->seg_stp->st_path));
    }
    list[i++] = NULL;
    if (i > len) bu_exit(EXIT_FAILURE, "rpt_hits_mike: array overflow\n");

    ap->a_uptr = (genptr_t)list;
    return len;
}


/*
 * Miss handler for use by rt_shootray().
 *
 * Stuffs the address of a null string in ap->a_uptr and returns 0.
 */

static int
rpt_miss(struct application *ap)
{
    ap->a_uptr = NULL;

    return 0;
}


/*
 * G E D _ S K E W E R _ S O L I D S
 *
 * Fire a ray at some geometry and obtain a list of the solids
 * encountered, sorted by first intersection.
 *
 * The function has five parameters: the model and objects at which to
 * fire (in an argc/argv pair) the origination point and direction for
 * the ray, and a result-format specifier.  So long as it could find
 * the objects in the model, skewer_solids() returns a null-
 * terminated array of solid names.  Otherwise, it returns 0.  If
 * full_path is nonzero, then the entire path for each solid is
 * recorded.  Otherwise, only the basename is recorded.
 *
 * N.B. - It is the caller's responsibility to free the array
 * of solid names.
 */
static char **
skewer_solids(struct ged *gedp, int argc, const char **argv, fastf_t *ray_orig, fastf_t *ray_dir, int UNUSED(full_path))
{
    struct application ap;
    struct rt_i *rtip;
    struct bu_list sol_list;

    if (argc <= 0) {
	bu_vls_printf(gedp->ged_result_str, "skewer_solids argc<=0\n");
	return (char **) 0;
    }

    /* .inmem rt_gettrees .rt -i -u [who] */
    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    rtip->useair = 1;
    rtip->rti_dont_instance = 1;	/* full paths to solids, too. */
    if (rt_gettrees(rtip, argc, argv, 1) == -1) {
	bu_vls_printf(gedp->ged_result_str, "rt_gettrees() failed\n");
	rt_clean(rtip);
	bu_free((genptr_t)rtip, "struct rt_i");
	return (char **) 0;
    }

    /* .rt prep 1 */
    rtip->rti_hasty_prep = 1;
    rt_prep(rtip);

    BU_LIST_INIT(&sol_list);

    /*
     * Initialize the application
     */
    RT_APPLICATION_INIT(&ap);
    ap.a_magic = RT_AP_MAGIC;
    ap.a_ray.magic = RT_RAY_MAGIC;
    ap.a_hit = rpt_hits;
    ap.a_miss = rpt_miss;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap = no_op;
    ap.a_onehit = 0;
    ap.a_user = 1;	/* Requests full paths to solids, not just basenames */
    ap.a_rt_i = rtip;
    ap.a_zero1 = ap.a_zero2 = 0;
    ap.a_purpose = "skewer_solids()";
    ap.a_no_booleans = 1;		/* full paths, no booleans */
    VMOVE(ap.a_ray.r_pt, ray_orig);
    VMOVE(ap.a_ray.r_dir, ray_dir);

    (void) rt_shootray(&ap);

    rt_clean(rtip);
    bu_free((genptr_t)rtip, "struct rt_i");

    return (char **) ap.a_uptr;
}


int
ged_solids_on_ray(struct ged *gedp, int argc, const char *argv[])
{
    char **solids_on_ray_cmd_vec = NULL;
    int solids_on_ray_cmd_vec_len = 0;

    size_t args;
    char **snames;
    int h = 0;
    int v = 0;
    int i;		/* Dummy loop index */
    double t;
    double t_in;
    point_t ray_orig;
    vect_t ray_dir;
    point_t extremum[2];
    vect_t unit_H, unit_V;
    static const char *usage = "[h v]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 1 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 3 &&
	(sscanf(argv[1], "%d", &h) != 1 ||
	 sscanf(argv[2], "%d", &v) != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((int)GED_VMIN > h || h > (int)GED_VMAX || (int)GED_VMIN > v || v > (int)GED_VMAX) {
	bu_vls_printf(gedp->ged_result_str, "Screen coordinates out of range\nMust be between +/-2048");
	return GED_ERROR;
    }

    VSET(ray_orig, -gedp->ged_gvp->gv_center[MDX],
	 -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
    /*
     * Compute bounding box of all objects displayed.
     * Borrowed from size_reset() in chgview.c
     */
    for (i = 0; i < 3; ++i) {
	extremum[0][i] = INFINITY;
	extremum[1][i] = -INFINITY;
    }

    VMOVEN(ray_dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(ray_dir, ray_dir, -1.0);
    for (i = 0; i < 3; ++i)
	if (NEAR_ZERO(ray_dir[i], 1e-10))
	    ray_dir[i] = 0.0;
    if ((ray_orig[X] >= extremum[0][X])
	&& (ray_orig[X] <= extremum[1][X])
	&& (ray_orig[Y] >= extremum[0][Y])
	&& (ray_orig[Y] <= extremum[1][Y])
	&& (ray_orig[Z] >= extremum[0][Z])
	&& (ray_orig[Z] <= extremum[1][Z]))
    {
	t_in = -INFINITY;
	for (i = 0; i < 6; ++i) {
	    if (ZERO(ray_dir[i%3]))
		continue;
	    t = (extremum[i/3][i%3] - ray_orig[i%3]) /
		ray_dir[i%3];
	    if ((t < 0) && (t > t_in))
		t_in = t;
	}
	VJOIN1(ray_orig, ray_orig, t_in, ray_dir);
    }

    VMOVEN(unit_H, gedp->ged_gvp->gv_model2view, 3);
    VMOVEN(unit_V, gedp->ged_gvp->gv_model2view + 4, 3);
    VJOIN1(ray_orig, ray_orig, h * gedp->ged_gvp->gv_scale * INV_GED_V, unit_H);
    VJOIN1(ray_orig, ray_orig, v * gedp->ged_gvp->gv_scale * INV_GED_V, unit_V);

    /* allocate space for display top-levels */
    args = 2 + ged_count_tops(gedp);
    solids_on_ray_cmd_vec = bu_calloc(1, sizeof(char *) * args, "alloca solids_on_ray_cmd_vec");

    /*
     * Build a list of all the top-level objects currently displayed
     */
    solids_on_ray_cmd_vec_len = ged_build_tops(gedp, &solids_on_ray_cmd_vec[0], &solids_on_ray_cmd_vec[args]);

    snames = skewer_solids(gedp, solids_on_ray_cmd_vec_len, (const char **)solids_on_ray_cmd_vec, ray_orig, ray_dir, 1);

    bu_free(solids_on_ray_cmd_vec, "free solids_on_ray_cmd_vec");
    solids_on_ray_cmd_vec = NULL;

    if (snames == 0) {
	bu_vls_printf(gedp->ged_result_str, "Error executing skewer_solids: ");
	return GED_ERROR;
    }

    for (i = 0; snames[i] != 0; ++i)
	bu_vls_printf(gedp->ged_result_str, " %s", snames[i]);

    bu_free((genptr_t) snames, "solid names");

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
