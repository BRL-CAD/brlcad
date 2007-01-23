/*                        A T T A C H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
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
/** @file attach.c
 *
 * Functions -
 *	f_refresh	request display refresh
 *	f_attach	attach display device
 *	attach		attach to a given display processor
 *	f_release	release display device
 *	release		guts for f_release
 *      gui_setup	called by loadtk to initialize the gui
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif

#include "tcl.h"
#include "tk.h"
#include "itk.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm-Null.h"

#include "./ged.h"
#include "./titles.h"
#include "./sedit.h"
#include "./mged_solid.h"
#include "./mged_dm.h"


#define NEED_GUI(_type) ( \
	IS_DM_TYPE_WGL(_type) || \
	IS_DM_TYPE_OGL(_type) || \
	IS_DM_TYPE_GLX(_type) || \
	IS_DM_TYPE_PEX(_type) || \
	IS_DM_TYPE_X(_type) )

/* All systems can compile these! */
extern int Plot_dm_init(struct dm_list *o_dm_list, int argc, char **argv);
extern int PS_dm_init(struct dm_list *o_dm_list, int argc, char **argv);

#ifdef DM_X
extern int X_dm_init();
extern void X_fb_open();
#endif /* DM_X */

#ifdef DM_WGL
extern int Wgl_dm_init();
extern void Wgl_fb_open();
#endif /* DM_WGL */

#ifdef DM_OGL
extern int Ogl_dm_init();
extern void Ogl_fb_open();
#endif /* DM_OGL */

#ifdef DM_GLX
extern int Glx_dm_init();
#endif /* DM_GLX */

#ifdef DM_PEX
extern int Pex_dm_init();
#endif /* DM_PEX */

extern void set_port(void);		/* defined in fbserv.c */

extern void share_dlist(struct dm_list *dlp2);	/* defined in share.c */
extern void predictor_init(void);	/* defined in predictor.c */
extern void view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2); /* defined in chgview.c */

extern void Tk_CreateCanvasBezierType();

extern struct _color_scheme default_color_scheme;

int gui_setup(char *dstr);
void get_attached(void);
void print_valid_dm(void);
void dm_var_init(struct dm_list *initial_dm_list);
void mged_slider_init_vls(struct dm_list *p);
void mged_slider_free_vls(struct dm_list *p);

void mged_fb_open(void);
void mged_fb_close(void);

int mged_default_dlist = 0;   /* This variable is available via Tcl for controlling use of display lists */
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list = (struct dm_list *)NULL;
static int windowbounds[6] = { XMIN, XMAX, YMIN, YMAX, (int)GED_MIN, (int)GED_MAX };

struct w_dm which_dm[] = {
  { DM_TYPE_PLOT, "plot", Plot_dm_init },  /* DM_PLOT_INDEX defined in mged_dm.h */
  { DM_TYPE_PS, "ps", PS_dm_init },      /* DM_PS_INDEX defined in mged_dm.h */
#ifdef DM_X
  { DM_TYPE_X, "X", X_dm_init },
#endif /* DM_X */
#ifdef DM_WGL
  { DM_TYPE_WGL, "wgl", Wgl_dm_init },
#endif /* DM_WGL */
#ifdef DM_OGL
  { DM_TYPE_OGL, "ogl", Ogl_dm_init },
#endif /* DM_OGL */
#ifdef DM_GLX
  { DM_TYPE_GLX, "glx", Glx_dm_init },
#endif /* DM_GLX */
#ifdef DM_PEX
  { DM_TYPE_PEX, "pex", Pex_dm_init },
#endif /* DM_PEX */
  { -1, (char *)NULL, (int (*)())NULL}
};


int
release(char *name, int need_close)
{
  struct dm_list *save_dm_list = DM_LIST_NULL;

  if(name != NULL){
    struct dm_list *p;

    if(!strcmp("nu", name))
      return TCL_OK;  /* Ignore */

    FOR_ALL_DISPLAYS(p, &head_dm_list.l){
      if(strcmp(name, bu_vls_addr(&p->dml_dmp->dm_pathName)))
	continue;

      /* found it */
      if(p != curr_dm_list){
	save_dm_list = curr_dm_list;
	curr_dm_list = p;
      }
      break;
    }

    if(p == &head_dm_list){
      Tcl_AppendResult(interp, "release: ", name,
		       " not found\n", (char *)NULL);
      return TCL_ERROR;
    }
  }else if(dmp && !strcmp("nu", bu_vls_addr(&pathName)))
      return TCL_OK;  /* Ignore */

  if(fbp){
    if(mged_variables->mv_listen){
      /* drop all clients */
      mged_variables->mv_listen = 0;
      set_port();
    }

    /* release framebuffer resources */
    mged_fb_close();
  }

  /*
   *  This saves the state of the resoures to the "nu" display manager, which
   *  is beneficial only if closing the last display manager. So when
   *  another display manager is opened, it looks like the last one
   *  the user had open. This depends on "nu" always being last in the list.
   */
  usurp_all_resources(BU_LIST_LAST(dm_list, &head_dm_list.l), curr_dm_list);

  /* If this display is being referenced by a command window,
     then remove the reference  */
  if(curr_dm_list->dml_tie != NULL)
    curr_dm_list->dml_tie->cl_tie = (struct dm_list *)NULL;

  if(need_close)
    DM_CLOSE(dmp);

  RT_FREE_VLIST(&curr_dm_list->dml_p_vlist);
  BU_LIST_DEQUEUE( &curr_dm_list->l );
  mged_slider_free_vls(curr_dm_list);
  bu_free( (genptr_t)curr_dm_list, "release: curr_dm_list" );

  if(save_dm_list != DM_LIST_NULL)
    curr_dm_list = save_dm_list;
  else
    curr_dm_list = (struct dm_list *)head_dm_list.l.forw;

  return TCL_OK;
}

