/*
 *			C H G M O D E L
 *
 * This module contains functions which change particulars of the
 * model, generally on a single solid or combination.
 * Changes to the tree structure of the model are done in chgtree.c
 *
 * Functions -
 *	f_itemair	add/modify item and air codes of a region
 *	f_mater		modify material information
 *	f_mirror	mirror image
 *	f_edcomb	modify combination record info
 *	f_units		change local units of description
 *	f_title		change current title of description
 *	aexists		announce already exists
 *	f_make		create new solid of given type
 *	f_rot_obj	allow precise changes to object rotation
 *	f_sc_obj	allow precise changes to object scaling
 *	f_tr_obj	allow precise changes to object translation
 *
 *  Author -
 *	Michael John Muuss
 *	Keith A. Applin
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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "externs.h"
#include "nmg.h"
#include "nurb.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"
#include "./cmd.h"

extern struct bn_tol mged_tol;

void set_tran();
void	aexists();

static char	tmpfil[17];
static char	*tmpfil_init = "/tmp/GED.aXXXXXX";

int		newedge;		/* new edge for arb editing */

/* Add/modify item and air codes of a region */
/* Format: item region item <air>	*/
int
f_itemair(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	int			ident, air, GIFTmater=0, los=0;
	int			GIFTmater_set, los_set;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 6 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help item");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( (dp->d_flags & DIR_REGION) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a region\n", (char *)NULL);
	  return TCL_ERROR;
	}

	air = ident = 0;
	GIFTmater_set = los_set = 0;
	ident = atoi( argv[2] );

	/*
	 * If <air> is not included, it is assumed to be zero.
	 * If, on the other hand, either of <GIFTmater> and <los>
	 * is not included, it is left unchanged.
	 */
	if( argc > 3 )  {
		air = atoi( argv[3] );
	}
	if( argc > 4 )  {
		GIFTmater = atoi( argv[4] );
		GIFTmater_set = 1;
	}
	if( argc > 5 )  {
		los = atoi( argv[5] );
		los_set = 1;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	comb->region_id = ident;
	comb->aircode = air;
	if ( GIFTmater_set )  {
		comb->GIFTmater = GIFTmater;
	}
	if ( los_set )  {
		comb->los = los;
	}
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

/* Modify material information */
/* Usage:  mater region_name shader r g b inherit */
int
f_mater(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	int r=0, g=0, b=0;
	int skip_args = 0;
	char inherit;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2 || 8 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help mater");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}
	
	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if( argc >= 3 )  {
	  if( strncmp( argv[2], "del", 3 ) != 0 )  {
		/* Material */
	  	bu_vls_trunc( &comb->shader, 0 );
	  	if( bu_shader_to_tcl_list( argv[2], &comb->shader ))
	  	{
	  		Tcl_AppendResult(interp, "Problem with shader string: ", argv[2], (char *)NULL );
	  		return TCL_ERROR;
	  	}
	  }else{
	  	bu_vls_free( &comb->shader );
	  }
	}else{
	  /* Shader */
	  struct bu_vls tmp_vls;

	  bu_vls_init( &tmp_vls );
	  if( bu_vls_strlen( &comb->shader ) )
	  {
	  	if( bu_shader_to_key_eq( bu_vls_addr(&comb->shader), &tmp_vls ) )
	  	{
	  		Tcl_AppendResult(interp, "Problem with on disk shader string: ", bu_vls_addr(&comb->shader), (char *)NULL );
	  		bu_vls_free( &tmp_vls );
	  		return TCL_ERROR;
	  	}
	  }
	  curr_cmd_list->cl_quote_string = 1;
	  Tcl_AppendResult(interp, "Shader = ", bu_vls_addr(&tmp_vls),
			"\n", MORE_ARGS_STR,
			"Shader?  ('del' to delete, CR to skip) ", (char *)NULL);

	  if( bu_vls_strlen( &comb->shader ) == 0 )
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "del");
	  else
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "\"%S\"", &tmp_vls );

	  bu_vls_free( &tmp_vls );

	  goto fail;
	}

	if(argc >= 4){
	  if( strncmp(argv[3], "del", 3) == 0 ){
	    /* leave color as is */
	  	comb->rgb_valid = 0;
		skip_args = 2;
	  }else if(argc < 6){	/* prompt for color */
	    goto color_prompt;
	  }else{	/* change color */
	    sscanf(argv[3], "%d", &r);
	    sscanf(argv[4], "%d", &g);
	    sscanf(argv[5], "%d", &b);
	    comb->rgb[0] = r;
	    comb->rgb[1] = g;
	    comb->rgb[2] = b;
	    comb->rgb_valid = 1;
	  }
	}else{
	/* Color */
color_prompt:
	  if( comb->rgb_valid ){
	    struct bu_vls tmp_vls;
	    
	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Color = %d %d %d\n",
			  comb->rgb[0], comb->rgb[1], comb->rgb[2] );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    bu_vls_printf(&curr_cmd_list->cl_more_default, "%d %d %d",
			  comb->rgb[0],
			  comb->rgb[1],
			  comb->rgb[2] );
	  }else{
	    Tcl_AppendResult(interp, "Color = (No color specified)\n", (char *)NULL);
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "del");
	  }

	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Color R G B (0..255)? ('del' to delete, CR to skip) ", (char *)NULL);
	  goto fail;
	}

	if(argc >= 7 - skip_args){
	  inherit = *argv[6 - skip_args];
	}else{
	  /* Inherit */
	  switch( comb->inherit )  {
	  case 0:
	    Tcl_AppendResult(interp, "Inherit = 0:  lower nodes (towards leaves) override\n",
			     (char *)NULL);
	    break;
	  default:
	    Tcl_AppendResult(interp, "Inherit = 1:  higher nodes (towards root) override\n",
			     (char *)NULL);
	    break;
	  }

	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Inheritance (0|1)? (CR to skip) ", (char *)NULL);
	  switch( comb->inherit ) {
	  default:
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "1");
	    break;
	  case 0:
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "0");
	    break;
	  }

	  goto fail;
	}

	switch( inherit )  {
	case '1':
		comb->inherit = 1;
		break;
	case '0':
		comb->inherit = 0;
		break;
	case '\0':
	case '\n':
		break;
	default:
	  Tcl_AppendResult(interp, "Unknown response ignored\n", (char *)NULL);
	  break;
	}		

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
fail:
	rt_db_free_internal( &intern, &rt_uniresource );
	return TCL_ERROR;
}

int
f_edmater(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    *argv[];
{
  int i;
  int status;

  char **av;
  
  CHECK_DBI_NULL;
  CHECK_READ_ONLY;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help edmater");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  strcpy(tmpfil, tmpfil_init);
#if 0
  (void)mktemp(tmpfil);
  i=creat(tmpfil, 0600);
#else
  i = mkstemp(tmpfil);
#endif
  if( i < 0 ){
    perror(tmpfil);
    return TCL_ERROR;
  }

  (void)close(i);

  av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edmater: av");
  av[0] = "wmater";
  av[1] = tmpfil;
  for(i = 2; i < argc + 1; ++i)
    av[i] = argv[i-1];

  av[i] = NULL;

  if( f_wmater(clientData, interp, argc + 1, av) == TCL_ERROR ){
    (void)unlink(tmpfil);
    bu_free((genptr_t)av, "f_edmater: av");
    return TCL_ERROR;
  }

  if( editit(tmpfil) ){
    av[0] = "rmater";
    av[2] = NULL;
    status = f_rmater(clientData, interp, 2, av);
  }else
    status = TCL_ERROR;

  (void)unlink(tmpfil);
  bu_free((genptr_t)av, "f_edmater: av");
  return status;
}


int
f_wmater(
	ClientData clientData,
	Tcl_Interp *interp,
	int     argc,
	char    *argv[])
{
  int i;
  int status = TCL_OK;
  FILE *fp;
  register struct directory *dp;
  struct rt_db_internal	intern;
  struct rt_comb_internal	*comb;

  CHECK_DBI_NULL;

  if(argc < 3){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help wmater");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if((fp = fopen(argv[1], "a")) == NULL){
    Tcl_AppendResult(interp, "f_wmater: Failed to open file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  for(i = 2; i < argc; ++i){
    if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) == DIR_NULL ){
      Tcl_AppendResult(interp, "f_wmater: Failed to find ", argv[i], "\n", (char *)NULL);
      status = TCL_ERROR;
      continue;
    }
    if( (dp->d_flags & DIR_COMB) == 0 )  {
      Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
      status = TCL_ERROR;
      continue;
    }
	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR;
		status = TCL_ERROR;
		continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	fprintf(fp, "\"%s\"\t\"%s\"\t%d\t%d\t%d\t%d\t%d\n", argv[i],
		bu_vls_strlen(&comb->shader) > 0 ?
			bu_vls_addr(&comb->shader) : "-",
	  	comb->rgb[0], comb->rgb[1], comb->rgb[2],
	  	comb->rgb_valid, comb->inherit);
	rt_db_free_internal( &intern, &rt_uniresource );
  }

  (void)fclose(fp);
  return status;
}


