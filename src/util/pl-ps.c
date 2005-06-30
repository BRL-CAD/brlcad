/*                         P L - P S . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pl-ps.c
 *
 *  Display plot3(5) as PostScript.
 *  Based on pl-X.c and bw-ps.c
 *
 *  Authors -
 *	Michael John Muuss
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for atof() */
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"

#define	TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define	TIEEE	3	/* IEEE 64-bit floating */
#define	TCHAR	4	/* unsigned chars */
#define	TSTRING	5	/* linefeed terminated string */

struct uplot {
	int	targ;	/* type of args */
	int	narg;	/* number or args */
	char	*desc;	/* description */
};
struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
/*A*/	{ 0, 0, 0 },
/*B*/	{ 0, 0, 0 },
/*C*/	{ TCHAR, 3, "color" },
/*D*/	{ 0, 0, 0 },
/*E*/	{ 0, 0, 0 },
/*F*/	{ TNONE, 0, "flush" },
/*G*/	{ 0, 0, 0 },
/*H*/	{ 0, 0, 0 },
/*I*/	{ 0, 0, 0 },
/*J*/	{ 0, 0, 0 },
/*K*/	{ 0, 0, 0 },
/*L*/	{ TSHORT, 6, "3line" },
/*M*/	{ TSHORT, 3, "3move" },
/*N*/	{ TSHORT, 3, "3cont" },
/*O*/	{ TIEEE, 3, "d_3move" },
/*P*/	{ TSHORT, 3, "3point" },
/*Q*/	{ TIEEE, 3, "d_3cont" },
/*R*/	{ 0, 0, 0 },
/*S*/	{ TSHORT, 6, "3space" },
/*T*/	{ 0, 0, 0 },
/*U*/	{ 0, 0, 0 },
/*V*/	{ TIEEE, 6, "d_3line" },
/*W*/	{ TIEEE, 6, "d_3space" },
/*X*/	{ TIEEE, 3, "d_3point" },
/*Y*/	{ 0, 0, 0 },
/*Z*/	{ 0, 0, 0 },
/*[*/	{ 0, 0, 0 },
/*\*/	{ 0, 0, 0 },
/*]*/	{ 0, 0, 0 },
/*^*/	{ 0, 0, 0 },
/*_*/	{ 0, 0, 0 },
/*`*/	{ 0, 0, 0 },
/*a*/	{ TSHORT, 6, "arc" },
/*b*/	{ 0, 0, 0 },
/*c*/	{ TSHORT, 3, "circle" },
/*d*/	{ 0, 0, 0 },
/*e*/	{ TNONE, 0, "erase" },
/*f*/	{ TSTRING, 1, "linmod" },
/*g*/	{ 0, 0, 0 },
/*h*/	{ 0, 0, 0 },
/*i*/	{ TIEEE, 3, "d_circle" },
/*j*/	{ 0, 0, 0 },
/*k*/	{ 0, 0, 0 },
/*l*/	{ TSHORT, 4, "line" },
/*m*/	{ TSHORT, 2, "move" },
/*n*/	{ TSHORT, 2, "cont" },
/*o*/	{ TIEEE, 2, "d_move" },
/*p*/	{ TSHORT, 2, "point" },
/*q*/	{ TIEEE, 2, "d_cont" },
/*r*/	{ TIEEE, 6, "d_arc" },
/*s*/	{ TSHORT, 4, "space" },
/*t*/	{ TSTRING, 1, "label" },
/*u*/	{ 0, 0, 0 },
/*v*/	{ TIEEE, 4, "d_line" },
/*w*/	{ TIEEE, 4, "d_space" },
/*x*/	{ TIEEE, 2, "d_point" },
/*y*/	{ 0, 0, 0 },
/*z*/	{ 0, 0, 0 }
};

void	getargs(struct uplot *up);
void	getstring(void);
void	draw(double x1, double y1, double z1, double x2, double y2, double z2);
void	label(double x, double y, char *str);
void	prolog(FILE *fp, char *name, int width, int height);
void	scaleinfo(FILE *fp, int width, int height), postlog(FILE *fp);
double	getieee(void);

int	verbose;
double	cx, cy, cz;		/* current x, y, z, point */
double	arg[6];			/* parsed plot command arguments */
double	sp[6];			/* space command */
char	strarg[512];		/* string buffer */

#define	DEFAULT_SIZE	6.75	/* default output size in inches */

extern char	*optarg;
extern int	optind;

int	encapsulated = 0;	/* encapsulated postscript */
int	center = 0;		/* center output on 8.5 x 11 page */
int	width = 4096;		/* Our integer plotting space */
int	height = 4096;
double	outwidth;		/* output plot size in inches */
double	outheight;
int	xpoints;		/* output plot size in points */
int	ypoints;
int	page_dirty = 0;		/* to skip extra erases */

