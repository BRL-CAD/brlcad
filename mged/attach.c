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
#include "./mged_solid.h"
#include "./sedit.h"
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

extern void set_scroll();  /* defined in set.c */
extern void color_soltab();

/* All systems can compile these! */
extern struct dm dm_Null;

extern int Plot_dm_init();
extern struct dm dm_Plot;	/* Unix Plot */
#define IS_DM_PLOT(dp) ((dp) == &dm_Plot)

extern int PS_dm_init();
extern struct dm dm_PS;		/* PostScript */
#define IS_DM_PS(dp) ((dp) == &dm_PS)

#ifdef DM_X
extern int X_dm_init();
extern struct dm dm_X;
#define IS_DM_X(dp) ((dp) == &dm_X)
#else
#define IS_DM_X(dp) (0)
#endif

#ifdef DM_OGL
extern int Ogl_dm_init();
extern struct dm dm_ogl;
#define IS_DM_OGL(dp) ((dp) == &dm_ogl)
#else
#define IS_DM_OGL(dp) (0)
#endif

#ifdef DM_GLX
extern int Glx_dm_init();
extern struct dm dm_glx;
#define IS_DM_GLX(dp) ((dp) == &dm_glx)
#else
#define IS_DM_GLX(dp) (0)
#endif

#ifdef DM_PEX
extern int Pex_dm_init();
extern struct dm dm_pex;
#define IS_DM_PEX(dp) ((dp) == &dm_pex)
#else
#define IS_DM_PEX(dp) (0)
#endif

#define NEED_GUI(dp) ( \
	IS_DM_OGL(dp) || \
	IS_DM_GLX(dp) || \
	IS_DM_PEX(dp) || \
	IS_DM_X(dp) )

extern Tk_Window tkwin;
extern struct _mged_variables default_mged_variables;
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list;
char tmp_str[1024];

static char *default_view_strings[] = {
  "top",
  "right",
  "front",
  "45,45",
  "35,25"
};

struct w_dm {
  struct dm *dp;
  int (*init)();
};

static struct w_dm which_dm[] = {
  { &dm_Plot, Plot_dm_init },
  { &dm_PS, PS_dm_init },
#ifdef DM_X
  { &dm_X, X_dm_init },
#endif
#ifdef DM_PEX
  { &dm_pex, Pex_dm_init },
#endif
#ifdef DM_OGL
  { &dm_ogl, Ogl_dm_init },
#endif
#ifdef DM_GLX
  { &dm_glx, Glx_dm_init },
#endif
  { (struct dm *)0, (int (*)())0}
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

    for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
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

  if(!--p->s_info->_rc){
    if(rate_tran_vls[X].vls_magic == BU_VLS_MAGIC){
#if TRY_NEW_MGED_VARS
      mged_variable_free_vls(p);
#endif
      mged_slider_unlink_vars(p);
      mged_slider_free_vls(p);
    }
    bu_free( (genptr_t)p->s_info, "release: s_info" );
  }else if(p->_owner)
    find_new_owner(p);

  /* If this display is being referenced by a command window,
     then remove the reference  */
  for( BU_LIST_FOR(p_cmd, cmd_list, &head_cmd_list.l) )
    if(p_cmd->aim == p)
      break;

  if(p_cmd->aim == p)
    p_cmd->aim = (struct dm_list *)NULL;

  if(need_close){
    /* Delete all references to display processor memory */
    FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
      rt_memfree( &(dmp->dm_map), sp->s_bytes, (unsigned long)sp->s_addr );
      sp->s_bytes = 0;
      sp->s_addr = 0;
    }
    rt_mempurge( &(dmp->dm_map) );
    