int
f_rmater(
	ClientData clientData,
	Tcl_Interp *interp,
	int     argc,
	char    *argv[])
{
#ifndef LINELEN
#define LINELEN 256
#endif
  int status = TCL_OK;
  FILE *fp;
  register struct directory *dp;
  struct rt_db_internal	intern;
  struct rt_comb_internal	*comb;
  char line[LINELEN];
  char name[128];
  char shader[256]; 
  int r,g,b;
  int override;
  int inherit;

  CHECK_DBI_NULL;
  CHECK_READ_ONLY;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help rmater");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if((fp = fopen(argv[1], "r")) == NULL){
    Tcl_AppendResult(interp, "f_rcodes: Failed to read file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  while(fgets( line , LINELEN, fp ) != NULL){
	if((extract_mater_from_line(line, name, shader,
			    &r, &g, &b, &override, &inherit)) == TCL_ERROR)
	continue;

	if( (dp = db_lookup( dbip,  name, LOOKUP_NOISY )) == DIR_NULL ){
		Tcl_AppendResult(interp, "f_rmater: Failed to find ", name, "\n", (char *)NULL);
		status = TCL_ERROR;
		continue;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR;
		status = TCL_ERROR;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	/* Assign new values */
	if(shader[0] == '-')
  		bu_vls_free( &comb->shader );
  	else
  		bu_vls_strcpy( &comb->shader, shader );

  	comb->rgb[0] = (unsigned char)r;
  	comb->rgb[1] = (unsigned char)g;
  	comb->rgb[2] = (unsigned char)b;
  	comb->rgb_valid = override;
  	comb->inherit = inherit;

	/* Write new values to database */
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR;
		status = TCL_ERROR;
	}
  }

  (void)fclose(fp);
  return status;
}

int
extract_mater_from_line(
	char *line,
	char *name,
	char *shader,
	int *r, int *g, int *b,
	int *override,
	int *inherit)
{
  int i,j,k;
  char *str[3];

  str[0] = name;
  str[1] = shader;

  /* Extract first 2 strings. */
  for(i=j=0; i < 2; ++i){

    /* skip white space */
    while(line[j] == ' ' || line[j] == '\t')
      ++j;

    if(line[j] == '\0')
      return TCL_ERROR;

    /* We found a double quote, so use everything between the quotes */
    if(line[j] == '"'){
      for(k = 0, ++j; line[j] != '"' && line[j] != '\0'; ++j, ++k)
	str[i][k] = line[j];
    }else{
      for(k = 0; line[j] != ' ' && line[j] != '\t' && line[j] != '\0'; ++j, ++k)
	str[i][k] = line[j];
    }

    if(line[j] == '\0')
      return TCL_ERROR;

    str[i][k] = '\0';
    ++j;
  }

  if((sscanf(line + j, "%d%d%d%d%d", r, g, b, override, inherit)) != 5)
    return TCL_ERROR;

  return TCL_OK;
}


/*
 *			F _ C O M B _ C O L O R
 *
 *  Simple command-line way to set object color
 *  Usage: ocolor combination R G B
 */
int
f_comb_color(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
    int				i;
    int				val;
    register struct directory	*dp;
    struct rt_db_internal	intern;
    struct rt_comb_internal	*comb;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 5 || 5 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help comb_color");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if ((dp = db_lookup(dbip,  argv[1], LOOKUP_NOISY)) == DIR_NULL)
      return TCL_ERROR;
    if( (dp->d_flags & DIR_COMB) == 0 )  {
      Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
      return TCL_ERROR;
    }

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

    for (i = 0; i < 3; ++i)  {
	if (((val = atoi(argv[i + 2])) < 0) || (val > 255))
	{
	  Tcl_AppendResult(interp, "RGB value out of range: ", argv[i + 2],
			   "\n", (char *)NULL);
	  rt_db_free_internal( &intern, &rt_uniresource );
	  return TCL_ERROR;
	}
	else
	    comb->rgb[i] = val;
    }

	comb->rgb_valid = 1;
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

/*
 *			F _ S H A D E R
 *
 *  Simpler, command-line version of 'mater' command.
 *  Usage: shader combination shader_material [shader_argument(s)]
 */
int
f_shader(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help shader");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if(argc == 2)  {
		/* Return the current shader string */
		Tcl_AppendResult( interp, bu_vls_addr(&comb->shader), (char *)NULL);
		rt_db_free_internal( &intern, &rt_uniresource );
	} else {
		CHECK_READ_ONLY;

		/* Replace with new shader string from command line */
		bu_vls_free( &comb->shader );

		/* Bunch up the rest of the args, space separated */
		bu_vls_from_argv( &comb->shader, argc-2, argv+2 );

		if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
			TCL_WRITE_ERR_return;
		}
		/* Internal representation has been freed by rt_db_put_internal */
	}
	return TCL_OK;
}


