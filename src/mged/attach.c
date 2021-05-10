/*                        A T T A C H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2021 United States Government as represented by
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
/** @file mged/attach.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bnetwork.h"

/* Make sure this comes after bio.h (for Windows) */
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include "vmath.h"
#include "bu/env.h"
#include "bu/ptbl.h"
#include "ged.h"
#include "tclcad.h"

#include "./mged.h"
#include "./titles.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* Geometry display instances used by MGED */
struct bu_ptbl active_dm_set = BU_PTBL_INIT_ZERO;  /* set of active display managers */
struct mged_dm *mged_curr_dm = NULL;
struct mged_dm *mged_dm_init_state = NULL;


extern struct _color_scheme default_color_scheme;
extern void share_dlist(struct mged_dm *dlp2);	/* defined in share.c */
int mged_default_dlist = 0;   /* This variable is available via Tcl for controlling use of display lists */

static fastf_t windowbounds[6] = { XMIN, XMAX, YMIN, YMAX, (int)GED_MIN, (int)GED_MAX };

/* If we changed the active dm, need to update GEDP as well.. */
void set_curr_dm(struct mged_dm *nc)
{
    mged_curr_dm = nc;
    if (nc != MGED_DM_NULL && nc->dm_view_state) {
	GEDP->ged_gvp = nc->dm_view_state->vs_gvp;
	GEDP->ged_gvp->gv_grid = *nc->dm_grid_state; /* struct copy */
    } else {
	if (GEDP) {
	    GEDP->ged_gvp = NULL;
	}
    }
}

int
mged_dm_init(struct mged_dm *o_dm,
	const char *dm_type,
	int argc,
	const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm);

    /* register application provided routines */
    cmd_hook = dm_commands;

    if ((DMP = dm_open(NULL, (void *)INTERP, dm_type, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Ogl's private structure */
    dm_set_vp(DMP, &view_state->vs_gvp->gv_scale);
    dm_set_perspective(DMP, mged_variables->mv_perspective_mode);

#ifdef HAVE_TK
    if (dm_graphical(DMP)) {
	Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
	Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    }
#endif
    (void)dm_configure_win(DMP, 0);

    struct bu_vls *pathname = dm_get_pathname(DMP);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_cstr(pathname));
	Tcl_Eval(INTERP, bu_vls_cstr(&vls));
    }
    bu_vls_free(&vls);

    return TCL_OK;
}



void
mged_fb_open(void)
{
    fbp = dm_get_fb(DMP);
}


void
mged_slider_init_vls(struct mged_dm *p)
{
    bu_vls_init(&p->dm_fps_name);
    bu_vls_init(&p->dm_aet_name);
    bu_vls_init(&p->dm_ang_name);
    bu_vls_init(&p->dm_center_name);
    bu_vls_init(&p->dm_size_name);
    bu_vls_init(&p->dm_adc_name);
}


void
mged_slider_free_vls(struct mged_dm *p)
{
    if (BU_VLS_IS_INITIALIZED(&p->dm_fps_name)) {
	bu_vls_free(&p->dm_fps_name);
	bu_vls_free(&p->dm_aet_name);
	bu_vls_free(&p->dm_ang_name);
	bu_vls_free(&p->dm_center_name);
	bu_vls_free(&p->dm_size_name);
	bu_vls_free(&p->dm_adc_name);
    }
}


