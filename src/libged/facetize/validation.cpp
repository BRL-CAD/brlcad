/*               V A L I D A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <cmath>

#include "bu/log.h"
#include "rt/calc.h"
#include "raytrace.h"

#include "./ged_facetize.h"
#include "./validation.h"

static int
facetize_discard_log(void *UNUSED(data), void *UNUSED(message))
{
    return 0;
}

static int
facetize_bound_internal(struct db_i *dbip, struct directory *dp,
	point_t bounds_min, point_t bounds_max)
{
    /* A failed bound is an expected validation outcome for unbounded or
     * otherwise unsampleable CSG.  rt_bound_internal logs every failure,
     * which floods large runs before facetize can report the aggregate. */
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    bu_log_hook_save_all(&saved_hooks);
    bu_log_hook_delete_all();
    bu_log_add_hook(facetize_discard_log, NULL);

    int ret = BRLCAD_ERROR;
    if (!BU_SETJUMP) {
	ret = rt_bound_internal(dbip, dp, bounds_min, bounds_max);
    } else {
	BU_UNSETJUMP;
	bu_log_hook_delete_all();
	bu_log_hook_restore_all(&saved_hooks);
	bu_hook_delete_all(&saved_hooks);
	return BRLCAD_ERROR;
    }
    BU_UNSETJUMP;

    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&saved_hooks);
    bu_hook_delete_all(&saved_hooks);
    return ret;
}

int
_ged_facetize_csg_bbox(struct db_i *dbip, const char *object_name,
	point_t bounds_min, point_t bounds_max)
{
    if (!dbip || !object_name || !bounds_min || !bounds_max)
	return BRLCAD_ERROR;

    struct directory *dp = db_lookup(dbip, object_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL ||
	facetize_bound_internal(dbip, dp, bounds_min, bounds_max) != 0)
	return BRLCAD_ERROR;

    vect_t dimensions;
    VSUB2(dimensions, bounds_max, bounds_min);
    if (dimensions[X] <= 0.0 || dimensions[Y] <= 0.0 ||
	dimensions[Z] <= 0.0)
	return BRLCAD_ERROR;

    for (int i = 0; i < 3; i++) {
	if (!std::isfinite(bounds_min[i]) || !std::isfinite(bounds_max[i]))
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

long
facetize_csg_metrics(struct db_i *dbip, const char *object_name,
	double *surface_area, double *volume)
{
    if (!dbip || !object_name || !object_name[0] || !surface_area || !volume)
	return -1L;

    *surface_area = -1.0;
    *volume = -1.0;

    point_t focus_min, focus_max;
    if (_ged_facetize_csg_bbox(dbip, object_name, focus_min, focus_max) !=
	    BRLCAD_OK) {
	/* Surface area and volume are not defined for an unbounded CSG result.
	 * A finite combination clipped by a halfspace still has valid bounds and
	 * proceeds; a standalone or otherwise unbounded halfspace stops here. */
	return -1L;
    }

    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return -1L;

    if (rt_gettree(rtip, object_name) != 0) {
	rt_i_destroy(rtip);
	return -1L;
    }
    rt_prep_parallel(rtip, 1);

    struct rt_crofton_params parameters = {};
    parameters.n_rays = 0;
    parameters.stability_mm = 0.05;
    parameters.time_ms = 2000.0;

    int crossings = rt_crofton_shoot(surface_area, volume, NULL, NULL, NULL,
	    NULL, NULL, rtip, &parameters, focus_min, focus_max);
    rt_i_destroy(rtip);
    return (crossings >= 0) ? (long)crossings : -1L;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
