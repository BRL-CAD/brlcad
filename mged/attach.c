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
#include "./mged_dm.h"

int gui_setup();
static int do_2nd_attach_prompt();
static void find_new_owner();
static void dm_var_init();

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
	  if( curr_dm_list == &head_dm_list )
	    return TCL_OK;  /* Ignore */
	  else
	    p = curr_dm_list;
	}

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
	
	if(!--p->s_info->_rc)
	  bu_free( (genptr_t)p->s_info, "release: s_info" );
	else if(p->_owner)
	  find_new_owner(p);

	/* If this display is being referenced by a command window,
	   then remove the reference  */
	for( BU_LIST_FOR(p_cmd, cmd_list, &head_cmd_list.l) )
	  if(p_cmd->aim == p)
	    break;

	if(p_cmd->aim == p)
	  p_cmd->aim = (struct dm_list *)NULL;

	bu_vls_free(&pathName);
	BU_LIST_DEQUEUE( &p->l );
	bu_free( (genptr_t)p->_dmp, "release: curr_dm_list->_dmp" );
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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if(argc == 2)
    return release(argv[1], 1);

  return release(NULL, 1);
}


int
reattach()
{
  char *av[4];

  av[0] = "attach";
  av[1] = dmp->dm_name;
  av[2] = dname;
  av[3] = NULL;

  return f_attach((ClientData)NULL, interp, 3, av);
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
  register struct solid *sp;
  register struct dm_list *dmlp;
  register struct dm_list *o_dm_list;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if(argc == 1){
    wp = &which_dm[2];  /* not advertising dm_Plot or dm_PS */
    Tcl_AppendResult(interp, MORE_ARGS_STR,
		     "attach (", (wp++)->dp->dm_name, (char *)NULL);
    for( ; wp->dp != (struct dm *)0; wp++ )
      Tcl_AppendResult(interp, "|", wp->dp->dm_name, (char *)NULL);
    Tcl_AppendResult(interp, ")? ",  (char *)NULL);
    
    return TCL_ERROR;
  }

  for( wp = &which_dm[0]; wp->dp != (struct dm *)0; wp++ )
    if( strcmp(argv[1], wp->dp->dm_name ) == 0 )
      break;

  if(wp->dp == (struct dm *)0){
    Tcl_AppendResult(interp, "attach(", argv[1], "): BAD\n", (char *)NULL);
    return TCL_ERROR;
  }
  
  if(argc == 2 && NEED_GUI(wp->dp)){
    return do_2nd_attach_prompt();
  }else{
    BU_GETSTRUCT(dmlp, dm_list);
    BU_LIST_APPEND(&head_dm_list.l, &dmlp->l);
    o_dm_list = curr_dm_list;
    curr_dm_list = dmlp;
    BU_GETSTRUCT(dmp, dm);
    *dmp = *wp->dp;
    curr_dm_list->dm_init = wp->init;
    dm_var_init(o_dm_list, argv[2]);
    dmp->dm_vp = &Viewscale;
  }

  no_memory = 0;

  /* Only need to do this once */
  if(tkwin == NULL && NEED_GUI(wp->dp)){
    if(gui_setup(argv[2]) == TCL_ERROR)
      goto Bad;
  }

  curr_dm_list->dm_init();
  bu_vls_init(&dmp->dm_initWinProc);
  bu_vls_strcpy(&dmp->dm_initWinProc, "mged_bind_dm_win");
  if(dmp->dm_open(dmp, argc - 2, argv + 2))
    goto Bad;

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
#if 0
  dmp->dm_colorchange(dmp);
  dmp->dm_viewchange( dmp, DM_CHGV_REDO, SOLID_NULL );
#endif
  ++dmaflag;
  return TCL_OK;

Bad:
  Tcl_AppendResult(interp, "attach(", argv[1], "): BAD\n", (char *)NULL);

  if(dmp->dm_vars != (genptr_t)0)
    release((char *)NULL, 1);  /* relesae() will call dm_close */
  else
    release((char *)NULL, 0);  /* release() will not call dm_close */

  return TCL_ERROR;
}


