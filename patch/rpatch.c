/*	Front end to patch.
 *	John R. Anderson
 *		Based on rpatch.f by Bill Mermagen Jr. 
 *     This pre-processor program alters the data file format
 *     for use by the main conversion program.
 */

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#if defined USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define	MAXLINELEN	256

static char *usage="Usage:\n\trpatch [-D] < fastgen_input_file > file.rp\n\
	where -D means that type 3 components are donuts (rather than triangles)\n";

double
get_ftn_float( str , start_col , format )
char *str,*format;
unsigned int start_col;
{
	char *ptr;
	char tmp_str[MAXLINELEN];
	int width,precision;
	int leading_spaces=1;
	int i;

	/* Check that format is legal for floating point or double */
	ptr = format;
	if( *ptr != 'F' && *ptr != 'f' && *ptr != 'E' && *ptr != 'e' && *ptr != 'D' && *ptr != 'd' )
	{
		fprintf( stderr , "Get_ftn_float( str=%s\n, start_col=%d, format=%s )\n",
				str, start_col, ptr );
		fprintf( stderr , "\tformat must be F, E, or D type\n" );
		exit( 1 );
	}

	/* if start column is beyond end of input string, return zero */
	if( start_col >= strlen( str ) )
		return( (double)0.0 );

	/* get width from format spec */
	ptr++;
	width = atoi( ptr );

	/* make sure input string will fit in tmp_str (minus trailing NULL and a decimal point) */
	if( width > MAXLINELEN-2 )
	{
		fprintf( stderr , "Get_ftn_float( str=%s\n, start_col=%d, format=%s )\n",
				str, start_col, ptr );
		fprintf( stderr , "\tfield width (%d) in format is too large. Max allowed is %d\n",
				width , MAXLINELEN-2 );
		exit( 1 );
	}

	/* copy the input string to tmp_str, converting
	 * imbedded blanks to zeros and 'D' or 'd' to 'e'
	 */
	for( i=0 ; i<width ; i++ )
	{
		if( isspace( str[start_col+i] ) )
		{
			if( leading_spaces )
				tmp_str[i] = ' ';
			else
				tmp_str[i] = '0';
		}
		else if( str[start_col+i] == 'D' || str[start_col+i] == 'd' )
		{
			leading_spaces = 0;
			tmp_str[i] = 'e';
		}
		else
		{
			leading_spaces = 0;
			tmp_str[i] = str[start_col+i];
		}
	}
	tmp_str[width] = '\0';

	/* get precision from format spec */
	ptr = strchr( ptr , '.' );
	if( ptr )
		precision = atoi( ++ptr );
	else
		precision = 0;


	/* if there is a decimal point, let atof handle the rest (including exponent) */
	if( !strchr( tmp_str , '.' ) && precision > 0 )
	{
		/* insert a decimal point where needed */
		for( i=0 ; i<precision ; i++ )
			tmp_str[width-i] = tmp_str[width-i-1];
		tmp_str[width-precision] = '.';
		tmp_str[width+1] = '\0';

		/* and atof can handle it from here */
	}

	return( atof( tmp_str ) );
}

int
get_ftn_int( str , start_col , format )
char *str,*format;
unsigned int start_col;
{
	char *ptr;
	char tmp_str[MAXLINELEN];
	int leading_spaces=1;
	int width;
	int i;

	/* check that format id for an integer */
	ptr = format;
	if( *ptr != 'I' && *ptr != 'i' )
	{
		fprintf( stderr , "Get_ftn_int( str=%s\n, start_col=%d, format=%s )\n",
				str, start_col, ptr );
		fprintf( stderr , "\tformat must be I type\n" );
		exit( 1 );
	}

	/* if start column is beyond end of input string, return zero */
	if( start_col >= strlen( str ) )
		return( 0 );

	/* get width from format spec */
	ptr++;
	width = atoi( ptr );

	/* make sure input string will fit in tmp_str */
	if( width > MAXLINELEN-1 )
	{
		fprintf( stderr , "Get_ftn_int( str=%s\n, start_col=%d, format=%s )\n",
				str, start_col, ptr );
		fprintf( stderr , "\tfield width (%d) in format is too large. Max allowed is %d\n",
				width , MAXLINELEN-1 );
		exit( 1 );
	}

	/* copy the input string to tmp_str, converting
	 * imbedded blanks to zeros */
	for( i=0 ; i<width ; i++ )
	{
		if( isspace( str[start_col+i] ) )
		{
			if( leading_spaces )
				tmp_str[i] = ' ';
			else
				tmp_str[i] = '0';
		}
		else
		{
			leading_spaces = 0;
			tmp_str[i] = str[start_col+i];
		}
	}
	tmp_str[width] = '\0';

	return( atoi( tmp_str ) );
}

main( argc, argv )
int argc;
char *argv[];
{
	char line[MAXLINELEN];
	float x,y,z,hold,work;
	char minus;
	int ity,ity1,ico,isq[8],m,n,k,cc,tmp;
	int i;
	int type3_is_donut=0;
	int c;

	if( argc > 2 )
	{
		fprintf( stderr, usage );
		exit( 1 );
	}

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "D")) != EOF)
	{
		switch (c)
		{
			case 'D':  /* donuts */
				type3_is_donut = 1;
				break;

			default:
				fprintf( stderr, "Illegal option (%c)\n", c );
			case '?':
				fprintf( stderr, usage );
				exit( 1 );
		}
	}

	while( gets(line) )
	{
		if( strlen( line ) == 0 )
			continue;

		x = get_ftn_float( line , 0 , "f8.3" );
		y = get_ftn_float( line , 8 , "f8.3" );
		z = get_ftn_float( line , 16 , "f9.3" );
		tmp = get_ftn_int( line , 25 , "i6" );
		cc = get_ftn_int( line , 31 , "i4" );
		isq[0] = get_ftn_int( line , 35 , "i11" );

		for( i=1 ; i<8 ; i++ )
			isq[i] = get_ftn_int( line , 46 + (i-1)*4 , "i4" );

		m = get_ftn_int( line , 74 , "i3" );
		n = get_ftn_int( line , 77 , "i3" );

		/* get plate mode flag */
		minus = '+';
		if( tmp < 0 )
		{
			tmp = (-tmp);
			minus = '-';
		}

		/* get solid type */
		hold = (float)tmp/10000.0;
		work = hold * 10.0;
		ity = work;
		hold = work - ity;
		if( ity == 4 )
			ity = 8;
		else if( ity == 3 && type3_is_donut )
			ity = 4;

		/* get thickness */
		work = hold * 100.0;
		ico = work;
		hold = work - ico;

		/* get space code */
		work = hold * 10.0;
		ity1 = work;
		hold = work - ity1;

		/* write output */
		printf( "%8.3f %8.3f %9.3f %c %2d %2d %1d %4d %11d ",
			x,y,z,minus,ity,ico,ity1,cc,isq[0] );
		for( i=1 ; i<8 ; i++ )
			printf( "%5d" , isq[i] );
		printf( " %3d %3d\n" , m , n );

	}
}
