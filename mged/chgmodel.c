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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
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
#include "db.h"
#include "nmg.h"
#include "nurb.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"

extern struct bn_tol mged_tol;


void set_tran();
void	aexists();

int		newedge;		/* new edge for arb editing */

/* Add/modify item and air codes of a region */
/* Format: I region item <air>	*/
int
f_itemair(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	int ident, air;
	union record record;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	air = ident = 0;
	ident = atoi( argv[2] );

	/* If <air> is not included, it is assumed to be zero */
	if( argc == 4 )  {
		air = atoi( argv[3] );
	}
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
	  TCL_READ_ERR_return;
	}
	if( record.u_id != ID_COMB ) {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( record.c.c_flags != 'R' ) {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a region\n", (char *)NULL);
	  return TCL_ERROR;
	}
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
	  TCL_WRITE_ERR_return;
	}

	return TCL_ERROR;
}

/* Modify material information */
/* Usage:  mater name */
int
f_mater(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int r=0, g=0, b=0;
	int skip_args = 0;
	char inherit;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;
	
	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_get( dbip, dp, &record, 0 , 1) < 0 ) {
	  TCL_READ_ERR_return;
	}

	if( record.u_id != ID_COMB )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( argc >= 3 )  {
	  if( strncmp( argv[2], "del", 3 ) != 0 )  {
	    /* Material */
	    strcpy( record.c.c_matname, argv[2]);
	  }else{
	    if(record.c.c_matname[0] != '\0'){
#if 0
	      Tcl_AppendResult(interp, "Was ", record.c.c_matname, " ",
			       record.c.c_matparm, "\n", (char *)NULL);
#endif
	      record.c.c_matname[0] = '\0';
	      record.c.c_override = 0;
#if 0
	      goto out;
#endif
	    }
	  }
	}else{
	  /* Material */
	  Tcl_AppendResult(interp, "Material = ", record.c.c_matname, "\n", MORE_ARGS_STR,
			   "Material?  ('del' to delete, CR to skip) ", (char *)NULL);

	  if(record.c.c_matname[0] == '\0')
	    bu_vls_printf(&curr_cmd_list->more_default, "del");
	  else
	    bu_vls_printf(&curr_cmd_list->more_default, "%s", record.c.c_matname);

	  return TCL_ERROR;
	}

	if(argc >= 4){
	  if( strncmp(argv[3],  "del", 3) == 0  )
	    record.c.c_matparm[0] = '\0';
	  else
	    strcpy( record.c.c_matparm, argv[3]);
	}else{
	  /* Parameters */
	  Tcl_AppendResult(interp, "Param = ", record.c.c_matparm, "\n", MORE_ARGS_STR,
			   "Parameter string? ('del' to delete, CR to skip) ", (char *)NULL);

	  if(record.c.c_matparm[0] == '\0')
	    bu_vls_printf(&curr_cmd_list->more_default, "del");
	  else
	    bu_vls_printf(&curr_cmd_list->more_default, "\"%s\"", record.c.c_matparm);

	  return TCL_ERROR;
	}

	if(argc >= 5){
	  if( strncmp(argv[4], "del", 3) == 0 ){
	    /* leave color as is */
	    record.c.c_override = 0;
	    skip_args = 2;
	  }else if(argc < 7){	/* prompt for color */
	    goto color_prompt;
	  }else{	/* change color */
	    sscanf(argv[4], "%d", &r);
	    sscanf(argv[5], "%d", &g);
	    sscanf(argv[6], "%d", &b);
	    record.c.c_rgb[0] = r;
	    record.c.c_rgb[1] = g;
	    record.c.c_rgb[2] = b;
	    record.c.c_override = 1;
	  }
	}else{
	/* Color */
color_prompt:
	  if( record.c.c_override ){
	    struct bu_vls tmp_vls;
	    
	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Color = %d %d %d\n",
			  record.c.c_rgb[0], record.c.c_rgb[1], record.c.c_rgb[2] );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    bu_vls_printf(&curr_cmd_list->more_default, "%d %d %d",
			  record.c.c_rgb[0],
			  record.c.c_rgb[1],
			  record.c.c_rgb[2] );
	  }else{
	    Tcl_AppendResult(interp, "Color = (No color specified)\n", (char *)NULL);
	    bu_vls_printf(&curr_cmd_list->more_default, "del");
	  }

	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Color R G B (0..255)? ('del' to delete, CR to skip) ", (char *)NULL);
	  return TCL_ERROR;
	}

	if(argc >= 8 - skip_args){
	  inherit = *argv[7 - skip_args];
	}else{
	  /* Inherit */
	  switch( record.c.c_inherit )  {
	  default:
	    /* This is necessary to clean up old databases with grunge here */
	    record.c.c_inherit = DB_INH_LOWER;
	    /* Fall through */
	  case DB_INH_LOWER:
	    Tcl_AppendResult(interp, "Inherit = 0:  lower nodes (towards leaves) override\n",
			     (char *)NULL);
	    break;
	  case DB_INH_HIGHER:
	    Tcl_AppendResult(interp, "Inherit = 1:  higher nodes (towards root) override\n",
			     (char *)NULL);
	    break;
	  }

	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Inheritance (0|1)? (CR to skip) ", (char *)NULL);
	  switch( record.c.c_inherit ) {
	  case DB_INH_HIGHER:
	    bu_vls_printf(&curr_cmd_list->more_default, "1");
	    break;
	  case DB_INH_LOWER:
	  default:
	    bu_vls_printf(&curr_cmd_list->more_default, "0");
	    break;
	  }

	  return TCL_ERROR;
	}

	switch( inherit )  {
	case '1':
		record.c.c_inherit = DB_INH_HIGHER;
		break;
	case '0':
		record.c.c_inherit = DB_INH_LOWER;
		break;
	case '\0':
	case '\n':
		break;
	default:
	  Tcl_AppendResult(interp, "Unknown response ignored\n", (char *)NULL);
	  break;
	}		
