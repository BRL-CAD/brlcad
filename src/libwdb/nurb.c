/*                          N U R B . C
 * BRL-CAD
 *
 * Copyright (C) 1987-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file nurb.c
 *
 *  Library for writing NURB objects into
 *  MGED databases from arbitrary procedures.
 *
 *  Authors -
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "nurb.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

/*
 *			M K _ B S P L I N E
 *
 *  Output an array of B-spline (NURBS) surfaces which comprise a solid.
 *  The surface is freed when it is written.
 */
int
mk_bspline( struct rt_wdb *wdbp, const char *name, struct face_g_snurb **surfs )
{
	struct rt_nurb_internal	*ni;

	BU_GETSTRUCT( ni, rt_nurb_internal );
	ni->magic = RT_NURB_INTERNAL_MAGIC;
	ni->srfs = surfs;

	for( ni->nsrf = 0; ni->srfs[ni->nsrf] != NULL; ni->nsrf++ )  /* NIL */ ;

	return wdb_export( wdbp, name, (genptr_t)ni, ID_BSPLINE, mk_conv2mm );
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
