/*			C V T . C
 *	This is the mainline for converting COM-GEOM
 * cards to a GED style database.
 * Ballistic Research Laboratory
 * February, 1980
Date	Name	REVISION HISTORY
March 81  CAS	Added processing for ARS
09/22/81  MJM	Converted to matrix transformations.
8205.20	Bob S.	Fixed the name generation.  It seems that when a zero
			is encountered CVT skips the rest of the digits
			when converting from an integer to a string.
			I am useing F2A to do the conversion.
			Also Keith wants a string appended to the
			output name. QED
8205.21 Bob S.	Appending the string to the g recoreds.
			I did this one the hard way.
 */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include "./ged_types.h"
#include "./3d.h"

char name_it[16];	/* stores argv if it exists and appends it
			to each name generated.*/
long tell();

char *titles[] = {
	"holder",
	"RPP",		/* 1 */
	"BOX",
	"RAW",
	"ARB4",
	"ARB5",
	"ARB6",
	"ARB7",
	"ARB8",
	"ELL",
	"ELL1",
	"SPH",
	"RCC",
	"REC",
	"TRC",
	"TEC",
	"TOR",
	"TGC",
	"CONE",
	"ELLG",
	"8888",		/* 20 */
};

#define NUMPERCOL 8

int outfd;		/* Output file descriptor */
int updfd;		/* Update file descriptor */

int sol_total, sol_work;	/* total num solids, num solids processed */

union record record;

long regionpos;
long endpos;

int digits;

main( argc, argv )
char **argv;
{
	register int i;
	register float *op;
	int nclm = 0;

	if( ! (argc == 3 || argc == 4) )
	{
		printf(	"Usage:  comg-g [opts] input_file output_file" );
		printf(		" [ name_postfix ]\n");
		exit(10);
	}
		/* get postfix, else use it as a null string.*/
	if( argc == 4 )		strncpy( name_it, argv[3], 16 );
	else			strncpy( name_it, "", 16 );
	
	close( 0 );		/* Want input as std in */
	if( open( argv[1], 0 ) != 0 )
	{
		perror( argv[1] );
		exit(10);
	}
	/* The two separate opens here are necessary to get us
	 * file descriptors with independent file pointers.
	 * This will be very important when the summary combinations
	 * are to be produced.  updfd will be used to read the
	 * processed file, and outfd will be used to append to it.
	 */
	if( (outfd = creat( argv[2], 0644 )) < 0 )  {
		perror( argv[2] );
		exit( 10 );
	}
	updfd = open( argv[2], 2 );

#ifdef GIFT5
	printf("\n**** CVT5: 12Jun84 ****\n\n");
#else
	printf("\n**** CVT4: 12Jun84 ****\n\n");
#endif

	printf("COMGEOM input file must have this format:\n");
	printf("     1.  title card\n");
	printf("     2.  control card\n");
	printf("     3.  solid table\n");
	printf("     4.  region table\n");
	printf("     5.  -1\n");
#ifndef GIFT4
	printf("     6.  blank\n");
#endif
   	printf("     7.  region ident table\n\n");

	/* read title */
	get_title();

	/* read control card */
	sol_total = 0;
	get_control();

	printf("Number Solids: %d\n\n",sol_total);

	printf("processing solid table\n");
	sol_work = 0;
	/* SOLID TABLE */
	while(1) {
		for(i=0; i<24; i++)
			record.s.s_values[i] = 0.0;

		if( (i = getsolid(&record.s)) == 0 ) {
			printf("\nprocessed %d of %d solids\n\n",sol_work,sol_total);
			break;		/* done */
		}

		if( i == 1 ) {
			/* solid type other than ARS */
			convert( &record.s );
			write(outfd, &record.s, sizeof record);
			printf("%s \t",record.s.s_name);
			if( ++nclm  >= NUMPERCOL ) {
				printf("\n");
				nclm = 0;
			}
		}
		if( i == 2 ) {
			/* ARS solid */
			if( ++nclm >= NUMPERCOL ) {
				printf("\n");
				nclm = 0;
			}
		}


	}

	/* Record the region position */
	regionpos = tell( outfd );

	/* REGION TABLE */

	printf("processing region table\n");
	nclm = 0;
	while(1) {
		if( getregion( &record.c ) == 0 )
			break;
		printf("%s \t",record.c.c_name);
		if( ++nclm  >= NUMPERCOL ) {
			printf("\n");
			nclm = 0;
		}
	}

	/* REGION IDENT TABLE */
	lseek( updfd, regionpos, 0 );

#ifndef GIFT5
	/* read the blank card (line) */
	getline( &record );
#endif

	printf("\nprocessing region ident table\n");

	while( 1 ) {
		if( (i = getid( &record )) == 0 )   /* finished */
			break;
	}

	endpos = tell(outfd);

	/* Grouping time
	 * outfd is extended sequentially past endpos,
	 * while updfd is used to scan region combinations,
	 * and update group headers.
join( &record, "g00", 0, 0 );	join( &record, "g0", 1, 99 );
join( &record, "g1", 100, 199)	join( &record, "g2", 200, 299 );
join( &record, "g3", 300, 399 );join( &record, "g4", 400, 499 );
join( &record, "g5", 500, 599 );join( &record, "g6", 600, 699 );
join( &record, "g7", 700, 799 );join( &record, "g8", 800, 899 );
join( &record, "g9", 900, 999 );join( &record, "g10", 1000, 1999 );
join( &record, "g11", 2000, 2999 );join( &record, "g12", 3000, 3999 );
join( &record, "g13", 4000, 4999 );join( &record, "g14", 5000, 5999 );
join( &record, "g15", 6000, 6999 );join( &record, "g16", 7000, 7999 );
join( &record, "g17", 8000, 8999 );join( &record, "g18", 9000, 9999 );
join( &record, "g19", 10000, 32767 );		**************/

printf("producing groups\n");
	{ static int i,k,n;	  static char str[12], st[12];
		strcpy( str, "g00");
		strcat( str, name_it );
		join( &record, str, 0,0 );

		strcpy( str, "g0" );
		strcat( str, name_it );
		join( &record, str, 1, 99 );

		n = 100;
		for( i=1, k=100; i <= 9; ++i, k += n)
		{	str[1] = '\0';
			f2a( (float)i, st, 12, 0 );
			strcat( str, st );
			strcat( str, name_it );
			join( &record, str, k, (k + n -1) );
		}
		n = 1000;
		for( ; i <= 18; ++i, k += n)
		{	str[1] = '\0';
			f2a( (float)i, st, 12, 0 );
			strcat( str, st );
			strcat( str, name_it );
			join( &record, str, k, (k + n -1) );
		}

		strcpy( str, "g19" );
		strcat( str, name_it );
		join( &record, str, 10000, 32767 );

	}
}