int
f_release(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help release");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(argc == 2){
    int status;
    struct bu_vls vls1;

    bu_vls_init(&vls1);

    if(*argv[1] != '.')
      bu_vls_printf(&vls1, ".%s", argv[1]);
    else
      bu_vls_strcpy(&vls1, argv[1]);

    status = release(bu_vls_addr(&vls1), 1);

    bu_vls_free(&vls1);
    return status;
  }else
    return release((char *)NULL, 1);
}


int
f_attach(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  register struct w_dm *wp;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help attach");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    print_valid_dm();
    return TCL_ERROR;
  }

  /* Look at last argument, skipping over any options which preceed it */
  for( wp = &which_dm[2]; wp->type != -1; wp++ )
    if( strcmp(argv[argc - 1], wp->name ) == 0 )
      break;

  if(wp->type == -1){
    Tcl_AppendResult(interp, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);
    print_valid_dm();
    return TCL_ERROR;
  }

  return mged_attach(wp, argc, argv);
}

void
print_valid_dm(void)
{
    int i = 0;
    Tcl_AppendResult(interp, "\tThe following display manager types are valid: ", (char *)NULL);
#ifdef DM_X
    Tcl_AppendResult(interp, "X  ", (char *)NULL);
    i++;
#endif /* DM_X */
#ifdef DM_WGL
    Tcl_AppendResult(interp, "wgl  ", (char *)NULL);
    i++;
#endif /* DM_WGL */
#ifdef DM_OGL
    Tcl_AppendResult(interp, "ogl  ", (char *)NULL);
    i++;
#endif /* DM_OGL */
#ifdef DM_GLX
    Tcl_AppendResult(interp, "glx", (char *)NULL);
    i++;
#endif /* DM_GLX */
    if (i==0) {
	Tcl_AppendResult(interp, "NONE AVAILABLE", (char *)NULL);
    }
    Tcl_AppendResult(interp, "\n", (char *)NULL);
}

int
mged_attach(
	struct w_dm *wp,
	int argc,
	char *argv[])
{
  register struct dm_list *o_dm_list;

  if (!wp) {
      return TCL_ERROR;
  }

  o_dm_list = curr_dm_list;
  BU_GETSTRUCT(curr_dm_list, dm_list);

  /* initialize predictor stuff */
  BU_LIST_INIT(&curr_dm_list->dml_p_vlist);
  predictor_init();

  /* Only need to do this once */
  if(tkwin == NULL && NEED_GUI(wp->type)){
    struct dm *tmp_dmp;
    struct bu_vls tmp_vls;

    /* look for "-d display_string" and use it if provided */
    BU_GETSTRUCT(tmp_dmp, dm);
    bu_vls_init(&tmp_dmp->dm_pathName);
    bu_vls_init(&tmp_dmp->dm_dName);
    bu_vls_init(&tmp_vls);
    dm_processOptions(tmp_dmp, &tmp_vls, argc - 1, argv + 1);
    if(strlen(bu_vls_addr(&tmp_dmp->dm_dName))){
      if(gui_setup(bu_vls_addr(&tmp_dmp->dm_dName)) == TCL_ERROR){
	bu_free( (genptr_t)curr_dm_list, "f_attach: dm_list" );
	curr_dm_list = o_dm_list;
	bu_vls_free(&tmp_dmp->dm_pathName);
	bu_vls_free(&tmp_dmp->dm_dName);
	bu_vls_free(&tmp_vls);
	bu_free((genptr_t)tmp_dmp, "mged_attach: tmp_dmp");
	return TCL_ERROR;
      }
    } else if(gui_setup((char *)NULL) == TCL_ERROR){
      bu_free( (genptr_t)curr_dm_list, "f_attach: dm_list" );
      curr_dm_list = o_dm_list;
      bu_vls_free(&tmp_dmp->dm_pathName);
      bu_vls_free(&tmp_dmp->dm_dName);
      bu_vls_free(&tmp_vls);
      bu_free((genptr_t)tmp_dmp, "mged_attach: tmp_dmp");
      return TCL_ERROR;
    }

    bu_vls_free(&tmp_dmp->dm_pathName);
    bu_vls_free(&tmp_dmp->dm_dName);
    bu_vls_free(&tmp_vls);
    bu_free((genptr_t)tmp_dmp, "mged_attach: tmp_dmp");
  }

  BU_LIST_APPEND(&head_dm_list.l, &curr_dm_list->l);

  if (!wp->name || !wp->init) {
      return TCL_ERROR;
  }

  if(wp->init(o_dm_list, argc, argv) == TCL_ERROR) {
    goto Bad;
  }

  /* initialize the background color */
  cs_set_bg();

  mged_link_vars(curr_dm_list);

  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "ATTACHING ", dmp->dm_name, " (", dmp->dm_lname,
		   ")\n", (char *)NULL);