out:
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
	  TCL_WRITE_ERR_return;
	}

	return TCL_OK;
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
  char *tmpfil = "/tmp/GED.aXXXXX";

  char **av;
  
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  (void)mktemp(tmpfil);
  i=creat(tmpfil, 0600);
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
f_wmater(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    *argv[];
{
  int i;
  int status = TCL_OK;
  FILE *fp;
  register struct directory *dp;
  union record record;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if((fp = fopen(argv[1], "w")) == NULL){
    Tcl_AppendResult(interp, "f_wmater: Failed to open file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  for(i = 2; i < argc; ++i){
    if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) == DIR_NULL ){
      Tcl_AppendResult(interp, "f_wmater: Failed to find ", argv[i], "\n", (char *)NULL);
      status = TCL_ERROR;
      continue;
    }

    if( db_get( dbip, dp, &record, 0 , 1) < 0 ){
      TCL_READ_ERR;
      status = TCL_ERROR;
      continue;
    }

    if( record.u_id != ID_COMB ) {
      Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
      status = TCL_ERROR;
      continue;
    }

    fprintf(fp, "\"%s\"\t\"%s\"\t\"%s\"\t%d\t%d\t%d\t%d\t%d\n", argv[i],
	    record.c.c_matname[0] ? record.c.c_matname : "-",
	    record.c.c_matparm[0] ? record.c.c_matparm : "-",
	    (int)record.c.c_rgb[0], (int)record.c.c_rgb[1], (int)record.c.c_rgb[2],
	    (int)record.c.c_override, (int)record.c.c_inherit);
  }

  (void)fclose(fp);
  return status;
}


