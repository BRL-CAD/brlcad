/*                         B O T _ D E C I M A T E . C
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
/** @file bot_decimate.c
 *
 * The bot_decimate command.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rtgeom.h"
#include "ged.h"


int
ged_bot_decimate(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    fastf_t max_chord_error=-1.0;
    fastf_t max_normal_error=-1.0;
    fastf_t min_edge_length=-1.0;
    static const char *usage = "-c maximum_chord_error -n maximum_normal_error -e minimum_edge_length new_bot_name current_bot_name";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* invalid command name */
    if (argc < 1) {
	bu_vls_printf(&gedp->ged_result_str, "Error: command name not provided");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc < 5 || argc > 9) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* process args */
    bu_optind = 1;
    bu_opterr = 0;
    while ( (c=bu_getopt(argc, (char * const *)argv, "c:n:e:")) != EOF )  {
	switch (c) {
	    case 'c':
		max_chord_error = atof( bu_optarg );
		if ( max_chord_error < 0.0 ) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Maximum chord error cannot be less than zero");
		    return BRLCAD_ERROR;
		}
		break;
	    case 'n':
		max_normal_error = atof( bu_optarg );
		if ( max_normal_error < 0.0 ) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Maximum normal error cannot be less than zero");
		    return BRLCAD_ERROR;
		}
		break;
	    case 'e':
		min_edge_length = atof( bu_optarg );
		if ( min_edge_length < 0.0 ) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "minumum edge length cannot be less than zero");
		    return BRLCAD_ERROR;
		}
		break;
	    default:
	    {
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
	    }
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    /* make sure new solid does not already exist */
    if ( (dp=db_lookup( gedp->ged_wdbp->dbip, argv[0], LOOKUP_QUIET ) ) != DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str, "%s already exists!!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* make sure current solid does exist */
    if ( (dp=db_lookup( gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET ) ) == DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str, "%s does not exist\n", argv[1]);
	return BRLCAD_ERROR;
    }

    /* import the current solid */
    RT_INIT_DB_INTERNAL( &intern );
    if ( rt_db_get_internal( &intern, dp, gedp->ged_wdbp->dbip, NULL, gedp->ged_wdbp->wdb_resp ) < 0 ) {
	bu_vls_printf(&gedp->ged_result_str, "Failed to get internal form of %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    /* make sure this is a BOT solid */
    if ( intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	 intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT ) {
	bu_vls_printf(&gedp->ged_result_str, "%s is not a BOT solid\n", argv[1]);
	rt_db_free_internal( &intern, gedp->ged_wdbp->wdb_resp );
	return BRLCAD_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;

    RT_BOT_CK_MAGIC( bot );

    /* convert maximum error and edge length to mm */
    max_chord_error = max_chord_error * gedp->ged_wdbp->dbip->dbi_local2base;
    min_edge_length = min_edge_length * gedp->ged_wdbp->dbip->dbi_local2base;

    /* do the decimation */
    if ( rt_bot_decimate( bot, max_chord_error, max_normal_error, min_edge_length) < 0 ) {
	bu_vls_printf(&gedp->ged_result_str, "Decimation Error\n");
	rt_db_free_internal( &intern, gedp->ged_wdbp->wdb_resp );
	return BRLCAD_ERROR;
    }

    /* save the result to the database */
    if ( wdb_put_internal( gedp->ged_wdbp, argv[0], &intern, 1.0 ) < 0 ) {
	bu_vls_printf(&gedp->ged_result_str,
		      "Failed to write decimated BOT back to database\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
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
