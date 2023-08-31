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

/* Shorthand for open/close db callback function pointer type */
typedef void (*db_clbk_t )(struct ged *, void *);

void
mged_output_handler(struct ged *UNUSED(gp), char *line)
{
    if (line)
	bu_log("%s", line);
}

void
mged_refresh_handler(void *UNUSED(clientdata))
{
    view_state->vs_flag = 1;
    refresh();
}

static void
_post_opendb_failed(struct ged *gedp, struct mged_opendb_ctx *ctx)
{
    char line[128];
    int argc = ctx->argc;
    const char **argv = ctx->argv;
    const char *fname = argv[argc-1];
    /*
     * Check to see if we can access the database
     */
    if (bu_file_exists(fname, NULL)) {
	if (!bu_file_readable(fname)) {
	    bu_log("ERROR: Unable to read from %s\n", fname);
	    ctx->ret = TCL_ERROR;
	    return;
	}

	bu_log("ERROR: Unable to open %s as geometry database file\n", fname);
	ctx->ret = TCL_ERROR;
	return;
    }

    /* Did the caller specify not creating a new database? If so,
     * we're done */
    if (ctx->no_create) {
	ctx->ret = TCL_ERROR;
	return;
    }

    /* File does not exist, but nobody told us one way or the other
     * about creation - ask */
    if (interactive && !ctx->force_create) {
	if (mged_init_flag) {
	    if (classic_mged) {
		bu_log("Create new database (y|n)[n]? ");
		(void)bu_fgets(line, sizeof(line), stdin);
		if (bu_str_false(line)) {
		    bu_log("Warning: no database is currently open!\n");
		    ctx->ret = TCL_ERROR;
		    return;
		}
	    } else {
		struct bu_vls vls = BU_VLS_INIT_ZERO;
		int status;

		if (dpy_string != (char *)NULL)
		    bu_vls_printf(&vls, "cad_dialog .createdb %s \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
			    dpy_string, fname);
		else
		    bu_vls_printf(&vls, "cad_dialog .createdb :0 \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
			    fname);

		status = Tcl_Eval(ctx->interpreter, bu_vls_addr(&vls));

		bu_vls_free(&vls);

		if (status != TCL_OK || Tcl_GetStringResult(ctx->interpreter)[0] == '2') {
		    ctx->ret = TCL_ERROR;
		    return;
		}

		if (Tcl_GetStringResult(ctx->interpreter)[0] == '1') {
		    bu_log("opendb: no database is currently opened!\n");
		    ctx->ret = TCL_OK;
		    return;
		}
	    } /* classic */
	} else {
	    /* not initializing mged */
	    if (argc == 2) {
		/* need to reset this before returning */
		Tcl_AppendResult(ctx->interpreter, MORE_ARGS_STR, "Create new database (y|n)[n]? ",
			(char *)NULL);
		bu_vls_printf(&curr_cmd_list->cl_more_default, "n");
		ctx->ret = TCL_ERROR;
		return;
	    }
	}
    }

    if (ctx->post_open_cnt < 2) {
	const char *av[3];
	av[0] = "opendb";
	av[1] = "-c";
	av[2] = fname;
	ctx->ged_ret = ged_exec(gedp, 3, (const char **)av);
    }

    if (gedp->dbip == DBI_NULL) {
	ctx->ret = TCL_ERROR;

	if (mged_init_flag) {
	    /* we need to use bu_log here */
	    bu_log("opendb: failed to create %s\n", fname);
	    bu_log("opendb: no database is currently opened!\n");
	    return;
	}

	Tcl_AppendResult(ctx->interpreter, "opendb: failed to create ", fname, "\n", (char *)NULL);
	if (DBIP == DBI_NULL)
	    Tcl_AppendResult(ctx->interpreter, "opendb: no database is currently opened!", (char *)NULL);

	return;
    }

    ctx->created_new_db = 1;
    bu_vls_printf(gedp->ged_result_str, "The new database %s was successfully created.\n", fname);
}

void
mged_pre_opendb_clbk(struct ged *UNUSED(gedp), void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mctx->force_create = 0;
    mctx->no_create = 1;
    mctx->created_new_db = 0;
    mctx->ret = 0;
    mctx->ged_ret = 0;
}

