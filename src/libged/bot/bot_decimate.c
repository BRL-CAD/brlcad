/*                         B O T _ D E C I M A T E . C
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
/** @file libged/bot_decimate.c
 *
 * The bot_decimate command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/getopt.h"
#include "rt/geom.h"
#include "ged.h"


int
ged_bot_decimate_core(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    fastf_t max_chord_error = -1.0;
    fastf_t max_normal_error = -1.0;
    fastf_t min_edge_length = -1.0;
    fastf_t feature_size = -1.0;
    static const char *usage = "-f feature_size (to use the newer GCT decimator)"
			       "\nOR: -c maximum_chord_error -n maximum_normal_error -e minimum_edge_length new_bot_name current_bot_name";

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

    if (argc < 5 || argc > 9) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* process args */
    bu_optind = 1;
    bu_opterr = 0;

    while ((c = bu_getopt(argc, (char * const *)argv, "c:n:e:f:")) != -1) {
	switch (c) {
	    case 'c':
		max_chord_error = atof(bu_optarg);

		if (max_chord_error < 0.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Maximum chord error cannot be less than zero");
		    return GED_ERROR;
		}

		break;

	    case 'n':
		max_normal_error = atof(bu_optarg);

		if (max_normal_error < 0.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Maximum normal error cannot be less than zero");
		    return GED_ERROR;
		}

		break;

	    case 'e':
		min_edge_length = atof(bu_optarg);

		if (min_edge_length < 0.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "minimum edge length cannot be less than zero");
		    return GED_ERROR;
		}

		break;

	    case 'f':
		feature_size = atof(bu_optarg);

		if (feature_size < 0.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "minimum feature size cannot be less than zero");
		    return GED_ERROR;
		}

		break;

	    default: {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	    }
	}
    }

    if (feature_size >= 0.0 && (max_chord_error >= 0.0 || max_normal_error >= 0.0 ||  min_edge_length >= 0.0)) {
	bu_vls_printf(gedp->ged_result_str, "-f may not be used with -c, -n, or -e");
	return GED_ERROR;
    }

    argc -= bu_optind;
    argv += bu_optind;

    /* make sure new solid does not already exist */
    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, GED_ERROR);

    /* make sure current solid does exist */
    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_QUIET, GED_ERROR);

    /* import the current solid */
    RT_DB_INTERNAL_INIT(&intern);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, NULL, gedp->ged_wdbp->wdb_resp, GED_ERROR);

    /* make sure this is a BOT solid */
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT solid\n", argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;

    RT_BOT_CK_MAGIC(bot);

    /* convert maximum error, edge length, and feature size to mm */
    if (max_chord_error > 0.0) {
	max_chord_error = max_chord_error * gedp->dbip->dbi_local2base;
    }

    if (min_edge_length > 0.0) {
	min_edge_length = min_edge_length * gedp->dbip->dbi_local2base;
    }

    if (feature_size >= 0.0) {
	/* use the new GCT decimator */
	const size_t orig_num_faces = bot->num_faces;
	size_t edges_removed;
	feature_size *= gedp->dbip->dbi_local2base;
	edges_removed = rt_bot_decimate_gct(bot, feature_size);
	bu_log("original face count = %zu\n", orig_num_faces);
	bu_log("\tedges removed = %zu\n", edges_removed);
	bu_log("\tnew face count = %zu\n", bot->num_faces);
    } else {
	/* use the old decimator */
	if (rt_bot_decimate(bot, max_chord_error, max_normal_error, min_edge_length) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Decimation Error\n");
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }

    /* save the result to the database */
    /* XXX - should this be rt_db_put_internal() instead? */
    if (wdb_put_internal(gedp->ged_wdbp, argv[0], &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write decimated BOT back to database\n");
	return GED_ERROR;
    }

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
