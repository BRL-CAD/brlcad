/*                           N M G . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
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
/** @file nmg.c
 *
 *  libwdb support for writing an NMG to disk.
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
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ N M G
 *
 *  The NMG is freed after being written.
 *
 *  Returns -
 *	<0	error
 *	 0	OK
 */
int
mk_nmg( struct rt_wdb *filep, const char *name, struct model *m )
{
	NMG_CK_MODEL( m );

	return wdb_export( filep, name, (genptr_t)m, ID_NMG, mk_conv2mm );
}

/*
 *			M K _ B O T _ F R O M _ N M G
 *
 *  For ray-tracing speed, many database conversion routines like to
 *  offer the option of converting NMG objects to bags of triangles (BoT).
 *  Here is a convenience routine to replace the old
 *  (now obsolete in BRL-CAD 6.0) routine write_shell_as_polysolid.
 */
int
mk_bot_from_nmg( struct rt_wdb *ofp, const char *name, struct shell *s )
{
	struct rt_bot_internal *botp;

	botp = nmg_bot( s, &ofp->wdb_tol );

	return wdb_export( ofp, name, (genptr_t)botp, ID_BOT, mk_conv2mm );
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
