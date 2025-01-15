/*                         S E T _ T R A N S P A R E N C Y . C
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
/** @file libged/set_transparency.c
 *
 * The set_transparency command.
 *
 */

#include "common.h"


#include "../ged_private.h"

void
dl_set_transparency(struct ged *gedp, struct directory **dpp, double transparency)
{
    struct bu_list *hdlp = gedp->ged_gdp->gd_headDisplay;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    size_t i;
    struct directory **tmp_dpp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    for (i = 0, tmp_dpp = dpp;
		 i < bdata->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
		 ++i, ++tmp_dpp) {
		if (bdata->s_fullpath.fp_names[i] != *tmp_dpp)
		    break;
	    }

	    if (*tmp_dpp != RT_DIR_NULL)
		continue;

	    /* found a match */
	    sp->s_os->transparency = transparency;

	}

	ged_create_vlist_display_list_cb(gedp, gdlp);

        gdlp = next_gdlp;
    }

}


/*
 * Set the transparency of the specified object
 *
 * Usage:
 * set_transparency obj tr
 *
 */
int
ged_set_transparency_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory **dpp;

    /* intentionally double for scan */
    double transparency;

    static const char *usage = "node tval";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &transparency) != 1) {
	bu_vls_printf(gedp->ged_result_str, "dgo_set_transparency: bad transparency - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if ((dpp = _ged_build_dpp(gedp, argv[1])) == NULL) {
	return BRLCAD_OK;
    }

    dl_set_transparency(gedp, dpp, transparency);

    if (dpp != (struct directory **)NULL)
	bu_free((void *)dpp, "ged_set_transparency_core: directory pointers");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl set_transparency_cmd_impl = {
    "set_transparency",
    ged_set_transparency_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd set_transparency_cmd = { &set_transparency_cmd_impl };
const struct ged_cmd *set_transparency_cmds[] = { &set_transparency_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  set_transparency_cmds, 1 };

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
