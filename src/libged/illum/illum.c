/*                         I L L U M . C
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
/** @file libged/illum.c
 *
 * The illum command.
 *
 */

#include "common.h"
#include <string.h>

#include "dm.h" // For labelvert - see if we really need the dm_set_dirty call there...

#include "ged.h"
#include "../ged_private.h"

/* Usage:  labelvert solid(s) */
int
ged_labelvert_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bv_vlblock*vbp;
    mat_t mat;
    fastf_t scale;
    static const char *usage = "object(s) - label vertices of wireframes of objects";

    if (!gedp || !gedp->dbip)
	return BRLCAD_ERROR;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, gedp->ged_gvp->gv_rotation);
    scale = gedp->ged_gvp->gv_size / 100;          /* divide by # chars/screen */

    for (i=1; i<argc; i++) {
	struct bv_scene_obj *s;
	struct directory *dp;
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	/* Find uses of this solid in the solid table */
	gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(s, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!s->s_u_data)
		    continue;
		struct ged_bv_data *bdata = (struct ged_bv_data *)s->s_u_data;
		if (db_full_path_search(&bdata->s_fullpath, dp)) {
		    rt_label_vlist_verts(vbp, &s->s_vlist, mat, scale, gedp->dbip->dbi_base2local);
		}
	    }

	    gdlp = next_gdlp;
	}
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, "_LABELVERT_", 0);

    bv_vlblock_free(vbp);
    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (dmp)
	dm_set_dirty(dmp, 1);
    return BRLCAD_OK;
}



static int
dl_set_illum(struct display_list *gdlp, const char *obj, int illum)
{
    int found = 0;
    struct bv_scene_obj *sp;

    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	size_t i;
	if (!sp->s_u_data)
	    continue;
	struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	for (i = 0; i < bdata->s_fullpath.fp_len; ++i) {
	    if (*obj == *DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep &&
		BU_STR_EQUAL(obj, DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep)) {
		found = 1;
		if (illum)
		    sp->s_iflag = UP;
		else
		    sp->s_iflag = DOWN;
	    }
	}
    }
    return found;
}

/*
 * Illuminate/highlight database object
 *
 * Usage:
 * illum [-n] obj
 *
 */
int
ged_illum_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int found = 0;
    int illum = 1;
    static const char *usage = "[-n] obj";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 3) {
	if (argv[1][0] == '-' && argv[1][1] == 'n')
	    illum = 0;
	else
	    goto bad;

	--argc;
	++argv;
    }

    if (argc != 2)
	goto bad;

    gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	found += dl_set_illum(gdlp, argv[1], illum);

	gdlp = next_gdlp;
    }


    if (!found) {
	bu_vls_printf(gedp->ged_result_str, "illum: %s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl illum_cmd_impl = {
    "illum",
    ged_illum_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd illum_cmd = { &illum_cmd_impl };

struct ged_cmd_impl labelvert_cmd_impl = {
    "labelvert",
    ged_labelvert_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd labelvert_cmd = { &labelvert_cmd_impl };


const struct ged_cmd *illum_cmds[] = { &illum_cmd, &labelvert_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  illum_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
