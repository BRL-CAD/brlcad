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
 *	f_rm	delete members of a combination
 *
 *  Author -
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

#include	<math.h>
#include	<stdio.h>
#include "ged_types.h"
#include "../h/db.h"
#include "sedit.h"
#include "ged.h"
#include "objdir.h"
#include "solid.h"
#include "dm.h"
#include "../h/vmath.h"

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
	char *name;
	long laddr;
	int flags, len;

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	/* Due to hashing, need to delete and add it back */
	laddr = dp->d_addr;
	flags = dp->d_flags;
	len = dp->d_len;
	dir_delete( dp );

	dp = dir_add( cmd_args[2], laddr, flags, len );
	db_getrec( dp, &record, 0 );

	NAMEMOVE( cmd_args[2], record.c.c_name );
	db_putrec( dp, &record, 0 );
}

/* Copy a solid */
/* Format: c oldname newname	*/
void
f_copy()
{
	register struct directory *proto;
	register struct directory *dp;
	union record record;
	register int i;

	if( (proto = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	db_getrec( proto, &record, 0 );

	if(record.u_id == ID_SOLID) {
		if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, proto->d_len )) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );

		/*
		 * Update the disk record
		 */
		NAMEMOVE( cmd_args[2], record.s.s_name );
		db_putrec( dp, &record, 0 );
	} else if(record.u_id == ID_ARS_A)  {
		if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, proto->d_len )) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );
		NAMEMOVE( cmd_args[2], record.a.a_name );
		db_putrec( dp, &record, 0 );

		/* Process the rest of the ARS (b records)  */
		for( i = 1; i < proto->d_len; i++ )  {
			db_getrec( proto, &record, i );
			db_putrec( dp, &record, i );
		}
	} else if( record.u_id == ID_B_SPL_HEAD ) {
		if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, proto->d_len )) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );
		NAMEMOVE( cmd_args[2], record.d.d_name );
		db_putrec( dp, &record, 0 );
		for( i = 1; i < proto->d_len; i++ ) {
			db_getrec( proto, &record, i );
			db_putrec( dp, &record, i );
		}
	} else if(record.u_id == ID_COMB) {
		if( (dp = dir_add(
			cmd_args[2],
			-1,
			record.c.c_flags == 'R' ? DIR_COMB|DIR_REGION : DIR_COMB,
			proto->d_len )) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );

		NAMEMOVE( cmd_args[2], record.c.c_name );
		db_putrec( dp, &record, 0 );

		/* Process member records  */
		for( i = 1; i < proto->d_len; i++ )  {
			db_getrec( proto, &record, i );
			db_putrec( dp, &record, i );
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

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	oper = UNION;
	if( numargs == 5 )
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

	if( lookup(cmd_args[1], LOOKUP_QUIET) == DIR_NULL ) {
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
		if( (dp = lookup( cmd_args[i + 1], LOOKUP_NOISY )) == DIR_NULL )  {
			(void)printf("skipping %s\n", cmd_args[i + 1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			(void)printf("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		db_getrec( dp, &record, 0 );
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

	if( lookup(cmd_args[1], LOOKUP_QUIET) == DIR_NULL ) {
		/* failed to create region */
		if(item_default > 1)
			item_default--;
	}
}

/* Remove an object or several from the description */
/* Format: k object1 object2 .... objectn	*/
void
f_kill()
{
	register struct directory *dp;
	register int i;

	for( i = 1; i < numargs; i++ )  {
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) != DIR_NULL )  {
			eraseobj( dp );
			db_delete( dp );
			dir_delete( dp );
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
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY)) != DIR_NULL )  {
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

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	/* Examine all the Member records, one at a time */
	num_deleted = 0;
	for( rec = 1; rec < dp->d_len; rec++ )  {
		db_getrec( dp, &record, rec );
top:
		/* Compare this member to each command arg */
		for( i = 2; i < numargs; i++ )  {
			if( strcmp( cmd_args[i], record.M.m_instname ) != 0 )
				continue;
			(void)printf("deleting member %s\n", cmd_args[i] );
			num_deleted++;

			/* If deleting last member, just truncate */
			if( rec == dp->d_len-1 ) {
				db_trunc(dp, 1);
				continue;
			}

			db_getrec( dp, &record, dp->d_len-1 );	/* last one */
			db_putrec( dp, &record, rec );		/* xch */
			db_trunc( dp, 1 );
			goto top;
		}
	}
	/* go back and undate the header record */
	if( num_deleted ) {
		db_getrec(dp, &record, 0);
		record.c.c_length -= num_deleted;
		db_putrec(dp, &record, 0);
	}
}
