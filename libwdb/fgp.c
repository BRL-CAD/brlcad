/*
 *			P L A T E . C
 *
 * Support for fgp mode solids (kludges from FASTGEN)
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
 *	This software is Copyright (C) 1999 by the United States Army
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

mk_fgp( fp, name, ref_solid, thickness, mode )
FILE *fp;
char *name;
char *ref_solid;
fastf_t thickness;
int mode;
{
	struct rt_fgp_internal plt;

	plt.magic = RT_FGP_INTERNAL_MAGIC;
	plt.thickness = thickness;
	plt.mode = mode;
	strncpy( plt.referenced_solid, ref_solid, NAMELEN );

	return mk_export_fwrite( fp, name, (genptr_t)&plt, ID_FGP );
}