static char	*file_name;
static FILE	*infp;

static char usage[] = "\
Usage: pl-ps [-e] [-c] [-S inches_square]\n\
        [-W width_inches] [-N height_inches] [file.pl]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "ecs:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'e':
			/* Encapsulated PostScript */
			encapsulated++;
			break;
		case 'c':
			center = 1;
			break;
		case 'S':
		case 's':
			/* square file size */
			outheight = outwidth = atof(optarg);
			break;
		case 'W':
		case 'w':
			outwidth = atof(optarg);
			break;
		case 'N':
		case 'n':
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
				"pl-ps: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		/*fileinput++;*/
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pl-ps: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int	c;
	struct	uplot *up;

	outwidth = outheight = DEFAULT_SIZE;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* Default space */
	sp[0] = sp[1] = sp[2] = 0.0;	/* minimum */
	sp[3] = sp[4] = sp[5] = 4096.0;	/* max */

	if( encapsulated ) {
		xpoints = width;
		ypoints = height;
	} else {
		xpoints = outwidth * 72 + 0.5;
		ypoints = outheight * 72 + 0.5;
	}
	prolog(stdout, file_name, xpoints, ypoints);

	while( (c = getc(infp)) != EOF ) {
		/* look it up */
		if( c < 'A' || c > 'z' ) {
			up = &uerror;
		} else {
			up = &letters[ c - 'A' ];
		}

		if( up->targ == TBAD ) {
			fprintf( stderr, "Bad command '%c' (0x%02x)\n", c, c );
			continue;
		}

		if( up->narg > 0 )
			getargs( up );

		switch( c ) {
		case 's':
		case 'w':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = 0;
			sp[3] = arg[2];
			sp[4] = arg[3];
			sp[5] = 0;
			break;
		case 'S':
		case 'W':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = arg[2];
			sp[3] = arg[3];
			sp[4] = arg[4];
			sp[5] = arg[5];
			break;
		case 'm':
		case 'o':
			cx = arg[0];
			cy = arg[1];
			cz = 0;
			break;
		case 'M':
		case 'O':
			cx = arg[0];
			cy = arg[1];
			cz = arg[2];
			break;
		case 'n':
		case 'q':
			draw( cx, cy, cz, arg[0], arg[1], 0.0 );
			break;
		case 'N':
		case 'Q':
			draw( cx, cy, cz, arg[0], arg[1], arg[2] );
			break;
		case 'l':
		case 'v':
			draw( arg[0], arg[1], 0.0, arg[2], arg[3], 0.0 );
			break;
		case 'L':
		case 'V':
			draw( arg[0], arg[1], arg[2], arg[3], arg[4], arg[5] );
			break;
		case 'p':
		case 'x':
			draw( arg[0], arg[1], 0.0, arg[0], arg[1], 0.0 );
			break;
		case 'P':
		case 'X':
			draw( arg[0], arg[1], arg[2], arg[0], arg[1], arg[2] );
			break;
		case 't':
			label( cx, cy, strarg );
			break;
		case 'F':
			/* Flush */
			break;
		case 'e':
			/* New page */
			if( page_dirty ) {
				printf("showpage\n");
				/* Recenter and rescale! */
				scaleinfo(stdout, xpoints, ypoints);
				page_dirty = 0;
			}
			break;
		case 'f':
			/* linmod */
			if( strcmp( strarg, "solid" ) == 0 )  {
				printf("NV ");
			} else if( strcmp( strarg, "dotted" ) == 0 )  {
				printf("DV ");
			} else if( strcmp( strarg, "longdashed" ) == 0 )  {
				printf("LDV ");
			} else if( strcmp( strarg, "shortdashed" ) == 0 )  {
				printf("SDV ");
			} else if( strcmp( strarg, "dotdashed" ) == 0 )  {
				printf("DDV ");
			} else {
				fprintf(stderr,"linmod %s unknown\n", strarg);
			}
			break;
		}

		if( verbose )
			fprintf( stderr, "%s\n", up->desc );
	}

	postlog( stdout );
	exit(0);
}

