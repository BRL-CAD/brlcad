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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "./sedit.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./mgedtcl.h"

extern void solid_list_callback(); /* chgview.c */
extern struct db_tree_state	mged_initial_tree_state;	/* dodraw.c */
extern struct bn_tol		mged_tol;	/* from ged.c */
extern struct rt_tess_tol	mged_ttol;	/* XXX needs to replace mged_abs_tol, et.al. from dodraw.c */

void	aexists();

/* Rename an object */
/* Format: mv oldname newname	*/
int
f_name(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register struct directory *dp;
	struct rt_db_internal	intern;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help mv");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}

	/*  Change object name in the in-memory directory. */
	if( db_rename( dbip, dp, argv[2] ) < 0 )  {
		rt_db_free_internal( &intern, &rt_uniresource );
	  Tcl_AppendResult(interp, "error in db_rename to ", argv[2],
			   ", aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return TCL_ERROR;
	}

	/* Re-write to the database.  New name is applied on the way out. */
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
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
	struct bu_external external;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help cp");
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

	if( db_get_external( &external , proto , dbip ) ) {
	  TCL_READ_ERR_return;
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp=db_diradd( dbip, argv[2], -1L, 0, proto->d_flags, NULL)) == DIR_NULL )  {
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
	char oper;
	struct bu_list	head;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	BU_LIST_INIT(&head);

	if(argc < 3 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help i");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( db_lookup( dbip, argv[1], LOOKUP_NOISY ) == DIR_NULL )
	  return TCL_ERROR;

	oper = WMOP_UNION;
	if( argc == 4 )
		oper = argv[3][0];
	if(oper != WMOP_UNION && oper != WMOP_SUBTRACT &&	oper != WMOP_INTERSECT) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "bad operation: %c\n", oper );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return TCL_ERROR;
	}
	mk_addmember( argv[2], &head, oper );

	if( mk_comb( wdbp, argv[1], &head,
	    0, NULL, NULL, NULL,
	    0, 0, 0, 0,
	    0, 1, 1 ) < 0 )
	{
		Tcl_AppendResult(interp,
			"An error has occured while adding '",
			argv[1], "' to the database.\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		return TCL_ERROR;
	}
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
	int i;
	int ident, air;
	char oper;
	struct bu_list head;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	BU_LIST_INIT(&head);

	if(argc < 4){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help r");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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

		if(oper != WMOP_UNION && oper != WMOP_SUBTRACT &&	oper != WMOP_INTERSECT) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  continue;
		}

		/* Adding region to region */
		if( dp->d_flags & DIR_REGION )  {
		    Tcl_AppendResult(interp, "Note: ", dp->d_namep,
				     " is a region\n", (char *)NULL);
		}

		mk_addmember( argv[i+1], &head, oper );
	}

	if( mk_comb( wdbp, argv[1], &head,
	    1, NULL, NULL, NULL,
	    ident, air, mat_default, los_default,
	    0, 1, 1 ) < 0 )
	{
		/* failed to create region */
		if(item_default > 1)
			item_default--;

		Tcl_AppendResult(interp,
			"An error has occured while adding '",
			argv[1], "' to the database.\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
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
	struct bu_list	head;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	BU_LIST_INIT( &head );

	if(argc < 4){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help comb");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Check for odd number of arguments */
	if( argc & 01 )  {
	  Tcl_AppendResult(interp, "error in number of args!\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Save combination name, for use inside loop */
	comb_name = argv[1];
	if( (dp=db_lookup( dbip, comb_name, LOOKUP_QUIET )) != DIR_NULL )
	{
		if( !(dp->d_flags & DIR_COMB))
		{
			Tcl_AppendResult(interp, "ERROR: ", comb_name, " is not a combination\n", (char *)0 );
			return TCL_ERROR;
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

		if(oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			continue;
		}

		/* Add to the list */
		(void)mk_addmember( argv[i+1], &head, oper );
	}

	/* Do them all at once */
	if( mk_comb( wdbp, comb_name, &head,
	    0, NULL, NULL, NULL,
	    0, 0, 0, 0,
	    0, 1, 1 ) < 0 )
	{
		Tcl_AppendResult(interp,
			"An error has occured while adding '",
			comb_name, "' to the database.\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		return TCL_ERROR;
	}
	return TCL_OK;
}

/* Remove an object or several from the description */
/* Format: kill [-f] object1 object2 .... objectn	*/
int
f_kill(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register int		i;
	struct directory	*dp;
	struct directory	*dpp[2] = {DIR_NULL, DIR_NULL};
	int			is_phony;
	int			verbose = LOOKUP_NOISY;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help kill");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( argc > 1 && strcmp( argv[1], "-f" ) == 0 )  {
		verbose = LOOKUP_QUIET;
		argc--;
		argv++;
	}

	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], verbose )) != DIR_NULL )  {
			is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);
			dpp[0] = dp;
			eraseobjall(dpp);
			/* eraseobjall() does db_dirdelete() on phony entries, don't re-do. */
			if( is_phony )  continue;

			if( db_delete( dbip, dp ) < 0 ||
			    db_dirdelete( dbip, dp ) < 0 )  {
			  /* Abort kill processing on first error */
			  TCL_DELETE_ERR_return(argv[i]);
			}
		}
	}

	solid_list_callback();
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
	register int i;
	struct bu_list	head;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	BU_LIST_INIT(&head);

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help g");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* get objects to add to group */
	for( i = 2; i < argc; i++ )  {
		if( db_lookup( dbip,  argv[i], LOOKUP_NOISY) != DIR_NULL )  {
			/* Add to list */
			(void)mk_addmember( argv[i], &head, WMOP_UNION );
		}  else
		  Tcl_AppendResult(interp, "skip member ", argv[i], "\n", (char *)NULL);
	}

	/* Do them all at once */
	if( mk_comb( wdbp, argv[1], &head,
	    0, NULL, NULL, NULL,
	    0, 0, 0, 0,
	    0, 1, 1 ) < 0 )
	{
		Tcl_AppendResult(interp,
			"An error has occured while adding '",
			argv[1], "' to the database.\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		return TCL_ERROR;
	}
	return TCL_OK;
}

/* Delete members of a combination */
/* Format: rm comb memb1 memb2 .... membn	*/
int
f_rm(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int	i;
	int		num_deleted;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help rm");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( (dp->d_flags & DIR_COMB) == 0 )  {
		Tcl_AppendResult(interp, "rm: ", dp->d_namep,
			" is not a combination\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	/* Process each argument */
	num_deleted = 0;
	ret = TCL_OK;
	for( i = 2; i < argc; i++ )  {
		if( db_tree_del_dbleaf( &(comb->tree), argv[i], &rt_uniresource ) < 0 )  {
			Tcl_AppendResult(interp, "  ERROR_deleting ",
				dp->d_namep, "/", argv[i],
				"\n", (char *)NULL);
			ret = TCL_ERROR;
		} else {
			Tcl_AppendResult(interp, "deleted ",
				dp->d_namep, "/", argv[i],
				"\n", (char *)NULL);
			num_deleted++;
		}
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}
	return ret;
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
	struct rt_db_internal internal;
	struct rt_tgc_internal *tgc_ip;
	int id;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help cpi");
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

	if( (id = rt_db_get_internal( &internal, proto, dbip, (fastf_t *)NULL, &rt_uniresource )) < 0 )  {
		TCL_READ_ERR_return;
	}
	/* make sure it is a TGC */
	if( id != ID_TGC )
	{
	  Tcl_AppendResult(interp, "f_copy_inv: ", argv[1],
			   " is not a cylinder\n", (char *)NULL);
		rt_db_free_internal( &internal, &rt_uniresource );
		return TCL_ERROR;
	}
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

	/* translate to end of "original" cylinder */
	VADD2( tgc_ip->v , tgc_ip->v , tgc_ip->h );

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, argv[2], -1L, 0, proto->d_flags, NULL)) == DIR_NULL )  {
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
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	union tree		*tp;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help arced");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
	if( anp->an_path.fp_len < 2 )  {
		Tcl_AppendResult(interp, "arced: path spec has insufficient elements\n", (char *)NULL);
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
	bn_mat_idn( stack );

	/* Load the combination into memory */
	dp = anp->an_path.fp_names[anp->an_path.fp_len-2];
	RT_CK_DIR(dp);
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		db_free_1anim( anp );
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	if( !comb->tree )  {
		Tcl_AppendResult(interp, dp->d_namep, ": empty combination\n", (char *)NULL);
		goto fail;
	}

	/* Search for first mention of arc */
	if( (tp = db_find_named_leaf( comb->tree, anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep )) == TREE_NULL )  {
		Tcl_AppendResult(interp, "Unable to find instance of '",
			anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep,
			"' in combination '",
			anp->an_path.fp_names[anp->an_path.fp_len-2]->d_namep,
			"', error\n", (char *)NULL);
		goto fail;
	}

	/* Found match.  Update tl_mat in place. */
	if( !tp->tr_l.tl_mat )
		tp->tr_l.tl_mat = bn_mat_dup( bn_mat_identity );

	if( db_do_anim( anp, stack, tp->tr_l.tl_mat, NULL ) < 0 )  {
		goto fail;
	}

	if( bn_mat_is_identity( tp->tr_l.tl_mat ) )  {
		bu_free( (genptr_t)tp->tr_l.tl_mat, "tl_mat" );
		tp->tr_l.tl_mat = (matp_t)NULL;
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR;
		goto fail;
	}
	db_free_1anim( anp );
	return TCL_OK;
		
fail:
	rt_db_free_internal( &intern, &rt_uniresource );
	db_free_1anim( anp );
	return TCL_ERROR;
}

/*
 *			P A T H L I S T _ L E A F _ F U N C
 */
static union tree *
pathlist_leaf_func( tsp, pathp, ip, client_data )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_db_internal	*ip;
genptr_t		client_data;
{
	char	*str;

	RT_CK_FULL_PATH( pathp );
	RT_CK_DB_INTERNAL(ip);

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
  CHECK_DBI_NULL;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help pathlist");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  mged_initial_tree_state.ts_ttol = &mged_ttol;
  mged_initial_tree_state.ts_tol = &mged_tol;

  if( db_walk_tree( dbip, argc-1, (CONST char **)argv+1, 1,
		    &mged_initial_tree_state,
		    0, 0, pathlist_leaf_func, (genptr_t)NULL ) < 0 )  {
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
	int			i;
	struct solid		*ret = (struct solid *)NULL;

	RT_CK_FULL_PATH(pathp);

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		int not_this_solid=0;

		if( pathp->fp_len != sp->s_last+1 )
			continue;

		for( i=0 ; i<pathp->fp_len ; i++ )
		{
			if( pathp->fp_names[i] != sp->s_path[i] )
			{
				not_this_solid = 1;
				break;
			}
		}

		if( not_this_solid )
			continue;

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

	CHECK_DBI_NULL;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help oed");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
	bn_mat_idn( modelchanges );	/* No changes yet */
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
    int			result = TCL_OK;	/* Return code */
    char		*newargv[20+2];
    struct bu_vls	*avp;
    int			got;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 3 || 18 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help putmat");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

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
    	    /* Sometimes the matrix is sent through Tcl as one long string.
    	     * Copy it so we can crack it, below.
    	     */
	    avp = bu_vls_vlsinit();
    	    bu_vls_strcat(avp, argv[2]);
    	    break;
	default:
	  Tcl_AppendResult(interp, "putmat: error in matrix specification (wrong number of args)\n", (char *)NULL);
	  return TCL_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    /* 16+1 allows space for final NULL entry in argv[] */
    got = bu_argv_from_string( &newargv[4], 16+1, bu_vls_addr(avp) );
    if( got != 16 )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "putmat: %s:%d: bad matrix, only got %d elements: %s\n",
			__FILE__, __LINE__, got, bu_vls_addr(&tmp_vls));
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    	  bu_vls_free(&tmp_vls);
	  result = TCL_ERROR;
    }

    if (result != TCL_ERROR)
	result = f_arced(clientData, interp, 20, newargv);

    bu_vls_vlsfree(avp);
    return result;
}

