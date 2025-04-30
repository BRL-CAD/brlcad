/*                           M G E D . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @file mged/mged.c
 *
 * Main logic of the Multiple-display Graphics EDitor (MGED)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#ifdef HAVE_SYS_TYPES_H
/* for select */
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
/* for select */
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#ifdef HAVE_WINDOWS_H
#  include <direct.h> /* For chdir */
#endif

#include "bio.h"
#include "bsocket.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/debug.h"
#include "bu/units.h"
#include "bu/version.h"
#include "bu/time.h"
#include "bu/snooze.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#ifndef HAVE_WINDOWS_H
#  define LIBTERMIO_IMPLEMENTATION
#  include "libtermio.h"
#endif
#include "bv/util.h"
#include "ged.h"
#include "tclcad.h"

/* private */
#include "./mged.h"
#include "./sedit.h"
#include "./menu.h"
#include "./mged_dm.h"
#include "./cmd.h"
#include "./f_cmd.h" // for f_opendb
#include "brlcad_ident.h"

#ifndef COMMAND_LINE_EDITING
#  define COMMAND_LINE_EDITING 1
#endif

#define SPACES "                                                                                                                                                                                                                                                                                                           "


extern void draw_e_axes(struct mged_state *);
extern void draw_m_axes(struct mged_state *);
extern void draw_v_axes(struct mged_state *);

/* defined in chgmodel.c */
extern void set_localunit_TclVar(void);

/* defined in history.c */
extern struct bu_vls *history_prev(const char *);
extern struct bu_vls *history_cur(const char *);
extern struct bu_vls *history_next(const char *);

/* Ew. Global. */
/* defined in dozoom.c */
extern unsigned char geometry_default_color[];

/* Ew. Global. */
/* defined in set.c */
extern struct _mged_variables default_mged_variables;

/* Ew. Global. */
/* defined in color_scheme.c */
extern struct _color_scheme default_color_scheme;

/* Ew. Global. */
/* defined in grid.c */
extern struct bv_grid_state default_grid_state;

/* Ew. Global. */
/* defined in axes.c */
extern struct _axes_state default_axes_state;

/* Ew. Global. */
/* defined in rect.c */
extern struct _rubber_band default_rubber_band;

/* these two file descriptors are where we store fileno(stdout) and
 * fileno(stderr) during graphical startup so that we may restore them
 * when we're done (which is needed so atexit() calls to bu_log() will
 * still work).
 */
/* Ew. Global. */
static int stdfd[2] = {1, 2};

/* Container for passing I/O data through Tcl callbacks */
struct stdio_data {
    long fd;
    Tcl_Channel chan;
    struct mged_state *s;
};

struct mged_state *MGED_STATE = NULL;

jmp_buf jmp_env;	/* For non-local gotos */
/* Ew. Global. */
double frametime;	/* time needed to draw last frame */

/* Ew. Global. */
struct rt_wdb rtg_headwdb;  /* head of database object list */

void (*cur_sigint)(int);	/* Current SIGINT status */

/* Ew. Global. */
int cbreak_mode = 0;    /* >0 means in cbreak_mode */


/* The old mged gui is temporarily the default. */
/* Ew. Global. */
int old_mged_gui=1;

static int
mged_bomb_hook(void *clientData, void *data)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char *str = (char *)data;
    Tcl_Interp *interpreter = (Tcl_Interp *)clientData;

    bu_vls_printf(&vls, "set mbh_dialog [Dialog .#auto -modality application];");
    bu_vls_printf(&vls, "$mbh_dialog hide 1; $mbh_dialog hide 2; $mbh_dialog hide 3;");
    bu_vls_printf(&vls, "label [$mbh_dialog childsite].l -text {%s};", str);
    bu_vls_printf(&vls, "pack [$mbh_dialog childsite].l;");
    bu_vls_printf(&vls, "update; $mbh_dialog activate");
    Tcl_Eval(interpreter, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


static void
notify_parent_done(int parent) {
    int buffer[2] = {0};

    if (write(parent, buffer, 1) == -1) {
	perror("Unable to write to communication pipe");
    }
    if (close(parent) == -1) {
	perror("Unable to close communication pipe");
    }

    return;
}


void
mgedInvalidParameterHandler(const wchar_t* UNUSED(expression),
			    const wchar_t* UNUSED(function),
			    const wchar_t* UNUSED(file),
			    unsigned int UNUSED(line),
			    uintptr_t UNUSED(pReserved))
{
/*
 * Windows, I think you're number one!
 */
}


void
pr_prompt(struct mged_state *s)
{
    if (s->interactive)
	bu_log("%s", bu_vls_addr(&s->mged_prompt));
}


void
pr_beep(void)
{
    bu_log("%c", 7);
}


/* so the Windows-specific calls blend in */
#if !defined(_WIN32) || defined(__CYGWIN__)
void _set_invalid_parameter_handler(void (*callback)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)) { if (callback) return; }
#endif




/*
 * attaches the specified display manager
 */
static void
attach_display_manager(Tcl_Interp *interpreter, const char *manager, const char *display)
{
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    bu_vls_printf(&tcl_cmd, "attach ");
    if (display && strlen(display) > 0)
	bu_vls_printf(&tcl_cmd, "-d %s ", display);
    if (!manager)
	manager = "nu";
    bu_vls_printf(&tcl_cmd, "%s", manager);
    Tcl_Eval(interpreter, bu_vls_cstr(&tcl_cmd));
    bu_vls_free(&tcl_cmd);
    bu_log("%s\n", Tcl_GetStringResult(interpreter));
}


static void
mged_notify(int UNUSED(i))
{
    pr_prompt(MGED_STATE);
}


void
reset_input_strings(struct mged_state *s)
{
    if (BU_LIST_IS_HEAD(curr_cmd_list, &head_cmd_list.l)) {
	/* Truncate input string */
	bu_vls_trunc(&s->input_str, 0);
	bu_vls_trunc(&s->input_str_prefix, 0);
	bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);
	s->input_str_index = 0;

	curr_cmd_list->cl_quote_string = 0;
	bu_vls_strcpy(&s->mged_prompt, MGED_PROMPT);
	bu_log("\n");
	pr_prompt(s);
    } else {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "reset_input_strings");
	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
}





void
sig2(int UNUSED(sig))
{
    reset_input_strings(MGED_STATE);

    (void)signal(SIGINT, SIG_IGN);
}


void
sig3(int UNUSED(sig))
{
    (void)signal(SIGINT, SIG_IGN);
    longjmp(jmp_env, 1);
}


void
new_edit_mats(struct mged_state *s)
{
    struct mged_dm *save_dm_list;

    save_dm_list = s->mged_curr_dm;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (!p->dm_owner)
	    continue;

	set_curr_dm(s, p);
	bn_mat_mul(view_state->vs_model2objview, view_state->vs_gvp->gv_model2view, s->edit_state.model_changes);
	bn_mat_inv(view_state->vs_objview2model, view_state->vs_model2objview);
	view_state->vs_flag = 1;
    }

    set_curr_dm(s, save_dm_list);
}


void
mged_view_callback(struct bview *gvp,
		   void *clientData)
{
    struct mged_state *s = MGED_STATE;
    struct _view_state *vsp = (struct _view_state *)clientData;

    if (!gvp)
	return;

    if (s->edit_state.global_editing_state != ST_VIEW) {
	bn_mat_mul(vsp->vs_model2objview, gvp->gv_model2view, s->edit_state.model_changes);
	bn_mat_inv(vsp->vs_objview2model, vsp->vs_model2objview);
    }
    vsp->vs_flag = 1;
    dm_set_dirty(s->mged_curr_dm->dm_dmp, 1);
}


/**
 * Derive the inverse and editing matrices, as required.  Centralized
 * here to simplify things.
 */
void
new_mats(struct mged_state *s)
{
    bv_update(view_state->vs_gvp);
}


/**
 * If an mgedrc file exists, open it and process the commands within.
 * Look first for a Shell environment variable, then for a file in the
 * user's home directory, and finally in the current directory.
 *
 * Returns -
 * -1 FAIL
 *  0 OK
 */
static int
do_rc(struct mged_state *s)
{
    FILE *fp = NULL;
    char *path;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    int bogus;

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".mgedrc"

    if ((path = getenv(ENVRC)) != (char *)NULL) {
	if ((fp = fopen(path, "r")) != NULL) {
	    bu_vls_strcpy(&str, path);
	}
    }

    if (!fp) {
	if ((path = getenv("HOME")) != (char *)NULL) {
	    bu_vls_strcpy(&str, path);
	    bu_vls_strcat(&str, "/");
	    bu_vls_strcat(&str, RCFILE);

	    fp = fopen(bu_vls_addr(&str), "r");
	}
    }

    if (!fp) {
	if ((fp = fopen(RCFILE, "r")) != NULL) {
	    bu_vls_strcpy(&str, RCFILE);
	}
    }

    /* At this point, if none of the above attempts panned out, give up. */

    if (!fp) {
	bu_vls_free(&str);
	return -1;
    }

    bogus = 0;
    while (!feof(fp)) {
	char buf[80];

	/* Get beginning of line */
	bu_fgets(buf, 80, fp);
	/* If the user has a set command with an equal sign, remember to warn */
	if (strstr(buf, "set") != NULL)
	    if (strchr(buf, '=') != NULL) {
		bogus = 1;
		break;
	    }
    }

    fclose(fp);
    if (bogus) {
	bu_log("\nWARNING: The new format of the \"set\" command is:\n");
	bu_log("    set varname value\n");
	bu_log("If you are setting variables in your %s, you will ", RCFILE);
	bu_log("need to change those\ncommands.\n\n");
    }
    if (Tcl_EvalFile(s->interp, bu_vls_addr(&str)) != TCL_OK) {
	bu_log("Error reading %s:\n%s\n", RCFILE,
	       Tcl_GetVar(s->interp, "errorInfo", TCL_GLOBAL_ONLY));
    }

    bu_vls_free(&str);

    /* No telling what the commands may have done to the result string -
     * make sure we start with a clean slate */
    bu_vls_trunc(s->gedp->ged_result_str, 0);
    return 0;
}


