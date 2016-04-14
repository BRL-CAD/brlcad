/*                         E X I S T S . C
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
/** @file libged/stat.c
 *
 * Report information about database objects.
 *
 * Modeled on the Unix "stat" command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/opt.h"

#include "./ged_private.h"

/* Based on the f_type logic from search, but we can't make this a common
 * function because we're mallocing strings here and there are a lot of
 * situations in the search logic where we don't need/want the string itself.
 * Still need a better/universal/low-cost way to get a descriptive string for
 * an object's type.
 */
HIDDEN char *
_ged_db_obj_type(struct db_i *dbip, struct directory *dp)
{
    int type;
    char *retstr = NULL;
    struct rt_db_internal intern;
    if (!dp || dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) return NULL;

    if (dp->d_flags & RT_DIR_COMB) {
	if (dp->d_flags & RT_DIR_REGION) {
	    retstr = bu_strdup("region");
	} else {
	    retstr = bu_strdup("comb");
	}
	return retstr;
    }

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return NULL;
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	rt_db_free_internal(&intern);
	return NULL;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, NULL);
	    switch (type) {
		case 4:
		    retstr = bu_strdup("arb4");
		    break;
		case 5:
		    retstr = bu_strdup("arb5");
		    break;
		case 6:
		    retstr = bu_strdup("arb6");
		    break;
		case 7:
		    retstr = bu_strdup("arb7");
		    break;
		case 8:
		    retstr = bu_strdup("arb8");
		    break;
		default:
		    retstr = NULL;
		    break;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_METABALL:
	    /* Because ft_label is only 8 characters, ft_label doesn't work in fnmatch for metaball*/
	    retstr = bu_strdup("metaball");
	    break;
	default:
	    retstr = bu_strdup(intern.idb_meth->ft_label);
	    break;
    }

    rt_db_free_internal(&intern);
    return retstr;
}

/**
 * Checks for the existence of a specified object.
 */
int
ged_stat(struct ged *gedp, int argc, const char **argv)
{
    char *type = NULL;
    /* The directory struct is the closest analog to a filesystem's
     * stat structure.  Don't currently have things like create/access/modify
     * time stamps recorded in that struct, or for that matter in the
     * database, but if/when we decide to do so follow the UNIX stat
     * command model to report/expose that info here... */
    struct directory *dp = RT_DIR_NULL;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 2) return GED_ERROR;

    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot stat %s: no such object.", argv[1]);
	return GED_ERROR;
    }

    type = _ged_db_obj_type(gedp->ged_wdbp->dbip, dp);

    bu_vls_printf(gedp->ged_result_str, "Object: %s  Type: %s\n", dp->d_namep, type);
    bu_vls_printf(gedp->ged_result_str, "Size(obj): %d Size(full): %d\n", dp->d_len, db5_get_full_size(gedp->ged_wdbp->dbip,dp), type);

    bu_free(type, "free type string");
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
