/*                      G E N C O L O R . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
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
/** @file gencolor.c
 *
 *  Output a pattern of bytes either forever or the requested
 *  number of times.  It gets the byte values either from the
 *  command line, or stdin.  Defaults to black.
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
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#define	MAX_BYTES	(128*1024)

char Usage[] = "usage: gencolor [-r#] [val1 .. valN] > output_file\n";

int	bytes_in_buf, copies_per_buf;

unsigned char	buf[MAX_BYTES];

int
main(int argc, char **argv)
{
	int	i, len, times;
	register long	count;
	register unsigned char *bp;

	if( argc < 1 || isatty(fileno(stdout)) ) {
		fprintf( stderr, Usage );
		exit( 1 );
	}

	count = -1;
	if( argc > 1 && strncmp( argv[1], "-r", 2 ) == 0 ) {
		count = atoi( &argv[1][2] );
		argv++;
		argc--;
	}

	if( argc > 1 ) {
		/* get values from the command line */
		i = 0;
		while( argc > 1 && i < MAX_BYTES ) {
			buf[i] = atoi( argv[i+1] );
			argc--;
			i++;
		}
		len = i;
	} else if( !isatty(fileno(stdin)) ) {
		/* get values from stdin */
		len = fread( (char *)buf, 1, MAX_BYTES, stdin );
		if( len <= 0 ) {
			fprintf( stderr, Usage );
			exit( 2 );
		}
	} else {
		/* assume black */
		buf[0] = 0;
		len = 1;
	}

	/*
	 * Replicate the pattern as many times as it will fit
	 * in the buffer.
	 */
	copies_per_buf = 1;
	bytes_in_buf = len;
	bp = &buf[len];
	while( (MAX_BYTES - bytes_in_buf) >= len ) {
		for( i = 0; i < len; i++ )
			*bp++ = buf[i];
		copies_per_buf++;
		bytes_in_buf += len;
	}

	if( count < 0 ) {
		/* output forever */
		while( 1 )  {
			if( write( 1, (char *)buf, bytes_in_buf ) != bytes_in_buf )  {
				perror("write");
				break;
			}
		}
		exit(1);
	}

	while( count > 0 ) {
		times = copies_per_buf > count ? count : copies_per_buf;
		if( write( 1, (char *)buf, len * times ) != len * times )  {
			perror("write");
			exit(1);
		}
		count -= times;
	}

	exit( 0 );
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
