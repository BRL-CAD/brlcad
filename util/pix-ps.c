/*
 *			P I X - P S . C
 *
 *  Convert an RGB (pix) file to an 24-bit color PostScript image.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>	/* for atof() */
#include <time.h>	/* for ctime() */

#include "machine.h"
#include "externs.h"	/* for getopt() */

#define	DEFAULT_SIZE	6.75		/* default output size in inches */
#define	MAX_BYTES	(3*64*128)	/* max bytes per image chunk */

static int	encapsulated = 0;	/* encapsulated postscript */
static int	center = 1;		/* center output on 8.5 x 11 page */
static int	landscape = 0;		/* landscape mode */

static int	width = 512;		/* input size in pixels */
static int	height = 512;
static double	outwidth;		/* output image size in inches */
static double	outheight;
static int	xpoints;		/* output image size in points */
static int	ypoints;
static int	pagewidth = 612;	/* page size in points - 8.5 inches */
static int	pageheight = 792;	/* 11 inches */

static char	*file_name;
static FILE	*infp;

void prolog(FILE *fp, char *name, int width, int height), postlog(FILE *fp), hexout(FILE *fp, int byte);

static char usage[] = "\
Usage: pix-ps [-e] [-c|-l] [-L] [-h]\n\
        [-s input_squaresize] [-w input_width] [-n input_height]\n\
        [-S inches_square] [-W inches_width] [-N inches_height] [file.pix]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "ehclLs:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'e':
			/* Encapsulated PostScript */
			encapsulated++;
			break;
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'c':
			center = 1;
			break;
		case 'l':
			center = 0;	/* lower left */
			break;
		case 'L':
			landscape = 1;
			break;
		case 's':
			/* square file size */
			height = width = atoi(optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;
		case 'S':
			/* square file size */
			outheight = outwidth = atof(optarg);
			break;
		case 'W':
			outwidth = atof(optarg);
			break;
		case 'N':
			outheight = atof(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "[stdin]";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix-ps: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		/*fileinput++;*/
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-ps: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	FILE	*ofp = stdout;
	int	num = 0;
	int	scans_per_patch, bytes_per_patch;
	int	y;

	outwidth = outheight = DEFAULT_SIZE;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( encapsulated ) {
		xpoints = width;
		ypoints = height;
	} else {
		xpoints = outwidth * 72 + 0.5;
		ypoints = outheight * 72 + 0.5;
	}
	prolog(ofp, file_name, xpoints, ypoints);

	scans_per_patch = MAX_BYTES / (width*3);
	if( scans_per_patch > height )
		scans_per_patch = height;
	bytes_per_patch = scans_per_patch * (width*3);

	for( y = 0; y < height; y += scans_per_patch ) {
		if (y + scans_per_patch > height) {
			scans_per_patch = height-y;
			bytes_per_patch = scans_per_patch * (width*3);
		}

		/* start a patch */
		fprintf(ofp, "save\n");
		fprintf(ofp, "%d %d 8 [%d 0 0 %d 0 %d] {<\n ",
			width, scans_per_patch,		/* patch size */
			width, height,			/* total size = 1.0 */
			-y );				/* patch y origin */

		/* data */
		num = 0;
		while( num < bytes_per_patch ) {
			/*fprintf( ofp, "%02x", getc(infp) );*/
			hexout(ofp,getc(infp));
			if( (++num % 32) == 0 )
				fprintf( ofp, "\n " );
		}

		/* close this patch */
		/*fprintf(ofp, ">} image\n"); BW*/
		fprintf(ofp, ">} false 3 colorimage\n");
		fprintf(ofp, "restore\n");
	}

	postlog( ofp );
	exit( 0 );
}

void
prolog(FILE *fp, char *name, int width, int height)
    	    
    	      
   	              		/* in points */
{
	time_t	ltime;

	ltime = time(0);

	if( encapsulated ) {
		fputs( "%!PS-Adobe-2.0 EPSF-1.2\n", fp );
		fprintf(fp, "%%%%Title: %s\n", name );
		fputs( "%%Creator: BRL-CAD pix-ps\n", fp );
		fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime) );
		fputs( "%%Pages: 0\n", fp );
	} else {
		fputs( "%!PS-Adobe-1.0\n", fp );
		fputs( "%begin(plot)\n", fp );
		fprintf(fp, "%%%%Title: %s\n", name );
		fputs( "%%Creator: BRL-CAD pix-ps\n", fp );
		fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime) );
	}
	fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", width, height );
	fputs( "%%EndComments\n\n", fp );

	if( !encapsulated && landscape ) {
		int	tmp;
		tmp = pagewidth;
		pagewidth = pageheight;
		pageheight = tmp;
		fprintf( fp, "90 rotate\n" );
		fprintf( fp, "0 -%d translate\n", pageheight );
	}
	if( !encapsulated && center ) {
		int	xtrans, ytrans;
		xtrans = (pagewidth - width)/2.0;
		ytrans = (pageheight - height)/2.0;
		fprintf( fp, "%d %d translate\n", xtrans, ytrans );
	}
	fprintf( fp, "%d %d scale\n\n", width, height );
}

void
postlog(FILE *fp)
{
	if( !encapsulated )
		fputs( "%end(plot)\n", fp );

	fputs( "\nshowpage\n", fp );
}

/*
 * Print a byte in 2-character hexadecimal.
 */
void
hexout(FILE *fp, int byte)
         
         		/* 0 <= byte <= 255 */
{
	int high, low;
	static int symbol[16] = { '0', '1', '2', '3', '4', '5', '6',
		'7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	high = (byte>>4) & 0xf;
	low = byte & 0xf;

	putc(symbol[high], fp);
	putc(symbol[low], fp);
}
