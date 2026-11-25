/*                           F _ D B . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
#include "ged/history.h"
#include "tclcad.h"

/* private */
#include "./cmd.h"
#include "./mged.h"
#include "./mged_wdb.h"

/* defined in chgmodel.c */
extern void set_localunit_TclVar(struct mged_state *s);

/* Shorthand for open/close db callback function pointer type */
typedef void (*db_clbk_t )(struct ged *, void *);

static void
_post_opendb_failed(struct mged_state *s, struct ged *gedp, struct mged_opendb_ctx *ctx)
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
    if (s->startup.ui != MGED_UI_HEADLESS && !ctx->force_create) {
	if (ctx->init_flag) {
	    if (s->startup.ui == MGED_UI_CLASSIC) {
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

		if (s->dpy_string != (char *)NULL)
		    bu_vls_printf(&vls, "cad_dialog .createdb %s \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
			    s->dpy_string, fname);
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
	const char *av[3] = {"opendb", "-c", NULL};
	av[2] = fname;
	ctx->ged_ret = ged_exec_opendb(gedp, 3, av);
    }

    if (gedp->dbip == DBI_NULL) {
	ctx->ret = TCL_ERROR;

	if (ctx->init_flag) {
	    /* we need to use bu_log here */
	    bu_log("opendb: failed to create %s\n", fname);
	    bu_log("opendb: no database is currently opened!\n");
	    return;
	}

	Tcl_AppendResult(ctx->interpreter, "opendb: failed to create ", fname, "\n", (char *)NULL);
	if (s->dbip == DBI_NULL)
	    Tcl_AppendResult(ctx->interpreter, "opendb: no database is currently opened!", (char *)NULL);

	return;
    }

    ctx->created_new_db = 1;
    bu_vls_printf(gedp->ged_result_str, "The new database %s was successfully created.\n", fname);
}

int
mged_pre_opendb_clbk(int ac, const char **argv, void *UNUSED(gedp), void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mctx->argc = ac;
    mctx->argv = argv;
    mctx->force_create = 0;
    mctx->no_create = 1;
    mctx->created_new_db = 0;
    mctx->ret = 0;
    mctx->ged_ret = 0;
    return BRLCAD_OK;
}

static int
mged_bind_database(struct mged_state *s)
{
    struct rt_wdb *inmem = wdb_dbopen(s->dbip, RT_WDB_TYPE_DB_INMEM);
    s->wdbp = wdb_dbopen(s->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!s->wdbp || !inmem) {
	Tcl_SetResult(s->interp, "Could not bind the open database to MGED", TCL_STATIC);
	return TCL_ERROR;
    }

    if (s->wdbp != inmem) {
	bu_vls_strcpy(&s->wdbp->wdb_name, MGED_DB_NAME);
	s->wdbp->wdb_interp = s->interp;
	BU_LIST_APPEND(&rtg_headwdb.l, &s->wdbp->l);
    }

    /* The .inmem Tcl object closes this reference when deleted. */
    struct bu_vls command = BU_VLS_INIT_ZERO;
    bu_vls_printf(&command, "wdb_open %s inmem %p", MGED_INMEM_NAME, (void *)s->dbip);
    (void)db_clone_dbi(s->dbip, NULL);
    int ret = Tcl_Eval(s->interp, bu_vls_addr(&command));
    bu_vls_free(&command);
    if (ret != TCL_OK) {
	db_close(s->dbip);
	return ret;
    }

    if (Tcl_LinkVar(s->interp, "local2base", (char *)&s->dbip->dbi_local2base,
	    TCL_LINK_DOUBLE | TCL_LINK_READ_ONLY) != TCL_OK ||
	Tcl_LinkVar(s->interp, "base2local", (char *)&s->dbip->dbi_base2local,
	    TCL_LINK_DOUBLE | TCL_LINK_READ_ONLY) != TCL_OK)
	return TCL_ERROR;

    set_localunit_TclVar(s);
    return TCL_OK;
}

int
mged_database_writable(struct mged_state *s)
{
    if (!s->dbip)
	return 0;
    if (!s->dbip->dbi_read_only)
	return 1;

    const struct ged_history_status *status = ged_history_status_create(s->gedp, NULL);
    const int writable = status && status->writable;
    ged_history_status_destroy(status);
    return writable;
}

int
mged_require_writable_database(struct mged_state *s)
{
    if (mged_database_writable(s))
	return TCL_OK;
    Tcl_AppendResult(s->interp, "The database is open read-only", (char *)NULL);
    return TCL_ERROR;
}

int
mged_post_opendb_clbk(int UNUSED(ac), const char **UNUSED(argv), void *vgedp, void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mctx->post_open_cnt++;
    struct ged *gedp = (struct ged *)vgedp; // TODO - just use s->gedp here?
    struct mged_state *s = mctx->s;

    // If the command didn't succeed, don't do anything.  Return OK for our execution
    // so the ged_exec return code is the command's ret, not ours.
    if (ged_results_ret(gedp->ged_results) != BRLCAD_OK)
	return BRLCAD_OK;

    /* Sync global to GED results */
    s->dbip = gedp->dbip;

    if (s->dbip == DBI_NULL || mctx->old_dbip == gedp->dbip) {
	_post_opendb_failed(s, gedp, mctx);
	mctx->post_open_cnt--;
	return BRLCAD_OK;
    }

    /* Opened database file */
    mctx->old_dbip = gedp->dbip;
    if (!mged_database_writable(s))
	bu_vls_printf(gedp->ged_result_str, "%s: READ ONLY\n", s->dbip->dbi_filename);

    /* Binding and the GUI open hook may invoke commands that reset GED output. */
    struct bu_vls saved_output = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&saved_output, bu_vls_cstr(gedp->ged_result_str));
    const int bind_result = mged_bind_database(s);
    if (bind_result == TCL_OK) {
	Tcl_Obj *args[] = {Tcl_NewStringObj("opendb_callback", -1),
	    Tcl_NewStringObj(s->dbip->dbi_filename, -1)};
	const int arg_count = sizeof(args) / sizeof(args[0]);
	for (int i = 0; i < arg_count; ++i)
	    Tcl_IncrRefCount(args[i]);
	(void)Tcl_EvalObjv(s->interp, arg_count, args, TCL_EVAL_GLOBAL);
	for (int i = 0; i < arg_count; ++i)
	    Tcl_DecrRefCount(args[i]);
    } else {
	bu_vls_printf(&saved_output, "%s\n", Tcl_GetStringResult(s->interp));
	mctx->ret = TCL_ERROR;
    }
    bu_vls_strcpy(gedp->ged_result_str, bu_vls_cstr(&saved_output));
    bu_vls_free(&saved_output);
    if (bind_result != TCL_OK) {
	mctx->post_open_cnt--;
	return BRLCAD_OK;
    }

    /* Print title/units information */
    if (s->startup.ui != MGED_UI_HEADLESS) {
	bu_vls_printf(gedp->ged_result_str, "%s (units=%s)\n", s->dbip->dbi_title,
		      bu_units_string(s->dbip->dbi_local2base));
    }

    /*
     * We have an old database version AND we're not in the process of
     * creating a new database.
     */
    if (db_version(s->dbip) < 5 && !mctx->created_new_db) {
	if (mctx->db_upgrade) {
	    if (mctx->db_warn)
		bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tConverting to the new format.\n");

	    (void)Tcl_Eval(mctx->interpreter, "after idle dbupgrade -f y");
	} else {
	    if (mctx->db_warn) {
		if (s->startup.ui == MGED_UI_CLASSIC)
		    bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tSee the dbupgrade command.");
		else
		    bu_vls_printf(gedp->ged_result_str, "Warning:\n\tDatabase version is old.\n\tSelect Tools-->Upgrade Database for info.");
	    }
	}
    }

    Tcl_ResetResult(mctx->interpreter);
    Tcl_AppendResult(mctx->interpreter, bu_vls_addr(gedp->ged_result_str), (char *)NULL);

    /* Update the background colors now that we have a file open */
    cs_set_bg(NULL, NULL, NULL, NULL, mctx->s);

    mctx->post_open_cnt--;
    mctx->ret = TCL_OK;
    return BRLCAD_OK;
}

