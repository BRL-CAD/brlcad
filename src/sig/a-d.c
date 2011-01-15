/*                           A - D . C
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
/** @file a-d.c
 *
 * Ascii to double.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


#define	COMMENT_CHAR	'#'


int main(int argc, char **argv)
{
    double	d;
    int	i;

    if ( isatty(fileno(stdout)) ) {
	bu_exit(1, "Usage: a-d [values] < ascii > doubles\n");
    }

    if ( argc > 1 ) {
	/* get them from the command line */
	for ( i = 1; i < argc; i++ ) {
	    d = atof( argv[i] );
	    fwrite( &d, sizeof(d), 1, stdout );
	}
    } else {
	/* get them from stdin */
#if 0
	char	s[80];
	while ( bu_fgets(s, 80, stdin) != NULL ) {
	    d = atof( s );
#else
	    /* XXX This one is slower but allows more than 1 per line */
	    while (1) {
		int	ch;

		while (isspace(ch = getchar()))
		    ;
		if (ch == COMMENT_CHAR) {
		    while (((ch = getchar()) != '\n') && (ch != EOF))
			;
		}
		if (ch == EOF)
		    bu_exit(0, NULL);
		else
		    ungetc(ch, stdin);

		if ( scanf("%lf", &d) == 1 ) {
#endif
		    fwrite( &d, sizeof(d), 1, stdout );
		}
		else if (feof(stdin))
		    bu_exit(0, NULL);
		else {
		    bu_exit(1, "Error in input stream\n");
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
