/*                           L O G . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file log.c
 *
 * The log command.
 *
 */

#include "ged.h"


/**
 * Get the output from bu_log and append it to clientdata vls.
 */
static int
ged_logHook(genptr_t clientdata,
	    genptr_t str)
{
    register struct bu_vls *vp = (struct bu_vls *)clientdata;
    register int len;

    BU_CK_VLS(vp);
    len = bu_vls_strlen(vp);
    bu_vls_strcat(vp, str);
    len = bu_vls_strlen(vp) - len;

    return len;
}


int
ged_log(struct rt_wdb *wdbp, int argc, const char *argv[])
{
    static char *usage = "get|start|stop";

    GED_CHECK_DATABASE_OPEN(wdbp, GED_ERROR);
    GED_CHECK_READ_ONLY(wdbp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&wdbp->wdb_result_str, 0);
    wdbp->wdb_result = GED_RESULT_NULL;
    wdbp->wdb_result_flags = 0;

    if (argc != 2) {
	bu_vls_printf(&wdbp->wdb_result_str,"Usage: %s %s", argv[0], usage);

	/* must be wanting help */
	if (argc == 1) {
	    wdbp->wdb_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	    return GED_OK;
	}

	return GED_ERROR;
    }

    if (argv[1][0] == 'g' && !strcmp(argv[1], "get")) {
	bu_vls_vlscatzap(&wdbp->wdb_result_str, &wdbp->wdb_log);
	return GED_OK;
    }

    if (argv[1][0] == 's' && !strcmp(argv[1], "start")) {
	bu_log_add_hook(ged_logHook, (genptr_t)&wdbp->wdb_log);
	return GED_OK;
    }

    if (argv[1][0] == 's' && !strcmp(argv[1], "stop")) {
	bu_log_delete_hook(ged_logHook, (genptr_t)&wdbp->wdb_log);
	return GED_OK;
    }

    bu_log("Usage: %s ", argv[0], usage);
    return GED_ERROR;
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
