/*                        S H A D E R . C
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
/** @file libged/shader.c
 *
 * The shader command.
 *
 */

#include "ged.h"


int
ged_shader_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combination shader_material [shader_argument(s)]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, GED_ERROR);
    GED_CHECK_COMB(gedp, dp, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (argc == 2) {
	/* Return the current shader string */
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&comb->shader));
	rt_db_free_internal(&intern);
    } else {
	if (gedp->dbip->dbi_read_only) {
	    bu_vls_printf(gedp->ged_result_str, "Sorry, this database is READ-ONLY");
	    rt_db_free_internal(&intern);

	    return GED_ERROR;
	}

	/* Replace with new shader string from command line */
	bu_vls_free(&comb->shader);

	/* Bunch up the rest of the args, space separated */
	bu_vls_from_argv(&comb->shader, argc-2, (const char **)argv+2);

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
	/* Internal representation has been freed by rt_db_put_internal */
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl shader_cmd_impl = {
    "shader",
    ged_shader_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd shader_cmd = { &shader_cmd_impl };
const struct ged_cmd *shader_cmds[] = { &shader_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  shader_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
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
