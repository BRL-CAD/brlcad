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
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

MGED_EXTERN(void	Nu_input, (fd_set *input, int noblock) );
static void	Nu_void();
static int	Nu_int0();
static unsigned Nu_unsign();
void find_new_owner();

struct dm dm_Null = {
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
	Nu_void, Nu_void,
	0,			/* no displaylist */
	0,			/* no display to release */
	0.0,
	"nu", "Null Display"
};

/* All systems can compile these! */
extern struct dm dm_Tek;	/* Tek 4014 */
extern struct dm dm_T49;	/* Tek 4109 */
extern struct dm dm_Plot;	/* Unix Plot */
extern struct dm dm_PS;		/* PostScript */

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
#endif

#ifdef DM_XGL
extern struct dm dm_XGL;
#endif

#ifdef DM_OGL
extern struct dm dm_ogl;
#endif

#ifdef DM_OGL2
extern struct dm dm_ogl2;
#endif

#ifdef DM_GLX
extern struct dm dm_glx;
#endif

#ifdef DM_PEX
extern struct dm dm_pex;
#endif

extern struct _mged_variables default_mged_variables;
struct dm_list head_dm_list;  /* list of active display managers */
struct dm_list *curr_dm_list;
void dm_var_init();

/* The [0] entry will be the startup default */
static struct dm *which_dm[] = {
#if 0
	&dm_Null,		/* This should go first */
#endif
	&dm_Tek,
	&dm_T49,
	&dm_PS,
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
#ifdef DM_OGL2
	&dm_ogl2,
#endif
	0
};

void
release(name)
char *name;
{
	register struct solid *sp;
	struct dm_list *p;
	struct dm_list *save_dm_list = (struct dm_list *)NULL;

	if(name != NULL){
	  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
	    if(strcmp(name, rt_vls_addr(&p->_pathName)))
	      continue;

	    /* found it */
	    if(p != curr_dm_list){
	      save_dm_list = curr_dm_list;
	      curr_dm_list = p;
	    }
	    break;
	  }

	  if(p == &head_dm_list){
	    rt_log("release: %s not found\n", name);
	    return;
	  }
	}else{
	  if( curr_dm_list == &head_dm_list )
	    return;
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

	dmp->dmr_close();

	if(save_dm_list == (struct dm_list *)NULL)
	  curr_dm_list = (struct dm_list *)curr_dm_list->l.forw;
	else
	  curr_dm_list = save_dm_list;  /* put it back the way it was */

	if(!--p->s_info->_rc)
	  rt_free( (char *)p->s_info, "release: s_info" );
	else if(p->_owner)
	  find_new_owner(p);

	RT_LIST_DEQUEUE( &p->l );
	rt_free( (char *)p, "release: curr_dm_list" );

	/* 
	 * If there are any more active displays other than that which is
         * found in head_dm_list, then assign that as the current dm_list.
	 */
	if( curr_dm_list == &head_dm_list &&
	    (struct dm_list *)head_dm_list.l.forw != &head_dm_list)
	  curr_dm_list = (struct dm_list *)head_dm_list.l.forw;
}