/* Mirror image */
/* Format: m oldobject newobject axis	*/
int
f_mirror(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *proto;
	register struct directory *dp;
	register int i, j, k;
	struct rt_db_internal	internal;
	int			id;
	mat_t mirmat;
	mat_t temp;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help mirror");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	k = -1;
	if( strcmp( argv[3], "x" ) == 0 )
		k = 0;
	if( strcmp( argv[3], "y" ) == 0 )
		k = 1;
	if( strcmp( argv[3], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
	  Tcl_AppendResult(interp, "axis must be x, y or z\n", (char *)NULL);
	  return TCL_ERROR;
	}

	id = rt_db_get_internal( &internal, proto, dbip, NULL, &rt_uniresource );
	if( id < 0 )  {
	  Tcl_AppendResult(interp, "f_mirror(", argv[1], argv[2],
		   "):  solid import failure\n", (char *)NULL);
	  return TCL_ERROR;				/* FAIL */
	}
	RT_CK_DB_INTERNAL( &internal );

	/* Build mirror transform matrix, for those who need it. */
	MAT_IDN( mirmat );
	mirmat[k*5] = -1.0;

	switch( id )
	{
		case ID_TOR:
		{
			struct rt_tor_internal *tor;

			tor = (struct rt_tor_internal *)internal.idb_ptr;
			RT_TOR_CK_MAGIC( tor );

			tor->v[k] *= -1.0;
			tor->h[k] *= -1.0;

			break;
		}
		case ID_TGC:
		case ID_REC:
		{
			struct rt_tgc_internal *tgc;

			tgc = (struct rt_tgc_internal *)internal.idb_ptr;
			RT_TGC_CK_MAGIC( tgc );

			tgc->v[k] *= -1.0;
			tgc->h[k] *= -1.0;
			tgc->a[k] *= -1.0;
			tgc->b[k] *= -1.0;
			tgc->c[k] *= -1.0;
			tgc->d[k] *= -1.0;

			break;
		}
		case ID_ELL:
		case ID_SPH:
		{
			struct rt_ell_internal *ell;

			ell = (struct rt_ell_internal *)internal.idb_ptr;
			RT_ELL_CK_MAGIC( ell );

			ell->v[k] *= -1.0;
			ell->a[k] *= -1.0;
			ell->b[k] *= -1.0;
			ell->c[k] *= -1.0;

			break;
		}
		case ID_ARB8:
		{
			struct rt_arb_internal *arb;

			arb = (struct rt_arb_internal *)internal.idb_ptr;
			RT_ARB_CK_MAGIC( arb );

			for( i=0 ; i<8 ; i++ )
				arb->pt[i][k] *= -1.0;
			break;
		}
		case ID_HALF:
		{
			struct rt_half_internal *haf;

			haf = (struct rt_half_internal *)internal.idb_ptr;
                                RT_HALF_CK_MAGIC( haf );

			haf->eqn[k] *= -1.0;

			break;
		}
		case ID_GRIP:
		{
			struct rt_grip_internal *grp;

			grp = (struct rt_grip_internal *)internal.idb_ptr;
			RT_GRIP_CK_MAGIC( grp );

			grp->center[k] *= -1.0;
			grp->normal[k] *= -1.0;

			break;
		}
		case ID_POLY:
		{
			struct rt_pg_internal *pg;
			fastf_t *verts;
			fastf_t *norms;

			pg = (struct rt_pg_internal *)internal.idb_ptr;
			RT_PG_CK_MAGIC( pg );

			verts = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "f_mirror: verts" );
			norms = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "f_mirror: norms" );

			for( i=0 ; i<pg->npoly ; i++ )
			{
				int last;

				last = (pg->poly[i].npts - 1)*3;
				/* mirror coords and temporarily store in reverse order */
				for( j=0 ; j<pg->poly[i].npts*3 ; j += 3 )
				{
					pg->poly[i].verts[j+k] *= -1.0;
					VMOVE( &verts[last-j], &pg->poly[i].verts[j] )
					pg->poly[i].norms[j+k] *= -1.0;
					VMOVE( &norms[last-j], &pg->poly[i].norms[j] )
				}

				/* write back mirrored and reversed face loop */
				for( j=0 ; j<pg->poly[i].npts*3 ; j += 3 )
				{
					VMOVE( &pg->poly[i].norms[j], &norms[j] )
					VMOVE( &pg->poly[i].verts[j], &verts[j] )
				}
			}

			bu_free( (char *)verts, "f_mirror: verts" );
			bu_free( (char *)norms, "f_mirror: norms" );

			break;
		}
		case ID_BSPLINE:
		{
			struct rt_nurb_internal *nurb;

			nurb = (struct rt_nurb_internal *)internal.idb_ptr;
			RT_NURB_CK_MAGIC( nurb );

			for( i=0 ; i<nurb->nsrf ; i++ )
			{
				fastf_t *ptr;
				int tmp;
				int orig_size[2];
				int ncoords;
				int m;
				int l;

				/* swap knot vetcors between u and v */
				ptr = nurb->srfs[i]->u.knots;
				tmp = nurb->srfs[i]->u.k_size;

				nurb->srfs[i]->u.knots = nurb->srfs[i]->v.knots;
				nurb->srfs[i]->u.k_size = nurb->srfs[i]->v.k_size;
				nurb->srfs[i]->v.knots = ptr;
				nurb->srfs[i]->v.k_size = tmp;

				/* swap order */
				tmp = nurb->srfs[i]->order[0];
				nurb->srfs[i]->order[0] = nurb->srfs[i]->order[1];
				nurb->srfs[i]->order[1] = tmp;

				/* swap mesh size */
				orig_size[0] = nurb->srfs[i]->s_size[0];
				orig_size[1] = nurb->srfs[i]->s_size[1];

				nurb->srfs[i]->s_size[0] = orig_size[1];
				nurb->srfs[i]->s_size[1] = orig_size[0];

				/* allocat memory for a new control mesh */
				ncoords = RT_NURB_EXTRACT_COORDS( nurb->srfs[i]->pt_type );
				ptr = (fastf_t *)bu_calloc( orig_size[0]*orig_size[1]*ncoords, sizeof( fastf_t ), "f_mirror: ctl mesh ptr" );

				/* mirror each control point */
				for( j=0 ; j<orig_size[0]*orig_size[1] ; j++ )
					nurb->srfs[i]->ctl_points[j*ncoords+k] *= -1.0;

				/* copy mirrored control points into new mesh
				 * while swaping u and v */
				m = 0;
				for( j=0 ; j<orig_size[0] ; j++ )
				{
					for( l=0 ; l<orig_size[1] ; l++ )
					{
						VMOVEN( &ptr[(l*orig_size[0]+j)*ncoords], &nurb->srfs[i]->ctl_points[m*ncoords], ncoords )
						m++;
					}
				}

				/* free old mesh */
				bu_free( (char *)nurb->srfs[i]->ctl_points , "f_mirror: ctl points" );

				/* put new mesh in place */
				nurb->srfs[i]->ctl_points = ptr;
			}

			break;
		}
		case ID_ARBN:
		{
			struct rt_arbn_internal *arbn;

			arbn = (struct rt_arbn_internal *)internal.idb_ptr;
			RT_ARBN_CK_MAGIC( arbn );

			for( i=0 ; i<arbn->neqn ; i++ )
				arbn->eqn[i][k] *= -1.0;

			break;
		}
		case ID_PIPE:
		{
			struct rt_pipe_internal *pipe;
			struct wdb_pipept *ps;

			pipe = (struct rt_pipe_internal *)internal.idb_ptr;
			RT_PIPE_CK_MAGIC( pipe );

			for( BU_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
				ps->pp_coord[k] *= -1.0;

			break;
		}
		case ID_PARTICLE:
		{
			struct rt_part_internal *part;

			part = (struct rt_part_internal *)internal.idb_ptr;
			RT_PART_CK_MAGIC( part );

			part->part_V[k] *= -1.0;
			part->part_H[k] *= -1.0;

			break;
		}
		case ID_RPC:
		{
			struct rt_rpc_internal *rpc;

			rpc = (struct rt_rpc_internal *)internal.idb_ptr;
			RT_RPC_CK_MAGIC( rpc );

			rpc->rpc_V[k] *= -1.0;
			rpc->rpc_H[k] *= -1.0;
			rpc->rpc_B[k] *= -1.0;

			break;
		}
		case ID_RHC:
		{
			struct rt_rhc_internal *rhc;

			rhc = (struct rt_rhc_internal *)internal.idb_ptr;
			RT_RHC_CK_MAGIC( rhc );

			rhc->rhc_V[k] *= -1.0;
			rhc->rhc_H[k] *= -1.0;
			rhc->rhc_B[k] *= -1.0;

			break;
		}
		case ID_EPA:
		{
			struct rt_epa_internal *epa;

			epa = (struct rt_epa_internal *)internal.idb_ptr;
			RT_EPA_CK_MAGIC( epa );

			epa->epa_V[k] *= -1.0;
			epa->epa_H[k] *= -1.0;
			epa->epa_Au[k] *= -1.0;

			break;
		}
		case ID_ETO:
		{
			struct rt_eto_internal *eto;

			eto = (struct rt_eto_internal *)internal.idb_ptr;
			RT_ETO_CK_MAGIC( eto );

			eto->eto_V[k] *= -1.0;
			eto->eto_N[k] *= -1.0;
			eto->eto_C[k] *= -1.0;

			break;
		}
		case ID_NMG:
		{
			struct model *m;
			struct nmgregion *r;
			struct shell *s;
			struct bu_ptbl table;
			struct vertex *v;

			m = (struct model *)internal.idb_ptr;
			NMG_CK_MODEL( m );

			/* move every vertex */
			nmg_vertex_tabulate( &table, &m->magic );
			for( i=0 ; i<BU_PTBL_END( &table ) ; i++ )
			{
				v = (struct vertex *)BU_PTBL_GET( &table, i );
				NMG_CK_VERTEX( v );

				v->vg_p->coord[k] *= -1.0;
			}

			bu_ptbl_reset( &table );

			nmg_face_tabulate( &table, &m->magic );
			for( i=0 ; i<BU_PTBL_END( &table ) ; i++ )
			{
				struct face *f;

				f = (struct face *)BU_PTBL_GET( &table, i );
				NMG_CK_FACE( f );

				if( !f->g.magic_p )
					continue;

				if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
				{
					Tcl_AppendResult(interp, "Sorry, Can only mirror NMG solids with planar faces", (char *)0 );
					bu_ptbl_free( &table );
					rt_db_free_internal( &internal, &rt_uniresource );
					return TCL_ERROR;
				}

				
			}

			for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
				for( BU_LIST_FOR( s, shell, &r->s_hd ) )
					nmg_invert_shell( s, &mged_tol );


			for( i=0 ; i<BU_PTBL_END( &table ) ; i++ )
			{
				struct face *f;
				struct faceuse *fu;

				f = (struct face *)BU_PTBL_GET( &table, i );
				NMG_CK_FACE( f );

				fu = f->fu_p;
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;
				if( fu->orientation != OT_SAME )
				{
					Tcl_AppendResult(interp, "nmg_calc_face_g() failed", (char *)0 );
					bu_ptbl_free( &table );
					rt_db_free_internal( &internal, &rt_uniresource );
					return TCL_ERROR;
				}

				if( nmg_calc_face_g( fu ) )
				{
					Tcl_AppendResult(interp, "nmg_calc_face_g() failed", (char *)0 );
					bu_ptbl_free( &table );
					rt_db_free_internal( &internal, &rt_uniresource );
					return TCL_ERROR;
				}
			}

			bu_ptbl_free( &table );
			nmg_rebound( m, &mged_tol );

			break;
		}
		case ID_ARS:
		{
			struct rt_ars_internal *ars;
			fastf_t *tmp_curve;

			ars = (struct rt_ars_internal *)internal.idb_ptr;
			RT_ARS_CK_MAGIC( ars );

			/* mirror each vertex */
			for( i=0 ; i<ars->ncurves ; i++ )
			{
				for( j=0 ; j<ars->pts_per_curve ; j++ )
					ars->curves[i][j*3+k] *= -1.0;
			}

			/* now reverse order of vertices in each curve */
			tmp_curve = (fastf_t *)bu_calloc( 3*ars->pts_per_curve, sizeof( fastf_t ), "f_mirror: tmp_curve" );
			for( i=0 ; i<ars->ncurves ; i++ )
			{
				/* reverse vertex order */
				for( j=0 ; j<ars->pts_per_curve ; j++ )
					VMOVE( &tmp_curve[(ars->pts_per_curve-j-1)*3], &ars->curves[i][j*3] )

				/* now copy back */
				bcopy( tmp_curve, ars->curves[i], ars->pts_per_curve*3*sizeof( fastf_t ) );
			}

			bu_free( (char *)tmp_curve, "f_mirror: tmp_curve" );

			break;
		}
		case ID_EBM:
		{
			struct rt_ebm_internal *ebm;

			ebm = (struct rt_ebm_internal *)internal.idb_ptr;
			RT_EBM_CK_MAGIC( ebm );

			bn_mat_mul( temp, mirmat, ebm->mat );
			MAT_COPY( ebm->mat, temp );

			break;
		}
		case ID_DSP:
		{
			struct rt_dsp_internal *dsp;
			
			dsp = (struct rt_dsp_internal *)internal.idb_ptr;
			RT_DSP_CK_MAGIC( dsp );
			
			bn_mat_mul( temp, mirmat, dsp->dsp_mtos);
			MAT_COPY( dsp->dsp_mtos, temp);
			
			break;
		}
		case ID_VOL:
		{
			struct rt_vol_internal *vol;

			vol = (struct rt_vol_internal *)internal.idb_ptr;
			RT_VOL_CK_MAGIC( vol );

			bn_mat_mul( temp, mirmat, vol->mat );
			MAT_COPY( vol->mat, temp );

			break;
		}
		case ID_COMBINATION:
		{
			struct rt_comb_internal	*comb;

			comb = (struct rt_comb_internal *)internal.idb_ptr;
			RT_CK_COMB(comb);

			if( comb->tree )
				db_tree_mul_dbleaf( comb->tree, mirmat );
			break;
		}
		default:
		{
			rt_db_free_internal( &internal, &rt_uniresource );
			Tcl_AppendResult(interp, "Cannot mirror this solid type\n", (char *)NULL);
			return TCL_ERROR;
		}
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, argv[2], -1L, 0, proto->d_flags, (genptr_t)&internal.idb_type)) == DIR_NULL )  {
	    	TCL_ALLOC_ERR_return;
	}
	if( rt_db_put_internal( dp, dbip, &internal, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}

	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = argv[2]; /* depends on solid name being in argv[2] */
	  av[2] = NULL;

	  return cmd_draw( clientData, interp, 2, av );
	}
}

