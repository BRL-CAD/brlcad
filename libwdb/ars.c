/*
 *			A R S . C
 *
 *  libwdb support for writing an ARS.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
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

/*
 *			M K _ A R S
 *
 *  The input is an array of pointers to an array of fastf_t values.
 *  There is one pointer for each curve.
 *  It is anticipated that there will be pts_per_curve+1 elements per
 *  curve, the first point being repeated as the final point, although
 *  this is not checked here.
 *
 *  Returns -
 *	 0	OK
 *	-1	Fail
 */
int
mk_ars( filep, name, ncurves, pts_per_curve, curves )
FILE	*filep;
char	*name;
int	ncurves;
int	pts_per_curve;
fastf_t	**curves;
{
	struct rt_ars_internal	ars;

	ars.magic = RT_ARS_INTERNAL_MAGIC;
	ars.ncurves = ncurves;
	ars.pts_per_curve = pts_per_curve;
	ars.curves = curves;

	return mk_export_fwrite( filep, name, (genptr_t)&ars, ID_ARS );
}
