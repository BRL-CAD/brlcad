/*                           F _ D B . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2023 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/f_db.c
 *
 * Multiple-display Graphics EDitor (MGED) specific wrappers around
 * opendb/closedb commands.
 *
 */

#include "common.h"
#include <string.h>
#include "bu/getopt.h"
#include "bu/units.h"
#include "ged.h"
#include "tclcad.h"

/* private */
#include "./cmd.h"
#include "./mged.h"

/* defined in chgmodel.c */
extern void set_localunit_TclVar(void);

static void
mged_output_handler(struct ged *UNUSED(gp), char *line)
{
    if (line)
	bu_log("%s", line);
}

static void
mged_refresh_handler(void *UNUSED(clientdata))
{
    view_state->vs_flag = 1;
    refresh();
}

/**
 * Close the current database, if open, and then open a new database.
 * May also open a display manager, if interactive and none selected
 * yet.
 *
 * argv[1] is the filename.
 *
 * argv[2] is optional 'y' or 'n' indicating whether to create the
 * database if it does not exist.
 *
 * Returns TCL_OK if database was opened, TCL_ERROR if database was
 * NOT opened (and the user didn't abort).
 */
int
f_opendb(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct db_i *save_dbip = DBI_NULL;
    struct mater *save_materp = MATER_NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;	/* use this to hold returned message */
    int created_new_db = 0;
    int c;
    int flip_v4 = 0;

    if (argc <= 1) {
	/* Invoked without args, return name of current database */

	if (DBIP != DBI_NULL) {
	    Tcl_AppendResult(interpreter, DBIP->dbi_filename, (char *)NULL);
	    return TCL_OK;
	}

	Tcl_AppendResult(interpreter, "", (char *)NULL);
	return TCL_OK;
    }

    /* handle getopt arguments */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "f")) != -1) {
	switch (c) {
	    case 'f':
		flip_v4=1;
		break;
	}
    }
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* validate arguments */
    if (argc > 3
	|| (argc == 2
	    && strlen(argv[1]) == 0)
	|| (argc == 3
	    && !BU_STR_EQUAL("y", argv[2])
	    && !BU_STR_EQUAL("Y", argv[2])
	    && !BU_STR_EQUAL("n", argv[2])
	    && !BU_STR_EQUAL("N", argv[2])))
    {
	Tcl_Eval(interpreter, "help opendb");
	return TCL_ERROR;
    }

    save_dbip = DBIP;
    DBIP = DBI_NULL;
    save_materp = rt_material_head();
    rt_new_material_head(MATER_NULL);

    /* Get input file */
    if (((DBIP = db_open(argv[1], DB_OPEN_READWRITE)) == DBI_NULL) &&
	((DBIP = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL)) {
	char line[128];

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(argv[1], NULL)) {
	    /* need to reset things before returning */
	    DBIP = save_dbip;
	    rt_new_material_head(save_materp);

	    if (!bu_file_readable(argv[1])) {
		bu_log("ERROR: Unable to read from %s\n", argv[1]);
		bu_vls_free(&msg);
		return TCL_ERROR;
	    }

	    bu_log("ERROR: Unable to open %s as geometry database file\n", argv[1]);
	    bu_vls_free(&msg);
	    return TCL_ERROR;
	}

	/* File does not exist */
	if (interactive && argc < 3) {
	    if (mged_init_flag) {
		if (classic_mged) {
		    bu_log("Create new database (y|n)[n]? ");
		    (void)bu_fgets(line, sizeof(line), stdin);
		    if (bu_str_false(line)) {
			bu_log("Warning: no database is currently open!\n");
			bu_vls_free(&msg);
			return TCL_OK;
		    }
		} else {
		    struct bu_vls vls = BU_VLS_INIT_ZERO;
		    int status;

		    if (dpy_string != (char *)NULL)
			bu_vls_printf(&vls, "cad_dialog .createdb %s \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
				      dpy_string, argv[1]);
		    else
			bu_vls_printf(&vls, "cad_dialog .createdb :0 \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
				      argv[1]);

		    status = Tcl_Eval(interpreter, bu_vls_addr(&vls));

		    bu_vls_free(&vls);

		    if (status != TCL_OK || Tcl_GetStringResult(interpreter)[0] == '2') {
			bu_vls_free(&msg);
			return TCL_ERROR;
		    }

		    if (Tcl_GetStringResult(interpreter)[0] == '1') {
			bu_log("opendb: no database is currently opened!\n");
			bu_vls_free(&msg);
			return TCL_OK;
		    }
		} /* classic */
	    } else {
		/* not initializing mged */
		if (argc == 2) {
		    /* need to reset this before returning */
		    DBIP = save_dbip;
		    rt_new_material_head(save_materp);
		    Tcl_AppendResult(interpreter, MORE_ARGS_STR, "Create new database (y|n)[n]? ",
				     (char *)NULL);
		    bu_vls_printf(&curr_cmd_list->cl_more_default, "n");
		    bu_vls_free(&msg);
		    return TCL_ERROR;
		}

	    }
	}

	/* did the caller specify not creating a new database? */
	if (argc >= 3 && bu_str_false(argv[2])) {
	    DBIP = save_dbip; /* restore previous database */
	    rt_new_material_head(save_materp);
	    bu_vls_free(&msg);
	    return TCL_OK;
	}

	/* File does not exist, and should be created */
	if ((DBIP = db_create(argv[1], mged_db_version)) == DBI_NULL) {
	    DBIP = save_dbip; /* restore previous database */
	    rt_new_material_head(save_materp);
	    bu_vls_free(&msg);

	    if (mged_init_flag) {
		/* we need to use bu_log here */
		bu_log("opendb: failed to create %s\n", argv[1]);
		bu_log("opendb: no database is currently opened!\n");
		return TCL_OK;
	    }

	    Tcl_AppendResult(interpreter, "opendb: failed to create ", argv[1], "\n", (char *)NULL);
	    if (DBIP == DBI_NULL)
		Tcl_AppendResult(interpreter, "opendb: no database is currently opened!", (char *)NULL);

	    return TCL_ERROR;
	}
	/* New database has already had db_dirbuild() by here */

	created_new_db = 1;
	bu_vls_printf(&msg, "The new database %s was successfully created.\n", argv[1]);
    } else {
	/* Opened existing database file */

	/* Version indicates whether we have a valid .g file */
	if (db_version(DBIP) < 0) {
	    bu_free(DBIP->dbi_filename, "free filename");
	    DBIP = DBI_NULL;
	    Tcl_AppendResult(interpreter, "opendb:  ", argv[1], " is not a valid database\n", (char *)NULL);
	    return TCL_ERROR;
	}

	/* Scan geometry database and build in-memory directory */
	(void)db_dirbuild(DBIP);
    }

    /* close out the old dbip */
    if (save_dbip) {
	struct db_i *new_dbip;
	struct mater *new_materp;

	new_dbip = DBIP;
	new_materp = rt_material_head();

	/* activate the 'saved' values so we can cleanly close the previous db */
	DBIP = save_dbip;
	rt_new_material_head(save_materp);

	/* bye bye db */
	f_closedb(clientData, interpreter, 1, NULL);

	/* restore to the new db just opened */
	DBIP = new_dbip;
	rt_new_material_head(new_materp);
    }

    if (flip_v4) {
	if (db_version(DBIP) != 4) {
	    bu_log("WARNING: [%s] is not a v4 database.  The -f option will be ignored.\n", DBIP->dbi_filename);
	} else {
	    if (DBIP->dbi_version < 0) {
		bu_log("Database [%s] was already (perhaps automatically) flipped, -f is redundant.\n", DBIP->dbi_filename);
	    } else {
		bu_log("Treating [%s] as a binary-incompatible v4 geometry database.\n", DBIP->dbi_filename);
		bu_log("Endianness flipped.  Converting to READ ONLY.\n");

		/* flip the version number to indicate a flipped database. */
		DBIP->dbi_version *= -1;

		/* do NOT write to a flipped database */
		DBIP->dbi_read_only = 1;
	    }
	}
    }

    if (DBIP->dbi_read_only) {
	bu_vls_printf(&msg, "%s: READ ONLY\n", DBIP->dbi_filename);
    }

    /* This must occur before the call to [get_dbip] since
     * that hooks into a libged callback.
     */
    GEDP->dbip = DBIP;
    GEDP->ged_output_handler = mged_output_handler;
    GEDP->ged_refresh_handler = mged_refresh_handler;
    GEDP->ged_create_vlist_scene_obj_callback = createDListSolid;
    GEDP->ged_create_vlist_display_list_callback = createDListAll;
    GEDP->ged_destroy_vlist_callback = freeDListsAll;
    GEDP->ged_create_io_handler = &tclcad_create_io_handler;
    GEDP->ged_delete_io_handler = &tclcad_delete_io_handler;
    GEDP->ged_interp = (void *)interpreter;
    GEDP->ged_interp_eval = &mged_db_search_callback;
    struct tclcad_io_data *t_iod = tclcad_create_io_data();
    t_iod->io_mode = TCL_READABLE;
    t_iod->interp = interpreter;
    GEDP->ged_io_data = t_iod;

    /* increment use count for gedp db instance */
    (void)db_clone_dbi(DBIP, NULL);

    /* Provide LIBWDB C access to the on-disk database */
    if ((WDBP = wdb_dbopen(DBIP, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	Tcl_AppendResult(interpreter, "wdb_dbopen() failed?\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* increment use count for tcl db instance */
    (void)db_clone_dbi(DBIP, NULL);

    /* Establish LIBWDB TCL access to both disk and in-memory databases */

    /* initialize rt_wdb */
    bu_vls_init(&WDBP->wdb_name);
    bu_vls_strcpy(&WDBP->wdb_name, MGED_DB_NAME);

    WDBP->wdb_interp = interpreter;

    /* append to list of rt_wdb's */
    BU_LIST_APPEND(&RTG.rtg_headwdb.l, &WDBP->l);

    /* This creates a "db" command object */

    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand((Tcl_Interp *)WDBP->wdb_interp, MGED_DB_NAME, (Tcl_CmdProc *)wdb_cmd, (ClientData)WDBP, wdb_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult((Tcl_Interp *)WDBP->wdb_interp, MGED_DB_NAME, (char *)NULL);

    /* This creates the ".inmem" in-memory geometry container and sets
     * up the GUI.
     */
    {
	struct bu_vls cmd = BU_VLS_INIT_ZERO;

	bu_vls_printf(&cmd, "wdb_open %s inmem [get_dbip]", MGED_INMEM_NAME);
	if (Tcl_Eval(interpreter, bu_vls_addr(&cmd)) != TCL_OK) {
	    bu_vls_printf(&msg, "%s\n%s\n", Tcl_GetStringResult(interpreter), Tcl_GetVar(interpreter, "errorInfo", TCL_GLOBAL_ONLY));
	    Tcl_AppendResult(interpreter, bu_vls_addr(&msg), (char *)NULL);
	    bu_vls_free(&msg);
	    bu_vls_free(&cmd);
	    return TCL_ERROR;
	}

	/* Perhaps do something special with the GUI */
	bu_vls_trunc(&cmd, 0);
	bu_vls_printf(&cmd, "opendb_callback {%s}", DBIP->dbi_filename);
	(void)Tcl_Eval(interpreter, bu_vls_addr(&cmd));

	bu_vls_strcpy(&cmd, "local2base");
	Tcl_UnlinkVar(interpreter, bu_vls_addr(&cmd));
	Tcl_LinkVar(interpreter, bu_vls_addr(&cmd), (char *)&local2base, TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	bu_vls_strcpy(&cmd, "base2local");
	Tcl_UnlinkVar(interpreter, bu_vls_addr(&cmd));
	Tcl_LinkVar(interpreter, bu_vls_addr(&cmd), (char *)&base2local, TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	bu_vls_free(&cmd);
    }

    set_localunit_TclVar();

    /* Print title/units information */
    if (interactive) {
	bu_vls_printf(&msg, "%s (units=%s)\n", DBIP->dbi_title,
		      bu_units_string(DBIP->dbi_local2base));
    }

    /*
     * We have an old database version AND we're not in the process of
     * creating a new database.
     */
    if (db_version(DBIP) < 5 && !created_new_db) {
	if (mged_db_upgrade) {
	    if (mged_db_warn)
		bu_vls_printf(&msg, "Warning:\n\tDatabase version is old.\n\tConverting to the new format.\n");

	    (void)Tcl_Eval(interpreter, "after idle dbupgrade -f y");
	} else {
	    if (mged_db_warn) {
		if (classic_mged)
		    bu_vls_printf(&msg, "Warning:\n\tDatabase version is old.\n\tSee the dbupgrade command.");
		else
		    bu_vls_printf(&msg, "Warning:\n\tDatabase version is old.\n\tSelect Tools-->Upgrade Database for info.");
	    }
	}
    }

    Tcl_ResetResult(interpreter);
    Tcl_AppendResult(interpreter, bu_vls_addr(&msg), (char *)NULL);
    bu_vls_free(&msg);

    /* Update the background colors now that we have a file open */
    cs_set_bg(NULL, NULL, NULL, NULL, NULL);

    return TCL_OK;
}


/**
 * Close the current database, if open.
 */
int
f_closedb(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char *av[2];

    if (argc != 1) {
	Tcl_AppendResult(interpreter, "Unexpected argument [%s]\n", (const char *)argv[1], NULL);
	Tcl_Eval(interpreter, "help closedb");
	return TCL_ERROR;
    }

    if (DBIP == DBI_NULL) {
	Tcl_AppendResult(interpreter, "No database is open\n", NULL);
	return TCL_OK;
    }

    /* Clear out anything in the display */
    av[0] = "zap";
    av[1] = NULL;
    cmd_zap(clientData, interpreter, 1, av);

    /* Close the Tcl database objects */
    Tcl_Eval(interpreter, "rename " MGED_DB_NAME " \"\"; rename .inmem \"\"");

    /* close the geometry instance */
    db_close(GEDP->dbip);
    GEDP->dbip = NULL;

    WDBP = RT_WDB_NULL;
    DBIP = DBI_NULL;

    /* wipe out the material list */
    rt_new_material_head(MATER_NULL);

    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
