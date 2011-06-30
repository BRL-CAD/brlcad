/*                         C O P Y . C
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
/** @file libged/copy.c
 *
 * The copy command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_copy(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *from_dp;
    struct bu_external external;
    static const char *usage = "from to";

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

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, from_dp, argv[1], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, GED_ERROR);

    if (db_get_external(&external, from_dp, gedp->ged_wdbp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	return GED_ERROR;
    }

    if (wdb_export_external(gedp->ged_wdbp, &external, argv[2],
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      argv[2]);
	return GED_ERROR;
    }

    bu_free_external(&external);

    return GED_OK;
}


int
ged_dbcopy(struct ged *from_gedp, struct ged *to_gedp, const char *from, const char *to, int fflag)
{
    struct directory *from_dp;
    struct bu_external external;

    GED_CHECK_DATABASE_OPEN(from_gedp, GED_ERROR);
    GED_CHECK_DATABASE_OPEN(to_gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(to_gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(from_gedp->ged_result_str, 0);
    bu_vls_trunc(to_gedp->ged_result_str, 0);

    GED_DB_LOOKUP(from_gedp, from_dp, from, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

    if (!fflag && db_lookup(to_gedp->ged_wdbp->dbip, to, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(from_gedp->ged_result_str, "%s already exists.", to);
	return GED_ERROR;
    }

    if (db_get_external(&external, from_dp, from_gedp->ged_wdbp->dbip)) {
	bu_vls_printf(from_gedp->ged_result_str, "Database read error, aborting\n");
	return GED_ERROR;
    }

    if (wdb_export_external(to_gedp->ged_wdbp, &external, to,
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(from_gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      to);
	return GED_ERROR;
    }

    bu_free_external(&external);

    /* Need to do something extra for _GLOBAL */
    if (db_version(to_gedp->ged_wdbp->dbip) > 4 && BU_STR_EQUAL(to, DB5_GLOBAL_OBJECT_NAME)) {
	struct directory *to_dp;
	struct bu_attribute_value_set avs;
	const char *val;

	GED_DB_LOOKUP(to_gedp, to_dp, to, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(to_gedp->ged_wdbp->dbip, &avs, to_dp)) {
	    bu_vls_printf(from_gedp->ged_result_str, "Cannot get attributes for object %s\n", to_dp->d_namep);
	    return GED_ERROR;
	}

	if ((val = bu_avs_get(&avs, "title")) != NULL)
	    to_gedp->ged_wdbp->dbip->dbi_title = strdup(val);

	if ((val = bu_avs_get(&avs, "units")) != NULL) {
	    double loc2mm;

	    if ((loc2mm = bu_mm_value(val)) > 0) {
		to_gedp->ged_wdbp->dbip->dbi_local2base = loc2mm;
		to_gedp->ged_wdbp->dbip->dbi_base2local = 1.0 / loc2mm;
	    }
	}

	if ((val = bu_avs_get(&avs, "regionid_colortable")) != NULL) {
	    rt_color_free();
	    db5_import_color_table((char *)val);
	}

	bu_avs_free(&avs);
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
