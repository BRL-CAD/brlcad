/*
 *			U T I L I T Y 1 . C
 *
 *  Functions -
 *	f_tables()	control routine for building ascii tables
 *	tables()	builds ascii summary tables
 *	f_edcodes()	control routine for editing region ident codes
 *	edcodes()	allows for easy editing of region ident codes
 *	f_which_id()	lists all regions with given ident number
 *
 *  Author -
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
static char RCSid[] = "@(#)$Header$ (BRL)";
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

extern void f_quit();

extern char	*local_unit[];

extern struct passwd *getpwuid();

void		tables(), edcodes(), changes(), prfield();

/* structure to distinguish new solids from existing (old) solids */
struct identt {
	int	i_index;
	char	i_name[NAMESIZE];
	mat_t	i_mat;
};
struct identt identt, idbuf;

static union record record;

#define ABORTED		-99
#define MAXSOL 		2000
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

/*
 *
 *	F _ T A B L E S :	control routine to build ascii tables
 *
 *
 */

char operate;
int regflag, numreg, lastmemb, numsol, old_or_new, oper_ok;
int discr[MAXSOL], idfd, rd_idfd;
int flag;	/* which type of table to make */
FILE *fopen(), *tabptr;

void
f_tables( argc, argv )
int	argc;
char	**argv;
{

	register struct directory *dp;
	register int i;
	char *timep;
	long now;
	static char sortcmd[80] = "sort -n +1 -2 -o /tmp/ord_id < ";
	static char catcmd[80] = "cat /tmp/ord_id >> ";

	(void)signal( SIGINT, sig2 );		/* allow interrupts */

	/* find out which ascii table is desired */
	if( strcmp(argv[0], "solids") == 0 ) {
		/* complete summary - down to solids/paremeters */
		flag = SOL_TABLE;
	}
	else if( strcmp(argv[0], "regions") == 0 ) {
		/* summary down to solids as members of regions */
		flag = REG_TABLE;
	}
	else if( strcmp(argv[0], "idents") == 0 ) {
		/* summary down to regions */
		flag = ID_TABLE;
	}
	else {
		/* should never reach here */
		(void)printf("tables:  input error\n");
		return;
	}

	regflag = numreg = lastmemb = numsol = 0;
	tabptr = NULL;

	/* open the file */
	if( (tabptr=fopen(argv[1], "w+")) == NULL ) {
		(void)fprintf(stderr,"Can't open %s\n",argv[1]);
		return;
	}

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		/* temp file for discrimination of solids */
		if( (idfd = creat("/tmp/mged_discr", 0600)) < 0 ) {
			perror( "/tmp/mged_discr" );
			return;
		}
		rd_idfd = open( "/tmp/mged_discr", 2 );
	}

	(void)time( &now );
	timep = ctime( &now );
	timep[24] = '\0';
	(void)fprintf(tabptr,"1 -8    Summary Table {%s}  (written: %s)\n",argv[0],timep);
	(void)fprintf(tabptr,"2 -7         file name    : %s\n",dbip->dbi_filename);    
	(void)fprintf(tabptr,"3 -6         \n");
	(void)fprintf(tabptr,"4 -5         \n");
	(void)fprintf(tabptr,"5 -4         user         : %s\n",getpwuid(getuid())->pw_gecos);
	(void)fprintf(tabptr,"6 -3         target title : %s\n",cur_title);
	(void)fprintf(tabptr,"7 -2         target units : %s\n",local_unit[localunit]);
	(void)fprintf(tabptr,"8 -1         objects      :");
	for(i=2; i<argc; i++) {
		if( (i%8) == 0 )
			(void)fprintf(tabptr,"\n                           ");
		(void)fprintf(tabptr," %s",argv[i]);
	}
	(void)fprintf(tabptr,"\n\n");

	/* make table of the objects */
	mat_idn( identity );
	for(i=2; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i],LOOKUP_NOISY)) != DIR_NULL )
			tables(dp, 0, identity, flag);
		else
			(void)printf(" skip this object\n");
	}

	(void)printf("Summary written in: %s\n",argv[1]);

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		/* remove the temp file */
		(void)unlink( "/tmp/mged_discr\0" );
		(void)fprintf(tabptr,"\n\nNumber Solids = %d  Number Regions = %d\n",
				numsol,numreg);
		(void)printf("Processed %d Solids and %d Regions\n",numsol,numreg);
		(void)fclose( tabptr );
	}

	else {
		(void)fprintf(tabptr,"* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
		(void)fclose( tabptr );
		(void)printf("Processed %d Regions\n",numreg);
		/* make ordered idents */
		sortcmd[31] = '\0';
		catcmd[19] = '\0';
		(void)strcat(sortcmd, argv[1]);
		(void)system( sortcmd );
		(void)strcat(catcmd, argv[1]);
		(void)system( catcmd );
		(void)unlink( "/tmp/ord_id\0" );
	}

	return;
}