/*
 *			F _ C O P Y M A T
 *
 *	Copy the matrix on one arc in the database to another arc.
 *
 */
int
f_copymat(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    char			*child;
    char			*parent;
    struct bu_vls		pvls;
    int				i;
    int				sep;
    int				status;
    struct db_tree_state	ts;
    struct directory		*dp;
    struct rt_comb_internal	*comb;
    struct rt_db_internal	intern;
    struct animate		*anp;
    union tree			*tp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 3 || 3 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help copymat");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if (not_state( ST_VIEW, "Command-line matrix copy"))
	return TCL_ERROR;

    /*
     *	Ensure that each argument contains exactly one slash
     */
    for (i = 1; i <= 2; ++i)
	if (((child = strchr(argv[i], '/')) == NULL)
	 || (strchr(++child, '/') != NULL))
	{
	    Tcl_AppendResult(interp,
		"copymat: bad arc: '", argv[i], "'\n", (char *) NULL);
	    return TCL_ERROR;
	}

    BU_GETSTRUCT(anp, animate);
    anp -> magic = ANIMATE_MAGIC;

    ts = mged_initial_tree_state;	/* struct copy */
    ts.ts_dbip = dbip;
    ts.ts_resp = &rt_uniresource;
    mat_idn(ts.ts_mat);
    db_full_path_init(&anp -> an_path);
    if (db_follow_path_for_state(&ts, &(anp -> an_path), argv[1], LOOKUP_NOISY)
	< 0 )
    {
	Tcl_AppendResult(interp,
	    "copymat: cannot follow path for arc: '", argv[1], "'\n",
	    (char *) NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&pvls);
    bu_vls_strcat(&pvls, argv[2]);
    parent = bu_vls_addr(&pvls);
    sep = strchr(parent, '/') - parent;
    bu_vls_trunc(&pvls, sep);
    switch (rt_db_lookup_internal(dbip, parent, &dp, &intern, LOOKUP_NOISY, &rt_uniresource))
    {
	case ID_COMBINATION:
	    if (dp -> d_flags & DIR_COMB)
		break;
	    else
	    {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "Non-combination directory <x%x> '%s' for combination rt_db_internal <x%x>\nThis should not happen\n",
		    dp, dp -> d_namep, intern);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *) NULL);
	    }
	    /* Fall through this case */
	default:
	    Tcl_AppendResult(interp,
		"copymat: Object '", parent, "' is not a combination\n",
		(char *) NULL);
	    /* Fall through this case */
	case ID_NULL:
	    bu_vls_free(&pvls);
	    return TCL_ERROR;
    }
    comb = (struct rt_comb_internal *) intern.idb_ptr;
    RT_CK_COMB(comb);

    if ((tp = db_find_named_leaf(comb -> tree, child)) == TREE_NULL)
    {
	Tcl_AppendResult(interp, "copymat: unable to find instance of '",
		child, "' in combination '", dp -> d_namep,
		"'\n", (char *)NULL);
	status = TCL_ERROR;
	goto wrapup;
    }

    /*
     *	Finally, copy the matrix
     */
    if (!bn_mat_is_identity(ts.ts_mat))
    {
	if (tp -> tr_l.tl_mat == NULL)
	    tp -> tr_l.tl_mat = bn_mat_dup(ts.ts_mat);
	else
	    MAT_COPY(tp -> tr_l.tl_mat, ts.ts_mat);
    }
    else if (tp -> tr_l.tl_mat != NULL)
    {
	bu_free((genptr_t) tp -> tr_l.tl_mat, "tl_mat");
	tp -> tr_l.tl_mat = (matp_t) 0;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0)
    {
	TCL_WRITE_ERR;
	status = TCL_ERROR;
	goto wrapup;
    }

    status = TCL_OK;

wrapup:

    bu_vls_free(&pvls);
    if (status == TCL_ERROR)
	rt_db_free_internal(&intern, &rt_uniresource);
    return status;
}