/* Modify Combination record information */
/* Format: edcomb combname Regionflag regionid air los GIFTmater */
int
f_edcomb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	int regionid, air, mat, los;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 6 || 7 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help edcomb");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	regionid = atoi( argv[3] );
	air = atoi( argv[4] );
	los = atoi( argv[5] );
	mat = atoi( argv[6] );

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if( argv[2][0] == 'R' )
		comb->region_flag = 1;
	else
		comb->region_flag = 0;
	comb->region_id = regionid;
	comb->aircode = air;
	comb->los = los;
	comb->GIFTmater = mat;
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

/* tell him it already exists */
void
aexists( name )
char	*name;
{
  Tcl_AppendResult(interp, name, ":  already exists\n", (char *)NULL);
}

/*
 *  			F _ M A K E
 *  
 *  Create a new solid of a given type
 *  (Generic, or explicit)
 */
int
f_make(ClientData	clientData,
       Tcl_Interp	*interp,
       int		argc,
       char		**argv)
{
	register struct directory *dp;
	int i;
	struct rt_db_internal	internal;
	struct rt_arb_internal	*arb_ip;
	struct rt_ars_internal	*ars_ip;
	struct rt_tgc_internal	*tgc_ip;
	struct rt_ell_internal	*ell_ip;
	struct rt_tor_internal	*tor_ip;
	struct rt_grip_internal	*grp_ip;
	struct rt_half_internal *half_ip;
	struct rt_rpc_internal *rpc_ip;
	struct rt_rhc_internal *rhc_ip;
	struct rt_epa_internal *epa_ip;
	struct rt_ehy_internal *ehy_ip;
	struct rt_eto_internal *eto_ip;
	struct rt_part_internal *part_ip;
	struct rt_pipe_internal *pipe_ip;
	struct rt_sketch_internal *sketch_ip;
	struct rt_extrude_internal *extrude_ip;
	struct rt_bot_internal *bot_ip;
	struct rt_arbn_internal *arbn_ip;

	if(argc == 2){
	  struct bu_vls vls;

	  if(argv[1][0] == '-' && argv[1][1] == 't'){
	    Tcl_AppendElement(interp, "arb8");
	    Tcl_AppendElement(interp, "arb7");
	    Tcl_AppendElement(interp, "arb6");
	    Tcl_AppendElement(interp, "arb5");
	    Tcl_AppendElement(interp, "arb4");
	    Tcl_AppendElement(interp, "arbn");
	    Tcl_AppendElement(interp, "ars");
	    Tcl_AppendElement(interp, "bot");
	    Tcl_AppendElement(interp, "ehy");
	    Tcl_AppendElement(interp, "ell");
	    Tcl_AppendElement(interp, "ell1");
	    Tcl_AppendElement(interp, "epa");
	    Tcl_AppendElement(interp, "eto");
	    Tcl_AppendElement(interp, "extrude");
	    Tcl_AppendElement(interp, "grip");
	    Tcl_AppendElement(interp, "half");
	    Tcl_AppendElement(interp, "nmg");
	    Tcl_AppendElement(interp, "part");
	    Tcl_AppendElement(interp, "pipe");
	    Tcl_AppendElement(interp, "rcc");
	    Tcl_AppendElement(interp, "rec");
	    Tcl_AppendElement(interp, "rhc");
	    Tcl_AppendElement(interp, "rpc");
	    Tcl_AppendElement(interp, "rpp");
	    Tcl_AppendElement(interp, "sketch");
	    Tcl_AppendElement(interp, "sph");
	    Tcl_AppendElement(interp, "tec");
	    Tcl_AppendElement(interp, "tgc");
	    Tcl_AppendElement(interp, "tor");
	    Tcl_AppendElement(interp, "trc");

	    return TCL_OK;
	  }

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help make");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc != 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help make");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[1] );
	  return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL( &internal );

	/* make name <arb8 | arb7 | arb6 | arb5 | arb4 | ellg | ell |
	 * sph | tor | tgc | rec | trc | rcc | grp | half | nmg | bot | sketch | extrude> */
	if (strcmp(argv[2], "arb8") == 0 ||
	    strcmp(argv[2],  "rpp") == 0)  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARB8;
		internal.idb_meth = &rt_functab[ID_ARB8];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-view_state->vs_vop->vo_center[MDX] +view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] -view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] -view_state->vs_vop->vo_scale );
		for( i=1 ; i<8 ; i++ )			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Z] += view_state->vs_vop->vo_scale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[5][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[6][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[6][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[7][Z] += view_state->vs_vop->vo_scale*2.0;
	} else if( strcmp( argv[2], "arb7" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARB8;
		internal.idb_meth = &rt_functab[ID_ARB8];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-view_state->vs_vop->vo_center[MDX] +view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] -view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] -(0.5*view_state->vs_vop->vo_scale) );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Z] += view_state->vs_vop->vo_scale;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[5][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[6][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[6][Z] += view_state->vs_vop->vo_scale;
	} else if( strcmp( argv[2], "arb6" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARB8;
		internal.idb_meth = &rt_functab[ID_ARB8];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0],
			-view_state->vs_vop->vo_center[MDX] +view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] -view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] -view_state->vs_vop->vo_scale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Z] += view_state->vs_vop->vo_scale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[4][Y] += view_state->vs_vop->vo_scale;
		arb_ip->pt[5][Y] += view_state->vs_vop->vo_scale;
		arb_ip->pt[6][Y] += view_state->vs_vop->vo_scale;
		arb_ip->pt[6][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[7][Y] += view_state->vs_vop->vo_scale;
		arb_ip->pt[7][Z] += view_state->vs_vop->vo_scale*2.0;
	} else if( strcmp( argv[2], "arb5" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARB8;
		internal.idb_meth = &rt_functab[ID_ARB8];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-view_state->vs_vop->vo_center[MDX] +view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] -view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] -view_state->vs_vop->vo_scale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Z] += view_state->vs_vop->vo_scale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= view_state->vs_vop->vo_scale*2.0;
			arb_ip->pt[i][Y] += view_state->vs_vop->vo_scale;
			arb_ip->pt[i][Z] += view_state->vs_vop->vo_scale;
		}
	} else if( strcmp( argv[2], "arb4" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARB8;
		internal.idb_meth = &rt_functab[ID_ARB8];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-view_state->vs_vop->vo_center[MDX] +view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] -view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] -view_state->vs_vop->vo_scale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[2][Z] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Y] += view_state->vs_vop->vo_scale*2.0;
		arb_ip->pt[3][Z] += view_state->vs_vop->vo_scale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= view_state->vs_vop->vo_scale*2.0;
			arb_ip->pt[i][Y] += view_state->vs_vop->vo_scale*2.0;
		}
	} else if( strcmp( argv[2], "arbn") == 0 ) {
		point_t view_center;

		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARBN;
		internal.idb_meth = &rt_functab[ID_ARBN];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_arbn_internal), "rt_arbn_internal" );
		arbn_ip = (struct rt_arbn_internal *)internal.idb_ptr;
		arbn_ip->magic = RT_ARBN_INTERNAL_MAGIC;
		arbn_ip->neqn = 8;
		arbn_ip->eqn = (plane_t *)bu_calloc( arbn_ip->neqn,
			     sizeof( plane_t ), "arbn plane eqns" );
		VSET( arbn_ip->eqn[0], 1, 0, 0 );
		arbn_ip->eqn[0][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[1], -1, 0, 0 );
		arbn_ip->eqn[1][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[2], 0, 1, 0 );
		arbn_ip->eqn[2][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[3], 0, -1, 0 );
		arbn_ip->eqn[3][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[4], 0, 0, 1 );
		arbn_ip->eqn[4][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[5], 0, 0, -1 );
		arbn_ip->eqn[5][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[6], 0.57735, 0.57735, 0.57735 );
		arbn_ip->eqn[6][3] = view_state->vs_vop->vo_scale;
		VSET( arbn_ip->eqn[7], -0.57735, -0.57735, -0.57735 );
		arbn_ip->eqn[7][3] = view_state->vs_vop->vo_scale;
		VSET( view_center, 
			-view_state->vs_vop->vo_center[MDX],
			-view_state->vs_vop->vo_center[MDY],
		        -view_state->vs_vop->vo_center[MDZ] );
		for( i=0 ; i<arbn_ip->neqn ; i++ ) {
			arbn_ip->eqn[i][3] +=
				VDOT( view_center, arbn_ip->eqn[i] );
		}
	} else if( strcmp( argv[2], "ars" ) == 0 )  {
	        int curve;
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ARS;
		internal.idb_meth = &rt_functab[ID_ARS];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ars_internal) , "rt_ars_internal" );
		ars_ip = (struct rt_ars_internal *)internal.idb_ptr;
		ars_ip->magic = RT_ARS_INTERNAL_MAGIC;
		ars_ip->ncurves = 3;
		ars_ip->pts_per_curve = 3;
		ars_ip->curves = (fastf_t **)bu_malloc((ars_ip->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs" );
		
		for ( curve=0 ; curve < ars_ip->ncurves ; curve++ ) {
		    ars_ip->curves[curve] = (fastf_t *)bu_calloc(
			     (ars_ip->pts_per_curve + 1) * 3,
			     sizeof(fastf_t), "ARS points");

		    if (curve == 0) {
			VSET( &(ars_ip->curves[0][0]),
			      -view_state->vs_vop->vo_center[MDX],
			      -view_state->vs_vop->vo_center[MDY],
			      -view_state->vs_vop->vo_center[MDZ] );
			VMOVE(&(ars_ip->curves[curve][3]), &(ars_ip->curves[curve][0]));
			VMOVE(&(ars_ip->curves[curve][6]), &(ars_ip->curves[curve][0]));
		    } else if (curve == (ars_ip->ncurves - 1) ) {
			VSET( &(ars_ip->curves[curve][0]),
			      -view_state->vs_vop->vo_center[MDX],
			      -view_state->vs_vop->vo_center[MDY],
			      -view_state->vs_vop->vo_center[MDZ]+curve*(0.25*view_state->vs_vop->vo_scale));
			VMOVE(&(ars_ip->curves[curve][3]), &(ars_ip->curves[curve][0]));
			VMOVE(&(ars_ip->curves[curve][6]), &(ars_ip->curves[curve][0]));

		    } else {
			fastf_t x, y, z;
			x = -view_state->vs_vop->vo_center[MDX]+curve*(0.25*view_state->vs_vop->vo_scale);
			y = -view_state->vs_vop->vo_center[MDY]+curve*(0.25*view_state->vs_vop->vo_scale);
			z = -view_state->vs_vop->vo_center[MDZ]+curve*(0.25*view_state->vs_vop->vo_scale);

			VSET(&ars_ip->curves[curve][0], 
			      -view_state->vs_vop->vo_center[MDX],
			      -view_state->vs_vop->vo_center[MDY],
			     z);
			VSET(&ars_ip->curves[curve][3], 
			     x,
			      -view_state->vs_vop->vo_center[MDY],
			     z);
			VSET(&ars_ip->curves[curve][6], 
			     x, y, z);
		    }
		}

	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ELL;
		internal.idb_meth = &rt_functab[ID_ELL];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ] );
		VSET( ell_ip->a, (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );	/* A */
		VSET( ell_ip->b, 0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );	/* B */
		VSET( ell_ip->c, 0.0, 0.0, (0.5*view_state->vs_vop->vo_scale) );	/* C */
	} else if(( strcmp( argv[2], "grp" ) == 0 ) ||
		  ( strcmp( argv[2], "grip") == 0 )) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_GRIP;
		internal.idb_meth = &rt_functab[ID_GRIP];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_grip_internal), "rt_grp_internal" );
		grp_ip = (struct rt_grip_internal *) internal.idb_ptr;
		grp_ip->magic = RT_GRIP_INTERNAL_MAGIC;
		VSET( grp_ip->center, -view_state->vs_vop->vo_center[MDX], -view_state->vs_vop->vo_center[MDY],
		    -view_state->vs_vop->vo_center[MDZ]);
		VSET( grp_ip->normal, 1.0, 0.0, 0.0);
		grp_ip->mag = view_state->vs_vop->vo_scale*0.75;
	} else if( strcmp( argv[2], "ell1" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ELL;
		internal.idb_meth = &rt_functab[ID_ELL];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ] );
		VSET( ell_ip->a, (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );	/* A */
		VSET( ell_ip->b, 0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );	/* B */
		VSET( ell_ip->c, 0.0, 0.0, (0.25*view_state->vs_vop->vo_scale) );	/* C */
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ELL;
		internal.idb_meth = &rt_functab[ID_ELL];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ] );
		VSET( ell_ip->a, view_state->vs_vop->vo_scale, 0.0, 0.0 );		/* A */
		VSET( ell_ip->b, 0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );	/* B */
		VSET( ell_ip->c, 0.0, 0.0, (0.25*view_state->vs_vop->vo_scale) );	/* C */
	} else if( strcmp( argv[2], "tor" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TOR;
		internal.idb_meth = &rt_functab[ID_TOR];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tor_internal) , "rt_tor_internal" );
		tor_ip = (struct rt_tor_internal *)internal.idb_ptr;
		tor_ip->magic = RT_TOR_INTERNAL_MAGIC;
		VSET( tor_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ] );
		VSET( tor_ip->h , 1.0 , 0.0 , 0.0 );	/* unit normal */
		tor_ip->r_h = 0.5*view_state->vs_vop->vo_scale;
		tor_ip->r_a = view_state->vs_vop->vo_scale;
		tor_ip->r_b = view_state->vs_vop->vo_scale;
		VSET( tor_ip->a , 0.0 , view_state->vs_vop->vo_scale , 0.0 );
		VSET( tor_ip->b , 0.0 , 0.0 , view_state->vs_vop->vo_scale );
	} else if( strcmp( argv[2], "tgc" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TGC;
		internal.idb_meth = &rt_functab[ID_TGC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( tgc_ip->h,  0.0, 0.0, (view_state->vs_vop->vo_scale*2) );
		VSET( tgc_ip->a,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->b,  0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );
		VSET( tgc_ip->c,  (0.25*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->d,  0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );
	} else if( strcmp( argv[2], "tec" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TGC;
		internal.idb_meth = &rt_functab[ID_TGC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( tgc_ip->h,  0.0, 0.0, (view_state->vs_vop->vo_scale*2) );
		VSET( tgc_ip->a,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->b,  0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );
		VSET( tgc_ip->c,  (0.25*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->d,  0.0, (0.125*view_state->vs_vop->vo_scale), 0.0 );
	} else if( strcmp( argv[2], "rec" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TGC;
		internal.idb_meth = &rt_functab[ID_TGC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( tgc_ip->h,  0.0, 0.0, (view_state->vs_vop->vo_scale*2) );
		VSET( tgc_ip->a,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->b,  0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );
		VSET( tgc_ip->c,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->d,  0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );
	} else if( strcmp( argv[2], "trc" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TGC;
		internal.idb_meth = &rt_functab[ID_TGC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( tgc_ip->h,  0.0, 0.0, (view_state->vs_vop->vo_scale*2) );
		VSET( tgc_ip->a,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->b,  0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );
		VSET( tgc_ip->c,  (0.25*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->d,  0.0, (0.25*view_state->vs_vop->vo_scale), 0.0 );
	} else if( strcmp( argv[2], "rcc" ) == 0 )  {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_TGC;
		internal.idb_meth = &rt_functab[ID_TGC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( tgc_ip->h,  0.0, 0.0, (view_state->vs_vop->vo_scale*2) );
		VSET( tgc_ip->a,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->b,  0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );
		VSET( tgc_ip->c,  (0.5*view_state->vs_vop->vo_scale), 0.0, 0.0 );
		VSET( tgc_ip->d,  0.0, (0.5*view_state->vs_vop->vo_scale), 0.0 );
	} else if( strcmp( argv[2], "half" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_HALF;
		internal.idb_meth = &rt_functab[ID_HALF];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_half_internal) , "rt_half_internal" );
		half_ip = (struct rt_half_internal *)internal.idb_ptr;
		half_ip->magic = RT_HALF_INTERNAL_MAGIC;
		VSET( half_ip->eqn , 0.0 , 0.0 , 1.0 );
		half_ip->eqn[3] = (-view_state->vs_vop->vo_center[MDZ]);
	} else if( strcmp( argv[2], "rpc" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_RPC;
		internal.idb_meth = &rt_functab[ID_RPC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rpc_internal) , "rt_rpc_internal" );
		rpc_ip = (struct rt_rpc_internal *)internal.idb_ptr;
		rpc_ip->rpc_magic = RT_RPC_INTERNAL_MAGIC;
		VSET( rpc_ip->rpc_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( rpc_ip->rpc_H, 0.0, 0.0, view_state->vs_vop->vo_scale );
		VSET( rpc_ip->rpc_B, 0.0, (view_state->vs_vop->vo_scale*0.5), 0.0 );
		rpc_ip->rpc_r = view_state->vs_vop->vo_scale*0.25;
	} else if( strcmp( argv[2], "rhc" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_RHC;
		internal.idb_meth = &rt_functab[ID_RHC];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rhc_internal) , "rt_rhc_internal" );
		rhc_ip = (struct rt_rhc_internal *)internal.idb_ptr;
		rhc_ip->rhc_magic = RT_RHC_INTERNAL_MAGIC;
		VSET( rhc_ip->rhc_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( rhc_ip->rhc_H, 0.0, 0.0, view_state->vs_vop->vo_scale );
		VSET( rhc_ip->rhc_B, 0.0, (view_state->vs_vop->vo_scale*0.5), 0.0 );
		rhc_ip->rhc_r = view_state->vs_vop->vo_scale*0.25;
		rhc_ip->rhc_c = view_state->vs_vop->vo_scale*0.10;
	} else if( strcmp( argv[2], "epa" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_EPA;
		internal.idb_meth = &rt_functab[ID_EPA];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_epa_internal) , "rt_epa_internal" );
		epa_ip = (struct rt_epa_internal *)internal.idb_ptr;
		epa_ip->epa_magic = RT_EPA_INTERNAL_MAGIC;
		VSET( epa_ip->epa_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( epa_ip->epa_H, 0.0, 0.0, view_state->vs_vop->vo_scale );
		VSET( epa_ip->epa_Au, 0.0, 1.0, 0.0 );
		epa_ip->epa_r1 = view_state->vs_vop->vo_scale*0.5;
		epa_ip->epa_r2 = view_state->vs_vop->vo_scale*0.25;
	} else if( strcmp( argv[2], "ehy" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_EHY;
		internal.idb_meth = &rt_functab[ID_EHY];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ehy_internal) , "rt_ehy_internal" );
		ehy_ip = (struct rt_ehy_internal *)internal.idb_ptr;
		ehy_ip->ehy_magic = RT_EHY_INTERNAL_MAGIC;
		VSET( ehy_ip->ehy_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( ehy_ip->ehy_H, 0.0, 0.0, view_state->vs_vop->vo_scale );
		VSET( ehy_ip->ehy_Au, 0.0, 1.0, 0.0 );
		ehy_ip->ehy_r1 = view_state->vs_vop->vo_scale*0.5;
		ehy_ip->ehy_r2 = view_state->vs_vop->vo_scale*0.25;
		ehy_ip->ehy_c = ehy_ip->ehy_r2;
	} else if( strcmp( argv[2], "eto" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_ETO;
		internal.idb_meth = &rt_functab[ID_ETO];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_eto_internal) , "rt_eto_internal" );
		eto_ip = (struct rt_eto_internal *)internal.idb_ptr;
		eto_ip->eto_magic = RT_ETO_INTERNAL_MAGIC;
		VSET( eto_ip->eto_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ] );
		VSET( eto_ip->eto_N, 0.0, 0.0, 1.0 );
		VSET( eto_ip->eto_C, view_state->vs_vop->vo_scale*0.1, 0.0, view_state->vs_vop->vo_scale*0.1 );
		eto_ip->eto_r = view_state->vs_vop->vo_scale*0.5;
		eto_ip->eto_rd = view_state->vs_vop->vo_scale*0.05;
	} else if( strcmp( argv[2], "part" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_PARTICLE;
		internal.idb_meth = &rt_functab[ID_PARTICLE];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_part_internal) , "rt_part_internal" );
		part_ip = (struct rt_part_internal *)internal.idb_ptr;
		part_ip->part_magic = RT_PART_INTERNAL_MAGIC;
		VSET( part_ip->part_V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( part_ip->part_H, 0.0, 0.0, view_state->vs_vop->vo_scale );
		part_ip->part_vrad = view_state->vs_vop->vo_scale*0.5;
		part_ip->part_hrad = view_state->vs_vop->vo_scale*0.25;
		part_ip->part_type = RT_PARTICLE_TYPE_CONE;
	} else if( strcmp( argv[2], "nmg" ) == 0 ) {
		struct model *m;
		struct nmgregion *r;
		struct shell *s;

		m = nmg_mm();
		r = nmg_mrsv( m );
		s = BU_LIST_FIRST( shell , &r->s_hd );
		nmg_vertex_g( s->vu_p->v_p, -view_state->vs_vop->vo_center[MDX], -view_state->vs_vop->vo_center[MDY], -view_state->vs_vop->vo_center[MDZ]);
		(void)nmg_meonvu( s->vu_p );
		(void)nmg_ml( s );
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_NMG;
		internal.idb_meth = &rt_functab[ID_NMG];
		internal.idb_ptr = (genptr_t)m;
	} else if( strcmp( argv[2], "pipe" ) == 0 ) {
		struct wdb_pipept *ps;

		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_PIPE;
		internal.idb_meth = &rt_functab[ID_PIPE];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal" );
		pipe_ip = (struct rt_pipe_internal *)internal.idb_ptr;
		pipe_ip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
		BU_LIST_INIT( &pipe_ip->pipe_segs_head );
		BU_GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		ps->pp_od = 0.5*view_state->vs_vop->vo_scale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		BU_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
		BU_GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]+view_state->vs_vop->vo_scale );
		ps->pp_od = 0.5*view_state->vs_vop->vo_scale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		BU_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
	} else if( strcmp( argv[2], "bot" ) == 0 ) {
		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_BOT;
		internal.idb_meth = &rt_functab[ID_BOT];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_bot_internal ), "rt_bot_internal" );
		bot_ip = (struct rt_bot_internal *)internal.idb_ptr;
		bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
		bot_ip->mode = RT_BOT_SOLID;
		bot_ip->orientation = RT_BOT_UNORIENTED;
		bot_ip->error_mode = 0;
		bot_ip->num_vertices = 4;
		bot_ip->num_faces = 4;
		bot_ip->faces = (int *)bu_calloc( bot_ip->num_faces * 3, sizeof( int ), "BOT faces" );
		bot_ip->vertices = (fastf_t *)bu_calloc( bot_ip->num_vertices * 3, sizeof( fastf_t ), "BOT vertices" );
		bot_ip->thickness = (fastf_t *)NULL;
		bot_ip->face_mode = (struct bu_bitv *)NULL;
		VSET( &bot_ip->vertices[0],  -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]+view_state->vs_vop->vo_scale );
		VSET( &bot_ip->vertices[3], -view_state->vs_vop->vo_center[MDX]-0.5*view_state->vs_vop->vo_scale , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( &bot_ip->vertices[6], -view_state->vs_vop->vo_center[MDX]-0.5*view_state->vs_vop->vo_scale , -view_state->vs_vop->vo_center[MDY]-0.5*view_state->vs_vop->vo_scale , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( &bot_ip->vertices[9], -view_state->vs_vop->vo_center[MDX]+0.5*view_state->vs_vop->vo_scale , -view_state->vs_vop->vo_center[MDY]-0.5*view_state->vs_vop->vo_scale , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale );
		VSET( &bot_ip->faces[0], 0, 1, 3 );
		VSET( &bot_ip->faces[3], 0, 1, 2 );
		VSET( &bot_ip->faces[6], 0, 2, 3 );
		VSET( &bot_ip->faces[9], 1, 2, 3 );
	} else if( strcmp( argv[2], "extrude" ) == 0 ) {
		char *av[3];

		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_EXTRUDE;
		internal.idb_meth = &rt_functab[ID_EXTRUDE];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_extrude_internal), "rt_extrude_internal" );
		extrude_ip = (struct rt_extrude_internal *)internal.idb_ptr;
		extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;
		VSET( extrude_ip->V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		VSET( extrude_ip->h, 0.0, 0.0, view_state->vs_vop->vo_scale/3.0 );
		VSET( extrude_ip->u_vec, 1.0, 0.0, 0.0 );
		VSET( extrude_ip->v_vec, 0.0, 1.0, 0.0 );
		extrude_ip->keypoint = 0;
		av[0] = "make_name";
		av[1] = "skt_";
		Tcl_ResetResult( interp );
		cmd_make_name( (ClientData)NULL, interp, 2, av );
		extrude_ip->sketch_name = bu_strdup( interp->result );
		Tcl_ResetResult( interp );
		extrude_ip->skt = (struct rt_sketch_internal *)NULL;
		av[0] = "make";
		av[1] = extrude_ip->sketch_name;
		av[2] = "sketch";
		f_make( clientData, interp, 3, av );
	} else if( strcmp( argv[2], "sketch" ) == 0 ) {
		struct carc_seg *csg;
		struct line_seg *lsg;

		internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		internal.idb_type = ID_SKETCH;
		internal.idb_meth = &rt_functab[ID_SKETCH];
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_sketch_internal), "rt_sketch_internal" );
		sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
		sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
		VSET( sketch_ip->u_vec, 1.0, 0.0, 0.0 );
		VSET( sketch_ip->v_vec, 0.0, 1.0, 0.0 );
		VSET( sketch_ip->V, -view_state->vs_vop->vo_center[MDX] , -view_state->vs_vop->vo_center[MDY] , -view_state->vs_vop->vo_center[MDZ]-view_state->vs_vop->vo_scale*0.5 );
		sketch_ip->vert_count = 7;
		sketch_ip->verts = (point2d_t *)bu_calloc( sketch_ip->vert_count, sizeof( point2d_t ), "sketch_ip->verts" );
		sketch_ip->verts[0][0] = 0.25*view_state->vs_vop->vo_scale;
		sketch_ip->verts[0][1] = 0.0;
		sketch_ip->verts[1][0] = 0.5*view_state->vs_vop->vo_scale;
		sketch_ip->verts[1][1] = 0.0;
		sketch_ip->verts[2][0] = 0.5*view_state->vs_vop->vo_scale;
		sketch_ip->verts[2][1] = 0.5*view_state->vs_vop->vo_scale;
		sketch_ip->verts[3][0] = 0.0;
		sketch_ip->verts[3][1] = 0.5*view_state->vs_vop->vo_scale;
		sketch_ip->verts[4][0] = 0.0;
		sketch_ip->verts[4][1] = 0.25*view_state->vs_vop->vo_scale;
		sketch_ip->verts[5][0] = 0.25*view_state->vs_vop->vo_scale;
		sketch_ip->verts[5][1] = 0.25*view_state->vs_vop->vo_scale;
		sketch_ip->verts[6][0] = 0.125*view_state->vs_vop->vo_scale;
		sketch_ip->verts[6][1] = 0.125*view_state->vs_vop->vo_scale;
		sketch_ip->skt_curve.seg_count = 6;
		sketch_ip->skt_curve.reverse = (int *)bu_calloc( sketch_ip->skt_curve.seg_count, sizeof( int ), "sketch_ip->skt_curve.reverse" );
		sketch_ip->skt_curve.segments = (genptr_t *)bu_calloc( sketch_ip->skt_curve.seg_count, sizeof( genptr_t ), "sketch_ip->skt_curve.segments" );

		csg = (struct carc_seg *)bu_calloc( 1, sizeof( struct carc_seg ), "segments" );
		sketch_ip->skt_curve.segments[0] = (genptr_t)csg;
		csg->magic = CURVE_CARC_MAGIC;
		csg->start = 4;
		csg->end = 0;
		csg->radius = 0.25*view_state->vs_vop->vo_scale;
		csg->center_is_left = 1;
		csg->orientation = 0;

		lsg = (struct line_seg *)bu_calloc( 1, sizeof( struct line_seg ), "segments" );
		sketch_ip->skt_curve.segments[1] = (genptr_t)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = 0;
		lsg->end = 1;

		lsg = (struct line_seg *)bu_calloc( 1, sizeof( struct line_seg ), "segments" );
		sketch_ip->skt_curve.segments[2] = (genptr_t)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = 1;
		lsg->end = 2;

		lsg = (struct line_seg *)bu_calloc( 1, sizeof( struct line_seg ), "segments" );
		sketch_ip->skt_curve.segments[3] = (genptr_t)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = 2;
		lsg->end = 3;

		lsg = (struct line_seg *)bu_calloc( 1, sizeof( struct line_seg ), "segments" );
		sketch_ip->skt_curve.segments[4] = (genptr_t)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = 3;
		lsg->end = 4;

		csg = (struct carc_seg *)bu_calloc( 1, sizeof( struct carc_seg ), "segments" );
		sketch_ip->skt_curve.segments[5] = (genptr_t)csg;
		csg->magic = CURVE_CARC_MAGIC;
		csg->start = 6;
		csg->end = 5;
		csg->radius = -1.0;
		csg->center_is_left = 1;
		csg->orientation = 0;
	} else if (strcmp(argv[2], "hf") == 0) {
		Tcl_AppendResult(interp, "make: the height field is deprecated and not supported by this command.\nUse the dsp primitive.\n", (char *)NULL);
		return TCL_ERROR;
	} else if (strcmp(argv[2], "pg") == 0 ||
		   strcmp(argv[2], "poly") == 0) {
		Tcl_AppendResult(interp, "make: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.\n", (char *)NULL);
		return TCL_ERROR;
	} else if (strcmp(argv[2], "cline") == 0 ||
		   strcmp(argv[2], "dsp") == 0 ||
		   strcmp(argv[2], "ebm") == 0 ||
		   strcmp(argv[2], "nurb") == 0 ||
		   strcmp(argv[2], "spline") == 0 ||
		   strcmp(argv[2], "submodel") == 0 ||
		   strcmp(argv[2], "vol") == 0) {
		Tcl_AppendResult(interp, "make: the ", argv[2], " primitive is not supported by this command.\n", (char *)NULL);
		return TCL_ERROR;
	} else {
	  Tcl_AppendResult(interp, "make:  ", argv[2], " is not a known primitive\n",
			   "\tchoices are: arb8, arb7, arb6, arb5, arb4, arbn, ars, bot,\n",
			   "\t\tehy, ell, ell1, epa, eto, extrude, grip, half, nmg,\n",
			   "\t\tpart, pipe, rcc, rec, rhc, rpc, sketch, sph, tec,\n",
			   "\t\ttgc, tor, trc\n",
			   (char *)NULL);
	  return TCL_ERROR;
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, argv[1], -1L, 0, DIR_SOLID, (genptr_t)&internal.idb_type)) == DIR_NULL )  {
	    	TCL_ALLOC_ERR_return;
	}
	if( rt_db_put_internal( dp, dbip, &internal, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}

	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = argv[1]; /* depends on name being in argv[1] */
	  av[2] = NULL;

	  /* draw the "made" solid */
	  return cmd_draw( clientData, interp, 2, av );
	}
}

int
mged_rot_obj(interp, iflag, argvect)
Tcl_Interp *interp;
int iflag;
vect_t argvect;
{
	point_t model_pt;
	point_t point;
	point_t s_point;
	mat_t temp;
	vect_t v_work;

  update_views = 1;

  if(movedir != ROTARROW) {
    /* NOT in object rotate mode - put it in obj rot */
    movedir = ROTARROW;
  }

  /* find point for rotation to take place wrt */
#if 0
  MAT4X3PNT(model_pt, es_mat, es_keypoint);
#else
  VMOVE(model_pt, es_keypoint);
#endif
  MAT4X3PNT(point, modelchanges, model_pt);

  /* Find absolute translation vector to go from "model_pt" to
   * 	"point" without any of the rotations in "modelchanges"
   */
  VSCALE(s_point, point, modelchanges[15]);
  VSUB2(v_work, s_point, model_pt);

  /* REDO "modelchanges" such that:
   *	1. NO rotations (identity)
   *	2. trans == v_work
   *	3. same scale factor
   */
  MAT_IDN(temp);
  MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
  temp[15] = modelchanges[15];
  MAT_COPY(modelchanges, temp);

  /* build new rotation matrix */
  MAT_IDN(temp);
  bn_mat_angles(temp, argvect[0], argvect[1], argvect[2]);

  if(iflag){
    /* apply accumulated rotations */
    bn_mat_mul2(acc_rot_sol, temp);
  }

  /*XXX*/ MAT_COPY(acc_rot_sol, temp); /* used to rotate solid/object axis */
  
  /* Record the new rotation matrix into the revised
   *	modelchanges matrix wrt "point"
   */
  wrt_point(modelchanges, temp, modelchanges, point);

#ifdef DO_NEW_EDIT_MATS
  new_edit_mats();
#else
  new_mats();
#endif

  return TCL_OK;
}


/* allow precise changes to object rotation */
int
f_rot_obj(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int iflag = 0;
  vect_t argvect;

  CHECK_DBI_NULL;
  CHECK_READ_ONLY;

  if(argc < 4 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( not_state( ST_O_EDIT, "Object Rotation" ) )
    return TCL_ERROR;

  /* Check for -i option */
  if(argv[1][0] == '-' && argv[1][1] == 'i'){
    iflag = 1;  /* treat arguments as incremental values */
    ++argv;
    --argc;
  }

  if(argc != 4)
    return TCL_ERROR;

  argvect[0] = atof(argv[1]);
  argvect[1] = atof(argv[2]);
  argvect[2] = atof(argv[3]);

  return mged_rot_obj(interp, iflag, argvect);
}

/* allow precise changes to object scaling, both local & global */
int
f_sc_obj(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	mat_t incr;
	vect_t point, temp;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help oscale");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_O_EDIT, "Object Scaling" ) )
	  return TCL_ERROR;

	if( atof(argv[1]) <= 0.0 ) {
	  Tcl_AppendResult(interp, "ERROR: scale factor <=  0\n", (char *)NULL);
	  return TCL_ERROR;
	}

	update_views = 1;

#if 0
	if(movedir != SARROW) {
		/* Put in global object scale mode */
		if( edobj == 0 )
			edobj = BE_O_SCALE;	/* default is global scaling */
		movedir = SARROW;
	}
#endif

	MAT_IDN(incr);

	/* switch depending on type of scaling to do */
	switch( edobj ) {
	default:
	case BE_O_SCALE:
		/* global scaling */
		incr[15] = 1.0 / (atof(argv[1]) * modelchanges[15]);
		break;
	case BE_O_XSCALE:
		/* local scaling ... X-axis */
		incr[0] = atof(argv[1]) / acc_sc[0];
		acc_sc[0] = atof(argv[1]);
		break;
	case BE_O_YSCALE:
		/* local scaling ... Y-axis */
		incr[5] = atof(argv[1]) / acc_sc[1];
		acc_sc[1] = atof(argv[1]);
		break;
	case BE_O_ZSCALE:
		/* local scaling ... Z-axis */
		incr[10] = atof(argv[1]) / acc_sc[2];
		acc_sc[2] = atof(argv[1]);
		break;
	}

	/* find point the scaling is to take place wrt */
#if 0
	MAT4X3PNT(temp, es_mat, es_keypoint);
#else
	VMOVE(temp, es_keypoint);
#endif
	MAT4X3PNT(point, modelchanges, temp);

	wrt_point(modelchanges, incr, modelchanges, point);
#ifdef DO_NEW_EDIT_MATS
	new_edit_mats();
#else
	new_mats();
#endif

	return TCL_OK;
}

/*
 *			F _ T R _ O B J
 *
 *  Bound to command "translate"
 *
 *  Allow precise changes to object translation
 */
int
f_tr_obj(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int i;
	mat_t incr, old;
	vect_t model_sol_pt, model_incr, ed_sol_pt, new_vertex;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help translate");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( state == ST_S_EDIT )  {
		/* In solid edit mode,
		 * perform the equivalent of "press sxy" and "p xyz"
		 */
		if( be_s_trans(clientData, interp, argc, argv) == TCL_ERROR )
			return TCL_ERROR;
		return f_param(clientData, interp, argc, argv);
	}

	if( not_state( ST_O_EDIT, "Object Translation") )
	  return TCL_ERROR;

	/* Remainder of code concerns object edit case */

	update_views = 1;

	MAT_IDN(incr);
	MAT_IDN(old);

	if( (movedir & (RARROW|UARROW)) == 0 ) {
		/* put in object trans mode */
		movedir = UARROW | RARROW;
	}

	for(i=0; i<3; i++) {
		new_vertex[i] = atof(argv[i+1]) * local2base;
	}
#if 0
	MAT4X3PNT(model_sol_pt, es_mat, es_keypoint);
#else
	VMOVE(model_sol_pt, es_keypoint);
#endif
	MAT4X3PNT(ed_sol_pt, modelchanges, model_sol_pt);
	VSUB2(model_incr, new_vertex, ed_sol_pt);
	MAT_DELTAS(incr, model_incr[0], model_incr[1], model_incr[2]);
	MAT_COPY(old,modelchanges);
	bn_mat_mul(modelchanges, incr, old);
#ifdef DO_NEW_EDIT_MATS
	new_edit_mats();
#else
	new_mats();
#endif

	return TCL_OK;
}

/* Change the default region ident codes: item air los mat
 */
int
f_regdef(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if (argc == 1) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "ident %d air %d los %d material %d",
		  item_default, air_default, los_default, mat_default);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
  }

  if(argc < 2 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help regdef");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  view_state->vs_flag = 1;
  item_default = atoi(argv[1]);
  wdbp->wdb_item_default = item_default;

  if(argc == 2)
    return TCL_OK;

  air_default = atoi(argv[2]);
  wdbp->wdb_air_default = air_default;
  if(air_default) {
    item_default = 0;
    wdbp->wdb_item_default = 0;
  }

  if(argc == 3)
    return TCL_OK;

  los_default = atoi(argv[3]);
  wdbp->wdb_los_default = los_default;

  if(argc == 4)
    return TCL_OK;

  mat_default = atoi(argv[4]);
  wdbp->wdb_mat_default = mat_default;

  return TCL_OK;
}