char ctemp[7];
/*
 *
 *	F _ E D C O D E S ( )
 *
 *		control routine for editing region ident codes
 *
 *
 */
void
f_edcodes( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	(void)signal( SIGINT, sig2 );		/* allow interrupts */

	/* need user interaction for this command */
	if( isatty(0) == 0 ) {
		(void)printf("Need user interaction for the 'edcodes' command\n");
		return;
	}

	regflag = lastmemb = 0;

	/* put terminal in raw mode  no echo */
	/* need "nice" way to do this */
	(void)system( "stty raw -echo" );

	for(i=1; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL )
			edcodes(dp, 0);
		else
			(void)printf(" skip this object\n");
	}

	/* put terminal back in cooked mode  -  need "nice" way to do this */
	(void)system( "stty cooked echo" );

	return;
}


/*
 *
 *	F _ D U P ( ) :	checks for dup names in preparation for cat'ing of files
 *
 *
 */
char new_name[NAMESIZE];
char prestr[15];
int ncharadd;



/*
 *
 *		T A B L E S ( ) :    builds ascii tables (summary)
 *
 *
 */

#define MAX_LEVELS 12
struct directory *path[MAX_LEVELS];

void
tables( dp, pathpos, old_xlate, flag)
register struct directory *dp;
int pathpos;
mat_t old_xlate;
int flag;
{

	struct directory *nextdp;
	mat_t new_xlate;
	int nparts, i, k, j;
	int	arslen;
	int	kk = 1;
	int	npt, n, nsoltemp;
	int dchar = 0;
	vect_t	vertex;
	vect_t	vec;

	if( pathpos >= MAX_LEVELS ) {
		(void)printf("nesting exceeds %d levels\n",MAX_LEVELS);
		for(i=0; i<MAX_LEVELS; i++)
			(void)printf("/%s", path[i]->d_namep);
		(void)printf("\n");
		return;
	}

	if( db_get( dbip, dp, &record, 0, 1) < 0 )  READ_ERR_return;
	if( record.u_id == ID_COMB ) {
		if(regflag > 0) {
			/* this comb record is part of a region */
			if(record.c.c_flags == 'R') {
				oper_ok++;
				if(flag == SOL_TABLE || flag == REG_TABLE)
					(void)fprintf(tabptr,"   RG %c %s\n",operate,record.c.c_name);
			}
			else {
				(void)printf("**WARNING** group= %s is member of region= ",record.c.c_name);
				for(k=0;k<pathpos;k++)
					(void)printf("/%s",path[k]->d_namep);
				(void)printf("\n");
			}
			if(lastmemb)
				regflag = lastmemb = 0;
			return;
		}
		regflag = 0;
		nparts = dp->d_len-1;
		if(record.c.c_flags == 'R') {
			/* first region in this path */
			numreg++;
			regflag = 1;

			if( nparts <= 0 )	/* dummy region */
				regflag = 0;

			oper_ok = 0;

			(void)fprintf(tabptr," %-4d%5d%5d%5d%5d  ",numreg,record.c.c_regionid,
					record.c.c_aircode,record.c.c_material,
					record.c.c_los);
			for(k=0;k<pathpos;k++) {
				(void)fprintf(tabptr,"/%s",path[k]->d_namep);
			}
			(void)fprintf(tabptr,"/%s:\n",record.c.c_name);
		}
		lastmemb = 0;
		for(i=1; i<=nparts; i++) {
			mat_t	xmat;

			if(i == nparts)
				lastmemb = 1;
			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;
			operate = record.M.m_relation;

			if(regflag && operate != SUBTRACT)
				oper_ok++;

			path[pathpos] = dp;
			if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
				continue;

			rt_mat_dbmat( xmat, record.M.m_mat );
			mat_mul(new_xlate, old_xlate, xmat);

			/* Recursive call */
			tables(nextdp, pathpos+1, new_xlate, flag);
		}
		return;
	}

	/* not a combination  -  should have a solid */
	/*
	 *
	 *	TO DO ..... include splines and polygons 
	 *
	 */
	if(record.u_id != ID_SOLID && record.u_id != ID_ARS_A) {
		(void)printf("bad record type '%c' should be 'S' or 'A'\n",record.u_id);
		return;
	}

	/* last (bottom) position */
	path[pathpos] = dp;

	if(regflag == 0) {
		/* have a solid that's not part of a region */
		(void)printf("**WARNING** following path (solid) has no region:\n");
		for(k=0;k<=pathpos;k++)
			(void)printf("/%s",path[k]->d_namep);
		(void)printf("\n");
	}

	if( regflag && lastmemb && oper_ok == 0 ) {
		(void)printf("**WARNING** following region has only '-' oprations:\n");
		for(k=0; k<pathpos; k++)
			(void)printf("/%s",path[k]->d_namep);
		(void)printf("\n");
	}

	if(flag == ID_TABLE) {
		if(lastmemb)
			regflag = lastmemb = 0;
		return;
	}

	/* check if this is a new or old solid */
	if(record.u_id == ID_SOLID) {
		(void)strncpy(identt.i_name, record.s.s_name, NAMESIZE);
	}
	else {
		(void)strncpy(identt.i_name, record.a.a_name, NAMESIZE);
	}
	mat_copy(identt.i_mat, old_xlate);

	/* first (quick) look discriminator is based on name alone */
	dchar = 0;
	for(i=0; i<NAMESIZE; i++) {
		if(identt.i_name[i] == 0) 
			break;
		dchar += (identt.i_name[i] << (i&7));
	}

	/* now check if solid is an old one -- method:
		1.  check quick look method
		2.  if quick look match check long method
	 */
	nsoltemp = 0;
	for(i=0; i<numsol; i++) {
		if(dchar == discr[i]) {
			/* have a quick look match - check further */
			(void)lseek(rd_idfd, i*(long)sizeof identt, 0);
			(void)read(rd_idfd, &idbuf, sizeof identt);
			identt.i_index = i + 1;
			if( check(&identt, &idbuf) == 1 ) {
				/* really is old...number = i+1 */
				nsoltemp = i + 1;
				old_or_new = OLDSOLID;
				goto notnew;
			}
			/* false alarm - quick look match but the
			      	long check indicates is not really old
			 */
		}
	}

	/* Have a NEW solid */
	old_or_new = NEWSOLID;
	discr[numsol++] = dchar;
	identt.i_index = numsol;
	if(numsol > MAXSOL) {
		(void)printf("tables: number of solids > max (%d)\n",MAXSOL);
		exit(10);
	}
	(void)lseek(idfd, 0L, 2);
	(void)write(idfd, &identt, sizeof identt);
	nsoltemp = numsol;

	notnew:		/* sent here if solid is an old one */

	if( regflag == 0 ) {
		(void)fprintf(tabptr,"\n\n***** NO REGION for the following solid:\n");
	}

	/*
	 *
	 *	TO DO ..... include splines and polygons 
	 *
	 */

	if(record.u_id == ID_SOLID) {
		(void)fprintf(tabptr,"   %c [%d] %s",operate,nsoltemp,record.s.s_name);
		if(lastmemb) {
			regflag = 0;
			lastmemb = 0;
		}

		if(flag == REG_TABLE || old_or_new == OLDSOLID) {
			(void)fprintf(tabptr,"\n");
			return;
		}

		if( old_or_new == NEWSOLID ) {
			MAT4X3PNT(vec, old_xlate, &record.s.s_values[0]);
			VMOVE( &record.s.s_values[0], vec );
			for(i=3; i<=21; i+=3) {
				MAT4X3VEC(	vec,
						old_xlate,
						&record.s.s_values[i]	);
				VMOVE( &record.s.s_values[i], vec );
			}
			(void)fprintf(tabptr,": %s",
				record.s.s_type==GENARB8 ? "GENARB8" :
				record.s.s_type==GENTGC ? "GENTGC" :
				record.s.s_type==GENELL ? "GENELL": "TOR" );

			if(record.s.s_type == GENARB8) {
				if( (i=record.s.s_cgtype) < 0 )
					i *= -1;
				(void)fprintf(tabptr," (%s)",i==ARB4 ? "ARB4" :
					i==ARB5 ? "ARB5" : i==ARB6 ? "ARB6" :
					i==ARB7 ? "ARB7" : i==RAW ? "ARB6" : "ARB8");
			}
			(void)fprintf(tabptr,"\n");

			/* put parameters in "nice" format and print */
			pr_solid( &record.s );
			for( i=0; i < es_nlines; i++ ) 
				(void)fprintf(tabptr,"%s\n",&es_display[ES_LINELEN*i]);

			/* If in solid edit, re-compute solid params */
			if(state == ST_S_EDIT)
				pr_solid(&es_rec.s);

		}

		return;
	}

	if(record.u_id == ID_ARS_A) {
		n = record.a.a_n;
		(void)fprintf(tabptr,"   %c [%d] %s",operate,nsoltemp,record.a.a_name);
		if(flag == REG_TABLE || old_or_new == OLDSOLID) {
			if(lastmemb)
				regflag = lastmemb = 0;
			(void)fprintf(tabptr,"\n");
			return;
		}

		if(lastmemb) {
			regflag = lastmemb = 0;
		}

		if(old_or_new == OLDSOLID) 
			return;

		(void)fprintf(tabptr," ARS %7d curves%7d points/curve",record.a.a_m,n);
		arslen = record.a.a_totlen;
		for(i=1; i<=arslen; i++) {
			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;
			if( (npt = (n - ((record.b.b_ngranule-1)*8))) > 8 )
				npt = 8;
			if(i == 1) {
				/* vertex */
				MAT4X3PNT(	vertex,
						old_xlate,
						&record.b.b_values[0]	);
				VMOVE( &record.b.b_values[0], vertex );
				kk =1;
			}
			/* rest of vectors */
			for(k=kk; k<npt; k++) {
				MAT4X3VEC(	vec,
						old_xlate,
						&record.b.b_values[k*3]	);
				VADD2(	&record.b.b_values[k*3],
					vertex,
					vec );
			}
			kk = 0;
			/* print this granule */
			for(k=0; k<npt; k+=2) {
				(void)fprintf(tabptr,"\n              ");
				for(j=k*3; (j<(k+2)*3 && j<npt*3); j++) 
					(void)fprintf(tabptr," %10.4f",record.b.b_values[j]*base2local);
			}

		}
		(void)fprintf(tabptr,"\n");
		if(lastmemb)
			regflag = lastmemb = 0;

		return;
	}

}






