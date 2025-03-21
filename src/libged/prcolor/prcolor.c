/*                         P R C O L O R . C
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
/** @file libged/prcolor.c
 *
 * The prcolor command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/db4.h"
#include "raytrace.h"

#include "../ged_private.h"


static void
pr_vls_col_item(struct bu_vls *str,
		 char *cp,
		 int *ccp,		/* column count pointer */
		 int *clp)		/* column length pointer */
{
    /* Output newline if last column printed. */
    if (*ccp >= _GED_COLUMNS || (*clp+_GED_V4_MAXNAME-1) >= _GED_TERMINAL_WIDTH) {
	/* line now full */
	bu_vls_putc(str, '\n');
	*ccp = 0;
    } else if (*ccp != 0) {
	/* Space over before starting new column */
	do {
	    bu_vls_putc(str, ' ');
	    ++*clp;
	}  while ((*clp % _GED_V4_MAXNAME) != 0);
    }
    /* Output string and save length for next tab. */
    *clp = 0;
    while (*cp != '\0') {
	bu_vls_putc(str, *cp);
	++cp;
	++*clp;
    }
    ++*ccp;
}


static void
pr_vls_col_eol(struct bu_vls *str,
		int *ccp,
		int *clp)
{
    if (*ccp != 0)		/* partial line */
	bu_vls_putc(str, '\n');
    *ccp = 0;
    *clp = 0;
}


static void
pr_mater(struct ged *gedp,
	     const struct mater *mp,
	     int *ccp,
	     int *clp)
{
    char buf[128];

    (void)sprintf(buf, "%5ld..%ld", mp->mt_low, mp->mt_high);
    pr_vls_col_item(gedp->ged_result_str, buf, ccp, clp);
    (void)sprintf(buf, "%3d, %3d, %3d", mp->mt_r, mp->mt_g, mp->mt_b);
    pr_vls_col_item(gedp->ged_result_str, buf, ccp, clp);
    pr_vls_col_eol(gedp->ged_result_str, ccp, clp);
}


int
ged_prcolor_core(struct ged *gedp, int argc, const char *argv[])
{
    const struct mater *mp;
    int col_count = 0;
    int col_len = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    if (rt_material_head() == MATER_NULL) {
	bu_vls_printf(gedp->ged_result_str, "none");
	return BRLCAD_OK;
    }

    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw)
	pr_mater(gedp, mp, &col_count, &col_len);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl prcolor_cmd_impl = {
    "prcolor",
    ged_prcolor_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd prcolor_cmd = { &prcolor_cmd_impl };
const struct ged_cmd *prcolor_cmds[] = { &prcolor_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  prcolor_cmds, 1 };

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
