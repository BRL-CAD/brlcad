/*
 *			A T T A C H . C
 *
 * Functions -
 *	f_refresh	request display refresh
 *	f_attach	attach display device
 *	attach		attach to a given display processor
 *	f_release	release display device
 *	release		guts for f_release
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "tk.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm-Null.h"
#include "./ged.h"
#include "./titles.h"
#include "./sedit.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

int gui_setup();
int mged_attach();
void get_attached();
void print_valid_dm();
void dm_var_init();
void mged_slider_init_vls();
void mged_slider_free_vls();
void mged_slider_link_vars();
void mged_slider_unlink_vars();
static int do_2nd_attach_prompt();
static void find_new_owner();

extern void predictor_init(); /* defined in predictor.c */
extern void set_scroll();  /* defined in set.c */
extern void color_soltab();
extern int mged_view_init(); /* defined in chgview.c */

/* All systems can compile these! */
extern int Plot_dm_init();
extern int PS_dm_init();
#ifdef DM_X
extern int X_dm_init();
#endif
#ifdef DM_OGL
extern int Ogl_dm_init();
#endif
#ifdef DM_GLX
extern int Glx_dm_init();
#endif
#ifdef DM_PEX
extern int Pex_dm_init();
#endif

#define NEED_GUI(_type) ( \
	IS_DM_TYPE_OGL(_type) || \
	IS_DM_TYPE_GLX(_type) || \
	IS_DM_TYPE_PEX(_type) || \
	IS_DM_TYPE_X(_type) )

extern Tk_Window tkwin;
extern struct _mged_variables default_mged_variables;
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list;
char tmp_str[1024];
static int windowbounds[6] = { XMAX, XMIN, YMAX, YMIN, 2047, -2048 };

static char *default_view_strings[] = {
  "top",
  "right",
  "front",
  "45,45",
  "35,25"
};

struct w_dm which_dm[] = {
  { DM_TYPE_PLOT, "plot", Plot_dm_init },  /* DM_PLOT_INDEX defined in mged_dm.h */
  { DM_TYPE_PS, "ps", PS_dm_init },      /* DM_PS_INDEX defined in mged_dm.h */
#ifdef DM_X
  { DM_TYPE_X, "X", X_dm_init },
#endif
#ifdef DM_OGL
  { DM_TYPE_OGL, "ogl", Ogl_dm_init },
#endif
#ifdef DM_GLX
  { DM_TYPE_GLX, "glx", Glx_dm_init },
#endif
#ifdef DM_PEX
  { DM_TYPE_PEX, "pex", Pex_dm_init },
#endif
  { -1, (char *)NULL, (int (*)())NULL}
};


