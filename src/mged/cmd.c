/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "dg.h"
#include "tclcad.h"

#include "./mged.h"
#include "./cmd.h"
#include "./mged_dm.h"
#include "./sedit.h"


extern void update_grids(fastf_t sf);		/* in grid.c */
extern void set_localunit_TclVar(void);		/* in chgmodel.c */
extern void init_qray(void);			/* in qray.c */
extern int gui_setup(const char *dstr);		/* in attach.c */
extern int mged_default_dlist;			/* in attach.c */
extern int classic_mged;
struct cmd_list head_cmd_list;
struct cmd_list *curr_cmd_list;

extern int mged_db_warn;
extern int mged_db_upgrade;
extern int mged_db_version;

int glob_compat_mode = 1;
int output_as_return = 1;

Tk_Window tkwin = NULL;

/* The following is for GUI output hooks: contains name of function to
 * run with output.
 */
static struct bu_vls tcl_output_hook;


/**
 * G U I _ O U T P U T
 *
 * Used as a hook for bu_log output.  Sends output to the Tcl
 * procedure whose name is contained in the vls "tcl_output_hook".
 * Useful for user interface building.
 */
int
gui_output(genptr_t clientData, genptr_t str)
{
    int len;
    Tcl_DString tclcommand;
    Tcl_Obj *save_result;
    static int level = 0;

    if (level > 50) {
	bu_log_delete_hook(gui_output, clientData);
	/* Now safe to run bu_log? */
	bu_log("Ack! Something horrible just happened recursively.\n");
	return 0;
    }

    Tcl_DStringInit(&tclcommand);
    (void)Tcl_DStringAppendElement(&tclcommand, bu_vls_addr(&tcl_output_hook));
    (void)Tcl_DStringAppendElement(&tclcommand, str);

    save_result = Tcl_GetObjResult(INTERP);
    Tcl_IncrRefCount(save_result);
    ++level;
    Tcl_Eval((Tcl_Interp *)clientData, Tcl_DStringValue(&tclcommand));
    --level;
    Tcl_SetObjResult(INTERP, save_result);
    Tcl_DecrRefCount(save_result);

    Tcl_DStringFree(&tclcommand);

    len = (int)strlen(str);
    return len;
}


int
cmd_ged_edit_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    const char *av[3];
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

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
    int i, ret;
    const char *av[3];
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;


    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

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


int
cmd_ged_info_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char **av;
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;

    if (argc >= 2) {
        (void)(*ctp->ged_func)(gedp, argc, (const char **)argv);
        Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);
    } else {
        if ((argc == 1) && (STATE == ST_S_EDIT)) {
	    argc = 2;
	    av = (const char **)bu_malloc(sizeof(char *)*(argc + 1), "f_list: av");
	    av[0] = (const char *)argv[0];
	    av[1] = (const char *)LAST_SOLID(illump)->d_namep;
	    av[argc] = (const char *)NULL;
	    (void)(*ctp->ged_func)(gedp, argc, (const char **)av);
	    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);
	    bu_free(av, "cmd_ged_info_wrapper: av");
        } else {
	    (void)(*ctp->ged_func)(gedp, argc, (const char **)argv);    
	    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);
        }
    }

    return TCL_OK;
}


int
cmd_ged_erase_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    solid_list_callback();
    update_views = 1;

    return TCL_OK;
}


int
cmd_ged_gqa(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    char **vp;
    int i;
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    size_t args;

    if (gedp == GED_NULL)
	return TCL_OK;

    args = argc + 2 + ged_count_tops(gedp);
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gedp->ged_gdp->gd_rt_cmd[0];

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
	gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
	*vp = 0;
	vp = &gedp->ged_gdp->gd_rt_cmd[0];
	while (*vp)
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(gedp->ged_result_str, "\n");
    } else {
	gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
	gedp->ged_gdp->gd_rt_cmd_len += ged_build_tops(gedp,
						       vp,
						       &gedp->ged_gdp->gd_rt_cmd[args]);
    }

    ret = (*ctp->ged_func)(gedp, gedp->ged_gdp->gd_rt_cmd_len, (const char **)gedp->ged_gdp->gd_rt_cmd);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
    gedp->ged_gdp->gd_rt_cmd = NULL;

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    update_views = 1;

    return TCL_OK;
}


int
cmd_ged_in(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    const char *new_cmd[3];
    int c;
    int do_solid_edit = 0;
    int dont_draw = 0;

    if (gedp == GED_NULL)
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
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "in: option '%c' unknown\n", bu_optopt);
		    Tcl_AppendResult(interpreter, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		}

		break;
	}
    }
    argc -= bu_optind-1;
    argv += bu_optind-1;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    if (ret & GED_MORE)
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (dont_draw) {
	if (ret & GED_HELP || ret == GED_OK)
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
	/* Also kick off solid edit mode */
	new_cmd[0] = "sed";
	new_cmd[1] = argv[1];
	new_cmd[2] = (char *)NULL;
	(void)f_sed(clientData, interpreter, 2, new_cmd);
    }
    return TCL_OK;
}


