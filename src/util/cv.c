/*
 *			C V . C
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1991-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
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
		exit(1);
	}

	in_cookie = bu_cv_cookie( argv[1] );
	out_cookie = bu_cv_cookie( argv[2] );

	if( argc >= 5 )  {
		if( (outfp = fopen( argv[4], "w" )) == NULL )  {
			perror(argv[4]);
			exit(2);
		}
	} else {
		outfp = stdout;
	}

	if( argc >= 4 )  {
		if( (infp = fopen( argv[3], "r" )) == NULL )  {
			perror(argv[3]);
			exit(3);
		}
	} else {
		infp = stdin;
	}

	if( isatty(fileno(outfp)) )  {
		fprintf(stderr, "cv: trying to send binary output to terminal\n");
		exit(5);
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
			exit(4);
		}
		m = fwrite( obuf, oitem, n, outfp );
		if( m != n )  {
			perror("fwrite");
			fprintf(stderr, "cv: fwrite() ret=%d, count=%d\n", m, n );
			exit(5);
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
