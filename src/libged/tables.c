/*                         T A B L E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/tables.c
 *
 * The tables command.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


/* structure to distinguish new solids from existing (old) solids */
struct identt {
    size_t i_index;
    char i_name[NAMESIZE+1];
    mat_t i_mat;
};


#define ABORTED		-99
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

static int idfd = 0;
static int rd_idfd = 0;
static FILE *tabptr = NULL;


HIDDEN int
tables_check(char *a, char *b)
{

    int c= sizeof(struct identt);

    while (c--) if (*a++ != *b++) return 0;	/* no match */
    return 1;	/* match */

}


HIDDEN size_t
tables_sol_number(const matp_t matrix, char *name, size_t *old, size_t *numsol)
{
    off_t i;
    struct identt idbuf1, idbuf2;
    static struct identt identt = {0, {0}, MAT_INIT_ZERO};
    ssize_t readval;

    memset(&idbuf1, 0, sizeof(struct identt));
    bu_strlcpy(idbuf1.i_name, name, sizeof(idbuf1.i_name));
    MAT_COPY(idbuf1.i_mat, matrix);

    for (i = 0; i < (ssize_t)*numsol; i++) {
	(void)lseek(rd_idfd, i*sizeof(identt), 0);
	readval = read(rd_idfd, &idbuf2, sizeof identt);

	if (readval < 0) {
	    perror("READ ERROR");
	}

	idbuf1.i_index = i + 1;

	if (tables_check((char *)&idbuf1, (char *)&idbuf2) == 1) {
	    *old = 1;
	    return idbuf2.i_index;
	}
    }
    (*numsol)++;
    idbuf1.i_index = *numsol;

    (void)lseek(idfd, 0, 2);
    i = write(idfd, &idbuf1, sizeof identt);
    if (i < 0)
	perror("write");

    *old = 0;
    return idbuf1.i_index;
}


