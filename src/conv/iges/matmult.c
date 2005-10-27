/*                       M A T M U L T . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file matmult.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "vmath.h"

void Matmult( a , b , c )
mat_t a,b,c;
{
	mat_t tmp;
	int i,j,k;

	for( i=0 ; i<4 ; i++ )
		for( j=0 ; j<4 ; j++ )
		{
			tmp[i*4+j] = 0.0;
			for( k=0 ; k<4 ; k++ )
				tmp[i*4+j] += a[i*4+k] * b[k*4+j];
		}
	for( i=0 ; i<4 ; i++ )
		for( j=0 ; j<4 ; j++ )
			c[i*4+j] = tmp[i*4+j];
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