/*
 *
 *		E D C O D E S ( ) :	edit region ident codes
 *
 *
 */
void
edcodes( dp, pathpos )
register struct directory *dp;
int pathpos;
{

	struct directory *nextdp;
	int nparts, i;

	if( regflag == ABORTED )
		return;

	if( pathpos >= MAX_LEVELS ) {
		(void)printf("nesting exceeds %d levels\n",MAX_LEVELS);
		for(i=0; i<MAX_LEVELS; i++)
			(void)printf("/%s", path[i]->d_namep);
		(void)printf("\n");
		regflag = ABORTED;
		return;
	}

	if( db_get( dbip, dp, &record, 0, 1) < 0 )  READ_ERR_return;
	if( record.u_id == ID_COMB ) {
		if(regflag > 0) {
			/* this comb record is part of a region */
			if(record.c.c_flags == 'R') 
				oper_ok++;
			else {
				(void)printf("**WARNING** group= %s is member of region= ",record.c.c_name);
				for(i=0;i<pathpos;i++)
					(void)printf("/%s",path[i]->d_namep);
				(void)printf("\n");
			}
			if(lastmemb)
				regflag = lastmemb = 0;
			return;
		}
		regflag = 0;
		nparts = dp->d_len-1;
		if(record.c.c_flags == 'R') {
			/* first region in this path */
			regflag = 1;

			if( nparts == 0 )	/* dummy region */
				regflag = 0;

			oper_ok = 0;

			(void)fprintf(stdout,"%-6d%-3d%-3d%-4d  ",record.c.c_regionid,
					record.c.c_aircode,record.c.c_material,
					record.c.c_los);
			for(i=0;i<pathpos;i++) {
				(void)fprintf(stdout,"/%s",path[i]->d_namep);
			}
			(void)fprintf(stdout,"/%s%s\r",record.c.c_name,
					nparts==0 ? " **DUMMY REGION**" : "");
			/*edit this line */
			if( editline(dp) ) {
				(void)printf("aborted\n");
				regflag = ABORTED;
				return;
			}
		}
		lastmemb = 0;
		for(i=1; i<=nparts; i++) {
			if(i == nparts)
				lastmemb = 1;
			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;
			operate = record.M.m_relation;

			if(regflag && operate != SUBTRACT)
				oper_ok++;

			path[pathpos] = dp;
			if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
				continue;
			/* Recursive call */
			edcodes(nextdp, pathpos+1);

		}
		return;
	}

	/* not a combination  -  should have a solid */
	if(record.u_id != ID_SOLID && record.u_id != ID_ARS_A) {
		(void)printf("bad record type '%c' should be 'S' or 'A'\n",record.u_id);
		return;
	}

	/* last (bottom) position */
	path[pathpos] = dp;

	if( lastmemb )
		regflag = lastmemb = 0;

	return;
}