static int frac_stat;
void
mged_add_nmg_part(newname, m)
char *newname;
struct model *m;
{
	struct rt_db_internal	new_intern;
	struct directory *new_dp;
	struct nmgregion *r;

	if(dbip == DBI_NULL)
	  return;

	if( db_lookup( dbip,  newname, LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( newname );
		/* Free memory here */
		nmg_km(m);
		frac_stat = 1;
		return;
	}

	if( (new_dp=db_diradd( dbip, newname, -1, 0, DIR_SOLID, (genptr_t)&new_intern.idb_type)) == DIR_NULL )  {
	    	TCL_ALLOC_ERR;
		return;
	}

	/* make sure the geometry/bounding boxes are up to date */
	for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
		nmg_region_a(r, &mged_tol);


	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&new_intern);
	new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	new_intern.idb_type = ID_NMG;
	new_intern.idb_meth = &rt_functab[ID_NMG];
	new_intern.idb_ptr = (genptr_t)m;

	if( rt_db_put_internal( new_dp, dbip, &new_intern, &rt_uniresource ) < 0 )  {
		/* Free memory */
		nmg_km(m);
		Tcl_AppendResult(interp, "rt_db_put_internal() failure\n", (char *)NULL);
		frac_stat = 1;
		return;
	}
	/* Internal representation has been freed by rt_db_put_internal */
	new_intern.idb_ptr = (genptr_t)NULL;
	frac_stat = 0;
}
/*
 *			F _ F R A C T U R E
 *
 * Usage: fracture nmgsolid [prefix]
 *
 *	given an NMG solid, break it up into several NMG solids, each
 *	containing a single shell with a single sub-element.
 */
