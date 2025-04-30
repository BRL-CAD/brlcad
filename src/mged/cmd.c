/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/cmd.c
 *
 * The hooks to most of mged's commands when running in console mode.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "vmath.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/time.h"
#include "bn.h"
#include "rt/geom.h"
#include "ged.h"
#include "tclcad.h"

#include "./mged.h"
#include "./cmd.h"
#include "./mged_dm.h"
#include "./sedit.h"

extern void mged_finish(struct mged_state *s, int exitcode); /* in mged.c */
extern void update_grids(struct mged_state *s, fastf_t sf);		/* in grid.c */
extern void set_localunit_TclVar(struct mged_state *s);		/* in chgmodel.c */
extern void init_qray(void);			/* in qray.c */

/* Ew. Globals. */
extern int mged_default_dlist;			/* in attach.c */
struct cmd_list head_cmd_list;
struct cmd_list *curr_cmd_list;

static int glob_compat_mode = 1;
static int output_as_return = 1;

Tk_Window tkwin = NULL;


/* GUI output hooks use this variable to store the Tcl function used to run to
 * produce output. */
static struct bu_vls tcl_output_cmd = BU_VLS_INIT_ZERO;

/* Container to store up bu_log strings for eventual Tcl printing.  This is
 * done to ensure only one thread attempts to print to the Tcl output - bu_log
 * may be called from multiple threads, and if the hook itself tries to print
 * to Tcl Bad Things can happen. */
static struct bu_vls tcl_log_str = BU_VLS_INIT_ZERO;


/**
 * Used as a hook for bu_log output.  Sends output to the Tcl
 * procedure whose name is contained in the vls "tcl_output_hook".
 *
 * NOTE:  There is a problem with this code - per Tcl's documentation
 * (https://www.tcl.tk/doc/howto/thread_model.html) "errors will occur if you
 * let more than one thread call into the same interpreter (e.g., with
 * Tcl_Eval)"  However, gui_output may be called from multiple threads
 * during (say) a parallel raytrace, when lower level routines encounter
 * problems and bu_log about them.
 *
 * On some platforms we seem to get away with this despite the Tcl
 * documentation warning, but on Windows we've frequently seen the MGED command
 * prompt locking up - usually when we have heavy bu_log output from librt.
 * Since we're going to freeze up anyway, in that situation we accumulate
 * the output in a vls buffer rather than trying to force it to the Tcl
 * prompt - this avoids putting the Tcl interp in a problematic state.
 * Unfortunately, this comes at the expense of intermediate feedback reaching
 * the end user - because ged_exec calls are made from the main thread,
 * they are blocking as far as the refresh() call is concerned and we don't
 * see any bu_log output until the command completes.
 *
 * The correct fix here is to set up a separate thread for ged_exec calls
 * that doesn't block the main GUI thread.  That way, the intermediate
 * results being accumulated into the tcl_log_str buffer can be flushed
 * to the Tcl command prompt by refresh() while the GED command is still
 * running.  Not clear yet how much effort that will take to implement.
 */
int
gui_output(void *clientData, void *str)
{
    struct mged_state *s = (struct mged_state *)clientData;
    MGED_CK_STATE(s);

	int len;
    Tcl_DString tclcommand;
    Tcl_Obj *save_result;
    static int level = 0;

    if (level > 50) {
	bu_log_delete_hook(gui_output, s);
	/* Now safe to run bu_log? */
	bu_log("Ack! Something horrible just happened recursively.\n");
	return 0;
    }

    Tcl_DStringInit(&tclcommand);
    (void)Tcl_DStringAppendElement(&tclcommand, bu_vls_addr(&tcl_output_cmd));
    (void)Tcl_DStringAppendElement(&tclcommand, (const char *)str);

    save_result = Tcl_GetObjResult(s->interp);
    Tcl_IncrRefCount(save_result);
    ++level;
    Tcl_Eval(s->interp, Tcl_DStringValue(&tclcommand));
    --level;
    Tcl_SetObjResult(s->interp, save_result);
    Tcl_DecrRefCount(save_result);

    Tcl_DStringFree(&tclcommand);

    len = (int)strlen((const char *)str);
    return len;
}
#if 0
// Version of the above callback that just accumulates output in a buffer
// rather than writing it immediately to the interp - should be a starting
// point when we work on multithreading ged_exec calls
int
gui_output(void *UNUSED(clientData), void *str)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    bu_vls_printf(&tcl_log_str, "%s", (const char *)str);
    bu_semaphore_release(BU_SEM_SYSCALL);
    int len = (int)strlen((const char *)str);
    return len;
}
#endif

void
mged_pr_output(Tcl_Interp *interp)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if (!bu_vls_strlen(&tcl_output_cmd))
	bu_vls_sprintf(&tcl_output_cmd, "output_callback");

    if (bu_vls_strlen(&tcl_log_str)) {
	Tcl_DString tclcommand;
	Tcl_DStringInit(&tclcommand);
	(void)Tcl_DStringAppendElement(&tclcommand, bu_vls_cstr(&tcl_output_cmd));
	(void)Tcl_DStringAppendElement(&tclcommand, bu_vls_cstr(&tcl_log_str));
	Tcl_Obj *save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);
	Tcl_Eval(interp, Tcl_DStringValue(&tclcommand));
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);
	Tcl_DStringFree(&tclcommand);
	bu_vls_trunc(&tcl_log_str, 0);
    }

    bu_semaphore_release(BU_SEM_SYSCALL);
}

#define GED_OUTPUT do { \
    mged_pr_output(interpreter);\
    Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL); \
} while (0)


int
mged_db_search_callback(int argc, const char *argv[], void *UNUSED(u1), void *u2)
{
    struct mged_state *s = (struct mged_state *)u2;
    MGED_CK_STATE(s);
    Tcl_Interp *interp = s->interp;

    /* FIXME: pretty much copied from tclcad, ideally this should call
     * tclcad's eval instead of doing its own thing but this is probably
     * fine for now */
    int ret;
    int i;
    size_t len;
    const char *result = NULL;

    Tcl_DString script;
    Tcl_DStringInit(&script);
    if (argc<=0) /* empty exec is a true no-op */
    	return 1;
    Tcl_DStringAppend(&script, argv[0], -1);

    for (i = 1; i < argc; ++i)
	Tcl_DStringAppendElement(&script, argv[i]);

    ret = Tcl_Eval(interp, Tcl_DStringValue(&script));
    Tcl_DStringFree(&script);

    result = Tcl_GetStringResult(interp);
    len = strlen(result);
    if (len > 0)
	bu_log("%s%s", result, result[len-1] == '\n' ? "" : "\n");

    Tcl_ResetResult(interp);

    /* NOTE: Tcl_Eval saves the last -exec result to s->gedp->ged_result_str
       this causes a duplicate print of the last 'search -exec' in mged (since
       we're bu_logging here and then the ged_result_str is later flushed).
       To fix this, we need to clear the ged_result_str
    */
    bu_vls_trunc(s->gedp->ged_result_str, 0);

    return TCL_OK == ret;
}


