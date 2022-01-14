/*                         T A B L E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "bu/sort.h"
#include "bu/units.h"
#include "../ged_private.h"


#define ABORTED		-99
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

static int idfd = 0;
static int rd_idfd = 0;


/* TODO - this approach to tables_sol_number assignment is pretty
 * ugly, and arguably even wrong in that it is hiding an exact
 * floating point comparison of matrices behind the (char *) case of
 * the identt structure.
 *
 * That said, this logic is doing something interesting in that it is
 * attempting to move the definition of a unique solid beyond just the
 * object to the object instance - e.g. it incorporates the matrix in
 * the parent comb in its uniqueness test.
 *
 * Need to think about what it actually means to be a unique instance
 * in the database and do something more intelligent.  For example, if
 * we have two paths:
 *
 * a/b/c.s  and  d/e/c.s
 *
 * do they describe the same instance in space if their matrices are
 * all identity and their booleans all unions?  Their path structure
 * is different, but the volume in space they are denoting as occupied
 * is not.
 *
 * One possible approach to this would be to enhance full path data
 * structures to incorporate matrix awareness, define an API to
 * compare two such paths including a check for solid uniqueness
 * (e.g. return same if the two paths define the same solid, even if
 * the paths themselves differ), and then construct the set of paths
 * for the tables input object trees and use that set to test for the
 * uniqueness of a given path.
 */


/* structure to distinguish new solids from existing (old) solids */
struct identt {
    size_t i_index;
    char i_name[NAMESIZE+1];
    mat_t i_mat;
};


HIDDEN int
tables_check(char *a, char *b)
{

    int c= sizeof(struct identt);

    while (c--) {
	if (*a++ != *b++) {
	    return 0;	/* no match */
	}
    }
    return 1;	/* match */

}