int
release(char *name, int need_close)
{
    struct mged_dm *save_dm_list = MGED_DM_NULL;
    struct bu_vls *pathname = NULL;

    if (name != NULL) {
	struct mged_dm *p = MGED_DM_NULL;

	if (BU_STR_EQUAL("nu", name))
	    return TCL_OK;  /* Ignore */

	for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	    struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	    if (!m_dmp || !m_dmp->dm_dmp)
		continue;

	    pathname = dm_get_pathname(m_dmp->dm_dmp);
	    if (!BU_STR_EQUAL(name, bu_vls_cstr(pathname)))
		continue;

	    /* found it */
	    if (p != mged_curr_dm) {
		save_dm_list = mged_curr_dm;
		p = m_dmp;
		set_curr_dm(p);
	    }
	    break;
	}

	if (p == MGED_DM_NULL) {
	    Tcl_AppendResult(INTERP, "release: ", name, " not found\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else if (DMP && BU_STR_EQUAL("nu", bu_vls_cstr(dm_get_pathname(DMP))))
	return TCL_OK;  /* Ignore */

    if (fbp) {
	if (mged_variables->mv_listen) {
	    /* drop all clients */
	    mged_variables->mv_listen = 0;
	    fbserv_set_port(NULL, NULL, NULL, NULL, NULL);
	}

	/* release framebuffer resources */
	fb_close_existing(fbp);
	fbp = (struct fb *)NULL;
    }

    /*
     * This saves the state of the resources to the "nu" display
     * manager, which is beneficial only if closing the last display
     * manager. So when another display manager is opened, it looks
     * like the last one the user had open.
     */
    usurp_all_resources(mged_dm_init_state, mged_curr_dm);

    /* If this display is being referenced by a command window, then
     * remove the reference.
     */
    if (mged_curr_dm->dm_tie != NULL)
	mged_curr_dm->dm_tie->cl_tie = (struct mged_dm *)NULL;

    if (need_close)
	dm_close(DMP);

    RT_FREE_VLIST(&mged_curr_dm->dm_p_vlist);
    bu_ptbl_rm(&active_dm_set, (long *)mged_curr_dm);
    mged_slider_free_vls(mged_curr_dm);
    bu_free((void *)mged_curr_dm, "release: mged_curr_dm");

    if (save_dm_list != MGED_DM_NULL)
	set_curr_dm(save_dm_list);
    else {
	if (BU_PTBL_LEN(&active_dm_set) > 0) {
	    set_curr_dm((struct mged_dm *)BU_PTBL_GET(&active_dm_set, 0));
	} else {
	    set_curr_dm(MGED_DM_NULL);
	}
    }
    return TCL_OK;
}


int
f_release(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&vls, "help release");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (argc == 2) {
	int status;

	if (*argv[1] != '.')
	    bu_vls_printf(&vls, ".%s", argv[1]);
	else
	    bu_vls_strcpy(&vls, argv[1]);

	status = release(bu_vls_addr(&vls), 1);

	bu_vls_free(&vls);
	return status;
    } else
	return release((char *)NULL, 1);
}


static void
print_valid_dm(Tcl_Interp *interpreter)
{
    Tcl_AppendResult(interpreter, "\tThe following display manager types are valid: ", (char *)NULL);
    struct bu_vls dm_types = BU_VLS_INIT_ZERO;
    dm_list_types(&dm_types, " ");

    if (bu_vls_strlen(&dm_types)) {
	Tcl_AppendResult(interpreter, bu_vls_cstr(&dm_types), (char *)NULL);
    } else {
	Tcl_AppendResult(interpreter, "NONE AVAILABLE", (char *)NULL);
    }
    bu_vls_free(&dm_types);
    Tcl_AppendResult(interpreter, "\n", (char *)NULL);
}


int
f_attach(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    if (argc < 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help attach");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	print_valid_dm(interpreter);

	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[argc-1], "nu")) {
	/* nothing to do */
	return TCL_OK;
    }

    if (!dm_valid_type(argv[argc-1], NULL)) {
	Tcl_AppendResult(interpreter, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);
	print_valid_dm(interpreter);
	return TCL_ERROR;
    }

    return mged_attach(argv[argc - 1], argc, argv);
}


