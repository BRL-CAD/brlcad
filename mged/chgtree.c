/*
 *			C H G T R E E
 *
 * This module contains functions which change the tree structure
 * of the model, and delete solids or combinations or combination elements.
 *
 * Functions -
 *	f_name		rename an object
 *	f_copy		copy a solid
 *	f_instance	create an instance of something
 *	f_region	add solids to a region or create the region
 *	f_kill		remove an object or several from the description
 *	f_group		"grouping" command
 *	f_rm		delete members of a combination
 *	f_copy_inv	copy cyl and position at "end" of original cyl
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A Applin
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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./mgedtcl.h"

extern struct db_tree_state	mged_initial_tree_state;	/* dodraw.c */
extern struct rt_tol		mged_tol;	/* from ged.c */
extern struct rt_tess_tol	mged_ttol;	/* XXX needs to replace mged_abs_tol, et.al. from dodraw.c */

void	aexists();

/* Rename an object */
/* Format: n oldname newname	*/
int
f_name(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, argv[2] ) < 0 ||
	    db_get( dbip,  dp, &record, 0 , 1) < 0 )  {
	  Tcl_AppendResult(interp, "error in rename to ", argv[2],
			   ", aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return TCL_ERROR;
	}

	/* Change name in the file */
	NAMEMOVE( argv[2], record.c.c_name );
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ){
	  TCL_WRITE_ERR_return;
	}

	return TCL_OK;
}

/* Copy an object */
/* Format: cp oldname newname	*/
int
f_copy(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *proto;
	register struct directory *dp;
	struct rt_external external;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	if( db_get_external( &external , proto , dbip ) ) {
	  TCL_READ_ERR_return;
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp=db_diradd( dbip, argv[2], -1, proto->d_len, proto->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
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
	  av[1] = argv[2]; /* depends on solid name being in argv[2] */
	  av[2] = NULL;

	  /* draw the new object */
	  return f_edit( clientData, interp, 2, av );
	}
}

/* Create an instance of something */
/* Format: i object combname [op]	*/
int
f_instance(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	char oper;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	oper = UNION;
	if( argc == 4 )
		oper = argv[3][0];
	if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "bad operation: %c\n", oper );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return TCL_ERROR;
	}
	if( combadd( dp, argv[2], 0, oper, 0, 0 ) == DIR_NULL )
	  return TCL_ERROR;

	return TCL_OK;
}

