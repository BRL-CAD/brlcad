/*
 *		P I X 3 F I L T E R . C
 *
 *  Filters a color pix file set with an arbitrary 3x3x3 kernel.
 *  Leaves the outer rows untouched.
 *  Allows an alternate divisor and offset to be given.
 *
 *  Author -
 *	Phillip Dykstra
 *	15 Aug 1985
 *
 *  Modified -
 *	Christopher T. Johnson - 01 March 1989
 *	    Added the 3rd dimension.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include	<string.h>
#else
#include	<strings.h>
#endif

#include "machine.h"
#include "externs.h"		/* For getopt */

#define MAXLINE		(8*1024)
#define DEFAULT_WIDTH	512
unsigned char	l11[3*MAXLINE], l12[3*MAXLINE], l13[3*MAXLINE];
unsigned char	l21[3*MAXLINE], l22[3*MAXLINE], l23[3*MAXLINE];
unsigned char	l31[3*MAXLINE], l32[3*MAXLINE], l33[3*MAXLINE];
unsigned char	obuf[3*MAXLINE];
unsigned char	*topold, *middleold, *bottomold, *temp;
unsigned char	*topcur, *middlecur, *bottomcur;
unsigned char	*topnew, *middlenew, *bottomnew;

/* The filter kernels */
struct	kernels {
	char	*name;
	char	*uname;		/* What is needed to recognize it */
	int	kern[27];
	int	kerndiv;	/* Divisor for kernel */
	int	kernoffset;	/* To be added to result */
} kernel[] = {
	{ "Low Pass", "lo", {1, 3, 1, 3, 5, 3, 1, 3, 1,
			    3, 5, 3, 5,10, 5, 3, 5, 3,
			    1, 3, 1, 3, 5, 3, 1, 3, 1}, 84, 0 },
	{ "High Pass", "hi", {-1, -2, -1, -2, -4, -2, -1, -2, -1,
			     -2, -4, -2, -4, 56, -4, -2, -4, -2,
			     -1, -2, -1, -2, -4, -2, -1, -2, -1}, 1, 0},
	{ "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1,
				 1, 1, 1, 1,27, 1, 1, 1, 1,
				 1, 1, 1, 1, 1, 1, 1, 1, 1}, 53, 0},
	{ "Animation Smear", "s",{1, 1, 1, 1, 1, 1, 1, 1, 1,
				 1, 1, 1, 1, 1, 1, 1, 1, 1,
				 2, 2, 2, 2,35, 2, 2, 2, 2}, 69, 0},
	{ NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0 },
};

int	*kern;
int	kerndiv;
int	kernoffset;
int	width = DEFAULT_WIDTH;
int	height = DEFAULT_WIDTH;
int	verbose = 0;
int	dflag = 0;	/* Different divisor specified */
int	oflag = 0;	/* Different offset specified */

char *file_name;
FILE *oldfp, *curfp, *newfp;

void	select_filter(char *str), dousage(void);

