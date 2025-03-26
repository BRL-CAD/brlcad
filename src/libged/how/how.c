/*                         H O W . C
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
/** @file libged/how.c
 *
 * The how command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"
#include "dm.h"
#include "../ged_private.h"

static int
dl_how(struct bu_list *hdlp, struct bu_vls *vls, struct directory **dpp, int both)
{
    size_t i;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
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
	    if (sp->s_os->s_dmode == 4) {
		if (both)
		    bu_vls_printf(vls, "%d 1", _GED_HIDDEN_LINE);
		else
		    bu_vls_printf(vls, "%d", _GED_HIDDEN_LINE);
	    } else {
		if (both)
		    bu_vls_printf(vls, "%d %g", sp->s_os->s_dmode, sp->s_os->transparency);
		else
		    bu_vls_printf(vls, "%d", sp->s_os->s_dmode);
	    }

	    return 1;
	}

	gdlp = next_gdlp;
    }

    return 0;
}


/*
 * Returns "how" an object is being displayed.
 *
 * Usage:
 * how [-b] object
 *
 */
int
ged_how_core(struct ged *gedp, int argc, const char *argv[])
{
    int good;
    struct directory **dpp;
    int both = 0;
    static const char *usage = "[-b] object";

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

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 3 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'b') {
	both = 1;

	if ((dpp = _ged_build_dpp(gedp, argv[2])) == NULL)
	    goto good_label;
    } else {
	if ((dpp = _ged_build_dpp(gedp, argv[1])) == NULL)
	    goto good_label;
    }

    good = dl_how(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_result_str, dpp, both);

    /* match NOT found */
    if (!good) bu_vls_printf(gedp->ged_result_str, "-1");

good_label:
    if (dpp != (struct directory **)NULL)
	bu_free((void *)dpp, "ged_how_core: directory pointers");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl how_cmd_impl = {
    "how",
    ged_how_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd how_cmd = { &how_cmd_impl };
const struct ged_cmd *how_cmds[] = { &how_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  how_cmds, 1 };

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
