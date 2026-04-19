/*                           M G E D . C
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
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/debug.h"
#include "bu/units.h"
#include "bu/version.h"
#include "bu/time.h"
#include "bu/snooze.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#define LIBTERMIO_IMPLEMENTATION
#include "libtermio.h"
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
extern struct bu_vls *history_cur(void);
extern struct bu_vls *history_next(const char *);

// FIXME: Globals

/* defined in dozoom.c */
extern unsigned char geometry_default_color[];

/* defined in set.c */
extern struct _mged_variables default_mged_variables;

/* defined in color_scheme.c */
extern struct _color_scheme default_color_scheme;

/* defined in grid.c */
extern struct bv_grid_state default_grid_state;

/* defined in axes.c */
extern struct _axes_state default_axes_state;

/* defined in rect.c */
extern struct _rubber_band default_rubber_band;

#ifdef HAVE_PIPE
/* these two file descriptors are where we store fileno(stdout) and
 * fileno(stderr) during graphical startup so that we may restore them
 * when we're done (which is needed so atexit() calls to bu_log() will
 * still work).
 */
// FIXME: Global
static int stdfd[2] = {1, 2};
#endif

/* Container for passing I/O data through Tcl callbacks */
struct stdio_data {
    Tcl_Channel chan;
    struct mged_state *s;
};

struct mged_state *MGED_STATE = NULL;

jmp_buf jmp_env;	/* For non-local gotos */
// FIXME: Global
double frametime;	/* time needed to draw last frame */

// FIXME: Global
struct rt_wdb rtg_headwdb;  /* head of database object list */

void (*cur_sigint)(int);	/* Current SIGINT status */

// FIXME: Global
int cbreak_mode = 0;    /* >0 means in cbreak_mode */


/* The old mged gui is temporarily the default. */
// FIXME: Global
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
	bn_mat_mul(view_state->vs_model2objview, view_state->vs_gvp->gv_model2view, MEDIT(s)->model_changes);
	bn_mat_inv(view_state->vs_objview2model, view_state->vs_model2objview);

	/* Keep rt_edit’s own cached matrix in sync for external users */
	MAT_COPY(MEDIT(s)->model2objview, view_state->vs_model2objview);

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

    if (s->global_editing_state != ST_VIEW) {
	bn_mat_mul(vsp->vs_model2objview, gvp->gv_model2view, MEDIT(s)->model_changes);
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


/*
 * Sentinel values used by struct mged_cli_overrides to distinguish "not
 * specified on command line" from a legitimate zero or negative value.
 * INT_MIN is safe for all int settings (no setting takes that value).
 * The DBL sentinel is a large negative number no geometric value would use.
 */
#define MGED_CLI_UNSET_INT  INT_MIN
#define MGED_CLI_UNSET_DBL  (-1.0e+38)


/*
 * Helper struct for colour options that use bu_opt_color.  The .set flag
 * distinguishes "not supplied on command line" from an explicit black (0 0 0).
 * Defined here so it can be embedded in struct mged_cli_overrides below.
 */
struct mged_color_opt {
    struct bu_color color;  /* populated by bu_opt_color */
    int set;                /* 1 once the option has been parsed */
};


/*
 * Holds every value parsed from the MGED command line.  Fields that were not
 * supplied by the user keep their sentinel initial values so that
 * apply_cli_overrides() can distinguish "not set" from an explicit zero.
 *
 * Two usage phases:
 *   (a) Early options (attach, classic_mged, …) are applied immediately
 *       after bu_opt_parse returns, before Tcl/DM initialisation.
 *   (b) Deferred options (mged_variables, grid, colours, …) are applied by
 *       apply_cli_overrides() after do_rc() so that CLI flags always win
 *       over any .mgedrc settings.
 */
struct mged_cli_overrides {
    /* --- Phase (a): applied immediately after bu_opt_parse() --- */
    const char *attach;         /* -a / --attach */
    const char *dpy_string;     /* -d / --display */
    int classic_mode;           /* -c / --classic  (set to 1 when given) */
    int gui_mode;               /* -C / --gui      (set to 1 when given) */
    int read_only;              /* -r / --read-only */
    int pipe_mode;              /* -p / --pipe */
    int background;             /* -b / --background (run in background) */
    int print_version;          /* -v / --version */
    int print_help;             /* -h / --help / -? */
    int old_gui_flag;           /* -o developer option */
    unsigned int rt_debug_val;  /* -x / --rt-debug (hex uint) */
    int rt_debug_set;           /* 1 if -x was given */
    unsigned int bu_debug_val;  /* -X / --bu-debug (hex uint) */
    int bu_debug_set;           /* 1 if -X was given */

    /* --- rc file control (applied before phase-b) --- */
    const char *rcfile;         /* --rcfile: use this file instead of .mgedrc */
    int skip_rc;                /* --no-rc: skip all rc processing */

    /* --- Phase (b): applied by apply_cli_overrides() after do_rc() ---
     *
     * All int fields: MGED_CLI_UNSET_INT  means "not given".
     * For boolean on/off pairs (--use-air / --no-use-air) the int is
     * initialised to MGED_CLI_UNSET_INT; --X sets it to 1, --no-X sets it to 0.
     *
     * All double fields: MGED_CLI_UNSET_DBL means "not given".
     *
     * All char fields: '\0' means "not given".
     *
     * All pointer fields: NULL means "not given".
     */

    /* mged_variables (rset var / set <name>) */
    int use_air;            /* --use-air / --no-use-air */
    int dlist;              /* --dlist   / --no-dlist   */
    int faceplate;          /* --faceplate / --no-faceplate */
    int orig_gui;           /* MGED_CLI_UNSET_INT / 0 / 1 */
    int linewidth;          /* --linewidth #  (pixels, >=1) */
    int port;               /* --port #       (0-65535) */
    int listen;             /* MGED_CLI_UNSET_INT / 0 / 1 */
    int fb;                 /* MGED_CLI_UNSET_INT / 0 / 1 */
    int fb_all;             /* MGED_CLI_UNSET_INT / 0 / 1 */
    int fb_overlay;         /* --fb-overlay 0|1|2 */
    int predictor;          /* --predictor / --no-predictor */
    int hot_key;            /* MGED_CLI_UNSET_INT / keycode */
    int context;            /* MGED_CLI_UNSET_INT / 0 / 1 */
    int sliders;            /* MGED_CLI_UNSET_INT / 0 / 1 */
    int perspective_mode;   /* MGED_CLI_UNSET_INT / 0 / 1 */
    char linestyle;         /* --linestyle s|d  ('\0' = not set) */
    char mouse_behavior;    /* --mouse-behavior v|a|e */
    char coords;            /* --coords m|v */
    char rotate_about;      /* --rotate-about m|v|e */
    char transform;         /* --transform v|a|e */
    double perspective;     /* --perspective #  (degrees, MGED_CLI_UNSET_DBL = not set) */
    double predictor_advance;   /* --predictor-advance # */
    double predictor_length;    /* --predictor-length # */
    double nmg_eu_dist;         /* --nmg-eu-dist # */
    double eye_sep_dist;        /* --eye-sep-dist # (mm, 0 = mono) */

    /* Tcl mged_default array entries */
    const char *dm_type;    /* --dm-type type */
    const char *geom;       /* --geom WxH+X+Y (command window) */
    const char *ggeom;      /* --ggeom WxH+X+Y (graphics window) */

    /* grid (rset g …) */
    int grid_draw;          /* --grid-draw 0|1 */
    int grid_snap;          /* --grid-snap 0|1 */
    int grid_mrh;           /* --grid-mrh # */
    int grid_mrv;           /* --grid-mrv # */
    double grid_rh;         /* --grid-rh # */
    double grid_rv;         /* --grid-rv # */

    /* colour scheme subset (rset cs …) */
    struct mged_color_opt bg_color;      /* --bg  (bg_color.set = 0 means not given) */
    struct mged_color_opt geo_def_color; /* --geo-color */

    /* general escape hatches */
    struct bu_ptbl set_pairs;   /* --set VAR=VALUE  (strings in argv) */
    struct bu_ptbl rset_pairs;  /* --rset ARGS      (strings in argv) */
};


/* ---------------------------------------------------------------------------
 * bu_opt helper: set the pointed-to int to 0 (used by --no-X boolean flags).
 * Returns 0 (no argv element consumed).
 * -------------------------------------------------------------------------- */
static int
flag_set_zero(struct bu_vls *UNUSED(msg), size_t UNUSED(argc),
	      const char **UNUSED(argv), void *set_var)
{
    *(int *)set_var = 0;
    return 0;
}


/* ---------------------------------------------------------------------------
 * bu_opt helper: parse a hex unsigned int (for -x / -X debug flags).
 * Consumes one argv element.
 * -------------------------------------------------------------------------- */
static int
parse_debug_uint(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    unsigned int *val = (unsigned int *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "hex value");
    if (sscanf(argv[0], "%x", val) != 1) {
	if (msg)
	    bu_vls_printf(msg, "ERROR: expected hex integer, got \"%s\"\n", argv[0]);
	return -1;
    }
    return 1;
}


/* ---------------------------------------------------------------------------
 * bu_opt_color wrapper: stores the parsed colour in a struct that also holds
 * a "was this option given?" flag.  Used for --bg and --geo-color so that
 * apply_cli_overrides() can reliably distinguish "not supplied" from an
 * explicit black (0 0 0).
 * struct mged_color_opt is defined above (before mged_cli_overrides).
 * -------------------------------------------------------------------------- */
static int
parse_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct mged_color_opt *co = (struct mged_color_opt *)set_var;
    int ret = bu_opt_color(msg, argc, argv, &co->color);
    if (ret > 0)
	co->set = 1;
    return ret;
}