int
f_fracture(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int i;
	struct directory *old_dp;
	struct rt_db_internal	old_intern;
	struct model	*m, *new_model;
	char		newname[32];
	char		prefix[32];
	int	maxdigits;
	struct nmgregion *r, *new_r;
	struct shell *s, *new_s;
	struct faceuse *fu;
	struct vertex *v_new, *v;
	unsigned long tw, tf, tp;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help fracture");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	Tcl_AppendResult(interp, "fracture:", (char *)NULL);
	for (i=0 ; i < argc ; i++)
		Tcl_AppendResult(interp, " ", argv[i], (char *)NULL);
	Tcl_AppendResult(interp, "\n", (char *)NULL);

	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &old_intern, old_dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )  {
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( old_intern.idb_type != ID_NMG )
	{
		Tcl_AppendResult(interp, argv[1], " is not an NMG solid!!\n", (char *)NULL );
		rt_db_free_internal( &old_intern, &rt_uniresource );
		return TCL_ERROR;
	}

	m = (struct model *)old_intern.idb_ptr;
	NMG_CK_MODEL(m);

	/* how many characters of the solid names do we reserve for digits? */
	nmg_count_shell_kids(m, &tf, &tw, &tp);
	
	maxdigits = (int)(log10((double)(tf+tw+tp)) + 1.0);

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "%d = %d digits\n", tf+tw+tp, maxdigits);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	/*	for(maxdigits=1,i=tf+tw+tp ; i > 0 ; i /= 10)
	 *	maxdigits++;
	 */

	/* get the prefix for the solids to be created. */
	bzero(prefix, sizeof(prefix));
	strncpy(prefix, argv[argc-1], sizeof(prefix)-2-maxdigits);
	strcat(prefix, "_");

	/* Bust it up here */

	i = 1;
	for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
		NMG_CK_REGION(r);
		for (BU_LIST_FOR(s, shell, &r->s_hd)) {
			NMG_CK_SHELL(s);
			if (s->vu_p) {
				NMG_CK_VERTEXUSE(s->vu_p);
				NMG_CK_VERTEX(s->vu_p->v_p);
				v = s->vu_p->v_p;

/*	nmg_start_dup(m); */
				new_model = nmg_mm();
				new_r = nmg_mrsv(new_model);
				new_s = BU_LIST_FIRST(shell, &r->s_hd);
				v_new = new_s->vu_p->v_p;
				if (v->vg_p) {
					nmg_vertex_gv(v_new, v->vg_p->coord);
				}
/*	nmg_end_dup(); */

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
				continue;
			}
			for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (fu->orientation != OT_SAME)
					continue;

				NMG_CK_FACEUSE(fu);

				new_model = nmg_mm();
				NMG_CK_MODEL(new_model);
				new_r = nmg_mrsv(new_model);
				NMG_CK_REGION(new_r);
				new_s = BU_LIST_FIRST(shell, &new_r->s_hd);
				NMG_CK_SHELL(new_s);
/*	nmg_start_dup(m); */
				NMG_CK_SHELL(new_s);
				nmg_dup_face(fu, new_s);
/*	nmg_end_dup(); */

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);
				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
			}
