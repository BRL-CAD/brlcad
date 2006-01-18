/*                       H T E S T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbn */
/*@{*/

/** @file htester.c
 *  Test network float conversion.
 *  Expected to be used in pipes, or with TTCP links to other machines,
 *  or with files RCP'ed between machines.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include "common.h"



#include <stdio.h>

#define	NUM	3000
double	orig[NUM], after[NUM];

char	buf[NUM*8];

main(int argc, char **argv)
{
	register int i;
	register int nbytes;

	if( argc != 2 || argv[1][0] != '-' )  {
		fprintf(stderr,"Usage:  htester [-i|-o]\n");
		exit(1);
	}

	/* First stage, generate the reference pattern */
	for( i=0; i<1000; i++ )  {
		orig[i] = ((double)i)/(7*16);
	}
	for( i=1000; i<2000; i++ )  {
		orig[i] = -1.0/(i-1000+1);
	}
	orig[2000] = -1;
	for( i=2001; i<2035; i++ )  {
		orig[i] = orig[i-1] * -0.1;
	}
	for( i=2035; i<3000; i++ )  {
		orig[i] = orig[i-1000] + (i&1)?(-1):1;
	}

	/* Second stage, write out, or read and compare */
	if( argv[1][1] == 'o' )  {
		/* Write out */
		htond( buf, (char *)orig, NUM );
		fwrite( buf, 8, NUM, stdout );
		exit(0);
	}

	/* Read and compare */
	if( sizeof(double) >= 8 )
		nbytes = 6;
	else
		nbytes = 4;
	fread( buf, 8, NUM, stdin );
/*	ntohd( (char *)after, buf, NUM );	/* bulk conversion */
	for( i=0; i<NUM; i++ )  {
		ntohd( (char *)&after[i], &buf[i*8], 1 );	/* incremental */
		/* Floating point compare */
		if( orig[i] == after[i] )  continue;

		/* Byte-for-byte compare */
		if( ckbytes( &orig[i], &after[i], nbytes ) == 0 )
			continue;

		/* Wrong */
		printf("%4d: calc ", i);
		flpr( &orig[i] );
		printf(" %g\n", orig[i]);
		printf("      aftr ");
		flpr( &after[i] );
		printf(" %g\n", after[i] );
		printf("      buf  ");
		flpr( &buf[i*8] );
		printf("\n");
	}
	exit(0);
}

flpr(register unsigned char *cp)
{
	register int i;
	for( i=0; i<sizeof(double); i++ )  {
		putchar("0123456789ABCDEFx"[*cp>>4]);
		putchar("0123456789ABCDEFx"[*cp&0xF]);
		cp++;
	}
}

ckbytes(register unsigned char *a, register unsigned char *b, register int n)
{
#ifndef vax
	while( n-- > 0 )  {
		if( *a++ != *b++ )
			return(-1);	/* BAD */
	}
	return(0);			/* OK */
#else
	/* VAX floating point has bytes swapped, vis-a-vis normal VAX order */
	register int i;
	for( i=0; i<n; i++ )  {
		if( a[i^1] != b[i^1] )
			return(-1);	/* BAD */
	}
	return(0);			/* OK */
#endif
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
