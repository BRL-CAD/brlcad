/*                        R E G D E F . C
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
/** @file libged/regdef.c
 *
 * The regdef command.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "ged.h"

int
ged_regdef_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "item air los mat";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get region defaults */
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "ident %d air %d los %d material %d",
		      wdbp->wdb_item_default,
		      wdbp->wdb_air_default,
		      wdbp->wdb_los_default,
		      wdbp->wdb_mat_default);
	return BRLCAD_OK;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    int *fields[] = {&wdbp->wdb_item_default, &wdbp->wdb_air_default,
	&wdbp->wdb_los_default, &wdbp->wdb_mat_default};
    int values[sizeof(fields) / sizeof(fields[0])];
    const int count = argc - 1;
    if ((size_t)count > sizeof(values) / sizeof(values[0])) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* A bad late argument must leave all the defaults as they were. */
    for (int i = 0; i < count; ++i) {
	const char *arg = argv[i + 1];
	char *end = NULL;
	errno = 0;
	const long value = strtol(arg, &end, 10);
	if (arg == end || errno == ERANGE || value < INT_MIN || value > INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
	values[i] = (int)value;
    }
    for (int i = 0; i < count; ++i)
	*fields[i] = values[i];
    if (count > 1 && wdbp->wdb_air_default)
	wdbp->wdb_item_default = 0;

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_REGDEF_COMMANDS(X, XID) \
    X(regdef, ged_regdef_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_REGDEF_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_regdef", 1, GED_REGDEF_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