/* add solids to a region or create the region */
/* and then add solids */
/* Format: r regionname opr1 sol1 opr2 sol2 ... oprn soln */
int
f_region(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int i;
	int ident, air;
	char oper;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

 	ident = item_default;
 	air = air_default;
 
	/* Check for even number of arguments */
	if( argc & 01 )  {
	  Tcl_AppendResult(interp, "error in number of args!\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( db_lookup( dbip, argv[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* will attempt to create the region */
		if(item_default) {
		  struct bu_vls tmp_vls;

		  item_default++;
		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "Defaulting item number to %d\n", item_default);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}
	}

	/* Get operation and solid name for each solid */
	for( i = 2; i < argc; i += 2 )  {
		if( argv[i][1] != '\0' )  {
		  Tcl_AppendResult(interp, "bad operation: ", argv[i],
				   " skip member: ", argv[i+1], "\n", (char *)NULL);
		  continue;
		}
		oper = argv[i][0];
		if( (dp = db_lookup( dbip,  argv[i+1], LOOKUP_NOISY )) == DIR_NULL )  {
		  Tcl_AppendResult(interp, "skipping ", argv[i+1], "\n", (char *)NULL);
		  continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  continue;
		}

		if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		  TCL_READ_ERR_return;
		}
		
		if( record.u_id == ID_COMB ) {
		  if( record.c.c_flags == 'R' ) {
		    Tcl_AppendResult(interp, "Note: ", dp->d_namep,
				     " is a region\n", (char *)NULL);
		  }
		}

		if( combadd( dp, argv[1], 1, oper, ident, air ) == DIR_NULL )  {
		  Tcl_AppendResult(interp, "error in combadd\n", (char *)NULL);
		  return TCL_ERROR;
		}
	}

	if( db_lookup( dbip, argv[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* failed to create region */
		if(item_default > 1)
			item_default--;
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *			F _ C O M B
 *
 *  Create or add to the end of a combination, with one or more solids,
 *  with explicitly specified operations.
 *
 *  Format: comb comb_name sol1 opr2 sol2 ... oprN solN
 */
int
f_comb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	char	*comb_name;
	register int	i;
	char	oper;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* Check for odd number of arguments */
	if( argc & 01 )  {
	  Tcl_AppendResult(interp, "error in number of args!\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Save combination name, for use inside loop */
	comb_name = argv[1];

	/* Get operation and solid name for each solid */
	for( i = 2; i < argc; i += 2 )  {
		if( argv[i][1] != '\0' )  {
		  Tcl_AppendResult(interp, "bad operation: ", argv[i],
				   " skip member: ", argv[i+1], "\n", (char *)NULL);
		  continue;
		}
		oper = argv[i][0];
		if( (dp = db_lookup( dbip,  argv[i+1], LOOKUP_NOISY )) == DIR_NULL )  {
		  Tcl_AppendResult(interp, "skipping ", argv[i+1], "\n", (char *)NULL);
		  continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			continue;
		}

		if( combadd( dp, comb_name, 0, oper, 0, 0 ) == DIR_NULL )  {
		  Tcl_AppendResult(interp, "error in combadd\n", (char *)NULL);
		  return TCL_ERROR;
		}
	}

	if( db_lookup( dbip, comb_name, LOOKUP_QUIET) == DIR_NULL ) {
	  Tcl_AppendResult(interp, "Error:  ", comb_name,
			   " not created\n", (char *)NULL);
	  return TCL_ERROR;
	}

	return TCL_OK;
}

/* Remove an object or several from the description */
/* Format: kill [-f] object1 object2 .... objectn	*/
int
f_kill(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;
	int	is_phony;
	int	verbose = LOOKUP_NOISY;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc > 1 && strcmp( argv[1], "-f" ) == 0 )  {
		verbose = LOOKUP_QUIET;
		argc--;
		argv++;
	}

	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], verbose )) != DIR_NULL )  {
			is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);
			eraseobj( dp );
			/* eraseobj() does db_dirdelete() on phony entries, don't re-do. */
			if( is_phony )  continue;

			if( db_delete( dbip, dp ) < 0 ||
			    db_dirdelete( dbip, dp ) < 0 )  {
			  /* Abort kill processing on first error */
			  TCL_DELETE_ERR_return(argv[i]);
			}
		}
	}
	return TCL_OK;
}

/* Grouping command */
/* Format: g groupname object1 object2 .... objectn	*/
int
f_group(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* get objects to add to group */
	for( i = 2; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY)) != DIR_NULL )  {
			if( combadd( dp, argv[1], 0,
				     UNION, 0, 0) == DIR_NULL )
			  return TCL_ERROR;
		}  else
		  Tcl_AppendResult(interp, "skip member ", argv[i], "\n", (char *)NULL);
	}
	return TCL_OK;
}

/* Delete members of a combination */
/* Format: D comb memb1 memb2 .... membn	*/
int
f_rm(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i, rec, num_deleted;
	union record record;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	/* Examine all the Member records, one at a time */
	num_deleted = 0;
top:
	for( rec = 1; rec < dp->d_len; rec++ )  {
		if( db_get( dbip,  dp, &record, rec , 1) < 0 ) {
		  TCL_READ_ERR_return;
		}
		/* Compare this member to each command arg */
		for( i = 2; i < argc; i++ )  {
		  if( strcmp( argv[i], record.M.m_instname ) != 0 )
		    continue;

		  Tcl_AppendResult(interp, "deleting member ", argv[i],
				   "\n", (char *)NULL);
		  if( db_delrec( dbip, dp, rec ) < 0 )  {
		    Tcl_AppendResult(interp, "Error in deleting member.\n", (char *)NULL);
		    TCL_ERROR_RECOVERY_SUGGESTION;
		    return TCL_ERROR;
		  }
		  num_deleted++;
		  goto top;
		}
	}

	return TCL_OK;
}

/* Copy a cylinder and position at end of original cylinder

 *	Used in making "wires"
 *
 * Format: cpi old new
 */