int
release(name, need_close)
char *name;
int need_close;
{
  register struct solid *sp;
  struct dm_list *p;
  struct cmd_list *p_cmd;
  struct dm_list *save_dm_list = DM_LIST_NULL;

  if(name != NULL){
    if(!strcmp("nu", name))
      return TCL_OK;  /* Ignore */

    FOR_ALL_DISPLAYS(p, &head_dm_list.l){
      if(strcmp(name, bu_vls_addr(&p->_dmp->dm_pathName)))
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
  }else{
    if(dmp && !strcmp("nu", bu_vls_addr(&pathName)))
      return TCL_OK;  /* Ignore */
    else
      p = curr_dm_list;
  }

#if 1
  {
    struct dm_list *sdm;

    sdm = curr_dm_list;

    curr_dm_list = BU_LIST_LAST(dm_list, &head_dm_list.l);
    bn_mat_copy(Viewrot, p->s_info->_Viewrot);
    bn_mat_copy(toViewcenter, p->s_info->_toViewcenter);
    Viewscale = p->s_info->_Viewscale;
    new_mats();

    curr_dm_list = sdm;
  }
#endif

  if(!--p->s_info->_rc){
    if(rate_tran_vls[X].vls_magic == BU_VLS_MAGIC){
      mged_slider_unlink_vars(p);
      mged_slider_free_vls(p);
    }

    /* free view ring */
    if(p->s_info->_headView.l.magic == BU_LIST_HEAD_MAGIC){
      struct view_list *vlp;
      struct view_list *nvlp;

      for(vlp = BU_LIST_FIRST(view_list, &p->s_info->_headView.l);
	  BU_LIST_NOT_HEAD(vlp, &p->s_info->_headView.l);){
	nvlp = BU_LIST_PNEXT(view_list, vlp);
	BU_LIST_DEQUEUE(&vlp->l);
	bu_free((genptr_t)vlp, "release: vlp");
	vlp = nvlp;
      }
    }

    bu_free( (genptr_t)p->s_info, "release: s_info" );
  }else if(p->_owner)
    find_new_owner(p);

  /* If this display is being referenced by a command window,
     then remove the reference  */
  if(p->aim != NULL)
    p->aim->aim = (struct dm_list *)NULL;

  if(need_close){
#ifdef DO_DISPLAY_LISTS
    if(displaylist &&
       mged_variables->dlist &&
       BU_LIST_NON_EMPTY(&HeadSolid.l))
#ifdef DO_SINGLE_DISPLAY_LIST
    dmp->dm_freeDLists(dmp, dmp->dm_displaylist + 1, 1);
#else
    dmp->dm_freeDLists(dmp, BU_LIST_FIRST(solid, &HeadSolid.l)->s_dlist +
		       dmp->dm_displaylist,
		       BU_LIST_LAST(solid, &HeadSolid.l)->s_dlist -
		       BU_LIST_FIRST(solid, &HeadSolid.l)->s_dlist + 1);
#endif
#endif
    dmp->dm_close(dmp);
  }

  RT_FREE_VLIST(&p->p_vlist);
  BU_LIST_DEQUEUE( &p->l );
  if(p->_scroll_edit_vls.vls_magic == BU_VLS_MAGIC){
    Tcl_UnlinkVar(interp, bu_vls_addr(&p->_scroll_edit_vls));
    bu_vls_free(&p->_scroll_edit_vls);
  }

  bu_free( (genptr_t)p, "release: curr_dm_list" );

  if(save_dm_list != DM_LIST_NULL)
    curr_dm_list = save_dm_list;
  else
    curr_dm_list = (struct dm_list *)head_dm_list.l.forw;

  return TCL_OK;
}


int
f_release(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
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

static int
do_2nd_attach_prompt()
{
  char *dm_default;
  char  hostname[80];
  char  display[82];
  struct bu_vls prompt;


  bu_vls_init(&prompt);

  /* get or create the default display */
  if( (dm_default = getenv("DISPLAY")) == NULL ) {
    /* Env not set, use local host */
    gethostname( hostname, 80 );
    hostname[79] = '\0';
    (void)sprintf( display, "%s:0", hostname );
    dm_default = display;
  }

  bu_vls_printf(&prompt, "Display [%s]? ", dm_default);

  Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&prompt), (char *)NULL);
  bu_vls_printf(&curr_cmd_list->more_default, "%s", dm_default);

  return TCL_ERROR;
}

int
f_attach(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  register struct w_dm *wp;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help attach");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    print_valid_dm();
    return TCL_ERROR;
  }

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
print_valid_dm()
{
    Tcl_AppendResult(interp, "\tThe following display manager types are valid: ", (char *)NULL);
#ifdef DM_X
    Tcl_AppendResult(interp, "X  ", (char *)NULL);
#endif
#ifdef DM_OGL
    Tcl_AppendResult(interp, "ogl  ", (char *)NULL);
#endif
#ifdef DM_GLX
    Tcl_AppendResult(interp, "glx", (char *)NULL);
#endif
    Tcl_AppendResult(interp, "\n", (char *)NULL);
}

