/*
 *			N U R B . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1987-2004 by the United States Army.
 *	All rights reserved.
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
