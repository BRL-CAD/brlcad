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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"

usage()
{
	fprintf( stderr , "Usage:  iges-g [-n|d|t] -o file.g file.iges\n" );
	fprintf( stderr , "\t-n - Convert all rational B-spline surfaces to a single spline solid\n" );
	fprintf( stderr , "\t-d - Convert IGES drawings to NMG objects (and ignore solid objects)\n" );
	fprintf( stderr , "\t-t - Convert all trimmed surfaces to NMG trimmed NURBS\n" );
	fprintf( stderr , "\t\tthe n, d, and t options are mutually exclusive\n" );
	fprintf( stderr , "\t-o - Specify BRLCAD output file\n" );
	exit( 1 );
}
