/*                         E D C O D E S . C
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
/** @file libged/edcodes.c
 *
 * The edcodes command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


#define EDCODES_OK GED_OK
#define EDCODES_NOTOK GED_ERROR
#define EDCODES_HALT -99


HIDDEN int
edcodes_id_compare(const void *p1, const void *p2)
{
    int id1, id2;

    id1 = atoi(*(char **)p1);
    id2 = atoi(*(char **)p2);

    return id1 - id2;
}


HIDDEN int
edcodes_reg_compare(const void *p1, const void *p2)
{
    char *reg1, *reg2;

    reg1 = strchr(*(char **)p1, '/');
    reg2 = strchr(*(char **)p2, '/');

    return bu_strcmp(reg1, reg2);
}


HIDDEN int edcodes_collect_regnames(struct ged *, struct directory *, int);

HIDDEN void
edcodes_traverse_node(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3)
{
    int ret;
    int *pathpos;
    struct directory *nextdp;
    struct ged *gedp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((nextdp=db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    pathpos = (int *)user_ptr1;
    gedp = (struct ged *)user_ptr2;

    /* recurse on combinations */
    if (nextdp->d_flags & RT_DIR_COMB) {
	int *status = (int *)user_ptr3;
	ret = edcodes_collect_regnames(gedp, nextdp, (*pathpos)+1);
	if (status && ret == EDCODES_HALT)
	    *status = EDCODES_HALT;
    }
}


HIDDEN int
edcodes_collect_regnames(struct ged *gedp, struct directory *dp, int pathpos)
{
    int id;
    int status = 0;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return EDCODES_OK;

    if ((id=rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip,
			       (matp_t)NULL, &rt_uniresource)) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "Cannot get records for %s\n", dp->d_namep);
	return EDCODES_NOTOK;
    }

    if (id != ID_COMBINATION) {
	intern.idb_meth->ft_ifree(&intern);
	return EDCODES_OK;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	bu_vls_printf(gedp->ged_result_str, " %s", dp->d_namep);
	intern.idb_meth->ft_ifree(&intern);
	return EDCODES_OK;
    }

    if (comb->tree) {
	db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, edcodes_traverse_node, (genptr_t)&pathpos, (genptr_t)gedp, (genptr_t)&status);
    }

    intern.idb_meth->ft_ifree(&intern);

    if (status == EDCODES_HALT)
	return EDCODES_HALT;
    return EDCODES_OK;
}


int
ged_edcodes(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int nflag = 0;
    int status;
    int sort_by_ident=0;
    int sort_by_region=0;
    int c;
    char **av;
    FILE *fp = NULL;
    char tmpfil[MAXPATHLEN] = {0};
    const char *editstring = NULL;

    static const char *usage = "[-i|-n|-r] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* First, grab the editstring off of the argv list */

    bu_optind = 1;
    /* First, grab the editstring off of the argv list */
    while ((c = bu_getopt(argc, (char * const *)argv, "E:")) != -1) {
	switch (c) {
	    case 'E' :
		editstring = bu_optarg;
		break;
	    default :
		break;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "inr")) != -1) {
	switch(c) {
	    case 'i':
		sort_by_ident = 1;
		break;
	    case 'n':
		nflag = 1;
		break;
	    case 'r':
		sort_by_region = 1;
		break;
	}
    }

    if ((nflag + sort_by_ident + sort_by_region) > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (nflag) {
	struct directory *dp;

	for (i = 1; i < argc; ++i) {
	    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) != RT_DIR_NULL) {
		status = edcodes_collect_regnames(gedp, dp, 0);

		if (status != EDCODES_OK) {
		    if (status == EDCODES_HALT)
			bu_vls_printf(gedp->ged_result_str, "%s: nesting is too deep\n", argv[0]);

		    return GED_ERROR;
		}
	    }
	}

	return GED_OK;
    }

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return GED_ERROR;

    av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "ged_edcodes av");
    av[0] = "wcodes";
    av[1] = tmpfil;
    for (i = 2; i < argc + 1; ++i)
	av[i] = (char *)argv[i-1];

    av[i] = NULL;

    if (ged_wcodes(gedp, argc + 1, (const char **)av) == GED_ERROR) {
	bu_file_delete(tmpfil);
	bu_free((genptr_t)av, "ged_edcodes av");
	return GED_ERROR;
    }

    (void)fclose(fp);

    if (sort_by_ident || sort_by_region) {
	char **line_array;
	char aline[RT_MAXLINE];
	FILE *f_srt;
	int line_count=0;
	int j;

	if ((f_srt=fopen(tmpfil, "r+")) == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Failed to open temp file for sorting\n", argv[0]);
	    bu_file_delete(tmpfil);
	    return GED_ERROR;
	}

	/* count lines */
	while (bu_fgets(aline, RT_MAXLINE, f_srt)) {
	    line_count++;
	}

	/* build array of lines */
	line_array = (char **)bu_calloc(line_count, sizeof(char *), "edcodes line array");

	/* read lines and save into the array */
	rewind(f_srt);
	line_count = 0;
	while (bu_fgets(aline, RT_MAXLINE, f_srt)) {
	    line_array[line_count] = bu_strdup(aline);
	    line_count++;
	}

	/* sort the array of lines */
	if (sort_by_ident) {
	    qsort(line_array, line_count, sizeof(char *), edcodes_id_compare);
	} else {
	    qsort(line_array, line_count, sizeof(char *), edcodes_reg_compare);
	}

	/* rewrite the temp file using the sorted lines */
	rewind(f_srt);
	for (j=0; j<line_count; j++) {
	    fprintf(f_srt, "%s", line_array[j]);
	    bu_free(line_array[j], "ged_edcodes line array element");
	}
	bu_free((char *)line_array, "ged_edcodes line array");
	fclose(f_srt);
    }

    if (_ged_editit(editstring, tmpfil)) {
	av[0] = "rcodes";
	av[2] = NULL;
	status = ged_rcodes(gedp, 2, (const char **)av);
    } else
	status = GED_ERROR;

    bu_file_delete(tmpfil);
    bu_free((genptr_t)av, "ged_edcodes av");
    return status;
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
