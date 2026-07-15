/*                         Z A P . C
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
/** @file libged/zap.c
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "../ged_private.h"
#include "zap.h"

extern int ged_zap2_core(struct ged *gedp, int argc, const char *argv[]);

static const struct bu_cmd_option zap_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct zap_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    {"V", "view", "view", "name", "Specify view to clear", BU_CMD_VALUE_STRING,
	offsetof(struct zap_args, view), NULL, NULL, "ged.view", NULL, 0, 0, NULL,
	BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_FLAG("S", "shared", struct zap_args, shared_only, "Clear shared view objects"),
    BU_CMD_FLAG("v", "view-objs", struct zap_args, clear_view_objs,
	"Clear view-local objects"),
    BU_CMD_FLAG("g", "solid-objs", struct zap_args, clear_solid_objs,
	"Clear database solid objects"),
    BU_CMD_FLAG(NULL, "all", struct zap_args, clear_all_views, "Clear all scene objects"),
    BU_CMD_OPTION_NULL
};

const struct bu_cmd_schema ged_Z_cmd_schema = {
    "Z", "Clear displayed geometry", zap_schema_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, {NULL}
};
const struct bu_cmd_schema ged_zap_cmd_schema = {
    "zap", "Clear displayed geometry", zap_schema_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, {NULL}
};

#define FIRST_SOLID(_sp)      ((_sp)->s_fullpath.fp_names[0])
#define FREE_BV_SCENE_OBJ(p, fp) { \
        BU_LIST_APPEND(fp, &((p)->l)); \
        BV_FREE_VLIST(vlfree, &((p)->s_vlist)); }

static void
dl_zap(struct ged *gedp)
{
    struct bu_list *hdlp = gedp->i->ged_gdp->gd_headDisplay;
    struct db_i *dbip = gedp->dbip;
    struct bv_scene_obj *sp = NULL;
    struct display_list *gdlp = NULL;
    struct bu_ptbl dls = BU_PTBL_INIT_ZERO;
    struct directory *dp = RT_DIR_NULL;
    size_t i = 0;
    struct bv_scene_obj *free_scene_obj = bv_set_fsos(&gedp->ged_views);
    struct bu_list *vlfree = &rt_vlfree;

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

    if (gedp->new_cmd_forms)
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


#include "../include/plugin.h"

#define GED_ZAP_COMMANDS(X, XID) \
    X(Z, ged_zap_core, GED_CMD_DEFAULT, &ged_Z_cmd_schema) \
    X(zap, ged_zap_core, GED_CMD_DEFAULT, &ged_zap_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ZAP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_zap", 1, GED_ZAP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