    dmp->dm_close(dmp);
  }
	
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
    Tcl_Eval(interp, "help release");
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
reattach()
{
  char *av[6];

  av[0] = "attach";
  av[1] = "-d";
  av[2] = bu_vls_addr(&dName);
  av[3] = "-n";
  av[4] = dmp->dm_name;
  av[5] = NULL;

  return f_attach((ClientData)NULL, interp, 5, av);
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
    Tcl_Eval(interp, "help attach");
    print_valid_dm();
    return TCL_ERROR;
  }

  for( wp = &which_dm[0]; wp->dp != (struct dm *)0; wp++ )
    if( strcmp(argv[argc - 1], wp->dp->dm_name ) == 0 )
      break;

  if(wp->dp == (struct dm *)0){
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
  BU_LIST_APPEND(&head_dm_list.l, &curr_dm_list->l);
  /* Only need to do this once */
  if(tkwin == NULL && NEED_GUI(wp->dp)){
    if(gui_setup() == TCL_ERROR){
      BU_LIST_DEQUEUE( &curr_dm_list->l );
      bu_free( (genptr_t)curr_dm_list, "f_attach: dm_list" );
      curr_dm_list = o_dm_list;
      return TCL_ERROR;
    }
  }

  if(wp->init(o_dm_list, argc, argv) == TCL_ERROR)
    goto Bad;
  no_memory = 0;

#if TRY_NEW_MGED_VARS
  mged_variable_setup(curr_dm_list);
#endif

  bu_vls_init(&fps_name);
  bu_vls_printf(&fps_name, "mged_display(%S,fps)",
		&curr_dm_list->_dmp->dm_tkName);
  bu_vls_init(&curr_dm_list->_scroll_edit_vls);
  bu_vls_printf(&curr_dm_list->_scroll_edit_vls, "scroll_edit(%S)",
		&curr_dm_list->_dmp->dm_tkName);
  Tcl_LinkVar(interp, bu_vls_addr(&curr_dm_list->_scroll_edit_vls),
	      (char *)&curr_dm_list->_scroll_edit, TCL_LINK_INT);
  mged_slider_link_vars(curr_dm_list);
  (void)f_load_dv((ClientData)NULL, interp, 0, NULL);
  mmenu_init();
  btn_head_menu(0,0,0);

  if(state == ST_S_EDIT)
    sedit_menu();
  else if(state == ST_O_EDIT)
    chg_l2menu(ST_O_EDIT);

  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "ATTACHING ", dmp->dm_name, " (", dmp->dm_lname,
		   ")\n", (char *)NULL);

  FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
    /* Write vector subs into new display processor */
    if( (sp->s_bytes = dmp->dm_cvtvecs( dmp, sp )) != 0 )  {
      sp->s_addr = rt_memalloc( &(dmp->dm_map), sp->s_bytes );
      if( sp->s_addr == 0 )  break;
      sp->s_bytes = dmp->dm_load(dmp, sp->s_addr, sp->s_bytes);
    } else {
      sp->s_addr = 0;
      sp->s_bytes = 0;
    }
  }

  color_soltab();
  ++dmaflag;
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

  while(1){
    bu_log("attach (nu");
    /* skip plot and ps */
    wp = &which_dm[2];
    for( ; wp->dp != (struct dm *)0; wp++ )
      bu_log("|%s", wp->dp->dm_name);
    bu_log(")[nu]? ");
    (void)fgets(line, sizeof(line), stdin); /* \n, Null terminated */

    if(line[0] == '\n' || strncmp(line, "nu", 2) == 0)
      return;  /* Nothing more to do. */

    line[strlen(line)-1] = '\0';        /* remove newline */

    for( wp = &which_dm[0]; wp->dp != (struct dm *)0; wp++ )
      if( strcmp( line, wp->dp->dm_name ) == 0 )
	break;

    if( wp->dp != (struct dm *)0 )
      break;

    /* Not a valid choice, loop. */
  }

  argc = 2;
  argv[0] = "";
  argv[1] = "";
  argv[2] = (char *)NULL;
  (void)mged_attach(wp, argc, argv);
}


