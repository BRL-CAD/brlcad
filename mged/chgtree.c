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