int
f_copy_inv(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *proto;
	register struct directory *dp;
	struct rt_external external;
	struct rt_db_internal internal;
	struct rt_tgc_internal *tgc_ip;
	int id;
	int ngran;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	/* get external representation of slid to be copied */
	if( db_get_external( &external, proto, dbip )) {
	  TCL_READ_ERR_return;
	}

	/* make sure it is a TGC */
	id = rt_id_solid( &external );
	if( id != ID_TGC )
	{
	  Tcl_AppendResult(interp, "f_copy_inv: ", argv[1],
			   " is not a cylinder\n", (char *)NULL);
	  db_free_external( &external );
	  return TCL_ERROR;
	}

	/* import the TGC */
	if( rt_functab[id].ft_import( &internal , &external , rt_identity ) < 0 )
	{
	  Tcl_AppendResult(interp, "f_copy_inv: import failure for ",
			   argv[1], "\n", (char *)NULL);
	  return TCL_ERROR;
	}
	db_free_external( &external );

	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

	/* translate to end of "original" cylinder */
	VADD2( tgc_ip->v , tgc_ip->v , tgc_ip->h );

	/* now export the new TGC */
	if( rt_functab[internal.idb_type].ft_export( &external, &internal, (double)1.0 ) < 0 )
	{
	  Tcl_AppendResult(interp, "f_copy_inv: export failure\n", (char *)NULL);
	  rt_functab[internal.idb_type].ft_ifree( &internal );
	  return TCL_ERROR;
	}
	rt_functab[internal.idb_type].ft_ifree( &internal );   /* free internal rep */

	/*
	 * Update the disk record
	 */

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	ngran = (external.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
	if( (dp = db_diradd( dbip, argv[2], -1L, ngran, DIR_SOLID)) == DIR_NULL ||
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
	  av[1] = argv[2]; /* depends on solid name being in argv[2] */
	  av[2] = NULL;

	  /* draw the new solid */
	  (void)f_edit( clientData, interp, 2, av );
	}

	if(state == ST_VIEW) {
	  char *av[3];

	  av[0] = "sed";
	  av[1] = argv[2];  /* new name in argv[2] */
	  av[2] = NULL;

	  /* solid edit this new cylinder */
	  (void)f_sed( clientData, interp, 2, av );
	}

	return TCL_OK;
}

/* XXX Move to raytrace.h */
BU_EXTERN(struct animate	*db_parse_1anim, (struct db_i *dbip,
				int argc, CONST char **argv));

/*
 *			F _ A R C E D
 *
 *  Allow editing of the matrix, etc., along an arc in the database
 *  from the command line, when NOT in an edit state.
 *  Used mostly to facilitate shell script writing.
 *
 *  Syntax:  arced a/b ("anim" command)
 *	arced a/b matrix rmul translate dx dy dz
 *
 *  Extensions should be formulated along the lines of the "anim"
 *  commands from the script language.
 */
int
f_arced(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct animate		*anp;
	struct directory	*dp;
	mat_t			stack;
	mat_t			arc;
	union record		*rec;
	int			i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( not_state( ST_VIEW, "Arc edit" ) )  return TCL_ERROR;

	if( !strchr( argv[1], '/' ) )  {
	  Tcl_AppendResult(interp, "arced: bad path specification '", argv[1],
			   "'\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( !(anp = db_parse_1anim( dbip, argc, (CONST char **)argv ) ) )  {
	  Tcl_AppendResult(interp, "arced: unable to parse command\n", (char *)NULL);
	  return TCL_ERROR;
	}
#if 0
	if( anp->an_path.fp_len != 2 )  {
		char	*thepath = db_path_to_string( &(anp->an_path) );
		bu_log("arced: '%s' is not a 2-element path spec\n");
		bu_free( thepath, "path" );
		db_free_1anim( anp );
		return TCL_ERROR;
	}
#endif

#if 0
	db_write_anim(stdout, anp);	/* XXX temp */
#endif

	/* Only the matrix rarc, lmul, and rmul directives are useful here */
	mat_idn( stack );

	/* Load the combination into memory */
	dp = anp->an_path.fp_names[0];
	if( (rec = db_getmrec( dbip, dp )) == (union record *)NULL )  {
	  TCL_READ_ERR;
	  db_free_1anim( anp );
	  return TCL_ERROR;
	}
	if( rec[0].u_id != ID_COMB )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  goto fail;
	}

	/* Search for first mention of arc */
	for( i=1; i < dp->d_len; i++ )  {
		if( rec[i].u_id != ID_MEMB )  {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "%s: element %d not a member! Database corrupted.\n",
				dp->d_namep, i);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  goto fail;
		}
		if( strncmp( rec[i].M.m_instname,
		    anp->an_path.fp_names[1]->d_namep,
		    NAMESIZE ) != 0 )  continue;

		/* Found match */
		/* XXX For future, also import/export materials here */
		rt_mat_dbmat( arc, rec[i].M.m_mat );

		if( db_do_anim( anp, stack, arc, NULL ) < 0 )  {
			goto fail;
		}
		rt_dbmat_mat( rec[i].M.m_mat, arc );

		/* Write back */
		if( db_put( dbip, dp, rec, 0, dp->d_len ) < 0 )  {
		  Tcl_AppendResult(interp, "arced: write error, aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  goto fail;
		}
		bu_free( (genptr_t)rec, "union record []");
		db_free_1anim( anp );
		return TCL_OK;
	}

	Tcl_AppendResult(interp, "Unable to find instance of '",
			 anp->an_path.fp_names[1]->d_namep, "' in combination '",
			 anp->an_path.fp_names[0]->d_namep, "', error\n", (char *)NULL);
		
fail:
	bu_free( (genptr_t)rec, "union record []");
	db_free_1anim( anp );
	return TCL_ERROR;
}

/*
 *			P A T H L I S T _ L E A F _ F U N C
 */
static union tree *
pathlist_leaf_func( tsp, pathp, ext, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ext;
int			id;
{
	char	*str;

	RT_CK_FULL_PATH( pathp );

	str = db_path_to_string( pathp );

	Tcl_AppendElement( interp, str );

	bu_free( (genptr_t)str, "path string" );
	return TREE_NULL;
}

/*
 *			C M D _ P A T H L I S T
 *
 *  Given the name(s) of a database node(s), return a TCL list of all
 *  paths from there to each leaf below it.
 *
 *  Similar to the "tree" command, only suitable for programs, not humans.
 */
int
cmd_pathlist(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char	        **argv;
{
	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

	if( db_walk_tree( dbip, argc-1, (CONST char **)argv+1, 1,
			 &mged_initial_tree_state,
			 0, 0, pathlist_leaf_func ) < 0 )  {
	    	Tcl_AppendResult(interp, "db_walk_tree() error", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *			F I N D _ S O L I D _ W I T H _ P A T H
 */
struct solid *
find_solid_with_path( pathp )
register struct db_full_path	*pathp;
{
	register struct solid	*sp;
	int			count = 0;
	struct solid		*ret = (struct solid *)NULL;

	RT_CK_FULL_PATH(pathp);

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		if( pathp->fp_len != sp->s_last+1 )
			continue;

		if( bcmp( (char *)&pathp->fp_names[0],
			  (char *)&sp->s_path[0],
			  pathp->fp_len * sizeof(struct directory *)
		        ) != 0 )  continue;
		/* Paths are the same */
		ret = sp;
		count++;
	}
	if( count > 1 ){
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls,"find_solid_with_path() found %d matches\n", count);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	return ret;
}

/*
 *			C M D _ O E D
 *
 *  Transition from VIEW state to OBJECT EDIT state in a single command,
 *  rather than requiring "press oill", "ill leaf", "matpick a/b".
 *
 *  Takes two parameters which when combined represent the full path to
 *  the reference solid of the object to be edited.
 *  The conceptual slash between the two strings signifies which
 *  matrix is to be edited.
 */
int
cmd_oed(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char      	**argv;
{
	struct db_full_path	lhs;
	struct db_full_path	rhs;
	struct db_full_path	both;
	char			*new_argv[4];
	char			number[32];

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( not_state( ST_VIEW, "Object Illuminate" ) )  {
		Tcl_AppendResult(interp, "MGED state is not VIEW", (char *)NULL);
		return TCL_ERROR;
	}
	if(BU_LIST_IS_EMPTY(&HeadSolid.l)) {
		Tcl_AppendResult(interp, "no solids in view", (char *)NULL);
		return TCL_ERROR;
	}

	if( db_string_to_path( &lhs, dbip, argv[1] ) < 0 )  {
		Tcl_AppendResult(interp, "bad lhs path", (char *)NULL);
		return TCL_ERROR;
	}
	if( db_string_to_path( &rhs, dbip, argv[2] ) < 0 )  {
		db_free_full_path( &lhs );
		Tcl_AppendResult(interp, "bad rhs path", (char *)NULL);
		return TCL_ERROR;
	}
	if( rhs.fp_len <= 0 )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		Tcl_AppendResult(interp, "rhs must not be null", (char *)NULL);
		return TCL_ERROR;
	}

	db_full_path_init( &both );
	db_dup_full_path( &both, &lhs );
	db_append_full_path( &both, &rhs );
#if 0
	db_pr_full_path( "lhs ", &lhs );
	db_pr_full_path( "rhs ", &rhs );
	db_pr_full_path( "both", &both);
#endif

	/* Patterned after  ill_common() ... */
	illump = BU_LIST_NEXT(solid, &HeadSolid.l);/* any valid solid would do */
	edobj = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	mat_idn( modelchanges );	/* No changes yet */
	(void)chg_state( ST_VIEW, ST_O_PICK, "internal change of state");
	/* reset accumulation local scale factors */
	acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;
	new_mats();

	/* Find the one solid, set s_iflag UP, point illump at it */
	illump = find_solid_with_path( &both );
	if( !illump )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "Unable to find solid matching path", (char *)NULL);
		illump = 0;
		(void)chg_state( ST_O_PICK, ST_VIEW, "error recovery");
		return TCL_ERROR;
	}
	(void)chg_state( ST_O_PICK, ST_O_PATH, "internal change of state");

	/* Select the matrix */
#if 0
	bu_log("matpick %d\n", lhs.fp_len);
#endif
	sprintf( number, "%d", lhs.fp_len );
	new_argv[0] = "matpick";
	new_argv[1] = number;
	new_argv[2] = NULL;
	if( f_matpick( clientData, interp, 2, new_argv ) != TCL_OK )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "error detected inside f_matpick", (char *)NULL);
		return TCL_ERROR;
	}
	if( not_state( ST_O_EDIT, "Object EDIT" ) )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "MGED state did not advance to Object EDIT", (char *)NULL);
		return TCL_ERROR;
	}
	db_free_full_path( &lhs );
	db_free_full_path( &rhs );
	db_free_full_path( &both );
	return TCL_OK;
}