HIDDEN void
tables_new(struct ged *gedp, struct directory *dp, struct bu_ptbl *cur_path, const fastf_t *old_mat, int flag, size_t *numreg, size_t *numsol)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *tree_list;
    size_t node_count;
    size_t actual_count;
    size_t i, k;

    RT_CK_DIR(dp);
    BU_CK_PTBL(cur_path);

    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	return;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for editing\n");
	    intern.idb_meth->ft_ifree(&intern);
	    return;
	}
    }

    if (!comb->tree) {
	/* empty combination */
	intern.idb_meth->ft_ifree(&intern);
	return;
    }

    node_count = db_tree_nleaves(comb->tree);
    tree_list = (struct rt_tree_array *)bu_calloc(node_count,
						  sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    actual_count = (struct rt_tree_array *)db_flatten_tree(tree_list,
							   comb->tree, OP_UNION, 0, &rt_uniresource) - tree_list;
    BU_ASSERT_SIZE_T(actual_count, ==, node_count);

    if (dp->d_flags & RT_DIR_REGION) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	(*numreg)++;
	bu_vls_printf(&str, " %-4zu %4ld %4ld %4ld %4ld  ",
		      *numreg, comb->region_id, comb->aircode, comb->GIFTmater, comb->los);
	bu_vls_fwrite(tabptr, &str);
	bu_vls_free(&str);

	for (k = 0; k < BU_PTBL_LEN(cur_path); k++) {
	    struct directory *path_dp;

	    path_dp = (struct directory *)BU_PTBL_GET(cur_path, k);
	    RT_CK_DIR(path_dp);
	    fprintf(tabptr, "/%s", path_dp->d_namep);
	}
	fprintf(tabptr, "/%s:\n", dp->d_namep);

	if (flag == ID_TABLE)
	    goto out;

	for (i = 0; i < actual_count; i++) {
	    char op;
	    int nsoltemp=0;
	    struct rt_db_internal sol_intern;
	    struct directory *sol_dp;
	    mat_t temp_mat;
	    size_t old;

	    switch (tree_list[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		default:
		    bu_log("unrecognized operation in region %s\n", dp->d_namep);
		    op = '?';
		    break;
	    }

	    if ((sol_dp=db_lookup(gedp->ged_wdbp->dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
		if (sol_dp->d_flags & RT_DIR_COMB) {
		    fprintf(tabptr, "   RG %c %s\n",
				  op, sol_dp->d_namep);
		    continue;
		} else if (!(sol_dp->d_flags & RT_DIR_SOLID)) {
		    fprintf(tabptr, "   ?? %c %s\n",
				  op, sol_dp->d_namep);
		    continue;
		} else {
		    if (tree_list[i].tl_tree->tr_l.tl_mat) {
			bn_mat_mul(temp_mat, old_mat,
				   tree_list[i].tl_tree->tr_l.tl_mat);
		    } else {
			MAT_COPY(temp_mat, old_mat);
		    }
		    if (rt_db_get_internal(&sol_intern, sol_dp, gedp->ged_wdbp->dbip, temp_mat, &rt_uniresource) < 0) {
			bu_log("Could not import %s\n", tree_list[i].tl_tree->tr_l.tl_name);
			nsoltemp = 0;
		    }
		    nsoltemp = tables_sol_number((const matp_t)temp_mat, tree_list[i].tl_tree->tr_l.tl_name, &old, numsol);
		    fprintf(tabptr, "   %c [%d] ", op, nsoltemp);
		}
	    } else {
		const matp_t mat = (const matp_t)old_mat;
		nsoltemp = tables_sol_number(mat, tree_list[i].tl_tree->tr_l.tl_name, &old, numsol);
		fprintf(tabptr, "   %c [%d] ", op, nsoltemp);
		continue;
	    }

	    if (flag == REG_TABLE || old) {
		(void) fprintf(tabptr, "%s\n", tree_list[i].tl_tree->tr_l.tl_name);
		continue;
	    } else
		(void) fprintf(tabptr, "%s:  ", tree_list[i].tl_tree->tr_l.tl_name);

	    if (!old && (sol_dp->d_flags & RT_DIR_SOLID)) {
		/* if we get here, we must be looking for a solid table */
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		if (!OBJ[sol_intern.idb_type].ft_describe ||
		    OBJ[sol_intern.idb_type].ft_describe(&tmp_vls, &sol_intern, 1, gedp->ged_wdbp->dbip->dbi_base2local, &rt_uniresource, gedp->ged_wdbp->dbip) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s describe error\n", tree_list[i].tl_tree->tr_l.tl_name);
		}
		fprintf(tabptr, "%s", bu_vls_addr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }
	    if (nsoltemp && (sol_dp->d_flags & RT_DIR_SOLID))
		rt_db_free_internal(&sol_intern);
	}
    } else if (dp->d_flags & RT_DIR_COMB) {
	int cur_length;

	bu_ptbl_ins(cur_path, (long *)dp);
	cur_length = BU_PTBL_END(cur_path);

	for (i = 0; i < actual_count; i++) {
	    struct directory *nextdp;
	    mat_t new_mat;

	    /* For the 'idents' command skip over non-union combinations above the region level,
	     * these members of a combination don't add positively to the defined regions of space
	     * and their region ID's will not show up along a shotline unless positively added
	     * elsewhere in the hierarchy. This is causing headaches for users generating an
	     * association table from our 'idents' listing.
	     */
	    if (flag == ID_TABLE) {
		switch (tree_list[i].tl_op) {
		    case OP_UNION:
			break;
		    case OP_SUBTRACT:
		    case OP_INTERSECT:
			continue;
			break;
		    default:
			bu_log("unrecognized operation in combination %s\n", dp->d_namep);
			break;
		}
	    }

	    nextdp = db_lookup(gedp->ged_wdbp->dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY);
	    if (nextdp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "\tskipping this object\n");
		continue;
	    }

	    /* recurse */
	    if (tree_list[i].tl_tree->tr_l.tl_mat) {
		bn_mat_mul(new_mat, old_mat, tree_list[i].tl_tree->tr_l.tl_mat);
	    } else {
		MAT_COPY(new_mat, old_mat);
	    }
	    tables_new(gedp, nextdp, cur_path, new_mat, flag, numreg, numsol);
	    bu_ptbl_trunc(cur_path, cur_length);
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "Illegal flags for %s skipping\n", dp->d_namep);
	return;
    }

out:
    bu_free((char *)tree_list, "new_tables: tree_list");
    intern.idb_meth->ft_ifree(&intern);
    return;
}


int
ged_tables(struct ged *gedp, int argc, const char *argv[])
{
    static const char sortcmd_orig[] = "sort -n +1 -2 -o /tmp/ord_id ";
    static const char sortcmd_long[] = "sort --numeric --key=2, 2 --output /tmp/ord_id ";
    static const char catcmd[] = "cat /tmp/ord_id >> ";
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    struct bu_ptbl cur_path;
    int flag;
    int status;
    char *timep;
    time_t now;
    int i;
    const char *usage = "file object(s)";

    size_t numreg = 0;
    size_t numsol = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_ptbl_init(&cur_path, 8, "f_tables: cur_path");

    status = GED_OK;

    /* find out which ascii table is desired */
    if (BU_STR_EQUAL(argv[0], "solids")) {
	/* complete summary - down to solids/parameters */
	flag = SOL_TABLE;
    } else if (BU_STR_EQUAL(argv[0], "regions")) {
	/* summary down to solids as members of regions */
	flag = REG_TABLE;
    } else if (BU_STR_EQUAL(argv[0], "idents")) {
	/* summary down to regions */
	flag = ID_TABLE;
    } else {
	/* should never reach here */
	bu_vls_printf(gedp->ged_result_str, "%s:  input error\n", argv[0]);
	status = GED_ERROR;
	goto end;
    }

    /* open the file */
    if ((tabptr=fopen(argv[1], "w+")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s:  Can't open %s\n", argv[0], argv[1]);
	status = GED_ERROR;
	goto end;
    }

    if (flag == SOL_TABLE || flag == REG_TABLE) {
	/* temp file for discrimination of solids */
	/* !!! this needs to be a bu_temp_file() */
	if ((idfd = creat("/tmp/mged_discr", 0600)) < 0) {
	    perror("/tmp/mged_discr");
	    status = GED_ERROR;
	    goto end;
	}
	rd_idfd = open("/tmp/mged_discr", 2);
    }

    (void)time(&now);
    timep = ctime(&now);
    timep[24] = '\0';
    fprintf(tabptr, "1 -8    Summary Table {%s}  (written: %s)\n", argv[0], timep);
    fprintf(tabptr, "2 -7         file name    : %s\n", gedp->ged_wdbp->dbip->dbi_filename);
    fprintf(tabptr, "3 -6         \n");
    fprintf(tabptr, "4 -5         \n");
#ifndef _WIN32
    fprintf(tabptr, "5 -4         user         : %s\n", getpwuid(getuid())->pw_gecos);
#else
    {
	char uname[256];
	DWORD dwNumBytes = 256;
	if (GetUserName(uname, &dwNumBytes))
	    fprintf(tabptr, "5 -4         user         : %s\n", uname);
	else
	    fprintf(tabptr, "5 -4         user         : UNKNOWN\n");
    }
#endif
    fprintf(tabptr, "6 -3         target title : %s\n", gedp->ged_wdbp->dbip->dbi_title);
    fprintf(tabptr, "7 -2         target units : %s\n",
		  bu_units_string(gedp->ged_wdbp->dbip->dbi_local2base));
    fprintf(tabptr, "8 -1         objects      :");
    for (i = 2; i < argc; i++) {
	if ((i%8) == 0)
	    fprintf(tabptr, "\n                           ");
	fprintf(tabptr, " %s", argv[i]);
    }
    fprintf(tabptr, "\n\n");

    /* make the tables */
    for (i = 2; i < argc; i++) {
	struct directory *dp;

	bu_ptbl_reset(&cur_path);
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) != RT_DIR_NULL)
	    tables_new(gedp, dp, &cur_path, (const fastf_t *)bn_mat_identity, flag, &numreg, &numsol);
	else
	    bu_vls_printf(gedp->ged_result_str, "%s:  skip this object\n", argv[i]);
    }

    bu_vls_printf(gedp->ged_result_str, "Summary written in: %s\n", argv[1]);

    if (flag == SOL_TABLE || flag == REG_TABLE) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	/* FIXME: should not assume /tmp */
	bu_file_delete("/tmp/mged_discr\0");

	bu_vls_printf(&str, "\n\nNumber Primitives = %zu  Number Regions = %zu\n",
		      numsol, numreg);
	bu_vls_fwrite(tabptr, &str);
	bu_vls_free(&str);

	bu_vls_printf(gedp->ged_result_str, "Processed %lu Primitives and %lu Regions\n",
		      numsol, numreg);

	(void)fclose(tabptr);
    } else {
	int ret;

	fprintf(tabptr, "* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
	(void)fclose(tabptr);

	bu_vls_printf(gedp->ged_result_str, "Processed %lu Regions\n", numreg);

	/* make ordered idents - tries newer gnu 'sort' syntax if not successful */
	bu_vls_strcpy(&cmd, sortcmd_orig);
	bu_vls_strcat(&cmd, argv[1]);
	bu_vls_strcat(&cmd, " 2> /dev/null");
	if (system(bu_vls_addr(&cmd)) != 0) {
	    bu_vls_trunc(&cmd, 0);
	    bu_vls_strcpy(&cmd, sortcmd_long);
	    bu_vls_strcat(&cmd, argv[1]);
	    ret = system(bu_vls_addr(&cmd));
	    if (ret != 0)
		bu_log("WARNING: sort failure detected\n");
	}
	bu_vls_printf(gedp->ged_result_str, "%V\n", &cmd);

	bu_vls_trunc(&cmd, 0);
	bu_vls_strcpy(&cmd, catcmd);
	bu_vls_strcat(&cmd, argv[1]);
	bu_vls_printf(gedp->ged_result_str, "%V\n", &cmd);
	ret = system(bu_vls_addr(&cmd));
	if (ret != 0)
	    bu_log("WARNING: cat failure detected\n");

	bu_file_delete("/tmp/ord_id\0");
    }

end:
    bu_vls_free(&cmd);
    bu_vls_free(&tmp_vls);
    bu_ptbl_free(&cur_path);

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