int
getshort(void)
{
	register long	v, w;

	v = getc(infp);
	v |= (getc(infp)<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}

void
getargs(struct uplot *up)
{
	int	i;

	for( i = 0; i < up->narg; i++ ) {
		switch( up->targ ){
			case TSHORT:
				arg[i] = getshort();
				break;
			case TIEEE:
				arg[i] = getieee();
				break;
			case TSTRING:
				getstring();
				break;
			case TCHAR:
				arg[i] = getc(infp);
				break;
			case TNONE:
			default:
				arg[i] = 0;	/* ? */
				break;
		}
	}
}

void
getstring(void)
{
	int	c;
	char	*cp;

	cp = strarg;
	while( (c = getc(infp)) != '\n' && c != EOF )
		*cp++ = c;
	*cp = 0;
}


double
getieee(void)
{
	unsigned char	in[8];
	double	d;

	fread( in, 8, 1, infp );
	ntohd( (unsigned char *)&d, in, 1 );
	return	d;
}

void
draw(double x1, double y1, double z1, double x2, double y2, double z2)
      	           	/* from point */
      	           	/* to point */
{
	int	sx1, sy1, sx2, sy2;

	sx1 = (x1 - sp[0]) / (sp[3] - sp[0]) * width;
	sy1 = (y1 - sp[1]) / (sp[4] - sp[1]) * height;
	sx2 = (x2 - sp[0]) / (sp[3] - sp[0]) * width;
	sy2 = (y2 - sp[1]) / (sp[4] - sp[1]) * height;

#if 0
	if( sx1 == sx2 && sy1 == sy2 )
		XDrawPoint( dpy, win, gc, sx1, sy1 );
	else
#endif
	printf("newpath %d %d moveto %d %d lineto stroke\n",
		sx1, sy1, sx2, sy2 );

	cx = x2;
	cy = y2;
	cz = z2;

	page_dirty = 1;
}

void
label(double x, double y, char *str)
{
	int	sx, sy;
	static int lastx = -1;
	static int lasty = -1;

	sx = (x - sp[0]) / (sp[3] - sp[0]) * width;
	sy = (y - sp[1]) / (sp[4] - sp[1]) * height;

	/* HACK for "continued text", i.e. more text
	 * without a move command in between.  We
	 * really need a better solution. - XXX
	 */
	if( lastx == x && lasty == y ) {
		printf("DFntM (%s) show\n", str );

	} else {
		printf("DFntM (%s) %d %d moveto show\n",
			str, sx, sy );
		lastx = x;
		lasty = y;
	}

	page_dirty = 1;
}

char boilerplate[] = "\
4 setlinewidth\n\
\n\
% Sizes, made functions to avoid scaling if not needed\n\
/FntH /Courier findfont 80 scalefont def\n\
/DFntL { /FntL /Courier findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /Courier findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /Courier findfont 44 scalefont def } def\n\
\n\
% line styles\n\
/NV { [] 0 setdash } def		% normal vectors\n\
/DV { [8] 0 setdash } def		% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def		% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def		% long-dash vectors\n\
\n\
FntH  setfont\n\
NV\n\
% Begin Plot Data\n\
";

void
prolog(FILE *fp, char *name, int width, int height)
    	    
    	      
   	              		/* in points */
{
	time_t	ltime;

	ltime = time(0);

	if( encapsulated ) {
		fputs( "%!PS-Adobe-2.0 EPSF-1.2\n", fp );
		fputs( "%%Creator: BRL-CAD pl-ps\n", fp );
		fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime) );
		fprintf(fp, "%%%%Title: %s\n", name );
		fputs( "%%Pages: 0\n", fp );
	} else {
		fputs( "%!PS-Adobe-1.0\n", fp );
		fputs( "%begin(plot)\n", fp );
		fputs( "%%DocumentFonts:  Courier\n", fp );
		fprintf(fp, "%%%%Title: %s\n", name );
		fputs( "%%Creator: BRL-CAD pl-ps\n", fp );
		fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime) );
	}
	fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", width, height );
	fputs( "%%EndComments\n\n", fp );

	scaleinfo(fp, xpoints, ypoints);
	fputs( boilerplate, fp );
}

void
scaleinfo(FILE *fp, int width, int height)
    	    
   	              		/* in points */
{
	/*
	 * About this PostScript scaling issue...
	 * A "unit" in postscript with no scaling is 1/72 of an inch
	 * (i.e. one point).  Thus below, 8.5 x 11 inches is converted
	 * to POINTS (*72).  The width and height are already given
	 * in points.
	 * All of our UnixPlot commands are scaled to a first quadrant
	 * space from 0-4096 x 0-4096.  We thus calculate a scale
	 * command to bring this space down to our width and height.
	 */
	if( !encapsulated && center ) {
		int	xtrans, ytrans;
		xtrans = (8.5*72 - width)/2.0;
		ytrans = (11*72 - height)/2.0;
		fprintf( fp, "%d %d translate\n", xtrans, ytrans );
	}
	fprintf( fp, "%f %f scale\n", width/4096.0, height/4096.0 );
}

void
postlog(FILE *fp)
{
	fputs( "\n", fp );
	if( !encapsulated )
		fputs( "%end(plot)\n", fp );
	/*
	 * I believe the Adobe spec says that even Encapsulated
	 * PostScript files can end with a showpage.
	 */
	fputs( "showpage\n", fp );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
