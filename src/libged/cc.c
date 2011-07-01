/*                         C C . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file libged/cc.c
 *
 * The cc (create constraint) command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "raytrace.h"
#include "wdb.h"
#include "cmd.h"

#include "./ged_private.h"


#define RT_TERMINAL_WIDTH 80
#define RT_COLUMNS ((RT_TERMINAL_WIDTH + V4_MAXNAME - 1) / V4_MAXNAME)


/*
 * List constraint objects in this database
 */
int
ged_cc(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "name constraint_expression";

    struct rt_db_internal internal;
    struct rt_constraint_internal *con_ip;
    struct directory *dp;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, GED_ERROR);
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, GED_ERROR);

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_CONSTRAINT;
    internal.idb_meth=&rt_functab[ID_CONSTRAINT];
    internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_constraint_internal), "rt_constraint_internal");
    con_ip = (struct rt_constraint_internal *)internal.idb_ptr;
    con_ip->magic = RT_CONSTRAINT_MAGIC;
    con_ip->id = 324;
    con_ip->type = 4;
    bu_vls_init(&(con_ip->expression));
    bu_vls_strcat(&(con_ip->expression), argv[2]);

    GED_DB_DIRADD(gedp, dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_NON_GEOM , (genptr_t)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Constraint saved");
    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
