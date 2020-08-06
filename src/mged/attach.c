/*                        A T T A C H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2020 United States Government as represented by
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
#include "ged.h"
#include "tclcad.h"

#include "./mged.h"
#include "./titles.h"
#include "./sedit.h"
#include "./mged_dm.h"

#define NEED_GUI(_name) (\
	BU_STR_EQUIV(_name, "wgl") || \
	BU_STR_EQUIV(_name, "ogl") || \
	BU_STR_EQUIV(_name, "osgl") || \
	BU_STR_EQUIV(_name, "tk") || \
	BU_STR_EQUIV(_name, "X") || \
	BU_STR_EQUIV(_name, "txt") || \
	BU_STR_EQUIV(_name, "qt"))

extern void share_dlist(struct dm_list *dlp2);	/* defined in share.c */

extern struct _color_scheme default_color_scheme;

int mged_default_dlist = 0;   /* This variable is available via Tcl for controlling use of display lists */
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list = (struct dm_list *)NULL;
static fastf_t windowbounds[6] = { XMIN, XMAX, YMIN, YMAX, (int)GED_MIN, (int)GED_MAX };

/* If we changed the active dm, need to update GEDP as well.. */
void set_curr_dm(struct dm_list *nl)
{
    curr_dm_list = nl;
    if (nl != DM_LIST_NULL) {
	GEDP->ged_gvp = nl->dml_view_state->vs_gvp;
	GEDP->ged_gvp->gv_grid = *nl->dml_grid_state; /* struct copy */
    } else {
	if (GEDP) {
	    GEDP->ged_gvp = NULL;
	}
    }
}

