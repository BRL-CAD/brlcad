/*
 *			A T T A C H . C
 *
 * Functions -
 *	attach		attach to a given display processor
 *	release		detach from current display processor
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
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

MGED_EXTERN(void	Nu_input, (fd_set *input, int noblock) );
static void	Nu_void();
static int	Nu_int0();
static unsigned Nu_unsign();
static int do_2nd_attach_prompt();
static void find_new_owner();
static void dm_var_init();

struct dm dm_Null = {
  Nu_int0,
  Nu_int0, Nu_void,
  Nu_input,
  Nu_void, Nu_void,
  Nu_void, Nu_void,
  Nu_void,
  Nu_void, Nu_void,
  Nu_void,
  Nu_int0,
  Nu_unsign, Nu_unsign,
  Nu_void,
  Nu_void,
  Nu_void,
  Nu_void, Nu_void, Nu_int0, Nu_int0,
  0,			/* no displaylist */
  0,			/* no display to release */
  0.0,
  "nu", "Null Display",
  0,
  0,
  0,
  0
};

/* All systems can compile these! */
extern struct dm dm_Tek;	/* Tek 4014 */
extern struct dm dm_T49;	/* Tek 4109 */
extern struct dm dm_Plot;	/* Unix Plot */
extern struct dm dm_PS;		/* PostScript */
#define IS_DM_PS(dp) ((dp) == &dm_PS)

#ifdef DM_MG
/* We only supply a kernel driver for Berkeley VAX systems for the MG */
extern struct dm dm_Mg;
#endif

#ifdef DM_VG
/* We only supply a kernel driver for Berkeley VAX systems for the VG */
extern struct dm dm_Vg;
#endif

#ifdef DM_RAT
extern struct dm dm_Rat;
#endif

#ifdef DM_RAT80
extern struct dm dm_Rat80;
#endif

#ifdef DM_MER
extern struct dm dm_Mer;
#endif

#ifdef DM_PS
extern struct dm dm_Ps;
#endif

#ifdef DM_IR
extern struct dm dm_Ir;
#endif

#ifdef DM_4D
extern struct dm dm_4d;
#endif

#ifdef DM_SUNPW
extern struct dm dm_SunPw;
#endif

#ifdef DM_X
extern struct dm dm_X;
#define IS_DM_X(dp) ((dp) == &dm_X)
#else
#define IS_DM_X(dp) (0)
#endif

#ifdef DM_XGL
extern struct dm dm_XGL;
#endif

#ifdef DM_OGL
extern struct dm dm_ogl;
#define IS_DM_OGL(dp) ((dp) == &dm_ogl)
#else
#define IS_DM_OGL(dp) (0)
#endif

#ifdef DM_GLX
extern struct dm dm_glx;
#define IS_DM_GLX(dp) ((dp) == &dm_glx)
#else
#define IS_DM_GLX(dp) (0)
#endif

#ifdef DM_PEX
extern struct dm dm_pex;
#define IS_DM_PEX(dp) ((dp) == &dm_pex)
#else
#define IS_DM_PEX(dp) (0)
#endif

#ifdef USE_LIBDM
extern int Ogl_dm_init();
extern int X_dm_init();
extern int Glx_dm_init();
extern int Pex_dm_init();
extern int PS_dm_init();
extern void color_soltab();

#define NEED_GUI(dp) ( \
	IS_DM_OGL(dp) || \
	IS_DM_GLX(dp) || \
	IS_DM_PEX(dp) || \
	IS_DM_X(dp) || \
	0)
#endif

extern Tk_Window tkwin;
extern struct _mged_variables default_mged_variables;
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list;

static char *view_cmd_str[] = {
  "press top",
  "press right",
  "press front",
  "press 45,45",
  "press 35,25"
};

