/*                        P L - A S C . C
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
/** @file pl-asc.c
 *
 *  Plot3(5) to ASCII converter.
 *  Grew out of pldebug to be more generally useful as an ascii
 *  editable plot file format.
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
#include <string.h>
#else
#include <strings.h>
#endif
#include <unistd.h>

#include "common.h"


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
/*L*/	{ TSHORT, 6, "line" },
/*M*/	{ TSHORT, 3, "move" },
/*N*/	{ TSHORT, 3, "draw" },
/*O*/	{ TIEEE, 3, "move" },
/*P*/	{ TSHORT, 3, "point" },
/*Q*/	{ TIEEE, 3, "draw" },
/*R*/	{ 0, 0, 0 },
/*S*/	{ TSHORT, 6, "space" },
/*T*/	{ 0, 0, 0 },
/*U*/	{ 0, 0, 0 },
/*V*/	{ TIEEE, 6, "line" },
/*W*/	{ TIEEE, 6, "space" },
/*X*/	{ TIEEE, 3, "point" },
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
/*i*/	{ TIEEE, 3, "circle" },
/*j*/	{ 0, 0, 0 },
/*k*/	{ 0, 0, 0 },
/*l*/	{ TSHORT, 4, "line" },
/*m*/	{ TSHORT, 2, "move" },
/*n*/	{ TSHORT, 2, "draw" },
/*o*/	{ TIEEE, 2, "move" },
/*p*/	{ TSHORT, 2, "point" },
/*q*/	{ TIEEE, 2, "draw" },
/*r*/	{ TIEEE, 6, "arc" },
/*s*/	{ TSHORT, 4, "space" },
/*t*/	{ TSTRING, 1, "label" },
/*u*/	{ 0, 0, 0 },
/*v*/	{ TIEEE, 4, "line" },
/*w*/	{ TIEEE, 4, "space" },
/*x*/	{ TIEEE, 2, "point" },
/*y*/	{ 0, 0, 0 },
/*z*/	{ 0, 0, 0 }
};

int	verbose;
long	counts['z'-'A'+1];	/* for counting command usage */
FILE	*fp;

void	outchar(int n);
void	outstring(int n);
void	outshort(int n);
void	outfloat(int n);

static char usage[] = "\
Usage: pl-asc [-v] [unix_plot]\n";

int
main(int argc, char **argv)
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
	if( argc == 2 ) {
		if( (fp = fopen(argv[1],"r")) == NULL ) {
			perror( "pl-asc" );
			exit( 1 );
		}
	} else {
		fp = stdin;
		if( argc > 1 || isatty(fileno(stdin)) ) {
			fprintf( stderr, usage );
			exit( 1 );
		}
	}

	while( (c = getc(fp)) != EOF ) {
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
		if( verbose ) {
			counts[ c - 'A' ]++;
			printf( "%s\t", up->desc );
		} else {
			printf( "%c\t", c );
		}

		if( up->narg > 0 ) {
			switch( up->targ ) {
			case TNONE:
				break;
			case TSHORT:
				outshort( up->narg );
				break;
			case TIEEE:
				outfloat( up->narg );
				break;
			case TSTRING:
				outstring( up->narg );
				break;
			case TCHAR:
				outchar( up->narg );
				break;
			}
		}

		putchar( '\n' );
	}

	if( verbose ) {
		/* write command usage summary */
		for( i = 0; i < 'z'-'A'+1; i++ ) {
			if( counts[i] != 0 ) {
				fprintf( stderr, "%s %ld\n", letters[i].desc, counts[i] );
			}
		}
	}
	return 0;
}

void
outchar(int n)
{
	int	i, c;

	for( i = 0; i < n; i++ ) {
		if( i != 0 )
			putchar(' ');
		c = getc(fp);
		printf("%3d", c );
	}
}

void
outstring(int n)
{
	int	c;

	putchar('"');
	while( (c = getc(fp)) != '\n' && c != EOF )
		putchar(c);
	putchar('"');
}

short
getshort(void)
{
	register long	v, w;

	v = getc(fp);
	v |= (getc(fp)<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}

void
outshort(int n)
{
	int	i;
	short	s;

	for( i = 0; i < n; i++ ) {
		if( i != 0 )
			putchar(' ');
		s = getshort();
		printf("%d", s );
	}
}

void
outfloat(int n)
{
	int	i;
	char	in[8*16];
	double	out[16];

	fread( in, 8, n, fp );
	ntohd( (unsigned char *)out, (unsigned char *)in, n );

	for( i = 0; i < n; i++ ) {
		if( i != 0 )
			putchar(' ');
		printf("%g", out[i] );
	}
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
