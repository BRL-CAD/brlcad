/*                       M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2009 United States Government as represented by
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
/** @file metaball.c
 *
 * Support for the Metaball solid
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

int
mk_metaball( 
	struct rt_wdb *fp, 
	const char *name, 
	const int nctlpt, 
	const int method, 
	const fastf_t threshold, 
	const fastf_t *pts[5])
{
    struct rt_metaball_internal *mb;
    int i;

    BU_GETSTRUCT( mb, rt_metaball_internal );
    mb->magic = RT_METABALL_INTERNAL_MAGIC;
    mb->method = method;
    mb->threshold = threshold;

    return wdb_export( fp, name, (genptr_t)mb, ID_METABALL, mk_conv2mm );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
