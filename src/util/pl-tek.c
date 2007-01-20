/*                        P L - T E K . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2007 United States Government as represented by
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
/** @file pl-tek.c
 *
 *  Convert 3-D color extended UNIX-plot file to Tektronix 4014 plot.
 *  Gets rid of (floating point, flush, 3D, color, text).
 *
 *  Authors -
 *	Michael John Muss
 *	Phillip Dykstra
 *
 *  Based heavily on pl-pl.c
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <unistd.h>
#include <stdlib.h>

#include "common.h"


#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

struct uplot {
	int	targ;	/* type of args */
	int	narg;	/* number or args */
	char	*desc;	/* description */
	int	t3d;	/* non-zero if 3D */
};

void	getstring(void);
void	getargs(struct uplot *up);
double	getieee(void);
void	doscale(void);

static void	tekmove(int xi, int yi);
static void	tekcont(register int x, register int y);
static void	tekerase(void);
static void	teklabel(register char *s);
static void	teklinemod(register char *s);
static void	tekpoint(int xi, int yi);

#define BELL	007
#define	FF	014
#define SUB	032		/* Turn on graphics cursor */
#define GS	035		/* Enter Graphics Mode (1st vec dark) */
#define ESC	033
#define US	037		/* Enter Alpha Mode */

#define	TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define	TIEEE	3	/* IEEE 64-bit floating */
#define	TCHAR	4	/* unsigned chars */
#define	TSTRING	5	/* linefeed terminated string */

struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
/*A*/	{ 0, 0, 0, 0 },
/*B*/	{ 0, 0, 0, 0 },
/*C*/	{ TCHAR, 3, "color", 0 },
/*D*/	{ 0, 0, 0, 0 },
/*E*/	{ 0, 0, 0, 0 },
/*F*/	{ TNONE, 0, "flush", 0 },
/*G*/	{ 0, 0, 0, 0 },
/*H*/	{ 0, 0, 0, 0 },
/*I*/	{ 0, 0, 0, 0 },
/*J*/	{ 0, 0, 0, 0 },
/*K*/	{ 0, 0, 0, 0 },
/*L*/	{ TSHORT, 6, "3line", 1 },
/*M*/	{ TSHORT, 3, "3move", 1 },
/*N*/	{ TSHORT, 3, "3cont", 1 },
/*O*/	{ TIEEE, 3, "d_3move", 1 },
/*P*/	{ TSHORT, 3, "3point", 1 },
/*Q*/	{ TIEEE, 3, "d_3cont", 1 },
/*R*/	{ 0, 0, 0, 0 },
/*S*/	{ TSHORT, 6, "3space", 1 },
/*T*/	{ 0, 0, 0, 0 },
/*U*/	{ 0, 0, 0, 0 },
/*V*/	{ TIEEE, 6, "d_3line", 1 },
/*W*/	{ TIEEE, 6, "d_3space", 1 },
/*X*/	{ TIEEE, 3, "d_3point", 1 },
/*Y*/	{ 0, 0, 0, 0 },
/*Z*/	{ 0, 0, 0, 0 },
/*[*/	{ 0, 0, 0, 0 },
/*\*/	{ 0, 0, 0, 0 },
/*]*/	{ 0, 0, 0, 0 },
/*^*/	{ 0, 0, 0, 0 },
/*_*/	{ 0, 0, 0, 0 },
/*`*/	{ 0, 0, 0, 0 },
/*a*/	{ TSHORT, 6, "arc", 0 },
/*b*/	{ 0, 0, 0, 0 },
/*c*/	{ TSHORT, 3, "circle", 0 },
/*d*/	{ 0, 0, 0, 0 },
/*e*/	{ TNONE, 0, "erase", 0 },
/*f*/	{ TSTRING, 1, "linmod", 0 },
/*g*/	{ 0, 0, 0, 0 },
/*h*/	{ 0, 0, 0, 0 },
/*i*/	{ TIEEE, 3, "d_circle", 0 },
/*j*/	{ 0, 0, 0, 0 },
/*k*/	{ 0, 0, 0, 0 },
/*l*/	{ TSHORT, 4, "line", 0 },
/*m*/	{ TSHORT, 2, "move", 0 },
/*n*/	{ TSHORT, 2, "cont", 0 },
/*o*/	{ TIEEE, 2, "d_move", 0 },
/*p*/	{ TSHORT, 2, "point", 0 },
/*q*/	{ TIEEE, 2, "d_cont", 0 },
/*r*/	{ TIEEE, 6, "d_arc", 0 },
/*s*/	{ TSHORT, 4, "space", 0 },
/*t*/	{ TSTRING, 1, "label", 0 },
/*u*/	{ 0, 0, 0, 0 },
/*v*/	{ TIEEE, 4, "d_line", 0 },
/*w*/	{ TIEEE, 4, "d_space", 0 },
/*x*/	{ TIEEE, 2, "d_point", 0 },
/*y*/	{ 0, 0, 0, 0 },
/*z*/	{ 0, 0, 0, 0 }
};

