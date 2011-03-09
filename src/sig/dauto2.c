/*                        D A U T O 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file dauto2.c
 *
 * Compute the autocorrelation function of doubles.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"


#define	BSIZE	2048		/* Must be AT LEAST 2*Points in spectrum */
double	data[BSIZE];		/* Input buffer */
int	clip[BSIZE];		/* clipped buffer */
int	out[BSIZE];
double	r[BSIZE];

static const char usage[] = "\
Usage: dauto2 [window_size (512)] < doubles\n";

int main(int argc, char **argv)
{
    int	i, j, n, L;
    int *dp1, *dp2;
    double	max1, max2, m, m2;
    size_t ret;

    if ( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
	bu_exit(1, "%s", usage );
    }

    L = (argc > 1) ? atoi(argv[1]) : 512;

    while ( !feof( stdin ) ) {
	n = fread( data, sizeof(*data), L, stdin );
	if ( n <= 0 )
	    break;
	if ( n < L )
	    memset((char *)&data[n], 0, (L-n)*sizeof(*data));

	max1 = data[0];
	for ( i = 1; i < L/2; i++ ) {
	    m = data[i] >= 0 ? data[i] : - data[i];
	    if ( m > max1 ) max1 = m;
	}
	max2 = data[2*L/3];
	for ( i = 2*L/3+1; i < L; i++ ) {
	    m = data[i] >= 0 ? data[i] : - data[i];
	    if ( m > max2 ) max2 = m;
	}
	m = max1 > max2 ? max2 : max1;
	m *= 0.6;
	m2 = -m;
	for ( i = 0; i < L; i++ ) {
	    if ( data[i] > m )
		clip[i] = 1;
	    else if ( data[i] < m2 )
		clip[i] = -1;
	    else
		clip[i] = 0;
	}
	for ( i = 0; i < L; i++ ) {
	    out[i] = 0;
	    dp1 = &clip[0];
	    dp2 = &clip[i];
	    for ( j = L-i; j > 0; j-- ) {
		if ( *dp1 == 0 || *dp2 == 0 )
		    continue;
		else if ( *dp1 == *dp2 )
		    out[i]++;
		else
		    out[i]--;
	    }
	}
	for ( i = 0; i < L; i++ ) {
	    r[i] = out[i];
	}
	ret = fwrite( r, sizeof(*r), L, stdout );
	if (ret != (size_t)L)
	    perror("fwrite");
    }

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
