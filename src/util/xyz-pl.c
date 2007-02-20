/*                        X Y Z - P L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2007 United States Government as represented by
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
/** @file xyz-pl.c
 *
 *  Program to take input with up to 3 white-space separated columns,
 *  expressed as
 *	x y z
 *  and produce a 3-D UNIX-plot file of the resulting space curve.
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
#include <stdio.h>
#include "common.h"


#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "plot3.h"


char	buf[2048];

int	debug = 0;

int
main(int argc, char *argv)
{
	double	xyz[3];
	int	i;
	int	first = 1;

	for(;;)  {
		xyz[0] = xyz[1] = xyz[2] = 0.0;

		buf[0] = '\0';
		bu_fgets( buf, sizeof(buf), stdin );
		if( feof(stdin) )  break;
		i = sscanf( buf, "%lf %lf %lf",
			&xyz[0], &xyz[1], &xyz[2] );
		if(debug)  {
			fprintf(stderr,"buf=%s", buf);
			fprintf(stderr,"%d: %f\t%f\t%f\n",
				i, xyz[0], xyz[1], xyz[2] );
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