static void
mged_insert_char(struct mged_state *s, char ch)
{
    if (s->input_str_index == bu_vls_strlen(&s->input_str)) {
	bu_log("%c", (int)ch);
	bu_vls_putc(&s->input_str, (int)ch);
	++s->input_str_index;
    } else {
	struct bu_vls temp = BU_VLS_INIT_ZERO;

	bu_vls_strcat(&temp, bu_vls_addr(&s->input_str)+s->input_str_index);
	bu_vls_trunc(&s->input_str, s->input_str_index);
	bu_log("%c%s", (int)ch, bu_vls_addr(&temp));
	pr_prompt(s);
	bu_vls_putc(&s->input_str, (int)ch);
	bu_log("%s", bu_vls_addr(&s->input_str));
	bu_vls_vlscat(&s->input_str, &temp);
	++s->input_str_index;
	bu_vls_free(&temp);
    }
}


static void
do_tab_expansion(struct mged_state *s)
{
    int ret;
    Tcl_Obj *result;
    Tcl_Obj *newCommand;
    Tcl_Obj *matches;
    int numExpansions=0;
    struct bu_vls tab_expansion = BU_VLS_INIT_ZERO;

    bu_vls_printf(&tab_expansion, "tab_expansion {%s}", bu_vls_addr(&s->input_str));
    ret = Tcl_Eval(s->interp, bu_vls_addr(&tab_expansion));
    bu_vls_free(&tab_expansion);

    if (ret == TCL_OK) {
	result = Tcl_GetObjResult(s->interp);
	Tcl_ListObjIndex(s->interp, result, 0, &newCommand);
	Tcl_ListObjIndex(s->interp, result, 1, &matches);
	Tcl_ListObjLength(s->interp, matches, &numExpansions);
	if (numExpansions > 1) {
	    /* show the possible matches */
	    bu_log("\n%s\n", Tcl_GetString(matches));
	}

	/* display the expanded line */
	pr_prompt(s);
	s->input_str_index = 0;
	bu_vls_trunc(&s->input_str, 0);
	bu_vls_strcat(&s->input_str, Tcl_GetString(newCommand));

	/* only one match remaining, pad space so we can keep going */
	if (numExpansions == 1)
	    bu_vls_strcat(&s->input_str, " ");

	s->input_str_index = bu_vls_strlen(&s->input_str);
	bu_log("%s", bu_vls_addr(&s->input_str));
    } else {
	bu_log("ERROR\n");
	bu_log("%s\n", Tcl_GetStringResult(s->interp));
    }
}