char	usage[] = "\
Usage: pix3filter [-f<type>] [-v] [-d#] [-o#]\n\
        [-s squaresize] [-w width] [-n height]\n\
	file.pix.n | file.pix1 file.pix2 file.pix3  > file.pix\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "vf:d:o:w:n:s:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 'f':
			select_filter(optarg);
			break;
		case 'd':
			dflag++;
			kerndiv = atoi(optarg);
			break;
		case 'o':
			oflag++;
			kernoffset = atoi(optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;
		case 's':
			width = height = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		(void) fprintf( stderr,
		    "pix3filter: must supply a file name\n");
		return(0);
	} else if ( optind + 3 <= argc ) {

		if( (oldfp = fopen(argv[optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix3filter: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}

		if( (curfp = fopen(argv[++optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix3filter: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}

		if( (newfp = fopen(argv[++optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix3filter: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}
		optind += 3;
	} else {
		int frameNumber;
		char *idx, *working_name;
		
		file_name = argv[optind];
		working_name = (char *)malloc(strlen(file_name)+5);

		if( (curfp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix3filter: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		idx = file_name + strlen(file_name) -1;
		while ((idx >= file_name) && (*idx != '.')) --idx;
		if (idx >= file_name) {
			*idx = '\0';
			++idx;
			frameNumber = atoi(idx);
		} else {
			(void) fprintf(stderr,
			    "pix3filter: no frame number on %s.\n",
			    file_name);
			return(0);
		}

		sprintf(working_name,"%s.%d", file_name, frameNumber-1);
		if ( (oldfp = fopen(working_name, "r")) == NULL) {
			if (frameNumber-1 != 0) {
				(void)fprintf(stderr,
				    "pix3filter: cannot open \"%s\" for reading.\n",
				    working_name);
				return(0);
			}
			if ( (oldfp = fopen(file_name, "r")) == NULL) {
				(void)fprintf(stderr,
				    "pix3filter: cannot open \"%s\" for reading.\n",
				    file_name);
				return(0);
			}
		}

		sprintf(working_name,"%s.%d", file_name, frameNumber+1);
		if ((newfp = fopen(working_name, "r")) == NULL) {
			(void)fprintf(stderr,
			    "pix3filter: cannot open \"%s\" for reading.\n",
			    working_name);
			return(0);
		}
		free(working_name);
		optind += 1;
	}

	if( isatty(fileno(stdout)) )
		return(0);

	if ( argc > optind )
		(void)fprintf( stderr, "pix3filter: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	int	x, y, color;
	int	value, r1, r2, r3, r4, r5, r6, r7, r8, r9;
	int	max, min;

	/* Select Default Filter (low pass) */
	select_filter( "low" );

	if ( !get_args( argc, argv ) )  {
		dousage();
		exit( 1 );
	}

	if( width > MAXLINE )  {
		fprintf(stderr, "pix3filter:  limited to scanlines of %d\n", MAXLINE);
		exit( 1 );
	}

/*
 * Set up the pointers to the working areas.
 */
	bottomold = l11;
	middleold = l12;
	topold    = l13;

	bottomcur = l21;
	middlecur = l22;
	topcur    = l23;

	bottomnew = l31;
	middlenew = l32;
	topnew    = l33;
/*
 * Read in the bottom and middle rows of the old picture.
 */
	fread( bottomold, sizeof( char ), 3*width, oldfp );
	fread( middleold, sizeof( char ), 3*width, oldfp );
/*
 * Read in the bottom and middle rows of the current picture.
 */
	fread( bottomcur, sizeof( char ), 3*width, curfp );
	fread( middlecur, sizeof( char ), 3*width, curfp );
/*
 * Read in the bottom and middle rows of the new picture.
 */
	fread( bottomnew, sizeof( char ), 3*width, newfp );
	fread( middlenew, sizeof( char ), 3*width, newfp );
/*
 * Write out the bottome row.
 */
	fwrite( bottomcur, sizeof( char ), 3*width, stdout );

	if(verbose) {
		for( x = 0; x < 29; x++ )
			fprintf(stderr, "kern[%d] = %d\n", x, kern[x]);
	}

	max = 0;
	min = 255;

	for( y = 1; y < height-1; y++ ) {
		/* read in top lines */
		fread( topold, sizeof( char ), 3*width, oldfp );
		fread( topcur, sizeof( char ), 3*width, curfp );
		fread( topnew, sizeof( char ), 3*width, newfp );

		for( color = 0; color < 3; color++ ) {
			obuf[0+color] = middlecur[0+color];
			/* Filter a line */
			for( x = 3+color; x < 3*(width-1); x += 3 ) {
				r1 = topold[x-3] * kern[0] +
				    topold[x] * kern[1] +
				    topold[x+3] * kern[2];
				r2 = middleold[x-3] * kern[3] +
				    middleold[x] * kern[4] +
				    middleold[x+3] * kern[5];
				r3 = bottomold[x-3] * kern[6] +
				    bottomold[x] * kern[7] +
				    bottomold[x+3] * kern[8];
				r4 = topcur[x-3] * kern[9] +
				    topcur[x] * kern[10] +
				    topcur[x+3] * kern[11];
				r5 = middlecur[x-3] * kern[12] +
				    middlecur[x] * kern[13] +
				    middlecur[x+3] * kern[14];
				r6 = bottomcur[x-3] * kern[15] +
				    bottomcur[x] * kern[16] +
				    bottomcur[x+3] * kern[17];
				r7 = topnew[x-3] * kern[18] +
				    topnew[x] * kern[19] +
				    topnew[x+3] * kern[20];
				r8 = middlenew[x-3] * kern[21] +
				    middlenew[x] * kern[22] +
				    middlenew[x+3] * kern[23];
				r9 = bottomnew[x-3] * kern[24] +
				    bottomnew[x] * kern[25] +
				    bottomnew[x+3] * kern[26];
				value = (r1+r2+r3+r4+r5+r6+r7+r8+r9) /
				     kerndiv + kernoffset;
				if( value > max ) max = value;
				if( value < min ) min = value;
				if( verbose && (value > 255 || value < 0) ) {
					fprintf(stderr,"Value %d\n", value);
					fprintf(stderr,"r1=%d, r2=%d, r3=%d\n", r1, r2, r3);
				}
				if( value < 0 )
					obuf[x] = 0;
				else if( value > 255 )
					obuf[x] = 255;
				else
					obuf[x] = value;
			}
			obuf[3*(width-1)+color] = middlecur[3*(width-1)+color];
		}
		fwrite( obuf, sizeof( char ), 3*width, stdout );
		/* Adjust row pointers */
		temp = bottomold;
		bottomold = middleold;
		middleold = topold;
		topold = temp;

		temp = bottomcur;
		bottomcur = middlecur;
		middlecur = topcur;
		topcur = temp;

		temp = bottomnew;
		bottomnew = middlenew;
		middlenew = topnew;
		topnew = temp;

	}
	/* write out last line untouched */
	fwrite( topcur, sizeof( char ), 3*width, stdout );

	/* Give advise on scaling factors */
	if( verbose )
		fprintf( stderr, "Max = %d,  Min = %d\n", max, min );

	exit( 0 );
}

/*
 *	S E L E C T _ F I L T E R
 *
 * Looks at the command line string and selects a filter based
 *  on it.
 */
void
select_filter(char *str)
{
	int	i;

	i = 0;
	while( kernel[i].name != NULL ) {
		if( strncmp( str, kernel[i].uname, strlen( kernel[i].uname ) ) == 0 )
			break;
		i++;
	}

	if( kernel[i].name == NULL ) {
		/* No match, output list and exit */
		fprintf( stderr, "Unrecognized filter type \"%s\"\n", str );
		dousage();
		exit( 3 );
	}

	/* Have a match, set up that kernel */
	kern = &kernel[i].kern[0];
	if( dflag == 0 )
		kerndiv = kernel[i].kerndiv;
	if( oflag == 0 )
		kernoffset = kernel[i].kernoffset;
}

void
dousage(void)
{
	int	i;

	fputs( usage, stderr );
	i = 0;
	while( kernel[i].name != NULL ) {
		fprintf( stderr, "%-10s%s\n", kernel[i].uname, kernel[i].name );
		i++;
	}
}
