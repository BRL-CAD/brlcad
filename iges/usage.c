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

char *message="Usage:  iges-g [-n|d|t] -o file.g file.iges\n\
	-n - Convert all rational B-spline surfaces to a single spline solid\n\
	-d - Convert IGES drawings to NMG objects (and ignore solid objects)\n\
	-t - Convert all trimmed surfaces to NMG trimmed NURBS\n\
	-o - Specify BRLCAD output file\n\
	-p - Write BREP objects as polysolids rather than NMG's\n\
The n, d, and t options are mutually exclusive.\n\
With none of the n, d, or t options specified, the default action\n\
is to convert only IGES solid model entities (CSG and planar face BREP)\n";

	

usage()
{
	fprintf( stderr , message );
	exit( 1 );
}
