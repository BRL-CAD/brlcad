/*                         P L - P L . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
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
/** @file pl-pl.c
 *
 *  Plot smasher.
 *  Gets rid of (floating point, flush, 3D, color, text).
 *
 *  Author -
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
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <unistd.h>

#include "machine.h"
#include "bu.h"


struct uplot {
	int	targ;	/* type of args */
	int	narg;	/* number or args */
	char	*desc;	/* description */
	int	t3d;	/* non-zero if 3D */
};

void	putshort(short int s);
void    putieee(double d);
void    getstring(void);
void	putargs(struct uplot *up);
void    getargs(struct uplot *up);
void	doscale(void);

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

double	getieee(void);
int	verbose;
double	arg[6];			/* parsed plot command arguments */
double	sp[6];			/* space command */
char	strarg[512];		/* string buffer */
double	cx, cy, cz;		/* center */
double	scale = 0;
int	seenscale = 0;

int	nofloat = 1;
int	noflush = 1;
int	nocolor = 1;
int	no3d = 1;

static char usage[] = "\
Usage: pl-pl [-v] [-S] < unix_plot > unix_plot\n";

int
main(int argc, char **argv)
{
	register int	c;
	struct	uplot *up;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-v") == 0 ) {
			verbose++;
		} else if( strcmp(argv[1], "-S") == 0 ) {
			scale = 1;
		} else
			break;

		argc--;
		argv++;
	}
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	/* Assume default space, in case one is not provided */
	sp[0] = sp[1] = sp[2] = -32767;
	sp[3] = sp[4] = sp[5] = 32767;
	if( scale )
		doscale();

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

		/* check for space command */
		switch( c ) {
		case 's':
		case 'w':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = 0;
			sp[3] = arg[2];
			sp[4] = arg[3];
			sp[5] = 0;
			if( scale )
				doscale();
			seenscale++;
			break;
		case 'S':
		case 'W':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = arg[2];
			sp[3] = arg[3];
			sp[4] = arg[4];
			sp[5] = arg[5];
			if( scale )
				doscale();
			seenscale++;
			break;
		}

		/* do it */
		switch( c ) {
		case 's':
		case 'm':
		case 'n':
		case 'p':
		case 'l':
		case 'c':
		case 'a':
		case 'f':
		case 'e':
			/* The are as generic as unix plot gets */
			putchar( c );
			putargs( up );
			break;

		case 't': /* XXX vector lists */
			putchar( c );
			putargs( up );
			break;

		case 'C':
			if( nocolor == 0 ) {
				putchar( c );
				putargs( up );
			}
			break;

		case 'F':
			if( noflush == 0 ) {
				putchar( c );
				putargs( up );
			}
			break;

		case 'S':
		case 'M':
		case 'N':
		case 'P':
		case 'L':
			if( no3d )
				putchar( c + 'a' - 'A' );
			else
				putchar( c );
			putargs( up );
			break;

		case 'w':
		case 'o':
		case 'q':
		case 'x':
		case 'v':
		case 'i':
		case 'r':
			/* 2d floating */
			if( nofloat ) {
				/* to 2d integer */
				if( c == 'w' )
					putchar( 's' );
				else if( c == 'o' )
					putchar( 'm' );
				else if( c == 'q' )
					putchar( 'n' );
				else if( c == 'x' )
					putchar( 'p' );
				else if( c == 'v' )
					putchar( 'l' );
				else if( c == 'i' )
					putchar( 'c' );
				else if( c == 'r' )
					putchar( 'a' );
			} else {
				putchar( c );
			}
			putargs( up );
			break;

		case 'W':
		case 'O':
		case 'Q':
		case 'X':
		case 'V':
			/* 3d floating */
			if( nofloat ) {
				if( no3d ) {
					/* to 2d integer */
					if( c == 'W' )
						putchar( 's' );
					else if( c == 'O' )
						putchar( 'm' );
					else if( c == 'Q' )
						putchar( 'n' );
					else if( c == 'X' )
						putchar( 'p' );
					else if( c == 'V' )
						putchar( 'l' );
				} else {
					/* to 3d integer */
					if( c == 'W' )
						putchar( 'S' );
					else if( c == 'O' )
						putchar( 'M' );
					else if( c == 'Q' )
						putchar( 'N' );
					else if( c == 'X' )
						putchar( 'P' );
					else if( c == 'V' )
						putchar( 'L' );
				}
			} else {
				if( no3d ) {
					/* to 2d floating */
					putchar( c + 'a' - 'A' );
				} else {
					/* to 3d floating */
					putchar( c );
				}
			}
			putargs( up );
			break;
		}

		if( verbose )
			printf( "%s\n", up->desc );
	}

	if( scale && !seenscale ) {
		fprintf( stderr, "pl-pl: WARNING no space command in file, defaulting to +/-32k\n" );
	} else if( !scale && seenscale ) {
		fprintf( stderr, "pl-pl: WARNING space command(s) ignored, use -S to apply them.\n" );
	}

	return(0);
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

short
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

double
getieee(void)
{
	char	in[8];
	double	d;

	fread( in, 8, 1, stdin );
	ntohd( (unsigned char *)&d, (unsigned char *)in, 1 );
	return	d;
}

/*** Input args ***/

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
/*** Output args ***/

void
putargs(struct uplot *up)
{
	int	i;

	for( i = 0; i < up->narg; i++ ) {
		if( no3d && ((i % 3) == 2) && up->t3d )
			continue;	/* skip z coordinate */
		/* gag me with a spoon... */
		if( scale && (up->targ == TSHORT || up->targ == TIEEE) ) {
			if( up->t3d ) {
				if( i % 3 == 0 )
					arg[i] = (arg[i] - cx) * scale;
				else if( i % 3 == 1 )
					arg[i] = (arg[i] - cy) * scale;
				else
					arg[i] = (arg[i] - cz) * scale;
			} else {
				if( i % 2 == 0 )
					arg[i] = (arg[i] - cx) * scale;
				else
					arg[i] = (arg[i] - cy) * scale;
			}
		}
		switch( up->targ ) {
			case TSHORT:
				if( arg[i] > 32767 ) arg[i] = 32767;
				if( arg[i] < -32767 ) arg[i] = -32767;
				putshort( (short)arg[i] );
				break;
			case TIEEE:
				if( nofloat ) {
					if( arg[i] > 32767 ) arg[i] = 32767;
					if( arg[i] < -32767 ) arg[i] = -32767;
					putshort( (short)arg[i] );
				} else
					putieee( arg[i] );
				break;
			case TSTRING:
				printf( "%s\n", strarg );
				break;
			case TCHAR:
				putchar( arg[i] );
				break;
			case TNONE:
			default:
				break;
		}
	}
}

void
putshort(short int s)
{
	/* For the sake of efficiency, we trust putc()
	 * to write only one byte
	 */
	putchar( s );
	putchar( s>>8 );
}

void
putieee(double d)
{
	unsigned char	out[8];

	htond( out, (unsigned char *)&d, 1 );
	fwrite( out, 1, 8, stdout );
}

void
doscale(void)
{
	double	dx, dy, dz;
	double	max;

	cx = (sp[3] + sp[0]) / 2.0;
	cy = (sp[4] + sp[1]) / 2.0;
	cz = (sp[5] + sp[2]) / 2.0;

	dx = (sp[3] - sp[0]) / 2.0;
	dy = (sp[4] - sp[1]) / 2.0;
	dz = (sp[5] - sp[2]) / 2.0;

	max = dx;
	if( dy > max ) max = dy;
	if( dz > max ) max = dz;

	scale = 32767.0 / max;
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
