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

/* spline function */

#include "conf.h"

#include <stdio.h>

#include "machine.h"

fastf_t
splinef( c , s )
fastf_t c[4],s;
{
	int i;
	float retval;
	double stopow=1.0;

	retval = c[0];
	for( i=1 ; i<4 ; i++ )
	{
		stopow *= s;
		if( c[i] != 0.0 )
			retval += c[i]*stopow;
	}

	return( retval );
}
