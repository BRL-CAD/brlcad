/*
 *			P L - P S . C
 *
 *  Display plot3(5) as PostScript.
 *  Based on pl-X.c
 *
 *  Authors -
 *	Michael John Muuss
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

extern	char	*getenv();

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

double	getieee();
int	verbose;
double	cx, cy, cz;		/* current x, y, z, point */
double	arg[6];			/* parsed plot command arguments */
double	sp[6];			/* space command */
char	strarg[512];		/* string buffer */
int	width = 4096;
int	height = 4096;

static char usage[] = "\
Usage: pl-ps [-v] < unix_plot\n";


main( argc, argv )
int	argc;
char	**argv;
{
	register int	c;
	struct	uplot *up;
	int	i;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-v") == 0 ) {
			verbose++;
		} else
			break;

		argc--;
		argv++;
	}

	/* Input file will be binary */
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	/* Default space */
	sp[0] = sp[1] = sp[2] = 0.0;	/* minimum */
	sp[3] = sp[4] = sp[5] = 4096.0;	/* max */

	/* Postscript prolog */
	fputs( "%!PS-Adobe-1.0\n\
%begin(plot)\n\
%%DocumentFonts:  Courier\n", stdout );

	printf("%%%%Title: Converted BRL unix-plot file\n" );

	fputs( "\
%%Creator: BRL-CAD pl-ps.c\n\
%%BoundingBox: 0 0 324 324	% 4.5in square, for TeX\n\
%%EndComments\n\
\n", stdout );

	fputs( "\
4 setlinewidth\n\
\n\
% Sizes, made functions to avoid scaling if not needed\n\
/FntH /Courier findfont 80 scalefont def\n\
/DFntL { /FntL /Courier findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /Courier findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /Courier findfont 44 scalefont def } def\n\
\n\
% line styles\n\
/NV { [] 0 setdash } def	% normal vectors\n\
/DV { [8] 0 setdash } def	% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	% long-dash vectors\n\
\n\
/NEWPG {\n\
	.0791 .0791 scale	% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH  setfont\n\
NV\n\
NEWPG\n\
", stdout);

	while( (c = getchar()) != EOF ) {
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
			printf("NEWPG\n");
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

	exit(0);
}

getargs( up )
struct uplot *up;
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
				arg[i] = getchar();
				break;
			case TNONE:
			default:
				arg[i] = 0;	/* ? */
				break;
		}
	}
}

getstring()
{
	int	c;
	char	*cp;

	cp = strarg;
	while( (c = getchar()) != '\n' && c != EOF )
		*cp++ = c;
	*cp = 0;
}

getshort()
{
	register long	v, w;

	v = getchar();
	v |= (getchar()<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}

double
getieee()
{
	char	in[8];
	double	d;

	fread( in, 8, 1, stdin );
	ntohd( &d, in, 1 );
	return	d;
}

draw( x1, y1, z1, x2, y2, z2 )
double	x1, y1, z1;	/* from point */
double	x2, y2, z2;	/* to point */
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
}

label( x, y, str )
double	x, y;
char	*str;
{
	int	sx, sy;

	sx = (x - sp[0]) / (sp[3] - sp[0]) * width;
	sy = (y - sp[1]) / (sp[4] - sp[1]) * height;

	printf("DFntM (%s) %d %d moveto show\n",
		str, sx, sy );
}