/*    C H E C K      -     compares solids       returns 1 if they match
							 0 otherwise
 */

check( a, b )
register char *a, *b;
{

	register int	c= sizeof( struct identt );

	while( c-- )	if( *a++ != *b++ ) return( 0 );	/* no match */
	return( 1 );	/* match */

}




/*   E D I T L I N E :   allows the user to edit a line of region codes
 */

#define TAB		9
#define BLANK		32
#define BACKSPACE	8
#define CONTROLC	3
#define CRETURN		13
#define LINEFEED	10
#define RESTORE		82	/* R */
#define QUIT_q		113	/* q */

int item, air, mat, los;	/* temp values */

editline(dp)
struct directory	*dp;
{

	char c;
	int field, lpos, eflag, i;
	int elflag, maxpos[4];
 	lpos = eflag = field = elflag = 0;

	/* field lengths */
	maxpos[0] = 6;
	maxpos[1] = maxpos[2] = 3;
	maxpos[3] = 4;

	item = record.c.c_regionid;
	air = record.c.c_aircode;
	mat = record.c.c_material;
	los = record.c.c_los;

	while(1) {
		switch( (c = getchar()) ) {

			case TAB:
			case BLANK:
				/* record any changes to this field */
				if( elflag && lpos > 0 ) {
					ctemp[lpos] = c;
					changes( field );
					elflag = 0;
				}

				/* move cursor to beginning of next field */
				if(lpos == 0) 
					prfield(field);
				else {
					for(i=0;i<(maxpos[field] - lpos); i++)
						(void)putchar(' ');
				}

				lpos = 0;
				if(++field > 3) {
					(void)putchar('\r');
					field = 0;
				}
			break;

			case BACKSPACE:
				if(lpos > 0) {
					(void)putchar('\b');
					lpos--;
				}
				else {
					/* go to beginning of previous field */
					if(elflag) {
						/* this field changed on screen
						 *   but change was not recorded.
						 *   Print out latest recorded
						 *   value.
						 */
						prfield(field);
						elflag = 0;
						for(i=0;i<maxpos[field];i++)
							(void)putchar('\b');
					}
					if(field == 0)
						break;
					field--;
					for(i=0; i<maxpos[field]; i++)
						(void)putchar('\b');
				}
			break;

			case CRETURN:
				(void)putchar('\r');
				(void)putchar('\n');
				if(eflag) {
					if(elflag) {
						/* change present field */
						ctemp[lpos] = ' ';
						elflag = 0;
						changes( field );
					}
					/* update the record with changes */
					record.c.c_regionid = item;
					record.c.c_aircode = air;
					record.c.c_material = mat;
					record.c.c_los = los;
					/* write out all changes */
					if( db_put( dbip, dp, &record, 0, 1 ) < 0 )
						WRITE_ERR_return;
				}
				/* get out of loop */
				return(0);

			case QUIT_q:
				/* 'q' entered ==> quit the editing */
			case CONTROLC:
				(void)putchar('\r');
				(void)putchar('\n');
				return(1);

			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
				/* integer was input */
				eflag++;
				if(lpos < maxpos[field]) {
					elflag++;
					(void)putchar( c );
					ctemp[lpos++] = c;
				}
				else {
					(void)putchar(7);	/* bell */
					(void)putchar(7);	/* bell */
				}
			break;

			case RESTORE:
				/* 'R' was input - restore the line */
				item = record.c.c_regionid;
				air = record.c.c_aircode;
				mat = record.c.c_material;
				los = record.c.c_los;
				lpos = field = eflag = elflag = 0;

				(void)putchar('\r');
				(void)fprintf(stdout,"%-6d%-3d%-3d%-4d",item,air,mat,los);
				(void)putchar('\r');

			break;

			default:
				(void)putchar(7); 	/* ring bell */
			break;
		}	/* end of switch */
	}	/* end of while loop */
}

