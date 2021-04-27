/*                         D R A W 2 . C
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
/** @file libged/draw2.c
 *
 * Testing command for experimenting with drawing routines.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/getopt.h"

#include "../ged_private.h"

extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    const struct bn_tol *tol = &gedp->ged_wdbp->wdb_tol;
    const struct bg_tess_tol *ttol = &gedp->ged_wdbp->wdb_ttol;
    static const char *usage = "objname";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Look up object.  NOTE: for testing simplicity this is just an object lookup
     * right now, but it needs to walk the tree and do the full-path instance creation.
     * Note also that view objects aren't what we want to report with a "who" return,
     * so we'll need to track the list of drawn paths independently. */
    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_NOISY);
    if (dp == RT_DIR_NULL)
	return GED_ERROR;
    struct rt_db_internal dbintern;
    struct rt_db_internal *ip = &dbintern;
    mat_t idn;
    MAT_IDN(idn);
    int ret = rt_db_get_internal(ip, dp, dbip, idn, &rt_uniresource);
    if (ret < 0 || !ip->idb_meth->ft_plot)
	return GED_ERROR;

    // Have database object, make scene object
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    BU_LIST_INIT(&(s->s_vlist));
    BU_PTBL_INIT(&s->children);
    s->s_v = gedp->ged_gvp;
    s->s_type_flags |= BVIEW_DBOBJ_BASED;
    s->s_i_data = (void *)dp;
    VSET(s->s_color, 255, 0, 0);

    // Add object to scene
    bu_ptbl_ins(s->s_v->gv_scene_objs, (long *)s);

    // Get wireframe
    struct rt_view_info info;
    info.bot_threshold = gedp->ged_gvp->gv_bot_threshold;
    ret = ip->idb_meth->ft_plot(&s->s_vlist, ip, ttol, tol, &info);
    if (ret < 0)
	return GED_ERROR;

    // Draw labels
    if (ip->idb_meth->ft_labels)
	ret = ip->idb_meth->ft_labels(&s->children, ip, s->s_v);

    // Increment changed flag
    s->s_changed++;

    return (ret < 0) ? GED_ERROR : GED_OK;
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