static void
mged_unbind_database(struct mged_state *s)
{
    Tcl_UnlinkVar(s->interp, "local2base");
    Tcl_UnlinkVar(s->interp, "base2local");
    if (s->wdbp && s->wdbp != wdb_dbopen(s->dbip, RT_WDB_TYPE_DB_INMEM))
	bu_observer_free(&s->wdbp->wdb_observers);
    (void)Tcl_DeleteCommand(s->interp, MGED_INMEM_NAME);
}

int
mged_pre_closedb_clbk(int UNUSED(ac), const char **UNUSED(argv), void *UNUSED(gedp), void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    mged_unbind_database(mctx->s);
    return BRLCAD_OK;
}

int
mged_history_prepare(struct ged *gedp, void *ctx)
{
    struct mged_state *s = (struct mged_state *)ctx;
    if (s->global_editing_state == ST_VIEW || s->history_accepting)
	return BRLCAD_OK;
    bu_vls_strcat(gedp->ged_result_str, "Finish the active MGED edit before changing database history");
    return BRLCAD_ERROR;
}

struct mged_history_replacement {
    struct mged_state *state;
    struct db_i *retired;
};

static void
mged_history_rebind(void *ctx)
{
    struct mged_history_replacement *replacement = (struct mged_history_replacement *)ctx;
    struct mged_state *s = replacement->state;
    Tcl_SavedResult saved;
    Tcl_SaveResult(s->interp, &saved);
    mged_unbind_database(s);
    s->dbip = s->gedp->dbip;
    mged_global_db_ctx.old_dbip = s->dbip;
    if (mged_bind_database(s) != TCL_OK)
	bu_vls_printf(s->gedp->ged_result_str, "\nDatabase saved. Reopen it to refresh MGED: %s",
	    Tcl_GetStringResult(s->interp));
    Tcl_RestoreResult(s->interp, &saved);
    ged_history_rebuild_legacy_display(s->gedp, replacement->retired);
    if (!s->history_accepting && s->gedp->ged_refresh_handler)
	s->gedp->ged_refresh_handler(s->gedp->ged_refresh_clientdata);
}

