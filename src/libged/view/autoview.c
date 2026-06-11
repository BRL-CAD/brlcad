/*                         A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/autoview.c
 *
 * The autoview command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/str.h"
#include "dm.h"
#include "../ged_private.h"

extern int ged_autoview2_core(struct ged *gedp, int argc, const char *argv[]);

/* Return 1 (and set *v) if the entire string parses as a number. */
static int
_autoview_arg_is_num(const char *s, double *v)
{
    char *endptr = NULL;
    double d;

    if (!s || *s == '\0')
	return 0;

    d = strtod(s, &endptr);
    if (endptr == s || *endptr != '\0')
	return 0;

    if (v)
	*v = d;
    return 1;
}

/*
 * Auto-adjust the view so that geometry is framed within the view.  By
 * default all displayed geometry is framed, but if a list of objects
 * (or full paths) is supplied the view is instead centered and sized to
 * frame only those objects.
 *
 * Usage:
 * autoview [-s scale] [object ...]
 *
 */
int
ged_autoview_core(struct ged *gedp, int argc, const char *argv[])
{
    if (gedp->new_cmd_forms)
       return ged_autoview2_core(gedp, argc, argv);

    vect_t min, max;

    /* less than or near zero uses default, 0.5 model scale == 2.0 view factor */
    fastf_t factor = -1.0;
    double scale = -1.0;
    int got_scale = 0;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Gather the object arguments, honoring an explicit -s/--scale option
     * (which may appear anywhere). */
    const char **objv = (const char **)bu_calloc(argc, sizeof(char *), "autoview objv");
    int objc = 0;
    for (int i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "-s") || BU_STR_EQUAL(argv[i], "--scale")) {
	    if (i + 1 >= argc || !_autoview_arg_is_num(argv[i + 1], &scale)) {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s [-s scale] [object ...]", argv[0]);
		bu_free(objv, "autoview objv");
		return BRLCAD_ERROR;
	    }
	    got_scale = 1;
	    i++;
	    continue;
	}
	objv[objc++] = argv[i];
    }

    /* Backward compatibility: a lone leading numeric argument historically
     * set the view scale.  Honor that only when -s/--scale was not supplied
     * and the token does not name an existing object, so an object with a
     * numeric name still wins. */
    const char **objs = objv;
    if (!got_scale && objc > 0 && _autoview_arg_is_num(objs[0], &scale) &&
	(!gedp->dbip || db_lookup(gedp->dbip, objs[0], LOOKUP_QUIET) == RT_DIR_NULL)) {
	got_scale = 1;
	objs++;
	objc--;
    }

    if (got_scale && scale > 0.0)
	factor = 1.0 / scale;

    if (objc > 0) {
	/* Frame only the named objects.  Bound them directly from the
	 * database (they need not be displayed). */
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, objc, objs, 0, min, max) != BRLCAD_OK) {
	    bu_free(objv, "autoview objv");
	    return BRLCAD_ERROR;
	}
    } else {
	/* Frame everything currently displayed. */
	int is_empty = dl_bounding_sph(gedp->i->ged_gdp->gd_headDisplay, &min, &max, 1);
	if (is_empty) {
	    /* Nothing is in view - frame a default region centered on
	     * the origin (equivalent to a radial extent of 1000). */
	    VSETALL(min, -1000.0);
	    VSETALL(max,  1000.0);
	}
    }

    bu_free(objv, "autoview objv");

    bv_autoview_bounds(gedp->ged_gvp, factor, min, max);

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
