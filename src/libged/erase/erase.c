/*                         E R A S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/erase.c
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "../ged_private.h"

/*
 * Erase objects from the display.
 *
 */
int
ged_erase_core(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    int flag_A_attr=0;
    int flag_o_nonunique=1;
    int last_opt=0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    static const char *usage = "[[-r] | [[-o] -A attribute=value]] [object(s)]";
    const char *cmdName = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    --argc;
    ++argv;

    /* check args for options */
    for (i = 0; i < (size_t)argc; i++) {
		/* Erase all and quit (ignore other options) */
	char *ptr_A=NULL;
	char *ptr_o=NULL;

	if (*argv[i] != '-')
	    break;

	if (strchr(argv[i], 'r')) {
		for (i = 1; i < (size_t)argc; ++i)
		_dl_eraseAllPathsFromDisplay(gedp, argv[i], 0);
	    return GED_OK;
	}

	ptr_A=strchr(argv[i], 'A');
	if (ptr_A)
	    flag_A_attr = 1;

	ptr_o = strchr(argv[i], 'o');
	if (ptr_o)
	    flag_o_nonunique = 2;

	last_opt = i;

	if (!ptr_A && !ptr_o) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	    return GED_ERROR;
	}

	if (strlen(argv[i]) == ((size_t)1 + (ptr_A != NULL) + (ptr_o != NULL))) {
	    /* argv[i] is just a "-A" or "-o" */
	    continue;
	}

	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_ERROR;
    }

    if (flag_A_attr) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	int max_count=0;
	int remaining_args=0;
	int new_argc=0;
	char **new_argv=NULL;
	struct bu_ptbl *tbl;

	remaining_args = argc - last_opt - 1;
	if (remaining_args < 2 || remaining_args%2) {
	    bu_vls_printf(gedp->ged_result_str, "Error: must have even number of arguments (name/value pairs)\n");
	    bu_vls_free(&vls);
	    return GED_ERROR;
	}

	bu_avs_init(&avs, (argc - last_opt)/2, "ged_erase_core avs");
	i = 0;
	while (i < (size_t)argc) {
	    if (*argv[i] == '-') {
		i++;
		continue;
	    }

	    /* this is a name/value pair */
	    if (flag_o_nonunique == 2) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	    i += 2;
	}

	tbl = db_lookup_by_attr(gedp->ged_wdbp->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return GED_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return GED_OK;
	}
	for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, dp->d_namep);
	}

	max_count = BU_PTBL_LEN(tbl) + last_opt + 1;
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "ged_erase_core ptbl");
	new_argv = (char **)bu_calloc(max_count+1, sizeof(char *), "ged_erase_core new_argv");
	new_argc = bu_argv_from_string(new_argv, max_count, bu_vls_addr(&vls));

	for (i = 0; i < (size_t)new_argc; ++i) {
	    /* Skip any options */
	    if (new_argv[i][0] == '-')
		continue;

	    dl_erasePathFromDisplay(gedp, new_argv[i], 1);
	}
    } else {
	for (i = 0; i < (size_t)argc; ++i)
	    dl_erasePathFromDisplay(gedp, argv[i], 1);
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl erase_cmd_impl = {"erase", ged_erase_core, GED_CMD_DEFAULT};
const struct ged_cmd erase_cmd = { &erase_cmd_impl };

struct ged_cmd_impl d_cmd_impl = {"d", ged_erase_core, GED_CMD_DEFAULT};
const struct ged_cmd d_cmd = { &d_cmd_impl };

extern int ged_erase2_core(struct ged *gedp, int argc, const char **argv);
struct ged_cmd_impl erase2_cmd_impl = {"erase2", ged_erase2_core, GED_CMD_DEFAULT};
const struct ged_cmd erase2_cmd = { &erase2_cmd_impl };

const struct ged_cmd *erase_cmds[] = { &erase_cmd, &d_cmd, &erase2_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  erase_cmds, 3 };

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