void
mged_history_replaced(struct ged *UNUSED(gedp), struct db_i *retired, void *ctx)
{
    struct mged_history_replacement replacement = {(struct mged_state *)ctx, retired};
    mged_run_on_gui_thread(replacement.state, mged_history_rebind, &replacement);
}


int
mged_db_during_clbk(int argc, const char **argv, void *u1, void *u2)
{
    struct ged *gedp = (struct ged *)u1;
    struct mged_state *s = (struct mged_state *)u2;

    if (!gedp || !s || argc < 2 || !argv)
	return BRLCAD_ERROR | GED_UNKNOWN;

    int mged_cmd = BU_STR_EQUAL(argv[1], "make_bb") ||
	BU_STR_EQUAL(argv[1], "observer") ||
	BU_STR_EQUAL(argv[1], "rt_gettrees");
    if (!mged_cmd) {
	bu_vls_printf(gedp->ged_result_str, "db: unknown subcommand '%s'", argv[1]);
	return BRLCAD_ERROR | GED_UNKNOWN;
    }

    if (!s->wdbp) {
	bu_vls_printf(gedp->ged_result_str, "db %s: no database is currently open", argv[1]);
	return BRLCAD_ERROR;
    }

    Tcl_ResetResult(s->interp);
    int ret = mged_wdb_db_cmd(s->wdbp, argc, argv);
    const char *tresult = Tcl_GetStringResult(s->interp);
    if (tresult && tresult[0])
	bu_vls_strcpy(gedp->ged_result_str, tresult);

    return ret;
}