int
gui_setup(const char *dstr)
{
#ifdef HAVE_TK
    Tk_GenericProc *handler = doEvent;
#endif
    /* initialize only once */
    if (tkwin != NULL)
	return TCL_OK;

    Tcl_ResetResult(INTERP);

    /* set DISPLAY to dstr */
    if (dstr != (char *)NULL) {
	Tcl_SetVar(INTERP, "env(DISPLAY)", dstr, TCL_GLOBAL_ONLY);
    }

#ifdef HAVE_TK
    /* This runs the tk.tcl script */
    if (Tk_Init(INTERP) == TCL_ERROR) {
	const char *result = Tcl_GetStringResult(INTERP);
	/* hack to avoid a stupid Tk error */
	if (bu_strncmp(result, "this isn't a Tk applicationcouldn't", 35) == 0) {
	    result = (result + 27);
	    Tcl_ResetResult(INTERP);
	    Tcl_AppendResult(INTERP, result, (char *)NULL);
	}
	return TCL_ERROR;
    }

    /* Initialize [incr Tk] */
    if (Tcl_Eval(INTERP, "package require Itk") != TCL_OK) {
      return TCL_ERROR;
    }

    /* Import [incr Tk] commands into the global namespace */
    if (Tcl_Import(INTERP, Tcl_GetGlobalNamespace(INTERP),
		   "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
	return TCL_ERROR;
    }
#endif

    /* Initialize the Iwidgets package */
    if (Tcl_Eval(INTERP, "package require Iwidgets") != TCL_OK) {
	return TCL_ERROR;
    }

    /* Import iwidgets into the global namespace */
    if (Tcl_Import(INTERP, Tcl_GetGlobalNamespace(INTERP),
		   "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Initialize libtclcad */
#ifdef HAVE_TK
    (void)tclcad_init(INTERP, 1, NULL);
#else
    (void)tclcad_init(INTERP, 0, NULL);
#endif

#ifdef HAVE_TK
    if ((tkwin = Tk_MainWindow(INTERP)) == NULL) {
	return TCL_ERROR;
    }

    /* create the event handler */
    Tk_CreateGenericHandler(handler, (ClientData)NULL);

    Tcl_Eval(INTERP, "wm withdraw .");
    Tcl_Eval(INTERP, "tk appname mged");
#endif

    return TCL_OK;
}


int
mged_attach(const char *wp_name, int argc, const char *argv[])
{
    int opt_argc;
    char **opt_argv;
    struct mged_dm *o_dm;

    if (!wp_name) {
	return TCL_ERROR;
    }

    o_dm = mged_curr_dm;
    BU_ALLOC(mged_curr_dm, struct mged_dm);

    /* initialize predictor stuff */
    BU_LIST_INIT(&mged_curr_dm->dm_p_vlist);
    predictor_init();

    /* Only need to do this once */
    if (tkwin == NULL && BU_STR_EQUIV(dm_graphics_system(wp_name), "Tk")) {
	struct dm *tmp_dmp;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	/* look for "-d display_string" and use it if provided */
	tmp_dmp = dm_get();

	opt_argc = argc - 1;
	opt_argv = bu_argv_dup(opt_argc, argv + 1);
	dm_processOptions(tmp_dmp, &tmp_vls, opt_argc, (const char **)opt_argv);
	bu_argv_free(opt_argc, opt_argv);

	struct bu_vls *dname = dm_get_dname(tmp_dmp);
	if (dname && bu_vls_strlen(dname)) {
	    if (gui_setup(bu_vls_cstr(dname)) == TCL_ERROR) {
		bu_free((void *)mged_curr_dm, "f_attach: dm_list");
		set_curr_dm(o_dm);
		bu_vls_free(&tmp_vls);
		dm_put(tmp_dmp);
		return TCL_ERROR;
	    }
	} else if (gui_setup((char *)NULL) == TCL_ERROR) {
	    bu_free((void *)mged_curr_dm, "f_attach: dm_list");
	    set_curr_dm(o_dm);
	    bu_vls_free(&tmp_vls);
	    dm_put(tmp_dmp);
	    return TCL_ERROR;
	}

	bu_vls_free(&tmp_vls);
	dm_put(tmp_dmp);
    }

    bu_ptbl_ins(&active_dm_set, (long *)mged_curr_dm);

    if (!wp_name) {
	return TCL_ERROR;
    }

    if (mged_dm_init(o_dm, wp_name, argc, argv) == TCL_ERROR) {
	goto Bad;
    }

    /* initialize the background color */
    {
	/* need dummy values for func signature--they are unused in the func */
	const struct bu_structparse *sdp = 0;
	const char name[] = "name";
	void *base = 0;
	const char value[] = "value";
	cs_set_bg(sdp, name, base, value, NULL);
    }

    mged_link_vars(mged_curr_dm);

    Tcl_ResetResult(INTERP);
    const char *dm_name = dm_get_dm_name(DMP);
    const char *dm_lname = dm_get_dm_lname(DMP);
    if (dm_name && dm_lname) {
	Tcl_AppendResult(INTERP, "ATTACHING ", dm_name, " (", dm_lname,	")\n", (char *)NULL);
    }

    share_dlist(mged_curr_dm);

    if (dm_get_displaylist(DMP) && mged_variables->mv_dlist && !dlist_state->dl_active) {
	createDLists(GEDP->ged_gdp->gd_headDisplay);
	dlist_state->dl_active = 1;
    }

    (void)dm_make_current(DMP);
    (void)dm_set_win_bounds(DMP, windowbounds);
    mged_fb_open();

    GEDP->ged_gvp = mged_curr_dm->dm_view_state->vs_gvp;
    GEDP->ged_gvp->gv_grid = *mged_curr_dm->dm_grid_state; /* struct copy */

    return TCL_OK;

 Bad:
    Tcl_AppendResult(INTERP, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);

    if (DMP != (struct dm *)0)
	release((char *)NULL, 1);  /* release() will call dm_close */
    else
	release((char *)NULL, 0);  /* release() will not call dm_close */

    return TCL_ERROR;
}


#define MAX_ATTACH_RETRIES 100

void
get_attached(void)
{
    char *tok;
    int inflimit = MAX_ATTACH_RETRIES;
    int ret;
    struct bu_vls avail_types = BU_VLS_INIT_ZERO;
    struct bu_vls wanted_type = BU_VLS_INIT_ZERO;
    struct bu_vls prompt = BU_VLS_INIT_ZERO;

    const char *DELIM = " ";

    dm_list_types(&avail_types, DELIM);

    bu_vls_sprintf(&prompt, "attach (nu");
    for (tok = strtok(bu_vls_addr(&avail_types), " "); tok; tok = strtok(NULL, " ")) {
	if (BU_STR_EQUAL(tok, "nu"))
	    continue;
	if (BU_STR_EQUAL(tok, "plot"))
	    continue;
	if (BU_STR_EQUAL(tok, "postscript"))
	    continue;
	bu_vls_printf(&prompt, " %s", tok);
    }
    bu_vls_printf(&prompt, ")[nu]? ");

    bu_vls_free(&avail_types);

    while (inflimit > 0) {
	bu_log("%s", bu_vls_cstr(&prompt));

	ret = bu_vls_gets(&wanted_type, stdin);
	if (ret < 0) {
	    /* handle EOF */
	    bu_log("\n");
	    bu_vls_free(&wanted_type);
	    bu_vls_free(&prompt);
	    return;
	}

	if (bu_vls_strlen(&wanted_type) == 0 || BU_STR_EQUAL(bu_vls_addr(&wanted_type), "nu")) {
	    /* Nothing more to do. */
	    bu_vls_free(&wanted_type);
	    bu_vls_free(&prompt);
	    return;
	}

	/* trim whitespace before comparisons (but not before checking empty) */
	bu_vls_trimspace(&wanted_type);

	if (dm_valid_type(bu_vls_cstr(&wanted_type), NULL)) {
	    break;
	}

	/* Not a valid choice, loop. */
	inflimit--;
    }

    bu_vls_free(&prompt);

    if (inflimit <= 0) {
	bu_log("\nInfinite loop protection, attach aborted!\n");
	bu_vls_free(&wanted_type);
	return;
    }

    bu_log("Starting an %s display manager\n", bu_vls_cstr(&wanted_type));

    int argc = 1;
    const char *argv[3];
    argv[0] = "";
    argv[1] = "";
    argv[2] = (char *)NULL;
    (void)mged_attach(bu_vls_cstr(&wanted_type), argc, argv);
    bu_vls_free(&wanted_type);
}


/*
 * Run a display manager specific command(s).
 */
int
f_dm(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 2) {
	bu_vls_printf(&vls, "help dm");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "valid")) {
	if (argc < 3) {
	    bu_vls_printf(&vls, "help dm");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	if (dm_valid_type(argv[argc-1], NULL)) {
	    Tcl_AppendResult(interpreter, argv[argc-1], (char *)NULL);
	}
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "type")) {
	if (argc != 2) {
	    bu_vls_printf(&vls, "help dm");
	    Tcl_Eval(interpreter, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interpreter, dm_get_type(DMP), (char *)NULL);
	return TCL_OK;
    }

    if (!cmd_hook) {
	const char *dm_name = dm_get_dm_name(DMP);
	if (dm_name) {
	    Tcl_AppendResult(interpreter, "The '", dm_name,
		    "' display manager does not support local commands.\n",
		    (char *)NULL);
	}
	return TCL_ERROR;
    }


    return cmd_hook(argc-1, argv+1);
}


/**
 * Returns -
 *  0  If the display manager goes to a real screen.
 * !0  If the null display manager is attached.
 */
int
is_dm_null(void)
{
    return mged_curr_dm == mged_dm_init_state;
}


void
dm_var_init(struct mged_dm *target_dm)
{
    BU_ALLOC(adc_state, struct _adc_state);
    *adc_state = *target_dm->dm_adc_state;		/* struct copy */
    adc_state->adc_rc = 1;

    BU_ALLOC(menu_state, struct _menu_state);
    *menu_state = *target_dm->dm_menu_state;		/* struct copy */
    menu_state->ms_rc = 1;

    BU_ALLOC(rubber_band, struct _rubber_band);
    *rubber_band = *target_dm->dm_rubber_band;		/* struct copy */
    rubber_band->rb_rc = 1;

    BU_ALLOC(mged_variables, struct _mged_variables);
    *mged_variables = *target_dm->dm_mged_variables;	/* struct copy */
    mged_variables->mv_rc = 1;
    mged_variables->mv_dlist = mged_default_dlist;
    mged_variables->mv_listen = 0;
    mged_variables->mv_port = 0;
    mged_variables->mv_fb = 0;

    BU_ALLOC(color_scheme, struct _color_scheme);

    /* initialize using the nu display manager */
    if (mged_dm_init_state && mged_dm_init_state->dm_color_scheme) {
	*color_scheme = *mged_dm_init_state->dm_color_scheme;
    }

    color_scheme->cs_rc = 1;

    BU_ALLOC(grid_state, struct bv_grid_state);
    *grid_state = *target_dm->dm_grid_state;		/* struct copy */
    grid_state->rc = 1;

    BU_ALLOC(axes_state, struct _axes_state);
    *axes_state = *target_dm->dm_axes_state;		/* struct copy */
    axes_state->ax_rc = 1;

    BU_ALLOC(dlist_state, struct _dlist_state);
    dlist_state->dl_rc = 1;

    BU_ALLOC(view_state, struct _view_state);
    *view_state = *target_dm->dm_view_state;			/* struct copy */
    BU_ALLOC(view_state->vs_gvp, struct bv);
    BU_GET(view_state->vs_gvp->callbacks, struct bu_ptbl);
    bu_ptbl_init(view_state->vs_gvp->callbacks, 8, "bv callbacks");

    *view_state->vs_gvp = *target_dm->dm_view_state->vs_gvp;	/* struct copy */
    view_state->vs_gvp->gv_clientData = (void *)view_state;
    view_state->vs_gvp->adaptive_plot = 0;
    view_state->vs_gvp->redraw_on_zoom = 0;
    view_state->vs_gvp->point_scale = 1.0;
    view_state->vs_gvp->curve_scale = 1.0;
    view_state->vs_rc = 1;
    view_ring_init(mged_curr_dm->dm_view_state, (struct _view_state *)NULL);

    DMP_dirty = 1;
    if (target_dm->dm_dmp) {
	dm_set_dirty(target_dm->dm_dmp, 1);
    }
    mapped = 1;
    netfd = -1;
    owner = 1;
    am_mode = AMM_IDLE;
    adc_auto = 1;
    grid_auto_size = 1;
}


void
mged_link_vars(struct mged_dm *p)
{
    mged_slider_init_vls(p);
    struct bu_vls *pn = dm_get_pathname(p->dm_dmp);
    if (pn) {
	bu_vls_printf(&p->dm_fps_name, "%s(%s,fps)", MGED_DISPLAY_VAR,	bu_vls_cstr(pn));
	bu_vls_printf(&p->dm_aet_name, "%s(%s,aet)", MGED_DISPLAY_VAR,	bu_vls_cstr(pn));
	bu_vls_printf(&p->dm_ang_name, "%s(%s,ang)", MGED_DISPLAY_VAR,	bu_vls_cstr(pn));
	bu_vls_printf(&p->dm_center_name, "%s(%s,center)", MGED_DISPLAY_VAR, bu_vls_cstr(pn));
	bu_vls_printf(&p->dm_size_name, "%s(%s,size)", MGED_DISPLAY_VAR, bu_vls_cstr(pn));
	bu_vls_printf(&p->dm_adc_name, "%s(%s,adc)", MGED_DISPLAY_VAR,	bu_vls_cstr(pn));
    }
}


int
f_get_dm_list(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    if (argc != 1 || !argv) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel get_dm_list");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *dlp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	struct bu_vls *pn = dm_get_pathname(dlp->dm_dmp);
	if (pn && bu_vls_strlen(pn))
	    Tcl_AppendElement(interpreter, bu_vls_cstr(pn));
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
