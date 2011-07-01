/*                         H I D E . C
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
/** @file libged/hide.c
 *
 * The hide command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_hide(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct db_i *dbip;
    struct bu_external ext;
    struct bu_external tmp;
    struct db5_raw_internal raw;
    int i;
    static const char *usage = "<objects>";

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

    dbip = gedp->ged_wdbp->dbip;

    if (db_version(dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Database was created with a previous release of BRL-CAD.\nSelect \"Tools->Upgrade Database...\" to enable support for this feature.");
	return GED_ERROR;
    }

    for (i=1; i<argc; i++) {
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	RT_CK_DIR(dp);

	BU_EXTERNAL_INIT(&ext);

	if (db_get_external(&ext, dp, dbip) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "db_get_external failed for %s\n", dp->d_namep);
	    continue;
	}

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "db5_get_raw_internal_ptr() failed for %s\n", dp->d_namep);
	    bu_free_external(&ext);
	    continue;
	}

	raw.h_name_hidden = (unsigned char)(0x1);

	BU_EXTERNAL_INIT(&tmp);
	db5_export_object3(&tmp, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			   dp->d_namep,
			   raw.h_name_hidden,
			   &raw.attributes,
			   &raw.body,
			   raw.major_type, raw.minor_type,
			   raw.a_zzz, raw.b_zzz);
	bu_free_external(&ext);

	if (db_put_external(&tmp, dp, dbip)) {
	    bu_vls_printf(gedp->ged_result_str, "db_put_external() failed for %s\n", dp->d_namep);
	    bu_free_external(&tmp);
	    continue;
	}
	bu_free_external(&tmp);
	dp->d_flags |= RT_DIR_HIDDEN;
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
