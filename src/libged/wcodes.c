/*                         W C O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file wcodes.c
 *
 * The wcodes command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


#define MAX_LEVELS 12
#define ABORTED		-99
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

static int regflag;
static int lastmemb;
static struct directory *path[MAX_LEVELS];

static void Do_printnode(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3);
static int printcodes(struct ged *gedp, FILE *fp, struct directory *dp, int pathpos);

int
ged_wcodes(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    FILE *fp;
    struct directory *dp;
    static const char *usage = "filename object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[1], "w")) == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Failed to open file - %s",
		      argv[0], argv[1]);
	return GED_ERROR;
    }

    regflag = lastmemb = 0;
    for (i = 2; i < argc; ++i) {
	if ( (dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL) {
	    status = printcodes(gedp, fp, dp, 0);

	    if (status == GED_ERROR) {
		(void)fclose(fp);
		return GED_ERROR;
	    }
	}
    }

    (void)fclose(fp);
    return GED_OK;
}

static void
Do_printnode(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3)
{
    FILE *fp;
    int *pathpos;
    struct directory *nextdp;
    struct ged *gedp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((nextdp=db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL)
	return;

    fp = (FILE *)user_ptr1;
    pathpos = (int *)user_ptr2;
    gedp = (struct ged *)user_ptr3; 

    /* recurse on combinations */
    if (nextdp->d_flags & DIR_COMB)
	(void)printcodes(gedp, fp, nextdp, (*pathpos)+1);
}

static int
printcodes(struct ged *gedp, FILE *fp, struct directory *dp, int pathpos)
{
    int i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    if (pathpos >= MAX_LEVELS) {
	regflag = ABORTED;
	return GED_ERROR;
    }

    if (!(dp->d_flags & DIR_COMB))
	return GED_OK;

    if ((id=rt_db_get_internal( &intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource)) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "printcodes: Cannot get records for %s\n", dp->d_namep);
	return GED_ERROR;
    }

    if (id != ID_COMBINATION) {
	intern.idb_meth->ft_ifree(&intern);
	return GED_OK;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	fprintf(fp, "%-6d %-3d %-3d %-4d  ",
		comb->region_id,
		comb->aircode,
		comb->GIFTmater,
		comb->los);
	for (i=0; i < pathpos; i++)
	    fprintf(fp, "/%s", path[i]->d_namep);
	fprintf(fp, "/%s\n", dp->d_namep);
	intern.idb_meth->ft_ifree(&intern);
	return GED_OK;
    }

    if (comb->tree) {
	path[pathpos] = dp;
	db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, Do_printnode,
			 (genptr_t)fp, (genptr_t)&pathpos, (genptr_t)gedp);
    }

    intern.idb_meth->ft_ifree(&intern);
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