static struct dm *which_dm[] = {
	&dm_PS,  /* We won't be advertising this guy --- access is now through the ps command */
	&dm_Tek,
	&dm_T49,
	&dm_Plot,
#ifdef DM_IR
	&dm_Ir,
#endif
#ifdef DM_4D
	&dm_4d,
#endif
#ifdef DM_XGL
	&dm_XGL,
#endif
#ifdef DM_GT
	&dm_gt,
#endif
#ifdef DM_SUNPW
	&dm_SunPw,
#endif
#ifdef DM_GLX
	&dm_glx,
#endif
#ifdef DM_PEX
	&dm_pex,
#endif
#ifdef DM_X
	&dm_X,
#endif
#ifdef DM_MG
	&dm_Mg,
#endif
#ifdef DM_VG
	&dm_Vg,
#endif
#ifdef DM_RAT
	&dm_Rat,
#endif
#ifdef DM_MER
	&dm_Mer,
#endif
#ifdef DM_PS
	&dm_Ps,
#endif
#ifdef DM_OGL
	&dm_ogl,
#endif
	0
};

int
release(name)
char *name;
{
	register struct solid *sp;
	struct dm_list *p;
	struct cmd_list *p_cmd;
	struct dm_list *save_dm_list = DM_LIST_NULL;

	if(name != NULL){
	  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
	    if(strcmp(name, bu_vls_addr(&p->_dmp->dmr_pathName)))
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

	/* Delete all references to display processor memory */
	FOR_ALL_SOLIDS( sp )  {
		rt_memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
		sp->s_bytes = 0;
		sp->s_addr = 0;
	}
	rt_mempurge( &(dmp->dmr_map) );

#ifdef USE_LIBDM
	dmp->dmr_close(dmp);
#else
	dmp->dmr_close();
#endif

	if(!--p->s_info->_rc)
	  bu_free( (char *)p->s_info, "release: s_info" );
	else if(p->_owner)
	  find_new_owner(p);

	/* If this display is being referenced by a command window, remove it */
	for( BU_LIST_FOR(p_cmd, cmd_list, &head_cmd_list.l) )
	  if(p_cmd->aim == p)
	    p_cmd->aim = (struct dm_list *)NULL;

	bu_vls_free(&pathName);
	BU_LIST_DEQUEUE( &p->l );
	bu_free( (char *)p, "release: curr_dm_list" );

	if(save_dm_list != DM_LIST_NULL)
	  curr_dm_list = save_dm_list;
	else
	  curr_dm_list = (struct dm_list *)head_dm_list.l.forw;

	return TCL_OK;
}

static int Nu_int0() { return(0); }
static void Nu_void() { ; }
static unsigned Nu_unsign() { return(0); }

/*
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
Nu_input( input, noblock )
fd_set		*input;
int		noblock;
{
	struct timeval	tv;
	int		width;
	int		cnt;

	if( !isatty(fileno(stdin)) )  return;	/* input awaits! */

#if defined(_SC_OPEN_MAX)
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 || width > 32)
#endif
		width = 32;

	if( noblock )  {
		/* 1/20th second */
		tv.tv_sec = 0;
		tv.tv_usec = 50000;
	} else {
		/* Wait a VERY long time for user to type something */
		tv.tv_sec = 9999999;
		tv.tv_usec = 0;
	}
	cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
	if( cnt < 0 )  {
		perror("Nu_input/select()");
	}
}

int
reattach()
{
  struct bu_vls cmd;
  int status;

  bu_vls_init(&cmd);
  bu_vls_printf(&cmd, "attach %s %s\n", dmp->dmr_name, dname);
  release((char *)NULL);
  status = cmdline(&cmd, FALSE);
  bu_vls_free(&cmd);
  return status;
}


