/*                        W M A T E R . C
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
/** @file libged/wmater.c
 *
 * The wmater command.
 *
 */

#include "ged.h"


int
ged_wmater(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status = GED_OK;
    FILE *fp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    static const char *usage = "filename comb1 [comb2 ...]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[1], "a")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Failed to open file - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    for (i = 2; i < argc; ++i) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Failed to find %s", argv[0], argv[i]);
	    status = GED_ERROR;
	    continue;
	}
	if ((dp->d_flags & RT_DIR_COMB) == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a combination", argv[0], dp->d_namep);
	    status = GED_ERROR;
	    continue;
	}
	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Unable to read %s from database", argv[0], argv[i]);
	    status = GED_ERROR;
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	fprintf(fp, "\"%s\"\t\"%s\"\t%d\t%d\t%d\t%d\t%d\n", argv[i],
		bu_vls_strlen(&comb->shader) > 0 ?
		bu_vls_addr(&comb->shader) : "-",
		comb->rgb[0], comb->rgb[1], comb->rgb[2],
		comb->rgb_valid, comb->inherit);
	rt_db_free_internal(&intern);
    }

    (void)fclose(fp);
    return status;
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
