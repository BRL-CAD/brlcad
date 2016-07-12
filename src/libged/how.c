/*                         H O W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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

#include "./ged_private.h"


/*
 * Returns "how" an object is being displayed.
 *
 * Usage:
 * how [-b] object
 *
 */
int
ged_how(struct ged *gedp, int argc, const char *argv[])
{
    int good;
    struct directory **dpp;
    int both = 0;
    static const char *usage = "[-b] object";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    good = dl_how(gedp->ged_gdp->gd_headDisplay, gedp->ged_result_str, dpp, both);

    /* match NOT found */
    if (!good) bu_vls_printf(gedp->ged_result_str, "-1");

good_label:
    if (dpp != (struct directory **)NULL)
	bu_free((void *)dpp, "ged_how: directory pointers");

    return GED_OK;
}


struct directory **
_ged_build_dpp(struct ged *gedp,
	       const char *path) {
    struct directory *dp;
    struct directory **dpp;
    int i;
    char *begin;
    char *end;
    char *newstr;
    char *list;
    int ac;
    const char **av;
    const char **av_orig = NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /*
     * First, build an array of the object's path components.
     * We store the list in av_orig below.
     */
    newstr = bu_strdup(path);
    begin = newstr;
    while ((end = strchr(begin, '/')) != NULL) {
	*end = '\0';
	bu_vls_printf(&vls, "%s ", begin);
	begin = end + 1;
    }
    bu_vls_printf(&vls, "%s ", begin);
    free((void *)newstr);

    list = bu_vls_addr(&vls);

    if (bu_argv_from_tcl_list(list, &ac, &av_orig) != 0) {
	bu_vls_printf(gedp->ged_result_str, "-1");
	bu_vls_free(&vls);
	return (struct directory **)NULL;
    }

    /* skip first element if empty */
    av = av_orig;
    if (*av[0] == '\0') {
	--ac;
	++av;
    }

    /* ignore last element if empty */
    if (*av[ac-1] == '\0')
	--ac;

    /*
     * Next, we build an array of directory pointers that
     * correspond to the object's path.
     */
    dpp = (struct directory **)bu_calloc(ac+1, sizeof(struct directory *), "_ged_build_dpp: directory pointers");
    for (i = 0; i < ac; ++i) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, av[i], 0)) != RT_DIR_NULL)
	    dpp[i] = dp;
	else {
	    /* object is not currently being displayed */
	    bu_vls_printf(gedp->ged_result_str, "-1");

	    bu_free((void *)dpp, "_ged_build_dpp: directory pointers");
	    bu_free((char *)av_orig, "free av_orig");
	    bu_vls_free(&vls);
	    return (struct directory **)NULL;
	}
    }

    dpp[i] = RT_DIR_NULL;

    bu_free((char *)av_orig, "free av_orig");
    bu_vls_free(&vls);
    return dpp;
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