static int
do_2nd_attach_prompt(name)
char *name;
{
  static char plot_default[] = "pl-fb";
  static char tek_default[] = "/dev/tty";
#if 0
  static char ps_default[] = "mged.ps";
#endif
  char *dm_default;
  struct bu_vls prompt;

  bu_vls_init(&prompt);

  if(!strcmp(name, "plot")){
    dm_default = plot_default;
    bu_vls_printf(&prompt, "UNIX-Plot filter [pl-fb]? ");
  }else if(!strcmp(name, "tek")){
    dm_default = tek_default;
    bu_vls_printf(&prompt, "Output tty [stdout]? ");
#if 0
  }else if(!strcmp(name, "ps")){
    dm_default = ps_default;
    bu_vls_printf(&prompt, "PostScript file [mged.ps]? ");
#endif
  }else{
    char  hostname[80];
    char  display[82];

    /* get or create the default display */
    if( (dm_default = getenv("DISPLAY")) == NULL ) {
      /* Env not set, use local host */
      gethostname( hostname, 80 );
      hostname[79] = '\0';
      (void)sprintf( display, "%s:0", hostname );
      dm_default = display;
    }

    bu_vls_printf(&prompt, "Display [%s]? ", dm_default);
  }

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
  register struct dm **dp;
  register struct solid *sp;
  register struct dm_list *dmlp;
  register struct dm_list *o_dm_list;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if(argc == 1){
    dp = &which_dm[1];  /* not advertising dm_PS */
    Tcl_AppendResult(interp, MORE_ARGS_STR, "attach (", (*dp++)->dmr_name, (char *)NULL);
    for( ; *dp != (struct dm *)0; dp++ )
      Tcl_AppendResult(interp, "|", (*dp)->dmr_name, (char *)NULL);
    Tcl_AppendResult(interp, ")? ",  (char *)NULL);
    
    return TCL_ERROR;
  }

  for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
    if( strcmp(argv[1], (*dp)->dmr_name ) == 0 )
      break;

  if(*dp == (struct dm *)0)
    goto Bad;

  if(argc == 2){
    return do_2nd_attach_prompt((*dp)->dmr_name);
  }else{
    dmlp = (struct dm_list *)bu_malloc(sizeof(struct dm_list), "struct dm_list");
    bzero((void *)dmlp, sizeof(struct dm_list));
    BU_LIST_APPEND(&head_dm_list.l, &dmlp->l);
    o_dm_list = curr_dm_list;
    curr_dm_list = dmlp;
    dmp = (struct dm *)bu_malloc(sizeof(struct dm), "struct dm");
    *dmp = **dp;
    dm_var_init(o_dm_list, argv[2], *dp);
  }

  no_memory = 0;

  /* Only need to do this once */
  if(tkwin == NULL && NEED_GUI(*dp))
    gui_setup();

#ifdef USE_LIBDM
  if( dm_init() )
    goto Bad;
#else
  if( dmp->dmr_open() )
    goto Bad;
#endif

  Tcl_AppendResult(interp, "ATTACHING ", dmp->dmr_name, " (", dmp->dmr_lname,
		   ")\n", (char *)NULL);

  FOR_ALL_SOLIDS( sp )  {
    /* Write vector subs into new display processor */
#ifdef USE_LIBDM
    if( (sp->s_bytes = dmp->dmr_cvtvecs( dmp, sp )) != 0 )  {
#else
    if( (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
#endif
      sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
      if( sp->s_addr == 0 )  break;
#ifdef USE_LIBDM
      sp->s_bytes = dmp->dmr_load(dmp, sp->s_addr, sp->s_bytes);
#else
      sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes);
#endif
    } else {
      sp->s_addr = 0;
      sp->s_bytes = 0;
    }
  }

#ifdef USE_LIBDM
  dmp->dmr_colorchange(dmp);
#else
  dmp->dmr_colorchange();
#endif
  color_soltab();
#ifdef USE_LIBDM
  dmp->dmr_viewchange( dmp, DM_CHGV_REDO, SOLID_NULL );
#else
  dmp->dmr_viewchange( DM_CHGV_REDO, SOLID_NULL );
#endif
  ++dmaflag;
  return TCL_OK;

Bad:
  Tcl_AppendResult(interp, "attach(", argv[1], "): BAD\n", (char *)NULL);

  if(*dp != (struct dm *)0)
    release((char *)NULL);

  return TCL_ERROR;
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
  if( !dmp->dmr_cmd )  {
    Tcl_AppendResult(interp, "The '", dmp->dmr_name,
		     "' display manager does not support any local commands.\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  return dmp->dmr_cmd( argc-1, argv+1 );
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
    if(!strcmp(argv[1], bu_vls_addr(&p->_dmp->dmr_pathName)))
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
      p->s_info = (struct shared_info *)bu_malloc(sizeof(struct shared_info),
						  "shared_info");
      bcopy((void *)sip, (void *)p->s_info, sizeof(struct shared_info));
      p->s_info->_rc = 1;
      
      return TCL_OK;
    }else
      return TCL_OK; /* Nothing to do */
  }else{
    --p->s_info->_rc;
    sip = p->s_info;
    p->s_info = (struct shared_info *)bu_malloc(sizeof(struct shared_info),
						"shared_info");
    bcopy((void *)sip, (void *)p->s_info, sizeof(struct shared_info));
    p->s_info->_rc = 1;
    p->_owner = 1;

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
    if(p1 == (struct dm_list *)NULL && !strcmp(argv[1], bu_vls_addr(&p->_dmp->dmr_pathName)))
	p1 = p;
    else if(p2 == (struct dm_list *)NULL && !strcmp(argv[2], bu_vls_addr(&p->_dmp->dmr_pathName)))
      p2 = p;
    else if(p1 != (struct dm_list *)NULL && p2 != (struct dm_list *)NULL)
      break;
  }

  if(p1 == (struct dm_list *)NULL || p2 == (struct dm_list *)NULL){
    Tcl_AppendResult(interp, "f_tie: bad pathname(s)\n\tpathName1 - ", argv[1],
		     "\t\tpathName2 - ", argv[2], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  /*XXX this screws things up for dm-ogl's viewscale pointer --- needs fixing */
  /* free p1's s_info struct if not being used */
  if(!--p1->s_info->_rc)
    bu_free( (genptr_t)p1->s_info, "tie: s_info" );
  /* otherwise if p1's s_info struct is being used and p1 is the owner */
  else if(p1->_owner)
    find_new_owner(p1);

  p1->_owner = 0;

  /* p1 now shares p2's s_info */
  p1->s_info = p2->s_info;

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
dm_var_init(initial_dm_list, name, dp)
struct dm_list *initial_dm_list;
char *name;
struct dm *dp;
{
  int i;

  curr_dm_list->s_info = (struct shared_info *)bu_malloc(sizeof(struct shared_info),
							 "shared_info");
  bzero((void *)curr_dm_list->s_info, sizeof(struct shared_info));
  bcopy((void *)&default_mged_variables, (void *)&mged_variables,
	sizeof(struct _mged_variables));

  bu_vls_init(&pathName);
  strcpy(dname, name);
  mat_copy(Viewrot, identity);
  size_reset();
  new_mats();
  (void)f_load_dv((ClientData)NULL, interp, 0, NULL);

  MAT_DELTAS_GET(orig_pos, toViewcenter);

#ifdef USE_LIBDM
  if(IS_DM_X(dp))
    dm_init = X_dm_init;
  else if(IS_DM_OGL(dp))
    dm_init = Ogl_dm_init;
  else if(IS_DM_GLX(dp))
    dm_init = Glx_dm_init;
  else if(IS_DM_PEX(dp))
    dm_init = Pex_dm_init;
  else if(IS_DM_PS(dp))
    dm_init = PS_dm_init;
  else
    dm_init = dmp->dmr_open;
#endif
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
  struct bu_vls vls;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  bu_vls_init(&vls);
  for(i = 0; i < VIEW_TABLE_SIZE; ++i){
    bu_vls_strcpy(&vls, view_cmd_str[i]);
    (void)cmdline(&vls, False);
    mat_copy(viewrot_table[i], Viewrot);
    viewscale_table[i] = Viewscale;
  }
  bu_vls_free(&vls);

  current_view = 0;
  mat_copy(Viewrot, viewrot_table[current_view]);
  Viewscale = viewscale_table[current_view];
  new_mats();

  return TCL_OK;
}