int
cmd_ged_edit_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    const char *av[3];
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    av[0] = "draw";
    av[1] = argv[argc-1];
    av[2] = NULL;
    cmd_draw(clientData, interpreter, 2, av);

    return TCL_OK;
}


/**
 * Wrapper for the Mged simulate command : draws argv[argc-1] after execution
 *
 */
int
cmd_ged_simulate_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    const char *av[3];
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;


    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    av[0] = "draw";
    av[1] = argv[1];
    av[2] = NULL;
    cmd_draw(clientData, interpreter, 2, av);


    return TCL_OK;
}


int
cmd_ged_info_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char **av;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct ged_bv_data *bdata = NULL;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (argc >= 2) {
	(void)(*ctp->ged_func)(s->gedp, argc, (const char **)argv);
	GED_OUTPUT;
    } else {
	if ((argc == 1) && (s->edit_state.global_editing_state == ST_S_EDIT)) {
	    argc = 2;
	    av = (const char **)bu_malloc(sizeof(char *)*(argc + 1), "f_list: av");
	    av[0] = (const char *)argv[0];
	    if (illump && illump->s_u_data) {
		bdata = (struct ged_bv_data *)illump->s_u_data;
		if (bdata->s_fullpath.fp_len > 0) {
		    av[1] = (const char *)LAST_SOLID(bdata)->d_namep;
		    av[argc] = (const char *)NULL;
		    (void)(*ctp->ged_func)(s->gedp, argc, (const char **)av);
		    GED_OUTPUT;
		}
	    }
	    bu_free((void *)av, "cmd_ged_info_wrapper: av");
	} else {
	    (void)(*ctp->ged_func)(s->gedp, argc, (const char **)argv);
	    GED_OUTPUT;
	}
    }

    return TCL_OK;
}


int
cmd_ged_erase_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    solid_list_callback(s);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    return TCL_OK;
}


int
cmd_ged_gqa(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    char **vp;
    int i;
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    size_t args;
    int gd_rt_cmd_len = 0;
    char **gd_rt_cmd = NULL;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    args = argc + 2 + ged_who_argc(s->gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];

    /* Grab command name and any options */
    *vp++ = (char *)argv[0];
    for (i=1; i < argc; i++) {
	if (argv[i][0] != '-')
	    break;

	if (argv[i][0] == '-' &&
	    argv[i][1] == '-' &&
	    argv[i][2] == '\0') {
	    ++i;
	    break;
	}

	*vp++ = (char *)argv[i];
    }

    /*
     * Append remaining args, if any. Otherwise, append currently
     * displayed objects.
     */
    if (i < argc) {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	gd_rt_cmd_len = vp - gd_rt_cmd;
	*vp = 0;
	vp = &gd_rt_cmd[0];
	while (*vp)
	    bu_vls_printf(s->gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(s->gedp->ged_result_str, "\n");
    } else {
	gd_rt_cmd_len = vp - gd_rt_cmd;
	gd_rt_cmd_len += ged_who_argv(s->gedp, vp, (const char **)&gd_rt_cmd[args]);
    }

    ret = (*ctp->ged_func)(s->gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd);
    GED_OUTPUT;

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    return TCL_OK;
}


int
cmd_ged_in(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    const char *new_cmd[3];
    int c;
    int do_solid_edit = 0;
    int dont_draw = 0;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    /* Parse options. */
    bu_optind = 1; /* re-init bu_getopt() */
    bu_opterr = 0; /* suppress bu_getopt()'s error message */
    while ((c=bu_getopt(argc, (char * const *)argv, "sf")) != -1) {
	switch (c) {
	    case 's':
		do_solid_edit = 1;
		break;
	    case 'f':
		dont_draw = 1;
		break;
	    default:
		{
		    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		    bu_vls_printf(&tmp_vls, "in: option '%c' unknown\n", bu_optopt);
		    Tcl_AppendResult(interpreter, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		}

		break;
	}
    }

    if (bu_optind > 1) {
	int offset = bu_optind - 1;
	// we've handled the flags - remove them but keep the rest of the string intact
	for (int i = 1; i < argc - offset; i++) {
	    argv[i] = argv[i + offset];
	}
	// null out the rest
	for (int i = argc - offset; i < argc; i++) {
	    argv[i] = NULL;
	}

	// update count
	argc -= offset;
    }

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    if (ret & GED_MORE) {
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
	Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL);
    } else {
	GED_OUTPUT;
    }

    if (dont_draw) {
	if (ret & GED_HELP || ret == BRLCAD_OK)
	    return TCL_OK;

	return TCL_ERROR;
    }

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    /* draw the newly "made" solid */
    new_cmd[0] = "draw";
    new_cmd[1] = argv[1];
    new_cmd[2] = (char *)NULL;
    (void)cmd_draw(clientData, interpreter, 2, new_cmd);

    if (do_solid_edit) {
	struct bu_vls sed_cmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&sed_cmd, "sed %s", argv[1]);

	/* Also kick off solid edit mode */
	Tcl_Eval(interpreter, bu_vls_cstr(&sed_cmd));

	bu_vls_free(&sed_cmd);
    }
    return TCL_OK;
}

