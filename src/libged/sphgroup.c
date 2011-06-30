/*                         G R O U P . C
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
/** @file libged/group.c
 *
 * The group command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "wdb.h"

#include "./ged_private.h"


int
ged_sphgroup(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp, *sphdp;
    point_t obj_min, obj_max;
    point_t rpp_min, rpp_max;
    point_t centerpt;
    int i;
    int inside_flag = 0;
    struct rt_db_internal sph_intern;
    struct rt_ell_internal *bsph;
    static const char *usage = "gname target_sphere.s";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((sphdp = db_lookup(gedp->ged_wdbp->dbip, argv[argc-1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Specified bounding sphere %s not found\n", argv[argc-1]);
	return GED_ERROR;
    } else {
	rt_db_get_internal(&sph_intern, sphdp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource);
	if ((sph_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ELL) && (sph_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SPH)) {
	    bu_vls_printf(gedp->ged_result_str, "Specified bounding object %s not a sphere\n", argv[argc-1]);
	    return GED_ERROR;
	}
	bsph = (struct rt_ell_internal *)sph_intern.idb_ptr;
    }

    /* get objects to add to group - at the moment, only gets regions*/
    for (i = 0; i < RT_DBNHASH; i++)
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_nref == 0 && !(dp->d_flags & RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) continue;
	    if (BU_STR_EQUAL(dp->d_namep, sphdp->d_namep)) continue;
	    if (!(dp->d_flags & RT_DIR_REGION)) continue;
	    inside_flag = 0;
	    if (_ged_get_obj_bounds(gedp, 1, (const char **)&(dp->d_namep), 0, obj_min, obj_max) != GED_ERROR) {
		VSETALL(rpp_min, MAX_FASTF);
		VSETALL(rpp_max, -MAX_FASTF);
		VMINMAX(rpp_min, rpp_max, (double *)obj_min);
		VMINMAX(rpp_min, rpp_max, (double *)obj_max);
		/*
		  VMOVE(testpts[0], rpp_min);
		  VSET(testpts[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
		  VSET(testpts[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
		  VSET(testpts[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
		  VSET(testpts[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
		  VSET(testpts[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
		  VMOVE(testpts[6], rpp_max);
		  VSET(testpts[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
		  for (j = 0; j < 8; j++) {
		  if (DIST_PT_PT(testpts[j], bsph->v) <= MAGNITUDE(bsph->a)) inside_flag = 1;
		  }*/
		VSET(centerpt, (rpp_min[0] + rpp_max[0])*0.5, (rpp_min[1] + rpp_max[1])*0.5, (rpp_min[2] + rpp_max[2])*0.5);
		if (DIST_PT_PT(centerpt, bsph->v) <= MAGNITUDE(bsph->a)) inside_flag = 1;
		if (inside_flag == 1) {
		    if (_ged_combadd(gedp, dp, (char *)argv[1], 0, WMOP_UNION, 0, 0) == RT_DIR_NULL) return GED_ERROR;
		    inside_flag = 0;
		}
	    }
	}
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
