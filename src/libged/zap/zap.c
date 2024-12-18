/*                         Z A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/zap.c
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>


#include "../ged_private.h"

extern int ged_zap2_core(struct ged *gedp, int argc, const char *argv[]);

#define FIRST_SOLID(_sp)      ((_sp)->s_fullpath.fp_names[0])
#define FREE_BV_SCENE_OBJ(p, fp) { \
        BU_LIST_APPEND(fp, &((p)->l)); \
        BV_FREE_VLIST(vlfree, &((p)->s_vlist)); }

static void
dl_zap(struct ged *gedp)
{
    struct bu_list *hdlp = gedp->ged_gdp->gd_headDisplay;
    struct db_i *dbip = gedp->dbip;
    struct bv_scene_obj *sp = NULL;
    struct display_list *gdlp = NULL;
    struct bu_ptbl dls = BU_PTBL_INIT_ZERO;
    struct directory *dp = RT_DIR_NULL;
    size_t i = 0;
    struct bv_scene_obj *free_scene_obj = bv_set_fsos(&gedp->ged_views);
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    while (BU_LIST_WHILE(gdlp, display_list, hdlp)) {

	if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj))
	    ged_destroy_vlist_cb(gedp, BU_LIST_FIRST(bv_scene_obj, &gdlp->dl_head_scene_obj)->s_dlist,
				 BU_LIST_LAST(bv_scene_obj, &gdlp->dl_head_scene_obj)->s_dlist -
				 BU_LIST_FIRST(bv_scene_obj, &gdlp->dl_head_scene_obj)->s_dlist + 1);

	while (BU_LIST_WHILE(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;
	    dp = FIRST_SOLID(bdata);
	    RT_CK_DIR(dp);
	    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		if (db_dirdelete(dbip, dp) < 0) {
		    bu_log("ged_zap: db_dirdelete failed\n");
		}
	    }

	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_BV_SCENE_OBJ(sp, &free_scene_obj->l);
	}

	BU_LIST_DEQUEUE(&gdlp->l);
	/* queue up for free */
	bu_ptbl_ins_unique(&dls, (long *)gdlp);
	gdlp = NULL;
    }

    /* Free all display lists */
    for(i = 0; i < BU_PTBL_LEN(&dls); i++) {
	gdlp = (struct display_list *)BU_PTBL_GET(&dls, i);
	bu_vls_free(&gdlp->dl_path);
	BU_FREE(gdlp, struct display_list);
    }
    bu_ptbl_free(&dls);
}

/*
 * Erase all currently displayed geometry
 *
 * Usage:
 * zap
 *
 */
int
ged_zap_core(struct ged *gedp, int argc, const char *argv[])
{

    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
	return ged_zap2_core(gedp, argc, argv);

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    dl_zap(gedp);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl zap_cmd_impl = {"zap", ged_zap_core, GED_CMD_DEFAULT};
const struct ged_cmd zap_cmd = { &zap_cmd_impl };

struct ged_cmd_impl Z_cmd_impl = {"Z", ged_zap_core, GED_CMD_DEFAULT};
const struct ged_cmd Z_cmd = { &Z_cmd_impl };

const struct ged_cmd *zap_cmds[] = { &zap_cmd, &Z_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  zap_cmds, 2 };

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