int
f_rmater(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    *argv[];
{
#ifndef LINELEN
#define LINELEN 256
#endif
  int status = TCL_OK;
  FILE *fp;
  register struct directory *dp;
  union record record;
  char line[LINELEN];
  char name[NAMESIZE];
  char matname[32]; 
  char parm[60];
  int r,g,b;
  int override;
  int inherit;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if((fp = fopen(argv[1], "r")) == NULL){
    Tcl_AppendResult(interp, "f_rcodes: Failed to read file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  while(fgets( line , LINELEN, fp ) != NULL){
    if((extract_mater_from_line(line, name, matname, parm,
			    &r, &g, &b, &override, &inherit)) == TCL_ERROR)
      continue;

    if( (dp = db_lookup( dbip,  name, LOOKUP_NOISY )) == DIR_NULL ){
      Tcl_AppendResult(interp, "f_rmater: Failed to find ", name, "\n", (char *)NULL);
      status = TCL_ERROR;
      continue;
    }

    if( db_get( dbip, dp, &record, 0, 1) < 0 )
      continue;

    /* Assign new values */
    if(matname[0] == '-')
      record.c.c_matname[0] = '\0';
    else
      strcpy(record.c.c_matname, matname);

    if(parm[0] == '-')
      record.c.c_matparm[0] = '\0';
    else
      strcpy(record.c.c_matparm, parm);

    record.c.c_rgb[0] = (unsigned char)r;
    record.c.c_rgb[1] = (unsigned char)g;
    record.c.c_rgb[2] = (unsigned char)b;
    record.c.c_override = (char)override;
    record.c.c_inherit = (char)inherit;

    /* Write new values to database */
    if( db_put( dbip, dp, &record, 0, 1 ) < 0 ){
      Tcl_AppendResult(interp, "Database write error, aborting.\n",
		       (char *)NULL);
      return TCL_ERROR;
    }
  }

  (void)fclose(fp);
  return status;
}

int
extract_mater_from_line(line, name, matname, parm, r, g, b, override, inherit)
char *line;
char *name;
char *matname;
char *parm;
int *r, *g, *b;
int *override;
int *inherit;
{
  int i,j,k;
  char *str[3];

  str[0] = name;
  str[1] = matname;
  str[2] = parm;

  /* Extract first 3 strings. */
  for(i=j=0; i < 3; ++i){

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
    union record		record;

    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

    if ((dp = db_lookup(dbip,  argv[1], LOOKUP_NOISY)) == DIR_NULL)
      return TCL_ERROR;

    if (db_get(dbip,  dp, &record, 0 , 1) < 0)
    {
      TCL_READ_ERR_return;
    }

    if (record.u_id != ID_COMB)
    {
      Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
      return TCL_ERROR;
    }

    for (i = 0; i < 3; ++i)
	if (((val = atoi(argv[i + 2])) < 0) || (val > 255))
	{
	  Tcl_AppendResult(interp, "RGB value out of range: ", argv[i + 2],
			   "\n", (char *)NULL);
	  return TCL_ERROR;
	}
	else
	    record.c.c_rgb[i] = val;
    record.c.c_override = 1;

    if (db_put( dbip, dp, &record, 0, 1) < 0)
    {
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
	union record		record;
	struct bu_vls		args;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
	  TCL_READ_ERR_return;
	}

	if( record.u_id != ID_COMB )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	strncpy( record.c.c_matname, argv[2], sizeof(record.c.c_matname)-1 );
	record.c.c_matname[sizeof(record.c.c_matname)-1] = '\0';

	/* Bunch up the rest of the args, space separated */
	bu_vls_init( &args );
	bu_vls_from_argv( &args, argc-3, argv+3 );
	bu_vls_trunc( &args, sizeof(record.c.c_matparm)-1 );
	strcpy( record.c.c_matparm, bu_vls_addr( &args ) );
	bu_vls_free( &args );

	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
	  TCL_WRITE_ERR_return;
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
	union record	*rec;
	mat_t mirmat;
	mat_t temp;
	int ngran;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

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

	if( proto->d_flags & DIR_SOLID )
	{
		struct rt_db_internal	internal;
		struct bu_external	ext;
		int			id;

		BU_INIT_EXTERNAL( &ext );
		RT_INIT_DB_INTERNAL( &internal );

		if( db_get_external( &ext, proto, dbip ) < 0 )
			return TCL_ERROR;

		id = rt_id_solid( &ext );
		if( rt_functab[id].ft_import( &internal, &ext, bn_mat_identity ) < 0 )  {
		  Tcl_AppendResult(interp, "f_mirror(", argv[1], argv[2],
			   "):  solid import failure\n", (char *)NULL);
		  if( internal.idb_ptr )  rt_functab[id].ft_ifree( &internal );
		  db_free_external( &ext );
		  return TCL_ERROR;				/* FAIL */
		}
		RT_CK_DB_INTERNAL( &internal );

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
					struct face_g_plane *fg;

					f = (struct face *)BU_PTBL_GET( &table, i );
					NMG_CK_FACE( f );

					if( !f->g.magic_p )
						continue;

					if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
					{
						Tcl_AppendResult(interp, "Sorry, Can only mirror NMG solids with planar faces", (char *)0 );
						bu_ptbl_free( &table );
						rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */
						return TCL_ERROR;
					}

					
				}

				for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
					for( BU_LIST_FOR( s, shell, &r->s_hd ) )
						nmg_invert_shell( s, &mged_tol );


				for( i=0 ; i<BU_PTBL_END( &table ) ; i++ )
				{
					struct face *f;
					struct face_g_plane *fg;
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
						rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */
						return TCL_ERROR;
					}

					if( nmg_calc_face_g( fu ) )
					{
						Tcl_AppendResult(interp, "nmg_calc_face_g() failed", (char *)0 );
						bu_ptbl_free( &table );
						rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */
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

				MAT_IDN( mirmat )
				mirmat[k*5] = -1.0;
				bn_mat_mul( temp, mirmat, ebm->mat );
				MAT_COPY( ebm->mat, temp )

				break;
			}
			case ID_VOL:
			{
				struct rt_vol_internal *vol;

				vol = (struct rt_vol_internal *)internal.idb_ptr;
				RT_VOL_CK_MAGIC( vol );


				MAT_IDN( mirmat )
				mirmat[k*5] = -1.0;
				bn_mat_mul( temp, mirmat, vol->mat );
				MAT_COPY( vol->mat, temp )

				break;
			}
			default:
			{
				rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */
				Tcl_AppendResult(interp, "Cannot mirror this solid type\n", (char *)NULL);
				return TCL_ERROR;
			}
		}

		if( rt_functab[internal.idb_type].ft_export( &ext, &internal, 1.0 ) < 0 )
		{
		  Tcl_AppendResult(interp, "f_mirror: export failure\n", (char *)NULL);
		  rt_functab[internal.idb_type].ft_ifree( &internal );
		  return TCL_ERROR;
		}
		rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */

		/* no interuprts */
		(void)signal( SIGINT, SIG_IGN );

		ngran = (ext.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
		if( (dp = db_diradd( dbip, argv[2], -1L, ngran, DIR_SOLID)) == DIR_NULL ||
		    db_alloc( dbip, dp, 1 ) < 0 )
		    {
		    	db_free_external( &ext );
		    	TCL_ALLOC_ERR_return;
		    }

		if (db_put_external( &ext, dp, dbip ) < 0 )
		{
			db_free_external( &ext );
			TCL_WRITE_ERR_return;
		}
		db_free_external( &ext );

	} else if( proto->d_flags & DIR_COMB ) {
		if( (rec = db_getmrec( dbip, proto )) == (union record *)0 ) {
		  TCL_READ_ERR_return;
		}

		if( (dp = db_diradd( dbip, argv[2], -1, proto->d_len, proto->d_flags ) ) == DIR_NULL ||
		    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
		  TCL_ALLOC_ERR_return;
		}
		NAMEMOVE(argv[2], rec[0].c.c_name);
		bn_mat_idn( mirmat );
		mirmat[k*5] = -1.0;
		for( i=1; i < proto->d_len; i++) {
			mat_t	xmat;

			if(rec[i].u_id != ID_MEMB) {
			  Tcl_AppendResult(interp, "f_mirror: bad db record\n", (char *)NULL);
			  return TCL_ERROR;
			}
			rt_mat_dbmat( xmat, rec[i].M.m_mat );
			bn_mat_mul(temp, mirmat, xmat);
			rt_dbmat_mat( rec[i].M.m_mat, temp );
		}
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 ) {
		  TCL_WRITE_ERR_return;
		}
		bu_free((genptr_t)rec, "record");
	} else {
	  Tcl_AppendResult(interp, argv[2], ": Cannot mirror\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( no_memory )  {
	  Tcl_AppendResult(interp, "Mirror image (", argv[2],
			   ") created but NO memory left to draw it\n", (char *)NULL);
	  return TCL_ERROR;
	}

	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = argv[2]; /* depends on solid name being in argv[2] */
	  av[2] = NULL;

	  return f_edit( clientData, interp, 2, av );
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
	union record record;
	int regionid, air, mat, los;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	regionid = atoi( argv[3] );
	air = atoi( argv[4] );
	los = atoi( argv[5] );
	mat = atoi( argv[6] );

	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
	  TCL_READ_ERR_return;
	}
	if( record.u_id != ID_COMB ) {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( argv[2][0] == 'R' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags =' ';
	record.c.c_regionid = regionid;
	record.c.c_aircode = air;
	record.c.c_los = los;
	record.c.c_material = mat;
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
	  TCL_WRITE_ERR_return;
	}

	return TCL_ERROR;
}

/*
 *			F _ U N I T S
 *
 * Change the local units of the description.
 * Base unit is fixed in mm, so this just changes the current local unit
 * that the user works in.
 */
int
f_units(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	double	loc2mm;
	int	new_unit = 0;
	CONST char	*str;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc < 2 )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  str = rt_units_string(dbip->dbi_local2base);
	  bu_vls_printf(&tmp_vls, "You are currently editing in '%s'.  1%s = %gmm \n",
			str, str, dbip->dbi_local2base );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return TCL_OK;
	}

	if( strcmp(argv[1], "mm") == 0 ) 
		new_unit = ID_MM_UNIT;
	else
	if( strcmp(argv[1], "cm") == 0 ) 
		new_unit = ID_CM_UNIT;
	else
	if( strcmp(argv[1],"m")==0 || strcmp(argv[1],"meters")==0 ) 
		new_unit = ID_M_UNIT;
	else
	if( strcmp(argv[1],"in")==0 || strcmp(argv[1],"inches")==0 ) 
		new_unit = ID_IN_UNIT;
	else
	if( strcmp(argv[1],"ft")==0 || strcmp(argv[1],"feet")==0 ) 
		new_unit = ID_FT_UNIT;

	if( new_unit ) {
		/* One of the recognized db.h units */
		/* change to the new local unit */
		db_conversions( dbip, new_unit );

		if( db_ident( dbip, dbip->dbi_title, new_unit ) < 0 )
		  Tcl_AppendResult(interp,
				   "Warning: unable to stash working units into database\n",
				   (char *)NULL);

	} else if( (loc2mm = rt_units_conversion(argv[1]) ) <= 0 )  {
	  Tcl_AppendResult(interp, argv[1], ": unrecognized unit\n",
			   "valid units: <mm|cm|m|in|ft|meters|inches|feet>\n", (char *)NULL);
	  return TCL_ERROR;
	} else {
		/*
		 *  Can't stash requested units into the database for next session,
		 *  but there is no problem with the user editing in these units.
		 */
		dbip->dbi_localunit = ID_MM_UNIT;
		dbip->dbi_local2base = loc2mm;
		dbip->dbi_base2local = 1.0 / loc2mm;
		Tcl_AppendResult(interp, "\
Due to a database restriction in the current format of .g files,\n\
this choice of units will not be remembered on your next editing session.\n", (char *)NULL);
	}
	Tcl_AppendResult(interp, "New editing units = '", rt_units_string(dbip->dbi_local2base),
			 "'\n", (char *)NULL);
	dmaflag = 1;

	return TCL_OK;
}

