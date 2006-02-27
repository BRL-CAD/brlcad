/*                            C V . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2006 United States Government as represented by
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
/** @file cv.c
 *
 *  Author -
 *	Michael John Muuss
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"


char usage[] = "\
Usage: cv in_pat out_pat [[infile] outfile]\n\
\n\
Where a pattern is: [h|n][s|u] c|s|i|l|d|8|16|32|64\n\
e.g., hui is host unsigned int, nl is network (signed) long\n\
";

int	in_cookie;
int	out_cookie;

int	iitem;
int	oitem;

int	inbytes;
int	outbytes;

FILE	*infp;
FILE	*outfp;

genptr_t	ibuf;
genptr_t	obuf;

int
main(int argc, char **argv)
{
	int	m;
	int	n;

	if( argc < 3 || argc > 5 )  {
		fputs( usage, stderr );
		return 1;
	}

	in_cookie = bu_cv_cookie( argv[1] );
	out_cookie = bu_cv_cookie( argv[2] );

	if( argc >= 5 )  {
		if( (outfp = fopen( argv[4], "w" )) == NULL )  {
			perror(argv[4]);
			return 2;
		}
	} else {
		outfp = stdout;
	}

	if( argc >= 4 )  {
		if( (infp = fopen( argv[3], "r" )) == NULL )  {
			perror(argv[3]);
			return 3;
		}
	} else {
		infp = stdin;
	}

	if( isatty(fileno(outfp)) )  {
		fprintf(stderr, "cv: trying to send binary output to terminal\n");
		return 5;
	}

	iitem = bu_cv_itemlen( in_cookie );
	oitem = bu_cv_itemlen( out_cookie );
#define NITEMS	(64*1024)
	inbytes = NITEMS*iitem;
	outbytes = NITEMS*oitem;

	ibuf = (genptr_t)bu_malloc( inbytes, "cv input buffer" );
	obuf = (genptr_t)bu_malloc( outbytes, "cv output buffer" );

	while( !feof( infp ) )  {
		if( (n = fread( ibuf, iitem, NITEMS, infp )) <= 0 )
			break;
		m = bu_cv_w_cookie( obuf, out_cookie, outbytes, ibuf, in_cookie, n );
		if( m != n )  {
			fprintf(stderr, "cv: bu_cv_w_cookie() ret=%d, count=%d\n", m, n );
			return 4;
		}
		m = fwrite( obuf, oitem, n, outfp );
		if( m != n )  {
			perror("fwrite");
			fprintf(stderr, "cv: fwrite() ret=%d, count=%d\n", m, n );
			return 5;
		}
	}
	fclose(infp);
	fclose(outfp);

	return 0;
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