/*
 *			F _ P U T M A T
 *
 *	Replace the matrix on an arc in the database from the command line,
 *	when NOT in an edit state.  Used mostly to facilitate writing shell
 *	scripts.  There are two valid syntaxes, each of which is implemented
 *	as an appropriate call to f_arced.  Commands of the form
 *
 *		    putmat a/b m0 m1 ... m15
 *
 *	are converted to
 *
 *		arced a/b matrix rarc m0 m1 ... m15,
 *
 *	while commands of the form
 *
 *			    putmat a/b I
 *
 *	are converted to
 *
 *	    arced a/b matrix rarc 1 0 0 0   0 1 0 0   0 0 1 0   0 0 0 1.
 *
 */
int
f_putmat (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;

{
    int			i;			/* Dummy loop index */
    int			result = TCL_OK;	/* Return code */
    char		*ep;			/* Matrix element */
    char		*eep;			/* End of element */
    char		*newargv[20];
    struct bu_vls	*avp;

    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

    if (not_state( ST_VIEW, "Command-line matrix replace"))
      return TCL_ERROR;

    if (!strchr( argv[1], '/'))
    {
      Tcl_AppendResult(interp, "putmat: bad path specification '",
		       argv[1], "'\n", (char *)NULL);
      return TCL_ERROR;
    }
    switch (argc)
    {
	case 18:
	    avp = bu_vls_vlsinit();
	    bu_vls_from_argv(avp, 16, argv + 2);
	    break;
	case 3:
	    if ((argv[2][0] == 'I') && (argv[2][1] == '\0'))
	    {
		avp = bu_vls_vlsinit();
		bu_vls_printf(avp, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 ");
		break;
	    }
	    /* This case falls through intentionally */
	default:
	  Tcl_AppendResult(interp, "error in matrix specification\n", (char *)NULL);
	  return TCL_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    ep = bu_vls_addr(avp);
    for (i = 0; i < 16; ++i)
    {
	if ((eep = strchr(ep, ' ')) == NULL)
	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "%s:%d: bad matrix.  This shouldn't happen\n",
			__FILE__, __LINE__);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  result = TCL_ERROR;
	  break;
	}
	newargv[4 + i] = ep;
	*eep = '\0';
	ep = eep + 1;
    }

    if (result != TCL_ERROR)
	result = f_arced(clientData, interp, 20, newargv);

    bu_vls_vlsfree(avp);
    return result;
}
