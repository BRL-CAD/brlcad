/*                         R M A P . C
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
/** @file libged/rmap.c
 *
 * The rmap command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_rmap(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct _ged_id_to_names headIdName;
    struct _ged_id_to_names *itnp;
    struct _ged_id_names *inp;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s is not available prior to version 5 of the .g file format\n", argv[0]);
	return GED_ERROR;
    }

    BU_LIST_INIT(&headIdName.l);

    /* For all regions not hidden */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    int found = 0;

	    if (!(dp->d_flags & RT_DIR_REGION) ||
		(dp->d_flags & RT_DIR_HIDDEN))
		continue;

	    if (rt_db_get_internal(&intern,
				   dp,
				   gedp->ged_wdbp->dbip,
				   (fastf_t *)NULL,
				   &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting", argv[0]);
		return GED_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    for (BU_LIST_FOR(itnp, _ged_id_to_names, &headIdName.l)) {
		if ((comb->region_id == itnp->id) ||
		    (comb->aircode != 0 && -comb->aircode == itnp->id)) {
		    /* add region name to our name list for this region */
		    BU_GET(inp, struct _ged_id_names);
		    bu_vls_init(&inp->name);
		    bu_vls_strcpy(&inp->name, dp->d_namep);
		    BU_LIST_INSERT(&itnp->headName.l, &inp->l);
		    found = 1;
		    break;
		}
	    }

	    if (!found) {
		/* create new id_to_names node */
		BU_GET(itnp, struct _ged_id_to_names);
		if (0 < comb->region_id)
		    itnp->id = comb->region_id;
		else
		    itnp->id = -comb->aircode;
		BU_LIST_INSERT(&headIdName.l, &itnp->l);
		BU_LIST_INIT(&itnp->headName.l);

		/* add region name to our name list for this region */
		BU_GET(inp, struct _ged_id_names);
		bu_vls_init(&inp->name);
		bu_vls_strcpy(&inp->name, dp->d_namep);
		BU_LIST_INSERT(&itnp->headName.l, &inp->l);
	    }

	    rt_db_free_internal(&intern);
	}
    }

    /* place data in the result string */
    while (BU_LIST_WHILE(itnp, _ged_id_to_names, &headIdName.l)) {

	/* add this id to the list */
	bu_vls_printf(gedp->ged_result_str, "%d {", itnp->id);

	/* start sublist of names associated with this id */
	while (BU_LIST_WHILE(inp, _ged_id_names, &itnp->headName.l)) {
	    /* add the this name to this sublist */
	    bu_vls_printf(gedp->ged_result_str, " %s", bu_vls_addr(&inp->name));

	    BU_LIST_DEQUEUE(&inp->l);
	    bu_vls_free(&inp->name);
	    bu_free((genptr_t)inp, "ged_rmap: inp");
	}
	bu_vls_printf(gedp->ged_result_str, " } "); /* , itnp->id); */

	BU_LIST_DEQUEUE(&itnp->l);
	bu_free((genptr_t)itnp, "ged_rmap: itnp");
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