/*
 *	Change the current title of the description
 */
int
f_title(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct bu_vls	title;
	int bad = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc < 2 )  {
	  Tcl_AppendResult(interp, dbip->dbi_title, "\n", (char *)NULL);
	  return TCL_OK;
	}

	bu_vls_init( &title );
	bu_vls_from_argv( &title, argc-1, argv+1 );

	if( db_ident( dbip, bu_vls_addr(&title), dbip->dbi_localunit ) < 0 ) {
	  Tcl_AppendResult(interp, "Error: unable to change database title\n");
	  bad = 1;
	}

	bu_vls_free( &title );
	dmaflag = 1;

	return bad ? TCL_ERROR : TCL_OK;
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
f_make(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	int ngran;
	int i;
	struct rt_db_internal	internal;
	struct bu_external	external;
	struct rt_arb_internal	*arb_ip;
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[1] );
	  return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL( &internal );

	/* make name <arb8 | arb7 | arb6 | arb5 | arb4 | ellg | ell |
	 * sph | tor | tgc | rec | trc | rcc | grp | half | nmg> */
	if( strcmp( argv[2], "arb8" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[5][Y] += Viewscale*2.0;
		arb_ip->pt[6][Y] += Viewscale*2.0;
		arb_ip->pt[6][Z] += Viewscale*2.0;
		arb_ip->pt[7][Z] += Viewscale*2.0;
	} else if( strcmp( argv[2], "arb7" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -(0.5*Viewscale) );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[5][Y] += Viewscale*2.0;
		arb_ip->pt[6][Y] += Viewscale*2.0;
		arb_ip->pt[6][Z] += Viewscale;
	} else if( strcmp( argv[2], "arb6" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
			arb_ip->pt[i][X] -= Viewscale*2.0;
		arb_ip->pt[4][Y] += Viewscale;
		arb_ip->pt[5][Y] += Viewscale;
		arb_ip->pt[6][Y] += Viewscale;
		arb_ip->pt[6][Z] += Viewscale*2.0;
		arb_ip->pt[7][Y] += Viewscale;
		arb_ip->pt[7][Z] += Viewscale*2.0;
	} else if( strcmp( argv[2], "arb5" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= Viewscale*2.0;
			arb_ip->pt[i][Y] += Viewscale;
			arb_ip->pt[i][Z] += Viewscale;
		}
	} else if( strcmp( argv[2], "arb4" ) == 0 )  {
		internal.idb_type = ID_ARB8;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal) , "rt_arb_internal" );
		arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
		arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
		VSET( arb_ip->pt[0] ,
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		for( i=1 ; i<8 ; i++ )
			VMOVE( arb_ip->pt[i] , arb_ip->pt[0] );
		arb_ip->pt[1][Y] += Viewscale*2.0;
		arb_ip->pt[2][Y] += Viewscale*2.0;
		arb_ip->pt[2][Z] += Viewscale*2.0;
		arb_ip->pt[3][Y] += Viewscale*2.0;
		arb_ip->pt[3][Z] += Viewscale*2.0;
		for( i=4 ; i<8 ; i++ )
		{
			arb_ip->pt[i][X] -= Viewscale*2.0;
			arb_ip->pt[i][Y] += Viewscale*2.0;
		}
	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, (0.5*Viewscale), 0, 0 );	/* A */
		VSET( ell_ip->b, 0, (0.5*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.5*Viewscale) );	/* C */
	} else if(( strcmp( argv[2], "grp" ) == 0 ) ||
		  ( strcmp( argv[2], "grip") == 0 )) {
		internal.idb_type = ID_GRIP;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_grip_internal), "rt_grp_internal" );
		grp_ip = (struct rt_grip_internal *) internal.idb_ptr;
		grp_ip->magic = RT_GRIP_INTERNAL_MAGIC;
		VSET( grp_ip->center, -toViewcenter[MDX], -toViewcenter[MDY],
		    -toViewcenter[MDZ]);
		VSET( grp_ip->normal, 1.0, 0.0, 0.0);
		grp_ip->mag = Viewscale*0.75;
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, (0.5*Viewscale), 0, 0 );	/* A */
		VSET( ell_ip->b, 0, (0.25*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "ellg" ) == 0 )  {
		internal.idb_type = ID_ELL;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal) , "rt_ell_internal" );
		ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
		ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
		VSET( ell_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( ell_ip->a, Viewscale, 0, 0 );		/* A */
		VSET( ell_ip->b, 0, (0.5*Viewscale), 0 );	/* B */
		VSET( ell_ip->c, 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( argv[2], "tor" ) == 0 )  {
		internal.idb_type = ID_TOR;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tor_internal) , "rt_tor_internal" );
		tor_ip = (struct rt_tor_internal *)internal.idb_ptr;
		tor_ip->magic = RT_TOR_INTERNAL_MAGIC;
		VSET( tor_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( tor_ip->h , 1.0 , 0.0 , 0.0 );	/* unit normal */
		tor_ip->r_h = 0.5*Viewscale;
		tor_ip->r_a = Viewscale;
		tor_ip->r_b = Viewscale;
		VSET( tor_ip->a , 0.0 , Viewscale , 0.0 );
		VSET( tor_ip->b , 0.0 , 0.0 , Viewscale );
	} else if( strcmp( argv[2], "tgc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "tec" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.125*Viewscale), 0 );
	} else if( strcmp( argv[2], "rec" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.25*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "trc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.5*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.25*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.25*Viewscale), 0 );
	} else if( strcmp( argv[2], "rcc" ) == 0 )  {
		internal.idb_type = ID_TGC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal) , "rt_tgc_internal" );
		tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
		tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
		VSET( tgc_ip->v , -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		VSET( tgc_ip->h,  0, 0, (Viewscale*2) );
		VSET( tgc_ip->a,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->b,  0, (0.5*Viewscale), 0 );
		VSET( tgc_ip->c,  (0.5*Viewscale), 0, 0 );
		VSET( tgc_ip->d,  0, (0.5*Viewscale), 0 );
	} else if( strcmp( argv[2], "half" ) == 0 ) {
		internal.idb_type = ID_HALF;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_half_internal) , "rt_half_internal" );
		half_ip = (struct rt_half_internal *)internal.idb_ptr;
		half_ip->magic = RT_HALF_INTERNAL_MAGIC;
		VSET( half_ip->eqn , 0 , 0 , 1 );
		half_ip->eqn[3] = (-toViewcenter[MDZ]);
	} else if( strcmp( argv[2], "rpc" ) == 0 ) {
		internal.idb_type = ID_RPC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rpc_internal) , "rt_rpc_internal" );
		rpc_ip = (struct rt_rpc_internal *)internal.idb_ptr;
		rpc_ip->rpc_magic = RT_RPC_INTERNAL_MAGIC;
		VSET( rpc_ip->rpc_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( rpc_ip->rpc_H, 0, 0, Viewscale );
		VSET( rpc_ip->rpc_B, 0, (Viewscale*0.5), 0 );
		rpc_ip->rpc_r = Viewscale*0.25;
	} else if( strcmp( argv[2], "rhc" ) == 0 ) {
		internal.idb_type = ID_RHC;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rhc_internal) , "rt_rhc_internal" );
		rhc_ip = (struct rt_rhc_internal *)internal.idb_ptr;
		rhc_ip->rhc_magic = RT_RHC_INTERNAL_MAGIC;
		VSET( rhc_ip->rhc_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( rhc_ip->rhc_H, 0, 0, Viewscale );
		VSET( rhc_ip->rhc_B, 0, (Viewscale*0.5), 0 );
		rhc_ip->rhc_r = Viewscale*0.25;
		rhc_ip->rhc_c = Viewscale*0.10;
	} else if( strcmp( argv[2], "epa" ) == 0 ) {
		internal.idb_type = ID_EPA;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_epa_internal) , "rt_epa_internal" );
		epa_ip = (struct rt_epa_internal *)internal.idb_ptr;
		epa_ip->epa_magic = RT_EPA_INTERNAL_MAGIC;
		VSET( epa_ip->epa_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( epa_ip->epa_H, 0, 0, Viewscale );
		VSET( epa_ip->epa_Au, 0, 1, 0 );
		epa_ip->epa_r1 = Viewscale*0.5;
		epa_ip->epa_r2 = Viewscale*0.25;
	} else if( strcmp( argv[2], "ehy" ) == 0 ) {
		internal.idb_type = ID_EHY;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ehy_internal) , "rt_ehy_internal" );
		ehy_ip = (struct rt_ehy_internal *)internal.idb_ptr;
		ehy_ip->ehy_magic = RT_EHY_INTERNAL_MAGIC;
		VSET( ehy_ip->ehy_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( ehy_ip->ehy_H, 0, 0, Viewscale );
		VSET( ehy_ip->ehy_Au, 0, 1, 0 );
		ehy_ip->ehy_r1 = Viewscale*0.5;
		ehy_ip->ehy_r2 = Viewscale*0.25;
		ehy_ip->ehy_c = ehy_ip->ehy_r2;
	} else if( strcmp( argv[2], "eto" ) == 0 ) {
		internal.idb_type = ID_ETO;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_eto_internal) , "rt_eto_internal" );
		eto_ip = (struct rt_eto_internal *)internal.idb_ptr;
		eto_ip->eto_magic = RT_ETO_INTERNAL_MAGIC;
		VSET( eto_ip->eto_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ] );
		VSET( eto_ip->eto_N, 0, 0, 1 );
		VSET( eto_ip->eto_C, Viewscale*0.1, 0, Viewscale*0.1 );
		eto_ip->eto_r = Viewscale*0.5;
		eto_ip->eto_rd = Viewscale*0.05;
	} else if( strcmp( argv[2], "part" ) == 0 ) {
		internal.idb_type = ID_PARTICLE;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_part_internal) , "rt_part_internal" );
		part_ip = (struct rt_part_internal *)internal.idb_ptr;
		part_ip->part_magic = RT_PART_INTERNAL_MAGIC;
		VSET( part_ip->part_V, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale*0.5 );
		VSET( part_ip->part_H, 0, 0, Viewscale );
		part_ip->part_vrad = Viewscale*0.5;
		part_ip->part_hrad = Viewscale*0.25;
		part_ip->part_type = RT_PARTICLE_TYPE_CONE;
	} else if( strcmp( argv[2], "nmg" ) == 0 ) {
		struct model *m;
		struct nmgregion *r;
		struct shell *s;

		m = nmg_mm();
		r = nmg_mrsv( m );
		s = BU_LIST_FIRST( shell , &r->s_hd );
		nmg_vertex_g( s->vu_p->v_p, -toViewcenter[MDX], -toViewcenter[MDY], -toViewcenter[MDZ]);
		(void)nmg_meonvu( s->vu_p );
		(void)nmg_ml( s );
		internal.idb_type = ID_NMG;
		internal.idb_ptr = (genptr_t)m;
	} else if( strcmp( argv[2], "pipe" ) == 0 ) {
		struct wdb_pipept *ps;

		internal.idb_type = ID_PIPE;
		internal.idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal" );
		pipe_ip = (struct rt_pipe_internal *)internal.idb_ptr;
		pipe_ip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
		BU_LIST_INIT( &pipe_ip->pipe_segs_head );
		BU_GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]-Viewscale );
		ps->pp_od = 0.5*Viewscale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		BU_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
		BU_GETSTRUCT( ps, wdb_pipept );
		ps->l.magic = WDB_PIPESEG_MAGIC;
		VSET( ps->pp_coord, -toViewcenter[MDX] , -toViewcenter[MDY] , -toViewcenter[MDZ]+Viewscale );
		ps->pp_od = 0.5*Viewscale;
		ps->pp_id = 0.5*ps->pp_od;
		ps->pp_bendradius = ps->pp_od;
		BU_LIST_INSERT( &pipe_ip->pipe_segs_head, &ps->l );
	} else if( strcmp( argv[2], "ars" ) == 0 ||
		   strcmp( argv[2], "poly" ) == 0 ||
		   strcmp( argv[2], "ebm" ) == 0 ||
		   strcmp( argv[2], "vol" ) == 0 ||
		   strcmp( argv[2], "arbn" ) == 0 ||
		   strcmp( argv[2], "nurb" ) == 0 ||
		   strcmp( argv[2], "spline" ) == 0 )  {
	  Tcl_AppendResult(interp, "make ", argv[2], " not implimented yet\n", (char *)NULL);
	  return TCL_ERROR;
	} else {
	  Tcl_AppendResult(interp, "make:  ", argv[2], " is not a known primitive\n",
			   "\tchoices are: arb8, arb7, arb6, arb5, arb4, sph, ell, ellg, grip, tor,\n",
			   "\t\ttgc, tec, rec, trc, rcc, half, rpc, rhc, epa, ehy, eto, part\n",
			   (char *)NULL);
	  return TCL_ERROR;
	}

	if( rt_functab[internal.idb_type].ft_export( &external, &internal, 1.0 ) < 0 )
	{
	  Tcl_AppendResult(interp, "f_make: export failure\n", (char *)NULL);
	  rt_functab[internal.idb_type].ft_ifree( &internal );
	  return TCL_ERROR;
	}
	rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	ngran = (external.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
	if( (dp = db_diradd( dbip, argv[1], -1L, ngran, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )
	    {
	    	db_free_external( &external );
	    	TCL_ALLOC_ERR_return;
	    }

	if (db_put_external( &external, dp, dbip ) < 0 )
	{
		db_free_external( &external );
		TCL_WRITE_ERR_return;
	}
	db_free_external( &external );

	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = argv[1]; /* depends on name being in argv[1] */
	  av[2] = NULL;

	  /* draw the "made" solid */
	  return f_edit( clientData, interp, 2, av );
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
#if 0
    dmp->dm_light( dmp, LIGHT_ON, BE_O_ROTATE );
    dmp->dm_light( dmp, LIGHT_OFF, BE_O_SCALE );
    dmp->dm_light( dmp, LIGHT_OFF, BE_O_XY );
#endif
    movedir = ROTARROW;
  }

  /* find point for rotation to take place wrt */
  MAT4X3PNT(model_pt, es_mat, es_keypoint);
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
  bn_mat_idn(temp);
  MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
  temp[15] = modelchanges[15];
  bn_mat_copy(modelchanges, temp);

  /* build new rotation matrix */
  bn_mat_idn(temp);
  buildHrot(temp,
	    argvect[0]*degtorad,
	    argvect[1]*degtorad,
	    argvect[2]*degtorad );

  if(iflag){
    /* apply accumulated rotations */
    bn_mat_mul2(acc_rot_sol, temp);
  }

  /*XXX*/ bn_mat_copy(acc_rot_sol, temp); /* used to rotate solid/object axis */
  
  /* Record the new rotation matrix into the revised
   *	modelchanges matrix wrt "point"
   */
  wrt_point(modelchanges, temp, modelchanges, point);

  new_mats();

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
  mat_t temp;
  vect_t s_point, point, v_work, model_pt;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( not_state( ST_O_EDIT, "Object Scaling" ) )
	  return TCL_ERROR;

	if( atof(argv[1]) <= 0.0 ) {
	  Tcl_AppendResult(interp, "ERROR: scale factor <=  0\n", (char *)NULL);
	  return TCL_ERROR;
	}

	update_views = 1;

	if(movedir != SARROW) {
		/* Put in global object scale mode */
#if 0
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_ROTATE );
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_XY );
#endif
		if( edobj == 0 )
			edobj = BE_O_SCALE;	/* default is global scaling */
#if 0
		dmp->dm_light( dmp, LIGHT_ON, edobj );
#endif
		movedir = SARROW;
	}

	bn_mat_idn(incr);

	/* switch depending on type of scaling to do */
	switch( edobj ) {

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
	MAT4X3PNT(temp, es_mat, es_keypoint);
	MAT4X3PNT(point, modelchanges, temp);

	wrt_point(modelchanges, incr, modelchanges, point);
	new_mats();

	return TCL_OK;
}