void
attach(name)
char *name;
{
  register struct dm **dp;
  register struct solid *sp;

  register struct dm_list *dmlp;
  register struct dm_list *o_dm_list;

  /* The Null display manager is already attached */
  if(!strcmp(name, which_dm[0]->dmr_name))
    return;

  for( dp=which_dm; *dp != (struct dm *)0; dp++ )  {
    if( strcmp( (*dp)->dmr_name, name ) != 0 )
      continue;

    dmlp = (struct dm_list *)rt_malloc(sizeof(struct dm_list),
				       "dm_list");
    bzero((void *)dmlp, sizeof(struct dm_list));

    RT_LIST_APPEND(&head_dm_list.l, &dmlp->l);
    o_dm_list = curr_dm_list;
    curr_dm_list = dmlp;
    dmp = *dp;
    dm_var_init(o_dm_list);

    no_memory = 0;
    if( dmp->dmr_open() )
      break;

    rt_log("ATTACHING %s (%s)\n",
	   dmp->dmr_name, dmp->dmr_lname);

    FOR_ALL_SOLIDS( sp )  {
      /* Write vector subs into new display processor */
      if( (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
	sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
	if( sp->s_addr == 0 )  break;
	sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes);
      } else {
	sp->s_addr = 0;
	sp->s_bytes = 0;
      }
    }
    dmp->dmr_colorchange();
    color_soltab();
    dmp->dmr_viewchange( DM_CHGV_REDO, SOLID_NULL );
    dmaflag++;
    return;
  }
  rt_log("attach(%s): BAD\n", name);

  if(*dp != (struct dm *)0)
    release(NULL);
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

/*
 *  			G E T _ A T T A C H E D
 *
 *  Prompt the user with his options, and loop until a valid choice is made.
 */
void
get_attached()
{
	char line[80];
	register struct dm **dp;

	/* If non-interactive, don't attach a device and don't ask */
	if( !isatty(0) )  {
		attach( "nu" );
		return;
	}

	while(1)  {
		rt_log("attach (");
		dp = &which_dm[0];
		rt_log("%s", (*dp++)->dmr_name);
		for( ; *dp != (struct dm *)0; dp++ )
			rt_log("|%s", (*dp)->dmr_name);
		rt_log(")[%s]? ", which_dm[0]->dmr_name);

		(void)fgets(line, sizeof(line), stdin);	/* \n, Null terminated */
		line[strlen(line)-1] = '\0';		/* remove newline */

		if( feof(stdin) )  quit();
		if( line[0] == '\0' )  {
			dp = &which_dm[0];	/* default */
			break;
		}
		for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
			if( strcmp( line, (*dp)->dmr_name ) == 0 )
				break;
		if( *dp != (struct dm *)0 )
			break;
		/* Not a valid choice, loop. */
	}
	/* Valid choice made, attach to it */
	attach( (*dp)->dmr_name );
}

void
reattach()
{
  char *name;

#if 0
  if(curr_dm_list == &head_dm_list)
    return;

  name = strdup(dmp->dmr_name);
  release(NULL);
  attach(name);
  free((void *)name);

  dmaflag = 1;
#else
  struct rt_vls cmd;

  rt_vls_init(&cmd);
  rt_vls_printf(&cmd, "attach %s %s\n", dmp->dmr_name, dname);
  release(NULL);
  cmdline(&cmd, FALSE);
  rt_vls_free(&cmd);
#endif
}

int
f_attach( argc, argv )
int     argc;
char    **argv;
{
  register struct dm **dp;
  register struct solid *sp;
  register struct dm_list *dmlp;
  register struct dm_list *o_dm_list;

  if(argc == 1){
    rt_log("attach (");
    dp = &which_dm[0];
    rt_log("%s", (*dp++)->dmr_name);
    for( ; *dp != (struct dm *)0; dp++ )
      rt_log("|%s", (*dp)->dmr_name);
    rt_log(")[%s]? ", which_dm[0]->dmr_name);
    
    return CMD_MORE;
  }

  for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
    if( strcmp(argv[1], (*dp)->dmr_name ) == 0 )
      break;

  if(*dp == (struct dm *)0)
    goto Bad;

  /* The Null display manager is already attached */
  if(dp == &which_dm[0])
    return CMD_OK;

#if 0
  if(argc == 2){
    rt_log("X Display: ? ");
    return CMD_MORE;
  }
#endif

  dmlp = (struct dm_list *)rt_malloc(sizeof(struct dm_list), "dm_list");
  bzero((void *)dmlp, sizeof(struct dm_list));
  RT_LIST_APPEND(&head_dm_list.l, &dmlp->l);
  o_dm_list = curr_dm_list;
  curr_dm_list = dmlp;
  dmp = *dp;

  if(argc == 2)
    /* Use local display */
    dm_var_init(o_dm_list, ":0");
  else
    dm_var_init(o_dm_list, argv[2]);

  no_memory = 0;
  if( dmp->dmr_open() )
    goto Bad;

  rt_log("ATTACHING %s (%s)\n",
	 dmp->dmr_name, dmp->dmr_lname);

  FOR_ALL_SOLIDS( sp )  {
    /* Write vector subs into new display processor */
    if( (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
      sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
      if( sp->s_addr == 0 )  break;
      sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes);
    } else {
      sp->s_addr = 0;
      sp->s_bytes = 0;
    }
  }
  dmp->dmr_colorchange();
  color_soltab();
  dmp->dmr_viewchange( DM_CHGV_REDO, SOLID_NULL );
  ++dmaflag;
  return CMD_OK;

Bad:
  rt_log("attach(%s): BAD\n", argv[1]);

  if(*dp != (struct dm *)0)
    release(NULL);

  return CMD_BAD;
}


/*
 *			F _ D M
 *
 *  Run a display manager specific command(s).
 */
int
f_dm(argc, argv)
int	argc;
char	**argv;
{
	if( !dmp->dmr_cmd )  {
		rt_log("The '%s' display manager does not support any local commands.\n", dmp->dmr_name);
		return CMD_BAD;
	}

#if 0
/* mged_cmd already checks for this */
	if( argc-1 < 1 )  {
		rt_log("'dm' command requires an argument.\n");
		return CMD_BAD;
	}
#endif
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
f_tie( argc, argv )
int     argc;
char    **argv;
{
  struct dm_list *p;
  struct dm_list *p1 = (struct dm_list *)NULL;
  struct dm_list *p2 = (struct dm_list *)NULL;

  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    if(p1 == (struct dm_list *)NULL && !strcmp(argv[1], rt_vls_addr(&p->_pathName)))
	p1 = p;
    else if(p2 == (struct dm_list *)NULL && !strcmp(argv[2], rt_vls_addr(&p->_pathName)))
      p2 = p;
    else if(p1 != (struct dm_list *)NULL && p2 != (struct dm_list *)NULL)
      break;
  }

  if(p1 == (struct dm_list *)NULL || p2 == (struct dm_list *)NULL){
    rt_log("Bad pathname(s)\n\tpathName1: %s\t\tpathName2: %s\n", argv[1], argv[2]);
    return CMD_BAD;
  }

  /* free p1's s_info struct if not being used */
  if(!--p1->s_info->_rc)
    rt_free( (char *)p1->s_info, "tie: s_info" );
  /* otherwise if p1's s_info struct is being used and p1 is the owner */
  else if(p1->_owner)
    find_new_owner(p1);

  p1->_owner = 0;

  /* p1 now shares p2's s_info */
  p1->s_info = p2->s_info;

  /* increment the reference count */
  ++p2->s_info->_rc;

  dmaflag = 1;
  return CMD_OK;
}

void
find_new_owner( op )
struct dm_list *op;
{
  struct dm_list *p;

  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    /* first one found is the new owner */
    if(op != p && p->s_info == op->s_info){
      p->_owner = 1;
      return;
    }
  }

    rt_log("find_new_owner: Failed to find a new owner\n");
}


void
dm_var_init(initial_dm_list, name)
struct dm_list *initial_dm_list;
char *name;
{
  curr_dm_list->s_info = (struct shared_info *)rt_malloc(sizeof(struct shared_info),
							    "shared_info");
  bzero((void *)curr_dm_list->s_info, sizeof(struct shared_info));
  strcpy(dname, name);
  mged_variables = default_mged_variables;
  mat_copy( Viewrot, initial_dm_list->s_info->_Viewrot );
  size_reset();
  new_mats();

  MAT_DELTAS_GET(orig_pos, toViewcenter);

  rc = 1;
  dmaflag = 1;
  owner = 1;
}