/* Process character */
static void
mged_process_char(struct mged_state *s, char ch)
{
    struct bu_vls *vp = (struct bu_vls *)NULL;
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    static int escaped = 0;
    static int bracketed = 0;
    static int tilded = 0;
    static int freshline = 1;

#define CTRL_A      1
#define CTRL_B      2
#define CTRL_D      4
#define CTRL_E      5
#define CTRL_F      6
#define CTRL_K      11
#define CTRL_L      12
#define CTRL_N      14
#define CTRL_P      16
#define CTRL_T      20
#define CTRL_U      21
#define CTRL_W      '\027'
#define ESC         27
#define BACKSPACE   '\b'
#ifdef _WIN32
# undef DELETE
#endif
#define DELETE      127


    /* bu_log("KEY: %d (esc=%d, brk=%d)\n", ch, escaped, bracketed); */

    /* ANSI sequence */
    if (escaped && bracketed) {

	/* arrow keys */
	if (ch == 'A') ch = CTRL_P;
	if (ch == 'B') ch = CTRL_N;
	if (ch == 'C') ch = CTRL_F;
	if (ch == 'D') ch = CTRL_B;

	/* Mac forward delete key */
	if (ch == '3') {
	    tilded = 1;
	    ch = CTRL_D;
	}

	escaped = bracketed = 0;
    }

    switch (ch) {
	case ESC:           /* Used for building up ANSI arrow keys */
	    escaped = 1;
	    break;
	case '\n':          /* Carriage return or line feed */
	case '\r':
	    bu_log("\n");   /* Display newline */

	    /* If there are any characters already in the command
	     * string (left over from a CMD_MORE), then prepend them
	     * to the new input.
	     */

	    /* If no input and a default is supplied then use it */
	    if (!bu_vls_strlen(&s->input_str) && bu_vls_strlen(&curr_cmd_list->cl_more_default))
		bu_vls_printf(&s->input_str_prefix, "%s%s\n",
			      bu_vls_strlen(&s->input_str_prefix) > 0 ? " " : "",
			      bu_vls_addr(&curr_cmd_list->cl_more_default));
	    else {
		if (curr_cmd_list->cl_quote_string)
		    bu_vls_printf(&s->input_str_prefix, "%s\"%s\"\n",
				  bu_vls_strlen(&s->input_str_prefix) > 0 ? " " : "",
				  bu_vls_addr(&s->input_str));
		else
		    bu_vls_printf(&s->input_str_prefix, "%s%s\n",
				  bu_vls_strlen(&s->input_str_prefix) > 0 ? " " : "",
				  bu_vls_addr(&s->input_str));
	    }

	    curr_cmd_list->cl_quote_string = 0;
	    bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);

	    /* If this forms a complete command (as far as the Tcl
	     * parser is concerned) then execute it.
	     */

#ifdef USE_TCL_CHAN
	    /*XXX Nothing yet */
	    bu_log("WARNING: not running cmdline(%s)\n", bu_vls_cstr(&s->input_str_prefix));
#else
	    if (Tcl_CommandComplete(bu_vls_addr(&s->input_str_prefix))) {
		curr_cmd_list = &head_cmd_list;
		if (curr_cmd_list->cl_tie)
		    set_curr_dm(s, curr_cmd_list->cl_tie);

		reset_Tty(fileno(stdin)); /* Backwards compatibility */
		(void)signal(SIGINT, SIG_IGN);
		if (cmdline(s, &s->input_str_prefix, 1) == CMD_MORE) {
		    /* Remove newline */
		    bu_vls_trunc(&s->input_str_prefix,
				 bu_vls_strlen(&s->input_str_prefix)-1);
		    bu_vls_trunc(&s->input_str, 0);
		    (void)signal(SIGINT, sig2);
		    /*
		   *** The mged_prompt vls now contains prompt for
		   *** more input.
		   */
		} else {
		    /* All done; clear all strings. */
		    bu_vls_trunc(&s->input_str_prefix, 0);
		    bu_vls_trunc(&s->input_str, 0);
		    (void)signal(SIGINT, SIG_IGN);
		}
		set_Cbreak(fileno(stdin)); /* Back to single-character mode */
		clr_Echo(fileno(stdin));
	    } else {
		bu_vls_trunc(&s->input_str, 0);
		bu_vls_strcpy(&s->mged_prompt, "\r? ");

		/* Allow the user to hit ^C */
		(void)signal(SIGINT, sig2);
	    }
#endif
	    pr_prompt(s); /* Print prompt for more input */
	    s->input_str_index = 0;
	    freshline = 1;
	    escaped = bracketed = 0;
	    break;
	case DELETE:
	case BACKSPACE:
	    if (s->input_str_index == 0) {
		pr_beep();
		break;
	    }

	    if (s->input_str_index == bu_vls_strlen(&s->input_str)) {
		bu_log("\b \b");
		bu_vls_trunc(&s->input_str, bu_vls_strlen(&s->input_str)-1);
	    } else {
		bu_vls_strcat(&temp, bu_vls_addr(&s->input_str)+s->input_str_index);
		bu_vls_trunc(&s->input_str, s->input_str_index-1);
		bu_log("\b%s ", bu_vls_addr(&temp));
		pr_prompt(s);
		bu_log("%s", bu_vls_addr(&s->input_str));
		bu_vls_vlscat(&s->input_str, &temp);
		bu_vls_free(&temp);
	    }
	    --s->input_str_index;
	    escaped = bracketed = 0;
	    break;
	case '\t':                      /* do TAB expansion */
	    do_tab_expansion(s);
	    break;
	case CTRL_A:                    /* Go to beginning of line */
	    pr_prompt(s);
	    s->input_str_index = 0;
	    escaped = bracketed = 0;
	    break;
	case CTRL_E:                    /* Go to end of line */
	    if (s->input_str_index < bu_vls_strlen(&s->input_str)) {
		bu_log("%s", bu_vls_addr(&s->input_str)+s->input_str_index);
		s->input_str_index = bu_vls_strlen(&s->input_str);
	    }
	    escaped = bracketed = 0;
	    break;
	case CTRL_D:                    /* Delete character at cursor */
	    if (s->input_str_index == bu_vls_strlen(&s->input_str) && s->input_str_index != 0) {
		pr_beep(); /* Beep if at end of input string */
		break;
	    }
	    if (s->input_str_index == bu_vls_strlen(&s->input_str) && s->input_str_index == 0) {
		/* act like a usual shell, quit if the command prompt is empty */
		bu_log("exit\n");
		quit(s);
	    }

	    bu_vls_strcpy(&temp, bu_vls_addr(&s->input_str)+s->input_str_index+1);
	    bu_vls_trunc(&s->input_str, s->input_str_index);
	    bu_log("%s ", bu_vls_addr(&temp));
	    pr_prompt(s);
	    bu_log("%s", bu_vls_addr(&s->input_str));
	    bu_vls_vlscat(&s->input_str, &temp);
	    bu_vls_free(&temp);
	    escaped = bracketed = 0;
	    break;
	case CTRL_U:                   /* Delete whole line */
	    pr_prompt(s);
	    bu_vls_strncpy(&temp, SPACES, bu_vls_strlen(&s->input_str));
	    bu_log("%s", bu_vls_addr(&temp));
	    bu_vls_free(&temp);
	    pr_prompt(s);
	    bu_vls_trunc(&s->input_str, 0);
	    s->input_str_index = 0;
	    escaped = bracketed = 0;
	    break;
	case CTRL_K:                    /* Delete to end of line */
	    bu_vls_strncpy(&temp, SPACES, bu_vls_strlen(&s->input_str)-s->input_str_index);
	    bu_log("%s", bu_vls_addr(&temp));
	    bu_vls_free(&temp);
	    bu_vls_trunc(&s->input_str, s->input_str_index);
	    pr_prompt(s);
	    bu_log("%s", bu_vls_addr(&s->input_str));
	    escaped = bracketed = 0;
	    break;
	case CTRL_L:                   /* Redraw line */
	    bu_log("\n");
	    pr_prompt(s);
	    bu_log("%s", bu_vls_addr(&s->input_str));
	    if (s->input_str_index == bu_vls_strlen(&s->input_str))
		break;
	    pr_prompt(s);
	    bu_log("%*s", (int)s->input_str_index, bu_vls_addr(&s->input_str));
	    escaped = bracketed = 0;
	    break;
	case CTRL_B:                   /* Back one character */
	    if (s->input_str_index == 0) {
		pr_beep();
		break;
	    }
	    --s->input_str_index;
	    bu_log("\b"); /* hopefully non-destructive! */
	    escaped = bracketed = 0;
	    break;
	case CTRL_F:                   /* Forward one character */
	    if (s->input_str_index == bu_vls_strlen(&s->input_str)) {
		pr_beep();
		break;
	    }

	    bu_log("%c", bu_vls_addr(&s->input_str)[s->input_str_index]);
	    ++s->input_str_index;
	    escaped = bracketed = 0;
	    break;
	case CTRL_T:                  /* Transpose characters */
	    if (s->input_str_index == 0) {
		pr_beep();
		break;
	    }
	    if (s->input_str_index == bu_vls_strlen(&s->input_str)) {
		bu_log("\b");
		--s->input_str_index;
	    }
	    ch = bu_vls_addr(&s->input_str)[s->input_str_index];
	    bu_vls_addr(&s->input_str)[s->input_str_index] =
		bu_vls_addr(&s->input_str)[s->input_str_index - 1];
	    bu_vls_addr(&s->input_str)[s->input_str_index - 1] = ch;
	    bu_log("\b");
	    bu_log("%c%c", bu_vls_addr(&s->input_str)[s->input_str_index-1], bu_vls_addr(&s->input_str)[s->input_str_index]);
	    ++s->input_str_index;
	    escaped = bracketed = 0;
	    break;
	case CTRL_N:                  /* Next history command */
	case CTRL_P:                  /* Last history command */
	    /* Work the history routines to get the right string */
	    curr_cmd_list = &head_cmd_list;
	    if (freshline) {
		if (ch == CTRL_P) {
		    vp = history_prev((const char *)NULL);
		    if (vp == NULL) {
			pr_beep();
			break;
		    }
		    bu_vls_trunc(&s->scratchline, 0);
		    bu_vls_vlscat(&s->scratchline, &s->input_str);
		    freshline = 0;
		} else {
		    pr_beep();
		    break;
		}
	    } else {
		if (ch == CTRL_P) {
		    vp = history_prev((const char *)NULL);
		    if (vp == NULL) {
			pr_beep();
			break;
		    }
		} else {
		    vp = history_next((const char *)NULL);
		    if (vp == NULL) {
			vp = &s->scratchline;
			freshline = 1;
		    }
		}
	    }

	    pr_prompt(s);
	    bu_vls_strncpy(&temp, SPACES, bu_vls_strlen(&s->input_str));
	    bu_log("%s", bu_vls_addr(&temp));
	    bu_vls_free(&temp);

	    pr_prompt(s);
	    bu_vls_trunc(&s->input_str, 0);
	    bu_vls_vlscat(&s->input_str, vp);
	    if (bu_vls_strlen(&s->input_str) > 0) {
		if (bu_vls_addr(&s->input_str)[bu_vls_strlen(&s->input_str)-1] == '\n')
		    bu_vls_trunc(&s->input_str, bu_vls_strlen(&s->input_str)-1); /* del \n */
		bu_log("%s", bu_vls_addr(&s->input_str));
		s->input_str_index = bu_vls_strlen(&s->input_str);
	    }
	    escaped = bracketed = 0;
	    break;
	case CTRL_W:                   /* backward-delete-word */
	    {
		char *start;
		char *curr;
		int len;
		struct bu_vls temp2 = BU_VLS_INIT_ZERO;

		start = bu_vls_addr(&s->input_str);
		curr = start + s->input_str_index - 1;

		/* skip spaces */
		while (curr > start && *curr == ' ')
		    --curr;

		/* find next space */
		while (curr > start && *curr != ' ')
		    --curr;

		bu_vls_strcpy(&temp, start+s->input_str_index);

		if (curr == start)
		    s->input_str_index = 0;
		else
		    s->input_str_index = curr - start + 1;

		len = bu_vls_strlen(&s->input_str);
		bu_vls_trunc(&s->input_str, s->input_str_index);
		pr_prompt(s);
		bu_log("%s%s", bu_vls_addr(&s->input_str), bu_vls_addr(&temp));

		bu_vls_strncpy(&temp2, SPACES, len - s->input_str_index);
		bu_log("%s", bu_vls_addr(&temp2));
		bu_vls_free(&temp2);

		pr_prompt(s);
		bu_log("%s", bu_vls_addr(&s->input_str));
		bu_vls_vlscat(&s->input_str, &temp);
		bu_vls_free(&temp);
	    }

	    escaped = bracketed = 0;
	    break;
	case 'd':
	    if (escaped) {
		/* delete-word */
		char *start;
		char *curr;
		int i;
		struct bu_vls temp2 = BU_VLS_INIT_ZERO;

		start = bu_vls_addr(&s->input_str);
		curr = start + s->input_str_index;

		/* skip spaces */
		while (*curr != '\0' && *curr == ' ')
		    ++curr;

		/* find next space */
		while (*curr != '\0' && *curr != ' ')
		    ++curr;

		i = curr - start;
		bu_vls_strcpy(&temp, curr);
		bu_vls_trunc(&s->input_str, s->input_str_index);
		pr_prompt(s);
		bu_log("%s%s", bu_vls_addr(&s->input_str), bu_vls_addr(&temp));

		bu_vls_strncpy(&temp2, SPACES, i - s->input_str_index);
		bu_log("%s", bu_vls_addr(&temp2));
		bu_vls_free(&temp2);

		pr_prompt(s);
		bu_log("%s", bu_vls_addr(&s->input_str));
		bu_vls_vlscat(&s->input_str, &temp);
		bu_vls_free(&temp);
	    } else
		mged_insert_char(s, ch);

	    escaped = bracketed = 0;
	    break;
	case 'f':
	    if (escaped) {
		/* forward-word */
		char *start;
		char *curr;

		start = bu_vls_addr(&s->input_str);
		curr = start + s->input_str_index;

		/* skip spaces */
		while (*curr != '\0' && *curr == ' ')
		    ++curr;

		/* find next space */
		while (*curr != '\0' && *curr != ' ')
		    ++curr;

		s->input_str_index = curr - start;
		bu_vls_strcpy(&temp, start+s->input_str_index);
		bu_vls_trunc(&s->input_str, s->input_str_index);
		pr_prompt(s);
		bu_log("%s", bu_vls_addr(&s->input_str));
		bu_vls_vlscat(&s->input_str, &temp);
		bu_vls_free(&temp);
	    } else
		mged_insert_char(s, ch);

	    escaped = bracketed = 0;
	    break;
	case 'b':
	    if (escaped) {
		/* backward-word */
		char *start;
		char *curr;

		start = bu_vls_addr(&s->input_str);
		curr = start + s->input_str_index - 1;

		/* skip spaces */
		while (curr > start && *curr == ' ')
		    --curr;

		/* find next space */
		while (curr > start && *curr != ' ')
		    --curr;

		if (curr == start)
		    s->input_str_index = 0;
		else
		    s->input_str_index = curr - start + 1;

		bu_vls_strcpy(&temp, start+s->input_str_index);
		bu_vls_trunc(&s->input_str, s->input_str_index);
		pr_prompt(s);
		bu_log("%s", bu_vls_addr(&s->input_str));
		bu_vls_vlscat(&s->input_str, &temp);
		bu_vls_free(&temp);
	    } else
		mged_insert_char(s, ch);

	    escaped = bracketed = 0;
	    break;
	case '[':
	    if (escaped) {
		bracketed = 1;
		break;
	    }

	    /* fall through */
	case '~':
	    if (tilded) {
		/* we were in an escape sequence (Mac delete key),
		 * just ignore the trailing tilde.
		 */
		tilded = 0;
		break;
	    }
	    /* Fall through if not escaped! */
	    /* fall through */
	default:
	    if (!isprint((int)ch))
		break;

	    mged_insert_char(s, ch);
	    escaped = bracketed = 0;
	    break;
    }
}

