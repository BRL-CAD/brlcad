/*
 *			C O N C A T . C
 *
 *  Functions -
 *	f_dup()		checks for dup names before cat'ing of two files
 *	f_concat()	routine to cat another GED file onto end of current file
 *
 *  Authors -
 *	Keith A. Applin
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSconcat[] = "@(#)$Header$ (BRL)";
#endif

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

extern int	args;		/* total number of args available */
extern int	argcnt;		/* holder for number of args added later */
extern char	*cmd_args[];	/* array of pointers to args */

void		prename();

int			num_dups;
struct directory	**dup_dirp;

char	new_name[NAMESIZE];
char	prestr[15];
int	ncharadd;

/*    P R E N A M E ( ): 	actually adds prefix to a name
 *				new_name[] = prestr[] + old_name[]
 */
void
prename( old_name )
char old_name[NAMESIZE];
{

	(void)strcpy(new_name, prestr);
	(void)strncat(new_name, old_name, NAMESIZE-1-ncharadd);
	if( ncharadd + strlen( old_name ) > NAMESIZE-1 ) {
		(void)printf("name truncated : %s + %s = %s\n",
			prestr,old_name,new_name);
	}
}

/*
 *			M G E D _ D I R A D D
 *
 * Add an entry to the directory
 */
int
mged_diradd( loc_dbip, name, laddr, len, flags )
register struct db_i	*loc_dbip;
register char		*name;
long			laddr;
int			len;
int			flags;
{
	register struct directory **headp;
	register struct directory *dp;
	struct directory	*dupdp;
	char			local[NAMESIZE+2];

	if( loc_dbip->dbi_magic != DBI_MAGIC )  rt_bomb("mged_diradd:  bad dbip\n");

	/* Add the prefix, if any */
	if( ncharadd > 0 )  {
		(void)strncpy( local, prestr, ncharadd );
		(void)strncpy( local+ncharadd, name, NAMESIZE-ncharadd );
	} else {
		(void)strncpy( local, name, NAMESIZE );
	}
	local[NAMESIZE] = '\0';
		
	/* Look up this new name in the existing (main) database */
	if( (dupdp = db_lookup( dbip, name, 0 )) != DIR_NULL )  {
		/* Duplicate found, add it to the list */
		num_dups++;
		*dup_dirp++ = dupdp;
	}
	return 0;
}

/*
 *
 *			F _ D U P ( )
 *
 *  Check for duplicate names in preparation for cat'ing of files
 *
 *  Usage:  dup file.g [prefix]
 */
void
f_dup( argc, argv )
int	argc;
char	**argv;
{
	struct db_i		*newdbp;
	struct directory	**dirp0;

	(void)signal( SIGINT, sig2 );		/* allow interrupts */

	/* get any prefix */
	if( argc < 3 ) {
		prestr[0] = '\0';
	} else {
		(void)strcpy(prestr, argv[2]);
	}
	num_dups = 0;
	if( (ncharadd = strlen( prestr )) > 12 )  {
		ncharadd = 12;
		prestr[12] = '\0';
	}

	/* open the target file */
	if( (newdbp = db_open( argv[1], "r" )) == DBI_NULL )  {
		perror( argv[1] );
		(void)fprintf(stderr, "dup: Can't open %s\n", argv[1]);
		return;
	}

	(void)printf("\n*** Comparing %s with %s for duplicate names\n",
		dbip->dbi_filename,argv[1]);
	if( ncharadd ) {
		(void)printf("  For comparison, all names in %s prefixed with:  %s\n",
				argv[1],prestr);
	}

	/* Get array to hold names of duplicates */
	if( (dup_dirp = dir_getspace(0)) == (struct directory **) 0) {
		(void) printf( "f_dup: unable to get memory\n");
		return;
	}
	dirp0 = dup_dirp;

	/* Scan new directory for overlaps */
	if( db_scan( newdbp, mged_diradd ) < 0 )  {
		(void)fprintf(stderr, "dup: db_scan failure\n");
		return;
	}

	col_pr4v( dirp0, (int)(dup_dirp - dirp0));
	rt_free( (char *)dirp0, "dir_getspace array" );
	(void)printf("\n -----  %d duplicate names found  -----\n",num_dups);

	db_close( newdbp );
}



/*
 *
 *		F _ C O N C A T
 *
 *	concatenates another GED file onto end of current file
 */
