/*                       T X Y Z - P L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2007 United States Government as represented by
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
/** @file txyz-pl.c
 *			X Y Z - P L . C
 *
 *  Program to take (up to) 4-column input, expressed as
 *	time x y z
 *  and produce a plot file of the resulting space curve.
 *  Mostly useful to preview animation results.
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



#include <stdio.h>
#include "machine.h"
#include "plot3.h"

char	buf[2048];

int	debug = 0;

int
main( argc, argv )
int	argc;
char	*argv[];
{
	double	t, xyz[3];
	int	i;
	int	first = 1;

	if( argc > 1 )  debug = 1;

	for(;;)  {
		t = xyz[0] = xyz[1] = xyz[2] = 0.0;

		buf[0] = '\0';
		fgets( buf, sizeof(buf), stdin );
		if( feof(stdin) )  break;

		if( buf[0] == '#' )  continue;

		i = sscanf( buf, "%lf %lf %lf %lf",
			&t, &xyz[0], &xyz[1], &xyz[2] );
		if(debug)  {
			fprintf(stderr,"buf=%s", buf);
			fprintf(stderr,"%d: %f\t%f\t%f\t%f\n",
				i, t, xyz[0], xyz[1], xyz[2] );
		}
		if( i <= 0 )
			break;
		if( first )  {
			first = 0;
			pdv_3move( stdout, xyz );
		} else {
			pdv_3cont( stdout, xyz );
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