/**
 * Check for events, and dispatch them.  Eventually, this will be done
 * entirely by generating commands
 *
 * Returns - recommended new value for non_blocking
 */
int
event_check(struct mged_state *s, int non_blocking)
{
    struct mged_dm *save_dm_list;
    int save_edflag;

    /* Let cool Tk event handler do most of the work */

    if (non_blocking) {

	/* When in non_blocking-mode, we want to deal with as many
	 * events as possible before the next redraw (multiple
	 * keypresses, redraw events, etc...
	 */

	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT)) {
	}
    } else {
	/* Wait for an event, then handle it */
	Tcl_DoOneEvent(TCL_ALL_EVENTS);

	/* Handle any other events in the queue */
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT)) {
	}
    }

    non_blocking = 0;

    if (s->dbip == DBI_NULL)
	return non_blocking;

    /*********************************
     * Handle rate-based processing *
     *********************************/
    save_dm_list = s->mged_curr_dm;
    if (s->edit_state.k.rot_m_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->edit_state.k.rot_m_udata);
	save_coords = view_state->vs_gvp->gv_coord;
	view_state->vs_gvp->gv_coord = 'm';

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_ROTATE)
		es_edflag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      s->edit_state.k.origin_m,
		      s->edit_state.k.rot_m[X],
		      s->edit_state.k.rot_m[Y],
		      s->edit_state.k.rot_m[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	view_state->vs_gvp->gv_coord = save_coords;

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (s->edit_state.k.rot_o_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->edit_state.k.rot_o_udata);
	save_coords = view_state->vs_gvp->gv_coord;
	view_state->vs_gvp->gv_coord = 'o';

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_ROTATE)
		es_edflag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      s->edit_state.k.origin_o,
		      s->edit_state.k.rot_o[X],
		      s->edit_state.k.rot_o[Y],
		      s->edit_state.k.rot_o[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	view_state->vs_gvp->gv_coord = save_coords;

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (s->edit_state.k.rot_v_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->edit_state.k.rot_v_udata);
	save_coords = view_state->vs_gvp->gv_coord;
	view_state->vs_gvp->gv_coord = 'v';

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_ROTATE)
		es_edflag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      s->edit_state.k.origin_v,
		      s->edit_state.k.rot_v[X],
		      s->edit_state.k.rot_v[Y],
		      s->edit_state.k.rot_v[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	view_state->vs_gvp->gv_coord = save_coords;

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (s->edit_state.k.tra_m_flag) {
	char save_coords;
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	set_curr_dm(s, s->edit_state.k.tra_m_udata);
	save_coords = view_state->vs_gvp->gv_coord;
	view_state->vs_gvp->gv_coord = 'm';

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_TRAN)
		es_edflag = STRANS;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		      s->edit_state.k.tra_m[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      s->edit_state.k.tra_m[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      s->edit_state.k.tra_m[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	view_state->vs_gvp->gv_coord = save_coords;

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (s->edit_state.k.tra_v_flag) {
	char save_coords;
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	set_curr_dm(s, s->edit_state.k.tra_v_udata);
	save_coords = view_state->vs_gvp->gv_coord;
	view_state->vs_gvp->gv_coord = 'v';

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_TRAN)
		es_edflag = STRANS;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		      s->edit_state.k.tra_v[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      s->edit_state.k.tra_v[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      s->edit_state.k.tra_v[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	view_state->vs_gvp->gv_coord = save_coords;

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (s->edit_state.k.sca_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (s->edit_state.global_editing_state == ST_S_EDIT) {
	    save_edflag = es_edflag;
	    if (!SEDIT_SCALE)
		es_edflag = SSCALE;
	} else {
	    save_edflag = edobj;
	    if (!OEDIT_SCALE)
		edobj = BE_O_SCALE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aS %f\n", s->edit_state.k.sca * 0.01);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else
	    edobj = save_edflag;
    }

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (!p->dm_owner)
	    continue;

	set_curr_dm(s, p);

	if (!view_state->vs_gvp)
	   continue;

	if (view_state->vs_gvp->k.rot_m_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -o %c -i -m ax %f ay %f az %f\n",
			  view_state->vs_gvp->k.origin_m,
			  view_state->vs_gvp->k.rot_m[X],
			  view_state->vs_gvp->k.rot_m[Y],
			  view_state->vs_gvp->k.rot_m[Z]);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->vs_gvp->k.tra_m_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -i -m aX %f aY %f aZ %f\n",
			  view_state->vs_gvp->k.tra_m[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->vs_gvp->k.tra_m[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->vs_gvp->k.tra_m[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->vs_gvp->k.rot_v_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -o %c -i -v ax %f ay %f az %f\n",
			  view_state->vs_gvp->k.origin_v,
			  view_state->vs_gvp->k.rot_v[X],
			  view_state->vs_gvp->k.rot_v[Y],
			  view_state->vs_gvp->k.rot_v[Z]);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->vs_gvp->k.tra_v_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -i -v aX %f aY %f aZ %f",
			  view_state->vs_gvp->k.tra_v[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->vs_gvp->k.tra_v[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->vs_gvp->k.tra_v[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->vs_gvp->k.sca_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "zoom %f",
			  1.0 / (1.0 - (view_state->vs_gvp->k.sca / 10.0)));
	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}

	set_curr_dm(s, save_dm_list);
    }

    return non_blocking;
}

/*
 * standard input handling
 *
 * When the Tk event handler sees input on standard input, it calls
 * the routine "stdin_input" (registered with the
 * Tcl_CreateFileHandler call).  This routine simply appends the new
 * input to a growing string until the command is complete (it is
 * assumed that the routine gets a fill line.)
 *
 * If the command is incomplete, then allow the user to hit ^C to
 * start over, by setting up the multi_line_sig routine as the SIGINT
 * handler.
 */

/**
 * stdin_input
 *
 * Called when a single character is ready for reading on standard
 * input (or an entire line if the terminal is not in cbreak mode.)
 */
void
stdin_input(ClientData clientData, int UNUSED(mask))
{
    int count;
    char ch;
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    struct stdio_data *sd = (struct stdio_data *)clientData;
    struct mged_state *s = sd->s;

    /* When not in cbreak mode, just process an entire line of input,
     * and don't do any command-line manipulation.
     */

    if (!cbreak_mode) {

#ifdef USE_TCL_CHAN
	Tcl_DString ds;
	Tcl_DStringInit(&ds);
	count = Tcl_Gets(sd->chan, &ds);

	if (count < 0) {
	    BU_PUT(sd, struct stdio_data);
	    quit(s); /* does not return */
	}

	bu_vls_strcat(&s->input_str, Tcl_DStringValue(&ds));
	Tcl_DStringFree(&ds);
#else
	/* Get line from stdin */
	if (bu_vls_gets(&temp, stdin) < 0) {
	    BU_PUT(sd, struct stdio_data);
	    quit(s);				/* does not return */
	}
	bu_vls_vlscat(&s->input_str, &temp);
#endif

	/* If there are any characters already in the command string
	 * (left over from a CMD_MORE), then prepend them to the new
	 * input.
	 */

	/* If no input and a default is supplied then use it */
	if (!bu_vls_strlen(&s->input_str) && bu_vls_strlen(&curr_cmd_list->cl_more_default))
	    bu_vls_printf(&s->input_str_prefix, "%s%s\n",
			  bu_vls_strlen(&s->input_str_prefix) > 0 ? " " : "",
			  bu_vls_addr(&curr_cmd_list->cl_more_default));
	else
	    bu_vls_printf(&s->input_str_prefix, "%s%s\n",
			  bu_vls_strlen(&s->input_str_prefix) > 0 ? " " : "",
			  bu_vls_addr(&s->input_str));

	bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);

	/* If a complete line was entered, attempt to execute command. */

	if (Tcl_CommandComplete(bu_vls_addr(&s->input_str_prefix))) {
	    curr_cmd_list = &head_cmd_list;
	    if (curr_cmd_list->cl_tie)
		set_curr_dm(s, curr_cmd_list->cl_tie);

	    if (cmdline(s, &s->input_str_prefix, 1) == CMD_MORE) {
		/* Remove newline */
		bu_vls_trunc(&s->input_str_prefix,
			     bu_vls_strlen(&s->input_str_prefix)-1);
		bu_vls_trunc(&s->input_str, 0);

		(void)signal(SIGINT, sig2);
	    } else {
		bu_vls_trunc(&s->input_str_prefix, 0);
		bu_vls_trunc(&s->input_str, 0);
		(void)signal(SIGINT, SIG_IGN);
	    }
	    pr_prompt(s);
	    s->input_str_index = 0;
	} else {
	    bu_vls_trunc(&s->input_str, 0);
	    /* Allow the user to hit ^C. */
	    (void)signal(SIGINT, sig2);
	}

	bu_vls_free(&temp);
	return;
    }

    /*XXXXX*/
#define TRY_STDIN_INPUT_HACK
#ifdef TRY_STDIN_INPUT_HACK
    /* Grab everything --- assuming everything is <= page size */
    {
	char buf[BU_PAGE_SIZE];
	int idx;
#  ifdef USE_TCL_CHAN
	count = Tcl_Read(sd->chan, buf, BU_PAGE_SIZE);
#  else
	count = read((int)sd->fd, (void *)buf, BU_PAGE_SIZE);
#  endif

#else
	/* Grab single character from stdin */
	count = read((int)sd->fd, (void *)&ch, 1);
#endif

	if (count < 0)
	    perror("READ ERROR");

	if (count <= 0 && feof(stdin))
	    Tcl_Eval(s->interp, "q");

	if (buf[0] == '\0')
	    bu_bomb("Read a buf with a 0 starting it?\n");

#ifdef TRY_STDIN_INPUT_HACK
	/* Process everything in buf */
	for (idx = 0, ch = buf[idx]; idx < count; ch = buf[++idx]) {
	    int c = ch;

	    /* explicit input sanitization */
	    if (c < 0)
		c = 0;
	    if (c > CHAR_MAX)
		c = CHAR_MAX;
#endif
	    mged_process_char(s, c);
#ifdef TRY_STDIN_INPUT_HACK
	}
    }
#endif
}

void
std_out_or_err(ClientData clientData, int UNUSED(mask))
{
    // TODO - we're already using clientData for something else, and experience
    // with fbserv makes me wary of trying to change what is being passed
    // through clientData with an fd - for now, just punt and use the overall
    // state global.
    struct mged_state *s = MGED_STATE;

#ifdef USE_TCL_CHAN
    Tcl_Channel chan = (Tcl_Channel)clientData;
    Tcl_DString ds;
#else
    int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */
#endif
    int count;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char line[RT_MAXLINE+1] = {0};
    Tcl_Obj *save_result;

    /* Get data from stdout or stderr */

#ifdef USE_TCL_CHAN
    Tcl_DStringInit(&ds);
    count = Tcl_Gets(chan, &ds);
    bu_strlcpy(line, Tcl_DStringValue(&ds), RT_MAXLINE);
#else
    count = read((int)fd, line, RT_MAXLINE);
#endif

    if (count <= 0) {
	if (count < 0) {
	    perror("READ ERROR");
	}
	return;
    }

    line[count] = '\0';

    save_result = Tcl_GetObjResult(s->interp);
    Tcl_IncrRefCount(save_result);

    bu_vls_printf(&vls, "output_callback {%s}", line);
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    Tcl_SetObjResult(s->interp, save_result);
    Tcl_DecrRefCount(save_result);
}



/**
 * NOTE that this routine is not to be casually used to refresh the
 * screen.  The normal procedure for screen refresh is to manipulate
 * the necessary global variables, and wait for refresh to be called
 * at the bottom of the while loop in main().  However, when it is
 * absolutely necessary to flush a change in the solids table out to
 * the display in the middle of a routine somewhere (such as the "B"
 * command processor), then this routine may be called.
 *
 * If you don't understand the ramifications of using this routine,
 * then you don't want to call it.
 */
void
refresh(struct mged_state *s)
{
    struct mged_dm *save_dm_list;
    struct bu_vls overlay_vls = BU_VLS_INIT_ZERO;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    int do_overlay = 1;
    int64_t elapsed_time, start_time = bu_gettime();
    int do_time = 0;

    /* Print any text output that has accumulated to the command prompt
     * TODO - this is currently a no-op because the gui_output callback
     * is still in the old form of trying to immediately print the bu_log
     * output to the interp. */
    mged_pr_output(s->interp);

    /* Display Manager / Views */
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (!p->dm_view_state)
	    continue;
	if (s->update_views || p->dm_view_state->vs_flag)
	    p->dm_dirty = 1;
    }

    /*
     * This needs to be done separately because dm_view_state may be
     * shared.
     */
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (!p->dm_view_state)
	    continue;
	p->dm_view_state->vs_flag = 0;
    }

    s->update_views = 0;

    save_dm_list = s->mged_curr_dm;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	/*
	 * if something has changed, then go update the display.
	 * Otherwise, we are happy with the view we have
	 */
	set_curr_dm(s, p);
	if (mapped && DMP_dirty) {
	    int restore_zbuffer = 0;

	    if (mged_variables->mv_fb &&
		dm_get_zbuffer(DMP)) {
		restore_zbuffer = 1;
		(void)dm_make_current(DMP);
		(void)dm_set_zbuffer(DMP, 0);
	    }

	    DMP_dirty = 0;
	    do_time = 1;
	    VMOVE(geometry_default_color, color_scheme->cs_geo_def);

	    if (s->dbip != DBI_NULL) {
		if (do_overlay) {
		    bu_vls_trunc(&overlay_vls, 0);
		    create_text_overlay(s, &overlay_vls);
		    do_overlay = 0;
		}

		/* XXX VR hack */
		if (viewpoint_hook)  (*viewpoint_hook)();
	    }

	    if (mged_variables->mv_predictor)
		predictor_frame(s);

	    if (dm_get_dirty(DMP)) {

		dm_draw_begin(DMP);	/* update displaylist prolog */

		if (s->dbip != DBI_NULL) {
		    /* do framebuffer underlay */
		    if (mged_variables->mv_fb && !mged_variables->mv_fb_overlay) {
			if (mged_variables->mv_fb_all)
			    fb_refresh(fbp, 0, 0, dm_get_width(DMP), dm_get_height(DMP));
			else if (mged_variables->mv_mouse_behavior != 'z')
			    paint_rect_area(s);
		    }

		    /* do framebuffer overlay for entire window */
		    if (mged_variables->mv_fb &&
			    mged_variables->mv_fb_overlay &&
			    mged_variables->mv_fb_all) {
			fb_refresh(fbp, 0, 0, dm_get_width(DMP), dm_get_height(DMP));

			if (restore_zbuffer)
			    dm_set_zbuffer(DMP, 1);
		    } else {
			if (restore_zbuffer)
			    dm_set_zbuffer(DMP, 1);

			/* Draw each solid in its proper place on the
			 * screen by applying zoom, rotation, &
			 * translation.  Calls dm_loadmatrix() and
			 * dm_draw_vlist().
			 */

			if (dm_get_stereo(DMP) == 0 ||
				mged_variables->mv_eye_sep_dist <= 0) {
			    /* Normal viewing */
			    dozoom(s, 0);
			} else {
			    /* Stereo viewing */
			    dozoom(s, 1);
			    dozoom(s, 2);
			}

			/* do framebuffer overlay in rectangular area */
			if (mged_variables->mv_fb &&
				mged_variables->mv_fb_overlay &&
				mged_variables->mv_mouse_behavior != 'z')
			    paint_rect_area(s);
		    }


		    /* Restore to non-rotated, full brightness */
		    dm_hud_begin(DMP);

		    /* only if not doing overlay */
		    if (!mged_variables->mv_fb ||
			    mged_variables->mv_fb_overlay != 2) {
			if (rubber_band->rb_active || rubber_band->rb_draw)
			    draw_rect(s);

			if (grid_state->draw)
			    draw_grid(s);

			/* Compute and display angle/distance cursor */
			if (adc_state->adc_draw)
			    adcursor(s);

			if (axes_state->ax_view_draw)
			    draw_v_axes(s);

			if (axes_state->ax_model_draw)
			    draw_m_axes(s);

			if (axes_state->ax_edit_draw &&
				(s->edit_state.global_editing_state == ST_S_EDIT || s->edit_state.global_editing_state == ST_O_EDIT))
			    draw_e_axes(s);

			/* Display titles, etc., if desired */
			bu_vls_strcpy(&tmp_vls, bu_vls_addr(&overlay_vls));
			dotitles(s, &tmp_vls);
			bu_vls_trunc(&tmp_vls, 0);
		    }
		}

		/* only if not doing overlay */
		if (!mged_variables->mv_fb ||
			mged_variables->mv_fb_overlay != 2) {
		    /* Draw center dot */
		    dm_set_fg(DMP,
			    color_scheme->cs_center_dot[0],
			    color_scheme->cs_center_dot[1],
			    color_scheme->cs_center_dot[2], 1, 1.0);
		    dm_draw_point_2d(DMP, 0.0, 0.0);
		}

		dm_draw_end(DMP);
		dm_set_dirty(DMP, 0);

	    }
	}
    }

    /* a frame was drawn */
    if (do_time) {
	elapsed_time = bu_gettime() - start_time;
	/* Only use reasonable measurements */
	if (elapsed_time > 10LL && elapsed_time < 30000000LL) {
	    /* Smoothly transition to new speed */
	    frametime = 0.9 * frametime + 0.1 * elapsed_time / 1000000LL;
	}
    }

    set_curr_dm(s, save_dm_list);

    bu_vls_free(&overlay_vls);
    bu_vls_free(&tmp_vls);
}


/**
 * This routine should be called in place of exit() everywhere in GED,
 * to permit an accurate finish time to be recorded in the (ugh)
 * logfile, also to remove the device access lock.
 */
void
mged_finish(struct mged_state *s, int exitcode)
{
    char place[64];
    struct cmd_list *c;
    int ret;

    (void)sprintf(place, "exit_status=%d", exitcode);

    /* If we're in script mode, wait for subprocesses to finish before we
     * wrap up */
    if (s->gedp && !s->interactive) {
	struct bu_ptbl rmp = BU_PTBL_INIT_ZERO;
	while (BU_PTBL_LEN(&s->gedp->ged_subp)) {
	    for (size_t i = 0; i < BU_PTBL_LEN(&s->gedp->ged_subp); i++) {
		struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&s->gedp->ged_subp, i);
		if (!bu_process_wait_n(&rrp->p, 1)) {
		    bu_ptbl_ins(&rmp, (long *)rrp);
		}
	    }
	    for (size_t i = 0; i < BU_PTBL_LEN(&rmp); i++) {
		struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&s->gedp->ged_subp, i);
		bu_ptbl_rm(&s->gedp->ged_subp, (long *)rrp);
		BU_PUT(rrp, struct ged_subprocess);
	    }
	    bu_ptbl_reset(&rmp);
	}
    }

    /* Release all displays */
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);

	bu_ptbl_rm(&active_dm_set, (long *)p);

	if (p && p->dm_dmp) {
	    dm_close(p->dm_dmp);
	    BV_FREE_VLIST(s->vlfree, &p->dm_p_vlist);
	    mged_slider_free_vls(p);
	    bu_free(p, "release: mged_curr_dm");
	}

	set_curr_dm(s, MGED_DM_NULL);
    }
    bu_ptbl_free(&active_dm_set);

    for (BU_LIST_FOR (c, cmd_list, &head_cmd_list.l)) {
	bu_vls_free(&c->cl_name);
	bu_vls_free(&c->cl_more_default);
    }

    /* no longer send bu_log() output to Tcl */
    bu_log_delete_hook(gui_output, (void *)s);

    /* restore stdout/stderr just in case anyone tries to write before
     * we finally exit (e.g., an atexit() callback).
     */
    ret = dup2(stdfd[0], fileno(stdout));
    if (ret == -1)
	perror("dup2");
    ret = dup2(stdfd[1], fileno(stderr));
    if (ret == -1)
	perror("dup2");

    /* Be certain to close the database cleanly before exiting */
    Tcl_Preserve((ClientData)s->interp);
    Tcl_Eval(s->interp, "rename " MGED_DB_NAME " \"\"; rename .inmem \"\"");
    Tcl_Release((ClientData)s->interp);

    struct tclcad_io_data *giod = (struct tclcad_io_data *)s->gedp->ged_io_data;
    ged_close(s->gedp);
    s->gedp = GED_NULL;
    if (giod) {
	tclcad_destroy_io_data(giod);
    }

    s->wdbp = RT_WDB_NULL;
    s->dbip = DBI_NULL;

    /* XXX should deallocate libbu semaphores */

    mged_global_variable_teardown(s);

    s->magic = 0; // make sure anything trying to use this after free gets a magic failure
    bu_vls_free(&s->input_str);
    bu_vls_free(&s->input_str_prefix);
    bu_vls_free(&s->scratchline);
    bu_vls_free(&s-> mged_prompt);
    BU_PUT(s, struct mged_state);
    MGED_STATE = NULL; // sanity

    /* 8.5 seems to have some bugs in their reference counting */
    /* Tcl_DeleteInterp(INTERP); */

#ifndef USE_TCL_CHAN
    if (cbreak_mode > 0) {
	reset_Tty(fileno(stdin));
    }
#endif

    Tcl_Exit(exitcode);
}

/*
 * Handles finishing up.  Also called upon EOF on STDIN.
 */
void
quit(struct mged_state *s)
{
    mged_finish(s, 0);
    /* NOTREACHED */
}

int
main(int argc, char *argv[])
{
    /* pipes used for setting up read/write channels during graphical
     * initialization.
     */
#ifdef HAVE_PIPE
    int pipe_out[2] = {-1, -1};
    int pipe_err[2] = {-1, -1};
#endif

    int rateflag = 0;
    int c;
    int read_only_flag=0;

    int parent_pipe[2] = {0, 0};
    int use_pipe = 0;
    int run_in_foreground=1;

#ifndef USE_TCL_CHAN
    fd_set read_set;
    int result;
#endif

    BU_GET(MGED_STATE, struct mged_state);
    struct mged_state *s = MGED_STATE;
    s->magic = MGED_STATE_MAGIC;
    s->classic_mged = 1;
    s->update_views = 0;
    s->interactive = 0; /* >0 means interactive, intentionally starts
                         * 0 to know when interactive, e.g., via -C
                         * option
			 */
    bu_vls_init(&s->input_str);
    bu_vls_init(&s->input_str_prefix);
    bu_vls_init(&s->scratchline);
    bu_vls_init(&s->mged_prompt);
    s->dpy_string = NULL;

    /* Set up linked lists */
    s->vlfree = &rt_vlfree;
    BU_LIST_INIT(&rtg_headwdb.l);

    // Start out in an initialization state
    mged_global_db_ctx.init_flag = 1;
    mged_global_db_ctx.db_upgrade = 0; // no upgrade
    mged_global_db_ctx.db_warn = 0; // no warn
    mged_global_db_ctx.db_version = BRLCAD_DB_FORMAT_LATEST;

    char *attach = (char *)NULL;
    BU_ALLOC(s->mged_curr_dm, struct mged_dm);
    bu_ptbl_init(&active_dm_set, 8, "dm set");
    bu_ptbl_ins(&active_dm_set, (long *)s->mged_curr_dm);
    mged_dm_init_state = s->mged_curr_dm;
    s->mged_curr_dm->dm_netfd = -1;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    (void)_set_invalid_parameter_handler(mgedInvalidParameterHandler);

    bu_setprogname(argv[0]);

#if defined(HAVE_TK)
    if (dm_have_graphics()) {
	s->classic_mged = 0;
    }
#endif

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "a:d:hbcCorx:X:v?")) != -1) {
	if (bu_optopt == '?') c='h';
	switch (c) {
	    case 'a':
		attach = bu_optarg;
		break;
	    case 'd':
		s->dpy_string = bu_optarg;
		break;
	    case 'r':
		read_only_flag = 1;
		break;
	    case 'c':
		s->classic_mged = 2; // >1 to indicate requested
		break;
	    case 'C':
		s->classic_mged = 0;
		s->interactive = 2; // >1 to indicate requested
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug);
		break;
	    case 'b':
		run_in_foreground = 0;  /* run in background */
		break;
	    case 'v':	/* print a lot of version information */
		printf("%s%s%s%s%s%s\n",
		       brlcad_ident("MGED Geometry Editor"),
		       dm_version(),
		       fb_version(),
		       rt_version(),
		       bn_version(),
		       bu_version());
		return EXIT_SUCCESS;
		break;
	    case 'o':
		/* Eventually this will be used for the old mged gui.
		 * I'm temporarily hijacking it for the new gui until
		 * it becomes the default.
		 */
		bu_log("WARNING: -o is a developer option and subject to change.  Do not use.\n");
		old_mged_gui = 0;
		break;
	    default:
		bu_log("Unrecognized option (%c)\n", bu_optopt);
		/* fall through */
	    case 'h':
		bu_exit(1, "Usage:  %s [-a attach] [-b] [-c|-C] [-f] [-d display] [-h|?] [-r] [-x#] [-X#] [-v] [database [command]]\n", argv[0]);
	}
    }

    /* Change the working directory to BU_DIR_HOME if we are invoking
     * without any arguments. */
    if (argc == 1) {
	const char *homed = bu_dir(NULL, 0, BU_DIR_HOME, NULL);
	if (homed && chdir(homed)) {
	    bu_exit(1, "Failed to change working directory to \"%s\" ", homed);
	}
    }

    /* skip the args and invocation name */
    argc -= bu_optind;
    argv += bu_optind;

    if (argc > 1) {
	/* if there is more than a file name remaining, mged is not interactive */
	s->interactive = 0;
    } else {
	/* we init to 0, so if we're non-0 here, it was probably requested */
	if (!s->interactive) {
	    s->interactive = bu_interactive();
	}
    } /* argc > 1 */

    if (bu_debug > 0)
	fprintf(stdout, "DEBUG: interactive=%d, classic_mged=%d\n", s->interactive, s->classic_mged);