int
mged_post_closedb_clbk(int UNUSED(ac), const char **UNUSED(argv), void *vgedp, void *ctx)
{
    struct mged_opendb_ctx *mctx = (struct mged_opendb_ctx *)ctx;
    struct mged_state *s = mctx->s;
    struct ged *gedp = (struct ged *)vgedp; // TODO - just use s->gedp here?

    // If the command didn't succeed, don't do anything.  Return OK for our execution
    // so the ged_exec return code is the command's ret, not ours.
    if (ged_results_ret(gedp->ged_results) != BRLCAD_OK)
	return BRLCAD_OK;

    mctx->old_dbip = NULL;
    s->wdbp = RT_WDB_NULL;
    s->dbip = DBI_NULL;
    return BRLCAD_OK;
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
f_opendb(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct mged_opendb_ctx ctx;
    MGED_OPENDB_CTX_INIT(&ctx);
    ctx.old_dbip = NULL;
    ctx.s = s;

    if (argc <= 1) {
	/* Invoked without args, return name of current database */

	if (s->dbip != DBI_NULL) {
	    Tcl_AppendResult(interpreter, s->dbip->dbi_filename, (char *)NULL);
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
    ctx.no_create = s->read_only_startup;
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
    for (int i = 0; i + 1 < argc; ++i) {
	if (BU_STR_EQUAL("-r", argv[i]) || BU_STR_EQUAL("--read-only", argv[i]))
	    ctx.no_create = 1;
    }

    // In order for MGED to work with GED calls that aren't prompted by
    // f_opendb or f_closedb, we must have a default data container available,
    // and callbacks to manage a default ctx.  However, if we're going the
    // f_opendb route, our options are handled a bit differently - use a local
    // data container for this call and stash the default.
    bu_clbk_t opendb_clbk[2] = {NULL};
    void * opendb_clbk_data[2] = {NULL};
    bu_clbk_t closedb_clbk[2] = {NULL};
    void * closedb_clbk_data[2] = {NULL};
    ged_clbk_get(&opendb_clbk[0], &opendb_clbk_data[0], s->gedp, "opendb", BU_CLBK_PRE);
    ged_clbk_get(&opendb_clbk[1], &opendb_clbk_data[1], s->gedp, "opendb", BU_CLBK_POST);
    ged_clbk_get(&closedb_clbk[0], &closedb_clbk_data[0], s->gedp, "closedb", BU_CLBK_PRE);
    ged_clbk_get(&closedb_clbk[1], &closedb_clbk_data[1], s->gedp, "closedb", BU_CLBK_POST);

    // Assign the local values
    ged_clbk_set(s->gedp, "opendb", BU_CLBK_PRE, NULL, NULL);
    ged_clbk_set(s->gedp, "opendb", BU_CLBK_POST, &mged_post_opendb_clbk, (void *)&ctx);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_PRE, &mged_pre_closedb_clbk, (void *)&ctx);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_POST, &mged_post_closedb_clbk, (void *)&ctx);

    const char **av = (const char **)bu_calloc(argc+2, sizeof(const char *), "av");
    int ind = 0;
    av[ind] = "opendb";
    ind++;
    if (s->read_only_startup) {
	av[ind] = "-r";
	ind++;
    } else if (ctx.force_create) {
	av[ind] = "-c";
	ind++;
    }
    for (int i = 0; i < argc; i++)
	av[i+ind] = argv[i];

    ctx.argv = av;
    ctx.argc = argc+ind;

    ctx.ged_ret = ged_exec_opendb(s->gedp, argc+ind, (const char **)av);

    // Done - restore standard values
    ged_clbk_set(s->gedp, "opendb", BU_CLBK_PRE, opendb_clbk[0], opendb_clbk_data[0]);
    ged_clbk_set(s->gedp, "opendb", BU_CLBK_POST, opendb_clbk[1], opendb_clbk_data[1]);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_PRE, closedb_clbk[0], closedb_clbk_data[0]);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_POST, closedb_clbk[1], closedb_clbk_data[1]);

    if (ctx.ged_ret == GED_HELP) {
	Tcl_Eval(interpreter, "help opendb");
    }

    bu_free((void *)av, "f_opendb: av");
    return ctx.ret;
}


/**
 * Close the current database, if open.
 */
int
f_closedb(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    // For the most part when it comes to close, the default
    // callbacks should be fine, but since f_closedb potentially
    // specifies a Tcl_Interp prepare a context to be sure
    // we're using the one expected.
    struct mged_opendb_ctx ctx;
    MGED_OPENDB_CTX_INIT(&ctx);
    ctx.interpreter = interpreter;
    ctx.ret = TCL_OK;
    ctx.old_dbip = NULL;
    ctx.s = s;

    if (argc != 1) {
	Tcl_AppendResult(interpreter, "Unexpected argument [%s]\n", (const char *)argv[1], NULL);
	Tcl_Eval(interpreter, "help closedb");
	return TCL_ERROR;
    }

    if (s->dbip == DBI_NULL) {
	Tcl_AppendResult(interpreter, "No database is open\n", NULL);
	return TCL_OK;
    }

    bu_clbk_t closedb_clbk[2] = {NULL};
    void * closedb_clbk_data[2] = {NULL};
    ged_clbk_get(&closedb_clbk[0], &closedb_clbk_data[0], s->gedp, "closedb", BU_CLBK_PRE);
    ged_clbk_get(&closedb_clbk[1], &closedb_clbk_data[1], s->gedp, "closedb", BU_CLBK_POST);

    ged_clbk_set(s->gedp, "closedb", BU_CLBK_PRE, closedb_clbk[0], (void *)&ctx);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_POST, closedb_clbk[1], (void *)&ctx);

    const char *av[1] = {"closedb"};
    ged_exec_closedb(s->gedp, 1, (const char **)av);

    ged_clbk_set(s->gedp, "closedb", BU_CLBK_PRE, closedb_clbk[0], closedb_clbk_data[0]);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_POST, closedb_clbk[1], closedb_clbk_data[1]);

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