#if 0
			while (BU_LIST_NON_EMPTY(&s->lu_hd)) {
				lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
				new_model = nmg_mm();
				r = nmg_mrsv(new_model);
				new_s = BU_LIST_FIRST(shell, &r->s_hd);

				nmg_dup_loop(lu, new_s);
				nmg_klu(lu);

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);
				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return CMD_BAD;
			}
			while (BU_LIST_NON_EMPTY(&s->eu_hd)) {
				eu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
				new_model = nmg_mm();
				r = nmg_mrsv(new_model);
				new_s = BU_LIST_FIRST(shell, &r->s_hd);

				nmg_dup_edge(eu, new_s);
				nmg_keu(eu);

				sprintf(newname, "%s%0*d", prefix, maxdigits, i++);

				mged_add_nmg_part(newname, new_model);
				if (frac_stat) return TCL_ERROR;
			}
#endif
		}
	}
	return TCL_OK;

}
/*
 *			F _ Q O R O T
 *
 * Usage: qorot x y z dx dy dz theta
 *
 *	rotate an object through a specified angle
 *	about a specified ray.
 */
int
f_qorot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	mat_t temp;
	vect_t s_point, point, v_work, model_pt;
	vect_t	specified_pt, direc;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 8 || 8 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help qorot");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
	  return TCL_ERROR;

	if(movedir != ROTARROW) {
		/* NOT in object rotate mode - put it in obj rot */
		movedir = ROTARROW;
	}
	VSET(specified_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
	VSCALE(specified_pt, specified_pt, dbip->dbi_local2base);
	VSET(direc, atof(argv[4]), atof(argv[5]), atof(argv[6]));

	/* find point for rotation to take place wrt */
	MAT4X3PNT(model_pt, es_mat, specified_pt);
	MAT4X3PNT(point, modelchanges, model_pt);

	/* Find absolute translation vector to go from "model_pt" to
	 * 	"point" without any of the rotations in "modelchanges"
	 */
	VSCALE(s_point, point, modelchanges[15]);
	VSUB2(v_work, s_point, model_pt);

	/* REDO "modelchanges" such that:
	 *	1. NO rotations (identity)
	 *	2. trans == v_work
	 *	3. same scale factor
	 */
	MAT_IDN(temp);
	MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
	temp[15] = modelchanges[15];
	MAT_COPY(modelchanges, temp);

	/* build new rotation matrix */
	MAT_IDN(temp);
	bn_mat_angles(temp, 0.0, 0.0, atof(argv[7]));

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point_direc(modelchanges, temp, modelchanges, point, direc);

#ifdef DO_NEW_EDIT_MATS
	new_edit_mats();
#else
	new_mats();
#endif

	return TCL_OK;
}

void
set_localunit_TclVar()
{
  struct bu_vls vls;
  struct bu_vls units_vls;
  const char	*str;

  if (dbip == DBI_NULL)
	  return;

  bu_vls_init(&vls);
  bu_vls_init(&units_vls);

  str = bu_units_string(dbip->dbi_local2base);
  if(str)
	bu_vls_strcpy(&units_vls, str);
  else
	bu_vls_printf(&units_vls, "%gmm", dbip->dbi_local2base);

  bu_vls_strcpy(&vls, "localunit");
  Tcl_SetVar(interp, bu_vls_addr(&vls), bu_vls_addr(&units_vls), TCL_GLOBAL_ONLY);

  bu_vls_free(&vls);
  bu_vls_free(&units_vls);
}


int
f_binary(ClientData	clientData,
	 Tcl_Interp	*interp,
	 int		argc,
	 char 		**argv)
{
	CHECK_DBI_NULL;

	return wdb_binary_cmd(wdbp, interp, argc, argv);
}
