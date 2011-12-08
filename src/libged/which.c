/*                         W H I C H . C
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
/** @file libged/which.c
 *
 * The whichair and whichid commands.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_which(struct ged *gedp, int argc, const char *argv[])
{
    int i, j;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct _ged_id_to_names headIdName;
    struct _ged_id_to_names *itnp;
    struct _ged_id_names *inp;
    int isAir;
    int sflag;
    static const char *usageAir = "code(s)";
    static const char *usageIds = "region_id(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (BU_STR_EQUAL(argv[0], "whichair"))
	isAir = 1;
    else
	isAir = 0;

    /* must be wanting help */
    if (argc == 1) {
	if (isAir)
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usageAir);
	else
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usageIds);
	return GED_HELP;
    }

    if (BU_STR_EQUAL(argv[1], "-s")) {
	--argc;
	++argv;

	if (argc < 2) {
	    if (isAir)
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usageAir);
	    else
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usageIds);

	    return GED_ERROR;
	}

	sflag = 1;
    } else {
	sflag = 0;
    }

    BU_LIST_INIT(&headIdName.l);

    /* Build list of id_to_names */
    for (j=1; j<argc; j++) {
	int n;
	int start, end;
	int range;
	int k;

	n = sscanf(argv[j], "%d%*[:-]%d", &start, &end);
	switch (n) {
	    case 1:
		for (BU_LIST_FOR(itnp, _ged_id_to_names, &headIdName.l))
		    if (itnp->id == start)
			break;

		/* id not found */
		if (BU_LIST_IS_HEAD(itnp, &headIdName.l)) {
		    BU_GET(itnp, struct _ged_id_to_names);
		    itnp->id = start;
		    BU_LIST_INSERT(&headIdName.l, &itnp->l);
		    BU_LIST_INIT(&itnp->headName.l);
		}

		break;
	    case 2:
		if (start < end)
		    range = end - start + 1;
		else if (end < start) {
		    range = start - end + 1;
		    start = end;
		} else
		    range = 1;

		for (k = 0; k < range; ++k) {
		    int id = start + k;

		    for (BU_LIST_FOR(itnp, _ged_id_to_names, &headIdName.l))
			if (itnp->id == id)
			    break;

		    /* id not found */
		    if (BU_LIST_IS_HEAD(itnp, &headIdName.l)) {
			BU_GET(itnp, struct _ged_id_to_names);
			itnp->id = id;
			BU_LIST_INSERT(&headIdName.l, &itnp->l);
			BU_LIST_INIT(&itnp->headName.l);
		    }
		}

		break;
	}
    }

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_REGION))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		return GED_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    for (BU_LIST_FOR(itnp, _ged_id_to_names, &headIdName.l)) {
		if ((!isAir && comb->region_id == itnp->id) ||
		    (isAir && comb->aircode == itnp->id)) {
		    /* add region name to our name list for this region */
		    BU_GET(inp, struct _ged_id_names);
		    bu_vls_init(&inp->name);
		    bu_vls_strcpy(&inp->name, dp->d_namep);
		    BU_LIST_INSERT(&itnp->headName.l, &inp->l);
		    break;
		}
	    }

	    rt_db_free_internal(&intern);
	}
    }

    /* place data in interp and free memory */
    while (BU_LIST_WHILE(itnp, _ged_id_to_names, &headIdName.l)) {
	if (!sflag) {
	    bu_vls_printf(gedp->ged_result_str, "Region[s] with %s %d:\n",
			  isAir ? "air code" : "ident", itnp->id);
	}

	while (BU_LIST_WHILE(inp, _ged_id_names, &itnp->headName.l)) {
	    if (sflag)
		bu_vls_printf(gedp->ged_result_str, " %V", &inp->name);
	    else
		bu_vls_printf(gedp->ged_result_str, "   %V\n", &inp->name);

	    BU_LIST_DEQUEUE(&inp->l);
	    bu_vls_free(&inp->name);
	    bu_free((genptr_t)inp, "which: inp");
	}

	BU_LIST_DEQUEUE(&itnp->l);
	bu_free((genptr_t)itnp, "which: itnp");
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