#ifdef DO_DISPLAY_LISTS
  share_dlist(curr_dm_list);

  if(displaylist && mged_variables->mv_dlist && !dlist_state->dl_active){
    createDLists(&dgop->dgo_headSolid);
    dlist_state->dl_active = 1;
  }
#endif

  DM_SET_WIN_BOUNDS(dmp, windowbounds);
  mged_fb_open();

  return TCL_OK;

Bad:
  Tcl_AppendResult(interp, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);

  if(dmp != (struct dm *)0)
    release((char *)NULL, 1);  /* release() will call dm_close */
  else
    release((char *)NULL, 0);  /* release() will not call dm_close */

  return TCL_ERROR;
}

void
get_attached(void)
{
  int argc;
  char *argv[3];
  char line[80] = {0};
  struct w_dm *wp = (struct w_dm *)NULL;
  int inflimit = 1000;
  char *ret = (char *)NULL;

  while(inflimit > 0){
    memset(line, 0, 80);

    bu_log("attach (nu");

    /* print all the available display manager types, skipping plot and ps */
    wp = &which_dm[2];
    for( ; wp->type != -1; wp++ ) {
	bu_log("|%s", wp->name);
    }
    bu_log(")[nu]? ");

    ret = fgets(line, sizeof(line), stdin); /* \n, Null terminated */

    /* handle EOF */
    if (!ret) {
	bu_log("\n");
	return;
    }

    if(line[0] == '\n' || strncmp(line, "nu", 2) == 0) {
	return;  /* Nothing more to do. */
    }

    line[strlen(line)-1] = '\0';        /* remove newline */

    for( wp = &which_dm[2]; wp->type != -1; wp++ ) {
	if( strcmp( line, wp->name ) == 0 ) {
	    break;
	}
    }

    if( wp->type != -1 ) {
	break;
    }

    /* Not a valid choice, loop. */
    inflimit--;
  }

  if (inflimit <= 0) {
      bu_log("\nInfinite Loop protection, attach aborted!\n");
      return;
  }

  argc = 2;
  argv[0] = "";
  argv[1] = "";
  argv[2] = (char *)NULL;
  (void)mged_attach(wp, argc, argv);
}


