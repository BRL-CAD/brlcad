/*                         K E E P . C
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
/** @file libged/keep.c
 *
 * The keep command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"

#include "../ged_private.h"


struct keep_node_data {
    struct rt_wdb *wdbp;
    struct ged *gedp;
};


struct keep_args {
    int named_objects_only;
};


#include "../include/plugin.h"

static const struct bu_cmd_option keep_schema_options[] = {
    BU_CMD_FLAG("R", NULL, struct keep_args, named_objects_only,
	"Keep only named objects, without descendants"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand keep_schema_operands[] = {
    BU_CMD_OPERAND("file", BU_CMD_VALUE_FILE, 1, 1, "Output database file", "ged.file_path"),
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_STRING, 1, BU_CMD_COUNT_UNLIMITED,
	"Database objects to copy", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema keep_cmd_schema = {
    "keep", "Copy selected objects into another database", keep_schema_options,
    keep_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    {NULL}
};


/*
 * Supports for the 'keep' method.
 * Write each node encountered exactly once.
 */
static void
node_write(struct db_i *dbip, struct directory *dp, void *ptr)
{
    struct keep_node_data *kndp = (struct keep_node_data *)ptr;
    struct rt_db_internal intern;

    RT_CK_WDB(kndp->wdbp);

    if (dp->d_nref++ > 0)
	return;		/* already written */

    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	bu_vls_printf(kndp->gedp->ged_result_str, "Database read error, aborting\n");
	return;
    }

    if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	/* if this is an extrusion, keep the referenced sketch */
	struct rt_extrude_internal *extr;
	struct directory *dp2;

	extr = (struct rt_extrude_internal *)intern.idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extr);

	if ((dp2 = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	    node_write(dbip, dp2, ptr);
	}
    } else if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	/* if this is a revolve, keep the referenced sketch */
	struct rt_revolve_internal *rev;
	struct directory *dp2;

	rev = (struct rt_revolve_internal *)intern.idb_ptr;
	RT_REVOLVE_CK_MAGIC(rev);

	if ((dp2 = db_lookup(dbip, bu_vls_addr(&rev->sketch_name), LOOKUP_QUIET)) != RT_DIR_NULL) {
	    node_write(dbip, dp2, ptr);
	}
    } else if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	/* if this is a DSP, keep the referenced binary object too */
	struct rt_dsp_internal *dsp;
	struct directory *dp2;

	dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp);

	if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ) {
	    /* need to keep this object */
	    if ((dp2 = db_lookup(dbip, bu_vls_addr(&dsp->dsp_name),  LOOKUP_QUIET)) != RT_DIR_NULL) {
		node_write(dbip, dp2, ptr);
	    }
	}
    }

    if (wdb_put_internal(kndp->wdbp, dp->d_namep, &intern, 1.0) < 0) {
	bu_vls_printf(kndp->gedp->ged_result_str, "Database write error, aborting\n");
	return;
    }
}


int
ged_keep_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct keep_node_data knd;
    struct rt_wdb *keepfp;
    struct directory *dp;
    struct bu_vls title = BU_VLS_INIT_ZERO;
    struct db_i *new_dbip;
    const char *cmd = argv[0];
    static const char *usage = "[-R] file object(s)";
    struct keep_args args = {0};
    int operand_index = 0;
    int operand_count = 0;
    const char *file = NULL;
    const char **objects = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&keep_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: missing file or object names\n");
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    operand_count = argc - 1 - operand_index;
    file = argv[1 + operand_index];
    objects = argv + 2 + operand_index;

    /* First, clear any existing counts */
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	dp->d_nref = 0;
    FOR_ALL_DIRECTORY_END;

    /* Alert user if named file already exists */

    new_dbip = db_open(file, DB_OPEN_READWRITE);

    if (new_dbip != DBI_NULL) {
	if (db_version(new_dbip) != db_version(gedp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str, "%s: File format mismatch between '%s' and '%s'\n",
		  cmd, file, gedp->dbip->dbi_filename);
	    return BRLCAD_ERROR;
	}

	keepfp = wdb_dbopen(new_dbip, RT_WDB_TYPE_DB_DISK);
	if (keepfp == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s:  Error opening '%s'\n", cmd, file);
	    return BRLCAD_ERROR;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s:  Appending to '%s'\n", cmd, file);

	    /* --- Scan geometry database and build in-memory directory --- */
	    db_dirbuild(new_dbip);
	}
    } else {
	/* Create a new database */
	keepfp = wdb_fopen_v(file, db_version(gedp->dbip));

	if (keepfp == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s command was unable to create file '%s'\n", cmd, file);
	    return BRLCAD_ERROR;
	}
    }

    knd.wdbp = keepfp;
    knd.gedp = gedp;

    /* ident record */
    if (bu_strncmp(gedp->dbip->dbi_title, "Parts of: ", 10) != 0) {
	bu_vls_strcat(&title, "Parts of: ");
    }
    bu_vls_strcat(&title, gedp->dbip->dbi_title);

    if (db_update_ident(keepfp->dbip, bu_vls_addr(&title), gedp->dbip->dbi_local2base) < 0) {
	perror("fwrite");
	bu_vls_printf(gedp->ged_result_str, "db_update_ident() failed\n");
	if (new_dbip != DBI_NULL) {
	    db_close(new_dbip);
	} else {
	    db_close(keepfp->dbip);
	}
	bu_vls_free(&title);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&title);


    for (i = 0; i < operand_count - 1; i++) {
	if ((dp = db_lookup(gedp->dbip, objects[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	if (!args.named_objects_only) {
	    /* recursively keep objects */
	    db_treewalk_basic(gedp->dbip, dp, node_write, node_write, (void *)&knd);
	} else {
	    /* keep just this object */
	    node_write(gedp->dbip, dp, (void *)&knd);
	}
    }

    db_close(keepfp->dbip);

    return BRLCAD_OK;
}

#define GED_KEEP_COMMANDS(X, XID) \
    X(keep, ged_keep_core, GED_CMD_DEFAULT, &keep_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_KEEP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_keep", 1, GED_KEEP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
