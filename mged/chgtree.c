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

#include "tcl.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"
#include "./mgedtcl.h"

extern struct db_tree_state	mged_initial_tree_state;	/* dodraw.c */
extern struct rt_tol		mged_tol;	/* from ged.c */
extern struct rt_tess_tol	mged_ttol;	/* XXX needs to replace mged_abs_tol, et.al. from dodraw.c */

void	aexists();

/* Rename an object */
/* Format: n oldname newname	*/
int
f_name( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[2] );
		return CMD_BAD;
	}

	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, argv[2] ) < 0 ||
	    db_get( dbip,  dp, &record, 0 , 1) < 0 )  {
		rt_log("error in rename to %s, aborting\n", argv[2] );
	    	ERROR_RECOVERY_SUGGESTION;
	    	return CMD_BAD;
	}

	/* Change name in the file */
	NAMEMOVE( argv[2], record.c.c_name );
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) { WRITE_ERR; return CMD_BAD; }

	return CMD_OK;
}

/* Copy an object */
/* Format: cp oldname newname	*/
int
f_copy( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *proto;
	register struct directory *dp;
	struct rt_external external;

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[2] );
		return CMD_BAD;
	}

	if( db_get_external( &external , proto , dbip ) ) {
		READ_ERR;
		return CMD_BAD;
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp=db_diradd( dbip, argv[2], -1, proto->d_len, proto->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, proto->d_len ) < 0 )  {
	    	ALLOC_ERR;
		return CMD_BAD;
	}

	if (db_put_external( &external, dp, dbip ) < 0 )
	{
		db_free_external( &external );
		WRITE_ERR;
		return CMD_BAD;
	}
	db_free_external( &external );

	/* draw the new object */
	return f_edit( 2, argv+1 );	/* depends on name being in argv[2] ! */
}

/* Create an instance of something */
/* Format: i object combname [op]	*/
int
f_instance( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	char oper;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	oper = UNION;
	if( argc == 4 )
		oper = argv[3][0];
	if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
		rt_log("bad operation: %c\n", oper );
		return CMD_BAD;
	}
	if( combadd( dp, argv[2], 0, oper, 0, 0 ) == DIR_NULL )
		return CMD_BAD;

	return CMD_OK;
}