int
cmd_ged_inside(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    int arg;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    const char *new_cmd[3];
    struct rt_db_internal intern;
    struct ged_bv_data *bdata = NULL;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    RT_DB_INTERNAL_INIT(&intern);

    if (s->edit_state.global_editing_state == ST_S_EDIT) {
	/* solid edit mode */
	/* apply s->edit_state.e_mat editing to parameters */
	struct directory *outdp = RT_DIR_NULL;
	transform_editing_solid(s, &intern, s->edit_state.e_mat, &s->edit_state.es_int, 0);
	if (illump && illump->s_u_data) {
	    bdata = (struct ged_bv_data *)illump->s_u_data;
	    outdp = LAST_SOLID(bdata);
	}

	if (argc < 2) {
	    Tcl_AppendResult(interpreter, "You are in Primitive Edit mode, using edited primitive as outside primitive: ", (char *)NULL);
	    add_solid_path_to_result(interpreter, illump);
	    Tcl_AppendResult(interpreter, "\n", (char *)NULL);
	}

	arg = 1;
	if (outdp) {
	    ret = ged_inside_internal(s->gedp, &intern, argc, argv, arg, outdp->d_namep);
	} else {
	    ret = TCL_ERROR;
	}
    }  else if (s->edit_state.global_editing_state == ST_O_EDIT) {
	mat_t newmat;
	struct directory *outdp = RT_DIR_NULL;

	/* object edit mode */
	if (illump->s_old.s_Eflag) {
	    Tcl_AppendResult(interpreter, "Cannot find inside of a processed (E'd) region\n",
			     (char *)NULL);
	    (void)signal(SIGINT, SIG_IGN);
	    return TCL_ERROR;
	}
	/* use the solid at bottom of path (key solid) */
	/* apply s->edit_state.e_mat and s->edit_state.model_changes editing to parameters */
	bn_mat_mul(newmat, s->edit_state.model_changes, s->edit_state.e_mat);
	transform_editing_solid(s, &intern, newmat, &s->edit_state.es_int, 0);
	if (illump && illump->s_u_data) {
	    bdata = (struct ged_bv_data *)illump->s_u_data;
	    outdp = LAST_SOLID(bdata);
	}

	if (illump && argc < 2) {
	    Tcl_AppendResult(interpreter, "You are in Object Edit mode, using key solid as outside solid: ", (char *)NULL);
	    add_solid_path_to_result(interpreter, illump);
	    Tcl_AppendResult(interpreter, "\n", (char *)NULL);
	}

	arg = 1;
	if (outdp) {
	    ret = ged_inside_internal(s->gedp, &intern, argc, argv, arg, outdp->d_namep);
	} else {
	    ret = TCL_ERROR;
	}
    } else {
	arg = 2;
	ret = ged_exec(s->gedp, argc, (const char **)argv);
    }

    if (ret & GED_MORE) {
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
	Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL);
    } else {
	GED_OUTPUT;
    }

    if (ret & GED_HELP) {
	(void)signal(SIGINT, SIG_IGN);
	return TCL_OK;
    }

    if (ret) {
	(void)signal(SIGINT, SIG_IGN);
	return TCL_ERROR;
    }

    /* draw the "inside" solid */
    new_cmd[0] = "draw";
    new_cmd[1] = argv[arg];
    new_cmd[2] = (char *)NULL;
    (void)cmd_draw(clientData, interpreter, 2, new_cmd);

    (void)signal(SIGINT, SIG_IGN);

    return TCL_OK;
}


int
cmd_ged_more_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    const char *new_cmd[3];

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    if (ret & GED_MORE) {
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
	Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL);
    } else {
	GED_OUTPUT;
    }

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    /* FIXME: this wrapper shouldn't be aware of individual commands.
     * presently needed to draw the changed arg for a limited set of
     * commands.
     */
    new_cmd[0] = "draw";
    if (BU_STR_EQUAL(argv[0], "3ptarb"))
	new_cmd[1] = argv[1];
    else if (BU_STR_EQUAL(argv[0], "inside"))
	new_cmd[1] = argv[2];
    else {
	(void)signal(SIGINT, SIG_IGN);
	return TCL_OK;
    }

    new_cmd[2] = (char *)NULL;
    (void)cmd_draw(clientData, interpreter, 2, new_cmd);

    (void)signal(SIGINT, SIG_IGN);

    return TCL_OK;
}


int
cmd_ged_plain_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);

/* This code is for debugging/testing the new ged return mechanism */
#if 0
    {
	int r_loop = 0;
	size_t result_cnt = 0;

	result_cnt = ged_results_count(s->gedp->ged_results);
	if (result_cnt > 0) {
	    bu_log("Results container holds results(%d):\n", result_cnt);
	    for (r_loop = 0; r_loop < (int)result_cnt; r_loop++) {
		bu_log("%s\n", ged_results_get(s->gedp->ged_results, r_loop));
	    }
	}
    }
#endif

    if (ret & GED_MORE) {
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
	Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL);
    } else {
	GED_OUTPUT;
    }

    /* redraw any objects specified that are already drawn */
    if (argc > 1) {
	struct bu_vls rcache = BU_VLS_INIT_ZERO;
	int who_ret;
	const char *who_cmd[1] = {"who"};

	/* Stash previous result string state so who cmd doesn't replace it */
	bu_vls_sprintf(&rcache, "%s", bu_vls_addr(s->gedp->ged_result_str));

	who_ret = ged_exec_who(s->gedp, 1, who_cmd);
	if (who_ret == BRLCAD_OK) {
	    /* worst possible is a bunch of 1-char names, allocate and
	     * split into an argv accordingly.
	     */

	    int i, j;
	    char *str = bu_strdup(bu_vls_addr(s->gedp->ged_result_str));
	    size_t who_argc = (bu_vls_strlen(s->gedp->ged_result_str) / 2) + 1;
	    char **who_argv = (char **)bu_calloc(who_argc+1, sizeof(char *), "who_argv");

	    who_ret = bu_argv_from_string(who_argv, who_argc, str);
	    for (i=0; i < who_ret; i++) {
		for (j=1; j < argc; j++) {
		    /* compare all displayed to all argv items, see if
		     * we find any matches...
		     */
		    if (BU_STR_EQUAL(who_argv[i], argv[j])) {
			/* FOUND A MATCH! */

			const char *draw_cmd[3] = {"draw", NULL, NULL};
			draw_cmd[1] = who_argv[i];
			(void)cmd_draw(clientData, interpreter, 2, draw_cmd);
		    }
		}
	    }

	    bu_free(who_argv, "who_argv");
	    bu_free(str, "result strdup");
	}

	/* Restore ged result str */
	bu_vls_sprintf(s->gedp->ged_result_str, "%s", bu_vls_addr(&rcache));
	bu_vls_free(&rcache);
    }

    if (ret & GED_HELP || ret == BRLCAD_OK)
	return TCL_OK;

    return TCL_ERROR;
}