void
f_concat( argc, argv )
int	argc;
char	**argv;
{
	union record	record;
	FILE *catfp;
	int nskipped, i, length;
	register struct directory *dp;

	(void)signal( SIGINT, sig2 );		/* interrupts */

	/* save number of args entered initially */
	args = argc;
	argcnt = 0;

	/* get target file name */
	while( args < 2 ) {
		(void)printf("Enter the target file name: ");
		argcnt = getcmd(args);
		/* add any new args entered */
		args += argcnt;
	}

	/* open the target file */
	if( (catfp=fopen(cmd_args[1], "r")) == NULL ) {
		(void)fprintf(stderr,"Can't open %s\n",cmd_args[1]);
		return;
	}

	/* get any prefix */
	if( args < 3 ) {
		(void)printf("Enter prefix string or CR: ");
		argcnt = getcmd(args);
		/* add any new args entered */
		args += argcnt;
		/* no prefix is acceptable */
		if(args == 2)
			cmd_args[2][0] = '\0';
	}

	ncharadd = nskipped = 0;
	(void)strcpy(prestr, cmd_args[2]);
	ncharadd = strlen( prestr );

	fread( (char *)&record, sizeof record, 1, catfp );
	if(record.u_id != ID_IDENT) {
		(void)printf("%s: Not a correct GED data base - STOP\n",
				cmd_args[1]);
		(void)fclose( catfp );
		return;
	}

	(void)signal( SIGINT, SIG_IGN );		/* NO interrupts */

	while( fread( (char *)&record, sizeof record, 1, catfp ) == 1 && ! feof(catfp) ) {

		switch( record.u_id ) {

			case ID_IDENT:
			case ID_FREE:
			case ID_ARS_B:
			case ID_MEMB:
			case ID_P_DATA:
				break;

			case ID_MATERIAL:
				rt_color_addrec( &record, -1 );
				break;

			case ID_SOLID:
				if(record.s.s_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.s.s_name);
					NAMEMOVE(new_name, record.s.s_name);
				}
				if( db_lookup( dbip, record.s.s_name, LOOKUP_QUIET) != DIR_NULL ) {
					nskipped++;
					(void)printf("SOLID (%s) already exists in %s....will skip\n",
						record.s.s_name, dbip->dbi_filename);
					break;
				}
				/* add to the directory */
printf("adding solid \"%s\"\n", record.s.s_name);
				if( (dp = db_diradd( dbip, record.s.s_name, -1, 1, DIR_SOLID)) == DIR_NULL )
					return;
				/* add the record to the data base file */
				db_alloc( dbip, dp, 1 );
				db_put( dbip, dp, &record, 0, 1 );
				break;

			case ID_ARS_A:
				if( ncharadd ) {
					prename(record.a.a_name);
					NAMEMOVE(new_name, record.a.a_name);
				}
				if( db_lookup( dbip, record.a.a_name, LOOKUP_QUIET) != DIR_NULL ) {
					nskipped++;
					(void)printf("ARS (%s) already exists in %s....will skip\n",
							record.a.a_name, dbip->dbi_filename);
					break;
				}
				/* add to the directory */
				length = record.a.a_totlen;
				if( (dp = db_diradd( dbip, record.a.a_name, -1, length+1, DIR_SOLID)) == DIR_NULL )
					return;
				/* add the record to the data base file */
				db_alloc( dbip, dp, length+1 );
				db_put( dbip, dp, &record, 0, 1 );
				/* get the b_records */
				for(i=1; i<=length; i++) {
					(void)fread( (char *)&record, sizeof record, 1, catfp );
					db_put( dbip, dp, &record, i, 1 );
				}
				break;

			case ID_COMB:
				if( ncharadd ) {
					prename(record.c.c_name);
					NAMEMOVE(new_name, record.c.c_name);
				}
				if( db_lookup( dbip, record.c.c_name, LOOKUP_QUIET) != DIR_NULL ) {
					nskipped++;
					(void)printf("COMB (%s) already exists in %s....will skip\n",
							record.c.c_name, dbip->dbi_filename);
					break;
				}
				/* add to the directory */
#if 0
				length = record.c.c_length;
#else
	/* XXXXX XXXXX XXXXX */
				length = 0;
#endif
printf("adding comb \"%s\"\n", record.c.c_name);
				if( (dp = db_diradd( dbip,  record.c.c_name, -1,
						length+1,
						record.c.c_flags == 'R' ?
						DIR_COMB|DIR_REGION : DIR_COMB
						) ) == DIR_NULL )
					return;
				/* add the record to the data base file */
				db_alloc( dbip, dp, length+1 );
				db_put( dbip, dp, &record, 0, 1 );
				/* get the member records */
				for(i=1; i<=length; i++) {
					(void)fread( (char *)&record, sizeof record, 1, catfp );
					if( record.M.m_brname[0] && ncharadd ) {
						prename(record.M.m_brname);
						NAMEMOVE(new_name, record.M.m_brname);
					}
					if( record.M.m_instname[0] && ncharadd ) {
						prename(record.M.m_instname);
						NAMEMOVE(new_name, record.M.m_instname);
					}
					db_put( dbip, dp, &record, i, 1 );
				}
				break;

			case ID_BSOLID:
				if( ncharadd ) {
					prename(record.B.B_name);
					NAMEMOVE(new_name, record.B.B_name);
				}
				if( db_lookup( dbip, record.B.B_name, LOOKUP_QUIET) != DIR_NULL ) {
					nskipped++;
					(void)printf("SPLINE (%s) already exists in %s....will skip\n",
							record.B.B_name, dbip->dbi_filename);
					break;
				}
	printf("SPLINE not implemented yet, aborting!\n");
				/* Need to miss the knots and mesh */
				return;

			case ID_P_HEAD:
				if(record.p.p_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.p.p_name);
					NAMEMOVE(new_name, record.p.p_name);
				}
				if( db_lookup( dbip, record.p.p_name, LOOKUP_QUIET) != DIR_NULL ) {
					nskipped++;
					(void)printf("POLYGON (%s) already exists in %s....will skip\n",
							record.p.p_name, dbip->dbi_filename);
					break;
				}
printf("POLYGONS not implemented yet.....SKIP %s\n",record.p.p_name);
				break;

			default:
				(void)printf("BAD record type (%c) in %s\n",
						record.u_id,cmd_args[1]);
				break;

		}

	}
}
