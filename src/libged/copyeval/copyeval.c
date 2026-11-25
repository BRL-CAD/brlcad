/*                         C O P Y E V A L . C
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
/** @file libged/copyeval.c
 *
 * The copyeval command.
 *
 */

#include "common.h"

#include "bu/cmd.h"
#include "bu/path.h"
#include "bu/str.h"
#include "ged.h"


int
ged_copyeval_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "path_to_old_prim new_prim";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, BRLCAD_ERROR);

    int component_count = 0;
    char **components = bu_path_to_argv(argv[1], &component_count);
    if (!components) {
	bu_vls_printf(gedp->ged_result_str, "Empty source path");
	return BRLCAD_ERROR;
    }
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    const int found = db_argv_to_path(&path, gedp->dbip, component_count, (const char *const *)components);
    bu_argv_free(component_count, components);

    struct directory *source = DB_FULL_PATH_CUR_DIR(&path);
    mat_t transform;
    const int resolved = found == 0 && source && db_path_to_mat(gedp->dbip, &path, transform, 0);
    db_free_full_path(&path);
    if (!resolved) {
	bu_vls_printf(gedp->ged_result_str, "Unable to resolve source path %s", argv[1]);
	return BRLCAD_ERROR;
    }
    if (component_count > 1 && (source->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "Final component on path must be a primitive");
	return BRLCAD_ERROR;
    }

    /* Apply the accumulated matrix during import. This avoids exporting
     * and importing an intermediate copy of a potentially large body. */
    struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
    GED_DB_GET_INTERN(gedp, &internal, source, transform, BRLCAD_ERROR);
    struct directory *target = db_diradd(gedp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0,
					 source->d_flags, (void *)&internal.idb_type);
    if (!target) {
	rt_db_free_internal(&internal);
	bu_vls_printf(gedp->ged_result_str, "Unable to add %s to the database directory", argv[2]);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(target, gedp->dbip, &internal) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_COPYEVAL_COMMANDS(X, XID) \
    X(copyeval, ged_copyeval_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_COPYEVAL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_copyeval", 1, GED_COPYEVAL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