int
mged_attach(wp, argc, argv)
register struct w_dm *wp;
int argc;
char *argv[];
{
  register struct solid *sp;
  register struct dm_list *o_dm_list;

  o_dm_list = curr_dm_list;
  BU_GETSTRUCT(curr_dm_list, dm_list);

  /* initialize predictor stuff */
  BU_LIST_INIT(&curr_dm_list->p_vlist);
  predictor_init();

  BU_LIST_APPEND(&head_dm_list.l, &curr_dm_list->l);
  /* Only need to do this once */
  if(tkwin == NULL && NEED_GUI(wp->type)){
    struct dm *tmp_dmp;
    struct bu_vls tmp_vls;

    /* look for "-d display_string" and use it if provided */
    BU_GETSTRUCT(tmp_dmp, dm);
    bu_vls_init(&tmp_dmp->dm_pathName);
    bu_vls_init(&tmp_dmp->dm_dName);
    bu_vls_init(&tmp_vls);
    dm_process_options(tmp_dmp, &tmp_vls, argc - 1, argv + 1);
    if(strlen(bu_vls_addr(&tmp_dmp->dm_dName))){
      if(gui_setup(bu_vls_addr(&tmp_dmp->dm_dName)) == TCL_ERROR){
	BU_LIST_DEQUEUE( &curr_dm_list->l );
	bu_free( (genptr_t)curr_dm_list, "f_attach: dm_list" );
	curr_dm_list = o_dm_list;
	bu_vls_free(&tmp_dmp->dm_pathName);
	bu_vls_free(&tmp_dmp->dm_dName);
	bu_vls_free(&tmp_vls);
	bu_free((genptr_t)tmp_dmp, "mged_attach: tmp_dmp");
	return TCL_ERROR;
      }
    } else if(gui_setup((char *)NULL) == TCL_ERROR){
      BU_LIST_DEQUEUE( &curr_dm_list->l );
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

  if(wp->init(o_dm_list, argc, argv) == TCL_ERROR)
    goto Bad;

#if TRY_NEW_MGED_VARS
  mged_variable_setup(curr_dm_list);
#endif

  bu_vls_init(&fps_name);
  bu_vls_printf(&fps_name, "mged_display(%S,fps)",
		&curr_dm_list->_dmp->dm_pathName);
  bu_vls_init(&curr_dm_list->_scroll_edit_vls);
  bu_vls_printf(&curr_dm_list->_scroll_edit_vls, "scroll_edit(%S)",
		&curr_dm_list->_dmp->dm_pathName);
  Tcl_LinkVar(interp, bu_vls_addr(&curr_dm_list->_scroll_edit_vls),
	      (char *)&curr_dm_list->_scroll_edit, TCL_LINK_INT);
  mged_slider_link_vars(curr_dm_list);
  mmenu_init();
  btn_head_menu(0,0,0);

  if(state == ST_S_EDIT)
    sedit_menu();
  else if(state == ST_O_EDIT)
    chg_l2menu(ST_O_EDIT);

  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "ATTACHING ", dmp->dm_name, " (", dmp->dm_lname,
		   ")\n", (char *)NULL);

#ifdef DO_DISPLAY_LISTS
  if(displaylist && mged_variables->dlist)
#ifdef DO_SINGLE_DISPLAY_LIST
    createDList(&HeadSolid);
#else
    createDLists(&HeadSolid); 
#endif
#endif

  color_soltab();
  ++dirty;
  dmp->dm_setWinBounds(dmp, windowbounds);

  return TCL_OK;

Bad:
  Tcl_AppendResult(interp, "attach(", argv[argc - 1], "): BAD\n", (char *)NULL);

  if(dmp != (struct dm *)0)
    release((char *)NULL, 1);  /* relesae() will call dm_close */
  else
    release((char *)NULL, 0);  /* release() will not call dm_close */

  return TCL_ERROR;
}

void
get_attached()
{
  int argc;
  char *argv[3];
  char line[80];
  register struct w_dm *wp;
  struct bu_vls vls;

  while(1){
    bu_log("attach (nu");
    /* skip plot and ps */
    wp = &which_dm[2];
    for( ; wp->type != -1; wp++ )
      bu_log("|%s", wp->name);
    bu_log(")[nu]? ");
    (void)fgets(line, sizeof(line), stdin); /* \n, Null terminated */

    if(line[0] == '\n' || strncmp(line, "nu", 2) == 0)
      return;  /* Nothing more to do. */

    line[strlen(line)-1] = '\0';        /* remove newline */

    for( wp = &which_dm[2]; wp->type != -1; wp++ )
      if( strcmp( line, wp->name ) == 0 )
	break;

    if( wp->type != -1 )
      break;

    /* Not a valid choice, loop. */
  }

#if 0
  bu_vls_init(&vls);
  bu_vls_printf(&vls, "openw -s -gt %s\n", line);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
#else
  argc = 2;
  argv[0] = "";
  argv[1] = "";
  argv[2] = (char *)NULL;
  (void)mged_attach(wp, argc, argv);
#endif
}


int
gui_setup(dstr)
char *dstr;
{
  char *filename;
  int status;
  struct bu_vls vls;

  /* initialize only once */
  if(tkwin != NULL)
    return TCL_OK;

  bu_vls_init(&vls);

  if(dstr != (char *)NULL){
    bu_vls_strcpy(&vls, "env(DISPLAY)");
    Tcl_SetVar(interp, bu_vls_addr(&vls), dstr, TCL_GLOBAL_ONLY);
  }

  /* This runs the tk.tcl script */
  if(Tk_Init(interp) == TCL_ERROR){
    Tcl_AppendResult(interp, "\ngui_setup: Couldn't initialize Tk\n", (char *)NULL);
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if((tkwin = Tk_MainWindow(interp)) == NULL){
    Tcl_AppendResult(interp, "gui_setup: Failed to get main window.\n", (char *)NULL);
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
f_dm(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if( !cmd_hook ){
    Tcl_AppendResult(interp, "The '", dmp->dm_name,
		     "' display manager does not support any local commands.\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(argc < 2 || MAXARGS < argc){
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
is_dm_null()
{
  return(curr_dm_list == &head_dm_list);
}


int
f_unshare_view(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_list *p;
  struct shared_info *sip;
  struct dm_list *save_cdlp;
  struct cmd_list *save_cclp;
  struct bu_vls vls1;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help unshare_view");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  bu_vls_init(&vls1);

  if(*argv[1] != '.')
    bu_vls_printf(&vls1, ".%s", argv[1]);
  else
    bu_vls_strcpy(&vls1, argv[1]);

  FOR_ALL_DISPLAYS(p, &head_dm_list.l)
    if(!strcmp(bu_vls_addr(&vls1), bu_vls_addr(&p->_dmp->dm_pathName)))
      break;

  if(p == &head_dm_list){
    Tcl_AppendResult(interp, "unshare_view: bad pathname - %s\n",
		     bu_vls_addr(&vls1), (char *)NULL);
    bu_vls_free(&vls1);
    return TCL_ERROR;
  }

  bu_vls_free(&vls1);

  if(p->_owner){
    if(p->s_info->_rc > 1){  /* sharing s_info with another display manager */
      struct dm_list *nop;

      --p->s_info->_rc;
      sip = p->s_info;
      find_new_owner(p);
    }else
      return TCL_OK;  /* Nothing to do */
  }else{
    --p->s_info->_rc;
    sip = p->s_info;
  }

  BU_GETSTRUCT(p->s_info, shared_info);
  bcopy((void *)sip, (void *)p->s_info, sizeof(struct shared_info));
  p->s_info->_rc = 1;
  p->_owner = 1;
  p->_dmp->dm_vp = &p->s_info->_Viewscale;
  p->s_info->opp = &p->_dmp->dm_pathName;
  mged_slider_link_vars(p);
  mged_view_init(p);

  save_cdlp = curr_dm_list;
  curr_dm_list = p;
  if(p->aim){
    save_cclp = curr_cmd_list;
    curr_cmd_list = p->aim;
  }
  set_scroll();
  curr_dm_list = save_cdlp;
  if(p->aim)
    curr_cmd_list = save_cclp;
  
  return TCL_OK;
}

int
f_share_view(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_list *p;
  struct dm_list *p1 = (struct dm_list *)NULL;
  struct dm_list *p2 = (struct dm_list *)NULL;
  struct cmd_list *save_cclp;  /* save current cmd_list pointer */
  struct bu_vls vls1, vls2;

  if(argc < 3 || 3 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help share_view");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init(&vls1);
  bu_vls_init(&vls2);

  if(*argv[1] != '.')
    bu_vls_printf(&vls1, ".%s", argv[1]);
  else
    bu_vls_strcpy(&vls1, argv[1]);

  if(*argv[2] != '.')
    bu_vls_printf(&vls2, ".%s", argv[2]);
  else
    bu_vls_strcpy(&vls2, argv[2]);

  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    if(p1 == (struct dm_list *)NULL && !strcmp(bu_vls_addr(&vls1),
					       bu_vls_addr(&p->_dmp->dm_pathName)))
      p1 = p;
    else if(p2 == (struct dm_list *)NULL && !strcmp(bu_vls_addr(&vls2),
						    bu_vls_addr(&p->_dmp->dm_pathName)))
      p2 = p;
    else if(p1 != (struct dm_list *)NULL && p2 != (struct dm_list *)NULL)
      break;
  }

  if(p1 == (struct dm_list *)NULL || p2 == (struct dm_list *)NULL){
    Tcl_AppendResult(interp, "f_share_view: bad pathname(s)\n\tpathName1 - ",
		     bu_vls_addr(&vls1), "\t\tpathName2 - ",
		     bu_vls_addr(&vls2), "\n", (char *)NULL);
    bu_vls_free(&vls1);
    bu_vls_free(&vls2);
    return TCL_ERROR;
  }

  bu_vls_free(&vls1);
  bu_vls_free(&vls2);

  /* free p2's s_info struct if not being used */
  if(!--p2->s_info->_rc){
    mged_slider_unlink_vars(p2);
    mged_slider_free_vls(p2);

    /* free view ring */
    if(p2->s_info->_headView.l.magic == BU_LIST_HEAD_MAGIC){
      struct view_list *vlp;
      struct view_list *nvlp;

      for(vlp = BU_LIST_FIRST(view_list, &p2->s_info->_headView.l);
	  BU_LIST_NOT_HEAD(vlp, &p2->s_info->_headView.l);){
	nvlp = BU_LIST_PNEXT(view_list, vlp);
	BU_LIST_DEQUEUE(&vlp->l);
	bu_free((genptr_t)vlp, "release: vlp");
	vlp = nvlp;
      }
    }

    bu_free( (genptr_t)p2->s_info, "share_view: s_info" );
  /* otherwise if p2's s_info struct is being used and p2 is the owner */
  }else if(p2->_owner)
    find_new_owner(p2);

  p2->_owner = 0;

  /* p2 now shares p1's s_info */
  p2->s_info = p1->s_info;

  /* reuse p to save curr_dm_list */
  p = curr_dm_list;
  curr_dm_list = p2;
  if(p2->aim){
    save_cclp = curr_cmd_list;
    curr_cmd_list = p1->aim;
  }
  set_scroll();
  curr_dm_list = p;
  if(p2->aim)
    curr_cmd_list = save_cclp;

  p2->_dmp->dm_vp = &p2->s_info->_Viewscale;

  /* increment the reference count */
  ++p1->s_info->_rc;

  dmaflag = 1;
  return TCL_OK;
}

static void
find_new_owner( op )
struct dm_list *op;
{
  struct dm_list *p;
  struct dm_list *save_cdlp;
  struct cmd_list *save_cclp;

  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    /* first one found is the new owner */
    if(op != p && p->s_info == op->s_info){
      p->_owner = 1;
      p->s_info->opp = &p->_dmp->dm_pathName;
      mged_slider_unlink_vars(p);
      mged_slider_free_vls(p);
      mged_slider_link_vars(p);

      save_cdlp = curr_dm_list;
      curr_dm_list = p;
      if(p->aim){
	save_cclp = curr_cmd_list;
	curr_cmd_list = p->aim;
      }
      set_scroll();
      curr_dm_list = save_cdlp;
      if(p->aim)
	curr_cmd_list = save_cclp;

      return;
    }
  }

  Tcl_AppendResult(interp, "find_new_owner: Failed to find a new owner\n", (char *)NULL);
}

int
f_share_vars(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_list *dlp1, *dlp2, *dlp3;
  struct _mged_variables *save_mvp;

  if(argc != 3){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help share_vars");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l)
    if(!strcmp(argv[1], bu_vls_addr(&dlp1->_dmp->dm_pathName)))
      break;

  if(dlp1 == &head_dm_list){
     Tcl_AppendResult(interp, "f_share_vars: unrecognized pathName - ",
		      argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  FOR_ALL_DISPLAYS(dlp2, &head_dm_list.l)
    if(!strcmp(argv[2], bu_vls_addr(&dlp2->_dmp->dm_pathName)))
      break;

  if(dlp2 == &head_dm_list){
     Tcl_AppendResult(interp, "f_share_vars: unrecognized pathName - ",
		      argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(dlp1 == dlp2)
    return TCL_OK;

  /* already sharing mged variables */
  if(dlp1->_mged_variables == dlp2->_mged_variables)
    return TCL_OK;


  save_mvp = dlp2->_mged_variables;
  dlp2->_mged_variables = dlp1->_mged_variables;

  /* check if save_mvp is being used elsewhere */
  FOR_ALL_DISPLAYS(dlp3, &head_dm_list.l)
    if(save_mvp == dlp3->_mged_variables)
      break;

  /* save_mvp is not being used */
  if(dlp3 == &head_dm_list)
    bu_free((genptr_t)save_mvp, "f_share_menu: save_mvp");

  /* need to redraw this guy */
  dlp2->_dirty = 1;

  return TCL_OK;
}

int
f_unshare_vars(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_list *dlp1, *dlp2;

  if(argc != 2){
    return TCL_ERROR;
  }

  FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l)
    if(!strcmp(argv[1], bu_vls_addr(&dlp1->_dmp->dm_pathName)))
      break;

  if(dlp1 == &head_dm_list){
     Tcl_AppendResult(interp, "f_unshare_menu: unrecognized pathName - ",
		      argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  FOR_ALL_DISPLAYS(dlp2, &head_dm_list.l)
    if(dlp1 != dlp2 && dlp1->_mged_variables == dlp2->_mged_variables)
      break;

  /* not sharing mged_variables ---- nothing to do */
  if(dlp2 == &head_dm_list)
    return TCL_OK;

  BU_GETSTRUCT(dlp1->_mged_variables, _mged_variables);
  *dlp1->_mged_variables = *dlp2->_mged_variables;  /* struct copy */

  return TCL_OK;
}

void
dm_var_init(initial_dm_list)
struct dm_list *initial_dm_list;
{
  int i;

  BU_GETSTRUCT(curr_dm_list->s_info, shared_info);
  BU_GETSTRUCT(mged_variables, _mged_variables);
  *mged_variables = *initial_dm_list->_mged_variables; /* struct copy */

#if 1
  bn_mat_copy(Viewrot, initial_dm_list->s_info->_Viewrot);
  bn_mat_copy(toViewcenter, initial_dm_list->s_info->_toViewcenter);
  bn_mat_copy(ModelDelta, bn_mat_identity);
  Viewscale = initial_dm_list->s_info->_Viewscale;
#else
  bn_mat_copy(Viewrot, bn_mat_identity);
  size_reset();
#endif
  MAT_DELTAS_GET_NEG(orig_pos, toViewcenter);
  new_mats();

  am_mode = AMM_IDLE;
  rc = 1;
  dmaflag = 1;
  owner = 1;
  frametime = 1;
  mapped = 1;
  adc_a1_deg = adc_a2_deg = 45.0;
  mged_variables->v_axes_pos = initial_dm_list->_mged_variables->v_axes_pos;
  mged_view_init(curr_dm_list);

  BU_GETSTRUCT(curr_dm_list->menu_vars, menu_vars);
}

void
mged_slider_init_vls(p)
struct dm_list *p;
{
  bu_vls_init(&p->s_info->_aet_name);
  bu_vls_init(&p->s_info->_ang_name);
  bu_vls_init(&p->s_info->_center_name);
  bu_vls_init(&p->s_info->_size_name);
  bu_vls_init(&p->s_info->_adc_name);
  bu_vls_init(&p->s_info->_rate_tran_vls[X]);
  bu_vls_init(&p->s_info->_rate_tran_vls[Y]);
  bu_vls_init(&p->s_info->_rate_tran_vls[Z]);
  bu_vls_init(&p->s_info->_rate_rotate_vls[X]);
  bu_vls_init(&p->s_info->_rate_rotate_vls[Y]);
  bu_vls_init(&p->s_info->_rate_rotate_vls[Z]);
  bu_vls_init(&p->s_info->_rate_scale_vls);
  bu_vls_init(&p->s_info->_absolute_tran_vls[X]);
  bu_vls_init(&p->s_info->_absolute_tran_vls[Y]);
  bu_vls_init(&p->s_info->_absolute_tran_vls[Z]);
  bu_vls_init(&p->s_info->_absolute_rotate_vls[X]);
  bu_vls_init(&p->s_info->_absolute_rotate_vls[Y]);
  bu_vls_init(&p->s_info->_absolute_rotate_vls[Z]);
  bu_vls_init(&p->s_info->_absolute_scale_vls);
  bu_vls_init(&p->s_info->_xadc_vls);
  bu_vls_init(&p->s_info->_yadc_vls);
  bu_vls_init(&p->s_info->_ang1_vls);
  bu_vls_init(&p->s_info->_ang2_vls);
  bu_vls_init(&p->s_info->_distadc_vls);
  bu_vls_init(&p->s_info->_Viewscale_vls);
}

void
mged_slider_free_vls(p)
struct dm_list *p;
{
  bu_vls_free(&p->s_info->_aet_name);
  bu_vls_free(&p->s_info->_ang_name);
  bu_vls_free(&p->s_info->_center_name);
  bu_vls_free(&p->s_info->_size_name);
  bu_vls_free(&p->s_info->_adc_name);
  bu_vls_free(&p->s_info->_rate_tran_vls[X]);
  bu_vls_free(&p->s_info->_rate_tran_vls[Y]);
  bu_vls_free(&p->s_info->_rate_tran_vls[Z]);
  bu_vls_free(&p->s_info->_rate_rotate_vls[X]);
  bu_vls_free(&p->s_info->_rate_rotate_vls[Y]);
  bu_vls_free(&p->s_info->_rate_rotate_vls[Z]);
  bu_vls_free(&p->s_info->_rate_scale_vls);
  bu_vls_free(&p->s_info->_absolute_tran_vls[X]);
  bu_vls_free(&p->s_info->_absolute_tran_vls[Y]);
  bu_vls_free(&p->s_info->_absolute_tran_vls[Z]);
  bu_vls_free(&p->s_info->_absolute_rotate_vls[X]);
  bu_vls_free(&p->s_info->_absolute_rotate_vls[Y]);
  bu_vls_free(&p->s_info->_absolute_rotate_vls[Z]);
  bu_vls_free(&p->s_info->_absolute_scale_vls);
  bu_vls_free(&p->s_info->_xadc_vls);
  bu_vls_free(&p->s_info->_yadc_vls);
  bu_vls_free(&p->s_info->_ang1_vls);
  bu_vls_free(&p->s_info->_ang2_vls);
  bu_vls_free(&p->s_info->_distadc_vls);
  bu_vls_free(&p->s_info->_Viewscale_vls);
}

void
mged_slider_link_vars(p)
struct dm_list *p;
{
  mged_slider_init_vls(p);

  bu_vls_printf(&p->s_info->_aet_name, "mged_display(%S,aet)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_ang_name, "mged_display(%S,ang)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_center_name, "mged_display(%S,center)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_size_name, "mged_display(%S,size)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_adc_name, "mged_display(%S,adc)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[X], "rate_tran(%S,X)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[Y], "rate_tran(%S,Y)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[Z], "rate_tran(%S,Z)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[X], "rate_rotate(%S,X)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[Y], "rate_rotate(%S,Y)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[Z], "rate_rotate(%S,Z)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_rate_scale_vls, "rate_scale(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[X], "abs_tran(%S,X)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[Y], "abs_tran(%S,Y)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[Z], "abs_tran(%S,Z)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[X], "abs_rotate(%S,X)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[Y], "abs_rotate(%S,Y)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[Z], "abs_rotate(%S,Z)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_absolute_scale_vls, "abs_scale(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_xadc_vls, "xadc(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_yadc_vls, "yadc(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_ang1_vls, "ang1(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_ang2_vls, "ang2(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_distadc_vls, "distadc(%S)",
		&p->_dmp->dm_pathName);
  bu_vls_printf(&p->s_info->_Viewscale_vls, "Viewscale(%S)",
		&p->_dmp->dm_pathName);

  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[X]),
	      (char *)&p->s_info->_rate_tran[X], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[Y]),
	      (char *)&p->s_info->_rate_tran[Y], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[Z]),
	      (char *)&p->s_info->_rate_tran[Z], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[X]),
	      (char *)&p->s_info->_rate_rotate[X], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[Y]),
	      (char *)&p->s_info->_rate_rotate[Y], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[Z]),
	      (char *)&p->s_info->_rate_rotate[Z], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_rate_scale_vls),
	      (char *)&p->s_info->_rate_scale, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[X]),
	      (char *)&p->s_info->_absolute_tran[X], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[Y]),
	      (char *)&p->s_info->_absolute_tran[Y], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[Z]),
	      (char *)&p->s_info->_absolute_tran[Z], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[X]),
	      (char *)&p->s_info->_absolute_rotate[X], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[Y]),
	      (char *)&p->s_info->_absolute_rotate[Y], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[Z]),
	      (char *)&p->s_info->_absolute_rotate[Z], TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_absolute_scale_vls),
	      (char *)&p->s_info->_absolute_scale, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_xadc_vls),
	      (char *)&p->s_info->_dv_xadc, TCL_LINK_INT);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_yadc_vls),
	      (char *)&p->s_info->_dv_yadc, TCL_LINK_INT);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_ang1_vls),
	      (char *)&p->s_info->_adc_a1_deg, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_ang2_vls),
	      (char *)&p->s_info->_adc_a2_deg, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_distadc_vls),
	      (char *)&p->s_info->_dv_distadc, TCL_LINK_INT);
  Tcl_LinkVar(interp, bu_vls_addr(&p->s_info->_Viewscale_vls),
	      (char *)&p->s_info->_Viewscale,
	      TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);
}

void
mged_slider_unlink_vars(p)
struct dm_list *p;
{
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[X]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[Y]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_tran_vls[Z]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[X]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[Y]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_rotate_vls[Z]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_rate_scale_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[X]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[Y]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_tran_vls[Z]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[X]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[Y]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_rotate_vls[Z]));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_absolute_scale_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_xadc_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_yadc_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_ang1_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_ang2_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_distadc_vls));
  Tcl_UnlinkVar(interp, bu_vls_addr(&p->s_info->_Viewscale_vls));
}