/* allow precise changes to object translation */
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( not_state( ST_O_EDIT, "Object Translation") )
	  return TCL_ERROR;

	update_views = 1;

	bn_mat_idn(incr);
	bn_mat_idn(old);

	if( (movedir & (RARROW|UARROW)) == 0 ) {
		/* put in object trans mode */
#if 0
		dmp->dm_light( dmp, LIGHT_ON, BE_O_XY );
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_SCALE );
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_ROTATE );
#endif
		movedir = UARROW | RARROW;
	}

	for(i=0; i<3; i++) {
		new_vertex[i] = atof(argv[i+1]) * local2base;
	}
	MAT4X3PNT(model_sol_pt, es_mat, es_keypoint);
	MAT4X3PNT(ed_sol_pt, modelchanges, model_sol_pt);
	VSUB2(model_incr, new_vertex, ed_sol_pt);
	MAT_DELTAS(incr, model_incr[0], model_incr[1], model_incr[2]);
	bn_mat_copy(old,modelchanges);
	bn_mat_mul(modelchanges, incr, old);
	new_mats();

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  dmaflag = 1;
  item_default = atoi(argv[1]);

  if(argc == 2)
    return TCL_OK;

  air_default = atoi(argv[2]);
  if(air_default) 
    item_default = 0;

  if(argc == 3)
    return TCL_OK;

  los_default = atoi(argv[3]);

  if(argc == 4)
    return TCL_OK;

  mat_default = atoi(argv[4]);

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

	if( db_lookup( dbip,  newname, LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( newname );
		/* Free memory here */
		nmg_km(m);
		frac_stat = 1;
		return;
	}

	if( (new_dp=db_diradd( dbip, newname, -1, 0, DIR_SOLID)) == DIR_NULL )  {
	    	TCL_ALLOC_ERR;
		return;
	}

	/* make sure the geometry/bounding boxes are up to date */
	for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
		nmg_region_a(r, &mged_tol);


	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&new_intern);
	new_intern.idb_type = ID_NMG;
	new_intern.idb_ptr = (genptr_t)m;

	if( rt_db_put_internal( new_dp, dbip, &new_intern ) < 0 )  {
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	Tcl_AppendResult(interp, "fracture:", (char *)NULL);
	for (i=0 ; i < argc ; i++)
		Tcl_AppendResult(interp, " ", argv[i], (char *)NULL);
	Tcl_AppendResult(interp, "\n", (char *)NULL);

	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &old_intern, old_dp, dbip, bn_mat_identity ) < 0 )  {
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
	  return TCL_ERROR;

	if(movedir != ROTARROW) {
		/* NOT in object rotate mode - put it in obj rot */
#if 0
		dmp->dm_light( dmp, LIGHT_ON, BE_O_ROTATE );
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_SCALE );
		dmp->dm_light( dmp, LIGHT_OFF, BE_O_XY );
#endif
		movedir = ROTARROW;
	}
	VSET(specified_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
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
	bn_mat_idn(temp);
	MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
	temp[15] = modelchanges[15];
	bn_mat_copy(modelchanges, temp);

	/* build new rotation matrix */
	bn_mat_idn(temp);
	buildHrot(temp, 0.0, 0.0, atof(argv[7])*degtorad);

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point_direc(modelchanges, temp, modelchanges, point, direc);

	new_mats();

	return TCL_OK;
}
