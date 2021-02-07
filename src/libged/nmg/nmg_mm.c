/*                         N M G _ M M . C
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
/** @file libged/nmg_mm.c
 *
 * The mm subcommand.
 *
 */

#include "common.h"

#include <signal.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "../ged_private.h"

int
ged_nmg_mm_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct model *m;
    struct directory *dp;
    const char *name;
    static const char *usage = "name";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[1];

    GED_CHECK_EXISTS(gedp, name, LOOKUP_QUIET, GED_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    m = nmg_mm();

    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_NMG;
    internal.idb_meth = &OBJ[ID_NMG];
    internal.idb_ptr = (void *)m;

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    /* add model to database */
    GED_DB_DIRADD(gedp, dp, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    return GED_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