HIDDEN size_t
tables_sol_number(const matp_t matrix, char *name, size_t *old, size_t *numsol)
{
    b_off_t i;
    struct identt idbuf1, idbuf2;
    static struct identt identt = {0, {0}, MAT_INIT_ZERO};
    ssize_t readval;

    memset(&idbuf1, 0, sizeof(struct identt));
    bu_strlcpy(idbuf1.i_name, name, sizeof(idbuf1.i_name));
    MAT_COPY(idbuf1.i_mat, matrix);

    for (i = 0; i < (ssize_t)*numsol; i++) {
	(void)bu_lseek(rd_idfd, i*sizeof(identt), 0);
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

    (void)bu_lseek(idfd, 0, 2);
    i = write(idfd, &idbuf1, sizeof identt);
    if (i < 0)
	perror("write");

    *old = 0;
    return idbuf1.i_index;
}


/* Build up sortable entities */

struct tree_obj {
    struct bu_vls *tree;
    struct bu_vls *describe;
};


struct table_obj {
    int numreg;
    int region_id;
    int aircode;
    int GIFTmater;
    int los;
    struct bu_vls *path;
    struct bu_ptbl *tree_objs;
};


static int
sort_table_objs(const void *a, const void *b, void *UNUSED(arg))
{
    struct table_obj *ao = *(struct table_obj **)a;
    struct table_obj *bo = *(struct table_obj **)b;

    if (ao->region_id > bo->region_id)
	return 1;
    if (ao->region_id < bo->region_id)
	return -1;
    if (ao->numreg > bo->numreg)
	return 1;
    if (ao->numreg < bo->numreg)
	return -1;
    return 0;
}


HIDDEN void
tables_objs_print(struct bu_vls *tabvls, struct bu_ptbl *tabptr, int type)
{
    size_t i, j;
    struct table_obj *o;
    for (i = 0; i < BU_PTBL_LEN(tabptr); i++) {
	o = (struct table_obj *)BU_PTBL_GET(tabptr, i);
	bu_vls_printf(tabvls, " %-4d %4d %4d %4d %4d  ",
		      o->numreg, o->region_id, o->aircode, o->GIFTmater, o->los);

	bu_vls_printf(tabvls, "%s", bu_vls_addr(o->path));
	if (type != ID_TABLE) {
	    for (j = 0; j < BU_PTBL_LEN(o->tree_objs); j++) {
		struct tree_obj *t = (struct tree_obj *)BU_PTBL_GET(o->tree_objs, j);
		bu_vls_printf(tabvls, "%s", bu_vls_addr(t->tree));
		if (type == SOL_TABLE) {
		    bu_vls_printf(tabvls, "%s", bu_vls_addr(t->describe));
		}
	    }
	}
    }

}


HIDDEN void
tables_new(struct ged *gedp, struct bu_ptbl *tabptr, struct directory *dp, struct bu_ptbl *cur_path, const fastf_t *old_mat, int flag, size_t *numreg, size_t *numsol)
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

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
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
    BU_ASSERT(actual_count == node_count);

    if (dp->d_flags & RT_DIR_REGION) {
	struct table_obj *nobj;
	BU_GET(nobj, struct table_obj);
	BU_GET(nobj->path, struct bu_vls);
	bu_vls_init(nobj->path);
	BU_GET(nobj->tree_objs , struct bu_ptbl);
	bu_ptbl_init(nobj->tree_objs, 8, "tree objs tbl");

	bu_ptbl_ins(tabptr, (long *)nobj);

	(*numreg)++;

	nobj->numreg = *numreg;
	nobj->region_id = comb->region_id;
	nobj->aircode = comb->aircode;
	nobj->GIFTmater = comb->GIFTmater;
	nobj->los = comb->los;

	for (k = 0; k < BU_PTBL_LEN(cur_path); k++) {
	    struct directory *path_dp;

	    path_dp = (struct directory *)BU_PTBL_GET(cur_path, k);
	    RT_CK_DIR(path_dp);
	    bu_vls_printf(nobj->path, "/%s", path_dp->d_namep);
	}
	bu_vls_printf(nobj->path, "/%s:\n", dp->d_namep);

	if (flag == ID_TABLE)
	    goto out;

	for (i = 0; i < actual_count; i++) {
	    char op;
	    int nsoltemp=0;
	    struct rt_db_internal sol_intern;
	    struct directory *sol_dp;
	    mat_t temp_mat;
	    size_t old;
	    struct tree_obj *tobj;
	    BU_GET(tobj, struct tree_obj);
	    BU_GET(tobj->tree, struct bu_vls);
	    BU_GET(tobj->describe, struct bu_vls);
	    bu_vls_init(tobj->tree);
	    bu_vls_init(tobj->describe);
	    bu_ptbl_ins(nobj->tree_objs, (long *)tobj);

	    switch (tree_list[i].tl_op) {
		case OP_UNION:
		    op = DB_OP_UNION;
		    break;
		case OP_SUBTRACT:
		    op = DB_OP_UNION;
		    break;
		case OP_INTERSECT:
		    op = DB_OP_INTERSECT;
		    break;
		default:
		    bu_log("unrecognized operation in region %s\n", dp->d_namep);
		    op = '?';
		    break;
	    }

	    sol_dp = db_lookup(gedp->dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET);
	    if (sol_dp != RT_DIR_NULL) {
		if (sol_dp->d_flags & RT_DIR_COMB) {
		    bu_vls_printf(tobj->tree, "   RG %c %s\n", op, sol_dp->d_namep);
		    continue;
		} else if (!(sol_dp->d_flags & RT_DIR_SOLID)) {
		    bu_vls_printf(tobj->tree, "   ?? %c %s\n", op, sol_dp->d_namep);
		    continue;
		} else {
		    if (tree_list[i].tl_tree->tr_l.tl_mat) {
			bn_mat_mul(temp_mat, old_mat,
				   tree_list[i].tl_tree->tr_l.tl_mat);
		    } else {
			MAT_COPY(temp_mat, old_mat);
		    }
		    if (rt_db_get_internal(&sol_intern, sol_dp, gedp->dbip, temp_mat, &rt_uniresource) < 0) {
			bu_log("Could not import %s\n", tree_list[i].tl_tree->tr_l.tl_name);
		    }
		    nsoltemp = tables_sol_number((matp_t)temp_mat, tree_list[i].tl_tree->tr_l.tl_name, &old, numsol);
		    bu_vls_printf(tobj->tree, "   %c [%d] ", op, nsoltemp);
		}
	    } else {
		const matp_t mat = (matp_t)old_mat;
		nsoltemp = tables_sol_number(mat, tree_list[i].tl_tree->tr_l.tl_name, &old, numsol);
		bu_vls_printf(tobj->tree, "   %c [%d] ", op, nsoltemp);
		continue;
	    }

	    if (flag == REG_TABLE || old) {
		bu_vls_printf(tobj->tree, "%s\n", tree_list[i].tl_tree->tr_l.tl_name);
		continue;
	    } else {
		bu_vls_printf(tobj->tree, "%s:  ", tree_list[i].tl_tree->tr_l.tl_name);
	    }

	    if (!old && (sol_dp->d_flags & RT_DIR_SOLID)) {
		/* if we get here, we must be looking for a solid table */
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		if (!OBJ[sol_intern.idb_type].ft_describe ||
		    OBJ[sol_intern.idb_type].ft_describe(&tmp_vls, &sol_intern, 1, gedp->dbip->dbi_base2local) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s describe error\n", tree_list[i].tl_tree->tr_l.tl_name);
		}
		bu_vls_printf(tobj->describe, "%s", bu_vls_addr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }
	    if (nsoltemp && (sol_dp->d_flags & RT_DIR_SOLID))
		rt_db_free_internal(&sol_intern);
	}
    } else if (dp->d_flags & RT_DIR_COMB) {
	int cur_length;

	bu_ptbl_ins(cur_path, (long *)dp);
	cur_length = BU_PTBL_LEN(cur_path);

	for (i = 0; i < actual_count; i++) {
	    struct directory *nextdp;
	    mat_t new_mat;

	    /* For the 'idents' command skip over non-union
	     * combinations above the region level, these members of a
	     * combination don't add positively to the defined regions
	     * of space and their region ID's will not show up along a
	     * shotline unless positively added elsewhere in the
	     * hierarchy. This is causing headaches for users
	     * generating an association table from our 'idents'
	     * listing.
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

	    nextdp = db_lookup(gedp->dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY);
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
	    tables_new(gedp, tabptr, nextdp, cur_path, new_mat, flag, numreg, numsol);
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


HIDDEN void
tables_header(struct bu_vls *tabvls, int argc, const char **argv, struct ged *gedp, char *timep)
{
    int i;
    bu_vls_printf(tabvls, "1 -8    Summary Table {%s}  (written: %s)\n", argv[0], timep);
    bu_vls_printf(tabvls, "2 -7         file name    : %s\n", gedp->dbip->dbi_filename);
    bu_vls_printf(tabvls, "3 -6         \n");
    bu_vls_printf(tabvls, "4 -5         \n");
#ifndef _WIN32
    bu_vls_printf(tabvls, "5 -4         user         : %s\n", getpwuid(getuid())->pw_gecos);
#else
    {
	char uname[256];
	DWORD dwNumBytes = 256;
	if (GetUserName(uname, &dwNumBytes))
	    bu_vls_printf(tabvls, "5 -4         user         : %s\n", uname);
	else
	    bu_vls_printf(tabvls, "5 -4         user         : UNKNOWN\n");
    }
#endif
    bu_vls_printf(tabvls, "6 -3         target title : %s\n", gedp->dbip->dbi_title);
    bu_vls_printf(tabvls, "7 -2         target units : %s\n", bu_units_string(gedp->dbip->dbi_local2base));
    bu_vls_printf(tabvls, "8 -1         objects      :");
    for (i = 2; i < argc; i++) {
	if ((i%8) == 0)
	    bu_vls_printf(tabvls, "\n                           ");
	bu_vls_printf(tabvls, " %s", argv[i]);
    }
    bu_vls_printf(tabvls, "\n\n");
}


int
ged_tables_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "file object(s)";

    FILE *ftabvls = NULL;
    FILE *test_f = NULL;
    char *timep;
    int flag;
    int status;
    size_t i, j;
    size_t numreg = 0;
    size_t numsol = 0;
    struct bu_ptbl tabobjs;
    struct bu_ptbl cur_path;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    struct bu_vls tabvls = BU_VLS_INIT_ZERO;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    time_t now;

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
    bu_ptbl_init(&tabobjs, 8, "f_tables: objects");

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
    test_f = fopen(argv[1], "w+");
    if (test_f == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s:  Can't open file [%s]\n\tMake sure the directory and file are writable.\n", argv[0], argv[1]);
	status = GED_ERROR;
	goto end;
    }
    fclose(test_f);

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

    tables_header(&tabvls, argc, argv, gedp, timep);


    /* make the tables */
    for (i = 2; i < (size_t)argc; i++) {
	struct directory *dp;

	bu_ptbl_reset(&cur_path);
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) != RT_DIR_NULL)
	    tables_new(gedp, &tabobjs, dp, &cur_path, (const fastf_t *)bn_mat_identity, flag, &numreg, &numsol);
	else
	    bu_vls_printf(gedp->ged_result_str, "%s:  skip this object\n", argv[i]);
    }

    tables_objs_print(&tabvls, &tabobjs, flag);

    bu_vls_printf(gedp->ged_result_str, "Summary written to: %s\n", argv[1]);

    if (flag == SOL_TABLE || flag == REG_TABLE) {
	bu_vls_printf(&tabvls, "\n\nNumber Primitives = %zu  Number Regions = %zu\n",
		      numsol, numreg);

	bu_vls_printf(gedp->ged_result_str, "Processed %lu Primitives and %lu Regions\n",
		      numsol, numreg);
    } else {
	bu_vls_printf(&tabvls, "* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");

	tables_header(&tabvls, argc, argv, gedp, timep);

	bu_vls_printf(gedp->ged_result_str, "Processed %lu Regions\n", numreg);

	/* make ordered idents and re-print */
	bu_sort(BU_PTBL_BASEADDR(&tabobjs), BU_PTBL_LEN(&tabobjs), sizeof(struct table_obj *), sort_table_objs, NULL);

	tables_objs_print(&tabvls, &tabobjs, flag);

	bu_vls_printf(&tabvls, "* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
    }

    ftabvls = fopen(argv[1], "w+");
    if (ftabvls == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s:  Can't open file [%s]\n\tMake sure the directory and file are still writable.\n", argv[0], argv[1]);
	status = GED_ERROR;
	goto end;
    }
    bu_vls_fwrite(ftabvls, &tabvls);
    (void)fclose(ftabvls);

end:
    bu_vls_free(&cmd);
    bu_vls_free(&tmp_vls);
    bu_vls_free(&tabvls);
    bu_ptbl_free(&cur_path);

    for (i = 0; i < BU_PTBL_LEN(&tabobjs); i++) {
	struct table_obj *o = (struct table_obj *)BU_PTBL_GET(&tabobjs, i);
	for (j = 0; j < BU_PTBL_LEN(o->tree_objs); j++) {
	    struct tree_obj *t = (struct tree_obj *)BU_PTBL_GET(o->tree_objs, j);
	    bu_vls_free(t->tree);
	    bu_vls_free(t->describe);
	    BU_PUT(t->tree, struct bu_vls);
	    BU_PUT(t->describe, struct bu_vls);
	    BU_PUT(t, struct tree_obj);
	}
	bu_vls_free(o->path);
	BU_PUT(o->path, struct bu_vls);
	BU_PUT(o, struct table_obj);
    }
    bu_ptbl_free(&tabobjs);

    return status;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl tables_cmd_impl = {"tables", ged_tables_core, GED_CMD_DEFAULT};
const struct ged_cmd tables_cmd = { &tables_cmd_impl };

struct ged_cmd_impl idents_cmd_impl = {"idents", ged_tables_core, GED_CMD_DEFAULT};
const struct ged_cmd idents_cmd = { &idents_cmd_impl };

struct ged_cmd_impl regions_cmd_impl = {"regions", ged_tables_core, GED_CMD_DEFAULT};
const struct ged_cmd regions_cmd = { &regions_cmd_impl };

struct ged_cmd_impl solids_cmd_impl = {"solids", ged_tables_core, GED_CMD_DEFAULT};
const struct ged_cmd solids_cmd = { &solids_cmd_impl };

const struct ged_cmd *tables_cmds[] = { &tables_cmd, &idents_cmd, &regions_cmd, &solids_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  tables_cmds, 4 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
