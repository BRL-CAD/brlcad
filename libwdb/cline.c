/*
 *			C L I N E . C
 *
 * Support for cline solids (kludges from FASTGEN)
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

mk_cline( fp, name, V, height, radius, thickness )
FILE *fp;
char *name;
point_t V;
vect_t height;
fastf_t radius;
fastf_t thickness;
{
	struct rt_cline_internal cli;

	cli.magic = RT_CLINE_INTERNAL_MAGIC;
	VMOVE( cli.v, V );
	VMOVE( cli.h, height );
	cli.thickness = thickness;
	cli.radius = radius;

	return mk_export_fwrite( fp, name, (genptr_t)&cli, ID_CLINE );
}
