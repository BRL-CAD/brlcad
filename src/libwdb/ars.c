/*                           A R S . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file ars.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



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
mk_ars(struct rt_wdb *filep, const char *name, int ncurves, int pts_per_curve, fastf_t **curves)
{
	struct rt_ars_internal	*ars;

	BU_GETSTRUCT( ars, rt_ars_internal );
	ars->magic = RT_ARS_INTERNAL_MAGIC;
	ars->ncurves = ncurves;
	ars->pts_per_curve = pts_per_curve;
	ars->curves = curves;

	return wdb_export( filep, name, (genptr_t)ars, ID_ARS, mk_conv2mm );
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
