/*                     P I X F I E L D S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file pixfields.c
 *
 *  pixfields takes two input pictures and extracts field 1 from the first
 *  pix file and field 2 comes from the second pix file.
 *  This is useful for creating field-by-field animation for
 *  NTSC video display.
 *
 *  Author -
 *	Christopher T. Johnson - 05 December 1989
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

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include	<string.h>
#else
#  include	<strings.h>
#endif

#include "machine.h"
#include "bu.h"


#define DEFAULT_WIDTH	512

int	width = DEFAULT_WIDTH;
int	height = DEFAULT_WIDTH;
int	verbose = 0;

char	*file_name;
FILE	*fldonefp,*fldtwofp;

char	usage[] = "\
Usage: pixfields [-v]\n\
        [-s squaresize] [-w width] [-n height]\n\
	 field1.pix field2.pix > file.pix\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "vw:n:s:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 'w':
			width = atoi(bu_optarg);
			break;
		case 'n':
			height = atoi(bu_optarg);
			break;
		case 's':
			width = height = atoi(bu_optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc + 1 )  {
		(void) fprintf( stderr,
		    "pixfields: must supply two file names\n");
		return(0);
	} else {

		if( (fldonefp = fopen(argv[bu_optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixfields: cannot open \"%s\" for reading\n",
				argv[bu_optind] );
			return(0);
		}

		if( (fldtwofp = fopen(argv[++bu_optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixfields: cannot open \"%s\" for reading\n",
				argv[bu_optind] );
			return(0);
		}

	}

	if( isatty(fileno(stdout)) )
		return(0);

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "pixfields: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	char	*line1;
	char	*line2;
	int	line_number;

	if ( !get_args( argc, argv ) )  {
		fputs( usage, stderr );
		exit( 1 );
	}

	line1 = (char *) malloc(width*3+1);
	line2 = (char *) malloc(width*3+1);

	line_number = 0;
	for(;;)  {
		if( fread( line1, 3*sizeof(char), width, fldonefp ) != width )
			break;
		if( fread( line2, 3*sizeof(char), width, fldtwofp ) != width )  {
			fprintf(stderr,"pixfields: premature EOF on 2nd file?\n");
			exit(2);
		}
		if ( (line_number & 1) == 0 )  {
			if( fwrite( line1, 3*sizeof(char), width, stdout ) != width )  {
				perror("fwrite line1");
				exit(1);
			}
		} else {
			if( fwrite( line2, 3*sizeof(char), width, stdout ) != width )  {
				perror("fwrite line2");
				exit(1);
			}
		}
		line_number++;
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
