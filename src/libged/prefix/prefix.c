/*                         P R E F I X . C
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
/** @file libged/prefix.c
 *
 * The prefix command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"


static void
prefix_do(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *prefix_ptr, void *obj_ptr, void *UNUSED(user_ptr3), void *UNUSED(user_ptr4))
{
    char *prefix, *obj;
    char tempstring_v4[NAMESIZE+1];
    size_t len = NAMESIZE+1;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    prefix = (char *)prefix_ptr;
    obj = (char *)obj_ptr;

    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj))
	return;

    bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
    if (db_version(dbip) < 5) {
	bu_strlcpy(tempstring_v4, prefix, len);
	bu_strlcat(tempstring_v4, obj, len);
	comb_leaf->tr_l.tl_name = bu_strdup(tempstring_v4);
    } else {
	len = strlen(prefix)+strlen(obj)+1;
	comb_leaf->tr_l.tl_name = (char *)bu_malloc(len, "Adding prefix");

	bu_strlcpy(comb_leaf->tr_l.tl_name , prefix, len);
	bu_strlcat(comb_leaf->tr_l.tl_name , obj, len);
    }
}


static const struct bu_cmd_operand prefix_schema_operands[] = {
    BU_CMD_OPERAND("prefix", BU_CMD_VALUE_STRING, 1, 1,
	"Prefix to add to object names", NULL),
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 1,
	BU_CMD_COUNT_UNLIMITED, "Database objects to rename", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema prefix_cmd_schema = {
    "prefix", "Prefix database object names and update references", NULL,
    prefix_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_prefix_core(struct ged *gedp, int argc, const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    char tempstring_v4[NAMESIZE+1];
    struct bu_vls tempstring_v5 = BU_VLS_INIT_ZERO;
    char *tempstring;
    int len = NAMESIZE+1;
    int operand_index;
    int parse_dummy = 0;
    static const char *usage = "new_prefix object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&prefix_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    bu_log("!!! ged_prefix_core: step 1\n");

    /* First, check validity, and change node names */
    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    argv[i] = "";
	    continue;
	}

	if (db_version(gedp->dbip) < 5 && (int)(strlen(argv[0]) + strlen(argv[i])) > NAMESIZE) {
	    bu_vls_printf(gedp->ged_result_str, "'%s%s' too long, must be %d characters or less.\n",
			  argv[0], argv[i], NAMESIZE);

	    argv[i] = "";
	    continue;
	}

	if (db_version(gedp->dbip) < 5) {
	    bu_strlcpy(tempstring_v4, argv[0], len);
	    bu_strlcat(tempstring_v4, argv[i], len);
	    tempstring = tempstring_v4;
	} else {
	    bu_vls_trunc(&tempstring_v5, 0);
	    bu_vls_strcpy(&tempstring_v5, argv[0]);
	    bu_vls_strcat(&tempstring_v5, argv[i]);
	    tempstring = bu_vls_addr(&tempstring_v5);
	}

	if (db_lookup(gedp->dbip, tempstring, LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: already exists\n", tempstring);
	    argv[i] = "";
	    continue;
	}

	/* Change object name in the directory. */
	if (db_rename(gedp->dbip, dp, tempstring) < 0) {
	    bu_vls_free(&tempstring_v5);
	    bu_vls_printf(gedp->ged_result_str, "error in rename to %s, aborting\n", tempstring);
	    return BRLCAD_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	    return BRLCAD_ERROR;
	}

	/* Change object name on disk. */
	if (rt_db_put_internal(dp, gedp->dbip, &intern)) {
	    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	    return BRLCAD_ERROR;
	}
	bu_log("XXXged_prefix_core: changed name from %s to %s\n", argv[i], tempstring);
    }

    bu_vls_free(&tempstring_v5);

    /* Examine all COMB nodes */
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	if (!(dp->d_flags & RT_DIR_COMB))
	    continue;

	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	    return BRLCAD_ERROR;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	for (k = 1; k < argc; k++)
	    db_tree_funcleaf(gedp->dbip, comb, comb->tree, prefix_do,
			     (void *)argv[0], (void *)argv[k], (void *)NULL, (void *)NULL);
	if (rt_db_put_internal(dp, gedp->dbip, &intern)) {
	    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	    return BRLCAD_ERROR;
	}
    } FOR_ALL_DIRECTORY_END;

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_PREFIX_COMMANDS(X, XID) \
    X(prefix, ged_prefix_core, GED_CMD_DEFAULT, &prefix_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PREFIX_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_prefix", 1, GED_PREFIX_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
