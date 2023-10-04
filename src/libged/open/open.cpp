/*                       O P E N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
#include "bg/lod.h"
#include "bu/opt.h"

#include "../ged_private.h"


extern "C" int
ged_opendb_core(struct ged *gedp, int argc, const char *argv[])
{
    int flip_endian = 0;
    int force_create = 0;
    int print_help = 0;
    const char *cmdname = argv[0];
    static const char *usage = "[options] [filename]\n";

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get database filename */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", gedp->dbip->dbi_filename);
	return BRLCAD_OK;
    }

    if (!BU_STR_EQUAL(cmdname, "reopen")) {
	struct bu_opt_desc d[4];
	BU_OPT(d[0], "c", "create",  "",  NULL,  &force_create,  "Creates a new database if not already extant.");
	BU_OPT(d[1], "f", "flip-endian",  "",  NULL,  &flip_endian,  "Opens file as a binary-incompatible v4 geometry database.");
	BU_OPT(d[2], "h", "help",  "",  NULL,  &print_help,  "Print help.");
	BU_OPT_NULL(d[3]);

	// We know we're the opendb command - start processing args
	argc--; argv++;

	int ac = bu_opt_parse(NULL, argc, argv, d);
	argc = ac;

	if (argc != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", cmdname);
	    _ged_cmd_help(gedp, usage, d);
	    return BRLCAD_ERROR;
	}
    } else {
	// If we're doing reopen, the options aren't active
	bu_vls_printf(gedp->ged_result_str, "%s filename", cmdname);
	return BRLCAD_ERROR;
    }

    /* Before proceeding with the full open logic, see if
     * we can actually open what the caller provided */
    struct db_i *new_dbip = NULL;
    struct mater *old_materp = rt_material_head();
    int existing_only = !force_create;
    if ((new_dbip = _ged_open_dbip(argv[0], existing_only)) == DBI_NULL) {

	/* Restore RT's material head */
	rt_new_material_head(old_materp);

	bu_vls_printf(gedp->ged_result_str, "ged_opendb_core: failed to open %s\n", argv[0]);

	// If the caller has work to do after opendb call, trigger it - even
	// though the open in its current form can't succeed, the caller may
	// want to try again
	if (gedp->ged_post_opendb_callback)
	    (*gedp->ged_post_opendb_callback)(gedp, gedp->ged_db_callback_udata);

	return BRLCAD_ERROR;
    }

    /* Handle forced endian flipping, if needed and requested */
    if (flip_endian) {
	if (db_version(new_dbip) != 4) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: [%s] is not a v4 database.  The -f option will be ignored.\n", argv[0]);
	} else {
	    if (new_dbip->dbi_version < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database [%s] was already (perhaps automatically) flipped, -f is redundant.\n", argv[0]);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "Treating [%s] as a binary-incompatible v4 geometry database.\n", argv[0]);
		bu_vls_printf(gedp->ged_result_str, "Endianness flipped.  Converting to READ ONLY.\n");
		/* flip the version number to indicate a flipped database. */
		new_dbip->dbi_version *= -1;
		/* do NOT write to a flipped database */
		new_dbip->dbi_read_only = 1;
	    }
	}
    }

    /* Close current database, if we have one */
    if (gedp->dbip) {
	const char *av[2];
	av[0] = "closedb";
	av[1] = (char *)0;
	ged_exec(gedp, 1, (const char **)av);
    }

    /* Set up the new database info in gedp */
    gedp->dbip = new_dbip;
    rt_new_material_head(rt_material_head());

    /* New database open, need to initialize reference counts */
    db_update_nref(gedp->dbip, &rt_uniresource);

    // LoD context creation (DbiState initialization can use info
    // stored here, so do this first)
    gedp->ged_lod = bg_mesh_lod_context_create(argv[0]);

    // If enabled, set up the DbiState container for fast structure access
    const char *use_dbi_state = getenv("LIBGED_DBI_STATE");
    if (use_dbi_state)
	gedp->dbi_state = new DbiState(gedp);

    // Set the view units, if we have a view
    if (gedp->ged_gvp) {
	gedp->ged_gvp->gv_base2local = gedp->dbip->dbi_base2local;
	gedp->ged_gvp->gv_local2base = gedp->dbip->dbi_local2base;
    }

    // If the caller has work to do after open, trigger it
    if (gedp->ged_post_opendb_callback)
	(*gedp->ged_post_opendb_callback)(gedp, gedp->ged_db_callback_udata);

    return BRLCAD_OK;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl reopen_cmd_impl = {"reopen", ged_opendb_core, GED_CMD_DEFAULT};
const struct ged_cmd reopen_cmd = { &reopen_cmd_impl };

struct ged_cmd_impl opendb_cmd_impl = {"opendb", ged_opendb_core, GED_CMD_DEFAULT};
const struct ged_cmd opendb_cmd = { &opendb_cmd_impl };

struct ged_cmd_impl open_cmd_impl = {"open", ged_opendb_core, GED_CMD_DEFAULT};
const struct ged_cmd open_cmd = { &open_cmd_impl };

const struct ged_cmd *open_cmds[] = { &reopen_cmd, &opendb_cmd, &open_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  open_cmds, 3 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

