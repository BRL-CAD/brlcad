/*                         M O V E _ A L L . C
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
/** @file libged/move_all.c
 *
 * The move_all command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bu/str.h"
#include "rt/geom.h"

#include "../ged_private.h"


struct move_all_args {
    int no_change;
    const char *mapping_file;
};


#include "../include/plugin.h"

static const struct bu_cmd_option move_all_schema_options[] = {
    BU_CMD_FLAG("n", NULL, struct move_all_args, no_change,
	"Report changes without modifying the database"),
    BU_CMD_FILE("f", NULL, struct move_all_args, mapping_file, "file",
	"Read old/new name mappings from a file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand move_all_schema_operands[] = {
    BU_CMD_OPERAND("source_object", BU_CMD_VALUE_DB_OBJECT, 0, 1,
	"Existing object to rename", "ged.db_object"),
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 0, 1,
	"New object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const char *move_all_schema_file_option[] = {"f", NULL};
static const struct bu_cmd_constraint move_all_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT,
	move_all_schema_file_option, 0, 0,
	"-f cannot be combined with old/new operands"),
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_NO_OPTION_PRESENT,
	move_all_schema_file_option, 2, 2,
	"source and destination object names are required without -f"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema move_all_cmd_schema = {
    "move_all", "Rename an object and all references", move_all_schema_options,
    move_all_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, move_all_schema_constraints)
};
static const struct bu_cmd_schema mvall_cmd_schema = {
    "mvall", "Rename an object and all references", move_all_schema_options,
    move_all_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, move_all_schema_constraints)
};

static int
move_all_func(struct ged *gedp, int nflag, const char *old_name, const char *new_name)
{
    struct display_list *gdlp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_ptbl stack;
    size_t moved = 0;

    /* check the old_name source and new_name target */

    /* The destination is an object name, not a path - reject slashes so
     * we don't end up creating an object whose name contains the path
     * separator (which produces broken, unlistable hierarchies).
     */
    if (strchr(new_name, '/') != NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: destination name may not contain slashes", new_name);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, old_name, LOOKUP_NOISY);

    if (dp && db_lookup(gedp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists", new_name);
	return BRLCAD_ERROR;
    }

    /* if this was a sketch, we need to look for all the extrude
     * objects that might use it.
     *
     * This has to be done here, before we rename the (possible)
     * sketch object because the extrude will do a rt_db_get on the
     * sketch when we call rt_db_get_internal on it.
     */
    if (dp
	&& dp->d_major_type == DB5_MAJORTYPE_BRLCAD
	&& dp->d_minor_type == DB5_MINORTYPE_BRLCAD_SKETCH)
    {
	struct directory *dirp;

	FOR_ALL_DIRECTORY_START(dirp, gedp->dbip)
		struct rt_extrude_internal *extrude;

		if (dirp->d_major_type != DB5_MAJORTYPE_BRLCAD || \
		    dirp->d_minor_type != DB5_MINORTYPE_BRLCAD_EXTRUDE) {
		    continue;
		}

		if (rt_db_get_internal(&intern, dirp, gedp->dbip, (fastf_t *)NULL) < 0) {
		    bu_log("WARNING: Can't get extrude %s?\n", dirp->d_namep);
		    continue;
		}

		extrude = (struct rt_extrude_internal *)intern.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extrude);

		if (!BU_STR_EQUAL(extrude->sketch_name, old_name)) {
		    rt_db_free_internal(&intern);
		    continue;
		}

		if (nflag) {
		    bu_vls_printf(gedp->ged_result_str, "%s ", dirp->d_namep);
		    rt_db_free_internal(&intern);
		    continue;
		}

		bu_free(extrude->sketch_name, "sketch name");
		extrude->sketch_name = bu_strdup(new_name);

		if (rt_db_put_internal(dirp, gedp->dbip, &intern) < 0) {
		    bu_log("INTERNAL ERROR: unable to write sketch [%s] during mvall\n", new_name);
		} else {
		    moved++;
		}
		rt_db_free_internal(&intern);
	FOR_ALL_DIRECTORY_END;
    }

    if (!nflag && dp) {
	/* Change object name in the directory. */
	if (db_rename(gedp->dbip, dp, new_name) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "error in rename to %s, aborting", new_name);
	    return BRLCAD_ERROR;
	}

	/* Change object name on disk */
	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	    return BRLCAD_ERROR;
	}

	if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	    return BRLCAD_ERROR;
	}
	moved++;
    }

    bu_ptbl_init(&stack, 64, "combination stack for wdb_mvall_cmd");

    /* Examine all COMB nodes */
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	    if (nflag) {
		union tree *comb_leaf;

		if (!(dp->d_flags & RT_DIR_COMB))
		    continue;
		if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0)
		    continue;

		comb = (struct rt_comb_internal *)intern.idb_ptr;
		bu_ptbl_reset(&stack);

		/* visit each leaf in the combination */
		comb_leaf = comb->tree;
		if (comb_leaf) {
		    while (1) {
			while (comb_leaf->tr_op != OP_DB_LEAF) {
			    bu_ptbl_ins(&stack, (long *)comb_leaf);
			    comb_leaf = comb_leaf->tr_b.tb_left;
			}

			if (BU_STR_EQUAL(comb_leaf->tr_l.tl_name, old_name)) {
			    bu_vls_printf(gedp->ged_result_str, "%s ", dp->d_namep);
			}

			if (BU_PTBL_LEN(&stack) < 1) {
			    break;
			}
			comb_leaf = (union tree *)BU_PTBL_GET(&stack, BU_PTBL_LEN(&stack)-1);
			if (comb_leaf->tr_op != OP_DB_LEAF) {
			    bu_ptbl_rm(&stack, (long *)comb_leaf);
			    comb_leaf = comb_leaf->tr_b.tb_right;
			}
		    }
		}
		rt_db_free_internal(&intern);
	    } else {
		int comb_mvall_status = db_comb_mvall(dp, gedp->dbip, old_name, new_name, &stack);
		if (!comb_mvall_status)
		    continue;
		if (comb_mvall_status == 2) {
		    bu_ptbl_free(&stack);
		    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
		    return BRLCAD_ERROR;
		}
	    }
	    moved++;
    FOR_ALL_DIRECTORY_END;

    bu_ptbl_free(&stack);

    if (!nflag) {
	/* Change object name anywhere in the display list path. */
	for (BU_LIST_FOR(gdlp, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
	    int first = 1;
	    int found = 0;
	    struct bu_vls new_path = BU_VLS_INIT_ZERO;
	    char *dupstr = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	    char *tok = strtok(dupstr, "/");

	    while (tok) {
		if (BU_STR_EQUAL(tok, old_name)) {
		    found = 1;

		    if (first) {
			first = 0;
			bu_vls_printf(&new_path, "%s", new_name);
		    } else
			bu_vls_printf(&new_path, "/%s", new_name);
		} else {
		    if (first) {
			first = 0;
			bu_vls_printf(&new_path, "%s", tok);
		    } else
			bu_vls_printf(&new_path, "/%s", tok);
		}

		tok = strtok((char *)NULL, "/");
	    }

	    if (found) {
		bu_vls_free(&gdlp->dl_path);
		bu_vls_printf(&gdlp->dl_path, "%s", bu_vls_addr(&new_path));
	    }

	    free((void *)dupstr);
	    bu_vls_free(&new_path);
	}
    }

    if (!moved) {
	bu_log("ERROR: move %s to %s: no such object or reference\n", old_name, new_name);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
move_all_file(struct ged *gedp, int nflag, const char *file)
{
    FILE *fp = NULL;
    char line[512];

    fp = fopen(file, "r");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "cannot open %s\n", file);
	return BRLCAD_ERROR;
    }

    while (bu_fgets(line, sizeof(line), fp) != NULL) {
	char *cp = NULL;
	char *new_av[3] = {NULL};

	/* Skip comments */
	if ((cp = strchr(line, '#')) != NULL)
	    *cp = '\0';

	if (bu_argv_from_string(new_av, 2, line) != 2)
	    continue;

	move_all_func(gedp, nflag, (const char *)new_av[0], (const char *)new_av[1]);
    }

    fclose(fp);

    return BRLCAD_OK;
}


