/*
 *			C M A P - F B . C
 *
 *  Load a colormap into a framebuffer.
 *
 *  Author -
 *	Robert Reschly
 *	Phillip Dykstra
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
#include <stdlib.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include "machine.h"
#include "fb.h"

static char *nextsym(register char *b, register char *cp);
static int htoi(register char *s);

ColorMap cm;
static char usage[] = "\
Usage: cmap-fb [-h -o] [colormap]\n";

int
main(int argc, char **argv)
{
	FBIO	*fbp;
	FILE	*fp;
	int	fbsize = 512;
	int	overlay = 0;
	int	index, ret;
	char	line[512], buf[512], *str;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-h") == 0 ) {
			fbsize = 1024;
		} else if( strcmp(argv[1], "-o") == 0 ) {
			overlay++;
		} else if( argv[1][0] == '-' ) {
			/* unknown flag */
			fprintf( stderr, usage );
			exit( 1 );
		} else
			break;	/* must be a filename */
		argc--;
		argv++;
	}

	if( argc > 1 ) {
		if( (fp = fopen(argv[1], "r")) == NULL ) {
			fprintf( stderr, "cmap-fb: can't open \"%s\"\n", argv[1] );
			fprintf( stderr, usage );
			exit( 2 );
		}
	} else
		fp = stdin;

	if( (fbp = fb_open( NULL, fbsize, fbsize )) == FBIO_NULL )
		exit( 3 );

	if( overlay )
		fb_rmap( fbp, &cm );

	while( fgets(line, 511, fp) != NULL ) {
		str = line;
		str = nextsym( buf, str );
		if( ! isdigit(buf[0]) ) {
			/* spare the 0 entry the garbage */
			continue;
		}
		index = atoi( buf );
		if( index < 0 || index > 255 ) {
			continue;
		}
		str = nextsym( buf, str );
		cm.cm_red[index] = htoi( buf );

		str = nextsym( buf, str );
		cm.cm_green[index] = htoi( buf );

		str = nextsym( buf, str );
		cm.cm_blue[index] = htoi( buf );
	}

	ret = fb_wmap( fbp, &cm );
	fb_close( fbp );
	if( ret < 0 ) {
		fprintf( stderr, "cmap-fb: couldn't write colormap\n" );
		exit(1);
	}
	exit(0);
}

/*
 *  Puts the next symbol from cp into the buffer b.
 *  Returns a pointer to the current location (one
 *  char beyond the symbol or at a NULL).
 */
static
char *
nextsym(register char *b, register char *cp)
{
	/* skip white */
	while( isspace(*cp) )
		cp++;

	while( *cp != '\0' && !isspace(*cp) )
		*b++ = *cp++;

	*b = '\0';
	return( cp );
}

/*
 *  Hex to integer
 *  must have NO leading blanks
 *  does not check for errors.
 */
static
int
htoi(register char *s)
{
	register int	i;

	i = 0;

	while( *s != '\0' ) {
		i <<= 4;	/* times 16 */
		if( *s == 'x' || *s == 'X' )
			i = 0;
		else if( *s >= 'a' )
			i += *s - 'a' + 10;
		else if( *s >= 'A' )
			i += *s - 'A' + 10;
		else
			i += *s - '0';
		s++;
	}
	return( i );
}