int	verbose;
double	arg[6];			/* parsed plot command arguments */
double	sp[6];			/* space command */
double	scale;			/* rescale factor */
char	strarg[512];		/* string buffer */
int	seenscale = 0;
int	expand_it = 0;		/* expand plot to 4k, beyond what will fit on real Tek screen */

static char usage[] = "\
Usage: pl-tek [-e] [-v] < file.pl > file.tek\n";

int
main(int argc, char **argv)
{
	register int	c;
	struct	uplot *up;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-v") == 0 ) {
			verbose++;
		} else if( strcmp( argv[1], "-e" ) == 0 )  {
			expand_it = 1;
		} else {
			fprintf(stderr, "pl-tek: argument '%s' ignored\n", argv[1]);
			break;
		}

		argc--;
		argv++;
	}
	/* Stdout may be a genuine Tektronix! */
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	/* Assume default space, in case one is not provided */
	sp[0] = sp[1] = sp[2] = -32767;
	sp[3] = sp[4] = sp[5] = 32767;
	doscale();

	/* Initialize the Tektronix */
	(void)putc(ESC,stdout);
	(void)putc(';',stdout);		/* Miniature typeface */
	(void)putc(US,stdout);

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

		if( verbose )  {
			register int	i;
			fprintf( stderr, "%s", up->desc );
			switch( up->targ )  {
			case TCHAR:
			case TSHORT:
			case TIEEE:
				for( i=0; i < up->narg; i++ )
					fprintf( stderr, " %g", arg[i] );
				break;
			case TSTRING:
				fprintf( stderr, " '%s'", strarg );
				break;
			}
			fprintf( stderr, "\n");
		}

		/* check for space command */
		switch( c ) {
		case 's':		/* space */
		case 'w':		/* d_space */
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = 0;
			sp[3] = arg[2];
			sp[4] = arg[3];
			sp[5] = 0;
			doscale();
			seenscale++;
			continue;
		case 'S':		/* 3space */
		case 'W':		/* d_3space */
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = arg[2];
			sp[3] = arg[3];
			sp[4] = arg[4];
			sp[5] = arg[5];
			doscale();
			seenscale++;
			continue;
		}

		/* do it */
		switch( c ) {
		case 'm':	/* 2-d move */
		case 'M':	/* 3move */
		case 'o':	/* d_move */
		case 'O':	/* d_3move */
			tekmove( (int)((arg[0] - sp[0]) * scale),
				 (int)((arg[1] - sp[1]) * scale) );
			break;

		case 'n':	/* 2-d continue */
		case 'N':	/* 3cont */
		case 'q':	/* d_cont */
		case 'Q':	/* d_3cont */
			tekcont( (int)((arg[0] - sp[0]) * scale),
				 (int)((arg[1] - sp[1]) * scale) );
			break;

		case 'p':	/* 2-d point */
		case 'P':	/* 3point */
		case 'x':	/* d_point */
		case 'X':	/* d_3point */
			tekpoint( (int)((arg[0] - sp[0]) * scale),
				 (int)((arg[1] - sp[1]) * scale) );
			break;

		case 'l':	/* 2-d line */
		case 'v':	/* d_line */
			tekmove( (int)((arg[0] - sp[0]) * scale),
				 (int)((arg[1] - sp[1]) * scale) );
			tekcont( (int)((arg[2] - sp[0]) * scale),
				 (int)((arg[3] - sp[1]) * scale) );
			break;

		case 'L':	/* 3line */
		case 'V':	/* d_3line */
			tekmove( (int)((arg[0] - sp[0]) * scale),
				 (int)((arg[1] - sp[1]) * scale) );
			tekcont( (int)((arg[3] - sp[0]) * scale),
				 (int)((arg[4] - sp[1]) * scale) );
			break;

		case 'c':	/* circle */
		case 'i':	/* d_circle */
			fprintf(stderr,"pl-tek: circle unimplemented\n");
			break;

		case 'a':	/* arc */
		case 'r':	/* d_arc */
			fprintf(stderr,"pl-tek: arc unimplemented\n");
			break;

		case 'f':	/* linmod */
			teklinemod( strarg );
			break;

		case 'e':	/* erase */
			tekerase();
			break;

		case 't': 	/* text label */
			teklabel( strarg );
			break;

		case 'C':	/* set color */
			break;

		case 'F':	/* flush buffer */
			break;

		default:
			fprintf(stderr, "pl-tek: unknown command byte x%x\n", c );
		}
	}

	if( !seenscale ) {
		fprintf( stderr, "pl-tek: WARNING no space command in file, defaulting to +/-32k\n" );
	}

	return(0);
}

