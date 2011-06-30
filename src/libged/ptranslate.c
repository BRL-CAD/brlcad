/*                         P T R A N S L A T E . C
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
/** @file libged/ptranslate.c
 *
 * The ptranslate command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


int
ged_ptranslate(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd_name = argv[0];
    int ret;
    int rflag;
    struct rt_db_internal intern;
    vect_t tvec;
    char *last;
    struct directory *dp;
    static const char *usage = "[-r] obj attribute tvec";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    if (argc < 4 || argc > 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] == '-' && argv[1][1] == 'r' && argv[1][2] == '\0') {
	    rflag = 1;
	    --argc;
	    ++argv;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	    return GED_ERROR;
	}
    } else
	rflag = 0;

    if (sscanf(argv[3], "%lf %lf %lf", &tvec[0], &tvec[1], &tvec[2]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad translation vector - %s", cmd_name, argv[3]);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", cmd_name, argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", cmd_name, argv[1]);
	return GED_ERROR;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	bu_vls_printf(gedp->ged_result_str, "%s: Object not eligible for translating.", cmd_name);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_TGC:
	    ret = _ged_translate_tgc(gedp, (struct rt_tgc_internal *)intern.idb_ptr, argv[2], tvec, rflag);
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    ret = _ged_translate_extrude(gedp, (struct rt_extrude_internal *)intern.idb_ptr, argv[2], tvec, rflag);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: Object not yet supported.", cmd_name);
	    rt_db_free_internal(&intern);

	    return GED_ERROR;
    }

    if (ret == GED_OK) {
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    } else if (ret == GED_ERROR) {
	rt_db_free_internal(&intern);
    }

    return ret;
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