void
mged_post_opendb_clbk(struct ged *gedp, void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mctx->post_open_cnt++;

    /* Sync global to GED results */
    DBIP = gedp->dbip;

    if (DBIP == DBI_NULL || mctx->old_dbip == gedp->dbip) {
	_post_opendb_failed(gedp, mctx);
	mctx->post_open_cnt--;
	return;
    }

    /* Opened database file */
    mctx->old_dbip = gedp->dbip;
    if (DBIP->dbi_read_only)
	bu_vls_printf(gedp->ged_result_str, "%s: READ ONLY\n", DBIP->dbi_filename);

    /* increment use count for gedp db instance */
    (void)db_clone_dbi(DBIP, NULL);

    /* Provide LIBWDB C access to the on-disk database */
    if ((WDBP = wdb_dbopen(DBIP, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	Tcl_AppendResult(mctx->interpreter, "wdb_dbopen() failed?\n", (char *)NULL);
	mctx->ret = TCL_ERROR;
	mctx->post_open_cnt--;
	return;
    }

    /* increment use count for tcl db instance */
    (void)db_clone_dbi(DBIP, NULL);

    /* Establish LIBWDB TCL access to both disk and in-memory databases */

    /* initialize rt_wdb */
    bu_vls_init(&WDBP->wdb_name);
    bu_vls_strcpy(&WDBP->wdb_name, MGED_DB_NAME);

    WDBP->wdb_interp = mctx->interpreter;

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

	// Stash the result string state prior to doing the following Tcl commands.
	// get_dbip in particular uses it...
	struct bu_vls tmp_gedr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tmp_gedr, "%s", bu_vls_cstr(gedp->ged_result_str));

	bu_vls_printf(&cmd, "wdb_open %s inmem [get_dbip]", MGED_INMEM_NAME);
	if (Tcl_Eval(mctx->interpreter, bu_vls_addr(&cmd)) != TCL_OK) {
	    bu_vls_sprintf(gedp->ged_result_str, "%s\n%s\n", Tcl_GetStringResult(mctx->interpreter), Tcl_GetVar(mctx->interpreter, "errorInfo", TCL_GLOBAL_ONLY));
	    Tcl_AppendResult(mctx->interpreter, bu_vls_addr(gedp->ged_result_str), (char *)NULL);
	    bu_vls_free(&cmd);
	    mctx->ret = TCL_ERROR;
	    return;
	}

	/* Perhaps do something special with the GUI */
	bu_vls_trunc(&cmd, 0);
	bu_vls_printf(&cmd, "opendb_callback {%s}", DBIP->dbi_filename);
	(void)Tcl_Eval(mctx->interpreter, bu_vls_addr(&cmd));

	bu_vls_strcpy(&cmd, "local2base");
	Tcl_UnlinkVar(mctx->interpreter, bu_vls_addr(&cmd));
	Tcl_LinkVar(mctx->interpreter, bu_vls_addr(&cmd), (char *)&local2base, TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	bu_vls_strcpy(&cmd, "base2local");
	Tcl_UnlinkVar(mctx->interpreter, bu_vls_addr(&cmd));
	Tcl_LinkVar(mctx->interpreter, bu_vls_addr(&cmd), (char *)&base2local, TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	// Restore the pre Tcl ged_result_str
	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&tmp_gedr));
	bu_vls_free(&tmp_gedr);

	bu_vls_free(&cmd);
    }

    set_localunit_TclVar();

    /* Print title/units information */
    if (interactive) {
	bu_vls_printf(gedp->ged_result_str, "%s (units=%s)\n", DBIP->dbi_title,
		      bu_units_string(DBIP->dbi_local2base));
    }

    /*
     * We have an old database version AND we're not in the process of
     * creating a new database.
     */
    if (db_version(DBIP) < 5 && !mctx->created_new_db) {
	if (mged_db_upgrade) {
	    if (mged_db_warn)
		bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tConverting to the new format.\n");

	    (void)Tcl_Eval(mctx->interpreter, "after idle dbupgrade -f y");
	} else {
	    if (mged_db_warn) {
		if (classic_mged)
		    bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tSee the dbupgrade command.");
		else
		    bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tSelect Tools-->Upgrade Database for info.");
	    }
	}
    }

    Tcl_ResetResult(mctx->interpreter);
    Tcl_AppendResult(mctx->interpreter, bu_vls_addr(gedp->ged_result_str), (char *)NULL);

    /* Update the background colors now that we have a file open */
    cs_set_bg(NULL, NULL, NULL, NULL, NULL);

    mctx->post_open_cnt--;
    mctx->ret = TCL_OK;
}

void
mged_pre_closedb_clbk(struct ged *UNUSED(gedp), void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;

   /* Close the Tcl database objects */
    Tcl_Eval(mctx->interpreter, "rename " MGED_DB_NAME " \"\"; rename .inmem \"\"");
}

void
mged_post_closedb_clbk(struct ged *UNUSED(gedp), void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mctx->old_dbip = NULL;
    WDBP = RT_WDB_NULL;
    DBIP = DBI_NULL;
}


/**
 * Close the current database, if open, and then open a new database.  May also
 * open a display manager, if interactive and none selected yet.
 *
 * Syntax is that of opendb from libged, with one addition - the last argument
 * can be an optional 'y' or 'n' indicating whether to create the database if
 * it does not exist.  This is used to avoid triggering MGED's interactive
 * prompting if presented with a filename that doesn't exist.
 *
 * Returns TCL_OK if database was opened, TCL_ERROR if database was
 * NOT opened (and the user didn't abort).
 */