#if defined(SIGPIPE) && defined(SIGINT)
    (void)signal(SIGPIPE, SIG_IGN);

    /*
     * Sample and hold current SIGINT setting, so any commands that
     * might be run (e.g., by .mgedrc) which establish cur_sigint as
     * their signal handler get the initial behavior.  This will
     * change after setjmp() is called, below.
     */
    cur_sigint = signal(SIGINT, SIG_IGN);		/* sample */
    (void)signal(SIGINT, cur_sigint);		/* restore */
#endif /* SIGPIPE && SIGINT */

#ifdef HAVE_PIPE
    if (!s->classic_mged && !run_in_foreground) {
	pid_t pid;

	fprintf(stdout, "Initializing and backgrounding, please wait...");
	fflush(stdout);

	if (pipe(parent_pipe) == -1) {
	    perror("pipe failed");
	} else {
	    use_pipe=1;
	}

	pid = fork();
	if (pid > 0) {
	    /* just so it does not appear that MGED has died, wait
	     * until the gui is up before exiting the parent process
	     * (child sends us a byte after the window is displayed).
	     */
	    if (use_pipe) {
		struct timeval timeout;

		FD_ZERO(&read_set);
		FD_SET(parent_pipe[0], &read_set);
		timeout.tv_sec = 90;
		timeout.tv_usec = 0;
		result = select(parent_pipe[0]+1, &read_set, NULL, NULL, &timeout);

		if (result == -1) {
		    perror("Unable to read from communication pipe");
		} else if (result == 0) {
		    fprintf(stdout, "Detached\n");
		} else {
		    fprintf(stdout, "Done\n");
		}

	    } else {
		/* no pipe, so just wait a little while */
		bu_snooze(BU_SEC2USEC(3));
	    }

	    /* exit instead of mged_finish as this is the parent
	     * process.
	     */
	    bu_exit(0, NULL);
	}
    }