int
gui_setup()
{
  char *filename;
  int status;

  /* initialize only once */
  if(tkwin != NULL)
    return TCL_OK;

  /* This runs the tk.tcl script */
  if(Tk_Init(interp) == TCL_ERROR){
    Tcl_AppendResult(interp, "\ngui_setup: Try setting the TK_LIBRARY environment variable\n",
		               "           to the path where tk.tcl lives.\n\n", (char *)NULL);
    return TCL_ERROR;
  }

  if((tkwin = Tk_MainWindow(interp)) == NULL){
    Tcl_AppendResult(interp, "gui_setup: Failed to get main window.\n", (char *)NULL);
    return TCL_ERROR;
  }

  (void)Tcl_Eval( interp, "wm withdraw .");

  /* Check to see if user specified MGED_GUIRC */
  if((filename = getenv("MGED_GUIRC")) != (char *)NULL )  {
	if( Tcl_EvalFile( interp, filename ) != TCL_OK )  {
		bu_log("Error reading %s:\n%s\n", filename,
			Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
	}
  }

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
    Tcl_Eval(interp, "help dm");
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
f_untie(clientData, interp, argc, argv )
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
    Tcl_Eval(interp, "help untie");
    return TCL_ERROR;
  }

  bu_vls_init(&vls1);

  if(*argv[1] != '.')
    bu_vls_printf(&vls1, ".%s", argv[1]);
  else
    bu_vls_strcpy(&vls1, argv[1]);

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) )
    if(!strcmp(bu_vls_addr(&vls1), bu_vls_addr(&p->_dmp->dm_pathName)))
      break;

  if(p == &head_dm_list){
    Tcl_AppendResult(interp, "untie: bad pathname - %s\n",
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
  p->s_info->opp = &p->_dmp->dm_tkName;
#if TRY_NEW_MGED_VARS
  mged_variable_setup(p);
#endif
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
  
  return TCL_OK;
}

int
f_tie(clientData, interp, argc, argv )
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
    Tcl_Eval(interp, "help tie");
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

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
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
    Tcl_AppendResult(interp, "f_tie: bad pathname(s)\n\tpathName1 - ",
		     bu_vls_addr(&vls1), "\t\tpathName2 - ",
		     bu_vls_addr(&vls2), "\n", (char *)NULL);
    bu_vls_free(&vls1);
    bu_vls_free(&vls2);
    return TCL_ERROR;
  }

  bu_vls_free(&vls1);
  bu_vls_free(&vls2);

  /* free p1's s_info struct if not being used */
  if(!--p1->s_info->_rc){
#if TRY_NEW_MGED_VARS
    mged_variable_free_vls(p1);
#endif
    mged_slider_unlink_vars(p1);
    mged_slider_free_vls(p1);
    bu_free( (genptr_t)p1->s_info, "tie: s_info" );
  /* otherwise if p1's s_info struct is being used and p1 is the owner */
  }else if(p1->_owner)
    find_new_owner(p1);

  p1->_owner = 0;

  /* p1 now shares p2's s_info */
  p1->s_info = p2->s_info;

  /* reuse p to save curr_dm_list */
  p = curr_dm_list;
  curr_dm_list = p1;
  if(p1->aim){
    save_cclp = curr_cmd_list;
    curr_cmd_list = p1->aim;
  }
  set_scroll();
  curr_dm_list = p;
  if(p1->aim)
    curr_cmd_list = save_cclp;

  p1->_dmp->dm_vp = &p1->s_info->_Viewscale;

  /* increment the reference count */
  ++p2->s_info->_rc;

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

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    /* first one found is the new owner */
    if(op != p && p->s_info == op->s_info){
      p->_owner = 1;
      p->s_info->opp = &p->_dmp->dm_tkName;
#if TRY_NEW_MGED_VARS
      mged_variable_free_vls(p);
      mged_variable_setup(p);
#endif
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


void
dm_var_init(initial_dm_list)
struct dm_list *initial_dm_list;
{
  int i;

  BU_GETSTRUCT(curr_dm_list->s_info, shared_info);
  mged_variables = default_mged_variables;

  bn_mat_copy(Viewrot, bn_mat_identity);
  size_reset();
  MAT_DELTAS_GET(orig_pos, toViewcenter);
  new_mats();

  am_mode = ALT_MOUSE_MODE_IDLE;
  rc = 1;
  dmaflag = 1;
  owner = 1;
  frametime = 1;
  adc_a1_deg = adc_a2_deg = 45.0;
  last_v_axes = 2; /* center location */
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
}

void
mged_slider_link_vars(p)
struct dm_list *p;
{
  mged_slider_init_vls(p);

  bu_vls_printf(&p->s_info->_aet_name, "mged_display(%S,aet)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_ang_name, "mged_display(%S,ang)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_center_name, "mged_display(%S,center)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_size_name, "mged_display(%S,size)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_adc_name, "mged_display(%S,adc)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[X], "rate_tran(%S,X)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[Y], "rate_tran(%S,Y)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_tran_vls[Z], "rate_tran(%S,Z)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[X], "rate_rotate(%S,X)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[Y], "rate_rotate(%S,Y)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_rotate_vls[Z], "rate_rotate(%S,Z)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_rate_scale_vls, "rate_scale(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[X], "abs_tran(%S,X)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[Y], "abs_tran(%S,Y)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_tran_vls[Z], "abs_tran(%S,Z)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[X], "abs_rotate(%S,X)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[Y], "abs_rotate(%S,Y)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_rotate_vls[Z], "abs_rotate(%S,Z)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_absolute_scale_vls, "abs_scale(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_xadc_vls, "xadc(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_yadc_vls, "yadc(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_ang1_vls, "ang1(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_ang2_vls, "ang2(%S)",
		&p->_dmp->dm_tkName);
  bu_vls_printf(&p->s_info->_distadc_vls, "distadc(%S)",
		&p->_dmp->dm_tkName);

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
}


/* Load default views */
int
f_load_dv(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int i;

  if(argc < 1 || 1 < argc){
    Tcl_Eval(interp, "help load_dv");
    return TCL_ERROR;
  }

  for(i = 0; i < VIEW_TABLE_SIZE; ++i){
    press(default_view_strings[i]);
    bn_mat_copy(viewrot_table[i], Viewrot);
    viewscale_table[i] = Viewscale;
  }

  current_view = 0;
  bn_mat_copy(Viewrot, viewrot_table[current_view]);
  Viewscale = viewscale_table[current_view];
  new_mats();

  return TCL_OK;
}
