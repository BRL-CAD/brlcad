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

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "bu.h"


char *message="Usage:  iges-g [-N solid_name] [-X nmg_debug_flag] [-x rt_debug_flag] [-n|d|t] -o file.g file.iges\n\
	-n - Convert all rational B-spline surfaces to a single spline solid\n\
	-d - Convert IGES drawings to NMG objects (and ignore solid objects)\n\
	-3 - Convert IGES drawings to NMG objects, but don't project to 2D (and ignore solid objects)\n\
	-t - Convert all trimmed surfaces to a single NMG trimmed NURBS solid\n\
	-o - Specify BRLCAD output file\n\
	-p - Write BREP objects as NMG's rather than BOT's\n\
	-X - Set debug flag for NMG routines\n\
	-x - Set debug flag for librt\n\
	-N - Specify name of solid to be created\n\
The n, d (or 3), and t options are mutually exclusive.\n\
With none of the n, d (or 3), or t options specified, the default action\n\
is to convert only IGES solid model entities (CSG and planar face BREP)\n\
The N option provides a name for the single solid created with the n or t\n\
options, it is ignored for all other options\n";

	
void
usage()
{
	bu_log( message );
	exit( 1 );
}