int
mged_dm_init(struct dm_list *o_dm_list,
	const char *dm_type,
	int argc,
	const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = dm_commands;

#ifdef HAVE_TK
    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
#endif

    if ((DMP = dm_open((void *)INTERP, dm_type, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Ogl's private structure */
    dm_set_vp(DMP, &view_state->vs_gvp->gv_scale);
    dm_set_perspective(DMP, mged_variables->mv_perspective_mode);

#ifdef HAVE_TK
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
#endif
    (void)dm_configure_win(DMP, 0);

    if (dm_get_pathname(DMP)) {
	bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(dm_get_pathname(DMP)));
	Tcl_Eval(INTERP, bu_vls_addr(&vls));
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
mged_slider_init_vls(struct dm_list *p)
{
    bu_vls_init(&p->dml_fps_name);
    bu_vls_init(&p->dml_aet_name);
    bu_vls_init(&p->dml_ang_name);
    bu_vls_init(&p->dml_center_name);
    bu_vls_init(&p->dml_size_name);
    bu_vls_init(&p->dml_adc_name);
}


void
mged_slider_free_vls(struct dm_list *p)
{
    if (BU_VLS_IS_INITIALIZED(&p->dml_fps_name)) {
	bu_vls_free(&p->dml_fps_name);
	bu_vls_free(&p->dml_aet_name);
	bu_vls_free(&p->dml_ang_name);
	bu_vls_free(&p->dml_center_name);
	bu_vls_free(&p->dml_size_name);
	bu_vls_free(&p->dml_adc_name);
    }
}


int
release(char *name, int need_close)
{
    struct dm_list *save_dm_list = DM_LIST_NULL;

    if (name != NULL) {
	struct dm_list *p;

	if (BU_STR_EQUAL("nu", name))
	    return TCL_OK;  /* Ignore */

	FOR_ALL_DISPLAYS(p, &head_dm_list.l) {
	    if (dm_get_pathname(p->dml_dmp) && !BU_STR_EQUAL(name, bu_vls_addr(dm_get_pathname(p->dml_dmp))))
		continue;

	    /* found it */
	    if (p != curr_dm_list) {
		save_dm_list = curr_dm_list;
		set_curr_dm(p);
	    }
	    break;
	}

	if (p == &head_dm_list) {
	    Tcl_AppendResult(INTERP, "release: ", name,
			     " not found\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else if (DMP && dm_get_pathname(DMP) && BU_STR_EQUAL("nu", bu_vls_addr(dm_get_pathname(DMP))))
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
     * like the last one the user had open. This depends on "nu"
     * always being last in the list.
     */
    usurp_all_resources(BU_LIST_LAST(dm_list, &head_dm_list.l), curr_dm_list);

    /* If this display is being referenced by a command window, then
     * remove the reference.
     */
    if (curr_dm_list->dml_tie != NULL)
	curr_dm_list->dml_tie->cl_tie = (struct dm_list *)NULL;

    if (need_close)
	dm_close(DMP);

    RT_FREE_VLIST(&curr_dm_list->dml_p_vlist);
    BU_LIST_DEQUEUE(&curr_dm_list->l);
    mged_slider_free_vls(curr_dm_list);
    bu_free((void *)curr_dm_list, "release: curr_dm_list");

    if (save_dm_list != DM_LIST_NULL)
	set_curr_dm(save_dm_list);
    else
	set_curr_dm((struct dm_list *)head_dm_list.l.forw);

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
    struct dm_list *o_dm_list;

    if (!wp_name) {
	return TCL_ERROR;
    }

    o_dm_list = curr_dm_list;
    BU_ALLOC(curr_dm_list, struct dm_list);

    /* initialize predictor stuff */
    BU_LIST_INIT(&curr_dm_list->dml_p_vlist);
    predictor_init();

    /* Only need to do this once */
    if (tkwin == NULL && NEED_GUI(wp_name)) {
	struct dm *tmp_dmp;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	/* look for "-d display_string" and use it if provided */
	tmp_dmp = dm_get();

	opt_argc = argc - 1;
	opt_argv = bu_argv_dup(opt_argc, argv + 1);
	dm_processOptions(tmp_dmp, &tmp_vls, opt_argc, (const char **)opt_argv);
	bu_argv_free(opt_argc, opt_argv);

	if (dm_get_dname(tmp_dmp) && strlen(bu_vls_addr(dm_get_dname(tmp_dmp)))) {
	    if (gui_setup(bu_vls_addr(dm_get_dname(tmp_dmp))) == TCL_ERROR) {
		bu_free((void *)curr_dm_list, "f_attach: dm_list");
		set_curr_dm(o_dm_list);
		bu_vls_free(&tmp_vls);
		dm_put(tmp_dmp);
		return TCL_ERROR;
	    }
	} else if (gui_setup((char *)NULL) == TCL_ERROR) {
	    bu_free((void *)curr_dm_list, "f_attach: dm_list");
	    set_curr_dm(o_dm_list);
	    bu_vls_free(&tmp_vls);
	    dm_put(tmp_dmp);
	    return TCL_ERROR;
	}

	bu_vls_free(&tmp_vls);
	dm_put(tmp_dmp);
    }

    BU_LIST_APPEND(&head_dm_list.l, &curr_dm_list->l);

    if (!wp_name) {
	return TCL_ERROR;
    }

    if (mged_dm_init(o_dm_list, wp_name, argc, argv) == TCL_ERROR) {
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

    mged_link_vars(curr_dm_list);

    Tcl_ResetResult(INTERP);
    if (dm_get_dm_name(DMP) && dm_get_dm_lname(DMP)) {
	Tcl_AppendResult(INTERP, "ATTACHING ", dm_get_dm_name(DMP), " (", dm_get_dm_lname(DMP),
		")\n", (char *)NULL);
    }

    share_dlist(curr_dm_list);

    if (dm_get_displaylist(DMP) && mged_variables->mv_dlist && !dlist_state->dl_active) {
	createDLists(GEDP->ged_gdp->gd_headDisplay);
	dlist_state->dl_active = 1;
    }

    (void)dm_make_current(DMP);
    (void)dm_set_win_bounds(DMP, windowbounds);
    mged_fb_open();

    GEDP->ged_gvp = curr_dm_list->dml_view_state->vs_gvp;
    GEDP->ged_gvp->gv_grid = *curr_dm_list->dml_grid_state; /* struct copy */

    return TCL_OK;

 Bad:
    Tcl_AppendResult(INTERP, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);

    if (DMP != (struct dm *)0)
	release((char *)NULL, 1);  /* release() will call dm_close */
    else
	release((char *)NULL, 0);  /* release() will not call dm_close */

    return TCL_ERROR;
}


void
get_attached(void)
{
    int inflimit = 1000;
    int ret;
    struct bu_vls type = BU_VLS_INIT_ZERO;

    struct bu_vls type_msg = BU_VLS_INIT_ZERO;
    struct bu_vls dm_types = BU_VLS_INIT_ZERO;
    dm_list_types(&dm_types, " ");
    char **dms = (char **)bu_calloc(bu_vls_strlen(&dm_types), sizeof(char *), "dm name array");
    int nargc = bu_argv_from_string(dms, bu_vls_strlen(&dm_types), bu_vls_addr(&dm_types));

    bu_vls_sprintf(&type_msg, "attach (nu");
    for (int i = 0; i < nargc; i++) {
	if (BU_STR_EQUAL(dms[i], "nu"))
	    continue;
	if (BU_STR_EQUAL(dms[i], "plot"))
	    continue;
	if (BU_STR_EQUAL(dms[i], "postscript"))
	    continue;
	bu_vls_printf(&type_msg, " %s", dms[i]);
    }
    bu_vls_printf(&type_msg, ")[nu]? ");
    bu_free(dms, "array");
    bu_vls_free(&dm_types);

    while (inflimit > 0) {
	bu_log("%s", bu_vls_cstr(&type_msg));

	ret = bu_vls_gets(&type, stdin);
	if (ret < 0) {
	    /* handle EOF */
	    bu_log("\n");
	    bu_vls_free(&type);
	    return;
	}

	if (bu_vls_strlen(&type) == 0 || BU_STR_EQUAL(bu_vls_addr(&type), "nu")) {
	    /* Nothing more to do. */
	    bu_vls_free(&type);
	    return;
	}

	/* trim whitespace before comparisons (but not before checking empty) */
	bu_vls_trimspace(&type);

	if (dm_valid_type(bu_vls_cstr(&type), NULL)) {
	    break;
	}

	/* Not a valid choice, loop. */
	inflimit--;
    }

    bu_vls_free(&type_msg);

    if (inflimit <= 0) {
	bu_log("\nInfinite loop protection, attach aborted!\n");
	bu_vls_free(&type);
	return;
    }

    bu_log("Starting an %s display manager\n", bu_vls_cstr(&type));

    int argc = 1;
    const char *argv[3];
    argv[0] = "";
    argv[1] = "";
    argv[2] = (char *)NULL;
    (void)mged_attach(bu_vls_cstr(&type), argc, argv);
    bu_vls_free(&type);
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

    if (!cmd_hook) {
	if (dm_get_dm_name(DMP)) {
	    Tcl_AppendResult(interpreter, "The '", dm_get_dm_name(DMP),
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
    return curr_dm_list == &head_dm_list;
}


void
dm_var_init(struct dm_list *initial_dm_list)
{
    BU_ALLOC(adc_state, struct _adc_state);
    *adc_state = *initial_dm_list->dml_adc_state;		/* struct copy */
    adc_state->adc_rc = 1;

    BU_ALLOC(menu_state, struct _menu_state);
    *menu_state = *initial_dm_list->dml_menu_state;		/* struct copy */
    menu_state->ms_rc = 1;

    BU_ALLOC(rubber_band, struct _rubber_band);
    *rubber_band = *initial_dm_list->dml_rubber_band;		/* struct copy */
    rubber_band->rb_rc = 1;

    BU_ALLOC(mged_variables, struct _mged_variables);
    *mged_variables = *initial_dm_list->dml_mged_variables;	/* struct copy */
    mged_variables->mv_rc = 1;
    mged_variables->mv_dlist = mged_default_dlist;
    mged_variables->mv_listen = 0;
    mged_variables->mv_port = 0;
    mged_variables->mv_fb = 0;

    BU_ALLOC(color_scheme, struct _color_scheme);

    /* initialize using the nu display manager */
    *color_scheme = *BU_LIST_LAST(dm_list, &head_dm_list.l)->dml_color_scheme;

    color_scheme->cs_rc = 1;

    BU_ALLOC(grid_state, struct bview_grid_state);
    *grid_state = *initial_dm_list->dml_grid_state;		/* struct copy */
    grid_state->rc = 1;

    BU_ALLOC(axes_state, struct _axes_state);
    *axes_state = *initial_dm_list->dml_axes_state;		/* struct copy */
    axes_state->ax_rc = 1;

    BU_ALLOC(dlist_state, struct _dlist_state);
    dlist_state->dl_rc = 1;

    BU_ALLOC(view_state, struct _view_state);
    *view_state = *initial_dm_list->dml_view_state;			/* struct copy */
    BU_ALLOC(view_state->vs_gvp, struct bview);
    BU_GET(view_state->vs_gvp->callbacks, struct bu_ptbl);
    bu_ptbl_init(view_state->vs_gvp->callbacks, 8, "bview callbacks");

    *view_state->vs_gvp = *initial_dm_list->dml_view_state->vs_gvp;	/* struct copy */
    view_state->vs_gvp->gv_clientData = (void *)view_state;
    view_state->vs_gvp->gv_adaptive_plot = 0;
    view_state->vs_gvp->gv_redraw_on_zoom = 0;
    view_state->vs_gvp->gv_point_scale = 1.0;
    view_state->vs_gvp->gv_curve_scale = 1.0;
    view_state->vs_rc = 1;
    view_ring_init(curr_dm_list->dml_view_state, (struct _view_state *)NULL);

    dirty = 1;
    mapped = 1;
    netfd = -1;
    owner = 1;
    am_mode = AMM_IDLE;
    adc_auto = 1;
    grid_auto_size = 1;
}


void
mged_link_vars(struct dm_list *p)
{
    mged_slider_init_vls(p);
    if (dm_get_pathname(p->dml_dmp)) {
	bu_vls_printf(&p->dml_fps_name, "%s(%s,fps)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
	bu_vls_printf(&p->dml_aet_name, "%s(%s,aet)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
	bu_vls_printf(&p->dml_ang_name, "%s(%s,ang)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
	bu_vls_printf(&p->dml_center_name, "%s(%s,center)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
	bu_vls_printf(&p->dml_size_name, "%s(%s,size)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
	bu_vls_printf(&p->dml_adc_name, "%s(%s,adc)", MGED_DISPLAY_VAR,
		bu_vls_addr(dm_get_pathname(p->dml_dmp)));
    }
}


int
f_get_dm_list(ClientData UNUSED(clientData), Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct dm_list *dlp;

    if (argc != 1 || !argv) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel get_dm_list");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    FOR_ALL_DISPLAYS(dlp, &head_dm_list.l) {
	if (dm_get_pathname(dlp->dml_dmp))
	    Tcl_AppendElement(interpreter, bu_vls_addr(dm_get_pathname(dlp->dml_dmp)));
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
