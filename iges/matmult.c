/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

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