/*
 *			C H A N G E S 
 */
void
changes( num )
int num;
{

	switch( num ) {

		case 0:
			item = atoi( ctemp );
		break;

		case 1:
			air = atoi( ctemp );
		break;

		case 2:
			mat = atoi( ctemp );
		break;

		case 3:
			los = atoi( ctemp );
		break;

	}
	return;
}

/*
 *			P R F I E L D
 */
void
prfield( num )
int num;
{

	switch( num ) {

		case 0:
			(void)fprintf(stdout,"%-6d",item);
		break;

		case 1:
			(void)fprintf(stdout,"%-3d",air);
		break;

		case 2:
			(void)fprintf(stdout,"%-3d",mat);
		break;

		case 3:
			(void)fprintf(stdout,"%-4d",los);
		break;
	}

	return;

}

/*
 *	F _ W H I C H _ I D ( ) :	finds all regions with given idents
 */
void
f_which_id( argc, argv )
int	argc;
char	**argv;
{
	union record	rec;
	register int	i,j;
	register struct directory *dp;
	int		item;

	/* allow interupts */
	(void)signal( SIGINT, sig2 );

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		(void)printf("Region[s] with ident %d:\n",item);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( (dp->d_flags & DIR_COMB|DIR_REGION) !=
				    (DIR_COMB|DIR_REGION) )
					continue;
				if( db_get( dbip, dp, &rec, 0, 1 ) < 0 )
					READ_ERR_return;
				if( rec.c.c_regionid != item )
					continue;
				(void)printf("   %s\n",rec.c.c_name);
			}
		}
	}
}
