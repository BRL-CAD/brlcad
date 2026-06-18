/*                        S H A D E R . C
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
/** @file libged/shader.c
 *
 * The shader command.
 *
 */

#include "ged.h"
#include "ged/event_txn.h"


int
ged_shader_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combination shader_material [shader_argument(s)]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (argc == 2) {
	/* Return the current shader string */
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&comb->shader));
	rt_db_free_internal(&intern);
    } else {
	struct bu_vls new_shader = BU_VLS_INIT_ZERO;
	int event_batch_opened = 0;

	if (gedp->dbip->dbi_read_only) {
	    bu_vls_printf(gedp->ged_result_str, "Sorry, this database is READ-ONLY");
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
	}

	/* Bunch up the rest of the args, space separated */
	bu_vls_from_argv(&new_shader, argc-2, (const char **)argv+2);

	if (BU_STR_EQUAL(bu_vls_cstr(&comb->shader),
		bu_vls_cstr(&new_shader))) {
	    bu_vls_free(&new_shader);
	    rt_db_free_internal(&intern);
	    return BRLCAD_OK;
	}

	/* Replace with new shader string from command line */
	bu_vls_trunc(&comb->shader, 0);
	bu_vls_vlscat(&comb->shader, &new_shader);
	bu_vls_free(&new_shader);

	event_batch_opened = (ged_event_batch_begin(gedp) > 0);
	if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	    if (event_batch_opened)
		ged_event_batch_end(gedp, NULL);
	    bu_vls_printf(gedp->ged_result_str, "Database write failure.");
	    return BRLCAD_ERROR;
	}
	/* Internal representation has been freed by rt_db_put_internal */
	(void)ged_event_notify_object_material_changed(gedp, dp->d_namep, NULL);
	if (event_batch_opened)
	    ged_event_batch_end(gedp, NULL);
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SHADER_COMMANDS(X, XID) \
    X(shader, ged_shader_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SHADER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_shader", 1, GED_SHADER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
