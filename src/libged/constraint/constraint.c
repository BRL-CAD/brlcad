/*                    C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include "bu.h"
#include "cmd.h"
#include "ged.h"


HIDDEN int
constraint_set(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "set called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_get(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "get called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_show(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "show called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_rm(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "rm called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_eval(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "eval called\n");

    return BRLCAD_OK;
}


int
ged_constraint(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "{set|get|show|rm|eval} [constraint_name] [key [value] ...]";

    static struct bu_cmdtab pc_cmds[] = {
	{"set", constraint_set},
	{"get", constraint_get},
	{"show", constraint_show},
	{"rm", constraint_rm},
	{"eval", constraint_eval},
	{(const char *)NULL, BU_CMD_NULL}
    };

    int ret;
    int cmdret;

    /* intialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc < 2) {
	/* must be wanting help */
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* this is only valid for v5 databases */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return GED_ERROR;
    }

    ret = bu_cmd(pc_cmds, argc-1, argv+1, 0, gedp, &cmdret);
    if (ret != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	return GED_ERROR;
    }

    return 0;
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
