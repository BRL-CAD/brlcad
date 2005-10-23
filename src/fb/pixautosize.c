/*                   P I X A U T O S I Z E . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file pixautosize.c
 *
 *  Program to determine if a given file is one of the "standard"
 *  sizes as known by the framebuffer library.
 *
 *  Used by pixinfo.sh to determine size of .pix and .bw files.
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
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static int	bytes_per_sample = 3;
static int	file_length = 0;
static char	*file_name;

static unsigned long int	width;
static unsigned long int	height;

static char usage[] = "\
Usage:	pixautosize [-b bytes_per_sample] [-f file_name]\n\
or	pixautosize [-b bytes_per_sample] [-l file_length]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "b:f:l:" )) != EOF )  {
		switch( c )  {
		case 'b':
			bytes_per_sample = atoi(bu_optarg);
			break;
		case 'f':
			file_name = bu_optarg;
			break;
		case 'l':
			file_length = atoi(bu_optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "pixautosize: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	int	ret = 0;
	int	nsamp;

	if ( !get_args( argc, argv ) || bytes_per_sample <= 0 )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if ( !file_name && file_length <= 0 )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( file_name ) {
		if( !bn_common_file_size(&width, &height, file_name, bytes_per_sample) ) {
			fprintf(stderr,"pixautosize: unable to autosize file '%s'\n", file_name);
			ret = 1;		/* ERROR */
		}
	} else {
		nsamp = file_length/bytes_per_sample;
		if( !bn_common_image_size(&width, &height, nsamp) ) {
			fprintf(stderr,"pixautosize: unable to autosize nsamples=%d\n", nsamp);
			ret = 2;		/* ERROR */
		}
	}

	/*
	 *  Whether or not an error message was printed to stderr above,
	 *  print out the width and height on stdout.
	 *  They will be zero on error.
	 */
	printf("WIDTH=%lu; HEIGHT=%lu\n", width, height);
	return ret;
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
