/*                         E D C O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "../ged_private.h"


#define EDCODES_OK BRLCAD_OK
#define EDCODES_NOTOK BRLCAD_ERROR
#define EDCODES_HALT -99


struct edcodes_args {
    const char *editor;
    int sort_by_ident;
    int name_mode;
    int sort_by_region;
};


#include "../include/plugin.h"

#define EDCODES_OPTIONS(a) \
    BU_OPT_STR(a, "E", NULL, editor, "editor", "Editor command"), \
    BU_OPT_FLAG(a, "i", NULL, sort_by_ident, "Sort by identifier"), \
    BU_OPT_FLAG(a, "n", NULL, name_mode, "List region names without editing"), \
    BU_OPT_FLAG(a, "r", NULL, sort_by_region, "Sort by region"),
BU_OPT_DESC_BUILDER(edcodes_options, struct edcodes_args, EDCODES_OPTIONS);

static const ged_opt_rule edcodes_opt_rules[] = {
    GED_RULE_OPTIONS("i n r", 0, 1, "-i, -n, and -r are mutually exclusive"),
    GED_RULE_OPERANDS(BU_CMD_CONDITION_NO_OPTION_PRESENT, "n", 1,
	BU_CMD_COUNT_UNLIMITED, "object required for region-code editing"),
    GED_RULE_NULL
};
static const ged_opt_spec edcodes_opt_spec =
    GED_OPT_WITH("edcodes", "Edit region codes", edcodes_options,
	"options-first objects:object*", edcodes_opt_rules);


static int
edcodes_id_compare(const void *p1, const void *p2, void *UNUSED(arg))
{
    int id1, id2;

    id1 = atoi(*(char **)p1);
    id2 = atoi(*(char **)p2);

    return id1 - id2;
}


static int
edcodes_reg_compare(const void *p1, const void *p2, void *UNUSED(arg))
{
    char *reg1, *reg2;

    reg1 = strchr(*(char **)p1, '/');
    reg2 = strchr(*(char **)p2, '/');

    return bu_strcmp(reg1, reg2);
}


static int edcodes_collect_regnames(struct ged *, struct directory *, int);

static void
edcodes_traverse_node(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *user_ptr1, void *user_ptr2, void *user_ptr3, void *UNUSED(user_ptr4))
{
    int ret;
    int *pathpos;
    struct directory *nextdp;
    struct ged *gedp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    nextdp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY);
    if (nextdp == RT_DIR_NULL)
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


static int
edcodes_collect_regnames(struct ged *gedp, struct directory *dp, int pathpos)
{
    int id;
    int status = 0;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return EDCODES_OK;

    id = rt_db_get_internal(&intern, dp, gedp->dbip, (matp_t)NULL);
    if (id < 0) {
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
	db_tree_funcleaf(gedp->dbip, comb, comb->tree, edcodes_traverse_node, (void *)&pathpos, (void *)gedp, (void *)&status, (void *)NULL);
    }

    intern.idb_meth->ft_ifree(&intern);

    if (status == EDCODES_HALT)
	return EDCODES_HALT;
    return EDCODES_OK;
}


int
ged_edcodes_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    char **av;
    FILE *fp = NULL;
    char tmpfil[MAXPATHLEN] = {0};
    struct edcodes_args args = {NULL, 0, 0, 0};
    int object_count = 0;
    const char **objects = NULL;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    object_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, edcodes_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (object_count < 0 ||
	(args.sort_by_ident + args.name_mode + args.sort_by_region) > 1 ||
	(!args.name_mode && object_count < 1)) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }

    objects = argv;

    if (args.name_mode) {
	struct directory *dp;

	for (i = 0; i < object_count; ++i) {
	    if ((dp = db_lookup(gedp->dbip, objects[i], LOOKUP_NOISY)) != RT_DIR_NULL) {
		status = edcodes_collect_regnames(gedp, dp, 0);

		if (status != EDCODES_OK) {
		    if (status == EDCODES_HALT)
			bu_vls_printf(gedp->ged_result_str, "%s: nesting is too deep\n", command);

		    return BRLCAD_ERROR;
		}
	    }
	}

	return BRLCAD_OK;
    }

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return BRLCAD_ERROR;

    av = (char **)bu_malloc(sizeof(char *) * (object_count + 3), "ged_edcodes_core av");
    av[0] = "wcodes";
    av[1] = tmpfil;
    for (i = 0; i < object_count; ++i)
	av[i + 2] = (char *)objects[i];

    av[object_count + 2] = NULL;

    (void)fclose(fp);

    if (ged_exec_wcodes(gedp, object_count + 2, (const char **)av) & BRLCAD_ERROR) {
	bu_file_delete(tmpfil);
	bu_free((void *)av, "ged_edcodes_core av");
	return BRLCAD_ERROR;
    }

    if (args.sort_by_ident || args.sort_by_region) {
	char **line_array;
	char aline[RT_MAXLINE];
	FILE *f_srt;
	int line_count=0;
	int j;

	f_srt = fopen(tmpfil, "r+");
	if (f_srt == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Failed to open temp file for sorting\n", command);
	    bu_file_delete(tmpfil);
	    return BRLCAD_ERROR;
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
	if (args.sort_by_ident) {
	    bu_sort((void *)line_array, line_count, sizeof(char *), edcodes_id_compare, NULL);
	} else {
	    bu_sort((void *)line_array, line_count, sizeof(char *), edcodes_reg_compare, NULL);
	}

	/* rewrite the temp file using the sorted lines */
	rewind(f_srt);
	for (j = 0; j < line_count; j++) {
	    fprintf(f_srt, "%s", line_array[j]);
	    bu_free(line_array[j], "ged_edcodes_core line array element");
	}
	bu_free((char *)line_array, "ged_edcodes_core line array");
	fclose(f_srt);
    }

    if (_ged_editit(gedp, args.editor, tmpfil)) {
	av[0] = "rcodes";
	av[2] = NULL;
	status = ged_exec_rcodes(gedp, 2, (const char **)av);
    } else
	status = BRLCAD_ERROR;

    bu_file_delete(tmpfil);
    bu_free((void *)av, "ged_edcodes_core av");
    return status;
}


#define GED_EDCODES_COMMANDS(X, XID) \
    X(edcodes, ged_edcodes_core, GED_CMD_DEFAULT, &edcodes_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_EDCODES_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_edcodes", 1, GED_EDCODES_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
