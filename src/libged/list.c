/*                         L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/list.c
 *
 * The l command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


void
_ged_do_list(struct ged *gedp, struct directory *dp, int verbose)
{
    int id;
    struct rt_db_internal intern;

    RT_CK_DBI(gedp->ged_wdbp->dbip);

    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	/* this is the _GLOBAL object */
	struct bu_attribute_value_set avs;
	struct bu_attribute_value_pair *avp;

	bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
	bu_vls_strcat(gedp->ged_result_str, ": global attributes object\n");
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for %s\n", dp->d_namep);
	    return;
	}
/* !!! left off here*/
	for (BU_AVS_FOR(avp, &avs)) {
	    if (BU_STR_EQUAL(avp->name, "units")) {
		double conv;
		const char *str;

		conv = atof(avp->value);
		bu_vls_strcat(gedp->ged_result_str, "\tunits: ");
		if ((str=bu_units_string(conv)) == NULL) {
		    bu_vls_strcat(gedp->ged_result_str, "Unrecognized units\n");
		} else {
		    bu_vls_strcat(gedp->ged_result_str, str);
		    bu_vls_putc(gedp->ged_result_str, '\n');
		}
	    } else {
		bu_vls_putc(gedp->ged_result_str, '\t');
		bu_vls_strcat(gedp->ged_result_str, avp->name);
		bu_vls_strcat(gedp->ged_result_str, ": ");
		bu_vls_strcat(gedp->ged_result_str, avp->value);
		bu_vls_putc(gedp->ged_result_str, '\n');
	    }
	}
    } else {

	if ((id = rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip,
				     (fastf_t *)NULL, &rt_uniresource)) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure\n", dp->d_namep);
	    return;
	}

	bu_vls_printf(gedp->ged_result_str, "%s:  ", dp->d_namep);

	if (!rt_functab[id].ft_describe ||
	    rt_functab[id].ft_describe(gedp->ged_result_str,
				       &intern,
				       verbose,
				       gedp->ged_wdbp->dbip->dbi_base2local,
				       &rt_uniresource,
				       gedp->ged_wdbp->dbip) < 0)
	    bu_vls_printf(gedp->ged_result_str, "%s: describe error\n", dp->d_namep);
	rt_db_free_internal(&intern);
    }
}


int
ged_list(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int arg;
    int id;
    int recurse = 0;
    char *listeval = "listeval";
    struct rt_db_internal intern;
    static const char *usage = "[-r] <objects>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], "-r")) {
	recurse = 1;

	/* skip past used args */
	--argc;
	++argv;
    }

    /* skip past used args */
    --argc;
    ++argv;

    for (arg = 0; arg < argc; arg++) {
	if (recurse) {
	    char *tmp_argv[3];

	    tmp_argv[0] = listeval;
	    tmp_argv[1] = (char *)argv[arg];
	    tmp_argv[2] = (char *)NULL;

	    ged_pathsum(gedp, 2, (const char **)tmp_argv);
	} else if (strchr(argv[arg], '/')) {
	    struct db_tree_state ts;
	    struct db_full_path path;

	    db_full_path_init(&path);
	    ts = gedp->ged_wdbp->wdb_initial_tree_state;     /* struct copy */
	    ts.ts_dbip = gedp->ged_wdbp->dbip;
	    ts.ts_resp = &rt_uniresource;
	    MAT_IDN(ts.ts_mat);

	    if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
		continue;

	    dp = DB_FULL_PATH_CUR_DIR(&path);

	    if ((id = rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, ts.ts_mat, &rt_uniresource)) < 0) {
		bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
		continue;
	    }

	    db_free_full_path(&path);

	    bu_vls_printf(gedp->ged_result_str, "%s:  ", argv[arg]);

	    if (!rt_functab[id].ft_describe || rt_functab[id].ft_describe(gedp->ged_result_str, &intern, 99, gedp->ged_wdbp->dbip->dbi_base2local, &rt_uniresource, gedp->ged_wdbp->dbip) < 0)
		bu_vls_printf(gedp->ged_result_str, "%s: describe error", dp->d_namep);

	    rt_db_free_internal(&intern);
	} else {
	    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[arg], LOOKUP_NOISY)) == RT_DIR_NULL)
		continue;

	    _ged_do_list(gedp, dp, 99);	/* very verbose */
	}
    }

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