int
f_opendb(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct mged_opendb_ctx ctx;

    if (argc <= 1) {
	/* Invoked without args, return name of current database */

	if (DBIP != DBI_NULL) {
	    Tcl_AppendResult(interpreter, DBIP->dbi_filename, (char *)NULL);
	    return TCL_OK;
	}

	Tcl_AppendResult(interpreter, "", (char *)NULL);
	return TCL_OK;
    }

    argc--; argv++;

    /* For the most part GED handles the options, but there is one
     * exception - the y/n option at the end of the command.  If
     * present, we replace that with a -c creation flag */
    ctx.post_open_cnt = 0;
    ctx.force_create = 0;
    ctx.no_create = 0;
    ctx.created_new_db = 0;
    ctx.interpreter = interpreter;
    ctx.ret = TCL_OK;
    if (BU_STR_EQUIV("y", argv[argc-1]) || BU_STR_EQUIV("n", argv[argc-1])) {
	if (BU_STR_EQUIV("y", argv[argc-1]))
	    ctx.force_create = 1;
	if (BU_STR_EQUIV("n", argv[argc-1]))
	    ctx.no_create = 1;

	argv[argc-1] = NULL;
	argc--;
    }

    // In order for MGED to work with GED calls that aren't prompted by
    // f_opendb or f_closedb, we must have a default data container available,
    // and callbacks to manage a default ctx.  However, if we're going the
    // f_opendb route, our options are handled a bit differently - use a local
    // data container for this call and stash the default.
    void (*pre_opendb_clbk)(struct ged *, void *) = GEDP->ged_pre_opendb_callback;
    void (*post_opendb_clbk)(struct ged *, void *) = GEDP->ged_post_opendb_callback;
    void (*pre_closedb_clbk)(struct ged *, void *) = GEDP->ged_pre_closedb_callback;
    void (*post_closedb_clbk)(struct ged *, void *) = GEDP->ged_post_closedb_callback;
    void *gctx = GEDP->ged_db_callback_udata;

    // Assign the local values
    GEDP->ged_pre_opendb_callback = NULL;
    GEDP->ged_post_opendb_callback = &mged_post_opendb_clbk;
    GEDP->ged_pre_closedb_callback = &mged_pre_closedb_clbk;
    GEDP->ged_post_closedb_callback = &mged_post_closedb_clbk;
    GEDP->ged_db_callback_udata = (void *)&ctx;

    const char **av = (const char **)bu_calloc(argc+2, sizeof(const char *), "av");
    int ind = 0;
    av[ind] = "opendb";
    ind++;
    if (ctx.force_create) {
	av[ind] = "-c";
	ind++;
    }
    for (int i = 0; i < argc; i++)
	av[i+ind] = argv[i];

    ctx.argv = av;
    ctx.argc = argc+ind;

    ctx.ged_ret = ged_exec(GEDP, argc+ind, (const char **)av);

    // Done - restore standard values
    GEDP->ged_pre_opendb_callback = pre_opendb_clbk;
    GEDP->ged_post_opendb_callback = post_opendb_clbk;
    GEDP->ged_pre_closedb_callback = pre_closedb_clbk;
    GEDP->ged_post_closedb_callback = post_closedb_clbk;
    GEDP->ged_db_callback_udata = gctx;

    if (ctx.ged_ret == GED_HELP) {
	Tcl_Eval(interpreter, "help opendb");
    }

    return ctx.ret;
}


/**
 * Close the current database, if open.
 */
int
f_closedb(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    // For the most part when it comes to close, the default
    // callbacks should be fine, but since f_closedb potentially
    // specifies a Tcl_Interp prepare a context to be sure
    // we're using the one expected.
    struct mged_opendb_ctx ctx;
    ctx.interpreter = interpreter;
    ctx.ret = TCL_OK;

    if (argc != 1) {
	Tcl_AppendResult(interpreter, "Unexpected argument [%s]\n", (const char *)argv[1], NULL);
	Tcl_Eval(interpreter, "help closedb");
	return TCL_ERROR;
    }

    if (DBIP == DBI_NULL) {
	Tcl_AppendResult(interpreter, "No database is open\n", NULL);
	return TCL_OK;
    }

    void *gctx = GEDP->ged_db_callback_udata;
    GEDP->ged_db_callback_udata = (void *)&ctx;

    const char *av[2];
    av[0] = "closedb";
    av[1] = NULL;
    ged_exec(GEDP, 1, (const char **)av);

    GEDP->ged_db_callback_udata = gctx;

    return ctx.ret;
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
