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

#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

extern int	atoi();
void	aexists();

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

/* Rename an object */
/* Format: n oldname newname	*/
void
f_name()
{
	register struct directory *dp;
	union record record;

	if( (dp = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( db_lookup( dbip,  cmd_args[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, cmd_args[2] ) < 0 )
		printf("error in rename to %s\n", cmd_args[2] );

	/* Change name in the file */
	(void)db_get( dbip,  dp, &record, 0 , 1);
	NAMEMOVE( cmd_args[2], record.c.c_name );
	(void)db_put( dbip, dp, &record, 0, 1 );
}

/* Copy an object */
/* Format: cp oldname newname	*/
void
f_copy()
{
	register struct directory *proto;
	register struct directory *dp;
	union record record;
	register int i;

	if( (proto = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( db_lookup( dbip,  cmd_args[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	db_get( dbip,  proto, &record, 0 , 1);

	if(record.u_id == ID_SOLID) {
		if( (dp = db_diradd( dbip,  cmd_args[2], -1, proto->d_len, DIR_SOLID )) == DIR_NULL )
			return;
		db_alloc( dbip, dp, proto->d_len );

		/*
		 * Update the disk record
		 */
		NAMEMOVE( cmd_args[2], record.s.s_name );
		db_put( dbip, dp, &record, 0, 1 );

		/* draw the new solid */
		drawtree( dp );
		dmaflag = 1;

		/* color the solid */
		dmp->dmr_colorchange();
	} else if(record.u_id == ID_ARS_A)  {
		if( (dp = db_diradd( dbip,  cmd_args[2], -1, proto->d_len, DIR_SOLID )) == DIR_NULL )
			return;
		db_alloc( dbip, dp, proto->d_len );
		NAMEMOVE( cmd_args[2], record.a.a_name );
		db_put( dbip, dp, &record, 0, 1 );

		/* Process the rest of the ARS (b records)  */
		for( i = 1; i < proto->d_len; i++ )  {
			db_get( dbip, proto, &record, i, 1);
			db_put( dbip, dp, &record, i, 1 );
		}
	} else if( record.u_id == ID_BSOLID ) {
		if( (dp = db_diradd( dbip,  cmd_args[2], -1, proto->d_len, DIR_SOLID )) == DIR_NULL )
			return;
		db_alloc( dbip, dp, proto->d_len );
		NAMEMOVE( cmd_args[2], record.B.B_name );
		db_put( dbip, dp, &record, 0, 1 );
		for( i = 1; i < proto->d_len; i++ ) {
			db_get( dbip, proto, &record, i, 1);
			db_put( dbip, dp, &record, i, 1 );
		}
	} else if(record.u_id == ID_COMB) {
		if( (dp = db_diradd( dbip, 
			cmd_args[2], -1, proto->d_len,
			record.c.c_flags == 'R' ?
			DIR_COMB|DIR_REGION : DIR_COMB)) == DIR_NULL )
			return;
		db_alloc( dbip, dp, proto->d_len );

		NAMEMOVE( cmd_args[2], record.c.c_name );
		db_put( dbip, dp, &record, 0, 1 );

		/* Process member records  */
		for( i = 1; i < proto->d_len; i++ )  {
			db_get( dbip, proto, &record, i, 1);
			db_put( dbip, dp, &record, i, 1 );
		}
	} else {
		(void)printf("%s: cannot copy\n",cmd_args[2]);
	}
}

/* Create an instance of something */
/* Format: i object combname [op]	*/
void
f_instance()
{
	register struct directory *dp;
	char oper;

	if( (dp = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	oper = UNION;
	if( numargs == 4 )
		oper = cmd_args[3][0];
	if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
		(void)printf("bad operation: %c\n", oper );
		return;
	}
	if( combadd( dp, cmd_args[2], 0, oper, 0, 0 ) == DIR_NULL )
		return;
}

/* add solids to a region or create the region */
/* and then add solids */
/* Format: r regionname opr1 sol1 opr2 sol2 ... oprn soln */
void
f_region()
{
	register struct directory *dp;
	union record record;
	int i;
	int ident, air;
	char oper;

 	ident = item_default;
 	air = air_default;
 
	/* Check for even number of arguments */
	if( numargs & 01 )  {
		(void)printf("error in number of args!\n");
		return;
	}

	if( db_lookup( dbip, cmd_args[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* will attempt to create the region */
		if(item_default) {
			item_default++;
			(void)printf("Defaulting item number to %d\n", item_default);
		}
	}

	/* Get operation and solid name for each solid */
	for( i = 2; i < numargs; i += 2 )  {
		if( cmd_args[i][1] != '\0' )  {
			(void)printf("bad operation: %s skip member: %s\n",
				cmd_args[i], cmd_args[i+1] );
			continue;
		}
		oper = cmd_args[i][0];
		if( (dp = db_lookup( dbip,  cmd_args[i + 1], LOOKUP_NOISY )) == DIR_NULL )  {
			(void)printf("skipping %s\n", cmd_args[i + 1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			(void)printf("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		db_get( dbip,  dp, &record, 0 , 1);
		if( record.u_id == ID_COMB ) {
			if( record.c.c_flags == 'R' ) {
				(void)printf(
				     "Note: %s is a region\n",
				     dp->d_namep );
			}
		}

		if( combadd( dp, cmd_args[1], 1, oper, ident, air ) == DIR_NULL )  {
			(void)printf("error in combadd\n");
			return;
		}
	}

	if( db_lookup( dbip, cmd_args[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* failed to create region */
		if(item_default > 1)
			item_default--;
	}
}

/*
 *			F _ C O M B
 *
 *  Create or add to the end of a combination, with one or more solids,
 *  with explicitly specified operations.
 *
 *  Format: comb comb_name sol1 opr2 sol2 ... oprN solN
 */
void
f_comb()
{
	register struct directory *dp;
	char	*comb_name;
	register int	i;
	char	oper;

	/* Check for odd number of arguments */
	if( numargs & 01 )  {
		(void)printf("error in number of args!\n");
		return;
	}

	/* Save combination name, for use inside loop */
	comb_name = cmd_args[1];

	/* Get operation and solid name for each solid */
	for( i = 2; i < numargs; i += 2 )  {
		if( cmd_args[i][1] != '\0' )  {
			(void)printf("bad operation: %s skip member: %s\n",
				cmd_args[i], cmd_args[i+1] );
			continue;
		}
		oper = cmd_args[i][0];
		if( (dp = db_lookup( dbip,  cmd_args[i + 1], LOOKUP_NOISY )) == DIR_NULL )  {
			(void)printf("skipping %s\n", cmd_args[i + 1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			(void)printf("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		if( combadd( dp, comb_name, 0, oper, 0, 0 ) == DIR_NULL )  {
			(void)printf("error in combadd\n");
			return;
		}
	}

	if( db_lookup( dbip, comb_name, LOOKUP_QUIET) == DIR_NULL )
		(void)printf("Error:  %s not created\n", comb_name );
}

/* Remove an object or several from the description */
/* Format: k object1 object2 .... objectn	*/
void
f_kill()
{
	register struct directory *dp;
	register int i;

	for( i = 1; i < numargs; i++ )  {
		if( (dp = db_lookup( dbip,  cmd_args[i], LOOKUP_NOISY )) != DIR_NULL )  {
			eraseobj( dp );
			db_delete( dbip, dp );
			db_dirdelete( dbip, dp );
		}
	}
	dmaflag = 1;
}

/* Grouping command */
/* Format: g groupname object1 object2 .... objectn	*/
void
f_group()
{
	register struct directory *dp;
	register int i;

	/* get objects to add to group */
	for( i = 2; i < numargs; i++ )  {
		if( (dp = db_lookup( dbip,  cmd_args[i], LOOKUP_NOISY)) != DIR_NULL )  {
			if( combadd( dp, cmd_args[1], 0,
				UNION, 0, 0) == DIR_NULL )
				return;
		}  else
			(void)printf("skip member %s\n", cmd_args[i]);
	}
}

/* Delete members of a combination */
/* Format: D comb memb1 memb2 .... membn	*/
void
f_rm()
{
	register struct directory *dp;
	register int i, rec, num_deleted;
	union record record;

	if( (dp = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	/* Examine all the Member records, one at a time */
	num_deleted = 0;
top:
	for( rec = 1; rec < dp->d_len; rec++ )  {
		db_get( dbip,  dp, &record, rec , 1);
		/* Compare this member to each command arg */
		for( i = 2; i < numargs; i++ )  {
			if( strcmp( cmd_args[i], record.M.m_instname ) != 0 )
				continue;
			(void)printf("deleting member %s\n", cmd_args[i] );
			num_deleted++;
			db_delrec( dbip, dp, rec );
			goto top;
		}
	}
	/* go back and update the header record */
	if( num_deleted ) {
		db_get( dbip, dp, &record, 0, 1 );
		record.c.c_length -= num_deleted;
		db_put( dbip, dp, &record, 0, 1 );
	}
}

/* Copy a cylinder and position at end of original cylinder
 *	Used in making "wires"
 *
 * Format: cpi old new
 */
void
f_copy_inv()
{
	register struct directory *proto;
	register struct directory *dp;
	union record record;
	register int i;

	if( (proto = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	NAMEMOVE( cmd_args[2], record.s.s_name );
	if( db_lookup( dbip,  record.s.s_name, LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( record.s.s_name );
		return;
	}

	db_get( dbip,  proto, &record, 0 , 1);

	if(record.u_id != ID_SOLID || record.s.s_type != GENTGC )  {
		(void)printf("%s: Not a cylinder\n",record.s.s_name);
		return;
	}

	if( (dp = db_diradd( dbip,  cmd_args[2], -1, proto->d_len, DIR_SOLID )) == DIR_NULL )
		return;
	db_alloc( dbip, dp, proto->d_len );

	/* translate to end of "original" cylinder */
	for(i=0; i<3; i++) {
		record.s.s_values[i] += record.s.s_values[i+3];
	}

	/*
	 * Update the disk record
	 */
	NAMEMOVE( cmd_args[2], record.s.s_name );
	db_put( dbip, dp, &record, 0, 1 );

	/* draw the new solid */
	drawtree( dp );
	dmaflag = 1;

	/* color the solid */
	dmp->dmr_colorchange();

	if(state == ST_VIEW) {
		/* solid edit this new cylinder */
		strcpy(cmd_args[1], cmd_args[2]);
		f_sed();
	}
}
