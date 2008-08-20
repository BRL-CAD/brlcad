/*                         B O T _ S M O O T H . C
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
/** @file bot_smooth.c
 *
 * The bot_smooth command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "rtgeom.h"
#include "ged_private.h"


int
ged_bot_smooth(struct ged *gedp, int argc, const char *argv[])
{
    char *new_bot_name, *old_bot_name;
    struct directory *dp_old, *dp_new;
    struct rt_bot_internal *old_bot;
    struct rt_db_internal intern;
    fastf_t tolerance_angle=180.0;
    int arg_index=1;
    int id;
    static const char *usage = "[-t ntol] new_bot old_bot";

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

    /* check that we are using a version 5 database */
    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
	bu_vls_printf(&gedp->ged_result_str, "This is an older database version.\nIt does not support BOT surface normals.\nUse \"dbupgrade\" to upgrade this database to the current version.\n");
	return BRLCAD_ERROR;
    }

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    while (*argv[arg_index] == '-') {
	/* this is an option */
	if ( !strcmp( argv[arg_index], "-t" ) ) {
	    arg_index++;
	    tolerance_angle = atof( argv[arg_index] );
	} else {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
	arg_index++;
    }

    if ( arg_index >= argc ) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    new_bot_name = (char *)argv[arg_index++];
    old_bot_name = (char *)argv[arg_index];

    if ( (dp_old=db_lookup( gedp->ged_wdbp->dbip, old_bot_name, LOOKUP_QUIET ) ) == DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str, "%s does not exist!!\n", old_bot_name);
	return BRLCAD_ERROR;
    }

    if ( strcmp( old_bot_name, new_bot_name ) ) {

	if ( (dp_new=db_lookup( gedp->ged_wdbp->dbip, new_bot_name, LOOKUP_QUIET ) ) != DIR_NULL ) {
	    bu_vls_printf(&gedp->ged_result_str, "%s already exists!!\n", new_bot_name);
	    return BRLCAD_ERROR;
	}
    } else {
	dp_new = dp_old;
    }

    if ( (id=rt_db_get_internal( &intern, dp_old, gedp->ged_wdbp->dbip, NULL, gedp->ged_wdbp->wdb_resp ) ) < 0 ) {
	bu_vls_printf(&gedp->ged_result_str, "Failed to get internal form of %s\n", old_bot_name);
	return BRLCAD_ERROR;
    }

    if ( id != ID_BOT ) {
	bu_vls_printf(&gedp->ged_result_str, "%s is not a BOT primitive\n", old_bot_name);
	rt_db_free_internal( &intern, gedp->ged_wdbp->wdb_resp );
	return BRLCAD_ERROR;
    }

    old_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC( old_bot );

    if ( rt_bot_smooth( old_bot, old_bot_name, gedp->ged_wdbp->dbip, tolerance_angle*M_PI/180.0 ) ) {
	bu_vls_printf(&gedp->ged_result_str, "Failed to smooth %s\n", old_bot_name);
	rt_db_free_internal( &intern, gedp->ged_wdbp->wdb_resp );
	return BRLCAD_ERROR;
    }

    if ( dp_new == DIR_NULL ) {
	if ( (dp_new=db_diradd( gedp->ged_wdbp->dbip, new_bot_name, -1L, 0, DIR_SOLID,
				(genptr_t)&intern.idb_type)) == DIR_NULL ) {
	    rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	    bu_vls_printf(&gedp->ged_result_str, "Cannot add %s to directory\n", new_bot_name);
	    return BRLCAD_ERROR;
	}
    }

    if ( rt_db_put_internal( dp_new, gedp->ged_wdbp->dbip, &intern, gedp->ged_wdbp->wdb_resp ) < 0 ) {
	rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting.\n");
	return BRLCAD_ERROR;
    }

    rt_db_free_internal( &intern, gedp->ged_wdbp->wdb_resp );

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