/* add solids to a region or create the region */
/* and then add solids */
/* Format: r regionname opr1 sol1 opr2 sol2 ... oprn soln */
int
f_region( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	union record record;
	int i;
	int ident, air;
	char oper;

 	ident = item_default;
 	air = air_default;
 
	/* Check for even number of arguments */
	if( argc & 01 )  {
		rt_log("error in number of args!\n");
		return CMD_BAD;
	}

	if( db_lookup( dbip, argv[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* will attempt to create the region */
		if(item_default) {
			item_default++;
			rt_log("Defaulting item number to %d\n", item_default);
		}
	}

	/* Get operation and solid name for each solid */
	for( i = 2; i < argc; i += 2 )  {
		if( argv[i][1] != '\0' )  {
			rt_log("bad operation: %s skip member: %s\n",
				argv[i], argv[i+1] );
			continue;
		}
		oper = argv[i][0];
		if( (dp = db_lookup( dbip,  argv[i+1], LOOKUP_NOISY )) == DIR_NULL )  {
			rt_log("skipping %s\n", argv[i+1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			rt_log("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
			READ_ERR;
			return CMD_BAD;
		}
		
		if( record.u_id == ID_COMB ) {
			if( record.c.c_flags == 'R' ) {
				rt_log(
				     "Note: %s is a region\n",
				     dp->d_namep );
			}
		}

		if( combadd( dp, argv[1], 1, oper, ident, air ) == DIR_NULL )  {
			rt_log("error in combadd\n");
			return CMD_BAD;
		}
	}

	if( db_lookup( dbip, argv[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* failed to create region */
		if(item_default > 1)
			item_default--;
		return CMD_BAD;
	}

	return CMD_OK;
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
f_comb( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	char	*comb_name;
	register int	i;
	char	oper;

	/* Check for odd number of arguments */
	if( argc & 01 )  {
		rt_log("error in number of args!\n");
		return CMD_BAD;
	}

	/* Save combination name, for use inside loop */
	comb_name = argv[1];

	/* Get operation and solid name for each solid */
	for( i = 2; i < argc; i += 2 )  {
		if( argv[i][1] != '\0' )  {
			rt_log("bad operation: %s skip member: %s\n",
				argv[i], argv[i+1] );
			continue;
		}
		oper = argv[i][0];
		if( (dp = db_lookup( dbip,  argv[i+1], LOOKUP_NOISY )) == DIR_NULL )  {
			rt_log("skipping %s\n", argv[i+1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			rt_log("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		if( combadd( dp, comb_name, 0, oper, 0, 0 ) == DIR_NULL )  {
			rt_log("error in combadd\n");
			return CMD_BAD;
		}
	}

	if( db_lookup( dbip, comb_name, LOOKUP_QUIET) == DIR_NULL ) {
		rt_log("Error:  %s not created\n", comb_name );
		return CMD_BAD;
	}

	return CMD_OK;
}

/* Remove an object or several from the description */
/* Format: kill [-f] object1 object2 .... objectn	*/
int
f_kill( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;
	int	is_phony;
	int	verbose = LOOKUP_NOISY;

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
			    	DELETE_ERR(argv[i]);
			    	/* Abort kill processing on first error */
				return CMD_BAD;
			}
		}
	}
	return CMD_OK;
}

/* Grouping command */
/* Format: g groupname object1 object2 .... objectn	*/
int
f_group( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	/* get objects to add to group */
	for( i = 2; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY)) != DIR_NULL )  {
			if( combadd( dp, argv[1], 0,
				UNION, 0, 0) == DIR_NULL )
				return CMD_BAD;
		}  else
			rt_log("skip member %s\n", argv[i]);
	}
	return CMD_OK;
}

/* Delete members of a combination */
/* Format: D comb memb1 memb2 .... membn	*/
int
f_rm( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i, rec, num_deleted;
	union record record;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	/* Examine all the Member records, one at a time */
	num_deleted = 0;
top:
	for( rec = 1; rec < dp->d_len; rec++ )  {
		if( db_get( dbip,  dp, &record, rec , 1) < 0 ) {
			READ_ERR;
			return CMD_BAD;
		}
		/* Compare this member to each command arg */
		for( i = 2; i < argc; i++ )  {
			if( strcmp( argv[i], record.M.m_instname ) != 0 )
				continue;
			rt_log("deleting member %s\n", argv[i] );
			if( db_delrec( dbip, dp, rec ) < 0 )  {
				rt_log("Error in deleting member.\n");
				ERROR_RECOVERY_SUGGESTION;
				return CMD_BAD;
			}
			num_deleted++;
			goto top;
		}
	}

	return CMD_OK;
}

/* Copy a cylinder and position at end of original cylinder

 *	Used in making "wires"
 *
 * Format: cpi old new
 */
int
f_copy_inv( argc, argv )
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

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( argv[2] );
		return CMD_BAD;
	}

	/* get external representation of slid to be copied */
	if( db_get_external( &external, proto, dbip )) {
		READ_ERR;
		return CMD_BAD;
	}

	/* make sure it is a TGC */
	id = rt_id_solid( &external );
	if( id != ID_TGC )
	{
		rt_log( "f_copy_inv: %d is not a cylinder\n" , argv[1] );
		db_free_external( &external );
		return CMD_BAD;
	}

	/* import the TGC */
	if( rt_functab[id].ft_import( &internal , &external , rt_identity ) < 0 )
	{
		rt_log( "f_copy_inv: import failure for %s\n" , argv[1] );
		return CMD_BAD;
	}
	db_free_external( &external );

	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

	/* translate to end of "original" cylinder */
	VADD2( tgc_ip->v , tgc_ip->v , tgc_ip->h );

	/* now export the new TGC */
	if( rt_functab[internal.idb_type].ft_export( &external, &internal, (double)1.0 ) < 0 )
	{
		rt_log( "f_copy_inv: export failure\n" );
		rt_functab[internal.idb_type].ft_ifree( &internal );
		return CMD_BAD;
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
	    	ALLOC_ERR;
		return CMD_BAD;
	    }

	if (db_put_external( &external, dp, dbip ) < 0 )
	{
		db_free_external( &external );
		WRITE_ERR;
		return CMD_BAD;
	}
	db_free_external( &external );

	/* draw the new solid */
	f_edit( 2, argv+1 );	/* depends on name being in argv[2] ! */

	if(state == ST_VIEW) {
		/* solid edit this new cylinder */
		f_sed( 2, argv+1 );	/* new name in argv[2] */
	}

	return CMD_OK;
}

/* XXX Move to raytrace.h */
RT_EXTERN(struct animate	*db_parse_1anim, (struct db_i *dbip,
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
f_arced( argc, argv )
int	argc;
char	**argv;
{
	struct animate		*anp;
	struct directory	*dp;
	mat_t			stack;
	mat_t			arc;
	union record		*rec;
	int			i;

	if( not_state( ST_VIEW, "Viewing state" ) )  return CMD_BAD;

	if( !strchr( argv[1], '/' ) )  {
		rt_log("arced: bad path specification '%s'\n", argv[1]);
		return CMD_BAD;
	}
	if( !(anp = db_parse_1anim( dbip, argc, (CONST char **)argv ) ) )  {
		rt_log("arced: unable to parse command\n");
		return CMD_BAD;
	}
	if( anp->an_path.fp_len != 2 )  {
		char	*thepath = db_path_to_string( &(anp->an_path) );
		rt_log("arced: '%s' is not a 2-element path spec\n");
		rt_free( thepath, "path" );
		db_free_1anim( anp );
		return CMD_BAD;
	}

#if 0
	db_write_anim(stdout, anp);	/* XXX temp */
#endif

	/* Only the matrix rarc, lmul, and rmul directives are useful here */
	mat_idn( stack );

	/* Load the combination into memory */
	dp = anp->an_path.fp_names[0];
	if( (rec = db_getmrec( dbip, dp )) == (union record *)NULL )  {
		READ_ERR;
		db_free_1anim( anp );
		return CMD_BAD;
	}
	if( rec[0].u_id != ID_COMB )  {
		rt_log("%s: not a combination\n", dp->d_namep );
		goto fail;
	}

	/* Search for first mention of arc */
	for( i=1; i < dp->d_len; i++ )  {
		if( rec[i].u_id != ID_MEMB )  {
			rt_log("%s: element %d not a member! Database corrupted.\n", dp->d_namep, i );
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
			rt_log("arced: write error, aborting\n");
			ERROR_RECOVERY_SUGGESTION;
			goto fail;
		}
		rt_free( (char *)rec, "union record []");
		db_free_1anim( anp );
		return CMD_OK;
	}

	rt_log("Unable to find instance of '%s' in combination '%s', error\n",
		anp->an_path.fp_names[1]->d_namep,
		anp->an_path.fp_names[0]->d_namep );
		
fail:
	rt_free( (char *)rec, "union record []");
	db_free_1anim( anp );
	return CMD_BAD;
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

	rt_free( str, "path string" );
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
CONST char	**argv;
{
	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

	if( db_walk_tree( dbip, argc-1, argv+1, 1, &mged_initial_tree_state,
	    0, 0, pathlist_leaf_func ) < 0 )  {
	    	Tcl_SetResult(interp, "db_walk_tree() error", TCL_STATIC);
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

	FOR_ALL_SOLIDS(sp)  {
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
	if( count > 1 )  rt_log("find_solid_with_path() found %d matches\n", count);
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
CONST char	**argv;
{
	struct db_full_path	lhs;
	struct db_full_path	rhs;
	struct db_full_path	both;
	char			*new_argv[4];
	char			number[32];

	if( argc != 3 )  {
		Tcl_SetResult(interp, "Usage: oed path_lhs path_rhs", TCL_STATIC);
		return TCL_ERROR;
	}
	if( not_state( ST_VIEW, "Object Illuminate" ) )  {
		Tcl_SetResult(interp, "MGED state is not VIEW", TCL_STATIC);
		return TCL_ERROR;
	}
	if( HeadSolid.s_forw == &HeadSolid )  {
		Tcl_SetResult(interp, "no solids in view", TCL_STATIC);
		return TCL_ERROR;
	}

	if( db_string_to_path( &lhs, dbip, argv[1] ) < 0 )  {
		Tcl_SetResult(interp, "bad lhs path", TCL_STATIC);
		return TCL_ERROR;
	}
	if( db_string_to_path( &rhs, dbip, argv[2] ) < 0 )  {
		db_free_full_path( &lhs );
		Tcl_SetResult(interp, "bad rhs path", TCL_STATIC);
		return TCL_ERROR;
	}
	if( rhs.fp_len <= 0 )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		Tcl_SetResult(interp, "rhs must not be null", TCL_STATIC);
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
	illump = HeadSolid.s_forw;/* any valid solid would do */
	edobj = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	mat_idn( modelchanges );	/* No changes yet */
	(void)chg_state( ST_VIEW, ST_O_PICK, "internal change of state");

	/* Find the one solid, set s_iflag UP, point illump at it */
	illump = find_solid_with_path( &both );
	if( !illump )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_SetResult(interp, "Unable to find solid matching path", TCL_STATIC);
		illump = 0;
		(void)chg_state( ST_O_PICK, ST_VIEW, "error recovery");
		return TCL_ERROR;
	}
	(void)chg_state( ST_O_PICK, ST_O_PATH, "internal change of state");

	/* Select the matrix */
#if 0
	rt_log("matpick %d\n", lhs.fp_len);
#endif
	sprintf( number, "%d", lhs.fp_len );
	new_argv[0] = "matpick";
	new_argv[1] = number;
	new_argv[2] = NULL;
	if( f_matpick( 2, new_argv ) != CMD_OK )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_SetResult(interp, "error detected inside f_matpick", TCL_STATIC);
		return TCL_ERROR;
	}
	if( not_state( ST_O_EDIT, "Object EDIT" ) )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_SetResult(interp, "MGED state did not advance to Object EDIT", TCL_STATIC);
		return TCL_ERROR;
	}
	db_free_full_path( &lhs );
	db_free_full_path( &rhs );
	db_free_full_path( &both );
	return TCL_OK;
}