int
gui_setup(screen)
char *screen;
{
  char *filename;
  int status;

  /* initialize only once */
  if(tkwin != NULL)
    return TCL_OK;

#if 0
  if((tkwin = Tk_CreateMainWindow(interp, screen, "MGED", "MGED")) == NULL){
    bu_log("gui_setup: Failed to create main window.\n");
    return TCL_ERROR;
  }

  /* This runs the tk.tcl script */
  if (Tk_Init(interp) == TCL_ERROR){
    bu_log("Tk_Init error %s\n", interp->result);
    return TCL_ERROR;
  }
#else

#if 0
  {
    Display *tkdpy;

    tkdpy = XOpenDisplay(":0.0");
    printf("stop\n");
  }
#endif

  /* This runs the tk.tcl script */
  if (Tk_Init(interp) == TCL_ERROR){
    bu_log("Tk_Init error %s\n", interp->result);
    return TCL_ERROR;
  }

  if((tkwin = Tk_MainWindow(interp)) == NULL){
    bu_log("gui_setup: Failed to get main window.\n");
    return TCL_ERROR;
  }
#endif

  Tcl_Eval( interp, "wm withdraw .");

  /* Check to see if user specified MGED_GUIRC */
  if((filename = getenv("MGED_GUIRC")) == (char *)NULL )
    return TCL_OK;

  if(Tcl_EvalFile( interp, filename ) == TCL_ERROR)
    bu_log("gui_setup: %s\n", interp->result);

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
        return TCL_ERROR;

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) )
    if(!strcmp(argv[1], bu_vls_addr(&p->_dmp->dm_pathName)))
      break;

  if(p == &head_dm_list){
    Tcl_AppendResult(interp, "f_untie: bad pathname - %s\n", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  if(p->_owner){
    if(p->s_info->_rc > 1){  /* sharing s_info with another display manager */
      --p->s_info->_rc;
      sip = p->s_info;
      find_new_owner(p);
      BU_GETSTRUCT(p->s_info, shared_info);
      bcopy((void *)sip, (void *)p->s_info, sizeof(struct shared_info));
      p->s_info->_rc = 1;
      p->_owner = 1;
      p->_dmp->dm_vp = &p->s_info->_Viewscale;
      
      return TCL_OK;
    }else
      return TCL_OK; /* Nothing to do */
  }else{
    --p->s_info->_rc;
    sip = p->s_info;
    BU_GETSTRUCT(p->s_info, shared_info);
    bcopy((void *)sip, (void *)p->s_info, sizeof(struct shared_info));
    p->s_info->_rc = 1;
    p->_owner = 1;
    p->_dmp->dm_vp = &p->s_info->_Viewscale;

    return TCL_OK;
  }
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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    if(p1 == (struct dm_list *)NULL && !strcmp(argv[1], bu_vls_addr(&p->_dmp->dm_pathName)))
	p1 = p;
    else if(p2 == (struct dm_list *)NULL && !strcmp(argv[2], bu_vls_addr(&p->_dmp->dm_pathName)))
      p2 = p;
    else if(p1 != (struct dm_list *)NULL && p2 != (struct dm_list *)NULL)
      break;
  }

  if(p1 == (struct dm_list *)NULL || p2 == (struct dm_list *)NULL){
    Tcl_AppendResult(interp, "f_tie: bad pathname(s)\n\tpathName1 - ", argv[1],
		     "\t\tpathName2 - ", argv[2], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  /* free p1's s_info struct if not being used */
  if(!--p1->s_info->_rc)
    bu_free( (genptr_t)p1->s_info, "tie: s_info" );
  /* otherwise if p1's s_info struct is being used and p1 is the owner */
  else if(p1->_owner)
    find_new_owner(p1);

  p1->_owner = 0;

  /* p1 now shares p2's s_info */
  p1->s_info = p2->s_info;

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

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    /* first one found is the new owner */
    if(op != p && p->s_info == op->s_info){
      p->_owner = 1;
      return;
    }
  }

  Tcl_AppendResult(interp, "find_new_owner: Failed to find a new owner\n", (char *)NULL);
}


static void
dm_var_init(initial_dm_list, name)
struct dm_list *initial_dm_list;
char *name;
{
  int i;

  BU_GETSTRUCT(curr_dm_list->s_info, shared_info);
  bcopy((void *)&default_mged_variables, (void *)&mged_variables,
	sizeof(struct _mged_variables));

  if(name)
    strcpy(dname, name);
  bn_mat_copy(Viewrot, bn_mat_identity);
  size_reset();
  new_mats();
  (void)f_load_dv((ClientData)NULL, interp, 0, NULL);

  MAT_DELTAS_GET(orig_pos, toViewcenter);

  am_mode = ALT_MOUSE_MODE_IDLE;
  rc = 1;
  dmaflag = 1;
  owner = 1;
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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