/*** Input args ***/

int
getshort(void)
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

void
getargs(struct uplot *up)
{
	int	i;

	for( i = 0; i < up->narg; i++ ) {
		switch( up->targ ) {
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

void
getstring(void)
{
	int	c;
	char	*cp;

	cp = strarg;
	while( (c = getchar()) != '\n' && c != EOF )
		*cp++ = c;
	*cp = 0;
}


double
getieee(void)
{
	unsigned char	in[8];
	double	d;

	fread( in, 8, 1, stdin );
	ntohd( (unsigned char *)&d, in, 1 );
	return	d;
}

/*
 * Establish display coordinate conversion:
 *  Input ranges from min=(sp[0], sp[1]) to max=(sp[3], sp[4]).
 *  Tektronix is using 0..4096, but not all is visible.
 *  So, use a little less.
 *  To convert, subtract the min val, and multiply by 'scale'.
 *  Out of range detection is converters problem.
 */
void
doscale(void)
{
	double	dx, dy, dz;
	double	max;

	dx = (sp[3] - sp[0]);
	dy = (sp[4] - sp[1]);
	dz = (sp[5] - sp[2]);

	max = dx;
	if( dy > max ) max = dy;
	if( dz > max ) max = dz;

	if( expand_it )
		scale = 4096 / max;
	else
		scale = (4096-1000) / max;
	if( verbose )  {
		fprintf( stderr, "doscale: min=(%g, %g), max=(%g, %g), scale=%g\n",
			sp[0], sp[1],
			sp[3], sp[4],
			scale );
	}
}

/*
 * Perform the interface functions
 * for the Tektronix 4014-1 with Extended Graphics Option.
 * The Extended Graphics Option makes available a field of
 * 10 inches vertical, and 14 inches horizontal, with a resolution
 * of 287 points per inch.
 *
 * The Tektronix is Quadrant I, 4096x4096 (not all visible).
 */
static int oloy = -1;
static int ohiy = -1;
static int ohix = -1;
static int oextra = -1;

/* Continue motion from last position */
static void
tekcont(register int x, register int y)
{
	int hix,hiy,lox,loy,extra;
	int n;

	if( verbose ) fprintf(stderr," tekcont(%d,%d)\n", x, y );
	hix=(x>>7) & 037;
	hiy=(y>>7) & 037;
	lox = (x>>2)&037;
	loy=(y>>2)&037;
	extra = (x & 03) + ((y<<2) & 014);
	n = (abs(hix-ohix) + abs(hiy-ohiy) + 6) / 12;
	if(hiy != ohiy){
		(void)putc(hiy|040,stdout);
		ohiy=hiy;
	}
	if(hix != ohix) {
		if(extra != oextra) {
			(void)putc(extra|0140,stdout);
			oextra=extra;
		}
		(void)putc(loy|0140,stdout);
		(void)putc(hix|040,stdout);
		ohix=hix;
		oloy=loy;
	} else {
		if(extra != oextra) {
			(void)putc(extra|0140,stdout);
			(void)putc(loy|0140,stdout);
			oextra=extra;
			oloy=loy;
		} else if(loy != oloy) {
			(void)putc(loy|0140,stdout);
			oloy=loy;
		}
	}
	(void)putc(lox|0100,stdout);
	while(n--)
		(void)putc(0,stdout);
}

static void
tekmove(int xi, int yi)
{
	(void)putc(GS,stdout);			/* Next vector blank */
	tekcont(xi,yi);
}

static void
tekerase(void)
{
	extern unsigned sleep(unsigned int);	/* DAG -- was missing */

	(void)putc(ESC,stdout);
	(void)putc(FF,stdout);
	ohix = ohiy = oloy = oextra = -1;
	(void)fflush(stdout);

	(void)sleep(3);
}

static void
teklabel(register char *s)
{
	(void)putc(US,stdout);
	for( ; *s; s++ )
		(void)putc(*s,stdout);
	ohix = ohiy = oloy = oextra = -1;
}

static void
teklinemod(register char *s)
{
	register int c;				/* DAG -- was char */

	(void)putc(ESC,stdout);
	switch(s[0]){
	case 'l':
		c = 'd';
		break;
	case 'd':
		if(s[3] != 'd')c='a';
		else c='b';
		break;
	case 's':
		if(s[5] != '\0')c='c';
		else c='`';
		break;
	default:			/* DAG -- added support for colors */
		c = '`';
		break;
	}
	(void)putc(c,stdout);
}

static void
tekpoint(int xi, int yi) {
	tekmove(xi,yi);
	tekcont(xi,yi);
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