int
gui_setup(char *dstr)
{
  struct bu_vls vls;

  /* initialize only once */
  if(tkwin != NULL)
    return TCL_OK;

  bu_vls_init(&vls);
  Tcl_ResetResult(interp);

  if(dstr != (char *)NULL){
    bu_vls_strcpy(&vls, "env(DISPLAY)");
    Tcl_SetVar(interp, bu_vls_addr(&vls), dstr, TCL_GLOBAL_ONLY);
  }

  /* This runs the tk.tcl script */
  if(Tk_Init(interp) == TCL_ERROR){
      /* hack to avoid a stupid Tk error */
      if (strncmp(interp->result, "this isn't a Tk applicationcouldn't", 35) == 0) {
	  interp->result = (interp->result + 27);
      }
      bu_vls_free(&vls);
      return TCL_ERROR;
  }

  /* Add Bezier Curves to the canvas widget */
  Tk_CreateCanvasBezierType();

  /* Initialize [incr Tk] */
  if (Itk_Init(interp) == TCL_ERROR) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Import [incr Tk] commands into the global namespace */
  if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		 "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Initialize the Iwidgets package */
  if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Import iwidgets into the global namespace */
  if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		 "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

#ifdef BRLCAD_DEBUG
  /* Initialize libdm */
  (void)Dm_d_Init(interp);

  /* Initialize libfb */
  (void)Fb_d_Init(interp);
#else
  /* Initialize libdm */
  (void)Dm_Init(interp);

  /* Initialize libfb */
  (void)Fb_Init(interp);
#endif

  if((tkwin = Tk_MainWindow(interp)) == NULL){
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* create the event handler */
  Tk_CreateGenericHandler(doEvent, (ClientData)NULL);

  bu_vls_strcpy(&vls, "wm withdraw . ; tk appname mged");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_OK;
}


/*
 *			F _ D M
 *
 *  Run a display manager specific command(s).
 */
int
f_dm(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  if( !cmd_hook ){
    Tcl_AppendResult(interp, "The '", dmp->dm_name,
		     "' display manager does not support local commands.\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help dm");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  return cmd_hook( argc-1, argv+1 );
}

/*
 *			 I S _ D M _ N U L L
 *
 *  Returns -
 *	 0	If the display manager goes to a real screen.
 *	!0	If the null display manager is attached.
 */
int
is_dm_null(void)
{
  return(curr_dm_list == &head_dm_list);
}

void
dm_var_init(struct dm_list *initial_dm_list)
{
  BU_GETSTRUCT(adc_state, _adc_state);
  *adc_state = *initial_dm_list->dml_adc_state;			/* struct copy */
  adc_state->adc_rc = 1;
#if 0
  adc_state->adc_a1 = adc_state->adc_a2 = 45.0;
#endif

  BU_GETSTRUCT(menu_state, _menu_state);
  *menu_state = *initial_dm_list->dml_menu_state;		/* struct copy */
  menu_state->ms_rc = 1;

  BU_GETSTRUCT(rubber_band, _rubber_band);
  *rubber_band = *initial_dm_list->dml_rubber_band;		/* struct copy */
  rubber_band->rb_rc = 1;

  BU_GETSTRUCT(mged_variables, _mged_variables);
  *mged_variables = *initial_dm_list->dml_mged_variables;	/* struct copy */
  mged_variables->mv_rc = 1;
  mged_variables->mv_dlist = mged_default_dlist;
  mged_variables->mv_listen = 0;
  mged_variables->mv_port = 0;
  mged_variables->mv_fb = 0;

  BU_GETSTRUCT(color_scheme, _color_scheme);
#if 0
  /* initialize using the last curr_dm_list */
  *color_scheme = *initial_dm_list->dml_color_scheme;		/* struct copy */
#else
  /* initialize using the nu display manager */
  *color_scheme = *BU_LIST_LAST(dm_list, &head_dm_list.l)->dml_color_scheme;
#endif
  color_scheme->cs_rc = 1;

  BU_GETSTRUCT(grid_state, _grid_state);
  *grid_state = *initial_dm_list->dml_grid_state;		/* struct copy */
  grid_state->gr_rc = 1;

  BU_GETSTRUCT(axes_state, _axes_state);
  *axes_state = *initial_dm_list->dml_axes_state;		/* struct copy */
  axes_state->ax_rc = 1;

  BU_GETSTRUCT(dlist_state, _dlist_state);
  dlist_state->dl_rc = 1;

  BU_GETSTRUCT(view_state, _view_state);
  *view_state = *initial_dm_list->dml_view_state;			/* struct copy */
  view_state->vs_vop = vo_open_cmd("");
  *view_state->vs_vop = *initial_dm_list->dml_view_state->vs_vop;	/* struct copy */
  view_state->vs_vop->vo_clientData = view_state;
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

void
mged_link_vars(struct dm_list *p)
{
  mged_slider_init_vls(p);

  bu_vls_printf(&p->dml_fps_name, "%s(%S,fps)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
  bu_vls_printf(&p->dml_aet_name, "%s(%S,aet)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
  bu_vls_printf(&p->dml_ang_name, "%s(%S,ang)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
  bu_vls_printf(&p->dml_center_name, "%s(%S,center)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
  bu_vls_printf(&p->dml_size_name, "%s(%S,size)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
  bu_vls_printf(&p->dml_adc_name, "%s(%S,adc)", MGED_DISPLAY_VAR,
		&p->dml_dmp->dm_pathName);
}

int
f_get_dm_list(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct dm_list *dlp;

  if(argc != 1){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel get_dm_list");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
    Tcl_AppendElement(interp, bu_vls_addr(&dlp->dml_dmp->dm_pathName));

  return TCL_OK;
}

void
mged_fb_open(void)
{
#ifdef DM_X
  if(dmp->dm_type == DM_TYPE_X)
    X_fb_open();
#endif /* DM_X */
#ifdef DM_WGL
  if(dmp->dm_type == DM_TYPE_WGL)
      Wgl_fb_open();
#endif /* DM_WGL */
#ifdef DM_OGL
  if(dmp->dm_type == DM_TYPE_OGL)
      Ogl_fb_open();
#endif /* DM_OGL */
}

void
mged_fb_close(void)
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "fb_close_existing %lu", fbp);
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  fbp = (FBIO *)0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
