/*                           A - D . C
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
 */
/** @file a-d.c
 *
 * Ascii to double.
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include "machine.h"
#include "bu.h"

#define	COMMENT_CHAR	'#'

static char usage[] = "\
Usage: a-d [values] < ascii > doubles\n";

int main(int argc, char **argv)
{
	double	d;
	int	i;

	if( isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( argc > 1 ) {
		/* get them from the command line */
		for( i = 1; i < argc; i++ ) {
			d = atof( argv[i] );
			fwrite( &d, sizeof(d), 1, stdout );
		}
	} else {
		/* get them from stdin */
#if 0
		char	s[80];
		while( fgets(s, 80, stdin) != NULL ) {
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
			exit (0);
		    else
			ungetc(ch, stdin);

		    if ( scanf("%lf", &d) == 1 ) {
#endif
			fwrite( &d, sizeof(d), 1, stdout );
		    }
		    else if (feof(stdin))
			exit (0);
		    else {
			bu_log("Error in input stream\n");
			exit (1);
		    }
		}
	}
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
