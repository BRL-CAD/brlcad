/*                          A R B N . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2006 United States Government as represented by
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
/** @file arbn.c
 *
 *  libwdb support for writing an ARBN.
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
 *			M K _ A R B N
 *
 *  Caller is responsible for freeing eqn[]
 *
 *  Returns -
 *	<0	error
 *	 0	OK
 */
int
mk_arbn(struct rt_wdb *filep, const char *name, int neqn, plane_t (*eqn))
{
	struct rt_arbn_internal	*arbn;

	if( neqn <= 0 )  return(-1);

	BU_GETSTRUCT( arbn, rt_arbn_internal );
	arbn->magic = RT_ARBN_INTERNAL_MAGIC;
	arbn->neqn = neqn;
	arbn->eqn = eqn;

	return wdb_export( filep, name, (genptr_t)arbn, ID_ARBN, mk_conv2mm );
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