int
ged_move_all_core(struct ged *gedp, int argc, const char *argv[])
{
    const struct bu_cmd_schema *schema = NULL;
    struct move_all_args args = {0, NULL};
    int operand_index = 0;
    int object_count = 0;
    const char **objects = NULL;
    static const char *usage = "[-n] {-f <mapping_file>|<from> <to>}";

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

    schema = BU_STR_EQUAL(argv[0], "mvall") ? &mvall_cmd_schema : &move_all_cmd_schema;
    operand_index = bu_cmd_schema_parse_complete(schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    object_count = argc - 1 - operand_index;
    objects = argv + 1 + operand_index;
    if (args.mapping_file)
	return move_all_file(gedp, args.no_change, args.mapping_file);

    if (object_count != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    if (db_version(gedp->dbip) < 5 && (int)strlen(objects[1]) > NAMESIZE) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: name length limited to %zu characters in v4 databases\n", strlen(objects[1]));
	return BRLCAD_ERROR;
    }

    return move_all_func(gedp, args.no_change, objects[0], objects[1]);
}


#define GED_MOVE_ALL_COMMANDS(X, XID) \
    X(move_all, ged_move_all_core, GED_CMD_DEFAULT, &move_all_cmd_schema) \
    X(mvall, ged_move_all_core, GED_CMD_DEFAULT, &mvall_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MOVE_ALL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_move_all", 1, GED_MOVE_ALL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