int
cmd_ged_view_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (!s->gedp->ged_gvp)
	s->gedp->ged_gvp = view_state->vs_gvp;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    /* Note - we don't call mged_svbase here because we don't always want the
     * knob settings reset after a ged view command.  If a view command wants
     * that part of mged_svbase to happen it is the responsibility of that
     * command to call bv_knobs_reset(v, BV_KNOBS_ABS); */
    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui) {
	s->mged_curr_dm->dm_dirty = 1;
	dm_set_dirty(DMP, 1);
    }

    view_state->vs_flag = 1;

    return TCL_OK;
}


int
cmd_ged_dm_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    if (!s->gedp->ged_gvp)
	s->gedp->ged_gvp = view_state->vs_gvp;
    s->gedp->ged_gvp->dmp = (void *)s->mged_curr_dm->dm_dmp;

    ret = (*ctp->ged_func)(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    (void)signal(SIGINT, SIG_IGN);

    if (ret & GED_HELP || ret == BRLCAD_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * Command for initializing the Tk window and defaults.
 *
 * Usage:  loadtk [displayname[.screennum]]
 */
int
cmd_tk(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int status;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help loadtk");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1)
	status = gui_setup(s, (char *)NULL);
    else
	status = gui_setup(s, argv[1]);

    return status;
}


/**
 * Hooks the output to the given output hook.  Removes the existing
 * output hook!
 */
int
cmd_output_hook(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls infocommand = BU_VLS_INIT_ZERO;
    int status;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel output_hook");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_log_delete_hook(gui_output, (void *)s);/* Delete the existing hook */

    if (argc < 2)
	return TCL_OK;

    /* Make sure the command exists before putting in the hook! */
    /* Note - the parameters to proc could be wrong and/or the proc
     * could still disappear later.
     */
    bu_vls_strcat(&infocommand, "info commands ");
    bu_vls_strcat(&infocommand, argv[1]);
    status = Tcl_Eval(interpreter, bu_vls_addr(&infocommand));
    bu_vls_free(&infocommand);

    if (status != TCL_OK || Tcl_GetStringResult(interpreter)[0] == '\0') {
	Tcl_AppendResult(interpreter, "command does not exist", (char *)NULL);
	return TCL_ERROR;
    }

    /* Also, don't allow silly infinite loops. */

    if (BU_STR_EQUAL(argv[1], argv[0])) {
	Tcl_AppendResult(interpreter, "Detected potential infinite loop in cmd_output_hook.", (char *)NULL);
	return TCL_ERROR;
    }

    /* Set up the command */
    bu_vls_sprintf(&tcl_output_cmd, "%s", argv[1]);

    /* Set up the libbu hook */
    bu_log_add_hook(gui_output, (void *)s);

    Tcl_ResetResult(interpreter);
    return TCL_OK;
}


int
cmd_nop(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), const char *UNUSED(argv[]))
{
    return TCL_OK;
}


int
cmd_cmd_win(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc < 2) {
	bu_vls_printf(&vls, "helpdevel cmd_win");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "open")) {
	struct cmd_list *clp;
	int name_not_used = 1;

	if (argc != 3) {
	    bu_vls_printf(&vls, "helpdevel cmd_win");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* Search to see if there exists a command window with this name */
	for (BU_LIST_FOR (clp, cmd_list, &head_cmd_list.l))
	    if (BU_STR_EQUAL(argv[2], bu_vls_addr(&clp->cl_name))) {
		name_not_used = 0;
		break;
	    }

	if (name_not_used) {
	    clp = (struct cmd_list *)bu_malloc(sizeof(struct cmd_list), "cmd_list");
	    memset((void *)clp, 0, sizeof(struct cmd_list));
	    BU_LIST_APPEND(&head_cmd_list.l, &clp->l);
	    clp->cl_cur_hist = head_cmd_list.cl_cur_hist;
	    bu_vls_init(&clp->cl_more_default);
	    bu_vls_init(&clp->cl_name);
	    bu_vls_strcpy(&clp->cl_name, argv[2]);
	}

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "close")) {
	struct cmd_list *clp;

	if (argc != 3) {
	    bu_vls_printf(&vls, "helpdevel cmd_win");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* First, search to see if there exists a command window with
	 * the name in argv[2].
	 */
	for (BU_LIST_FOR (clp, cmd_list, &head_cmd_list.l))
	    if (BU_STR_EQUAL(argv[2], bu_vls_addr(&clp->cl_name)))
		break;

	if (clp == &head_cmd_list) {
	    if (BU_STR_EQUAL(argv[2], "mged"))
		Tcl_AppendResult(interpreter, "cmd_close: not allowed to close \"mged\"",
				 (char *)NULL);
	    else
		Tcl_AppendResult(interpreter, "cmd_close: did not find \"", argv[2],
				 "\"", (char *)NULL);
	    return TCL_ERROR;
	}

	if (clp == curr_cmd_list)
	    curr_cmd_list = &head_cmd_list;

	BU_LIST_DEQUEUE(&clp->l);
	if (clp->cl_tie != NULL)
	    clp->cl_tie->dm_tie = CMD_LIST_NULL;
	bu_vls_free(&clp->cl_more_default);
	bu_vls_free(&clp->cl_name);
	bu_free((void *)clp, "cmd_close: clp");

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "get")) {
	if (argc != 2) {
	    bu_vls_printf(&vls, "helpdevel cmd_win");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	Tcl_AppendElement(interpreter, bu_vls_addr(&curr_cmd_list->cl_name));

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "set")) {
	if (argc != 3) {
	    bu_vls_printf(&vls, "helpdevel cmd_win");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	for (BU_LIST_FOR (curr_cmd_list, cmd_list, &head_cmd_list.l)) {
	    if (!BU_STR_EQUAL(bu_vls_addr(&curr_cmd_list->cl_name), argv[2]))
		continue;

	    break;
	}

	if (curr_cmd_list->cl_tie) {
	    set_curr_dm(s, curr_cmd_list->cl_tie);

	    if (s->gedp != GED_NULL)
		s->gedp->ged_gvp = view_state->vs_gvp;
	}

	bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    bu_vls_printf(&vls, "helpdevel cmd_win");
    Tcl_Eval(interpreter, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


int
cmd_get_more_default(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    if (argc != 1) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_log("Unrecognized option [%s]\n", argv[1]);

	bu_vls_printf(&vls, "helpdevel get_more_default");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interpreter, bu_vls_addr(&curr_cmd_list->cl_more_default), (char *)NULL);
    return TCL_OK;
}


int
cmd_set_more_default(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel set_more_default");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_strcpy(&curr_cmd_list->cl_more_default, argv[1]);
    return TCL_OK;
}


/**
 * This routine is called to process a vls full of commands.  Each
 * command is newline terminated.  The input string will not be
 * altered in any way.
 *
 * Returns -
 * !0 when a prompt needs to be printed.
 * 0 no prompt needed.
 */
int
cmdline(struct mged_state *s, struct bu_vls *vp, int record)
{
    int status;
    struct bu_vls globbed = BU_VLS_INIT_ZERO;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    struct bu_vls save_vp = BU_VLS_INIT_ZERO;
    int64_t start;
    int64_t finish;
    size_t len;
    char *cp;
    const char *result;

    BU_CK_VLS(vp);

    if (bu_vls_strlen(vp) <= 0)
	return CMD_OK;

    bu_vls_vlscat(&save_vp, vp);

    /* MUST MAKE A BACKUP OF THE INPUT STRING AND USE THAT IN THE CALL
       TO Tcl_Eval!!!

       You never know who might change the string (append to it...)
       (f_mouse is notorious for adding things to the input string) If
       it were to change while it was still being evaluated, Horrible
       Things could happen.
    */

    if (glob_compat_mode) {
	struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
	if (s->gedp == GED_NULL)
	    return CMD_BAD;

	/* Cache the state bits we might change in s->gedp */
	bu_vls_sprintf(&tmpstr, "%s", bu_vls_addr(s->gedp->ged_result_str));

	/* Run ged_glob */
	const char *av[2] = {"glob", NULL};
	av[1] = bu_vls_cstr(vp);
	(void)ged_exec_glob(s->gedp, 2, av);
	if (bu_vls_strlen(s->gedp->ged_result_str) > 0) {
	    bu_vls_sprintf(&globbed, "%s", bu_vls_addr(s->gedp->ged_result_str));
	} else {
	    bu_vls_vlscat(&globbed, vp);
	}

	/* put s->gedp back where it was */
	bu_vls_sprintf(s->gedp->ged_result_str, "%s", bu_vls_addr(&tmpstr));

	/* cleanup */
	bu_vls_free(&tmpstr);
    } else {
	bu_vls_vlscat(&globbed, vp);
    }

    start = bu_gettime();
    status = Tcl_Eval(s->interp, bu_vls_addr(&globbed));
    finish = bu_gettime();
    result = Tcl_GetStringResult(s->interp);

    /* Contemplate the result reported by the Tcl interpreter. */

    switch (status) {
	case TCL_RETURN:
	case TCL_OK:
	    if (setjmp(jmp_env) == 0) {
		len = strlen(result);

		/* If the command had something to say, print it out. */
		if (len > 0) {
		    (void)signal(SIGINT, sig3);  /* allow interrupts */

		    bu_log("%s%s", result,
			   result[len-1] == '\n' ? "" : "\n");

		    (void)signal(SIGINT, SIG_IGN);
		}

		/* A user typed this command so let everybody see,
		 * then record it in the history.
		 */
		if (record && tkwin != NULL) {
		    bu_vls_printf(&tmp_vls, "distribute_text {} {%s} {%s}",
				  bu_vls_addr(&save_vp), result);
		    Tcl_Eval(s->interp, bu_vls_addr(&tmp_vls));
		    Tcl_SetResult(s->interp, "", TCL_STATIC);
		}

		if (record)
		    history_record(&save_vp, start, finish, CMD_OK);

	    } else {
		/* XXXXXX */
		bu_semaphore_release(BU_SEM_SYSCALL);
		bu_log("\n");
	    }

	    bu_vls_strcpy(&s->mged_prompt, MGED_PROMPT);
	    status = CMD_OK;
	    goto end;

	case TCL_ERROR:
	default:

	    /* First check to see if it's a secret message. */

	    if ((cp = strstr(result, MORE_ARGS_STR)) != NULL) {
		bu_vls_trunc(&s->mged_prompt, 0);

		if (cp == result) {
		    bu_vls_printf(&s->mged_prompt, "\r%s",
				  result+sizeof(MORE_ARGS_STR)-1);
		} else {
		    struct bu_vls buf = BU_VLS_INIT_ZERO;

		    len = cp - result;
		    bu_vls_strncpy(&buf, result, len);
		    bu_log("%s%s", bu_vls_addr(&buf), result[len-1] == '\n' ? "" : "\n");
		    bu_vls_free(&buf);

		    bu_vls_printf(&s->mged_prompt, "\r%s",
				  result+sizeof(MORE_ARGS_STR)-1+len);
		}

		status = CMD_MORE;
		goto end;
	    }

	    /* Otherwise, it's just a regular old error. */

	    len = strlen(result);
	    if (len > 0) bu_log("%s%s", result,
				result[len-1] == '\n' ? "" : "\n");

	    if (record)
		history_record(&save_vp, start, finish, CMD_BAD);

	    bu_vls_strcpy(&s->mged_prompt, MGED_PROMPT);
	    status = CMD_BAD;

	    /* Fall through to end */
    }

end:
    bu_vls_free(&globbed);
    bu_vls_free(&tmp_vls);
    bu_vls_free(&save_vp);

    return status;
}


void
mged_print_result(struct mged_state *s, int UNUSED(status))
{
    size_t len;
    const char *result = Tcl_GetStringResult(s->interp);

    len = strlen(result);
    if (len > 0) {
	bu_log("%s%s", result,
	       result[len-1] == '\n' ? "" : "\n");

	pr_prompt(s);
    }

    Tcl_ResetResult(s->interp);
}

/**
 * Let the user temporarily escape from the editor Format: %
 */
int
f_comm(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc != 1 || !s->classic_mged || curr_cmd_list != &head_cmd_list) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

#ifndef _WIN32
    {
	int pid, rpid;
	int retcode;

	(void)signal(SIGINT, SIG_IGN);
	if ((pid = fork()) == 0) {
	    (void)signal(SIGINT, SIG_DFL);
	    (void)execl("/bin/sh", "-", (char *)NULL);
	    perror("/bin/sh");
	    mged_finish(s, 11);
	}

	while ((rpid = wait(&retcode)) != pid && rpid != -1)
	    ;
    }
#endif

    Tcl_AppendResult(s->interp, "Returning to MGED\n", (char *)NULL);

    return TCL_OK;
}


/**
 * Quit and exit gracefully. Format: q
 */
int
f_quit(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_VIEW)
	button(s, BE_REJECT);

    quit(s);			/* Exiting time */
    /* NOTREACHED */
    return TCL_OK;
}


