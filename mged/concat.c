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
 *
 *			F _ D U P ( )
 *
 *  Check for duplicate names in preparation for cat'ing of files
 *
 *
 */
void
f_dup( argc, argv )
int	argc;
char	**argv;
{
	union record	record;
	register FILE *dupfp;
	register int i;
	int ndup;
	register struct directory *tmpdp;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2 );		/* allow interrupts */

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
	if( (dupfp=fopen(cmd_args[1], "r")) == NULL ) {
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

	ndup = 0;
	ncharadd = 0;
	(void)strcpy(prestr, cmd_args[2]);
	ncharadd = strlen( prestr );

	fread( (char *)&record, sizeof record, 1, dupfp );
	if(record.u_id != ID_IDENT) {
		(void)printf("%s: Not a correct GED data base - STOP\n",
				cmd_args[1]);
		(void)fclose( dupfp );
		return;
	}

	(void)printf("\n*** Comparing %s with %s for duplicate names\n",
		dbip->dbi_filename,cmd_args[1]);
	if( ncharadd ) {
		(void)printf("  For comparison, all names in %s prefixed with:  %s\n",
				cmd_args[1],prestr);
	}

	if( (dirp = dir_getspace(0)) == (struct directory **) 0) {
		(void) printf( "f_dup: unable to get memory\n");
		return;
	}
	dirp0 = dirp;

	while( fread( (char *)&record, sizeof record, 1, dupfp ) == 1 && ! feof(dupfp) ) {
		tmpdp = DIR_NULL;

		switch( record.u_id ) {

			case ID_IDENT:
			case ID_FREE:
			case ID_ARS_B:
			case ID_MEMB:
			case ID_P_DATA:
			case ID_MATERIAL:
				break;

			case ID_SOLID:
				if(record.s.s_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.s.s_name);
					tmpdp = db_lookup( dbip, new_name, LOOKUP_QUIET);
				}
				else
					tmpdp = db_lookup( dbip, record.s.s_name, LOOKUP_QUIET);
				break;

			case ID_ARS_A:
				if(record.a.a_name[0] == 0) 
					break;
				if( ncharadd ) {
					prename(record.a.a_name);
					tmpdp = db_lookup( dbip, new_name, LOOKUP_QUIET);
				}
				else
					tmpdp = db_lookup( dbip, record.a.a_name, LOOKUP_QUIET);
				break;

			case ID_COMB:
				if(record.c.c_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.c.c_name);
					tmpdp = db_lookup( dbip, new_name, LOOKUP_QUIET);
				}
				else
					tmpdp = db_lookup( dbip, record.c.c_name, LOOKUP_QUIET);
				break;

			case ID_BSOLID:
				if(record.B.B_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.B.B_name);
					tmpdp = db_lookup( dbip, new_name, LOOKUP_QUIET);
				}
				else
					tmpdp = db_lookup( dbip, record.B.B_name, LOOKUP_QUIET);
				break;

			case ID_BSURF:
				/* Need to skip over knots & mesh which follows! */
				(void)fseek( dupfp,
					(record.d.d_nknots+record.d.d_nctls) *
						(long)(sizeof(record)),
					1 );
				continue;

			case ID_P_HEAD:
				if(record.p.p_name[0] == 0)
					break;
				if( ncharadd ) {
					prename(record.p.p_name);
					tmpdp = db_lookup( dbip, new_name, LOOKUP_QUIET);
				}
				else
					tmpdp = db_lookup( dbip, record.p.p_name, LOOKUP_QUIET);
				break;

			default:
				(void)printf("Unknown record type (%c) in %s\n",
						record.u_id,cmd_args[1]);
				break;

		}

		if(tmpdp != DIR_NULL) {
			/* have a duplicate name */
			ndup++;
			*dirp++ = tmpdp;
			tmpdp = DIR_NULL;
		}
	}
	col_pr4v( dirp0, (int)(dirp - dirp0));
	free( dirp0);
	(void)printf("\n -----  %d duplicate names found  -----\n",ndup);
	(void)fclose( dupfp );
	return;
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
				length = record.c.c_length;
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