#endif /* HAVE_PIPE */

    memset((void *)&head_cmd_list, 0, sizeof(struct cmd_list));
    BU_LIST_INIT(&head_cmd_list.l);
    bu_vls_init(&head_cmd_list.cl_name);
    bu_vls_init(&head_cmd_list.cl_more_default);
    bu_vls_strcpy(&head_cmd_list.cl_name, "mged");
    curr_cmd_list = &head_cmd_list;

    /* initialize predictor stuff */
    BU_LIST_INIT(&s->mged_curr_dm->dm_p_vlist);
    predictor_init(s);

    /* register application provided routines */

    DMP = dm_open(NULL, s->interp, "nu", 0, NULL);
    struct bu_vls *dpvp = dm_get_pathname(DMP);
    if (dpvp) {
	bu_vls_strcpy(dpvp, "nu");
    }

    /* If we're only doing the 'nu' dm we don't need most of mged_dm_init, but
     * we do still need to register the dm_commands */
    s->mged_curr_dm->dm_cmd_hook = dm_commands;

    struct bu_vls *tnvp = dm_get_tkname(s->mged_curr_dm->dm_dmp);
    if (tnvp) {
	bu_vls_init(tnvp); /* this may leak */
	bu_vls_strcpy(tnvp, "nu");
    }

    BU_ALLOC(rubber_band, struct _rubber_band);
    *rubber_band = default_rubber_band;		/* struct copy */

    BU_ALLOC(mged_variables, struct _mged_variables);
    *mged_variables = default_mged_variables;	/* struct copy */

    BU_ALLOC(color_scheme, struct _color_scheme);
    *color_scheme = default_color_scheme;	/* struct copy */

    BU_ALLOC(grid_state, struct bv_grid_state);
    *grid_state = default_grid_state;		/* struct copy */

    BU_ALLOC(axes_state, struct _axes_state);
    *axes_state = default_axes_state;		/* struct copy */

    BU_ALLOC(adc_state, struct _adc_state);
    adc_state->adc_rc = 1;
    adc_state->adc_a1 = adc_state->adc_a2 = 45.0;

    BU_ALLOC(menu_state, struct _menu_state);
    menu_state->ms_rc = 1;

    BU_ALLOC(dlist_state, struct _dlist_state);
    dlist_state->dl_rc = 1;

    BU_ALLOC(view_state, struct _view_state);
    view_state->vs_rc = 1;
    view_ring_init(s->mged_curr_dm->dm_view_state, (struct _view_state *)NULL);
    MAT_IDN(view_state->vs_ModelDelta);

    am_mode = AMM_IDLE;
    owner = 1;
    frametime = 1;

    MAT_IDN(s->edit_state.model_changes);
    MAT_IDN(s->edit_state.acc_rot_sol);

    s->edit_state.global_editing_state = ST_VIEW;
    es_edflag = -1;
    s->edit_state.e_edclass = EDIT_CLASS_NULL;
    s->edit_state.e_inpara = newedge = 0;

    /* These values match old GED.  Use 'tol' command to change them. */
    s->tol.tol.magic = BN_TOL_MAGIC;
    s->tol.tol.dist = 0.0005;
    s->tol.tol.dist_sq = s->tol.tol.dist * s->tol.tol.dist;
    s->tol.tol.perp = 1e-6;
    s->tol.tol.para = 1 - s->tol.tol.perp;

    rt_prep_timer();		/* Initialize timer */

    es_edflag = -1;		/* no solid editing just now */

    /* prepare mged, adjust our path, get set up to use Tcl */

    mged_setup(s);
    new_mats(s);

    mmenu_init(s);
    btn_head_menu(s, 0, 0, 0);
    mged_link_vars(s->mged_curr_dm);

    bu_vls_printf(&s->input_str, "set version \"%s\"", brlcad_ident("Geometry Editor (MGED)"));
    (void)Tcl_Eval(s->interp, bu_vls_addr(&s->input_str));
    bu_vls_trunc(&s->input_str, 0);

    if (!s->dpy_string)
	s->dpy_string = getenv("DISPLAY");

    /* show ourselves */
    if (s->interactive) {
	if (s->classic_mged) {
	    /* identify */

	    (void)Tcl_Eval(s->interp, "set mged_console_mode classic");

	    bu_log("%s\n", brlcad_ident("Geometry Editor (MGED)"));

#ifndef USE_TCL_CHAN
	    if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
		/* Set up for character-at-a-time terminal IO. */
		cbreak_mode = COMMAND_LINE_EDITING;
		save_Tty(fileno(stdin));
	    }
#endif

	} else {
	    /* start up the gui */

	    int status;
	    struct bu_vls vls = BU_VLS_INIT_ZERO;
	    struct bu_vls error = BU_VLS_INIT_ZERO;

	    (void)Tcl_Eval(s->interp, "set mged_console_mode gui");

	    bu_vls_strcpy(&vls, "loadtk");
	    if (s->dpy_string)
		bu_vls_printf(&vls, " %s", s->dpy_string);

	    status = Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_strcpy(&error, Tcl_GetStringResult(s->interp));
	    bu_vls_free(&vls);

	    if (status != TCL_OK && !s->dpy_string) {
		/* failed to load tk, try localhost X11 if DISPLAY was not set */
		status = Tcl_Eval(s->interp, "loadtk :0");
	    }

	    if (status != TCL_OK) {
		if (!run_in_foreground && use_pipe) {
		    notify_parent_done(parent_pipe[1]);
		}
		bu_log("%s\nMGED Aborted.\n", bu_vls_addr(&error));
		mged_finish(s, 1);
	    }
	    bu_vls_free(&error);
	}
    } else {
	(void)Tcl_Eval(s->interp, "set mged_console_mode batch");
    }

    if (!s->interactive || s->classic_mged || old_mged_gui) {
	/* Open the database */
	if (argc >= 1) {
	    const char *av[3];

	    av[0] = "opendb";
	    av[1] = argv[0];
	    av[2] = NULL;

	    /* Command line may have more than 2 args, opendb only wants 2
	     * expecting second to be the file name.
	     * NOTE: this way makes it so f_opendb does not care about y/n
	     * and always create a new db if one does not exist since we want
	     * to allow mged to process args after the db as a command
	     */
	    struct cmdtab ec = {MGED_CMD_MAGIC, NULL, NULL, NULL, s};
	    if (f_opendb(&ec, s->interp, 2, av) == TCL_ERROR) {
		if (!run_in_foreground && use_pipe) {
		    notify_parent_done(parent_pipe[1]);
		}
		mged_finish(s, 1);
	    }
	} else {
	    (void)Tcl_Eval(s->interp, "opendb_callback nul");
	}
    }

    if (s->dbip != DBI_NULL && (read_only_flag || s->dbip->dbi_read_only)) {
	s->dbip->dbi_read_only = 1;
	bu_log("Opened in READ ONLY mode\n");
    }

    if (s->dbip != DBI_NULL) {
	setview(s, 0.0, 0.0, 0.0);
	ged_dl_notify_func_set(s->gedp, mged_notify);
    }

    /* --- Now safe to process commands. --- */
    if (s->interactive) {

	/* This is an interactive mged, process .mgedrc */
	do_rc(s);

	/*
	 * Initialize variables here in case the user specified changes
	 * to the defaults in their .mgedrc file.
	 */

	if (s->classic_mged) {
	    /* start up a text command console */

	    if (!run_in_foreground && use_pipe) {
		notify_parent_done(parent_pipe[1]);
	    }

	} else {
	    /* start up the GUI */

	    struct bu_vls vls = BU_VLS_INIT_ZERO;
	    int status;

#ifdef HAVE_SETPGID
	    /* make this a process group leader */
	    setpgid(0, 0);
#endif

	    if (old_mged_gui) {
		bu_vls_strcpy(&vls, "gui");
		status = Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    } else {
		Tcl_DString temp;
		const char *archer_trans;
		Tcl_DStringInit(&temp);
		const char *archer = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "archer", "archer_launch.tcl", NULL);
		archer_trans = Tcl_TranslateFileName(s->interp, archer, &temp);
		tclcad_set_argv(s->interp, argc, (const char **)argv);
		status = Tcl_EvalFile(s->interp, archer_trans);
		Tcl_DStringFree(&temp);
	    }
	    bu_vls_free(&vls);

#ifdef HAVE_PIPE
	    /* if we are going to run in the background, let the
	     * parent process know that we are done initializing so
	     * that it may exit.
	     */
	    if (!run_in_foreground && use_pipe) {
		notify_parent_done(parent_pipe[1]);
	    }
#endif /* HAVE_PIPE */

	    if (status != TCL_OK) {
		if (use_pipe) {
		    /* too late to fall back to classic, we forked and detached already */
		    bu_log("Unable to initialize an MGED graphical user interface.\nTry using foreground (-f) or classic-mode (-c) options to MGED.\n");
		    bu_log("%s\nMGED aborted.\n", Tcl_GetStringResult(s->interp));
		    mged_finish(s, 1);
		}
		bu_log("%s\nMGED unable to initialize gui, reverting to classic mode.\n", Tcl_GetStringResult(s->interp));
		s->classic_mged = 1;

#ifndef USE_TCL_CHAN
		cbreak_mode = COMMAND_LINE_EDITING;
		save_Tty(fileno(stdin));
#endif

	    } else {

#ifdef HAVE_PIPE
		/* we're going to close out stdout/stderr as we
		 * proceed in GUI mode, so create some pipes.
		 */
		result = pipe(pipe_out);
		if (result == -1)
		    perror("pipe");
		result = pipe(pipe_err);
		if (result == -1)
		    perror("pipe");
#endif  /* HAVE_PIPE */

		/* since we're in GUI mode, display any bu_bomb()
		 * calls in text dialog windows.
		 */
		bu_bomb_add_hook(mged_bomb_hook, s->interp);
	    } /* status -- gui initialized */
	} /* classic */

    } else {
	/* !interactive */

	if (!run_in_foreground && use_pipe) {
	    notify_parent_done(parent_pipe[1]);
	}

    } /* interactive */

    /* XXX total hack that fixes a dm init issue on Mac OS X where the
     * dm first opens filled with garbage.
     */
    {
	unsigned char *dm_bg;
	dm_get_bg(&dm_bg, NULL, DMP);
	dm_set_bg(DMP, dm_bg[0], dm_bg[1], dm_bg[2], dm_bg[0], dm_bg[1], dm_bg[2]);
    }

    /* initialize a display manager */
    if (s->interactive && s->classic_mged) {
	if (!attach) {
	    get_attached(s);
	} else {
	    attach_display_manager(s->interp, attach, s->dpy_string);
	}
    }

    /* --- Now safe to process geometry. --- */

    /* If this is an argv[] invocation, do it now */
    if (argc > 1) {
	for (argc -= 1, argv += 1; argc; --argc, ++argv)
	    bu_vls_printf(&s->input_str, "%s ", *argv);

	cmdline(s, &s->input_str, 1);
	bu_vls_free(&s->input_str);

	// If we launched subcommands, we need to process their
	// output before quitting.  Do one up front to catch
	// anything produced by a process that already exited,
	// and loop while libged still reports running processes.
	Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT);
	while (BU_PTBL_LEN(&s->gedp->ged_subp)) {
	    Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT);
	}

	Tcl_Eval(s->interp, "q");
	/* NOTREACHED */
    }

    if (s->classic_mged || !s->interactive) {
	struct stdio_data *sd;
	BU_GET(sd, struct stdio_data);
	sd->s = s;

#ifdef USE_TCL_CHAN
	sd->chan = Tcl_MakeFileChannel(GetStdHandle(STD_INPUT_HANDLE), TCL_READABLE);
	Tcl_CreateChannelHandler(sd->chan, TCL_READABLE, stdin_input, sd);
#else
	sd->fd = STDIN_FILENO;
	sd->chan = Tcl_MakeFileChannel(STDIN_FILENO, TCL_READABLE);
	Tcl_CreateChannelHandler(sd->chan, TCL_READABLE, stdin_input, sd);
#endif

#ifdef SIGINT
	(void)signal(SIGINT, SIG_IGN);
#endif

	bu_vls_strcpy(&s->mged_prompt, MGED_PROMPT);
	pr_prompt(s);

#ifndef USE_TCL_CHAN
	if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	}
