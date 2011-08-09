/*                 S O L I D S _ O N _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/solids_on_ray.c
 *
 * Routines to implement the click-to-pick-an-edit-solid feature.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

#include "./mged.h"
#include "./mged_dm.h"


#define ORDER_BY_NAME 0
#define ORDER_BY_DISTANCE 1

#define made_it() printf("Made it to %s%d\n", \
					__FILE__, __LINE__); \
				fflush(stdout)

/*
 * S O L _ N A M E _ D I S T
 *
 * Little pair for storing the name and distance of a solid
 */
struct sol_name_dist
{
    uint32_t magic;
    char *name;
    fastf_t dist;
};
#define SOL_NAME_DIST_MAGIC 0x736c6e64

#ifdef OLD_RPT
/*
 * S O L _ C O M P _ N A M E
 *
 * The function to order solids alphabetically by name
 */
static int
sol_comp_name(void *v1, void *v2)
{
    struct sol_name_dist *s1 = v1;
    struct sol_name_dist *s2 = v2;

    BU_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    BU_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    return bu_strcmp(s1->name, s2->name);
}


/*
 * S O L _ C O M P _ D I S T
 *
 * The function to order solids by distance along the ray
 */
static int
sol_comp_dist(void *v1, void *v2)
{
    struct sol_name_dist *s1 = v1;
    struct sol_name_dist *s2 = v2;

    BU_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    BU_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    if (s1->dist > s2->dist)
	return 1;
    else if (s1->dist == s2->dist)
	return 0;
    else /* (s1->dist < s2->dist) */
	return -1;
}


/*
 * M K _ S O L I D
 */
static struct sol_name_dist *
mk_solid(char *name, fastf_t dist)
{
    struct sol_name_dist *sp;

    sp = (struct sol_name_dist *)
	bu_malloc(sizeof(struct sol_name_dist), "solid");
    sp->magic = SOL_NAME_DIST_MAGIC;
    sp->name = (char *)
	bu_malloc(strlen(name)+1, "solid name");
    bu_strlcpy(sp->name, name, strlen(name)+1);
    sp->dist = dist;
    return sp;
}


/*
 * F R E E _ S O L I D
 *
 * This function has two parameters: the solid to free and
 * an indication whether the name member of the solid should
 * also be freed.
 */
static void
free_solid(struct sol_name_dist *sol, int free_name)
{
    BU_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    if (free_name)
	bu_free((genptr_t) sol->name, "solid name");
    bu_free((genptr_t) sol, "solid");
}


/*
 * P R I N T _ S O L I D
 */
static void
print_solid(void *vp)
{
    struct sol_name_dist *sol = vp;
    struct bu_vls tmp_vls;

    BU_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "solid %s at distance %g along ray\n",
		  sol->name, sol->dist);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
}
#endif /* OLD_RPT */


/*
 * N O _ O P
 *
 * Null event handler for use by rt_shootray().
 *
 * Does nothing.  Returns 1.
 */
static int
no_op(struct application *UNUSED(ap), struct partition *UNUSED(ph), struct region *UNUSED(r1), struct region *UNUSED(r2), struct partition *UNUSED(hp))
{
    return 1;
}


/*
 * R P T _ H I T S _ M I K E
 *
 * Each partition represents a segment, i.e. a single solid.
 * Boolean operations have not been performed.
 * The partition list is sorted by ascending inhit distance.
 * This code does not attempt to eliminate duplicate segs,
 * e.g. from piercing the torus twice.
 */
static int
rpt_hits_mike(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
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
 * R P T _ M I S S
 *
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
 * S K E W E R _ S O L I D S
 *
 * Fire a ray at some geometry and obtain a list of
 * the solids encountered, sorted by first intersection.
 *
 * The function has five parameters: the model and objects at which
 * to fire (in an argc/argv pair) the origination point and direction
 * for the ray, and a result-format specifier.  So long as it could
 * find the objects in the model, skewer_solids() returns a null-
 * terminated array of solid names.  Otherwise, it returns 0.  If
 * full_path is nonzero, then the entire path for each solid is
 * recorded.  Otherwise, only the basename is recorded.
 *
 * N.B. - It is the caller's responsibility to free the array
 * of solid names.
 */
char **skewer_solids (int argc, const char *argv[], fastf_t *ray_orig, fastf_t *ray_dir, int UNUSED(full_path))
{
    struct application ap;
    struct rt_i *rtip;
    struct bu_list sol_list;

    if (argc <= 0) {
	Tcl_AppendResult(INTERP, "skewer_solids argc<=0\n", (char *)NULL);
	return (char **) 0;
    }

    /* .inmem rt_gettrees .rt -i -u [who] */
    rtip = rt_new_rti(dbip);
    rtip->useair = 1;
    rtip->rti_dont_instance = 1;	/* full paths to solids, too. */
    if (rt_gettrees(rtip, argc, argv, 1) == -1) {
	Tcl_AppendResult(INTERP, "rt_gettrees() failed\n", (char *)NULL);
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
    ap.a_hit = rpt_hits_mike;
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