extern struct rt_db_internal es_int;

int
cmd_ged_inside(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    int arg;
    const char *new_cmd[3];
    struct rt_db_internal intern;
    struct directory *outdp;

    if (gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    RT_DB_INTERNAL_INIT(&intern);

    if (STATE == ST_S_EDIT) {
	/* solid edit mode */
	/* apply es_mat editing to parameters */
	transform_editing_solid(&intern, es_mat, &es_int, 0);
	outdp = LAST_SOLID(illump);

	if (argc < 2) {
	    Tcl_AppendResult(interpreter, "You are in Primitive Edit mode, using edited primitive as outside primitive: ", (char *)NULL);
	    add_solid_path_to_result(interpreter, illump);
	    Tcl_AppendResult(interpreter, "\n", (char *)NULL);
	}

	arg = 1;
	ret = ged_inside_internal(gedp, &intern, argc, argv, arg, outdp->d_namep);
    }  else if (STATE == ST_O_EDIT) {
	mat_t newmat;

	/* object edit mode */
	if (illump->s_Eflag) {
	    Tcl_AppendResult(interpreter, "Cannot find inside of a processed (E'd) region\n",
			     (char *)NULL);
	    (void)signal(SIGINT, SIG_IGN);
	    return TCL_ERROR;
	}
	/* use the solid at bottom of path (key solid) */
	/* apply es_mat and modelchanges editing to parameters */
	bn_mat_mul(newmat, modelchanges, es_mat);
	transform_editing_solid(&intern, newmat, &es_int, 0);
	outdp = LAST_SOLID(illump);

	if (argc < 2) {
	    Tcl_AppendResult(interpreter, "You are in Object Edit mode, using key solid as outside solid: ", (char *)NULL);
	    add_solid_path_to_result(interpreter, illump);
	    Tcl_AppendResult(interpreter, "\n", (char *)NULL);
	}

	arg = 1;
	ret = ged_inside_internal(gedp, &intern, argc, argv, arg, outdp->d_namep);
    } else {
	arg = 2;
	ret = ged_inside(gedp, argc, (const char **)argv);
    }

    if (ret & GED_MORE)
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

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
    const char *new_cmd[3];

    if (gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    if (ret & GED_MORE)
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    /* draw the "inside" solid */
    new_cmd[0] = "draw";
    if (ctp->ged_func == ged_3ptarb)
	new_cmd[1] = argv[1];
    else if (ctp->ged_func == ged_inside)
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

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    if (ret & GED_MORE)
	Tcl_AppendResult(interpreter, MORE_ARGS_STR, NULL);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    /* redraw any objects specified that are already drawn */
    if (argc > 1) {
	int who_ret;
	const char *who_cmd[2] = {"who", NULL};

	who_ret = ged_who(gedp, 1, who_cmd);
	if (who_ret == GED_OK) {
	    /* worst possible is a bunch of 1-char names, allocate and
	     * split into an argv accordingly.
	     */

	    int i, j;
	    char *str = bu_strdup(bu_vls_addr(gedp->ged_result_str));
	    size_t who_argc = (bu_vls_strlen(gedp->ged_result_str) / 2) + 1;
	    char **who_argv = (char **)bu_calloc(who_argc, sizeof(char *), "who_argv");

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
    }
    
    if (ret & GED_HELP || ret == GED_OK)
	return TCL_OK;

    return TCL_ERROR;
}


int
cmd_ged_view_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;

    if (!gedp->ged_gvp)
	gedp->ged_gvp = view_state->vs_gvp;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret)
	return TCL_ERROR;

    (void)mged_svbase();
    view_state->vs_flag = 1;

    return TCL_OK;
}

int
cmd_ged_dm_wrapper(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    struct cmdtab *ctp = (struct cmdtab *)clientData;

    if (gedp == GED_NULL)
	return TCL_OK;

    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    if (!gedp->ged_gvp)
	gedp->ged_gvp = view_state->vs_gvp;
    gedp->ged_dmp = (void *)curr_dm_list->dml_dmp;

    ret = (*ctp->ged_func)(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    (void)signal(SIGINT, SIG_IGN);

    if (ret & GED_HELP || ret == GED_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * C M D _ T K
 *
 * Command for initializing the Tk window and defaults.
 *
 * Usage:  loadtk [displayname[.screennum]]
 */
int
cmd_tk(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int status;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help loadtk");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1)
	status = gui_setup((char *)NULL);
    else
	status = gui_setup(argv[1]);

    return status;
}


/**
 * C M D _ O U T P U T _ H O O K
 *
 * Hooks the output to the given output hook.  Removes the existing
 * output hook!
 */
int
cmd_output_hook(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_vls infocommand;
    int status;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel output_hook");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_log_delete_hook(gui_output, (genptr_t)interpreter);/* Delete the existing hook */

    if (argc < 2)
	return TCL_OK;

    /* Make sure the command exists before putting in the hook! */
    /* Note - the parameters to proc could be wrong and/or the proc
     * could still disappear later.
     */
    bu_vls_init(&infocommand);
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
	Tcl_AppendResult(interpreter, "Don't be silly.", (char *)NULL);
	return TCL_ERROR;
    }

    /* Set up the hook! */
    bu_vls_init(&tcl_output_hook);
    bu_vls_strcpy(&tcl_output_hook, argv[1]);
    bu_log_add_hook(gui_output, (genptr_t)interpreter);

    Tcl_ResetResult(interpreter);
    return TCL_OK;
}


/**
 * C M D _ N O P
 */
int
cmd_nop(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), const char *UNUSED(argv[]))
{
    return TCL_OK;
}


int
cmd_cmd_win(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

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
	    clp->cl_tie->dml_tie = CMD_LIST_NULL;
	bu_vls_free(&clp->cl_more_default);
	bu_vls_free(&clp->cl_name);
	bu_free((genptr_t)clp, "cmd_close: clp");

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
	    curr_dm_list = curr_cmd_list->cl_tie;

	    if (gedp != GED_NULL)
		gedp->ged_gvp = view_state->vs_gvp;
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
	struct bu_vls vls;

	bu_log("Unrecognized option [%s]\n", argv[1]);

	bu_vls_init(&vls);
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
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel set_more_default");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_strcpy(&curr_cmd_list->cl_more_default, argv[1]);
    return TCL_OK;
}


/**
 * debackslash, backslash_specials, mged_compat: routines for original
 * mged emulation mode
 */
void
debackslash(struct bu_vls *dest, struct bu_vls *src)
{
    char *ptr;

    ptr = bu_vls_addr(src);
    while (*ptr) {
	if (*ptr == '\\')
	    ++ptr;
	if (*ptr == '\0')
	    break;
	bu_vls_putc(dest, *ptr++);
    }
}


void
backslash_specials(struct bu_vls *dest, struct bu_vls *src)
{
    int backslashed;
    char *ptr, buf[2];

    buf[1] = '\0';
    backslashed = 0;
    for (ptr = bu_vls_addr(src); *ptr; ptr++) {
	if (*ptr == '[' && !backslashed)
	    bu_vls_strcat(dest, "\\[");
	else if (*ptr == ']' && !backslashed)
	    bu_vls_strcat(dest, "\\]");
	else if (backslashed) {
	    bu_vls_strcat(dest, "\\");
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	    backslashed = 0;
	} else if (*ptr == '\\')
	    backslashed = 1;
	else {
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	}
    }
}


/**
 * M G E D _ C O M P A T
 *
 * This routine is called to perform wildcard expansion and character
 * quoting on the given vls (typically input from the keyboard.)
 */
void
mged_compat(struct bu_vls *dest, struct bu_vls *src, int use_first)
{
    char *start, *end;          /* Start and ends of words */
    int regexp;                 /* Set to TRUE when word is a regexp */
    int backslashed;
    int firstword;
    struct bu_vls word;         /* Current word being processed */
    struct bu_vls temp;

    if (dbip == DBI_NULL) {
	bu_vls_vlscat(dest, src);
	return;
    }

    bu_vls_init(&word);
    bu_vls_init(&temp);

    start = end = bu_vls_addr(src);
    firstword = 1;
    while (*end != '\0') {
	/* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while (*start != '\0') {
	    if (*start == ' '  ||
		*start == '\t' ||
		*start == '\n')
		bu_vls_putc(dest, *start++);
	    else
		break;
	}
	if (*start == '\0')
	    break;

	/* Next, advance "end" pointer to the end of the word, while
	 * adding each character to the "word" vls.  Also make a note
	 * of any unbackslashed wildcard characters.
	 */

	end = start;
	bu_vls_trunc(&word, 0);
	regexp = 0;
	backslashed = 0;
	while (*end != '\0') {
	    if (*end == ' '  ||
		*end == '\t' ||
		*end == '\n')
		break;
	    if ((*end == '*' || *end == '?' || *end == '[') && !backslashed)
		regexp = 1;
	    if (*end == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	    bu_vls_putc(&word, *end++);
	}

	if (firstword && !use_first)
	    regexp = 0;

	/* Now, if the word was suspected of being a wildcard, try to
	 * match it to the database.
	 */

	if (regexp) {
	    bu_vls_trunc(&temp, 0);
	    if (db_regexp_match_all(&temp, dbip,
				    bu_vls_addr(&word)) == 0) {
		debackslash(&temp, &word);
		backslash_specials(dest, &temp);
	    } else
		bu_vls_vlscat(dest, &temp);
	} else {
	    debackslash(dest, &word);
	}

	firstword = 0;
    }

    bu_vls_free(&temp);
    bu_vls_free(&word);
}


#ifdef _WIN32
/* limited to seconds only */
void gettimeofday(struct timeval *tp, struct timezone *tzp)
{

    time_t ltime;

    time(&ltime);

    tp->tv_sec = ltime;
    tp->tv_usec = 0;

}
#endif


/**
 * C M D L I N E
 *
 * This routine is called to process a vls full of commands.  Each
 * command is newline terminated.  The input string will not be
 * altered in any way.
 *
 * Returns -
 * !0 when a prompt needs to be printed.
 * 0 no prompt needed.
 */
int
cmdline(struct bu_vls *vp, int record)
{
    int status;
    struct bu_vls globbed;
    struct bu_vls tmp_vls;
    struct bu_vls save_vp;
    struct timeval start, finish;
    size_t len;
    extern struct bu_vls mged_prompt;
    char *cp;
    const char *result;

    BU_CK_VLS(vp);

    if (bu_vls_strlen(vp) <= 0)
	return CMD_OK;

    bu_vls_init(&globbed);
    bu_vls_init(&tmp_vls);
    bu_vls_init(&save_vp);
    bu_vls_vlscat(&save_vp, vp);

    /* MUST MAKE A BACKUP OF THE INPUT STRING AND USE THAT IN THE CALL
       TO Tcl_Eval!!!

       You never know who might change the string (append to it...)
       (f_mouse is notorious for adding things to the input string) If
       it were to change while it was still being evaluated, Horrible
       Things could happen.
    */

    if (glob_compat_mode) {
	mged_compat(&globbed, vp, 0);
    } else {
	bu_vls_vlscat(&globbed, vp);
    }

    gettimeofday(&start, (struct timezone *)NULL);
    status = Tcl_Eval(INTERP, bu_vls_addr(&globbed));
    gettimeofday(&finish, (struct timezone *)NULL);
    result = Tcl_GetStringResult(INTERP);

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
		    Tcl_Eval(INTERP, bu_vls_addr(&tmp_vls));
		    Tcl_SetResult(INTERP, "", TCL_STATIC);
		}

		if (record)
		    history_record(&save_vp, &start, &finish, CMD_OK);

	    } else {
		/* XXXXXX */
		bu_semaphore_release(BU_SEM_SYSCALL);
		bu_log("\n");
	    }

	    bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
	    status = CMD_OK;
	    goto end;

	case TCL_ERROR:
	default:

	    /* First check to see if it's a secret message. */

	    if ((cp = strstr(result, MORE_ARGS_STR)) != NULL) {
		if (cp == result) {
		    bu_vls_trunc(&mged_prompt, 0);
		    bu_vls_printf(&mged_prompt, "\r%s",
				  result+sizeof(MORE_ARGS_STR)-1);
		} else {
		    len = cp - result;
		    bu_log("%*s%s", len, result, result[len-1] == '\n' ? "" : "\n");
		    bu_vls_trunc(&mged_prompt, 0);
		    bu_vls_printf(&mged_prompt, "\r%s",
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
		history_record(&save_vp, &start, &finish, CMD_BAD);

	    bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
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
mged_print_result(int UNUSED(status))
{
    size_t len;
    const char *result = Tcl_GetStringResult(INTERP);

    len = strlen(result);
    if (len > 0) {
	bu_log("%s%s", result,
	       result[len-1] == '\n' ? "" : "\n");

	pr_prompt(interactive);
    }

    Tcl_ResetResult(INTERP);
}


/**
 * M G E D _ C M D
 *
 * Check a table for the command, check for the correct minimum and
 * maximum number of arguments, and pass control to the proper
 * function.  If the number of arguments is incorrect, print out a
 * short help message.
 */
int
mged_cmd(
    int argc,
    const char *argv[],
    struct funtab in_functions[])
{
    struct funtab *ftp;
    struct funtab *functions;

    if (argc == 0)
	return CMD_OK;	/* No command entered, that's fine */

    /* if no function table is provided, use the default mged function table */
    if (in_functions == (struct funtab *)NULL) {
	bu_log("mged_cmd: failed to supply function table!\n");
	return CMD_BAD;
    } else
	functions = in_functions;

    for (ftp = &functions[1]; ftp->ft_name; ftp++) {
	if (!BU_STR_EQUAL(ftp->ft_name, argv[0]))
	    continue;
	/* We have a match */
	if ((ftp->ft_min <= argc) && (ftp->ft_max < 0 || argc <= ftp->ft_max)) {
	    /* Input has the right number of args.  Call function
	     * listed in table, with main(argc, argv) style args
	     */

	    switch (ftp->ft_func(argc, argv)) {
		case CMD_OK:
		    return CMD_OK;
		case CMD_BAD:
		    return CMD_BAD;
		case CMD_MORE:
		    return CMD_MORE;
		default:
		    Tcl_AppendResult(INTERP, "mged_cmd(): Invalid return from ",
				     ftp->ft_name, "\n", (char *)NULL);
		    return CMD_BAD;
	    }
	}

	Tcl_AppendResult(INTERP, "Usage: ", functions[0].ft_name, ftp->ft_name,
			 " ", ftp->ft_parms, "\n\t(", ftp->ft_comment,
			 ")\n", (char *)NULL);
	return CMD_BAD;
    }

    Tcl_AppendResult(INTERP, functions[0].ft_name, argv[0],
		     ": no such command, type '", functions[0].ft_name,
		     "?' for help\n", (char *)NULL);
    return CMD_BAD;
}


/**
 * Let the user temporarily escape from the editor Format: %
 */
int
f_comm(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{

    if (argc != 1 || !classic_mged || curr_cmd_list != &head_cmd_list) {
	struct bu_vls vls;

	bu_vls_init(&vls);
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
	    mged_finish(11);
	}

	while ((rpid = wait(&retcode)) != pid && rpid != -1)
	    ;
    }
#endif

    Tcl_AppendResult(INTERP, "Returning to MGED\n", (char *)NULL);

    return TCL_OK;
}


/**
 * Quit and exit gracefully. Format: q
 */
int
f_quit(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (STATE != ST_VIEW)
	button(BE_REJECT);

    quit();			/* Exiting time */
    /* NOTREACHED */
    return TCL_OK;
}


/**
 * H E L P C O M M
 *
 * Common code for help commands
 */
HIDDEN int
helpcomm(int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;
    int i, bad;

    bad = 0;

    /* Help command(s) */
    for (i=1; i<argc; i++) {
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    if (!BU_STR_EQUAL(ftp->ft_name, argv[i]))
		continue;

	    Tcl_AppendResult(INTERP, "Usage: ", functions->ft_name, ftp->ft_name,
			     " ", ftp->ft_parms, "\n\t(", ftp->ft_comment, ")\n", (char *)NULL);
	    break;
	}
	if (!ftp->ft_name) {
	    Tcl_AppendResult(INTERP, functions->ft_name, argv[i],
			     ": no such command, type '", functions->ft_name,
			     "?' for help\n", (char *)NULL);
	    bad = 1;
	}
    }

    return bad ? TCL_ERROR : TCL_OK;
}


/**
 * F _ H E L P
 *
 * Print a help message, two lines for each command.  Or, help with
 * the indicated commands.
 */
int
f_help2(int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;

    if (argc <= 1) {
	Tcl_AppendResult(INTERP, "The following commands are available:\n", (char *)NULL);
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    Tcl_AppendResult(INTERP,  functions->ft_name, ftp->ft_name, " ",
			     ftp->ft_parms, "\n\t(", ftp->ft_comment, ")\n", (char *)NULL);
	}
	return TCL_OK;
    }
    return helpcomm(argc, argv, functions);
}


int
f_fhelp2(int argc, const char *argv[], struct funtab *functions)
{
    struct funtab *ftp;
    struct bu_vls str;

    if (argc <= 1) {
	bu_vls_init(&str);
	Tcl_AppendResult(INTERP, "The following ", functions->ft_name,
			 " commands are available:\n", (char *)NULL);
	for (ftp = functions+1; ftp->ft_name; ftp++) {
	    vls_col_item(&str, ftp->ft_name);
	}
	vls_col_eol(&str);
	Tcl_AppendResult(INTERP, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);
	return TCL_OK;
    }
    return helpcomm(argc, argv, functions);
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
    struct dm_list *dlp;
    struct bu_vls vls;

    bu_vls_init(&vls);

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
		bu_vls_printf(&vls, "%V %V", &clp->cl_name,
			      &clp->cl_tie->dml_dmp->dm_pathName);
		Tcl_AppendElement(interpreter, bu_vls_addr(&vls));
	    } else {
		bu_vls_printf(&vls, "%V {}", &clp->cl_name);
		Tcl_AppendElement(interpreter, bu_vls_addr(&vls));
	    }
	}

	bu_vls_trunc(&vls, 0);
	if (clp->cl_tie) {
	    bu_vls_printf(&vls, "%V %V", &clp->cl_name,
			  &clp->cl_tie->dml_dmp->dm_pathName);
	    Tcl_AppendElement(interpreter, bu_vls_addr(&vls));
	} else {
	    bu_vls_printf(&vls, "%V {}", &clp->cl_name);
	    Tcl_AppendElement(interpreter, bu_vls_addr(&vls));
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
	    clp->cl_tie->dml_tie = (struct cmd_list *)NULL;

	clp->cl_tie = (struct dm_list *)NULL;

	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* print out the display manager that we're tied to */
    if (argc == 2) {
	if (clp->cl_tie)
	    Tcl_AppendElement(interpreter, bu_vls_addr(&clp->cl_tie->dml_dmp->dm_pathName));
	else
	    Tcl_AppendElement(interpreter, "");

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (*argv[2] != '.')
	bu_vls_printf(&vls, ".%s", argv[2]);
    else
	bu_vls_strcpy(&vls, argv[2]);

    FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
	if (!bu_vls_strcmp(&vls, &dlp->dml_dmp->dm_pathName))
	    break;

    if (dlp == &head_dm_list) {
	Tcl_AppendResult(interpreter, "f_tie: unrecognized pathName - ",
			 bu_vls_addr(&vls), "\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* already tied */
    if (clp->cl_tie)
	clp->cl_tie->dml_tie = (struct cmd_list *)NULL;

    clp->cl_tie = dlp;

    /* already tied */
    if (dlp->dml_tie)
	dlp->dml_tie->cl_tie = (struct dm_list *)NULL;

    dlp->dml_tie = clp;

    bu_vls_free(&vls);
    return TCL_OK;
}


int
f_ps(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int status;
    const char *av[2];
    struct dm_list *dml;
    struct _view_state *vsp;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help ps");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (gedp == GED_NULL)
	return TCL_OK;

    dml = curr_dm_list;
    gedp->ged_gvp = view_state->vs_gvp;
    status = mged_attach(&which_dm[DM_PS_INDEX], argc, argv);
    if (status == TCL_ERROR)
	return TCL_ERROR;

    vsp = view_state;  /* save state info pointer */

    bu_free((genptr_t)menu_state, "f_ps: menu_state");
    menu_state = dml->dml_menu_state;

    scroll_top = dml->dml_scroll_top;
    scroll_active = dml->dml_scroll_active;
    scroll_y = dml->dml_scroll_y;
    memmove((void *)scroll_array, (void *)dml->dml_scroll_array, sizeof(struct scroll_item *) * 6);

    dirty = 1;
    refresh();

    view_state = vsp;  /* restore state info pointer */
    av[0] = "release";
    av[1] = NULL;
    status = f_release(clientData, interpreter, 1, av);
    curr_dm_list = dml;
    gedp->ged_gvp = view_state->vs_gvp;

    return status;
}


/**
 * Experimental - like f_plot except we attach to dm-plot, passing
 * along any arguments.
 */
int
f_pl(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int status;
    const char *av[2];
    struct dm_list *dml;
    struct _view_state *vsp;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help pl");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (gedp == GED_NULL)
	return TCL_OK;

    dml = curr_dm_list;
    gedp->ged_gvp = view_state->vs_gvp;
    status = mged_attach(&which_dm[DM_PLOT_INDEX], argc, argv);
    if (status == TCL_ERROR)
	return TCL_ERROR;

    vsp = view_state;  /* save state info pointer */
    view_state = dml->dml_view_state;  /* use dml's state info */
    *mged_variables = *dml->dml_mged_variables; /* struct copy */

    bu_free((genptr_t)menu_state, "f_pl: menu_state");
    menu_state = dml->dml_menu_state;

    scroll_top = dml->dml_scroll_top;
    scroll_active = dml->dml_scroll_active;
    scroll_y = dml->dml_scroll_y;
    memmove((void *)scroll_array, (void *)dml->dml_scroll_array, sizeof(struct scroll_item *) * 6);

    dirty = 1;
    refresh();

    view_state = vsp;  /* restore state info pointer */
    av[0] = "release";
    av[1] = NULL;
    status = f_release(clientData, interpreter, 1, av);
    curr_dm_list = dml;
    gedp->ged_gvp = view_state->vs_gvp;

    return status;
}


int
f_winset(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct dm_list *p;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel winset");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* print pathname of drawing window with primary focus */
    if (argc == 1) {
	Tcl_AppendResult(interpreter, bu_vls_addr(&pathName), (char *)NULL);
	return TCL_OK;
    }

    /* change primary focus to window argv[1] */
    FOR_ALL_DISPLAYS(p, &head_dm_list.l) {
	if (BU_STR_EQUAL(argv[1], bu_vls_addr(&p->dml_dmp->dm_pathName))) {
	    curr_dm_list = p;

	    if (curr_dm_list->dml_tie)
		curr_cmd_list = curr_dm_list->dml_tie;
	    else
		curr_cmd_list = &head_cmd_list;

	    if (gedp != GED_NULL)
		gedp->ged_gvp = view_state->vs_gvp;

	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interpreter, "Unrecognized pathname - ", argv[1],
		     "\n", (char *)NULL);
    return TCL_ERROR;
}


void
mged_global_variable_setup(Tcl_Interp *interpreter)
{
    Tcl_LinkVar(interpreter, "mged_default(dlist)", (char *)&mged_default_dlist, TCL_LINK_INT);
    Tcl_LinkVar(interpreter, "mged_default(db_warn)", (char *)&mged_db_warn, TCL_LINK_INT);
    Tcl_LinkVar(interpreter, "mged_default(db_upgrade)", (char *)&mged_db_upgrade, TCL_LINK_INT);
    Tcl_LinkVar(interpreter, "mged_default(db_version)", (char *)&mged_db_version, TCL_LINK_INT);

    Tcl_LinkVar(interpreter, "edit_class", (char *)&es_edclass, TCL_LINK_INT);
    Tcl_LinkVar(interpreter, "edit_solid_flag", (char *)&es_edflag, TCL_LINK_INT);
    Tcl_LinkVar(interpreter, "edit_object_flag", (char *)&edobj, TCL_LINK_INT);
}


void
mged_global_variable_teardown(Tcl_Interp *interpreter)
{
    Tcl_UnlinkVar(interpreter, "mged_default(dlist)");
    Tcl_UnlinkVar(interpreter, "mged_default(mged_db_warn)");
    Tcl_UnlinkVar(interpreter, "mged_default(mged_db_upgrade)");
    Tcl_UnlinkVar(interpreter, "mged_default(mged_db_version)");

    Tcl_UnlinkVar(interpreter, "edit_class");
    Tcl_UnlinkVar(interpreter, "edit_solid_flag");
    Tcl_UnlinkVar(interpreter, "edit_object_flag");
}


int
f_bomb(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    char buffer[1024] = {0};
    struct bu_vls vls;

    if (argc > 1) {
	argc--; argv++;

	bu_vls_init(&vls);
	bu_vls_from_argv(&vls, argc, argv);
	snprintf(buffer, 1024, "%s", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    bu_exit(EXIT_FAILURE, buffer);

    /* This is never reached */

    return TCL_OK;
}


int
cmd_rt_gettrees(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    CHECK_DBI_NULL;

    return wdb_rt_gettrees_cmd(wdbp, interpreter, argc, argv);
}


int
cmd_nmg_collapse(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    const char *av[3];
    int ret;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_nmg_collapse(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    av[0] = "e";
    av[1] = argv[2];
    av[2] = NULL;

    return cmd_draw(clientData, interpreter, 2, av);
}


/**
 * C M D _ U N I T S
 *
 * Change the local units of the description.  Base unit is fixed in
 * mm, so this just changes the current local unit that the user works
 * in.
 */
int
cmd_units(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;
    fastf_t sf;

    if (gedp == GED_NULL)
	return TCL_OK;

    if (dbip == DBI_NULL) {
	bu_log("Cannot run 'units' without a database open.\n");
	return TCL_ERROR;
    }

    sf = dbip->dbi_base2local;
    ret = ged_units(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    set_localunit_TclVar();
    sf = dbip->dbi_base2local / sf;
    update_grids(sf);
    update_views = 1;

    return TCL_OK;
}


/**
 * C M D _ S E A R C H 
 *
 * Search command in the style of the Unix find command for db
 * objects.
 */
int
cmd_search(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_search(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    return TCL_OK;

}


/**
 * C M D _ L M
 *
 * List regions based on values of their MUVES_Component attribute
 */
int
cmd_lm(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_attribute_value_set avs;
    struct bu_vls vls;
    struct bu_ptbl *tbl;
    struct directory *dp;
    size_t i;
    int last_opt=0;
    int new_arg_count=1;
    int new_argc;
    int ret;
    char **new_argv;

    bu_vls_init(&vls);
    bu_vls_strcat(&vls, argv[0]);
    for (i=1; i<(size_t)argc; i++) {
	if (*argv[i] == '-') {
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, argv[i]);
	    last_opt = i;
	    new_arg_count++;
	} else {
	    break;
	}
    }

    bu_avs_init(&avs, argc - last_opt, "cmd_lm avs");
    for (i=last_opt+1; i<(size_t)argc; i++) {
	bu_avs_add_nonunique(&avs, "MUVES_Component", argv[i]);
    }

    tbl = db_lookup_by_attr(dbip, RT_DIR_REGION, &avs, 2);
    if (!tbl) {
	Tcl_AppendResult(interpreter, "ERROR: db_lookup_by_attr() failed!\n", (char *)NULL);
	bu_vls_free(&vls);
	bu_avs_free(&avs);
	return TCL_ERROR;
    }

    if (BU_PTBL_LEN(tbl) == 0) {
	/* no matches */
	bu_vls_free(&vls);
	bu_avs_free(&avs);
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "cmd_lm ptbl");
	return TCL_OK;
    }

    for (i=0; i<BU_PTBL_LEN(tbl); i++) {
	dp = (struct directory *)BU_PTBL_GET(tbl, i);
	bu_vls_putc(&vls, ' ');
	bu_vls_strcat(&vls, dp->d_namep);
	new_arg_count++;
    }

    bu_ptbl_free(tbl);
    bu_free((char *)tbl, "cmd_lm ptbl");

    /* create a new argc and argv to pass to the cmd_ls routine */
    new_argv = (char **)bu_calloc(new_arg_count+1, sizeof(char *), "cmd_lm new_argv");
    new_argc = bu_argv_from_string(new_argv, new_arg_count, bu_vls_addr(&vls));

    ret = ged_ls(gedp, new_argc, (const char **)new_argv);
    bu_vls_free(&vls);
    bu_free((char *)new_argv, "cmd_lm new_argv");

    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (!ret)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * F _ T O L
 *
 * "tol" displays current settings
 * "tol abs #" sets absolute tolerance.  # > 0.0
 * "tol rel #" sets relative tolerance.  0.0 < # < 1.0
 * "tol norm #" sets normal tolerance, in degrees.
 * "tol dist #" sets calculational distance tolerance
 * "tol perp #" sets calculational normal tolerance.
 */
int
cmd_tol(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    int ret;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_tol(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    /* hack to keep mged tolerance settings current */
    mged_ttol = wdbp->wdb_ttol;
    mged_tol = wdbp->wdb_tol;
    mged_abs_tol = mged_ttol.abs;
    mged_rel_tol = mged_ttol.rel;
    mged_nrm_tol = mged_ttol.norm;

    return TCL_OK;
}


/* defined in chgview.c */
extern int edit_com(int argc, const char *argv[], int kind, int catch_sigint);

/**
 * ZAP the display -- then edit anew
 * Format: B object
 */
int
cmd_blast(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    const char *av[2];
    int ret;

    if (gedp == GED_NULL)
	return TCL_OK;

    av[0] = "Z";
    av[1] = (char *)0;

    ret = ged_zap(gedp, 1, av);
    if (ret)
	return TCL_ERROR;
        
    if (argc == 1) /* "B" alone is same as "Z" */
	return TCL_OK;

    return edit_com(argc, argv, 1, 1);
}


/**
 * Edit something (add to visible display).  Format: e object
 */
int
cmd_draw(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interpreter), int argc, const char *argv[])
{
    return edit_com(argc, argv, 1, 1);
}


extern int emuves_com(int argc, const char *argv[]);	/* from chgview.c */

/**
 * Add regions with attribute MUVES_Component haveing the specified values
 * Format: em value [value value ...]
 */
int
cmd_emuves(ClientData UNUSED(clientData),
	   Tcl_Interp *UNUSED(interpreter),
	   int argc,
	   const char *argv[])
{
    return emuves_com(argc, argv);
}


/**
 * Format: ev objects
 */
int
cmd_ev(ClientData UNUSED(clientData),
       Tcl_Interp *UNUSED(interpreter),
       int argc,
       const char *argv[])
{
    return edit_com(argc, argv, 3, 1);
}


/**
 * C M D _ E
 *
 * The "Big E" command.  Evaluated Edit something (add to visible
 * display).  Usage: E object(s)
 */
int
cmd_E(ClientData UNUSED(clientData),
      Tcl_Interp *UNUSED(interpreter),
      int argc,
      const char *argv[])
{
    return edit_com(argc, argv, 2, 1);
}


int
cmd_shaded_mode(ClientData UNUSED(clientData),
		Tcl_Interp *interpreter,
		int argc,
		const char *argv[])
{
    int ret;

    /* check to see if we have -a or -auto */
    if (argc == 3 &&
	strlen(argv[1]) >= 2 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'a') {
	struct bu_vls vls;

	/* set zbuffer, zclip and lighting for all */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "mged_shaded_mode_helper %s", argv[2]);
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	/* skip past -a */
	--argc;
	++argv;
    }

    ret = ged_shaded_mode(gedp, argc, (const char **)argv);
    Tcl_AppendResult(interpreter, bu_vls_addr(gedp->ged_result_str), NULL);

    if (ret)
	return TCL_ERROR;

    return TCL_OK;
}


/* XXX needs to be provided from points header */
extern int parse_point_file(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[]);
#ifndef BC_WITH_PARSERS
int
cmd_parse_points(ClientData UNUSED(clientData),
		 Tcl_Interp *UNUSED(interpreter),
		 int UNUSED(argc),
		 const char *UNUSED(argv[]))
{

    bu_log("parse_points was disabled in this compilation of mged due to system limitations\n");
    return TCL_ERROR;
}
#else
int
cmd_parse_points(ClientData clientData,
		 Tcl_Interp *interpreter,
		 int argc,
		 const char *argv[])
{
   if (argc != 2) {
	bu_log("parse_points only supports a single file name right now\n");
	bu_log("doing nothing\n");
	return TCL_ERROR;
    }
    return parse_point_file(clientData, interpreter, argc-1, &(argv[1]));
}
#endif


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
cmd_stub(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    CHECK_DBI_NULL;

    return wdb_stub_cmd(wdbp, interpreter, argc, argv);
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