#endif
    } else {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	int sout = fileno(stdout);
	int serr = fileno(stderr);

	/* stash stdout */
	stdfd[0] = dup(sout);
	if (stdfd[0] == -1)
	    perror("dup");

	/* stash stderr */
	stdfd[1] = dup(serr);
	if (stdfd[1] == -1)
	    perror("dup");

	bu_vls_printf(&vls, "output_hook output_callback");
	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

/* FIXME: windows has dup() and dup2(), so this should work there too */
#ifndef USE_TCL_CHAN
	{
	    ClientData outpipe, errpipe;

	    (void)close(fileno(stdout));

	    /* since we just closed stdout, fd 1 is what dup() should return */
	    result = dup(pipe_out[1]);
	    if (result == -1)
		perror("dup");
	    (void)close(pipe_out[1]); /* only a write pipe */

	    (void)close(fileno(stderr));

	    /* since we just closed stderr, fd 2 is what dup() should return */
	    result = dup(pipe_err[1]);
	    if (result == -1)
		perror("dup");
	    (void)close(pipe_err[1]); /* only a write pipe */

	    Tcl_Channel chan;

	    outpipe = (ClientData)(size_t)pipe_out[0];
	    chan = Tcl_MakeFileChannel(outpipe, TCL_READABLE);
	    Tcl_CreateChannelHandler(chan, TCL_READABLE, std_out_or_err, outpipe);

	    errpipe = (ClientData)(size_t)pipe_err[0];
	    chan = Tcl_MakeFileChannel(errpipe, TCL_READABLE);
	    Tcl_CreateChannelHandler(chan, TCL_READABLE, std_out_or_err, errpipe);
	}
#else
	{
	    HANDLE handle[2];
	    SECURITY_ATTRIBUTES saAttr;

	    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	    saAttr.bInheritHandle = 0;
	    saAttr.lpSecurityDescriptor = NULL;

	    Tcl_Channel chan;

	    if (CreatePipe(&handle[0], &handle[1], &saAttr, 0)) {
		chan = Tcl_GetStdChannel(TCL_STDOUT);
		Tcl_UnregisterChannel(s->interp, chan);
		chan = Tcl_MakeFileChannel(handle[1], TCL_WRITABLE);
		Tcl_RegisterChannel(s->interp, chan);
		Tcl_SetChannelOption(s->interp, chan, "-blocking", "false");
		Tcl_SetChannelOption(s->interp, chan, "-buffering", "line");
		chan = Tcl_MakeFileChannel(handle[0], TCL_READABLE);
		/* intermittently, the process of Tcl_UnregisterChannel does not
		 * finish cleaning up the write threads before we spawn new
		 * ones with Tcl_MakeFileChannel. This error prematurely invokes the
		 * new threads when the old ones finally signal which breaks all 'puts'
		 * from the channel. Calling puts with an empty string here
		 * *appears* to force a sync and resolve the issue.
		 */
		if (Tcl_Eval(s->interp, "puts \"\"") != TCL_OK)
		    perror("STDOUT chan broken");
		Tcl_CreateChannelHandler(chan, TCL_READABLE, std_out_or_err, chan);
	    }

	    if (CreatePipe(&handle[0], &handle[1], &saAttr, 0)) {
		chan = Tcl_GetStdChannel(TCL_STDERR);
		Tcl_UnregisterChannel(s->interp, chan);
		chan = Tcl_MakeFileChannel(handle[1], TCL_WRITABLE);
		Tcl_RegisterChannel(s->interp, chan);
		Tcl_SetChannelOption(s->interp, chan, "-blocking", "false");
		Tcl_SetChannelOption(s->interp, chan, "-buffering", "line");
		chan = Tcl_MakeFileChannel(handle[0], TCL_READABLE);
		if (Tcl_Eval(s->interp, "puts stderr \"\"") != TCL_OK)
		    perror("STDERR chan broken");
		Tcl_CreateChannelHandler(chan, TCL_READABLE, std_out_or_err, chan);
	    }
	}
#endif
    }

    mged_global_db_ctx.init_flag = 0; /* all done with initialization */

    /**************** M A I N   L O O P *********************/
    while (1) {
	/* This test stops optimizers from complaining about an
	 * infinite loop.
	 */
	if ((rateflag = event_check(s, rateflag)) < 0)
	    break;

	/*
	 * Cause the control portion of the displaylist to be updated
	 * to reflect the changes made above.
	 */
	refresh(s);
    }
    return 0;
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
