/*                     D U N N C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file dunncolor.c
 *			D U N N C O L O R . C
 *
 *	Sets the exposure values in the Dunn camera to the
 *	specified arguments.
 *
 *	dunncolor baseval redval greenval blueval
 *
 *  Author -
 *	Don Merritt
 *	August 1985
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdlib.h>

extern int	fd;
extern char	cmd;
extern int	polaroid;

extern void dunnopen(void);
extern int ready(int nsecs);
extern void getexposure(char *title);
extern int dunnsend(char color, int val);


int
main(int argc, char **argv)
{

	dunnopen();

	if(!ready(5)) {
		printf("dunncolor:  camera not ready\n");
		exit(50);
	}

	if( argc > 2 && strcmp( argv[1], "-p" ) == 0 )  {
		/* Polaroid rather than external camera */
		polaroid = 1;
		argc--; argv++;
	}
	getexposure("old");
	if(!ready(5)) {
		printf("dunncolor:  camera not ready\n");
		exit(50);
	}

	/* check argument */
	if ( argc != 5 && argc != 6 ) {
		printf("usage: dunncolor [-p] baseval redval greenval blueval\n");
		exit(25);
	}

	dunnsend('A',atoi(*++argv));
	dunnsend('R',atoi(*++argv));
	dunnsend('G',atoi(*++argv));
	dunnsend('B',atoi(*++argv));

	getexposure("new");

	if(!ready(5)) {
		printf("dunncolor:  camera not ready\n");
		exit(50);
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
