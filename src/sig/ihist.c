/*                         I H I S T . C
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
/** @file ihist.c
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"


long	bits[16] = {0};
long	values[65536] = {0};
long	*zerop;
short	ibuf[1024] = {0};

int	verbose = 0;

static const char usage[] = "\
Usage: ihist [-v] < shorts\n";

int main(int argc, char **argv)
{
    long i, bit;
    int	n;
    int	max, min;
    long	num, levels=0;

    while ( argc > 1 ) {
	if ( BU_STR_EQUAL( argv[1], "-v" ) ) {
	    verbose++;
	} else
	    break;
	argc--;
	argv++;
    }
    if ( argc > 1 || isatty(fileno(stdin)) ) {
	bu_exit(1, "%s", usage );
    }

    num = 0;
    zerop = &values[32768];
    while ( (n = fread(ibuf, 2, 1024, stdin)) > 0 ) {
	num += n;
	for ( i = 0; i < n; i++ ) {
	    zerop[ ibuf[i] ] ++;
	}
    }
    printf( "%ld values\n", num );
    max = 32767;
    while ( zerop[max] == 0 )
	max--;
    min = -32768;
    while ( zerop[min] == 0 )
	min++;
    printf( "Max = %d\n", max );
    printf( "Min = %d\n", min );

    printf( "Bits\n" );
    for ( i = -32768; i < 32768; i++ ) {
	if ( zerop[i] == 0 )
	    continue;
	levels++;
	for ( bit = 0; bit < 16; bit++ ) {
	    if ( i & (1<<bit) )
		bits[ bit ] += zerop[i];
	}
    }
    for ( bit = 0; bit < 16; bit++ ) {
	printf( "%8ld: %10ld (%f)\n",
		bit, bits[bit], (double)bits[bit]/(double)num );
    }

    printf( "%ld levels used (%04.2f %%)\n", levels, levels/65535.0 * 100.0 );
    if ( verbose ) {
	for ( i = -32768; i < 32768; i++ ) {
	    if ( zerop[i] ) {
		printf( "%ld %ld\n", i, zerop[i] );
	    }
	}
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