/* ---------------------------------------------------------------------------
 * bu_opt helper: collect a --set VAR=VALUE string into a bu_ptbl.
 * Validates the '=' separator; stores the original argv pointer (valid for
 * the duration of main()).  Consumes one argv element.
 * -------------------------------------------------------------------------- */
static int
parse_mged_set(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct bu_ptbl *t = (struct bu_ptbl *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "VAR=VALUE");
    if (!strchr(argv[0], '=')) {
	if (msg)
	    bu_vls_printf(msg, "ERROR: --set requires VAR=VALUE format (no '=' in \"%s\")\n",
			  argv[0]);
	return -1;
    }
    bu_ptbl_ins(t, (long *)argv[0]);
    return 1;
}


/* ---------------------------------------------------------------------------
 * bu_opt helper: collect a --rset ARGS string into a bu_ptbl.
 * The ARGS string should contain everything that would follow "rset " on the
 * MGED command line, e.g. "g snap 1" or "ax model_draw 1".
 * Consumes one argv element.
 * -------------------------------------------------------------------------- */
static int
parse_rset(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct bu_ptbl *t = (struct bu_ptbl *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "ARGS");
    bu_ptbl_ins(t, (long *)argv[0]);
    return 1;
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
do_rc(struct mged_state *s, int skip_rc, const char *rcfile_override)
{
    FILE *fp = NULL;
    char *path;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    int bogus;

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".mgedrc"

    /* --no-rc: skip all rc processing */
    if (skip_rc)
	return 0;

    /* --rcfile FILE: use the specified file directly */
    if (rcfile_override) {
	if ((fp = fopen(rcfile_override, "r")) == NULL) {
	    bu_log("mged: cannot open --rcfile \"%s\": %s\n",
		   rcfile_override, strerror(errno));
	    return -1;
	}
	bu_vls_strcpy(&str, rcfile_override);
    } else {
	/* Standard search: MGED_RCFILE env, then $HOME/.mgedrc, then ./.mgedrc */
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
	bu_log("Error reading %s:\n%s\n", bu_vls_cstr(&str),
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
    if (MEDIT(s)->k.rot_m_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->s_edit->edit_rate_mr_dm);
	save_coords = mged_variables->mv_coords;
	mged_variables->mv_coords = 'm';

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_ROTATE)
		MEDIT(s)->edit_flag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      MEDIT(s)->k.origin_m,
		      MEDIT(s)->k.rot_m[X],
		      MEDIT(s)->k.rot_m[Y],
		      MEDIT(s)->k.rot_m[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	mged_variables->mv_coords = save_coords;

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (MEDIT(s)->k.rot_o_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->s_edit->edit_rate_or_dm);
	save_coords = mged_variables->mv_coords;
	mged_variables->mv_coords = 'o';

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_ROTATE)
		MEDIT(s)->edit_flag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      MEDIT(s)->k.origin_o,
		      MEDIT(s)->k.rot_o[X],
		      MEDIT(s)->k.rot_o[Y],
		      MEDIT(s)->k.rot_o[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	mged_variables->mv_coords = save_coords;

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (MEDIT(s)->k.rot_v_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	char save_coords;

	set_curr_dm(s, s->s_edit->edit_rate_vr_dm);
	save_coords = mged_variables->mv_coords;
	mged_variables->mv_coords = 'v';

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_ROTATE)
		MEDIT(s)->edit_flag = SROT;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		      MEDIT(s)->k.origin_v,
		      MEDIT(s)->k.rot_v[X],
		      MEDIT(s)->k.rot_v[Y],
		      MEDIT(s)->k.rot_v[Z]);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	mged_variables->mv_coords = save_coords;

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (MEDIT(s)->k.tra_m_flag) {
	char save_coords;
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	set_curr_dm(s, s->s_edit->edit_rate_mt_dm);
	save_coords = mged_variables->mv_coords;
	mged_variables->mv_coords = 'm';

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_TRAN)
		MEDIT(s)->edit_flag = STRANS;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		      MEDIT(s)->k.tra_m[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      MEDIT(s)->k.tra_m[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      MEDIT(s)->k.tra_m[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	mged_variables->mv_coords = save_coords;

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (MEDIT(s)->k.tra_v_flag) {
	char save_coords;
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	set_curr_dm(s, s->s_edit->edit_rate_vt_dm);
	save_coords = mged_variables->mv_coords;
	mged_variables->mv_coords = 'v';

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_TRAN)
		MEDIT(s)->edit_flag = STRANS;
	} else {
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		      MEDIT(s)->k.tra_v[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      MEDIT(s)->k.tra_v[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		      MEDIT(s)->k.tra_v[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	mged_variables->mv_coords = save_coords;

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }
    if (MEDIT(s)->k.sca_flag) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (s->global_editing_state == ST_S_EDIT) {
	    save_edflag = MEDIT(s)->edit_flag;
	    if (!SEDIT_SCALE)
		MEDIT(s)->edit_flag = SSCALE;
	} else {
	    save_edflag = edobj;
	    if (!OEDIT_SCALE)
		edobj = BE_O_SCALE;
	}

	non_blocking++;
	bu_vls_printf(&vls, "knob -i -e aS %f\n", MEDIT(s)->k.sca * 0.01);

	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	if (s->global_editing_state == ST_S_EDIT)
	    MEDIT(s)->edit_flag = save_edflag;
	else
	    edobj = save_edflag;
    }

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (!p->dm_owner)
	    continue;

	set_curr_dm(s, p);

	if (view_state->k.rot_m_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -o %c -i -m ax %f ay %f az %f\n",
			  view_state->k.origin_m,
			  view_state->k.rot_m[X],
			  view_state->k.rot_m[Y],
			  view_state->k.rot_m[Z]);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->k.tra_m_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -i -m aX %f aY %f aZ %f\n",
			  view_state->k.tra_m[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->k.tra_m[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->k.tra_m[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->k.rot_v_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -o %c -i -v ax %f ay %f az %f\n",
			  view_state->k.origin_v,
			  view_state->k.rot_v[X],
			  view_state->k.rot_v[Y],
			  view_state->k.rot_v[Z]);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->k.tra_v_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "knob -i -v aX %f aY %f aZ %f",
			  view_state->k.tra_v[X] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->k.tra_v[Y] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  view_state->k.tra_v[Z] * 0.05 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

	    Tcl_Eval(s->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	}
	if (view_state->k.sca_flag) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;

	    non_blocking++;
	    bu_vls_printf(&vls, "zoom %f",
			  1.0 / (1.0 - (view_state->k.sca / 10.0)));
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
 * the routine "stdin_input" (registered via Tcl_CreateChannelHandler
 * on the stdin channel returned by Tcl_GetStdChannel(TCL_STDIN)).
 * This routine simply appends the new
 * input to a growing string until the command is complete (it is
 * assumed that the routine gets a full line.)
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
    struct stdio_data *sd = (struct stdio_data *)clientData;
    struct mged_state *s = sd->s;

    /* When not in cbreak mode, just process an entire line of input,
     * and don't do any command-line manipulation.
     */

    if (!cbreak_mode) {

	/* Read a line from stdin via the Tcl channel — this is the correct
	 * approach when inside a Tcl_CreateChannelHandler callback, and works
	 * on all platforms (POSIX and Windows).
	 */
	Tcl_DString ds;
	Tcl_DStringInit(&ds);
	count = Tcl_Gets(sd->chan, &ds);

	if (count < 0) {
	    /* On a non-blocking channel, Tcl_Gets returns -1 with no EOF
	     * flag when no complete line is yet available (EAGAIN).  Only
	     * treat a negative return as fatal when it signals true EOF. */
	    if (!Tcl_Eof(sd->chan)) {
		Tcl_DStringFree(&ds);
		return;
	    }
	    BU_PUT(sd, struct stdio_data);
	    quit(s); /* does not return */
	}

	/* If a GED command is already executing in a worker thread, consume the
	 * line so the channel does not remain readable, but replace input_str
	 * with it (rather than appending) so the user sees only their latest
	 * pending command, not a concatenation of all commands typed while
	 * the previous one was running.  Print a brief notice, re-show the
	 * prompt, then re-echo input_str so the user can see what they had
	 * typed and press Enter again once the running command finishes. */
	if (s->cmd_running) {
	    /* Replace (not append) so only the most-recently typed pending
	     * command is kept.  If the new line is empty, keep whatever is
	     * already in input_str unchanged so existing partial input is
	     * preserved. */
	    if (Tcl_DStringLength(&ds) > 0) {
		bu_vls_trunc(&s->input_str, 0);
		bu_vls_strcat(&s->input_str, Tcl_DStringValue(&ds));
	    }
	    Tcl_DStringFree(&ds);
	    s->input_str_index = bu_vls_strlen(&s->input_str);

	    bu_log("\nmged: command already running, please wait\n");
	    pr_prompt(s);
	    /* Re-echo so the user can see what they had typed */
	    if (bu_vls_strlen(&s->input_str))
		bu_log("%s", bu_vls_addr(&s->input_str));
	    return;
	}

	bu_vls_strcat(&s->input_str, Tcl_DStringValue(&ds));
	Tcl_DStringFree(&ds);

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
	    int cmd_status;
	    curr_cmd_list = &head_cmd_list;
	    if (curr_cmd_list->cl_tie)
		set_curr_dm(s, curr_cmd_list->cl_tie);

	    cmd_status = cmdline(s, &s->input_str_prefix, 1);
	    if (cmd_status == CMD_MORE) {
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

	    /* In pipe mode, emit a machine-readable completion sentinel so
	     * that a parent process driving MGED via stdin/stdout can detect
	     * when a command has finished, even if the command produced no
	     * output.  The sentinel is written directly to stdout (not via
	     * bu_log) so it is never swallowed by the Tcl log-redirect
	     * machinery.  The numeric status lets the parent distinguish
	     * CMD_OK (919), CMD_BAD (920), and CMD_MORE (921). */
	    if (s->pipe_mode) {
		fprintf(stdout, "MGED_CMD_DONE %d\n", cmd_status);
		fflush(stdout);
	    }
	} else {
	    bu_vls_trunc(&s->input_str, 0);
	    /* Allow the user to hit ^C. */
	    (void)signal(SIGINT, sig2);
	}

	return;
    }

    /*XXXXX*/
#define TRY_STDIN_INPUT_HACK
#ifdef TRY_STDIN_INPUT_HACK
    /* Grab everything --- assuming everything is <= page size */
    {
	char buf[BU_PAGE_SIZE];
	int idx;
	/* Use Tcl_Read on the channel — consistent with how the channel handler
	 * was registered and works cross-platform (POSIX and Windows). */
	count = Tcl_Read(sd->chan, buf, BU_PAGE_SIZE);

#else
	/* Grab single character from stdin */
	count = Tcl_Read(sd->chan, &ch, 1);
#endif

	if (count < 0)
	    perror("READ ERROR");

	if (count <= 0 && Tcl_Eof(sd->chan))
	    Tcl_Eval(s->interp, "q");

	/* On a non-blocking channel Tcl_Read returns 0 for EAGAIN (no data
	 * available right now).  Guard here so we don't crash on buf[0]. */
	if (count <= 0)
	    return;

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
    struct mged_state *s = MGED_STATE;

    /* clientData is the Tcl_Channel wrapping the pipe read end.  We read via
     * the Tcl channel API so this works correctly on all platforms.  On
     * Windows, Tcl_MakeFileChannel creates a channel backed by a native
     * HANDLE with its own internal reader thread; data arrives in Tcl's
     * channel buffer and must be consumed with Tcl_Read, not read(). */
    Tcl_Channel chan = (Tcl_Channel)clientData;
    int count;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char line[RT_MAXLINE+1] = {0};
    Tcl_Obj *save_result;

    count = Tcl_Read(chan, line, RT_MAXLINE);

    if (count <= 0)
	return;

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

    /* Flush any accumulated bu_log output to the command prompt.
     * The log-drain timer handles live streaming during long commands;
     * this call catches anything produced between timer ticks. */
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
				(s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT))
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

    /* cancel the periodic log-drain timer before tearing down the interp */
    mged_stop_log_drain_timer(s);

#ifdef HAVE_PIPE
    /* restore stdout/stderr just in case anyone tries to write before
     * we finally exit (e.g., an atexit() callback).
     */
    ret = dup2(stdfd[0], fileno(stdout));
    if (ret == -1)
	perror("dup2");
    ret = dup2(stdfd[1], fileno(stderr));
    if (ret == -1)
	perror("dup2");
#endif

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
    rt_edit_destroy(s->s_edit->e);
    BU_PUT(s->s_edit, struct mged_edit_state);
    BU_PUT(s, struct mged_state);
    MGED_STATE = NULL; // sanity

    /* 8.5 seems to have some bugs in their reference counting */
    /* Tcl_DeleteInterp(INTERP); */

#ifndef HAVE_WINDOWS_H
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

/*
 * apply_cli_overrides -- called after do_rc() so that command-line flags
 * always take precedence over .mgedrc settings.
 *
 * For mged_variables the Tcl variable write-traces installed by
 * mged_variable_setup() fire automatically when Tcl_SetVar() is called,
 * ensuring the same hooks (set_dirty_flag, set_perspective, etc.) execute
 * as if the user had typed the equivalent "set" command interactively.
 *
 * For rset resources (grid, colour scheme, etc.) a "rset …" Tcl command
 * string is constructed and evaluated directly.
 *
 * Double fields in mged_cli_overrides use MGED_CLI_UNSET_DBL as a "not
 * given" sentinel.  We compare with memcmp to avoid -Werror=float-equal.
 */

/* File-scope sentinel value used by cli_dbl_is_set() — initialised once
 * rather than per call, to avoid repeated redundant initialisation. */
static const double mged_cli_dbl_sentinel = MGED_CLI_UNSET_DBL;

static int
cli_dbl_is_set(double v)
{
    /* Compare bit-for-bit against the sentinel value to avoid
     * floating-point equality comparison (-Werror=float-equal). */
    return memcmp(&v, &mged_cli_dbl_sentinel, sizeof(double)) != 0;
}


static void
apply_cli_overrides(struct mged_state *s, struct mged_cli_overrides *cl)
{
    struct bu_vls cmd = BU_VLS_INIT_ZERO;

    /* Helper macro: set a Tcl global variable to an integer value string. */
#define CLI_SETVAR_INT(name, val) do { \
    char _vbuf[32]; \
    snprintf(_vbuf, sizeof(_vbuf), "%d", (int)(val)); \
    Tcl_SetVar(s->interp, (name), _vbuf, TCL_GLOBAL_ONLY); \
} while (0)

    /* Helper macro: set a Tcl global variable to a double value string. */
#define CLI_SETVAR_DBL(name, val) do { \
    char _vbuf[64]; \
    snprintf(_vbuf, sizeof(_vbuf), "%g", (double)(val)); \
    Tcl_SetVar(s->interp, (name), _vbuf, TCL_GLOBAL_ONLY); \
} while (0)

    /* Helper macro: set a Tcl global variable to a single-char string. */
#define CLI_SETVAR_CHAR(name, ch) do { \
    char _vbuf[2]; _vbuf[0] = (ch); _vbuf[1] = '\0'; \
    Tcl_SetVar(s->interp, (name), _vbuf, TCL_GLOBAL_ONLY); \
} while (0)

    /* --- mged_variables -------------------------------------------------- */
    if (cl->use_air        != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("use_air",        cl->use_air);
    if (cl->dlist          != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("dlist",          cl->dlist);
    if (cl->faceplate      != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("faceplate",      cl->faceplate);
    if (cl->orig_gui       != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("orig_gui",       cl->orig_gui);
    if (cl->linewidth      != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("linewidth",      cl->linewidth);
    if (cl->port           != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("port",           cl->port);
    if (cl->listen         != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("listen",         cl->listen);
    if (cl->fb             != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("fb",             cl->fb);
    if (cl->fb_all         != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("fb_all",         cl->fb_all);
    if (cl->fb_overlay     != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("fb_overlay",     cl->fb_overlay);
    if (cl->predictor      != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("predictor",      cl->predictor);
    if (cl->hot_key        != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("hot_key",        cl->hot_key);
    if (cl->context        != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("context",        cl->context);
    if (cl->sliders        != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("sliders",        cl->sliders);
    if (cl->perspective_mode != MGED_CLI_UNSET_INT) CLI_SETVAR_INT("perspective_mode", cl->perspective_mode);
    if (cl->linestyle      != '\0')               CLI_SETVAR_CHAR("linestyle",     cl->linestyle);
    if (cl->mouse_behavior != '\0')               CLI_SETVAR_CHAR("mouse_behavior", cl->mouse_behavior);
    if (cl->coords         != '\0')               CLI_SETVAR_CHAR("coords",        cl->coords);
    if (cl->rotate_about   != '\0')               CLI_SETVAR_CHAR("rotate_about",  cl->rotate_about);
    if (cl->transform      != '\0')               CLI_SETVAR_CHAR("transform",     cl->transform);
    if (cli_dbl_is_set(cl->perspective))         CLI_SETVAR_DBL("perspective",    cl->perspective);
    if (cli_dbl_is_set(cl->predictor_advance))   CLI_SETVAR_DBL("predictor_advance", cl->predictor_advance);
    if (cli_dbl_is_set(cl->predictor_length))    CLI_SETVAR_DBL("predictor_length",  cl->predictor_length);
    if (cli_dbl_is_set(cl->nmg_eu_dist))         CLI_SETVAR_DBL("nmg_eu_dist",    cl->nmg_eu_dist);
    if (cli_dbl_is_set(cl->eye_sep_dist))        CLI_SETVAR_DBL("eye_sep_dist",   cl->eye_sep_dist);

    /* --- Tcl mged_default array entries ---------------------------------- */
    if (cl->dm_type)
	Tcl_SetVar2(s->interp, "mged_default", "dm_type", cl->dm_type,  TCL_GLOBAL_ONLY);
    if (cl->geom)
	Tcl_SetVar2(s->interp, "mged_default", "geom",    cl->geom,     TCL_GLOBAL_ONLY);
    if (cl->ggeom)
	Tcl_SetVar2(s->interp, "mged_default", "ggeom",   cl->ggeom,    TCL_GLOBAL_ONLY);

    /* --- grid (rset g …) ------------------------------------------------- */
    if (cl->grid_draw != MGED_CLI_UNSET_INT) {
	bu_vls_printf(&cmd, "rset g draw %d", cl->grid_draw);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-draw: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cl->grid_snap != MGED_CLI_UNSET_INT) {
	bu_vls_printf(&cmd, "rset g snap %d", cl->grid_snap);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-snap: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cli_dbl_is_set(cl->grid_rh)) {
	bu_vls_printf(&cmd, "rset g rh %g", cl->grid_rh);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-rh: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cli_dbl_is_set(cl->grid_rv)) {
	bu_vls_printf(&cmd, "rset g rv %g", cl->grid_rv);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-rv: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cl->grid_mrh != MGED_CLI_UNSET_INT) {
	bu_vls_printf(&cmd, "rset g mrh %d", cl->grid_mrh);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-mrh: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cl->grid_mrv != MGED_CLI_UNSET_INT) {
	bu_vls_printf(&cmd, "rset g mrv %d", cl->grid_mrv);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --grid-mrv: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }

    /* --- colour scheme subset (rset cs …) -------------------------------- */
    if (cl->bg_color.set) {
	int r = 0, g = 0, b = 0;
	bu_color_to_rgb_ints(&cl->bg_color.color, &r, &g, &b);
	bu_vls_printf(&cmd, "rset cs bg %d %d %d", r, g, b);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --bg: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }
    if (cl->geo_def_color.set) {
	int r = 0, g = 0, b = 0;
	bu_color_to_rgb_ints(&cl->geo_def_color.color, &r, &g, &b);
	bu_vls_printf(&cmd, "rset cs geo_def %d %d %d", r, g, b);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --geo-color: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }

    /* --- --set VAR=VALUE escape hatch ------------------------------------ */
    for (size_t si = 0; si < BU_PTBL_LEN(&cl->set_pairs); si++) {
	const char *pair = (const char *)BU_PTBL_GET(&cl->set_pairs, si);
	const char *eq = strchr(pair, '=');
	if (!eq) continue;  /* validated in parse_mged_set; shouldn't happen */
	/* Split on the first '=' and set the Tcl variable.  The write-trace
	 * installed by mged_variable_setup() will call bu_struct_parse and
	 * fire any associated hook functions. */
	size_t klen = (size_t)(eq - pair);
	char *key = (char *)bu_malloc(klen + 1, "cli_set_key");
	bu_strlcpy(key, pair, klen + 1);
	Tcl_SetVar(s->interp, key, eq + 1, TCL_GLOBAL_ONLY);
	bu_free(key, "cli_set_key");
    }

    /* --- --rset ARGS escape hatch ---------------------------------------- */
    for (size_t ri = 0; ri < BU_PTBL_LEN(&cl->rset_pairs); ri++) {
	const char *args = (const char *)BU_PTBL_GET(&cl->rset_pairs, ri);
	bu_vls_printf(&cmd, "rset %s", args);
	if (Tcl_Eval(s->interp, bu_vls_cstr(&cmd)) != TCL_OK)
	    bu_log("mged: --rset \"%s\": %s\n", args, Tcl_GetStringResult(s->interp));
	bu_vls_trunc(&cmd, 0);
    }

#undef CLI_SETVAR_INT
#undef CLI_SETVAR_DBL
#undef CLI_SETVAR_CHAR

    bu_vls_free(&cmd);
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
    int read_only_flag=0;

    int parent_pipe[2] = {0, 0};
    int use_pipe = 0;
    int run_in_foreground=1;

#ifdef HAVE_PIPE
    int result;
#  ifndef HAVE_WINDOWS_H
    /* fd_set is used by select() in the fork-based backgrounding path,
     * which is POSIX-only (fork() and select()-on-pipes unavailable on Windows). */
    fd_set read_set;
#  endif
#endif

    BU_GET(MGED_STATE, struct mged_state);
    BU_GET(MGED_STATE->s_edit, struct mged_edit_state);
    MGED_STATE->s_edit->e = rt_edit_create(NULL, NULL, NULL, NULL);
    struct mged_state *s = MGED_STATE;
    s->magic = MGED_STATE_MAGIC;
    s->classic_mged = 1;
    s->interactive = 0; /* >0 means interactive, intentionally starts
                         * 0 to know when interactive, e.g., via -C
                         * option
			 */
    bu_vls_init(&s->input_str);
    bu_vls_init(&s->input_str_prefix);
    bu_vls_init(&s->scratchline);
    bu_vls_init(&s->mged_prompt);
    s->dpy_string = NULL;
    s->cmd_running = 0;
    s->log_drain_timer = NULL;
    s->pipe_mode = 0;

    /* Set up linked lists */
    s->vlfree = &rt_vlfree;
    BU_LIST_INIT(&rtg_headwdb.l);

    // Start out in an initialization state
    MGED_OPENDB_CTX_INIT(&mged_global_db_ctx);
    mged_global_db_ctx.init_flag = 1;

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

    /*
     * Parse command-line options using bu_opt.  All values are collected into
     * the mged_cli_overrides struct; "early" options (that affect behaviour
     * before Tcl/DM initialisation) are applied immediately below, while
     * "deferred" options (mged_variables, grid, colour) are applied after
     * do_rc() via apply_cli_overrides() so that CLI flags always win.
     */

    struct mged_cli_overrides cl;
    memset(&cl, 0, sizeof(cl));

    /* Initialise all int fields to MGED_CLI_UNSET_INT ("not given") */
    cl.use_air          = MGED_CLI_UNSET_INT;
    cl.dlist            = MGED_CLI_UNSET_INT;
    cl.faceplate        = MGED_CLI_UNSET_INT;
    cl.orig_gui         = MGED_CLI_UNSET_INT;
    cl.linewidth        = MGED_CLI_UNSET_INT;
    cl.port             = MGED_CLI_UNSET_INT;
    cl.listen           = MGED_CLI_UNSET_INT;
    cl.fb               = MGED_CLI_UNSET_INT;
    cl.fb_all           = MGED_CLI_UNSET_INT;
    cl.fb_overlay       = MGED_CLI_UNSET_INT;
    cl.predictor        = MGED_CLI_UNSET_INT;
    cl.hot_key          = MGED_CLI_UNSET_INT;
    cl.context          = MGED_CLI_UNSET_INT;
    cl.sliders          = MGED_CLI_UNSET_INT;
    cl.perspective_mode = MGED_CLI_UNSET_INT;
    cl.grid_draw        = MGED_CLI_UNSET_INT;
    cl.grid_snap        = MGED_CLI_UNSET_INT;
    cl.grid_mrh         = MGED_CLI_UNSET_INT;
    cl.grid_mrv         = MGED_CLI_UNSET_INT;
    /* bg_color and geo_def_color: memset-zero means .set == 0, which is
     * the correct "not given" initial state — no extra init needed. */

    /* Initialise double fields to MGED_CLI_UNSET_DBL ("not given") */
    cl.perspective        = MGED_CLI_UNSET_DBL;
    cl.predictor_advance  = MGED_CLI_UNSET_DBL;
    cl.predictor_length   = MGED_CLI_UNSET_DBL;
    cl.nmg_eu_dist        = MGED_CLI_UNSET_DBL;
    cl.eye_sep_dist       = MGED_CLI_UNSET_DBL;
    cl.grid_rh            = MGED_CLI_UNSET_DBL;
    cl.grid_rv            = MGED_CLI_UNSET_DBL;

    /* Initialise debug fields to the current global values so that an
     * unconditional copy back after parsing is a no-op when the user
     * does not supply -x/-X on the command line. */
    cl.rt_debug_val = (unsigned int)rt_debug;
    cl.bu_debug_val = (unsigned int)bu_debug;

    bu_ptbl_init(&cl.set_pairs,  8, "mged_cli_set_pairs");
    bu_ptbl_init(&cl.rset_pairs, 8, "mged_cli_rset_pairs");

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    /* Option table: the BU_OPT macro takes (slot, shortopt, longopt, arghelp,
     * argprocess, set_var, help) — 7 arguments.  Long-only options use NULL for
     * shortopt.  No-argument options use "" for arghelp and NULL for argprocess. */
    struct bu_opt_desc opt_defs[54];
    /* ---- Existing short options, now with long aliases ---- */
    BU_OPT(opt_defs[0],  "a", "attach",           "type",    bu_opt_str,      &cl.attach,           "display manager attach target");
    BU_OPT(opt_defs[1],  "d", "display",           "string",  bu_opt_str,      &cl.dpy_string,       "X display string");
    BU_OPT(opt_defs[2],  "r", "read-only",         "",        NULL,            &cl.read_only,        "open database read-only");
    BU_OPT(opt_defs[3],  "p", "pipe",              "",        NULL,            &cl.pipe_mode,        "pipe mode (emit CMD_DONE sentinels)");
    BU_OPT(opt_defs[4],  "c", "classic",           "",        NULL,            &cl.classic_mode,     "classic text-only mode");
    BU_OPT(opt_defs[5],  "C", "gui",               "",        NULL,            &cl.gui_mode,         "GUI (non-classic) mode");
    BU_OPT(opt_defs[6],  "b", "background",        "",        NULL,            &cl.background,       "run in background (fork)");
    BU_OPT(opt_defs[7],  "x", "rt-debug",          "hex",     parse_debug_uint,&cl.rt_debug_val,     "set librt debug flags (hex)");
    BU_OPT(opt_defs[8],  "X", "bu-debug",          "hex",     parse_debug_uint,&cl.bu_debug_val,     "set libbu debug flags (hex)");
    BU_OPT(opt_defs[9],  "v", "version",           "",        NULL,            &cl.print_version,    "print version info and exit");
    BU_OPT(opt_defs[10], "h", "help",              "",        NULL,            &cl.print_help,       "print help and exit");
    BU_OPT(opt_defs[11], "?", NULL,                "",        NULL,            &cl.print_help,       "print help and exit");
    /* -o is a developer option: preserved but not promoted with a long alias */
    BU_OPT(opt_defs[12], "o", NULL,                "",        NULL,            &cl.old_gui_flag,     "[developer] use old GUI");
    /* ---- rc file control ---- */
    BU_OPT(opt_defs[13], NULL, "rcfile",           "file",    bu_opt_str,      &cl.rcfile,           "use FILE instead of .mgedrc");
    BU_OPT(opt_defs[14], NULL, "no-rc",            "",        NULL,            &cl.skip_rc,          "skip loading any .mgedrc file");
    /* ---- general escape hatches ---- */
    BU_OPT(opt_defs[15], NULL, "set",              "VAR=VALUE",parse_mged_set, &cl.set_pairs,
	   "set an mged variable (e.g. --set use_air=1); applied after .mgedrc");
    BU_OPT(opt_defs[16], NULL, "rset",             "ARGS",    parse_rset,      &cl.rset_pairs,
	   "set an rset resource (e.g. --rset \"g snap 1\"); applied after .mgedrc");
    /* ---- mged_variables: boolean on/off pairs ---- */
    BU_OPT(opt_defs[17], NULL, "use-air",          "",        NULL,            &cl.use_air,          "enable use_air");
    BU_OPT(opt_defs[18], NULL, "no-use-air",       "",        flag_set_zero,   &cl.use_air,          "disable use_air");
    BU_OPT(opt_defs[19], NULL, "dlist",            "",        NULL,            &cl.dlist,            "enable display lists");
    BU_OPT(opt_defs[20], NULL, "no-dlist",         "",        flag_set_zero,   &cl.dlist,            "disable display lists");
    BU_OPT(opt_defs[21], NULL, "faceplate",        "",        NULL,            &cl.faceplate,        "show faceplate overlay");
    BU_OPT(opt_defs[22], NULL, "no-faceplate",     "",        flag_set_zero,   &cl.faceplate,        "hide faceplate overlay");
    BU_OPT(opt_defs[23], NULL, "predictor",        "",        NULL,            &cl.predictor,        "enable view predictor");
    BU_OPT(opt_defs[24], NULL, "no-predictor",     "",        flag_set_zero,   &cl.predictor,        "disable view predictor");
    /* ---- mged_variables: valued options ---- */
    BU_OPT(opt_defs[25], NULL, "linewidth",        "#",       bu_opt_int,      &cl.linewidth,        "wireframe line width (pixels, >=1)");
    BU_OPT(opt_defs[26], NULL, "linestyle",        "s|d",     bu_opt_char,     &cl.linestyle,        "line style: s=solid, d=dashed");
    BU_OPT(opt_defs[27], NULL, "perspective",      "#",       bu_opt_fastf_t,  &cl.perspective,      "perspective angle in degrees (-1=off)");
    BU_OPT(opt_defs[28], NULL, "eye-sep-dist",     "#",       bu_opt_fastf_t,  &cl.eye_sep_dist,     "stereo eye separation (mm, 0=mono)");
    BU_OPT(opt_defs[29], NULL, "port",             "#",       bu_opt_int,      &cl.port,             "framebuffer server listen port (0-65535)");
    BU_OPT(opt_defs[30], NULL, "coords",           "m|v",     bu_opt_char,     &cl.coords,           "constraint coords: m=model v=view");
    BU_OPT(opt_defs[31], NULL, "rotate-about",     "m|v|e",   bu_opt_char,     &cl.rotate_about,     "rotate center: m=model v=view e=eye");
    BU_OPT(opt_defs[32], NULL, "transform",        "v|a|e",   bu_opt_char,     &cl.transform,        "mouse transform: v=view a=adc e=edit");
    BU_OPT(opt_defs[33], NULL, "nmg-eu-dist",      "#",       bu_opt_fastf_t,  &cl.nmg_eu_dist,      "NMG edge-use distance tolerance");
    BU_OPT(opt_defs[34], NULL, "mouse-behavior",   "v|a|e",   bu_opt_char,     &cl.mouse_behavior,   "mouse behavior mode");
    BU_OPT(opt_defs[35], NULL, "predictor-advance","#",       bu_opt_fastf_t,  &cl.predictor_advance,"predictor advance time (s)");
    BU_OPT(opt_defs[36], NULL, "predictor-length", "#",       bu_opt_fastf_t,  &cl.predictor_length, "predictor trail length (s)");
    BU_OPT(opt_defs[37], NULL, "perspective-mode", "0|1",     bu_opt_int,      &cl.perspective_mode, "enable/disable perspective mode");
    BU_OPT(opt_defs[38], NULL, "context",          "0|1",     bu_opt_int,      &cl.context,          "context mode (0=off)");
    BU_OPT(opt_defs[39], NULL, "sliders",          "0|1",     bu_opt_int,      &cl.sliders,          "show sliders");
    BU_OPT(opt_defs[40], NULL, "hot-key",          "#",       bu_opt_int,      &cl.hot_key,          "hot key character code");
    BU_OPT(opt_defs[41], NULL, "fb-overlay",       "0|1|2",   bu_opt_int,      &cl.fb_overlay,       "framebuffer overlay: 0=under 1=inter 2=over");
    /* ---- window/display (Tcl mged_default array) ---- */
    BU_OPT(opt_defs[42], NULL, "dm-type",          "type",    bu_opt_str,      &cl.dm_type,          "display manager type (e.g. ogl, swrast)");
    BU_OPT(opt_defs[43], NULL, "geom",             "WxH+X+Y", bu_opt_str,      &cl.geom,             "command window geometry");
    BU_OPT(opt_defs[44], NULL, "ggeom",            "WxH+X+Y", bu_opt_str,      &cl.ggeom,            "graphics window geometry");
    /* ---- grid (rset g …) ---- */
    BU_OPT(opt_defs[45], NULL, "grid-draw",        "0|1",     bu_opt_int,      &cl.grid_draw,        "show/hide grid");
    BU_OPT(opt_defs[46], NULL, "grid-snap",        "0|1",     bu_opt_int,      &cl.grid_snap,        "enable/disable grid snap");
    BU_OPT(opt_defs[47], NULL, "grid-rh",          "#",       bu_opt_fastf_t,  &cl.grid_rh,          "horizontal grid resolution");
    BU_OPT(opt_defs[48], NULL, "grid-rv",          "#",       bu_opt_fastf_t,  &cl.grid_rv,          "vertical grid resolution");
    BU_OPT(opt_defs[49], NULL, "grid-mrh",         "#",       bu_opt_int,      &cl.grid_mrh,         "horizontal major grid interval");
    BU_OPT(opt_defs[50], NULL, "grid-mrv",         "#",       bu_opt_int,      &cl.grid_mrv,         "vertical major grid interval");
    /* ---- colour scheme subset (rset cs …) ---- */
    BU_OPT(opt_defs[51], NULL, "bg",               "R G B",   parse_opt_color, &cl.bg_color,         "background colour (0-255 per component, or #RRGGBB)");
    BU_OPT(opt_defs[52], NULL, "geo-color",        "R G B",   parse_opt_color, &cl.geo_def_color,    "default geometry wireframe colour");
    BU_OPT_NULL(opt_defs[53]);

    /* bu_opt_parse does not consume argv[0] (the program name).
     * Skip it manually so that the remaining args match what the
     * original bu_getopt-based code expected after optind adjustment. */
    int orig_argc = argc;
    argc--;
    argv++;
    argc = bu_opt_parse(&parse_msgs, (size_t)argc, (const char **)argv, opt_defs);

    if (bu_vls_strlen(&parse_msgs) > 0) {
	/* Print any warnings/errors from the parser */
	bu_log("%s", bu_vls_cstr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);

    if (argc < 0) {
	/* Fatal parse error */
	bu_exit(1, "Usage:  mged [options] [database [command]]\n"
		   "Run 'mged --help' for a full option list.\n");
    }

    /* ---- Apply early options immediately --------------------------------- */

    /* -v / --version */
    if (cl.print_version) {
	printf("%s%s%s%s%s%s\n",
	       brlcad_ident("MGED Geometry Editor"),
	       dm_version(),
	       fb_version(),
	       rt_version(),
	       bn_version(),
	       bu_version());
	return EXIT_SUCCESS;
    }

    /* -h / --help / -? */
    if (cl.print_help) {
	struct bu_vls usage = BU_VLS_INIT_ZERO;
	bu_vls_printf(&usage, "Usage:  mged [options] [database [command]]\n\n");
	char *help_str = bu_opt_describe(opt_defs, NULL);
	if (help_str) {
	    bu_vls_printf(&usage, "Options:\n%s", help_str);
	    bu_free(help_str, "bu_opt_describe");
	}
	bu_exit(1, "%s\n", bu_vls_cstr(&usage));
    }

    /* -o (developer option: use old GUI) */
    if (cl.old_gui_flag) {
	bu_log("WARNING: -o is a developer option and subject to change.  Do not use.\n");
	old_mged_gui = 0;
    }

    /* -x / --rt-debug  and  -X / --bu-debug: parse_debug_uint() stores the
     * parsed hex value in cl.rt_debug_val / cl.bu_debug_val.  Both fields
     * were initialised to the current values of rt_debug / bu_debug (see
     * below) so an unconditional copy here is safe: if the user did not
     * supply -x/-X the value is unchanged. */
    rt_debug = (int)cl.rt_debug_val;
    bu_debug = (unsigned int)cl.bu_debug_val;

    /* -a / --attach */
    if (cl.attach)
	attach = (char *)cl.attach;

    /* -d / --display */
    if (cl.dpy_string)
	s->dpy_string = (char *)cl.dpy_string;

    /* -r / --read-only */
    if (cl.read_only)
	read_only_flag = 1;

    /* -p / --pipe */
    if (cl.pipe_mode)
	s->pipe_mode = 1;

    /* -b / --background */
    if (cl.background)
	run_in_foreground = 0;

    /* -c / --classic and -C / --gui (mutually exclusive; last one wins) */
    if (cl.classic_mode) {
	s->classic_mged = 2; /* >1 to indicate explicitly requested */
    }
    if (cl.gui_mode) {
	s->classic_mged = 0;
	s->interactive = 2;  /* >1 to indicate explicitly requested */
    }

    /* Change the working directory to BU_DIR_HOME if invoked with no
     * arguments at all (not even options). */
    if (orig_argc == 1) {
	const char *homed = bu_dir(NULL, 0, BU_DIR_HOME, NULL);
	if (homed && chdir(homed)) {
	    bu_exit(1, "Failed to change working directory to \"%s\" ", homed);
	}
    }

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

#if defined(HAVE_PIPE) && !defined(HAVE_WINDOWS_H)
    /* fork()-based background detach: POSIX only. fork() and select() on
     * pipe file descriptors are not available on Windows. */
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
#endif /* HAVE_PIPE && !HAVE_WINDOWS_H */

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

    MAT_IDN(MEDIT(s)->model_changes);
    MAT_IDN(MEDIT(s)->acc_rot_sol);

    s->global_editing_state = ST_VIEW;
    MEDIT(s)->edit_flag = -1;
    s->s_edit->es_edclass = EDIT_CLASS_NULL;
    MEDIT(s)->e_inpara = newedge = 0;

    /* These values match old GED.  Use 'tol' command to change them. */
    s->tol.tol.magic = BN_TOL_MAGIC;
    s->tol.tol.dist = 0.0005;
    s->tol.tol.dist_sq = s->tol.tol.dist * s->tol.tol.dist;
    s->tol.tol.perp = 1e-6;
    s->tol.tol.para = 1 - s->tol.tol.perp;

    /* NOTE - at the moment, primitives such as rpc/rhc need this set to plot
     * properly after editing - otherwise post-edit wireframe restoration
     * looks coarse.  Probably needs more investigation. */
    s->tol.rel_tol = 0.01;             /* 1%, by default */

    rt_prep_timer();		/* Initialize timer */

    MEDIT(s)->edit_flag = -1;		/* no solid editing just now */

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

#ifndef HAVE_WINDOWS_H
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
	    const char *av[4];

	    av[0] = "opendb";
	    av[1] = "-c";
	    av[2] = argv[0];
	    av[3] = NULL;

	    /* Command line may have more than 2 args, opendb only wants 2
	     * expecting second to be the file name.
	     * NOTE: this way makes it so f_opendb does not care about y/n
	     * and always create a new db if one does not exist since we want
	     * to allow mged to process args after the db as a command
	     */
	    struct cmdtab ec = {MGED_CMD_MAGIC, NULL, NULL, NULL, s};
	    if (f_opendb(&ec, s->interp, 3, av) == TCL_ERROR) {
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

	/* This is an interactive mged, process .mgedrc (unless --no-rc or
	 * --rcfile overrides the default search logic). */
	do_rc(s, cl.skip_rc, cl.rcfile);

	/* Apply CLI overrides AFTER .mgedrc so command-line flags always win. */
	apply_cli_overrides(s, &cl);

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

#ifndef HAVE_WINDOWS_H
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
	/* !interactive: no .mgedrc is processed, but CLI overrides still apply */
	apply_cli_overrides(s, &cl);

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

	/* In GUI mode, openw.tcl's init_mged_gui applies mged_default(ggeom)
	 * to set the DM window size and position.  Classic mode bypasses that
	 * code path entirely, so honour the same preference here.  A user sets
	 * this in their .mgedrc before the attach happens, e.g.:
	 *   set mged_default(ggeom) 512x512+0+0
	 */
	if (DMP && dm_graphical(DMP)) {
	    struct bu_vls *pn = dm_get_pathname(DMP);
	    const char *ggeom = Tcl_GetVar(s->interp, "mged_default(ggeom)", TCL_GLOBAL_ONLY);
	    if (pn && bu_vls_strlen(pn) && ggeom && strlen(ggeom)) {
		struct bu_vls geom_cmd = BU_VLS_INIT_ZERO;
		/* Brace the geometry string so Tcl does not interpret any
		 * special characters that might appear in a user's .mgedrc. */
		bu_vls_printf(&geom_cmd, "wm geometry %s {%s}", bu_vls_cstr(pn), ggeom);
		if (Tcl_Eval(s->interp, bu_vls_cstr(&geom_cmd)) != TCL_OK)
		    bu_log("mged: failed to apply mged_default(ggeom) \"%s\": %s\n",
			   ggeom, Tcl_GetStringResult(s->interp));
		bu_vls_free(&geom_cmd);
		/* Flush pending Tk events so the resize takes effect before
		 * the main event loop starts. */
		(void)Tcl_Eval(s->interp, "update");
	    }
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

	/* Use Tcl's canonical stdin channel — Tcl_GetStdChannel works on all
	 * platforms (POSIX and Windows) and avoids mixing Tcl's channel
	 * buffering with a separately-created file channel. */
	sd->chan = Tcl_GetStdChannel(TCL_STDIN);
	if (sd->chan == NULL) {
	    bu_log("mged: unable to get stdin channel\n");
	    BU_PUT(sd, struct stdio_data);
	    mged_finish(s, 1);
	}

	/* Set non-blocking mode so that Tcl_Read/Tcl_Gets inside the channel
	 * handler returns whatever bytes are immediately available rather than
	 * looping until the full requested count is accumulated.  Without this,
	 * Tcl_Read(chan, buf, BU_PAGE_SIZE) on a blocking channel calls read()
	 * repeatedly in cbreak mode — each call returning just one character —
	 * until 4096 characters have been accumulated, permanently stalling the
	 * event loop. */
	Tcl_SetChannelOption(NULL, sd->chan, "-blocking", "0");

	Tcl_CreateChannelHandler(sd->chan, TCL_READABLE, stdin_input, sd);

#ifdef SIGINT
	(void)signal(SIGINT, SIG_IGN);
#endif

	bu_vls_strcpy(&s->mged_prompt, MGED_PROMPT);
	pr_prompt(s);

#ifndef HAVE_WINDOWS_H
	if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	}
#endif
    } else {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	int sout = fileno(stdout);
	int serr = fileno(stderr);

#ifdef HAVE_PIPE
	/* stash stdout and stderr so we can restore them at exit */
	stdfd[0] = dup(sout);
	if (stdfd[0] == -1)
	    perror("dup");

	stdfd[1] = dup(serr);
	if (stdfd[1] == -1)
	    perror("dup");
#endif

	bu_vls_printf(&vls, "output_hook output_callback");
	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	/* Redirect stdout/stderr into POSIX pipes so that any C-level output
	 * (printf, write(1,...), bu_log, etc.) is captured by the GUI.
	 * Windows supports pipe()/dup()/dup2() through its CRT, so this
	 * approach works on all platforms when HAVE_PIPE is available.
	 * NOTE: Tcl_MakeFileChannel on Windows requires a native HANDLE (via
	 * _get_osfhandle), not the raw CRT fd — see the fix below. */
#ifdef HAVE_PIPE
	{
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

#ifdef HAVE_WINDOWS_H
	    /* On Windows, Tcl's stdout/stderr channels are backed by the native
	     * Windows HANDLEs that Tcl captured at startup.  In a GUI-only
	     * process those are typically INVALID_HANDLE_VALUE (no console), so
	     * any attempt by Tcl to write to them — including `puts test` — fails
	     * with "error writing 'stdout': invalid argument".
	     *
	     * After the dup() calls above, CRT fd 1/fd 2 now point to the write
	     * ends of our pipes.  Create new Tcl writable channels from those
	     * HANDLEs and install them as Tcl's stdout/stderr so that `puts`
	     * writes into the pipe (and thus reaches std_out_or_err below).
	     *
	     * Order matters: register the new channel and set it as the std
	     * channel BEFORE unregistering the old one.  Tcl_GetChannel(interp,
	     * "stdout", ...) — used by `puts stdout ...` — checks the
	     * interpreter's channel hash table first; the old channel is
	     * registered there as "stdout" and would be returned instead of the
	     * new pipe channel.  Unregistering it causes the lookup to fall
	     * through to Tcl_GetStdChannel, which by then returns the new
	     * channel.
	     *
	     * This ordering also avoids the write-thread race that existed in
	     * the old Windows-only code path: that code called
	     * Tcl_UnregisterChannel BEFORE Tcl_MakeFileChannel, so the old
	     * channel's I/O completion-port threads could still be running when
	     * new ones were spawned, causing intermittent corruption.  A
	     * `puts ""` sync-point was needed as a workaround.  Here the new
	     * channel is fully created and its threads are running BEFORE the
	     * old channel begins tearing down, so no such sync is required.
	     * The old channel was also backed by INVALID_HANDLE_VALUE (no
	     * console), so no I/O threads were ever successfully dispatched on
	     * it. */
	    {
		Tcl_Channel old_out = Tcl_GetStdChannel(TCL_STDOUT);
		Tcl_Channel wout = Tcl_MakeFileChannel(
		    (ClientData)_get_osfhandle(fileno(stdout)), TCL_WRITABLE);
		if (wout) {
		    Tcl_SetChannelOption(s->interp, wout, "-blocking", "false");
		    Tcl_SetChannelOption(s->interp, wout, "-buffering", "line");
		    Tcl_RegisterChannel(s->interp, wout);
		    Tcl_SetStdChannel(wout, TCL_STDOUT);
		    if (old_out)
			Tcl_UnregisterChannel(s->interp, old_out);
		} else {
		    bu_log("mged: failed to create Tcl channel for stdout pipe\n");
		}

		Tcl_Channel old_err = Tcl_GetStdChannel(TCL_STDERR);
		Tcl_Channel werr = Tcl_MakeFileChannel(
		    (ClientData)_get_osfhandle(fileno(stderr)), TCL_WRITABLE);
		if (werr) {
		    Tcl_SetChannelOption(s->interp, werr, "-blocking", "false");
		    Tcl_SetChannelOption(s->interp, werr, "-buffering", "line");
		    Tcl_RegisterChannel(s->interp, werr);
		    Tcl_SetStdChannel(werr, TCL_STDERR);
		    if (old_err)
			Tcl_UnregisterChannel(s->interp, old_err);
		} else {
		    bu_log("mged: failed to create Tcl channel for stderr pipe\n");
		}
	    }
#endif /* HAVE_WINDOWS_H */

	    Tcl_Channel out_chan, err_chan;

	    /* On Windows, Tcl_MakeFileChannel expects a native Windows HANDLE,
	     * not a CRT integer file descriptor.  Passing a raw CRT fd as a
	     * HANDLE produces an invalid channel whose error state causes Tcl's
	     * event loop to spin continuously, freezing the GUI.  Use
	     * _get_osfhandle() to obtain the correct Windows HANDLE from the
	     * CRT fd.  On POSIX the fd is cast directly, as before. */
#ifdef HAVE_WINDOWS_H
	    out_chan = Tcl_MakeFileChannel((ClientData)_get_osfhandle(pipe_out[0]), TCL_READABLE);
	    err_chan = Tcl_MakeFileChannel((ClientData)_get_osfhandle(pipe_err[0]), TCL_READABLE);
#else
	    out_chan = Tcl_MakeFileChannel((ClientData)(size_t)pipe_out[0], TCL_READABLE);
	    err_chan = Tcl_MakeFileChannel((ClientData)(size_t)pipe_err[0], TCL_READABLE);
#endif

	    /* Register channels with the interpreter so they are tracked and
	     * kept alive until the interpreter is deleted. */
	    /* Non-blocking mode is essential: Tcl_Read on a blocking channel
	     * uses DoRead(allowShortReads=0), which loops calling read() until
	     * the full requested byte count is accumulated.  For a pipe that
	     * delivers only a few bytes ("test\n"), this causes read() to block
	     * on the empty pipe waiting for more data.  Non-blocking mode makes
	     * Tcl_Read return whatever bytes are currently available. */
	    Tcl_SetChannelOption(s->interp, out_chan, "-blocking", "0");
	    Tcl_RegisterChannel(s->interp, out_chan);
	    Tcl_CreateChannelHandler(out_chan, TCL_READABLE, std_out_or_err, out_chan);

	    Tcl_SetChannelOption(s->interp, err_chan, "-blocking", "0");
	    Tcl_RegisterChannel(s->interp, err_chan);
	    Tcl_CreateChannelHandler(err_chan, TCL_READABLE, std_out_or_err, err_chan);
	}
#endif /* HAVE_PIPE */
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