/*	J O I N			 */
join( rp, name, low, high )register union record *rp;
{ static struct members M;  static struct combination header;
  int count;  long joinpos;		/* where we joined in */
	int numb = 0;

  	printf("group(%s)  items(%d -> %d)\n",name,low,high);

	joinpos = tell( outfd );
	lseek( updfd, regionpos, 0 );
	movename( name, header.c_name );
	header.c_id = COMB;	header.c_flags = 0;
	header.c_regionid = -1;	header.c_aircode = 0;
	header.c_length = 1;	header.c_num = 0;
	header.c_material = 0;
	header.c_los = 0;
	count = 0;
	while( tell( updfd ) < endpos )  {
		read( updfd, rp, sizeof record );
		if(		rp->c.c_regionid >= low
			&&	rp->c.c_regionid <= high )
		{
			M.m_id = MEMB;
			M.m_relation = UNION;
			mat_idn( M.m_mat );

			movename( rp->c.c_name, M.m_instname );
			M.m_brname[0] = 0;

			/* If this is first entry, write header */
			if( count++ == 0 )
				write( outfd, &header, sizeof record );
			write( outfd, &M, sizeof record );
			printf("%s\t", M.m_instname );
			if( ++numb >= NUMPERCOL ) {
				printf("\n");
				numb = 0;
			}
		}

			/* Seek over the member elements of this
						(input) combination */
		lseek(	updfd,
			rp->c.c_length * ((long) sizeof record),
			1 );
	}

	/* If no members, we are done -- nothing has been written yet */
	if( count == 0 )
	{
		return;
	}

		/* go back to the COMB header and insert
					the proper length */
	lseek( updfd, joinpos, 0 );
	read( updfd, rp, sizeof record );
	rp->c.c_length = count;

	lseek( updfd, joinpos, 0 );
	write( updfd, rp, sizeof record );

	printf("\n");

}

/*		M O V E N A M E
 * Ensure that name field is null padded */
movename( from, to )
register char *from, *to;
{ register int i;
	for( i=0; i<16; i++ )		to[i] = 0;
	while( *to++ = *from++ );
}
