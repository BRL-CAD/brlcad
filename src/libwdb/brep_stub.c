/*                          B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2010 United States Government as represented by
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
/** @file brep.c
 *
 *  Library stub for writing BREP objects into
 *  MGED databases from arbitrary procedures.
 *
 *  Authors -
 *	Jason Owens
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "wdb.h"


/*
 *                        M K _ B R E P
 *
 *  Create a brep in the geometry file.
 */
int
mk_brep( struct rt_wdb* file, const char* name, ON_Brep* brep )
{
    bu_log("mk_brep() requires BRL-CAD to be built with openNURBS\n");
    return -1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
