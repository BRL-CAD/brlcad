/*                       O P E N . C P P
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
/** @file libged/open.cpp
 *
 * The open command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bv/lod.h"
#include "../../librt/librt_private.h"

#include "../ged_private.h"

#include "../dbi.h"

struct open_args {
    int create_requested;
    int flip_endian;
    int help;
    int open_only;
};

static const struct bu_cmd_option open_schema_options[] = {
    BU_CMD_FLAG("c", "create", struct open_args, create_requested,
	"Create a new database if the file does not already exist"),
    BU_CMD_FLAG("f", "flip-endian", struct open_args, flip_endian,
	"Open a binary-incompatible v4 geometry database"),
    BU_CMD_FLAG("h", "help", struct open_args, help,
	"Print command usage"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("o", "open", struct open_args, open_only,
	"Require an existing database file unless --create is supplied"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand open_operands[] = {
    BU_CMD_OPERAND("filename", BU_CMD_VALUE_FILE, 1, 1,
	"Database filename", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema open_cmd_schema = {
    "open", "Open a geometry database", open_schema_options, open_operands,
    BU_CMD_PARSE_INTERSPERSED, {NULL}
};
static const struct bu_cmd_schema opendb_cmd_schema = {
    "opendb", "Open a geometry database", open_schema_options, open_operands,
    BU_CMD_PARSE_INTERSPERSED, {NULL}
};
static const struct bu_cmd_schema reopen_cmd_schema = {
    "reopen", "Reopen a geometry database", NULL, open_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
open_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&open_cmd_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] filename", command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "open native option help");
    }
}


static void
reopen_show_help(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: reopen filename");
}

extern "C" int
ged_opendb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct open_args args = {0, 0, 0, 0};
    int operand_index;
    int operand_count;
    const char **operands;
    int is_reopen;
    const char *cmdname = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get database filename */
    if (argc == 1) {
	if (!gedp->dbip) {
	    bu_vls_printf(gedp->ged_result_str, "No database currently open.");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s", gedp->dbip->dbi_filename);
	}
	return BRLCAD_OK;
    }


    is_reopen = BU_STR_EQUAL(cmdname, "reopen");
    if (!is_reopen) {
	operand_index = bu_cmd_schema_parse(&open_cmd_schema, &args,
	    gedp->ged_result_str, argc - 1, argv + 1);
	if (operand_index < 0) {
	    open_show_help(gedp, cmdname);
	    return BRLCAD_ERROR;
	}
	if (args.help) {
	    open_show_help(gedp, cmdname);
	    return GED_HELP;
	}
	struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
	if (bu_cmd_schema_validate(&open_cmd_schema, (size_t)(argc - 1), argv + 1,
		(size_t)(argc - 1), &validation) != 0 ||
		validation.state != BU_CMD_VALIDATE_VALID) {
	    if (validation.hint)
		bu_vls_printf(gedp->ged_result_str, "%s\n", validation.hint);
	    bu_cmd_validate_result_clear(&validation);
	    open_show_help(gedp, cmdname);
	    return BRLCAD_ERROR;
	}
	bu_cmd_validate_result_clear(&validation);
    } else {
	operand_index = bu_cmd_schema_parse_complete(&reopen_cmd_schema, NULL,
	    gedp->ged_result_str, argc - 1, argv + 1);
	if (operand_index < 0) {
	    reopen_show_help(gedp);
	    return BRLCAD_ERROR;
	}
    }

    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;

    /* Before proceeding with the full open logic, see if
     * we can actually open what the caller provided */
    struct db_i *new_dbip = NULL;
    int existing_only = is_reopen || (args.open_only && !args.create_requested);
    if ((new_dbip = _ged_open_dbip(operands[0], existing_only)) == DBI_NULL) {

	bu_vls_printf(gedp->ged_result_str, "ged_opendb_core: failed to open %s\n", operands[0]);

	// If the caller has work to do after opendb call, trigger it - even
	// though the open in its current form can't succeed, the caller may
	// want to try again.  Doing this here rather than the normal ged_exec
	// post cmd callback because in this specific case we're returning an
	// error AND still wanting to execute the post-cmd hook - that's
	// non-standard behavior for ged_exec.
	bu_clbk_t opendb_clbk = NULL;
	void *opendb_clbk_data = NULL;
	ged_clbk_get(&opendb_clbk, &opendb_clbk_data, gedp, "opendb", BU_CLBK_POST);
	if (opendb_clbk)
	    (*opendb_clbk)(operand_count, operands, (void *)gedp, opendb_clbk_data);

	return BRLCAD_ERROR;
    }

    /* Handle forced endian flipping, if needed and requested */
    if (args.flip_endian) {
	if (db_version(new_dbip) != 4) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: [%s] is not a v4 database.  The -f option will be ignored.\n", operands[0]);
	} else {
	    if (new_dbip->i->dbi_version < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database [%s] was already (perhaps automatically) flipped, -f is redundant.\n", operands[0]);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "Treating [%s] as a binary-incompatible v4 geometry database.\n", operands[0]);
		bu_vls_printf(gedp->ged_result_str, "Endianness flipped.  Converting to READ ONLY.\n");
		/* flip the version number to indicate a flipped database. */
		new_dbip->i->dbi_version *= -1;
		/* do NOT write to a flipped database */
		new_dbip->dbi_read_only = 1;
	    }
	}
    }

    /* Close current database, if we have one */
    if (gedp->dbip) {
	const char *av[1] = {"closedb"};
	ged_exec_closedb(gedp, 1, (const char **)av);
    }

    /* Set up the new database info in gedp */
    gedp->dbip = new_dbip;

    // LoD context creation (DbiState initialization can use info
    // stored here, so do this first)
    if (gedp->new_cmd_forms)
	gedp->ged_lod = bv_mesh_lod_context_create(operands[0]);

    // If enabled, set up the DbiState container for fast structure access
    if (gedp->new_cmd_forms)
	gedp->dbi_state = new DbiState(gedp);

    // Set the view units, if we have a view
    if (gedp->ged_gvp) {
	gedp->ged_gvp->gv_base2local = gedp->dbip->dbi_base2local;
	gedp->ged_gvp->gv_local2base = gedp->dbip->dbi_local2base;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_OPEN_COMMANDS(X, XID) \
    X(reopen, ged_opendb_core, GED_CMD_DEFAULT, &reopen_cmd_schema) \
    X(opendb, ged_opendb_core, GED_CMD_DEFAULT, &opendb_cmd_schema) \
    X(open, ged_opendb_core, GED_CMD_DEFAULT, &open_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_OPEN_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_open", 1, GED_OPEN_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