/**
 * SYNOPSIS
 * tie [cw [dm]]
 * tie -u cw
 *
 * DESCRIPTION
 * This command ties/associates a command window (cw) to a display
 * manager window (dm).  When a command window is tied to a display
 * manager window, all commands issued from this window will be
 * directed at a particular display manager. Otherwise, the commands
 * issued will be directed at the current display manager window.
 *
 * EXAMPLES
 * tie ---> returns a list of the command_window/display_manager associations
 * tie cw1 ---> returns the display_manager, if it exists, associated with cw1
 * tie cw1 dm1 ---> associated cw1 with dm1
 * tie -u cw1 ---> removes the association, if it exists, cw1 has with a display manager
 */
int
f_tie(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int uflag = 0;		/* untie flag */
    struct cmd_list *clp;
    struct mged_dm *dlp = MGED_DM_NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 1 || 3 < argc) {
	bu_vls_printf(&vls, "helpdevel tie");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1) {
	for (BU_LIST_FOR (clp, cmd_list, &head_cmd_list.l)) {
	    bu_vls_trunc(&vls, 0);
	    if (clp->cl_tie) {
		struct bu_vls *pn = dm_get_pathname(clp->cl_tie->dm_dmp);
		if (pn && bu_vls_strlen(pn)) {
		    bu_vls_printf(&vls, "%s %s", bu_vls_cstr(&clp->cl_name), bu_vls_cstr(pn));
		    Tcl_AppendElement(interpreter, bu_vls_cstr(&vls));
		}
	    } else {
		bu_vls_printf(&vls, "%s {}", bu_vls_cstr(&clp->cl_name));
		Tcl_AppendElement(interpreter, bu_vls_cstr(&vls));
	    }
	}

	bu_vls_trunc(&vls, 0);
	if (clp->cl_tie) {
	    struct bu_vls *pn = dm_get_pathname(clp->cl_tie->dm_dmp);
	    if (pn && bu_vls_strlen(pn)) {
		bu_vls_printf(&vls, "%s %s", bu_vls_cstr(&clp->cl_name), bu_vls_cstr(pn));
		Tcl_AppendElement(interpreter, bu_vls_cstr(&vls));
	    }
	} else {
	    bu_vls_printf(&vls, "%s {}", bu_vls_cstr(&clp->cl_name));
	    Tcl_AppendElement(interpreter, bu_vls_cstr(&vls));
	}

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (argv[1][0] == '-' && argv[1][1] == 'u') {
	uflag = 1;
	--argc;
	++argv;
    }

    if (argc < 2) {
	bu_vls_printf(&vls, "help tie");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (BU_LIST_FOR (clp, cmd_list, &head_cmd_list.l))
	if (BU_STR_EQUAL(bu_vls_addr(&clp->cl_name), argv[1]))
	    break;

    if (clp == &head_cmd_list &&
	(!BU_STR_EQUAL(bu_vls_addr(&head_cmd_list.cl_name), argv[1]))) {
	Tcl_AppendResult(interpreter, "f_tie: unrecognized command_window - ", argv[1],
			 "\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (uflag) {
	if (clp->cl_tie)
	    clp->cl_tie->dm_tie = (struct cmd_list *)NULL;

	clp->cl_tie = (struct mged_dm *)NULL;

	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* print out the display manager that we're tied to */
    if (argc == 2) {
	if (clp->cl_tie) {
	    struct bu_vls *pn = dm_get_pathname(clp->cl_tie->dm_dmp);
	    if (pn && bu_vls_strlen(pn)) {
		Tcl_AppendElement(interpreter, bu_vls_cstr(pn));
	    }
	} else {
	    Tcl_AppendElement(interpreter, "");
	}
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (*argv[2] != '.')
	bu_vls_printf(&vls, ".%s", argv[2]);
    else
	bu_vls_strcpy(&vls, argv[2]);

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	struct bu_vls *pn = dm_get_pathname(m_dmp->dm_dmp);
	if (pn && !bu_vls_strcmp(&vls, pn)) {
	    dlp = m_dmp;
	    break;
	}
    }

    if (dlp == MGED_DM_NULL) {
	Tcl_AppendResult(interpreter, "f_tie: unrecognized path name - ",
			 bu_vls_addr(&vls), "\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* already tied */
    if (clp->cl_tie)
	clp->cl_tie->dm_tie = (struct cmd_list *)NULL;

    clp->cl_tie = dlp;

    /* already tied */
    if (dlp->dm_tie)
	dlp->dm_tie->cl_tie = (struct mged_dm *)NULL;

    dlp->dm_tie = clp;

    bu_vls_free(&vls);
    return TCL_OK;
}


int
f_postscript(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int status;
    struct mged_dm *dml;
    struct _view_state *vsp;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    if (argc < 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help postscript");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->gedp == GED_NULL)
	return TCL_OK;

    dml = s->mged_curr_dm;
    s->gedp->ged_gvp = view_state->vs_gvp;
    status = mged_attach(s, "postscript", argc, argv);
    if (status == TCL_ERROR)
	return TCL_ERROR;

    vsp = view_state;  /* save state info pointer */

    bu_free((void *)menu_state, "f_postscript: menu_state");
    menu_state = dml->dm_menu_state;

    scroll_top = dml->dm_scroll_top;
    scroll_active = dml->dm_scroll_active;
    scroll_y = dml->dm_scroll_y;
    memmove((void *)scroll_array, (void *)dml->dm_scroll_array, sizeof(struct scroll_item *) * 6);

    DMP_dirty = 1;
    dm_set_dirty(DMP, 1);
    refresh(s);

    view_state = vsp;  /* restore state info pointer */
    status = Tcl_Eval(interpreter, "release");
    set_curr_dm(s, dml);
    s->gedp->ged_gvp = view_state->vs_gvp;

    return status;
}


int
f_winset(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel winset");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* print pathname of drawing window with primary focus */
    if (argc == 1) {
	struct bu_vls *pn = dm_get_pathname(DMP);
	if (pn && bu_vls_strlen(pn)) {
	    Tcl_AppendResult(interpreter, bu_vls_cstr(pn), (char *)NULL);
	}
	return TCL_OK;
    }

    /* change primary focus to window argv[1] */
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	struct bu_vls *pn = dm_get_pathname(p->dm_dmp);
	if (pn && BU_STR_EQUAL(argv[1], bu_vls_cstr(pn))) {
	    set_curr_dm(s, p);

	    if (s->mged_curr_dm->dm_tie)
		curr_cmd_list = s->mged_curr_dm->dm_tie;
	    else
		curr_cmd_list = &head_cmd_list;

	    if (s->gedp != GED_NULL)
		s->gedp->ged_gvp = view_state->vs_gvp;

	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interpreter, "Unrecognized pathname - ", argv[1],
		     "\n", (char *)NULL);
    return TCL_ERROR;
}


void
mged_global_variable_setup(struct mged_state *s)
{
    Tcl_LinkVar(s->interp, "mged_default(dlist)", (char *)&mged_default_dlist, TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "mged_default(db_warn)", (char *)&mged_global_db_ctx.db_warn, TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "mged_default(db_upgrade)", (char *)&mged_global_db_ctx.db_upgrade, TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "mged_default(db_version)", (char *)&mged_global_db_ctx.db_version, TCL_LINK_INT);

    Tcl_LinkVar(s->interp, "edit_class", (char *)&s->edit_state.e_edclass, TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "edit_solid_flag", (char *)&es_edflag, TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "edit_object_flag", (char *)&edobj, TCL_LINK_INT);

    /* link some tcl variables to these corresponding globals */
    Tcl_LinkVar(s->interp, "glob_compat_mode", (char *)&glob_compat_mode, TCL_LINK_BOOLEAN);
    Tcl_LinkVar(s->interp, "output_as_return", (char *)&output_as_return, TCL_LINK_BOOLEAN);
}


void
mged_global_variable_teardown(struct mged_state *s)
{
    Tcl_UnlinkVar(s->interp, "mged_default(dlist)");
    Tcl_UnlinkVar(s->interp, "mged_default(db_warn)");
    Tcl_UnlinkVar(s->interp, "mged_default(db_upgrade)");
    Tcl_UnlinkVar(s->interp, "mged_default(db_version)");

    Tcl_UnlinkVar(s->interp, "edit_class");
    Tcl_UnlinkVar(s->interp, "edit_solid_flag");
    Tcl_UnlinkVar(s->interp, "edit_object_flag");

    Tcl_UnlinkVar(s->interp, "glob_compat_mode");
    Tcl_UnlinkVar(s->interp, "output_as_return");
}


int
f_bomb(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    char buffer[1024] = {0};
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc > 1) {
	argc--; argv++;

	bu_vls_from_argv(&vls, argc, argv);
	snprintf(buffer, 1024, "%s", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    bu_exit(EXIT_FAILURE, "%s", buffer);

    /* This is never reached */

    return TCL_OK;
}


/**
 *@brief
 * Called when the named proc created by rt_gettrees() is destroyed.
 */
static void
wdb_deleteProc_rt(void *clientData)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    RT_AP_CHECK(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_free_rti(rtip);
    ap->a_rt_i = (struct rt_i *)NULL;

    bu_free((void *)ap, "struct application");
}


int
cmd_rt_gettrees(ClientData clientData, Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct rt_i *rtip;
    struct application *ap;
    struct resource *resp;
    const char *newprocname;

    CHECK_DBI_NULL;
    RT_CK_WDB(s->wdbp);
    RT_CK_DBI(s->wdbp->dbip);

    if (argc < 3) {
        struct bu_vls vls;

        bu_vls_init(&vls);
        bu_vls_printf(&vls, "helplib_alias wdb_rt_gettrees %s", argv[0]);
        Tcl_Eval((Tcl_Interp *)s->wdbp->wdb_interp, bu_vls_addr(&vls));
        bu_vls_free(&vls);
        return TCL_ERROR;
    }

    rtip = rt_new_rti(s->wdbp->dbip);
    newprocname = argv[1];

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand((Tcl_Interp *)s->wdbp->wdb_interp, newprocname);

    while (argc > 2 && argv[2][0] == '-') {
        if (BU_STR_EQUAL(argv[2], "-i")) {
            rtip->rti_dont_instance = 1;
            argc--;
            argv++;
            continue;
        }
        if (BU_STR_EQUAL(argv[2], "-u")) {
            rtip->useair = 1;
            argc--;
            argv++;
            continue;
        }
        break;
    }

    if (argc-2 < 1) {
        Tcl_AppendResult((Tcl_Interp *)s->wdbp->wdb_interp,
                         "rt_gettrees(): no geometry has been specified ", (char *)NULL);
        return TCL_ERROR;
    }

    if (rt_gettrees(rtip, argc-2, (const char **)&argv[2], 1) < 0) {
        Tcl_AppendResult((Tcl_Interp *)s->wdbp->wdb_interp,
                         "rt_gettrees() returned error", (char *)NULL);
        rt_free_rti(rtip);
        return TCL_ERROR;
    }

    /* Establish defaults for this rt_i */
    rtip->rti_hasty_prep = 1;   /* Tcl isn't going to fire many rays */

    /*
     * In case of multiple instances of the library, make sure that
     * each instance has a separate resource structure,
     * because the bit vector lengths depend on # of solids.
     * And the "overwrite" sequence in Tcl is to create the new
     * proc before running the Tcl_CmdDeleteProc on the old one,
     * which in this case would trash rt_uniresource.
     * Once on the rti_resources list, rt_clean() will clean 'em up.
     */
    BU_ALLOC(resp, struct resource);
    rt_init_resource(resp, 0, rtip);
    BU_ASSERT(BU_PTBL_GET(&rtip->rti_resources, 0) != NULL);

    BU_ALLOC(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_magic = RT_AP_MAGIC;
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    ap->a_purpose = "Conquest!";

    rt_ck(rtip);

    /* Instantiate the proc, with clientData of wdb */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand((Tcl_Interp *)s->wdbp->wdb_interp, newprocname, tclcad_rt,
                            (ClientData)ap, wdb_deleteProc_rt);

    /* Return new function name as result */
    Tcl_AppendResult((Tcl_Interp *)s->wdbp->wdb_interp, newprocname, (char *)NULL);

    return TCL_OK;

}


int
cmd_nmg_collapse(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    const char *av[3];
    int ret;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    av[0] = "e";
    av[1] = argv[2];
    av[2] = NULL;

    return cmd_draw(clientData, interpreter, 2, av);
}


/**
 * Change the local units of the description.  Base unit is fixed in
 * mm, so this just changes the current local unit that the user works
 * in.
 */
int
cmd_units(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;
    fastf_t sf;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    if (s->dbip == DBI_NULL) {
	bu_log("Cannot run 'units' without a database open.\n");
	return TCL_ERROR;
    }

    sf = s->dbip->dbi_base2local;
    ret = ged_exec(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    set_localunit_TclVar(s);
    sf = s->dbip->dbi_base2local / sf;
    update_grids(s,sf);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    return TCL_OK;
}


/**
 * Search command in the style of the Unix find command for db
 * objects.
 */
int
cmd_search(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    return TCL_OK;

}

/**
 * "tol" displays current settings
 * "tol abs #" sets absolute tolerance.  # > 0.0
 * "tol rel #" sets relative tolerance.  0.0 < # < 1.0
 * "tol norm #" sets normal tolerance, in degrees.
 * "tol dist #" sets calculational distance tolerance
 * "tol perp #" sets calculational normal tolerance.
 */
int
cmd_tol(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    s->tol.tol = s->wdbp->wdb_tol;
    s->tol.ttol = s->wdbp->wdb_ttol;

    /* hack to keep mged tolerance settings current */
    s->tol.abs_tol = s->tol.ttol.abs;
    s->tol.rel_tol = s->tol.ttol.rel;
    s->tol.nrm_tol = s->tol.ttol.norm;

    return TCL_OK;
}


/* defined in chgview.c */
extern int edit_com(struct mged_state *s, int argc, const char *argv[]);

/**
 * Run ged_blast, then update the views
 * Format: B object
 */
int
cmd_blast(ClientData clientData, Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    ret = ged_exec(s->gedp, argc, argv);
    if (ret)
	return TCL_ERROR;

    /* update and resize the views */
    struct mged_dm *save_m_dmp = s->mged_curr_dm;
    struct cmd_list *save_cmd_list = curr_cmd_list;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	int non_empty = 0; /* start out empty */

	set_curr_dm(s, m_dmp);

	if (s->mged_curr_dm->dm_tie) {
	    curr_cmd_list = s->mged_curr_dm->dm_tie;
	} else {
	    curr_cmd_list = &head_cmd_list;
	}

	s->gedp->ged_gvp = view_state->vs_gvp;

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));

	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
		non_empty = 1;
		break;
	    }

	    gdlp = next_gdlp;
	}

	if (mged_variables->mv_autosize && non_empty) {
	    struct view_ring *vrp;
	    const char *av[1] = {"autoview"};
	    ged_exec_autoview(s->gedp, 1, (const char **)av);

	    (void)mged_svbase(s);

	    for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
		vrp->vr_scale = view_state->vs_gvp->gv_scale;
	    }
	}
    }

    set_curr_dm(s, save_m_dmp);
    curr_cmd_list = save_cmd_list;
    s->gedp->ged_gvp = view_state->vs_gvp;

    return TCL_OK;
}


/**
 * Edit something (add to visible display).  Format: e object
 */
int
cmd_draw(ClientData clientData, Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bview *gvp = NULL;

    if (s->gedp)
	gvp = s->gedp->ged_gvp;

    if (gvp && DMP) {
	gvp->gv_width = dm_get_width(DMP);
	gvp->gv_height = dm_get_height(DMP);
    }

    return edit_com(s, argc, argv);
}


/**
 * Format: ev objects
 */
int
cmd_ev(ClientData clientData,
       Tcl_Interp *UNUSED(interpreter),
       int argc,
       const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    return edit_com(s, argc, argv);
}


/**
 * The "Big E" command.  Evaluated Edit something (add to visible
 * display).  Usage: E object(s)
 */
int
cmd_E(ClientData clientData,
      Tcl_Interp *UNUSED(interpreter),
      int argc,
      const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    return edit_com(s, argc, argv);
}


int
cmd_shaded_mode(ClientData clientData,
		Tcl_Interp *interpreter,
		int argc,
		const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;

    /* check to see if we have -a or -auto */
    if (argc == 3 &&
	strlen(argv[1]) >= 2 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'a') {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	/* set zbuffer, zclip and lighting for all */
	bu_vls_printf(&vls, "mged_shaded_mode_helper %s", argv[2]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	/* skip past -a */
	--argc;
	++argv;
    }

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    GED_OUTPUT;

    if (ret)
	return TCL_ERROR;

    return TCL_OK;
}


int
cmd_has_embedded_fb(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int UNUSED(argc), const char **UNUSED(argv))
{
#ifdef USE_FBSERV
    Tcl_AppendResult(interpreter, "1", NULL);
#else
    Tcl_AppendResult(interpreter, "0", NULL);
#endif

    return TCL_OK;
}


int
cmd_ps(ClientData clientData,
       Tcl_Interp *interpreter,
       int UNUSED(argc),
       const char **UNUSED(argv))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret = 0;
    const char *av[2] = {"process", "list"};
    ret = ged_exec(s->gedp, 2, (const char **)av);
    /* For the next couple releases, print a rename notice */
    mged_pr_output(interpreter);
    Tcl_AppendResult(interpreter, "(Note: former 'ps' command has been renamed to 'postscript')\n", NULL);
    Tcl_AppendResult(interpreter, bu_vls_addr(s->gedp->ged_result_str), NULL);
    return (ret) ? TCL_ERROR : TCL_OK;
}


int
cmd_stub(ClientData clientData, Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc != 1) {
        struct bu_vls vls;
        bu_vls_init(&vls);
        bu_vls_printf(&vls, "helplib_alias wdb_%s %s", argv[0], argv[0]);
        Tcl_Eval((Tcl_Interp *)s->wdbp->wdb_interp, bu_vls_addr(&vls));
        bu_vls_free(&vls);
        return TCL_ERROR;
    }

    Tcl_AppendResult((Tcl_Interp *)s->wdbp->wdb_interp, "%s: no database is currently opened!", argv[0], (char *)NULL);
    return TCL_ERROR;
}

/**
 * Stuff a string to stdout while leaving the current command-line
 * alone
 */
int
cmd_stuff_str(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    size_t i;

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel stuff_str");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->classic_mged) {
	bu_log("\r%s\n", argv[1]);
	pr_prompt(s);
	bu_log("%s", bu_vls_addr(&s->input_str));
	pr_prompt(s);
	for (i = 0; i < s->input_str_index; ++i)
	    bu_log("%c", bu_vls_addr(&s->input_str)[i]);
    }

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
